#!/usr/bin/env python3
"""Build/run the development-only original NPC path-candidate conformance oracle."""
from pathlib import Path
import subprocess,zipfile,hashlib
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:
    raise ValueError('Unexpected original SSX3 executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/'tests/npc_path_reference.cpp'),str(root/'engine/npc_path.cpp'),str(root/'engine/race_event.cpp'),str(root/'engine/ground_motion.cpp')]
for fn in ['0026AFB8','0026A9B0','0010D1A0','0010D870','00112588','0026A428','0026A638','00112A50','0026AC48','0026AB20','001125C0','001125A8','001125B8','0026AC88','0031C228','0010BB18','0010B980','0010BBF8','0026AA80']:
    originalPath=next((root/'local/output').glob('sub_'+fn+'*'))
    source=None
    if fn=='0010D1A0':
        source=originalPath.read_text()
        assert 'case 0x10d410u:' not in source
        source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\n        case 0x10d410u: goto npc_score_entry;',1)
        marker='    // 0x10d410:';assert source.count(marker)==1
        source=source.replace(marker,'npc_score_entry:\n'+marker)
    command.append(str(write_scalar_fp_oracle(originalPath,root/'local/reference/pcsx2'/('npc-path-fp-'+originalPath.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_npc_path_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-npc-oracle-objects')
fixture=root/'local/reference/pcsx2/npc-path-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
