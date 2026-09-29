#!/usr/bin/env python3
"""Export the cutscene-only world locations (instances without terrain) as browser set packages (read-only).

The transport in-air scripts (transport\\heli_inair*, gond_inair*, docs/cutscenes.md) play at the looping-script
locator set 43 = the TRANSP location of BAM.SDB: the tilt-rotor (`mdl_TRANSP_os609_full_version_inair`), its cabin
corridor and the gondola cabin, far from the mountain. It has no terrain, so tools/import_world.py refuses it; this
exporter bakes every placed instance of the location the way tools/import_sky.py bakes the sky (model nodes x
instance matrix x uniform scale, baked vertex colours, material texture per batch).

Output (git-ignored) web/public/assets/CUTSCENES/SETS/<LOC>/:
  world.json      {location, units, coordinate_basis, textures {'9-<rid>': {width, height, has_alpha, pack, id}}
                   (the world texture library TEXTURES/world.tex, tools/export_world_textures.py),
                   batches [{first_index, index_count, texture, material_flags, has_alpha, instances}], instances}
  vertices.bin    10 float32 per vertex (native Y-up metres: position, normal, uv, lighting uv), absolute world
  colors.bin      4 float32 per vertex (baked instance colour, 16/31 = unity)
  indices.bin     uint32
  python3 tools/export_cutscene_sets.py [--location TRANSP ...]
  python3 tools/export_cutscene_sets.py --plane          the new-career midway plane (SETS/ABC1PLANE, export_plane_set)
  python3 tools/export_cutscene_sets.py --helis          the backcountry heli drops (SETS/<LOC>HELI) [--out DIR]
"""
import argparse, hashlib, json, struct, sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations, texture_rgba, world_vertex_to_native, world_resource_names  # noqa: E402
from world_models import decode_model, decode_instance, transform  # noqa: E402
from import_sky import chunk_range  # noqa: E402
from export_world_textures import world_entry  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
OUT = ROOT / 'web/public/assets/CUTSCENES/SETS'


def export_set(code, source=SOURCE, out=OUT):
    locs = locations(source / 'bam.sdb')
    index, begin, end = chunk_range(locs, code)
    names = world_resource_names((source / 'bam.phm').read_bytes(), (source / 'bam.psm').read_bytes())
    textures, materials, models, instances = {}, {}, {}, []
    for i, chunk in enumerate(world_chunks(source / 'bam.ssb')):
        if i > end:
            break
        own = begin <= i <= end   # chunk 0 is shared (textures/materials) except for location 0 (TRANSP) whose chunk it is
        if not own and i != 0:
            continue
        for kind, track, rid, data in records(chunk):
            if kind == 9:
                textures.setdefault(rid, data)
            elif kind == 0:
                materials[track, rid] = struct.unpack_from('<10h', data)
            elif kind == 2 and own:
                models[track, rid] = decode_model(data)
            elif kind == 3 and own:
                instances.append((track, rid, decode_instance(data)))
    batches = defaultdict(lambda: ([], [], [], []))
    placed = []
    for track, rid, (model_id, matrix, baked, scale) in instances:
        model = models.get(model_id)
        if model is None:
            continue
        name = names[1].get((track, rid), '')
        if 'trig' in name.lower():   # authored trigger helpers are not drawn (docs/asset-formats.md)
            continue
        placed.append(dict(track=track, rid=rid, name=name, model=list(model_id), position_cm=list(matrix[12:15])))
        for mesh in model:
            material = materials.get(tuple(mesh['material']))
            if material is None:
                continue
            tex = material[0]
            if tex not in textures:
                continue
            vs, ix, colors, _ = batches[(tex, material[7])]
            base = len(vs)
            ix.extend(base + k for k in mesh['indices'])
            for k, v in enumerate(mesh['vertices']):
                pos = [a / 100 for a in transform([x * scale for x in v[:3]], matrix)]
                normal = transform(v[3:6], matrix, True)
                vs.append(world_vertex_to_native(pos + normal + v[6:]))
                c = baked[mesh['color_offset'] + k] if baked and mesh['color_offset'] + k < len(baked) else (16 / 31, 16 / 31, 16 / 31, 1.0)
                colors.append(c)
    folder = out / code
    folder.mkdir(parents=True, exist_ok=True)
    meta = dict(version=1, location=code, location_index=index, source='SSX3 USA PS2 BAM.SSB', units='meters', up_axis='Y',
                coordinate_basis='source (x,y,z) cm -> native (x,z,-y) / 100, absolute world', vertex_stride=40, color_stride=16,
                index_format='uint32', vertex_color_unity=16 / 31, textures={}, batches=[], instances=placed)
    vertex_count = index_count = 0
    with (folder / 'vertices.bin').open('wb') as vf, (folder / 'indices.bin').open('wb') as xf, (folder / 'colors.bin').open('wb') as cf:
        for (tex, flags), (vertices, indices, colors, _) in sorted(batches.items()):
            vf.write(b''.join(struct.pack('<10f', *v) for v in vertices))
            cf.write(b''.join(struct.pack('<4f', *c) for c in colors))
            xf.write(struct.pack(f'<{len(indices)}I', *(k + vertex_count for k in indices)))
            meta['batches'].append(dict(first_index=index_count, index_count=len(indices), texture=tex, material_flags=flags))
            vertex_count += len(vertices); index_count += len(indices)
    for tex in sorted({t for t, _ in batches}):
        w, h, rgba = texture_rgba(textures[tex])
        alpha = any(rgba[k] != 255 for k in range(3, len(rgba), 4))
        # A world texture (global kind-9 id): a reference into the shared world texture library, no PNG copy.
        meta['textures'][f'9-{tex}'] = world_entry(f'9-{tex}', dict(width=w, height=h, has_alpha=alpha,
                                                   source_sha256=hashlib.sha256(textures[tex]).hexdigest()), rgba)
        (folder / f'9-{tex}.png').unlink(missing_ok=True)
    for b in meta['batches']:
        b['has_alpha'] = meta['textures'][f"9-{b['texture']}"]['has_alpha']
    meta.update(vertex_count=vertex_count, index_count=index_count)
    (folder / 'world.json').write_text(json.dumps(meta, separators=(',', ':')))
    return meta


