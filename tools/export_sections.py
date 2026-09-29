#!/usr/bin/env python3
"""Export the original section activation and texture-chunk streaming of a race event.

Output: web/public/assets/<LOC>/SECTIONS/sections.json (git-ignored), consumed by
engine/section_streaming.hpp (schema in the header comment and in `schema` below).

Recovered system (SLUS_207.72), see docs in the header and the task report:

Section activation = proximity activation manager A = *(game+0xA4)
(game = *(*(*(gp-0x848)+0x84)+0x0C), ctor 0x1286A0, 0x8E0 bytes):
  +0 point count, +0x10+0x20*i pointer to a tracked position, +0x20+0x20*i position at the last scan,
  +0xD0 last scan tick (-1 = rescan), +0xD4 parity, +0xD8 active count, +0xDC.. active instances.
  The only point is the human rider's rider+0x78C vector (0x11BB40, human created by 0x129C00),
  which 0x120E30 fills with rider+0x110 at the end of the rider pass 0x121818.
  0x101B60(A) runs once per game tick at the end of the rider manager 0x128AF0 (after every
  rider pass and the RFX updates, before game+8 is incremented):
    scan when A+0xD0 < 0, or tick - A+0xD0 >= 20, or |pos - A+0x20| >= 2000 cm.
    Box = pos -/+ (15000,15000,15000) (0x4A61E0).  The octree (8 roots {level,x,y,z,node})
    is walked: a node whose loose cell [(c-0.2)*2^L, (c+1.2)*2^L] overlaps the box contributes
    its whole subtree when level < 14 (0x101A28 recurse=1), otherwise its own instances and its
    children are tested (0x144368).  Collected = instances passing 0x30A310: has an entity
    (+0xC), or its track's stage row kind is 3 and it has a slot-1 or slot-3 handler.
    Leave = previous list - collected, enter = collected - previous, both merge-sorted by
    resource (+0x78) ascending (0x13F8F8); leave handlers (0x30A460) run first, then enter
    handlers (0x30A3A0).  Scans happen at the end of tick T with the position after tick T's
    rider physics (= capture record T+1 position); entities created there first update in T+1.
  0x103358 (race start/restart 0x129768) clears the list and sets A+0xD0 = -1 (entities kept).
  0x34FB00 (every entity ctor except types 6/16/22) inserts an instance created outside a scan
  into the list (0x1032C0); 0x103308 drops a location's instances on location unload.
  Instance octree cell = 0x328F28(bounds): level = max(11, exponent(0.714286 * max extent)),
  c = floor(centre / 2^level), grown until the loose cell contains the bounds; moving entities
  are relocated with their entity bounds (0x3291E0).

Texture chunks: W = **(gp+0x16C8), W+0x3EC count, W+0x3F0+24*c {state, lock, handle, priority,
  touch time, free frame}: 0 free, 1 requested, 2 loading, 3 resident, 4 releasing.
  Per frame 0x3A8290: 0x3A9258 (location chunks locked; viewer traversal 0x3A9D60 ->
  0x3AA028 over each location's chunk tree requests leaves within the viewer range, priority
  0.8*(1-d/range), 0.8 inside), 0x3A8668 (expire after W+0x1BF0 = 6 frames without request,
  start loading the highest-priority request when the loader is idle), 0x3A7098 (loader).
  Viewer 0 = the camera eye (DEFAULT_3 camera +0x40), range = 1.5 * 30000 cm (0x15EC98).

SPDX-License-Identifier: GPL-3.0-only
"""
import argparse, glob, hashlib, json, math, struct, sys, zipfile
from collections import Counter, defaultdict
from fractions import Fraction
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import location as location_info, state as location_state  # noqa: E402
from set_piece_location import Location  # noqa: E402

GP = 0x4A30F0
RUNS = ROOT / 'local/ps2-capture/runs'
CAPTURES = {'ARA1': RUNS / 'sections/ara1-full.bin', 'BRA2': RUNS / 'sections/bra2-full.bin', 'BHP1': RUNS / 'sections/bhp1-full.bin',
            # R&B / Crow's Nest: the setpieces runs carry the activation / chunk / viewer watches (no --ai-state: no RNG log)
            'ASS1': RUNS / 'setpieces-ass1/full.bin', 'ABA1': RUNS / 'setpieces-aba1/full.bin', 'ABC1': RUNS / 'setpieces-abc1/full.bin'}
