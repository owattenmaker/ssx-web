#!/usr/bin/env python3
"""Board flex morph targets for the race rider packages (docs/characters.md "Board flex").

The PS2 board (board_BoardFlex<X>, part file 2) is a morph-target part: 8 morphs whose weights the pose blender
30F2B0 writes from the clip's file-2 stream (8 channels) into *(geometry+0x3C) + part+0x8. The race packages
carried the board's rest shape only. This tool adds, per package:

  board-flex.json  {version, resource, file, first_vertex, vertex_count, morph_count, mirror, offsets}
                   mirror = the MNF morph_ids, which the live geometry holds at part+0x40 (Snow Jam Zoe,
                   geometry 0x5DC600 part 2: [4,5,6,7,0,1,2,3]): a mirrored layer's morph i is channel mirror[i].
  board-flex.bin   float32 xyz deltas, dense per board vertex of the package, morph by morph (same frame as
                   vertices.bin: Y-up metres).

The deltas are the PS2's: board_BoardFlex<X>.mpf (MDLPS2.BIG) keeps each morph after every vertex chunk as one
UNPACK V4-8 (VIF cmd 0x6E) {count,0,0,0} then count x {dx,dy,dz,slot}, slot = 3 x the chunk vertex, in 4 mm units
(docs/characters.md "Morph units"). The GameCube MNF twin's deltas (int8 mm) differ from them by up to 0.8 cm (a 0.987 fit
on board A); `gc_check` reports that difference. A PS2 delta lands on every package vertex at its chunk vertex's position.

The board's place in vertices.bin is found by decoding the package's parts in order (tools/export_characters.py
build_package concatenates them) and checked vertex by vertex against vertices.bin.

python3 tools/export_board_flex.py OUT_DIR [--package RIDER_ZOE ...]
Reads web/public/assets (never writes there); writes OUT_DIR/RIDER_<X>/board-flex.{json,bin}.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compare_character_assets import big_members  # noqa: E402
from rider_assets import decode_high_model  # noqa: E402

WEB = ROOT / 'web/public/assets'
GC = ROOT / 'local/gamecube/disc/files/data/char'
PS2_MODELS = ROOT / 'local/assets/source/ps2/mdlps2.big'
BOARD_FILE = 2
PS2_MORPH_UNIT_CM = 0.4


def gc_model(members, resource):
    stem = Path(resource).stem.lower()
    matches = [n for n in members if Path(n).stem.lower() == stem and n.lower().endswith('.mnf')]
    if len(matches) != 1:
        raise ValueError(f'No unique GameCube twin for {resource}: {matches}')
    return decode_high_model(members[matches[0]])


def ps2_morphs(raw):
    """[(chunk position (PS2 cm), {morph: delta cm})] of every chunk vertex of a PS2 MPF model (tools/compare_vertex_precision.py walk)."""
    from world_assets import refpack
    from world_models import Reader, align16
    r = Reader(raw)
    _, header, base = r.read('HHI', 4)
    body = refpack(raw[base + r.u32(header + 16):])
    b = Reader(body)
    groups_at = r.u32(header + 32)
    chunks = []
    for group in range(r.read('H', header + 72)[0]):
        kind, _, _, refs, refs_at = b.read('5I', groups_at + group * 20)
        for ref in range(refs):
            meshes_at, meshes = b.read('2I', refs_at + ref * 8)
            for mesh in range(meshes):
                at, _ = b.read('2I', meshes_at + mesh * 8)
                if at == 0xffffffff:
                    continue
                for _ in range(1024):
                    if b.read('B', at + 31)[0] != 0x6c:
                        break
                    strips, _, attributes, vertices = b.read('4I', at + 48)
                    at += 80 + strips * 16
                    if attributes:
                        at = align16(at + 48 + vertices * 8)
                        at = align16(at + 48 + vertices * 6)
                    if vertices:
                        at += 48
                        stride = 16 if kind == 17 else 12
                        positions = [b.read('3f', at + i * stride) for i in range(vertices)]
                        at = align16(at + vertices * stride)
                        chunks.append((at, positions))
                    at += 32
    ends = [c[0] for c in chunks[1:]] + [len(body)]
    out = []
    for (at, positions), end in zip(chunks, ends):
        packets = []
        k = at
        while k + 4 <= end:
            num = body[k + 2]
            if body[k + 3] == 0x6E and num and body[k + 4] == num - 1:
                rows = [struct.unpack_from('bbbB', body, k + 8 + 4 * j) for j in range(num - 1)]
                if all(s % 3 == 0 and s // 3 < len(positions) for *_, s in rows):
                    unit = PS2_MORPH_UNIT_CM
                    packets.append({slot // 3: (dx * unit, dy * unit, dz * unit) for dx, dy, dz, slot in rows})
                    k += 4 + num * 4
                    continue
            k += 4
        out.append((positions, packets))
    return out


def assign_packets(packets, gc_of, morph_count):
    """The morphs of a chunk's packets: they come in morph order with no index (a chunk lists only the morphs that move
    it), so each takes the morph, in increasing order, whose GameCube deltas at its vertices are nearest."""
    from itertools import combinations
    best = None
    for choice in combinations(range(morph_count), len(packets)):
        cost = 0.0
        for packet, m in zip(packets, choice):
            for vertex, d in packet.items():
                g = gc_of(vertex, m)
                cost += sum((g[j] - d[j]) ** 2 for j in range(3))
        if best is None or cost < best[0]:
            best = (cost, choice)
    return best[1]


def ps2_dense(package, raw, model, vertices, base, n):
    """The PS2 deltas, dense per package board vertex (vertices.bin frame), from the MPF's morph packets."""
    chunks = ps2_morphs(raw)
    morph_count = len(model['morphs'])
    points = []
    for k in range(n):
        x, y, z = struct.unpack_from('<3f', vertices, (base + k) * 40)
        points.append((x * 100, -z * 100, y * 100))
    gc = gc_dense(model)
    dense = [[0.0] * (3 * n) for _ in range(morph_count)]
    seen = [None] * n
    for positions, packets in chunks:
        # each chunk vertex is the package vertices at its position (PS2 cm)
        owners = [[k for k in range(n) if sum((p[j] - points[k][j]) ** 2 for j in range(3)) < 0.01] for p in positions]
        if any(not o for o in owners):
            raise ValueError(f'{package}: a PS2 chunk vertex has no package vertex')
        def gc_of(vertex, m, owners=owners):
            k = owners[vertex][0]
            return tuple(gc[m][3 * k + j] * 100 for j in range(3))
        morphs = assign_packets(packets, gc_of, morph_count)
        for v, owner in enumerate(owners):
            row = {m: packet[v] for packet, m in zip(packets, morphs) if v in packet}
            for k in owner:
                if seen[k] is not None and seen[k] != row:
                    raise ValueError(f'{package}: board vertex {k}: PS2 chunk vertices disagree')
                seen[k] = row
                for m, (dx, dy, dz) in row.items():
                    # PS2 cm Z up -> vertices.bin Y-up metres
                    dense[m][3 * k:3 * k + 3] = [dx / 100, dz / 100, -dy / 100]
    if any(r is None for r in seen):
        raise ValueError(f'{package}: a board vertex is in no PS2 chunk')
    return dense


