#!/usr/bin/env python3
"""Live MultiSplineModifier parity for Metro-City (BRA2) and The Junction (BHP1) set pieces
(tests/multi_spline_location_live.cpp; development oracle, never published).

For every group below: extract the start savestate, discover the live MultiSplineModifiers
(vtable 0x48F168 at the entity's modifier container), run the full original 0x35A560 + 0x3568B0
in lockstep with engine/multi_spline_modifier.hpp, compare the port against the later kept
snapshots of the same PS2 run, randomize 0x345248/0x35AC20/0x35B200, and write the
construction-time state (ctor 0x359F88 start distance, see START below) to
local/event-activation/<LOC>/multi-spline-construct-<group>.json for tools/export_set_pieces.py.

usage: test_multi_spline_location.py [--location BRA2|BHP1|ABA1] [--ticks N] [--cases N]
Log: local/reference/multi-spline/<LOC>-<group>.log
"""
import argparse, glob, json, re, struct, subprocess, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from original_live_build import build_live, vtable_targets  # noqa: E402
from locations import activation_dir  # noqa: E402

FOLDER = ROOT / 'local/reference/multi-spline'
RUNS = ROOT / 'local/ps2-capture/runs'
# group -> (start savestate, later snapshots of the same run). BRA2's bins are section-activated
# mid-race (setpieces-bra2 full-course run); BHP1's vehicles are resident from the load (the-junction-ready,
# race tick 0; the setpieces-bhp1 run starts from the countdown anchor of the same load).
GROUPS = {
    'BRA2': {'movingbin': (RUNS / 'setpieces-bra2/full.tick9619.p2s', 'setpieces-bra2/full.tick*.p2s'),
             'cargobin-end': (RUNS / 'setpieces-bra2/full.tick12018.p2s', 'setpieces-bra2/full.tick*.p2s'),
             'cargobins': (RUNS / 'setpieces-bra2/full.tick12418.p2s', 'setpieces-bra2/full.tick*.p2s')},
    'BHP1': {'vehicles': (ROOT / 'local/reference/pcsx2/the-junction-ready.p2s', 'setpieces-bhp1/full.tick*.p2s')},
    # Crow's Nest: the box vehicles (programs 64..66, start distance key 5) are section-built by tick 819, the
    # tramlores chairlift (program 38) by 1218 (setpieces-aba1 tuck run from crows-nest-countdown-anchor).
    'ABA1': {'boxvehicles': (RUNS / 'setpieces-aba1/full.tick819.p2s', 'setpieces-aba1/full.tick*.p2s'),
             'tramlores': (RUNS / 'setpieces-aba1/full.tick1218.p2s', 'setpieces-aba1/full.tick*.p2s')},
    # Peak 2/3 trams (docs/visual-parity.md 41): the first kept state with each modifier live (the visual-parity sweep runs).
    'CBA2': {'tramlores': (RUNS / 'peak2/cba2-full.tick818.p2s', 'peak2/cba2-full.tick*.p2s')},
    'CHP2': {'tramlores': (RUNS / 'peak2/chp2-full.tick1619.p2s', 'peak2/chp2-full.tick*.p2s')},
    'EBA3': {'tramwindya': (RUNS / 'peak3/much-2-much-full.tick420.p2s', 'peak3/much-2-much-full.tick*.p2s'),
             'tramwindyb': (RUNS / 'peak3/much-2-much-full.tick2019.p2s', 'peak3/much-2-much-full.tick*.p2s')},
    'EHP3': {'tramlores': (RUNS / 'peak3/perpendiculous-full.tick818.p2s', 'peak3/perpendiculous-full.tick*.p2s')},
}
GP = 0x4A30F0


def tick_of(ee):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    return u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8)


