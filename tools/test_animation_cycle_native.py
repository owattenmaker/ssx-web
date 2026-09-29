#!/usr/bin/env python3
"""Conformance for original five-way ground-cycle selection and clocks."""
import subprocess,zipfile
from pathlib import Path
from original_fp_oracle import write_scalar_fp_oracle
r=Path(__file__).resolve().parents[1];v=r/'local/vendor/PS2Recomp';b=r/'build/ps2recomp';includes=[v/'ps2xRuntime/include',v/'ps2xRuntime/src/lib/Kernel',v/'ps2xIOP/include',b/'_deps/sse2neon-src',r/'local/output']
cmd=['xcrun','clang++','-std=c++20','-O2','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p)for p in includes]+[str(r/'tests/animation_cycle_reference.cpp'),str(r/'engine/animation_cycle.cpp'),str(r/'engine/animation_sequence.cpp')]
for n in ['00103E28','00103CC8','003135B0','003139A8','00313CF0','00313D28','00313D40','00313800','00313A20','00104358']:
    out=r/'build'/('cycle-oracle-'+n+'.cpp');write_scalar_fp_oracle(next((r/'local/output').glob('sub_'+n+'*')),out);cmd.append(str(out))
air_source=next((r/'local/output').glob('sub_00133308*'));source=air_source.read_text()
assert source.count('label_134c3c:')==1
source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x134b80u:goto label_134b80;')
source=source.replace('label_134c3c:','label_134c3c:\n ctx->pc=0x12345678;return;')
out=r/'build/cycle-air-choice-oracle.cpp';write_scalar_fp_oracle(air_source,out,source);cmd.append(str(out))
cmd += [str(b/'ps2xRuntime/libps2_runtime.a'),str(b/'_deps/raylib-build/raylib/libraylib.a'),str(b/'ps2xIOP/libps2_iop.a')]
for name in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',name]
exe=r/'build/ssx3_animation_cycle_reference';cmd+=['-o',str(exe)];subprocess.run(cmd,check=True)
fixture=r/'build/cycle-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(r/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'));subprocess.run([str(exe),str(fixture)],check=True,timeout=60)
