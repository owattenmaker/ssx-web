#!/usr/bin/env python3
"""The original front-end rider preview (Select Character, Setup Character, Rider Details) as browser packages.

The FE preview is its own model, not the race rider (docs/characters.md "Front-end preview"): the preview
manager *(*(gp-0x848)+0x7C) keeps two slots at +0xB0 + 0xCE0*player (getter 0x1A0548); slot+0 is the character
id, slot+8 the geometry (parts +0x0C, count +8, 0x58 stride; per part +0 file id, +4 first bone slot, +0x18
active, +0x1C LOD0 variant table, +0x50 variant index, +0x38 bones, +0x44 bone count; bind bank +0x38). Its
assembly is the rider's outfit with the cinematic (NIS) head, eyes and hands: e.g. Kaori kaori_TopA,
board_BindingsA, board_BoardFlexA, kaori_BottomA, kaori_HeadA_NIS, kaori_Eyes_NIS, kaori_HandsA_NIS (left hand),
kaori_DummyA_NIS (right hand), kaori_BootsA, kaori_PigtailsA. The NIS head and hands carry morph targets whose
weights are the FE clips' channels of their part file (library.json fe streams '5', '8', '9'); the eyes are two
bones (file 6).

Per character this writes web/public/assets/RIDER_<ID>/fe/ (the rider package file set: world.json, vertices.bin,
indices.bin, colors.bin, 9-N.png, rider.json) plus morphs.bin (dense float32 xyz deltas, Y-up metres, per part
and morph) and rider.json `fe` (character id, variant mask, IRR.DAT record bits, the FE camera view matrix bits,
root transform, rim constants, clips, evidence):

  base riders   the live FE assembly, variant textures (resident in EE RAM), bind matrices and bone slots of the
                preview slot in local/reference/pcsx2/characters/<id>/select.p2s.
  cheat skins   the original preview never shows them: after Rider Details > Cheat Characters the Setup Character
                preview keeps the base rider (setup.p2s: setup slot +0x12 = the cheat id, preview slot = Zoe's FE
                assembly). Their packages follow the base riders' rule (race outfit; HeadX -> HeadX_NIS, + Eyes_NIS,
                HandsX -> HandsX_NIS + DummyX_NIS, all present in MDLPS2.BIG) with the race package's textures and
                bind matrices (the FE and race binds of every shared bone are bit-identical for the ten riders);
                marked evidence 'rule'.
  Sam           his own race package (the original has no Sam preview), lit with 'fe_map' like ids >= 10.

Lighting (0x19EE88): a cleared bank + the rider's IRR.DAT record x 1.0 with modulation (1,1,1,1) (0x389590; the
record itself), then 0x389CB8(bank, hips world position, 1.0) with the FE camera's view matrix. No environment
bank, no local lights. Record by id: 0 moby, 1 kaori, 2 allegra, 3 mac, 4 zoe, 5 griff, 6..8 elise (0x19EF00),
9 viggo, >= 10 fe_map.

python3 tools/export_fe_preview.py [--character kaori ...]
"""
import argparse, hashlib, json, math, shutil, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compare_character_assets import big_members  # noqa: E402
from rider_assets import decode_high_model, decode_rider_texture, read_bolt_entries  # noqa: E402
from world_assets import refpack  # noqa: E402
from export_characters import (ARCHIVES, GC, STATES, WEB, gc_name, memory_of, ps2_header_index,  # noqa: E402
                               ps2_texel_rgba, resident_textures, rule_textures)
from export_opponent_packages import png  # noqa: E402
from export_rider_bind_matrices import derive, local_matrix, multiply  # noqa: E402

GP = 0x4A30F0
IRR_NAMES = {0: 'moby', 1: 'kaori', 2: 'allegra', 3: 'mac', 4: 'zoe', 5: 'griff', 6: 'elise', 7: 'elise', 8: 'elise', 9: 'viggo'}
IRR_BANK = 0x5047F8                  # runtime IRR.DAT bank (names, 160-byte records), modulation 0x504810
RIM = json.loads((WEB / 'ARA1/environment.json').read_text())['irradiance']['rim_constants']   # ELF globals (389CB8)
UBERTRICK_CLIP = 'FE_A_CYC'          # semantic 436: Ubertrick Setup (66ut_btnmap, 0x184C60); Rider Details replays the cheer
BOARD = ('board_',)


def u32(m, a): return struct.unpack_from('<I', m, a)[0]


