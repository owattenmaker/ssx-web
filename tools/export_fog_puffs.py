#!/usr/bin/env python3
"""Fog-particle puffs of the world packages (docs/visual-parity.md 41, web/fog-puffs.js).

SSB kind-5 records are fog-particle instances; each one points to a kind-4 record that holds the puffs.
The PS2 draws them through cPS2FogParticleMan (vtable 0x4882E0, built in 0x22F6B0):
* load: resolver 0x3AB200 -> 0x3AA700 (+0x08 = 7, +0x64 = the kind-4 record, the octree's "other" list);
* cull 0x22A270 (VU0 0xDB8): the +0x68/+0x74 box against the view frustum (all 8 corners outside one plane:
  dropped), the +0x50 sphere against the node's occluder volumes; inside -> list 1, crossing the guard band -> list 2;
* 0x22C708 -> 0x2DC190 (list 1 mode 0, list 2 mode 2) -> 0x2DBF98 (sort far to near, material, draw 0x2DC7B0).

Record layouts (payload offsets, little endian):
* kind 5 (144 bytes): +0x10 4x4 matrix (4 qwords, qword 3 = translation), +0x50 sphere centre / radius,
  +0x60 (unused here), +0x64 kind-4 reference (track | rid << 8), +0x68 box min, +0x74 box max;
* kind 4: +0x04 entry count, +0x08 entry offset; entry (16 bytes) +0x04 puff-set offset; puff set +0x1C count,
  +0x20 puff offset (from the set); puff (28 bytes) = local x, y, z (cm), r, g, b (0..1), size (cm, half extent).
  0x2DC190 reads entry 0 for every entry (t8 is not advanced): `repeat` = the entry count.

Output (one file per package, next to its world.json): <out>/<package>/fog-puffs.json
{version, source, instances: [{track, rid, chunk, matrix[16], box_min[3], box_max[3], sphere[4], repeat, puffs[[x,y,z,r,g,b,size]]}]}
Floats are the records' float32 values. Default output is a scratch folder (never web/public/assets).

--check-state LOC:STATE compares the parse with a PS2 savestate: the loaded kind-5 instances (matrix, box, puffs through
the resolved +0x64 pointer) and the last frame's sprite buffer (manager +16 + 48 i, sorted list 0x537AC0) against the
0x2DC190 model with the frame's camera (depth, alpha, colour, sprite half extent / depth).
"""
import argparse, json, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import records, world_chunks, locations  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
ASSETS = ROOT / 'web/public/assets'
f32 = lambda b, o, n=1: list(struct.unpack_from('<%df' % n, b, o))


def parse_puffs(d4):
    cnt, off = struct.unpack_from('<2I', d4, 4)
    if cnt == 0: return 0, []
    ps = struct.unpack_from('<I', d4, off + 4)[0]
    if ps == 0: return cnt, []
    count, rel = struct.unpack_from('<2I', d4, ps + 28)
    return cnt, [f32(d4, ps + rel + 28 * k, 7) for k in range(count)]


def package_instances(chunks, sources):
    """kind-5 instances of the package's chunk ranges (world.json source / event_locations), kind 4 from the same ranges."""
    k4, k5 = {}, []
    for src in sources:
        lo, hi = src['chunks']
        for ci in range(lo, hi + 1):
            for kind, track, rid, d in records(chunks[ci]):
                if kind == 4: k4[(track, rid)] = d
                elif kind == 5: k5.append((ci, track, rid, d))
    out = []
    for ci, track, rid, d in k5:
        if len(d) != 144: raise ValueError(f'kind-5 {track}:{rid} size {len(d)}')
        ref = struct.unpack_from('<I', d, 0x64)[0]
        key = (ref & 0xFF, ref >> 8)
        if key not in k4: raise ValueError(f'kind-5 {track}:{rid} -> kind-4 {key} not in the package')
        repeat, puffs = parse_puffs(k4[key])
        out.append(dict(track=track, rid=rid, chunk=ci, matrix=f32(d, 0x10, 16), box_min=f32(d, 0x68, 3), box_max=f32(d, 0x74, 3),
                        sphere=f32(d, 0x50, 4), repeat=repeat, puffs=puffs))
    return out


def packages():
    for world in sorted(ASSETS.glob('*/world.json')) + sorted(ASSETS.glob('PEAK*/*/world.json')):
        doc = json.loads(world.read_text())
        sources = doc.get('event_locations') if world.parent.parent == ASSETS else None
        if sources is None:
            src = doc.get('source')
            sources = src if isinstance(src, list) else None
        if not sources:
            m = doc.get('event_locations') or []
            sources = m
        if not sources: continue
        yield world.parent.relative_to(ASSETS), sources


