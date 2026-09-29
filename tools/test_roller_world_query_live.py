#!/usr/bin/env python3
"""Live RollerModifier world-query check (engine/roller_world_query.hpp).

Runs the complete original 0x3303F0 -> 0x336850(0x2D1BE0()) -> 0x3304E8 (and
0x335D78 for the full packet list) from carve-bag savestates against the port
on the browser's native world (world_collision.json + terrain.json): the two
roller colliders as saved, the recorded roller trajectory and random poses set
through the original 0x32C648. Log: local/reference/roller-world-query/live.log.
"""
import subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import vtable_targets  # noqa: E402
from test_roller_world_query_native import FOLDER, build_live, savestate  # noqa: E402  (PCSX2 zero policy)

TICKS = [607, 612, 621, 642, 682, 738]


def main():
    states = []
    for tick in TICKS:
        ee, vuc, vud = savestate(FOLDER / 'states', ROOT / f'local/ps2-capture/runs/bag/carve-bag.tick{tick}.p2s', str(tick))
        states += [str(ee), str(vuc), str(vud), str(tick)]
    roots = [0x3303F0, 0x336850, 0x335D78, 0x3304E8, 0x2D1BE0, 0x32C648, 0x3A6CC8, 0x3A6D00]
    roots += vtable_targets([(0x48E5F0, 0x80), (0x490E80, 0x1A0), (0x48F080, 0xF8)])
    binary = ROOT / 'build/ssx3_roller_world_query_live'
    need = build_live(roots, ROOT / 'tests/roller_world_query_live.cpp', binary, FOLDER / 'live-oracle',
                      extra_sources=[ROOT / 'tests/roller_world_query_live_world.mm', ROOT / 'engine/world_collision_asset.mm'], cache=ROOT / 'build/roller-world-query-live-objects')
    run = subprocess.run([str(binary), str(ROOT / 'local/assets/native/ARA1'), str(ROOT / 'local/ps2-capture/runs/bag/carve-bag.bin')] + states,
                         capture_output=True, text=True, timeout=7200, env={'COUNTS': '1'})
    (FOLDER / 'live.log').write_text(f'{len(need)} original entries linked\n' + run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr[-4000:], end='')
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
