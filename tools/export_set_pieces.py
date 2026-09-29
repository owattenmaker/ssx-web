#!/usr/bin/env python3
"""Export the Snow Jam race-event set pieces (development export; local/event-activation/set-pieces.json
and the browser seed web/generated/set_piece_seed.hpp, both git-ignored).

Chairlift (MultiSplineModifier 0x48F168, engine/multi_spline_modifier.hpp): stage handler rows 134/135
(mdl_ARA1_tramlores_0 / _1, collision descriptor resource08 >> 8) run programs 69/71 in slot 1 when the
section loads: B3() -> self, B31(...), MultiSpline(self, spline 0x608 / 0x708, speed 35, rotation 90 deg)
(builtin20 0x2FE0C0 -> factory 0x355B30 -> ctor 0x359F88). The load happens before the countdown, so the
browser seeds the modifier, its car records and the three clone instances per lift from the race-tick-0
savestate snow-jam-ready (verified: 18 original updates from it reproduce the countdown anchor and the
setpieces/race capture bit for bit, tools/test_multi_spline_live.py). Every seed word is exported as
raw bits.
"""
import hashlib, json, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records  # noqa: E402

STATE = ROOT / 'local/reference/pcsx2/snow-jam-ready.p2s'
GP = 0x4A30F0
LIFTS = [  # modifier, entity, authored resource, program, spline
    (0x592E80, 0x592DB0, 407560, 69, 0x608),
    (0x593580, 0x5934B0, 711688, 71, 0x708),
]


