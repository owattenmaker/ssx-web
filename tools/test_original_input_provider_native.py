#!/usr/bin/env python3
"""Build/run the development-only original human input provider oracle.

Compares engine/original_input_provider.cpp against recompiled original code:
mapping VM 0x325450 with the snapshot's compiled INPUT.MAP (via getters
0x320BF0/0x320C48), grab selection 0x1276F0, Uber identity 0x127848 and the
complete provider 0x127998 for every controller state. The native port is built
twice: hardware toward-zero rounding, and the explicit software_float path used
by WebAssembly (SSX_ORIGINAL_INPUT_SOFTWARE_FLOAT, run under nearest rounding).
"""
from pathlib import Path
import hashlib,subprocess,zipfile
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1]
if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:
    raise ValueError('Unexpected original SSX3 executable')
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
base=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]
objects=root/'build/ssx3_original_input_provider_objects';objects.mkdir(parents=True,exist_ok=True)
original=[]
for fn in ['00127848','001276F0','0011FEE8','00320BF0','00320C48','00320FA8','00321108','00325250','00325260','003252E8','003252F8','00325430','00325450']:
    source=next((root/'local/output').glob('sub_'+fn+'*.cpp'));target=objects/(source.stem+'.o')
    if not target.exists() or target.stat().st_mtime<source.stat().st_mtime:
        subprocess.run(base+['-c',str(source),'-o',str(target)],check=True)
    original.append(str(target))
libraries=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):libraries+=['-framework',framework]
native=[root/'tests/original_input_provider_reference.cpp',root/'engine/original_input.cpp',root/'engine/original_input_provider.cpp',root/'engine/original_command.cpp']
binaries=[]
for name,flags in (('hardware',[]),('software',['-DSSX_ORIGINAL_INPUT_SOFTWARE_FLOAT'])):
    binary=root/f'build/ssx3_original_input_provider_reference_{name}'
    subprocess.run(base+flags+[str(p) for p in native]+original+libraries+['-o',str(binary)],check=True)
    binaries.append((name,binary))
fixture=root/'local/reference/pcsx2/input-oracle-ram.bin'
memory=zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-jump-30.p2s').read('eeMemory.bin')
if not fixture.exists() or fixture.read_bytes()!=memory:fixture.write_bytes(memory)
for name,binary in binaries:
    print(f'[{name} rounding path]',flush=True)
    subprocess.run([str(binary),str(fixture)],check=True,timeout=900)
