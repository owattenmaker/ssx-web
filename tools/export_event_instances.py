#!/usr/bin/env python3
"""Audit original per-instance node state at the verified Snow Jam countdown.

This is captured event evidence, not a general event/free-roam script executor.
"""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
from export_event_membership import draw_class
from locations import location as location_info,state as location_state,activation_dir
ROOT=Path(__file__).resolve().parents[1]
def main(location='ARA1'):
 location_info(location)
 snapshot=location_state(location,'countdown')  # ARA1: countdown-1.p2m2_SaveState.p2s
 memory=zipfile.ZipFile(snapshot).read('eeMemory.bin')
 event=json.loads((ROOT/f'local/assets/native/{location}/event-start.json').read_text())
 digest=hashlib.sha256(memory).hexdigest()
 # rolling starts (backcountry rival events, tools/export_backcountry.py) have no countdown: the ready state (phase 3) is the anchor
 if digest!=event['provenance']['ee_sha256'] or event['clock']['phase']!=(3 if event.get('rolling_start') else 4):raise ValueError('Not the audited event countdown')
 elf=(ROOT/'local/disc/SLUS_207.72').read_bytes()
 if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected executable')
 ph=struct.unpack_from('<I',elf,28)[0];stride,count=struct.unpack_from('<HH',elf,42)
 def file_at(address,size):
  for i in range(count):
   typ,offset,base,_,length,_,_,_=struct.unpack_from('<8I',elf,ph+i*stride)
   if typ==1 and base<=address and address+size<=base+length:return elf[offset+address-base:offset+address-base+size]
  raise ValueError('Unmapped ELF address')
 if file_at(0x4896b8,9)!=b'DeadNode\0':raise ValueError('Type6 allocation label changed')
 #Both original node classes inherit empty draw/contact methods.
 for table in [0x491b00,0x491680]:
  for slot,target in [(0x20,0x360790),(0x140,0x3609f0)]:
   entry=file_at(table+slot,8)
   if struct.unpack_from('<h',entry)[0]!=0 or struct.unpack_from('<I',entry,4)[0]!=target:raise ValueError('Node dispatch changed')
   if struct.unpack('<2I',file_at(target,8))!=(0x03e00008,0):raise ValueError('Node callback is not empty')
 u=lambda address:struct.unpack_from('<I',memory,address)[0]
 worldpath=ROOT/f'local/assets/native/{location}/world_collision.json';world=json.loads(worldpath.read_text())
 table=u(u(u(0x4a30f0+0x16c8))+8);rows=[];runtime=[]
 for source in world['instances']:
  track,rid=source['track'],source['rid'];resource=rid<<8|track
  lookup=u(u(table+track*4)+0x1c);raw=u(lookup+rid*4);address=(raw>>8)<<2
  if u(address+120)!=resource:raise ValueError(f'Resource identity mismatch: {resource}')
  binding=u(address+136);authored=u(binding+4)
  expected=world['bindings'][str(track)]['descriptors'][source['collision_descriptor']]['flags']
  if authored!=expected:raise ValueError('Captured descriptor differs from imported world')
  entity=u(address+12);node_type=struct.unpack_from('<h',memory,entity+16)[0] if entity else None
  node_table=u(entity+12) if entity else None
  if node_type==6 and node_table!=0x491b00:raise ValueError('Unexpected DeadNode class')
  if node_type==19 and node_table!=0x491680:raise ValueError('Unexpected RestoreNode class')
  runtime_flags=u(address+8)
  body_route='static' if runtime_flags&0x20 else 'entity' if runtime_flags&0x40 and entity else 'skip'
  ray_mode2_route='static' if runtime_flags&0x20 else 'entity' if runtime_flags&0x40 else 'skip'
  if ray_mode2_route=='entity' and not entity:raise ValueError('Mode2 ray entity invariant is violated')
  if node_type==6 and body_route!='skip':raise ValueError('Captured DeadNode still eligible for body collection')
  # Original draw eligibility (export_event_membership.draw_class): static collector 22A5A0 needs
  # (flags&3)==3; dynamic entities draw through vtable+0x20 (0x356298 needs flags&4; DeadNode,
  # RestoreNode and type-16 nodes draw nothing); type-10 cloth flags are drawn by cFlagManager.
  draw=draw_class(file_at,runtime_flags,node_table)
  rows.append(dict(body_route=body_route,ray_mode0_route=body_route,ray_mode2_route=ray_mode2_route,draw=draw,resource=resource,name=source.get('name'),authored_flags=authored,runtime_flags=runtime_flags,node_type=node_type,node_vtable=node_table))
  runtime.append(dict(resource=resource,name=source.get('name'),address=address,binding=binding,authored_flags=authored,runtime_flags=runtime_flags,entity=entity))
 result=dict(version=1,location=location,course_sha256=event['course_sha256'],ee_sha256=digest,elf_sha256=hashlib.sha256(elf).hexdigest(),world_package_sha256=hashlib.sha256(worldpath.read_bytes()).hexdigest(),source_sha256=world['source_sha256'],instances=rows,dead_resources=[r['resource'] for r in rows if r['node_type']==6],scope=f'Captured {"Snow Jam" if location=="ARA1" else location} countdown instance ownership. DeadNode draw/contact callbacks are empty. Body collector routing verified at3340F4..3341A8. Mode0/2 ray routing verified at335BB0 and336D64. Static renderer dispatch and event-script execution remain to be integrated.')
 output=activation_dir(location)/'countdown-instances.json';output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2)+'\n')
 # The countdown instances' EE addresses / bindings / entities (tools/export_uv_scroll.py and export_livecomp.py map captured
 # modifier pointers through them): runtime-instances.json.
 (activation_dir(location)/'runtime-instances.json').write_text(json.dumps(dict(snapshot=Path(snapshot).name.split('.')[0],instances=runtime))+'\n')
 print(json.dumps(dict(output=str(output),instances=len(rows),dead_nodes=len(result['dead_resources']))))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--location',default='ARA1');main(p.parse_args().location)
