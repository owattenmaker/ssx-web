#!/usr/bin/env python3
"""Live RollerModifier parity (tests/roller_modifier_live.cpp): 130-tick lockstep
of the port (world query = original 0x336850 on the live machine) against the
full original 0x35E850 + 0x3568B0 from the carve-bag tick-607 savestate, the
capture records, and constructions from the tick-600 savestate.
Log: local/reference/roller-modifier/live.log."""
import subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/roller-modifier'
RUN = ROOT / 'local/ps2-capture/runs/bag'


def savestate(name):
    FOLDER.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(RUN / f'carve-bag.tick{name}.p2s') as z:
        for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
            (FOLDER / f'{name}.{suffix}').write_bytes(z.read(member))


def main():
    for name in ['600', '607']:
        savestate(name)
    roots = [0x35E850, 0x3568B0, 0x3303F0, 0x2D1BE0, 0x336850, 0x3304E8, 0x3291E0, 0x35DA70, 0x350570]
    roots += vtable_targets([(0x48E5F0, 0x80), (0x490E80, 0x1A0), (0x48F080, 0xF8)])
    binary = ROOT / 'build/ssx3_roller_modifier_live'
    need = build_live(roots, ROOT / 'tests/roller_modifier_live.cpp', binary, FOLDER / 'live-oracle',
                      cache=ROOT / 'build/roller-modifier-live-objects')
    ticks = sys.argv[1] if len(sys.argv) > 1 else '130'
    run = subprocess.run([str(binary), str(FOLDER), str(RUN / 'carve-bag.bin'), ticks], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
