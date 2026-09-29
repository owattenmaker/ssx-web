#!/usr/bin/env python3
"""Compile and execute a real translated leaf routine with the PS2Recomp runtime."""
import hashlib
import subprocess
from pathlib import Path
from inspect_disc import EXPECTED_SHA1

ROOT = Path(__file__).resolve().parents[1]


def main():
    if hashlib.sha1((ROOT / 'local/disc/SLUS_207.72').read_bytes()).hexdigest() != EXPECTED_SHA1:
        raise SystemExit('Unexpected SSX 3 executable')
    vendor = ROOT / 'local/vendor/PS2Recomp'
    build = ROOT / 'build/ps2recomp'
    includes = [vendor / 'ps2xRuntime/include', vendor / 'ps2xRuntime/src/lib/Kernel',
                vendor / 'ps2xIOP/include', build / '_deps/sse2neon-src', ROOT / 'local/output']
    binary = ROOT / 'build/ssx3_hash_test'
    command = ['xcrun', 'clang++', '-std=c++20', '-O2', '-arch', 'arm64', '-DUSE_SSE2NEON', '-Werror=array-bounds']
    command += ['-I' + str(p) for p in includes]
    command += [str(ROOT / 'tests/native_hash.cpp'), str(ROOT / 'local/output/sub_00317618_0x317618.cpp'),
                str(ROOT / 'local/output/sub_003FE818_0x3fe818.cpp'),
                str(build / 'ps2xRuntime/libps2_runtime.a'), str(build / '_deps/raylib-build/raylib/libraylib.a'),
                str(build / 'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']:
        command += ['-framework', framework]
    command += ['-o', str(binary)]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run(['file', str(binary)], check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True, timeout=60)


if __name__ == '__main__':
    main()
