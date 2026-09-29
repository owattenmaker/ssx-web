#!/usr/bin/env python3
"""Build/run the development-only original input conformance oracle."""
from pathlib import Path
import subprocess,zipfile,hashlib,struct
from reference_input import extract_input,ACTION_NAMES
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:
    raise ValueError('Unexpected original SSX3 executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
command += [str(root/'tests/original_input_reference.cpp'),str(root/'engine/original_input.cpp'),str(root/'engine/original_command.cpp'),str(root/'engine/original_input_provider.cpp')]
for fn in ['00321298','00127848','00321108','00325250','00325450','003252E8','00325260','003252F8','00325430']:command.append(str(next((root/'local/output').glob('sub_'+fn+'*'))))
# Isolate the original driver's already-read packet conversion. Preserveevery
# instruction from327210 through3276BC, omit device RPC and rumble operations.
source=next((root/'local/output').glob('sub_00326DF0*')).read_text()
marker='    // 0x327210: 0x44800000'
assert source.count(marker)==1 and source.count('label_3276bc:')==1
source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\n        case 0x327210u: goto input_packet_stage;',1)
source=source.replace(marker,'input_packet_stage:\n'+marker).replace('label_3276bc:','label_3276bc:\n    ctx->pc=0x12345678; return;')
copy=root/'local/reference/pcsx2/input-driver-oracle.cpp';copy.write_text(source);command.append(str(copy))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_original_input_reference';command+=['-o',str(binary)]
subprocess.run(command,check=True)
fixture=root/'local/reference/pcsx2/input-oracle-ram.bin'
fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-jump-30.p2s').read('eeMemory.bin'))
expected=root/'local/reference/pcsx2/input-actions-expected.bin'
actions=extract_input(fixture.read_bytes())['actions']
expected.write_bytes(struct.pack('<27f',*(actions[name] for name in ACTION_NAMES)))
subprocess.run([str(binary),str(fixture),str(expected)],check=True,timeout=60)