CAPTURES.update({c: RUNS / f'peak2/{c.lower()}-full.bin' for c in ('CRA3', 'DRA4', 'DSS2', 'CBA2', 'CHP2', 'DBC2')})   # Peak 2 (docs/peak2.md)
SNAPSHOTS = {'ARA1': str(RUNS / 'setpieces/full.tick*.p2s'), 'BRA2': str(RUNS / 'setpieces-bra2/full.tick*.p2s'),
             'BHP1': str(RUNS / 'setpieces-bhp1/full.tick*.p2s'), 'ASS1': str(RUNS / 'setpieces-ass1/full.tick*.p2s'),
             'ABA1': str(RUNS / 'setpieces-aba1/full.tick*.p2s'), 'ABC1': str(RUNS / 'setpieces-abc1/full.tick*.p2s')}
SNAPSHOTS.update({c: str(RUNS / f'peak2/{c.lower()}-full.tick*.p2s') for c in ('CRA3', 'DRA4', 'DSS2', 'CBA2', 'CHP2', 'DBC2')})   # Peak 2
PEAK3_RUNS = {'ERA5': 'gravitude', 'ESS3': 'kick-doubt', 'EBA3': 'much-2-much', 'EHP3': 'perpendiculous', 'EBC3': 'the-throne'}   # Peak 3 (docs/peak3.md): runs/peak3/<name>-full.*
CAPTURES.update({c: RUNS / f'peak3/{n}-full.bin' for c, n in PEAK3_RUNS.items()})
SNAPSHOTS.update({c: str(RUNS / f'peak3/{n}-full.tick*.p2s') for c, n in PEAK3_RUNS.items()})
BOX_HALF, SCAN_PERIOD, SCAN_DISTANCE, SUBTREE_LEVEL, MIN_LEVEL = 15000.0, 20, 2000.0, 14, 11
LOOSE_BITS, EXTENT_BITS = 0x3E4CCCCD, 0x3F36DB6E
CHUNK_TIMEOUT, CHUNK_FREE_FRAMES, VIEW_RANGE = 6, 5, 45000.0
RNG_MARKER_101B60 = 0x12912C       # return address of the 101B60 call in 128AF0 (ps2_capture MARKERS)
AI_RNG = 29424                     # tools/ps2_capture.py ai_layout(): draws, total, marker a0/ra, ...; log follows
VT = {0x490E80: 'object', 0x490B10: 'livecomp', 0x48FC10: 'flag', 0x48EE60: 'emitter', 0x491800: 'type16',
      0x491B00: 'dead', 0x491680: 'restore', 0x4908F8: 'teeter'}


# ---- EE FPU / VU0 binary32 arithmetic (round toward zero; DIV rounds to nearest) ---------------
def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]
def fbits(x): return struct.unpack('<I', struct.pack('<f', x))[0]
def bitsf(b): return struct.unpack('<f', struct.pack('<I', b & 0xFFFFFFFF))[0]


def _toward_zero(q):
    r = f32(float(q))
    if math.isinf(r): return bitsf(fbits(r) - 1)
    if r != 0 and ((r > 0 and Fraction(r) > q) or (r < 0 and Fraction(r) < q)): r = bitsf(fbits(r) - 1)
    return r


def fadd(a, b): return _toward_zero(Fraction(a) + Fraction(b))     # VU0 ADD (chop)
def fsub(a, b): return _toward_zero(Fraction(a) - Fraction(b))     # VU0 SUB (chop)
def fmul(a, b): return _toward_zero(Fraction(a) * Fraction(b))     # EE MUL.S / VU0 MUL (chop)


def ee_addsub(a, b, subtract=False):
    """EE ADD.S/SUB.S (engine/original_float.hpp originalScalarAddSub): operands aligned with one guard bit, chop."""
    x, y = fbits(a), fbits(b); d = ((x >> 23) & 255) - ((y >> 23) & 255)
    if d >= 25: y &= 0x80000000
    elif d <= -25: x &= 0x80000000
    elif d > 0: y &= (0xFFFFFFFF << (d - 1)) & 0xFFFFFFFF
    elif d < 0: x &= (0xFFFFFFFF << (-d - 1)) & 0xFFFFFFFF
    return fsub(bitsf(x), bitsf(y)) if subtract else fadd(bitsf(x), bitsf(y))


def ee_add(a, b): return ee_addsub(a, b)
def ee_sub(a, b): return ee_addsub(a, b, True)


LOOSE, EXTENT = bitsf(LOOSE_BITS), bitsf(EXTENT_BITS)


def pow2(level): return bitsf((level + 127) << 23)


