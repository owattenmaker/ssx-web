"""Shared build/run step for the development-only board-press instruction oracles
(tools/test_boardpress_*_native.py). Recompiled originals get the PCSX2 EE scalar
FP correction; the native side is engine/board_press*.hpp plus its engine sources.
The fixture is the original executable mapped at ram+0xFF000 (tests/handplant_reference_common.hpp);
runtime-initialised bss the routines read (0x4FE920/0x4FE940 depth curves, 0x4FF130/0x4FF160)
is written by the harness and checked here against the Snow Jam glide snapshot."""
from pathlib import Path
import hashlib, struct, subprocess, zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1

HARNESS_BSS = {0x4FE920: (0.0, 0.2, 0.4, 0.4, 0.6, 0.4, 1.0, 0.2), 0x4FE940: (0.0, 0.3, 0.4, 1.2, 0.6, 1.2, 1.0, 0.3),
               0x4FF130: (0.0, 0.0, 0.0, 1.0), 0x4FF160: (0.0, 0.0, 1.0, 0.0)}

def verify_snapshot_bss(root):
    memory = zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin')
    for address, values in HARNESS_BSS.items():
        live = struct.unpack_from(f'<{len(values)}f', memory, address)
        expected = struct.unpack(f'<{len(values)}f', struct.pack(f'<{len(values)}f', *values))
        if live != expected: raise ValueError(f'Snapshot bss {address:#x} differs from the harness values: {live}')

def run_boardpress_oracle(name, functions, engine=('ground_motion.cpp','air_alignment.cpp','orientation_motion.cpp','animation_sequence.cpp')):
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    elf=root/'local/disc/SLUS_207.72'
    if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original SSX3 executable')
    verify_snapshot_bss(root)
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
    command+=[str(root/'tests'/f'boardpress_{name}_reference.cpp')]+[str(root/'engine'/source) for source in engine]
    work=root/'local/reference/boardpress'
    for prefix in functions:
        path=next((root/'local/output').glob('sub_'+prefix+'*'))
        command.append(str(write_scalar_fp_oracle(path,work/(f'{name}-'+path.name),path.read_text())))
    command+=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
    binary=root/f'build/ssx3_boardpress_{name}_reference';command+=['-o',str(binary)]
    cached_oracle_build(command,root/f'build/original-boardpress-{name}-objects')
    run=subprocess.run([str(binary),str(elf)],check=False,timeout=900,capture_output=True,text=True)
    print(run.stdout,end='');print(run.stderr,end='')
    if 'missing-target' in run.stdout or 'missing-target' in run.stderr:raise RuntimeError(f'Incomplete original board-press {name} call graph')
    run.check_returncode()
