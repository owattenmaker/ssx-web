#!/usr/bin/env python3
"""Export a location's UVScroll (builtin 21) instances for the browser (development
export; --location, default ARA1: writes the git-ignored web/public/assets/UVSCROLL/uv-scroll.json,
other locations web/public/assets/<LOC>/UVSCROLL/uv-scroll.json, and, when the location's race
snapshots exist, uv-scroll-snapshots.json for web/test-uv-scroll.mjs). Per-location stage/track
and inputs: tools/set_piece_location.py.

Sources: stage kind-16 handler rows (bam.ssb chunk 33) -> LUN programs whose
builtin 21 (0x2FE2C0) calls carry keyed arguments over the builtin defaults;
key types from 0x446238 (int argument into a float field is converted); target =
key 0 resource or the program's own instance. Initial state = 0x35F6E8
(engine/uv_scroll.hpp). The countdown audit 'draw' is carried so hidden
free-ride objects (Big Challenge) can be skipped.
Verification: every live UVScroll (Object entity container +4) in consecutive
race snapshots, for the JS test to replay tick by tick.
"""
import glob, json, re, struct, sys, zipfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(root / 'tools'))
from world_assets import world_chunks, records
from export_startfire import decode_program
from export_flags import bits, from_bits, mul, div_nearest, Mem, load_ee
from set_piece_location import Location, SNAPSHOTS, web_asset_dir
from locations import activation_dir

GP = 0x4A30F0


def main(output=None, pattern=None, location='ARA1'):
    loc = Location(location); track = loc.track
    output = output or web_asset_dir(location, 'UVSCROLL'); pattern = pattern or SNAPSHOTS[location]
    elf = (root / 'local/disc/SLUS_207.72').read_bytes()
    u = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    assert u(0x441F38 + 21 * 4) == 0x2FE2C0
    key_types = [u(0x446238 + 4 * k) for k in range(13)]
    step = from_bits(u(GP - 0x3384))
    world = loc.world
    programs = loc.programs
    stage = loc.stage; row_base = loc.rows[1]
    audit = {r['resource']: r for r in json.loads((activation_dir(location) / 'countdown-instances.json').read_text())['instances']}
    descriptors = world['bindings'][str(track)]['descriptors']; by_rid = {i['rid']: i for i in world['instances'] if i['track'] == track}
    out = {}
    for inst in world['instances']:
        if inst['track'] != track: continue
        r08 = descriptors[inst['collision_descriptor']]['resource08']
        if r08 == 0xFFFFFFFF or r08 & 255 != track: continue
        row = struct.unpack_from('<6I', stage, row_base + (r08 >> 8) * 24)
        for slot, w in enumerate(row):
            if w == 0xFFFFFFFF: continue
            program = w >> 8
            try: calls = decode_program(programs[program])
            except KeyError: continue   # register-indirect arguments; no builtin 21 among them in ARA1
            for builtin, _, keys in calls:
                if builtin != 21: continue
                words = [0xFFFFFFFF, 5, bits(0), bits(0), bits(step), bits(step), bits(1), bits(0), bits(0), bits(0), bits(0), bits(1), 0]
                for k, (kind, v) in keys.items():
                    if kind == 'resource': words[k] = v
                    elif kind == 'float': words[k] = bits(v)
                    else: words[k] = bits(float(v)) if key_types[k] == 2 else v & 0xFFFFFFFF
                target = inst if words[0] == 0xFFFFFFFF else by_rid[words[0] >> 8]
                resource = (target['rid'] << 8) | track
                r = lambda k: from_bits(words[k])
                initial = dict(mode=words[1], timer=0.0, onTime=r(6), offTime=r(7), angle=0.0, spin=mul(r(8), div_nearest(1.0, 60.0)),
                               axis=[r(9), r(10), r(11), 0.0], u=r(2), v=r(3), stepU=r(4), stepV=r(5), active=1)
                out[resource] = dict(resource=resource, name=target['name'], program=program, slot=slot, draw=audit.get(resource, {}).get('draw'), words=words, initial=initial)
    instances = sorted(out.values(), key=lambda x: x['resource'])
    output.mkdir(parents=True, exist_ok=True)
    (output / 'uv-scroll.json').write_text(json.dumps(dict(version=1, fps=60, source='stage programs builtin 21 (0x2FE2C0) -> 0x35F6E8; tick 0x35F7D0; texture translation 0x35FC20', instances=instances), separators=(',', ':')))
    print(len(instances), 'UVScroll instances ->', output / 'uv-scroll.json')
    paths = sorted(glob.glob(pattern), key=lambda p: int(re.search(r'tick(\d+)', p).group(1)))
    if not paths: return
    names = {x['resource']: x for x in instances}
    # ARA1: countdown runtime-instance addresses; other locations: the instance's own resource (+0x78).
    runtime = {i['address']: i for i in json.loads((root / 'local/event-activation/runtime-instances.json').read_text())['instances']} if location == 'ARA1' else None
    snaps = []
    for path in paths:
        m = load_ee(path); mm = Mem(m); tick = int(re.search(r'tick(\d+)', path).group(1)); states = {}; where = {}
        arr = memoryview(m).cast('I')
        for i in range(0x100000 // 4, len(arr)):
            if arr[i] not in (0x490E80, 0x490B10): continue
            e = i * 4 - 12; c = mm.u(e + 0x1C)
            if runtime is not None: inst = runtime.get(mm.u(e + 0x18))
            else:
                a_ = mm.u(e + 0x18)
                inst = dict(resource=mm.u(a_ + 0x78)) if 0x100000 < a_ < 0x2000000 and mm.u(a_ + 0xC) == e else None
            if not inst or not 0x100000 < c < 0x2000000: continue
            s = mm.u(c + 4)
            if not 0x100000 < s < 0x2000000 or inst['resource'] not in names: continue
            states[inst['resource']] = [mm.u(s + o) for o in (0, 4, 8, 0xC, 0x10, 0x14, 0x30, 0x34, 0x38, 0x3C, 0x40)]
            where[inst['resource']] = (e, s)
        snaps.append(dict(tick=tick, states=states, where=where))
    pairs = []
    for a, b in zip(snaps, snaps[1:]):
        for res, sa in a['states'].items():
            # Streamed sections (BRA2) unload and rebuild their UVScrolls: pair only the same live object.
            if res in b['states'] and (location == 'ARA1' or a['where'][res] == b['where'][res]): pairs.append(dict(resource=res, name=names[res]['name'], ticks=b['tick'] - a['tick'], before=sa, after=b['states'][res]))
    (output / 'uv-scroll-snapshots.json').write_text(json.dumps(dict(version=1, fields=['mode', 'timer', 'onTime', 'offTime', 'angle', 'spin', 'u', 'v', 'stepU', 'stepV', 'active'], pairs=pairs), separators=(',', ':')))
    print(f'{sum(len(s["states"]) for s in snaps)} live UVScroll states in {len(snaps)} snapshots, {len(pairs)} consecutive pairs')


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--location', default='ARA1'); ap.add_argument('--output', type=Path); ap.add_argument('--snapshots')
    a = ap.parse_args()
    main(a.output, a.snapshots, a.location)
