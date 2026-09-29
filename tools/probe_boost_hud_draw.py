#!/usr/bin/env python3
"""Capture original boost gauge geometry with only GPU submission intercepted."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];v=root/'local/vendor/PS2Recomp';b=root/'build/ps2recomp';out=root/'local/browser-ui/hud';out.mkdir(parents=True,exist_ok=True)
ids=['0021D1A0','001E91F8','001E9220','001E9290','001F10F8','0021E750','0021E7A8'];paths=[next((root/'local/output').glob('sub_'+x+'*')) for x in ids]
(out/'boost_draw_registry.inc').write_text(''.join(f'void {p.stem}(uint8_t*,R5900Context*,PS2Runtime*);\n' for p in paths)+'void registerBoostDraw(PS2Runtime&r){'+''.join(f'r.registerFunction(0x{x},{p.stem});' for x,p in zip(ids,paths))+'}\n')
incs=[v/'ps2xRuntime/include',v/'ps2xRuntime/src/lib/Kernel',v/'ps2xIOP/include',b/'_deps/sse2neon-src',root/'local/output',out,root/'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf']
cmd=['clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in incs]+[str(root/'tests/boost_hud_draw_probe.cpp')]
cmd += [str(write_scalar_fp_oracle(p,out/('draw-'+p.name),p.read_text())) for p in paths]
cmd +=[str(b/'ps2xRuntime/libps2_runtime.a'),str(b/'_deps/raylib-build/raylib/libraylib.a'),str(b/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',framework]
binary=root/'build/boost-hud-draw-probe';cmd+=['-o',str(binary)];cached_oracle_build(cmd,root/'build/boost-hud-draw-objects')
memory=out/'draw-memory.bin';memory.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(memory),str(out/'boost-draws.json')],check=True,timeout=60)
