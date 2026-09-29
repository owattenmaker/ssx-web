#!/usr/bin/env python3
"""Check the captured original jump camera continuously, with no per-frame reseeding."""
from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[1]
capture=root/'local/camera-continuous/jump'
baseline=root/'local/reference/pcsx2/snow-jam-jump-31.p2s'
baseline_hash=hashlib.sha256(baseline.read_bytes()).hexdigest()
files=sorted(capture.glob('tick-*.json'))
if len(files)!=122:
    raise ValueError('Expected all 122 original checkpoints, ticks369..490')
for tick,path in enumerate(files,369):
    record=json.loads(path.read_text())
    if record['baseline_sha256']!=baseline_hash or any(c['tick']!=tick for c in record['cameras']):
        raise ValueError('Capture provenance or continuity differs')
binary=root/'build/ssx3_camera_sequence_reference'
subprocess.run(['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off',
               '-I'+str(root/'engine'),'-I'+str(root/'build/ps2recomp/_deps/nlohmann_json-src/single_include'),
               str(root/'tests/camera_sequence_reference.cpp'),'-o',str(binary)],check=True)
report=root/'local/camera-continuous/jump-comparison.json'
subprocess.run([str(binary),str(capture),'0x1a58650',str(report)],check=True)
rows=json.loads(report.read_text())
fields=['single_step_different_words','carried_different_words','single_eye_error_cm',
        'carried_eye_error_cm','carried_target_error_cm']
if any(row[key]!=0 for row in rows for key in fields):
    raise ValueError('Original camera sequence diverges; inspect jump-comparison.json')
manifest=dict(baseline_sha256=baseline_hash,first_tick=369,last_tick=490,
              scope='DEFAULT_3 algorithm; excludes compositor collision, shake and rendered image',
              inputs='Post-frame original head/rider/trajectory; initial full algorithm and spline state',
              files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
              comparison_sha256=hashlib.sha256(report.read_bytes()).hexdigest())
(capture.parent/'jump-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('121 original camera updates match bit for bit, including touchdown at tick451 and 40 grounded updates; continuous and individually seeded comparisons pass.')