def package_vertex(model_vertex):
    v = model_vertex
    return [v[0], v[2], -v[1]]


def export(package, members, ps2_members, out):
    folder = WEB / package
    rig = json.loads((folder / 'rider.json').read_text())
    prefix = rig.get('resource_prefix') or rig.get('character') or 'zoe'
    parts = rig.get('parts') or []
    for part in parts:
        # rider_assets.py's Zoe / Mac packages name the part only
        if not part.get('resource') and part.get('part'):
            family = 'board' if part['part'].startswith(('Bindings', 'BoardFlex')) else prefix
            part['resource'] = f'{family}_{part["part"]}.mnf'
    parts = [p for p in parts if p.get('resource')]
    board = [p for p in parts if p.get('morph_count') and Path(p['resource']).stem.lower().startswith('board_boardflex')]
    if not board:
        return f'{package}: no morphing board part'
    vertices = Path(folder / 'vertices.bin').read_bytes()
    count = len(vertices) // 40
    base = 0
    for part in parts:
        model = gc_model(members, part['resource'])
        if part is board[0]:
            break
        base += len(model['vertices'])
    if model['file'] != BOARD_FILE:
        raise ValueError(f'{package}: board part file {model["file"]}')
    n = len(model['vertices'])
    if base + n > count:
        raise ValueError(f'{package}: board vertices outside vertices.bin')
    # vertex check: the package's board slice is the decoded board, converted as build_package converts it
    for k, v in enumerate(model['vertices']):
        stored = struct.unpack_from('<3f', vertices, (base + k) * 40)
        expected = package_vertex(v)
        if any(abs(stored[j] - struct.unpack('<f', struct.pack('<f', expected[j]))[0]) > 1e-6 for j in range(3)):
            raise ValueError(f'{package}: vertex {base + k} is not board vertex {k}')
    resource = board[0]['resource']
    mpf = [name for name in ps2_members if name.lower() == Path(resource).with_suffix('.mpf').name.lower()]
    if len(mpf) != 1:
        raise ValueError(f'{package}: no PS2 model for {resource}')
    morph_count = len(model['morphs'])
    source = 'PS2 MPF morph packets (4 mm units)'
    try:
        dense = ps2_dense(package, ps2_members[mpf[0]], model, vertices, base, n)
    except ValueError as error:
        # BoardFlexB: package vertices at one position whose PS2 chunk vertices differ (a morphed and an unmorphed copy)
        print(f'{package}: GameCube deltas ({error})')
        source = 'GameCube MNF twin (int8 mm): ' + str(error)
        dense = [[0.0] * (3 * n) for _ in range(morph_count)]
        for m, row in enumerate(gc_dense(model)):
            for k in range(n):
                d = row[3 * k:3 * k + 3]
                dense[m][3 * k:3 * k + 3] = [d[0], d[2], -d[1]]
    gc_worst = gc_check(model, dense)
    blob = bytearray()
    offsets = []
    for row in dense:
        offsets.append(len(blob))
        blob.extend(struct.pack(f'<{len(row)}f', *row))
    meta = dict(version=1, resource=board[0]['resource'], file=BOARD_FILE, first_vertex=base, vertex_count=n,
                morph_count=morph_count, mirror=[m['channel'] for m in model['morphs']], offsets=offsets,
                source_sha256=board[0].get('source_sha256'), frame='vertices.bin (Y-up metres)', deltas=source)
    dest = out / package
    dest.mkdir(parents=True, exist_ok=True)
    (dest / 'board-flex.json').write_text(json.dumps(meta) + '\n')
    (dest / 'board-flex.bin').write_bytes(bytes(blob))
    return f'{package}: {meta["resource"]} vertices {base}..{base + n} morphs {morph_count} mirror {meta["mirror"]} GC twin within {gc_worst:.2f} cm'


