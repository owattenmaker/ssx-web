#!/usr/bin/env python3
"""Build a portable native mesh package from the user's SSX3 PS2 world assets.

SPDX-License-Identifier: GPL-3.0-only
"""
import argparse
import hashlib
import json
import struct
import tempfile
import shutil
import time
from collections import defaultdict
from pathlib import Path
from world_assets import world_chunks, records, locations, event_locations, texture_rgba, patch_mesh, gamecube_cmpr, world_vertex_to_native, world_resource_names, world_model_role
from inspect_disc import Disc, inspect_big, EXPECTED_SHA1
from world_models import decode_model, decode_instance, transform


def extract(iso, target):
    disc = Disc(iso)
    try:
        if hashlib.sha1(disc.file('SLUS_207.72')).hexdigest() != EXPECTED_SHA1:
            raise ValueError('This importer requires the verified USA PS2 release')
        archive = next(e for e in disc.entries if e['path'] == 'DATA/WORLDS/BAM.BIG')
        for e in inspect_big(disc, archive)['files']:
            if e['path'].endswith(('.ssb', '.sdb', '.phm', '.psm')):
                payload = disc.read(e['disc_offset'], e['size'])
                p = target / Path(e['path']).name
                if not p.exists() or p.read_bytes() != payload:
                    p.write_bytes(payload)
    finally:
        disc.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path)
    parser.add_argument('--source', type=Path, default=Path('local/assets/source/ps2'))
    parser.add_argument('--output', type=Path, default=Path('local/assets/native'))
    parser.add_argument('--location', default='ARA1')
    parser.add_argument('--preserve-from',type=Path,help='Preserve non-generated per-location files from this directory')
    parser.add_argument('--subdivisions', type=int, default=8)
    parser.add_argument('--lighting', choices=('ps2', 'gamecube'), default='gamecube')
    parser.add_argument('--gamecube-source', type=Path, default=Path('local/assets/source/gamecube'))
    parser.add_argument('--instances', action=argparse.BooleanOptionalAction, default=True,
                        help='Include original placed world models (default); --no-instances for terrain only')
    parser.add_argument('--event', action=argparse.BooleanOptionalAction, default=True,
                        help='Import every location resident in the race event (the course and its two connector '
                             'locations, world_assets.event_locations; default); --no-event for the course alone')
    args = parser.parse_args()
    args.source.mkdir(parents=True, exist_ok=True)
    if args.iso:
        extract(args.iso, args.source)
    phm=args.source/'bam.phm';psm=args.source/'bam.psm'
    if not phm.is_file() or not psm.is_file():
        raise ValueError('World names are required to separate trigger volumes; rerun with --iso to extract BAM.PHM/BAM.PSM')
    source_names=world_resource_names(phm.read_bytes(),psm.read_bytes())
    locs = locations(args.source / 'bam.sdb')
    selected = next((i for i, l in enumerate(locs) if l['name'] == args.location), None)
    if selected is None:
        parser.error('Unknown location; choose ' + ', '.join(l['name'] for l in locs))
    begin = locs[selected-1]['chunk_end'] + 1 if selected else 0
    end = locs[selected]['chunk_end']
    # Race-event residency: the course plus its connectors, in load order (patch/instance
    # insertion order of the original octree). TRANSP (chunk 0) instances lie ~6 km below
    # the course and the sky location is exported by import_sky.py; both stay excluded.
    resident = event_locations(locs, args.location) if args.event else [(selected, args.location, begin, end)]
    ranges = [(b, e) for _, _, b, e in resident]
    resident_chunk = lambda i: any(b <= i <= e for b, e in ranges)
    args.output.mkdir(parents=True,exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix=f'.{args.location}-',dir=args.output))
    (folder / 'textures').mkdir(parents=True, exist_ok=True)
    textures, patches, kinds = {}, [], defaultdict(int)
    models, instances, materials = {}, [], {}
    model_errors, skipped_instances = {}, []
    last = max(e for _, e in ranges)
    for i, chunk in enumerate(world_chunks(args.source / 'bam.ssb')):
        if i > last:
            break
        if not resident_chunk(i) and i != 0:
            continue
        for kind, track, rid, data in records(chunk):
            kinds[kind] += 1
            if kind in (9, 10) and (kind, rid) in textures and textures[kind, rid] != data:
                # Texture RIDs are global: every location chunk carries identical copies.
                raise ValueError(f'Texture {kind}-{rid} differs between imported chunks')
            if kind in (9, 10):
                textures[kind, rid] = data
            elif kind == 1 and i != 0:
                patches.append((track, rid, data))
            elif kind == 0:
                materials[track,rid]=struct.unpack_from('<h',data)[0]
            elif kind == 2 and args.instances:
                try:
                    models[track,rid]=decode_model(data)
                except ValueError as error:
                    model_errors[f'{track}:{rid}']=str(error)
            elif kind == 3 and args.instances and i != 0:
                instances.append((track,rid,decode_instance(data)))
        print(f'Chunk {i}: {len(patches)} patches, {len(textures)} textures', flush=True)
    batches = defaultdict(lambda: ([], [], []))
    gc_patches, gc_base_textures = {}, {}
    if args.lighting == 'gamecube':
        if not (args.gamecube_source/'bam.gsb').exists():
            from compare_character_assets import big_members
            args.gamecube_source.mkdir(parents=True,exist_ok=True)
            for name,data in big_members(Path('local/gamecube/disc/files/data/worlds/bam.big').read_bytes()):
                if name.endswith(('.gsb','.gdb')):
                    (args.gamecube_source/Path(name).name).write_bytes(data)
        gc_locs=locations(args.gamecube_source/'bam.gdb','>')
        gc_ranges=[]
        for _,name,_,_ in resident:
            gi=next(i for i,l in enumerate(gc_locs) if l['name']==name)
            gc_ranges.append((gc_locs[gi-1]['chunk_end']+1 if gi else 0,gc_locs[gi]['chunk_end']))
        # The atlas packing differs between versions; use each GC patch's actual
        # lighting UVs/RID only after verifying its surface matches the PS2 one.
        for i,chunk in enumerate(world_chunks(args.gamecube_source/'bam.gsb')):
            if i>max(e for _,e in gc_ranges): break
            if not any(b<=i<=e for b,e in gc_ranges): continue
            for kind,track,rid,data in records(chunk,'big'):
                if kind==10: textures[10,rid]=data
                elif kind==1: gc_patches[track,rid]=data
                elif kind==9: gc_base_textures[rid]=data
                elif kind==0 and (track,rid) in materials:
                    if struct.unpack_from('>h',data)[0]!=materials[track,rid]:
                        raise ValueError(f'Cross-platform material texture mismatch {track}:{rid}')
    batch_sources=defaultdict(list)
    from terrain_contact import patch_record
    analytic_patches = []
    patch_metadata = []
    for track, rid, data in patches:
        analytic_patches.append(patch_record(track,rid,data))
        if args.lighting == 'gamecube':
            other=gc_patches.get((track,rid))
            if other is None or len(other)!=430:
                raise ValueError(f'No matching GC lighting for patch {track}:{rid}')
            if struct.unpack_from('<64f',data,64)!=struct.unpack_from('>64f',other,64):
                raise ValueError(f'Cross-platform surface mismatch for patch {track}:{rid}')
            updated=bytearray(data)
            struct.pack_into('<4f',updated,16,*struct.unpack_from('>4f',other,16))
            struct.pack_into('<h',updated,418,struct.unpack_from('>h',other,418)[0])
            data=bytes(updated)
        vertices, indices, tex, light = patch_mesh(data, args.subdivisions)
        vs, ix, colors = batches[tex, light, False]
        base = len(vs)
        batch_sources[tex,light,False].append(dict(first_index=len(ix),index_count=len(indices),kind='terrain',track=track,rid=rid))
        ix.extend(base + i for i in indices)
        vs.extend(vertices)
        colors.extend([(1,1,1,1)]*len(vertices))
        patch_metadata.append(dict(track=track, rid=rid, texture=tex, lightmap=light))
    instance_metadata=[];trigger_volumes=[]
    for track,rid,(model_id,matrix,baked_colors,instance_scale) in instances:
        if model_id not in models:
            skipped_instances.append(dict(track=track,rid=rid,model=model_id))
            continue
        meshes=models[model_id]
        count=sum(len(m['vertices']) for m in meshes)
        # A missing/mismatched color stream must be reported, never misindexed.
        colors_valid=len(baked_colors)==count
        model_name=source_names[2].get(model_id,'')
        instance_name=source_names[1].get((track,rid),'')
        role=world_model_role(model_name)
        instance_metadata.append(dict(track=track,rid=rid,model=model_id,name=instance_name,model_name=model_name,role=role,
                                      color_count=len(baked_colors),vertex_count=count,colors_valid=colors_valid,uniform_scale=instance_scale))
        if role=='trigger_volume':
            points=[];triangles=[]
            for mesh in meshes:
                base=len(points);triangles.extend(base+i for i in mesh['indices'])
                for v in mesh['vertices']:
                    pos=[a/100 for a in transform([x*instance_scale for x in v[:3]],matrix)]
                    points.append([pos[0],pos[2],-pos[1]])
            trigger_volumes.append(dict(track=track,rid=rid,model=list(model_id),name=instance_name,model_name=model_name,
                classification='Authored PHM/PSM model name contains trig/trigger; retained as helper volume',vertices=points,indices=triangles))
            continue
        for mesh_index,mesh in enumerate(meshes):
            if mesh['material'] not in materials:
                raise ValueError(f'Missing instance material {mesh["material"]}')
            tex=materials[mesh['material']]
            vs,ix,colors=batches[tex,-1,True]
            base=len(vs)
            batch_sources[tex,-1,True].append(dict(first_index=len(ix),index_count=len(mesh['indices']),kind='instance',track=track,rid=rid,model=list(model_id),mesh=mesh_index,name=instance_name,model_name=model_name,uniform_scale=instance_scale))
            ix.extend(base+i for i in mesh['indices'])
            for i,v in enumerate(mesh['vertices']):
                pos=[a/100 for a in transform([x*instance_scale for x in v[:3]],matrix)]
                normal=transform([x*(-1 if instance_scale<0 else 1) for x in v[3:6]],matrix,True)
                vs.append(pos+normal+v[6:])
                colors.append(baked_colors[mesh['color_offset']+i] if colors_valid else (1,1,1,1))
    if not batches:
        raise ValueError('Selected location contains no terrain')
    metadata = dict(version=1, terrain_uv_basis_version=2, location=args.location, source='SSX3 USA PS2 BAM.SSB',
                    event_locations=[dict(name=name,track=index,chunks=[b,e]) for index,name,b,e in resident],
                    lighting_source=args.lighting, lighting_verified=args.lighting=='gamecube',
                    source_sha256=hashlib.sha256((args.source/'bam.ssb').read_bytes()).hexdigest(),
                    units='meters', up_axis='Y', source_up_axis='Z',
                    coordinate_basis='source (x,y,z) -> native (x,z,-y)', basis_version=1, vertex_stride=40, patches=patch_metadata,
                    batches=[], collision_sources=[], textures={}, source_record_counts=dict(kinds),instances=instance_metadata,
                    instance_role_version=1, instance_scale_version=1, trigger_volumes=trigger_volumes,
                    name_source_sha256={'phm':hashlib.sha256(phm.read_bytes()).hexdigest(),'psm':hashlib.sha256(psm.read_bytes()).hexdigest()},
                    imported_instance_count=len(instance_metadata)-len(trigger_volumes),
                    imported_trigger_volume_count=len(trigger_volumes),skipped_instances=skipped_instances,
                    unsupported_models=model_errors)
    (folder/'terrain.json').write_text(json.dumps(dict(version=1,location=args.location,units='meters',up_axis='Y',
        basis='source (x,y,z) -> native (x,z,-y)',coefficient_order='u power + 4 * v power',
        source_sha256=metadata['source_sha256'],patches=analytic_patches),indent=2)+'\n')
    if args.lighting=='gamecube':
        metadata['lighting_source_sha256']=hashlib.sha256((args.gamecube_source/'bam.gsb').read_bytes()).hexdigest()
    minimum, maximum = [float('inf')]*3, [-float('inf')]*3
    vertex_count = index_count = 0
    with (folder/'vertices.bin').open('wb') as vf, (folder/'indices.bin').open('wb') as ix, (folder/'colors.bin').open('wb') as cf:
        for (tex, light, is_instance), (vertices, indices, colors) in sorted(batches.items()):
            metadata['batches'].append(dict(first_index=index_count, index_count=len(indices),
                                            texture=tex, lightmap=light,instance=is_instance))
            for entry in batch_sources[tex,light,is_instance]:
                provenance=dict(entry)
                provenance['first_triangle']=(index_count+provenance.pop('first_index'))//3
                provenance['triangle_count']=provenance.pop('index_count')//3
                if provenance['triangle_count']:metadata['collision_sources'].append(provenance)
            # One shared boundary conversion after all original node/instance transforms.
            vertices=[world_vertex_to_native(v) for v in vertices]
            vf.write(b''.join(struct.pack('<10f', *v) for v in vertices))
            cf.write(b''.join(struct.pack('<4f',*v) for v in colors))
            ix.write(struct.pack(f'<{len(indices)}I', *(i+vertex_count for i in indices)))
            for v in vertices:
                for j in range(3):
                    minimum[j], maximum[j] = min(minimum[j], v[j]), max(maximum[j], v[j])
            vertex_count += len(vertices)
            index_count += len(indices)
    used = {(9, t) for t, _, _ in batches} | {(10, l) for _, l, _ in batches if l >= 0}
    missing = []
    for kind, rid in sorted(used):
        key = f'{kind}-{rid}'
        if (kind, rid) not in textures:
            missing.append(key)
            continue
        decoder=gamecube_cmpr if kind==10 and args.lighting=='gamecube' else texture_rgba
        origin='gamecube' if kind==10 and args.lighting=='gamecube' else 'ps2'
        fallback_reason=None
        try:
            w, h, rgba = decoder(textures[kind, rid])
        except ValueError as error:
            alternate=gc_base_textures.get(rid) if kind==9 else None
            if alternate is None or alternate[0]!=30:
                raise ValueError(f'Texture {key}: {error}; header {textures[kind,rid][:16].hex()}') from error
            w,h,rgba=gamecube_cmpr(alternate)
            if (w,h)!=struct.unpack_from('<HH',textures[kind,rid],4):
                raise ValueError(f'Fallback texture dimensions differ: {key}') from error
            origin='gamecube'
            fallback_reason=str(error)
        path = f'textures/{key}.rgba'
        (folder/path).write_bytes(rgba)
        metadata['textures'][key] = dict(width=w, height=h, path=path,source=origin,
                                        fallback_reason=fallback_reason)
    metadata.update(vertex_count=vertex_count, index_count=index_count,
                    bounds=[minimum, maximum], missing_textures=missing,
                    imported_patch_count=len(patches))
    (folder/'world.json').write_text(json.dumps(metadata, indent=2)+'\n')
    if args.instances:
        # Collision classification/geometry is authored separately from rendering.
        # Emit it on clean imports too, rather than relying on a prior sidecar.
        from world_collision import extract as extract_collision_world
        collision_package=extract_collision_world(args.source,args.location,args.event)
        if collision_package['source_sha256']!=metadata['source_sha256']:
            raise ValueError('Render/collision source hash mismatch during import')
        (folder/'world_collision.json').write_text(json.dumps(collision_package,indent=2)+'\n')

    target=args.output/args.location
    preserve=args.preserve_from if args.preserve_from else target
    if preserve.exists():
        for entry in preserve.iterdir():
            destination=folder/entry.name
            if destination.exists():continue
            if entry.is_dir():shutil.copytree(entry,destination)
            else:shutil.copy2(entry,destination)
    backup=None
    if target.exists():
        backup=args.output/f'.{args.location}-previous-{time.time_ns()}'
        target.rename(backup)
    try:
        folder.rename(target)
    except BaseException:
        if backup: backup.rename(target)
        raise
    print(json.dumps({k: metadata[k] for k in ('location','vertex_count','index_count','bounds','missing_textures')}))
    print(f'Native package: {target.resolve()}')


if __name__ == '__main__':
    main()
