"""Shared build/run step for the development-only handplant instruction oracles
(tools/test_handplant_*_native.py). Recompiled originals get the PCSX2 EE scalar
FP correction; the native side is engine/handplant.hpp plus its engine sources."""
from pathlib import Path
import hashlib, subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1

def run_handplant_oracle(name, functions, engine=('air_alignment.cpp','orientation_motion.cpp','ground_motion.cpp')):
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    elf=root/'local/disc/SLUS_207.72'
    if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original SSX3 executable')
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
    command+=[str(root/'tests'/f'handplant_{name}_reference.cpp')]+[str(root/'engine'/source) for source in engine]
    work=root/'local/reference/handplant'
    for prefix in functions:
        path=next((root/'local/output').glob('sub_'+prefix+'*'))
        command.append(str(write_scalar_fp_oracle(path,work/(f'{name}-'+path.name),path.read_text())))
    command+=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
    binary=root/f'build/ssx3_handplant_{name}_reference';command+=['-o',str(binary)]
    cached_oracle_build(command,root/f'build/original-handplant-{name}-objects')
    run=subprocess.run([str(binary),str(elf)],check=False,timeout=600,capture_output=True,text=True)
    print(run.stdout,end='');print(run.stderr,end='')
    if 'missing-target' in run.stdout or 'missing-target' in run.stderr:raise RuntimeError(f'Incomplete original handplant {name} call graph')
    run.check_returncode()
