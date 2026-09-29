#!/usr/bin/env python3
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy

def main():
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/crash-detached';work.mkdir(parents=True,exist_ok=True)
    names=['sub_00137550_0x137550.cpp','sub_00137138_0x137138.cpp','sub_00329A90_0x329a90.cpp','sub_0032F650_0x32f650.cpp','sub_00329910_0x329910.cpp','sub_00329A28_0x329a28.cpp','sub_003E6574_0x3e6574.cpp','sub_00329DC8_0x329dc8.cpp','sub_00317830_0x317830.cpp','sub_0031BE50_0x31be50.cpp','sub_00329B40_0x329b40.cpp']
    sources=[scalar_oracle_copy(root/'local/output'/name,work/name) for name in names]
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    cmd=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    cmd += [str(root/'tests/crash_detached_reference.cpp')]+list(map(str,sources))+[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',framework]
    binary=root/'build/ssx3_crash_detached_reference';subprocess.run(cmd+['-o',str(binary)],check=True)
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:(work/'ee.bin').write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(work/'ee.bin')],check=True)
if __name__=='__main__':main()
