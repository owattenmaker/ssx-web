#!/usr/bin/env python3
"""Capture the original trick-scoring HUD slot draws (font 0x391CB0 / sprite 0x1F1190 submissions).

Runs the per-type cases of the single-player HUD slot loop in 0x1E9A30 (dispatch table 0x46EC70) for
representative slot states and writes web/public/assets/UI/trick-hud-draws.json plus the layout
descriptor table (settings 0x4768B0, 36-byte records) to web/public/assets/UI/trick-hud.json.
Development reference only: original code runs against the user's own snapshot memory; nothing is patched.
"""
from pathlib import Path
import bisect, json, re, struct, subprocess, zipfile
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root = Path(__file__).resolve().parents[1]; v = root / 'local/vendor/PS2Recomp'; b = root / 'build/ps2recomp'
out = root / 'local/browser-ui/trick-hud'; out.mkdir(parents=True, exist_ok=True)
assets = root / 'web/public/assets/UI'
snapshot = root / 'local/reference/pcsx2/snow-jam-glide.p2s'
memory = zipfile.ZipFile(snapshot).read('eeMemory.bin')
u32 = lambda a: struct.unpack_from('<I', memory, a)[0]

# Case labels of the dispatch table (type -> label) and the block ranges they own.
TABLE = 0x46EC70
labels = {t: u32(TABLE + 4 * t) for t in range(0x34)}
DEFAULT = 0x1EFAC0
TYPES = [0, 1, 2, 3, 4, 7, 0xB, 0xE, 0x19, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33]

import sys
EXPORT_ONLY = '--export-only' in sys.argv   # reuse local/browser-ui/trick-hud/trick-hud-draws.json
# ---- static call closure of the executed cases (excluding the HUD function itself) ----
files = []
for f in (root / 'local/output').glob('sub_*.cpp'):
    m = re.match(r'sub_([0-9A-F]+)_0x', f.name)
    if m: files.append((int(m.group(1), 16), f))
files.sort(); starts = [s for s, _ in files]
def file_of(a): return files[bisect.bisect_right(starts, a) - 1]
hud_src = file_of(0x1E9A30)[1].read_text()
asm = [(int(m.group(1), 16), m.group(2)) for m in re.finditer(r'// 0x([0-9a-f]+): 0x[0-9a-f]+\s+(.*)', hud_src)]
case_starts = sorted(set(labels.values()) | {DEFAULT, 0x1F10F8})
roots = set()
for t in TYPES:
    lo = labels[t]; hi = min(s for s in case_starts if s > lo)
    roots |= {int(x, 16) for a, text in asm if lo <= a < hi for x in re.findall(r'jal\s+func_([0-9A-F]+)', text)}
# Shared tails the cases branch into (text/number drawing in the loop epilogue region).
roots |= {int(x, 16) for a, text in asm if 0x1EFAC0 <= a < 0x1F10F8 for x in re.findall(r'jal\s+func_([0-9A-F]+)', text)}
STOP = {0x391CB0, 0x1F1190, 0x417828, 0x1E9A30}
# Indirect (vtable) targets seen at run time (loc/career managers reached from 0x198AF0 / 0x14DD58).
roots |= {0x1530E0}
seen = {}; entries = {}; todo = list(roots); calls = {}
while todo:
    a = todo.pop()
    if a in STOP: continue
    start, f = file_of(a)
    if start == 0x1E9A30: continue
    entries.setdefault(f, set()).update({start, a})   # merged files are also entered at internal labels
    if f in seen: continue
    seen[f] = start
    todo += [int(x, 16) for x in re.findall(r'jal\s+func_([0-9A-F]+)', f.read_text())]
paths = sorted(seen)
(out / 'trick_hud_registry.inc').write_text(''.join(f'void {p.stem}(uint8_t*,R5900Context*,PS2Runtime*);\n' for p in paths)
    + 'void sub_001E9A30_0x1e9a30(uint8_t*,R5900Context*,PS2Runtime*);\n'
    + 'void registerTrickHud(PS2Runtime&r){' + ''.join(f'r.registerFunction(0x{e:X},{p.stem});' for p in paths for e in sorted(entries[p])) + '}\n')
marker = '    // 0x1efac4:'
assert hud_src.count(marker) == 1
hud_stop = hud_src.replace(marker, 'ctx->pc=0x12345678;return;\n' + marker)
incs = [v / 'ps2xRuntime/include', v / 'ps2xRuntime/src/lib/Kernel', v / 'ps2xIOP/include', b / '_deps/sse2neon-src', root / 'local/output', out,
        root / 'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf']
