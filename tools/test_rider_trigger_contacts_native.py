#!/usr/bin/env python3
"""Original rider non-solid contact membership conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/rider_trigger_contacts_reference.cpp')]
path=next((root/'local/output').glob('sub_00108A48*'));source=path.read_text();source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x108c28u: goto pickup_entry;');source=source.replace('    // 0x108c28:','pickup_entry:\n    // 0x108c28:');patched=root/'local/browser-pickups/trigger-membership-original.cpp';patched.write_text(source);command.append(str(patched))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_rider_trigger_contacts_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-rider-trigger-objects')
run=subprocess.run([str(binary)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original Tricky timer update call graph')
