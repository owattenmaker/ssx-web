#!/usr/bin/env python3
"""Build native bridge references for actual authored assets and retained draws."""
import hashlib,json,struct,subprocess
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
root=Path(__file__).resolve().parents[1];out=root/'local/rider-lighting';work=root/'local/browser-validation'
elf=(root/'local/disc/SLUS_207.72').read_bytes()
if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Wrong executable')
constants=[struct.unpack_from('<f',elf,a-0xff000)[0]for a in [0x4a09f0,0x4a43c0,0x4a09f4,0x4a09f8,0x4a0a08]]
scopes=json.loads((out/'light-selection-checkpoints.json').read_text());bounds=json.loads((out/'light-tree-fixtures.json').read_text())
views=(out/'camera-view-snapshot-fixtures.bin').read_bytes();environment=json.loads((root/'web/public/assets/ARA1/environment.json').read_text())['irradiance']
frames=[]
for i in range(192):
 scope=scopes[(i//32)%3];b=next(b for b in bounds if b['snapshot']==scope['snapshot']);point=[p+((i%16)-8)*21*(k-1)for k,p in enumerate(scope['head_cm'])]+[1]
 bank=[v for row in environment[['bright','dark','alternate'][i%3]]for v in row]
 extra=[] if i%7==0 else [.01*i,.02,.03]+[v for j in range(i%6)for v in [0,0,1,.03*(j+1),.01,.02]]
 frames.append(dict(reset=i%64==0,refresh=i%8==0,bounds=b['minimum']+[1]+b['maximum']+[1],rankPoint=scope['head_cm'],point=point,environment=bank,view=list(struct.unpack_from('<16f',views,(i%4)*96+32)),constants=constants,rimScale=.75,extra=extra))
inputs=out/'browser-lighting-inputs.json';inputs.write_text(json.dumps(frames))
shim=work/'include/emscripten';shim.mkdir(parents=True,exist_ok=True);(shim/'emscripten.h').write_text('#pragma once\n#define EMSCRIPTEN_KEEPALIVE\n')
binary=out/'rider-lighting-bridge-reference';command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-I'+str(work/'include'),'-I'+str(root/'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf'),str(root/'web/rider_lighting_bridge.cpp'),str(root/'tests/rider_lighting_bridge_reference.cpp'),'-o',str(binary)]
subprocess.run(command,check=True)
output=root/'web/public/test-data/rider-lighting-bridge-reference.json'
with output.open('w')as target:subprocess.run([str(binary),str(root/'web/public/assets/ARA1/local-lights.json'),str(root/'web/public/assets/ARA1/light-tree.json'),str(inputs)],stdout=target,check=True)
print('Wrote192 native lighting bridge reference frames, with explicit query/draw/reset phases')
