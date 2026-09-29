#!/usr/bin/env python3
"""Compare native scene collision against original code using captured body poses."""
import json,subprocess,zipfile,struct
from pathlib import Path
from reference_probes import riders
from reference_scalar_lowering import scalar_oracle_copy

def main():
    root=Path(__file__).resolve().parents[1];folder=root/'local/reference/terrain/live-world';folder.mkdir(parents=True,exist_ok=True)
    samples=[]
    for name in ['glide','glide-120','turn-left-30','ready']:
        state=root/f'local/reference/pcsx2/snow-jam-{name}.p2s'
        with zipfile.ZipFile(state) as z:ee=z.read('eeMemory.bin');vu=z.read('vu0MicroMem.bin')
        ep=folder/(name+'.ee.bin');vp=folder/(name+'.vu.bin');ep.write_bytes(ee);vp.write_bytes(vu)
        samples.extend(dict(state=str(state),ee=str(ep),vu=str(vp),rider=int(r['address'],16)) for r in riders(ee))
    # Two independently located loaded type3 instances, with the body translated
    # to their authored bounds centers. Geometry and transforms remain original.
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:memory=z.read('eeMemory.bin')
    package=json.loads((root/'local/assets/native/ARA1/world_collision.json').read_text())
    human=next(x for x in samples if x['state'].endswith('snow-jam-glide.p2s'))
    shape=struct.unpack_from('<I',memory,human['rider']+0xaa0)[0];center=struct.unpack_from('<3f',memory,shape+16)
    for instance in package['instances']:
        descriptor=package['bindings'][str(instance['track'])]['descriptors'][instance['collision_descriptor']]
        if descriptor['type']!=3 or descriptor['flags']&0x40000000:continue
        needle=struct.pack('<I',instance['rid']*256+instance['track']);at=0
        while True:
            at=memory.find(needle,at)
            if at<0:break
            obj=at-0x78;at+=4
            if obj<0 or obj+0x90>len(memory):continue
            ptr=struct.unpack_from('<I',memory,obj+0x88)[0]
            if not 0<ptr<len(memory)-16 or struct.unpack_from('<3I',memory,ptr)!=(3,descriptor['flags'],descriptor['resource08']):continue
            offset=[(a+b)*.5-c for a,b,c in zip(instance['bounds_min_cm'],instance['bounds_max_cm'],center)]
            samples.append(dict(human,instance=obj,offset=offset))
    manifest=folder/'samples.json';manifest.write_text(json.dumps(samples))
    vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    functions=['0032E100_0x32e100','00327F18_0x327f18','0032CDB0_0x32cdb0','0032DF28_0x32df28','0032B620_0x32b620','003A6CD8_0x3a6cd8','003A6CE8_0x3a6ce8','003E6574_0x3e6574','00334888_0x334888','0032F650_0x32f650','0032F708_0x32f708','00329910_0x329910','00329DC8_0x329dc8','00329A28_0x329a28','00329B90_0x329b90','0032B2B8_0x32b2b8','00329590_0x329590','003A6CC8_0x3a6cc8','003A6D00_0x3a6d00','0032C0F8_0x32c0f8','0032A1C0_0x32a1c0','0032AA28_0x32aa28','0032B6A8_0x32b6a8']
    binary=root/'build/world_ray_live_reference'
    command=['clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    sources=[]
    for name in functions:
        path=scalar_oracle_copy(root/'local/output'/('sub_'+name+'.cpp'),folder/('policy-'+name+'.cpp'))
        if name=='0032F708_0x32f708':
            text=path.read_text()
            entries=[0x32f840,0x32f8b0,0x32fac0,0x32f760,0x32f8f0,0x32f990,0x32fa30]
            text=text.replace('switch (ctx->pc) {','switch (ctx->pc) {'+''.join(f'\n        case 0x{pc:x}u: goto original_entry_{pc:x};' for pc in entries),1)
            for pc in entries:
                marker=f'    // 0x{pc:x}:'
                assert marker in text
                text=text.replace(marker,f'original_entry_{pc:x}:\n'+marker,1)
            path=folder/'original-sphere-query-entrypoints.cpp';path.write_text(text)
        sources.append(str(path))
    command+=[str(root/'tests/world_ray_live_reference.mm'),str(root/'engine/world_collision_asset.mm')]+sources
    command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['Foundation','OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
    subprocess.run(command+['-o',str(binary)],check=True)
    subprocess.run([str(binary),str(root/'local/assets/native/ARA1/world_collision.json'),str(manifest)],check=True)

if __name__=='__main__':main()
