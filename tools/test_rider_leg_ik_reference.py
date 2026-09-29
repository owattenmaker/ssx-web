#!/usr/bin/env python3
"""Verify original partial foot-target blend and complete leg solve."""
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/leg-ik';work.mkdir(parents=True,exist_ok=True)
names=['sub_0011F3D8_0x11f3d8.cpp','sub_0031BCB0_0x31bcb0.cpp','sub_0031BF60_0x31bf60.cpp','sub_0031BE50_0x31be50.cpp','sub_0031C128_0x31c128.cpp']
sources=[scalar_oracle_copy(root/'local/output'/n,work/n)for n in names]
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
cmd=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in includes]
cmd+=[str(root/'tests/rider_leg_ik_reference.cpp')]+[str(root/'engine'/n)for n in ['rider_pose_motion.cpp','animation_motion.cpp','orientation_motion.cpp','ground_motion.cpp']]+list(map(str,sources))
cmd+=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',f]
binary=root/'build/ssx3_rider_leg_ik_reference';subprocess.run(cmd+['-o',str(binary)],check=True)
with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s')as z:(work/'ee.bin').write_bytes(z.read('eeMemory.bin'))
subprocess.run([str(binary),str(work/'ee.bin')],check=True)
