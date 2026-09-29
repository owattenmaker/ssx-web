#!/usr/bin/env python3
"""Audit which world data is live in the Snow Jam RACE EVENT (not free-ride).

Ground truth is the original's own memory in event savestates. Output:
local/event-activation/event-membership.json (see docs/obstacle-collision.md,
"Snow Jam event world membership").

Recovered structures (SLUS_207.72):
* ELF location table 0x43E250 (24-byte entries {id, name[16], kind}; 144D38/144D50):
  kind 0 event course, 1 peak hub, 2 connector, 3 TRANSP, 4 sky.
* Streaming table 0x442168 (50 x 16 bytes, built by 22CD40, driven by the loader
  state machine 22D8D8): {location id, SDB location index (= resource track),
  state (2 = resident), kind}.
* World manager W = **(gp+0x16C8): W+8 per-track resource registry, W+0x24+8*track
  location state (6 = active), W+0x3F0+24*chunk sub-chunk state (3 = resident).
* Octree (world = *(*(*(gp-0x848)+0x84)+0x20), 8 roots of 20 bytes, node+0x20
  instances, +0x24 terrain patches, +0x28 entities, children +0..+0x1C).
* Static draw collector 22A5A0 (instances): (flags & 3) == 3, instance+0x7D track /
  +0x7E chunk must be active/resident, then the frustum test. 22A698 (patches):
  patch+0x156 >= 0 and patch+0x155/+0x156 active/resident.
* Dynamic draw: flag 0x100 puts the entity in the renderer's dynamic list; its
  vtable slot 0x20 draws. 0x356298 (Object 0x490E80, type-1 0x490B10, type-13
  0x48EE60) needs instance flags & 4 and the chunk resident; DeadNode 0x491B00,
  RestoreNode 0x491680 and type-16 0x491800 use the empty 0x360790; type 10
  (0x48FC10, flags) uses the empty 0x34AF18 (cloth flags are drawn by cFlagManager).
"""
import hashlib, json, re, struct, sys, zipfile
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import EXPECTED_SHA1  # noqa: E402
from world_assets import world_resource_names, world_chunks, records, locations  # noqa: E402

GP = 0x4A30F0
PRIMARY = ROOT / 'local/reference/pcsx2/countdown-1.p2m2_SaveState.p2s'   # the audited countdown (export_event_instances.py)
STATES = [ROOT / 'local/reference/pcsx2' / n for n in [
    'snow-jam-countdown-anchor.p2s', 'snow-jam-countdown-1.p2s', 'snow-jam-ready.p2s', 'snow-jam-glide.p2s',
    'snow-jam-glide-120.p2s', 'snow-jam-600.p2s', 'snow-jam-turn-left-long-240.p2s', 'snow-jam-air-isolated-120.p2s']] + [
    ROOT / f'local/ps2-capture/runs/bag/carve-bag.tick{t}.p2s' for t in (600, 607, 738)]
EMPTY_DRAW = {0x360790, 0x34AF18}
OBJECT_DRAW = 0x356298


class Memory:
    def __init__(self, path):
        self.path = Path(path)
        with zipfile.ZipFile(path) as z:
            self.ee = z.read('eeMemory.bin')
        self.sha256 = hashlib.sha256(self.ee).hexdigest()

    def u(self, a): return struct.unpack_from('<I', self.ee, a & 0x1FFFFFF)[0]
    def h(self, a): return struct.unpack_from('<h', self.ee, a & 0x1FFFFFF)[0]
    def b(self, a): return self.ee[a & 0x1FFFFFF]


def elf_reader():
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest() != EXPECTED_SHA1:
        raise ValueError('Unexpected executable')
    ph = struct.unpack_from('<I', elf, 28)[0]; stride, count = struct.unpack_from('<HH', elf, 42)

    def read(address, size):
        for i in range(count):
            typ, offset, base, _, length, _, _, _ = struct.unpack_from('<8I', elf, ph + i * stride)
            if typ == 1 and base <= address and address + size <= base + length:
                return elf[offset + address - base:offset + address - base + size]
        raise ValueError('Unmapped ELF address')
    return elf, read


