#!/usr/bin/env python3
"""Verify original control9 progress and ordered placement/reentry callbacks."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/p) for p in ['tests/reset_control_reference.cpp']]
for fn in ['0012F398','0012F588']:
    path=next((root/'local/output').glob('sub_'+fn+'*'));source=path.read_text()
    command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('reset-control-fp-'+path.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_reset_control_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-reset-control-oracle-objects')
fixture=root/'local/reference/pcsx2/reset-control-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
