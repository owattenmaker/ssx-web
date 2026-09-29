#!/usr/bin/env python3
"""Moving set-piece seeds for locations other than Snow Jam (development export; run through
`tools/export_set_pieces.py --location LOC`). Writes the git-ignored
web/generated/set_piece_seed_<LOC>.hpp and local/event-activation/<LOC>/set-pieces.json.

MultiSplineModifier (engine/multi_spline_modifier.hpp; builtin20 0x2FE0C0 -> 0x355B30 -> ctor 0x359F88):
* resident at the load (The Junction's traffic: 8 modifiers on the anim/glow pairs) -> seeded from the
  race-tick-0 savestate (locations.state(LOC, 'ready')) like the Snow Jam chairlift;
* section-activated mid-race (Metro-City's mill bins) -> the construction-time state (distance d0 = 0,
  or the path length for a negative speed, then 0x35AC20 + bounds) computed by the verified port in
  tools/test_multi_spline_location.py (local/event-activation/<LOC>/multi-spline-construct-*.json;
  the construction state stepped N ticks equals the PS2 savestate bit for bit), plus the capture's
  construction tick and the human rider's remaining course distance then (activation proxy, like
  browserRavenSectionRemaining).
Every seed word is raw bits; car/clone arrays are flat (firstCar, count) because counts vary 1..6.
Clone resources are the runtime allocations of the capture (0x3F80, 0x3E80, ... pool order).

SplineModifier resident at the load (The Junction's blimp, program 39: loop, 10 km/h): the 0xF0-byte
modifier image of the ready state (+0xE0 = 0x90 * cursor, i.e. originalSplineModifierFromBytes(bytes,
&record, 0)). Triggered spline pieces (Metro-City dragons, program 245 on dragonTrig_1100 contact) use the
Snow Jam BrowserSplinePieceSeed layout from local/event-activation/<LOC>/spline-setpieces.json; slot-5
timer programs (EZrocketCore / rocketCore) are seeded too and launched by their timer program's builtin19.
"""
import hashlib, json, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from set_piece_location import Location  # noqa: E402
from locations import activation_dir, state as location_state  # noqa: E402
from export_startfire import decode_program  # noqa: E402

GP = 0x4A30F0
KMH = struct.unpack('<f', struct.pack('<I', 0x41DE38E4))[0]
# Full-course capture of the location's race event (activation evidence for section-streamed pieces).
CAPTURE = {'BRA2': ROOT / 'local/ps2-capture/runs/setpieces-bra2/full.bin', 'ASS1': ROOT / 'local/ps2-capture/runs/setpieces-ass1/full.bin',
           'ABA1': ROOT / 'local/ps2-capture/runs/setpieces-aba1/full.bin', 'ABC1': ROOT / 'local/ps2-capture/runs/setpieces-abc1/full.bin'}
CAPTURE.update({c: ROOT / f'local/ps2-capture/runs/peak2/{c.lower()}-full.bin' for c in ('CRA3', 'DRA4', 'DSS2', 'CBA2', 'CHP2', 'DBC2')})   # Peak 2 (docs/peak2.md)
CAPTURE.update({c: ROOT / f'local/ps2-capture/runs/peak3/{n}-full.bin' for c, n in {'ERA5': 'gravitude', 'ESS3': 'kick-doubt', 'EBA3': 'much-2-much', 'EHP3': 'perpendiculous', 'EBC3': 'the-throne'}.items()})   # Peak 3 (docs/peak3.md)


def f32bits(v): return struct.unpack('<I', struct.pack('<f', float(v)))[0]


def load(path):
    with zipfile.ZipFile(path) as z: return z.read('eeMemory.bin')


def tick_of(ee):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    return u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8)


