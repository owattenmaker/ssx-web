"""Compare two ARMSX2 capture rings of the same scenario record by record (e.g. clamp mode 1 vs 4).

usage: capture_diff.py MANIFEST.capture.json A.bin B.bin

Reports the first record whose bytes differ, which named layout regions differ there, how many
records differ in each region, and the human rider's position gap over the run.
"""
import json
import struct
import sys
from pathlib import Path


def regions(layout, record):
    named = sorted((offset, name) for name, offset in layout.items() if isinstance(offset, int))
    spans = []
    for index, (offset, name) in enumerate(named):
        end = named[index + 1][0] if index + 1 < len(named) else record
        spans.append((name, offset, end))
    return spans


def main():
    manifest = json.loads(Path(sys.argv[1]).read_text())
    record = manifest['record']
    a = Path(sys.argv[2]).read_bytes()
    b = Path(sys.argv[3]).read_bytes()
    count = min(len(a), len(b)) // record
    spans = regions(manifest['layout'], record)
    first = None
    first_regions = {}
    per_region = {}
    for index in range(count):
        ra = a[index * record:(index + 1) * record]
        rb = b[index * record:(index + 1) * record]
        if ra == rb:
            continue
        tick = struct.unpack_from('<I', ra, 4)[0]
        for name, start, end in spans:
            if ra[start:end] != rb[start:end]:
                per_region.setdefault(name, [tick, 0])[1] += 1
                if first is None or first == tick:
                    first_regions[name] = next(o for o in range(start, end) if ra[o] != rb[o]) - start
        if first is None:
            first = tick
    print(json.dumps({'records': count, 'firstDifferentTick': first, 'firstRegions': first_regions,
                      'regions': {name: {'firstTick': v[0], 'records': v[1]} for name, v in sorted(per_region.items(), key=lambda kv: kv[1][0])}},
                     indent=1))
    if count:
        base = manifest['layout']['rider_100_b40'] - 0x100
        # The human rider's position (rider +0x110) and velocity (+0x1E0), source units (cm, cm/s).
        worst = (0.0, None)
        samples = []
        for index in range(count):
            pa = struct.unpack_from('<3f', a, index * record + base + 0x110)
            pb = struct.unpack_from('<3f', b, index * record + base + 0x110)
            gap = sum((x - y) ** 2 for x, y in zip(pa, pb)) ** 0.5
            tick = struct.unpack_from('<I', a, index * record + 4)[0]
            if gap > worst[0]:
                worst = (gap, tick)
            if index in (count // 4, count // 2, 3 * count // 4, count - 1):
                samples.append((tick, round(gap, 3)))
        print('human position gap cm (tick, gap):', samples, 'max', round(worst[0], 3), 'at', worst[1])

if __name__ == '__main__':
    main()
