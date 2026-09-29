#!/usr/bin/env python3
"""Original fog projection and color preparation conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/fog_projection_reference.cpp')]
path=next((root/'local/output').glob('sub_0036A428*'));source=path.read_text()
for pc in ['36a428']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
marker='    // 0x36a61c:'
if source.count(marker)!=1:raise ValueError('Missing fog projection boundary')
source=source.replace(marker,'    ctx->pc=0x36a61c;return;\n'+marker)
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/fog-projection-original.cpp',source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_fog_projection_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-fog-projection-objects')
import struct,hashlib,json
memory=zipfile.ZipFile(root/'local/reference/pcsx2/countdown-1.p2m2_SaveState.p2s').read('eeMemory.bin')
context=struct.unpack_from('<I',memory,0x4a30f0-0x854)[0]
projection=struct.unpack_from('<16f',memory,context+0x5930)
near,far=struct.unpack_from('<2f',memory,context+0x6c64)
color=struct.unpack_from('<3f',memory,context+0x6c78)
fixture=root/'local/event-activation/fog-projection-captured.bin';fixture.write_bytes(struct.pack('<21f',*projection,near,far,*color))
fixture.with_suffix('.json').write_text(json.dumps(dict(ee_sha256=hashlib.sha256(memory).hexdigest(),render_context=hex(context),projection=projection,near=near,far=far,color=color),indent=2)+'\n')
run=subprocess.run([str(binary),str(fixture)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original fog depth-table call graph')
