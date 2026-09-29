#!/usr/bin/env python3
"""Build/run the development-only original race clock, path and event conformance oracle."""
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
command += [str(root/'tests/race_event_reference.cpp'),str(root/'engine/race_event.cpp')]
for fn in ['00113B10','00113B48','00113C20','0012A250','0026A638','0026AA80','00270AB0','001127F0','00112588','0026A428','0026AB20','0026A9B0','0026B178','00112338','00112FB0','0026AC48','0026AC88','00269F18']:
    originalPath=next((root/'local/output').glob('sub_'+fn+'*'))
    source=None
    if fn=='00269F18':
        source=originalPath.read_text()
        for address in ['26a090','26a0b8']:
            marker='    // 0x'+address+':'
            assert source.count(marker)==1
            source=source.replace(marker,'label_'+address+':\n'+marker)
            source=source.replace('        default: break;','        case 0x'+address+'u: goto label_'+address+';\n        default: break;')
    command.append(str(write_scalar_fp_oracle(originalPath,root/'local/reference/pcsx2'/('race-event-fp-'+originalPath.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_race_event_reference';command+=['-o',str(binary)]
subprocess.run(command,check=True)
fixture=root/'local/reference/pcsx2/race-event-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-jump-30.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=120)
