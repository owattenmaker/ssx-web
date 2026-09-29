#!/usr/bin/env python3
"""PS2 ground truth for the run-time rails of a location (web/rail_dynamic.inc; development export, git-ignored output
local/reference/set-pieces-peak1/<loc>-rails.json): every live RailModifier (vtable 0x4911D0 at +0x08; +0x10/+0x20 bounds,
+0x30 rail, +0x34 node, +0x40 instance, +0x50 rest inverse) and every AnimTeeter (entity vtable 0x4908F8 at object+0x50;
object +0x00..+0x40) of the location's countdown anchor and of each kept savestate of its setpieces run.

usage: export_rail_modifier_snapshots.py --location ASS1 [RUN_GLOB ...]
"""
import argparse, glob, json, re, struct, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import state as location_state  # noqa: E402
GP = 0x4A30F0


def objects(path):
    with zipfile.ZipFile(path) as z: ee = z.read('eeMemory.bin')
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    arr = memoryview(ee).cast('I'); rails, teeters = [], []
    for i in range(0x100000 // 4, len(arr)):
        if arr[i] == 0x4911D0:
            o = i * 4 - 8; inst = u(o + 0x40)
            if not 0x100000 < inst < 0x2000000: continue
            rails.append(dict(owner=u(inst + 0x78), packed=u(o + 0x30), node=u(o + 0x34), bounds=[u(o + 0x10 + 4 * k) for k in range(8)],
                              inverse=[u(o + 0x50 + 4 * k) for k in range(16)]))
        elif arr[i] == 0x4908F8:
            obj = i * 4 - 0x50; entity = obj + 0x44; inst = u(entity + 0x18)
            if not 0x100000 < inst < 0x2000000 or u(inst + 0xC) != entity: continue
            teeters.append(dict(resource=u(inst + 0x78), words=[u(obj + 4 * k) for k in range(17)]))
    tick = u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8)
    return tick, sorted(rails, key=lambda r: (r['owner'], r['packed'])), teeters


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--location', required=True); p.add_argument('runs', nargs='*')
    a = p.parse_args()
    anchor = location_state(a.location, 'anchor'); t0, r0, e0 = objects(anchor)
    out = dict(location=a.location, anchor=dict(state=str(anchor.relative_to(ROOT)), tick=t0, rails=r0, teeters=e0), snapshots={})
    runs = a.runs or [f'local/ps2-capture/runs/setpieces-{a.location.lower()}/full.tick*.p2s']
    for path in sorted({s for g in runs for s in glob.glob(str(ROOT / g))}, key=lambda s: int(re.search(r'tick(\d+)', s).group(1))):
        tick, rails, teeters = objects(path)
        if int(re.search(r'tick(\d+)', path).group(1)) != tick: raise SystemExit(f'{path}: tick {tick}')
        out['snapshots'][str(tick)] = dict(state=str(Path(path).relative_to(ROOT)), rails=rails, teeters=teeters)
    target = ROOT / f'local/reference/set-pieces-peak1/{a.location.lower()}-rails.json'; target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(out, separators=(',', ':')))
    print(json.dumps({'anchor': [len(r0), len(e0)], 'snapshots': {k: [len(v['rails']), len(v['teeters'])] for k, v in out['snapshots'].items()}}))


if __name__ == '__main__':
    main()
