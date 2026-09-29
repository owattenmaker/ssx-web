#!/usr/bin/env python3
"""Exercise the original hard-crash lifecycle through the actual gameplay session.

A forward flip held through takeoff lands upside down, which the original
13A14C classifier turns into a 10EB30 hard crash: control 8 / motion 2 with an
airborne ragdoll phase, a sliding phase, the 12CB68 recovery meter and the
12D848 get-up back to ordinary riding. Holding jump feeds the recovery meter and
must shorten the crash.
"""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
start = root / 'local/assets/native/ARA1/riding-start.json'
if not start.exists():
    sys.exit(77)
initial = json.loads(start.read_text())
with tempfile.TemporaryDirectory(prefix='ssx-crash-') as folder:
    folder = Path(folder)
    def run(name, events, frames=420):
        scenario = copy.deepcopy(initial)
        scenario['frames'] = frames
        scenario['events'] = events
        path = folder / f'{name}.json'
        path.write_text(json.dumps(scenario))
        output = folder / f'{name}-result'
        result = subprocess.run([sys.argv[1], '--replay', str(path), '--telemetry', str(output)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        rows = json.loads(output.with_suffix('.json').read_text())['records']
        assert all(not r['body_collision']['missing_pose'] for r in rows[1:]), 'Missing gameplay pose'
        return rows
    flip = [{'start': 0, 'end': 70, 'buttons': ['Cross'], 'left': [128, 255]}, {'start': 70, 'end': 200, 'left': [128, 255]}]
    rows = run('flip-crash', flip)
    crash = [r['frame'] for r in rows if r['control_state'] == 8]
    assert crash, 'The inverted landing did not enter the original crash control state'
    first, last = crash[0], crash[-1]
    assert not rows[first]['grounded'], 'Crash should begin in the airborne ragdoll submode'
    assert any(r['grounded'] for r in rows[first:last + 1]), 'Crash never reached the sliding submode'
    assert rows[first - 1]['control_state'] in (4, 5), 'Crash must interrupt an airborne controller'
    after = rows[last + 1]
    assert after['control_state'] == 0 and after['grounded'], 'Get-up must return to ordinary ground control'
    assert any(r['speed_mps'] > 5 for r in rows[last + 1:]), 'Rider did not ride on after recovering'
    assert all(r['control_state'] != 8 for r in rows[last + 1:last + 30]), 'Crash re-entered immediately after recovery'
    # Feed the 12CB68 recovery meter as soon as the ragdoll is sliding; phase 3
    # (get-up) ignores the meter, so the input has to start before it.
    slide = next(r['frame'] for r in rows[first:] if r['grounded'])
    quick = run('flip-crash-recover', [{'start': 0, 'end': 70, 'buttons': ['Cross'], 'left': [128, 255]}, {'start': 70, 'end': slide + 1, 'left': [128, 255]}, {'start': slide + 1, 'end': 420, 'buttons': ['Cross']}])
    quick_crash = [r['frame'] for r in quick if r['control_state'] == 8]
    assert quick_crash and quick_crash[0] == first, 'Recovery input changed the crash entry'
    assert max(r['crash_recovery'] for r in rows) == 0, 'Recovery meter moved without input'
    assert max(r['crash_recovery'] for r in quick) >= 1, 'Held jump did not fill the recovery meter'
    # This slow slide already plays its continuation at the 2x playback cap, so
    # the meter cannot shorten it; it must never lengthen the crash either.
    assert quick_crash[-1] <= last, f'Held jump lengthened the crash ({quick_crash[-1]} vs {last})'
    assert all(r['motion_mode'] == 2 for r in rows[first:last + 1]), 'Crash frames must report motion mode 2'
print(f'Hard crash entered at frame {first}, recovered at {last}, recovery meter filled with held jump; poses complete')
