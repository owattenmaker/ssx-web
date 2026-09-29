#!/usr/bin/env python3
"""The state pack: the few bytes of the PS2 savestates and captures the exporters read, without the savestates.

Some browser data is recorded from the running game (docs/iso-pipeline.md "State pack"): the event seeds, rail runtime
flags, set-piece load state, RNG words ... The exporters that produce it read PCSX2/ARMSX2 savestates (their EE memory)
and capture streams, which cannot be distributed. The pack holds only what they look at:

  trace    tools/setup_from_iso.py --trace runs every step with tools/pipeline_hook, which records the footprint
           (the byte ranges of each savestate member / capture file each step reads), existence probes and listings
  build    for every traced file keep only the footprint bytes that differ from the base image a user can make
           from their own disc: the ELF's load segments for eeMemory.bin, zeros elsewhere. Small evidence files
           (JSON manifests, oracle output) are kept whole. Every entry records its digests and what it feeds.
  restore  rebuilds the savestates (zip, eeMemory.bin = ELF image + pack bytes) and capture files under local/,
           never over an existing file, and writes local/statepack/restored.json for the restore hook, which makes
           the rebuilt buffers hash to the originals' digests.

    python3 tools/statepack.py collect --trace local/pipeline/trace --out local/pipeline/footprint.json
    python3 tools/statepack.py build --footprint local/pipeline/footprint.json --out ssx3-statepack.zip
    python3 tools/statepack.py restore ssx3-statepack.zip
    python3 tools/statepack.py info ssx3-statepack.zip [--feeds]

SPDX-License-Identifier: GPL-3.0
"""
import argparse
import hashlib
import io
import json
import os
import struct
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
FORMAT = 'ssx3-statepack/1'
EE_SIZE = 32 * 1024 * 1024
ELF_SHA1 = '77114dfd1205eaccf1ccc18c5f9650097fa78bd8'
MERGE_GAP = 16          # footprint spans closer than this are stored as one range
DIFF_GAP = 8            # base-equal gaps shorter than this stay inside a stored range
WHOLE_LIMIT = 4 << 20   # small non-binary evidence files are kept whole up to this size


def merge(spans, gap=0):
    out = []
    for a, b in sorted(spans):
        if out and a <= out[-1][1] + gap:
            out[-1][1] = max(out[-1][1], b)
        else:
            out.append([a, b])
    return out


def digests(data):
    return dict(sha256=hashlib.sha256(data).hexdigest(), sha1=hashlib.sha1(data).hexdigest(), md5=hashlib.md5(data).hexdigest())


def elf_image(elf):
    """The EE memory a freshly loaded SLUS_207.72 occupies (PT_LOAD file bytes at their addresses, zeros elsewhere)."""
    image = bytearray(EE_SIZE)
    ph = struct.unpack_from('<I', elf, 28)[0]
    stride, count = struct.unpack_from('<HH', elf, 42)
    for i in range(count):
        typ, off, vaddr, _, filesz, _, _, _ = struct.unpack_from('<8I', elf, ph + i * stride)
        if typ == 1 and filesz:
            at = vaddr & 0x1FFFFFF
            image[at:at + filesz] = elf[off:off + filesz]
    return bytes(image)


def differing(data, base, spans):
    """Sub-ranges of spans where data differs from base; equal gaps shorter than DIFF_GAP stay inside a range."""
    out = []
    for a, b in spans:
        b = min(b, len(data))
        i = a
        while i < b:
            j = min(i + 4096, b)
            if data[i:j] == base[i:j]:
                i = j
                continue
            while data[i] == base[i]:
                i += 1
            start = last = i
            k = i + 1
            while k < b and k - last < DIFF_GAP:
                if data[k] != base[k]:
                    last = k
                k += 1
            out.append([start, last + 1])
            i = last + 1
    return merge(out)


