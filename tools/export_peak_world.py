#!/usr/bin/env python3
"""Export the connected Peak 1 free-ride world as one package per streamed LOCATION (docs/peak-mountain.md).

    .venv/bin/python tools/export_peak_world.py                 # every Peak 1 location
    .venv/bin/python tools/export_peak_world.py --only A B      # some locations
    .venv/bin/python tools/export_peak_world.py --manifest-only

The original streams whole locations (ELF location table 0x43E250, streaming table 0x442168) in and out of
the world as the course index changes (residency table 0x442488, 22CEA8/22D088). A race event keeps one
residency row resident (the event packages of web/prepare.py merge those rows); free ride and the peak runs
switch rows while riding, so every location is packaged ALONE here:

    local/assets/native/PEAK1/<LOC>/   tools/import_world.py --location LOC --no-event --output local/assets/native/PEAK1
                                       + rails.json (import_rails, event=False), environment-lighting.json
    web/public/assets/PEAK1/<LOC>/     world.json (textures: world library refs + lightmaps.tex), vertices/indices/colors/vertex-alpha .bin,
                                       terrain.json, world_collision.json, rails.json, terrain-render.json,
                                       terrain-lighting.json, terrain-light-atlas.png, fog-tree.json (when authored)
    web/public/assets/PEAK1/peak.json  the manifest: locations {code, id, track, kind, bounds, files},
                                       residency rows by course index, course names, sky per row

Every location keeps its original resource ids (rid<<8 | SDB track), so the core can hold all of them at once
and gate queries by the live residency (web/peak_world.inc). Nothing here reads a savestate.
SPDX-License-Identifier: GPL-3.0-only
"""
import argparse, array, hashlib, importlib.util, json, shutil, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import elf_read, ELF, activation_dir  # noqa: E402
from world_assets import world_chunks, records, locations as sdb_locations, world_resource_names  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
NATIVE = ROOT / 'local/assets/native/PEAK1'
WEB = ROOT / 'web/public/assets/PEAK1'
PEAK = 0  # course table +0x54 career peak (0-based): Peak 1
NAME = 'PEAK1'  # the streamed world's location name (world_collision / stage world / assets folder)
# Static-model env-map modes (web/prepare.py ENV_MODES): material word & 0x660000 -> the second pass's ALPHA_2 (0x200000: Cs + Cd; 0x600000: Cs x Ad + Cd).
ENV_MODES = {0x200000: 0x200000, 0x220000: 0x200000, 0x260000: 0x200000, 0x600000: 0x600000, 0x620000: 0x600000, 0x660000: 0x600000}
OUT = None  # --out DIR: the re-split batches (and SETPIECES/attached.json) go to DIR/<LOC>/ instead of WEB (scratch exports)
# Per peak (--peak N, docs/peak3.md): the event packages whose set-piece exports merge into the peak's stage world, the
# course whose package supplies the environment's course-level scalars, and the stage seed of every location's programs.
PEAKS = {
    1: dict(setpiece_courses=['ARA1', 'BRA2', 'BHP1', 'ASS1', 'ABA1', 'ABC1'], environment_base='ARA1',
            stage_seed='peak_stage_seed.hpp'),
    2: dict(setpiece_courses=['CRA3', 'DRA4', 'DSS2', 'CBA2', 'CHP2', 'DBC2'], environment_base='CRA3',
            stage_seed='peak2_stage_seed.hpp'),
    3: dict(setpiece_courses=['ERA5', 'ESS3', 'EBA3', 'EHP3', 'EBC3'], environment_base='ERA5',
            stage_seed='peak3_stage_seed.hpp'),
}


def configure(peak):
    """Select the peak this run exports (1 = the historical Peak 1 paths and names, unchanged)."""
    global NATIVE, WEB, PEAK, NAME, SETPIECE_COURSES
    NAME = f'PEAK{peak}'; PEAK = peak - 1
    NATIVE = ROOT / f'local/assets/native/{NAME}'; WEB = ROOT / f'web/public/assets/{NAME}'
    SETPIECE_COURSES = PEAKS[peak]['setpiece_courses']


def stage_seed():
    """(header path, namespace) of the peak's stage tables (tools/export_peak_stage.py)."""
    return ROOT / 'web/generated' / PEAKS[PEAK + 1]['stage_seed'], f'browser_stage_{NAME.lower()}'


def elf_tables():
    """Location table 0x43E250, course table 0x43D950, residency table 0x442488, course kinds 0x442820."""
    elf = ELF.read_bytes()
    locs = {}
    for i in range(50):
        e = elf_read(0x43E250 + 24 * i, 24, elf)
        name = e[4:20].split(b'\0')[0].decode()
        if not name:
            break
        locs[i] = dict(id=struct.unpack_from('<I', e, 0)[0], code=name, kind=struct.unpack_from('<I', e, 20)[0])
    courses = []
    for i in range(23):
        e = elf_read(0x43D950 + 0x64 * i, 0x64, elf)
        index, = struct.unpack_from('<i', e, 0)
        row = struct.unpack_from('<10i', elf_read(0x442488 + 40 * i, 40, elf))
        kind = struct.unpack_from('<2i', elf_read(0x442820 + 8 * i, 8, elf))
        courses.append(dict(index=index, name=e[4:0x24].split(b'\0')[0].decode('latin-1'), short=e[0x24:0x34].split(b'\0')[0].decode('latin-1'),
                            code=e[0x34:0x44].split(b'\0')[0].decode(), peak=struct.unpack_from('<i', e, 0x54)[0],
                            station=struct.unpack_from('<i', e, 0x58)[0], field5c=struct.unpack_from('<i', e, 0x5C)[0],
                            region=struct.unpack_from('<i', e, 0x60)[0],
                            residency=dict(count=row[1], sky=row[2], transp=row[3], locations=[x for x in row[4:4 + row[1]]]),
                            kind=kind[1]))
    return locs, courses


def peak_locations(locs, courses):
    """Every location resident in some Peak 1 residency row (courses and stations of career peak 0)."""
    ids = []
    for c in courses:
        if c['peak'] != PEAK or c['code'] == 'dbg':
            continue
        for i in c['residency']['locations']:
            if i not in ids:
                ids.append(i)
    return ids


def run(*args):
    print('+', ' '.join(str(a) for a in args), flush=True)
    subprocess.run([sys.executable, *map(str, args)], cwd=ROOT, check=True)


def native_import(code, force=False):
    target = NATIVE / code
    if force or not (target / 'world.json').exists():
        run('tools/import_world.py', '--location', code, '--no-event', '--output', NATIVE)
    if force or not (target / 'rails.json').exists():
        from import_rails import export_location
        names = world_resource_names((SOURCE / 'bam.phm').read_bytes(), (SOURCE / 'bam.psm').read_bytes())
        export_location(code, SOURCE, target, names, event=False)
    if force or not (target / 'environment-lighting.json').exists():
        from export_environment_lighting import export
        export(SOURCE, target, code)


