#!/usr/bin/env python3
"""Independent original 0x1163B0 (attacks with no opponent) conformance; development runtime never ships."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];out=root/'build/attack-control-oracle';out.mkdir(parents=True,exist_ok=True)
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in includes]
command += [str(root/'tests/attack_control_reference.cpp'),str(root/'engine/attack_control.cpp')]
for prefix in ['001163B0']:
 source=next((root/'local/output').glob(f'sub_{prefix}*'));command.append(str(write_scalar_fp_oracle(source,out/source.name)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
fixture=out/'ee.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
binary=out/'attack_control';cached_oracle_build(command+['-o',str(binary)],root/'build/original-npc-oracle-objects')
result=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True,timeout=120);print(result.stdout)
if result.stderr:raise RuntimeError(result.stderr)
result.check_returncode()