def cell_from_bounds(lo, hi):
    """0x328F28: octree cell (level, x, y, z) of a bounding box."""
    ext = [fsub(h, l) for l, h in zip(lo, hi)]
    cen = [fadd(l, fmul(e, 0.5)) for l, e in zip(lo, ext)]
    m = ext[0]
    if m < ext[1]: m = ext[1]
    if m < ext[2]: m = ext[2]
    level = max(MIN_LEVEL, ((fbits(fmul(m, EXTENT)) >> 23) & 0xFF) - 127)
    s = pow2(level)
    c = []
    for v in cen:
        q = f32(v / s); t = int(q)
        if q < float(t): t -= 1
        c.append(t)
    x, y, z = c
    while True:
        s = pow2(level)
        if all(fmul(ee_sub(float(k), LOOSE), s) <= l and h <= fmul(ee_add(float(k + 1), LOOSE), s) for k, l, h in zip((x, y, z), lo, hi)):
            return (level, x, y, z)
        level += 1; x >>= 1; y >>= 1; z >>= 1


def collect_cell(cell):
    """The node whose overlap decides collection (0x101B60/0x144368): the cell itself at level >= 14,
    else its level-13 ancestor (whole subtrees below level 14 are taken)."""
    level, x, y, z = cell
    if level >= SUBTREE_LEVEL: return cell
    s = SUBTREE_LEVEL - 1 - level
    return (SUBTREE_LEVEL - 1, x >> s, y >> s, z >> s)


def box(pos):
    return [fsub(f32(p), BOX_HALF) for p in pos[:3]], [fadd(f32(p), BOX_HALF) for p in pos[:3]]


def overlaps(cell, bmin, bmax):
    level, *c = cell; s = pow2(level)
    return all(fmul(ee_sub(float(k), LOOSE), s) <= b and a <= fmul(ee_add(float(k + 1), LOOSE), s) for k, a, b in zip(c, bmin, bmax))


# ---- savestates ---------------------------------------------------------------------------------
class Memory:
    def __init__(self, path):
        self.path = Path(path)
        with zipfile.ZipFile(path) as z: self.ee = z.read('eeMemory.bin')
    def u(self, a): return struct.unpack_from('<I', self.ee, a & 0x1FFFFFF)[0]
    def i(self, a): return struct.unpack_from('<i', self.ee, a & 0x1FFFFFF)[0]
    def h(self, a): return struct.unpack_from('<h', self.ee, a & 0x1FFFFFF)[0]
    def b(self, a): return self.ee[a & 0x1FFFFFF]
    def f(self, a): return struct.unpack_from('<f', self.ee, a & 0x1FFFFFF)[0]
    def v(self, a, n=3): return list(struct.unpack_from(f'<{n}f', self.ee, a & 0x1FFFFFF))
    def game(self): return self.u(self.u(self.u(GP - 0x848) + 0x84) + 0x0C)
    def activation(self): return self.u(self.game() + 0xA4)
    def world(self): return self.u(self.u(GP + 0x16C8))
    def tick(self): return self.u(self.game() + 8)
    def active_list(self):
        a = self.activation(); return [self.u(a + 0xDC + 4 * k) for k in range(self.u(a + 0xD8))]


def octree_nodes(m):
    world = m.u(m.u(m.u(GP - 0x848) + 0x84) + 0x20); out = []
    def walk(node, level, x, y, z):
        out.append((node, (level, x, y, z)))
        for c in range(8):   # child slot = 4*dx + 2*dy + dz (0x101B60 child order)
            ch = m.u(node + 4 * c)
            if ch: walk(ch, level - 1, 2 * x + (c >> 2 & 1), 2 * y + (c >> 1 & 1), 2 * z + (c & 1))
    for slot in range(8):
        e = world + 20 * slot
        if m.u(e + 16): walk(m.u(e + 16), m.i(e), m.i(e + 4), m.i(e + 8), m.i(e + 12))
    return out


def linked(m, head):
    out = []; p = m.u(head)
    while p: out.append(p); p = m.u(p)
    return out


def handler(m, inst, slot):
    """(row kind of the instance's track, slot program word or -1): 0x30A310 / 0x3A6B78 / 0x3AD120."""
    table = m.u(m.u(m.u(GP + 0xCE8) + 0x28C) + 4)
    entry = m.u(table + 4) + 12 * m.b(inst + 0x78)
    kind, rows, binding = m.h(entry), m.u(entry + 8), m.u(inst + 0x88)
    if not binding or not rows or m.u(binding + 8) == 0xFFFFFFFF: return kind, -1
    return kind, m.i(m.u(rows + 0x1C) + (m.u(binding + 8) >> 8) * 0x18 + 4 * slot)


def entity_kind(m, e):
    """Leave behaviour class of an entity (0x30A460): multispline (vt+0xA0 true: modifier vt+0xD0 == 2),
    dead (type 6), type16 (vt+0x118 empty), plain (vt+0x118 = 0x34FD90: restore flags, destroy)."""
    if not e: return None
    vt, typ = m.u(e + 0xC), m.h(e + 0x10)
    container = m.u(e + 0x1C); modifier = m.u(container) if container else 0
    if typ == 6: return 'dead'
    if vt in (0x490E80, 0x490B10, 0x48EE60, 0x4908F8) and modifier and m.u(m.u(modifier) + 0xD4) == 0x361C70: return 'multispline'
    if m.u(vt + 0x11C) in (0x360AA0, 0x360A58): return 'type16'
    return 'plain'


