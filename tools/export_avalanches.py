#!/usr/bin/env python3
"""Avalanches and rock slides (docs/avalanche.md): the recorded tumbler paths of a location, for the core playback and the draw.

Retail SSX 3 does not simulate an avalanche: it plays back motion recorded into the location's SSB kind-22 record
(0x2D9FB8 -> 0x2DA028), bound to the groups the stage defines (builtin 93 0x3059A0 -> 0x2D92D0), with the builtin-96
emitter parameters (0x305F40 -> 0x2D9538: the builtin-16 particle block) and the builtin-95 AvaSpline modifiers
(0x305D90 -> 0x355A78, modifier 0x48F338) that make the group instances follow their tumblers. See docs/avalanche.md.

The avalanches are defined by global stage handlers (ABC1 programs 63 and 87, ERA5 118, EBA3 74, ...). 0x2D92D0 inserts
each avalanche and each of its groups at the head of its list; 0x2DA028 parses the record from its start in list order
(newest avalanche first, newest group first), so the export replays the definitions in handler order and reverses.

Output: the runtime asset (the core reads it as is: keep the field set and the list order) at <out>/<LOC>/avalanches.json (the event
package) and <out>/PEAK<n>/<LOC>/avalanches.json (the streamed package; MOUNTAIN uses the PEAK roots). --out defaults to the scratch
folder, never web/public/assets. JSON: {location, source, record_sha256, record_size, avalanches: [{id, groups: [{resource, name, type,
duration, fade_in, fade_out, speed (the float words as the ctor stores them: x30 ticks except speed), size, start (3 f32),
samples (hex, 10 bytes each), emitter (the 0xD8-byte builtin-96 block as 54 words or null), ava_spline}], sounds:
[{tumbler, tick}], dead_events}]}.

usage: python3 tools/export_avalanches.py [--location ABC1 ...] [--out DIR] [--check-states GLOB]
  --check-states: compare the export with the avalanche lists / tumblers of PS2 savestates (group words, record bytes at
  +252, and the live tumblers' sample index / integrated position against the export's integration, 0x2D5778)."""
import argparse, glob, hashlib, json, re, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations  # noqa: E402
from import_sky import chunk_range  # noqa: E402
from set_piece_location import Location  # noqa: E402
from export_startfire import decode_program  # noqa: E402

GP = 0x4A30F0
ELF = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
e32 = lambda a: struct.unpack_from('<I', ELF, a - 0xFF000)[0]
f32 = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]
bits = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]


def chop(x):
    """The EE FPU / VU round toward zero: the float32 nearest x, one ulp toward zero when that rounded away."""
    r = f32(x)
    if abs(r) > abs(x):
        b = bits(r); r = struct.unpack('<f', struct.pack('<I', b - 1))[0]
    return r
MAGIC = 0x02BEEF00
LOCATIONS_WITH_AVALANCHES = ('ABC1', 'DBC2', 'DRA4', 'ERA5', 'ESS3', 'EBA3', 'EBC3')


def check_elf():
    for b, a in ((93, 0x3059A0), (94, 0x305C88), (95, 0x305D90), (96, 0x305F40)):
        assert e32(0x441F38 + 4 * b) == a, f'builtin {b}'
    assert e32(0x3059A0 + 0x268) == 0x0C000000 | (0x2D92D0 >> 2)       # 0x305C08 jal 0x2D92D0
    assert e32(0x305F40 + 0x224) == 0x0C000000 | (0x2D9538 >> 2)       # 0x306164 jal 0x2D9538
    assert e32(0x2D9FB8 + 0x58) == 0x0C000000 | (0x2DA028 >> 2)        # 0x2DA010 jal 0x2DA028


def types(table, n):
    return [e32(table + 4 * k) for k in range(n)]


T93, T96 = types(0x4466D8, 50), types(0x4467A0, 56)


