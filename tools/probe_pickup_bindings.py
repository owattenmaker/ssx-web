"""Resolve authored pickup instances and collision bindings in captured EE RAM."""
import json,struct,zipfile,hashlib,sys,argparse
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from locations import location as location_info,pickup_file,pickups_dir,state as location_state
LOCATION=(lambda p:(p.add_argument('--location',default='ARA1'),p.parse_args().location)[1])(argparse.ArgumentParser(description=__doc__));location_info(LOCATION)
STATE=location_state(LOCATION,'glide' if LOCATION=='ARA1' else 'anchor')  # other courses: the race-start (countdown anchor) pickup state
with zipfile.ZipFile(STATE) as z: memory=z.read('eeMemory.bin')
u=lambda p:struct.unpack_from('<I',memory,p)[0]
catalog=json.loads(pickup_file(LOCATION,'catalog').read_text());rows=[]
for item in catalog['items']:
 if item['authored_name_category'] not in ('speedboost','trickboost'):continue
 pattern=struct.pack('<16f',*item['matrix']);hits=[];at=0
 while (at:=memory.find(pattern,at))>=0:
  base=at-16
  if base>=0 and u(base+120)==item['resource']:hits.append(base)
  at+=4
 assert len(hits)==1,(item['name'],hits)
 base=hits[0];binding=u(base+136);expected=item['collision_binding']
 assert [u(binding),u(binding+4),u(binding+8)]==[expected['type'],expected['flags'],expected['resource08']]
 assert struct.unpack_from('<h',memory,binding+26)[0]==-1
 rows.append(dict(name=item['name'],resource=item['resource'],instance_address=base,binding_address=binding,runtime_header=[u(base+i*4) for i in range(4)],binding_type=u(binding),binding_flags=u(binding+4),binding_resource08=u(binding+8),surface_id=-1,**({} if LOCATION=='ARA1' else dict(entity=u(base+12)))))
assert len(rows)==5 or LOCATION!='ARA1'
(pickups_dir(LOCATION)/'runtime-bindings.json').write_text(json.dumps(dict(snapshot=STATE.stem,ee_sha256=hashlib.sha256(memory).hexdigest(),instances=rows),indent=2)+'\n')
print(f'{len(rows)} pickup instances and surface-1 bindings resolved uniquely in original runtime capture')
