#!/usr/bin/env python3
"""Every selectable original rider as a browser human rider (docs/characters.md).

Per character this reads its OWN derived savestates (tools/ps2_navigate.py from the Select Character
screen, local/reference/pcsx2/characters/<id>/{select,countdown,glide}.p2s; recipe in docs/characters.md)
where that character is the human rider on the Snow Jam grid, and writes:

  package   local/assets/native/RIDER_<ID> + web/public/assets/RIDER_<ID> (same file set as the
            Snow Jam opponents): the live human assembly (every active LOD0 model part matched byte-exactly
            to MDLPS2.BIG headers, decoded from the GameCube MNF twins), textures, source skin weights,
            the live bind matrices + bone slots (tools/export_rider_bind_matrices.derive), animation seed.
  settings  web/public/assets/RIDER_<ID>/settings.json: the initial.json overrides for this human, i.e. every
            key of the character sections that export_npc_riders.py extracts (landing, boost, air entry,
            rail context, grab control, trick identity, animation inputs, secondary motion, reset stance)
            whose value differs from the same extraction of Zoe on her own derived countdown state (Zoe gets
            no overrides, so she stays bit-exact), plus identity (channel-1 masks 0x8C0/0x8C8/0x8D0, gameplay
            character, CHARDB weight for rider pairs 0x11FF98).

python3 tools/export_characters.py [--character psymon ...] [--skip-package]
"""
import argparse, hashlib, json, shutil, struct, sys, zipfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compare_character_assets import big_members  # noqa: E402
from rider_assets import decode_high_model, decode_rider_texture, read_bolt_entries  # noqa: E402
from world_assets import refpack  # noqa: E402
from locations import human_rider  # noqa: E402

STATES = ROOT / 'local/reference/pcsx2/characters'
SHARED_WITH_OPPONENTS = {'psymon', 'allegra', 'moby', 'griff', 'luther'}
NATIVE = ROOT / 'local/assets/native'
# New character packages live apart from local/assets/native/RIDER_MAC (rider_assets.py's Mac test outfit,
# the source of tools/sam_mesh.py) and the opponents' packages.
CHARACTER_NATIVE = NATIVE / 'CHARACTERS'
WEB = ROOT / 'web/public/assets'
GC = ROOT / 'local/gamecube/disc/files/data/char'
PS2_MODELS = ROOT / 'local/assets/source/ps2/mdlps2.big'

# Model resource prefix -> GameCube texture archive (DATA/CHAR/<X>TXN.BIG; PS2 <X>TXP.BIG).
ARCHIVES = dict(moby='mobytxn', kaori='kaoritxn', arielle='allegtxn', mac='mactxn', zoe='zoetxn', grommet='grifftxn',
                elise='elisetxn', rocco='natetxn', psymon='psymotxn', deiter='viggotxn')


def memory_of(path):
    with zipfile.ZipFile(path) as z: return z.read('eeMemory.bin')


def ps2_header_index():
    def key(raw): return raw[:56] + bytes(12) + raw[68:]  # three runtime pointers at 56..67 (audit_rider_assemblies.py)
    index = defaultdict(list)
    for name, data in big_members(PS2_MODELS.read_bytes()):
        count, at, _ = struct.unpack_from('<HHI', data, 4)
        for lod in range(count): index[key(data[at + lod * 96:at + (lod + 1) * 96])].append((name, lod))
    return index, key


def live_assembly(memory, actor, headers, key):
    """Active LOD0 model parts of the live actor (geometry +0x0C parts, 0x58 stride, +0x18 active, +0x1C model).
    A few parts are byte-identical across characters (moby/rocco HandsA): those take the rider's own prefix."""
    u = lambda a: struct.unpack_from('<I', memory, a)[0]
    geometry = u(actor + 0x780); found = []
    for i in range(u(geometry + 8)):
        part = u(geometry + 12) + i * 0x58
        if not u(part + 0x18): continue
        pointer = u(u(part + 0x1C)); candidates = headers[key(memory[pointer:pointer + 96])]
        if not candidates or any(lod for _, lod in candidates): raise ValueError(f'Missing LOD0 source model for part {i}: {candidates}')
        found.append((u(part), sorted({c[0] for c in candidates}), u(part + 0x44)))
    prefix_of = lambda name: name.split('_')[0].lower()
    prefixes = {prefix_of(c[0]) for _, c, _ in found if len(c) == 1} - {'board'}
    if len(prefixes) != 1: raise ValueError(f'Mixed character prefixes {prefixes}')
    prefix = prefixes.pop(); parts = []
    for part_id, candidates, bones in found:
        if len(candidates) > 1: candidates = [c for c in candidates if prefix_of(c) in (prefix, 'board')]
        if len(candidates) != 1: raise ValueError(f'Ambiguous source model {candidates}')
        parts.append(dict(part_id=part_id, resource_ps2=candidates[0], bones=bones))
    return prefix, parts, list(struct.unpack_from('<3f', memory, geometry + 0x140))


def gc_name(members, ps2_resource):
    stem = Path(ps2_resource).stem.lower()
    matches = [n for n in members if Path(n).stem.lower() == stem and n.lower().endswith('.mnf')]
    if len(matches) != 1: raise ValueError(f'No unique GameCube twin for {ps2_resource}: {matches}')
    return matches[0]


