#!/usr/bin/env python3
"""Original rail motion-4 update 0x13AF28 (and attach sequence 0x106848) instruction-oracle conformance."""
from pathlib import Path
import hashlib, subprocess, sys
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1

CASES={
 'motion':['0013AF28','001086B8','0013BD80','00121AA0','0011E098','0011FEE8','0031BB30','0031BE50','0031BF60','0031C128','0031C228'],
 'entry':['00106848','00108A48','001086B8','0013AD20','0011E098','00115358','00115168','00116930','0011DFE0','00311B48','001326C8','00131D08','0013ADC0','0013BD80','0011FEE8','0011FE98','0031BB30','0031BE50','0031BF60','0031C128','0031C228'],
}
def run(name,functions):
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    elf=root/'local/disc/SLUS_207.72'
    if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original SSX3 executable')
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
    command+=[str(root/'tests'/f'rail_{name}_reference.cpp')]+[str(root/'engine'/s) for s in ('air_alignment.cpp','orientation_motion.cpp','ground_motion.cpp')]
    work=root/'local/reference/rail'
    for prefix in functions:
        path=next((root/'local/output').glob('sub_'+prefix+'*'))
        command.append(str(write_scalar_fp_oracle(path,work/(f'{name}-'+path.name),path.read_text())))
    command+=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
    binary=root/f'build/ssx3_rail_{name}_reference';command+=['-o',str(binary)]
    cached_oracle_build(command,root/f'build/original-rail-{name}-objects')
    result=subprocess.run([str(binary),str(elf)],check=False,timeout=900,capture_output=True,text=True)
    print(result.stdout,end='');print(result.stderr,end='')
    if 'missing-target' in result.stdout or 'missing-target' in result.stderr:raise RuntimeError(f'Incomplete original rail {name} call graph')
    result.check_returncode()
for name in (sys.argv[1:] or CASES):run(name,CASES[name])
