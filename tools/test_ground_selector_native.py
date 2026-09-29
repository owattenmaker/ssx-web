#!/usr/bin/env python3
"""Complete original131878..131C04 normal animation selector conformance."""
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
command +=[str(root/p) for p in ['tests/ground_selector_reference.cpp','engine/ground_animation_control.cpp','engine/ground_motion.cpp']]
path=next((root/'local/output').glob('sub_00131620*'));source=path.read_text()
marker='    // 0x131c08:';assert source.count(marker)==1;source=source.replace(marker,'    ctx->pc=0x12345678;return;\n'+marker)
if 'case 0x131878u:' not in source:
    marker='    // 0x131878:';assert source.count(marker)==1;source=source.replace(marker,'ground_selector_entry:\n'+marker)
    source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x131878u: goto ground_selector_entry;',1)
command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('ground-selector-fp-'+path.name),source)))
command +=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_ground_selector_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-npc-oracle-objects')
fixture=root/'local/reference/pcsx2/ground-selector-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
result=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True,timeout=90);print(result.stdout)
if result.stderr:raise RuntimeError(result.stderr)
result.check_returncode()
