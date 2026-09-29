#!/usr/bin/env python3
"""Flag cloth conformance: randomized instruction-level oracles of the recompiled
originals (392DF0/31BF60 sine table, 34AC88 parameters, 34BCA0 vertices, 34B818
cloth tick, 34C668 manager tick, 34B228 build) against engine/flag_cloth.hpp.
Log: local/reference/flag-cloth/reference.log. Also writes the git-ignored
web/public/assets/FLAGS/flag-golden.json (port outputs) for web/test-flag-animation.mjs."""
from pathlib import Path
import subprocess, sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
folder=root/'local/reference/flag-cloth';folder.mkdir(parents=True,exist_ok=True)
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/flag_cloth_reference.cpp')]
extra_entries={'0034C600':[0x34c668]}   # manager tick folded into the 34C600 file
for prefix in ['00392DF0','0031BF60','0034AC88','0034BCA0','0034B818','0034C600','0034B228']:
    path=next((root/'local/output').glob('sub_'+prefix+'_*.cpp'))
    text=path.read_text(errors='replace')
    for pc in extra_entries.get(prefix,[]):
        text=text.replace('switch (ctx->pc) {',f'switch (ctx->pc) {{\n        case 0x{pc:x}u: goto original_entry_{pc:x};',1)
        marker=f'    // 0x{pc:x}:'
        if marker not in text:raise RuntimeError(f'no marker 0x{pc:x}')
        text=text.replace(marker,f'original_entry_{pc:x}:\n'+marker,1)
    command.append(str(write_scalar_fp_oracle(path,folder/path.name,source=text)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_flag_cloth_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-flag-cloth-objects')
golden=root/'web/public/assets/FLAGS/flag-golden.json';golden.parent.mkdir(parents=True,exist_ok=True)
run=subprocess.run([str(binary),str(root/'local/disc/SLUS_207.72'),str(golden)],check=False,timeout=1800,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='')
(folder/'reference.log').write_text(run.stdout+run.stderr)
run.check_returncode()
