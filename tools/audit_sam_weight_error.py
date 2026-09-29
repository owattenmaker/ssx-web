#!/usr/bin/env python3
"""Measure authored-weight quantization displacement on sampled replay poses."""
import json,math,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def audit():
 rig=json.loads((ROOT/'web/public/assets/RIDER_SAM/rider.json').read_text());report=json.loads((ROOT/'local/browser-validation/gameplay-parity.json').read_text());width=sum(n for _,n in report['fields']);offset=0
 for name,n in report['fields']:
  if name=='skinMatrices':break
  offset+=n
 if n!=len(rig['bones'])*16:raise ValueError('Trace does not contain Sam source matrices')
 vertices=list(struct.iter_unpack('<10f',(ROOT/'web/public/assets/RIDER_SAM/vertices.bin').read_bytes()));trace=(ROOT/'local/browser-validation/wasm-gameplay.bin').read_bytes();frames=len(trace)//(width*4)
 changed=[i for i,g in enumerate(rig['skin'])if any(abs(w*100-q[1])>1e-6 for (_,w),q in zip(g,rig['source_skin'][i]))]
 worst=dict(error_cm=0);sampled=list(range(0,frames,120))
 for frame in sampled:
  matrices=struct.unpack_from('<'+str(n)+'f',trace,(frame*width+offset)*4)
  for i in changed:
   v=vertices[i];point=[v[0]*100,-v[2]*100,v[1]*100,1];old=[0.]*4;new=[0.]*4
   for (bone,weight),(same,integer)in zip(rig['skin'][i],rig['source_skin'][i]):
    if bone!=same:raise ValueError('Quantization reordered bone influences')
    transformed=[sum(matrices[bone*16+c*4+r]*point[c]for c in range(4))for r in range(4)]
    for r in range(4):old[r]+=weight*transformed[r];new[r]+=integer/100*transformed[r]
   error=math.dist([v/old[3]for v in old[:3]],[v/new[3]for v in new[:3]])
   if error>worst['error_cm']:worst=dict(error_cm=error,frame=frame,vertex=i)
 result=dict(sampled_frames=len(sampled),changed_vertices=len(changed),maximum=worst,scope='Double-precision displacement check on sampled native/WASM replay matrices; isolates weight quantization, not all pose differences or full-course guarantee')
 (ROOT/'local/rider-lighting/sam-weight-displacement.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':audit()
