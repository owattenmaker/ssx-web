#!/usr/bin/env python3
"""Verify actual packaged event visibility boundaries against native triangle provenance."""
import array,collections,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read_indices(path):
 a=array.array('I');a.frombytes(path.read_bytes());return a
src=ROOT/'local/assets/native/ARA1';out=ROOT/'web/public/assets/ARA1'
source=json.loads((src/'world.json').read_text());packaged=json.loads((out/'world.json').read_text())
a=read_indices(src/'indices.bin');b=read_indices(out/'indices.bin')
def triangles(values,start=0,count=None):
 return collections.Counter(tuple(values[i:i+3]) for i in range(start,start+(len(values) if count is None else count),3))
assert triangles(a)==triangles(b),'Batching added, lost or changed triangles'
for file in ['vertices.bin','colors.bin']:assert (src/file).read_bytes()==(out/file).read_bytes(),file+' changed'
dead=set(json.loads((ROOT/'local/event-activation/countdown-instances.json').read_text())['dead_resources'])
assert set(packaged['event_dead_resources'])==dead
expected={r:collections.Counter() for r in dead};actual={r:collections.Counter() for r in dead}
for span in source['collision_sources']:
 if span['kind']!='instance':continue
 r=span['rid']<<8|span['track']
 if r in dead:expected[r].update(triangles(a,span['first_triangle']*3,span['triangle_count']*3))
for batch in packaged['batches']:
 r=batch.get('event_dead_resource')
 if r is None:continue
 assert r in dead and batch['instance'] and 'pickup_resource' not in batch
 actual[r].update(triangles(b,batch['first_index'],batch['index_count']))
assert all(expected.values()) and expected==actual,'Event batch includes another object or omits owned geometry'
print(json.dumps(dict(event_resources=len(dead),event_triangles=sum(sum(x.values()) for x in actual.values()),total_triangles=len(a)//3,all_geometry_preserved=True)))
