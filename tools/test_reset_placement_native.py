#!/usr/bin/env python3
"""Verify11D660 placement through11DACC; leave subsequent reset callbacks external."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/p) for p in ['tests/reset_placement_reference.cpp','engine/reset_placement.cpp','engine/orientation_motion.cpp','engine/animation_motion.cpp','engine/ground_motion.cpp']]
for fn in ['0011D660','0011E098','0011DFE0','0031BE50','0031C128','0031C228']:
    path=next((root/'local/output').glob('sub_'+fn+'*'));source=path.read_text()
    if fn=='0011D660':
        marker='    // 0x11dad0:';assert source.count(marker)==1
        source=source.replace(marker,'ctx->pc=0x12345678;return;\n'+marker)
    command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('reset-placement-fp-'+path.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_reset_placement_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-reset-placement-oracle-objects')
fixture=root/'local/reference/pcsx2/reset-placement-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