# The new-career drop's midway plane (docs/cutscenes.md #15, docs/presentation.md "New-career plane"): #153
# abc1_heli_arr_midway and #154-163 heli_arrb_<char>_midwayabc1 are anchored on anchor 37 = ABC1 locator 15, the instance
# mdl_ABC1_os609_full_version_inair2 (ABC1 track 6 rid 1803, model rid 55, 6 nodes: fuselage, nacelles, rotors, ramp).
# The world draws it static at its LiveComp frame 0, 252 m out; the scripts' kind-7 channel-0 calls (0x2808E8: the hash in
# the current course's location globals, 0x309E50) run ABC1 global program 5: 0x0DE99225 (#153 t0) = SetNodeState 1 +
# builtin 3 frames 0..185 once at 30 fps + sound 201 loop; 0x0EE53794 (#163 t0) = frames 186..550; cleanup 0x07D196B4
# (0x2807B0 at the step end) = SetNodeState 1 (the animation removed: back to the static pose), sound 201 stop. The
# spray (program 2: 0x0D9F751E / 0x0D905E74, builtin 25/69 on ospreySpray_1001) is not exported.
PLANE = dict(code='ABC1', rid=1803, set='ABC1PLANE', program=5, cleanup=0x07D196B4, sound=201, anchor=37,
             starts=[(0x0DE99225, 0, 185), (0x0EE53794, 186, 550)])
# The backcountry heli drops (docs/presentation.md 16): #123 / #124 / #127 `<bc>_heli_arr` and #128-137 `heli_arrb_<char>` are
# anchored on anchor 29 = the location's locator 5, the instance mdl_<LOC>_os609_full_version_inair (ABC1 rid 284, DBC2 rid 2266,
# EBC3 rid 715; LiveComp draw 'none', the world package bakes it static at frame 0). Their kind-7 calls, run in each location's
# stage globals by the core (stage_global_call, LiveComp log + audio events): 0x0E995385 (#12x t0) = stop + loop sound 201 on the
# heli + builtin 3 frames 0..185 once at 30 fps; 0x0DD5F634 (#13x t0) = the same with frames 186..550; their cleanup 0x0AE69AB4
# (0x2807B0 at the step end) = the same with frames 551..677 in mode 1 (loop: the heli hovers on after the drop). 0x03E0EA1E
# (cleanup 0x03EFC174) is the snow spray (no LiveComp start).
HELIS = [dict(code=code, rid=rid, set=f'{code}HELI', program=None, cleanup=0x0AE69AB4, sound=201, anchor=29,
              starts=[(0x0E995385, 0, 185), (0x0DD5F634, 186, 550)], cleanup_start=(551, 677, 1))
         for code, rid in (('ABC1', 284), ('DBC2', 2266), ('EBC3', 715))]
