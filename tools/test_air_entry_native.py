#!/usr/bin/env python3
"""Build/run the development-only original orientation conformance oracle."""
from pathlib import Path
import subprocess,zipfile,hashlib
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:
    raise ValueError('Unexpected original SSX3 executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/'tests/air_entry_reference.cpp'),str(root/'engine/air_entry.cpp'),str(root/'engine/air_control.cpp'),str(root/'engine/ground_motion.cpp')]
for fn in ['0012E9B8','001162C8']:
    originalPath=next((root/'local/output').glob('sub_'+fn+'*'))
    command.append(str(write_scalar_fp_oracle(originalPath,root/'local/reference/pcsx2'/('air-entry-fp-'+originalPath.name))))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_air_entry_reference';command+=['-o',str(binary)]
subprocess.run(command,check=True)
fixture=root/'local/reference/pcsx2/air-entry-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-jump-30.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
