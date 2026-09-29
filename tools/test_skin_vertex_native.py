#!/usr/bin/env python3
"""Original VU skin vertex transform."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/skin_vertex_reference.cpp')]
from extract_fx_microcode import extract_programs
from export_irradiance import decode_bank
import struct,hashlib,json
work=root/'local/rider-lighting';work.mkdir(parents=True,exist_ok=True)
code,source=extract_programs((root/'local/disc/SLUS_207.72').read_bytes())[2]
# Pin the source program and the exact shader block, not a hand-written reference.
program=work/'irradiance-program.bin';program.write_bytes(code)
# Recompile the VU upper executor with the oracle's no-contraction flags.
# The cached runtime library may otherwise contract MADD into host FMA.
command += [str(vendor/'ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp')]
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_skin_vertex_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-skin-vertex-objects')
golden=root/'web/public/test-data/rider-position-reference.bin';golden.parent.mkdir(parents=True,exist_ok=True)
run=subprocess.run([str(binary),str(program),str(golden)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr:raise RuntimeError('Incomplete original position stage')
(work/'skin-vertex-source.json').write_text(json.dumps(dict(program=source,program_sha256=hashlib.sha256(code).hexdigest(),golden_sha256=hashlib.sha256(golden.read_bytes()).hexdigest(),records=12000,record_floats=24,scope='Program2 upper04C0..04D8 and lower04C0; supplied matrix/float vertex, no projection'),indent=2)+'\n')
