#!/usr/bin/env python3
"""Replay the PS2 section/chunk captures through engine/section_streaming.hpp
(tests/section_streaming_replay.cpp) for ARA1 / BRA2 / BHP1.

Captures (tools/ps2_capture.py build --isolate --ai-state, script = the full-course set-piece scripts,
watch windows: activation manager act+0xD0 (0x400 bytes: +0xD0 last scan tick, +0xD4 parity, +0xD8 count,
list), W+0x3F0 chunk table ranges, W+0x380, W+0x250 viewer 0): local/ps2-capture/runs/sections/<loc>-full.bin.
Needs web/public/assets/<LOC>/SECTIONS/sections.json (tools/export_sections.py --location LOC).
Usage: python3 tools/test_sections_replay.py [LOC ...]
"""
import json, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import export_sections as X  # noqa: E402

BINARY = ROOT / 'build/ssx3_section_streaming_replay'
INCLUDES = [ROOT / 'engine', ROOT / 'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf']


def build():
    source = ROOT / 'tests/section_streaming_replay.cpp'
    deps = [source, ROOT / 'engine/section_streaming.hpp', ROOT / 'engine/original_spatial.hpp']
    if BINARY.exists() and all(BINARY.stat().st_mtime > d.stat().st_mtime for d in deps): return
    BINARY.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['xcrun', 'clang++', '-std=c++20', '-O2', '-frounding-math', '-ffp-contract=off'] + [f'-I{p}' for p in INCLUDES] +
                   [str(source), '-o', str(BINARY)], check=True)


def write_input(code, path):
    anchor = X.Memory(X.location_state(code, 'anchor'))
    rows = X.instance_rows(anchor); res = {i: r['resource'] for i, r in rows.items()}
    manifest, records = X.read_capture(X.CAPTURES[code], anchor)
    a = anchor.activation()
    lines = ['A %d %d %r %r %r %s' % (anchor.u(a + 0xD4), anchor.i(a + 0xD0), *anchor.v(a + 0x20),
                                       ' '.join(str(res[i]) for i in anchor.active_list() if i in res))]
    last_d0 = records[0]['act']['d0']; listed = set(anchor.active_list())
    for rec in records:
        eye = rec['viewer']
        lines.append('R %d %r %r %r %r %r %r %r %s' % (rec['tick'], *rec['position'], *eye[:3], eye[3],
                                                    ' '.join(f'{c}:{s}' for c, s in sorted(rec['chunks'].items()))))
        act = rec['act']
        if act['d0'] != last_d0:
            lines.append('L %d %d %s' % (act['d0'], len(rec['world_draws']), ' '.join(str(res[i]) for i in act['list'] if i in res)))
            last_d0 = act['d0']; listed = set(act['list'])
        else:
            for i in act['list']:
                if i not in listed and i in res: lines.append('N %d %d' % (rec['tick'] - 1, res[i]))
            listed = set(act['list'])
    path.write_text('\n'.join(lines) + '\n')
    return len(records)


def main():
    codes = sys.argv[1:] or ['ARA1', 'BRA2', 'BHP1']
    build(); failed = 0
    for code in codes:
        sections = ROOT / 'web/public/assets' / code / 'SECTIONS/sections.json'
        replay = ROOT / 'local/ps2-capture/runs/sections' / f'{code.lower()}-replay.txt'
        n = write_input(code, replay)
        run = subprocess.run([str(BINARY), str(sections), str(replay)], capture_output=True, text=True)
        out = run.stdout.strip().splitlines()
        print(f'{code}: {n} records; ' + next(l for l in out if l.startswith('tick 0')) + '; ' + out[-1])
        if run.returncode:
            print(run.stdout); failed += 1
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
