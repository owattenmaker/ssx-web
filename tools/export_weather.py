#!/usr/bin/env python3
"""Weather packages for the browser port (docs/weather.md; development export, git-ignored output):

  web/public/assets/<LOC>/weather.json  (ARA1: web/public/assets/ARA1/weather.json)
    regions     the location's world painter section 12 (tWPIGD_Weather, class 0x484058) point tree
    payloads    its payloads {transition, values[19]} (19 Weather properties: 0 snowfall, 1 flake size, 2 snowfall wind,
                3 wind direction (deg), 4 wind speed (km/h, the rider wind push 0x125970), 5 breath, 6 flurries, 7 gravity,
                8 fluff intensity, 9 fluff wind, 10 lightning chance, 11..14 flake A R G B, 15..18 fluff A R G B)
    defaults    the class defaults (reset 2BE108)
    ready       (event courses) the ready savestate = the start of game tick 0: the human's Weather painter (environment
                block rider+0x86C, +0x20 wrapper), the camera's (block 6), the snowfall object's layers (vtable 0x487FD8,
                0x2E4F50 fields), the camera-0 splash (vtable 0x488230), the camera eye (+0x20) and ScreenTint's lightning
                chance gp+0x1524.
  --state PATH --json   print only the state of any race savestate (mid-run seeds: web/stage_world.inc set_world_visual_state
                "weather", tools/export_world_visual_state.py).

Floats are written as their binary32 words (the core reads either).
usage: export_weather.py [--location LOC ...] | --state PATH --json
"""
import argparse, json, struct, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

GP = 0x4A30F0
WEATHER_CLASS, WRAPPER_CLASS, SNOWFALL_CLASS, SPLASH_CLASS = 0x484058, 0x483E00, 0x487FD8, 0x488230
DEFAULTS = [0, 6, 0, 0, 0, 10, 1, 1, 0, 0, 0, 1, 1, 1, 1, None, 1, 1, 1]   # 15 = gp-0x4290 (0.06)


class Mem:
    def __init__(self, path):
        with zipfile.ZipFile(path) as z: self.ee = z.read('eeMemory.bin')
    def u(self, a): return struct.unpack_from('<I', self.ee, a & 0x1FFFFFF)[0]
    def i(self, a): return struct.unpack_from('<i', self.ee, a & 0x1FFFFFF)[0]
    def f(self, a): return struct.unpack_from('<f', self.ee, a & 0x1FFFFFF)[0]
    def find(self, value):
        at = self.ee.find(struct.pack('<I', value))
        while at >= 0:
            if at % 4 == 0: yield at
            at = self.ee.find(struct.pack('<I', value), at + 1)


def painter(m, block_index):
    block = 0x4FA370 + block_index * 0xF0; wrapper = m.u(block + 0x20); obj = m.u(wrapper)
    if m.u(wrapper + 4) != WRAPPER_CLASS or m.u(obj + 4) != WEATHER_CLASS: raise ValueError(f'block {block_index}: not a Weather painter wrapper')
    return dict(block=block_index, distance=m.u(obj), last=[m.u(wrapper + 8), m.u(wrapper + 12)],
                current=[m.u(obj + 8 + 8 * k) for k in range(19)], target=[m.u(obj + 12 + 8 * k) for k in range(19)], shelter=m.u(block + 0x24))


def human_rider(m):
    game = m.u(m.u(m.u(GP - 0x848) + 0x84) + 0x0C)
    for n in range(m.u(game + 0x78)):
        r = m.u(game + 0x28 + 4 * n)
        if m.u(r + 0x874) == 1: return r
    raise ValueError('no human rider (+0x874) in the roster')


def layers(m):
    hits = []
    for at in m.find(SNOWFALL_CLASS):
        o = at - 0xC; slots = [m.u(o + 0x10 + 4 * k) for k in range(12)]
        if any(slots) and all(s == 0 or 0x100000 < s < 0x2000000 for s in slots): hits.append([s for s in slots if s])
    if len(hits) != 1: raise ValueError(f'snowfall object: {len(hits)} candidates')
    out = []
    for L in hits[0]:
        out.append(dict(camera=m.i(L), kind=m.i(L + 4), count=m.i(L + 8), extent=m.u(L + 0xC), speed=m.u(L + 0x10), size=m.u(L + 0x14),
                        alpha=m.u(L + 0x18), gravity=m.u(L + 0x1C), offset=[m.u(L + 0x20 + 4 * k) for k in range(4)],
                        gust_new=[m.u(L + 0x30 + 4 * k) for k in range(4)], gust_old=[m.u(L + 0x40 + 4 * k) for k in range(4)],
                        timer=m.u(L + 0x90), period=m.u(L + 0x94), seeds=[m.u(L + 0x98 + 4 * k) for k in range(3)],
                        colour=[m.u(L + 0xA4 + 4 * k) for k in range(3)], first=m.i(L + 0xB0)))
    return out


