#!/usr/bin/env python3
"""Export the original world-painter type-4 (camera far-clip cap) painter of a course.

Recovered from SLUS_207.72 (engine/far_painter.hpp has the full notes):
- Painter type 4: factory 2C0408 case 4 -> ctor 2BC7A0 (0x10 bytes: +0 distance = -99999,
  +4 vtable 485218, +8 current = +C sample = 30000 (gp-42EC)), blend 2BCEA8 (slot 66:
  w = weight^2, current = w*value + (1-w)*current, sample = value; EE mul.s/add.s),
  notify 2BDA38 (slot 67, empty, before and after the blend), compare 2BDB10 (slot 68:
  current == value), reset 2BE0F8 (slot 69: +0 = 0, current = 30000 (gp-42A8), sample kept),
  getter 2C14B8 (slot 6, returns +8) reached through 2EE3B8(view) = *(*(*(4FA370 + view*F0))) + 8.
- Payload (2 floats): rate, far cap in cm.
- Driver: 2C0778 with the -99999 automatic weight, called from the camera 15E668 (15EBBC:
  2ED490(view = outer+0x18 + 6, x = outer+0x20, y = outer+0x24)), then 15EBD8 reads the cap.

Writes web/public/assets/<L>/SECTIONS/far-painter.json (git-ignored). A course record without a
type-4 section (BRA2, BHP1) is written as painter=null: every tick resets to 30000.
`--survey` lists every SDB world painter record that authors type 4.
"""
import argparse, hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations  # noqa: E402
from import_sky import painter_sections, painted_entries  # noqa: E402
from course_painters import course_painter, point_tree, start_selection  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
ELF = ROOT / 'local/disc/SLUS_207.72'
GP = 0x4A30F0


def elf_float(address):
    data = ELF.read_bytes()
    phoff, phnum = struct.unpack_from('<I', data, 0x1C)[0], struct.unpack_from('<H', data, 0x2C)[0]
    for i in range(phnum):
        kind, off, va, _, size, _ = struct.unpack_from('<6I', data, phoff + 32 * i)
        if kind == 1 and va <= address < va + size:
            return struct.unpack_from('<f', data, off + address - va)[0]
    raise ValueError(f'{address:#x} outside the ELF')


def constants():
    """Every constant the port uses, read from the executable (the header hard-codes the same bits)."""
    c = dict(ctor_value=elf_float(GP - 0x42EC), ctor_distance=elf_float(GP - 0x42E8), reset_value=elf_float(GP - 0x42A8),
             auto_weight=elf_float(GP - 0x6C6C), split_far=elf_float(GP - 0x6C68), stream_ratio=elf_float(GP - 0x6C64),
             stream_min=elf_float(GP - 0x6C60), stream_max=elf_float(GP - 0x6C5C), range_scale=1.5)
    expect = dict(ctor_value=30000.0, ctor_distance=-99999.0, reset_value=30000.0, auto_weight=-99999.0, split_far=15000.0,
                  stream_min=15000.0, stream_max=30000.0)
    for k, v in expect.items():
        if c[k] != v:
            raise ValueError(f'unexpected executable constant {k} = {c[k]}')
    return c


def payloads(section):
    out = []
    for kind, (rate, value) in painted_entries(section, 2):
        if kind != 4:
            raise ValueError('Far-cap section entry with another type')
        out.append(dict(rate=rate, far_cm=value))
    return out


def far_package(code):
    found = course_painter(code)
    if found is None:
        raise ValueError(f'{code}: no world painter record')
    chunk, track, rid, data, sections = found
    base = dict(version=1, location=code, chunk=chunk, track=track, rid=rid, type=4,
                record_sha256=hashlib.sha256(data).hexdigest(),
                defaults=dict(distance=-99999.0, current=30000.0, sample=30000.0),
                constants=constants(),
                camera=dict(algorithm_far=30000.0, near_minimum=30.0, far_maximum=30000.0, view=6))
    if 4 not in sections:
        return dict(base, painter=None)
    section = sections[4]
    pays = payloads(section)
    tree = point_tree(section, len(pays))
    start = start_selection(code, tree)
    return dict(base, section_sha256=hashlib.sha256(section).hexdigest(), painter=dict(tree, payloads=pays), start=start)


def survey():
    locs = locations(SOURCE / 'bam.sdb')
    def name(ci):
        return next(l['name'] for l in locs if ci <= l['chunk_end'])
    rows = []
    for ci, chunk in enumerate(world_chunks(SOURCE / 'bam.ssb')):
        for kind, track, rid, data in records(chunk):
            if kind == 15 and len(data) >= 64 and 4 in painter_sections(data):
                section = painter_sections(data)[4]
                pays = payloads(section)
                rows.append(dict(location=name(ci), chunk=ci, track=track, nodes=len(point_tree(section, len(pays))['nodes']),
                                 payloads=[(p['rate'], p['far_cm']) for p in pays]))
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--location', default='ARA1')
    p.add_argument('--output', type=Path, help='Default: web/public/assets/<L>/SECTIONS/far-painter.json')
    p.add_argument('--survey', action='store_true')
    a = p.parse_args()
    if a.survey:
        for row in survey():
            print(row)
        return
    package = far_package(a.location)
    out = a.output or ROOT / 'web/public/assets' / a.location / 'SECTIONS/far-painter.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(package, separators=(',', ':')))
    painter = package['painter']
    print(a.location, 'far painter', 'absent (cap 30000)' if painter is None else
          f"{len(painter['payloads'])} payloads {[(q['rate'], q['far_cm']) for q in painter['payloads']]}, {len(painter['nodes'])} nodes, "
          f"start payload {package['start']['entry'] if package.get('start') else None}", '->', out)


if __name__ == '__main__':
    main()