def instance_models(code):
    """Model/material/instance records of one location (for the static-model blend classes and V4-5 alpha)."""
    locs = sdb_locations(SOURCE / 'bam.sdb')
    selected = next(i for i, l in enumerate(locs) if l['name'] == code)
    begin, end = (locs[selected - 1]['chunk_end'] + 1 if selected else 0), locs[selected]['chunk_end']
    models, materials, instances = {}, {}, {}
    for index, chunk in enumerate(world_chunks(SOURCE / 'bam.ssb')):
        if index > end:
            break
        if index != 0 and not begin <= index <= end:
            continue
        for kind, track, rid, data in records(chunk):
            if kind == 2:
                models[track, rid] = data
            elif kind == 0:
                materials[track, rid] = data
            elif kind == 3 and index != 0:
                instances[track, rid] = data
    return models, materials, instances


def event_moving(code, own):
    """Moving instances of an event course's package, as web/prepare.py splits them (batch.moving_resource, web/moving-instances.js from
    the core's moving_instances()): the crashbag rollers (scripted-instances.json 'roller'), the chairlift / MultiSpline cars, drawn
    SplineModifier pieces and looping splines (set-pieces.json), and every avalanche group an AvaSpline makes follow its tumbler
    (<LOC>/avalanches.json ava_spline, docs/avalanche.md). Drawn spline LiveComps (attached.json) are split per node instead. Only the
    resources of this location package (own). Hubs and connectors have no event export: none."""
    assets = ROOT / 'web/public/assets'; out = []
    scripted = activation_dir(code) / 'scripted-instances.json'
    if scripted.exists():
        out += [x['resource'] for x in json.loads(scripted.read_text())['instances'] if x['contact'] == 'roller']
    sp = activation_dir(code) / 'set-pieces.json'
    if sp.exists():
        s = json.loads(sp.read_text())
        out += [l['resource'] for l in s.get('chairlifts', [])] + s.get('drawn_spline_pieces', [])
        out += [m['resource'] for m in s.get('multisplines', [])] + [m['resource'] for m in s.get('spline_modifiers', [])]
    ava = assets / code / 'avalanches.json'
    if code != 'ARA1' and ava.exists():
        out += [g['resource'] for a in json.loads(ava.read_text())['avalanches'] for g in a['groups'] if g['ava_spline']]
    drawn = {x['resource'] for x in event_attached_doc(code).get('splineLiveComps', []) if x['drawn']}
    return sorted({r for r in out if r in own and r not in drawn})


def event_attached_doc(code):
    base = ROOT / 'web/public/assets' / ('' if code == 'ARA1' else code) / 'LIVECOMP/attached.json'
    return json.loads(base.read_text()) if base.exists() else {}


def event_attached(code, own):
    """ParentModifier children and drawn spline LiveComps of an event course (tools/export_attached_setpieces.py, web/attached-setpieces.js),
    split as web/prepare.py does: a child's meshes on node 0 (drawn with the parent's node matrix, 0x357108), a drawn spline LiveComp's
    meshes per node. Returns ((resource, mesh) -> node, the children: drawn although the free-ride audit may hide their static draw)."""
    doc = event_attached_doc(code); nodes = {}
    for x in doc.get('splineLiveComps', []):
        if x['drawn'] and x['resource'] in own:
            for m, node in enumerate(x['meshNodes']): nodes[(x['resource'], m)] = node
    children = {p['child'] for p in doc.get('parents', []) if p['parentKind'] != 'object' and p['child'] in own}
    for p in doc.get('parents', []):
        if p['child'] in children:
            for m in range(p['childMeshes']): nodes[(p['child'], m)] = 0
    return nodes, children


def attached_package(dest):
    """<peak>/SETPIECES/attached.json: every event course's ParentModifier parents and spline LiveComps merged (resources carry the
    SDB track), for web/peak-set-pieces.js AttachedSetPieces (parentKind 'livecomp': the searchlight glows on their bases, JS) and the
    spline LiveComps (ravens, blimps and their ads / lights: the core's set_piece_attached, not in the streamed core yet)."""
    merged = dict(version=1, location=NAME, fps=60, coordinate_system=None, source='tools/export_peak_world.py attached_package', splineLiveComps=[], parents=[])
    for code in SETPIECE_COURSES:
        doc = event_attached_doc(code)
        if not doc: continue
        merged['coordinate_system'] = merged['coordinate_system'] or doc.get('coordinate_system')
        merged['splineLiveComps'] += [dict(x, course=code) for x in doc.get('splineLiveComps', [])]
        merged['parents'] += [dict(p, course=code) for p in doc.get('parents', [])]
    out = dest / 'SETPIECES'; out.mkdir(parents=True, exist_ok=True)
    (out / 'attached.json').write_text(json.dumps(merged, separators=(',', ':')))
    print('attached: parents', len(merged['parents']), 'spline LiveComps', len(merged['splineLiveComps']))
    return merged


