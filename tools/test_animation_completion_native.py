#!/usr/bin/env python3
"""Original 0x103918 sequence-completion dispatch conformance (engine/animation_completion.hpp)."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/animation_completion_reference.cpp')]
folder=root/'local/reference/animation-completion'
for prefix in ['00103578','00104A40','00104A60','00104B48','00104B78','00104B98','00104BB8','00104BD8','00104C18','00104C38','00104C80','00104CA0','00312B18','00312BD0','00144670']:
 path=next((root/'local/output').glob('sub_'+prefix+'_*'))
 command.append(str(write_scalar_fp_oracle(path,folder/path.name)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_animation_completion_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-animation-completion-objects')
run=subprocess.run([str(binary)],check=False,timeout=300,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
(folder/'reference.log').write_text(run.stdout)
