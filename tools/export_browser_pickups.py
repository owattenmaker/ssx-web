#!/usr/bin/env python3
"""Package verified authored boost actions and starting instance state."""
import hashlib,json,struct,zipfile
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1]
import sys;sys.path.insert(0,str(root/'tools'))
from locations import pickups_dir,state as location_state
def extract(location='ARA1'):
 elf=(root/'local/disc/SLUS_207.72').read_bytes()
 if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original executable')
 links=json.loads((pickups_dir(location)/'reward-links.json').read_text())['links']
 with zipfile.ZipFile(location_state(location,'glide' if location=='ARA1' else 'anchor')) as z:ram=z.read('eeMemory.bin')
 bindings=json.loads((pickups_dir(location)/'runtime-bindings.json').read_text())
 if hashlib.sha256(ram).hexdigest()!=bindings['ee_sha256']:raise ValueError('Pickup fixture mismatch')
 records=[];unsupported=[]
 for item in bindings['instances']:
  link=next(x for x in links if x['instance_resource']==item['resource'])
  # Snow Jam: the verified handler programs; other courses: the link's own program (link_pickup_rewards.py checks its reward bytecode).
  expected={5128:(1,28),5384:(2,30)}[item['binding_resource08']] if location=='ARA1' else (link['effect_type'],link['slot2_program'])
  if (link['effect_type'],link['slot2_program'])!=expected or link['amount_literal']!=5 or link['dispatch_builtin']!=27:raise ValueError('Unverified pickup action')
  address=item['instance_address']
  if struct.unpack_from('<I',ram,address+12)[0]!=0:
   # A pickup owned by a live entity (e.g. BRA2 speedboost_1000: flags 0x210325, type-1 LiveComp 0x490B10, a moving
   # set piece) needs the entity's update; not ported, so it is left out and listed (never drawn as a static pickup).
   if location=='ARA1':raise ValueError('Starting pickup has unhandled entity predicate')
   unsupported.append(dict(resource=item['resource'],name=item['name'],flags=struct.unpack_from('<I',ram,address+8)[0],entity=struct.unpack_from('<I',ram,address+12)[0]));continue
  records.append(dict(resource=item['resource'],effect_type=expected[0],amount=5,debounce_ticks=60,completion_mode=3,initial_flags=struct.unpack_from('<I',ram,address+8)[0]))
 if location=='ARA1' and len(records)!=5:raise ValueError('Incomplete authored boost catalog')
 return dict(version=1,elf_sha256=hashlib.sha256(elf).hexdigest(),ee_sha256=bindings['ee_sha256'],items=records,**({'unsupported_entity_pickups':unsupported} if unsupported else {}))
if __name__=='__main__':
 out=root/'local/browser-pickups/browser-pickups.json';out.write_text(json.dumps(extract(),indent=2)+'\n');print(out)
