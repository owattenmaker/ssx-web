#!/usr/bin/env python3
"""Check the camera director port against the original finish capture (development reference).

Capture: tools/ps2_capture_director.py build local/reference/pcsx2/snow-jam-glide.p2s \
  local/ps2-capture/scripts/finish-neutral.json local/ps2-capture/runs/finish-neutral.p2s --isolate --finish-ahead 3000
then `run ... --frames 1100`. Skips (exit 77) when the private capture is absent."""
from pathlib import Path
import json, subprocess, sys
root = Path(__file__).resolve().parents[1]
run = root / 'local/ps2-capture/runs/finish-neutral'
camera, main, manifest = run.with_suffix('.camera.bin'), run.with_suffix('.bin'), run.with_suffix('.capture.json')
if not (camera.exists() and main.exists() and manifest.exists()):
    print('skip: finish-neutral director capture not present'); sys.exit(77)
binary = root / 'build/ssx3_camera_director_capture'
subprocess.run(['xcrun', 'clang++', '-std=c++20', '-O2', '-frounding-math', '-ffp-contract=off', '-I' + str(root / 'engine'),
                str(root / 'tests/camera_director_capture.cpp'), '-o', str(binary)], check=True)
record = json.loads(manifest.read_text())['record']
subprocess.run([str(binary), str(camera), str(main), str(record)], check=True)
