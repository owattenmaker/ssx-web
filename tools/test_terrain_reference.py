#!/usr/bin/env python3
"""Development-only original/native terrain Newton conformance."""
import subprocess
import zipfile
from pathlib import Path
from original_fp_oracle import write_scalar_fp_oracle
def main():
    root=Path(__file__).resolve().parents[1]
    vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    binary=root/'build/ssx3_terrain_reference'
    command=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']
    command+=['-I'+str(p) for p in includes]
    command += [str(root/'tests/terrain_reference.cpp'),str(root/'local/output/sub_0032E100_0x32e100.cpp'),str(root/'local/output/sub_0032B6A8_0x32b6a8.cpp'),str(root/'local/output/sub_0013D1B8_0x13d1b8.cpp'),str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
    command+=['-o',str(binary)]
    for i,arg in enumerate(command):
        if arg.startswith(str(root/'local/output')) and arg.endswith('.cpp'):
            command[i]=str(write_scalar_fp_oracle(Path(arg),root/'local/reference/terrain/original-fp-policy'/Path(arg).name))
    subprocess.run(command,check=True)
    vu=root/'local/reference/terrain/vu0-reference.bin'
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:vu.write_bytes(z.read('vu0MicroMem.bin'))
    subprocess.run([str(binary),str(root/'local/reference/terrain/native-contact-fixture.txt'),str(vu)],check=True)


if __name__=='__main__':main()
