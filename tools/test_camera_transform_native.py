#!/usr/bin/env python3
"""Original camera transform conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/camera_transform_reference.cpp')]
for prefix in ['00166F90','0031B748','0031B7A8','0031BE50','003956B0','0015E668']:
 path=next((root/'local/output').glob('sub_'+prefix+'*'))
 source=path.read_text()
 if prefix=='003956B0':
  source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x395750u: goto camera_view_begin;')
  source=source.replace('    // 0x395750:','camera_view_begin:\n    // 0x395750:')
 if prefix=='0015E668':
  source=source.replace('    // 0x15eaec:','    ctx->pc=0x15eaec;return;\n    // 0x15eaec:')
 command.append(str(write_scalar_fp_oracle(path,root/('local/event-activation/camera-transform-'+prefix+'.cpp'),source)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_camera_transform_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-camera-transform-objects')
import hashlib
from inspect_disc import EXPECTED_SHA1
elf=root/'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected irradiance executable')
subprocess.run(['python3',str(root/'tools/audit_rider_view_probe.py')],check=True)
run=subprocess.run([str(binary),str(elf),str(root/'local/rider-lighting/camera-view-snapshot-fixtures.bin')],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original camera transform call graph')
