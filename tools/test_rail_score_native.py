#!/usr/bin/env python3
"""Original rail score arithmetic and threshold-event conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/rail_score_reference.cpp')]
for address in ['00117C28','00119918','00119898','00119210']:
 path=next((root/'local/output').glob('sub_'+address+'*'));source=path.read_text()
 if address=='00117C28':
  for stop in ['117e00','117e04']:
   marker='    // 0x'+stop+':'
   assert source.count(marker)==1
   source=source.replace(marker,'ctx->pc=0x12345678;return;\n'+marker)
 command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('rail-score-fp-'+path.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_rail_score_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-rail-score-oracle-objects')
fixture=root/'local/reference/pcsx2/rail-score-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture),str(root/'local/browser-validation/rail-score-input.bin'),str(root/'local/browser-validation/rail-score-expected.bin')],check=True,timeout=60)
