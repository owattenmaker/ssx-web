#!/usr/bin/env python3
"""Export the cutscene props the rider packages do not carry (read-only; docs/cutscenes.md).

  board_PDA_NIS  the handheld the rider checks in the station scenes (transport\\lodge_arr3, hub_trans_arr,
                 endevent_trans_arr). Rider slot 11: hidden by rider init 0x11C61C, shown/hidden by the clip's layer-2
                 flags (the AFL clip events: code 0 at the frame it appears, 1 hides it). One part, part file 11, every
                 vertex skinned 100% to file 0 bone 15 (`handright`), material 'pdas' = board_pdas_a01.

Uses the same GameCube twins, vertex conversion ((x, y, z) -> (x, z, -y)) and PS2-texel texture rule as the FE preview
packages (tools/export_fe_preview.py build()), so the prop binds to an FE preview skeleton with its bind inverses.
Writes (git-ignored) web/public/assets/CUTSCENES/PROPS/pda/: prop.json, vertices.bin (10 floats), indices.bin, morphs.bin
(the flip-open target, weight = the clip's part-11 stream), 9-0.png.
  python3 tools/export_cutscene_props.py
"""
import json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from export_fe_preview import GC, big_members, decode_high_model, decode_rider_texture, refpack, png, ps2_texel_rgba  # noqa: E402

OUT = ROOT / 'web/public/assets/CUTSCENES/PROPS'


def export_pda():
    model = decode_high_model(dict(big_members((GC / 'mdlngc.big').read_bytes()))['board_PDA_NIS.mnf'])
    bones = {(x['file'], x['bone']) for s in model['skin'] for x in s}
    if bones != {(0, 15)} or any(len(s) != 1 or s[0]['weight'] != 1.0 for s in model['skin']):
        raise ValueError(f'PDA skin is not a single rigid hand bone: {bones}')
    data = dict(big_members((GC / 'othertxn.big').read_bytes()))['board_pdas_a01.gsh']
    data = refpack(data) if data[:2] == b'\x10\xfb' else data
    w, h, rgba = decode_rider_texture(data[struct.unpack_from('>I', data, 20)[0]:])
    rgba, texels = ps2_texel_rgba('board_pdas_a01.gsh', w, h, rgba)
    dest = OUT / 'pda'; dest.mkdir(parents=True, exist_ok=True)
    (dest / '9-0.png').write_bytes(png(w, h, rgba))
    verts = [[v[0], v[2], -v[1], v[3], v[5], -v[4]] + v[6:] for v in model['vertices']]
    (dest / 'vertices.bin').write_bytes(b''.join(struct.pack('<10f', *v) for v in verts))
    idx = [i for g in model['material_batches'] for i in g['indices']]
    # the flip-open morph (one target, channel 0 of the clip's part-11 stream), dense per vertex like the FE packages
    morphs = []; blob = bytearray()
    for m in model['morphs']:
        dense = [0.0] * (3 * len(verts)); by = dict(zip(m['positions'], m['deltas']))
        for k, pos in enumerate(model['vertex_positions']):
            d = by.get(pos)
            if d: dense[3 * k:3 * k + 3] = [d[0], d[2], -d[1]]
        morphs.append(dict(channel=m['channel'], offset=len(blob))); blob.extend(struct.pack(f'<{len(dense)}f', *dense))
    (dest / 'morphs.bin').write_bytes(bytes(blob))
    (dest / 'indices.bin').write_bytes(struct.pack(f'<{len(idx)}I', *idx))
    (dest / 'prop.json').write_text(json.dumps(dict(version=1, name='board_PDA_NIS', part_file=model['file'], bone=dict(file=0, index=15, name='handright'),
        vertex_count=len(verts), index_count=len(idx), morphs=morphs, texture=dict(path='9-0.png', width=w, height=h, texel_domain='ps2', resource='board_pdas_a01'),
        visibility='clip events: code 0 shows (at its frame), code 1 hides; hidden when the actor has no such event',
        source_sha256=model['source_sha256']), indent=1))
    return len(verts), len(idx)


if __name__ == '__main__':
    print('pda', export_pda(), '->', OUT)
