#!/usr/bin/env python3
"""Compare complete original336850 detached-board body queries with the native world adapter."""
import json,subprocess,zipfile,hashlib,re
from pathlib import Path
from original_fp_oracle import write_scalar_fp_oracle

def main():
    root=Path(__file__).resolve().parents[1];folder=root/'local/reference/terrain/crash-world-query';folder.mkdir(parents=True,exist_ok=True)
    samples=[]
    for name in ['glide','glide-120','jump-30','jump-31','jump-60','jump-90','air-spin-30']:
        state=root/f'local/reference/pcsx2/snow-jam-{name}.p2s'
        with zipfile.ZipFile(state) as z:ee=z.read('eeMemory.bin');vu=z.read('vu0MicroMem.bin')
        ep=folder/(name+'.ee');vp=folder/(name+'.vu');ep.write_bytes(ee);vp.write_bytes(vu);samples.append(dict(name=name,ee=str(ep),vu=str(vp)))
    # Probe the connector-location terrain too (A_ARA1/ARA1_B, live in the event octree): absolute
    # source-cm targets over and next to those patches (tools/terrain_probe_targets.py).
    from terrain_probe_targets import targets
    for sample in samples[:2]:sample['targets']=targets()
    manifest=folder/'samples.json';manifest.write_text(json.dumps(samples))
    functions=['00137550_0x137550','00329A90_0x329a90','00336850_0x336850','00335D78_0x335d78','00335960_0x335960','00335B90_0x335b90','00328360_0x328360','00340FA0_0x340fa0','0033CCF8_0x33ccf8','0032F650_0x32f650','0032F708_0x32f708','00329910_0x329910','00329DC8_0x329dc8','00329A28_0x329a28','00329B90_0x329b90','0032B2B8_0x32b2b8','00329590_0x329590','0032A1C0_0x32a1c0','0032AA28_0x32aa28','00356020_0x356020','0035FE10_0x35fe10','003568B0_0x3568b0','0013A7B0_0x13a7b0','003342D0_0x3342d0','00333EF8_0x333ef8','003279D0_0x3279d0','00327AC0_0x327ac0','00327B30_0x327b30','00327BB0_0x327bb0','00327C00_0x327c00','00327C68_0x327c68','00391418_0x391418','00391480_0x391480','003914F8_0x3914f8','00340970_0x340970','0032B6E0_0x32b6e0','0032E100_0x32e100','003E6574_0x3e6574','00334888_0x334888','003A6CC8_0x3a6cc8','003A6D00_0x3a6d00','0032C0F8_0x32c0f8','0032B6A8_0x32b6a8','003A6CD8_0x3a6cd8','003A6CE8_0x3a6ce8']
    vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    sources=[]
    for name in functions:
        path=write_scalar_fp_oracle(root/'local/output'/('sub_'+name+'.cpp'),folder/(name+'.cpp'))
        if name=='00340970_0x340970':
            text=path.read_text().replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x340a10u:goto landing_preference_entry;',1)
            text=text.replace('    // 0x340a10:','landing_preference_entry:\n    // 0x340a10:',1);path.write_text(text)
        if name=='0032F708_0x32f708':
            text=path.read_text()
            for pc in [0x32f760,0x32f840,0x32f8b0,0x32f8c0,0x32f8f0,0x32f990,0x32fa30,0x32fac0]:
                if f'case 0x{pc:x}u:' not in text:
                    text=text.replace('switch (ctx->pc) {',f'switch (ctx->pc) {{\ncase 0x{pc:x}u:goto body_entry_{pc:x};',1)
                    text=text.replace(f'    // 0x{pc:x}:',f'body_entry_{pc:x}:\n    // 0x{pc:x}:',1)
            path.write_text(text)
        if name=='0033CCF8_0x33ccf8':
            # The AOT generator lowered recursive JALs to goto, losing the host
            # call/return boundary. Restore only those eight original self-calls.
            text=path.read_text()
            pattern=r'ctx->pc = 0x33CCF8u;\n    goto label_33ccf8;\n    ctx->pc = (0x[0-9A-F]+u);'
            text,count=re.subn(pattern,lambda m:'ctx->pc = 0x33CCF8u;\n    sub_0033CCF8_0x33ccf8(rdram,ctx,runtime);\n    if(ctx->pc != '+m[1]+')return;\n    ctx->pc = '+m[1]+';',text)
            if count!=8:raise ValueError('Original recursive octree self-call count changed')
            path.write_text(text)
        sources.append(str(path))
    command=['clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    # Original functions are unchanged across probe-fixture edits. Cache their
    # objects by source, compiler flags, and all inline/runtime header inputs.
    header_hash=hashlib.sha256((root/'engine/original_float.hpp').read_bytes())
    for header in sorted((vendor/'ps2xRuntime/include').rglob('*.h')):header_hash.update(header.read_bytes())
    object_dir=root/'local/reference/terrain/landing/objects';object_dir.mkdir(exist_ok=True);objects=[]
    for source in sources:
        digest=hashlib.sha256(Path(source).read_bytes()+header_hash.digest()+repr(command).encode()).hexdigest()
        obj=object_dir/(digest+'.o')
        if not obj.exists():subprocess.run(command+['-c',source,'-o',str(obj)],check=True)
        objects.append(str(obj))
    command += [str(root/'tests/crash_world_query_reference.mm'),str(root/'engine/world_collision_asset.mm')]+objects
    command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['Foundation','OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
    binary=root/'build/ssx3_crash_world_query_reference';subprocess.run(command+['-o',str(binary)],check=True)
    subprocess.run([str(binary),str(root/'local/assets/native/ARA1'),str(manifest)],check=True)

if __name__=='__main__':main()