def entity_class(m, e):
    if not e: return None
    container = m.u(e + 0x1C); modifier = m.u(container) if container else 0
    return VT.get(m.u(e + 0xC), hex(m.u(e + 0xC))) + (f':{m.u(modifier):x}' if modifier else '')


def instance_rows(m):
    rows = {}
    for node, cell in octree_nodes(m):
        for inst in linked(m, node + 0x20):
            kind, s1 = handler(m, inst, 1); _, s3 = handler(m, inst, 3)
            e = m.u(inst + 0xC)
            rows[inst] = dict(inst=inst, resource=m.u(inst + 0x78), cell=cell, stage=kind == 3, slot1=s1, slot3=s3,
                              entity=entity_kind(m, e), entity_class=entity_class(m, e), flags=m.u(inst + 8),
                              bounds=[m.v(inst + 0x60), m.v(inst + 0x6C)], chunk=[m.b(inst + 0x7D), m.h(inst + 0x7E)])
    return rows


def patch_rows(m):
    out = []
    for node, cell in octree_nodes(m):
        for p in linked(m, node + 0x24):
            out.append(dict(resource=m.u(p + 0x150), track=m.b(p + 0x155), chunk=m.h(p + 0x156)))
    return out


def chunk_table(m):
    W = m.world(); n = m.u(W + 0x3EC)
    return [dict(chunk=c, state=m.u(W + 0x3F0 + 24 * c), lock=m.u(W + 0x3F4 + 24 * c), priority=m.f(W + 0x3FC + 24 * c),
                 touch=m.u(W + 0x400 + 24 * c)) for c in range(n)]


def chunk_trees(m):
    """Per resident location: 0x58-byte entry (+0x18 locked chunk, +0x1C tree), tree nodes
    {min +0, max +0x10, plane A +0x20 (child A when plane.(view min,1) < 0), plane B +0x30
    (child B when plane.(view max,1) < 0), child A +0x50, child B +0x54, leaf +0x58 -> u16 +2 chunk}."""
    W = m.world(); L = W + 0x10; n = m.u(m.u(L + 4) + 8); entries = m.u(L + 8); out = []
    for i in range(n):
        state = m.u(L + 0x14 + 8 * i)
        if not state: continue
        e = entries + 0x58 * i; nodes = []; index = {}
        def walk(node):
            if not node: return -1
            k = len(nodes); index[node] = k; nodes.append(None)
            leaf = m.u(node + 0x58)
            a, b = walk(m.u(node + 0x50)), walk(m.u(node + 0x54))
            nodes[k] = dict(min=m.v(node), max=m.v(node + 0x10), plane_a=m.v(node + 0x20, 4), plane_b=m.v(node + 0x30, 4),
                            child_a=a, child_b=b, chunk=(m.u(leaf) >> 16) if leaf else -1,
                            leaf_words=[m.u(leaf) & 0xFFFF, m.u(leaf + 4)] if leaf else None)
            return k
        walk(m.u(e + 0x1C))
        out.append(dict(track=i, name=m.ee[e:e + 16].split(b'\0')[0].decode(), state=state, locked_chunk=m.u(e + 0x18), nodes=nodes))
    return out


# ---- stage programs -------------------------------------------------------------------------------
def program_summary(program):
    """Builtins in call order and the gameplay-RNG (0x317810) draws of a program run:
    builtin3 (LiveComp 0x341AA0) constructs only when its target (key 0 resource, default the running
    instance) has no entity (else 0x2FAE38 updates the existing one) and then draws once for key 8
    (random start, 0x341BB4) and once more for a non-zero key 6 (rate spread, 0x341BE8);
    builtin19 (SplineModifier 0x359460) draws once; builtin77 (script random 0x303598) once.
    builtin52(target) = "target has an entity"; `if builtin52(x) == 1 return` guards the rest."""
    calls, args, sources, guard = [], {}, [], None
    ins = program['instructions']
    for n, x in enumerate(ins):
        op, wd = x['opcode'], x['word']; b1 = (wd >> 8) & 255
        if op in (0x28, 0x25):
            args[b1] = x['inline_words'][0] if op == 0x25 else (wd >> 16) & 0xFFFF
        elif op in (0x29, 0x26):
            args[b1] = (struct.unpack('<f', struct.pack('<I', x['inline_words'][0]))[0] if op == 0x26
                        else float(struct.unpack('<h', struct.pack('<H', (wd >> 16) & 0xFFFF))[0]))
        elif op == 0x27: args[b1] = ('resource', x['inline_words'][0])
        elif op == 0x20: args[b1] = 'register'
        elif op == 0x21:
            b = x['builtin']; calls.append(b)
            target = args[0][1] if isinstance(args.get(0), tuple) else 'self'
            if b == 3:
                if args.get(8) not in (None, 0): sources.append(dict(kind='builtin3:key8', target=target))
                if args.get(6) not in (None, 0, 0.0): sources.append(dict(kind='builtin3:key6', target=target))
            elif b in (19, 77): sources.append(dict(kind=f'builtin{b}', target=target))
            elif b == 52 and not calls[:-1] and n + 4 < len(ins) and [i['opcode'] for i in ins[n + 1:n + 6]][:5] == [0x16, 0x03, 0x02, 0x1E, 0x1F]:
                guard = target
            args = {}
    return dict(builtins=calls, rng_draws_if_constructed=len(sources), rng_draw_sources=[d['kind'] for d in sources],
                rng_draw_targets=[d['target'] for d in sources], guard=guard)