def main():
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    e = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    assert e(0x441F38 + 20 * 4) == 0x2FE0C0 and e(0x2FE278) == 0x0C000000 | (0x355B30 >> 2)
    assert e(0x355B30 + 0x2C) == 0x0C000000 | (0x359F88 >> 2) or any(e(0x355B30 + 4 * k) == 0x0C000000 | (0x359F88 >> 2) for k in range(24))
    for off, target in ((0x14, 0x35A560), (0x1C, 0x35A5D8), (0x44, 0x360B60), (0x94, 0x361C20), (0xB4, 0x35B200), (0xC4, 0x35A918)):
        assert e(0x48F168 + off) == target, hex(off)
    assert e(0x490B10 + 0x194) == 0x3568B0 and e(0x490B10 + 0x154) == 0x34E698 and e(0x490B10 + 0x74) == 0x355420
    assert e(0x360B60) == 0x03E00008 and e(0x360B64) == 0x24020001          # rigid predicate returns 1
    # stage rows and programs
    stage = None
    for ci, chunk in enumerate(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb')):
        if ci == 33:
            stage = next(d for k, t, r, d in records(chunk) if k == 16)
            break
    programs = {p['index']: p for p in json.loads((ROOT / 'local/browser-pickups/ara1-scripts.json').read_text())['programs']}
    world = json.loads((ROOT / 'local/assets/native/ARA1/world_collision.json').read_text())
    audit = json.loads((ROOT / 'local/event-activation/countdown-instances.json').read_text())
    runtime = {r['resource']: r for r in audit['instances']}
    by_resource = {(i['rid'] << 8) | i['track']: i for i in world['instances']}
    with zipfile.ZipFile(STATE) as z:
        ee = z.read('eeMemory.bin')
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    tick = u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8)
    if tick != 0 or struct.unpack('<f', struct.pack('<I', u(u(GP - 0x848) + 0x14)))[0] != struct.unpack('<f', struct.pack('<I', 0x3C888889))[0]:
        raise ValueError('snow-jam-ready is not the race-tick-0 state')
    lifts = []
    for modifier, entity, resource, program, spline in LIFTS:
        inst = by_resource[resource]
        d = world['bindings'][str(inst['track'])]['descriptors'][inst['collision_descriptor']]
        row = struct.unpack_from('<6I', stage, 0x70 + (d['resource08'] >> 8) * 24)
        if row[1] != (program << 8) | 8:
            raise ValueError(f'{inst["name"]}: slot-1 handler is not program {program}')
        words = programs[program]['code_words']
        if words[5] != spline or (words[-2] & 0xFFFF00FF) != 0x04140021:   # 27:1[spline] ... B20/4 -> 2
            raise ValueError(f'program {program} is not MultiSpline(self, {spline:#x}, ...)')
        if u(modifier) != 0x48F168 or u(u(entity + 0x1C)) != modifier or u(entity + 0xC) != 0x490B10 or u(entity + 0x18) != u(modifier + 0x40):
            raise ValueError('chairlift modifier layout')
        authored = u(modifier + 0x40)
        if u(authored + 0x78) != resource or (runtime[resource]['runtime_flags'] ^ u(authored + 8)) & ~0x300:   # 0x100/0x200: renderer bookkeeping
            raise ValueError('authored chairlift instance identity')
        count = u(modifier + 4)
        recs, clones = u(modifier + 0x3C), u(modifier + 0x44)
        cars = []
        for k in range(count):
            c = u(clones + 4 * k)
            if u(c + 0xC) != entity:
                raise ValueError('clone entity')
            cars.append(dict(resource=u(c + 0x78), runtime_flags=u(c + 8), record=[u(recs + 0x60 * k + 4 * i) for i in range(24)],
                             matrix=[u(c + 0x10 + 4 * i) for i in range(16)], low=[u(c + 0x60 + 4 * i) for i in range(3)],
                             high=[u(c + 0x6C + 4 * i) for i in range(3)]))
        if u(modifier + 0x48) != spline:
            raise ValueError('path resource')
        lifts.append(dict(name=inst['name'], resource=resource, runtime_flags=runtime[resource]['runtime_flags'], modifier=hex(modifier), program=program,
                          count=count, mode=u(modifier + 8), angle=u(modifier + 0xC), distance=u(modifier + 0x10), speed=u(modifier + 0x14),
                          radius=u(modifier + 0x18), dirty=u(modifier + 0x30), path=spline, cursor=u(modifier + 0x4C), length=u(modifier + 0x54), cars=cars))
    h = ['#pragma once', '// Generated by tools/export_set_pieces.py from local/reference/pcsx2/snow-jam-ready.p2s (race tick 0).',
         '#include <array>', '#include <cstdint>',
         'struct BrowserChairCarSeed {uint32_t resource,runtimeFlags;std::array<uint32_t,24> record;std::array<uint32_t,16> matrix;std::array<uint32_t,3> low,high;};',
         'struct BrowserChairliftSeed {uint32_t resource,runtimeFlags,count,mode,angle,distance,speed,radius,dirty,path,cursor,length;std::array<BrowserChairCarSeed,3> cars;};',
         'inline constexpr const char* browserSetPieceWorldHash=' + json.dumps(audit['source_sha256']) + ';']
    arr = lambda xs: '{' + ','.join('0x%08xu' % x for x in xs) + '}'
    rows = []
    for l in lifts:
        if l['count'] != 3:
            raise ValueError('car count')
        cars = ','.join('{%du,0x%08xu,%s,%s,%s,%s}' % (c['resource'], c['runtime_flags'], arr(c['record']), arr(c['matrix']), arr(c['low']), arr(c['high'])) for c in l['cars'])
        rows.append('{%du,0x%08xu,%du,%du,0x%08xu,0x%08xu,0x%08xu,0x%08xu,%du,0x%xu,%du,0x%08xu,{{%s}}}' % (
            l['resource'], l['runtime_flags'], l['count'], l['mode'], l['angle'], l['distance'], l['speed'], l['radius'], l['dirty'], l['path'], l['cursor'], l['length'], cars))
    h.append('inline constexpr std::array<BrowserChairliftSeed,2> browserChairliftSeeds={{' + ','.join(rows) + '}};')
    # Spline set pieces (engine/spline_modifier.hpp, tools/export_spline_setpieces.py): the raven flyby
    # (slot 1) and the invisible rocket/spintwin/dragon carriers of the particle trails (slot 2 triggers).
    # EZrocketCore (program 142, slot 5 of a LiveComp timer) is launched by the timer program in the core VM.
    splines = json.loads((ROOT / 'local/event-activation/spline-setpieces.json').read_text())
    f32 = lambda v: struct.unpack('<I', struct.pack('<f', float(v)))[0]
    names = {i['name']: i for i in world['instances']}
    piece_rows = []
    for piece in splines['pieces']:
        # program 142 (EZrocketCore): slot 5 of the EZseqTimer_1000 LiveComp; launched by the timer program's
        # builtin19 in the core's stage VM (web/stage_world.inc -> stage_launch_spline), never by a contact.
        r = piece['resolved']
        inst = by_resource[piece['resource']]
        trig = piece['trigger'][0]
        owner = names[trig['instance']]
        owner_resource = (owner['rid'] << 8) | owner['track']
        m = [f32(v) for v in inst['matrix']]
        piece_rows.append('{%du,%du,%du,%du,%du,%du,{%du,%du,%d,%d,0x%08xu,0x%08xu,0x%08xu,0x%08xu,%d,0x%08xu,0x%08xu},%s,%s,%s}' % (
            piece['resource'], owner_resource, trig['slot'], piece['program'], 1 if piece['guard_builtin52'] else 0, 1 if piece['drawn_while_on_path'] else 0,
            r['instance'] & 0xFFFFFFFF, r['spline'], r['end_mode'], r['orientation'], f32(r['speed_kmh']), f32(r['roll_degrees']), f32(r['start_distance']),
            f32(r['start_jitter']), r['running'], f32(r['acceleration']), f32(r['acceleration_time']), arr(m),
            arr([f32(v) for v in inst['bounds_min_cm']]), arr([f32(v) for v in inst['bounds_max_cm']])))
    h.append('struct BrowserSplinePieceArgs {uint32_t instance,spline;int32_t endMode,orientation;uint32_t speedKmh,rollDegrees,startDistance,startJitter;int32_t running;uint32_t acceleration,accelerationTime;};')
    h.append('struct BrowserSplinePieceSeed {uint32_t resource,owner,slot,program,guard,drawn;BrowserSplinePieceArgs args;std::array<uint32_t,16> matrix;std::array<uint32_t,3> low,high;};')
    h.append('inline constexpr std::array<BrowserSplinePieceSeed,%d> browserSplinePieceSeeds={{' % len(piece_rows) + ','.join(piece_rows) + '}};')
    # raven flyby slot 1: section activation, observed at human remaining 242,764 (setpieces/race tick 4041)
    # and 243,244 (setpieces/full tick 4281); the browser activates at the midpoint.
    h.append('inline constexpr float browserRavenSectionRemaining=243000.f;')
    # Slot-2 trigger owners (rider contact -> 121818 -> 30A060 slot 2) of course-script set pieces: every
    # instance whose handler row has a slot-2 program, except the pickups (28/30) and crashbags (49).
    owners = []
    for inst in world['instances']:
        d = world['bindings'][str(inst['track'])]['descriptors'][inst['collision_descriptor']]
        r8 = d['resource08']
        if r8 == -1 or r8 & 255 != 8:
            continue
        slot2 = struct.unpack_from('<6I', stage, 0x70 + (r8 >> 8) * 24)[2]
        if slot2 != 0xFFFFFFFF and slot2 >> 8 not in (28, 30, 49):
            owners.append((inst['rid'] << 8) | inst['track'])
    h.append('inline constexpr std::array<uint32_t,%d> browserSetPieceTriggerOwners={{' % len(owners) + ','.join('%du' % o for o in sorted(owners)) + '}};')
    drawn = [piece['resource'] for piece in splines['pieces'] if piece['drawn_while_on_path']]
    out = dict(source=str(STATE.relative_to(ROOT)), ee_sha256=hashlib.sha256(ee).hexdigest(), world_package_sha256=audit['world_package_sha256'],
               tick=0, chairlifts=lifts, drawn_spline_pieces=drawn, scope='Race-tick-0 MultiSplineModifier chairlift state (raw bits); drawn spline set pieces')
    (ROOT / 'local/event-activation/set-pieces.json').write_text(json.dumps(out, indent=1) + '\n')
    (ROOT / 'web/generated').mkdir(exist_ok=True)
    (ROOT / 'web/generated/set_piece_seed.hpp').write_text('\n'.join(h) + '\n')
    print('chairlifts:', [(l['name'], hex(l['runtime_flags']), [hex(c['resource']) for c in l['cars']]) for l in lifts])


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--location', default='ARA1', help='ARA1 (default): set_piece_seed.hpp as above; other locations: '
                    'web/generated/set_piece_seed_<LOC>.hpp via tools/export_location_set_pieces.py')
    location = ap.parse_args().location
    if location == 'ARA1':
        main()
    else:
        from export_location_set_pieces import export_location
        export_location(location)
