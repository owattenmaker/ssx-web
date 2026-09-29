#!/usr/bin/env python3
"""Original rail segment search conformance."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output',root/'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/rail_segment_reference.cpp')]
path=next((root/'local/output').glob('sub_00335128*'))
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/rail-segment-original.cpp',path.read_text())))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_rail_segment_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-rail-segment-objects')
import hashlib
from inspect_disc import EXPECTED_SHA1
elf=root/"local/disc/SLUS_207.72"
assert hashlib.sha1(elf.read_bytes()).hexdigest()==EXPECTED_SHA1
run=subprocess.run([str(binary),str(elf),str(root/'web/public/assets/ARA1/rails.json')],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original rail segment call graph')
