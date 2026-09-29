#!/usr/bin/env python3
"""Verify batching preserves every source triangle and pickup ownership."""
import array,collections,importlib.util,json,argparse
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('world_batches',root/'web/world-batches.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
source=root/'local/assets/native/ARA1';world=json.loads((source/'world.json').read_text())
vertices=array.array('f');vertices.frombytes((source/'vertices.bin').read_bytes());indices=array.array('I');indices.frombytes((source/'indices.bin').read_bytes())
ids=[p['resource'] for p in json.loads((root/'local/browser-pickups/runtime-bindings.json').read_text())['instances']]
batches,packed=module.spatial_batches(world,vertices,indices,ids)
triangles=lambda a:collections.Counter(tuple(a[i:i+3]) for i in range(0,len(a),3))
assert triangles(indices)==triangles(packed)
for resource in ids:
 expected=array.array('I');actual=array.array('I')
 for s in world['collision_sources']:
  if s['kind']=='instance' and ((s['rid']<<8)|s['track'])==resource:expected.extend(indices[s['first_triangle']*3:(s['first_triangle']+s['triangle_count'])*3])
 for b in batches:
  if b.get('pickup_resource')==resource:actual.extend(packed[b['first_index']:b['first_index']+b['index_count']])
 assert actual and triangles(expected)==triangles(actual),resource
 print(resource,len(actual)//3,'owned draw triangles')
#Update only the current browser draw batches/index buffer. Other metadata and textures remain packaged as before.
target=root/'web/public/assets/ARA1';current=json.loads((target/'world.json').read_text());assert current['source_sha256']==world['source_sha256']
assert (target/'vertices.bin').read_bytes()==(source/'vertices.bin').read_bytes()
if args.apply:
 current['batches']=batches;(target/'indices.bin').write_bytes(packed.tobytes());(target/'world.json').write_text(json.dumps(current,separators=(',',':')))
print('All course triangles preserved; dynamic pickup ownership retained')
