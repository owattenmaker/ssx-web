#!/usr/bin/env python3
"""Verify the composed original wake physics update, excluding render colour/UV preparation."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/p) for p in ['tests/wake_physics_reference.cpp','engine/wake_row.cpp','engine/wake_input.cpp','engine/wake_control.cpp','engine/wake_render.cpp']]
for fn in ['002DD0B8','002DDD30','002DE058','002D1928','002D18B0']:
    path=next((root/'local/output').glob('sub_'+fn+'*'));source=path.read_text()
    if fn=='002DE058':
        marker='    // 0x2de0d8:';assert source.count(marker)==1
        source=source.replace(marker,'ctx->pc=0x12345678;return;\n'+marker)
    command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('wake-physics-fp-'+path.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_wake_physics_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-wake-physics-oracle-objects')
fixture=root/'local/reference/pcsx2/wake-physics-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
