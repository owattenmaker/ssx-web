#!/usr/bin/env python3
"""Original ordinary-grab state, held-tick and score request conformance."""
from pathlib import Path
import subprocess,zipfile,hashlib
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected source executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output',root/'local/reference/pcsx2']
addresses='117838 119708 1197d8 119068 1176f8 150528 150540 150118 10e098 14dc80 14dd58 1530e0 14f900 117c28 119210 117948'.split();files={};mapping={}
for a in addresses:
    path=next((root/'local/output').glob(f'sub_{int(a,16):08X}_*'),None)
    if path is None:
        names=subprocess.check_output(['rg','-l',f'// 0x{a}:',str(root/'local/output')],text=True).splitlines();assert len(names)==1;path=Path(names[0])
    files[path]=path.read_text();mapping[a]=path.stem
registry=''.join(f'void {name}(uint8_t*,R5900Context*,PS2Runtime*);\n' for name in sorted(set(mapping.values())))
registry+='static void registerScoreOriginal(PS2Runtime&r){\n'+''.join(f'r.registerFunction(0x{a},{name});\n' for a,name in mapping.items())+'}\n'
(root/'local/reference/pcsx2/grab_score_registry.inc').write_text(registry)
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command +=[str(root/'tests/grab_score_reference.cpp'),str(root/'engine/grab_score.cpp')]
for path,source in files.items():
    if path.stem.startswith('sub_00117C28'):
        marker='    // 0x117d74:';assert source.count(marker)==1;source=source.replace(marker,'    ctx->pc=0x12345678;return;\n'+marker)
        if 'case 0x117d24u:' not in source:
            marker='    // 0x117d24:';assert source.count(marker)==1;source=source.replace(marker,'grab_score_tick_entry:\n'+marker)
            source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x117d24u: goto grab_score_tick_entry;',1)
    command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('grab-score-fp-'+path.name),source)))
command +=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_grab_score_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-npc-oracle-objects')
fixture=root/'local/reference/pcsx2/grab-score-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
result=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True,timeout=90);print(result.stdout)
if result.stderr:raise RuntimeError(result.stderr)
result.check_returncode()
