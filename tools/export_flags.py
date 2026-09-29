#!/usr/bin/env python3
"""Export a location's waving flags/streamers (flg_* instances) for the browser
(development export; --location, default ARA1: writes the git-ignored
web/public/assets/FLAGS/flags.json, other locations web/public/assets/<LOC>/FLAGS/flags.json,
and, when the location's race snapshots exist, flag-snapshots.json for web/test-flag-animation.mjs).
Stage/track/inputs per location: tools/set_piece_location.py. Wind mode per course: 0x2D1BA0
(course table 0x43D950 +0x54, plus 1; 1 for Snow Jam, Metro-City and The Junction).

Sources, all derived here rather than typed in:
- Stage kind-16 handler rows (bam.ssb chunk 33) slot 1 -> LUN programs 61..64,
  each a single builtin 12 (0x2FC9C8) call; keyed arguments over the builtin
  defaults (0x2FC9C8), key types from 0x445FC0 (int argument into a float field
  is converted). 0x34AC88 turns them into the 0x58-byte parameter block
  (engine/flag_cloth.hpp originalFlagParameters; chop products reproduced here).
- Corners as the renderer (0x380380) reads them: first mesh, first four strip
  vertices, position short * chop(scale * 1/32767), UV short/4096, colour of the
  instance V4-5 words (5-bit * 1/31, alpha = bit 15). The cloth grid is built
  from the first registered instance (lowest rid of the activation group, as
  observed in every race snapshot); all instances of a cloth draw that grid.
- Instance matrices from world_collision.json (row vectors, source cm, Z up).
Verification (when local/ps2-capture/runs/setpieces snapshots exist): cloth
parameter bits, grid height and rest-grid corners against the live flag
manager ([[gp-0x848]+0x84]+0x70, slots +0x20 stride 0x188).
"""
import argparse, glob, json, re, struct, sys, zipfile
from fractions import Fraction
from pathlib import Path
root = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(root / 'tools'))
from world_assets import world_chunks, records
from export_startfire import decode_program
import world_models
from set_piece_location import Location, SNAPSHOTS, web_asset_dir, course_wind_mode

GP = 0x4A30F0


def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]
def bits(x): return struct.unpack('<I', struct.pack('<f', x))[0]
def from_bits(b): return struct.unpack('<f', struct.pack('<I', b))[0]


def chop(exact):
    """Round an exactly representable double product/sum toward zero to float32."""
    r = f32(exact)
    if r != 0 and abs(r) > abs(exact): r = from_bits(bits(r) - 1)
    return r


def mul(a, b): return chop(a * b)


def div_nearest(a, b):
    q = Fraction(a) / Fraction(b); r = f32(float(q))
    best = min((r, from_bits(bits(r) + 1), from_bits(bits(r) - 1) if bits(r) & 0x7FFFFFFF else r), key=lambda c: abs(Fraction(c) - q))
    return best


def load_ee(path):
    with zipfile.ZipFile(path) as z: return z.read('eeMemory.bin')


class Mem:
    def __init__(self, m): self.m = m
    def u(self, a): return struct.unpack_from('<I', self.m, a & 0x1FFFFFF)[0]
    def f(self, a): return struct.unpack_from('<f', self.m, a & 0x1FFFFFF)[0]
    def fs(self, a, n): return list(struct.unpack_from('<%df' % n, self.m, a & 0x1FFFFFF))


