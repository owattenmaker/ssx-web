#!/usr/bin/env python3
"""Separate camera initialization history from browser-driven jump/landing drift."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
out = root / 'local/camera-continuous'
# Validates original capture continuity/provenance and rebuilds the replay runner.
subprocess.run([sys.executable, str(root / 'tools/test_camera_sequence_native.py')], check=True)
with (out / 'browser-history-inputs.log').open('w') as log:
    subprocess.run(['node', str(root / 'web/audit-camera-inputs.mjs')], check=True, stdout=log, stderr=subprocess.STDOUT)
report = out / 'browser-history-comparison.json'
subprocess.run([str(root / 'build/ssx3_camera_sequence_reference'), str(out / 'jump'),
                '0x1a58650', str(report), str(out / 'browser-camera-trace.json')], check=True)
rows = json.loads(report.read_text())
assert [r['tick'] for r in rows] == list(range(370, 491))
assert next(r for r in rows if r['mode'] == 0)['tick'] == 451
for row in rows:
    # Same cold start and actual WASM inputs reproduce the WASM eye/target exactly.
    assert row['browser_replay_eye_error_cm'] == 0, row
    assert row['browser_replay_target_error_cm'] == 0, row
    # Original history + browser inputs must track the original through touchdown.
    assert row['browser_seeded_eye_error_cm'] < .1, row
    assert row['browser_seeded_target_error_cm'] < .1, row
print('121 browser-driven camera updates: exact native/WASM cold replay; original-seeded eye/target under 1mm.')
print('Maximum seeded eye error (cm):', max(r['browser_seeded_eye_error_cm'] for r in rows))
print('Scope excludes compositor terrain lift, shake, interpolation and rendered images.')
