#!/usr/bin/env python3
"""Export the original SSX3 sky box for a world area into a native package.

SPDX-License-Identifier: GPL-3.0-only
Format references: GlitcherOG/SSX-Library (see docs/asset-formats.md).

Each peak letter owns one sky location in BAM.SDB: ASKY, BSKY, CSKY, DSKY and
ESKY.  A sky location is a single SSB chunk holding nine base textures
(kind 9, track 255), nine materials (kind 0), one MDR model `mdl_?SKY_SkyTop1`
(kind 2) and one placed instance with baked vertex colours (kind 3).  The game
draws that instance every frame translated to the camera position with the
identity rotation (SLUS_207.72 0x353b10, matrix 0x4ff1a0), so the exported
geometry is camera-relative and only 1.5 m across.

The painted fog values (world painter record kind 15, section type 5) of the
area itself are exported alongside because the horizon colour comes from them.
"""
import argparse
import hashlib
import json
import math
import struct
import sys
import time
import zlib
from collections import defaultdict
from pathlib import Path

from world_assets import world_chunks, records, locations, texture_rgba, world_vertex_to_native, world_resource_names
from world_models import decode_model, decode_instance, transform

AREAS = ('ARA1', 'BRA2', 'CRA3', 'DRA4', 'ERA5', 'A', 'BHP1')
# World painter section type ids equal the tWPIGD factory index (ELF 0x2c0408);
# the kind-15 header stores offsets for types 1..13 in order.
PAINTER_TYPES = {1: 'Mix', 2: 'Ambience', 3: 'Speech', 4: 'Camera', 5: 'Fog', 6: 'LightGlow',
                 7: 'ScreenTint', 8: 'SkyBox', 9: 'Sun', 10: 'Surface', 11: 'Lighting', 12: 'Weather', 13: 'Danger'}
# tWPIGD_Fog constructor defaults (0x2bc7c8 reading gp-0x42E4..): near, far, r, g, b.
FOG_DEFAULTS = dict(near_cm=3000.0, far_cm=30000.0, color=[0.43, 0.55, 0.71])


def sky_location_for(area):
    if not area or area[0] not in 'ABCDE':
        raise ValueError(f'No sky location convention for area {area!r}')
    return area[0] + 'SKY'


# Why each non-ARA1 area uses its peak-letter sky (disc evidence, no RAM capture yet):
# the only `mdl_*_skybox_trigger` helper instances (region list loop 0x22de98 switches the
# loaded sky among location-table ids 44..48 = ASKY..ESKY) sit in the race connectors that
# cross an area letter: ARA1_B (A->B), CRA3_D (C->D), DRA4_A (D->A), ERA5_C (E->C). Connectors
# inside one area (B_BRA2, B_BHP1, A_ARA1, ...) carry none, so an area keeps its letter's sky.
SKY_TRIGGER_EVIDENCE = ('peak-letter convention; supported by the disc: skybox trigger instances exist only in the '
                        'area-crossing race connectors ARA1_B (A->B), CRA3_D, DRA4_A, ERA5_C, none inside area {letter} '
                        '(so {area} keeps {sky}); not yet confirmed from a RAM capture')


def sky_selection_evidence(area, sky_name):
    if area == 'ARA1':
        return 'ARA1 confirmed from a PCSX2 RAM capture (SkyBox object 0x542d28 region 10 = ASKY); other areas follow the peak-letter convention'
    return SKY_TRIGGER_EVIDENCE.format(letter=area[0], area=area, sky=sky_name)


def start_fog(area, source, fog, painter):
    """Painted fog entry at the start. ARA1: RAM capture. Other areas: the course fog painter
    tree (tools/course_painters.py) queried at AIP start grid slot 0 and behind it; None
    when the area has no start grid or the fog painter is missing."""
    if area == 'ARA1':
        return dict(entry=0, evidence='PCSX2 ARA1 capture: tWPIGD_Fog objects at 0x56db00 hold near 3000 far 10000 colour (0.70,0.82,1.00)') if fog else None
    if not fog or len(painter) != 1:
        return None
    from course_painters import point_tree, start_selection
    section = painter_sections(painter[0][2])[5]
    selection = start_selection(area, point_tree(section, len(fog)), source)
    if selection is None or selection['entry'] is None:
        return None
    return dict(entry=selection['entry'], position_cm=selection['position'], cell_size_cm=selection['cell_size_cm'],
                uniform_near_start=selection['uniform'],
                evidence='disc: course AIP (SSB kind 14) start grid slot 0 queried in the course Fog painter point tree '
                         '(2C1CD8/2BAF90, tools/course_painters.py); same leaf 0-15 m behind the slot and at every grid slot'
                         if selection['uniform'] else 'disc: course AIP start grid slot 0 in the Fog painter tree; leaf differs nearby, see uniform_near_start')


