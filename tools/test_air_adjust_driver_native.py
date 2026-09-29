#!/usr/bin/env python3
"""Complete original kind11/1043F8 clocks,weights,initialization and fade proof."""
from pathlib import Path
import subprocess,zipfile,hashlib
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command +=[str(root/'tests/air_adjust_driver_reference.cpp'),str(root/'engine/animation_sequence.cpp')]
for name in ['001043F8','00313C50','00313CF0','00313D28','00313D40','003135B0','00313800']:
    path=next((root/'local/output').glob('sub_'+name+'*'));command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('air-adjust-driver-fp-'+path.name))))
command +=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_air_adjust_driver_reference';cached_oracle_build(command+['-o',str(binary)],root/'build/original-npc-oracle-objects')
fixture=root/'local/reference/pcsx2/air-adjust-driver-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
result=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True,timeout=90);print(result.stdout)
if result.stderr:raise RuntimeError(result.stderr)
result.check_returncode()