def fe_slot(memory, player=0):
    manager = u32(memory, u32(memory, GP - 0x848) + 0x7C)
    return manager, manager + 0xB0 + 0xCE0 * player


def fe_assembly(memory, slot, headers, key):
    """Active parts of the preview geometry. The LOD0 model header is entry +0x50 of the part's variant table
    (+0x1C, 16-byte entries {header, material names, flags, ...}); matched byte-exactly to MDLPS2.BIG (runtime
    pointer bytes 56..67 masked). Byte-identical shared parts (Eyes_NIS, Moby/Nate hands) take the rider's prefix."""
    geometry = u32(memory, slot + 8); found = []
    for i in range(u32(memory, geometry + 8)):
        part = u32(memory, geometry + 12) + i * 0x58
        if not u32(memory, part + 0x18): continue
        header = u32(memory, u32(memory, part + 0x1C) + 16 * u32(memory, part + 0x50))
        candidates = headers[key(memory[header:header + 96])]
        if not candidates or any(lod for _, lod in candidates): raise ValueError(f'Missing LOD0 source model for FE part {i}: {candidates}')
        found.append(dict(file=u32(memory, part), first_slot=u32(memory, part + 4), bones=u32(memory, part + 0x44),
                          variant=u32(memory, part + 0x50), candidates=sorted({c[0] for c in candidates})))
    prefix_of = lambda n: n.split('_')[0].lower()
    prefixes = {prefix_of(p['candidates'][0]) for p in found if len(p['candidates']) == 1} - {'board'}
    if len(prefixes) != 1: raise ValueError(f'Mixed FE prefixes {prefixes}')
    prefix = prefixes.pop()
    for p in found:
        c = p.pop('candidates')
        if len(c) > 1: c = [n for n in c if prefix_of(n) in (prefix, 'board')]
        if len(c) != 1: raise ValueError(f'Ambiguous FE source model {c}')
        p['resource_ps2'] = c[0]
    return prefix, found, geometry


def irr_record(name, memory=None):
    """IRR.DAT record (irradiance.json, all coefficient bits match the disc); checked against the runtime bank."""
    doc = json.loads((ROOT / 'local/assets/native/IRRADIANCE/irradiance.json').read_text())
    key = next(k for k in doc['records'] if k.split('\0')[0] == name)
    record = doc['records'][key]
    if memory is not None:
        names, count, records = (u32(memory, IRR_BANK + 4 * k) for k in range(3))   # 0x38ACD0: names, count, 160-byte records
        live = [struct.unpack_from('<40I', memory, records + 160 * i) for i in range(count)]
        if live[record['index']] != tuple(record['coefficient_bits']): raise ValueError(f'IRR record {name} differs from the live bank')
        if struct.unpack_from('<4f', memory, IRR_BANK + 0x18) != (1.0, 1.0, 1.0, 1.0): raise ValueError('FE modulation is not (1,1,1,1)')
    return dict(name=name, key=key, coefficient_bits=record['coefficient_bits'])


