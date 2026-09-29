#!/usr/bin/env python3
"""Live avalanche playback parity (tests/avalanche_live.cpp; development oracle, docs/avalanche.md).

engine/avalanche.hpp against the full recompiled original on PS2 savestates of Much 2 Much (EBA3, avalanche 45 playing
from ~540 to ~2819) and Gravitude (ERA5, avalanche 28 at 2019 / 2418): the running slots stepped by 0x2D7EF8 and every
avalanche triggered by 0x2D97A8, tumblers / slots / 0x2D9C00 matrices compared bit for bit per tick.
usage: test_avalanche_live.py [TICKS]   Log: local/reference/avalanche/live.log"""
import subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/avalanche'
RUNS = ROOT / 'local/ps2-capture/runs'
# playing: EBA3 820..2419, ERA5 2019 / 2418; not playing (the trigger runs): EBA3 420 (before), 2819 (after), ERA5 1619, 2818
STATES = [('peak3/much-2-much-full', t) for t in (420, 820, 1219, 1620, 2019, 2419, 2819)] + [('peak3/gravitude-full', t) for t in (1619, 2019, 2418, 2818)]


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    names = []
    for run, tick in STATES:
        path = RUNS / f'{run}.tick{tick}.p2s'
        if not path.exists():
            continue
        name = f'{Path(run).name}{tick}'
        with zipfile.ZipFile(path) as z:
            for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
                (FOLDER / f'{name}.{suffix}').write_bytes(z.read(member))
        names.append(name)
    roots = [0x2D7EF8, 0x2D97A8, 0x2D9C00, 0x3B5860, 0x2C1640, 0x2C1648, 0x2C1650]  # jalr targets: a sound callback, the painter ambient getters (0x2EE7C8..)
    roots += vtable_targets([(0x490E80, 0x1D0), (0x490B10, 0x1D0), (0x4906F0, 0x1D0), (0x491680, 0x1D0), (0x491B00, 0x1D0), (0x491220, 0x30), (0x483648, 0x40), (0x483688, 0x40), (0x4930D0, 0x80), (0x484058, 0x80)])
    binary = ROOT / 'build/ssx3_avalanche_live'
    need = build_live(roots, ROOT / 'tests/avalanche_live.cpp', binary, FOLDER / 'live-oracle', cache=ROOT / 'build/avalanche-live-objects')
    ticks = sys.argv[1] if len(sys.argv) > 1 else '400'
    run = subprocess.run([str(binary), str(FOLDER), ticks] + names, capture_output=True, text=True, timeout=7200)
    log = f'{len(need)} original entries linked; states {", ".join(names)}\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