# ---------------------------------------------------------------------------------------------------------------- collect
def collect(trace_dir, out_path):
    trace_dir = Path(trace_dir)
    files = {}          # rel -> dict(spans, whole, steps, members)
    steps = {}          # step -> dict(reads, writes)
    probes, listings, escapes = set(), {}, []
    for log_path in sorted(trace_dir.rglob('*.json')):
        log = json.loads(log_path.read_text())
        step = log['step']
        s = steps.setdefault(step, dict(reads=set(), writes=set()))
        s['reads'].update(log['reads']); s['writes'].update(log['writes'])
        for key, spans in log['footprints'].items():
            e = files.setdefault(key, dict(spans=[], whole=None, steps=set()))
            e['steps'].add(step)
            if spans:
                e['spans'].extend(spans)
        for key, why in log['whole'].items():
            e = files.setdefault(key, dict(spans=[], whole=None, steps=set()))
            e['whole'] = e['whole'] or why; e['steps'].add(step)
        probes.update(log['probes'])
        for d, names in log['listings'].items():
            listings.setdefault(d, set()).update(names)
        escapes.extend(dict(step=step, **x) for x in log['escapes'])
    for e in files.values():
        e['spans'] = merge(e['spans'])
        e['steps'] = sorted(e['steps'])
    written = set().union(*(s['writes'] for s in steps.values())) if steps else set()
    doc = dict(format=FORMAT + '/footprint', files=files, probes=sorted(probes), listings={k: sorted(v) for k, v in listings.items()},
               steps={k: dict(reads=sorted(v['reads']), writes=sorted(v['writes'])) for k, v in steps.items()},
               written=sorted(written), escapes=escapes)
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text(json.dumps(doc, indent=1))
    print(f'{len(files)} traced files/members, {len(probes)} probes, {len(listings)} listings, {len(escapes)} escapes -> {out_path}')
    return doc


