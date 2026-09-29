#!/usr/bin/env python3
"""Original irradiance matrix-transform conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from generate_irradiance_transform import generate
generate()
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/irradiance_transform_reference.cpp')]
path=next((root/'local/output').glob('sub_00389840*'));source=path.read_text()
for pc in ['389840']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/irradiance-transform-original.cpp',source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_irradiance_transform_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-irradiance-transform-objects')
run=subprocess.run([str(binary),str(root/"local/assets/source/ps2/irr.dat")],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original irradiance transform call graph')
