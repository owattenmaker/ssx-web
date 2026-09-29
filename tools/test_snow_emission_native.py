#!/usr/bin/env python3
"""Verify native snow birth requests and retain original authored constructor data."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
root=Path(__file__).resolve().parents[1];out=root/'build/snow-emission-oracle';out.mkdir(parents=True,exist_ok=True)
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in includes]
command += [str(root/'tests/snow_emission_reference.cpp'),str(root/'engine/snow_emission.cpp')]
for prefix in ['002E1F70','002DFE88','002E1120','002DE398','002DE4A8','002E0EE8','002E1598','002DF920','002E1A80','002E02B8']:
    source=next((root/'local/output').glob(f'sub_{prefix}*'));text=source.read_text()
    if prefix=='002DE4A8':
        marker='    // 0x2df180:';assert text.count(marker)==1;text=text.replace(marker,'    ctx->pc=0x12345678;return;\n'+marker)
    if prefix=='002DF920':
        if 'case 0x2df960u:' not in text:text=text.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x2df960u:goto snow_prefix;')
        text=text.replace('    // 0x2df960:','snow_prefix:\n    // 0x2df960:',1)
        text=text.replace('    // 0x2dfb64:','    ctx->pc=0x12345678;return;\n    // 0x2dfb64:',1)
        text=text.replace('    // 0x2dfb60:','    ctx->pc=0x12345678;return;\n    // 0x2dfb60:',1)
    command.append(str(write_scalar_fp_oracle(source,out/source.name,text)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
fixture=out/'ee.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
binary=out/'snow_emission';subprocess.run(command+['-o',str(binary)],check=True);subprocess.run([str(binary),str(fixture),str(out/'original-dynamic-spray-profiles.bin')],check=True,timeout=60)
