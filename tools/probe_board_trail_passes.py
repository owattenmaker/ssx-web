#!/usr/bin/env python3
"""Inspect original track GS packets with geometry submission intercepted."""
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/board-trail';work.mkdir(parents=True,exist_ok=True)
paths=[]
for name in ['sub_00386DD0_0x386dd0.cpp','sub_00368970_0x368970.cpp','sub_00362478_0x362478.cpp']:
    source=scalar_oracle_copy(root/'local/output'/name,work/('pass-'+name));paths.append(source)
    if name.startswith('sub_00386'):
        # Intercept an intra-AOT-range call just like external geometry calls.
        # Original command construction and material bits remain unchanged.
        text=source.read_text().replace('void sub_00386DD0_0x386dd0(', 'void callback(uint8_t*,R5900Context*,PS2Runtime*);\nvoid sub_00386DD0_0x386dd0(',1).replace('\n    goto label_386dd0;', '\n    callback(rdram,ctx,runtime);');source.write_text(text)
cmd=['xcrun','clang++','-std=c++20','-O0','-DUSE_SSE2NEON']+['-I'+str(p)for p in [vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']]+[str(root/'tests/board_trail_pass_probe.cpp')]+list(map(str,paths))+[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',f]
subprocess.run(cmd+['-o',str(work/'pass-probe')],check=True)
with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as archive:(work/'ee.bin').write_bytes(archive.read('eeMemory.bin'))
r=subprocess.run([str(work/'pass-probe'),str(work/'ee.bin')],capture_output=True,text=True)
print(r.stdout,end='');print(r.stderr,end='')
(work/'render-passes.txt').write_text(r.stdout+r.stderr)
r.check_returncode()
