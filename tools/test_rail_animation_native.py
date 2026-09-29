#!/usr/bin/env python3
"""Conformance for the original rail animation drivers (engine/rail_animation.hpp).

Builds tests/rail_animation_reference.cpp against oracle-corrected copies of the
recompiled 0x104238/0x103CC8 kind-5 chain, 0x1326C8, 0x104660 and 0x1042E0/0x103BE0,
0x136268 and 0x136508, then runs it on the Snow Jam glide EE memory so the real
lookup leaves, animation bank durations and control-12 record table are used.
"""
import struct,subprocess,sys,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from animation_bank import read_bank
from export_animation_samples import original_duration
from original_fp_oracle import write_scalar_fp_oracle
r=Path(__file__).resolve().parents[1];v=r/'local/vendor/PS2Recomp';b=r/'build/ps2recomp';includes=[v/'ps2xRuntime/include',v/'ps2xRuntime/src/lib/Kernel',v/'ps2xIOP/include',b/'_deps/sse2neon-src',r/'local/output']
cmd=['xcrun','clang++','-std=c++20','-O2','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p)for p in includes]+[str(r/'tests/rail_animation_reference.cpp'),str(r/'engine/animation_sequence.cpp')]
for n in ['00104238','00103CC8','003135B0','003139A8','00313C50','00313CF0','00313D28','00313D40','00313800','001326C8','00104660','001042E0','00103BE0','00136268','00136508','00312AA0','001446A0']:
    out=r/'build'/('rail-oracle-'+n+'.cpp');write_scalar_fp_oracle(next((r/'local/output').glob('sub_'+n+'*')),out);cmd.append(str(out))
cmd+=[str(b/'ps2xRuntime/libps2_runtime.a'),str(b/'_deps/raylib-build/raylib/libraylib.a'),str(b/'ps2xIOP/libps2_iop.a')]
for name in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',name]
exe=r/'build/ssx3_rail_animation_reference';cmd+=['-o',str(exe)];subprocess.run(cmd,check=True)
memory=zipfile.ZipFile(r/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin')
fixture=r/'build/rail-oracle-ram.bin';fixture.write_bytes(memory)
bank=read_bank((r/'local/assets/animation/ps2/basic.afl').read_bytes())
packed=struct.unpack_from('<I',memory,0x4a30f0+0xd8c)[0]+0x1030
args=[]
for leaf in [*range(63,72),346,347,352,353,362,363,371,372,380,381,389,390]:
    clip=struct.unpack_from('<I',memory,packed+leaf*4)[0]
    if clip&255:raise ValueError('Rail leaf outside the basic bank')
    duration=original_duration(bank['animations'][clip>>8]['frame_count_field'])
    args.append(f"{leaf}={struct.unpack('<I',struct.pack('<f',duration))[0]:x}")
subprocess.run([str(exe),str(fixture),*args],check=True,timeout=300)
