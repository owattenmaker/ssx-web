#!/usr/bin/env python3
"""Run original cruise force conformance across all 19 captured material records.

Uses a local copy of the existing original-force harness and writes its own
artifacts; does not change the shared harness or its ground golden fixtures.
"""
from pathlib import Path
import subprocess,zipfile
from original_fp_oracle import write_scalar_fp_oracle
ROOT=Path(__file__).resolve().parents[1]

def main():
    folder=ROOT/'local/reference/terrain/surface-ground';folder.mkdir(parents=True,exist_ok=True)
    fixture=folder/'ee.bin'
    with zipfile.ZipFile(ROOT/'local/reference/pcsx2/snow-jam-glide.p2s') as z:fixture.write_bytes(z.read('eeMemory.bin'))
    text=(ROOT/'engine/ground_motion_reference.cpp').read_text().replace('#include "ground_motion.hpp"','#include "ground_motion.hpp"\n#include "ground_surface.hpp"')
    marker='    PS2Runtime runtime;'
    text=text.replace(marker,r'''
    auto sourceU=[&](uint32_t at){uint32_t x;std::memcpy(&x,ram.data()+at,4);return x;};
    uint32_t sourceBase=sourceU(sourceU(sourceU(0x4a30f0-0x848)+0x84)+0x44);
    std::array<std::array<uint8_t,176>,19> originalSurfaces;
    std::array<ssx::OriginalGroundMaterial,19> materials;
    for(unsigned i=0;i<19;++i){std::memcpy(originalSurfaces[i].data(),ram.data()+sourceBase+i*176,176);
        auto f=[&](unsigned offset){float value;std::memcpy(&value,originalSurfaces[i].data()+offset,4);return value;};
        auto& m=materials[i];m.surface.id=i;m.surface.gravity=f(0);m.surface.lateralDrag=f(8);m.surface.powderDamping=f(0x1c);
        std::memcpy(m.surface.slipFriction.data(),originalSurfaces[i].data()+0x90,32);
        m.depthTarget1=f(0x14);m.depthTarget3=f(0x18);m.maxTurnAngle=f(0xc);m.airHeight=f(0x10);
        m.autoBoostSpeed=f(0x20);m.autoBoostFactor=f(0x24);m.alignmentRate=f(0x38);m.terminalVelocity=f(4);
        m.heading={f(0x28),f(0x2c),f(0x30),f(0x34)};
    }
'''+marker,1)
    marker='        ssx::OriginalGroundState state;'
    assert text.count(marker)==1
    text=text.replace(marker,r'''
        unsigned materialIndex=unsigned(i)%19;ssx::applyGroundMaterial(profile,materials[materialIndex]);
        write(surface,originalSurfaces[materialIndex]);write(manager+0x44,surface-materialIndex*176);
        write(rider+0x438,materialIndex);
'''+marker)
    text=text.replace('ssx::originalGroundVelocityContact(contacted,0,previousSurface);','ssx::originalGroundVelocityContact(contacted,profile.surface.id,previousSurface);')
    marker='        ssx::OriginalGroundProfile p;ssx::OriginalGroundState s;'
    assert text.count(marker)==1
    text=text.replace(marker,'        write(manager+0x44,surface);\n'+marker)
    text=text.replace('full cruise translation prefix/postcontact/speed-limit match float bits','full cruise translation prefix/postcontact across all19 original materials and speed-limit match float bits')
    text=text.replace('PS2Runtime runtime;', 'PS2Runtime runtime;runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);')
    harness=folder/'surface-force-reference.cpp';harness.write_text(text)
    original=(ROOT/'local/output/sub_0013D818_0x13d818.cpp').read_text().replace('label_13e124:','return;\nlabel_13e124:').replace('label_13ed28:','return;\nlabel_13ed28:').replace('label_13ed2c:','label_13ed2c:\nreturn;')
    prefix=write_scalar_fp_oracle(ROOT/'local/output/sub_0013D818_0x13d818.cpp',folder/'cruise.cpp',original)
    vendor=ROOT/'local/vendor/PS2Recomp';build=ROOT/'build/ps2recomp'
    includes=[ROOT/'engine',vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',ROOT/'local/output']
    command=['clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']+['-I'+str(p) for p in includes]
    command += [str(harness),str(ROOT/'engine/ground_motion.cpp'),str(prefix)]
    for name in ('sub_0031BE50_0x31be50.cpp','sub_0013C878_0x13c878.cpp','sub_0013CCF0_0x13ccf0.cpp','sub_0013D028_0x13d028.cpp','sub_0013C948_0x13c948.cpp','sub_0031C228_0x31c228.cpp','sub_0011B3F8_0x11b3f8.cpp','sub_00113E80_0x113e80.cpp','sub_00113F88_0x113f88.cpp','sub_001211F8_0x1211f8.cpp'):
        command.append(str(write_scalar_fp_oracle(ROOT/'local/output'/name,folder/name)))
    command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
    binary=ROOT/'build/ssx3_surface_ground_reference';subprocess.run(command+['-o',str(binary)],check=True)
    subprocess.run([str(binary),str(fixture),str(folder/'golden.bin')],check=True)

if __name__=='__main__':main()