def chunk_range(locs, name):
    index = next((i for i, l in enumerate(locs) if l['name'] == name), None)
    if index is None:
        raise ValueError(f'Unknown location {name}')
    begin = locs[index - 1]['chunk_end'] + 1 if index else 0
    return index, begin, locs[index]['chunk_end']


def painter_sections(data):
    """Split a kind-15 world painter record into per-type sections."""
    if len(data) < 0x40:
        return {}
    head = struct.unpack_from('<3I', data, 0)
    if head[0] != 0x10 or head[2] != 0x40:
        raise ValueError('Unexpected world painter header')
    offsets = struct.unpack_from('<13I', data, 12)
    valid = sorted(o for o in offsets if o != 0xffffffff)
    for o in valid:
        if o >= len(data):
            raise ValueError('World painter section outside record')
    sections = {}
    for i, o in enumerate(offsets):
        if o == 0xffffffff:
            continue
        end = min([v for v in valid if v > o] + [len(data)])
        sections[i + 1] = data[o:end]
    return sections


def painted_entries(section, payload_words):
    """Return the painted payloads of one section; the spatial quadtree is not decoded."""
    header_size, count, _ = struct.unpack_from('<3I', section, 0)
    if count > 4096 or header_size != 0x14 + 8 * (count - 1) or header_size > len(section):
        raise ValueError('Unexpected world painter section header')
    entries = []
    for i in range(count):
        kind, offset = struct.unpack_from('<2I', section, 12 + 8 * i)
        if offset + 4 * payload_words > len(section):
            raise ValueError('World painter payload outside section')
        entries.append((kind, struct.unpack_from(f'<{payload_words}f', section, offset)))
    return entries


def fog_entries(section):
    result = []
    for kind, values in painted_entries(section, 7):
        if kind != 5:
            raise ValueError('Fog section entry with another type')
        rate, mode, near, far, r, g, b = values
        if not all(math.isfinite(v) for v in values):
            raise ValueError('Nonfinite painted fog value')
        result.append(dict(blend_rate=rate, mode=mode, near_cm=near, far_cm=far, color=[r, g, b]))
    return result


