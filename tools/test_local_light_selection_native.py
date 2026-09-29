#!/usr/bin/env python3
"""Original local light ranking and eight-slot selection conformance."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[root/'build/ps2recomp/_deps/nlohmann_json-src/single_include',vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/local_light_selection_reference.cpp')]
path=next((root/'local/output').glob('sub_002F5D30*'));source=path.read_text()
for pc in ['2f5d30']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/local-light-selection-original.cpp',source)))
for prefix in ['002F6168','002F5B68']:
 path=next((root/'local/output').glob('sub_'+prefix+'*'))
 command.append(str(write_scalar_fp_oracle(path,root/('local/event-activation/local-light-selection-'+prefix+'.cpp'))))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_local_light_selection_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-local-light-selection-objects')
import hashlib
from inspect_disc import EXPECTED_SHA1
elf=root/'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected camera executable')
from export_local_lights import export
export()
run=subprocess.run([str(binary),str(elf),str(root/'local/assets/native/ARA1/local-lights.json'),str(root/'local/rider-lighting/light-selection-checkpoints.json')],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original light ranking/selection call graph')
