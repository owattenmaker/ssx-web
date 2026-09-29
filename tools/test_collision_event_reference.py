#!/usr/bin/env python3
"""Compare native collision reaction decisions with complete original105D98/108388."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle

def main():
    root=Path(__file__).resolve().parents[1];folder=root/'local/reference/terrain/collision-event';folder.mkdir(parents=True,exist_ok=True)
    vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    command=['clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    command.append(str(root/'tests/collision_event_reference.cpp'))
    for name in ['sub_001210B0_0x1210b0.cpp','sub_00317A08_0x317a08.cpp','sub_00105D98_0x105d98.cpp','sub_00108388_0x108388.cpp']:command.append(str(write_scalar_fp_oracle(root/'local/output'/name,folder/name)))
    command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
    binary=root/'build/ssx3_collision_event_reference';subprocess.run(command+['-o',str(binary)],check=True)
    ee=folder/'ee.bin'
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:ee.write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(ee)],check=True)

if __name__=='__main__':main()
