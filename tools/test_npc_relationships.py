#!/usr/bin/env python3
"""Build/run the development-only original NPC relationship-data conformance oracle."""
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
command += [str(root/'tests/npc_relationship_reference.cpp')]
for fn in ['00155B50','0014A080','0014A0E0','001448D8','00145750','00145970','00146E98','00147398','00147798','00144C78']:
    originalPath=next((root/'local/output').glob('sub_'+fn+'*'))
    command.append(str(write_scalar_fp_oracle(originalPath,root/'local/reference/pcsx2'/('npc-relationship-fp-'+originalPath.name))))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
binary=root/'build/ssx3_npc_relationship_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-npc-oracle-objects')
from reference_npc import extract_npc_relationships
for state in ['snow-jam-ready.p2s','snow-jam-glide.p2s','snow-jam-charge-long-240.p2s']:
    memory=zipfile.ZipFile(root/'local/reference/pcsx2'/state).read('eeMemory.bin')
    fixture=root/'local/reference/pcsx2/npc-relationship-oracle-ram.bin';fixture.write_bytes(memory)
    actual=[[int(x) for x in line.split()] for line in subprocess.check_output([str(binary),str(fixture)],text=True,timeout=30).splitlines()]
    expected=extract_npc_relationships(memory)['scores']
    if actual!=expected: raise ValueError(f'Original NPC relationship mismatch: {state}: {actual} != {expected}')
    print(f'{state}: all36 original155B50 relationship scores match data extractor')
