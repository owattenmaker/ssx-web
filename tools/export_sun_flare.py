#!/usr/bin/env python3
"""Export the original SSX 3 sun glow / lens flare data for the browser.

Recovered from SLUS_207.72 (see docs/terrain-render-fidelity.md, "Sun glow and
lens flare"):
- tWPIGD_Sun painter (factory 9, ctor 2BC910, vtable 484700, blend 2BD378,
  compare 2BDD38, reset 2BE1A8). Payload: rate, elevation deg, azimuth deg,
  R, G, B, glow alpha, texture index (int), flare alpha, glow half size.
- Sun object update 2F4DB8, draw 2F4A08 (glow) + 2F4690 (9 flare sprites set up
  by 2F43E0), visibility query 2EC478 (16x16 Z readback) via 2E3130.
- Textures: FX table 4891B0 index 45 'lens', 51 'sun1', 52 'sun2' from
  DATA/TEXTURES/EFFECTS.SSH (8-bit CLUT, GS alpha 0..128 kept raw).

Writes private assets to web/public/assets/SUN_FLARE (git-ignored); with
`--location X` (X != ARA1) to web/public/assets/X/sun-flare/ from X's course painter
record. A course record without a Sun section is written explicitly as
`painter: null` (textures/flares/constants still present): the browser should skip the
sun for that location. (2F4DB8 hides the sun for texture index -1; with no painter the
tWPIGD_Sun reset defaults would apply, texture 0, which is not observed on any course:
all 49 SDB painter records author a Sun section.)
"""
import argparse, hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc, EXPECTED_SHA1  # noqa: E402

GP = 0x4A30F0
ELF_BASE = 0xFF000


def f32(elf, va):
    return struct.unpack_from('<f', elf, va - ELF_BASE)[0]


def gp_f32(elf, offset):
    return f32(elf, GP + offset)


def member(data, name):
    if data[:4] != b'SHPS':
        raise ValueError('Expected PS2 SHPS texture container')
    count = struct.unpack_from('<I', data, 8)[0]
    for i in range(count):
        if data[16 + i * 8:20 + i * 8].decode('ascii') == name:
            at = struct.unpack_from('<I', data, 20 + i * 8)[0]
            return at, data[at:]
    raise ValueError(f'Missing original texture {name}')


def decode_clut8(data, name):
    width, height = struct.unpack_from('<HH', data, 4)
    size = int.from_bytes(data[1:4], 'little')
    if data[0] != 2 or size != 16 + width * height or data[size] != 33:
        raise ValueError(f'Unsupported original texture encoding for {name}')
    palette_size = int.from_bytes(data[size + 1:size + 4], 'little')
    palette = data[size + 16:size + palette_size]
    raw = bytearray()
    for index in data[16:size]:
        entry = (index & 0xe7) | ((index & 8) << 1) | ((index & 16) >> 1)  # PSMT8 CLUT swizzle
        if entry * 4 + 4 > len(palette):
            raise ValueError(f'CLUT index outside palette: {name}/{index}')
        raw.extend(palette[entry * 4:entry * 4 + 4])
    if max(raw[3::4]) > 128:
        raise ValueError('Unexpected PS2 FX alpha range')
    return width, height, bytes(raw)


def sun_painter(root, location='ARA1'):
    from world_assets import world_chunks, records, locations
    from import_sky import chunk_range, painter_sections
    folder = root / 'local/assets/source/ps2'
    _, begin, end = chunk_range(locations(folder / 'bam.sdb'), location)
    found = []
    for chunk_index, chunk in enumerate(world_chunks(folder / 'bam.ssb')):
        if chunk_index > end:
            break
        if chunk_index < begin:
            continue
        for kind, track, rid, data in records(chunk):
            if kind == 15 and len(data) >= 64 and 9 in painter_sections(data):
                found.append((chunk_index, track, rid, data, painter_sections(data)[9]))
    if not found and location != 'ARA1':
        return None
    if len(found) != 1:
        raise ValueError(f'Expected one {location} Sun painter section')
    chunk_index, track, rid, record, section = found[0]
    header, count = struct.unpack_from('<2I', section, 0)
    scale, ox, oy = struct.unpack_from('<3f', section, header)
    node_count = struct.unpack_from('<I', section, header + 12)[0]
    root_node = struct.unpack_from('<H', section, header + 20)[0]
    nodes = [list(struct.unpack_from('<4H', section, header + 40 + i * 8)) for i in range(node_count)]
    outside = list(struct.unpack_from('<2I', section, header + 24))
    payloads = []
    for i in range(count):
        kind, offset = struct.unpack_from('<2I', section, 12 + 8 * i)
        if kind != 9:
            raise ValueError('Sun section entry with another type')
        v = struct.unpack_from('<3f', section, offset) + struct.unpack_from('<4f', section, offset + 12)
        texture = struct.unpack_from('<i', section, offset + 28)[0]
        flare_alpha, size = struct.unpack_from('<2f', section, offset + 32)
        payloads.append(dict(rate=v[0], elevation=v[1], azimuth=v[2], colour=list(v[3:6]), glow_alpha=v[6],
                             texture=texture, flare_alpha=flare_alpha, size=size))
    return dict(location=location, chunk=chunk_index, track=track, rid=rid,
                record_sha256=hashlib.sha256(record).hexdigest(), scale=scale, origin=[ox, oy],
                root=root_node, nodes=nodes, outside_words=outside, payloads=payloads)


