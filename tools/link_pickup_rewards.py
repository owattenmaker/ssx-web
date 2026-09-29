"""Link authored boost bindings to LUN programs containing reward dispatch."""
from pathlib import Path
import sys,json,struct
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from world_assets import world_chunks,records,locations
import argparse
from locations import location as location_info,pickup_file,pickups_dir
LOCATION=(lambda p:(p.add_argument('--location',default='ARA1'),p.parse_args().location)[1])(argparse.ArgumentParser(description=__doc__));location_info(LOCATION)
locs=locations(root/'local/assets/source/ps2/bam.sdb');TRACK=next(i for i,l in enumerate(locs) if l['name']==LOCATION)
folder=pickups_dir(LOCATION);catalog=json.loads(pickup_file(LOCATION,'catalog').read_text());scripts=json.loads(pickup_file(LOCATION,'disassembly').read_text())['programs']
for ci,c in enumerate(world_chunks(root/'local/assets/source/ps2/bam.ssb')):
 if ci==locs[TRACK]['chunk_end']:stage=next(d for k,t,r,d in records(c) if k==16 and t==TRACK);break
count,offset=struct.unpack_from('<2I',stage,0x18);assert (count,offset)==(231,0x70) or LOCATION!='ARA1'
assert offset+count*24==struct.unpack_from('<I',stage,0x34)[0]
links=[]
for item in catalog['items']:
 if item['authored_name_category'] not in ['trickboost','speedboost']:continue
 resource=item['collision_binding']['resource08'];assert resource&255==TRACK
 row=struct.unpack_from('<6I',stage,offset+(resource>>8)*24)
 script=row[2];assert script&255==TRACK
 program=scripts[script>>8];first=program['instructions'][:3]
 assert [i['opcode'] for i in first]==[0x28,0x28,0x21] and first[2]['builtin']==27
 assert (first[0]['word']>>8)&255==1 and (first[1]['word']>>8)&255==2
 effect=(first[0]['word']>>16)&255;amount=(first[1]['word']>>16)&255
 assert effect==({'speedboost':1,'trickboost':2}[item['authored_name_category']]) and amount==5
 links.append(dict(name=item['name'],instance_resource=item['resource'],binding_resource=resource,handler_slots=list(row),slot2_program=script>>8,effect_type=effect,amount_literal=amount,dispatch_builtin=27,dispatcher=0x10f1c0))
assert len(links)==5 or LOCATION!='ARA1'
(folder/'reward-links.json').write_text(json.dumps(dict(links=links,scope='Authored handler-slot/reward-bytecode linkage; collection lifecycle and event-slot timing still unverified'),indent=2)+'\n')
print(f'Linked {len(links)} boosts:',sorted({(l["slot2_program"],l["effect_type"],l["amount_literal"]) for l in links}))
