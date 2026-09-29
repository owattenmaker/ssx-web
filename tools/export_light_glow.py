#!/usr/bin/env python3
"""Export the original SSX 3 light glow (halo) sources for the browser.

Recovered from SLUS_207.72 (see docs/terrain-render-fidelity.md, "Light glow halos"):
- Sources are world records kind 7 (80 bytes; runtime entity type 8 in the
  world-cell entity lists): +0C flags (& 0x70 selects the halo class), +10 RGB,
  +1C position (cm, Z up), +28..+3F bounds, +40 per-view visibility (runtime).
- 22A4A8/22A770 collect type-8 entities of the visible cells into
  +7BC0/+7BC4; 22C790 rebuilds the per-view glow list (2E2F98 clear,
  2E2FF8 -> 2E2FA8 add) and runs 2E30D0 -> 2E3338 (update 2E2B00 per record)
  and 2E3478 (draw 2E2868 per record, material 2E3578: ALPHA_1 enum 7 = GS 0x48,
  priority 7, ATST ALWAYS).
- 2E2B00: outcode test 37DBE8, projection 37DD20 (integer pixels), zf = Z*2^-24;
  zf <= 0.005 -> far (drawn depth-tested GEQUAL at the light, visibility 1);
  else count rect clamp(trunc(zf*800),1,16) x clamp(trunc(zf*400),1,8) centred,
  16x8 read rect at (x-8, y-4) clamped, reference Z of the point pulled toward the
  camera by 100/80/200 cm (class 0x10/0x20/0x40), rotation
  ((x - vx)*2/vw - 1)*pi/2.
- 2E3130 -> 2EC478: Z readback, vis = open/(w*h) or max((open - wh/2)/(wh/2), 0).
- 2E2868: FX 53 'shal' (class 0x10/0x40) or 54 'mhal' (0x20), half size 180/100/350
  cm, vertex RGB trunc(128*normalize(colour)), A trunc(128*vis); renderer 3781A0
  draws the rotated camera-facing quad twice (t2 = 2).

Writes private assets to web/public/assets/LIGHT_GLOW (git-ignored); with
`--location X` (X != ARA1) to web/public/assets/X/light-glow/. Sources are the kind-7
records of the event residency (world_assets.event_locations). A location without any
is written explicitly as `lights: []` (the browser should skip the pass).
"""
import argparse, hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc, EXPECTED_SHA1  # noqa: E402
from export_sun_flare import member, decode_clut8, gp_f32, ELF_BASE  # noqa: E402


def word(elf, va):
    return struct.unpack_from('<I', elf, va - ELF_BASE)[0]


def check_code(elf):
    # Immediates of 2E2B00/2E2868 that the browser port hard-codes.
    expect = {
        0x2E2BB8: 0x3C014448,  # lui at,0x4448 -> 800.0 (count width scale)
        0x2E2BC4: 0x3C0143C8,  # lui at,0x43C8 -> 400.0 (count height scale)
        0x2E2BC0: 0x24030010,  # addiu v1,zero,16 (max width)
        0x2E2C04: 0x24020008,  # addiu v0,zero,8 (max height)
        0x2E2D28: 0x3C0142C8,  # 100.0 cm pull, class 0x10
        0x2E2D38: 0x3C0142A0,  # 80.0 cm pull, class 0x20
        0x2E2D48: 0x3C014348,  # 200.0 cm pull, class 0x40
        0x2E2A50: 0x3C014334,  # 180.0 half size, class 0x10
        0x2E2A64: 0x3C0142C8,  # 100.0 half size, class 0x20
        0x2E2A78: 0x3C0143AF,  # 350.0 half size, class 0x40
        0x2E2AF0: 0x240A0002,  # t2 = 2: the quad is emitted twice (3781A0 loop)
        0x2E35C4: 0x240BFF83,  # ALPHA_1 mask; 0x2E3648 ori 0x1C -> enum 7 (0x48)
        0x2E365C: 0x344200E0,  # word2 priority 7
    }
    for va, value in expect.items():
        if word(elf, va) != value:
            raise ValueError(f'Unexpected light glow code at {va:#x}: {word(elf, va):#010x}')


