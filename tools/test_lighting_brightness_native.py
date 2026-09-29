#!/usr/bin/env python3
"""Complete original lighting brightness conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from generate_lighting_math import generate
generate()
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/lighting_brightness_reference.cpp')]
for prefix in ['002EDF00','0040DA10','0040D758','0040E550','0040E048','0040F9D0','0040F8C0']:
 path=next((root/'local/output').glob('sub_'+prefix+'*'))
 command.append(str(write_scalar_fp_oracle(path,root/('local/event-activation/lighting-math-'+prefix+'.cpp'))))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_lighting_brightness_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-lighting-brightness-objects')
import hashlib
from inspect_disc import EXPECTED_SHA1
elf=root/'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected irradiance executable')
run=subprocess.run([str(binary),str(elf),str(root/"local/event-activation/lighting-brightness-input.bin"),str(root/"local/event-activation/lighting-brightness-expected.bin")],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original lighting math call graph')
