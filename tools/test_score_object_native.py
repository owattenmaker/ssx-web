#!/usr/bin/env python3
"""Compare engine/score_object.cpp with the complete original score routines.

Runs 117C28 (with 117FE0 and the HUD slot clocks), 11A228, 117718, 117638, 119368 and 119608 from
the recompiled original on randomized score objects / HUD banks and checks the whole object
(+0..+0x1CC) and all 44 HUD slots byte-for-byte. Usage: test_score_object_native.py [CASES]
"""
from pathlib import Path
import hashlib, subprocess, sys, zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1
root = Path(__file__).resolve().parents[1]
assert hashlib.sha1((root / 'local/disc/SLUS_207.72').read_bytes()).hexdigest() == EXPECTED_SHA1
out = root / 'local/reference/score-object'; out.mkdir(parents=True, exist_ok=True)
v = root / 'local/vendor/PS2Recomp'; b = root / 'build/ps2recomp'
names = ('00117C28 00117FE0 00116FB8 00117718 00117638 001175F8 001179E0 00117A58 00117AE8 00117B88 00117048 001170A8 001171A8 '
         '00117008 00117908 00117948 00117990 00119310 00119210 00119EF8 0011A228 0011A8C8 0011B1A8 001190F0 00118FF8 00116950 '
         '00117708 00117838 00117900 0011FE98 0011FEE8 001360C8 00135F70 00136100 00136168 0012F118 001298C8 0031C040 0031C228 '
         '0011A7A8 00119368 00119608 00119B08 00119C98 0011A168 001176F8').split()
files = [next((root / 'local/output').glob('sub_' + n + '*')) for n in names]
(out / 'score_registry.inc').write_text(''.join(f'void {p.stem}(uint8_t*,R5900Context*,PS2Runtime*);\n' for p in files)
    + 'void registerScore(PS2Runtime& r){' + ''.join(f'r.registerFunction(0x{n},{p.stem});' for n, p in zip(names, files)) + '}\n')
incs = [v / 'ps2xRuntime/include', v / 'ps2xRuntime/src/lib/Kernel', v / 'ps2xIOP/include', b / '_deps/sse2neon-src', root / 'local/output', out]
cmd = ['xcrun', 'clang++', '-std=c++20', '-O2', '-arch', 'arm64', '-DUSE_SSE2NEON', '-frounding-math', '-ffp-contract=off'] + ['-I' + str(p) for p in incs]
cmd += [str(root / 'tests/score_object_reference.cpp')] + [str(root / 'engine' / f'{n}.cpp') for n in
        ['score_object', 'trick_commit', 'trick_identity', 'trick_history', 'trick_bonus', 'grab_score', 'ground_pose_motion', 'ground_motion']]
cmd += [str(write_scalar_fp_oracle(p, out / p.name)) for p in files]
cmd += [str(b / 'ps2xRuntime/libps2_runtime.a'), str(b / '_deps/raylib-build/raylib/libraylib.a'), str(b / 'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']: cmd += ['-framework', f]
binary = root / 'build/ssx3_score_object_reference'; cmd += ['-o', str(binary)]
cached_oracle_build(cmd, root / 'build/original-npc-oracle-objects')
(out / 'memory.bin').write_bytes(zipfile.ZipFile(root / 'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary), str(out / 'memory.bin')] + sys.argv[1:2], check=True, timeout=600)
