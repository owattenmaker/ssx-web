#!/usr/bin/env python3
"""Rider query scope on the landing probe: original 0x332DB8 + 0x13A7B0 against the browser's scoped query.

0x3342D0 walks the rider's query scope (rider+0x860), which 0x120E50 rebuilds with 0x332DB8 from the rider
query bounds (rider+0x400/+0x410) every third game tick; an inverted rider's landing probe reaches 200cm past
the board root on the head side, outside those bounds (pipe-tricks: the browser landed at record 1361, the PS2
at 1363). This oracle loads the pipe-tricks savestate (The Junction, rider 0x14542A0) and, with captured board
roots, presentation up vectors and query bounds from local/ps2-capture/runs/pipe-tricks.bin, checks
(1) the 332DB8 terrain and static-instance lists against terrain_original::RiderScope::admits,
(2) the complete original landing probe through a lagged scope against originalLandingContact(..., &scope),
(3) the captured inverted landing (miss at 1361/1362, contact at 1363).
Log: local/reference/terrain/rider-scope-landing/live.log.
"""
import subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import vtable_targets  # noqa: E402
from test_roller_world_query_native import build_live, savestate  # noqa: E402  (PCSX2 zero policy)

FOLDER = ROOT / 'local/reference/terrain/rider-scope-landing'
CAPTURE = ROOT / 'local/ps2-capture/runs/pipe-tricks'
# Same query-class and collector callbacks as tools/test_rider_scope_query_live.py.
VTABLES = [(0x48E590, 0x60), (0x48E5F0, 0x110), (0x490E80, 0x1A0), (0x48F080, 0xF8)] + [(v + 0x160, 0x10) for v in
           (0x490B10, 0x491B00, 0x491680, 0x491800, 0x48FC10, 0x48EE60)]


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    states = [str(p) for p in savestate(FOLDER / 'states', CAPTURE.with_suffix('.p2s'), 'pipe-tricks')]
    roots = [0x332DB8, 0x13A7B0, 0x32F650, 0x3342D0] + vtable_targets(VTABLES)
    binary = ROOT / 'build/ssx3_rider_scope_landing_live'
    need = build_live(roots, ROOT / 'tests/rider_scope_landing_live.cpp', binary, FOLDER / 'live-oracle',
                      extra_sources=[ROOT / 'tests/roller_world_query_live_world.mm', ROOT / 'tests/rider_scope_query_live_world.mm',
                                     ROOT / 'engine/world_collision_asset.mm'], cache=ROOT / 'build/rider-scope-live-objects')
    run = subprocess.run([str(binary), str(ROOT / 'local/assets/native/BHP1'), str(CAPTURE.with_suffix('.bin'))] + states,
                         capture_output=True, text=True, timeout=7200)
    (FOLDER / 'live.log').write_text(f'{len(need)} original entries linked\n' + run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr[-4000:], end='')
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
