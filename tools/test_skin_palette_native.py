#!/usr/bin/env python3
"""Original skin matrix palette conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
import hashlib
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected skin palette executable')
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/skin_palette_reference.cpp')]
path=next((root/'local/output').glob('sub_00386128*'));source=path.read_text()
if 'case 0x386bd0u:' not in source:
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x386bd0u: goto palette_begin;')
 source=source.replace('    // 0x386bd0:','palette_begin:\n    // 0x386bd0:')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/skin-palette-original.cpp',source)))
path=next((root/'local/output').glob('sub_00310120*'));command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/pose-matrix-original.cpp')))
path=next((root/'local/output').glob('sub_00310640*'));source=path.read_text();marker='    // 0x310790:'
if source.count(marker)!=1:raise ValueError('Missing palette stage boundary')
source=source.replace(marker,'    ctx->pc=0x310790;return;\n'+marker)
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/skin-pose-original.cpp',source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_skin_palette_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-skin-palette-objects')
import struct
captured=bytearray()
for name in ['glide','jump-31','jump-90']:
 with zipfile.ZipFile(root/f'local/reference/pcsx2/snow-jam-{name}.p2s') as archive:memory=archive.read('eeMemory.bin')
 u=lambda a:struct.unpack_from('<I',memory,a)[0]
 geometry=u(0x14701a0+0x780)
 if u(geometry+16)!=29:raise ValueError('Captured bone count changed')
 for i in range(29):
  pose=u(geometry+0x2c)+i*32;unscaled=u(geometry+0x30)+i*64;scaled=u(geometry+0x34)+i*64
  captured.extend(memory[pose:pose+32]+memory[geometry+0x140:geometry+0x150]+memory[unscaled:unscaled+64]+memory[scaled:scaled+64])
fixture=root/'local/rider-lighting/pose-matrix-fixtures.bin';fixture.write_bytes(captured)
run=subprocess.run([str(binary),str(fixture),str(root/'web/public/test-data/rider-skin-reference.bin')],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr:raise RuntimeError('Incomplete original skin palette call')

import json
artifact=root/'web/public/test-data/rider-skin-reference.bin'
(root/'local/rider-lighting/skin-gpu-reference.json').write_text(json.dumps(dict(executable_sha1=EXPECTED_SHA1,source_function='386BD0',records=12000,record_floats=28,sha256=hashlib.sha256(artifact.read_bytes()).hexdigest(),layout='count vec4; integer weights vec4; four supplied matrix columns; original output vec4',scope='Selected input columns/weights; no vertex palette selector or pose construction'),indent=2)+'\n')
