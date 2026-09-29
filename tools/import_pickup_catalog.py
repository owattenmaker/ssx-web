"""Inventory authored course pickup candidates (--location, default ARA1) without inventing pickup mechanics."""
import json,hashlib,math,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
import argparse
from locations import location as location_info,pickup_file,pickups_dir
LOCATION=(lambda p:(p.add_argument('--location',default='ARA1'),p.parse_args().location)[1])(argparse.ArgumentParser(description=__doc__));location_info(LOCATION)
path=root/f'web/public/assets/{LOCATION}/world_collision.json';world=json.loads(path.read_text())
render=json.loads((root/f'web/public/assets/{LOCATION}/world.json').read_text())
visuals={(i['track'],i['rid']):i for i in render['instances']}
items=[]
for instance in world['instances']:
 name=instance['name'].lower()
 kind=next((tag for tag in ['trickboost','speedboost','collectabreak','collecta'] if tag in name),None)
 if kind is None:continue
 track,rid=instance['track'],instance['rid'];descriptor=world['bindings'][str(track)]['descriptors'][instance['collision_descriptor']]
 position=instance['matrix'][12:15];assert all(math.isfinite(x) for x in position)
 visual=visuals.get((track,rid));assert visual is not None,f'Missing authored pickup render instance: {instance["name"]}'
 items.append(dict(name=instance['name'],authored_name_category=kind,resource=(rid<<8)|track,model_resource=instance['model_resource'],position_cm=position,position_m=[position[0]/100,position[2]/100,-position[1]/100],matrix=instance['matrix'],scale=instance['scale'],bounds_min_cm=instance['bounds_min_cm'],bounds_max_cm=instance['bounds_max_cm'],collision_binding=descriptor,render_model=visual['model'],render_vertex_count=visual['vertex_count']))
items.sort(key=lambda i:i['resource'])
counts={kind:sum(i['authored_name_category']==kind for i in items) for kind in ['trickboost','speedboost','collecta','collectabreak']}
assert LOCATION!='ARA1' or (counts['trickboost']==3 and counts['speedboost']==2)
pickups_dir(LOCATION).mkdir(parents=True,exist_ok=True)
pickup_file(LOCATION,'catalog').write_text(json.dumps(dict(location=LOCATION,source_sha256=world['source_sha256'],collision_manifest_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),counts=counts,items=items,scope='Authored names/transforms/bindings only; effects, collection thresholds, event eligibility and respawn rules unverified'),indent=2)+'\n')
print(counts)
