#!/usr/bin/env python3
"""Build/run the development-only original orientation conformance oracle."""
from pathlib import Path
import subprocess,zipfile,hashlib
from inspect_disc import EXPECTED_SHA1
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:
    raise ValueError('Unexpected original SSX3 executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/'tests/air_control_reference.cpp'),str(root/'engine/air_control.cpp'),str(root/'engine/ground_animation_control.cpp'),str(root/'engine/air_entry.cpp'),str(root/'engine/orientation_motion.cpp'),str(root/'engine/ground_motion.cpp')]
# PS2 SQRT.S reads Ft, unlike the translator's mistaken Fs operand.
# Original 0x31BEEC opcode0x46050044 has fd=1, ft=5, fs=0.
# Verified against PCSX2 v2.8.2 pcsx2/FPU.cpp::SQRT_S. Keep the original
# generated tree untouched; this corrected development-only copy is explicit.
original=next((root/'local/output').glob('sub_0031BE50*')).read_text()
old='ctx->pc = 0x31beecu;\n    ctx->f[1] = FPU_SQRT_S(ctx->f[0]);'
assert '// 0x31beec: 0x46050044' in original
assert original.count(old)==1
corrected=root/'local/reference/pcsx2/air-control-sincos-oracle.cpp'
write_scalar_fp_oracle(next((root/'local/output').glob('sub_0031BE50*')),corrected,original)
command.append(str(corrected))
for fn in ['00114CC0','00115168','00134CB0','0011E098','00133128','00135B30','001158B8','0031C228','00134DD0','00135180']:
    path=next((root/'local/output').glob('sub_'+fn+'*'))
    command.append(str(write_scalar_fp_oracle(path,root/'local/reference/pcsx2'/('air-fp-'+path.name))))
source=next((root/'local/output').glob('sub_00133308*')).read_text()
marker='    // 0x134334:'
assert source.count(marker)==1
source=source.replace(marker,'    ctx->pc=0x12345678; return;\n'+marker)
# Generated file exposes every label only when configured;addthispurestageentry.
if 'case 0x13366cu:' not in source:
    source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\n        case 0x13366cu: goto air_angular_start;',1)
    start='    // 0x13366c:';assert source.count(start)==1;source=source.replace(start,'air_angular_start:\n'+start)
if 'case 0x1333e0u:' not in source:
    source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\n        case 0x1333e0u: goto air_prefix_start;',1)
    start='    // 0x1333e0:';assert source.count(start)==1;source=source.replace(start,'air_prefix_start:\n'+start)
isolated=root/'local/reference/pcsx2/air-angular-oracle.cpp';write_scalar_fp_oracle(next((root/'local/output').glob('sub_00133308*')),isolated,source);command.append(str(isolated))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_air_control_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/"build/original-npc-oracle-objects")
fixture=root/'local/reference/pcsx2/air-control-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-jump-30.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
