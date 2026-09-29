#!/usr/bin/env python3
"""Add each spline's runtime query flags and surface to rails.json from a verified event savestate.

Usage: export_rail_runtime_flags.py [--location CODE] [rails.json ...]; the savestate is the
location's glide checkpoint (tools/locations.py; Snow Jam: snow-jam-glide.p2s).

The SSB kind-8 record's word at +0x1C is zero on disc; at runtime it holds the query
flags that 0x335128-family rail/handplant queries test (bit0 rail, bit1 handplant).
Moving-set-piece paths (dragon, rocket, raven, spintwin) stay 0 and are never
grindable. Records are located by their unchanged first 28 bytes (packed id + bounds).
"""
import argparse, hashlib, json, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import state as location_state  # noqa: E402
STATE = ROOT / 'local/reference/pcsx2/snow-jam-glide.p2s'


def main(paths, location='ARA1'):
    STATE = location_state(location, 'glide')
    memory = zipfile.ZipFile(STATE).read('eeMemory.bin')
    state_hash = hashlib.sha256(STATE.read_bytes()).hexdigest()
    for path in paths:
        path = Path(path)
        document = json.loads(path.read_text())
        if document.get('location') != location:
            raise ValueError(f'{path}: rails belong to {document.get("location")}, savestate to {location}')
        counts = {}
        for rail in document['rails']:
            key = struct.pack('<I6f', rail['packed_id'], *rail['source']['bounds_min'], *rail['source']['bounds_max'])
            hits, at = [], 0
            while (at := memory.find(key, at)) >= 0:
                if at % 4 == 0 and struct.unpack_from('<I', memory, at + 0x20)[0] == rail['segment_count']:
                    hits.append(at)
                at += 4
            if len(hits) > 1:   # an unpatched copy of the record (EHP3 slides 70..72: +0x1C 0, +0x28 -1 = the disc bytes) besides the live one
                live = [h for h in hits if (struct.unpack_from('<I', memory, h + 0x1C)[0], struct.unpack_from('<i', memory, h + 0x28)[0]) != (0, -1)]
                if len(live) == 1: hits = live
            if len(hits) != 1:
                raise ValueError(f"{rail['name']}: {len(hits)} runtime records")
            flags = struct.unpack_from('<I', memory, hits[0] + 0x1C)[0]
            # 0x30003 rail + handplant, 0x20002 handplant only (BHP1 pipe coping), 0 set piece / never queried.
            if flags not in ((0, 0x30003) if location == 'ARA1' else (0, 0x10001, 0x20002, 0x30003)):
                raise ValueError(f"{rail['name']}: unexpected runtime flags {flags:#x}")
            rail['runtime_flags'] = flags
            # +0x28 surface: -1 on disc, patched by the loader (Snow Jam: 10 metal/fence rails, 9 wood).
            # 0x334680 returns it at +0x4C and 0x106848/0x13AF28 store it to rider+0x438, which the
            # rider FX passes read (sparks 0x2DABC8, snow 0x2DF920).
            rail['runtime_surface'] = struct.unpack_from('<i', memory, hits[0] + 0x28)[0]
            counts[flags] = counts.get(flags, 0) + 1
        document['runtime_flags_source'] = dict(state=STATE.name, state_sha256=state_hash, offset='record+0x1C',
                                                note='bit0 rail query mask 1, bit1 handplant query mask 2',
                                                surface_offset='record+0x28 (runtime_surface)')
        __import__('atomic_write').write_text(path, json.dumps(document, indent=1) + '\n')
        print(path, {hex(k): v for k, v in counts.items()})


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--location', default='ARA1'); p.add_argument('paths', nargs='*'); a = p.parse_args()
    main(a.paths or [ROOT / f'local/assets/native/{a.location}/rails.json', ROOT / f'web/public/assets/{a.location}/rails.json'], a.location)
