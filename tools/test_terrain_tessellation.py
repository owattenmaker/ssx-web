#!/usr/bin/env python3
import json,math
from pathlib import Path
from terrain_tessellation import error_constant,resolution
from audit_terrain_tessellation import evaluate
zero=lambda:[[0.,0.,0.]for _ in range(16)]
flat=zero();flat[1][0]=1;flat[4][2]=1
assert error_constant(flat)<1e-12 and resolution(flat)[0]==8
quadratic=zero();quadratic[2][1]=4
assert abs(error_constant(quadratic)-1)<1e-12
cross=zero();cross[5][1]=4
assert abs(error_constant(cross)-1)<1e-12
root=Path(__file__).resolve().parents[1];patches=json.loads((root/'web/public/assets/ARA1/terrain.json').read_text())['patches'];checked=0
for patch in patches:
 c=patch['coefficients'];bound=error_constant(c)/64
 for x,y in [(0,0),(3,5),(7,7)]:
  for tri,t in [([(x,y),(x+1,y),(x,y+1)],1/3),([(x+1,y),(x+1,y+1),(x,y+1)],2/3)]:
   points=[evaluate(c,u/8,v/8)for u,v in tri];linear=[sum(p[k]for p in points)/3 for k in range(3)];exact=evaluate(c,(x+t)/8,(y+t)/8)
   assert math.dist(linear,exact)<=bound+1e-8,(patch['resource_id'],bound);checked+=1
print('Tessellation bound checks:',checked,'authored samples plus affine/quadratic/mixed closed-form cases')
