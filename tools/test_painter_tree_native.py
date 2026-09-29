#!/usr/bin/env python3
"""Original painter point query conformance on the real ARA1 fog tree."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from export_fog_tree import export
export()
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/painter_tree_reference.cpp')]
path=next((root/'local/output').glob('sub_002C1CD8*'));source=path.read_text()
for pc in ['2c1cd8']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/painter-tree-original.cpp',source)))
for name in ['002BAF90','002C0A10']:
 original=next((root/'local/output').glob('sub_'+name+'*'))
 command.append(str(write_scalar_fp_oracle(original,root/('local/event-activation/painter-'+name+'.cpp'))))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_painter_tree_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-painter-tree-objects')
run=subprocess.run([str(binary),str(root/"local/event-activation/fog-section.bin")],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original painter point query')
