#!/usr/bin/env python3
"""Verify snow's blend enum and quantify encoded-GS versus current linear blending."""
from pathlib import Path
import hashlib,json,math,struct
root=Path(__file__).resolve().parents[1]
elf=(root/'local/disc/SLUS_207.72').read_bytes()
from inspect_disc import EXPECTED_SHA1
assert hashlib.sha1(elf).hexdigest()==EXPECTED_SHA1
u=lambda address:struct.unpack_from('<I',elf,address-0xff000)[0]
asset=json.loads((root/'web/public/assets/SNOW_FX/snow-fx.json').read_text())
profiles=[]
for entry in asset['profiles']:
 mode=entry['parameters']['BlendMode']
 assert mode==1
 enum=u(0x44b420+mode*4)
 setter=u(0x491fb0+enum*4)
 assert enum==5 and setter==0x3624dc
 #3624E4: ADDIU v0,zero,0x44. GS ALPHA=(Cs-Cd)*As/128+Cd.
 assert u(setter+8)==0x24020044
 profiles.append(dict(emitter=entry['emitter_index'],profileBlendMode=mode,rendererBlendEnum=enum,setter=hex(setter),gsAlpha='0x44'))
def decode(x):
 x/=255
 return x/12.92 if x<=.04045 else ((x+.055)/1.055)**2.4
def encode(x):
 return x*12.92 if x<=.0031308 else 1.055*x**(1/2.4)-.055
linear=[decode(i) for i in range(256)]
maximum={'errorBytes':-1};total=0;cases=0
for destination in (0,64,128,192,255):
 for source in range(256):
  for alpha in range(129):
   original=max(0,min(255,(((source-destination)*alpha)>>7)+destination))
   browser=math.floor(encode(linear[source]*(alpha/128)+linear[destination]*(1-alpha/128))*255+.5)
   error=abs(original-browser);total+=error;cases+=1
   if error>maximum['errorBytes']:maximum=dict(source=source,destination=destination,alphaGs=alpha,originalByte=original,currentLinearBlendByte=browser,errorBytes=error)
report=dict(scope='Verified original enum/setter chain and ideal one-channel blend arithmetic; excludes texture modulation, depth, particle order, framebuffer format and actual rendered images.',
 elfSha1=hashlib.sha1(elf).hexdigest(),profiles=profiles,cases=cases,maxDifference=maximum,meanDifferenceBytes=total/cases,
 whiteHalfAlphaOverBlack=dict(originalByte=127,currentLinearBlendByte=188),
 runtimeStatus='Snow still blends in linear space. This audit does not change the renderer.')
output=root/'local/browser-validation/snow-blend-audit.json';output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='profiles'},indent=2))