def build(cid, gc_parts, decoded, choose, bind, dest, fe, slot_rule=None):
    """Write the FE package. `choose(name, material)` -> texture resource; `bind(rig)` -> (rows, slots, count, note)."""
    archive_names = {}
    bones, bone_map = [], {}
    for name in gc_parts:
        for bone in decoded[name]['bones']:
            k = (bone['file'], bone['index'])
            if k not in bone_map: bone_map[k] = len(bones); bones.append(dict(bone))
    for bone in bones:
        parent = (bone['parent_file'], bone['parent_index'])
        bone['parent'] = -1 if parent == (-1, -1) else bone_map[parent]
        x, y, z = bone['translation']; bone['translation'] = [x, z, -y]
        x, y, z, w = bone['rotation']; bone['rotation'] = [x, z, -y, w]
    cursors = {}
    for bone in sorted(bones, key=lambda b: (b['file'], b['index'])):   # cAnimModel_compile 0x30E140 channel cursors
        cursor = cursors.get(bone['file'], 0)
        bone['animation_translation_channel'] = cursor if bone['dof_flags'] & 1 else -1
        cursor += 3 if bone['dof_flags'] & 1 else 0
        bone['animation_rotation_channel'] = cursor if bone['dof_flags'] & 2 else -1
        cursors[bone['file']] = cursor + (3 if bone['dof_flags'] & 2 else 0)
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(parents=True)
    textures, texture_ids = {}, {}
    vertices, indices, skin, source_skin, batches, parts, morph_blob = [], [], [], [], [], [], bytearray()
    for name in gc_parts:
        model = decoded[name]; base = len(vertices); first_index = len(indices)
        file_id = model['file']                                   # MNF header +94 = the geometry part file id (checked live)
        for v in model['vertices']: vertices.append([v[0], v[2], -v[1], v[3], v[5], -v[4]] + v[6:])
        part_batches = []
        for group in model['material_batches']:
            resource, archive = choose(name, group['material'])
            if resource not in texture_ids:
                data = archive_names.setdefault(archive, dict(big_members((GC / archive).read_bytes())))[resource]
                data = refpack(data) if data[:2] == b'\x10\xfb' else data
                if struct.unpack_from('>I', data, 8)[0] != 1: raise ValueError('Expected one texture per resource')
                w, h, rgba = decode_rider_texture(data[struct.unpack_from('>I', data, 20)[0]:])
                rgba, texels = ps2_texel_rgba(resource, w, h, rgba)
                key = f'9-{len(texture_ids)}'; (dest / f'{key}.png').write_bytes(png(w, h, rgba))
                textures[key] = dict(width=w, height=h, path=f'{key}.png', source='gamecube', resource=resource, archive=archive, texel_domain='ps2', ps2_rgb=texels)
                texture_ids[resource] = len(texture_ids)
            part_batches.append(len(batches))
            batches.append(dict(first_index=len(indices), index_count=len(group['indices']), texture=texture_ids[resource], lightmap=-1,
                                instance=True, part=len(parts), material=group['material']))
            indices.extend(base + i for i in group['indices'])
        for influences in model['skin']:
            skin.append([(bone_map[(x['file'], x['bone'])], x['weight']) for x in influences])
            source_skin.append([(bone_map[(x['file'], x['bone'])], x['source_weight']) for x in influences])
        # Morph targets: dense per output vertex of this part (several output vertices share a raw position).
        morphs = []
        for m in model['morphs']:
            dense = [0.0] * (3 * len(model['vertices']))
            by_position = dict(zip(m['positions'], m['deltas']))
            for k, position in enumerate(model['vertex_positions']):
                d = by_position.get(position)
                if d: dense[3 * k:3 * k + 3] = [d[0], d[2], -d[1]]
            morphs.append(dict(channel=m['channel'], offset=len(morph_blob), vertices=len(m['positions'])))
            morph_blob.extend(struct.pack(f'<{len(dense)}f', *dense))
        skinned = sorted({(x['file'], x['bone']) for s in model['skin'] for x in s})
        parts.append(dict(part=Path(name).stem.split('_', 1)[1], resource=name, model=model['name'], file=file_id,
                          first_vertex=base, vertex_count=len(model['vertices']),
                          first_index=first_index, index_count=len(indices) - first_index, batches=part_batches,
                          board=name.startswith(BOARD), morph_count=model['morph_count'], morphs=morphs,
                          skinned_bones=[list(k) for k in skinned], source_sha256=model['source_sha256']))
    # A bone-less morph part (NIS head/hands) is animated by its own part file id: the FE clip stream of that file
    # holds one weight channel per morph (library.json fe clips, e.g. FE_GEAR_KAORI_CYC '5': 36, '8'/'9': 27).
    rig = dict(bones=bones, skin=skin, source_skin=source_skin, source_skin_weight_units='integer-percent', parts=parts, units='meters', up_axis='Y')
    rows, slots, count, note = bind(rig)
    rig.update(source_bind_matrix_words=rows, source_bone_slots=slots, source_bone_slot_count=count, source_bind_matrix_space='source-centimeters-Z-up',
               source_bind_provenance=note, character=cid, fe=fe)
    (dest / 'vertices.bin').write_bytes(b''.join(struct.pack('<10f', *v) for v in vertices))
    (dest / 'indices.bin').write_bytes(struct.pack(f'<{len(indices)}I', *indices))
    (dest / 'colors.bin').write_bytes(struct.pack('<4f', 1, 1, 1, 1) * len(vertices))
    (dest / 'morphs.bin').write_bytes(bytes(morph_blob))
    (dest / 'rider.json').write_text(json.dumps(rig, separators=(',', ':')))
    bounds = [[min(v[k] for v in vertices) for k in range(3)], [max(v[k] for v in vertices) for k in range(3)]]
    world = dict(version=1, location=f'RIDER_{cid.upper()}/fe', vertex_stride=40, vertex_count=len(vertices), index_count=len(indices), bounds=bounds,
                 batches=batches, textures=textures, imported_instance_count=len(parts), missing_textures=[], source=f'GameCube twins of the original FE preview assembly ({fe["evidence"]["kind"]})')
    (dest / 'world.json').write_text(json.dumps(world, separators=(',', ':')))
    return dict(character=cid, parts=[p['part'] for p in parts], bones=len(bones), slots=count, vertices=len(vertices),
                morphs={p['part']: p['morph_count'] for p in parts if p['morph_count']}, textures=[t['resource'] for t in textures.values()],
                irradiance=fe['irradiance']['name'], evidence=fe['evidence']['kind'])


