#!/usr/bin/env python3
"""CrowdMan2d (stage builtin88 0x303BA0, object *(gp-0x6F0), update 0x2294C8, draw 0x229BA8) data for the browser
(development export; git-ignored outputs):

  web/public/assets/FX/crowd/anNN.png   the 16 crowd frames of DATA/TEXTURES/CRWD.SSH (an00..an15, table 0x445D3C),
                                        in the world-texture PNG convention (web/prepare.py: RGBA, GS alpha doubled)
  web/public/assets/<LOC>/CROWD/crowd.json

Mode 0 (crowd2d_* part 0 groups 0..4, crowdpod_module_* part 1 groups 0..2; 0x229820): the instance's material records
of those groups' material slots are replaced by the shared record 0x536690 whose texture is advanced every 3 game ticks
through an00..an15 (0x2DBAC0, loop of 16). On the three courses those slots are exactly the materials of world texture
9-161 (no other instance uses it), so crowd.json lists the texture keys the browser animates.
Mode 2 (cameraflash_*, crowdpodflash_*; 0x229788 -> 0x229910): a camera-flash area from the first 4 vertices p0..p3 of
the model (part 0), in world space (instance matrix): centre = p0 + (p3 - p0) / 2 + (0, 0, 150), axis1 = (p2 - p0) * 0.45,
axis2 = (p1 - p0) * 0.45 (0x49DF00). Flashes: see web/crowd-2d.js.

usage: export_crowd.py [--locations ARA1 BRA2 BHP1]
"""
import argparse, hashlib, json, struct, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc, EXPECTED_SHA1  # noqa: E402
from export_sun_flare import member  # noqa: E402
from world_assets import world_chunks, records, locations, event_locations  # noqa: E402
from world_models import decode_model, Reader  # noqa: E402

SEED = ROOT / 'web/generated/stage_scripts_seed.hpp'


def png(w, h, rgba):
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


