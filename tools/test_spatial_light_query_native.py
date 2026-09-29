#!/usr/bin/env python3
"""Original spatial light candidate traversal conformance."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[root/'build/ps2recomp/_deps/nlohmann_json-src/single_include',vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/spatial_light_query_reference.cpp')]
path=next((root/'local/output').glob('sub_00332DB8*'));source=path.read_text()
for pc in ['332db8']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/spatial-light-query-original.cpp',source)))
for prefix in ['00328360','0033B748','00340DC0']:
 path=next((root/'local/output').glob('sub_'+prefix+'*'))
 text=path.read_text()
 if prefix in ('0033B748','00340DC0'):
  address=prefix.lstrip('0').lower();jump='    goto label_'+address+';'
  if text.count(jump)!=8:raise ValueError('Unexpected recursive traversal lowering')
  text=text.replace(jump,'    sub_'+prefix+'_0x'+address+'(rdram,ctx,runtime);')
 command.append(str(write_scalar_fp_oracle(path,root/('local/event-activation/spatial-light-query-'+prefix+'.cpp'),text)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_spatial_light_query_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-spatial-light-query-objects')
import hashlib
from inspect_disc import EXPECTED_SHA1
elf=root/'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected camera executable')
run=subprocess.run([str(binary),str(elf)],check=False,timeout=120,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original light ranking/selection call graph')

from export_light_tree import export
import zipfile
export()
fixture=root/'local/rider-lighting/query-reference-memory.bin'
for name in ['glide','jump-31','jump-90']:
 with zipfile.ZipFile(root/('local/reference/pcsx2/snow-jam-'+name+'.p2s')) as archive:fixture.write_bytes(archive.read('eeMemory.bin'))
 subprocess.run([str(binary),str(fixture),str(root/'local/assets/native/ARA1/light-tree.json'),str(root/'local/rider-lighting/light-tree-fixtures.json'),name],check=True,timeout=60)
