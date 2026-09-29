#!/usr/bin/env python3
"""Identify the live rider snow emitter class and texture phase fields."""
from pathlib import Path
import hashlib,json,struct,zipfile
root=Path(__file__).resolve().parents[1]
path=root/'local/reference/pcsx2/snow-jam-glide.p2s'
with zipfile.ZipFile(path) as archive:ram=archive.read('eeMemory.bin')
u=lambda a:struct.unpack_from('<I',ram,a)[0]
f=lambda a:struct.unpack_from('<f',ram,a)[0]
needle=struct.pack('<I',0x14701a0);offset=0;matches=[]
while (offset:=ram.find(needle,offset))>=0:
 at=offset;offset+=4
 if at%4 or at+0x30>=len(ram):continue
 emitter=u(at+0x28);profile=u(at+0x2c)
 if 0<emitter<len(ram)-10*0x210 and 0<profile<len(ram)-10*232 and u(profile)==2 and u(profile+0xc4)==5 and u(profile+0x10)==0x41200000:matches.append((at,emitter,profile))
assert len(matches)==1,matches
owner,base,profiles=matches[0];rows=[]
for index in range(10):
 emitter=base+index*0x210;vtable=u(emitter+0x1f8)
 assert vtable==0x4930d0
 rows.append(dict(index=index,address=hex(emitter),vtable=hex(vtable),resetConstructor=hex(u(vtable+0x1c)),
                  textureBase=u(emitter+4),flipCount=u(emitter+12),flipPhase=f(emitter+16),flipRate=f(emitter+20),allocated=u(emitter+0x174)))
report=dict(snapshot=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),owner=hex(owner),emitters=rows,
            birthFunction='3710D0, phase block371220..371248',drawFunction='371380, texture selected at3713A8..3713C4',
            scope='Live saved rider emitter identity and fields; does not prove scene submission order or full reset lifecycle.')
output=root/'local/browser-validation/original-snow-emitter-bindings.json';output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
