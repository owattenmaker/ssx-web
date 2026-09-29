#!/usr/bin/env python3
"""Extract original SSX3 authored collision bindings and separate collision meshes.

Binding data is SSB kind16, not inferred from visual-model names. Geometry is
retained in original local Z-up centimeters with original transforms/provenance.
This preparation tool does not execute console code.
"""
import argparse,hashlib,json,math,struct
from pathlib import Path
from world_assets import world_chunks,records,locations,world_resource_names,event_locations
from world_models import Reader,IDENTITY


def bindings(data):
    r=Reader(data)
    base,instance_count,mapping,descriptor_count,offsets=r.read('5I',0x40)
    if instance_count>1000000 or descriptor_count>65536:raise ValueError('Collision binding counts exceed bounds')
    indices=r.read(f'{instance_count}H',mapping)
    relative=list(r.read(f'{descriptor_count}I',offsets))+[mapping-base]
    if relative!=sorted(relative) or relative[0]!=0:raise ValueError('Collision descriptor offset order')
    descriptors=[]
    for i in range(descriptor_count):
        at=base+relative[i];length=relative[i+1]-relative[i]
        if length<16 or (length-16)%12:raise ValueError('Collision descriptor extent')
        kind,flags,resource,parameter=r.read('4I',at)
        nodes=[]
        for j in range((length-16)//12):
            value,aux,node_flags,surface=r.read('2I Hh',at+16+j*12)
            nodes.append(dict(value=value,auxiliary=aux,flags=node_flags,surface_id=surface))
        descriptors.append(dict(index=i,source_offset=at,type=kind,flags=flags,resource08=resource,collision_resource=parameter,
                                parameter=parameter,nodes=nodes,raw_sha256=hashlib.sha256(data[at:at+length]).hexdigest()))
    if any(i>=descriptor_count for i in indices):raise ValueError('Instance collision descriptor index outside array')
    return dict(instance_count=instance_count,descriptors=descriptors,instance_descriptor_indices=indices)


def sphere_tree_masks(data,expected):
    """Original 0x32B620 signed run/literal stream, bounded by authored counts."""
    output=bytearray();at=0
    while at<len(data):
        token=struct.unpack_from('b',data,at)[0];at+=1
        if token==0:
            if len(output)!=expected:raise ValueError('Sphere tree decoded mask count')
            if any(data[at:]):raise ValueError('Nonzero data after sphere tree RLE terminator')
            return list(output)
        length=-token if token<0 else token+1
        if len(output)+length>expected:raise ValueError('Sphere tree RLE exceeds decoded extent')
        if token<0:
            if at+length>len(data):raise ValueError('Truncated sphere tree literal')
            output.extend(data[at:at+length]);at+=length
        else:
            if at>=len(data):raise ValueError('Truncated sphere tree run')
            output.extend([data[at]]*length);at+=1
    if len(output)!=expected:raise ValueError('Sphere tree decoded mask count')
    return list(output)


def sphere_tree_models(data,count,start,metadata):
    r=Reader(data);models=[];at=start
    for index in range(count):
        extra,encoded,compressed,depth=r.read('4I',at)
        if depth>7 or compressed not in (0,1):raise ValueError('Unsupported sphere tree depth/compression')
        expected=sum(8**i for i in range(depth+1))
        center=r.read('3f',at+0x10);levels=[]
        for level in range(depth+1):
            radius,offset,stride=r.read('2fI',at+0x70+level*12)
            if not all(math.isfinite(x) for x in (radius,offset)) or radius<0 or stride!=8**level:
                raise ValueError('Invalid sphere tree level')
            levels.append(dict(radius_cm=radius,child_offset_cm=offset,stride=stride))
        payload_at=at+0x70+len(levels)*12;end=payload_at+encoded+extra
        if end>metadata:raise ValueError('Sphere tree payload crosses metadata')
        payload=data[payload_at:payload_at+encoded]
        masks=sphere_tree_masks(payload,expected) if compressed else list(payload)
        if len(masks)!=expected or not all(math.isfinite(x) for x in center):raise ValueError('Invalid sphere tree contents')
        models.append(dict(index=index,source_offset=at,center_cm=center,levels=levels,masks=masks,
                           encoded_bytes=encoded,extra_bytes=extra,compressed=bool(compressed),
                           encoded_sha256=hashlib.sha256(payload).hexdigest()))
        at=end
    if at!=metadata:raise ValueError('Sphere tree model/metadata boundary')
    return models


def collision_mesh(data):
    r=Reader(data);kind,count,start,metadata,table=r.read('2H3I',0)
    if kind==3:
        if count>4096 or start<16 or table<metadata:raise ValueError('Sphere tree collision header')
        return dict(format=kind,models=sphere_tree_models(data,count,start,metadata),source_sha256=hashlib.sha256(data).hexdigest())
    if kind!=1:return dict(format=kind,unsupported=True,source_sha256=hashlib.sha256(data).hexdigest())
    if count>4096 or start<16:raise ValueError('Collision model header')
    models=[];at=start
    for i in range(count):
        triangles,vertices,index_at,bounds_at,vertex_at,normal_at=r.read('2H4I',at)
        if vertices>256 or triangles>65535:raise ValueError('Collision geometry count')
        indices=list(r.read(f'{triangles*3}B',at+index_at))
        if any(x>=vertices for x in indices):raise ValueError('Collision index outside vertex array')
        points=[r.read('4f',at+vertex_at+j*16) for j in range(vertices)]
        normals=[r.read('4f',at+normal_at+j*16) for j in range(triangles)]
        boxes=[r.read('6f',at+bounds_at+j*24) for j in range((triangles+15)//16)]
        if at+bounds_at+len(boxes)*24>at+vertex_at:raise ValueError('Collision bounds overlap vertices')
        if not all(math.isfinite(x) for v in [*points,*normals,*boxes] for x in v):raise ValueError('Nonfinite collision geometry')
        models.append(dict(index=i,source_offset=at,vertices_cm=points,normals=normals,indices=indices,
                           bounds_per_16_triangles=boxes))
        at=at+normal_at+triangles*16
    if at!=metadata or table<metadata:raise ValueError('Collision geometry/metadata boundary')
    return dict(format=kind,models=models,source_sha256=hashlib.sha256(data).hexdigest())


def model_nodes(data):
    r=Reader(data);count,at=r.read('2I',4)
    if count>4096:raise ValueError('Collision model node count')
    nodes=[]
    for i in range(count):
        parent,draw,collision,matrix=r.read('4I',at+i*16)
        transform=r.read('16f',matrix) if matrix not in (0,0xffffffff) else IDENTITY
        node=dict(index=i,parent=parent,matrix=transform)
        if collision not in (0,0xffffffff):
            node['collision_bounds_cm']=r.read('6f',collision)
            node['collision_header_words']=r.read('5I',collision+24)
        if draw not in (0,0xffffffff):
            node['draw_bounds_cm']=r.read('6f',draw)
            node['draw_flags']=r.u32(draw+24)
        nodes.append(node)
    return nodes


def extract(source,location,event=True):
    locs=locations(source/'bam.sdb');index=next(i for i,l in enumerate(locs) if l['name']==location)
    begin=locs[index-1]['chunk_end']+1 if index else 0;end=locs[index]['chunk_end']
    # Race-event residency (world_assets.event_locations): the course and its connectors, in load order.
    resident=event_locations(locs,location) if event else [(index,location,begin,end)]
    ranges=[(b,e) for _,_,b,e in resident];inside=lambda i:any(b<=i<=e for b,e in ranges)
    names=world_resource_names((source/'bam.phm').read_bytes(),(source/'bam.psm').read_bytes())
    scripts={};instances=[];meshes={};models={}
    for i,chunk in enumerate(world_chunks(source/'bam.ssb')):
        if i>max(e for _,e in ranges):break
        if not inside(i) and i!=0:continue
        for kind,track,rid,data in records(chunk):
            key=f'{track}:{rid}'
            if kind==16 and i!=0:scripts[str(track)]=bindings(data)
            elif kind==12:
                meshes[key]=collision_mesh(data);meshes[key]['name']=names[4].get((track,rid),'')
            elif kind==2:models[key]=dict(name=names[2].get((track,rid),''),nodes=model_nodes(data))
            elif kind==3 and i!=0:
                r=Reader(data);model=r.u32(128)
                instances.append(dict(track=track,rid=rid,name=names[1].get((track,rid),''),model_resource=model,
                                      matrix=r.read('16f',16),scale=r.read('f',132)[0],
                                      bounds_min_cm=r.read('3f',96),bounds_max_cm=r.read('3f',108)))
    for instance in instances:
        table=scripts[str(instance['track'])];rid=instance['rid']
        if rid>=table['instance_count']:raise ValueError('Authored instance outside collision binding table')
        instance['collision_descriptor']=table['instance_descriptor_indices'][rid]
    return dict(version=1,location=location,event_locations=[dict(name=n,track=t,chunks=[b,e]) for t,n,b,e in resident],source_sha256=hashlib.sha256((source/'bam.ssb').read_bytes()).hexdigest(),
                source_axis='Z-up',source_units='centimeters',bindings=scripts,instances=instances,
                collision_meshes=meshes,render_model_nodes=models,
                note='Original authored data only. Types0/1/2/3 select no collision/triangle collision/AABB/sphere-tree geometry in0x334888; dynamic entity callbacks and runtime flags remain to integrate.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--source',type=Path,default=Path('local/assets/source/ps2'))
    parser.add_argument('--location',default='ARA1');parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=extract(args.source,args.location);args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(f"Preserved {len(result['instances'])} authored bindings and {len(result['collision_meshes'])} collision resources")

if __name__=='__main__':main()
