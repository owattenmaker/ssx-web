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
command += [str(root/'tests/landing_motion_reference.cpp'),str(root/'engine/landing_motion.cpp'),str(root/'engine/ground_pose_motion.cpp')]
for fn in ['00139C88','00106538','00113998','0013C7A8','0013F410','0013A968','0013A8F8','0013D818']:
    originalPath=next((root/'local/output').glob('sub_'+fn+'*'))
    source=None
    if fn=='00139C88':
        source=originalPath.read_text()
        source='extern int landingOracleStage;\n'+source
        for label,stage in [('13a14c',1),('13a4c8',2),('13a4cc',2)]:
            marker='label_'+label+':'
            assert source.count(marker)==1
            source=source.replace(marker,marker+'\n    if(landingOracleStage=='+str(stage)+'){ctx->pc=0x12345678;return;}')
    if fn=='0013D818':
        source=originalPath.read_text();marker='label_13f068:';assert source.count(marker)==1
        source=source.replace(marker,marker+'\n    ctx->pc=0x12345678;return;')
    command.append(str(write_scalar_fp_oracle(originalPath,root/'local/reference/pcsx2'/('landing-motion-fp-'+originalPath.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_landing_motion_reference';command+=['-o',str(binary)]
subprocess.run(command,check=True)
fixture=root/'local/reference/pcsx2/landing-motion-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-jump-30.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
