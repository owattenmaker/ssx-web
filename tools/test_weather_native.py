#!/usr/bin/env python3
"""Weather conformance (docs/weather.md): randomized instruction-level oracles of the recompiled originals 0x125970 (rider
wind push), 0x2E4F50 / 0x2E5430 (snowfall layers), 0x2F4330 / 0x2F4260 / 0x2F39E0 (camera splash with its spawns, drops,
crystals and the visual RNG) against engine/wind_push.hpp and engine/weather.hpp. Log: local/reference/weather/reference.log."""
from pathlib import Path
import subprocess, sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
folder=root/'local/reference/weather';folder.mkdir(parents=True,exist_ok=True)
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/weather_reference.cpp')]
for prefix in ['00125970','002EF0A0','002EE570','002E4F50','002E5430','002F39C8','002F3810','002F37A8','002F2598','002F3640','002F2D70','002F2810','002F3030','003177F0','002F4260','002F4330','00317A08']:
    path=next((root/'local/output').glob('sub_'+prefix+'_*.cpp'))
    command.append(str(write_scalar_fp_oracle(path,folder/path.name)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_weather_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-weather-objects')
run=subprocess.run([str(binary),str(root/'local/disc/SLUS_207.72')],check=False,timeout=1200,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='')
(folder/'reference.log').write_text(run.stdout+run.stderr)
run.check_returncode()
