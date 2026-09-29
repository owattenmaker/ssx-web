#!/usr/bin/env python3
"""SplineModifier conformance (tests/spline_modifier_reference.cpp): randomized
instruction-level oracles of the recompiled originals 0x359698 update, 0x359830
evaluate (with the real 0x345248/0x345048 on the runtime kind-8 records), 0x361B90,
0x359CF8 contact velocity, 0x359EB8 messages, 0x359460 constructor (real 0x3451C0 /
0x3454E8 bind through the world resource table, 0x317830 shared RNG) and the
PositionModifier 0x356F10/0x356FF0/0x361940, against engine/spline_modifier.hpp, on
the setpieces race tick-4319 savestate image. Log: local/reference/spline-modifier/reference.log."""
import json, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/spline-modifier'
STATE = ROOT / 'local/ps2-capture/runs/setpieces/race.tick4319.p2s'


def check_catalog():
    # The browser's spline catalog (rails.json -> web/rail_bridge.cpp browserRailRecord) must equal
    # the runtime segments the original binds (length, cubic rows, row50, start distance).
    words = struct.unpack(f'<{(FOLDER / "runtime-paths.bin").stat().st_size // 4}I', (FOLDER / 'runtime-paths.bin').read_bytes())
    rails = {r['packed_id']: r for r in json.loads((ROOT / 'web/public/assets/ARA1/rails.json').read_text())['rails']}
    f32 = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
    at, checked = 0, 0
    while at < len(words):
        resource, count = words[at], words[at + 1]; at += 2
        record = rails[resource]
        if len(record['segments']) != count:
            raise SystemExit(f'rails.json {record["name"]}: {len(record["segments"])} segments, runtime {count}')
        for part in record['segments']:
            src = part['source']
            expected = [f32(part['length_cm'])] + [f32(v) for row in src['coefficients'] for v in row] + [f32(v) for v in src['row50']] + [f32(part['distance_cm'])]
            if list(words[at:at + 18]) != expected:
                raise SystemExit(f'rails.json segment {part["index"]} of {record["name"]} differs from the runtime segment')
            at += 18; checked += 1
    return f'rails.json matches all {checked} runtime segments of the bound paths bit for bit'


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(STATE) as z:
        for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
            (FOLDER / f'4319.{suffix}').write_bytes(z.read(member))
    roots = [0x359460, 0x359698, 0x359830, 0x361B90, 0x359CF8, 0x359EB8, 0x356F10, 0x356FF0, 0x361940, 0x3451C0, 0x345248]
    roots += vtable_targets([(0x48F250, 0xE8), (0x48F5F0, 0xE8)])
    binary = ROOT / 'build/ssx3_spline_modifier_reference'
    need = build_live(roots, ROOT / 'tests/spline_modifier_reference.cpp', binary, FOLDER / 'reference-oracle',
                      cache=ROOT / 'build/spline-modifier-reference-objects')
    cases = sys.argv[1] if len(sys.argv) > 1 else '20000'
    run = subprocess.run([str(binary), str(FOLDER), cases], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked\n' + run.stdout + run.stderr
    if run.returncode == 0:
        log += check_catalog() + '\n'
    (FOLDER / 'reference.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
