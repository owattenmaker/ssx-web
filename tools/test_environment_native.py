#!/usr/bin/env python3
"""Direct original CPU texture, terrain-colour and filter conformance."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
root=Path(__file__).resolve().parents[1];out=root/'build/environment-oracle';out.mkdir(parents=True,exist_ok=True)
vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
incs=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
cmd=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(p)for p in incs]
cmd += [str(root/'tests/environment_reference.cpp'),str(root/'engine/environment_lighting.cpp')]
for prefix in ['002EDB20','002ED1D0','002ED338','003885E0']:
 source=next((root/'local/output').glob(f'sub_{prefix}*'));t=source.read_text()
 if prefix=='003885E0' and 'case 0x3889f0u:' not in t:
  t=t.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x3889f0u:goto env_sampler;').replace('    // 0x3889f0:','env_sampler:\n    // 0x3889f0:',1)
 cmd.append(str(write_scalar_fp_oracle(source,out/source.name,t)))
cmd += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',f]
fixture=out/'ee.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
binary=out/'environment';subprocess.run(cmd+['-o',str(binary)],check=True);subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
