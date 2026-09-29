#!/usr/bin/env python3
"""Original padded spatial-region classification conformance."""
from pathlib import Path
import subprocess,hashlib,struct
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
from inspect_disc import EXPECTED_SHA1
elf=(root/'local/disc/SLUS_207.72').read_bytes()
if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1 or struct.unpack_from('<I',elf,0x4a30f0-0x2dfc-0xff000)[0]!=0x3e4ccccd:raise ValueError('Unexpected original spatial padding constant')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/spatial_region_reference.cpp')]
path=next((root/'local/output').glob('sub_00328360*'));source=path.read_text()
for pc in ['328360']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/spatial-region-original.cpp',source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_spatial_region_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-spatial-region-objects')
run=subprocess.run([str(binary)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original spatial-region call graph')