cmd = ['xcrun', 'clang++', '-std=c++20', '-O1', '-arch', 'arm64', '-DUSE_SSE2NEON', '-frounding-math', '-ffp-contract=off'] + ['-I' + str(p) for p in incs]
cmd += [str(root / 'tests/trick_hud_probe.cpp')]
cmd += [str(write_scalar_fp_oracle(file_of(0x1E9A30)[1], out / ('hud-' + file_of(0x1E9A30)[1].name), hud_stop))]
cmd += [str(write_scalar_fp_oracle(p, out / p.name)) for p in paths]
cmd += [str(b / 'ps2xRuntime/libps2_runtime.a'), str(b / '_deps/raylib-build/raylib/libraylib.a'), str(b / 'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL', 'Cocoa', 'IOKit', 'CoreFoundation']: cmd += ['-framework', framework]
binary = root / 'build/trick-hud-probe'; cmd += ['-o', str(binary)]
if not EXPORT_ONLY: cached_oracle_build(cmd, root / 'build/trick-hud-objects')

# ---- cases ----
ratios = [0.0, 0.02, 0.05, 0.1, 0.2, 0.3, 0.5, 0.7, 0.8, 0.9, 0.95, 0.99]
cases = []
def add(t, **kw):
    for r in kw.pop('ratios', ratios): cases.append(dict(type=t, label=labels[t], ratio=r, **kw))
add(0, text='FS 360', arg=0, maximum=3.0); add(0, text='BACKSIDE RODEO 540', arg=1, maximum=3.0, ratios=[0.1, 0.5])
for g in range(6): add(1, points=[10, 1000, 2500, 4000, 7500, 11500][g], arg=g, ratios=[0.0])
add(2, points=620, arg=120, ratios=[0.0, 0.5, 1.0]);
for cnt, f10 in [(1, 4), (2, 4), (3, 2), (5, 0)]: add(3, points=549, arg=cnt, field10=f10, ratios=[0.0, 0.5, 1.0])
add(4, points=300, ratios=[0.0, 1.0]); add(7, points=1810, ratios=[0.0], flash=-1.0); add(7, points=1810, ratios=[0.0], flash=0.3, pulse=1.1)
add(0xB, ratios=[0.0, 0.3, 0.7, 1.0])
for k, msg in enumerate(['STALLED!', 'OFF-AXIS!', 'INVERTED!', 'LATE SPIN!', 'LATE FLIP!'], 1): add(0xE, text=msg, arg=k, ratios=[0.0])
add(0x19, points=3, ratios=[0.0])
for t, a in [(0x1C, 6), (0x1D, 10000), (0x1E, 5), (0x1F, 2), (0x20, 3)]: add(t, points=5000, arg=a, maximum=1.5)
add(0x21, maximum=1.5)
for t in (0x23, 0x24):
    for g in (0, 1, 3, 5): add(t, points=[500, 1000, 4000, 12000][[0, 1, 3, 5].index(g)], arg=g, maximum=2.5)
for t in (0x25, 0x27, 0x28): add(t, points=1340, maximum=1.5)
for cnt in (2, 5): add(0x26, points=2380, arg=cnt, maximum=1.5)
for t in (0x29, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33): add(t, points=750, arg=2, maximum=2.5)
# Conquer the Mountain (free ride flags 0x1530C380): career cash popups 0x2E/0x2F (0x1EF924) and 'Collect +%s' 0x31 (0x1EF624,
# pre-pass mode +0x88 = 3); the 0x31 case also leaves its ratio in f29 for the counter 21F9B0 (web/free-ride-hud.js).
for t in (0x2E, 0x2F): add(t, points=20, arg=0, maximum=2.5, flags=0x1530C380)
add(0x31, points=2000, arg=2000, maximum=1.5, flags=0x1530C380, mode88=3)
(out / 'trick-hud-draws.json.cases').write_text(json.dumps(cases))
(out / 'memory.bin').write_bytes(memory)
if not EXPORT_ONLY:
    run = subprocess.run([str(binary), str(out / 'memory.bin'), str(out / 'trick-hud-draws.json')], capture_output=True, text=True, timeout=600)
    print(run.stdout, end=''); print(run.stderr[-4000:], end='')
    if run.returncode: raise SystemExit(run.returncode)
    if 'missing-target' in run.stderr or 'missing-target' in run.stdout: raise RuntimeError('Incomplete original HUD call graph; capture is invalid')

# ---- export: layout descriptors + captured draws ----
descriptors = []
for k in range(0x70):
    a = 0x4768B0 + 36 * k
    x, y, w, h = struct.unpack_from('<4h', memory, a); sx, sy = struct.unpack_from('<2f', memory, a + 8)
    argb = struct.unpack_from('<4f', memory, a + 16); ah, av, order = struct.unpack_from('<3b', memory, a + 32)
    descriptors.append(dict(index=k, x=x, y=y, w=w, h=h, scale=[sx, sy], argb=list(argb), align=[ah, av], order=order))
captured = json.loads((out / 'trick-hud-draws.json').read_text())
f4 = lambda a: list(struct.unpack_from('<4f', memory, a))
cstr = lambda a: memory[a:a + 64].split(b'\0')[0].decode('latin-1')
owner = captured['owner']
# Owner sprite records (+0x470..+0x4F0): texture handle, UV (v0,u0,u1,v1 as 0x1F1190 reads +12..+24).
# Handle 0x656 is OV page "hude" (OV_1-4), 0x657 page "hud " (OV_1-3) -- checked against the pages' artwork; 0x658 is OV_1-2 (the
# button icon page: the handle of the 0x4C8980 icon records, whose 'square' UV the +0x4DC record repeats).
PAGES = {0x656: 'OV_1-4', 0x657: 'OV_1-3', 0x658: 'OV_1-2'}
def sprite(off):
    p = u32(owner + off); h = u32(p); return dict(owner_offset=hex(off), handle=h, page=PAGES.get(h), uv=list(struct.unpack_from('<4f', memory, p + 12)))
sprites = dict(star=sprite(0x4A0), watch=sprite(0x4B8), hands=[sprite(0x4BC), sprite(0x4C0), sprite(0x4C4), sprite(0x4C8)], bonusBox=sprite(0x4F0),
               checkpoint=sprite(0x4AC), timeBonus=sprite(0x4B0),   # 0x2A / 0x2B peak-run split "CHECKPOINT" (docs/peak3.md), 0x29 checkpoint time bonus
               go=sprite(0x434),   # big message 8 "GO!" (owner +0x3C4 = 8: HUD command 6, score slot 0x1A; 0x1F0818, descriptor 8)
               # crash recover meter 0x21D9A0 (pv recoverMeter): the frame's end cap / middle and the button (+0x4E0 for pad byte +0x13)
               recoverEnd=sprite(0x4D4), recoverBar=sprite(0x4D8), recoverButton=[sprite(0x4DC), sprite(0x4E0)],
               switchIcon=sprite(0x470))   # the switch-stance 'S' 0x1F0290 (HUD flags 0x10000000, descriptor 0x4F; alpha x 0.2 gp-0x55B0 while rider +0x320 == +0x324)
# 1F1840 clock layout (owner +0x16C colon gap, +0x170 digit pair advance, +0x174 sign advance, +0x178 shift of a '1' digit)
f1 = lambda a: struct.unpack_from('<f', memory, a)[0]
clock = dict(gap=f1(owner + 0x16C), pair=f1(owner + 0x170), sign=f1(owner + 0x174), one=f1(owner + 0x178), height=u32(u32(owner + 0x42C) + 0x14),
             colon=cstr(u32(owner + 0x430)) if False else None)
colours = dict(green=f4(0x4C8528), repeatPopup=f4(0x4C8548), stars=[f4(0x4C8568 + 0x20 * k) for k in range(5)], popup=f4(0x4C8528),
               comboGreen=f4(0x4C8728), bonusGreen=f4(0x4C8748), multiplier=f4(0x4C8608),
               timeBonus=f4(0x4C87E8), splitPositive=f4(0x4C88C8), splitNegative=f4(0x4C8888))   # 0x29 / 0x2A-0x2B >= 0 / < 0
strings = dict(combo=cstr(0x4A22C8), recovered=cstr(0x46EBD0), bonus=[cstr(0x4A22E0), cstr(0x46EB80), cstr(0x46EB90), cstr(0x46EBA0), cstr(0x46EBB0)],
               plus=cstr(0x4A2100), minus=cstr(0x4A2108), colon=cstr(u32(owner + 0x430)), points=cstr(0x4A2298),
               missionSuccess=cstr(0x46EC18),   # big message 0x3A (score slot 0x1B, 0x1F0928, descriptors 58 / 59)
               recover=cstr(u32(0x4A30F0 - 0xF18)))   # the recover meter label (gp-0xF18, laid out at init 0x1EA828 into owner +0x6AC / +0x7F8)
sin = list(struct.unpack_from('<512f', memory, 0x504FB8))
flags = (u32(owner + 0x3CC) & ~u32(owner + 0x48 + 0x80) | u32(owner + 0x48 + 0x84)) & 0xFFFFFFFF
source = 'single-player HUD 0x1E9A30 slot cases (table 0x46EC70), layout 0x4768B0; tools/probe_trick_hud.py'
clock.pop('colon')
assets.joinpath('trick-hud.json').write_text(json.dumps(dict(source=source, flags=flags, descriptors=descriptors, sprites=sprites,
    colours=colours, strings=strings, clock=clock, sin=sin), separators=(',', ':')) + '\n')
# The captured original draws stay local (derived from the user's snapshot); web/test-trick-hud.mjs reads them.
(out / 'trick-hud-cases.json').write_text(json.dumps(dict(source=source, cases=captured['cases']), separators=(',', ':')) + '\n')
print('wrote', assets / 'trick-hud.json', 'and', out / 'trick-hud-cases.json', len(captured['cases']), 'cases')
