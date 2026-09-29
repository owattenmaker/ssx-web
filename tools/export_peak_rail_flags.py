#!/usr/bin/env python3
"""Runtime query flags / surface of every Peak 1 spline (web/public/assets/PEAK1/<LOC>/rails.json).

The SSB kind-8 record's word +0x1C (query flags: bit0 rail, bit1 handplant) and +0x28 (surface) are written by the
loader (disc: 0 / -1). tools/export_rail_runtime_flags.py reads them from ONE event savestate; a streamed location
is resident in different savestates, so this scans every PS2 savestate that holds Peak 1 locations (event glide /
countdown states, the Conquer-the-Mountain free-ride states, local/ps2-capture/peak1/*.p2s) and takes each record
from whichever state has it loaded (all agreeing copies). Records never seen resident keep flags 0x30003 / surface
-1 marked `runtime_flags_source: "default"` (docs/peak-mountain.md gaps).

    .venv/bin/python tools/export_peak_rail_flags.py [--peak N] [--only LOC ...]

--peak 3 (docs/peak3.md): web/public/assets/PEAK3, from the Peak 3 event states and the Peak 3 free-ride states
(local/ps2-capture/peak3/*.p2s, the kept states of local/ps2-capture/runs/peak3/fr-*). An unpatched copy of a record
(disc bytes +0x1C 0 / +0x28 -1, e.g. EHP3's slides 70..72) next to the live one is ignored.
"""
import argparse, glob, hashlib, json, struct, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / 'web/public/assets/PEAK1'
STATES = ['local/reference/pcsx2/snow-jam-glide.p2s', 'local/reference/pcsx2/metro-city-glide-620.p2s', 'local/reference/pcsx2/the-junction-glide.p2s',
          'local/ps2-capture/menus/ctm/state-*.p2s', 'local/ps2-capture/peak1/*.p2s', 'local/reference/pcsx2/*r-and-b*.p2s', 'local/reference/pcsx2/*crows*.p2s',
          'local/reference/pcsx2/*happiness*.p2s']
PEAK_STATES = {2: ['local/reference/pcsx2/ruthless-ridge-glide.p2s', 'local/reference/pcsx2/intimidator-glide.p2s', 'local/reference/pcsx2/style-mile-glide.p2s',
                   'local/reference/pcsx2/launch-time-glide.p2s', 'local/reference/pcsx2/schizophrenia-glide.p2s', 'local/reference/pcsx2/ruthless*.p2s',
                   'local/ps2-capture/runs/peak2/*-full.tick*.p2s', 'local/ps2-capture/peak2/*.p2s', 'local/ps2-capture/runs/peak2/fr-*.tick*.p2s'],   # docs/peak2.md
               3: ['local/reference/pcsx2/gravitude-glide.p2s', 'local/reference/pcsx2/kick-doubt-glide.p2s', 'local/reference/pcsx2/much-2-much-glide.p2s',
                   'local/reference/pcsx2/perpendiculous-glide.p2s', 'local/reference/pcsx2/the-throne-glide.p2s', 'local/ps2-capture/peak3/*.p2s',
                   'local/ps2-capture/runs/peak3/fr-*.tick*.p2s']}


def memories():
    seen = set()
    for pattern in STATES:
        for path in sorted(glob.glob(str(ROOT / pattern))):
            if path in seen:
                continue
            seen.add(path)
            try:
                yield Path(path).name, zipfile.ZipFile(path).read('eeMemory.bin')
            except (KeyError, zipfile.BadZipFile):
                continue


def main():
    global WEB, STATES
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter); p.add_argument('--only', nargs='*')
    p.add_argument('--peak', type=int, default=1, choices=[1, 2, 3])
    a = p.parse_args()
    if a.peak != 1:
        WEB = ROOT / f'web/public/assets/PEAK{a.peak}'; STATES = PEAK_STATES.get(a.peak, [])
    docs = {}
    for folder in sorted(WEB.iterdir()):
        if folder.is_dir() and (folder / 'rails.json').exists() and (not a.only or folder.name in a.only):
            docs[folder.name] = json.loads((folder / 'rails.json').read_text())
    found = {}
    for name, memory in memories():
        for code, doc in docs.items():
            for rail in doc['rails']:
                key = struct.pack('<I6f', rail['packed_id'], *rail['source']['bounds_min'], *rail['source']['bounds_max'])
                at, hits = 0, []
                while (at := memory.find(key, at)) >= 0:
                    if at % 4 == 0 and struct.unpack_from('<I', memory, at + 0x20)[0] == rail['segment_count']:
                        hits.append((struct.unpack_from('<I', memory, at + 0x1C)[0], struct.unpack_from('<i', memory, at + 0x28)[0]))
                    at += 4
                if len(hits) > 1 and any(h != (0, -1) for h in hits):
                    hits = [h for h in hits if h != (0, -1)]   # an unpatched copy of the record beside the live one
                for value in hits:
                    found.setdefault((code, rail['packed_id']), {}).setdefault(value, []).append(name)
    for code, doc in docs.items():
        counts = {}
        for rail in doc['rails']:
            values = found.get((code, rail['packed_id']))
            if values and len(values) != 1:
                raise ValueError(f"{code} {rail['name']}: savestates disagree {values}")
            if values:
                (flags, surface), states = next(iter(values.items()))
                if flags not in (0, 0x10001, 0x20002, 0x30003):
                    raise ValueError(f"{code} {rail['name']}: unexpected runtime flags {flags:#x}")
                rail['runtime_flags'], rail['runtime_surface'], rail['runtime_flags_source'] = flags, surface, sorted(set(states))
            else:
                rail['runtime_flags'], rail['runtime_surface'], rail['runtime_flags_source'] = 0x30003, -1, 'default'
            counts[rail['runtime_flags_source'] != 'default'] = counts.get(rail['runtime_flags_source'] != 'default', 0) + 1
        doc['runtime_flags_note'] = 'record+0x1C / +0x28 read from the PS2 savestates in which the location was resident (tools/export_peak_rail_flags.py)'
        __import__('atomic_write').write_text(WEB / code / 'rails.json', json.dumps(doc, indent=1) + '\n')
        print(code, 'rails', len(doc['rails']), 'from savestates', counts.get(True, 0), 'default', counts.get(False, 0))


if __name__ == '__main__':
    main()
