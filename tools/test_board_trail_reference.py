#!/usr/bin/env python3
from pathlib import Path
import subprocess,zipfile
from reference_scalar_lowering import scalar_oracle_copy

def main():
    root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp';work=root/'local/reference/terrain/board-trail';work.mkdir(parents=True,exist_ok=True)
    names=['sub_002E8938_0x2e8938.cpp','sub_002E87E8_0x2e87e8.cpp','sub_002E86F0_0x2e86f0.cpp','sub_0031C228_0x31c228.cpp','sub_002EA538_0x2ea538.cpp']
    sources=[scalar_oracle_copy(root/'local/output'/name,work/name) for name in names]
    # Correct only the test-copy VRSQRT zero result to documented PCSX2 saturation.
    original_update=work/names[0]
    text=original_update.read_text().replace('(ft > 0.0f) ? (1.0f / sqrtf(ft)) : 0.0f', '(ft > 0.0f) ? (1.0f / sqrtf(ft)) : 3.4028234663852886e38f')
    original_update.write_text(text)
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    cmd=['xcrun','clang++','-std=c++20','-O2','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    cmd += [str(root/'tests/board_trail_reference.cpp')]+list(map(str,sources))+[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:cmd+=['-framework',framework]
    binary=root/'build/ssx3_board_trail_reference';subprocess.run(cmd+['-o',str(binary)],check=True)
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:(work/'ee.bin').write_bytes(z.read('eeMemory.bin'))
    subprocess.run([str(binary),str(work/'ee.bin')],check=True)
    with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z:(work/'vu1.bin').write_bytes(z.read('vu1MicroMem.bin'))
    rendercmd=cmd[:]
    rendercmd[rendercmd.index(str(root/'tests/board_trail_reference.cpp'))]=str(root/'tests/board_trail_render_reference.cpp')
    rendercmd=[x for x in rendercmd if x not in list(map(str,sources))]
    # VU MAX/MINI select raw bit representations (PCSX2 VUops.cpp fp_max/min).
    # Vendor interpreter normalizes denormals first, destroying uint RGB payloads.
    upper=vendor/'ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp'
    upper_text=upper.read_text().replace('    float *vd = m_state.vf[fd];', '    if (op==0x2b || op==0x2f || (op>=0x10 && op<=0x17)) {\n        for(int c=0;c<4;++c)if(fd && (dest & (8>>c))) {\n            int32_t a,b;std::memcpy(&a,&m_state.vf[fs][c],4);\n            std::memcpy(&b,&m_state.vf[ft][(op>=0x10&&op<=0x17)?(op&3):c],4);\n            bool maximum=op==0x2b||(op>=0x10&&op<=0x13);\n            int32_t result=(a<0&&b<0)?(maximum?std::min(a,b):std::max(a,b)):(maximum?std::max(a,b):std::min(a,b));\n            std::memcpy(&m_state.vf[fd][c],&result,4);\n        }return;\n    }\n    float *vd = m_state.vf[fd];')
    upper_copy=work/'vu1_upper_oracle.cpp';upper_copy.write_text(upper_text)
    rendercmd.insert(rendercmd.index(str(build/'ps2xRuntime/libps2_runtime.a')),str(upper_copy))
    rendercmd+=['-I'+str(upper.parent)]
    renderbinary=root/'build/ssx3_board_trail_render_reference'
    subprocess.run(rendercmd+['-o',str(renderbinary)],check=True)
    subprocess.run([str(renderbinary),str(work/'vu1.bin')],check=True)
if __name__=='__main__':main()