def flare_elements(elf):
    # 2F43E0: nine {quadrant, line position, half size/300, ARGB} records.
    # Colours are copied by 2F7804.. from 4D57C0+16*i, filled by 2F6BC0.. from
    # the gp pool (1.0/0.5/0 are immediates).
    g = lambda o: gp_f32(elf, o)
    layout = [(1, g(-0x38EC), g(-0x38F0)), (3, g(-0x38E8), g(-0x38E4)), (0, 0.5, g(-0x38E0)),
              (2, g(-0x38E0), g(-0x38DC)), (0, 0.0, g(-0x38F0)), (0, g(-0x38D8), g(-0x38D4)),
              (1, g(-0x38D0), g(-0x38CC)), (3, g(-0x38C8), g(-0x38F0)), (1, -1.0, g(-0x38C4))]
    colours = [(g(-0x3888), 1, 1, 0), (g(-0x3884), 1, 1, 0), (g(-0x3880), g(-0x387C), g(-0x3878), g(-0x3890)),
               (g(-0x3874), g(-0x3870), g(-0x386C), 0), (g(-0x3868), 1, 1, 0), (g(-0x3864), g(-0x3860), g(-0x385C), g(-0x388C)),
               (g(-0x3858), g(-0x3854), 1, 1), (g(-0x3850), g(-0x384C), g(-0x3840), 0), (g(-0x383C), g(-0x3838), g(-0x3834), g(-0x3848))]
    return [dict(quadrant=q, position=t, size=s, argb=list(c)) for (q, t, s), c in zip(layout, colours)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    from disc_paths import ps2_iso;p.add_argument('--ps2-iso', type=Path, default=ps2_iso())
    p.add_argument('--location', default='ARA1')
    p.add_argument('--output', type=Path, help='Default: web/public/assets/SUN_FLARE (ARA1) or web/public/assets/<X>/sun-flare')
    a = p.parse_args()
    if a.output is None:
        a.output = ROOT / 'web/public/assets/SUN_FLARE' if a.location == 'ARA1' else ROOT / 'web/public/assets' / a.location / 'sun-flare'
    painter = sun_painter(ROOT, a.location)
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest() != EXPECTED_SHA1:
        raise ValueError('Unexpected original executable')
    table = lambda i: elf[0x4891B0 + 12 * i - ELF_BASE:0x4891B0 + 12 * i - ELF_BASE + 4]
    names = {45: 'lens', 51: 'sun1', 52: 'sun2'}
    for i, n in names.items():
        if table(i) != n.encode():
            raise ValueError(f'FX texture table mismatch at {i}')
    a.output.mkdir(parents=True, exist_ok=True)
    disc = Disc(a.ps2_iso)
    try:
        container = disc.file('DATA/TEXTURES/EFFECTS.SSH')
    finally:
        disc.close()
    textures = {}
    for i, n in names.items():
        offset, data = member(container, n)
        width, height, raw = decode_clut8(data, n)
        (a.output / f'{n}.gs.rgba').write_bytes(raw)
        textures[n] = dict(fx_index=i, width=width, height=height, file=f'{n}.gs.rgba', source_offset=offset,
                           sha256=hashlib.sha256(raw).hexdigest(), alpha='raw GS 0..128')
    constants = dict(
        default_azimuth_rad=gp_f32(elf, -0x38BC), default_elevation_rad=gp_f32(elf, -0x38B8),
        degrees_to_radians=gp_f32(elf, -0x38C0), far_inset_cm=500.0, default_glow_half_size=320.0,
        flare_size_scale=300.0, query_size=16, viewport=[512, 448])
    if abs(constants['degrees_to_radians'] - 0.017453292) > 1e-7:
        raise ValueError('Unexpected degree constant')
    package = dict(version=1, painter=painter, flares=flare_elements(elf), textures=textures,
                   sun_textures=['sun1', 'sun2'], constants=constants,
                   container_sha256=hashlib.sha256(container).hexdigest(),
                   defaults=dict(elevation=0.0, azimuth=0.0, colour=[1, 1, 1], glow_alpha=1.0, texture=0,
                                 flare_alpha=1.0, size=1.0))
    (a.output / 'sun-flare.json').write_text(json.dumps(package, indent=1) + '\n')
    if painter is None:
        print(f'No Sun painter section in {a.location}: painter=null')
    else:
        print('Sun painter payloads', len(painter['payloads']), painter['payloads'][0])


if __name__ == '__main__':
    main()
