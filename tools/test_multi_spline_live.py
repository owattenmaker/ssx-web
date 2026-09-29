#!/usr/bin/env python3
"""Live MultiSplineModifier (Snow Jam chairlift) parity (tests/multi_spline_live.cpp): lockstep of
engine/multi_spline_modifier.hpp against the full original 0x35A560 + LiveComp 0x3568B0 from the
snow-jam-ready savestate (race tick 0), the setpieces/race capture watch windows, and randomized
0x345248 / 0x35AC20 / 0x35B200 cases. Also checks that the browser spline catalog
(web/public/assets/ARA1/rails.json: coefficients, row50, distance, length) is bit-identical to the
runtime segments of both gondola paths. Log: local/reference/multi-spline/live.log."""
import json, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/multi-spline'
STATE = ROOT / 'local/reference/pcsx2/snow-jam-ready.p2s'
CAPTURE = ROOT / 'local/ps2-capture/runs/setpieces/race.bin'


def check_catalog(ee):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    rails = {r['packed_id']: r for r in json.loads((ROOT / 'web/public/assets/ARA1/rails.json').read_text())['rails']}
    f32 = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
    checked = 0
    for modifier in (0x592E80, 0x593580):
        seg = u(modifier + 0x50)
        while u(seg + 0x60): seg = u(seg + 0x60)
        record = rails[u(modifier + 0x48)]
        for part in record['segments']:
            src = part['source']
            words = [f32(part['length_cm'])] + [f32(v) for row in src['coefficients'] for v in row] + [f32(v) for v in src['row50']] + [f32(part['distance_cm'])]
            mem = [u(seg + 0xC)] + [u(seg + 0x10 + 16 * r + 4 * k) for r in range(4) for k in range(3)] + [u(seg + 0x50 + 4 * k) for k in range(4)] + [u(seg + 0x84)]
            if words != mem: raise SystemExit(f'rails.json segment {part["index"]} of {record["name"]} differs from the runtime segment')
            seg = u(seg + 0x64); checked += 1
        if seg: raise SystemExit('runtime chain longer than the catalog record')
    return checked


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(STATE) as z:
        ee = z.read('eeMemory.bin')
        for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
            (FOLDER / f'ready.{suffix}').write_bytes(z.read(member))
    segments = check_catalog(ee)
    roots = [0x35A560, 0x3568B0, 0x35AC20, 0x345248, 0x35B200]
    roots += vtable_targets([(0x48F168, 0x110), (0x490B10, 0x1A0)])
    binary = ROOT / 'build/ssx3_multi_spline_live'
    need = build_live(roots, ROOT / 'tests/multi_spline_live.cpp', binary, FOLDER / 'live-oracle', cache=ROOT / 'build/multi-spline-live-objects')
    ticks = sys.argv[1] if len(sys.argv) > 1 else '2400'
    cases = sys.argv[2] if len(sys.argv) > 2 else '20000'
    run = subprocess.run([str(binary), str(FOLDER), str(CAPTURE), ticks, cases], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked; rails.json matches {segments} runtime gondola segments bit for bit\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log)
    print(log, end='')
    if run.returncode: raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
