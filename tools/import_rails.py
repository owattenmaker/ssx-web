#!/usr/bin/env python3
"""Export the authored SSX3 grind-rail splines (SSB record kind 8) to rails.json.

SPDX-License-Identifier: GPL-3.0-only

Evidence (see engine/RAIL_RECOVERY.md): the original world loader 0x26DED8 hands
kind-8 records to constructor 0x341548; each 144-byte segment inside a record is
the runtime type-1 collision object walked by 0x334680/0x335128 (cubic rows at
+0x10..+0x40, bounds at +0x6C..+0x80, descriptor link at +0x68). The rider
attaches through 0x108A48/0x106848 and rides through 0x13AF28.

Everything is retained in source centimeters/Z-up next to the native meters/Y-up
conversion `(x,y,z) -> (x,z,-y) * 0.01`, the same basis as world.json.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from world_assets import world_chunks, records, locations, world_resource_names, event_locations
from world_models import decode_instance, decode_model, transform

RECORD_HEADER = 48
SEGMENT_SIZE = 144
RAIL_KIND = 8


def native_point(p):
    return [p[0] / 100.0, p[2] / 100.0, -p[1] / 100.0]


def native_vector(v):
    return [v[0], v[2], -v[1]]


def evaluate(rows, t):
    """Original 0x335128 evaluation order: M0*t^3 + M1*t^2 + M2*t + M3."""
    return [rows[0][k] * t * t * t + rows[1][k] * t * t + rows[2][k] * t + rows[3][k] for k in range(3)]


def parse_rail_record(track, rid, data):
    if len(data) < RECORD_HEADER or (len(data) - RECORD_HEADER) % SEGMENT_SIZE:
        raise ValueError(f'Rail record {track}:{rid} has an unexpected size {len(data)}')
    packed_id, = struct.unpack_from('<I', data, 0)
    if packed_id != ((rid << 8) | track):
        raise ValueError(f'Rail record {track}:{rid} packed id {packed_id:#x} disagrees with its SSB header')
    bounds_min = list(struct.unpack_from('<3f', data, 4))
    bounds_max = list(struct.unpack_from('<3f', data, 16))
    word1c, count, word24, surface, word2c = struct.unpack_from('<IIIiI', data, 28)
    segments_present = (len(data) - RECORD_HEADER) // SEGMENT_SIZE
    if count != segments_present:
        raise ValueError(f'Rail record {track}:{rid} header count {count} != {segments_present} segments')
    if not all(math.isfinite(v) for v in bounds_min + bounds_max):
        raise ValueError(f'Rail record {track}:{rid} has nonfinite bounds')
    segments = []
    for k in range(count):
        base = RECORD_HEADER + k * SEGMENT_SIZE
        link_words = list(struct.unpack_from('<3I', data, base))
        length, = struct.unpack_from('<f', data, base + 12)
        rows = [list(struct.unpack_from('<4f', data, base + 0x10 + 16 * r)) for r in range(4)]
        row50 = list(struct.unpack_from('<4f', data, base + 0x50))
        previous, following, rail_index = struct.unpack_from('<3i', data, base + 0x60)
        seg_min = list(struct.unpack_from('<3f', data, base + 0x6C))
        seg_max = list(struct.unpack_from('<3f', data, base + 0x78))
        distance, = struct.unpack_from('<f', data, base + 0x84)
        word88, flags = struct.unpack_from('<2I', data, base + 0x88)
        floats = [length, distance] + [v for r in rows for v in r] + row50 + seg_min + seg_max
        if not all(math.isfinite(v) for v in floats):
            raise ValueError(f'Rail {track}:{rid} segment {k} has nonfinite floats')
        if rail_index != rid:
            raise ValueError(f'Rail {track}:{rid} segment {k} carries rail index {rail_index}')
        if length <= 0:
            raise ValueError(f'Rail {track}:{rid} segment {k} has nonpositive length {length}')
        if any(seg_min[a] > seg_max[a] for a in range(3)):
            raise ValueError(f'Rail {track}:{rid} segment {k} has inverted bounds')
        if rows[3][3] != 1.0 or any(r[3] != 0.0 for r in rows[:3]):
            raise ValueError(f'Rail {track}:{rid} segment {k} coefficient w components are not (0,0,0,1)')
        start = rows[3][:3]
        end = evaluate(rows, 1.0)
        raw = data[base:base + SEGMENT_SIZE]
        segments.append(dict(
            index=k, previous_segment=previous, next_segment=following,
            length_cm=length, distance_cm=distance, flags=flags,
            unpatched_words=[hex(w) for w in link_words] + [hex(word88)],
            source=dict(units='centimeters', up_axis='Z',
                        coefficients=[r[:3] for r in rows], row50=row50,
                        start=start, end=end, bounds_min=seg_min, bounds_max=seg_max),
            native=dict(units='meters', up_axis='Y',
                        coefficients=[native_vector([v / 100.0 for v in r[:3]]) for r in rows],
                        start=native_point(start), end=native_point(end),
                        bounds_min=native_point([seg_min[0], seg_max[1], seg_min[2]]),
                        bounds_max=native_point([seg_max[0], seg_min[1], seg_max[2]])),
            raw_sha256=hashlib.sha256(raw).hexdigest()))
    # Chain checks: consecutive segments are C0-continuous and cumulative distances agree.
    for a, b in zip(segments, segments[1:]):
        gap = math.dist(a['source']['end'], b['source']['start'])
        if gap > 1.0:
            raise ValueError(f'Rail {track}:{rid} segments {a["index"]}/{b["index"]} do not chain ({gap:.3f} cm)')
        if abs(a['distance_cm'] + a['length_cm'] - b['distance_cm']) > 0.5:
            raise ValueError(f'Rail {track}:{rid} cumulative distance mismatch at segment {b["index"]}')
    return dict(track=track, rid=rid, packed_id=packed_id, segment_count=count,
                header_words=dict(word1c=word1c, word24=hex(word24), surface_id=surface, word2c=word2c),
                total_length_cm=sum(s['length_cm'] for s in segments),
                source=dict(units='centimeters', up_axis='Z', bounds_min=bounds_min, bounds_max=bounds_max),
                native=dict(units='meters', up_axis='Y',
                            bounds_min=native_point([bounds_min[0], bounds_max[1], bounds_min[2]]),
                            bounds_max=native_point([bounds_max[0], bounds_min[1], bounds_max[2]])),
                segments=segments, raw_sha256=hashlib.sha256(data).hexdigest())


def point_segment_distance(p, a, b):
    ab = [b[k] - a[k] for k in range(3)]
    ap = [p[k] - a[k] for k in range(3)]
    denominator = sum(v * v for v in ab)
    t = 0.0 if denominator == 0 else max(0.0, min(1.0, sum(ap[k] * ab[k] for k in range(3)) / denominator))
    return math.dist(p, [a[k] + ab[k] * t for k in range(3)])


def rail_proximity(rails, instance_points):
    """Distance in centimeters from each rail-named instance centroid to the nearest sampled rail chord."""
    chords = []
    for rail in rails:
        for seg in rail['segments']:
            rows = seg['source']['coefficients']
            rows4 = [r + [0.0] for r in rows[:3]] + [rows[3] + [1.0]]
            samples = [evaluate(rows4, i / 8.0) for i in range(9)]
            chords.extend(zip(samples, samples[1:]))
    result = []
    for name, centroid in instance_points:
        best = min(point_segment_distance(centroid, a, b) for a, b in chords) if chords else float('inf')
        result.append((name, best))
    return result


def export_location(location, source, output, names, event=True):
    locs = locations(source / 'bam.sdb')
    selected = next((i for i, l in enumerate(locs) if l['name'] == location), None)
    if selected is None:
        raise ValueError('Unknown location; choose ' + ', '.join(l['name'] for l in locs))
    # Race-event residency (world_assets.event_locations): the course and its connector
    # locations are loaded together; their splines are live in the event octree too.
    resident = event_locations(locs, location) if event else [(selected, location, locs[selected - 1]['chunk_end'] + 1 if selected else 0, locs[selected]['chunk_end'])]
    inside = lambda i: any(b <= i <= e for _, _, b, e in resident)
    end = max(e for _, _, _, e in resident)
    rails, models, instances = [], {}, []
    record_counts = {}
    for i, chunk in enumerate(world_chunks(source / 'bam.ssb')):
        if i > end:
            break
        for kind, track, rid, data in records(chunk):
            if not inside(i) and i != 0:
                if kind == 2:
                    try:
                        models[track, rid] = decode_model(data)
                    except ValueError:
                        pass
                continue
            record_counts[kind] = record_counts.get(kind, 0) + 1
            if kind == RAIL_KIND:
                rails.append(parse_rail_record(track, rid, data))
            elif kind == 2:
                try:
                    models[track, rid] = decode_model(data)
                except ValueError:
                    pass
            elif kind == 3:
                instances.append((track, rid, decode_instance(data)))
    spline_names = names[3]
    for rail in rails:
        rail['name'] = spline_names.get((rail['track'], rail['rid']), '')
    # Sanity: rail-named scenery should sit near an exported rail.
    keywords = ('rail', 'grind', 'pipe', 'tram', 'lift', 'log')
    instance_points = []
    for track, rid, (model_id, matrix, _, scale) in instances:
        name = names[1].get((track, rid), '') or ''
        model_name = names[2].get(model_id, '') or ''
        label = name or model_name
        if not any(k in label.lower() for k in keywords):
            continue
        model = models.get(model_id)
        if not model:
            continue
        pts = []
        for mesh in model:
            for v in mesh['vertices'][:64]:
                pts.append(transform([x * scale for x in v[:3]], matrix))
        if not pts:
            continue
        centroid = [sum(p[k] for p in pts) / len(pts) for k in range(3)]
        instance_points.append((label, centroid))
    proximity = rail_proximity(rails, instance_points)
    near = sum(1 for _, d in proximity if d <= 300.0)
    summary = dict(rail_named_instances=len(instance_points), within_300cm=near,
                   examples=[dict(name=n, distance_cm=round(d, 1)) for n, d in sorted(proximity, key=lambda x: x[1])[:12]],
                   farthest=[dict(name=n, distance_cm=round(d, 1)) for n, d in sorted(proximity, key=lambda x: -x[1])[:6]])
    document = dict(
        version=1, location=location, source='SSX3 USA PS2 BAM.SSB', source_record_kind=RAIL_KIND,
        event_locations=[dict(name=n, track=t, chunks=[b, e]) for t, n, b, e in resident],
        source_sha256=hashlib.sha256((source / 'bam.ssb').read_bytes()).hexdigest(),
        name_source_sha256={'phm': hashlib.sha256((source / 'bam.phm').read_bytes()).hexdigest(),
                            'psm': hashlib.sha256((source / 'bam.psm').read_bytes()).hexdigest()},
        units='meters', up_axis='Y', source_up_axis='Z',
        coordinate_basis='source (x,y,z) -> native (x,z,-y)', basis_version=1,
        curve='point(t) = c0*t^3 + c1*t^2 + c2*t + c3, t in [0,1], original 0x335128 evaluation order',
        runtime_notes=dict(
            segment_type_1_object='each 144-byte segment is the runtime type-1 collision object; +0..+0xC and +0x68/+0x88 are patched by the loader',
            query_mask='rider rail queries use descriptor flag mask 1 (0x334680 argument a3=1), set at load time; the file header word +0x1C is 0',
            surface='header +0x28 is written to rider+0x438 through query result +0x4C'),
        rail_count=len(rails), segment_count=sum(r['segment_count'] for r in rails),
        source_record_counts={str(k): v for k, v in sorted(record_counts.items())},
        scenery_proximity=summary, rails=rails)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'rails.json').write_text(json.dumps(document, indent=1) + '\n')
    return document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('local/assets/source/ps2'))
    parser.add_argument('--output', type=Path, default=Path('local/assets/native'))
    parser.add_argument('--locations', nargs='+', default=['ARA1', 'BRA2', 'CRA3', 'DRA4', 'ERA5'])
    args = parser.parse_args()
    names = world_resource_names((args.source / 'bam.phm').read_bytes(), (args.source / 'bam.psm').read_bytes())
    for location in args.locations:
        document = export_location(location, args.source, args.output / location, names)
        lengths = [r['total_length_cm'] for r in document['rails']]
        print(f'{location}: {document["rail_count"]} rails, {document["segment_count"]} segments, '
              f'total {sum(lengths) / 100:.1f} m, longest {max(lengths) / 100:.1f} m')
        first = document['rails'][0]
        print(f'  first rail {first["name"] or first["rid"]}: start {first["segments"][0]["native"]["start"]} '
              f'end {first["segments"][-1]["native"]["end"]}')
        prox = document['scenery_proximity']
        print(f'  rail-named scenery instances: {prox["rail_named_instances"]}, within 3 m of a rail: {prox["within_300cm"]}')
        for e in prox['examples'][:4]:
            print(f'    near: {e["name"]} {e["distance_cm"]} cm')
        for e in prox['farthest'][:3]:
            print(f'    far:  {e["name"]} {e["distance_cm"]} cm')


if __name__ == '__main__':
    main()