def splash(m):
    found = []
    for at in m.find(SPLASH_CLASS):
        o = at - 0xC
        if m.i(o + 0x10) == 0 and 0 <= m.i(o + 0x14) <= 30 and 0 <= m.i(o + 0x18) <= 24: found.append(o)
    if len(found) != 1: raise ValueError(f'camera-0 splash: {len(found)} candidates')
    o = found[0]
    drops = [dict(alive=m.i(d), f=[m.u(d + 4 + 4 * k) for k in range(14)]) for d in (o + 0x1C + 0x3C * n for n in range(m.i(o + 0x14)))]
    crystals = [dict(spawned=m.i(c), alive=m.i(c + 4), f=[m.u(c + 8 + 4 * k) for k in range(17)]) for c in (o + 0x724 + 0x4C * n for n in range(m.i(o + 0x18)))]
    return dict(object=o, camera=0, has_previous=m.i(o + 0x100C), previous=[m.u(o + 0x1010 + 4 * k) for k in range(4)], speed=m.u(o + 0x1020),
                pending=m.u(o + 0x1024), snowfall=m.u(o + 0x1028), drops=drops, crystals=crystals)


def camera_eye(m):
    views = m.u(m.u(m.u(GP - 0x848) + 0x84) + 0x84)
    cam = m.u(views + 4)
    return [m.u(cam + 0x20 + 4 * k) for k in range(4)]


def state(path):
    m = Mem(path); rider = human_rider(m)
    return dict(source=__import__('disc_paths').repo_relative(Path(path).resolve()), rider=painter(m, m.u(rider + 0x86C)), camera=painter(m, 6), layers=layers(m), splash=splash(m),
                eye=camera_eye(m), lightning_chance=m.u(GP + 0x1524), lightning=dict(counter=m.i(GP + 0x15A8), delay=m.i(GP + 0x15C4)))


def package(code):
    from course_painters import course_painter, point_tree
    from import_sky import painted_entries
    p = course_painter(code)
    if not p or 12 not in p[4]: return None
    section = p[4][12]; entries = painted_entries(section, 20); tree = point_tree(section, len(entries))
    if any(k != 12 for k, _ in entries): raise ValueError(f'{code}: Weather section with another type')
    outside = tree['outside_words'][1]
    defaults = list(DEFAULTS); defaults[15] = struct.unpack('<f', struct.pack('<I', 0x3D75C28F))[0]  # gp-0x4290
    return dict(version=1, location=code, source=dict(painter_chunk=p[0], track=p[1]),
                regions=dict(scale=tree['scale'], origin=tree['origin'], root=tree['root'], outside=-1 if outside == 0xffffffff else outside, nodes=tree['nodes']),
                payloads=[dict(transition=v[0], values=list(v[1:])) for _, v in entries], defaults=defaults)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--location', action='append', default=[]); ap.add_argument('--state'); ap.add_argument('--json', action='store_true')
    ap.add_argument('--no-ready', action='store_true', help='regions / payloads only')
    ap.add_argument('--peaks', action='store_true', help='every location of the streamed worlds (web/public/assets/PEAK<n>/<LOC>/weather.json, no ready state)')
    a = ap.parse_args()
    if a.state and a.json: print(json.dumps(state(a.state), separators=(',', ':'))); return
    if a.peaks:
        for world in sorted((ROOT / 'web/public/assets').glob('PEAK[0-9]')):
            for loc in sorted(d for d in world.iterdir() if d.is_dir() and (d / 'fog-tree.json').exists()):
                pkg = package(loc.name)
                if not pkg: print(f'{world.name}/{loc.name}: no Weather painter'); continue
                __import__('atomic_write').write_text(loc / 'weather.json', json.dumps(pkg, separators=(',', ':')) + '\n')
                print(f'{world.name}/{loc.name}: {len(pkg["payloads"])} payloads, wind {max(p["values"][4] for p in pkg["payloads"])} km/h')
        return
    from locations import LOCATIONS, state as location_state
    codes = a.location or [c for c in LOCATIONS]
    for code in codes:
        pkg = package(code)
        if not pkg: print(f'{code}: no Weather painter'); continue
        ready = None
        if not a.no_ready:
            try: path = a.state or location_state(code, 'ready')
            except Exception: path = None
            if path and Path(path).exists():
                try: ready = state(path)
                except ValueError as e: print(f'{code}: ready state not read ({e})')
        if ready: pkg['ready'] = ready
        target = ROOT / 'web/public/assets' / code / 'weather.json'
        target.parent.mkdir(parents=True, exist_ok=True); __import__('atomic_write').write_text(target, json.dumps(pkg, separators=(',', ':')) + '\n')
        print(json.dumps(dict(location=code, payloads=len(pkg['payloads']), ready=bool(ready), wind_kmh=max(p['values'][4] for p in pkg['payloads']),
                              snowfall=max(p['values'][0] for p in pkg['payloads']), lightning=max(p['values'][10] for p in pkg['payloads']))))


if __name__ == '__main__':
    main()
