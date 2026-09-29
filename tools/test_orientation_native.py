#!/usr/bin/env python3
"""Build/run the development-only original orientation conformance oracle."""
from pathlib import Path
import subprocess,zipfile,hashlib
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:
    raise ValueError('Unexpected original SSX3 executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/'tests/orientation_reference.cpp'),str(root/'engine/orientation_motion.cpp'),str(root/'engine/ground_motion.cpp')]
# PS2 SQRT.S reads Ft, unlike the translator's mistaken Fs operand.
# Original 0x31BEEC opcode0x46050044 has fd=1, ft=5, fs=0.
# Verified against PCSX2 v2.8.2 pcsx2/FPU.cpp::SQRT_S. Keep the original
# generated tree untouched; this corrected development-only copy is explicit.
original=next((root/'local/output').glob('sub_0031BE50*')).read_text()
old='ctx->pc = 0x31beecu;\n    ctx->f[1] = FPU_SQRT_S(ctx->f[0]);'
assert '// 0x31beec: 0x46050044' in original
assert original.count(old)==1
corrected=root/'local/reference/pcsx2/orientation-sincos-oracle.cpp'
write_scalar_fp_oracle(next((root/'local/output').glob('sub_0031BE50*')),corrected,original)
command.append(str(corrected))
for fn in ['0011DFE0','0031C128','0031C228','0011E098']:
    originalPath=next((root/'local/output').glob('sub_'+fn+'*'))
    command.append(str(write_scalar_fp_oracle(originalPath,root/'local/reference/pcsx2'/('orientation-fp-'+originalPath.name))))
# Stop after the isolated original ground-alignment stage; no instructions in
# that stage are replaced. Its generated function exposes resume0x13ED68.
ground=next((root/'local/output').glob('sub_0013D818*')).read_text()
end='label_13ee9c:'
assert ground.count(end)==1
isolated=root/'local/reference/pcsx2/orientation-ground-oracle.cpp'
assert ground.count('label_13eea0:')==1
assert ground.count('label_13eb94:')==1
assert ground.count('label_13eb98:')==1
write_scalar_fp_oracle(next((root/'local/output').glob('sub_0013D818*')),isolated,ground.replace(end,end+'\n    ctx->pc=0x12345678; return;').replace('label_13eea0:','label_13eea0:\n    ctx->pc=0x12345678; return;').replace('label_13eb94:','label_13eb94:\n    ctx->pc=0x12345678; return;').replace('label_13eb98:','label_13eb98:\n    ctx->pc=0x12345678; return;'))
command.append(str(isolated))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_orientation_reference';command+=['-o',str(binary)]
subprocess.run(command,check=True)
fixture=root/'local/reference/pcsx2/orientation-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-jump-30.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
