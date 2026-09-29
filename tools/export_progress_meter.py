#!/usr/bin/env python3
"""Course data of the in-race progress meter (HUD flag 0x40, draw 0x20EDA0; docs/slopestyle-bigair.md "Progress meter").

The meter's path is the location's SSB record of kind 21 (loader 0x3AAE68 jump table 0x494FB0 case 21 -> 0x2105D0,
which keeps gp-0x9A0 = record, gp-0x99C = record + [+4], gp-0x998 = record + [+0xC] and resets the per-rider
entries 0x4C8BC8 through 0x2108F8). Record layout (little endian):
  +0x00 u32 segment count N      +0x04 u32 segment offset      +0x08 u32 marker count M
  +0x0C u32 marker offset        +0x10 f32 total length (cm)
  segment (0x14 bytes): f32 nx, ny (lateral axis), px, py (origin), d (distance from the start at the origin)
  marker (8 bytes): u32 type (0 start line, 1 checkpoint, 2 finish; sprites chkstart/chkpt/chkstart), f32 distance
The record is copied verbatim (the R&B savestate holds the same words at gp-0x9A0).

    .venv/bin/python tools/export_progress_meter.py [--location ASS1 ...]
    -> web/public/assets/<X>/progress-meter.json (git-ignored; without --location only for existing course dirs)
"""
import argparse, hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations  # noqa: E402
from locations import web_dir  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
KIND = 21


def parse(data):
    n, seg_off, m, mark_off = struct.unpack_from('<4I', data, 0)
    total, = struct.unpack_from('<f', data, 0x10)
    segments = [list(struct.unpack_from('<5f', data, seg_off + 0x14 * k)) for k in range(n)]
    markers = [[struct.unpack_from('<I', data, mark_off + 8 * k)[0], struct.unpack_from('<f', data, mark_off + 8 * k + 4)[0]] for k in range(m)]
    return dict(total=total, segments=segments, markers=markers)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--location', action='append', help='location code(s); default every location with a kind-21 record')
    args = ap.parse_args()
    locs = locations(SOURCE / 'bam.sdb')
    def owner(i):
        for l in locs:
            if i <= l['chunk_end']: return l['name']
    ssb = SOURCE / 'bam.ssb'; sha = hashlib.sha256(ssb.read_bytes()).hexdigest()
    found = {}
    for i, chunk in enumerate(world_chunks(ssb)):
        for kind, track, rid, data in records(chunk):
            if kind == KIND: found.setdefault(owner(i), (i, track, rid, data))
    for code, (chunk, track, rid, data) in sorted(found.items()):
        if args.location and code not in args.location: continue
        if not args.location and not web_dir(code).is_dir(): continue  # only courses the browser already ships
        out = dict(version=1, location=code, source='SSX3 USA PS2 BAM.SSB', source_sha256=sha, chunk=chunk, track=track, resource=rid,
                   note='SSB kind-21 record (0x2105D0): progress meter path of 0x20EDA0/0x210618/0x210820', **parse(data))
        path = web_dir(code) / 'progress-meter.json'; path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(out, separators=(',', ':')))
        print(code, 'chunk', chunk, 'segments', len(out['segments']), 'markers', out['markers'], 'total', out['total'], '->', path.relative_to(ROOT))


if __name__ == '__main__':
    main()
