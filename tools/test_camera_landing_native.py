#!/usr/bin/env python3
"""Original jump-camera touchdown and decay conformance."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/camera_landing_reference.cpp')]
path=next((root/'local/output').glob('sub_001635F8*'));source=path.read_text()
for pc in ['163e8c']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
for pc in ['163fd8']:
 marker='    // 0x'+pc+':'
 if source.count(marker)!=1:raise ValueError('Missing camera landing boundary')
 source=source.replace(marker,'    ctx->pc=0x'+pc+';return;\n'+marker)
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/camera-landing-original.cpp',source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_camera_landing_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-camera-landing-objects')
import hashlib
from inspect_disc import EXPECTED_SHA1
elf=root/"local/disc/SLUS_207.72"
assert hashlib.sha1(elf.read_bytes()).hexdigest()==EXPECTED_SHA1
run=subprocess.run([str(binary),str(elf)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original camera landing call graph')
