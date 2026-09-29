#!/usr/bin/env python3
"""Repair only terrain base UVs in an existing owned native/browser package."""
import argparse,array,hashlib,json,math,struct
from pathlib import Path
from world_assets import locations,world_chunks,records,patch_texture_uv,patch_mesh,world_vertex_to_native

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--location',default='ARA1');p.add_argument('--apply',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[1];native=root/'local/assets/native'/a.location;browser=root/'web/public/assets'/a.location;source=root/'local/assets/source/ps2'
 meta=json.loads((native/'world.json').read_text());browser_meta=json.loads((browser/'world.json').read_text());ssb=source/'bam.ssb'
 if hashlib.sha256(ssb.read_bytes()).hexdigest()!=meta['source_sha256']:raise ValueError('Wrong original course source')
 before=(native/'vertices.bin').read_bytes()
 if (browser/'vertices.bin').read_bytes()!=before:raise ValueError('Native/browser vertex packages differ before repair')
 vertices=array.array('f');vertices.frombytes(before);indices=array.array('I');indices.frombytes((native/'indices.bin').read_bytes())
 wanted={(x['track'],x['rid']) for x in meta['collision_sources'] if x['kind']=='terrain'};raw={}
 locs=locations(source/'bam.sdb');index=next(i for i,l in enumerate(locs) if l['name']==a.location);begin=locs[index-1]['chunk_end']+1 if index else 0;end=locs[index]['chunk_end']
 for i,chunk in enumerate(world_chunks(ssb)):
  if i>end:break
  if i<begin:continue
  for kind,track,rid,data in records(chunk):
   if kind==1 and (track,rid) in wanted:
    if (track,rid) in raw and raw[track,rid]!=data:raise ValueError('Conflicting original terrain variants')
    raw[track,rid]=data
 if set(raw)!=wanted:raise ValueError('Missing original patch records')
 changed=[];patches=0;seen=set();f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
 for entry in meta['collision_sources']:
  if entry['kind']!='terrain':continue
  data=raw[entry['track'],entry['rid']];n=math.isqrt(entry['triangle_count']//2)
  if 2*n*n!=entry['triangle_count']:raise ValueError('Unexpected patch subdivision')
  points,local_indices,_,_=patch_mesh(data,n);first=entry['first_triangle']*3;actual=indices[first:first+entry['triangle_count']*3];base=min(actual)
  if list(actual)!=[base+i for i in local_indices]:raise ValueError('Terrain vertex provenance mismatch')
  uv=[struct.unpack_from('<2f',data,32+8*i)for i in range(4)]
  for j,point in enumerate(points):
   vertex=base+j
   if vertex in seen:raise ValueError('Shared terrain vertices require explicit split')
   seen.add(vertex);at=vertex*10;expected=world_vertex_to_native(point)
   if list(vertices[at:at+3])!=list(map(f32,expected[:3])):raise ValueError(f'Terrain position/provenance mismatch {entry["track"]}:{entry["rid"]}/{j}')
   u=(j%(n+1))/n;v=(j//(n+1))/n
   legacy=[f32(uv[0][k]*(1-u)*(1-v)+uv[1][k]*u*(1-v)+uv[2][k]*(1-u)*v+uv[3][k]*u*v)for k in range(2)]
   target=list(map(f32,patch_texture_uv(uv,u,v)));old=list(vertices[at+6:at+8])
   if old!=legacy and old!=target:raise ValueError('Existing UVs match neither known basis')
   if old!=target:changed.append([vertex,*old]);vertices[at+6:at+8]=array.array('f',target)
  patches+=1
 after=vertices.tobytes()
 # Authoritative byte-level guard: positions/normals/light UVs cannot change.
 for i in range(0,len(before),40):
  if before[i:i+24]!=after[i:i+24] or before[i+32:i+40]!=after[i+32:i+40]:raise ValueError('Non-UV vertex fields changed')
 report=dict(location=a.location,patches=patches,changed_vertices=len(changed),vertex_count=len(vertices)//10,source_sha256=meta['source_sha256'],before_sha256=hashlib.sha256(before).hexdigest(),after_sha256=hashlib.sha256(after).hexdigest(),non_uv_bytes_unchanged=True,applied=a.apply)
 output=root/'local/browser-validation';output.mkdir(exist_ok=True)
 if a.apply:
  # Store the exact overwritten components for reversibility, not another full mesh.
  if changed:(output/'terrain-uv-before.json').write_text(json.dumps(dict(before_sha256=report['before_sha256'],components=changed)))
  for folder,metadata in [(native,meta),(browser,browser_meta)]:
   temp=folder/'vertices.uv.tmp';temp.write_bytes(after);temp.replace(folder/'vertices.bin')
   metadata['terrain_uv_basis_version']=2;(folder/'world.json').write_text(json.dumps(metadata,separators=(',',':')))
 (output/'terrain-uv-repair.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