def gc_dense(model):
    """The GameCube twin's deltas, dense per model vertex, in the MNF frame (Z up, metres: the PS2 frame's axes)."""
    n = len(model['vertices'])
    out = []
    for record in model['morphs']:
        row = [0.0] * (3 * n)
        by_position = dict(zip(record['positions'], record['deltas']))
        for k, position in enumerate(model['vertex_positions']):
            d = by_position.get(position)
            if d:
                # decode_high_model's deltas are in the MNF's Z-up frame (metres): the PS2 frame's axes
                row[3 * k:3 * k + 3] = d
        out.append(row)
    return out


def gc_check(model, dense):
    """The largest component difference (cm) between the PS2 deltas and the GameCube twin's."""
    gc = gc_dense(model)
    worst = 0.0
    for m in range(len(gc)):
        for k in range(len(model['vertices'])):
            d = gc[m][3 * k:3 * k + 3]
            ours = (d[0], d[2], -d[1])
            worst = max(worst, max(abs(ours[j] - dense[m][3 * k + j]) * 100 for j in range(3)))
    return worst


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--package', action='append', default=[])
    args = ap.parse_args()
    members = dict(big_members((GC / 'mdlngc.big').read_bytes()))
    ps2_members = dict(big_members(PS2_MODELS.read_bytes()))
    packages = args.package or sorted(p.name for p in WEB.glob('RIDER_*') if (p / 'rider.json').exists())
    out = Path(args.out)
    for package in packages:
        try:
            print(export(package, members, ps2_members, out))
        except (ValueError, KeyError) as error:
            print(f'{package}: skipped ({error})')


if __name__ == '__main__':
    main()
