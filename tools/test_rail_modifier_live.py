#!/usr/bin/env python3
"""Live AnimTeeter / RailModifier parity (tests/rail_modifier_live.cpp) on the race-capture
savestates local/ps2-capture/runs/setpieces/race.tick*.p2s: original builtin6/builtin48
construction before each log's section activation vs the port (seeded only from
web/generated/rail_teeter_seed.hpp) and vs the game's own objects, then a lockstep of random
rider forces (0x342538), the full entity update 0x356198 and 0x35C698 rail queries.
Log: local/reference/rail-modifier/live.log."""
import subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
FOLDER = ROOT / 'local/reference/rail-modifier'
RUN = ROOT / 'local/ps2-capture/runs/setpieces'


def savestate(name):
    FOLDER.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(RUN / f'race.tick{name}.p2s') as z:
        for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
            (FOLDER / f'{name}.{suffix}').write_bytes(z.read(member))


def main():
    for name in ['319', '718', '1518', '1919', '4719', '5118', '7919']:
        savestate(name)
    roots = [0x2FB498, 0x355E38, 0x356198, 0x342538, 0x35C698, 0x35C5A0, 0x3610E0, 0x2D1BD8]
    roots += vtable_targets([(0x4908F8, 0x1B0), (0x490AD0, 0x40), (0x4911D0, 0x50), (0x491200, 0x18)])
    binary = ROOT / 'build/ssx3_rail_modifier_live'
    need = build_live(roots, ROOT / 'tests/rail_modifier_live.cpp', binary, FOLDER / 'live-oracle',
                      cache=ROOT / 'build/rail-modifier-live-objects')
    ticks = sys.argv[1] if len(sys.argv) > 1 else '600'
    run = subprocess.run([str(binary), str(FOLDER), ticks], capture_output=True, text=True, timeout=3600)
    log = f'{len(need)} original entries linked\n' + run.stdout + run.stderr
    (FOLDER / 'live.log').write_text(log)
    print(log, end='')
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
