#!/usr/bin/env python3
"""Compare rail-style and inverted awards with the complete original11A228 routine."""
from pathlib import Path
import hashlib,subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1]
assert hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()==EXPECTED_SHA1
out=root/'local/reference/trick-commit';out.mkdir(exist_ok=True)
v=root/'local/vendor/PS2Recomp';b=root/'build/ps2recomp'
names='0011A228 0011A8C8 0011B1A8 001190F0 00117948 00117990 00117908 00119310 00117638 00117900 00117708 00119898 001198D8'.split()
files=[next((root/'local/output').glob('sub_'+n+'*')) for n in names]
(out/'commit_registry.inc').write_text(''.join(f'void {p.stem}(uint8_t*,R5900Context*,PS2Runtime*);\n' for p in files)+'void registerCommit(PS2Runtime& r){'+''.join(f'r.registerFunction(0x{n},{p.stem});' for n,p in zip(names,files))+'}\n')
incs=[v/'ps2xRuntime/include',v/'ps2xRuntime/src/lib/Kernel',v/'ps2xIOP/include',b/'_deps/sse2neon-src',root/'local/output',out]
cmd=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in incs]
cmd+=[str(root/'tests/rail_commit_reference.cpp')]+[str(root/'engine'/f'{n}.cpp') for n in ['trick_commit','trick_identity','trick_history','trick_bonus','grab_score']]
cmd +=[str(write_scalar_fp_oracle(p,out/p.name)) for p in files]
cmd +=[str(b/'ps2xRuntime/libps2_runtime.a'),str(b/'_deps/raylib-build/raylib/libraylib.a'),str(b/'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',f]
binary=root/'build/ssx3_rail_commit_reference';cmd+=['-o',str(binary)];cached_oracle_build(cmd,root/'build/original-npc-oracle-objects')
(out/'memory.bin').write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(out/'memory.bin')],check=True,timeout=60)
