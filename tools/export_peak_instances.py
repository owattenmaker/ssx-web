#!/usr/bin/env python3
"""Free-ride instance state of every streamed Peak 1 location, from PS2 free-ride savestates (docs/peak-mountain.md).

The event packages hide what the race-event countdown audit says the original never draws (tools/export_event_instances.py);
in free ride the course scripts leave different objects alive (Big Challenge gates and flags drawn, event helpers,
mode fences and start gates as the free-ride slot-1 programs left them). For every PEAK1 location this reads the live
instances (resource lookup table *(*(*(gp+0x16C8))+8), instance +8 runtime flags, +0xC entity) from a Conquer-the-Mountain
free-ride savestate in which the location is resident and ACTIVE (streaming row 0x442168 state 2), and writes
local/event-activation/PEAK1/<LOC>/freeride-instances.json with the draw class of export_event_membership.draw_class.
tools/export_peak_world.py uses it to hide the 'none' instances. Savestates are only read.
"""
import argparse, glob, hashlib, json, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from export_event_membership import draw_class  # noqa: E402
from locations import elf_read  # noqa: E402

GP = 0x4A30F0
STATES = ['local/ps2-capture/menus/fr/*.p2s', 'local/ps2-capture/menus/streamres/*.p2s', 'local/ps2-capture/menus/ctm/state-*.p2s', 'local/ps2-capture/menus/fr-courses/*-freeride.p2s']
# fr-courses: MCOMM Transport -> Freeride arrivals at R&B / Crow's Nest / Metro-City / The Junction, ~3 s after the
# arrival (race tick ~180). They sort last, so they only fill locations no older state has active.
WEB = ROOT / 'web/public/assets/PEAK1'
NAME = 'PEAK1'
# --peak 3 (docs/peak3.md): Peak 3 free-ride states (the CTM arrival at The Throne and the runs from it, the Transport
# arrivals), kept capture states last.
PEAK_STATES = {2: ['local/ps2-capture/peak2/*.p2s', 'local/ps2-capture/runs/peak2/fr-*.tick*.p2s',
                   'local/ps2-capture/menus/fr/*.p2s', 'local/ps2-capture/menus/ctm/state-*.p2s'],   # Peak 2 free ride (docs/peak2.md); Peak 1's hold DRA4_A (A's row)
               3: ['local/ps2-capture/peak3/*.p2s', 'local/ps2-capture/runs/peak3/fr-*.tick*.p2s']}


def main():
    global WEB, NAME, STATES
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--peak', type=int, default=1, choices=[1, 2, 3]); peak = p.parse_args().peak
    if peak != 1: NAME = f'PEAK{peak}'; WEB = ROOT / f'web/public/assets/{NAME}'; STATES = PEAK_STATES.get(peak, [])
    manifest = json.loads((WEB / 'peak.json').read_text())
    by_id = {r['id']: r for r in manifest['streaming']}
    wanted = {l['code']: l for l in manifest['locations']}
    found = {}
    elf_file = lambda a, n: elf_read(a, n)
    for path in sorted((p for pattern in STATES for p in glob.glob(str(ROOT / pattern))), key=lambda p: ('/fr-courses/' in p, p)):
        try:
            memory = zipfile.ZipFile(path).read('eeMemory.bin')
        except (KeyError, zipfile.BadZipFile):
            continue
        u = lambda a: struct.unpack_from('<I', memory, a)[0]
        kind = struct.unpack_from('<b', memory, 0x535C10)[0]
        if kind != 4:
            continue  # free ride only (0x535C10)
        for loc_id in range(50):
            state = u(0x442168 + 16 * loc_id + 8)
            code = by_id.get(loc_id, {}).get('code')
            if state != 2 or code not in wanted or code in found:
                continue
            world = json.loads((WEB / code / 'world_collision.json').read_text())
            table = u(u(u(GP + 0x16C8)) + 8)
            rows = []
            try:
                for source in world['instances']:
                    track, rid = source['track'], source['rid']; resource = rid << 8 | track
                    lookup = u(u(table + track * 4) + 0x1C); raw = u(lookup + rid * 4); address = (raw >> 8) << 2
                    if u(address + 120) != resource:
                        raise ValueError(f'resource identity mismatch {resource}')
                    flags = u(address + 8); entity = u(address + 12)
                    node_type = struct.unpack_from('<h', memory, entity + 16)[0] if entity else None
                    vtable = u(entity + 12) if entity else None
                    rows.append(dict(resource=resource, name=source.get('name'), runtime_flags=flags, node_type=node_type, node_vtable=vtable,
                                     draw=draw_class(elf_file, flags, vtable)))
            except (ValueError, struct.error, IndexError) as error:
                print(f'{Path(path).name}: {code} unreadable ({error})')
                continue
            found[code] = dict(version=1, location=code, savestate=str(Path(path).relative_to(ROOT)), ee_sha256=hashlib.sha256(memory).hexdigest(),
                               kind=kind, course=struct.unpack_from('<b', memory, 0x535C08)[0], instances=rows,
                               hidden=[r['resource'] for r in rows if r['draw'] == 'none'],
                               dead=[r['resource'] for r in rows if r['node_type'] == 6],
                               # DeadNodes (6) and type-16 nodes: their runtime flags route every collision query past them
                               # (the event seeds do the same for the countdown audit, web/world_bridge.cpp)
                               runtime=[[r['resource'], r['runtime_flags']] for r in rows if r['node_type'] in (6, 16)])
            print(f'{code}: {len(rows)} instances, {len(found[code]["hidden"])} hidden, {len(found[code]["dead"])} DeadNodes ({Path(path).name})')
    for code, doc in found.items():
        out = ROOT / f'local/event-activation/{NAME}/{code}'; out.mkdir(parents=True, exist_ok=True)
        (out / 'freeride-instances.json').write_text(json.dumps(doc, indent=1) + '\n')
    missing = sorted(set(wanted) - set(found))
    print('no free-ride savestate with these locations active:', missing)


if __name__ == '__main__':
    main()
