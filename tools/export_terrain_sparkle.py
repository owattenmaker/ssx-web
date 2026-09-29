#!/usr/bin/env python3
"""Export the terrain snow sparkle patches (docs/presentation.md 14) next to every terrain package:

  <root>/terrain-sparkle.bin   'SPKL', u32 version 2, u32 patch count, u32 Surface block offset (0: none); then per sparkle
                               patch 56 little-endian words: the record's 16 power-basis rows +0x40..+0x13F as float32 xyz
                               (source cm, z up, the record's own order: row q = coefficient 15 - q, i.e. the VU upload order),
                               the bounding box +0x158 / +0x164 (lo xyz, hi xyz), u32 resource id, u32 0.
                               Surface block (the location's world painter type 10, tWPIGD_Surface): f32 tree scale, f32 origin
                               x, y, u32 root, u32 node count, u32 outside words x2, u32 payload count; the nodes (4 x u16 each,
                               tools/course_painters.py point_tree); the payloads (f32 rate, f32 density). Its current density
                               is renderer+0xC4, the sprite count's last factor (38D720; 1.0 without a Surface section).

A patch sparkles when its record flag word +0xC has bit 23 (0x800000; on the disc: most snow patches). The flag is not in
terrain.json, so the patches are matched to the disc's SSB kind-1 records by their 16 rows (terrain.json keeps them exactly:
metres, native (x, z, -y)). Rows, box and corners come from the same record the game uploads to VU1 (0x38D690). The painter
record is the location's own (the root's directory name: a course root or a streamed location, bam.sdb track = its index).

usage: export_terrain_sparkle.py [--assets web/public/assets] [--out DIR (default: in place)] [ROOT ...]
"""
import argparse, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import records, world_chunks, locations  # noqa: E402
from import_sky import painter_sections, painted_entries  # noqa: E402
from course_painters import point_tree  # noqa: E402

SPARKLE = 0x800000
f32 = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]


def disc_records():
    """({16 rows (bytes) -> record} of every SSB kind-1 (terrain patch) record, {location code -> kind-15 painter record})."""
    out, painters = {}, {}
    codes = [l['name'] for l in locations(ROOT / 'local/assets/source/ps2/bam.sdb')]
    for chunk in world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb'):
        for kind, track, rid, data in records(chunk):
            if kind == 1 and len(data) == 432: out.setdefault(data[0x40:0x140], data)
            if kind == 15 and len(data) >= 64 and track < len(codes):
                if codes[track] in painters: raise ValueError(f'{codes[track]}: more than one world painter record')
                painters[codes[track]] = data
    return out, painters


def surface_block(painter):
    """The Surface section (type 10) of a kind-15 painter record as the file's block, or b'' (none: density 1.0)."""
    section = painter_sections(painter).get(10) if painter else None
    if section is None: return b''
    entries = painted_entries(section, 2)
    if any(kind != 10 for kind, _ in entries): raise ValueError('Surface section entry with another type')
    tree = point_tree(section, len(entries))
    head = struct.pack('<3f5I', tree['scale'], *tree['origin'], tree['root'], len(tree['nodes']), *tree['outside_words'], len(entries))
    return head + b''.join(struct.pack('<4H', *n) for n in tree['nodes']) + b''.join(struct.pack('<2f', *v) for _, v in entries)


def rows_of(patch):
    """terrain.json coefficients (metres, native x, z, -y; coefficient k = u^(k%4) v^(k//4)) -> the record's row bytes."""
    rows = []
    for q in range(16):
        c = patch['coefficients'][15 - q]
        rows.append(struct.pack('<4f', f32(c[0] * 100), f32(-c[2] * 100), f32(c[1] * 100), 1.0))
    return b''.join(rows)


def export(root, out_dir, table, painters):
    doc = json.loads((root / 'terrain.json').read_text())
    words = []; n = matched = 0
    for p in doc['patches']:
        rec = table.get(rows_of(p))
        if rec is None: continue
        matched += 1
        if not (struct.unpack_from('<I', rec, 12)[0] & SPARKLE): continue
        n += 1
        for q in range(16): words += list(struct.unpack_from('<3I', rec, 0x40 + 16 * q))
        words += list(struct.unpack_from('<6I', rec, 0x158)) + [p['resource_id'] & 0xFFFFFFFF, 0]
    if matched != len(doc['patches']): raise ValueError(f'{root}: {len(doc["patches"]) - matched} patches without a disc record')
    surface = surface_block(painters.get(root.name))
    out_dir.mkdir(parents=True, exist_ok=True)
    body = struct.pack(f'<{len(words)}I', *words)
    __import__('atomic_write').write_bytes(out_dir / 'terrain-sparkle.bin', b'SPKL' + struct.pack('<3I', 2, n, 16 + len(body) if surface else 0) + body + surface)
    return n, len(doc['patches']), len(surface)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets', type=Path, default=ROOT / 'web/public/assets')
    p.add_argument('--out', type=Path, default=None, help='write DIR/<root relative to assets>/terrain-sparkle.bin instead of in place')
    p.add_argument('roots', nargs='*')
    a = p.parse_args()
    roots = [a.assets / r for r in a.roots] if a.roots else sorted(q.parent for q in a.assets.glob('**/terrain.json'))
    table, painters = disc_records()
    summary = {}
    for root in roots:
        rel = root.relative_to(a.assets)
        summary[str(rel)] = list(export(root, (a.out / rel) if a.out else root, table, painters))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