def flag_manager(mm):
    return mm.u(mm.u(mm.u(GP - 0x848) + 0x84) + 0x70)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--location', default='ARA1')
    p.add_argument('--output', type=Path, default=None)
    p.add_argument('--snapshots', default=None)
    a = p.parse_args()
    loc = Location(a.location); track = loc.track
    a.output = a.output or web_asset_dir(a.location, 'FLAGS')
    a.snapshots = a.snapshots or SNAPSHOTS[a.location]
    elf = (root / 'local/disc/SLUS_207.72').read_bytes()
    u = lambda addr: struct.unpack_from('<I', elf, addr - 0xFF000)[0]
    assert u(0x441F38 + 12 * 4) == 0x2FC9C8, 'builtin 12 is not the flag builtin'
    key_types = [u(0x445FC0 + 4 * k) for k in range(26)]
    fk = lambda off: struct.unpack('<f', struct.pack('<I', u(GP + off)))[0]
    position_scale, colour_scale = fk(-0x2760), fk(-0x275C)
    world = loc.world
    programs = loc.programs
    model_records, instance_records, stage = {}, {}, None
    for ci, chunk in enumerate(world_chunks(root / 'local/assets/source/ps2/bam.ssb')):
        for kind, tr, rid, data in records(chunk):
            if ci == loc.chunk and kind == 16 and (tr == track or a.location == 'ARA1'): stage = data
            if tr == track and kind == 2: model_records[rid] = data
            if tr == track and kind == 3: instance_records[rid] = data
    rows, base = struct.unpack_from('<2I', stage, 0x18); assert a.location != 'ARA1' or (rows, base) == (231, 0x70)
    descriptors = world['bindings'][str(track)]['descriptors']

    def defaults():
        w = [0] * 26; fl = lambda k, v: w.__setitem__(k, bits(v))
        w[0] = 0xFFFFFFFF; w[1] = 1
        for k, v in {4: 1, 5: 25, 6: 1, 7: 1, 8: 0, 9: 1, 10: 1, 11: 0, 12: 1, 13: 1, 14: 0, 15: 1}.items(): fl(k, v)
        return w

    def parameters(words, fps=60):
        r = lambda k: from_bits(words[k]); inv = div_nearest(1.0, float(fps)); per = lambda k: mul(r(k), inv)
        return dict(attachStart=int(words[1] != 0), attachEnd=int(words[2] != 0), pinFirstRow=int(words[3] != 0), drawMode=int(words[25] != 0),
                    speed=[per(4 + 3 * i) for i in range(4)], amplitude=[r(5 + 3 * i) for i in range(4)], frequency=[r(6 + 3 * i) for i in range(4)],
                    calm=[r(16 + k) for k in range(3)], gust=[r(19 + k) for k in range(3)], minimumStrength=r(22), uvSpeed=[per(23), per(24)])

    # Raw MDR vertex shorts: decode_model with identity node transforms, inverted to shorts.
    def corners(model_rid, instance_rid):
        data = model_records[model_rid]; scale = struct.unpack_from('<3f', data, 24)
        saved = world_models.transform
        world_models.transform = lambda pt, m, normal=False: list(pt)
        try: mesh = world_models.decode_model(data)[0]
        finally: world_models.transform = saved
        verts = mesh['vertices']
        if len(verts) != 4: raise ValueError(f'flag model {model_rid} has {len(verts)} vertices')
        sc = [mul(scale[k], position_scale) for k in range(3)]
        pos, uv = [], []
        for v in verts:
            shorts = [round(v[k] * 32768 / scale[k]) if scale[k] else 0 for k in range(3)]
            if any(abs(shorts[k] / 32768 * scale[k] - v[k]) > 1e-9 for k in range(3)): raise ValueError('non-integral position short')
            pos.append([mul(float(shorts[k]), sc[k]) for k in range(3)])
            uv.append([v[6], v[7]])
        words = []
        rec = instance_records[instance_rid]; at = 160
        while at + 4 <= len(rec) and len(words) < 4:
            word = struct.unpack_from('<I', rec, at)[0]; at += 4
            if word >> 24 not in (0x6f, 0x7f): continue
            count = ((word >> 16) & 255) or 256
            words.extend(struct.unpack_from(f'<{count}H', rec, at)); at = (at + count * 2 + 3) & ~3
        colour = [[float(w >> 15), mul(float(w & 31), colour_scale), mul(float((w >> 5) & 31), colour_scale), mul(float((w >> 10) & 31), colour_scale)] for w in words[:4]]
        return dict(position=pos, uv=uv, colour=colour)

    flags = []
    for inst in world['instances']:
        if inst['track'] != track or not inst['name'].startswith('flg_'): continue
        resource08 = descriptors[inst['collision_descriptor']]['resource08']
        if resource08 == 0xFFFFFFFF: continue   # BigCFlag: free-ride type-16 nodes, never drawn in the race
        slot1 = struct.unpack_from('<6I', stage, base + (resource08 >> 8) * 24)[1]
        if slot1 == 0xFFFFFFFF: continue
        program = slot1 >> 8; calls = decode_program(programs[program])
        if [c[0] for c in calls] != [12]: raise ValueError(f'{inst["name"]}: program {program} is not a single flag builtin')
        words = defaults()
        for key, (kind, value) in calls[0][2].items():
            if kind == 'float': words[key] = bits(value)
            elif kind == 'int': words[key] = bits(float(value)) if key_types[key] == 2 else value & 0xFFFFFFFF
            else: raise ValueError('unexpected resource argument')
        model = inst['model_resource']
        flags.append(dict(resource=(inst['rid'] << 8) | track, rid=inst['rid'], name=inst['name'], program=program, model=model,
                          words=words, matrix=inst['matrix'], position=inst['matrix'][12:15]))
    # Cloths: one per (model, parameter block); rest grid from the lowest rid of each
    # activation group (instances of one cloth more than 200 m apart stream separately).
    wind_mode = course_wind_mode(a.location, elf)
    cloths = {}
    for f in sorted(flags, key=lambda f: f['rid']):
        key = (f['model'], tuple(f['words']))
        cloths.setdefault(key, []).append(f)
    out_cloths = []
    for index, ((model, words), members) in enumerate(cloths.items()):
        params = parameters(list(words))
        groups = []
        for f in members:
            g = next((g for g in groups if sum((f['position'][k] - g[0]['position'][k]) ** 2 for k in range(3)) < 20000 ** 2), None)
            if g is None: groups.append([f])
            else: g.append(f)
        height = 2 if params['amplitude'][2] < f32(0.01) and params['amplitude'][3] < f32(0.01) else 5
        cloth = dict(index=index, model=model, program=members[0]['program'], parameters=params, width=8, height=height,
                     groups=[dict(first=g[0]['resource'], resources=[x['resource'] for x in g], corners=corners(model >> 8, g[0]['rid'])) for g in groups])
        for f in members: f['cloth'] = index
        out_cloths.append(cloth)
    manifest = dict(version=1, coordinate_system='Original source centimeters, Z-up, row-vector instance matrices (native = (x,z,-y)/100)',
                    source='stage slot-1 programs 61..64 builtin 12 (0x2FC9C8) -> 0x34AC88; corners as 0x380380; engine/flag_cloth.hpp',
                    fps=60, wind=dict(mode=wind_mode, amplitude=from_bits({1: 0x3E19999A, 2: 0x3E99999A, 3: 0x3EE66666}[wind_mode]), initial=dict(wind=0.0, base=0.5, delta=0.25, timer=0.0)),
                    sine_table=[struct.unpack('<f', struct.pack('<I', w))[0] for w in sine_table(root)],
                    cloths=out_cloths,
                    instances=[dict(resource=f['resource'], name=f['name'], cloth=f['cloth'], matrix=f['matrix']) for f in sorted(flags, key=lambda f: f['rid'])])
    verify(manifest, a.snapshots, a.output, a.location)
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output / 'flags.json').write_text(json.dumps(manifest, separators=(',', ':')))
    print(f'{len(flags)} flag instances, {len(out_cloths)} cloths ->', a.output / 'flags.json')