# The heli is a lit instance (runtime flags 0x4000 | 0x1000 in PS2 RAM: DBC2 / EBC3 0x50015105, ABC1 0x50015003; the plane
# 0x10305 is not): the static-model draw 37E238 calls 2F5148 -> 2F5400, a per-instance light cache: the environment bank is the
# Lighting painter's reference 3 (the object bank xOBR1: painter +0x38, getter vtable+0x140 2C15C8; empty -> reference 0; PS2
# RAM DBC2 index 19 = DOBR1, EBC3 27 = EOBR1) of a private wrapper stepped at the instance X/Y, refreshed after 5 m of travel,
# plus up to 4 local lights from a +-10 m query (2F5AF0; none within kilometres of the three helipads). 37E098 uploads the
# ten rows; VU1 program 3 (0x2170) scales them by 128 (w = 128 on the constant row only) and 0x8B8 evaluates per vertex
# clamp(sum rows x (1, x^2, y^2, z^2, xy, zx, yz, x, y, z), 0, 255) (FTOI0) on the ITOF15 normal through the node's rows
# 25..27, which replaces the baked vertex colour (TFX MODULATE: Cs = T x L >> 7, As = Ta).
def lighting_bank(code, source=SOURCE):
    """The object bank (Lighting painter type 11, payload reference 3) of a location with a single Lighting payload."""
    from import_sky import painter_sections
    track = [l['name'] for l in locations(source / 'bam.sdb')].index(code); found = []
    for chunk in world_chunks(source / 'bam.ssb'):
        for kind, t, rid, data in records(chunk):
            if kind == 15 and t == track and len(data) >= 64: found.append(data)
    if len(found) != 1: raise ValueError(f'{code}: {len(found)} world painter records')
    section = painter_sections(found[0])[11]
    header, count, _ = struct.unpack_from('<3I', section)
    if count != 1: raise ValueError(f'{code}: {count} Lighting payloads (the heli bank would depend on its position)')
    at = struct.unpack_from('<2I', section, 12)[1]
    name = section[at + 4 + 3 * 8:at + 12 + 3 * 8].rstrip(b'\0').decode('ascii')
    irr = json.loads((ROOT / 'local/assets/native/IRRADIANCE/irradiance.json').read_text())
    return dict(bank=name, index=irr['records'][name]['index'], rows=irr['records'][name]['rows'], scale=128, source='2F5400 mode 1: Lighting reference 3')