def decode_shps(container, name):
    """SHPS member: 4-bit (1) or 8-bit (2) indexed with a CLUT32 record (33), or 32-bit (5); linear texels."""
    offset, data = member(container, name)
    fmt, size = data[0], int.from_bytes(data[1:4], 'little'); w, h = struct.unpack_from('<HH', data, 4)
    if fmt == 5: return w, h, bytes(data[16:16 + w * h * 4])
    if fmt not in (1, 2) or data[size] != 33: raise ValueError(f'{name}: unsupported SHPS record {fmt}')
    palette_size = int.from_bytes(data[size + 1:size + 4], 'little'); pal = data[size + 16:size + palette_size]
    out = bytearray()
    for i in range(w * h):
        if fmt == 1: index = (data[16 + i // 2] >> (4 * (i & 1))) & 15
        else: index = data[16 + i]; index = (index & 0xe7) | ((index & 8) << 1) | ((index & 16) >> 1)
        out.extend(pal[index * 4:index * 4 + 4])
    return w, h, bytes(out)


def group_texture(model, materials, node, group):
    """0x229820: node `part` (model+8 table), its mesh header +0x1C group count / +0x20 groups; group record +0 = the
    model material index -> material object -> texture id (material record word 0, low 16 bits)."""
    r = Reader(model); node_count, node_offset = r.u32(4), r.u32(8); material_count = r.u32(40)
    if node >= node_count: return None
    mesh_header = r.read('4I', node_offset + node * 16)[1]
    if mesh_header in (0, 0xffffffff): return None
    group_count, groups_at = r.read('2I', mesh_header + 28)
    if group >= group_count: return None
    slot = r.read('2H', r.u32(groups_at + group * 4))[0]
    if slot >= material_count: return None
    mat = materials.get(tuple(r.object_id(44 + slot * 4)))
    return struct.unpack_from('<I', mat, 0)[0] & 0xFFFF if mat else None


def stage_calls(code):
    """builtin88 calls per handler owner: [(owner resource, slot, mode, part, group)] from the decoded stage programs."""
    sys.path.insert(0, str(ROOT / 'tools'))
    from export_stage_world import seed, calls_of
    stages, programs, words, globals_, handlers = seed(f'browser_stage_{code.lower()}')
    stage_of = {s[0]: s for s in stages}; out = []
    for res, slots in handlers:
        st = stage_of.get(res & 255)
        if not st: continue
        for slot, prog in enumerate(slots):
            if prog < 0: continue
            first, count = programs[st[1] + prog]
            for builtin, keys, current in calls_of(words, first, count, res):
                if builtin != 88: continue
                target = keys.get(0, (1, 0xFFFFFFFF))[1]
                out.append(dict(resource=current if target in (None, 0xFFFFFFFF) else target, slot=slot, mode=keys.get(1, (1, 0))[1],
                                part=keys.get(2, (1, 0))[1], group=keys.get(3, (1, 0))[1]))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--locations', nargs='+', default=['ARA1', 'BRA2', 'BHP1'])
    from disc_paths import ps2_iso;p.add_argument('--ps2-iso', type=Path, default=ps2_iso()); a = p.parse_args()
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest() != EXPECTED_SHA1: raise ValueError('Unexpected original executable')
    disc = Disc(a.ps2_iso)
    try: crwd = disc.file('DATA/TEXTURES/CRWD.SSH')
    finally: disc.close()
    fx = ROOT / 'web/public/assets/FX/crowd'; fx.mkdir(parents=True, exist_ok=True); frames = []
    for i in range(16):
        w, h, rgba = decode_shps(crwd, 'an%02d' % i)
        if max(rgba[3::4]) <= 128: rgba = bytes(v if k % 4 != 3 else min(255, v * 2) for k, v in enumerate(rgba))  # world PNG convention
        (fx / ('an%02d.png' % i)).write_bytes(png(w, h, rgba)); frames.append(dict(file='an%02d.png' % i, width=w, height=h))
    ps2 = ROOT / 'local/assets/source/ps2'; locs = locations(ps2 / 'bam.sdb')
    for code in a.locations:
        resident = [(b, e) for _, _, b, e in event_locations(locs, code)]; models, materials = {}, {}
        for index, chunk in enumerate(world_chunks(ps2 / 'bam.ssb')):
            if index > max(e for _, e in resident): break
            if index != 0 and not any(b <= index <= e for b, e in resident): continue
            for kind, track, rid, data in records(chunk):
                if kind == 2: models[track, rid] = data
                elif kind == 0: materials[track, rid] = data
        world = json.loads((ROOT / f'web/public/assets/{code}/world_collision.json').read_text())
        by_res = {(i['rid'] << 8) | i['track']: i for i in world['instances']}
        native = json.loads((ROOT / f'local/assets/native/{code}/world.json').read_text())
        calls = stage_calls(code); textures = set(); flashes = []; crowds = []
        for c in calls:
            inst = by_res.get(c['resource'])
            if not inst: continue
            mr = inst['model_resource']; key = (mr & 255, mr >> 8)
            meshes = decode_model(models[key])
            if c['mode'] == 0:
                t = group_texture(models[key], materials, c['part'], c['group'])
                crowds.append(dict(resource=c['resource'], name=inst['name'], slot=c['slot'], part=c['part'], group=c['group'], texture=t))
                if c['slot'] == 1 and t is not None: textures.add(t)
            elif c['mode'] == 2 and c['slot'] == 1:
                m = inst['matrix']; v = meshes[0]['vertices'][:4]
                P = [[v[k][0] * m[0] + v[k][1] * m[4] + v[k][2] * m[8] + m[12], v[k][0] * m[1] + v[k][1] * m[5] + v[k][2] * m[9] + m[13],
                      v[k][0] * m[2] + v[k][1] * m[6] + v[k][2] * m[10] + m[14]] for k in range(4)]
                centre = [P[0][k] + 0.5 * (P[3][k] - P[0][k]) + (150 if k == 2 else 0) for k in range(3)]
                flashes.append(dict(resource=c['resource'], name=inst['name'], centre=centre, axis1=[(P[2][k] - P[0][k]) * 0.45 for k in range(3)],
                                    axis2=[(P[1][k] - P[0][k]) * 0.45 for k in range(3)]))
        # The replaced slots' textures must be used by crowd instances only (then animating the world texture is the
        # per-instance record swap).
        import bisect
        b = sorted(native['batches'], key=lambda x: x['first_index']); starts = [x['first_index'] // 3 for x in b]; users = {}
        for s in native['collision_sources']:
            if s['kind'] != 'instance': continue
            users.setdefault(b[bisect.bisect_right(starts, s['first_triangle']) - 1]['texture'], set()).add((s['rid'] << 8) | s['track'])
        crowd_res = {c['resource'] for c in crowds}; shared = {}
        for t in textures:
            if not users.get(t, set()) <= crowd_res:
                # Style Mile (docs/peak2.md): mdl_DSS2_eb_cp_sli_3 shares 9-161 without a builtin88 call; the PS2 swaps the
                # crowd instances' records only, so that piece keeps the static frame there (listed; the browser's texture
                # swap animates it too, a documented gap). Peak 1 courses have no such user.
                shared[f'9-{t}'] = sorted(users[t] - crowd_res)
                print(f'WARNING {code}: crowd texture 9-{t} also used by {shared[f"9-{t}"][:4]}')
        textures = sorted(textures)
        out = dict(version=1, location=code, frames=frames, frame_ticks=3, textures=[f'9-{t}' for t in textures],
                   crowds=crowds, flashes=flashes, **(dict(shared_texture_users=shared) if shared else {}), flash=dict(texture='flsh', half_size_cm=100.0, life_ticks=3, ring=40, slots=64,
                   countdown='300 + r % 300 (visual RNG), -= 10*|cheer| + 4 per tick', blend='GS 0x48 additive, depth tested, no Z write, priority 4'))
        target = ROOT / f'web/public/assets/{code}/CROWD'; target.mkdir(parents=True, exist_ok=True)
        (target / 'crowd.json').write_text(json.dumps(out, indent=1) + '\n')
        print(json.dumps(dict(location=code, textures=out['textures'], crowds=len(crowds), flashes=len(flashes))))


if __name__ == '__main__':
    main()
