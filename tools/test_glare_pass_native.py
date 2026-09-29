#!/usr/bin/env python3
"""Run the original framebuffer glare pass 36C790 on live PS2 memory and dump its GS packets.

Development-only oracle (needs the owned disc build in local/ and build/ps2recomp): the
recompiled originals 36C790/36C740/36B9D8/36C188/36C398/368138 run (395330/395350 transcribed in the harness) on the EE
RAM of a savestate whose course authors a world-painter type-6 (glare) section
(Metro-City glide 620: live context +6CD4 = 1.2406,1,1,1.2005,0.9599,0,0.4010). Case 0 is
the live state, cases 1..39 synthetic painter/debug values. Output:
local/browser-validation/glare-pass-packets.json, read by web/test-glare-pass.mjs.
"""
from pathlib import Path
import argparse, subprocess, zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build

root = Path(__file__).resolve().parents[1]
vendor = root / 'local/vendor/PS2Recomp'
build = root / 'build/ps2recomp'
parser = argparse.ArgumentParser()
parser.add_argument('--state', default='metro-city-glide-620.p2s')
parser.add_argument('--out', default=str(root / 'local/browser-validation/glare-pass-packets.json'))
args = parser.parse_args()
includes = [vendor / 'ps2xRuntime/include', vendor / 'ps2xRuntime/src/lib/Kernel', vendor / 'ps2xIOP/include',
            build / '_deps/sse2neon-src', root / 'local/output']
command = ['xcrun', 'clang++', '-std=c++20', '-O2', '-arch', 'arm64', '-DUSE_SSE2NEON', '-frounding-math',
           '-ffp-contract=off'] + ['-I' + str(p) for p in includes] + [str(root / 'tests/glare_pass_reference.cpp')]
for fn in ['0036C790', '0036C740', '0036B9D8', '0036C188', '0036C398', '00368138']:
    original = next((root / 'local/output').glob('sub_' + fn + '*'))
    command.append(str(write_scalar_fp_oracle(original, root / 'local/event-activation' / ('glare-' + original.name))))
command += [str(build / 'ps2xRuntime/libps2_runtime.a'), str(build / '_deps/raylib-build/raylib/libraylib.a'),
            str(build / 'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']:
    command += ['-framework', framework]
binary = root / 'build/ssx3_glare_pass_reference'
command += ['-o', str(binary)]
cached_oracle_build(command, root / 'build/original-glare-objects')
memory = root / 'local/event-activation/glare-pass-ram.bin'
memory.write_bytes(zipfile.ZipFile(root / 'local/reference/pcsx2' / args.state).read('eeMemory.bin'))
Path(args.out).parent.mkdir(parents=True, exist_ok=True)
run = subprocess.run([str(binary), str(memory), args.out], check=False, timeout=120, capture_output=True, text=True)
print(run.stdout, end=''); print(run.stderr, end='')
run.check_returncode()
if 'missing-target' in run.stdout + run.stderr:
    raise RuntimeError('Incomplete original glare pass call graph')