def write_png(path, width, height, rgba):
    if len(rgba) != width * height * 4:
        raise ValueError('PNG payload size mismatch')
    raw = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))

    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def export_area(area, source, output, preview_dir=None, names=None):
    locs = locations(source / 'bam.sdb')
    sky_name = sky_location_for(area)
    sky_index, sky_begin, sky_end = chunk_range(locs, sky_name)
    area_index, area_begin, area_end = chunk_range(locs, area)
    textures, materials, model, model_id, instance, painter = {}, {}, None, None, None, []
    kinds = defaultdict(int)
    sky_track = None
    for i, chunk in enumerate(world_chunks(source / 'bam.ssb')):
        if i > max(sky_end, area_end):
            break
        if sky_begin <= i <= sky_end:
            for kind, track, rid, data in records(chunk):
                kinds[kind] += 1
                if kind == 9:
                    textures[rid] = data
                elif kind == 0:
                    materials[track, rid] = struct.unpack_from('<10h', data)
                elif kind == 2:
                    if model is not None:
                        raise ValueError('Sky location holds more than one model')
                    model, model_id, sky_track = decode_model(data), (track, rid), track
                elif kind == 3:
                    if instance is not None:
                        raise ValueError('Sky location holds more than one instance')
                    instance = (track, rid, decode_instance(data))
        if area_begin <= i <= area_end:
            for kind, track, rid, data in records(chunk):
                if kind == 15 and len(data) > 8:
                    painter.append((track, rid, data))
    if model is None or instance is None:
        raise ValueError(f'{sky_name}: missing sky model or instance')
    track, rid, (instance_model, matrix, baked_colors, scale) = instance
    if instance_model != model_id:
        raise ValueError('Sky instance references another model')
    if any(abs(matrix[k] - (1.0 if k % 5 == 0 else 0.0)) > 1e-6 for k in range(16)) or scale != 1.0:
        raise ValueError('Sky instance is expected to carry the identity transform')
    vertex_total = sum(len(m['vertices']) for m in model)
    if len(baked_colors) != vertex_total:
        raise ValueError(f'Sky baked colour count {len(baked_colors)} != vertex count {vertex_total}')
    model_name = names[2].get(model_id, '') if names else ''
    instance_name = names[1].get((track, rid), '') if names else ''

    batches = defaultdict(lambda: ([], [], [], []))
    for mesh_index, mesh in enumerate(model):
        if mesh['material'] not in materials:
            raise ValueError(f'Missing sky material {mesh["material"]}')
        material = materials[mesh['material']]
        tex = material[0]
        if tex not in textures:
            raise ValueError(f'Sky material references missing texture {tex}')
        vs, ix, colors, sources = batches[tex]
        base = len(vs)
        sources.append(dict(mesh=mesh_index, material=list(mesh['material']), first_index=len(ix), index_count=len(mesh['indices'])))
        ix.extend(base + k for k in mesh['indices'])
        for k, v in enumerate(mesh['vertices']):
            pos = [a / 100 for a in transform([x * scale for x in v[:3]], matrix)]
            normal = transform(v[3:6], matrix, True)
            vs.append(pos + normal + v[6:])
            colors.append(baked_colors[mesh['color_offset'] + k])

    folder = output / area
    folder.mkdir(parents=True, exist_ok=True)
    stage = output / f'.{area}-sky-{time.time_ns()}'
    (stage / 'sky-textures').mkdir(parents=True)
    metadata = dict(version=1, location=area, sky_location=sky_name, sky_location_index=sky_index, sky_chunk=sky_begin,
                    source='SSX3 USA PS2 BAM.SSB', source_sha256=hashlib.sha256((source / 'bam.ssb').read_bytes()).hexdigest(),
                    sky_selection_evidence=sky_selection_evidence(area, sky_name),
                    units='meters', up_axis='Y', source_up_axis='Z', coordinate_basis='source (x,y,z) -> native (x,z,-y)', basis_version=1,
                    vertex_stride=40, color_stride=16, index_format='uint32',
                    model=dict(track=model_id[0], rid=model_id[1], name=model_name), instance=dict(track=track, rid=rid, name=instance_name),
                    draw=dict(anchor='camera_position', rotation='identity', scale=1.0, depth_write=False, draw_order='before_world',
                              evidence='SLUS_207.72 0x353b10: identity matrix 0x4ff1a0 with row 3 replaced by the camera position (0x2d1c20), render-state push with flag 0x5420, disabled by debug toggle gp+0x143C'),
                    materials={f'{t}:{r}': list(m) for (t, r), m in sorted(materials.items())},
                    batches=[], textures={}, source_record_counts=dict(kinds))
    minimum, maximum = [float('inf')] * 3, [-float('inf')] * 3
    vertex_count = index_count = 0
    with (stage / 'sky-vertices.bin').open('wb') as vf, (stage / 'sky-indices.bin').open('wb') as xf, (stage / 'sky-colors.bin').open('wb') as cf:
        for tex, (vertices, indices, colors, sources) in sorted(batches.items()):
            vertices = [world_vertex_to_native(v) for v in vertices]
            metadata['batches'].append(dict(first_index=index_count, index_count=len(indices), texture=tex,
                                            sources=[dict(s, first_index=s['first_index'] + index_count) for s in sources]))
            vf.write(b''.join(struct.pack('<10f', *v) for v in vertices))
            cf.write(b''.join(struct.pack('<4f', *c) for c in colors))
            xf.write(struct.pack(f'<{len(indices)}I', *(k + vertex_count for k in indices)))
            for v in vertices:
                for j in range(3):
                    minimum[j], maximum[j] = min(minimum[j], v[j]), max(maximum[j], v[j])
            vertex_count += len(vertices)
            index_count += len(indices)
    material_by_texture = {m[0]: m for m in materials.values()}
    for tex in sorted(batches):
        w, h, rgba = texture_rgba(textures[tex])
        path = f'sky-textures/9-{tex}.rgba'
        (stage / path).write_bytes(rgba)
        has_alpha = any(rgba[k] != 255 for k in range(3, len(rgba), 4))
        metadata['textures'][f'9-{tex}'] = dict(width=w, height=h, path=path, source='ps2', has_alpha=has_alpha,
                                                source_sha256=hashlib.sha256(textures[tex]).hexdigest())
        for batch in metadata['batches']:
            if batch['texture'] == tex:
                batch.update(material_flags=material_by_texture[tex][7], has_alpha=has_alpha)
        if preview_dir:
            preview_dir.mkdir(parents=True, exist_ok=True)
            write_png(preview_dir / f'{area}-{sky_name}-9-{tex}.png', w, h, rgba)
    metadata['draw']['vertex_color_unity'] = 16 / 31
    metadata['draw']['cull'] = 'none'
    fog, sky_paint = [], []
    for ptrack, prid, data in painter:
        sections = painter_sections(data)
        if 5 in sections:
            fog.extend(dict(e, painter_track=ptrack, painter_rid=prid, entry=k) for k, e in enumerate(fog_entries(sections[5])))
        sky_paint = [list(v) for _, v in painted_entries(sections[8], 2)] if 8 in sections else []
    metadata['fog'] = dict(defaults=FOG_DEFAULTS, painted=fog,
                           painted_skybox_weights=sky_paint,
                           note='Painted entries come from world painter record kind 15 (type 5); their spatial quadtree is not decoded, so the entry active at a given position is unknown except where stated',
                           start=start_fog(area, source, fog, painter))
    if area != 'ARA1' and sky_paint and len(painter) == 1:
        # tWPIGD_SkyBox (type 8) payload = rate, weight; the weight at the start (same tree
        # query as the fog start). ARA1 authors no SkyBox painter.
        from course_painters import point_tree, start_selection
        selection = start_selection(area, point_tree(painter_sections(painter[0][2])[8], len(sky_paint)), source)
        if selection is not None and selection['entry'] is not None:
            metadata['fog']['skybox_start'] = dict(entry=selection['entry'], weights=sky_paint[selection['entry']],
                                                   uniform_near_start=selection['uniform'])
    metadata.update(vertex_count=vertex_count, index_count=index_count, triangle_count=index_count // 3, bounds=[minimum, maximum])
    (stage / 'sky.json').write_text(json.dumps(metadata, indent=2) + '\n')
    # Replace only the sky files inside the existing area package.
    for entry in list(stage.iterdir()):
        target = folder / entry.name
        if entry.is_dir():
            if target.exists():
                for old in target.iterdir():
                    old.unlink()
                target.rmdir()
        elif target.exists():
            target.unlink()
        entry.rename(target)
    stage.rmdir()
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--iso', type=Path, help='Extract BAM.BIG members from the PS2 ISO first')
    parser.add_argument('--source', type=Path, default=Path('local/assets/source/ps2'))
    parser.add_argument('--output', type=Path, default=Path('local/assets/native'))
    parser.add_argument('--location', action='append', help='Area to export (repeatable); default all of AREAS')
    parser.add_argument('--preview-dir', type=Path, help='Write PNG previews of the sky textures here')
    args = parser.parse_args()
    if args.iso:
        from import_world import extract
        args.source.mkdir(parents=True, exist_ok=True)
        extract(args.iso, args.source)
    phm, psm = args.source / 'bam.phm', args.source / 'bam.psm'
    names = world_resource_names(phm.read_bytes(), psm.read_bytes()) if phm.is_file() and psm.is_file() else None
    for area in args.location or AREAS:
        meta = export_area(area, args.source, args.output, args.preview_dir, names)
        print(json.dumps({k: meta[k] for k in ('location', 'sky_location', 'vertex_count', 'triangle_count', 'bounds')}))
        print(f'  textures: {[(k, v["width"], v["height"]) for k, v in meta["textures"].items()]}')
        print(f'  painted fog entries: {len(meta["fog"]["painted"])}')


if __name__ == '__main__':
    sys.exit(main())