def live_modifiers(ee, vtable):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    arr = memoryview(ee).cast('I'); out = []
    for i in range(0x100000 // 4, len(arr) - 8):
        if arr[i] not in (0x490E80, 0x490B10): continue
        e = i * 4 - 12; inst = u(e + 0x18)
        if not 0x100000 < inst < 0x2000000 or u(inst + 0xC) != e: continue
        c = u(e + 0x1C)
        if not 0x100000 < c < 0x2000000: continue
        m = u(c)
        if 0x100000 < m < 0x2000000 and u(m) == vtable: out.append((m, e, inst))
    return out


def script_calls(loc, builtin):
    """{target resource: (program, slot, owner name, keys)} of every handler-row call of `builtin`."""
    out = {}
    for inst, row in loc.handler_rows():
        for slot, w in enumerate(row):
            if w == 0xFFFFFFFF: continue
            try: calls = decode_program(loc.programs[w >> 8])
            except (KeyError, TypeError): continue
            current = inst
            for b, _, keys in calls:
                if b == 44 and 0 in keys: current = loc.by_rid.get(keys[0][1] >> 8, current)
                if b != builtin: continue
                target = loc.by_rid[keys[0][1] >> 8] if 0 in keys and keys[0][0] == 'resource' else current
                out[loc.resource(target)] = (w >> 8, slot, inst['name'], keys)
    return out


def multispline_fields(ee, m):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    return dict(count=u(m + 4), mode=u(m + 8), angle=u(m + 0xC), distance=u(m + 0x10), speed=u(m + 0x14), radius=u(m + 0x18),
                dirty=u(m + 0x30), path=u(m + 0x48), cursor=u(m + 0x4C), length=u(m + 0x54))


def cars_from_memory(ee, m, entity):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    recs, clones, out = u(m + 0x3C), u(m + 0x44), []
    for k in range(u(m + 4)):
        c = u(clones + 4 * k)
        if u(c + 0xC) != entity: raise ValueError('clone entity')
        out.append(dict(resource=u(c + 0x78), runtime_flags=u(c + 8), record=[u(recs + 0x60 * k + 4 * i) for i in range(24)],
                        matrix=[u(c + 0x10 + 4 * i) for i in range(16)], low=[u(c + 0x60 + 4 * i) for i in range(3)], high=[u(c + 0x6C + 4 * i) for i in range(3)]))
    return out


def rider_remaining(code):
    path = CAPTURE.get(code)
    if not path or not path.exists(): return {}
    b = path.read_bytes(); out = {}
    for i in range(len(b) // 16384):
        tick = struct.unpack_from('<I', b, i * 16384 + 4)[0]
        out.setdefault(tick, struct.unpack_from('<f', b, i * 16384 + 32 + 0x4D0 - 0x100)[0])
    return out


def export_location(code):
    loc = Location(code)
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    e = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    assert e(0x441F38 + 20 * 4) == 0x2FE0C0 and e(0x441F38 + 19 * 4) == 0x2FDED0
    for off, target in ((0x14, 0x35A560), (0x1C, 0x35A5D8), (0x44, 0x360B60), (0x94, 0x361C20), (0xB4, 0x35B200), (0xC4, 0x35A918)):
        assert e(0x48F168 + off) == target, hex(off)
    assert e(0x490E80 + 0x194) == 0x3568B0 and e(0x490B10 + 0x194) == 0x3568B0   # Object and LiveComp share the entity update
    names = {loc.resource(i): i['name'] for i in loc.world['instances']}
    audit = {r['resource']: r for r in loc.audit['instances']}
    ms_calls = script_calls(loc, 20)
    ready_path = location_state(code, 'ready'); ready = load(ready_path)
    if tick_of(ready) != 0: raise ValueError(f'{ready_path.name} is not race tick 0')
    u = lambda a: struct.unpack_from('<I', ready, a & 0x1FFFFFF)[0]
    remaining = rider_remaining(code)
    multis = []
    # resident at race tick 0
    for m, entity, inst in live_modifiers(ready, 0x48F168):
        res = u(inst + 0x78); prog, slot, owner, keys = ms_calls[res]
        f = multispline_fields(ready, m)
        multis.append(dict(name=names[res], resource=res, entity_class='LiveComp' if u(entity + 0xC) == 0x490B10 else 'Object',
                           authored_flags=u(inst + 8), program=prog, slot=slot, owner=owner, activation='load', source=str(ready_path.relative_to(ROOT)),
                           **f, cars=cars_from_memory(ready, m, entity)))
    # section-activated (construction state from the verified port)
    seen = {x['name'] for x in multis}
    for path in sorted(activation_dir(code).glob('multi-spline-construct-*.json')):
        data = json.loads(path.read_text())
        if data['savestate_tick'] == 0: continue
        ee = load(ROOT / data['source']); uu = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
        for piece, mod in zip(data['pieces'], data['modifiers']):
            if piece['name'] in seen: continue
            if not piece['replay_exact']: raise ValueError(f'{piece["name"]}: construction replay not exact')
            m, entity = int(mod['modifier'], 16), int(mod['entity'], 16); inst = uu(entity + 0x18)
            res = uu(inst + 0x78); prog, slot, owner, keys = ms_calls[res]
            f = multispline_fields(ee, m); f.update(distance=piece['distance'], cursor=piece['cursor'], dirty=0)
            live = cars_from_memory(ee, m, entity)
            cars = [dict(resource=c['resource'], runtime_flags=c['runtime_flags'], **{k: p[k] for k in ('record', 'matrix', 'low', 'high')}) for c, p in zip(live, piece['cars'])]
            built = data['savestate_tick'] - piece['ticks_since_construction']
            multis.append(dict(name=piece['name'], resource=res, entity_class='LiveComp' if uu(entity + 0xC) == 0x490B10 else 'Object',
                               authored_flags=uu(inst + 8), program=prog, slot=slot, owner=owner, activation='section',
                               source=data['source'], constructed_after_tick=built, rider_remaining_at_construction=remaining.get(built),
                               **f, cars=cars))
            seen.add(piece['name'])
    # script arguments against the seeds (spline, count, speed, start distance)
    for x in multis:
        keys = ms_calls[x['resource']][3]
        spline = keys[1][1]; count = keys.get(11, ('int', 1))[1]
        kmh = keys.get(3, ('float', 20.0))[1]
        if x['path'] != spline or x['count'] != count: raise ValueError(f'{x["name"]}: program arguments {keys} vs {x}')
        if abs(struct.unpack('<f', struct.pack('<I', x['speed']))[0] - float(kmh) * KMH) > 1e-3: raise ValueError(f'{x["name"]}: speed')
    # resident looping SplineModifiers (0xF0 image; +0xE0 = 0x90 * cursor)
    splines = []
    for m, entity, inst in live_modifiers(ready, 0x48F250):
        if u(m + 0x30) in (0, 2, 4): continue
        seg = u(m + 0xE0); head = seg
        while u(head + 0x60): head = u(head + 0x60)
        words = [u(m + 4 * k) for k in range(60)]; words[0xE0 // 4] = seg - head
        if (seg - head) % 0x90: raise ValueError('spline cursor')
        res = u(inst + 0x78)
        splines.append(dict(name=names[res], resource=res, authored_flags=u(inst + 8), entity_class='LiveComp' if u(entity + 0xC) == 0x490B10 else 'Object',
                            path=u(m + 0xD8), cursor=(seg - head) // 0x90, bytes=words, source=str(ready_path.relative_to(ROOT))))
    # triggered spline pieces (Snow Jam layout)
    pieces_rows, piece_list = [], []
    spath = activation_dir(code) / 'spline-setpieces.json'
    if spath.exists():
        for piece in json.loads(spath.read_text())['pieces']:
            trig = piece['trigger'][0]
            if trig['slot'] not in (1, 2, 5): continue    # slot 5 = timer programs (core stage VM)
            # slot 1 pieces not resident at race tick 0 (R&B ravensplineanima/c) start from the section pass like the Snow Jam raven.
            # A slot-1 resident (R&B ravensplineanimb, seeded above as a looping Spline) gets its seed too: a section leave destroys
            # its entity (0x34FD90, flags = high half | high half >> 16 | 2; the dtor 0x3553C0 -> 0x3567E0 puts the instance back
            # into the octree cell of its own bounds) and the next enter runs the slot-1 program again (builtin19: a new Spline,
            # one draw). Drawn from those restored flags (& 3 == 3), not from the resident entity's countdown flags.
            resident = trig['slot'] == 1 and piece['instance'] in {s['name'] for s in splines}
            r = piece['resolved']; inst = loc.by_resource[piece['resource']]
            if resident:
                f = piece['countdown_runtime_flags']; restored = (f & 0xFFFF0000) | (((f - (f >> 31 << 32)) >> 16) & 0xFFFFFFFF) | 2   # sra 16
                piece = dict(piece, drawn_while_on_path=(restored & 3) == 3)
            owner = next(i for i in loc.world['instances'] if i['name'] == trig['instance'])
            m = [f32bits(v) for v in inst['matrix']]
            piece_list.append(piece['instance'])
            arr = lambda xs: '{' + ','.join('0x%08xu' % x for x in xs) + '}'
            pieces_rows.append('{%du,%du,%du,%du,%du,%du,{%du,%du,%d,%d,0x%08xu,0x%08xu,0x%08xu,0x%08xu,%d,0x%08xu,0x%08xu},%s,%s,%s}' % (
                # guard 2: a random-gated trigger program (Crow's Nest osprey) launches it from the core VM's builtin19
                piece['resource'], loc.resource(owner), trig['slot'], piece['program'], 2 if piece.get('vm_launch') else 1 if piece['guard_builtin52'] else 0, 1 if piece['drawn_while_on_path'] else 0,
                r['instance'] & 0xFFFFFFFF, r['spline'], r['end_mode'], r['orientation'], f32bits(r['speed_kmh']), f32bits(r['roll_degrees']), f32bits(r['start_distance']),
                f32bits(r['start_jitter']), r['running'], f32bits(r['acceleration']), f32bits(r['acceleration_time']), arr(m),
                arr([f32bits(v) for v in inst['bounds_min_cm']]), arr([f32bits(v) for v in inst['bounds_max_cm']])))
    # ---- header ----
    arr = lambda xs: '{' + ','.join('0x%08xu' % x for x in xs) + '}'
    ns = f'browser_set_pieces_{code.lower()}'
    h = ['#pragma once', f'// Generated by tools/export_set_pieces.py --location {code} (tools/export_location_set_pieces.py).',
         '#include <array>', '#include <cstdint>',
         '#ifndef BROWSER_LOCATION_SET_PIECE_TYPES', '#define BROWSER_LOCATION_SET_PIECE_TYPES',
         '// One MultiSpline car: clone instance resource/flags, 0x60-byte record, clone +0x10 matrix, clone +0x60/+0x6C bounds (raw bits).',
         'struct BrowserMultiSplineCarSeed {uint32_t resource,runtimeFlags;std::array<uint32_t,24> record;std::array<uint32_t,16> matrix;std::array<uint32_t,3> low,high;};',
         '// activation 0 = resident from the load (state at race tick 0), 1 = section streaming: construction state; the capture built it',
         '// at the end of tick constructedAfterTick (first update in the next tick) with the human rider at activationRemaining cm.',
         'struct BrowserMultiSplineSeed {uint32_t resource,authoredFlags,count,mode,angle,distance,speed,radius,dirty,path,cursor,length,firstCar,activation;int32_t constructedAfterTick;float activationRemaining;};',
         '// Looping SplineModifier resident from the load: 0xF0-byte image with +0xE0 = 0x90 * cursor (originalSplineModifierFromBytes(bytes, &record, 0)).',
         'struct BrowserSplineModifierSeed {uint32_t resource,authoredFlags,path,cursor;std::array<uint32_t,60> bytes;};',
         '// Triggered spline piece (same layout as the Snow Jam BrowserSplinePieceSeed).',
         'struct BrowserLocationSplinePieceArgs {uint32_t instance,spline;int32_t endMode,orientation;uint32_t speedKmh,rollDegrees,startDistance,startJitter;int32_t running;uint32_t acceleration,accelerationTime;};',
         'struct BrowserLocationSplinePieceSeed {uint32_t resource,owner,slot,program,guard,drawn;BrowserLocationSplinePieceArgs args;std::array<uint32_t,16> matrix;std::array<uint32_t,3> low,high;};',
         '#endif', f'namespace {ns} {{', f'inline constexpr const char* location="{code}";',
         'inline constexpr const char* worldHash=' + json.dumps(loc.audit['source_sha256']) + ';']
    car_rows, ms_rows = [], []
    for x in multis:
        first = len(car_rows)
        for c in x['cars']:
            car_rows.append('{%du,0x%08xu,%s,%s,%s,%s}' % (c['resource'], c['runtime_flags'], arr(c['record']), arr(c['matrix']), arr(c['low']), arr(c['high'])))
        rem = x.get('rider_remaining_at_construction')
        ms_rows.append('{%du,0x%08xu,%du,%du,0x%08xu,0x%08xu,0x%08xu,0x%08xu,%du,0x%xu,%du,0x%08xu,%du,%du,%d,%s}' % (
            x['resource'], x['authored_flags'], x['count'], x['mode'], x['angle'], x['distance'], x['speed'], x['radius'], x['dirty'], x['path'], x['cursor'], x['length'],
            first, 0 if x['activation'] == 'load' else 1, x.get('constructed_after_tick', -1), ('%.1ff' % rem) if rem is not None else '0.f'))
    h.append(f'inline constexpr std::array<BrowserMultiSplineCarSeed,{len(car_rows)}> multiSplineCars={{{{' + ','.join(car_rows) + '}};')
    h.append(f'inline constexpr std::array<BrowserMultiSplineSeed,{len(ms_rows)}> multiSplines={{{{' + ','.join(ms_rows) + '}};')
    h.append(f'inline constexpr std::array<BrowserSplineModifierSeed,{len(splines)}> splineModifiers={{{{' + ','.join(
        '{%du,0x%08xu,0x%xu,%du,%s}' % (s['resource'], s['authored_flags'], s['path'], s['cursor'], arr(s['bytes'])) for s in splines) + '}};')
    h.append(f'inline constexpr std::array<BrowserLocationSplinePieceSeed,{len(pieces_rows)}> splinePieces={{{{' + ','.join(pieces_rows) + '}};')
    # Slot-2 trigger owners (rider contact -> 121818 -> 30A060 slot 2), like Snow Jam browserSetPieceTriggerOwners:
    # every own-track instance with a slot-2 program, except pickups (slot 1 registers builtin99) and crashbags
    # (slot 2 builtin15 Roller); this rule reproduces the Snow Jam list (programs 28/30/49 excluded) exactly.
    def builtins(program):
        try: return [c[0] for c in decode_program(loc.programs[program])]
        except (KeyError, TypeError): return [i.get('builtin') for i in loc.programs[program]['instructions'] if i['opcode'] == 0x21]
    owners = []
    for inst, row in loc.handler_rows():
        if row[2] == 0xFFFFFFFF: continue
        if (row[1] != 0xFFFFFFFF and 99 in builtins(row[1] >> 8)) or 15 in builtins(row[2] >> 8): continue
        owners.append(loc.resource(inst))
    h.append(f'inline constexpr std::array<uint32_t,{len(owners)}> triggerOwners={{{{' + ','.join('%du' % o for o in sorted(owners)) + '}};')
    h.append('}')
    (ROOT / 'web/generated').mkdir(exist_ok=True)
    (ROOT / f'web/generated/set_piece_seed_{code}.hpp').write_text('\n'.join(h) + '\n')
    out = dict(location=code, ready=str(ready_path.relative_to(ROOT)), ready_ee_sha256=hashlib.sha256(ready).hexdigest(),
               world_package_sha256=loc.audit['world_package_sha256'], multisplines=multis, spline_modifiers=splines, spline_pieces=piece_list,
               scope='MultiSpline / resident Spline seeds (raw bits) and triggered spline pieces; see tools/export_location_set_pieces.py')
    (activation_dir(code) / 'set-pieces.json').write_text(json.dumps(out, indent=1) + '\n')
    print(f'{code}: multisplines', [(x['name'], x['activation'], x['count'], x.get('constructed_after_tick')) for x in multis],
          'splines', [s['name'] for s in splines], 'spline pieces', piece_list)


if __name__ == '__main__':
    export_location(sys.argv[1])