PS2_TEXTURES = dict(moby='MOBYTXP', kaori='KAORITXP', arielle='ALLEGTXP', mac='MACTXP', zoe='ZOETXP', grommet='GRIFFTXP', elise='ELISETXP',
                    rocco='NATETXP', psymon='PSYMOTXP', deiter='VIGGOTXP')
_ps2_archives = {}


def resident_textures(memory, prefix):
    """The equipped texture resources of a rider: EA keeps every loaded texture in EE RAM and uploads it per draw,
    so the equipped ones are the TXP.BIG members (DATA/CHAR/<X>TXP.BIG, PS2 .ssh) whose pixel data is resident
    (three 48-byte probes). Menu icons are skipped. Returns lowercased stems."""
    from inspect_disc import Disc
    from export_career import big_entries
    archive = PS2_TEXTURES.get(prefix, 'OTHERTXP')
    if archive not in _ps2_archives:
        from disc_paths import ps2_iso;disc = Disc(ps2_iso())
        try: _ps2_archives[archive] = big_entries(disc.file(f'DATA/CHAR/{archive}.BIG'))
        finally: disc.close()
    out = set()
    for name, data in _ps2_archives[archive].items():
        stem = Path(name).stem.lower()
        if 'icon' in stem or not stem.startswith(prefix + '_'): continue
        if data[:2] == b'\x10\xfb': data = refpack(data)
        if len(data) < 600: continue
        if all(memory.find(data[o:o + 48]) >= 0 for o in (len(data) // 3, len(data) // 2, 2 * len(data) // 3)): out.add(stem)
    return out


# ---- PS2 texel domain -----------------------------------------------------------------------------
# EA's PS2 rider textures (DATA/CHAR/<X>TXP.BIG .ssh, 8-bit CLUT) hold colour at half intensity: 128 = 1.0, so the
# rider's GS HIGHLIGHT2 (tex x light / 128 + light.a; lighting up to 255) can double it. The GameCube twins
# (CMPR, 255 = 1.0) are ~2x the PS2 values (+-CMPR error). The browser's lit rider path (web/rider-material.js,
# web/fe-preview.js) multiplies exactly what the GS multiplies, so the packages carry the PS2 texels.
def ps2_texel_rgba(resource, w, h, rgba):
    """GameCube RGBA -> PS2 texel domain: RGB of the PS2 texture of the same name when its size matches (alpha
    stays the GameCube one, which the browser's alpha test was built on), else the GameCube RGB halved.
    The PS2 CLUT is read as the GS holds it (shps_image gs_clut): 535 TXP textures (hair / hat 'alph', 'ea*',
    'ext*', a few boards and boots) have texels on CLUT entries past the block's count, which the old read turned
    into black specks (docs/xbox-textures.md section 6). Returns (rgba, exact_ps2_rgb)."""
    from export_career import shps_image
    stem = Path(resource).stem.lower(); prefix = stem.split('_')[0]
    archive = PS2_TEXTURES.get(prefix, 'OTHERTXP')
    if archive not in _ps2_archives: resident_textures(b'', prefix)          # loads that TXP.BIG from the ISO
    names = {Path(n).stem.lower(): d for n, d in _ps2_archives[archive].items()}
    out = bytearray(rgba); data = names.get(stem)
    if data is not None:
        data = refpack(data) if data[:2] == b'\x10\xfb' else data
        pw, ph, px = shps_image(data, gs_clut=True)
        if (pw, ph) == (w, h):
            for i in range(0, len(out), 4): out[i:i + 3] = px[i:i + 3]
            return bytes(out), True
    for i in range(0, len(out), 4): out[i:i + 3] = bytes((v + 1) >> 1 for v in out[i:i + 3])
    return bytes(out), False


def ps2_texel_pngs(web_folder):
    """Rewrite a browser rider package's 9-N.png (GameCube resources) in the PS2 texel domain, from the GameCube
    source (idempotent), and mark them texel_domain 'ps2' in world.json. Textures without a GameCube resource
    (Sam's painted maps, generated wardrobe packages) stay GameCube-domain; the shaders halve those."""
    from export_opponent_packages import png
    path = web_folder / 'world.json'; world = json.loads(path.read_text()); members = {}; report = []
    for key, t in world['textures'].items():
        if t.get('source') != 'gamecube' or not t.get('resource'): continue
        archive = t.get('archive') or ARCHIVES.get(Path(t['resource']).stem.split('_')[0].lower(), 'othertxn') + '.big'
        if archive not in members: members[archive] = {n.lower(): d for n, d in big_members((GC / archive).read_bytes())}
        data = members[archive][t['resource'].lower()]; data = refpack(data) if data[:2] == b'\x10\xfb' else data
        w, h, rgba = decode_rider_texture(data[struct.unpack_from('>I', data, 20)[0]:])
        if (w, h) != (t['width'], t['height']): raise ValueError(f'{web_folder.name} {key}: texture size differs')
        rgba, exact = ps2_texel_rgba(t['resource'], w, h, rgba)
        t.pop('pack', None); t.pop('id', None); t['path'] = t.get('path') or f'{key}.png'   # a packed entry: PNG again (tools/export_rider_textures.py packs it)
        (web_folder / t['path']).write_bytes(png(w, h, rgba)); t['texel_domain'] = 'ps2'; t['ps2_rgb'] = exact
        report.append((t['resource'], exact))
    path.write_text(json.dumps(world, separators=(',', ':')))
    return report


def rule_textures(cid):
    """material name -> GameCube texture member of the rider's default race outfit, by the original rule (docs/characters.md
    "Equip Gear and outfits"): the equipped entries of a fresh profile's race set (0x14D068; a cheat skin: its whole
    bucket, 0x11BBE8) load their textures (0x11BE88, '$' wildcards filled by 0x14B988 from another equipped entry of
    the same group; cheat skins raw), and a model material binds the loaded texture whose SSH entry name is the
    material's name. Shared with tools/export_wardrobe.py / web/wardrobe.js (which verify it against the PS2 states)."""
    from export_wardrobe import Bolt, Inventory, assemble, gc_resource, disc_file, ORIGINAL
    global _bolt
    if '_bolt' not in globals(): _bolt = Bolt(disc_file('DATA/CHAR/BOLTPS2.DAT', ORIGINAL / 'BOLTPS2.DAT'))
    ch = next(r['character'] for r in json.loads((WEB / 'riders.json').read_text()) if r['id'] == cid and r['kind'] in ('rider', 'cheat'))
    _, textures = assemble(Inventory(_bolt, ch).init(), cheat=ch >= 10)
    archives = sorted({gc_resource(e['texture'])[0] for e in _bolt.entries if e['char'] == ch and e['texture']})
    out = {}
    for stem in textures:
        hit = next(((a,) + _gc_archive(a)[stem.lower()] for a in archives if stem.lower() in _gc_archive(a)), None)
        if hit is None: raise ValueError(f'{cid}: loaded texture {stem} not in {archives}')
        archive, name, data = hit; data = refpack(data) if data[:2] == b'\x10\xfb' else data
        out.setdefault(data[16:20].decode('latin1'), (name, archive))   # the first loaded texture of a name
    return out


def choose_textures(prefix, archive_members=None, decoded=None, wardrobe=None, resident=frozenset()):
    """(part, material) -> texture resource by the original rule (rule_textures) for the rider of this resource prefix;
    kept for tools/export_fe_preview.py (the FE preview loads the same textures: its assembly is the same equipped
    set with the NIS models instead of the race ones)."""
    cid = next(r['id'] for r in json.loads((WEB / 'riders.json').read_text()) if r['kind'] in ('rider', 'cheat') and (r.get('resource_prefix') or r['id']) == prefix)
    rule = rule_textures(cid)
    return {(part, kind): rule[kind][0] for part, model in decoded.items() for kind in model['materials']}


def rule_prefix(cid):
    entry = next(r for r in json.loads((WEB / 'riders.json').read_text()) if r['id'] == cid)
    return entry.get('resource_prefix') or cid


_gc_archives = {}
def _gc_archive(archive):
    if archive not in _gc_archives: _gc_archives[archive] = {n.lower(): (n, d) for n, d in big_members((GC / archive).read_bytes())}
    return _gc_archives[archive]


def build_package(cid, prefix, gc_parts, folder, resident=frozenset()):
    """rider_assets.py main() for an explicit live part list (bones, Y-up conversion, animation channels)."""
    models = dict(big_members((GC / 'mdlngc.big').read_bytes()))
    archive = ARCHIVES.get(prefix, 'othertxn')
    textures_big = dict(big_members((GC / f'{archive}.big').read_bytes()))
    bolts = (GC / 'boltngc.dat').read_bytes(); wardrobe = read_bolt_entries(bolts)
    decoded = {name: decode_high_model(models[name]) for name in gc_parts}
    bones, bone_map = [], {}
    for model in decoded.values():
        for bone in model['bones']:
            k = (bone['file'], bone['index'])
            if k not in bone_map: bone_map[k] = len(bones); bones.append(bone)
    for bone in bones:
        parent = (bone['parent_file'], bone['parent_index'])
        if parent == (-1, -1): bone['parent'] = -1
        elif parent in bone_map: bone['parent'] = bone_map[parent]
        else: raise ValueError(f'Missing parent bone {parent}')
        x, y, z = bone['translation']; bone['translation'] = [x, z, -y]
        x, y, z, w = bone['rotation']; bone['rotation'] = [x, z, -y, w]
    cursors = defaultdict(int)
    for bone in sorted(bones, key=lambda b: (b['file'], b['index'])):
        cursor = cursors[bone['file']]
        bone['animation_translation_channel'] = cursor if bone['dof_flags'] & 1 else -1
        cursor += 3 if bone['dof_flags'] & 1 else 0
        bone['animation_rotation_channel'] = cursor if bone['dof_flags'] & 2 else -1
        cursors[bone['file']] = cursor + (3 if bone['dof_flags'] & 2 else 0)
    if folder.exists(): shutil.rmtree(folder)
    (folder / 'textures').mkdir(parents=True)
    rule = rule_textures(cid)
    chosen = {(part, kind): rule[kind][0] for part, model in decoded.items() for kind in model['materials']}
    archive_of = {name: arc for name, arc in rule.values()}
    order = ['suit', 'boot', 'head', 'bord', 'alph']
    resources = sorted(set(chosen.values()), key=lambda n: (min((order.index(k) if k in order else 9) for (p, k), v in chosen.items() if v == n), n))
    textures, texture_ids = {}, {}
    for i, name in enumerate(resources):
        data = _gc_archive(archive_of[name])[name.lower()][1]; data = refpack(data) if data[:2] == b'\x10\xfb' else data
        if struct.unpack_from('>I', data, 8)[0] != 1: raise ValueError('Expected one texture per resource')
        w, h, rgba = decode_rider_texture(data[struct.unpack_from('>I', data, 20)[0]:])
        (folder / f'textures/9-{i}.rgba').write_bytes(rgba)
        textures[f'9-{i}'] = dict(width=w, height=h, path=f'textures/9-{i}.rgba', source='gamecube', resource=name, archive=archive_of[name],
                                  equipped_variant_verified=Path(name).stem.lower() in resident)
        texture_ids[name] = i
    vertices, indices, skin, source_skin, batches, part_info = [], [], [], [], [], []
    for name, model in decoded.items():
        base = len(vertices)
        for v in model['vertices']: vertices.append([v[0], v[2], -v[1], v[3], v[5], -v[4]] + v[6:])
        for group in model['material_batches']:
            batches.append(dict(first_index=len(indices), index_count=len(group['indices']), texture=texture_ids[chosen[(name, group['material'])]], lightmap=-1, instance=True))
            indices.extend(base + i for i in group['indices'])
        for influences in model['skin']:
            skin.append([(bone_map[(x['file'], x['bone'])], x['weight']) for x in influences])
            source_skin.append([(bone_map[(x['file'], x['bone'])], x['source_weight']) for x in influences])
        part_info.append(dict(part=Path(name).stem.split('_', 1)[1], resource=name, model=model['name'], morph_count=model['morph_count'], source_sha256=model['source_sha256']))
    (folder / 'vertices.bin').write_bytes(b''.join(struct.pack('<10f', *v) for v in vertices))
    (folder / 'indices.bin').write_bytes(struct.pack(f'<{len(indices)}I', *indices))
    (folder / 'colors.bin').write_bytes(struct.pack('<4f', 1, 1, 1, 1) * len(vertices))
    hair = [n for n, m in decoded.items() if any(b['name'].startswith('sec_') for b in m['bones'])]
    rig = dict(bones=bones, skin=skin, source_skin=source_skin, source_skin_weight_units='integer-percent', parts=part_info, units='meters', up_axis='Y',
               hairstyle=dict(parts=hair, bolt_sha256=hashlib.sha256(bolts).hexdigest(), secondary_motion='original secondary motion slot in settings.json'),
               character=cid, resource_prefix=prefix, texture_archive=f'{archive}.big',
               texture_selection_verified=all(Path(n).stem.lower() in resident for n in chosen.values()))
    bounds = [[min(v[k] for v in vertices) for k in range(3)], [max(v[k] for v in vertices) for k in range(3)]]
    world = dict(version=1, location=f'RIDER_{cid.upper()}', vertex_stride=40, vertex_count=len(vertices), index_count=len(indices), bounds=bounds,
                 batches=batches, textures=textures, lighting_verified=False, imported_patch_count=0, imported_instance_count=len(decoded), missing_textures=[],
                 source=f'GameCube {prefix} original rider assets (live PS2 human assembly)')
    (folder / 'world.json').write_text(json.dumps(world, indent=2) + '\n')
    return rig


def package(cid, states):
    countdown, glide = states / 'countdown.p2s', states / 'glide.p2s'
    memory = memory_of(countdown); actor = human_rider(memory)
    headers, key = ps2_header_index()
    prefix, parts, body_scale = live_assembly(memory, actor, headers, key)
    if live_assembly(memory_of(glide), human_rider(memory_of(glide)), headers, key)[1] != parts: raise ValueError('Assembly differs between countdown and glide')
    members = dict(big_members((GC / 'mdlngc.big').read_bytes()))
    gc_parts = [gc_name(members, p['resource_ps2']) for p in parts]
    folder = native_folder(cid)
    if cid in SHARED_WITH_OPPONENTS:
        # The Snow Jam computer riders' packages (tools/export_opponent_packages.py) drive the AI cores; their
        # bone order must not change. Check the human assembly is the same default outfit and reuse them.
        existing = json.loads((folder / 'rider.json').read_text())
        if sorted(p['resource'].lower() for p in existing['parts']) != sorted(n.lower() for n in gc_parts):
            raise ValueError(f'{cid}: human assembly differs from the opponent package: {gc_parts}')
        return dict(package=f'RIDER_{cid.upper()}', reused='opponent package (same live assembly)', parts=gc_parts, retexture=retexture(cid, prefix, states))
    rig = build_package(cid, prefix, gc_parts, folder, resident_textures(memory_of(states / ('select.p2s' if (states / 'select.p2s').exists() else 'countdown.p2s')), prefix))
    for p, name in zip(parts, gc_parts):
        if p['bones'] != len(decode_high_model(members[name])['bones']): raise ValueError(f'Live bone count differs {name}')
    from export_rider_bind_matrices import derive
    baseline, provenance, linear, translation = derive(rig, actor, (countdown, glide))
    rig.update(source_bone_slots=baseline['slots'], source_bone_slot_count=baseline['count'], source_bind_matrix_words=baseline['matrices'],
               source_bind_matrix_space='source-centimeters-Z-up',
               source_bind_provenance=f'live PS2 human actor {actor:#x} geometry+0x38 bank, identical in {countdown.relative_to(ROOT)} and glide',
               assembly_evidence=dict(states=[str(countdown.relative_to(ROOT)), str(glide.relative_to(ROOT))], actor=hex(actor), body_scale=body_scale,
                                      active_lod0=[p['resource_ps2'] for p in parts], header_fixup_bytes=[56, 68], max_rest_identity_linear_error=linear))
    (folder / 'rider.json').write_text(json.dumps(rig, indent=2) + '\n')
    from probe_rider_pose import animation_inputs
    (folder / 'animation-start.json').write_text(json.dumps(animation_inputs(countdown, actor, rig_path=folder), indent=2) + '\n')
    return web_package(folder, WEB / f'RIDER_{cid.upper()}')


def retexture(cid, prefix, states=None):
    """Point every batch of a package at the texture the original binds to its material (rule_textures: the default
    race outfit's loaded textures by SSH name). Only textures and batch texture indices change; bone order, skin and
    geometry stay (the capture gates and the AI cores read rider.json). The batches follow the parts of rider.json,
    each model's material batches in order. Returns the changes [[old, new], ...]."""
    from export_opponent_packages import png
    folder, web = native_folder(cid), WEB / f'RIDER_{cid.upper()}'
    rig = json.loads((web / 'rider.json').read_text()); models = dict(big_members((GC / 'mdlngc.big').read_bytes()))
    names = [p.get('resource') or f"{'board' if p['part'] in ('BindingsA', 'BoardFlexA') else prefix}_{p['part']}.mnf" for p in rig['parts']]
    decoded = {n: decode_high_model(models[n]) for n in names}
    rule = rule_textures(cid); archive_of = {n.lower(): a for n, a in rule.values()}
    materials = [g['material'] for n in names for g in decoded[n]['material_batches']]
    wanted = [rule[m][0] for m in materials]
    changes = []
    for doc_path in (web / 'world.json', folder / 'world.json'):
        if not doc_path.exists(): continue
        is_web = doc_path.parent == web
        world = json.loads(doc_path.read_text())
        if len(world['batches']) != len(wanted): raise ValueError(f'{cid}: batch list does not follow the part/material order')
        by_resource = {t['resource'].lower(): k for k, t in world['textures'].items() if t.get('resource')}
        current = [world['textures'][f"9-{b['texture']}"]['resource'] for b in world['batches']]
        for name in dict.fromkeys(wanted):
            if name.lower() in by_resource: continue
            key = f'9-{len(world["textures"])}'; data = _gc_archive(archive_of[name.lower()])[name.lower()][1]; data = refpack(data) if data[:2] == b'\x10\xfb' else data
            w, h, rgba = decode_rider_texture(data[struct.unpack_from('>I', data, 20)[0]:])
            if is_web: (web / f'{key}.png').write_bytes(png(w, h, rgba)); path = f'{key}.png'
            else: path = f'textures/{key}.rgba'; (folder / path).write_bytes(rgba)
            world['textures'][key] = dict(width=w, height=h, path=path, source='gamecube', resource=name, archive=archive_of[name.lower()])
            by_resource[name.lower()] = key
        for b, name, m in zip(world['batches'], wanted, materials): b['texture'] = int(by_resource[name.lower()].split('-')[1]); b['material'] = m
        used = {w.lower() for w in wanted}
        for t in world['textures'].values(): t['equipped_variant_verified'] = t['resource'].lower() in used; t['texture_rule'] = '0x11BE88/0x14B988 by SSH name'
        doc_path.write_text(json.dumps(world, separators=(',', ':')) if is_web else json.dumps(world, indent=2) + '\n')
        if is_web: changes = [[c, w] for c, w in dict.fromkeys(zip(current, wanted)) if c.lower() != w.lower()]
    ps2_texel_pngs(web)
    return changes


def native_folder(cid):
    return (NATIVE if cid in SHARED_WITH_OPPONENTS or cid == 'zoe' else CHARACTER_NATIVE) / f'RIDER_{cid.upper()}'


def web_package(source, dest):
    """The browser file set (tools/export_opponent_packages.py package(): PNG textures, bins, rider.json, seeds)."""
    from export_opponent_packages import png
    keep = (dest / 'settings.json').read_bytes() if (dest / 'settings.json').exists() else None
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(parents=True)
    if keep is not None: (dest / 'settings.json').write_bytes(keep)   # written by settings(); a repackage keeps it
    world = json.loads((source / 'world.json').read_text())
    for k, t in world['textures'].items():
        rgba = (source / t['path']).read_bytes()
        if len(rgba) != t['width'] * t['height'] * 4: raise ValueError(f'{source.name} {k}: texture size mismatch')
        (dest / f'{k}.png').write_bytes(png(t['width'], t['height'], rgba)); t['path'] = f'{k}.png'
    for f in ['vertices.bin', 'indices.bin', 'colors.bin', 'rider.json', 'animation-start.json']: shutil.copy2(source / f, dest / f)  # clip table: ANIMATIONS/animation-samples.json
    (dest / 'world.json').write_text(json.dumps(world, separators=(',', ':')))
    ps2_texel_pngs(dest)
    rig = json.loads((dest / 'rider.json').read_text()); vertices = (dest / 'vertices.bin').stat().st_size // 40
    if len(rig['source_skin']) != vertices or len(rig['skin']) != vertices: raise ValueError(f'{source.name}: skin/vertex count mismatch')
    return dict(package=dest.name, bones=len(rig['bones']), source_slots=rig['source_bone_slot_count'], vertices=vertices,
                triangles=(dest / 'indices.bin').stat().st_size // 12, textures=len(world['textures']), hair=rig['hairstyle']['parts'],
                bytes=sum(p.stat().st_size for p in dest.iterdir()))


# ---- settings -------------------------------------------------------------------------------------
def extract_human(states, rig_folder):
    """The same extractions export_npc_riders.py makes for a computer rider, on the human actor."""
    from reference_landing import extract_landing
    from reference_boost import extract_boost
    from reference_air_entry import extract_air_entry
    from reference_rail_context import extract_memory as extract_rail
    from reference_grab_lifecycle import extract_grab_lifecycle
    from reference_trick_identity import extract_trick_identity
    from reference_reset import extract_reset
    from reference_secondary_motion import extract_secondary_motion
    from probe_rider_pose import animation_inputs
    snapshot = states / 'countdown.p2s'; memory = memory_of(snapshot); actor = human_rider(memory)
    s = dict(original_landing=extract_landing(memory, actor), original_boost=extract_boost(memory, actor), original_air_entry=extract_air_entry(memory, actor),
             original_rail_context=extract_rail(memory, actor), original_grab_control=extract_grab_lifecycle(memory, actor),
             original_trick_identity=extract_trick_identity(memory, actor))
    animation = animation_inputs(snapshot, actor, rig_path=rig_folder)
    s['original_animation'] = {k: animation[k] for k in ('scale', 'contact', 'pivot_bone', 'default_root_position', 'default_root_rotation', 'default_mirror',
                                                         'bone_mask', 'limited_bones', 'variant_flags', 'character') if k in animation}
    s['original_animation']['secondary_motion'] = extract_secondary_motion(memory, actor)
    # The grid spot: the human's countdown ground state (position/normal/lift follow the body scale, reverse_stance the
    # base rider's stance) and the ground-profile body scale (geometry+0x140) the core's compiled Zoe seed holds.
    from reference_ground_profile import extract_ground
    ground = extract_ground(memory, actor)
    s['original_event_start'] = dict(state=ground['state'], body_scale=ground['profile']['body_scale'])
    reset = extract_reset(memory, actor); reset.pop('paths')
    s['original_reset'] = {k: v for k, v in reset.items() if k in ('stance', 'allow_path_end')}
    u = lambda a: struct.unpack_from('<I', memory, a)[0]; q = lambda a: struct.unpack_from('<Q', memory, a)[0]
    entry = u(0x5305B0 + u(actor + 0x86C) * 4)
    identity = dict(upper_mask8c0=hex(q(actor + 0x8C0)), upper_mask8c8=hex(q(actor + 0x8C8)), upper_mask8d0=hex(q(actor + 0x8D0)),
                    gameplay_character_id=struct.unpack_from('<b', memory, 0x535B20 + entry * 28 + 17)[0], actor=hex(actor),
                    ee_sha256=hashlib.sha256(memory).hexdigest())
    from reference_pair_collision import extract_pair_collision
    pair = extract_pair_collision(memory)['participants'][0]
    identity['pair'] = dict(weight_attribute=pair['weight_attribute'], collision_stat=pair['resolved_collision_stat'], attack_stat=pair['resolved_attack_stat'])
    return s, identity


# Keys that describe the moment, not the character (provenance, clocks, the contact frame of the grid spot).
STATE_KEYS = {'provenance', 'runtime', 'ee_sha256', 'rider'}
STATE_PATHS = {'original_animation.contact.normal', 'original_animation.contact.board_direction', 'original_animation.contact.board_alignment',
               'original_animation.contact.board_lift_cm', 'original_animation.contact.leg_weight'}


def overrides(values, zoe, path):
    """Leaf differences (objects recurse, arrays/scalars compare whole); applied by a deep merge
    (web/character-roster.js mergeCharacterSettings)."""
    out = {}
    for k, v in values.items():
        here = f'{path}.{k}' if path else k
        if k in STATE_KEYS or here in STATE_PATHS: continue
        z = zoe.get(k) if isinstance(zoe, dict) else None
        if isinstance(v, dict) and isinstance(z, dict):
            sub = overrides(v, z, here)
            if sub: out[k] = sub
        elif json.dumps(v, sort_keys=True) != json.dumps(z, sort_keys=True): out[k] = v
    return out


def settings(cid, states, zoe_states, target=None):
    # target: where the document goes instead of web/public/assets/RIDER_<ID>/settings.json (tools/export_exact_event_starts.py:
    # the same document from the exact-derived states, local/assets/native-exact/RIDER_<ID>/settings.json)
    folder = native_folder(cid)
    mine, identity = extract_human(states, folder)
    base, zoe_identity = extract_human(zoe_states, NATIVE / 'RIDER_ZOE')
    diff = {section: overrides(values, base[section], section) for section, values in mine.items()}
    diff = {k: v for k, v in diff.items() if v}
    if 'original_event_start' in diff: diff['original_event_start'] = mine['original_event_start']   # the core reads the whole state
    # A cheat character (id 10..29) is a skin on the setup slot's base rider (0x14A080 +0x11 / 0x14A0B0 +0x12):
    # its states ride on Zoe, so its differences are the skin's own (scale, rig, masks, hair) plus the four
    # uber overrides (0x150198..: Stretch/Gutless/Canhuck Nose Grab, Snowballs Tail Grab). Those are kept as
    # rows so web/character-roster.js can lay them over any base rider's table.
    memory = memory_of(states / 'countdown.p2s'); actor = human_rider(memory)
    u = lambda a: struct.unpack_from('<I', memory, a)[0]; entry = u(0x5305B0 + u(actor + 0x86C) * 4)
    base_character, cheat_id = memory[0x535B20 + entry * 28 + 17], memory[0x535B20 + entry * 28 + 18]
    extra = {}
    if cheat_id:
        # A skin can be worn by any base rider (web/character-roster.js composeCheat): keep the model-dependent values
        # even where they equal Zoe's, so they replace the base rider's. Verified on brodi-on-psymon: composing Psymon's
        # and Brodi's documents reproduces that state's own extraction, with the air-entry pivot x and the grid seed's
        # reverse_stance following the base rider's stance.
        for section, keys in (('original_animation', ('scale', 'contact', 'bone_mask', 'secondary_motion', 'character')),
                              ('original_air_entry', ('pivot',)), ('original_landing', ('profile',))):
            for key in keys:
                value = mine[section].get(key)
                if value is None: continue
                if section == 'original_landing': value = dict(body_scale=value['body_scale'])
                if section == 'original_animation' and key == 'contact': value = dict(legs=value['legs'])
                diff.setdefault(section, {})[key] = value
        diff['original_event_start'] = mine['original_event_start']
        if base_character != 4: raise ValueError('Cheat character states must ride on Zoe')
        uber = diff.get('original_grab_control', {}).get('profile', {}).pop('uber', None)
        if uber is not None:
            zoe_uber = base['original_grab_control']['profile']['uber']
            extra['uber_rows'] = [[s, i, row] for s, rows in enumerate(uber) for i, row in enumerate(rows) if row != zoe_uber[s][i]]
            for key in ('profile',):
                if not diff['original_grab_control'].get(key): diff['original_grab_control'].pop(key, None)
            if not diff['original_grab_control']: diff.pop('original_grab_control')
    doc = dict(version=1, character=cid, kind='cheat' if cheat_id else 'rider', cheat_id=cheat_id or None, gameplay_character=base_character,
               settings=diff, **extra, identity=identity,
               provenance=dict(states=str(states.relative_to(ROOT)), zoe_states=str(zoe_states.relative_to(ROOT)), zoe_identity=zoe_identity,
                               rule='keys whose extraction differs from Zoe on her own derived countdown state; Zoe = initial.json'))
    if cid == 'zoe': return doc   # Zoe is the course initial.json itself (no settings.json: bit-exact capture gates)
    target = Path(target) if target else WEB / f'RIDER_{cid.upper()}/settings.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(doc, indent=1) + '\n')
    return doc


def sam_settings():
    """Sam (the port's own rider, sam_character/, config/characters/sam.json) the original way: CHARDB model size follows
    the height (5'11" Elise = 96; 5'10" Moby 94; 5'9" Psymon 92; 5'8" Viggo 89; 5'6" Zoe/Mac 85), so Sam (71 in) is 0.96.
    Every scale-dependent value of the original riders is a function of the body scale (grid spot, lift, contact legs,
    air-entry pivot, landing/ground body scale; verified by the equal-scale riders and brodi-on-psymon), so they come
    from Elise's live actor with the regular-stance sign (pivot x, reverse stance off). Character values use Mac's
    template (Sam takes Mac's slot in the Sam PS2 build and web/career.js): uber table and animation variant flags;
    the bone mask is Sam's own 26-bone skeleton. Weight attribute: CHARDB +0x40 grows ~5 per 20 lb (150 lb Elise 70,
    170 lb Viggo 75): 160 lb -> 70. Stats: the fresh-profile raw 5s like every rider."""
    elise, elise_id = extract_human(STATES / 'elise', native_folder('elise'))
    zoe, zoe_id = extract_human(STATES / 'zoe', NATIVE / 'RIDER_ZOE')
    mac = json.loads((WEB / 'RIDER_MAC/settings.json').read_text())
    sam = json.loads((ROOT / 'config/characters/sam.json').read_text())
    if sam['identity']['height_inches'] != 71 or sam['identity']['stance'] != 'regular': raise ValueError('Sam identity changed: revisit the template')
    rig = json.loads((WEB / 'RIDER_SAM/rider.json').read_text())
    pivot = list(elise['original_air_entry']['pivot']); pivot[0] = -abs(pivot[0])
    start = json.loads(json.dumps(elise['original_event_start'])); start['state']['reverse_stance'] = False
    settings = dict(
        original_landing=dict(profile=dict(body_scale=elise['original_landing']['profile']['body_scale'])),
        original_air_entry=dict(pivot=pivot),
        original_grab_control=mac['settings']['original_grab_control'],
        original_animation=dict(scale=elise['original_animation']['scale'], contact=dict(legs=elise['original_animation']['contact']['legs']),
                                bone_mask=(1 << len(rig['bones'])) - 1, variant_flags=mac['settings']['original_animation']['variant_flags'], character='sam'),
        original_event_start=start)
    identity = dict(upper_mask8c0=zoe_id['upper_mask8c0'], upper_mask8c8=zoe_id['upper_mask8c8'], upper_mask8d0=zoe_id['upper_mask8d0'],
                    gameplay_character_id=3, pair=dict(weight_attribute=70, collision_stat=elise_id['pair']['collision_stat'], attack_stat=elise_id['pair']['attack_stat']))
    doc = dict(version=1, character='sam', kind='custom', settings=settings, identity=identity,
               provenance=dict(rule=sam_settings.__doc__.split('\n')[0], scale_template='elise', character_template='mac'))
    (WEB / 'RIDER_SAM/settings.json').write_text(json.dumps(doc, indent=1) + '\n')
    return doc


def validate_compositions():
    """'<skin>-on-<base>' evidence states: web/character-roster.js composeCheat(base doc, skin doc) must equal that
    state's own extraction (leaf differences from Zoe, incl. the grid seed)."""
    def merge(a, b):
        out = dict(a)
        for k, v in b.items(): out[k] = merge(a[k], v) if isinstance(v, dict) and isinstance(a.get(k), dict) else v
        return out
    def flat(d, p=''):
        out = {}
        for k, v in d.items():
            if isinstance(v, dict) and k not in ('state',): out.update(flat(v, f'{p}.{k}'))
            else: out[f'{p}.{k}'] = json.dumps(v, sort_keys=True)
        return out
    for folder in sorted(p for p in STATES.iterdir() if '-on-' in p.name and (p / 'countdown.p2s').exists()):
        skin, base = folder.name.split('-on-')
        mine, _ = extract_human(folder, native_folder(skin))
        zoe, _ = extract_human(STATES / 'zoe', NATIVE / 'RIDER_ZOE')
        derived = {k: v for k, v in ((sec, overrides(v, zoe[sec], sec)) for sec, v in mine.items()) if v}
        derived['original_event_start'] = mine['original_event_start']
        b = json.loads((WEB / f'RIDER_{base.upper()}/settings.json').read_text())['settings']
        k = json.loads((WEB / f'RIDER_{skin.upper()}/settings.json').read_text())['settings']
        k = json.loads(json.dumps(k))
        if b.get('original_reset', {}).get('stance') == 1:
            k['original_air_entry']['pivot'][0] = -k['original_air_entry']['pivot'][0]; k['original_event_start']['state']['reverse_stance'] = True
        composed = merge(b, k)
        a, c = flat(derived), flat(composed)
        extra = sorted(x for x in a if x not in c or a[x] != c[x])
        # keys the skin forces to its own values may also equal Zoe's (absent from the leaf differences)
        stale = sorted(x for x in c if x not in a and c[x] != flat({x.split('.')[1]: zoe.get(x.split('.')[1], {})}).get(x, c[x]))
        if extra: raise ValueError(f'{folder.name}: composition differs from the derived state at {extra}')
        print(folder.name, 'composition = derived state', len(a), 'keys', 'stale' if stale else '')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--character', action='append')
    parser.add_argument('--skip-package', action='store_true')
    parser.add_argument('--skip-settings', action='store_true')
    parser.add_argument('--ps2-texels', action='store_true', help='only rewrite every RIDER_* package PNG in the PS2 texel domain')
    parser.add_argument('--retexture', action='store_true', help='keep the packages, only apply the texture rule (retexture)')
    args = parser.parse_args()
    if args.ps2_texels:
        for folder in sorted(WEB.glob('RIDER_*')):
            if (folder / 'world.json').exists(): print(folder.name, ps2_texel_pngs(folder))
        from export_rider_textures import pack_all; pack_all()
        return
    # '<skin>-on-<base>' folders are composition evidence (validated by web/test-characters.mjs), not characters
    # a roster character's folder only: '<skin>-on-<base>' folders are composition evidence, others (e.g. mac-junction) other evidence
    roster = {r['id'] for r in json.loads((WEB / 'riders.json').read_text()) if r['kind'] in ('rider', 'cheat')}
    ids = args.character or sorted(p.name for p in STATES.iterdir() if (p / 'countdown.p2s').exists() and p.name in roster)
    for cid in ids:
        states = STATES / cid
        if not args.skip_package:
            if cid == 'zoe' or args.retexture: print(cid, 'texture rule: swaps', retexture(cid, rule_prefix(cid), states))
            else: print(json.dumps(package(cid, states)))
        if not args.skip_settings:
            doc = settings(cid, states, STATES / 'zoe')
            print(cid, json.dumps({k: sorted(v) for k, v in doc['settings'].items()}), doc['identity']['upper_mask8c0'], doc['identity']['gameplay_character_id'])
    if not args.character:
        validate_compositions()
        # Sam (the port's own rider) only with his private inputs (config/characters/sam.json, his web package)
        if (ROOT / 'config/characters/sam.json').exists() and (WEB / 'RIDER_SAM/rider.json').exists():
            print('sam', sorted(sam_settings()['settings']))
    from export_rider_textures import pack_all; pack_all()   # the packages' PNGs -> the riders' texture archives


if __name__ == '__main__':
    main()
