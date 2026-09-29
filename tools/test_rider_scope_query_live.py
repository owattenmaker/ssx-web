#!/usr/bin/env python3
"""Live rider landing/body queries on the connector terrain through a refreshed rider scope.

The Snow Jam race event holds the A_ARA1 (track 3) and ARA1_B (track 9) patches besides ARA1
(tools/export_event_membership.py). Rider queries do not walk the octree: 0x3342D0/0x13A7B0 read
the rider's query scope (rider+0x860), which 0x120E50 rebuilds from the rider query bounds
(rider+0x400/+0x410) with 0x332DB8. This oracle moves the captured rider query (board root, body
shape and bounds) onto points over and next to the connector patches
(tools/terrain_probe_targets.py), runs the original scope refresh and the complete original
landing probe 0x13A7B0 and body query 0x32F650 -> 0x3342D0, and compares them with the native
originalLandingContact / WorldBodyCollision::query on local/assets/native/ARA1.
Log: local/reference/terrain/rider-scope/live.log.
"""
import struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import vtable_targets  # noqa: E402
from test_roller_world_query_native import build_live, savestate  # noqa: E402  (PCSX2 zero policy)
from terrain_probe_targets import targets  # noqa: E402

FOLDER = ROOT / 'local/reference/terrain/rider-scope'
# Query-class callbacks (0x48E590..0x48E700: body shape and body/landing query vtables incl. 0x32F8F0, 0x340A10) and entity callbacks the scope collectors reach: the roller live test's classes plus the collision
# predicates (vtable+0x160/+0x168, function words at +0x164/+0x16C) of the other event entity classes.
# Whole vtables would link most of the game (hours of compilation); a missing target throws.
VTABLES = [(0x48E590, 0x60), (0x48E5F0, 0x110), (0x490E80, 0x1A0), (0x48F080, 0xF8)] + [(v + 0x160, 0x10) for v in
           (0x490B10, 0x491B00, 0x491680, 0x491800, 0x48FC10, 0x48EE60)]


def main():
    FOLDER.mkdir(parents=True, exist_ok=True)
    points = targets()
    (FOLDER / 'targets.bin').write_bytes(b''.join(struct.pack('<3f', *p) for p in points))
    states = []
    for name in ['glide', 'glide-120']:
        states += [str(p) for p in savestate(FOLDER / 'states', ROOT / f'local/reference/pcsx2/snow-jam-{name}.p2s', name)]
    roots = [0x332DB8, 0x13A7B0, 0x32F650, 0x3342D0] + vtable_targets(VTABLES)
    binary = ROOT / 'build/ssx3_rider_scope_query_live'
    need = build_live(roots, ROOT / 'tests/rider_scope_query_live.cpp', binary, FOLDER / 'live-oracle',
                      extra_sources=[ROOT / 'tests/roller_world_query_live_world.mm', ROOT / 'tests/rider_scope_query_live_world.mm',
                                     ROOT / 'engine/world_collision_asset.mm'], cache=ROOT / 'build/rider-scope-live-objects')
    run = subprocess.run([str(binary), str(ROOT / 'local/assets/native/ARA1'), str(FOLDER / 'targets.bin')] + states,
                         capture_output=True, text=True, timeout=7200)
    (FOLDER / 'live.log').write_text(f'{len(need)} original entries linked, {len(points)} targets\n' + run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        print(run.stderr[-4000:], end='')
        raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
