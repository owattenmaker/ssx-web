"""Apply independently verified ray-only capabilities to private world packages."""
from pathlib import Path
import json,subprocess,sys,hashlib
root=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'tools/probe_crashbag_ray_capability.py')],check=True)
proof_path=root/'local/browser-validation/crashbag-ray-capability.json';proof=json.loads(proof_path.read_text());by_id={i['resource']:i for i in proof['instances']}
for path in [root/'local/assets/native/ARA1/world_collision.json',root/'web/public/assets/ARA1/world_collision.json']:
 world=json.loads(path.read_text());assert world['source_sha256']==proof['world_sha256'];count=0
 for instance in world['instances']:
  resource=instance['rid']*256+instance['track']
  if resource not in by_id:continue
  d=world['bindings'][str(instance['track'])]['descriptors'][instance['collision_descriptor']]
  assert d['type']==3 and instance['collision_descriptor']==by_id[resource]['binding']
  instance['ray_policy']='sphere-tree-no-override';count+=1
 assert count==len(by_id)
 world['ray_policy_provenance']=dict(elf_sha256=proof['elf_sha256'],proof_sha256=hashlib.sha256(proof_path.read_bytes()).hexdigest())
 path.write_text(json.dumps(world,separators=(',',':'))+'\n')
print('Applied',len(by_id),'ray-only policies to native and browser ARA1 packages')
