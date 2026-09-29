#!/usr/bin/env python3
"""Original instance-contact phase conformance: 104E70, 105398 (+1231A8) and 1057B8."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/instance_contact_reference.cpp')]
folder=root/'local/reference/instance-contact'
for prefix in ['00104E70','00105398','001057B8','001231A8']:
 path=next((root/'local/output').glob('sub_'+prefix+'*'))
 command.append(str(write_scalar_fp_oracle(path,folder/path.name)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_instance_contact_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-instance-contact-objects')
run=subprocess.run([str(binary)],check=False,timeout=300,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete instance-contact call graph')
(folder/'reference.log').write_text(run.stdout)