def fe_block(char_id, memory, manager, slot, state, kind, entry, extra=None):
    view = list(struct.unpack_from('<16I', memory, manager + 0x50))          # camera block +0x40: view matrix rows
    view_f = struct.unpack('<16f', struct.pack('<16I', *view))
    eye = [-sum(view_f[k * 4 + j] * view_f[12 + j] for j in range(3)) for k in range(3)]
    if max(abs(a - b) for a, b in zip(eye, (0, 200, 0))) > 1e-3: raise ValueError(f'FE camera eye {eye}')
    fov = struct.unpack_from('<f', memory, manager + 0x10)[0]
    root = list(struct.unpack_from('<4f', memory, slot + 0xC30)); quat = list(struct.unpack_from('<4f', memory, slot + 0xC40))
    if [round(x, 3) for x in root[:3]] != [136, -250, -82]: raise ValueError(f'FE root {root}')
    name = IRR_NAMES.get(char_id, 'fe_map')
    return dict(character=char_id, variant_mask=u32(memory, slot + 0xCD8), irradiance=irr_record(name, memory),
                view_matrix_bits=view, fov_half_horizontal=fov, root_position_cm=root[:3], root_quaternion=quat,
                rim_constants=RIM, rim_scale=1.0, hips_bone='hips',
                clips=dict(idle=(entry.get('fe') or {}).get('idle'), cheer=(entry.get('fe') or {}).get('cheer'), ubertrick=UBERTRICK_CLIP),
                evidence=dict(kind=kind, state=str(state.relative_to(ROOT)), ee_sha256=hashlib.sha256(memory).hexdigest(),
                              manager=hex(manager), slot=hex(slot), geometry=hex(u32(memory, slot + 8)), **(extra or {})))


def race_textures(cid, prefix):
    """(material kind -> texture resource) of the race package: the live verified equipped textures."""
    folder = WEB / f'RIDER_{cid.upper()}'
    rig = json.loads((folder / 'rider.json').read_text()); world = json.loads((folder / 'world.json').read_text())
    models = dict(big_members((GC / 'mdlngc.big').read_bytes()))
    prefix = prefix or rig.get('resource_prefix') or cid
    names = [p.get('resource') or f"{'board' if p['part'] in ('BindingsA', 'BoardFlexA') else prefix}_{p['part']}.mnf" for p in rig['parts']]
    kinds = [g['material'] for n in names for g in decode_high_model(models[n])['material_batches']]
    if len(kinds) != len(world['batches']) and cid == 'mac':   # the old rider_assets.py Mac package draws no TopA
        kinds = [g['material'] for n in names if n != 'mac_TopA.mnf' for g in decode_high_model(models[n])['material_batches']]
    if len(kinds) != len(world['batches']): raise ValueError(f'{cid}: race batches do not follow part/material order')
    out = {}
    for kind, b in zip(kinds, world['batches']):
        t = world['textures'][f"9-{b['texture']}"]; out.setdefault(kind, (t['resource'], t.get('archive') or ARCHIVES.get(prefix, 'othertxn') + '.big'))
    return out, rig


