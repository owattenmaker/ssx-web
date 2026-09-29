#!/usr/bin/env python3
"""Development-only original/native ground helper conformance, no GUI."""
import subprocess
import zipfile
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from original_fp_oracle import write_scalar_fp_oracle

def main():
    folder=ROOT/'local/native-qa';folder.mkdir(parents=True,exist_ok=True)
    fixture=folder/'ground-reference-ee.bin'
    with zipfile.ZipFile(ROOT/'local/reference/pcsx2/snow-jam-glide.p2s') as z:fixture.write_bytes(z.read('eeMemory.bin'))
    vendor=ROOT/'local/vendor/PS2Recomp';build=ROOT/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',ROOT/'local/output']
    command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-arch','arm64','-DUSE_SSE2NEON']
    command+=['-I'+str(p) for p in includes]
    original=(ROOT/'local/output/sub_0013D818_0x13d818.cpp').read_text()
    marker='label_13e124:'
    if original.count(marker)!=1:raise ValueError('Cruise prefix boundary not unique')
    original=original.replace(marker,'return; // End original translation prefix before heading/contact-tail.\n'+marker)
    original=original.replace('label_13ed28:','return; // End original velocity-contact stage.\nlabel_13ed28:')
    prefix=write_scalar_fp_oracle(ROOT/'local/output/sub_0013D818_0x13d818.cpp',folder/'ground-cruise-prefix.cpp',original)
    corrected=write_scalar_fp_oracle(ROOT/'local/output/sub_0031BE50_0x31be50.cpp',folder/'ground-original-sincos.cpp')
    command+=[str(ROOT/'engine/ground_motion_reference.cpp'),str(ROOT/'engine/ground_motion.cpp'),str(prefix),str(corrected)]
    command+=[str(write_scalar_fp_oracle(ROOT/'local/output'/name,folder/('ground-fp-'+name))) for name in ('sub_0013C878_0x13c878.cpp','sub_0013CCF0_0x13ccf0.cpp','sub_0013D028_0x13d028.cpp','sub_0013C948_0x13c948.cpp','sub_0031C228_0x31c228.cpp','sub_0011B3F8_0x11b3f8.cpp','sub_00113E80_0x113e80.cpp','sub_00113F88_0x113f88.cpp','sub_001211F8_0x1211f8.cpp')]
    command+=[str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
    binary=ROOT/'build/ssx3_ground_reference';command+=['-o',str(binary)]
    subprocess.run(command,check=True)
    golden=folder/'ground-golden.bin';temporary=folder/'ground-golden.tmp'
    subprocess.run([str(binary),str(fixture),str(temporary)],check=True,timeout=60)
    temporary.replace(golden)

if __name__=='__main__':main()