def keyed(defaults, table, keys):
    """A builtin's argument block: defaults, then each key (an int for a float field is converted, cvt.s.w)."""
    out = list(defaults)
    for k, (kind, v) in keys.items():
        if kind == 'float': out[k] = ('f', f32(v))
        elif kind == 'resource': out[k] = ('i', v)
        elif table[k] == 2: out[k] = ('f', f32(float(v)))
        else: out[k] = ('i', v & 0xFFFFFFFF)
    return out


def defaults93():   # 0x3059E4.. (block 0x4FBD90, 50 words)
    d = [('i', 0)] + [('i', 0xFFFFFFFF)] * 8 + [('i', 0)] * 8 + [('f', 10.0)] * 8 + [('f', 0.0)] * 8 + [('f', 3.0)] * 8 + [('f', 1.0)] * 8
    return d[:49] + [('i', 0)]


def defaults96():   # 0x305F70.. (block 0x4FBE58, 56 words): the builtin-16 particle defaults, Instance (key 54) = -1
    d = [('f', 0.0)] * 56
    d[0], d[1], d[2], d[3], d[4] = ('i', 1), ('i', 0), ('f', -1.0), ('f', 1.0), ('f', 4.0)
    for k in (33, 37, 38, 39, 40, 41, 45): d[k] = ('f', 1.0)
    d[49], d[50], d[51], d[52], d[53], d[54], d[55] = ('i', 16), ('i', 0), ('f', 0.0), ('i', 1), ('f', 20.0), ('i', 0xFFFFFFFF), ('i', 0)
    return d


def word(v): return v[1] if v[0] == 'i' else bits(v[1])


def stage_calls(loc):
    """Builtin 93 / 95 / 96 calls in run order: the global handlers first (in index order), then every handler-row program."""
    progs = loc.programs if isinstance(loc.programs, dict) else {p['index']: p for p in loc.programs}
    order = [w >> 8 for w in loc.globals if w != 0xFFFFFFFF]
    rows = sorted({w >> 8 for _, row in loc.handler_rows() for w in row if w != 0xFFFFFFFF} - set(order))
    out = []
    for index in order + rows:
        p = progs.get(index)
        if p is None: continue
        try: calls = decode_program(p)
        except KeyError:   # a register the linear decoder cannot follow: fine unless the program calls one of ours
            if any(i['opcode'] == 0x21 and i.get('builtin') in (93, 95, 96) for i in p['instructions']): raise
            continue
        for b, _, keys in calls:
            if b in (93, 95, 96): out.append((index, index in order, b, keys))
    return out


