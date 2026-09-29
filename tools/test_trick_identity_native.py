#!/usr/bin/env python3
"""Compare the native trick identity with original11A8C8 instructions."""
from pathlib import Path
import hashlib,subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';output=root/'local/reference/trick-identity';output.mkdir(exist_ok=True)
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/'tests/trick_identity_reference.cpp'),str(root/'engine/trick_identity.cpp')]
for name in ('0011A8C8',):
 source=next((root/'local/output').glob('sub_'+name+'*'));command.append(str(write_scalar_fp_oracle(source,output/source.name)))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_trick_identity_reference';command+=['-o',str(binary)];subprocess.run(command,check=True)
with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as archive:(output/'memory.bin').write_bytes(archive.read('eeMemory.bin'))
subprocess.run([str(binary),str(output/'memory.bin')],check=True,timeout=60)