def modifiers(ee, names, starts=None):
    """(modifier, entity, name, start distance bits) of every live MultiSpline. starts: {name: builtin20 key-5
    start distance} of the programs that pass one (ctor 0x359F88 0x35A078: a nonzero key 5 replaces the default,
    then d0 < 0 or d0 > length gives 0)."""
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    f = lambda a: struct.unpack_from('<f', ee, a & 0x1FFFFFF)[0]
    arr = memoryview(ee).cast('I'); out = []
    for i in range(0x100000 // 4, len(arr) - 8):
        if arr[i] not in (0x490E80, 0x490B10): continue
        e = i * 4 - 12; inst = u(e + 0x18)
        if not 0x100000 < inst < 0x2000000 or u(inst + 0xC) != e: continue
        c = u(e + 0x1C)
        if not 0x100000 < c < 0x2000000: continue
        m = u(c)
        if not 0x100000 < m < 0x2000000 or u(m) != 0x48F168: continue
        name = names[u(inst + 0x78)]
        # ctor 0x359F88: distance 0 (speed >= 0) or the path length (speed < 0); the script start distance
        # (key 5) is resolved by tools/export_set_pieces.py from the program, here only the default rule.
        d0 = 0 if f(m + 0x14) >= 0 else u(m + 0x54)
        start = (starts or {}).get(name)
        if start:
            d0 = struct.unpack('<I', struct.pack('<f', start))[0]
            if start < 0 or f(m + 0x54) < start: d0 = 0
        out.append((m, e, name, d0))
    return out


def splines(ee, names):
    """(modifier, entity, name) of every live looping SplineModifier (vtable 0x48F250, end mode 1/3/5+)."""
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    arr = memoryview(ee).cast('I'); out = []
    for i in range(0x100000 // 4, len(arr) - 8):
        if arr[i] not in (0x490E80, 0x490B10): continue
        e = i * 4 - 12; inst = u(e + 0x18)
        if not 0x100000 < inst < 0x2000000 or u(inst + 0xC) != e: continue
        c = u(e + 0x1C)
        if not 0x100000 < c < 0x2000000: continue
        m = u(c)
        if 0x100000 < m < 0x2000000 and u(m) == 0x48F250 and u(m + 0x30) not in (0, 2, 4):
            out.append((m, e, names[u(inst + 0x78)]))
    return out


def check_catalog(ee, location, paths):
    """web/public/assets/<LOC>/rails.json segments (length, coefficients, row50, distance) equal the runtime
    segment chains of the live paths bit for bit (path object: resource, cursor, segment, length)."""
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    rails = {r['packed_id']: r for r in json.loads((ROOT / f'web/public/assets/{location}/rails.json').read_text())['rails']}
    f32 = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
    checked = 0
    for path in paths:
        seg = u(path + 8)
        while u(seg + 0x60): seg = u(seg + 0x60)
        record = rails[u(path)]
        for part in record['segments']:
            src = part['source']
            words = [f32(part['length_cm'])] + [f32(v) for row in src['coefficients'] for v in row] + [f32(v) for v in src['row50']] + [f32(part['distance_cm'])]
            mem = [u(seg + 0xC)] + [u(seg + 0x10 + 16 * r + 4 * k) for r in range(4) for k in range(3)] + [u(seg + 0x50 + 4 * k) for k in range(4)] + [u(seg + 0x84)]
            if words != mem: raise SystemExit(f'rails.json segment {part["index"]} of {record["name"]} differs from the runtime segment')
            seg = u(seg + 0x64); checked += 1
        if seg: raise SystemExit(f'runtime chain of {record["name"]} longer than the catalog record')
    return checked


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--location', default='BRA2', choices=sorted(GROUPS))
    ap.add_argument('--ticks', type=int, default=None)
    ap.add_argument('--cases', type=int, default=20000)
    a = ap.parse_args()
    world = json.loads((ROOT / f'local/assets/native/{a.location}/world_collision.json').read_text())
    names = {(i['rid'] << 8) | i['track']: i['name'] for i in world['instances']}
    roots = [0x35A560, 0x3568B0, 0x35AC20, 0x345248, 0x35B200, 0x356198, 0x359698]
    roots += vtable_targets([(0x48F168, 0x110), (0x48F250, 0x110), (0x490B10, 0x1A0), (0x490E80, 0x1A0)])
    binary = ROOT / 'build/ssx3_multi_spline_location_live'
    build_live(roots, ROOT / 'tests/multi_spline_location_live.cpp', binary, FOLDER / 'location-live-oracle', cache=ROOT / 'build/multi-spline-live-objects')
    out_dir = activation_dir(a.location); out_dir.mkdir(parents=True, exist_ok=True)
    from set_piece_location import Location
    from export_location_set_pieces import script_calls
    loc = Location(a.location)
    starts = {names[r]: float(k[5][1]) for r, (_, _, _, k) in script_calls(loc, 20).items() if 5 in k and k[5][0] in ('float', 'int')}
    failed = False
    for group, (start, pattern) in GROUPS[a.location].items():
        work = FOLDER / f'{a.location}-{group}'; work.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(start) as z:
            ee = z.read('eeMemory.bin')
            for suffix, member in [('ee', 'eeMemory.bin'), ('vuc', 'vu0MicroMem.bin'), ('vud', 'vu0Memory.bin')]:
                (work / f'start.{suffix}').write_bytes(z.read(member))
        t0 = tick_of(ee)
        found = modifiers(ee, names, starts)
        lines = [f'mod {m:x} {e:x} {n} {d0:x}' for m, e, n, d0 in found]
        spl = splines(ee, names); lines += [f'spline {m:x} {e:x} {n}' for m, e, n in spl]
        later = sorted((p for p in glob.glob(str(RUNS / pattern))), key=lambda p: int(re.search(r'tick(\d+)', p).group(1)))
        horizon = 0
        for p in later:
            t = int(re.search(r'tick(\d+)', p).group(1))
            if t <= t0: continue
            target = work / f'tick{t}.ee'
            with zipfile.ZipFile(p) as z:
                mem = z.read('eeMemory.bin')
            if tick_of(mem) != t: raise SystemExit(f'{p}: tick {tick_of(mem)}')
            target.write_bytes(mem); lines.append(f'target {t - t0} {target}'); horizon = t - t0
        construct = out_dir / f'multi-spline-construct-{group}.json'
        lines.append(f'construct {construct}')
        (work / 'config.txt').write_text('\n'.join(lines) + '\n')
        segments = check_catalog(ee, a.location, sorted({m + 0x48 for m, *_ in found} | {m + 0xD8 for m, *_ in spl}))
        ticks = a.ticks or max(horizon, 600)
        run = subprocess.run([str(binary), str(work), str(ticks), str(a.cases), str(work / 'config.txt')], capture_output=True, text=True, timeout=7200)
        log = f'{a.location} {group}: start {start.name} (tick {t0}), {len(found)} MultiSpline + {len(spl)} Spline modifiers, {ticks} ticks; rails.json matches {segments} runtime path segments bit for bit\n' + run.stdout + run.stderr
        (FOLDER / f'{a.location}-{group}.log').write_text(log)
        print(log, end='')
        # tag the construct file with its source
        data = json.loads(construct.read_text())
        data.update(location=a.location, group=group, source=str(start.relative_to(ROOT)), savestate_tick=t0,
                    modifiers=[dict(name=n, modifier=hex(m), entity=hex(e), d0=d0) for m, e, n, d0 in found])
        construct.write_text(json.dumps(data, indent=1) + '\n')
        failed |= run.returncode != 0
    if failed: raise SystemExit(1)


if __name__ == '__main__':
    main()
