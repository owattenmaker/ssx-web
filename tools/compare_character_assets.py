#!/usr/bin/env python3
"""Compare matching SSX3 model resources from the user's two releases.

SPDX-License-Identifier: GPL-3.0-only
Header layout reference: SSX-Library, see docs/asset-formats.md.
"""
import argparse
import json
import struct
from collections import Counter
from pathlib import Path
from inspect_disc import Disc, inspect_big
from world_assets import refpack


def big_members(data):
    if data[:4] not in (b'BIGF', b'BIG4'):
        raise ValueError('Unknown BIG format')
    count, end = struct.unpack_from('>II', data, 8)
    if not 16 <= end <= len(data):
        raise ValueError('Invalid BIG directory')
    pos = 16
    for _ in range(count):
        offset, size = struct.unpack_from('>II', data, pos)
        stop = data.find(b'\0', pos+8, end)
        if stop < 0 or offset+size > len(data):
            raise ValueError('Invalid BIG member')
        name = data[pos+8:stop].decode('ascii')
        pos = stop+1
        yield name, data[offset:offset+size]


def model_headers(data, platform):
    endian = '>' if platform == 'gamecube' else '<'
    count, header, base = struct.unpack_from(endian+'HHI', data, 4)
    stride = 96
    if header < 12 or base > len(data) or header+count*stride > base:
        raise ValueError('Invalid model directory')
    result = []
    for i in range(count):
        p = header+i*stride
        name = data[p:p+16].split(b'\0')[0].decode('ascii')
        offset, size = struct.unpack_from(endian+'II', data, p+16)
        if base+offset > len(data):
            raise ValueError('Invalid model data offset')
        if platform == 'ps2':
            weights, refs, groups, bones, materials, ik, morphs, file_id, triangles = struct.unpack_from('<9H', data, p+68)
            row = dict(name=name, triangles=triangles, bones=bones, morphs=morphs, materials=materials)
        else:
            strips, vertices, bones, morphs, weights, meshes, materials, file_id = struct.unpack_from('>8H', data, p+80)
            body = refpack(data[base+offset:base+offset+size])
            strip_offset, = struct.unpack_from('>I', data, p+44)
            vertex_offset, = struct.unpack_from('>I', data, p+48)
            positions = [struct.unpack_from('>3h', body, vertex_offset+v*16) for v in range(vertices)]
            triangles = strip_triangles = 0
            for m in range(meshes):
                _, group_count, _, groups_at = struct.unpack_from('>4I', body, strip_offset+m*16)
                for g in range(group_count):
                    group_at, byte_length = struct.unpack_from('>IH', body, groups_at+g*16)
                    vertex_count, = struct.unpack_from('>H', body, group_at+1)
                    shadow = '_sh' in name.casefold()
                    stride_bytes = 4 if shadow else 8
                    pos_indices = [struct.unpack_from('>H', body, group_at+4+v*stride_bytes+(0 if shadow else 1))[0]
                                   for v in range(vertex_count)]
                    strip_triangles += max(0, vertex_count-2)
                    for v in range(2, vertex_count):
                        a,b,c = (positions[j] for j in pos_indices[v-2:v+1])
                        ab, ac = [b[j]-a[j] for j in range(3)], [c[j]-a[j] for j in range(3)]
                        cross = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
                        triangles += any(cross)
            row = dict(name=name, triangles=triangles, strip_triangles=strip_triangles,
                       vertices=vertices, bones=bones, morphs=morphs, materials=materials)
        result.append(row)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    from disc_paths import ps2_iso;parser.add_argument('--ps2', type=Path, default=ps2_iso())
    parser.add_argument('--gamecube', type=Path, default=Path('local/gamecube/disc/files/data/char/mdlngc.big'))
    parser.add_argument('--output', type=Path, default=Path('local/assets/character-comparison.json'))
    args = parser.parse_args()
    gc = {Path(n).stem.casefold(): (n, d) for n,d in big_members(args.gamecube.read_bytes())}
    disc = Disc(args.ps2)
    rows, comparison = [], Counter()
    try:
        archive = next(e for e in disc.entries if e['path'] == 'DATA/CHAR/MDLPS2.BIG')
        for entry in inspect_big(disc, archive)['files']:
            key = Path(entry['path']).stem.casefold()
            if key not in gc:
                continue
            a = model_headers(disc.read(entry['disc_offset'],entry['size']), 'ps2')
            b = model_headers(gc[key][1], 'gamecube')
            by_name = {m['name']:m for m in b}
            for model in a:
                other = by_name.get(model['name'])
                if other is None:
                    continue
                outcome = 'same' if model['triangles']==other['triangles'] else 'ps2_more' if model['triangles']>other['triangles'] else 'gamecube_more'
                comparison[outcome] += 1
                rows.append(dict(resource=key, model=model['name'], ps2=model, gamecube=other, triangle_comparison=outcome))
    finally:
        disc.close()
    report = dict(note='Triangle count is a screening metric, not a final quality ranking. PS2 header counts vs decoded GameCube nondegenerate triangles. Compare positions, weights, morphs and texture detail before selecting.',
                  comparison=dict(comparison), matched_models=rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(compared=len(rows), **comparison)))


if __name__ == '__main__':
    main()
