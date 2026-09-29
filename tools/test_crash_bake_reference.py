#!/usr/bin/env python3
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy

def main():
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/crash-bake';work.mkdir(parents=True,exist_ok=True)
    names=['sub_0010EB30_0x10eb30.cpp','sub_0011E098_0x11e098.cpp','sub_0012E010_0x12e010.cpp']
    sources=[scalar_oracle_copy(root/'local/output'/name,work/name) for name in names]
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    cmd=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    cmd += [str(root/'tests/crash_bake_reference.cpp'),str(root/'engine/animation_motion.cpp'),str(root/'engine/orientation_motion.cpp'),str(root/'engine/ground_motion.cpp')]+list(map(str,sources))+[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',framework]
    binary=root/'build/ssx3_crash_bake_reference';subprocess.run(cmd+['-o',str(binary)],check=True)
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:(work/'ee.bin').write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(work/'ee.bin')],check=True)
if __name__=='__main__':main()
