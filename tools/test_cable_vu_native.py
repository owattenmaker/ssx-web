#!/usr/bin/env python3
"""Cable line-strip draw oracle (docs/visual-parity.md, section 37): the original VU1 program 3 at 0x3BF0 (MSCAL 0x77E, queued by
renderer slot +0x25C 0x381F10 for every visible segment of a MultiSpline path, 0x345430) on synthetic uploads; checks the model
web/set-piece-cables.js draws: `count` vertices at t = k / (count - 1) (the VU adds Q = 1 / (count - 1) each step), P(t) =
((A t + B) t + C) t + D through the guard-band clip matrix, screen = ndc * (1024, 1024) + 2047.5 (FTOI4), and ADC (no line into the
vertex) when the vertex or the previous one is outside the clip volume (|x|, |y|, |z| > w after the divide), always on vertex 0.
Writes local/reference/cables/cable-vu.json."""
from pathlib import Path
import json, struct, subprocess, sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from extract_fx_microcode import export
vendor = root / 'local/vendor/PS2Recomp'; build = root / 'build/ps2recomp'; out = root / 'build/cable-vu'; out.mkdir(parents=True, exist_ok=True)
export(out / 'vu'); program = out / 'vu/program3.bin'
includes = [vendor / 'ps2xRuntime/include', vendor / 'ps2xRuntime/src/lib/Kernel', vendor / 'ps2xIOP/include', build / '_deps/sse2neon-src', root / 'local/output']
command = ['xcrun', 'clang++', '-std=c++20', '-O2', '-frounding-math', '-ffp-contract=off', '-DUSE_SSE2NEON'] + ['-I' + str(x) for x in includes]
command += [str(root / 'tests/cable_vu_reference.cpp'), str(vendor / 'ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp'), str(build / 'ps2xRuntime/libps2_runtime.a'), str(build / '_deps/raylib-build/raylib/libraylib.a'), str(build / 'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']: command += ['-framework', framework]
binary = out / 'cable_vu'; subprocess.run(command + ['-o', str(binary)], check=True)
run = subprocess.run([str(binary), str(program)], check=True, capture_output=True, text=True, timeout=600)
target = root / 'local/reference/cables/cable-vu.json'; target.parent.mkdir(parents=True, exist_ok=True); target.write_text(run.stdout)
f = lambda w: struct.unpack('<f', struct.pack('<I', w))[0]
f32 = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]
cases = json.loads(run.stdout)['cases']; worst = 0.0; adc_bad = 0; verts = 0; border = 0
for c in cases:
    m = [f(w) for w in c['matrix']]; r = [f(w) for w in c['rows']]; A, B, C, D = r[0:3], r[3:6], r[6:9], r[9:12]
    n = c['count']; assert len(c['verts']) == n, (n, len(c['verts']))
    assert c['tag'][0] & 0x7fff == n and c['tag'][0] & 0x8000 and c['tag'][1] == 0x10014000 and c['tag'][2] == 4, c['tag']
    step = f32(1.0 / f32(n - 1)); t = 0.0; prev_out = True
    for k, v in enumerate(c['verts']):
        p = [((A[a] * t + B[a]) * t + C[a]) * t + D[a] for a in range(3)]
        clip = [m[0 + j] * p[0] + m[4 + j] * p[1] + m[8 + j] * p[2] + m[12 + j] for j in range(4)]
        ndc = [clip[j] / clip[3] for j in range(3)]
        out_now = any(abs(x) > 1 for x in ndc)
        near = any(abs(abs(x) - 1) < 1e-4 for x in ndc)
        adc = (v[3] & 0x8000) != 0
        if adc != (out_now or prev_out):
            if near or k and any(abs(abs(x) - 1) < 1e-4 for x in prev_ndc): border += 1
            else: adc_bad += 1
        if not out_now:
            sx = (ndc[0] * 1024 + 2047.5) * 16; sy = (ndc[1] * 1024 + 2047.5) * 16
            got = [struct.unpack('<i', struct.pack('<I', v[0]))[0], struct.unpack('<i', struct.pack('<I', v[1]))[0]]
            worst = max(worst, abs(got[0] - sx), abs(got[1] - sy))
        verts += 1; prev_out = out_now; prev_ndc = ndc; t = f32(t + step)
print(target, f'{len(cases)} cases, {verts} vertices, worst screen error {worst:.2f}/16 px, ADC mismatches {adc_bad} (+{border} at |ndc| = 1 +- 1e-4)')
if adc_bad or worst > 4: sys.exit(1)
