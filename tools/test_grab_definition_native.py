#!/usr/bin/env python3
"""Verify extended grab definition extraction against original150xxx getters."""
from pathlib import Path
import subprocess,zipfile,struct
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from reference_grab_lifecycle import extract_grab_lifecycle
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
names=['001500D8','001500F8','00150118','00150138','00150158','00150178','00150198','001502C8','001503F8','0014FEA8','0014A0B0','0014A0E0','0014A080','00150528','00150540']
paths=[next((root/'local/output').glob('sub_'+n+'*')) for n in names]
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in includes]
registry=''.join('void '+p.stem+'(uint8_t*,R5900Context*,PS2Runtime*);\n' for p in paths)
registry+='static void registerDefinitions(PS2Runtime&r){'+''.join(f'r.registerFunction(0x{p.stem.split("_0x")[1]},{p.stem});' for p in paths)+'}\n'
(root/'local/reference/pcsx2/grab-definition-registry.inc').write_text(registry)
command+=['-I'+str(root/'local/reference/pcsx2'),str(root/'tests/grab_definition_reference.cpp')]
for p in paths:command.append(str(write_scalar_fp_oracle(p,root/'local/reference/pcsx2'/('grab-definition-fp-'+p.name))))
command +=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_grab_definition_reference';cached_oracle_build(command+['-o',str(binary)],root/'build/original-npc-oracle-objects')
original=zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin')
for special in [0,20,24,25,28]:
    memory=bytearray(original);u=lambda a:struct.unpack_from('<I',memory,a)[0]
    game=u(u(u(0x4a30f0-0x848)+0x84)+12)
    for slot in range(6):memory[0x535b20+u(0x5305b0+slot*4)*28+18]=special
    fixture=root/'local/reference/pcsx2/grab-definition-oracle.bin';fixture.write_bytes(memory)
    result=subprocess.run([str(binary),str(fixture)],capture_output=True,text=True,timeout=60,check=True)
    if result.stderr:raise RuntimeError(result.stderr)
    expected=[]
    for slot in range(6):
        profile=extract_grab_lifecycle(memory,u(game+0x28+slot*4))['profile']
        for group,entries in enumerate([profile['grabs'],profile['tweak'],*profile['uber']]):
            for index,d in enumerate(entries):expected.append([slot,group,index,d['semantic'],d['upper_semantic'],d['score_id'],d['begin_points'],d['hold_points']])
    actual=[[int(x) for x in line.split()] for line in result.stdout.splitlines()]
    if actual!=expected:
        for a,b in zip(actual,expected):
            if a!=b:raise ValueError(f'Grab definition special{special}: original{a} native{b}')
        raise ValueError('Grab definition count')
    print(f'Special type{special}: all360 original grab animation/upper/score definitions match extractor')
