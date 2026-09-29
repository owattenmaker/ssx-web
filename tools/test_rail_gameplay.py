#!/usr/bin/env python3
"""Exercise the original rail grind lifecycle through the actual gameplay session.

The rider is placed on the snow just before an authored ARA1 rail, facing along
it. The 0x106848 attach must enter control 7 / motion 4, 0x13AF28 must keep the
rider on the rail with the balance cycle playing, and leaving the rail must hand
back to the airborne controllers and land into ordinary riding.
"""
import copy
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
area = root / 'local/assets/native/ARA1'
start = area / 'riding-start.json'
if not start.exists() or not (area / 'rails.json').exists():
    sys.exit(77)
initial = json.loads(start.read_text())
rails = {r['name']: r for r in json.loads((area / 'rails.json').read_text())['rails']}
rail = rails['spline_ARA1_RAIL_3007']
seg = rail['segments'][0]['native']
p0, p1 = seg['start'], seg['end']
direction = [p1[k] - p0[k] for k in range(3)]
length = math.hypot(direction[0], direction[2])
direction = [d / length for d in direction]
# Terrain height a few metres before the rail start, from the probe used while
# integrating (RAIL_3007 starts flush with the snow at -4132.1 m).
terrain_y = -4132.04
start_native = [p0[0] - direction[0] * 3.5, terrain_y + 0.05, p0[2] - direction[2] * 3.5]


def rotz(v, a):
    c, s = math.cos(a), math.sin(a)
    return [c * v[0] - s * v[1], s * v[0] + c * v[1], v[2]]


def qmul(a, b):
    x1, y1, z1, w1 = a
    x2, y2, z2, w2 = b
    return [w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
            w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2]


with tempfile.TemporaryDirectory(prefix='ssx-rail-') as folder:
    folder = Path(folder)
    scenario = copy.deepcopy(initial)
    state = scenario['native']['initial']['original_ground']['state']
    source_dir = [direction[0], -direction[2], 0.0]
    h = math.hypot(source_dir[0], source_dir[1])
    source_dir = [source_dir[0] / h, source_dir[1] / h, 0.0]
    forward = state['physical_forward']
    angle = math.atan2(source_dir[1], source_dir[0]) - math.atan2(forward[1], forward[0])
    state['quaternion'] = qmul([0, 0, math.sin(angle / 2), math.cos(angle / 2)], state['quaternion'])
    for key in ('normal', 'previous_normal', 'board_normal', 'presentation_up', 'board_up', 'forward', 'lateral', 'physical_forward'):
        state[key] = rotz(state[key], angle)
    speed = 900.0
    state['position'] = [start_native[0] * 100, -start_native[2] * 100, start_native[1] * 100]
    state['velocity'] = [source_dir[0] * speed, source_dir[1] * speed, 0.0]
    state['heading_offset'] = 0
    scenario['native']['initial']['position'] = start_native
    scenario['native']['initial']['velocity'] = [direction[0] * speed / 100, 0, direction[2] * speed / 100]
    scenario['native']['initial']['heading'] = math.atan2(direction[0], direction[2])
    # With the original 0x13AF28 entry pull (unclamped offset x -5 imbalance) the rider
    # stays on this nearly flat rail for about 650 frames before it leaves (was < 300).
    scenario['frames'] = 900
    scenario['events'] = []
    path = folder / 'rail.json'
    path.write_text(json.dumps(scenario))
    output = folder / 'rail-result'
    result = subprocess.run([sys.argv[1], '--replay', str(path), '--telemetry', str(output)], capture_output=True, text=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    rows = json.loads(output.with_suffix('.json').read_text())['records']
    assert all(not r['body_collision']['missing_pose'] for r in rows[1:]), 'Missing gameplay pose'
    grinding = [r['frame'] for r in rows if r['control_state'] == 7 and r['motion_mode'] == 4]
    assert grinding, 'The rider never attached to the rail'
    first = grinding[0]
    assert first < 10, f'Rail attach happened late at frame {first}'
    assert len(grinding) >= 60, f'Grind lasted only {len(grinding)} frames'
    on_rail = [r for r in rows if r['control_state'] == 7 and r['motion_mode'] == 4]
    assert sum(1 for r in on_rail if r['original_ground']['animation_index'] in (18, 19, 20)) > len(on_rail) * 0.8, 'Rail cycle animation not playing while grinding'
    travelled = math.dist(on_rail[0]['position'], on_rail[-1]['position'])
    assert travelled > 10, f'Rider only moved {travelled:.1f} m along the rail'
    left = next(r['frame'] for r in rows if r['frame'] > first and r['motion_mode'] == 1)
    landed = [r for r in rows if r['frame'] > left and r['control_state'] == 0 and r['grounded']]
    assert landed, 'Rider never landed back into ordinary riding after the rail'
    assert all(not r['reset_requested'] for r in rows), 'Rail run requested a reset'
print(f'Rail attach at frame {first}, {len(grinding)} grinding frames over {travelled:.1f} m, left at {left}, landed at {landed[0]["frame"]}; poses complete')
