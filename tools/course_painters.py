#!/usr/bin/env python3
"""Disc-only helpers shared by the per-location painter exporters (tools/locations.py).

SPDX-License-Identifier: GPL-3.0-only

World painter records (SSB kind 15) exist once per SDB location, connectors included
(e.g. A_ARA1 / ARA1 / ARA1_B, B_BRA2 / BRA2, B_BHP1 / BHP1). The browser port uses the
record of the event course itself (ARA1: chunk 33, track 8), the region the original
level manager resolves while the camera is on the course (2C0778 via the current
region); connector-region painters are not exported, for any location.

Start position (no savestate): the course's SSB kind-14 AIP record ends with a table of
`II6fII` rows (tools/race_event_assets.py `regions`): {slot, kind, x, y, z, dx, dy, dz,
ai/node index, track path}. Kind 0 rows are the start grid. On ARA1 they are the six
grid slots of the countdown savestate (slot 0 at (-131878.6, 13856.3, -228770.8) is the
human, participant 0 at (-131865.2, 13858.5, -228770.8), path 3 = participants'
path_index 3). Kind 1 rows are reset/checkpoint positions.
"""
import struct
from pathlib import Path

from world_assets import world_chunks, records, locations
from import_sky import chunk_range, painter_sections, painted_entries

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'local/assets/source/ps2'


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def course_painter(code, source=SOURCE):
    """The course location's single kind-15 world painter record: (chunk, track, rid, data, sections)."""
    _, begin, end = chunk_range(locations(source / 'bam.sdb'), code)
    found = []
    for index, chunk in enumerate(world_chunks(source / 'bam.ssb')):
        if index > end:
            break
        if index < begin:
            continue
        for kind, track, rid, data in records(chunk):
            if kind == 15 and len(data) >= 64:
                found.append((index, track, rid, data, painter_sections(data)))
    if len(found) > 1:
        raise ValueError(f'{code}: more than one world painter record')
    return found[0] if found else None


def point_tree(section, payload_count):
    """Spatial point tree that follows a painter section's (type, offset) table."""
    header = struct.unpack_from('<I', section)[0]
    scale, x, y = struct.unpack_from('<3f', section, header)
    count = struct.unpack_from('<I', section, header + 12)[0]
    root = struct.unpack_from('<H', section, header + 20)[0]
    if header + 40 + count * 8 > len(section) or root >= count:
        raise ValueError('Invalid painter tree extent')
    nodes = [list(struct.unpack_from('<4H', section, header + 40 + i * 8)) for i in range(count)]
    for node in nodes:
        if node[0] & 1:
            if any(v >> 1 >= count for v in node):
                raise ValueError('Painter child outside tree')
        elif (node[2] | node[3] << 16) != 0xffffffff and (node[2] | node[3] << 16) >= payload_count:
            raise ValueError('Painter leaf outside payloads')
    outside = list(struct.unpack_from('<2I', section, header + 24))
    if outside[0] & 1 or (outside[1] != 0xffffffff and outside[1] >= payload_count):
        raise ValueError('Invalid painter outside leaf')
    return dict(scale=scale, origin=[x, y], root=root, nodes=nodes, outside_words=outside)


def tree_leaf(tree, x, y):
    """2C1CD8 + 2BAF90/2C0A10 (engine/painter_tree.hpp) in float32; returns (payload or None, leaf cell size)."""
    px = f32(f32(x - tree['origin'][0]) * tree['scale'])
    py = f32(f32(y - tree['origin'][1]) * tree['scale'])
    nodes = tree['nodes']
    if not (-1 < px < 32768 and -1 < py < 32768):
        value, size = tree['outside_words'][1], None
    else:
        a, b, index, size = (int(px) << 1) & 0xffff, (int(py) << 1) & 0xffff, tree['root'], 32768
        for _ in range(len(nodes) + 1):
            node = nodes[index]
            if not node[0] & 1:
                break
            index = node[((a >> 15) << 1) | (b >> 15)] >> 1
            a, b, size = (a << 1) & 0xfffc, (b << 1) & 0xfffc, size / 2
        else:
            raise ValueError('Cyclic painter tree')
        value = node[2] | node[3] << 16
    return (None if value == 0xffffffff else value), (None if size is None else size / tree['scale'])


def start_grid(code, source=SOURCE):
    """Kind-0 rows of the course AIP region table (start grid), sorted by slot; [] if none."""
    from race_event_assets import decode_aip
    _, begin, end = chunk_range(locations(source / 'bam.sdb'), code)
    rows = []
    for index, chunk in enumerate(world_chunks(source / 'bam.ssb')):
        if index > end:
            break
        if index < begin:
            continue
        for kind, track, rid, data in records(chunk):
            if kind == 14 and data:
                aip = decode_aip(data)
                rows += [dict(slot=r[0], position=r[2:5], direction=r[5:8], node=r[8], path=r[9], chunk=index, track=track, rid=rid)
                         for r in aip['regions'] if r[1] == 0]
    return sorted(rows, key=lambda r: r['slot'])


def start_selection(code, tree, source=SOURCE, back_cm=(0, 250, 500, 1000, 1500)):
    """Payload selected at the start: grid slot 0 and points behind it (the chase camera
    samples its own X/Y, 15EBBC -> 2ED490), plus every other grid slot. Returns None
    when the course has no start grid."""
    grid = start_grid(code, source)
    if not grid:
        return None
    slot = grid[0]
    x, y = slot['position'][:2]
    dx, dy = slot['direction'][:2]
    norm = (dx * dx + dy * dy) ** 0.5 or 1.0
    behind = [dict(back_cm=d, payload=tree_leaf(tree, x - dx / norm * d, y - dy / norm * d)[0]) for d in back_cm]
    slots = [dict(slot=r['slot'], payload=tree_leaf(tree, *r['position'][:2])[0]) for r in grid]
    payload, cell = tree_leaf(tree, x, y)
    return dict(entry=payload, cell_size_cm=cell, position=slot['position'], direction=slot['direction'],
                aip=dict(chunk=slot['chunk'], track=slot['track'], rid=slot['rid'], path=slot['path']),
                behind=behind, slots=slots,
                uniform=all(b['payload'] == payload for b in behind) and all(s['payload'] == payload for s in slots))
