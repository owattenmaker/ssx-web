#!/usr/bin/env python3
"""Original Tricky timer HUD slot9 update conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/boost_flash_hud_reference.cpp')]
path=next((root/'local/output').glob('sub_00117FE0*'));source=path.read_text();marker='    // 0x118d50:';assert source.count(marker)==1;source=source.replace(marker,'ctx->pc=0x12345678;return;\n'+marker)
marker='    // 0x118d54:';assert source.count(marker)==1;source=source.replace(marker,'ctx->pc=0x12345678;return;\n'+marker)
command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('boost-flash-hud-fp-'+path.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_boost_flash_hud_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-boost-flash-hud-oracle-objects')
fixture=root/'local/reference/pcsx2/boost-flash-hud-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
run=subprocess.run([str(binary),str(fixture)],check=True,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='')
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original Tricky timer update call graph')
