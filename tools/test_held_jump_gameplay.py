#!/usr/bin/env python3
"""Exercise ledge departure with jump held through the actual gameplay session."""
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
with tempfile.TemporaryDirectory(prefix='ssx-held-jump-') as folder:
    folder = Path(folder)
    def run(name, release):
        scenario = copy.deepcopy(initial)
        scenario['frames'] = 300
        scenario['events'] = [{'start': 0, 'end': release, 'buttons': ['Cross']}]
        path = folder / f'{name}.json'
        path.write_text(json.dumps(scenario))
        output = folder / f'{name}-result'
        result = subprocess.run([sys.argv[1], '--replay', str(path), '--telemetry', str(output)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        rows = json.loads(output.with_suffix('.json').read_text())['records']
        assert all(not r['body_collision']['missing_pose'] for r in rows[1:]), 'Missing gameplay pose'
        return rows
    held = run('held-through-landing', 220)
    departure = next(r['frame'] for r in held if not r['grounded'])
    touchdown = next(r for r in held[departure + 1:220] if r['grounded'])
    assert held[departure]['control_state'] == 2
    assert touchdown['control_state'] == 2, 'Landing discarded the held jump'
    release = departure + 16
    released = run('released-in-flight', release)
    before, after = released[release:release + 2]
    assert not before['grounded'] and not after['grounded']
    assert before['control_state'] == 2 and after['control_state'] == 5
    assert after['velocity'][1] <= before['velocity'][1], 'Release added a second jump impulse'
    assert any(r['grounded'] for r in released[release + 1:]), 'No landing after airborne release'
print('Held ledge departure, held touchdown, airborne release and collision poses passed')
