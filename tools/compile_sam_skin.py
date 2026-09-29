#!/usr/bin/env python3
"""Compile the authored Sam mesh into the source engine's skinning representation.

These bind matrices/weights are compiled custom content, not captured PS2 data.
"""
import hashlib,json,math,struct
from pathlib import Path
from export_rider_bind_matrices import multiply,local_matrix
ROOT=Path(__file__).resolve().parents[1]
def inverse(matrix):
 rows=[[matrix[c*4+r]for c in range(4)]+[float(r==c)for c in range(4)]for r in range(4)]
 for c in range(4):
  pivot=max(range(c,4),key=lambda r:abs(rows[r][c]));rows[c],rows[pivot]=rows[pivot],rows[c]
  if abs(rows[c][c])<1e-12:raise ValueError('Singular authored bind matrix')
  scale=rows[c][c];rows[c]=[x/scale for x in rows[c]]
  for r in range(4):
   if r!=c:
    factor=rows[r][c];rows[r]=[a-factor*b for a,b in zip(rows[r],rows[c])]
 return [rows[r][c+4]for c in range(4)for r in range(4)]
def compile_skin(rig):
 matrices=[];words=[]
 for i,bone in enumerate(rig['bones']):
  q=bone['rotation'];length=math.sqrt(sum(x*x for x in q))
  if not length:raise ValueError('Zero authored bind quaternion')
  q=[x/length for x in q];p=bone['translation'];local=local_matrix(dict(source_rotation=[q[0],-q[2],q[1],q[3]],source_translation_cm=[p[0]*100,-p[2]*100,p[1]*100,1]))
  parent=bone['parent']
  if parent>=i or parent < -1:raise ValueError('Invalid authored hierarchy')
  world=local if parent<0 else multiply(matrices[parent],local);matrices.append(world);words.append(list(struct.unpack('<16I',struct.pack('<16f',*inverse(world)))))
 weights=[];max_error=0;changed=0
 for group in rig['skin']:
  if not 1<=len(group)<=4 or any(not 0<=bone<len(matrices) or not math.isfinite(w) or w<0 for bone,w in group):raise ValueError('Invalid authored weights')
  total=sum(w for _,w in group)
  if total<=0:raise ValueError('Empty authored weight sum')
  exact=[w/total*100 for _,w in group];quantized=[math.floor(w)for w in exact]
  for i in sorted(range(len(group)),key=lambda i:(-(exact[i]-quantized[i]),i))[:100-sum(quantized)]:quantized[i]+=1
  if sum(quantized)!=100:raise ValueError('Percent conversion failed')
  error=max(abs(a-b)/100 for a,b in zip(exact,quantized));max_error=max(max_error,error);changed+=error>1e-8
  weights.append([[group[i][0],w]for i,w in enumerate(quantized)])
 if max_error>.01:raise ValueError('Weight quantization exceeds one percent')
 linear=translation=0
 for world,row in zip(matrices,words):
  product=multiply(world,struct.unpack('<16f',struct.pack('<16I',*row)))
  linear=max(linear,max(abs(product[c*4+r]-float(c==r))for c in range(3)for r in range(3)));translation=max(translation,max(abs(v)for v in product[12:15]))
 if linear>1e-5 or translation>.001:raise ValueError('Compiled bind does not invert authored rest pose')
 update=dict(source_normal_quantization=32768,source_skin=weights,source_skin_weight_units='integer-percent',source_skin_provenance='Authored Sam weights quantized by largest remainder, not original game weights',source_bone_slots=list(range(len(matrices))),source_bone_slot_count=len(matrices),source_bind_matrix_words=words,source_bind_matrix_space='source-centimeters-Z-up',source_bind_provenance='Compiled from authored Sam rest hierarchy, not captured PS2 inverse binds')
 report=dict(bones=len(matrices),vertices=len(weights),groups=len({tuple(map(tuple,g))for g in weights}),quantized_vertices=changed,max_weight_error=max_error,max_rest_linear_error=linear,max_rest_translation_cm=translation)
 return update,report
def compile_files(native,web,report_path):
 rig=json.loads(native.read_text());client=json.loads(web.read_text())
 if any(rig[k]!=client[k]for k in ['bones','skin']):raise ValueError('Authored/native browser rigs differ')
 update,report=compile_skin(rig);report['authored_rig_sha256']=hashlib.sha256(json.dumps({k:rig[k]for k in ['bones','skin']},sort_keys=True).encode()).hexdigest()
 rig.update(update);client.update(update);native.write_text(json.dumps(rig,indent=2)+'\n');web.write_text(json.dumps(client,indent=2)+'\n')
 report_path.parent.mkdir(parents=True,exist_ok=True);report_path.write_text(json.dumps(report,indent=2)+'\n');return report
def main():
 print(json.dumps(compile_files(ROOT/'local/assets/native/RIDER_SAM/rider.json',ROOT/'web/public/assets/RIDER_SAM/rider.json',ROOT/'local/rider-lighting/sam-skin-compilation.json')))
if __name__=='__main__':main()
