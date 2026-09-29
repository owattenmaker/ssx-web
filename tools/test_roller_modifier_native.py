#!/usr/bin/env python3
"""RollerModifier conformance: randomized instruction-level oracles of the
recompiled originals (35D340, 35E248, 35D288, 35E770, 32C648, 35CFF0, 35D908,
35DDE8, 361CD8, 35D4A0, 35EDC8, 35E850, 31B748/31B7A8, 35DA70, 3568B0) against
engine/roller_modifier.hpp. Log: local/reference/roller-modifier/reference.log."""
from pathlib import Path
import re, subprocess, sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
folder=root/'local/reference/roller-modifier'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/roller_modifier_reference.cpp')]
# Function entries folded into a neighbouring recompiled file get an explicit entry label.
extra_entries={'0035E770':[0x35e850]}
for prefix in ['0035D340','0035E248','0035D288','0035E770','0032C648','0035CFF0','0035DDE8','0035D4A0','0035D908','0035EDC8','0035ED90',
               '0032C630','0032C5A8','0035DA70','0035CFE8','0032C508','0032C540','00327CC8','0031B748','0031B7A8','003612C0','003568B0',
               '00352B88','00352BF8','00352BC0','00356020','0035FE10','003554B0','00355468','00352AA8','003567E0','00356780','00350570','00352C38','00355DB8']:
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
binary=root/'build/ssx3_roller_modifier_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-roller-modifier-objects')
run=subprocess.run([str(binary),str(root/'local/disc/SLUS_207.72')],check=False,timeout=1800,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='')
(folder/'reference.log').write_text(run.stdout+run.stderr)
run.check_returncode()
