#!/usr/bin/env python3
"""Original rider spatial-query bounds conformance."""
from pathlib import Path
import subprocess,hashlib,struct
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
from inspect_disc import EXPECTED_SHA1
elf=(root/'local/disc/SLUS_207.72').read_bytes()
if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original bounds executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/rider_query_bounds_reference.cpp')]
path=next((root/'local/output').glob('sub_0011E150*'));source=path.read_text()
for pc in ['11e150']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/rider-query-bounds-original.cpp',source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_rider_query_bounds_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-rider-query-bounds-objects')
import json
capture=root/'local/camera-continuous/jump'
rows=[]
for path in sorted(capture.glob('tick-*.json')):
 row=json.loads(path.read_text())['cameras'][1]['rider_words']
 words=[]
 for offset in [0x110,0x1a0,0x1b0,0x1c0,0x400,0x410,0x420]:words.extend(row[offset//4:offset//4+4])
 rows.append(struct.pack('<28I',*words))
fixture=root/'local/rider-lighting/rider-bounds-capture.bin';fixture.write_bytes(b''.join(rows))
run=subprocess.run([str(binary),str(fixture)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original rider-query-bounds call graph')
