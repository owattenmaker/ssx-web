#!/usr/bin/env python3
"""Capture original orb sprite across phase/palette values before its glow pass."""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];v=root/'local/vendor/PS2Recomp';b=root/'build/ps2recomp';out=root/'local/browser-ui/hud';out.mkdir(parents=True,exist_ok=True)
ids=['001E9A30','001E91A8','001E91F8','001E9220','001F10F8','0021E750','0021E7A8'];paths=[next((root/'local/output').glob('sub_'+x+'*')) for x in ids]
(out/'boost_orb_registry.inc').write_text(''.join(f'void {p.stem}(uint8_t*,R5900Context*,PS2Runtime*);\n' for p in paths)+'void registerBoostOrb(PS2Runtime&r){'+''.join(f'r.registerFunction(0x{x},{p.stem});' for x,p in zip(ids,paths))+'}\n')
incs=[v/'ps2xRuntime/include',v/'ps2xRuntime/src/lib/Kernel',v/'ps2xIOP/include',b/'_deps/sse2neon-src',root/'local/output',out,root/'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf']
cmd=['clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in incs]+[str(root/'tests/boost_orb_probe.cpp')]
for p in paths:
 source=p.read_text()
 if '001E9A30' in p.name:
  marker='    // 0x1efdb4:';assert source.count(marker)==1
  source=source.replace(marker,'ctx->pc=0x12345678;return;\n'+marker)
 cmd.append(str(write_scalar_fp_oracle(p,out/('orb-'+p.name),source)))
cmd +=[str(b/'ps2xRuntime/libps2_runtime.a'),str(b/'_deps/raylib-build/raylib/libraylib.a'),str(b/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',framework]
binary=root/'build/boost-orb-probe';cmd+=['-o',str(binary)];cached_oracle_build(cmd,root/'build/boost-orb-objects')
memory=out/'draw-memory.bin';memory.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
run=subprocess.run([str(binary),str(memory),str(out/'boost-orb.json')],capture_output=True,text=True,check=True,timeout=60)
print(run.stdout,end='');print(run.stderr,end='')
if 'missing-target' in run.stderr or 'missing-target' in run.stdout: raise RuntimeError('Incomplete original call graph; capture is invalid')
