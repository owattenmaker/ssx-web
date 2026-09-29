#!/usr/bin/env python3
"""Full ordered shared pair dispatcher conformance with live mutable bodies."""
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy

def main():
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/rider-pair-system';work.mkdir(parents=True,exist_ok=True)
    names=['sub_00107888_0x107888.cpp','sub_00107E70_0x107e70.cpp','sub_00329F98_0x329f98.cpp','sub_00106538_0x106538.cpp','sub_00329B40_0x329b40.cpp','sub_0010F560_0x10f560.cpp','sub_0031C228_0x31c228.cpp']
    sources=[scalar_oracle_copy(root/'local/output'/name,work/name) for name in names]
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    cmd=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    cmd += [str(root/'tests/rider_pair_system_reference.cpp')]+list(map(str,sources))+[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',framework]
    binary=root/'build/ssx3_rider_pair_system_reference';subprocess.run(cmd+['-o',str(binary)],check=True)
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:(work/'ee.bin').write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(work/'ee.bin')],check=True)
if __name__=='__main__':main()
