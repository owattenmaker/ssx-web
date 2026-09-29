#!/usr/bin/env python3
"""Development-only soft collision lifecycle and existing classifier regression."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
root=Path(__file__).resolve().parents[1];out=root/'build/soft-collision-oracle';out.mkdir(parents=True,exist_ok=True)
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
base=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in includes]
link=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:link+=['-framework',framework]
fixture=out/'ee.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
for test,original,native in [('soft_collision_reference',['0012E778','00116378','00116120','00113E80','00113F88','00108388'],['soft_collision_control.cpp','ground_motion.cpp']),('collision_event_reference',['001210B0','00317A08','00105D98','00108388'],[])]:
    command=base+[str(root/'tests'/f'{test}.cpp')]+[str(root/'engine'/f)for f in native]
    for prefix in original:
        source=next((root/'local/output').glob(f'sub_{prefix}*'));command.append(str(write_scalar_fp_oracle(source,out/source.name)))
    binary=out/test;subprocess.run(command+link+['-o',str(binary)],check=True);subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
