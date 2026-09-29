#!/usr/bin/env python3
"""Compare standalone native pair kernels with original instructions, no game patches."""
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy

def main():
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/rider-pair';work.mkdir(parents=True,exist_ok=True)
    sources=[]
    for name in ['sub_00329F98_0x329f98.cpp','sub_00107E70_0x107e70.cpp','sub_0011FF98_0x11ff98.cpp','sub_00107888_0x107888.cpp','sub_0010F560_0x10f560.cpp','sub_0031C228_0x31c228.cpp']:
        path=scalar_oracle_copy(root/'local/output'/name,work/name)
        if '107E70' in name:
            text=path.read_text().replace('// Function:','extern void capturePairGate(R5900Context*); extern bool stopPairBeforeReaction;\n// Function:',1)
            text=text.replace('ctx->pc = 0x108048u;','ctx->pc = 0x108048u; capturePairGate(ctx);')
            text=text.replace('ctx->pc = 0x108084u;','ctx->pc = 0x108084u; if(stopPairBeforeReaction)throw 84;')
            path.write_text(text)
        sources.append(path)
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    cmd=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    cmd += [str(root/'tests/rider_pair_reference.cpp')]+list(map(str,sources))+[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',framework]
    binary=root/'build/ssx3_rider_pair_reference';subprocess.run(cmd+['-o',str(binary)],check=True)
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:(work/'ee.bin').write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(work/'ee.bin')],check=True)
if __name__=='__main__':main()
