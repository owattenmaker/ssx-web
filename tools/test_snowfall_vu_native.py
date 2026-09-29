#!/usr/bin/env python3
"""Snowfall draw oracle (docs/weather.md): the original VU1 program 5 (flakes 0x000, fluff 0x408) on synthetic layer uploads;
writes local/reference/weather/snowfall-vu.json for web/test-weather.mjs (flake positions of web/weather-renderer.js)."""
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from extract_fx_microcode import export
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';out=root/'build/snowfall-vu';out.mkdir(parents=True,exist_ok=True)
export(out/'vu');program=out/'vu/program5.bin'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(x) for x in includes]
command+=[str(root/'tests/snowfall_vu_reference.cpp'),str(vendor/'ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp'),str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=out/'snowfall_vu';subprocess.run(command+['-o',str(binary)],check=True)
run=subprocess.run([str(binary),str(program)],check=True,capture_output=True,text=True,timeout=600)
target=root/'local/reference/weather/snowfall-vu.json';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(run.stdout)
print(target, len(run.stdout))
