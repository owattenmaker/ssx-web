#!/usr/bin/env python3
"""Prepare authored patch descriptors and conservative full-edge adjacency.

Usage: python3 tools/prepare_terrain_render.py [--location X] [--output DIR] [--audit FILE]
ARA1 (default): web/public/assets/ARA1/terrain-render.json and
local/browser-validation/terrain-adjacency-audit.json. Other locations:
web/public/assets/X/terrain-render.json and local/browser-validation/X/terrain-adjacency-audit.json.
The browser copy of vertices.bin (web/prepare.py) must equal the native one when present.
"""
import argparse,array,collections,hashlib,itertools,json,math,sys
from pathlib import Path
from terrain_tessellation import control_net,error_constant
ROOT=Path(__file__).resolve().parents[1]
def prepare(location='ARA1',output=None,audit=None):
 source=ROOT/'local/assets/native'/location;target=ROOT/'web/public/assets'/location
 audit=Path(audit) if audit else ROOT/('local/browser-validation/terrain-adjacency-audit.json' if location=='ARA1' else f'local/browser-validation/{location}/terrain-adjacency-audit.json')
 output=Path(output) if output else target;output.mkdir(parents=True,exist_ok=True);audit.parent.mkdir(parents=True,exist_ok=True);meta=json.loads((source/'world.json').read_text());terrain=json.loads((source/'terrain.json').read_text());lookup={p['resource_id']:p for p in terrain['patches']}
 vb=(source/'vertices.bin').read_bytes()
 if location!='ARA1' and not (target/'vertices.bin').exists():print(f'WARNING: {target}/vertices.bin missing (web/prepare.py not run yet); skipping the browser/native vertex check',file=sys.stderr)
 elif vb!=(target/'vertices.bin').read_bytes():raise ValueError('Browser/native render vertices disagree')
 vertices=array.array('f');vertices.frombytes(vb);indices=array.array('I');indices.frombytes((source/'indices.bin').read_bytes());colors=array.array('f');colors.frombytes((source/'colors.bin').read_bytes());patches=[];edge_records=[]
 for entry in meta['collision_sources']:
  if entry['kind']!='terrain':continue
  resource=entry['rid']*256+entry['track'];p=lookup[resource];first=entry['first_triangle']*3;n=math.isqrt(entry['triangle_count']//2)
  if n!=8 or entry['triangle_count']!=128:raise ValueError('Unexpected baseline patch grid')
  ids=indices[first:first+384];base=min(ids)
  for y in range(8):
   for x in range(8):
    a=base+y*9+x;at=(y*8+x)*6
    if list(ids[at:at+6])!=[a,a+1,a+9,a+1,a+10,a+9]:raise ValueError('Baseline triangle provenance differs')
  corners=[base,base+72,base+8,base+80];uv=[list(vertices[i*10+6:i*10+8])for i in corners];light=[vertices[base*10+8],vertices[base*10+9],vertices[(base+8)*10+8]-vertices[base*10+8],vertices[(base+72)*10+9]-vertices[base*10+9]]
  color=list(colors[base*4:base*4+4])
  if any(list(colors[i*4:i*4+4])!=color for i in set(ids)):raise ValueError('Patch vertex colors need interpolation support')
  max_uv=0
  for j in range(81):
   u=(j%9)/8;v=(j//9)/8;expected=[sum(uv[i][k]*w for i,w in enumerate([(1-u)*(1-v),(1-u)*v,u*(1-v),u*v]))for k in range(2)]+[light[0]+u*light[2],light[1]+v*light[3]]
   actual=vertices[(base+j)*10+6:(base+j)*10+10];max_uv=max(max_uv,max(abs(a-b)for a,b in zip(actual,expected)))
  if max_uv>2e-5:raise ValueError('Installed UVs are not represented by authored interpolation')
  batch=next(b for b in meta['batches'] if b['first_index']<=first<b['first_index']+b['index_count'])
  if batch['instance']:raise ValueError('Terrain belongs to instance batch')
  net=control_net(p['coefficients']);curves=[net[0],[row[3]for row in net],net[3],[row[0]for row in net]];index=len(patches)
  patches.append(dict(resource=resource,coefficients=p['coefficients'],textureUv=uv,lightUv=light,color=color,texture=batch['texture'],lightmap=batch['lightmap'],bounds=[[min(q[k]for row in net for q in row)for k in range(3)],[max(q[k]for row in net for q in row)for k in range(3)]],errorConstant=error_constant(p['coefficients']),neighbors=[None]*4,baseline_vertex_start=base,baseline_first_triangle=entry['first_triangle'],baseline_triangle_count=128))
  for edge,curve in enumerate(curves):edge_records.append(dict(patch=index,edge=edge,curve=curve))
 # Both endpoints enter a spatial hash so matching is independent of direction.
 tolerance=.002;cell=.01;grid=collections.defaultdict(set)
 def key(p):return tuple(math.floor(v/cell)for v in p)
 for i,e in enumerate(edge_records):
  for p in [e['curve'][0],e['curve'][3]]:grid[key(p)].add(i)
 matches=[[]for _ in edge_records]
 for i,e in enumerate(edge_records):
  origin=key(e['curve'][0]);candidates=set()
  for delta in itertools.product([-1,0,1],repeat=3):candidates.update(grid.get(tuple(a+b for a,b in zip(origin,delta)),()))
  for j in candidates:
   if j<=i or edge_records[j]['patch']==e['patch']:continue
   other=edge_records[j]['curve']
   for reverse in [False,True]:
    gap=max(math.dist(a,b)for a,b in zip(e['curve'],other[::-1]if reverse else other))
    if gap<=tolerance:matches[i].append((j,reverse,gap));matches[j].append((i,reverse,gap));break
 linked=0;max_gap=0
 for i,m in enumerate(matches):
  if len(m)!=1 or len(matches[m[0][0]])!=1:continue
  j,reverse,gap=m[0];a,b=edge_records[i],edge_records[j];patches[a['patch']]['neighbors'][a['edge']]=dict(patch=b['patch'],edge=b['edge'],reverse=reverse,maxGapM=gap);linked+=1;max_gap=max(max_gap,gap)
 report=dict(patches=len(patches),matched_edges=linked//2,unmatched_edges=sum(not m for m in matches),ambiguous_edges=sum(len(m)>1 for m in matches),max_control_gap_m=max_gap,tolerance_m=tolerance,scope='Whole cubic edges only; partial-edge joins and overlapping variants unresolved. No live refinement enabled.')
 package=dict(version=1,source_sha256=meta['source_sha256'],baseline_vertex_sha256=hashlib.sha256(vb).hexdigest(),edge_parameter_order=['(t,0)','(1,t)','(t,1)','(0,t)'],patches=patches)
 (output/'terrain-render.json').write_text(json.dumps(package,separators=(',',':'))+'\n');audit.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter);p.add_argument('--location',default='ARA1');p.add_argument('--output',type=Path,help='terrain-render.json folder override');p.add_argument('--audit',type=Path,help='audit file override')
 a=p.parse_args();prepare(a.location,a.output,a.audit)
