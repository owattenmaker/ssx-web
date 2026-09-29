#!/usr/bin/env python3
"""Verify native snow birth requests and retain original authored constructor data."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from extract_fx_microcode import export
root=Path(__file__).resolve().parents[1];out=root/'build/snow-emission-oracle';out.mkdir(parents=True,exist_ok=True)
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in includes]
command += [str(root/'tests/snow_particle_reference.cpp'),str(root/'engine/snow_particles.cpp')]
for prefix in ('003717C0','003710D0','003177F0','00317A08'):
    source=next((root/'local/output').glob('sub_'+prefix+'*'));command.append(str(write_scalar_fp_oracle(source,out/source.name)))
# Strict FP test-local interpreter object avoids host FMA contraction in the prebuiltdevelopment archive.
command.append(str(vendor/'ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp'))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
export(out/'vu')
fixture=out/'particle-ee.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
binary=out/'snow_particle';subprocess.run(command+['-o',str(binary)],check=True);subprocess.run([str(binary),str(out/'vu/program4.bin'),str(fixture)],check=True,timeout=120)