def export_plane_set(source=SOURCE, out=OUT, spec=PLANE):
    """CUTSCENES/SETS/<spec set>: a LiveComp plane / heli instance split per LiveComp node, with its LiveComp record
    (web/livecomp-animation.js instance shape) and the kind-7 starts, for web/cutscene-stage-sets.js. spec: PLANE or HELIS[k]."""
    PLANE = spec
    from export_rail_teeters import parse_model
    from export_livecomp import mesh_nodes
    fb = lambda b: struct.unpack('<f', struct.pack('<I', b))[0]
    bits = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
    code, rid = PLANE['code'], PLANE['rid']
    locs = locations(source / 'bam.sdb')
    index, begin, end = chunk_range(locs, code)
    names = world_resource_names((source / 'bam.phm').read_bytes(), (source / 'bam.psm').read_bytes())
    textures, materials, raw_models, inst = {}, {}, {}, None
    for i, chunk in enumerate(world_chunks(source / 'bam.ssb')):
        if i > end:
            break
        own = begin <= i <= end
        if not own and i != 0:
            continue
        for kind, track, r, data in records(chunk):
            if kind == 9:
                textures.setdefault(r, data)
            elif kind == 0:
                materials[track, r] = struct.unpack_from('<10h', data)
            elif kind == 2 and own:
                raw_models[track, r] = data
            elif kind == 3 and own and track == index and r == rid:
                inst = decode_instance(data)
    if inst is None:
        raise ValueError(f'{code} rid {rid}: no plane instance')
    model_id, matrix, baked, scale = inst
    data = raw_models[model_id]; model = decode_model(data); owner = mesh_nodes(data); pm = parse_model(data)
    name = names[1].get((index, rid), '')
    resource = rid << 8 | index
    words = lambda lo, hi, mode=0: [resource, mode, 0, bits(float(lo)), bits(float(hi)), bits(30.0), bits(0.0), bits(-1.0), 0, 0, 0]
    live = dict(resource=resource, name=name, model=(model_id[1] << 8) | model_id[0], length=fb(pm['length']), matrix=list(matrix), scale=scale,
                nodes=[dict(parent=n['parent'], bind=[fb(x) for x in n['bind']],
                            track=None if not n['track'] else dict(base=[fb(x) for x in n['track']['base']], mask=n['track']['mask'],
                                                                    curves=[[[fb(x) for x in seg] for seg in c] for c in n['track']['curves']]))
                       for n in pm['nodes']],
                meshNodes=owner,
                starts=[dict(trigger='stage', symbol=sym, cleanup=PLANE['cleanup'], program=PLANE['program'], words=words(lo, hi)) for sym, lo, hi in PLANE['starts']]
                + ([dict(trigger='cleanup', symbol=PLANE['cleanup'], cleanup=None, program=PLANE['program'], words=words(*PLANE['cleanup_start']))] if PLANE.get('cleanup_start') else []))
    batches = defaultdict(lambda: ([], [], []))
    for m, mesh in enumerate(model):
        material = materials.get(tuple(mesh['material']))
        if material is None or material[0] not in textures:
            continue
        vs, ix, colors = batches[(material[0], material[7], owner[m])]
        base = len(vs); ix.extend(base + k for k in mesh['indices'])
        for k, v in enumerate(mesh['vertices']):
            pos = [a / 100 for a in transform([x * scale for x in v[:3]], matrix)]
            vs.append(world_vertex_to_native(pos + transform(v[3:6], matrix, True) + v[6:]))
            colors.append(baked[mesh['color_offset'] + k] if baked and mesh['color_offset'] + k < len(baked) else (16 / 31, 16 / 31, 16 / 31, 1.0))
    folder = out / PLANE['set']
    folder.mkdir(parents=True, exist_ok=True)
    meta = dict(version=1, location=code, location_index=index, source='SSX3 USA PS2 BAM.SSB', units='meters', up_axis='Y',
                coordinate_basis='source (x,y,z) cm -> native (x,z,-y) / 100, absolute world (bind pose = LiveComp frame 0)',
                vertex_stride=40, color_stride=16, index_format='uint32', vertex_color_unity=16 / 31, textures={}, batches=[],
                instances=[dict(track=index, rid=rid, name=name, model=list(model_id), position_cm=list(matrix[12:15]))],
                anchor=PLANE['anchor'], sound_loop=PLANE['sound'], livecomp=dict(version=1, fps=60, instances=[live]))
    if PLANE.get('cleanup_start'): meta['lighting'] = lighting_bank(code, source)
    vc = ic = 0; lo, hi = [1e9] * 3, [-1e9] * 3
    with (folder / 'vertices.bin').open('wb') as vf, (folder / 'indices.bin').open('wb') as xf, (folder / 'colors.bin').open('wb') as cf:
        for (tex, flags, node), (vertices, indices, colors) in sorted(batches.items()):
            vf.write(b''.join(struct.pack('<10f', *v) for v in vertices))
            cf.write(b''.join(struct.pack('<4f', *c) for c in colors))
            xf.write(struct.pack(f'<{len(indices)}I', *(k + vc for k in indices)))
            for v in vertices:
                for a in range(3): lo[a] = min(lo[a], v[a]); hi[a] = max(hi[a], v[a])
            meta['batches'].append(dict(first_index=ic, index_count=len(indices), texture=tex, material_flags=flags, livecomp_resource=resource, livecomp_node=node))
            vc += len(vertices); ic += len(indices)
    for tex in sorted({t for t, _, _ in batches}):
        w, h, rgba = texture_rgba(textures[tex])
        alpha = any(rgba[k] != 255 for k in range(3, len(rgba), 4))
        meta['textures'][f'9-{tex}'] = world_entry(f'9-{tex}', dict(width=w, height=h, has_alpha=alpha, source_sha256=hashlib.sha256(textures[tex]).hexdigest()), rgba)
    for b in meta['batches']:
        b['has_alpha'] = meta['textures'][f"9-{b['texture']}"]['has_alpha']
    # The world's own static copy (the same instance baked at frame 0 in the ABC1 / PEAK1 packages): hidden while the set plays.
    centre = [(lo[a] + hi[a]) / 2 for a in range(3)]
    meta.update(vertex_count=vc, index_count=ic, bounds_native=dict(min=lo, max=hi), world_copy=dict(centre=centre, radius=max(hi[a] - lo[a] for a in range(3)) / 2 + 2))
    (folder / 'world.json').write_text(json.dumps(meta, separators=(',', ':')))
    return meta


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--location', action='append', help='location code (default TRANSP)')
    ap.add_argument('--plane', action='store_true', help='only the new-career midway plane set (SETS/ABC1PLANE)')
    ap.add_argument('--helis', action='store_true', help='only the backcountry heli sets (SETS/ABC1HELI, DBC2HELI, EBC3HELI)')
    ap.add_argument('--out', type=Path, default=OUT, help='output SETS directory (default: web/public/assets/CUTSCENES/SETS)')
    a = ap.parse_args()
    if a.plane or a.helis:
        for spec in ([PLANE] if a.plane else []) + (HELIS if a.helis else []):
            m = export_plane_set(out=a.out, spec=spec)
            print(f"{spec['set']}: {m['vertex_count']} vertices, {len(m['batches'])} batches, {len(m['textures'])} textures, world copy {m['world_copy']}")
        return
    for code in a.location or ['TRANSP']:
        m = export_set(code)
        print(f"{code}: {len(m['instances'])} instances, {m['vertex_count']} vertices, {len(m['batches'])} batches, {len(m['textures'])} textures")
        for i in m['instances']:
            print('  ', i['name'], [round(x) for x in i['position_cm']])


if __name__ == '__main__':
    main()
