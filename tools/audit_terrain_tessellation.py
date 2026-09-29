#!/usr/bin/env python3
"""Measure installed render-triangle error against the authored bicubic surface."""
import array,hashlib,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def evaluate(c,u,v):
 return [sum(((c[j*4+3][k]*u+c[j*4+2][k])*u+c[j*4+1][k])*u*v**j+c[j*4][k]*v**j for j in range(4))for k in range(3)]
def audit():
 folder=ROOT/'local/assets/native/ARA1'
 if (ROOT/'web/public/assets/ARA1/vertices.bin').read_bytes()!=(folder/'vertices.bin').read_bytes():raise ValueError('Browser render vertices differ from audited native vertices')
 meta=json.loads((folder/'world.json').read_text());terrain=json.loads((folder/'terrain.json').read_text());patches={p['resource_id']:p for p in terrain['patches']};vertices=array.array('f');vertices.frombytes((folder/'vertices.bin').read_bytes());indices=array.array('I');indices.frombytes((folder/'indices.bin').read_bytes());rows=[];samples=0
 for entry in meta['collision_sources']:
  if entry['kind']!='terrain':continue
  patch=patches[(entry['rid']<<8)|entry['track']];c=patch['coefficients'];n=math.isqrt(entry['triangle_count']//2)
  if 2*n*n!=entry['triangle_count']:raise ValueError('Unexpected render grid')
  first=entry['first_triangle']*3;ids=indices[first:first+entry['triangle_count']*3];base=min(ids);maximum=above=below=0;worst=None
  for y in range(n):
   for x in range(n):
    a=y*(n+1)+x
    for local,fraction in [((a,a+1,a+n+1),1/3),((a+1,a+n+2,a+n+1),2/3)]:
     expected=[base+i for i in local];at=((y*n+x)*2+(fraction>.5))*3
     if list(ids[at:at+3])!=expected:raise ValueError('Installed mesh does not match documented patch layout')
     p=[vertices[i*10:i*10+3]for i in expected];u=(x+fraction)/n;v=(y+fraction)/n;exact=evaluate(c,u,v);linear=[sum(t[k]for t in p)/3 for k in range(3)];error=math.dist(exact,linear);samples+=1
     if error>maximum:maximum=error;worst=[u,v]
     # Only compare height when the analytic X/Z is actually inside this triangle.
     ax,az=p[1][0]-p[0][0],p[1][2]-p[0][2];bx,bz=p[2][0]-p[0][0],p[2][2]-p[0][2];dx,dz=exact[0]-p[0][0],exact[2]-p[0][2];det=ax*bz-az*bx
     if abs(det)<1e-8:continue
     s=(dx*bz-dz*bx)/det;t=(ax*dz-az*dx)/det
     if min(s,t,1-s-t)<-1e-6:continue
     height=p[0][1]+s*(p[1][1]-p[0][1])+t*(p[2][1]-p[0][1]);gap=height-exact[1];above=max(above,gap);below=max(below,-gap)
  rows.append(dict(resource=patch['resource_id'],subdivisions=n,max_centroid_error_m=maximum,max_render_above_curve_m=above,max_render_below_curve_m=below,worst_uv=worst))
 report=dict(patches=len(rows),samples=samples,vertex_sha256=hashlib.sha256((folder/'vertices.bin').read_bytes()).hexdigest(),scope='Triangle centroid and contained-XZ height samples against native authored bicubic coefficients; not a bound or a rider contact replay',worst=sorted(rows,key=lambda x:x['max_render_above_curve_m'],reverse=True)[:30],above_10cm=sum(r['max_render_above_curve_m']>.1 for r in rows),above_50cm=sum(r['max_render_above_curve_m']>.5 for r in rows))
 target=ROOT/'local/browser-validation/terrain-tessellation-audit.json';target.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k!='worst'}));print(json.dumps(report['worst'][:3],indent=2))
if __name__=='__main__':audit()