def export(out_root):
    chunks = list(world_chunks(SOURCE / 'bam.ssb'))
    total = 0
    for rel, sources in packages():
        inst = package_instances(chunks, sources)
        if not inst: continue
        doc = dict(version=1, source='SSX3 USA PS2 BAM.SSB kind 5 / kind 4 (tools/export_fog_puffs.py)', instances=inst)
        p = Path(out_root) / rel / 'fog-puffs.json'; p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(doc, separators=(',', ':')) + '\n')
        n = sum(len(i['puffs']) * max(1, i['repeat']) for i in inst); total += n
        print(f'{rel}: {len(inst)} instances, {n} puffs -> {p}')
    print('puffs', total)


# ---- PS2 check ----
GP = 0x4A30F0
def ps2_frame(ee):
    """The last drawn frame's sprites: (index, depth) pairs at 0x537AC0 (the count is cleared after the draw, so the frame's
    count is the longest prefix whose indices are 0..n-1, depths descending) and the manager's sprite words."""
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    fl = lambda a: struct.unpack_from('<f', ee, a & 0x1FFFFFF)[0]
    arr = memoryview(ee).cast('I')
    # the live manager: stale copies of the vtable word lie around; its sprite words are RGBA 0..128
    cands = [i * 4 - 4 for i in range(0x100000 // 4, len(arr)) if arr[i] == 0x4882E0]
    ok = lambda m: sum(all(0 <= fl(m + 16 + 48 * k + 32 + 4 * j) <= 128.5 for j in range(4)) for k in range(8))
    mgr = max(cands, key=ok)
    ps = [(u(0x537AC0 + 8 * k), u(0x537AC0 + 8 * k + 4)) for k in range(512)]
    m = 0
    while m < len(ps) and ps[m][0] < 512 and (m == 0 or ps[m][1] <= ps[m - 1][1]): m += 1
    n = 0
    for k in range(m, 0, -1):
        if sorted(i for i, _ in ps[:k]) == list(range(k)): n = k; break
    out = []
    for i, d in ps[:n]:
        v = [fl(mgr + 16 + 48 * i + 4 * j) for j in range(12)]
        out.append(dict(depth=d, a=v[0:4], b=v[4:8], argb=v[8:12]))
    return out


def model_frame(inst, V, A, B):
    """0x22A270 + 0x2DC190 in doubles: V = world -> view (renderer +22400, z row = the view depth), A / B = the frustum and
    guard-band clip matrices (VU0 64..67 / 68..71). The GS transform is A's: x = 32768 + 4096 xA / wA, y = 32768 - 3584 yA / wA."""
    mul = lambda M, p: [M[j] * p[0] + M[4 + j] * p[1] + M[8 + j] * p[2] + M[12 + j] * p[3] for j in range(4)]
    def flags(c):
        f = 0
        for j, (hi, lo) in enumerate(((1, 2), (4, 8), (16, 32))):   # VU CLIP: against |w|
            if c[j] > abs(c[3]): f |= hi
            if c[j] < -abs(c[3]): f |= lo
        return f
    px = (A[0] ** 2 + A[4] ** 2 + A[8] ** 2) ** 0.5 / (V[0] ** 2 + V[4] ** 2 + V[8] ** 2) ** 0.5   # P00 (NDC) of the frame
    py = (A[1] ** 2 + A[5] ** 2 + A[9] ** 2) ** 0.5 / (V[1] ** 2 + V[5] ** 2 + V[9] ** 2) ** 0.5
    out, results = [], []
    for j, i in enumerate(inst):
        lo, hi = i['box_min'], i['box_max']
        corners = [[x, y, z, 1] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
        fa = [flags(mul(A, c)) for c in corners]; any_ = all_ = 0; all_ = 63
        for f in fa: any_ |= f; all_ &= f
        res = 1 if all_ else 0 if not any_ else (3 if not any(flags(mul(B, c)) for c in corners) else 4)
        results.append(res)
        if res in (1, 2): continue
        m = i['matrix']; c = [(a + b) * 0.5 for a, b in zip(lo, hi)]
        wc = int(mul(V, c + [1])[2])
        if 25000.0 <= wc: continue
        f10 = 1.0 if wc < 18000.0 else (25000.0 - wc) * 0.0001428571413271129
        for _ in range(max(1, i['repeat'])):
            for q in i['puffs']:
                w_ = mul(m, q[:3] + [1]); xv, yv, w, _1 = mul(V, w_)
                d = int(w)
                if d <= 500: continue
                f0 = (d - 500.0) * 0.0005000831442885101 if d < 2499.66748046875 else 1.0
                f7 = f10 * f0
                if f7 == 0: continue
                if res == 4:   # mode 2: both corners off the same side of the 512 x 448 screen box
                    X, Y = 32768 + 4096 * px * xv / w, 32768 - 3584 * py * yv / w
                    sx, sy = 4096 * px * q[6] / w, -3584 * py * q[6] / w
                    def sf(x, y): return (2 if x < 28672 else 1 if x > 36864 else 0) | (8 if y < 29184 else 4 if y > 36352 else 0)
                    if sf(X - sx, Y - sy) & sf(X + sx, Y + sy): continue
                out.append(dict(depth=d, alpha=int(f7 * 128), rgb=[int(v * 128) for v in q[3:6]], instance=j))
    out.sort(key=lambda s: -s['depth'])
    return out, results


def check_state(code, state, fixture=None):
    with zipfile.ZipFile(state) as z: ee = z.read('eeMemory.bin'); vu0 = z.read('vu0Memory.bin')
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    fl = lambda a: struct.unpack_from('<f', ee, a & 0x1FFFFFF)[0]
    chunks = list(world_chunks(SOURCE / 'bam.ssb'))
    doc = json.loads((ASSETS / code / 'world.json').read_text())
    inst = package_instances(chunks, doc['event_locations'])
    # 1. the loaded records: matrix, box, sphere and the puffs through the resolved +0x64
    found = 0
    for i in inst:
        at = ee.find(struct.pack('<16f', *i['matrix']) + struct.pack('<4f', *i['sphere']))
        if at < 0: continue
        rec = at - 0x10
        if struct.pack('<6f', *i['box_min'], *i['box_max']) != ee[rec + 0x68:rec + 0x80]: raise SystemExit(f'{i["rid"]}: box differs in RAM')
        k4 = u(rec + 0x64); ps = u(u(k4 + 8) + 4); n = u(ps + 28); p = u(ps + 32)
        if u(k4 + 4) != i['repeat'] or [list(struct.unpack_from('<7f', ee, (p + 28 * k) & 0x1FFFFFF)) for k in range(n)] != i['puffs']: raise SystemExit(f'{i["rid"]}: puffs differ in RAM')
        found += 1
    # 2. the last frame's sprites against the model with the state's camera
    r = u(GP + 10896); V = [fl(r + 22400 + 4 * k) for k in range(16)]
    A = list(struct.unpack_from('<16f', vu0, 64 * 16)); B = list(struct.unpack_from('<16f', vu0, 68 * 16))
    ps2 = ps2_frame(ee); model, results = model_frame(inst, V, A, B)
    # a frame with no sprite leaves the older list in place: the list is this frame's only if its nearest entry is a puff at the
    # state's camera (depth within 1.5 cm)
    mul = lambda M, p: [M[j] * p[0] + M[4 + j] * p[1] + M[8 + j] * p[2] + M[12 + j] * p[3] for j in range(4)]
    depths = [mul(V, mul(i['matrix'], q[:3] + [1]))[2] for i in inst for q in i['puffs']]
    # 0x2DC190 writes a sprite's (index, depth) pair and its corners before the mode-2 test: when the frame's last sprite is dropped
    # there, it stays behind at position n with index n, its corners wholly off one side of the screen box (colour words stale)
    side = lambda x, y: (2 if x < 28672 else 1 if x > 36864 else 0) | (8 if y < 29184 else 4 if y > 36352 else 0)
    if len(ps2) == len(model) + 1 and side(*ps2[-1]['a'][:2]) & side(*ps2[-1]['b'][:2]): ps2.pop()
    if ps2 and not any(abs(d - ps2[-1]['depth']) <= 1.5 for d in depths): ps2 = []
    got = [(s['depth'], int(s['argb'][0]), int(s['argb'][1]), int(s['argb'][2]), int(s['argb'][3])) for s in ps2]
    want = [(s['depth'], s['alpha'], *s['rgb']) for s in model]
    same = sum(1 for a, b in zip(got, want) if a == b)
    near = sum(1 for a, b in zip(got, want) if a != b and abs(a[0] - b[0]) <= 1 and a[1:] == b[1:])
    print(f'{Path(state).name}: {found}/{len(inst)} instances in RAM; last frame {len(got)} sprites, model {len(want)}: {same} identical in order, {near} with the depth 1 off (EE rounding); instance results {dict((k, results.count(k)) for k in set(results))}')
    if fixture is not None:
        fixture.append(dict(state=str(Path(state).resolve().relative_to(ROOT)) if Path(state).resolve().is_relative_to(ROOT) else str(state), location=code,
                            view=V, frustum=A, guard=B, sprites=got, puffs=dict(version=1, instances=inst)))
    return len(got), same, near


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=None, help='output root (a scratch folder; never web/public/assets)')
    ap.add_argument('--check-state', default=None, help='LOC:state.p2s[,LOC:state.p2s...]')
    ap.add_argument('--fixture', default=None, help='with --check-state: write the cases for web/test-fog-puffs.mjs here')
    a = ap.parse_args()
    if a.check_state:
        cases = [] if a.fixture else None
        for item in a.check_state.split(','):
            code, state = item.split(':', 1); check_state(code, state, cases)
        if a.fixture:
            Path(a.fixture).parent.mkdir(parents=True, exist_ok=True)
            Path(a.fixture).write_text(json.dumps(dict(source='tools/export_fog_puffs.py --check-state', cases=cases), separators=(',', ':')) + '\n')
    else:
        if not a.out: raise SystemExit('--out is required (a scratch folder)')
        if Path(a.out).resolve().is_relative_to(ASSETS.resolve()): raise SystemExit('refusing to write into web/public/assets')
        export(a.out)