def kind22(code):
    locs = locations(ROOT / 'local/assets/source/ps2/bam.sdb')
    index, begin, end = chunk_range(locs, code)
    for ci, chunk in enumerate(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb')):
        if begin <= ci <= end:
            for kind, track, rid, data in records(chunk):
                if kind == 22 and track == index and data: return data
    return None


def export(code):
    loc = Location(code)
    world = {(i['rid'] << 8) | i['track']: i for i in loc.world['instances']}
    avalanches, emitters, followers = [], {}, set()
    for program, is_global, b, keys in stage_calls(loc):
        if b == 93:
            if not is_global: raise ValueError(f'{code}: builtin 93 outside a global handler (program {program}): the record order would depend on sections')
            a = keyed(defaults93(), T93, keys)
            aid = a[0][1]
            av = next((x for x in avalanches if x['id'] == aid), None)
            if av is None: av = {'id': aid, 'groups': []}; avalanches.append(av)
            for k in range(8):
                res = a[1 + k][1]
                if res == 0xFFFFFFFF: continue
                inst = world.get(res)
                if inst is None: raise ValueError(f'{code}: avalanche {aid} group instance {res} not in the world')
                t = a[9 + k][1]
                lo, hi = inst['bounds_min_cm'], inst['bounds_max_cm']
                av['groups'].append(dict(resource=res, name=inst['name'], program=program, type=0 if t == 2 else 2 if t == 5 else 1,
                                         duration=bits(chop(a[17 + k][1] * 30.0)), fade_in=bits(chop(a[25 + k][1] * 30.0)),
                                         fade_out=bits(chop(a[33 + k][1] * 30.0)), speed=bits(a[41 + k][1]),
                                         extent_cm=[abs(hi[i] - lo[i]) for i in range(3)]))
        elif b == 95:
            res = keys.get(0, ('int', -1))[1]
            if res != -1: followers.add(res)
        elif b == 96:
            p = keyed(defaults96(), T96, keys)
            res = p[54][1]
            if res != 0xFFFFFFFF: emitters[res] = [word(v) for v in p[:54]]
    record = kind22(code)
    out = dict(location=code, source='SSX3 USA PS2 BAM.SSB kind 22 + stage builtins 93/95/96', avalanches=[])
    if record is None or not avalanches: return out
    if struct.unpack_from('<I', record, 0)[0] != MAGIC: raise ValueError(f'{code}: kind-22 magic')
    out['record_sha256'] = hashlib.sha256(record).hexdigest(); out['record_size'] = len(record)
    at = 8   # 0x2DA060: s0 = record + 8
    for av in reversed(avalanches):   # list head = the last defined (0x2D9364)
        for g in reversed(av['groups']):
            tag, size = struct.unpack_from('<2H', record, at); at += 4
            if tag != 0xBEEF: raise ValueError(f'{code}: avalanche {av["id"]} group tag {tag:#x} at {at - 4}')
            start = struct.unpack_from('<3I', record, at); at += 12
            n = int(struct.unpack('<f', struct.pack('<I', g['duration']))[0])   # trunc.w.s of +256
            if size != n * 10 + 12: raise ValueError(f'{code}: avalanche {av["id"]} {g["name"]}: block {size} for {n} samples')
            g.update(start=list(start), samples=record[at:at + n * 10].hex(), emitter=emitters.get(g['resource']), ava_spline=g['resource'] in followers, record_offset=at)
            at += n * 10
        n12, n14 = struct.unpack_from('<2H', record, at); at += 4
        av['dead_events'] = [dict(time=struct.unpack_from('<I', record, at + 8 * k)[0], instance=struct.unpack_from('<I', record, at + 8 * k + 4)[0]) for k in range(n12)]
        at += 8 * n12
        av['sounds'] = [dict(tumbler=struct.unpack_from('<H', record, at + 4 * k)[0], tick=struct.unpack_from('<H', record, at + 4 * k + 2)[0]) for k in range(n14)]
        at += 4 * n14
        av['groups'].reverse()   # list order (the tumblers take this order, 0x2D97A8)
    avalanches.reverse()
    if at != len(record): raise ValueError(f'{code}: parsed {at} of {len(record)} bytes')
    out['avalanches'] = avalanches
    return out


# --- 0x2D5778 position integration (s8 x 2 per sample, lerp the fraction), EE float order -----------------------------
def position(group, sample_index, frac):
    raw = bytes.fromhex(group['samples']); p = [struct.unpack('<f', struct.pack('<I', w))[0] for w in group['start']]
    for s in range(sample_index):
        d = struct.unpack_from('<3b', raw, 10 * s); p = [chop(p[i] + float(d[i] + d[i])) for i in range(3)]
    d = struct.unpack_from('<3b', raw, 10 * sample_index)
    return [chop(p[i] + chop(chop(d[i] * frac) * 2.0)) for i in range(3)]


def check_states(doc, pattern):
    by_resource = {g['resource']: (av, g) for av in doc['avalanches'] for g in av['groups']}
    worst, groups_checked, tumblers = 0.0, 0, 0
    for path in sorted(glob.glob(pattern)):
        ee = zipfile.ZipFile(path).read('eeMemory.bin')
        u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
        fl = lambda a: struct.unpack_from('<f', ee, a & 0x1FFFFFF)[0]
        a = u(GP + 2496); seen = []
        while a:
            ids = u(a); g = u(a + 4); order = []
            while g:
                inst = u(g + 248); res = u(inst + 0x78); order.append(res)
                av, eg = by_resource[res]
                assert av['id'] == ids, (path, res)
                for key, off in (('duration', 256), ('fade_in', 260), ('fade_out', 264), ('speed', 268)):
                    assert u(g + off) == eg[key], (path, eg['name'], key, hex(u(g + off)), hex(eg[key]))
                assert [u(g + 224 + 4 * i) for i in range(3)] == eg['start'], (path, eg['name'], 'start')
                n = len(eg['samples']) // 20
                assert ee[u(g + 252) & 0x1FFFFFF:(u(g + 252) & 0x1FFFFFF) + 10 * n].hex() == eg['samples'], (path, eg['name'], 'samples')
                assert struct.unpack_from('<H', ee, (g + 240) & 0x1FFFFFF)[0] == eg['type'], (path, eg['name'], 'type')
                groups_checked += 1; g = u(g + 244)
            assert order == [x['resource'] for x in next(v for v in doc['avalanches'] if v['id'] == ids)['groups']], (path, ids, 'group order')
            seen.append(ids); a = u(a + 8)
        assert seen == [v['id'] for v in doc['avalanches']], (path, seen)
        for k in range(16):   # live tumblers: 0x2D7CA8 left +0 = floor(t x speed x 30) and +96 = the lerped position
            s = 0x538938 + 28 * k
            if not u(s): continue
            t = fl(s + 24); tu = u(s + 4)
            while tu:
                g = u(tu + 736)
                if not g: tu = u(tu + 740); continue   # released (0x2D7DD8 clears +736, the tumbler stays linked)
                res = u(u(g + 248) + 0x78); av, eg = by_resource[res]
                x = chop(chop(t * fl(g + 268)) * 30.0); n = len(eg['samples']) // 20
                idx = int(x); frac = chop(x - idx)
                if idx >= n - 1: idx, frac = n - 2, 1.0
                if u(tu) == idx:
                    p = position(eg, idx, frac); got = [fl(tu + 96 + 4 * i) for i in range(3)]
                    worst = max(worst, max(abs(p[i] - got[i]) for i in range(3))); tumblers += 1
                tu = u(tu + 740)
    return groups_checked, tumblers, worst


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--location', action='append'); ap.add_argument('--out', type=Path, default=ROOT / 'local/export/avalanches')
    ap.add_argument('--check-states', action='append', default=[], help='LOC:GLOB of PS2 savestates of that location')
    args = ap.parse_args(); check_elf()
    if args.out.resolve().is_relative_to((ROOT / 'web/public/assets').resolve()): raise SystemExit('refusing to write into web/public/assets')
    docs = {}
    for code in args.location or LOCATIONS_WITH_AVALANCHES:
        doc = export(code); docs[code] = doc
        roots = [m['root'] for m in json.loads((ROOT / 'web/public/assets/MOUNTAIN/peak.json').read_text())['locations'] if m['code'] == code]
        for rel in [code] + [r.strip('/').removeprefix('assets/') for r in roots]:
            path = args.out / rel / 'avalanches.json'; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(doc, separators=(',', ':')) + '\n')
        print(code, len(doc['avalanches']), 'avalanches', sum(len(a['groups']) for a in doc['avalanches']), 'groups',
              sum(len(a.get('sounds', [])) for a in doc['avalanches']), 'sounds', doc.get('record_size'), 'bytes ->', path)
    for spec in args.check_states:
        code, pattern = spec.split(':', 1)
        n, t, worst = check_states(docs[code], pattern)
        print(f'{code}: {n} group records equal to the PS2 states (words, record bytes, order); {t} tumbler positions, worst {worst:.4f} cm')


if __name__ == '__main__':
    main()
