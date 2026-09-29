#!/usr/bin/env python3
"""Export a location's LiveComp animation players (builtin 3 on animated models) for the browser
(development export; --location, default ARA1: writes the git-ignored web/public/assets/LIVECOMP/livecomp.json,
other locations web/public/assets/<LOC>/LIVECOMP/livecomp.json, and, when PS2 snapshots exist,
livecomp-snapshots.json for web/test-livecomp-animation.mjs). Per-location stage/track/inputs:
tools/set_piece_location.py. Outside ARA1, programs that also attach a Spline/MultiSpline (builtin
19/20: the instance is moved by another port) are skipped like ARA1's 54/69/71 and listed in
`skipped_programs`; snapshot records carry the LiveComp object address (`object`) because streamed
sections rebuild their LiveComps.

Sources (asserted/derived here): stage kind-16 handler rows slot 1 (section activation) and
the stage global handler table (stage+0x5C, 5 entries; entry 2 = program 3, run at race GO)
-> LUN programs whose builtin 3 (0x2FBCB8 -> 0x341AA0) calls carry keyed arguments over the
defaults (0x4FB498; key types 0x445F48); target = key 0 or the program's instance. Only
targets whose model has animated nodes (node field 2) are exported; tram/raven programs
(69/71/54, other owners) are skipped. Model node tables/curves via export_rail_teeters.parse_model
(bit patterns); instance matrix/scale/flags from world_collision.json + the countdown audit.
Snapshots: every LiveComp entity (vtable 0x490B10 at obj+0x3C) of the race captures
(local/ps2-capture/runs/setpieces/race.tick*.p2s and any extra --snapshots globs):
object words +0x00..+0x2C, dirty bit (+0x42), node matrices at [obj+0x60].
"""
import argparse, glob, json, re, struct, sys, zipfile


def draw_class(row, name):
    """The countdown audit's draw class, except 'object': an owner whose entity is its LiveComp Object player (vtable 0x490B10)
    with instance flags & 4 is drawn by that player's 0x356298 while it lives, although the static collector skips it
    ((flags & 3) != 3). The Throne's summit flag pole (PS2 local/reference/pcsx2/the-throne-ready: the pole at the card's left;
    web/set-pieces-renderer.js, pv liveCompObject). The backcountry heli os609 is drawn from SETS/<LOC>HELI (pv bcHeli,
    docs/presentation.md 16) and keeps its class."""
    draw = row.get('draw')
    if draw == 'none' and row.get('node_vtable') == 0x490B10 and row.get('runtime_flags', 0) & 4 and 'os609' not in (name or ''):
        return 'object'
    return draw
from pathlib import Path
root = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(root / 'tools'))
from world_assets import world_chunks, records  # noqa: E402
from export_startfire import decode_program  # noqa: E402
from export_rail_teeters import parse_model  # noqa: E402
import world_models  # noqa: E402
from set_piece_location import Location, SNAPSHOTS, web_asset_dir  # noqa: E402
from locations import activation_dir  # noqa: E402

GP = 0x4A30F0
OWNED_ELSEWHERE = {54, 69, 71}   # raven spline (ravensplineanima), trams (other ports)


def fb(b): return struct.unpack('<f', struct.pack('<I', b))[0]
def bits(x): return struct.unpack('<I', struct.pack('<f', x))[0]


