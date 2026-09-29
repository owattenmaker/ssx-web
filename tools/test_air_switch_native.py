#!/usr/bin/env python3
"""Original in-flight stance switch 0x135BE0/0x114DB8 (+0x11E098, 0x115168) conformance
against engine/air_switch.hpp (development-only instruction oracle)."""
from pathlib import Path
import hashlib, subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
elf=root/'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original SSX3 executable')
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command+=[str(root/'tests/air_switch_reference.cpp')]+[str(root/'engine'/s) for s in ('orientation_motion.cpp','ground_motion.cpp','air_control.cpp')]
work=root/'local/reference/air-switch'
for prefix in ('00135BE0','00114DB8','0011E098','00115168','0031BE50'):
    path=next((root/'local/output').glob('sub_'+prefix+'*'))
    command.append(str(write_scalar_fp_oracle(path,work/('air-switch-'+path.name),path.read_text())))
command+=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_air_switch_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-air-switch-objects')
run=subprocess.run([str(binary),str(elf)],check=False,timeout=600,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='')
if 'missing-target' in run.stdout or 'missing-target' in run.stderr:raise RuntimeError('Incomplete original air-switch call graph')
run.check_returncode()