def web_package(code, batches_only=False):
    """The browser package of one location (same file formats as a course package, web/prepare.py). batches_only: only
    re-split the draw batches (world.json batches, indices.bin, vertex-alpha.bin, instance-flags.json) of an existing
    package, keeping its texture table (like web/prepare.py --batches-only)."""
    from world_models import decode_model
    src, dest = NATIVE / code, (OUT or WEB) / code
    dest.mkdir(parents=True, exist_ok=True)
    d = json.loads((src / 'world.json').read_text())
    if batches_only:
        d['textures'] = json.loads((WEB / code / 'world.json').read_text())['textures']
    else:
        # World textures: references into the shared world texture library, GameCube lightmaps in <dest>/lightmaps.tex
        # (tools/export_world_textures.py); no per-location PNG copies.
        from export_world_textures import package_textures
        package_textures(dest, d['textures'], lambda key, t: (src / t['path']).read_bytes())
        for f in ['vertices.bin', 'colors.bin', 'terrain.json', 'world_collision.json', 'rails.json']:
            shutil.copy2(src / f, dest / f)
    vs = array.array('f'); vs.frombytes((src / 'vertices.bin').read_bytes())
    inds = array.array('I'); inds.frombytes((src / 'indices.bin').read_bytes())
    models, materials, instances = instance_models(code)
    # Static-model GS blend classes (web/prepare.py: 37E238/37F2A4..37F6C8) and the VIF V4-5 alpha bit.
    blends = {}

    def mesh_blend(model, mesh):
        key = tuple(model)
        if key not in blends:
            data = models[key]; header = struct.unpack_from('<I', data, 16)[0]; classes = []
            for m in decode_model(data):
                word = struct.unpack_from('<I', materials[m['material']], 12)[0] | (0x40000 if m['group_flags'] & 8 else 0)
                classes.append(3 if header & 8 else {0: 0, 0x20000: 1, 0x40000: 2, 0x60000: 2}[word & 0x60000])
            blends[key] = classes
        return blends[key][mesh]
    wraps = {}

    def mesh_wrap(model, mesh):   # GS CLAMP_1 bits of the material (web/prepare.py mesh_wrap, web/world-material.js)
        key = tuple(model)
        if key not in wraps:
            wraps[key] = [(struct.unpack_from('<I', materials[m['material']], 12)[0] >> 19) & 3 for m in decode_model(models[key])]
        return wraps[key][mesh]
    triangle_blend, triangle_wrap = {}, {}
    for source in d['collision_sources']:
        if source['kind'] == 'instance':
            blend, wrap = mesh_blend(source['model'], source['mesh']), mesh_wrap(source['model'], source['mesh'])
            for tri in range(source['first_triangle'], source['first_triangle'] + source['triangle_count']) if blend or wrap else ():
                if blend: triangle_blend[tri] = blend
                if wrap: triangle_wrap[tri] = wrap
    # Env-map second pass (web/prepare.py mesh_env: 37F2A4..37FD2C, the record's second texture in GS context 2; pv envMap).
    envs = {}

    def mesh_env(model, mesh):
        key = tuple(model)
        if key not in envs:
            out = []
            for m in decode_model(models[key]):
                rec = materials[m['material']]; word = struct.unpack_from('<I', rec, 12)[0] | (0x40000 if m['group_flags'] & 8 else 0)
                mode = ENV_MODES.get(word & 0x660000)
                out.append((struct.unpack_from('<h', rec, 2)[0], mode) if mode else 0)
            envs[key] = out
        return envs[key][mesh]
    triangle_env = {}
    for source in d['collision_sources']:
        if source['kind'] == 'instance':
            env = mesh_env(source['model'], source['mesh'])
            for tri in range(source['first_triangle'], source['first_triangle'] + source['triangle_count']) if env else ():
                triangle_env[tri] = env
    if __import__('os').environ.get('SSX_ENV_SPLIT') == '0': triangle_env = {}   # comparisons: the batches without the env split
    if triangle_env:   # the second textures join the texture table as world texture library references
        from export_world_textures import library_index, LIBRARY_URL
        for tex in sorted({e[0] for e in triangle_env.values()}):
            e = library_index()[tex]
            d['textures'].setdefault(f'9-{tex}', dict(width=e['width'], height=e['height'], source='ps2', fallback_reason=None, pack=LIBRARY_URL, id=tex))

    def colour_words(data):
        at, words = 160, []
        while at + 4 <= len(data):
            word = struct.unpack_from('<I', data, at)[0]; at += 4
            if word >> 24 not in (0x6f, 0x7f):
                continue
            count = ((word >> 16) & 255) or 256; words.extend(struct.unpack_from(f'<{count}H', data, at)); at = (at + count * 2 + 3) & ~3
        return words
    colours = array.array('f'); colours.frombytes((src / 'colors.bin').read_bytes())
    alpha = bytearray([128]) * (len(colours) // 4); cache = {}
    for source in d['collision_sources']:
        if source['kind'] != 'instance':
            continue
        key = tuple(source['model'])
        if key not in cache:
            cache[key] = decode_model(models[key])
        mesh = cache[key][source['mesh']]; words = colour_words(instances[source['track'], source['rid']])
        tri = source['first_triangle']; base = min(inds[tri * 3:(tri + source['triangle_count']) * 3])
        for i in range(len(mesh['vertices'])):
            value = words[mesh['color_offset'] + i]; at = (base + i) * 4
            if any(abs(colours[at + k] - ((value >> (5 * k)) & 31) / 31) > 1e-6 for k in range(3)):
                raise ValueError(f"{code}: instance colour/vertex mismatch {source['track']}:{source['rid']}")
            if not value >> 15:
                alpha[base + i] = 0
    (dest / 'vertex-alpha.bin').write_bytes(bytes(alpha))
    spec = importlib.util.spec_from_file_location('world_batches', ROOT / 'web/world-batches.py')
    batch_module = importlib.util.module_from_spec(spec); spec.loader.exec_module(batch_module)
    # Free-ride instance visibility is runtime state (the countdown audit of the event packages does not apply);
    # the importer already drops the authored trigger volumes. The free-ride audit is a documented gap.
    audit_path = ROOT / f'local/event-activation/{NAME}/{code}/freeride-instances.json'  # tools/export_peak_instances.py
    audit = json.loads(audit_path.read_text()) if audit_path.exists() else None
    hidden = [r for r in (audit['hidden'] if audit else []) if any(s['kind'] == 'instance' and ((s['rid'] << 8) | s['track']) == r for s in d['collision_sources'])] if audit else []
    if audit is None:
        print(f'WARNING {code}: no free-ride instance audit; every imported instance is drawn')
    else:
        (dest / 'instance-flags.json').write_text(json.dumps(dict(version=1, location=code, savestate=audit['savestate'], runtime=audit['runtime']), separators=(',', ':')))
    # Stage world of the streamed world (setpiece_packages, web/peak-set-pieces.js): the same batch splits as an event package.
    sp = setpiece_batches(load_setpieces()); own = {(s['rid'] << 8) | s['track'] for s in d['collision_sources'] if s['kind'] == 'instance'}
    mine = lambda values: {r for r in values if r in own}
    if sp:
        hidden = sorted(set(hidden) | mine(sp['flags']))
    # The event course's moving instances and ParentModifier / spline-LiveComp splits (web/prepare.py: rollers, attached.json).
    moving = event_moving(code, own); attached_nodes, children = event_attached(code, own)
    hidden = [r for r in hidden if r not in children]
    live_nodes = {**({k: v for k, v in sp['livecomp_nodes'].items() if k[0] in own} if sp else {}), **attached_nodes}
    batches, new = batch_module.spatial_batches(d, vs, inds, triangle_blend=triangle_blend, triangle_wrap=triangle_wrap, triangle_env=triangle_env, hidden_resources=hidden,
        moving_resources=moving,
        scroll_groups={r: g for r, g in sp['scroll_groups'].items() if r in own} if sp else None,
        livecomp_nodes=live_nodes or None,
        script_resources=sorted(mine(sp['script_resources']) - set(hidden)) if sp else (),
        meshanim_nodes={k: v for k, v in sp['meshanim_nodes'].items() if k[0] in own} if sp else None)
    d['batches'] = batches
    (dest / 'indices.bin').write_bytes(new.tobytes())
    (dest / 'world.json').write_text(json.dumps(d, separators=(',', ':')))
    if batches_only:
        return d
    # PS2 terrain combine inputs (web/world-material.js): authored patch descriptors + the light-page atlas.
    spec = importlib.util.spec_from_file_location('prepare_terrain_render', ROOT / 'tools/prepare_terrain_render.py')
    terrain_render = importlib.util.module_from_spec(spec); spec.loader.exec_module(terrain_render)
    terrain_render.prepare(f'{NAME}/' + code, output=dest, audit=ROOT / f'local/browser-validation/{NAME}/{code}/terrain-adjacency-audit.json')
    spec = importlib.util.spec_from_file_location('prepare_environment', ROOT / 'web/prepare-environment.py')
    environment = importlib.util.module_from_spec(spec); spec.loader.exec_module(environment)
    j = json.loads((src / 'environment-lighting.json').read_text()); raw = bytearray()
    for texture in j['textures']:
        for field in ['rgba', 'valid']:
            texture[field + '_offset'] = len(raw); raw.extend((src / texture[field]).read_bytes())
    environment.terrain_lighting(j, raw, dest)
    environment.screen_tint(code, dest)  # the location's own ScreenTint painter (absent package when it has none)
    try:
        from export_fog_tree import export as export_fog
        export_fog(code, ROOT / f'local/event-activation/{NAME}/{code}')
        shutil.copy2(ROOT / f'local/event-activation/{NAME}/{code}/fog-tree.json', dest / 'fog-tree.json')
    except ValueError as error:  # connectors carry no Fog painter section of their own
        print(f'{code}: no fog painter ({error})')
        (dest / 'fog-tree.json').unlink(missing_ok=True)
    return d


def environment_package(codes):
    """PEAK1/environment.json + .bin: every location's environment colour lattice (the rider lighting reads the lattice of
    the patch under the rider, keyed by patch resource) merged, textures renumbered. The course-level scalars and the
    Lighting (irradiance) section come from the Snow Jam package (area A); per-location Lighting painters are a gap."""
    base = json.loads((ROOT / f"web/public/assets/{PEAKS[PEAK + 1]['environment_base']}/environment.json").read_text())
    merged = {k: base[k] for k in base if k not in ('patches', 'textures')}
    merged['location'] = NAME; merged['patches'] = []; merged['textures'] = []
    merged['streaming_assumption'] = 'Peak 1 locations merged by patch resource (tools/export_peak_world.py); residency gates the patches'
    raw = bytearray(); shared = {}; sdb = [l['name'] for l in sdb_locations(SOURCE / 'bam.sdb')]
    for code in codes:
        src = NATIVE / code; j = json.loads((src / 'environment-lighting.json').read_text()); remap = {}
        track = sdb.index(code)  # the export covers the location's event residency: keep its own patches only
        j['patches'] = [p for p in j['patches'] if p['resource'] & 255 == track]
        used = {i for p in j['patches'] for i in p['textures'] if i >= 0}
        for t in j['textures']:
            if t['id'] not in used:
                continue
            if t['source_sha256'] in shared:  # the same disc texture in several locations: one copy
                remap[t['id']] = shared[t['source_sha256']]; continue
            shared[t['source_sha256']] = remap[t['id']] = len(merged['textures']); t = dict(t, id=len(merged['textures']))
            for field in ['rgba', 'valid']:
                t[field + '_offset'] = len(raw); raw.extend((src / t[field]).read_bytes())
            merged['textures'].append(t)
        for patch in j['patches']:
            merged['patches'].append(dict(patch, textures=[remap[i] if i >= 0 else -1 for i in patch['textures']]))
    (WEB / 'environment.bin').write_bytes(raw); (WEB / 'environment.json').write_text(json.dumps(merged))
    print('environment: patches', len(merged['patches']), 'textures', len(merged['textures']), 'bytes', len(raw))


def paths_packages(codes):
    """PEAK1/<LOC>/paths.json: the location's AIP records (SSB kind 14, one per variant rid) as the runtime path banks.
    12A340 (resource dispatcher 3AAE60 case 14) replaces the race/reset path bank 0x4D33A0 when a location with id < 22
    loads (courses and hubs; connectors are rejected): hubs take variant 1 in a time challenge (kind 5), 2 in a points
    challenge (kind 6, mode 11 -> 1), otherwise 0; courses only variant 0. Then 112180(rider, 0) re-attaches every human
    to the region row (26B5E0 kind 1, index = player) paths. Event types renumbered as tools/export_course_initial.py."""
    from race_event_assets import decode_aip
    from export_course_initial import RACE_EVENT_TYPES, RESET_EVENT_TYPES, remap
    locs = sdb_locations(SOURCE / 'bam.sdb'); names = {i: l['name'] for i, l in enumerate(locs)}
    found = {}
    for chunk in world_chunks(SOURCE / 'bam.ssb'):
        for kind, track, rid, data in records(chunk):
            if kind == 14 and names.get(track) in codes:
                try:
                    found.setdefault(names[track], {})[rid] = decode_aip(data)
                except ValueError as error:
                    print(f'{names[track]} AIP rid {rid}: {error}')
    for code, variants in found.items():
        out = {}
        for rid, aip in sorted(variants.items()):
            unknown = set()
            race = [dict(index=p['index'], origin=p['position'], low=p['low'], high=p['high'], remaining_at_origin=p['field2'],
                         segments=p['segments'], events=remap(p['events'], RACE_EVENT_TYPES, unknown)) for p in aip['track_paths']]
            reset = [dict(origin=p['position'], low=p['low'], high=p['high'], segments=p['segments'],
                          events=remap(p['events'], RESET_EVENT_TYPES, unknown), flags38=p['fields'][3], field3c=p['fields'][6]) for p in aip['ai_paths']]
            # region +0x20 = AI (reset) path, +0x24 = race path: Snow Jam grid slot 0 = reset path 2 (event route) and race path 3
            regions = [dict(index=r[0], kind=r[1], position=r[2:5], direction=r[5:8], reset_path=r[8], race_path=r[9]) for r in aip['regions']]
            out[str(rid)] = dict(race_paths=race, reset_paths=reset, regions=regions, links=aip['links'], sha256=aip['provenance']['sha256'],
                                 unknown_event_types=sorted(unknown))
        (WEB / code / 'paths.json').write_text(json.dumps(dict(version=1, location=code, variants=out), separators=(',', ':')))
        print(code, 'AIP variants', sorted(variants))


def painter_packages(codes):
    """Per-location world painter sections the streamed world swaps by region (the rider's contacted location, like the
    Fog / ScreenTint trees): PEAK1/<LOC>/lighting.json (type 11, tWPIGD_Lighting: payload references + scalars and the
    point tree; web/environment_bridge.cpp lighting_region), sun-painter.json (type 9, web/sun-flare.js setTree) and
    glare-painter.json (type 6, web/glare-pass.js setTree; null when the location has none). PEAK1/lighting-banks.json
    holds every IRR bank a Peak 1 Lighting payload or the peak default (22E180: xPBR1 by course) references."""
    import export_sun_flare, export_glare
    from import_sky import painter_sections
    from course_painters import point_tree
    irr = json.loads((ROOT / 'local/assets/native/IRRADIANCE/irradiance.json').read_text())
    sdb = [l['name'] for l in sdb_locations(SOURCE / 'bam.sdb')]
    names = {'APBR1', 'BPBR1', 'CPBR1', 'DPBR1', 'EPBR1'}  # 22E180 defaults (jump table 0x47B4E0)
    for code in codes:
        track = sdb.index(code); found = None
        for chunk_index, chunk in enumerate(world_chunks(SOURCE / 'bam.ssb')):
            for kind, t, rid, data in records(chunk):
                if kind == 15 and t == track and len(data) >= 64:
                    if found:
                        raise ValueError(f'{code}: more than one world painter record')
                    found = (chunk_index, rid, data)
        lighting = None
        if found and 11 in painter_sections(found[2]):
            section = painter_sections(found[2])[11]
            header, count, _ = struct.unpack_from('<3I', section)
            if header != 12 + 8 * count:
                raise ValueError(f'{code}: Lighting table header changed')
            entries = []
            for i in range(count):
                typ, at = struct.unpack_from('<2I', section, 12 + i * 8)
                if typ != 11:
                    raise ValueError(f'{code}: Lighting payload type {typ}')
                rate = struct.unpack_from('<f', section, at)[0]
                refs = [section[at + 4 + j * 8:at + 12 + j * 8].rstrip(b'\0').decode('ascii') for j in range(4)]
                if any(r not in irr['records'] for r in refs[:3]):
                    raise ValueError(f'{code}: Lighting reference without an IRR bank {refs}')
                entries.append(dict(index=i, blend_rate=rate, references=refs, scalars=list(struct.unpack_from('<2f', section, at + 36))))
                names.update(refs[:3])
            lighting = dict(version=1, location=code, chunk=found[0], track=track, rid=found[1], section_sha256=hashlib.sha256(section).hexdigest(),
                            entries=entries, tree=point_tree(section, len(entries)))
        (WEB / code / 'lighting.json').write_text(json.dumps(dict(version=1, location=code, painter=lighting), separators=(',', ':')))
        sun = export_sun_flare.sun_painter(ROOT, code)
        (WEB / code / 'sun-painter.json').write_text(json.dumps(dict(version=1, location=code, painter=sun), separators=(',', ':')))
        glare = export_glare.glare_package(code)
        (WEB / code / 'glare-painter.json').write_text(json.dumps(glare, separators=(',', ':')))
        print(code, 'painters: lighting', lighting and len(lighting['entries']), 'sun', sun and len(sun['payloads']), 'glare', glare['painter'] and len(glare['painter']['payloads']))
    banks = {n: irr['records'][n]['rows'] for n in sorted(names)}
    (WEB / 'lighting-banks.json').write_text(json.dumps(dict(version=1, source_sha256=irr['ps2_sha256'], banks=banks,
        defaults={str(c): n + 'PBR1' for c, n in enumerate('ABCDEADEACEBCEADEABCDE')}), separators=(',', ':')))


SETPIECE_COURSES = PEAKS[1]['setpiece_courses']  # event packages with set-piece exports (web/prepare.py); configure() per peak


def setpiece_sources():
    """(code, {kind: document}) of every event package's set-piece exports: flags (FLAGS/flags.json), uv (UVSCROLL/uv-scroll.json),
    livecomp (LIVECOMP/livecomp.json), particles (PARTICLES/particles.json), stage (STAGE/stage-world.json). Snow Jam keeps its
    FLAGS / UVSCROLL / LIVECOMP at the asset root."""
    assets = ROOT / 'web/public/assets'
    for code in SETPIECE_COURSES:
        base = assets if code == 'ARA1' else assets / code
        paths = dict(flags=base / 'FLAGS/flags.json', uv=base / 'UVSCROLL/uv-scroll.json', livecomp=base / 'LIVECOMP/livecomp.json',
                     particles=assets / code / 'PARTICLES/particles.json', stage=assets / code / 'STAGE/stage-world.json')
        yield code, {k: json.loads(p.read_text()) for k, p in paths.items() if p.exists()}


def setpiece_packages():
    """PEAK1/SETPIECES/: the stage world of the streamed world (web/peak-set-pieces.js, web/stage_world.inc init_stage_world with
    location PEAK1). One stage context holds every resident location in the original (one set of entity lists for all tracks), and
    resource ids carry the track, so the event packages' per-instance exports merge by resource. What a race event seeds from its
    ready savestate (effects alive at race tick 0, crowd slots, magnets, halos, one-way volumes, MultiParticle groups, flag grids,
    start-gate GO) is left out: in free ride the section activation builds those entities when their section is entered.
    Covered: the course tracks and the connectors of the event residencies (their stage-world script/MeshAnim lists); the hubs
    A / B and DRA4_A have no event export (gap, docs/peak-mountain.md)."""
    out = WEB / 'SETPIECES'; out.mkdir(parents=True, exist_ok=True)
    flags = dict(version=1, location=NAME, coordinate_system=None, fps=60, wind=None, sine_table=None, cloths=[], instances=[])
    uv = dict(version=1, location=NAME, fps=60, instances=[])
    live = dict(version=1, location=NAME, fps=60, go_program=None, go_tick=-1, instances=[], program_starts=[])
    particles = dict(version=1, location=NAME, coordinate_system=None, engine='engine/set_piece_particles.hpp', fields=None, blend_table_0x44B420=None,
                     programs=[], blocks=[], initial=None, instances={}, carriers={}, multi=[], magnets=[], halos=[], crowd=None)
    stage = dict(version=1, location=NAME, meshanim=[], script=[], teleports=[], collections={}, calls={})
    seen = set()
    for code, docs in setpiece_sources():
        if 'flags' in docs:
            f = docs['flags']; base = len(flags['cloths'])
            for k in ('coordinate_system', 'wind', 'sine_table', 'fps'):
                if flags.get(k) is None: flags[k] = f[k]
                elif k in ('wind', 'sine_table') and flags[k] != f[k]: raise ValueError(f'{code}: flag {k} differs between courses')
            flags['cloths'] += f['cloths']
            flags['instances'] += [dict(x, cloth=x['cloth'] + base, course=code) for x in f['instances'] if ('flag', x['resource']) not in seen]
            seen.update(('flag', x['resource']) for x in f['instances'])
        if 'uv' in docs:
            uv['instances'] += [dict(x, course=code) for x in docs['uv']['instances'] if ('uv', x['resource']) not in seen]
            seen.update(('uv', x['resource']) for x in docs['uv']['instances'])
        if 'livecomp' in docs:
            L = docs['livecomp']
            # the race GO program (stage global handler 2: the start-gate doors) does not run in free ride
            keep = [x for x in L['instances'] if ('live', x['resource']) not in seen]
            for x in keep:
                x = dict(x, course=code, starts=[s for s in x.get('starts', []) if s.get('trigger') != 'go'])
                live['instances'].append(x)
            live['program_starts'] += [s for s in L.get('program_starts', []) if s.get('trigger') != 'go']
            seen.update(('live', x['resource']) for x in L['instances'])
        if 'particles' in docs:
            P = docs['particles']
            for k in ('coordinate_system', 'fields', 'blend_table_0x44B420'):
                particles[k] = particles[k] or P[k]
            if particles['initial'] is None:  # the visual stream's start words (the snow-seed convention of the event packages)
                particles['initial'] = dict(savestate=None, visual_random_0x4FF018=P['initial']['visual_random_0x4FF018'], effects=[])
            particles['programs'] += [dict(p, course=code) for p in P['programs']]
            particles['blocks'] += P['blocks']
            particles['instances'].update(P['instances']); particles['carriers'].update(P['carriers'])
            # MultiParticle groups (builtin105 from the locations' global programs at load, 4 slots); members join by builtin106
            # as their sections are entered, so the groups start empty here.
            for g in P.get('multi', []):
                old = next((m for m in particles['multi'] if m['group'] == g['group']), None)
                if old is None: particles['multi'].append(dict(g, members=[], course=code))
                elif old['emitter'] != g['emitter'] or old['block'] != g['block']: print(f'WARNING {code}: MultiParticle group {g["group"]} differs from {old["course"]}; kept the first')
        if 'stage' in docs:
            S = docs['stage']
            for k in ('meshanim', 'script', 'teleports'):
                stage[k] += [dict(x, course=code) for x in S[k] if (k, x['resource']) not in seen]
                seen.update((k, x['resource']) for x in S[k])
            for key, rows in S['collections'].items():
                stage['collections'][f'{code}:{key}'] = rows
    # Every location's stage lists from the Peak 1 stage tables (hubs and connectors included), and the LiveComp / UV-scroll
    # instances of the tracks without an event export.
    locs, courses = elf_tables(); codes = [locs[i]['code'] for i in peak_locations(locs, courses)]
    stage, extra_live, extra_uv = peak_stage_setpieces(codes)
    have = {x['resource'] for x in live['instances']}; live['instances'] += [x for x in extra_live if x['resource'] not in have]
    have = {x['resource'] for x in uv['instances']}; uv['instances'] += [x for x in extra_uv if x['resource'] not in have]
    (out / 'flags.json').write_text(json.dumps(flags, separators=(',', ':')))
    (out / 'uv-scroll.json').write_text(json.dumps(uv, separators=(',', ':')))
    (out / 'livecomp.json').write_text(json.dumps(live, separators=(',', ':')))
    (out / 'particles.json').write_text(json.dumps(particles, separators=(',', ':')))
    (out / 'stage-world.json').write_text(json.dumps(stage, separators=(',', ':')))
    print('setpieces: flags', len(flags['instances']), 'uv', len(uv['instances']), 'livecomp', len(live['instances']),
          'particle owners', len(particles['instances']), 'meshanim', len(stage['meshanim']), 'script', len(stage['script']))
    return dict(flags=flags, uv=uv, livecomp=live, particles=particles, stage=stage)


def setpiece_batches(merged):
    """spatial_batches inputs from the merged set-piece exports (the same rules as web/prepare.py): flag instances' static batches
    hidden (the cloth redraws them), UV-scroll groups by first appearance of a distinct initial state (web/peak-set-pieces.js
    numbers them the same way), LiveComp / MeshAnim meshes split per node, script-changed instances and collectibles own batches."""
    if merged is None:
        return {}
    groups, keys = {}, {}
    for x in merged['uv']['instances']:
        groups[x['resource']] = keys.setdefault(json.dumps(x['initial'], sort_keys=True), len(keys))
    live = {(x['resource'], m): node for x in merged['livecomp']['instances'] for m, node in enumerate(x.get('meshNodes') or [])}
    meshanim = {(m['resource'], mesh): node for m in merged['stage']['meshanim'] for mesh, node in enumerate(m['meshNodes'])}
    collectibles = {x['resource'] for rows in merged['stage']['collections'].values() for x in rows}
    return dict(flags={x['resource'] for x in merged['flags']['instances']}, scroll_groups=groups, livecomp_nodes=live, meshanim_nodes=meshanim,
                script_resources={x['resource'] for x in merged['stage']['script']} | collectibles)


def peak_stage_setpieces(codes):
    """Stage-world lists straight from the Peak 1 stage tables (web/generated/peak_stage_seed.hpp, every location's programs), with
    the decoder of tools/export_stage_world.py: script-changed instances (builtins 1/2/13/29/58), MeshAnim models (13), teleports
    (34), collectible lists (38) for EVERY location (the hubs and connectors too), and the LiveComp (builtin 3) / UV-scroll (21)
    instances of the tracks the event exports do not cover (their own-track exporters skip the connectors and hubs)."""
    import export_stage_world as E
    from export_rail_teeters import parse_model
    from export_livecomp import mesh_nodes
    from export_flags import bits, from_bits, mul, div_nearest
    E.SEED, namespace = stage_seed()
    stages, programs, words, globals_, handlers = E.seed(namespace)
    world = {}
    for code in codes:
        for i in json.loads((WEB / code / 'world_collision.json').read_text())['instances']:
            world[(i['rid'] << 8) | i['track']] = i
    stage_of = {st[0]: st for st in stages}
    runs = []
    for res, slots in handlers:
        st = stage_of.get(res & 255)
        if st:
            runs += [(st[1] + prog, res, slot) for slot, prog in enumerate(slots) if prog >= 0]
    for st in stages:
        runs += [(st[1] + globals_[st[3] + g], None, 'global') for g in range(st[4]) if globals_[st[3] + g] >= 0]
    elf = ELF.read_bytes(); u = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    live_types = [u(0x445F48 + 4 * k) for k in range(11)]; uv_types = [u(0x446238 + 4 * k) for k in range(13)]
    step = from_bits(u(0x4A30F0 - 0x3384))
    covered = {i for i, l in enumerate(sdb_locations(SOURCE / 'bam.sdb')) if l['name'] in SETPIECE_COURSES}
    TRIGGER = {1: 'section', 2: 'contact', 4: 'done', 5: 'tick'}
    script, meshanim_t, teleports, collections, live, uv = set(), {}, {}, {}, {}, {}
    models = {}

    def model(key):
        if key not in models:
            for chunk in world_chunks(SOURCE / 'bam.ssb'):
                for kind, track, rid, data in records(chunk):
                    if kind == 2 and ((rid << 8) | track) not in models:
                        models[(rid << 8) | track] = data
        return models[key]

    def fill(defaults, keys, types):
        w = list(defaults)
        for k, (t, v) in keys.items():
            if v is None or k >= len(w): continue
            w[k] = v if t == 2 or types[k] != 2 else bits(float(struct.unpack('<i', struct.pack('<I', v & 0xFFFFFFFF))[0]))
        return w
    for gp, owner, slot in runs:
        first, count = programs[gp]
        calls = E.calls_of(words, first, count, owner)
        spline = any(b in (19, 20) for b, _, _ in calls)
        for builtin, keys, current in calls:
            if builtin == 38:
                cid = keys.get(0, (1, None))[1]
                track = next(st[0] for st in stages if st[1] <= gp < st[1] + st[2])  # the stage whose global program builds the list
                collections.setdefault(f'{cid}:{track}', []).extend(v for k, (t, v) in sorted(keys.items()) if 1 <= k <= 20 and v not in (None, 0xFFFFFFFF))
                continue
            key0 = keys.get(0, (1, 0xFFFFFFFF))[1]
            if key0 is None: continue
            target = current if key0 == 0xFFFFFFFF else key0
            if target is None or target not in world: continue
            if builtin in E.SCRIPT_BUILTINS:
                script.add(target)
                if builtin == 13: meshanim_t[target] = world[target]
            elif builtin == 34: teleports[target] = world[target]
            elif builtin == 3 and (target & 255) not in covered and slot in TRIGGER and not spline:
                w = fill([0xFFFFFFFF, 1, 0, bits(-1), bits(-1), bits(30), bits(0), bits(-1), 0, 0, 0], keys, live_types)
                inst = world[target]; data = model(inst['model_resource']); m = parse_model(data)
                if not any(n['track'] for n in m['nodes']): continue
                fb = lambda x: struct.unpack('<f', struct.pack('<I', x))[0]
                nodes = [dict(parent=n['parent'], bind=[fb(x) for x in n['bind']],
                              track=None if not n['track'] else dict(base=[fb(x) for x in n['track']['base']], mask=n['track']['mask'],
                                                                     curves=[[[fb(x) for x in seg] for seg in c] for c in n['track']['curves']]))
                         for n in m['nodes']]
                e = live.setdefault(target, dict(resource=target, name=inst['name'], model=inst['model_resource'], length=fb(m['length']), nodes=nodes,
                                                 matrix=inst['matrix'], scale=inst['scale'], authoredFlags=0, draw=None, meshNodes=mesh_nodes(data), starts=[], course=NAME))
                e['starts'].append(dict(program=gp, trigger=TRIGGER[slot], owner=world.get(owner, {}).get('name'), ownerResource=owner, guard=None, words=w))
            elif builtin == 21 and (target & 255) not in covered:
                w = fill([0xFFFFFFFF, 5, bits(0), bits(0), bits(step), bits(step), bits(1), bits(0), bits(0), bits(0), bits(0), bits(1), 0], keys, uv_types)
                r = lambda k: from_bits(w[k])
                uv[target] = dict(resource=target, name=world[target]['name'], program=gp, slot=slot, draw=None, words=w, course=NAME,
                                  initial=dict(mode=w[1], timer=0.0, onTime=r(6), offTime=r(7), angle=0.0, spin=mul(r(8), div_nearest(1.0, 60.0)),
                                               axis=[r(9), r(10), r(11), 0.0], u=r(2), v=r(3), stepU=r(4), stepV=r(5), active=1))
    fbits = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
    meshanim = []
    for res, inst in sorted(meshanim_t.items()):
        data = model(inst['model_resource']); m = parse_model(data)
        meshanim.append(dict(resource=res, name=inst['name'], model=inst['model_resource'], matrix_bits=[fbits(x) for x in inst['matrix']], scale_bits=fbits(inst['scale']),
                             nodes=[dict(parent=n['parent'], local_bits=n['bind']) for n in m['nodes']], meshNodes=mesh_nodes(data), runtime_flags=None, draw=None))
    stage = dict(version=1, location=NAME, source=str(stage_seed()[0].relative_to(ROOT)), meshanim=meshanim,
                 script=[dict(resource=r, name=world[r]['name'], runtime_flags=None, draw=None) for r in sorted(script)],
                 teleports=[dict(resource=r, name=i['name'], matrix_bits=[fbits(x) for x in i['matrix']]) for r, i in sorted(teleports.items())],
                 collections={k: [dict(index=i, resource=r, name=world.get(r, {}).get('name')) for i, r in enumerate(v)] for k, v in collections.items()}, calls={})
    return stage, sorted(live.values(), key=lambda x: x['resource']), sorted(uv.values(), key=lambda x: x['resource'])


def load_setpieces():
    root = WEB / 'SETPIECES'
    if not (root / 'livecomp.json').exists():
        return None
    return dict(flags=json.loads((root / 'flags.json').read_text()), uv=json.loads((root / 'uv-scroll.json').read_text()),
                livecomp=json.loads((root / 'livecomp.json').read_text()), stage=json.loads((root / 'stage-world.json').read_text()))


def light_glow_package(codes):
    """PEAK1/LIGHT_GLOW/: the kind-7 light glow sources of every Peak 1 location (tools/export_light_glow.py record decode), each
    with its SDB track; web/light-glow.js draws the ones of the drawn locations (the type-8 entities of the resident cells). The
    textures and constants are the Snow Jam package's (the same FX 53/54 and ELF values)."""
    base = ROOT / 'web/public/assets/LIGHT_GLOW'; out = WEB / 'LIGHT_GLOW'; out.mkdir(parents=True, exist_ok=True)
    pkg = json.loads((base / 'light-glow.json').read_text())
    for t in pkg['textures'].values():
        shutil.copy2(base / t['file'], out / t['file'])
    tracks = {i for i, l in enumerate(sdb_locations(SOURCE / 'bam.sdb')) if l['name'] in codes}
    lights = []
    for chunk_index, chunk in enumerate(world_chunks(SOURCE / 'bam.ssb')):
        for kind, track, rid, data in records(chunk):
            if kind != 7 or track not in tracks:
                continue
            flags = struct.unpack_from('<I', data, 12)[0]
            if len(data) != 80 or flags & 0x70 not in (0x10, 0x20, 0x40) or flags & ~0x70:
                raise ValueError(f'Unsupported light glow record track {track} rid {rid}')
            lights.append(dict(chunk=chunk_index, track=track, rid=rid, flags=flags, colour=list(struct.unpack_from('<3f', data, 16)),
                               position=list(struct.unpack_from('<3f', data, 28)), bounds_min=list(struct.unpack_from('<3f', data, 40)),
                               bounds_max=list(struct.unpack_from('<3f', data, 52))))
    pkg.update(location=NAME, lights=lights)
    (out / 'light-glow.json').write_text(json.dumps(pkg, indent=1) + '\n')
    print('light glow sources', len(lights), {t: sum(1 for l in lights if l['track'] == t) for t in sorted({l['track'] for l in lights})})


def bounds(world):
    return world['bounds']  # native metres (x, z, -y of the source)


def manifest(codes):
    locs, courses = elf_tables()
    sdb = [l['name'] for l in sdb_locations(SOURCE / 'bam.sdb')]
    by_code = {v['code']: k for k, v in locs.items()}
    out = []
    for code in codes:
        entry = dict(code=code, id=by_code[code], track=sdb.index(code), kind=locs[by_code[code]]['kind'], root=f'/assets/{NAME}/{code}/')
        world = WEB / code / 'world.json'
        if world.exists():
            w = json.loads(world.read_text()); entry['bounds_m'] = bounds(w)
            entry['files'] = dict(fog=(WEB / code / 'fog-tree.json').exists())
            entry['bytes'] = sum(p.stat().st_size for p in (WEB / code).iterdir() if p.is_file())
        out.append(entry)
    rows = [dict(course=c['index'], name=c['name'], code=c['code'], peak=c['peak'] + 1, kind=c['kind'], station=c['station'], map_id=c['field5c'], region=c['region'],
                 sky=locs[c['residency']['sky']]['code'] if c['residency']['sky'] >= 0 else None,
                 locations=[locs[i]['code'] for i in c['residency']['locations']]) for c in courses if c['peak'] == PEAK and c['code'] != 'dbg']
    # Streaming rows (0x442168) of every location a Peak 1 residency row wants, with the disc read of its main chunk
    # (desc+0x18 = the location's last SDB chunk): measured on the PS2 (S/peak1/streaming.md) or, when not measured,
    # the chunk size at 20,500 bytes per tick (the measured reads run 17,800 .. 21,600).
    MEASURED = dict(A=75, A_ABA1=23, A_ARA1=36, A_ASS1=21, DRA4_A=25, ARA1=239, ARA1_B=21, BSKY=30, B=57, B_BHP1=34, B_BRA2=27)
    if PEAK != 0: MEASURED = dict(BSKY=30)  # other peaks: unmeasured (chunk size at 20.5 KB/tick; skies take the BSKY read)
    sdb_locs = sdb_locations(SOURCE / 'bam.sdb'); chunk_sizes = [len(c) for c in world_chunks(SOURCE / 'bam.ssb')]
    wanted = set(codes) | {locs[c['residency']['sky']]['code'] for c in courses if c['peak'] == PEAK and c['residency']['sky'] >= 0} | {'TRANSP'}
    streaming = []
    for code in sorted(wanted, key=lambda c: by_code[c]):
        k = sdb.index(code); chunk = sdb_locs[k]['chunk_end']; lk = locs[by_code[code]]['kind']
        streaming.append(dict(id=by_code[code], code=code, track=k if lk not in (3, 4) else -1, **{'class': 1 if lk == 3 else 2 if lk == 4 else 0},
                              chunk=chunk, read_ticks=MEASURED.get(code, MEASURED['BSKY'] if lk == 4 else max(1, round(chunk_sizes[chunk] / 20500))), measured=code in MEASURED))  # skies: the measured BSKY read (same size)
    document = dict(version=1, peak=PEAK + 1, name=NAME, streaming=streaming, source='SLUS_207.72 tables 0x43E250 (locations), 0x43D950 (courses), 0x442488 (residency), 0x442820 (course kinds)',
                    location_ids={v['code']: k for k, v in locs.items()}, sdb_tracks={n: i for i, n in enumerate(sdb)},
                    locations=out, residency=rows)
    WEB.mkdir(parents=True, exist_ok=True)
    (WEB / 'peak.json').write_text(json.dumps(document, indent=1) + '\n')
    return document


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--only', nargs='*'); p.add_argument('--force', action='store_true'); p.add_argument('--manifest-only', action='store_true')
    p.add_argument('--painters-only', action='store_true', help='only the per-location painter packages (lighting/sun/glare)')
    p.add_argument('--peak', type=int, default=1, choices=[1, 2, 3], help='the peak to export (PEAK<N>; 1 keeps the historical Peak 1 run)')
    p.add_argument('--no-setpieces', action='store_true', help='skip the merged set pieces (first run of a new peak: the stage seed needs the packages)')
    p.add_argument('--batches-only', action='store_true', help='only re-split the draw batches of the existing location packages (world.json batches, indices.bin, vertex-alpha.bin)')
    p.add_argument('--out', help='with --batches-only: write the re-split packages and SETPIECES/attached.json under this folder (e.g. scratch), not web/public/assets')
    a = p.parse_args()
    configure(a.peak)
    global OUT
    if a.out:
        if not a.batches_only: raise SystemExit('--out needs --batches-only')
        OUT = Path(a.out).resolve() / NAME
    locs, courses = elf_tables()
    codes = [locs[i]['code'] for i in peak_locations(locs, courses)]
    if a.painters_only:
        painter_packages(codes)
        return
    if a.batches_only:   # e.g. after a web/world-batches.py change: --peak 2 --only CRA3 --batches-only
        attached_package(OUT or WEB)
        for code in (a.only or codes):
            web_package(code, batches_only=True); print(f'{code}: batches re-split', flush=True)
        return
    if not a.manifest_only:
        if not a.no_setpieces:
            setpiece_packages()  # the merged event-package set pieces (the location packages split their batches by them)
            attached_package(WEB)
        for code in (a.only or codes):
            if code not in codes:
                raise SystemExit(f'{code} is not a {NAME} location ({", ".join(codes)})')
            native_import(code, a.force)
            web_package(code)
            print(f'{code}: packaged', flush=True)
        # rails.json is copied from the native import (disc flags): the runtime query flags / surfaces come from the PS2 states
        run('tools/export_peak_rail_flags.py', '--peak', str(a.peak), '--only', *(a.only or codes))
    environment_package(codes)
    painter_packages(codes)
    light_glow_package(codes)
    paths_packages(codes)
    m = manifest(codes)
    print(json.dumps([(l['code'], l['id'], l['track'], l.get('bytes')) for l in m['locations']]))


if __name__ == '__main__':
    main()
