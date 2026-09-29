#!/usr/bin/env python3
"""Build the development-only original-code/native-animation comparison."""
import hashlib
import subprocess
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
from animation_bank import read_bank
from animation_curves import CurvePacket
import struct


def main():
    root=Path(__file__).resolve().parents[1]
    if hashlib.sha1((root/'local/disc/SLUS_207.72').read_bytes()).hexdigest()!=EXPECTED_SHA1:
        raise ValueError('Unexpected SSX3 executable')
    vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',
              build/'_deps/sse2neon-src',root/'local/output']
    binary=root/'build/ssx3_animation_reference'
    command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']
    command+=['-I'+str(p) for p in includes]
    command+=[str(root/'tests/animation_reference.cpp'),str(root/'engine/animation_motion.cpp'),str(root/'local/output/sub_00310200_0x310200.cpp'),str(root/'local/output/sub_0030ECD8_0x30ecd8.cpp'),
              str(root/'local/output/sub_003130A8_0x3130a8.cpp'),str(root/'local/output/sub_00312C20_0x312c20.cpp'),
              str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ('OpenGL','Cocoa','IOKit','CoreFoundation'):command+=['-framework',framework]
    command+=['-o',str(binary)]
    from original_fp_oracle import write_scalar_fp_oracle
    for original in (root/'local/output').glob('sub_*.cpp'):
        if str(original) in command:
            corrected=root/'build'/('animation-oracle-'+original.name)
            write_scalar_fp_oracle(original,corrected)
            command=[str(corrected) if item==str(original) else item for item in command]
    records=[]
    for name in ('basic','fe'):
        data=(root/f'local/assets/animation/{name}.afb').read_bytes();bank=read_bank(data)
        for c in bank['channels']:
            at=bank['payload_offset']+c['offset'];raw=bytearray(data[at:at+c['byte_size']])
            decoded=CurvePacket(raw,c['frame_field'])
            expected=[]
            for time in (0.,(decoded.frames-1)*.5,min(decoded.frames-1,decoded.frames*.37)):
                expected.append(struct.pack('<f',time)+struct.pack(f'<{decoded.dofs}f',*decoded.sample(time)))
            # AFB float24s are platform independent. Convert GC quantized words
            # to PS2 byte order before feeding the PS2 original-code reference.
            for i in range(decoded.offsets[8],decoded.offsets[8]+decoded.sizes[8],2):raw[i:i+2]=raw[i:i+2][::-1]
            records.append(struct.pack('<3I',len(raw),decoded.frames,decoded.dofs)+raw+b''.join(expected))
    fixture=root/'local/assets/animation/reference-fixtures.bin'
    fixture.write_bytes(struct.pack('<I',len(records))+b''.join(records))
    subprocess.run(command,check=True)
    subprocess.run([str(binary),str(root/'local/disc/SLUS_207.72'),str(fixture)],check=True,timeout=60)


if __name__=='__main__':main()
