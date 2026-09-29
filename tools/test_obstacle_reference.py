#!/usr/bin/env python3
"""Build development-only original/native obstacle response conformance."""
import subprocess
import zipfile
from reference_scalar_lowering import scalar_oracle_copy
from pathlib import Path

def main():
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    binary=root/'build/ssx3_obstacle_reference'
    command=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    command += [str(root/'tests/obstacle_reference.cpp'),str(root/'local/output/sub_0013F488_0x13f488.cpp'),str(root/'local/output/sub_0013AA48_0x13aa48.cpp'),str(root/'local/output/sub_001065B0_0x1065b0.cpp'),str(root/'local/output/sub_0031C228_0x31c228.cpp'),str(root/'local/output/sub_0031BE50_0x31be50.cpp'),str(root/'engine/ground_motion.cpp'),str(root/'local/output/sub_0032A1C0_0x32a1c0.cpp'),str(root/'local/output/sub_0032AA28_0x32aa28.cpp'),str(root/'local/output/sub_00329B90_0x329b90.cpp'),str(root/'local/output/sub_00334888_0x334888.cpp'),str(root/'local/output/sub_0032B6A8_0x32b6a8.cpp'),str(root/'local/output/sub_00329590_0x329590.cpp'),str(root/'local/output/sub_0032B2B8_0x32b2b8.cpp'),str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
    for i,arg in enumerate(command):
        if arg.startswith(str(root/'local/output')) and arg.endswith('.cpp'):
            command[i]=str(scalar_oracle_copy(Path(arg),root/'local/reference/terrain/scalar-policy'/Path(arg).name))
    subprocess.run(command+['-o',str(binary)],check=True)
    vu=root/'local/reference/terrain/vu0-obstacle-reference.bin'
    ee=root/'local/reference/terrain/ee-obstacle-reference.bin'
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:
        vu.write_bytes(z.read('vu0MicroMem.bin'));ee.write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(vu),str(ee)],check=True)

if __name__=='__main__':main()
