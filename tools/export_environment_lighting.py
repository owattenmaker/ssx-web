#!/usr/bin/env python3
"""Export original PS2 CPU environment-colour terrain samples (2EDB20/3889F0).

RGBA alpha remains raw PS2 alpha. Unknown source fetches outside authored
texture bytes are marked invalid, never invented by clamping/wrapping.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from world_assets import world_chunks,records,locations,event_locations

def texel(data,x,y):
 fmt=data[0];w,h=struct.unpack_from('<HH',data,4);size=int.from_bytes(data[1:4],'little')
 if fmt==5:
  at=128+4*(y*w+x)
  if at+4>len(data):return None
  return data[at:at+4]
 if fmt==2:
  row=(y>>6)*32+((y&0x3c)>>1)+(y&1)
  col=(x>>7)*64+((x&0x70)>>1)+((x&7)^((y&2)<<1)^(y&4))
  bits=((row*w//2+col)<<5)+((x&8)<<1)+((y&2)<<2)
 elif fmt==1:
  row=(y>>7)*32+((x&0x60)>>2)+((y&0xc)>>1)+(y&1)
  col=(x>>7)*64+((y&0x70)>>1)+((x&7)^((y&2)<<1)^(y&4))
  bits=((row*w//2+col)<<5)+(x&0x18)+((y&2)<<1)
 else:raise ValueError(f'Unsupported CPU texture format {fmt}')
 at=128+(bits>>3)
 if at>=size:return None
 index=data[at] if fmt==2 else ((data[at]>>(4 if bits&4 else 0))&15)
 if fmt==2:index=(index&7)|((index&8)<<1)|((index&16)>>1)|(index&0xe0)
 at=size+128+index*4
 if at+4>len(data):return None
 return data[at:at+4]

def export(source,output,location):
 locs=locations(source/'bam.sdb')
 # Race-event residency (world_assets.event_locations): the course and its connectors, in load order.
 resident=event_locations(locs,location);ranges=[(b,e)for _,_,b,e in resident];inside=lambda i:any(b<=i<=e for b,e in ranges)
 textures={};patches=[];materials={}
 for i,chunk in enumerate(world_chunks(source/'bam.ssb')):
  if i>max(e for _,e in ranges):break
  if not inside(i) and i!=0:continue
  for kind,track,rid,data in records(chunk):
   if kind in(9,10):textures[kind,rid]=data
   elif kind==0:materials[track,rid]=data
   elif kind==1 and i!=0:patches.append((track,rid,data))
  print('environment chunk',i,'patches',len(patches),flush=True)
 used={};out=[]
 for track,rid,data in patches:
  flags=struct.unpack_from('<I',data,12)[0];refs=struct.unpack_from('<3h',data,416);keys=[]
  for slot,ref in enumerate(refs):
   kind=(flags>>(slot*3))&7
   key=(9 if kind==1 else 10,ref)if kind in(1,5)and ref>=0 else None
   if key is not None and key not in textures:raise ValueError(f'Missing source texture {key}')
   if key is not None:used[key]=len(used)if key not in used else used[key]
   keys.append(used[key]if key is not None else -1)
  source_chunk=struct.unpack_from('<h',data,0x156)[0]
  # +156 is the SSB streaming chunk, not a material RID. The original
  # checks track state6 and chunk state3 (loaded). Native packages retain
  # the complete selected location, so these source chunks are resident.
  eligible=any(b<=source_chunk<e for _,_,b,e in resident)
  out.append(dict(resource=(rid<<8)|track,flags=flags,eligible=eligible,light_uv=struct.unpack_from('<4f',data,16),base_uv=[struct.unpack_from('<2f',data,32+k*8)for k in range(4)],textures=keys,source_chunk=source_chunk,source_sha256=hashlib.sha256(data).hexdigest()))
 output.mkdir(parents=True,exist_ok=True);td=output/'environment-textures';td.mkdir(exist_ok=True);tmeta=[]
 for (kind,rid),index in used.items():
  data=textures[kind,rid];w,h=struct.unpack_from('<HH',data,4);rgba=bytearray();valid=bytearray()
  for y in range(h+1):
   for x in range(w+1):
    pixel=texel(data,x,y);valid.append(pixel is not None);rgba.extend(pixel or b'\0\0\0\0')
  path=f'environment-textures/{index}.rgba';mask=f'environment-textures/{index}.valid';(output/path).write_bytes(rgba);(output/mask).write_bytes(valid)
  tmeta.append(dict(id=index,width=w,height=h,rgba=path,valid=mask,source_kind=kind,source_id=rid,source_format=data[0],source_sha256=hashlib.sha256(data).hexdigest(),unknown_guard_texels=valid.count(0)))
 result=dict(version=1,location=location,source='owned PS2 BAM.SSB',source_sha256=hashlib.sha256((source/'bam.ssb').read_bytes()).hexdigest(),streaming_assumption='All exported source chunks are resident; original track state6/chunk state3 gates.',colour_order='ARGB',raw_alpha=True,multiplier=[1,.550000011920929,.5350000262260437,.550000011920929],air_ambient=[1,1,1,1],air_ratio=[1,1,1,1],patches=out,textures=tmeta)
 (output/'environment-lighting.json').write_text(json.dumps(result,separators=(',',':'))+'\n');print(len(out),'patches',len(tmeta),'textures',sum(p['eligible']for p in out),'eligible')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,default=Path('local/assets/source/ps2'));p.add_argument('--output',type=Path,help='Default: locations.native_dir(location), e.g. local/assets/native/BRA2');p.add_argument('--location',default='ARA1');a=p.parse_args()
 if a.output is None:
  from locations import native_dir;a.output=native_dir(a.location)
 export(a.source,a.output,a.location)
