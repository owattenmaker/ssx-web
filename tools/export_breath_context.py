#!/usr/bin/env python3
"""Recover breath and environment state from the demo's original starting savestate."""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--state',type=Path,default=Path('local/reference/pcsx2/snow-jam-glide.p2s'))
 p.add_argument('--output',type=Path,default=Path('local/browser-validation/breath-context.json'))
 p.add_argument('--rider',type=lambda x:int(x,0),default=0x14701a0)
 p.add_argument('--location',default='ARA1',help='Non-Snow-Jam: FX object discovered, regions from the disc painter (docs/locations.md)')
 a=p.parse_args()
 with zipfile.ZipFile(a.state) as z:ram=z.read('eeMemory.bin')
 def u(addr):return struct.unpack_from('<I',ram,addr)[0]
 def f(addr):return struct.unpack_from('<f',ram,addr)[0]
 rider=a.rider
 # FX pointer recovered in the snow context exporter: rider motion/FX owner+B40.
 candidates=[at for at in range(0x1400000,0x1500000,16) if u(at)==rider and u(at+0x28)>0 and u(at+0x2c)>0 and u(at+0x28)<len(ram) and u(at+0x2c)<len(ram)]
 # Known source instance is checked rather than trusting a match on one pointer.
 fx=0x146fed0 if a.location=='ARA1' else rider-0x2d0  # the FX object precedes the rider (Snow Jam 146FED0 = 14701A0-0x2D0)
 if fx not in candidates:raise ValueError('Starting rider FX object no longer matches the fixture')
 iface=u(rider+0x6c0)
 env_adjust=struct.unpack_from('<h',ram,iface+0x38)[0];env_getter=u(iface+0x3c)
 if env_adjust!=-0x6c0 or env_getter!=0x140b80 or u(env_getter)!=0x03e00008 or u(env_getter+4)!=0x8c82086c:raise ValueError('Unexpected rider environment-index getter')
 env=u(rider+0x86c)
 if env!=0 and a.location=='ARA1':raise ValueError('This exporter supports the verified Snow Jam environment0 only')
 wrapper=u(0x4fa370+env*0xf0+0x20);obj=u(wrapper);vtable=u(obj+4)
 adjust=struct.unpack_from('<h',ram,vtable+0x180)[0];getter=u(vtable+0x184)
 if vtable!=0x484058 or adjust!=0 or getter!=0x2c1608 or u(getter)!=0x03e00008 or u(getter+4)!=0x24820030:raise ValueError('Unexpected environment property reader')
 head=u(rider+0x8a8)
 if head!=5:raise ValueError('Unexpected head bone')
 matrix=u(u(rider+0x780)+0x30)+64*head
 result={'provenance':{'state':str(a.state),'state_sha256':hashlib.sha256(a.state.read_bytes()).hexdigest(),'ram_sha256':hashlib.sha256(ram).hexdigest(),'rider':rider,'fx':fx,'environment_object':obj,'environment_vtable':vtable,'environment_getter':getter},
 'state':{'accumulator':f(fx+0x14),'phase':u(fx+0x18),'effort':f(fx+0x1c),'clock':f(fx+0x20),'duration':f(fx+0x24)},
 'head_bone':head,'head_matrix':[list(struct.unpack_from('<4f',ram,matrix+i*16)) for i in range(4)],
 'environment_index':env,'environment_distance':f(obj),'environment_last_coordinates':[f(wrapper+8),f(wrapper+12)],'environment_defaults':[0,6,0,0,0,10,1,1,0,0,0,1,1,1,1,f(0x4a30f0-0x4290),1,1,1],'environment_properties':{'current':[f(obj+8+i*8) for i in range(19)],'target':[f(obj+12+i*8) for i in range(19)]},
 'environment_interface_slot':{'adjustment':struct.unpack_from('<h',ram,iface+0x38)[0],'target':u(iface+0x3c)},
 'limitations':['Snapshot initialization only. Rider environment-index getter is verified; runtime target changes and blend weight are not recovered by this exporter.']}
 if a.location!='ARA1':
  # The region tree/payloads are the course painter's type-12 section verbatim (verified equal to the Snow Jam
  # RAM descriptor below: nodes, root, origin, scale, outside and all 3 payloads).
  import sys;sys.path.insert(0,str(Path(__file__).resolve().parent))
  from course_painters import course_painter,point_tree
  from import_sky import painted_entries
  section=course_painter(a.location)[4][12];entries=painted_entries(section,20);tree=point_tree(section,len(entries))
  if any(k!=12 for k,_ in entries):raise ValueError('Unexpected environment property type')
  outside=tree['outside_words'][1];outside=-1 if outside==0xffffffff else outside
  result['regions']={'scale':tree['scale'],'origin':tree['origin'],'root':tree['root'],'outside':outside,'nodes':tree['nodes'],'payloads':[{'type':k,'transition':v[0],'values':list(v[1:])} for k,v in entries]}
  result['region_source']={'painter':a.location,'section':12}
  a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(a.output);return
 # Descriptor observed through the actual source2C0778 lookup (see source oracle).
 descriptor=0x122bb04;tree=u(descriptor);count=u(tree+0xc);nodes=u(tree+0x20);records=u(descriptor+8);record_count=u(descriptor+4)
 if not 0<count<=32768 or not 0<record_count<=256:raise ValueError('Invalid environment region counts')
 node_words=[list(struct.unpack_from('<4H',ram,nodes+i*8)) for i in range(count)]
 root=struct.unpack_from('<H',ram,tree+0x14)[0]
 if root>=count:raise ValueError('Invalid environment region root')
 for node in node_words:
  if node[0]&1 and any((child>>1)>=count for child in node):raise ValueError('Invalid environment region child')
 payloads=[]
 for i in range(record_count):
  tag=u(records+8*i);payload=u(records+8*i+4)
  if tag!=12:raise ValueError('Unexpected environment property type')
  payloads.append({'type':tag,'transition':f(payload),'values':[f(payload+4+k*4) for k in range(19)]})
 result['regions']={'scale':f(tree),'origin':[f(tree+4),f(tree+8)],'root':root,'outside':struct.unpack_from('<i',ram,tree+0x1c)[0],'nodes':node_words,'payloads':payloads}
 result['region_source']={'descriptor':descriptor,'tree':tree,'nodes':nodes,'records':records}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(a.output)
if __name__=='__main__':main()