def base_rider(cid, entry, headers, key, models):
    state = STATES / cid / 'select.p2s'; memory = memory_of(state)
    manager, slot = fe_slot(memory)
    prefix, parts, geometry = fe_assembly(memory, slot, headers, key)
    char_id = u32(memory, slot)
    if char_id != entry['character']: raise ValueError(f'{cid}: FE slot holds character {char_id}')
    gc_parts = [gc_name(models, p['resource_ps2']) for p in parts]
    decoded = {n: decode_high_model(models[n]) for n in gc_parts}
    for p, n in zip(parts, gc_parts):
        if p['bones'] != len(decoded[n]['bones']): raise ValueError(f'{cid}: live bone count differs {n}')
        if p['file'] != decoded[n]['file']: raise ValueError(f'{cid}: live part file {p["file"]} differs from {n} header {decoded[n]["file"]}')
    # Textures: the outfit rule of the race packages (export_characters.rule_textures: material name -> loaded
    # texture); every one is resident in the select state, or is the race package's texture of that material
    # (tiny ones the residency probe skips, e.g. moby_alph_a01).
    rule = rule_textures(cid); resident = resident_textures(memory, prefix)
    chosen = {(n, m): rule[m] for n in gc_parts for m in decoded[n]['materials']}
    race, race_rig = race_textures(cid, prefix)
    unverified = sorted({v[0] for (n, m), v in chosen.items() if Path(v[0]).stem.lower() not in resident and race.get(m, (None,))[0] != v[0]})
    if unverified: raise ValueError(f'{cid}: FE textures not resident {unverified}')
    def bind(rig):
        baseline, provenance, linear, translation = derive(rig, slot + 8 - 0x780, (state,))
        race_rows = {(b['file'], b['index']): row for b, row in zip(race_rig['bones'], race_rig.get('source_bind_matrix_words', []))}
        differ = [b['name'] for b, row in zip(rig['bones'], baseline['matrices']) if (b['file'], b['index']) in race_rows and race_rows[(b['file'], b['index'])] != row]
        if differ: raise ValueError(f'{cid}: FE bind differs from the race bind for {differ}')
        return baseline['matrices'], baseline['slots'], baseline['count'], f'live FE preview geometry {geometry:#x} bind bank (+0x38) in {state.relative_to(ROOT)}; max rest identity error {linear:.2g}'
    fe = fe_block(char_id, memory, manager, slot, state, 'live', entry, dict(active_lod0=[p['resource_ps2'] for p in parts],
                  variants=[p['variant'] for p in parts], first_slots=[p['first_slot'] for p in parts]))
    return build(cid, gc_parts, decoded, lambda n, m: chosen[(n, m)], bind, WEB / f'RIDER_{cid.upper()}/fe', fe)


def nis_twin(members, prefix, part, kind):
    """<prefix>_<kind><letter>_NIS.mnf for a race part (HeadB -> HeadB_NIS), else the A variant."""
    letter = part[len(kind):len(kind) + 1] or 'A'
    for stem in (f'{prefix}_{kind}{letter}_NIS', f'{prefix}_{kind}A_NIS'):
        hit = [n for n in members if n.lower() == stem.lower() + '.mnf']
        if hit: return hit[0]
    return None


def cheat_skin(cid, entry, models):
    """The base riders' rule on the cheat's race outfit (the original never draws a cheat skin in the preview)."""
    race, race_rig = race_textures(cid, None)
    prefix = race_rig['resource_prefix']
    gc_parts = []
    for p in race_rig['parts']:
        name, part = p['resource'], p['part']
        if part.startswith('Head') and not part.startswith('HeadBolt'):
            twin = nis_twin(models, prefix, part, 'Head'); gc_parts.append(twin or name)
            eyes = [n for n in models if n.lower() == f'{prefix}_eyes_nis.mnf'] if twin else []
            gc_parts.extend(eyes)
        elif part.startswith('Hands'):
            hands, dummy = nis_twin(models, prefix, part, 'Hands'), nis_twin(models, prefix, 'Dummy' + part[5:], 'Dummy')
            gc_parts.extend([hands, dummy] if hands and dummy else [name])
        else: gc_parts.append(name)
    decoded = {n: decode_high_model(models[n]) for n in gc_parts}
    gc_parts.sort(key=lambda n: decoded[n]['file'])            # geometry parts in file order (as every live FE assembly)
    def choose(n, m):
        if m not in race: raise ValueError(f'{cid}: no race texture for material {m} ({n})')
        return race[m]
    def bind(rig):
        race_rows = {(b['file'], b['index']): row for b, row in zip(race_rig['bones'], race_rig['source_bind_matrix_words'])}
        world, rows = [], []
        for b in rig['bones']:
            local = local_matrix(b); world.append(local if b['parent'] < 0 else multiply(world[b['parent']], local))
            row = race_rows.get((b['file'], b['index']))
            if row is None:                                       # a NIS-only bone (the eyes): the authored inverse rest
                inv = invert(world[-1]); row = list(struct.unpack('<16I', struct.pack('<16f', *inv)))
            rows.append(row)
        slots, cursor = [], 0                                     # FE geometry rule: part bone slots are cumulative in file order
        for n in sorted((n for n in gc_parts if decoded[n]['bones']), key=lambda n: decoded[n]['file']):
            for b in decoded[n]['bones']: slots.append(cursor + b['index'])
            cursor += len(decoded[n]['bones'])
        order = {(b['file'], b['index']): i for i, b in enumerate(rig['bones'])}
        by_part = [(b['file'], b['index']) for n in sorted((n for n in gc_parts if decoded[n]['bones']), key=lambda n: decoded[n]['file']) for b in decoded[n]['bones']]
        mapped = [None] * len(rig['bones'])
        for k, s in zip(by_part, slots): mapped[order[k]] = s
        return rows, mapped, cursor, f'race package bind rows of {cid} (bit-identical to the FE binds for every base rider); NIS-only bones: inverse authored rest'
    fe = dict(character=entry['character'], variant_mask=None, irradiance=irr_record('fe_map'),
              rim_constants=RIM, rim_scale=1.0, hips_bone='hips', clips=None,
              evidence=dict(kind='rule', note='the original Setup Character preview keeps the base rider for a cheat skin (characters/<cheat>/setup.p2s)',
                            parts=gc_parts))
    return build(cid, gc_parts, decoded, choose, bind, WEB / f'RIDER_{cid.upper()}/fe', fe)


