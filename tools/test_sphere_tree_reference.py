#!/usr/bin/env python3
"""Compare the recovered native sphere-tree traversal with original instructions."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle

def main():
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    command=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    command += [str(root/'tests/sphere_tree_reference.cpp')]
    for name in ('sub_0032CDB0_0x32cdb0.cpp','sub_00327F18_0x327f18.cpp'):
        target=root/'local/reference/terrain/scalar-policy'/name
        write_scalar_fp_oracle(root/'local/output'/name,target);command.append(str(target))
    command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
    binary=root/'build/ssx3_sphere_tree_reference';subprocess.run(command+['-o',str(binary)],check=True)
    ee=root/'local/reference/terrain/ee-obstacle-reference.bin'
    if not ee.exists():
        with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:ee.write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(ee)],check=True)

if __name__=='__main__':main()
