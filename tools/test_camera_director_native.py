#!/usr/bin/env python3
"""Original camera director / POST_RACE_1 / compositor-blend conformance (development oracle).

Compiles the recompiled original functions (opcode-corrected EE scalar FP copies) and
compares them with engine/original_camera_director.hpp and the POST_RACE_1 path of
engine/original_camera.hpp:

* POST_RACE_1 per-frame step 0x1624E8 (pending set-target 0x166F28 -> 0x178E90 -> 0x166C60 /
  0x166550, driver 0x178BB0 with 0x162568 / 0x31C040 / 0x31BE50, finish 0x166228);
* POST_RACE_1 ctor 0x1789E8 (base ctor boundary);
* director update 0x161BB8 including transitions 0x161AB0 and node removal 0x15CA50;
* list insertion 0x161E58 / 0x15C988 / 0x15CB08;
* compositor 0x15E668 gather (single copy and multi-node blend) and fov/near/far blend.
"""
from pathlib import Path
import hashlib, subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
from inspect_disc import EXPECTED_SHA1

root = Path(__file__).resolve().parents[1]
vendor = root / 'local/vendor/PS2Recomp'; build = root / 'build/ps2recomp'
includes = [vendor / 'ps2xRuntime/include', vendor / 'ps2xRuntime/src/lib/Kernel', vendor / 'ps2xIOP/include',
            build / '_deps/sse2neon-src', root / 'local/output', build / '_deps/nlohmann_json-src/single_include']
command = ['xcrun', 'clang++', '-std=c++20', '-O2', '-arch', 'arm64', '-DUSE_SSE2NEON', '-frounding-math', '-ffp-contract=off']
command += ['-I' + str(p) for p in includes] + [str(root / 'tests/camera_director_reference.cpp')]
STOPS = {
    # Stop the compositor before the terrain clearance (gather stage) and after the lens blend.
    '0015E668': ['15e914', '15ebbc'],
}
for prefix in ['001789E8', '00162458', '00166F28', '00166C60', '00166550', '00165938', '00166640', '00166530', '00168150',
               '00166F90', '0031B748', '0031B7A8', '0031BE50', '0031C040', '0031C128', '0031C228', '00162568',
               '00161AB0', '0015CA50', '00161E58', '0015C988', '0015CB08', '0015E668', '001673A0', '00167E30']:
    path = next((root / 'local/output').glob('sub_' + prefix + '*'))
    source = path.read_text()
    for pc in STOPS.get(prefix, []):
        marker = '    // 0x' + pc + ':'
        if source.count(marker) != 1: raise ValueError('Missing oracle boundary 0x' + pc)
        source = source.replace(marker, '    ctx->pc = 0x' + pc + 'u; return;\n' + marker)
    command.append(str(write_scalar_fp_oracle(path, root / ('local/event-activation/camera-director-' + prefix + '.cpp'), source)))
command += [str(build / 'ps2xRuntime/libps2_runtime.a'), str(build / '_deps/raylib-build/raylib/libraylib.a'), str(build / 'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']: command += ['-framework', framework]
binary = root / 'build/ssx3_camera_director_reference'; command += ['-o', str(binary)]
cached_oracle_build(command, root / 'build/original-camera-director-objects')
elf = root / 'local/disc/SLUS_207.72'
if hashlib.sha1(elf.read_bytes()).hexdigest() != EXPECTED_SHA1: raise ValueError('Unexpected original executable')
run = subprocess.run([str(binary), str(elf)], check=False, timeout=600, capture_output=True, text=True)
print(run.stdout, end=''); print(run.stderr, end=''); run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original camera director call graph')
