#!/usr/bin/env python3
"""Audit installed spline search and ideal attach approaches without gameplay mutation."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'local/browser-validation'
out.mkdir(parents=True,exist_ok=True)
shim=out/'include/emscripten'
shim.mkdir(parents=True,exist_ok=True)
(shim/'emscripten.h').write_text('#pragma once\n#define EMSCRIPTEN_KEEPALIVE\n')
subprocess.run(['node',str(root/'web/audit-rail-coverage.mjs')],check=True)
binary=out/'rail-coverage-audit'
subprocess.run(['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off',
 '-I'+str(out/'include'),'-I'+str(root/'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf'),
 str(root/'web/rail_bridge.cpp'),str(root/'tests/rail_coverage_audit.cpp'),'-o',str(binary)],check=True)
subprocess.run([str(binary),str(root/'web/public/assets/ARA1/rails.json'),str(out/'rail-attach-coverage.json')],check=True)
