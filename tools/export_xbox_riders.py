#!/usr/bin/env python3
"""The Xbox HD rider texture set (docs/xbox-textures.md section 8): WARDROBE/<ID>/textures-xbox.tex and gear-xbox.tex.

Each twin holds every id of the rider's PS2 archive (WARDROBE/<ID>/textures.tex / gear.tex, tools/export_rider_textures.py),
in the same order:
  - the Xbox texture of the same name where it is the better one: the Xbox's own DXT1 / DXT3 blocks (level 0, no mips: the
    Xbox rider shapes carry none) behind a 16-byte 'SXBC' header (web/bc-texels.js), codec 'bc1' / 'bc2', domain 'xbox'
    (full intensity: the rider material halves it into the PS2 texel domain; a BC block cannot be halved);
  - the PS2 entry, byte for byte, everywhere else: no Xbox member, not 2x / 4x the PS2 size, not DXT1 / DXT3, or listed in
    KEEP_PS2 (docs/xbox-textures.md section 4.1: a different layout on the Xbox, or no measurable gain).
The browser picks the twin while the Xbox HD set is chosen (web/texture-archive.js riderArchiveUrl); UVs, materials, the
alpha test and blending are the same, only the texels change. Xbox data never goes to git: the output is game data like
every archive under web/public/assets.

Sources: the PS2 archives (--assets, default web/public/assets/WARDROBE) and the Xbox character archives
local/xbox/disc/data/char/<x>txx.big (tools/xdvdfs.py extract ... data/char). Index rows of a BC entry: id, offset, size,
width, height (the Xbox size), codec, domain, rgba (first 16 hex digits of SHA-256 of the decoded RGBA, tools/xbox_textures.py
decode_dxt rules: web/bc-texels.js decodeBC must give the same), ps2 [w, h], xbox (member / entry name).

    python3 tools/export_xbox_riders.py OUT [--rider ZOE ...]      # writes OUT/WARDROBE/<ID>/{textures,gear}-xbox.tex

SPDX-License-Identifier: GPL-3.0-only
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from texture_archive import MAGIC, read_archive  # noqa: E402
from xbox_textures import shpx_entries, unpack_member, decode_dxt_fast  # noqa: E402

ASSETS = ROOT / 'web/public/assets/WARDROBE'
XBOX = ROOT / 'local/xbox/disc/data/char'
XBOX_ARCHIVES = ('alleg', 'elise', 'griff', 'kaori', 'mac', 'moby', 'nate', 'psymo', 'viggo', 'zoe', 'other')
HEADER = 16
# PS2 kept (docs/xbox-textures.md section 4.1, measured against the fixed PS2 archives): a different layout on the Xbox (the UVs
# would not fit) and textures without a measurable gain (flat colours; detail within the upscale + DXT noise baseline).
KEEP_PS2 = frozenset((
    # a different layout: the Xbox texture fills the square / moves the items
    'stretch_boot_a01_a01', 'psymon_extn_01', 'psymon_extk_01', 'grommet_extk_01',
    # no measurable gain (detail above the PS2 resolution within 1.5 of the upscale + BC1 baseline, or flat colours)
    'arielle_extc_01', 'arielle_extc_02', 'bunny_boot_a01_a01', 'kaori_alph_d01', 'kaori_eatf_01', 'kaori_eath_01', 'psymon_extl_01',
    'rocco_eatd_02', 'rocco_extd_02', 'rocco_extm_01', 'seeiah_head_a01', 'elise_alph_e01', 'moby_exti_01', 'zoe_alph_z01'))


def big(data):
    if data[:4] != b'BIGF':
        raise ValueError('Expected a BIGF archive')
    count = struct.unpack_from('>I', data, 8)[0]
    at, out = 16, {}
    for _ in range(count):
        off, size = struct.unpack_from('>II', data, at)
        at += 8
        end = data.index(b'\0', at)
        out[data[at:end].decode('latin-1')] = data[off:off + size]
        at = end + 1
    return out


def xbox_members():
    if not XBOX.exists():
        raise SystemExit(f'{XBOX} missing: python3 tools/xdvdfs.py extract "<Xbox ISO>" local/xbox/disc data/char')
    members = {}
    for name in XBOX_ARCHIVES:
        for member, data in big((XBOX / f'{name}txx.big').read_bytes()).items():
            members.setdefault(member.rsplit('.', 1)[0].lower(), (f'{name}txx.big', member, data))
    return members


def rgba_digest(codec, blocks, w, h):
    return hashlib.sha256(decode_dxt_fast(0x60 if codec == 1 else 0x61, bytes(blocks), w, h)).hexdigest()[:16]


def hd_entry(stem, name, pw, ph, members):
    """-> (codec, width, height, blocks, source) of the Xbox texture replacing the PS2 one, or (None, reason)."""
    if stem in KEEP_PS2:
        return None, 'kept'
    hit = members.get(stem.lower())
    if not hit:
        return None, 'no Xbox member'
    archive, member, data = hit
    data = unpack_member(data)
    entries = shpx_entries(data)
    e = next((x for x in entries if name and x['name'].strip() == name.strip()), entries[0])
    code = e['code']
    if code not in (0x60, 0x61):
        return None, f'format {e["format"]}'
    w, h = e['width'], e['height']
    if not any(w == k * pw and h == k * ph for k in (2, 4)):
        return None, f'size {w}x{h} for {pw}x{ph}'
    codec = 1 if code == 0x60 else 2
    size = max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * (8 if codec == 1 else 16)
    blocks = data[e['at']:e['at'] + size]
    if len(blocks) != size:
        raise ValueError(f'{member}: truncated blocks')
    return (codec, w, h, blocks, f'{archive}|{member}|{e["name"]}'), None


def build(entries, kind, **extra):
    index, payload = [], bytearray()
    for e in entries:
        row = {k: v for k, v in e.items() if k != 'payload'}
        row = dict(id=row.pop('id'), offset=len(payload), size=len(e['payload']), **{k: v for k, v in row.items() if k not in ('offset', 'size')})
        index.append(row)
        payload += e['payload']
    head = json.dumps(dict(version=1, kind=kind, format='png+bc', entries=index, **extra), separators=(',', ':')).encode()
    data = MAGIC + struct.pack('<I', len(head)) + head
    return data + b'\0' * (-len(data) % 16) + bytes(payload)


def export(out, riders=None, assets=ASSETS, log=print):
    members = xbox_members()
    report = []
    for folder in sorted(p for p in Path(assets).iterdir() if p.is_dir()):
        if riders and folder.name not in riders:
            continue
        doc = json.loads((folder / 'wardrobe.json').read_text())
        for pack in ('textures', 'gear'):
            path = folder / f'{pack}.tex'
            if not path.exists():
                continue
            index, files = read_archive(path)
            entries, hd, kept = [], 0, {}
            for e in index['entries']:
                name = doc['textures'].get(e['id'], {}).get('name')
                got, reason = hd_entry(e['id'], name, e['width'], e['height'], members)
                if got:
                    codec, w, h, blocks, source = got
                    head = bytes([0x53, 0x58, 0x42, 0x43, codec, 1, w & 255, w >> 8, h & 255, h >> 8]) + bytes(6)
                    entries.append(dict(id=e['id'], payload=head + blocks, width=w, height=h, codec=f'bc{codec}', domain='xbox',
                                        rgba=rgba_digest(codec, blocks, w, h), ps2=[e['width'], e['height']], xbox=source))
                    hd += 1
                else:
                    row = {k: v for k, v in e.items() if k not in ('offset', 'size')}
                    entries.append(dict(payload=files[e['id']], **row))
                    kept[reason] = kept.get(reason, 0) + 1
            extra = {k: v for k, v in index.items() if k not in ('version', 'kind', 'format', 'entries', 'payload_offset')}
            extra['source'] = 'Xbox SSX 3 (USA) data/char/<x>txx.big DXT blocks where better (docs/xbox-textures.md section 8), else ' + str(extra.get('source', 'the PS2 entry'))
            data = build(entries, index['kind'] + '-xbox', **extra)
            dest = Path(out) / 'WARDROBE' / folder.name / f'{pack}-xbox.tex'
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
            report.append(dict(path=f'WARDROBE/{folder.name}/{pack}-xbox.tex', entries=len(entries), xbox=hd, kept=kept, bytes=len(data),
                               ps2_bytes=path.stat().st_size, sha256=hashlib.sha256(data).hexdigest()))
            log(f'{folder.name}/{pack}: {hd} Xbox, {sum(kept.values())} PS2 {kept}, {len(data)} bytes (PS2 {path.stat().st_size})')
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('out', type=Path)
    ap.add_argument('--rider', nargs='*')
    ap.add_argument('--assets', type=Path, default=ASSETS)
    a = ap.parse_args()
    report = export(a.out, a.rider, a.assets)
    (a.out / 'xbox-riders.json').write_text(json.dumps(report, indent=1))


if __name__ == '__main__':
    main()