def sine_table(root):
    text = (root / 'engine/flag_cloth.hpp').read_text()
    body = text[text.index('originalFlagSineTableBits{') : text.index('};', text.index('originalFlagSineTableBits{'))]
    values = [int(x, 16) for x in re.findall(r'0x([0-9A-F]{8})u', body)]
    assert len(values) == 640
    return values


def verify(manifest, pattern, output, location='ARA1'):
    paths = sorted(glob.glob(pattern), key=lambda p: int(re.search(r'tick(\d+)', p).group(1)))
    if not paths:
        print('No race snapshots: skipping PS2 verification'); return
    by_model = {}
    for c in manifest['cloths']: by_model.setdefault(c['model'], []).append(c)
    name_of = {i['resource']: i['name'] for i in manifest['instances']}
    checked, fixtures = 0, []
    for path in paths:
        mm = Mem(load_ee(path)); m = flag_manager(mm); tick = int(re.search(r'tick(\d+)', path).group(1))
        wind, base_, delta, timer = mm.fs(m + 0x10, 4)
        for s in range(15):
            c = m + 0x20 + 0x188 * s; n = mm.u(c + 0x5C)
            if not n: continue
            model = mm.u(c + 0x58); cloth = next(x for x in by_model[model] if [bits(v) for v in mm.fs(c + 4, 4)] == [bits(v) for v in x['parameters']['speed']])
            prm = cloth['parameters']
            got = dict(speed=mm.fs(c + 4, 4), amplitude=mm.fs(c + 0x14, 4), frequency=mm.fs(c + 0x24, 4), calm=mm.fs(c + 0x34, 3), gust=mm.fs(c + 0x40, 3),
                       minimumStrength=mm.f(c + 0x4C), uvSpeed=mm.fs(c + 0x50, 2))
            for k, v in got.items():
                want = prm[k]
                if [bits(x) for x in (v if isinstance(v, list) else [v])] != [bits(x) for x in (want if isinstance(want, list) else [want])]:
                    raise ValueError(f'{path}: slot {s} {k} {v} != {want}')
            if list(mm.m[c & 0x1FFFFFF:(c & 0x1FFFFFF) + 4]) != [prm['attachStart'], prm['attachEnd'], prm['pinFirstRow'], prm['drawMode']]: raise ValueError('flag bytes')
            w, h = mm.u(c + 0x64), mm.u(c + 0x68)
            if (w, h) != (cloth['width'], cloth['height']): raise ValueError('grid size')
            insts = [mm.u(c + 0x94 + 4 * k) for k in range(n)]
            grid = [mm.fs(mm.u(c + 0x74) + 12 * i, 3) for i in range(w * h)]
            # The grid corners must equal one group's first-instance corners bit for bit.
            corner_ids = [0, w - 1, (h - 1) * w, h * w - 1]
            match = [g for g in cloth['groups'] if all([bits(x) for x in grid[i]] == [bits(x) for x in g['corners']['position'][k]] for k, i in enumerate(corner_ids))]
            if not match: raise ValueError(f'{path}: slot {s} rest-grid corners match no exported group')
            mesh = mm.u(c + 0x60); scale = mm.f(mesh + 0x198); buffers = []
            for b in (mm.u(mesh + 0x1B4), mm.u(mesh + 0x1B8)):
                verts = [None] * (w * h)
                for strip in range(h - 1):
                    q = mm.u(b + 16 * strip + 4); sh = struct.unpack_from('<%dh' % (6 * w), mm.m, q & 0x1FFFFFF)
                    for col in range(w): verts[strip * w + col] = list(sh[6 * col:6 * col + 3]); verts[(strip + 1) * w + col] = list(sh[6 * col + 3:6 * col + 6])
                buffers.append(verts)
            fixtures.append(dict(snapshot=Path(path).name, tick=tick, slot=s, cloth=cloth['index'], parity=mm.u(c + 0x90),
                                 manager=dict(wind=wind, base=base_, delta=delta, timer=timer), phase=mm.fs(c + 0x78, 4), grid=grid, mesh_scale=scale, buffers=buffers,
                                 instances=[i for i in insts]))
            checked += 1
    output.mkdir(parents=True, exist_ok=True)
    __import__('disc_paths').test_data(output / 'flag-snapshots.json').write_text(json.dumps(dict(version=1, source='local/ps2-capture/runs/setpieces race snapshots' if location == 'ARA1' else str(Path(pattern).parent.relative_to(root)) + ' snapshots', cases=fixtures), separators=(',', ':')))
    print(f'Verified {checked} live cloth slots in {len(paths)} race snapshots (parameters, grid size, rest-grid corners)')


if __name__ == '__main__':
    main()
