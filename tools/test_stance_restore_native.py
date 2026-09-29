#!/usr/bin/env python3
"""Complete115640 physical/root state and animation request conformance."""
from pathlib import Path
import subprocess,zipfile,hashlib
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected source executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command +=[str(root/p) for p in ['tests/stance_restore_reference.cpp','engine/stance_restore.cpp','engine/orientation_motion.cpp','engine/ground_motion.cpp','engine/animation_motion.cpp']]
for name in ['00115640','0011DFE0','0011E098','0011FE98','00311B48','00311BF0','00314760','0031BE50','00116930']:
    path=next((root/'local/output').glob('sub_'+name+'*'))
    command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('stance-restore-fp-'+path.name))))
command +=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_stance_restore_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-npc-oracle-objects')
fixture=root/'local/reference/pcsx2/stance-restore-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
result=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True,timeout=90)
print(result.stdout)
if result.stderr:raise RuntimeError(result.stderr)
result.check_returncode()
