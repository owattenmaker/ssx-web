"""Verify the source callback chain for scripted type3 object/particle instances."""
import json,struct,sys,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from world_assets import world_chunks,records
elf=(root/'local/disc/SLUS_207.72').read_bytes();u=lambda a:struct.unpack_from('<I',elf,a-0xff000)[0]
assert u(0x441f38)==0x2fc0d0 and u(0x441f38+15*4)==0x2fd250
assert u(0x490e80+0x134)==0x356a28 and u(0x490e80+0x13c)==0x356a70
assert u(0x48f080+0xa4)==0x360bc8 and u(0x48f080+0xac)==0x360bd0
for pc in [0x360bc8,0x360bd0,0x32e688]:assert u(pc)==0x03e00008 and u(pc+4)==0x0000102d
for ci,c in enumerate(world_chunks(root/'local/assets/source/ps2/bam.ssb')):
 if ci==33:stage=next(d for k,t,r,d in records(c) if k==16);break
scripts=json.loads((root/'local/browser-pickups/ara1-scripts.json').read_text())['programs'];world=json.loads((root/'web/public/assets/ARA1/world_collision.json').read_text());matches=[]
for i in world['instances']:
 d=world['bindings'][str(i['track'])]['descriptors'][i['collision_descriptor']]
 if d['type']!=3 or not d['flags']&0x40000000:continue
 resource=d['resource08'];row=struct.unpack_from('<6I',stage,0x70+(resource>>8)*24)
 if resource&255!=8 or any(row[k]!=0xffffffff for k in [0,1,3,4,5]) or row[2]==0xffffffff:continue
 program=row[2]>>8
 if row[2]&255!=8 or scripts[program]['code_words']!=[0x221,0xf0221,0xff2a]:continue
 matches.append(dict(resource=(i['rid']<<8)|i['track'],name=i['name'],program=program,binding=d['index']))
assert any(i['resource']==30472 for i in matches)
out=root/'local/browser-validation/crashbag-ray-capability.json';out.write_text(json.dumps(dict(elf_sha256=hashlib.sha256(elf).hexdigest(),world_sha256=world['source_sha256'],instances=matches,scope='Source callback/script capability evidence; no general dynamic-world exemption'),indent=2)+'\n')
print('Verified object/particle no-override chain for',len(matches),'authored type3 instances')