def glow_sources(root, location='ARA1'):
    from world_assets import world_chunks, records, locations, event_locations
    folder = root / 'local/assets/source/ps2'
    # Race-event residency: the kind-7 records of ARA1 and its connectors are all type-8
    # entities of the event octree (128 ARA1 + 5 ARA1_B, tools/export_event_membership.py).
    resident = [(b, e) for _, _, b, e in event_locations(locations(folder / 'bam.sdb'), location)]
    lights = []
    for chunk_index, chunk in enumerate(world_chunks(folder / 'bam.ssb')):
        if chunk_index > max(e for _, e in resident):
            break
        if not any(b <= chunk_index <= e for b, e in resident):
            continue
        for kind, track, rid, data in records(chunk):
            if kind != 7:
                continue
            if len(data) != 80:
                raise ValueError('Unexpected light glow record extent')
            flags = struct.unpack_from('<I', data, 12)[0]
            if flags & 0x70 not in (0x10, 0x20, 0x40) or flags & ~0x70:
                raise ValueError(f'Unsupported light glow flags {flags:#x}')
            lights.append(dict(chunk=chunk_index, track=track, rid=rid, flags=flags,
                               colour=list(struct.unpack_from('<3f', data, 16)),
                               position=list(struct.unpack_from('<3f', data, 28)),
                               bounds_min=list(struct.unpack_from('<3f', data, 40)),
                               bounds_max=list(struct.unpack_from('<3f', data, 52))))
    return lights


def main():
    p = argparse.ArgumentParser(description=__doc__)
    from disc_paths import ps2_iso;p.add_argument('--ps2-iso', type=Path, default=ps2_iso())
    p.add_argument('--location', default='ARA1')
    p.add_argument('--output', type=Path, help='Default: web/public/assets/LIGHT_GLOW (ARA1) or web/public/assets/<X>/light-glow')
    a = p.parse_args()
    if a.output is None:
        a.output = ROOT / 'web/public/assets/LIGHT_GLOW' if a.location == 'ARA1' else ROOT / 'web/public/assets' / a.location / 'light-glow'
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest() != EXPECTED_SHA1:
        raise ValueError('Unexpected original executable')
    check_code(elf)
    names = {53: 'shal', 54: 'mhal'}
    for i, n in names.items():
        at = 0x4891B0 + 12 * i - ELF_BASE
        if elf[at:at + 4] != n.encode():
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
        z_scale=gp_f32(elf, -0x3A40), far_threshold=gp_f32(elf, -0x3A3C), rotation_scale=gp_f32(elf, -0x3A38),
        count_scale=[800.0, 400.0], count_max=[16, 8], read_size=[16, 8], copies=2, viewport=[512, 448],
        classes={'16': dict(texture='shal', half_size=180.0, pull_cm=100.0),
                 '32': dict(texture='mhal', half_size=100.0, pull_cm=80.0),
                 '64': dict(texture='shal', half_size=350.0, pull_cm=200.0)})
    if abs(constants['far_threshold'] - 0.005) > 1e-9 or abs(constants['rotation_scale'] - 1.5707963) > 1e-6:
        raise ValueError('Unexpected light glow constants')
    lights = glow_sources(ROOT, a.location)
    package = dict(version=1, location=a.location, lights=lights, textures=textures, constants=constants,
                   container_sha256=hashlib.sha256(container).hexdigest())
    (a.output / 'light-glow.json').write_text(json.dumps(package, indent=1) + '\n')
    print('Light glow sources', len(lights), {c: sum(1 for l in lights if l['flags'] == c) for c in (16, 32, 64)})


if __name__ == '__main__':
    main()