# ---- captures (tools/ps2_capture.py --ai-state --watch act+0xD0:0x400 --watch W+0x3F0.. ...) ------
def read_capture(path, m):
    path = Path(path)
    manifest = json.loads(path.with_suffix('.capture.json').read_text())
    size, layout = manifest['record'], manifest['layout']
    windows = [(int(w['address'], 16), w['length']) for w in layout['watches']]
    A, W = m.activation(), m.world()
    data = path.read_bytes(); records = []
    for at in range(0, len(data) - size + 1, size):
        r = data[at:at + size]; tick = struct.unpack_from('<I', r, 4)[0]
        if records and tick <= records[-1]['tick']: break          # post-race restart
        o = layout['watch_offset']; act = None; chunks = {}; viewer = None
        for address, length in windows:
            w = r[o:o + length]; o += length
            if address == A + 0xD0:
                d0, d4, d8 = struct.unpack_from('<iII', w)
                act = dict(d0=d0, parity=d4, list=list(struct.unpack_from(f'<{min(d8, (length - 12) // 4)}I', w, 12)), count=d8)
            elif W + 0x3F0 <= address < W + 0x3F0 + 24 * 159:
                first = (address - W - 0x3F0) // 24
                for k in range(length // 24): chunks[first + k] = struct.unpack_from('<I', w, 24 * k)[0]
            elif address == W + 0x250:
                viewer = list(struct.unpack_from('<3f', w, 0x10)) + [struct.unpack_from('<f', w, 0x40)[0]]   # eye, range
        if 'ai_state' in layout:
            draws, total = struct.unpack_from('<2I', r, AI_RNG)
            log = [struct.unpack_from('<4I', r, AI_RNG + 32 + 16 * k) for k in range(min(draws, 64))]
        else: log = []   # capture without the per-draw RNG attribution
        records.append(dict(tick=tick, position=list(struct.unpack_from('<3f', r, 32 + 0x10)), act=act, chunks=chunks, viewer=viewer,
                            world_draws=[f'{e[0]:x}' for e in log if e[3] == RNG_MARKER_101B60]))
    return manifest, records


# ---- model (mirrors engine/section_streaming.hpp) --------------------------------------------------
class ActivationModel:
    def __init__(self, rows):
        self.rows = rows                                 # inst -> row
        self.cells = defaultdict(list)
        for r in rows.values(): self.cells[collect_cell(r['cell'])].append(r['inst'])
        self.entity = {i: r['entity'] for i, r in rows.items()}
        self.active, self.last_tick, self.last = [], -1, None
    def eligible(self, i):
        r = self.rows[i]
        return self.entity[i] is not None or (r['stage'] and (r['slot1'] != -1 or r['slot3'] != -1))
    def due(self, tick, pos):
        if self.last_tick < 0 or tick - self.last_tick >= SCAN_PERIOD: return True
        d = math.sqrt(sum((f32(a) - f32(b)) ** 2 for a, b in zip(pos, self.last)))
        return d >= SCAN_DISTANCE
    def scan(self, tick, pos, created_kind):
        bmin, bmax = box(pos); collected = []
        for cell, insts in self.cells.items():
            if overlaps(cell, bmin, bmax): collected += [i for i in insts if self.eligible(i)]
        old, new = set(self.active), set(collected); res = lambda i: self.rows[i]['resource']
        leave, enter = sorted(old - new, key=res), sorted(new - old, key=res); events = []
        for i in leave:
            r, k = self.rows[i], self.entity[i]
            if k == 'multispline': events.append((res(i), 'leave', 'multispline_release', -1))
            elif k in (None, 'dead', 'type16') or r['slot3'] != -1:
                events.append((res(i), 'leave', 'slot3' if r['slot3'] != -1 else 'none', r['slot3'] >> 8 if r['slot3'] != -1 else -1))
            else: events.append((res(i), 'leave', 'destroy', -1)); self.entity[i] = None
        for i in enter:
            r, k = self.rows[i], self.entity[i]
            if k is not None: events.append((res(i), 'enter', 'multispline_acquire' if k == 'multispline' else 'none', -1))
            elif r['stage'] and r['slot1'] != -1:
                events.append((res(i), 'enter', 'slot1', r['slot1'] >> 8)); self.entity[i] = created_kind.get(r['slot1'] >> 8)
            else: events.append((res(i), 'enter', 'none', -1))
        self.active, self.last_tick, self.last = collected, tick, list(pos)
        return events
    def inserted(self, inst):
        """0x34FB00 -> 0x1032C0: an entity constructed on an instance that is not in the list."""
        if inst in self.rows and inst not in set(self.active):
            self.active.append(inst)
            if self.entity[inst] is None: self.entity[inst] = 'plain'


def replay(rows, records, anchor_list, created_kind):
    """Replays the capture: scans at the recorded D0 ticks with the record D0+1 position; instances
    inserted between scans (GO doors, trigger pieces) are applied as 0x1032C0 insertions."""
    model = ActivationModel(rows); model.active = list(anchor_list); model.last_tick = 0
    by_tick = {r['tick']: r for r in records}; scans = []; exact = mismatch = 0; last_d0 = records[0]['act']['d0']; slot_exact = [0]
    predicted_scan_ticks = []
    for r in records[1:]:
        act = r['act']
        if act['d0'] != last_d0:
            tick = act['d0']; pos = by_tick[tick + 1]['position'] if tick + 1 in by_tick else r['position']
            events = model.scan(tick, pos, created_kind)
            # movers (entity-routed instances relocated by 0x3291E0) are compared by the browser with live bounds
            movers = {i for i, x in rows.items() if x['moving']} | (set(act['list']) - set(rows))
            ok = set(act['list']) - movers == set(model.active) - movers and len(act['list']) == len(set(act['list']))
            slots = {i for i, x in rows.items() if x['stage'] and (x['slot1'] != -1 or x['slot3'] != -1)}
            slot_ok = (set(act['list']) & slots) - movers == (set(model.active) & slots) - movers
            exact += ok; mismatch += not ok; slot_exact[0] += slot_ok
            if not ok:   # resync on the captured list (entity lifetimes the model does not see)
                for i in set(act['list']) - set(model.active):
                    if i in rows and model.entity[i] is None: model.entity[i] = 'plain'
                model.active = [i for i in act['list'] if i in rows]   # runtime-allocated clones are not modeled
            scans.append(dict(tick=tick, exact=ok, enter=[e for e in events if e[1] == 'enter'], leave=[e for e in events if e[1] == 'leave'],
                              world_draws=by_tick[tick + 1]['world_draws'] if tick + 1 in by_tick else []))
            last_d0 = tick
        else:
            for i in act['list']:
                if i not in set(model.active): model.inserted(i)
    # independent scan-tick prediction (0x101B60 trigger) from the recorded positions
    last_tick, last = 0, by_tick[records[0]['tick']]['position']
    for t in range(records[0]['tick'], records[-1]['tick']):
        if t + 1 not in by_tick: break
        pos = by_tick[t + 1]['position']
        d = math.sqrt(sum((a - b) ** 2 for a, b in zip(pos, last)))
        if t - last_tick >= SCAN_PERIOD or d >= SCAN_DISTANCE: predicted_scan_ticks.append(t); last_tick, last = t, pos
    return scans, exact, mismatch, predicted_scan_ticks, slot_exact[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--location', default='ARA1'); ap.add_argument('--capture'); ap.add_argument('--output')
    args = ap.parse_args(); code = args.location; info = location_info(code)
    anchor, ready = Memory(location_state(code, 'anchor')), Memory(location_state(code, 'ready'))
    loc = Location(code)
    rows = instance_rows(anchor); ready_rows = {r['resource']: r for r in instance_rows(ready).values()}
    # cells from bounds (0x328F28) must reproduce the octree except for entity-routed movers
    world = json.loads((ROOT / f'local/assets/native/{code}/world_collision.json').read_text())
    package = {(i['rid'] << 8) | i['track']: i for i in world['instances']}
    moving, cell_checked = [], 0
    for r in rows.values():
        computed = cell_from_bounds(*r['bounds']); r['moving'] = computed != tuple(r['cell'])
        if r['moving']: moving.append(r['resource'])
        cell_checked += 1
        p = package.get(r['resource'])
        if p and (p['bounds_min_cm'] != r['bounds'][0] or p['bounds_max_cm'] != r['bounds'][1]):
            raise ValueError(f'{code}: package bounds differ from the runtime instance {r["resource"]:#x}')
    # stage handler rows: runtime lookup vs the stage record (set_piece_location)
    staged = {(i['rid'] << 8) | i['track']: row for i, row in loc.handler_rows()}
    for r in rows.values():
        row = staged.get(r['resource'])
        if r['stage'] and row is not None and ((row[1] if row[1] != 0xFFFFFFFF else -1) & 0xFFFFFFFF, (row[3] if row[3] != 0xFFFFFFFF else -1) & 0xFFFFFFFF) != (r['slot1'] & 0xFFFFFFFF, r['slot3'] & 0xFFFFFFFF):
            raise ValueError(f'{code}: runtime handler row of {r["resource"]:#x} differs from the stage record')
    programs = {}
    for r in rows.values():
        for slot in ('slot1', 'slot3'):
            if r['stage'] and r[slot] != -1:
                pid = r[slot] >> 8; programs.setdefault(pid, dict(program_summary(loc.programs[pid]), slots=set()))['slots'].add(int(slot[-1]))
    # entity kind each slot-1 program leaves on its own instance (all snapshots of the course run)
    kinds = defaultdict(Counter)
    for path in sorted(glob.glob(SNAPSHOTS[code])) + [str(location_state(code, 'anchor'))]:
        m = Memory(path); act = set(m.active_list())
        for r in instance_rows(m).values():
            if r['stage'] and r['slot1'] != -1 and r['inst'] in act and r['resource'] not in moving: kinds[r['slot1'] >> 8][r['entity']] += 1
    created = {}
    for pid, c in kinds.items():
        (kind, n), = c.most_common(1)
        created[pid] = kind
        programs.setdefault(pid, dict(program_summary(loc.programs[pid]), slots={1}))['creates'] = kind
        programs[pid]['creates_observed'] = {str(k): v for k, v in c.items()}
    # capture: scans, RNG draws of 101B60, chunk states
    capture = Path(args.capture) if args.capture else CAPTURES[code]
    reference = None; chunk_events = []; latency = defaultdict(list)
    if capture.exists():
        manifest, records = read_capture(capture, anchor)
        if manifest['baseline_sha256'] != hashlib.sha256(location_state(code, 'anchor').read_bytes()).hexdigest():
            raise ValueError('Capture baseline is not the location anchor')
        scans, exact, mismatch, predicted, slot_exact = replay(rows, records, anchor.active_list(), created)
        captured_ticks = [s['tick'] for s in scans]
        draw_ok = sum(1 for s in scans if len(s['world_draws']) == sum(programs.get(e[3], {}).get('rng_draws_if_constructed', 0) for e in s['enter'] if e[2] == 'slot1'))
        prev = records[0]['chunks']; loading = {}
        for rec in records[1:]:
            for c, st in rec['chunks'].items():
                if st != prev.get(c):
                    chunk_events.append([rec['tick'], c, prev.get(c), st])
                    if st == 2: loading[c] = rec['tick']
                    if st == 3 and c in loading: latency[c].append(rec['tick'] - loading.pop(c))
            prev = rec['chunks']
        reference = dict(capture=str(capture.relative_to(ROOT)), sha256=hashlib.sha256(capture.read_bytes()).hexdigest(),
                         first_tick=records[0]['tick'], last_tick=records[-1]['tick'],
                         scans_exact=exact, scans_resynced=mismatch, scans_exact_handler_instances=slot_exact, scans_total=len(scans), scan_ticks_predicted_equal=predicted == captured_ticks[:len(predicted)] and len(predicted) >= len(captured_ticks) - 1,
                         scan_draws_matching=draw_ok, scans=[dict(tick=s['tick'], enter=[e[:1] + e[2:] for e in s['enter']], leave=[e[:1] + e[2:] for e in s['leave']],
                                                                   world_draws=s['world_draws']) for s in scans if s['enter'] or s['leave'] or s['world_draws']],
                         chunk_events=chunk_events)
    names = {k: v.get('name') for k, v in package.items()}
    terrain = json.loads((ROOT / f'local/assets/native/{code}/terrain.json').read_text())['patches']
    patch_index = {p['resource_id']: k for k, p in enumerate(terrain)}
    patches = patch_rows(anchor)
    if {p['resource'] for p in patches} != set(patch_index): raise ValueError('Live patches differ from terrain.json')
    chunks = chunk_table(anchor); trees = chunk_trees(anchor); W = anchor.world()
    leaf_chunks = {n['chunk'] for t in trees for n in t['nodes'] if n['chunk'] >= 0}
    locked = {t['locked_chunk'] for t in trees if t['locked_chunk']} | {c['chunk'] for c in chunks if c['lock']}
    ready_chunks = chunk_table(ready)
    instances = []
    for r in sorted(rows.values(), key=lambda r: r['resource']):
        rr = ready_rows.get(r['resource'])
        eligible_ever = (r['stage'] and (r['slot1'] != -1 or r['slot3'] != -1)) or r['entity'] is not None or (rr and rr['entity'])
        instances.append(dict(resource=r['resource'], name=names.get(r['resource']), cell=list(r['cell']),
                              slot1=r['slot1'] >> 8 if r['stage'] and r['slot1'] != -1 else -1, slot3=r['slot3'] >> 8 if r['stage'] and r['slot3'] != -1 else -1,
                              entity_at_start=rr['entity'] if rr else r['entity'], entity_class_at_start=rr['entity_class'] if rr else r['entity_class'],
                              moving=r['moving'], chunk=r['chunk'], activation=bool(eligible_ever)))
    out = dict(
        version=1, location=code, name=info['name'],
        schema=('instances[]: resource (rid<<8|track, world_collision.json key), cell [level,x,y,z] (0x328F28 of the bounds; '
                'movers: entity bounds), slot1/slot3 stage program (-1 none), entity_at_start (ready savestate, race tick 0 before '
                'the first scan: null|plain|dead|type16|multispline), activation (can ever be in the list), chunk [track (255 = always), '
                'chunk]. programs{id}: builtins, rng_draws_if_constructed (gameplay RNG 0x317810 draws of one run on an '
                'instance without entity), creates (entity kind left on the instance). patches[]: resource_id, index into '
                'local/assets/native/<LOC>/terrain.json patches (= browser terrain package order), track, chunk. chunks[]: id, '
                'track, locked, resident_at_start, load_latency_ticks (capture), leaf bounds in chunk_trees. reference_run: the '
                'PS2 capture (scan ticks, enter/leave with actions, 0x101B60 RNG callers, chunk state changes).'),
        constants=dict(box_half_cm=BOX_HALF, scan_period_ticks=SCAN_PERIOD, scan_distance_cm=SCAN_DISTANCE, subtree_level=SUBTREE_LEVEL,
                       min_level=MIN_LEVEL, loose_bits=f'{LOOSE_BITS:#x}', extent_bits=f'{EXTENT_BITS:#x}', chunk_timeout_frames=anchor.u(W + 0x1BF0),
                       chunk_free_frames=CHUNK_FREE_FRAMES, viewer_range_cm=anchor.f(W + 0x290), viewer_priority_scale=0.8),
        provenance=dict(anchor=str(location_state(code, 'anchor').relative_to(ROOT)), ready=str(location_state(code, 'ready').relative_to(ROOT)),
                        cells_checked=cell_checked, movers_at_anchor=[f'{x:#x}' for x in moving]),
        initial=dict(active_after_tick0_scan=sorted(anchor.u(i + 0x78) for i in anchor.active_list()),
                     resident_chunks=sorted(c['chunk'] for c in ready_chunks if c['state'] == 3),
                     scan_tick0_position=anchor.v(anchor.activation() + 0x20)),
        programs={str(k): dict(v, slots=sorted(v['slots'])) for k, v in sorted(programs.items())},
        instances=[i for i in instances if i['activation']],
        instance_chunks={f'{i["resource"]:#x}': i['chunk'] for i in instances},
        patches=[dict(resource_id=p['resource'], index=patch_index[p['resource']], track=p['track'], chunk=p['chunk'])
                 for p in sorted(patches, key=lambda p: patch_index[p['resource']])],
        chunks=[dict(id=c, locked=c in locked, resident_at_start=next(x['state'] for x in ready_chunks if x['chunk'] == c) == 3,
                     load_latency_ticks=sorted(set(latency.get(c, []))))
                for c in sorted(leaf_chunks | locked)],
        chunk_trees=trees, reference_run=reference)
    target = Path(args.output) if args.output else ROOT / 'web/public/assets' / code / 'SECTIONS/sections.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(out, separators=(',', ':')) + '\n')
    summary = dict(output=str(target.relative_to(ROOT)), instances=len(out['instances']), programs=len(programs), patches=len(patches),
                   chunks=[(c['id'], c['locked'], c['resident_at_start'], c['load_latency_ticks']) for c in out['chunks']])
    if reference: summary.update({k: reference[k] for k in ('scans_total', 'scans_exact', 'scans_exact_handler_instances', 'scans_resynced', 'scan_ticks_predicted_equal', 'scan_draws_matching')}, scans=len(reference['scans']))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
