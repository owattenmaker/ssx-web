#!/usr/bin/env python3
"""Full original10A768 provider vs native on authentic retained participant state."""
from pathlib import Path
import subprocess,zipfile,hashlib,re,sys,struct
air_mode="--air" in sys.argv
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
assert hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()==EXPECTED_SHA1
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output',root/'local/reference/pcsx2']
addresses='10a768 10a960 10c0a8 10c140 100348 100610 31c228 10aa70 26aa80 26ab20 10bfa8 10c1d0 10def0 10deb0 10d8f8 113128 120090 10bd10 10d9e8 31c040 100680 1009e0 100f88 10bb18 10b980 10bbf8 112a50 26afb8 26a9b0 26a428 26a638 26ac48 10d1a0 10d410 10d870 112588 10fc30 11fe98 11fee8 10da10 10dbf0 140910 1408f0 140bc0 140bc8 140b80 1298c8 311ae8 3115c8 155a50 155b50 14a080 14a0e0 1448d8 145750 145970 146e98 147398 147798 144c78 14dc80 14dd58 317810 317a08 3e6448 1530e0 115d48 10b250 10b590 10b790 10b0e8 135cb0 135db0 10cad8 10c258 10c320 10c3b8 10cbe0 10cd20 10cf68 120038 149690 1495a8 1477e8 144be0 1500d8 150138 150198 14fea8 14a0b0 312820 104cf8 311710 147f78 14f900'.split()
files={}; mapping={}
for address in addresses:
    path=next((root/'local/output').glob(f'sub_{int(address,16):08X}_*'),None)
    if path is None:
        names=subprocess.check_output(['rg','-l',f'// 0x{address}:',str(root/'local/output')],text=True).splitlines()
        assert len(names)==1,(address,names)
        path=Path(names[0])
    mapping[address]=path.stem;files[path]=path.read_text()
registry=''.join(f'void {name}(uint8_t*,R5900Context*,PS2Runtime*);\n' for name in sorted(set(mapping.values())))
registry+='static void registerProviderOriginal(PS2Runtime&rt){\n'+''.join(f'rt.registerFunction(0x{a},{fn});\n' for a,fn in mapping.items())+'}\n'
(root/'local/reference/pcsx2/npc_provider_registry.inc').write_text(registry)
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command +=[str(root/p) for p in [('tests/npc_air_reference.cpp' if air_mode else 'tests/npc_provider_reference.cpp'),'engine/npc_air.cpp','engine/npc_input.cpp','engine/npc_path.cpp','engine/ground_motion.cpp','engine/ground_pose_motion.cpp','engine/race_event.cpp']]
for path,source in files.items():
    for address,name in mapping.items():
        if name!=path.stem or f'case 0x{address}u:' in source:continue
        marker=f'    // 0x{address}:'
        if marker not in source:raise ValueError(address)
        label=f'provider_entry_{address}'
        source=source.replace('switch (ctx->pc) {',f'switch (ctx->pc) {{\ncase 0x{address}u: goto {label};',1)
        source=source.replace(marker,f'{label}:\n'+marker,1)
    command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('npc-provider-fp-'+path.name),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/('build/ssx3_npc_air_reference' if air_mode else 'build/ssx3_npc_provider_reference');command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-npc-oracle-objects')
if air_mode:
    from reference_npc_air import extract_npc_grabs
    memory=zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin')
    fixture=root/'local/reference/pcsx2/npc-air-oracle-ram.bin';fixture.write_bytes(memory)
    def u(at):return struct.unpack_from('<I',memory,at)[0]
    game=u(u(u(0x4a30f0-0x848)+0x84)+12)
    data=bytearray()
    for slot in range(1,6):
        profile=extract_npc_grabs(memory,u(game+0x28+slot*4))
        data+=struct.pack('<fi',profile['grab_stat'],profile['event_id'])
        for group in [profile['normal'],profile['tweak'],*profile['uber']]:
            for item in group:data+=struct.pack('<iff',item['semantic'],item['marker1'],item['marker2'])
    profiles=root/'local/reference/pcsx2/npc-air-oracle-catalog.bin';profiles.write_bytes(data)
    completed=subprocess.run([str(binary),str(fixture),str(profiles)],text=True,capture_output=True,timeout=120)
    print(completed.stdout)
    if completed.stderr:raise RuntimeError(completed.stderr)
    completed.check_returncode()
    sys.exit(0)
for name in ['snow-jam-glide.p2s','snow-jam-glide-1.p2s']:
    fixture=root/'local/reference/pcsx2/npc-provider-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2'/name).read('eeMemory.bin'))
    completed=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True,timeout=60,check=True)
    if completed.stderr: raise RuntimeError(completed.stderr)
    result=completed.stdout
    (root/'local/reference/pcsx2'/(name+'.npc-provider.txt')).write_text(result)
    print(name+':\n'+result)

fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
completed=subprocess.run([str(binary),str(fixture),'upper-reaction'],text=True,capture_output=True,timeout=60,check=True)
if completed.stderr:raise RuntimeError(completed.stderr)
(root/'local/reference/pcsx2/glide-upper-reaction.txt').write_text(completed.stdout)
print(completed.stdout)