def location_table(read):
    rows = []
    for i in range(50):
        entry = read(0x43E250 + 24 * i, 24)
        ident, kind = struct.unpack_from('<I', entry)[0], struct.unpack_from('<I', entry, 20)[0]
        rows.append(dict(id=ident, name=entry[4:20].split(b'\0')[0].decode(), kind=kind))
    return rows


def vtable_draw(read, vtable):
    return struct.unpack_from('<I', read(vtable + 0x20, 8), 4)[0]


def octree(m):
    world = m.u(m.u(m.u(GP - 0x848) + 0x84) + 0x20)
    patches, instances, entities = [], [], []
    seen = set()

    def walk(node):
        if not node or node in seen:
            return
        seen.add(node)
        p = m.u(node + 0x24)
        while p:
            patches.append(p); p = m.u(p)
        i = m.u(node + 0x20)
        while i:
            instances.append(i); i = m.u(i)
        e = m.u(node + 0x28)
        while e:
            entities.append(e); e = m.u(e)
        for c in range(8):
            walk(m.u(node + 4 * c))
    for slot in range(8):
        walk(m.u(world + slot * 20 + 16))
    return patches, instances, entities


def draw_class(read, flags, vtable):
    static = (flags & 3) == 3
    if flags & 0x100 and vtable:
        slot = vtable_draw(read, vtable)
        if slot == OBJECT_DRAW:
            return 'entity' if flags & 4 else ('static' if static else 'none')
        if vtable == 0x48FC10:
            return 'flag_manager'
        if slot in EMPTY_DRAW:
            return 'static' if static else 'none'
        return f'entity_draw_{slot:#x}'
    if vtable == 0x48FC10:
        return 'flag_manager'
    return 'static' if static else 'none'


def snapshot(m, read, names):
    manager = m.u(m.u(GP + 0x16C8))
    streaming = []
    for i in range(50):
        ident, track, state, kind = struct.unpack_from('<4i', m.ee, 0x442168 + 16 * i)
        streaming.append(dict(id=ident, track=track, state=state, kind=kind))
    location_state = {t: m.u(manager + 0x24 + 8 * t) for t in range(64)}
    chunk_state = {c: m.u(manager + 0x3F0 + 24 * c) for c in range(160)}
    patches, instances, entities = octree(m)
    patch_rows = [dict(resource=m.u(p + 0x150), runtime_flags=struct.unpack_from('<H', m.ee, p + 0xA)[0], chunk=m.h(p + 0x156), track=m.b(p + 0x155)) for p in patches]
    inst_rows = []
    for a in instances:
        resource = m.u(a + 0x78); flags = m.u(a + 8); entity = m.u(a + 12)
        node_type = m.h(entity + 16) if entity else None; vtable = m.u(entity + 12) if entity else None
        track, rid = resource & 255, resource >> 8
        body = 'static' if flags & 0x20 else 'entity' if flags & 0x40 and entity else 'skip'
        inst_rows.append(dict(resource=resource, track=track, rid=rid, name=names[1].get((track, rid)), address=a,
                              runtime_flags=flags, entity=entity, node_type=node_type, node_vtable=vtable,
                              chunk=m.h(a + 0x7E), chunk_track=m.b(a + 0x7D), model=m.u(a + 0x80),
                              draw=draw_class(read, flags, vtable), body_route=body))
    entity_types = Counter(m.u(e + 8) for e in entities)
    return dict(manager=manager, streaming=streaming, location_state=location_state, chunk_state=chunk_state,
                patches=patch_rows, instances=inst_rows, entities=entities, entity_types=entity_types)


