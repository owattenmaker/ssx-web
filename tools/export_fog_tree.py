#!/usr/bin/env python3
"""Export original fog painter bytes and quadtree records for source comparison.

Default (no arguments, as web/prepare.py calls export()): ARA1 -> local/event-activation/.
`--location X` uses the course painter record of X (tools/course_painters.py) and writes
locations.activation_dir(X)/fog-tree.json + fog-section.bin; for X != ARA1 the CLI also
publishes web/public/assets/X/fog-tree.json (ARA1's copy stays in web/prepare.py).
`track` is the course's SDB location index (ARA1 8, BRA2 16, BHP1 15).
"""
import argparse,hashlib,json,shutil,struct
from pathlib import Path
from world_assets import world_chunks,records,locations
from import_sky import chunk_range,painter_sections,fog_entries
ROOT=Path(__file__).resolve().parents[1]
def export(location='ARA1',output=None):
 folder=ROOT/'local/assets/source/ps2';_,begin,end=chunk_range(locations(folder/'bam.sdb'),location);found=[]
 for index,chunk in enumerate(world_chunks(folder/'bam.ssb')):
  if index>end:break
  if index<begin:continue
  for kind,track,rid,data in records(chunk):
   if kind!=15 or len(data)<64:continue
   section=painter_sections(data).get(5)
   if section is not None:found.append((index,track,rid,data,section))
 if len(found)!=1:raise ValueError(f'Expected one {location} fog section')
 chunk,track,rid,data,section=found[0];header=struct.unpack_from('<I',section)[0]
 scale,x,y=struct.unpack_from('<3f',section,header);count=struct.unpack_from('<I',section,header+12)[0];root=struct.unpack_from('<H',section,header+20)[0]
 if header+40+count*8>len(section) or root>=count:raise ValueError('Invalid tree extent')
 nodes=[list(struct.unpack_from('<4H',section,header+40+i*8)) for i in range(count)]
 for node in nodes:
  if node[0]&1 and any(value>>1>=count for value in node):raise ValueError('Child outside tree')
 payloads=fog_entries(section)
 for node in nodes:
  if not node[0]&1:
   index=node[2]|node[3]<<16
   if index!=0xffffffff and index>=len(payloads):raise ValueError('Leaf payload outside section table')
 representatives={}
 def walk(index,low_x,low_y,size,depth=0):
  if depth>16:raise ValueError('Painter depth exceeds coordinate precision')
  node=nodes[index]
  if node[0]&1:
   half=size/2
   for q,child in enumerate(node):walk(child>>1,low_x+(q>>1)*half,low_y+(q&1)*half,half,depth+1)
  else:
   payload=node[2]|node[3]<<16
   if payload!=0xffffffff and (payload not in representatives or representatives[payload]['cell_size']<size):representatives[payload]=dict(payload=payload,cell_size=size,position=[x+(low_x+size/2)/scale,y+(low_y+size/2)/scale])
 walk(root,0,0,32768)
 from locations import activation_dir
 out=Path(output) if output else activation_dir(location);out.mkdir(parents=True,exist_ok=True);(out/'fog-section.bin').write_bytes(section)
 result=dict(version=1,location=location,chunk=chunk,track=track,rid=rid,record_sha256=hashlib.sha256(data).hexdigest(),section_sha256=hashlib.sha256(section).hexdigest(),scale=scale,origin=[x,y],root=root,nodes=nodes,outside_words=list(struct.unpack_from('<2I',section,header+24)),payloads=payloads,representative_samples=list(representatives.values()),leaf_payload_indices=[None if n[0]&1 else n[2]|n[3]<<16 for n in nodes],scope='Original point-tree records; leaf payload indices verified by2BAF90/2C0A10; transition-driver integration remains open.')
 (out/'fog-tree.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 from locations import web_dir,activation_dir
 parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter);parser.add_argument('--location',default='ARA1');parser.add_argument('--output',type=Path,help='Override the evidence folder (e.g. a scratch dir); skips the browser copy')
 args=parser.parse_args();result=export(args.location,args.output)
 if args.location!='ARA1' and not args.output:
  web=web_dir(args.location);web.mkdir(parents=True,exist_ok=True);shutil.copy2(activation_dir(args.location)/'fog-tree.json',web/'fog-tree.json')
 print(json.dumps(dict(nodes=len(result['nodes']),payloads=len(result['payloads']),origin=result['origin'],scale=result['scale'])))
