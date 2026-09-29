#!/usr/bin/env python3
"""Live SplineModifier parity (tests/spline_modifier_live.cpp): the raven modifier in flight
at race tick 4319 stepped by the full original entity update 0x356198 + 0x3568B0 against
engine/spline_modifier.hpp (and compared with the PS2 savestates 4719/5118/5518), plus the
real stage programs (rockets 120 from tick 718, spintwins 131 and raven 54 from 3519, dragons
136/137 from 4319) launched through the original script dispatchers and stepped through the
path end (slot-4 builtin16 conversion to a PositionModifier). The pieces' arguments are
checked against local/event-activation/spline-setpieces.json (tools/export_spline_setpieces.py).
Log: local/reference/spline-modifier/live.log."""
import json, re, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/spline-modifier'
RUN = ROOT / 'local/ps2-capture/runs/setpieces'


def check_export():
    source = (ROOT / 'tests/spline_modifier_live.cpp').read_text()
    pieces = {p['instance']: p for p in json.loads((ROOT / 'local/event-activation/spline-setpieces.json').read_text())['pieces']}
    found = re.findall(r'\{"(mdl_ARA1_\w+)",0x([0-9A-F]+),0x([0-9A-F]+),(\d+),([-\d.]+)f,([-\d.]+)f\}', source)
    for name, _instance, spline, end, kmh, roll in found:
        r = pieces[name]['resolved']
        if (r['spline'], r['end_mode'], r['speed_kmh'], r['roll_degrees']) != (int(spline, 16), int(end), float(kmh), float(roll)):
            raise SystemExit(f'{name}: live test arguments differ from the export {r}')
    return len(found)


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    checked = check_export()
    for tick in ('718', '3519', '4319', '4719', '5118', '5518'):
        with zipfile.ZipFile(RUN / f'race.tick{tick}.p2s') as z:
            for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
                (FOLDER / f'{tick}.{suffix}').write_bytes(z.read(member))
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    builtins = sorted({struct.unpack_from('<I', elf, 0x441F38 + 4 * i - 0xFF000)[0] for i in range(111)} - {0})
    ee = (FOLDER / '718.ee').read_bytes()
    interface = struct.unpack_from('<I', ee, 0x14701A0 + 0x6C0)[0]
    roots = [0x30A060, 0x30A298, 0x356198, 0x3568B0, 0x3451C0] + [b for b in builtins if 0x100000 <= b < 0x440000]
    roots += vtable_targets([(0x48F250, 0xE8), (0x48F5F0, 0xE8), (0x490E80, 0x1D0), (0x490B10, 0x1D0), (0x48EE60, 0x1D0),
                             (0x491268, 0xE8), (0x491370, 0xE8), (0x4912B0, 0xE8), (0x491680, 0x1D0), (0x4906F0, 0x1D0), (interface, 0x200),
                             (0x483648, 0x40), (0x483688, 0x40)])  # sound/stream objects reached from the particle updates
    binary = ROOT / 'build/ssx3_spline_modifier_live'
    need = build_live(roots, ROOT / 'tests/spline_modifier_live.cpp', binary, FOLDER / 'live-oracle',
                      cache=ROOT / 'build/spline-modifier-live-objects')
    run = subprocess.run([str(binary), str(FOLDER)], capture_output=True, text=True, timeout=7200)
    log = f'{len(need)} original entries linked; {checked} live pieces match the exported arguments\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
