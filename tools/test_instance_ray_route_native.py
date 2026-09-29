#!/usr/bin/env python3
"""Original instance event6 fallback conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/instance_ray_route_reference.cpp')]
for prefix,entry,stops in [('00335B90','335bb0',['335bc0','335c68','335d68']),('00336D40','336d64',['336d78','336e18','336f1c'])]:
 path=next((root/'local/output').glob('sub_'+prefix+'*'));source=path.read_text()
 if 'case 0x'+entry+'u:' not in source:raise ValueError('Missing ray entry')
 for pc in stops:
  marker='label_'+pc+':'
  if source.count(marker)!=1:raise ValueError('Missing ray boundary')
  source=source.replace(marker,marker+'\n ctx->pc=0x'+pc+';return;')
 command.append(str(write_scalar_fp_oracle(path,root/('local/event-activation/ray-'+prefix+'.cpp'),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_instance_ray_route_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-instance-ray-route-objects')
run=subprocess.run([str(binary)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original instance state call graph')