def mesh_nodes(data):
    """Node index of every mesh decode_model emits (collision-source 'mesh' index -> node):
    decode a copy with every other node's mesh header cleared."""
    count, table = struct.unpack_from('<2I', data, 4); owner = []
    for i in range(count):
        copy = bytearray(data)
        for j in range(count):
            if j != i: struct.pack_into('<I', copy, table + 16 * j + 4, 0)
        owner += [i] * len(world_models.decode_model(bytes(copy)))
    assert len(owner) == len(world_models.decode_model(data))
    return owner


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--location', default='ARA1')
    p.add_argument('--output', type=Path, default=None)
    p.add_argument('--snapshots', nargs='*', default=None)
    a = p.parse_args()
    loc = Location(a.location); track = loc.track; ara1 = a.location == 'ARA1'
    a.output = a.output or web_asset_dir(a.location, 'LIVECOMP')
    a.snapshots = a.snapshots or [SNAPSHOTS[a.location]]
    elf = (root / 'local/disc/SLUS_207.72').read_bytes()
    u = lambda addr: struct.unpack_from('<I', elf, addr - 0xFF000)[0]
    assert u(0x441F38 + 3 * 4) == 0x2FBCB8 and u(0x2FBE80) == 0x0C000000 | (0x341AA0 >> 2)
    key_types = [u(0x445F48 + 4 * k) for k in range(11)]
    world = loc.world
    programs = loc.programs
    stage, models = loc.stage, loc.models
    row_base = loc.rows[1]
    audit = {r['resource']: r for r in json.loads((activation_dir(a.location) / 'countdown-instances.json').read_text())['instances']}
    runtime = {r['resource']: r for r in json.loads((root / 'local/event-activation/runtime-instances.json').read_text())['instances']} if ara1 else {}
    descriptors = world['bindings'][str(track)]['descriptors']; by_rid = {i['rid']: i for i in world['instances'] if i['track'] == track}
    SLOT_TRIGGER = {1: 'section', 2: 'contact', 4: 'done', 5: 'tick'}
    sources = []   # (program, trigger, owner instance or None)
    for inst in world['instances']:
        if inst['track'] != track: continue
        r08 = descriptors[inst['collision_descriptor']]['resource08']
        if r08 == 0xFFFFFFFF or r08 & 255 != track: continue
        row = struct.unpack_from('<6I', stage, row_base + (r08 >> 8) * 24)
        for slot, trig in SLOT_TRIGGER.items():
            if row[slot] != 0xFFFFFFFF: sources.append((row[slot] >> 8, trig, inst))
    count, table = struct.unpack_from('<2I', stage, 0x10)
    assert (count, table) == (5, 0x5C) or not ara1
    go = struct.unpack_from(f'<{count}I', stage, table)[2] >> 8
    sources.append((go, 'go', None))
    out = {}
    starts = []   # every builtin-3 call: who starts which LiveComp, when
    skipped = set()
    for program, trigger, own in sources:
        if ara1 and program in OWNED_ELSEWHERE: continue
        try: calls = decode_program(programs[program])
        except KeyError: continue
        moved = set()   # builtin3 targets that the same program also moves with builtin19/20 (another port draws them)
        if not ara1 and any(c[0] in (19, 20) for c in calls):
            skipped.add(program); cur = own
            for builtin, _, keys in calls:
                if builtin == 44: cur = by_rid.get(keys[0][1] >> 8) if 0 in keys else cur
                if builtin in (19, 20): moved.add(None if cur is None else cur['rid'] if 0 not in keys or keys[0][0] != 'resource' else keys[0][1] >> 8)
            moved = {m for m in moved}
            targets3 = set(); cur = own
            for builtin, _, keys in calls:
                if builtin == 44: cur = by_rid.get(keys[0][1] >> 8) if 0 in keys else cur
                if builtin == 3: targets3.add(None if cur is None else cur['rid'] if 0 not in keys or keys[0][0] != 'resource' else keys[0][1] >> 8)
            if not targets3 - moved: continue   # every LiveComp of the program rides a spline (Crow's Nest program 42 also starts ospreytimer)
        current = own; guard = None
        for builtin, _, keys in calls:
            if builtin == 44:   # 0x302968: current instance (+0x290) = key 0
                current = by_rid.get(keys[0][1] >> 8) if 0 in keys else current
            elif builtin == 55:  # 0x3019C8: crossed key1/30 s on the current instance's LiveComp this tick (0x34EBE0)
                # A slot-5 ('tick') program re-runs every tick of its owner's LiveComp; each guarded block
                # fires on a different tick, when the current instance is still the owner (the op-2 jump
                # skips the block's builtin 44 on the other ticks).
                who = own if trigger == 'tick' else current
                guard = dict(instance=who['name'] if who else None, frames30=float(keys.get(1, ('f', -1.0))[1]))
            if builtin != 3: continue
            words = [0xFFFFFFFF, 1, 0, bits(-1), bits(-1), bits(30), bits(0), bits(-1), 0, 0, 0]
            for k, (kind, v) in keys.items():
                if kind == 'resource': words[k] = v
                elif kind == 'float': words[k] = bits(v)
                else: words[k] = bits(float(v)) if key_types[k] == 2 else v & 0xFFFFFFFF
            target = current if words[0] == 0xFFFFFFFF else by_rid.get(words[0] >> 8)
            if target is None or target['rid'] in moved: continue
            ownerResource = ((own['rid'] << 8) | track) if own else None
            starts.append(dict(program=program, trigger=trigger, owner=own['name'] if own else None, ownerResource=ownerResource, target=target['name'], guard=guard, mode=words[1]))
            model = parse_model(models[target['model_resource'] >> 8])
            if not any(n['track'] for n in model['nodes']): continue
            resource = (target['rid'] << 8) | track
            nodes = [dict(parent=n['parent'], bind=[fb(x) for x in n['bind']],
                          track=None if not n['track'] else dict(base=[fb(x) for x in n['track']['base']], mask=n['track']['mask'],
                                                                 curves=[[[fb(x) for x in seg] for seg in c] for c in n['track']['curves']]))
                     for n in model['nodes']]
            flags = runtime.get(resource, audit.get(resource, {})).get('authored_flags', 0)
            entry = out.setdefault(resource, dict(resource=resource, name=target['name'], model=target['model_resource'], length=fb(model['length']),
                                                  nodes=nodes, matrix=target['matrix'], scale=target['scale'], authoredFlags=flags,
                                                  draw=draw_class(audit.get(resource, {}), target['name']), meshNodes=mesh_nodes(models[target['model_resource'] >> 8]), starts=[]))
            entry['starts'].append(dict(program=program, trigger=trigger, owner=own['name'] if own else None, ownerResource=ownerResource, guard=guard, words=words))
    instances = sorted(out.values(), key=lambda x: x['resource'])
    # Lit instances: authored descriptor flag 0x40000000 is runtime flag 0x4000 (instance +8) in every countdown audit (296 of
    # 35,349 instances). The static-model draw 37E238 tests it (0x37E3B8) and calls 2F5148 -> 2F5400: the instance's light cache
    # row set is the Lighting painter's object bank (reference 3) plus local lights of a +-10 m query (2F5AF0); VU1 program 3
    # (0x8B8) evaluates it per vertex on the normal through the node's rotation, replacing the baked colour. PS2 grav-bb
    # tick1209 / 1230 RAM: both ERA5 crash billboards' cache entries (0xACC2D0 / 0xACC450, 0x180 bytes) hold EOBR1's ten rows
    # unchanged (no local light). web/set-pieces-renderer.js applies it (pv litLiveComp, web/world-material.js litWorldMaterial).
    lit = [x for x in instances if x['authoredFlags'] & 0x40000000]
    if lit:
        from export_cutscene_sets import lighting_bank
        bank = lighting_bank(a.location)
        for x in lit: x['lighting'] = bank
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output / 'livecomp.json').write_text(json.dumps(dict(
        version=1, fps=60, coordinate_system='Original source centimeters, Z-up, row vectors p * M (native = (x, z, -y)/100)',
        go_program=go, go_tick=181,
        go_note='Stage global handler 2 (program 3) builds the start-gate door LiveComps in browser/capture tick 181 '
                '(first tick after the race-phase transition at 180) and ticks them in the same tick: time 1/60 after 181, '
                'range end 1/6 s from 191, done flag from 192 (PS2 captures ticks 172..255).',
        instances=instances, program_starts=starts, **({} if ara1 else dict(location=a.location, skipped_programs=sorted(skipped)))), separators=(',', ':')))
    print(len(instances), 'LiveComp instances ->', a.output / 'livecomp.json')
    names = {x['resource'] for x in instances}
    by_address = {r['address']: r['resource'] for r in runtime.values()}
    records_out = []
    paths = sorted({p for g in a.snapshots for p in glob.glob(g)})
    for path in paths:
        with zipfile.ZipFile(path) as z: m = z.read('eeMemory.bin')
        U = lambda addr: struct.unpack_from('<I', m, addr & 0x1FFFFFF)[0]
        frame = U(U(U(U(GP - 0x848) + 0x84) + 0xC) + 8)
        arr = memoryview(m).cast('I')
        for i in range(0x100000 // 4, len(arr)):
            if arr[i] != 0x490B10: continue
            entity = i * 4 - 12; obj = entity - 0x30
            if ara1: res = by_address.get(U(entity + 0x18))
            else:
                ia = U(entity + 0x18)
                res = U(ia + 0x78) if 0x100000 < ia < 0x2000000 and U(ia + 0xC) == entity else None
            if res not in names: continue
            inst = out[res]; n = len(inst['nodes']); mats = U(obj + 0x60)
            records_out.append(dict(snapshot=Path(path).name, frame=frame, resource=res, name=inst['name'],
                                    head=[U(obj + 4 * k) for k in range(12)], dirty=(U(obj + 0x40) >> 16) & 1,
                                    matrices=[U(mats + 4 * k) for k in range(16 * n)], **({} if ara1 else dict(object=obj))))
    (a.output / 'livecomp-snapshots.json').write_text(json.dumps(dict(version=1, records=records_out), separators=(',', ':')))
    print(f'{len(records_out)} LiveComp snapshot records from {len(paths)} savestates')


if __name__ == '__main__':
    main()