def invert(m):
    """Inverse of a column-major 4x4 rigid transform (rotation + translation)."""
    r = [[m[c * 4 + r] for c in range(3)] for r in range(3)]; t = m[12:15]
    rt = [[r[c][rr] for c in range(3)] for rr in range(3)]
    ti = [-sum(rt[rr][c] * t[c] for c in range(3)) for rr in range(3)]
    return [rt[0][0], rt[1][0], rt[2][0], 0, rt[0][1], rt[1][1], rt[2][1], 0, rt[0][2], rt[1][2], rt[2][2], 0, *ti, 1]


def sam(entry):
    """Sam's own race package as his preview (no original Sam preview; ids >= 10 light with fe_map)."""
    src, dest = WEB / 'RIDER_SAM', WEB / 'RIDER_SAM/fe'
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(parents=True)
    for f in src.iterdir():
        if f.is_file() and f.suffix in ('.png', '.bin') or f.name == 'world.json': shutil.copy2(f, dest / f.name)
    rig = json.loads((src / 'rider.json').read_text())
    vertices = (src / 'vertices.bin').stat().st_size // 40
    rig['parts'] = [dict(part=p['name'], resource=None, file=None, first_vertex=p['first_vertex'], vertex_count=p['vertex_count'],
                         first_index=p['first_index'], index_count=p['index_count'], board=p['name'].startswith(('derived_BindingsA', 'K2_')),
                         morph_count=0, morphs=[]) for p in rig['parts']]
    if sum(p['vertex_count'] for p in rig['parts']) != vertices: raise ValueError('Sam parts do not cover the mesh')
    rig['fe'] = dict(character=10, variant_mask=None, irradiance=irr_record('fe_map'), rim_constants=RIM, rim_scale=1.0, hips_bone='hips',
                     clips=dict(idle=entry['fe']['idle'], cheer=entry['fe']['cheer'], ubertrick=UBERTRICK_CLIP),
                     evidence=dict(kind='sam', note="the port's own rider; Mac's FE clips (the Sam build uses Mac's slot)"))
    (dest / 'morphs.bin').write_bytes(b'')
    (dest / 'rider.json').write_text(json.dumps(rig, separators=(',', ':')))
    return dict(character='sam', parts=len(rig['parts']), bones=len(rig['bones']), evidence='sam')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--character', action='append')
    args = parser.parse_args()
    roster = json.loads((WEB / 'riders.json').read_text())
    headers, key = ps2_header_index()
    models = dict(big_members((GC / 'mdlngc.big').read_bytes()))
    report = []
    for entry in roster:
        if args.character and entry['id'] not in args.character: continue
        if entry['kind'] == 'rider': r = base_rider(entry['id'], entry, headers, key, models)
        elif entry['kind'] == 'cheat': r = cheat_skin(entry['id'], entry, models)
        else: r = sam(entry)
        print(json.dumps(r)); report.append(r)
    from export_rider_textures import pack_all; pack_all()   # the packages' PNGs -> the riders' texture archives
    return report


if __name__ == '__main__':
    main()
