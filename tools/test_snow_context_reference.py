#!/usr/bin/env python3
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/snow-context';work.mkdir(parents=True,exist_ok=True)
names=['sub_002DF920_0x2df920.cpp','sub_002E23E0_0x2e23e0.cpp','sub_002E2550_0x2e2550.cpp']
sources=[scalar_oracle_copy(root/'local/output'/n,work/n)for n in names]
p=sources[0];s=p.read_text();s=s.replace('label_2dfdac:','label_2dfdac:\nctx->pc=0x12345678;return;');p.write_text(s)
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
cmd=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in includes]
cmd+=[str(root/'tests/snow_context_reference.cpp')]+[str(root/'engine'/n)for n in ['snow_context.cpp','snow_emission.cpp','rider_pose_motion.cpp','animation_motion.cpp','orientation_motion.cpp','ground_motion.cpp']]+list(map(str,sources))
cmd+=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',f]
binary=root/'build/ssx3_snow_context_reference';subprocess.run(cmd+['-o',str(binary)],check=True)
with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s')as z:(work/'ee.bin').write_bytes(z.read('eeMemory.bin'))
subprocess.run([str(binary),str(work/'ee.bin')],check=True)
