#!/usr/bin/env python3
"""Replay the PS2 sections captures through engine/far_painter.hpp (tests/far_painter_replay.cpp).

For every record: the type-4 painter steps at the captured outer camera eye X/Y (outer+0x20/+0x24),
predicts outer+0x08 and the texture-chunk viewer range (W+0x250 +0x40) bit for bit, and
engine/section_streaming.hpp ChunkStreaming runs with the predicted range (and, for comparison,
with the captured range and a fixed 45000) against the captured chunk table W+0x3F0.
The painter is seeded from the capture baseline savestate's camera block 6 (0x4FA370 + 6*0xF0).
Needs web/public/test-data/<L>/SECTIONS/far-painter.json and web/public/assets/<L>/SECTIONS/sections.json (tools/export_far_painter.py,
tools/export_sections.py).  Usage: python3 tools/test_far_painter.py [LOC ...]
"""
import json, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import export_sections as X  # noqa: E402

BINARY = ROOT / 'build/ssx3_far_painter_replay'
INCLUDES = [ROOT / 'engine', ROOT / 'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf']


def build():
    source = ROOT / 'tests/far_painter_replay.cpp'
    deps = [source] + [ROOT / 'engine' / n for n in ('far_painter.hpp', 'painter_driver.hpp', 'painter_tree.hpp', 'section_streaming.hpp', 'original_spatial.hpp')]
    if BINARY.exists() and all(BINARY.stat().st_mtime > d.stat().st_mtime for d in deps):
        return
    BINARY.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['xcrun', 'clang++', '-std=c++20', '-O2', '-frounding-math', '-ffp-contract=off'] + [f'-I{p}' for p in INCLUDES] +
                   [str(source), '-o', str(BINARY)], check=True)


def bits(v): return struct.unpack('<I', struct.pack('<f', v))[0]


def write_input(code, path):
    capture = X.CAPTURES[code]
    manifest = json.loads(capture.with_suffix('.capture.json').read_text())
    anchor = X.Memory(X.location_state(code, 'anchor'))
    _, records = X.read_capture(capture, anchor)
    ee = zipfile.ZipFile(manifest['baseline']).read('eeMemory.bin')
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    size, outer = manifest['record'], manifest['layout']['outer_camera_000_480']
    data = capture.read_bytes()
    view = struct.unpack_from('<I', data, outer + 0x18)[0] + 6
    wrapper = u(0x4FA370 + view * 0xF0); painter = u(wrapper)
    if u(painter + 4) != 0x485218:
        raise ValueError('camera block has no type-4 painter')
    game = u(u(X.GP - 0x848) + 0x84); cameras = u(u(game + 0x84) + 0x10)
    lines = ['S %d %d %d %d %d %d' % (u(painter), u(painter + 8), u(painter + 12), u(wrapper + 8), u(wrapper + 12), cameras)]
    for k, rec in enumerate(records):
        r = data[k * size:(k + 1) * size]
        if struct.unpack_from('<I', r, 4)[0] != rec['tick']:
            raise ValueError('record order')
        o = struct.unpack_from('<16I', r, outer)
        eye = struct.unpack_from('<3I', r, outer + 0x20)
        vr = rec['viewer']
        lines.append('R %d %d %d %d %d %d %d %d %d %d %d %s' % (rec['tick'], *eye, o[2], *[bits(v) for v in vr], o[4], o[5],
                                                    ' '.join(f'{c}:{s}' for c, s in sorted(rec['chunks'].items()))))
    path.write_text('\n'.join(lines) + '\n')
    return len(records), view, cameras


def main():
    codes = sys.argv[1:] or ['ARA1', 'BRA2', 'BHP1']
    build(); failed = 0
    for code in codes:
        base = ROOT / 'web/public/assets' / code / 'SECTIONS'
        replay = ROOT / 'local/ps2-capture/runs/sections' / f'{code.lower()}-far-replay.txt'
        n, view, cameras = write_input(code, replay)
        run = subprocess.run([str(BINARY), str(ROOT / 'web/public/test-data' / code / 'SECTIONS/far-painter.json'), str(base / 'sections.json'), str(replay)], capture_output=True, text=True)
        print(f'{code}: {n} records, camera block {view}, cameras {cameras}')
        print(run.stdout.rstrip())
        if run.returncode:
            print(run.stderr); failed += 1
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
