#!/usr/bin/env python3
"""Direct original19-property environment blend conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
root=Path(__file__).resolve().parents[1];out=root/'build/environment-properties-oracle';out.mkdir(parents=True,exist_ok=True)
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
incs=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
cmd=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in incs]
cmd += [str(root/'tests/environment_properties_reference.cpp')]
for prefix in ['002BCBD0','002C0408','002C0A10','002BAF90','002C1CD8']:
 source=next((root/'local/output').glob(f'sub_{prefix}*'));t=source.read_text()
 cmd.append(str(write_scalar_fp_oracle(source,out/source.name,t)))
cmd += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',f]
fixture=out/'ee.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
binary=out/'environment';subprocess.run(cmd+['-o',str(binary)],check=True);subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