def main():
    elf, read = elf_reader()
    if read(0x43E254, 5) != b'ARA1\0':
        raise ValueError('Location table moved')
    names = world_resource_names((ROOT / 'local/assets/source/ps2/bam.phm').read_bytes(), (ROOT / 'local/assets/source/ps2/bam.psm').read_bytes())
    locs = locations(ROOT / 'local/assets/source/ps2/bam.sdb')
    table = location_table(read)
    # Authored records of every location, for provenance (kind 1 patches, 3 instances, 7 lights, 8 rails).
    authored = defaultdict(lambda: defaultdict(set)); light_keys = {}; rail_keys = {}
    for index, chunk in enumerate(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb')):
        for kind, track, rid, data in records(chunk):
            if kind in (1, 3, 7, 8):
                authored[kind][track].add(rid)
            if kind == 7:
                light_keys[data[0x1C:0x28]] = (track, rid)
            if kind == 8:
                rail_keys[data[:0x1C]] = (track, rid, struct.unpack_from('<I', data, 0x20)[0])

    primary = Memory(PRIMARY)
    event = json.loads((ROOT / 'local/assets/native/ARA1/event-start.json').read_text())
    if primary.sha256 != event['provenance']['ee_sha256']:
        raise ValueError('Primary savestate is not the audited event countdown')
    base = snapshot(primary, read, names)

    resident = [dict(location=table[r['id']]['name'], kind=table[r['id']]['kind'], track=r['track'])
                for r in base['streaming'] if r['state'] == 2]
    resident_tracks = sorted(r['track'] for r in resident)
    for r in resident:
        if base['location_state'][r['track']] != 6:
            raise ValueError(f"Resident location {r['location']} is not active")
    if any(base['location_state'][t] == 6 for t in range(64) if t not in resident_tracks):
        raise ValueError('Active location outside the streaming residency')
    if [locs[r['track']]['name'] for r in resident] != [r['location'] for r in resident]:
        raise ValueError('SDB/ELF location order differs')

    patches_by_track = defaultdict(list)
    for p in base['patches']:
        patches_by_track[p['resource'] & 255].append(p)
    for track, rows in patches_by_track.items():
        if {p['resource'] >> 8 for p in rows} != authored[1][track]:
            raise ValueError(f'Track {track}: live patches differ from the authored set')
        if any(p['track'] != track for p in rows):
            raise ValueError('Patch track byte differs from its resource')
    instances_by_track = defaultdict(list)
    for i in base['instances']:
        instances_by_track[i['track']].append(i)
    runtime_created = [i for i in base['instances'] if i['track'] not in resident_tracks]
    for track in resident_tracks:
        live = {i['rid'] for i in instances_by_track.get(track, [])}
        if live != authored[3][track]:
            raise ValueError(f'Track {track}: live instances differ from the authored set')
    # Runtime-created instances share their owner's entity (e.g. the tram cabins of mdl_ARA1_tramlores).
    owner_of = {i['entity']: i for i in base['instances'] if i['track'] in resident_tracks and i['entity']}
    runtime_rows = []
    for i in runtime_created:
        owner = owner_of.get(i['entity'])
        runtime_rows.append(dict(resource=i['resource'], runtime_flags=i['runtime_flags'], entity=i['entity'], node_type=i['node_type'],
                                 node_vtable=i['node_vtable'], model=i['model'], owner_resource=owner['resource'] if owner else None,
                                 owner_name=owner['name'] if owner else None, draw=i['draw'], body_route=i['body_route']))
    # Entities: kind-7 lights and kind-8 rails by track.
    lights = Counter(); rails = Counter(); rail_runtime = []
    for e in base['entities']:
        typ = primary.u(e + 8)
        if typ == 8:
            key = primary.ee[e + 0x1C:e + 0x28]
            if key not in light_keys:
                raise ValueError('Unmatched light entity')
            lights[light_keys[key][0]] += 1
    rail_segments = 0
    for key, (track, rid, segment_count) in rail_keys.items():
        if track not in resident_tracks:
            continue
        at = primary.ee.find(key)
        found = []
        while at >= 0:
            if at % 4 == 0:
                found.append(at)
            at = primary.ee.find(key, at + 1)
        if len(found) != 1:
            raise ValueError(f'Rail {track}:{rid}: {len(found)} runtime records')
        rails[track] += 1; rail_segments += segment_count
        rail_runtime.append(dict(track=track, rid=rid, runtime_flags=primary.u(found[0] + 0x1C)))
    # Every rail segment is a type-1 entity of the octree (kind-8 constructor 0x341548).
    if base['entity_types'][1] != rail_segments:
        raise ValueError(f"{base['entity_types'][1]} type-1 entities for {rail_segments} resident rail segments")
    # Local (rider) lights: type-6 entities, all ARA1 kind-6 records but two (tools/export_light_tree.py);
    # the kind-6 records of the connectors are not in the event octree.
    local_lights = json.loads((ROOT / 'local/assets/native/ARA1/light-tree.json').read_text())
    if base['entity_types'][6] != local_lights['light_count']:
        raise ValueError('Type-6 entity count differs from the verified light index')
    # Painter objects actually constructed (ScreenTint vtable 0x484B70): only ARA1's record is instantiated.
    tint_objects = len([m for m in re.finditer(re.escape(struct.pack('<I', 0x484B70)), primary.ee) if m.start() % 4 == 0])

    # Membership must be identical in every event savestate (runtime flags may evolve: crashbags).
    consistency = []
    ref_patches = {p['resource'] for p in base['patches']}; ref_instances = {i['resource'] for i in base['instances']}
    for path in STATES:
        if not path.exists():
            continue
        m = Memory(path); s = snapshot(m, read, names)
        patches = {p['resource'] for p in s['patches']}; instances = {i['resource'] for i in s['instances']}
        changed = sorted((i['resource'], i['runtime_flags']) for i in s['instances']
                         if next((j for j in base['instances'] if j['resource'] == i['resource']), {}).get('runtime_flags') != i['runtime_flags'])
        consistency.append(dict(state=path.name, ee_sha256=m.sha256, patches_equal=patches == ref_patches, instances_equal=instances == ref_instances,
                                resident_equal=[r['state'] for r in s['streaming']] == [r['state'] for r in base['streaming']],
                                resident_chunks=sorted(c for c, v in s['chunk_state'].items() if v == 3),
                                runtime_flag_changes=[dict(resource=r, runtime_flags=f) for r, f in changed]))
        if patches != ref_patches or instances != ref_instances:
            raise ValueError(f'{path.name}: event membership differs from the countdown')

    # Browser package parity: the event set, as exported for the browser (tools/import_world.py --event,
    # import_rails.py, export_light_glow.py), must equal the live set.
    native = ROOT / 'local/assets/native/ARA1'
    terrain = {p['resource_id']: p for p in json.loads((native / 'terrain.json').read_text())['patches']}
    live_patches = {p['resource']: p for p in base['patches']}
    if set(terrain) != set(live_patches):
        raise ValueError('Browser terrain differs from the live event patches')
    if any(live_patches[r]['runtime_flags'] != terrain[r]['authored_flags'] | 0x40 for r in terrain):
        raise ValueError('Live patch flags differ from authored | 0x40')
    package_instances = {(i['rid'] << 8) | i['track'] for i in json.loads((native / 'world_collision.json').read_text())['instances']}
    course_instances = {i['resource'] for i in base['instances'] if i['track'] in (3, 8, 9)}
    if package_instances != course_instances:
        raise ValueError('Browser instances differ from the live course/connector instances')
    package_rails = {r['packed_id'] for r in json.loads((native / 'rails.json').read_text())['rails']}
    if package_rails != {(r['rid'] << 8) | r['track'] for r in rail_runtime}:
        raise ValueError('Browser rails differ from the live rails')
    glow = json.loads((ROOT / 'web/public/assets/LIGHT_GLOW/light-glow.json').read_text())['lights']
    if Counter(l['track'] for l in glow) != lights:
        raise ValueError('Browser glow sources differ from the live type-8 entities')
    parity = dict(terrain_patches=len(terrain), instances=len(package_instances), rails=len(package_rails), glow_lights=len(glow),
                  not_imported=dict(instances=sorted(i['resource'] for i in base['instances'] if i['track'] not in (3, 8, 9)),
                                    reason='TRANSP (track 0) models ~6.4 km below the course, ASKY SkyTop (sky.json draws the sky dome), '
                                           'runtime-created track-128 tram cabins (set-piece port)'))
    # Instances whose event state changes during the race (set pieces, breakables, proximity entities):
    # every runtime flag / draw / entity class seen across the full-race capture savestates
    # (local/ps2-capture/runs/setpieces/race.tick*.p2s) for the set-piece port.
    race_states = sorted((ROOT / 'local/ps2-capture/runs/setpieces').glob('race.tick*.p2s'), key=lambda f: int(re.search(r'tick(\d+)', f.name).group(1)))
    seen = defaultdict(list)
    for path in race_states:
        s = snapshot(Memory(path), read, names)
        tick = int(re.search(r'tick(\d+)', path.name).group(1))
        for i in s['instances']:
            seen[i['resource']].append((tick, i['runtime_flags'], i['draw'], i['node_type'], i['node_vtable']))
    dynamic = []
    for i in base['instances']:
        states = [(0, i['runtime_flags'], i['draw'], i['node_type'], i['node_vtable'])] + seen.get(i['resource'], [])
        if len({x[1:] for x in states}) > 1:
            dynamic.append(dict(resource=i['resource'], name=i['name'], states=[dict(tick=t, runtime_flags=f, draw=d, node_type=n, node_vtable=v) for t, f, d, n, v in states]))
    draw_counts = defaultdict(Counter)
    for i in base['instances']:
        draw_counts[i['track']][i['draw']] += 1
    result = dict(
        version=1, event='Snow Jam race (ARA1)', primary_state=PRIMARY.name, primary_ee_sha256=primary.sha256,
        elf_sha256=hashlib.sha256(elf).hexdigest(),
        rule=('Resident locations = streaming table 0x442168 state 2 (world manager location state 6): the event course, '
              'its two connectors and TRANSP/ASKY. Every authored patch/instance of a resident location is in the octree; '
              'event vs free-ride differences are runtime instance flags set at race load. Drawn: static (flags&3)==3 '
              '(22A5A0), entity (flag 0x100, draw 0x356298 and flags&4), flag_manager (type-10 cloth flags); '
              'chunk residency (W+0x3F0) only streams texture chunks by region and is not membership.'),
        resident_locations=resident, resident_tracks=resident_tracks,
        location_kinds={r['name']: r['kind'] for r in table},
        resident_chunks=sorted(c for c, v in base['chunk_state'].items() if v == 3),
        patches={str(t): len(v) for t, v in sorted(patches_by_track.items())},
        patch_chunks={str(t): sorted({p['chunk'] for p in v}) for t, v in sorted(patches_by_track.items())},
        patch_runtime_flags_equal_authored_or_0x40=True, browser_parity=parity,
        instances={str(t): len(v) for t, v in sorted(instances_by_track.items())},
        draw_counts={str(t): dict(v) for t, v in sorted(draw_counts.items())},
        instance_rows=[{k: v for k, v in i.items() if k != 'address'} for i in base['instances'] if i['track'] in resident_tracks],
        runtime_created_instances=runtime_rows,
        dynamic_instances=dict(states=[p.name for p in race_states], note='tick 0 = countdown; flags 0x100/0x200 are renderer/entity bookkeeping', instances=dynamic),
        light_entities={str(k): v for k, v in sorted(lights.items())},
        rails={str(k): v for k, v in sorted(rails.items())}, rail_segments=rail_segments, rail_runtime_flags=rail_runtime,
        local_lights=dict(count=local_lights['light_count'], tracks=[8], excluded_ara1=local_lights['excluded_resources'],
                          note='kind-6 records of A_ARA1/ARA1_B/TRANSP/ASKY are not in the event octree'),
        painters='Only the course painter record is instantiated (8 tWPIGD_ScreenTint objects = ARA1 payloads); connector painter records are resident data only',
        entity_types={str(k): v for k, v in sorted(base['entity_types'].items())},
        screen_tint_objects=tint_objects,
        consistency=consistency)
    output = ROOT / 'local/event-activation/event-membership.json'
    output.write_text(json.dumps(result, indent=1) + '\n')
    print(json.dumps(dict(output=str(output), resident=[(r['location'], r['track']) for r in resident], patches=result['patches'],
                          instances=result['instances'], draw=result['draw_counts'], runtime_created=len(runtime_rows),
                          lights=result['light_entities'], rails=result['rails'], screen_tint_objects=tint_objects,
                          states=[(c['state'], c['resident_chunks']) for c in consistency])))


if __name__ == '__main__':
    main()
