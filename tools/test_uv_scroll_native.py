#!/usr/bin/env python3
"""UVScroll conformance: randomized instruction-level oracles of the recompiled
originals 35F6E8 (ctor), 35F7D0 (tick) and 35FC20 (texture matrix, zero spin)
against engine/uv_scroll.hpp. Log: local/reference/uv-scroll/reference.log."""
from pathlib import Path
import subprocess, sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
folder=root/'local/reference/uv-scroll';folder.mkdir(parents=True,exist_ok=True)
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/uv_scroll_reference.cpp')]
for prefix in ['0035F6E8','0035F7D0','0035FC20']:
    path=next((root/'local/output').glob('sub_'+prefix+'_*.cpp'))
    command.append(str(write_scalar_fp_oracle(path,folder/path.name)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_uv_scroll_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-uv-scroll-objects')
run=subprocess.run([str(binary),str(root/'local/disc/SLUS_207.72')],check=False,timeout=600,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='')
(folder/'reference.log').write_text(run.stdout+run.stderr)
run.check_returncode()