# ---------------------------------------------------------------------------------------------------------------- build
def build(footprint_path, out_path, root=ROOT, state_roots=('local/reference', 'local/ps2-capture'), extra=()):
    root = Path(root)
    fp = json.loads(Path(footprint_path).read_text())
    elf = (root / 'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest() != ELF_SHA1:
        raise SystemExit('statepack: local/disc/SLUS_207.72 is not the verified NTSC-U executable')
    base_ee = elf_image(elf)
    is_state = lambda rel: any(rel == r or rel.startswith(r + '/') for r in state_roots)
    # every file (and zip member) the steps read under the state roots
    reads = {}
    for step, s in fp['steps'].items():
        for rel in s['reads']:
            if is_state(rel) or rel in extra:
                reads.setdefault(rel, set()).add(step)
    members = {}
    for key, e in fp['files'].items():
        rel, _, member = key.partition('::')
        if member:
            members.setdefault(rel, {})[member] = e
        reads.setdefault(rel, set()).update(e['steps'])
    entries, blobs = [], []
    total_kept = 0

    def feeds(step_names):
        outs = set()
        for s in step_names:
            outs.update(w for w in fp['steps'].get(s, {}).get('writes', []) if not w.startswith('local/pipeline/'))
        return dict(steps=sorted(step_names), outputs=sorted(outs)[:40], output_count=len(outs))

    for rel in sorted(reads):
        path = root / rel
        if not path.is_file():
            print(f'  missing (probe only?): {rel}')
            continue
        raw = path.read_bytes()
        entry = dict(path=rel, size=len(raw), digests=digests(raw), feeds=feeds(reads[rel]))
        if rel in members or rel.endswith('.p2s'):
            z = zipfile.ZipFile(io.BytesIO(raw))
            entry['kind'] = 'zip'
            entry['members'] = {}
            for member, e in sorted(members.get(rel, {}).items()):
                data = z.read(member)
                base = base_ee if member == 'eeMemory.bin' else bytes(len(data))
                spans = [[0, len(data)]] if e['whole'] else merge(e['spans'], MERGE_GAP)
                keep = differing(data, base, spans)
                blob = b''.join(data[a:b] for a, b in keep)
                entry['members'][member] = dict(size=len(data), base='elf' if member == 'eeMemory.bin' else 'zero',
                                                ranges=keep, blob=len(blobs), digests=digests(data),
                                                footprint_bytes=sum(b - a for a, b in spans), whole=e['whole'])
                blobs.append(blob); total_kept += len(blob)
        elif rel in fp['files'] and not fp['files'][rel]['whole'] and len(raw) > WHOLE_LIMIT:
            e = fp['files'][rel]
            spans = merge(e['spans'], MERGE_GAP)
            keep = differing(raw, bytes(len(raw)), spans)
            blob = b''.join(raw[a:b] for a, b in keep)
            entry.update(kind='sparse', base='zero', ranges=keep, blob=len(blobs), footprint_bytes=sum(b - a for a, b in spans))
            blobs.append(blob); total_kept += len(blob)
        else:
            entry.update(kind='whole', blob=len(blobs))
            blobs.append(raw); total_kept += len(raw)
        entries.append(entry)
    # existence probes / listings of files nothing read: placeholders
    have = {e['path'] for e in entries}
    placeholders = sorted({p for p in fp['probes'] if (root / p).is_file() and p not in have and is_state(p)} |
                          {f'{d}/{n}' for d, names in fp['listings'].items() for n in names
                           if (root / d / n).is_file() and f'{d}/{n}' not in have and is_state(f'{d}/{n}')})
    dirs = sorted({p for p in fp['probes'] if (root / p).is_dir() and is_state(p)} | set(fp['listings']))
    manifest = dict(format=FORMAT, created=time.strftime('%Y-%m-%d'), elf_sha1=ELF_SHA1, entries=entries,
                    placeholders=placeholders, directories=dirs, note='Bytes read by the exporters from PS2 savestates '
                    'and captures, minus what the user\'s own disc provides (docs/iso-pipeline.md "State pack").')
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    tmp = Path(str(out_path) + '.tmp')
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_LZMA) as z:
        z.writestr('manifest.json', json.dumps(manifest, indent=1))
        for i, blob in enumerate(blobs):
            z.writestr(f'blobs/{i}.bin', blob)
    os.replace(tmp, out_path)
    size = Path(out_path).stat().st_size
    print(f'state pack: {len(entries)} files ({sum(1 for e in entries if e["kind"] == "zip")} savestates), '
          f'{len(placeholders)} placeholders, {total_kept / 1e6:.2f} MB kept, {size / 1e6:.2f} MB packed -> {out_path}')
    return manifest


# ---------------------------------------------------------------------------------------------------------------- restore
def _atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + '.statepack-tmp')
    tmp.write_bytes(data)
    os.replace(tmp, path)


