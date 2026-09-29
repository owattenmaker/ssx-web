#!/usr/bin/env python3
"""Complete original rider lighting assembly conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/rider_irradiance_reference.cpp')]
path=next((root/'local/output').glob('sub_001220D8*'));source=path.read_text()
for pc in ['1220d8']:
 if 'case 0x'+pc+'u:' in source: continue
 source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x'+pc+'u: goto reward_'+pc+';')
 source=source.replace('    // 0x'+pc+':','reward_'+pc+':\n    // 0x'+pc+':')
command.append(str(write_scalar_fp_oracle(path,root/'local/event-activation/rider-irradiance-original.cpp',source)))
for prefix in ['00389260','00389CB8','00389840','00389620','0031BE50','0031C128','0031C228','0038A6A8','0038A530','0038A618','00389520','00389308','00389558']:
 path=next((root/'local/output').glob('sub_'+prefix+'*'))
 command.append(str(write_scalar_fp_oracle(path,root/('local/event-activation/rider-irradiance-'+prefix+'.cpp'))))
# Point query's generated inline zero-divisor branch must retain EE saturation.
query=root/'local/event-activation/rider-irradiance-0038A618.cpp'
text=query.read_text();old='float result=left/right;'
if text.count(old)!=1:raise ValueError('Unexpected divide helper')
text=text.replace(old,'float result=right==0 ? std::bit_cast<float>(0x7f7fffffu|((std::bit_cast<uint32_t>(left)^std::bit_cast<uint32_t>(right))&0x80000000u)) : left/right;')
old='ctx->f[0] = copysignf(INFINITY, ctx->f[1] * 0.0f);'
if text.count(old)!=1:raise ValueError('Unexpected point divide branch')
query.write_text(text.replace(old,'ctx->f[0] = originalOracleScalarDivide(ctx->f[1],ctx->f[0]);'))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_rider_irradiance_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-rider-irradiance-objects')
import hashlib
from inspect_disc import EXPECTED_SHA1
elf=root/'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected irradiance executable')
run=subprocess.run([str(binary),str(elf)],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original rider lighting call graph')