def restore(pack_path, root=ROOT, force=False):
    root = Path(root)
    elf = (root / 'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest() != ELF_SHA1:
        raise SystemExit('statepack: extract the disc first (local/disc/SLUS_207.72 missing or not the NTSC-U executable)')
    base_ee = elf_image(elf)
    registry_path = root / 'local/statepack/restored.json'
    registry = json.loads(registry_path.read_text()) if registry_path.exists() else dict(files={})
    with zipfile.ZipFile(pack_path) as z:
        manifest = json.loads(z.read('manifest.json'))
        if manifest['format'] != FORMAT or manifest['elf_sha1'] != ELF_SHA1:
            raise SystemExit(f'statepack: {pack_path} is not a {FORMAT} pack for this executable')
        made = skipped = 0
        for e in manifest['entries']:
            path = root / e['path']
            if path.exists() and not force and e['path'] not in registry['files']:
                skipped += 1      # a real file (maintainer tree): never replaced
                continue
            if e['kind'] == 'whole':
                data = z.read(f'blobs/{e["blob"]}.bin')
            elif e['kind'] == 'sparse':
                data = _fill(bytearray(e['size']), e['ranges'], z.read(f'blobs/{e["blob"]}.bin'))
            else:
                buf = io.BytesIO()
                with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED, compresslevel=1) as out:
                    for member, m in e['members'].items():
                        base = bytearray(base_ee) if m['base'] == 'elf' else bytearray(m['size'])
                        out.writestr(member, bytes(_fill(base, m['ranges'], z.read(f'blobs/{m["blob"]}.bin'))))
                    out.comment = b'ssx3-statepack: rebuilt from the state pack (tools/statepack.py)'
                data = buf.getvalue()
            if e['kind'] != 'zip' and digests(data)['sha256'] != e['digests']['sha256'] and e['kind'] == 'whole':
                raise SystemExit(f'statepack: {e["path"]} does not match its recorded digest')
            _atomic_write(path, data)
            registry['files'][e['path']] = dict(digests=e['digests'],
                                                members={k: dict(digests=m['digests']) for k, m in e.get('members', {}).items()})
            made += 1
        for p in manifest['placeholders']:
            path = root / p
            if not path.exists():
                _atomic_write(path, b'')
                registry['files'].setdefault(p, dict(digests={}, placeholder=True))
        for d in manifest['directories']:
            (root / d).mkdir(parents=True, exist_ok=True)
    _atomic_write(registry_path, json.dumps(registry, indent=1).encode())
    print(f'state pack: rebuilt {made} files, kept {skipped} existing, {len(manifest["placeholders"])} placeholders')


def _fill(buf, ranges, blob):
    at = 0
    for a, b in ranges:
        buf[a:b] = blob[at:at + b - a]
        at += b - a
    if at != len(blob):
        raise SystemExit('statepack: corrupt entry (blob length)')
    return buf


def info(pack_path, show_feeds=False):
    with zipfile.ZipFile(pack_path) as z:
        m = json.loads(z.read('manifest.json'))
        sizes = {i.filename: i.file_size for i in z.infolist()}
    rows = []
    for e in m['entries']:
        if e['kind'] == 'zip':
            kept = sum(sizes[f'blobs/{x["blob"]}.bin'] for x in e['members'].values())
        else:
            kept = sizes[f'blobs/{e["blob"]}.bin']
        rows.append((kept, e))
    rows.sort(key=lambda r: -r[0])
    print(f'{m["format"]}  {len(m["entries"])} entries, {sum(r[0] for r in rows) / 1e6:.2f} MB kept')
    for kept, e in rows:
        print(f'{kept:10d}  {e["kind"]:6s} {e["path"]}' + (f'\n            feeds {", ".join(e["feeds"]["steps"])}' if show_feeds else ''))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='command', required=True)
    c = sub.add_parser('collect'); c.add_argument('--trace', type=Path, default=ROOT / 'local/pipeline/trace')
    c.add_argument('--out', type=Path, default=ROOT / 'local/pipeline/footprint.json')
    b = sub.add_parser('build'); b.add_argument('--footprint', type=Path, default=ROOT / 'local/pipeline/footprint.json')
    b.add_argument('--out', type=Path, required=True); b.add_argument('--extra', action='append', default=[],
                                                                      help='additional evidence file (repo-relative) to keep whole')
    r = sub.add_parser('restore'); r.add_argument('pack', type=Path); r.add_argument('--force', action='store_true')
    i = sub.add_parser('info'); i.add_argument('pack', type=Path); i.add_argument('--feeds', action='store_true')
    a = p.parse_args()
    if a.command == 'collect':
        collect(a.trace, a.out)
    elif a.command == 'build':
        from iso_pipeline_steps import EVIDENCE
        build(a.footprint, a.out, extra=tuple(a.extra) + tuple(EVIDENCE))
    elif a.command == 'restore':
        restore(a.pack, force=a.force)
    else:
        info(a.pack, a.feeds)


if __name__ == '__main__':
    main()
