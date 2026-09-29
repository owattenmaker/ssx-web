"""Export original single-player gauge draw rectangles from the source draw probe."""
import json, hashlib, struct, zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
probe=root/'local/browser-ui/hud/boost-draws.json'
data=json.loads(probe.read_text())
with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z: memory=z.read('eeMemory.bin')
layers=[]
for case in data['cases']:
 if case['fraction']!=1: continue
 rects=[]
 for q in case['quads']:
  a,b,c,_=q['vertices']
  rects.append(dict(x=a['position'][0],y=a['position'][1],w=b['position'][0]-a['position'][0],h=c['position'][1]-a['position'][1],u=a['uv'][0],v=a['uv'][1],uw=b['uv'][0]-a['uv'][0],vh=c['uv'][1]-a['uv'][1]))
 layers.append(dict(widget=case['widget'],order=(case['quads'][0]['material_words'][2]>>5)&31,rects=rects))
result=dict(provenance=dict(function='0x21D1A0',settings='0x4768B0',probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest()),atlas='OV_1-4',width=640,height=480,bottom=392,fillHeight=242,layers=sorted(layers,key=lambda x:x['order']),colours=[struct.unpack_from('<4f',memory,a) for a in [0x4c84c8,0x4c84e8,0x4c8508]])
# Orb is owner+538, descriptor3. Preserve half-texel atlas coordinates.
orb_ptr=struct.unpack_from('<I',memory,data['owner']+0x538)[0]
texture,sx,sy,v,u,u1,v1,name_hash=struct.unpack_from('<I6fI',memory,orb_ptr)
x,y,w,h=struct.unpack_from('<4h',memory,0x4768b0+3*36)
assert texture==data['cases'][0]['quads'][0]['texture_handle']
result['orb']=dict(sprite_hash=name_hash,uv=[u,v,u1,v1],position=[x-w/2,y-h/2],size=[w,h],scale=[sx,sy])
lui=(root/'local/browser-ui/hud/OV.LUI').read_bytes()
object_table=struct.unpack_from('<6I',lui)[4]
object_count=struct.unpack_from('<I',lui,object_table)[0]
orb_records=[struct.unpack_from('<II4f',lui,object_table+4+24*i) for i in range(object_count)]
orb_records=[r for r in orb_records if r[0]==name_hash]
assert len(orb_records)==1 and list(orb_records[0][2:])==result['orb']['uv'], 'Orb runtime and LUI UVs disagree'
result['flashColours']=[struct.unpack_from('<4f',memory,a) for a in [0x4c8428,0x4c8448,0x4c8468,0x4c8488]]
result['orbGlow']=dict(padding=struct.unpack_from('<f',memory,0x4a30f0-0x55e0)[0],extent=struct.unpack_from('<f',memory,0x4a30f0-0x55dc)[0],alphaLow=struct.unpack_from('<f',memory,0x4a30f0-0x55d8)[0],alphaHigh=struct.unpack_from('<f',memory,0x4a30f0-0x55d4)[0],texture='part-glow')
coil_capture=json.loads((root/'local/browser-ui/hud/boost-coil-glow.json').read_text())['cases']
coil_active=next(c for c in coil_capture if c['phase']==0 and c['palette']==0)
result['coilGlows']=[{k:d[k] for k in ['position','size','uv']} for d in coil_active['glow'][1:]]
for case in coil_capture:
 assert len(case['coilQuads'])==20
 assert len(case['glow'])==(0 if case['phase']<0 else 5)
 if case['phase']>=0:
  assert [{k:d[k] for k in ['position','size','uv']} for d in case['glow'][1:]]==result['coilGlows']
result['flashBackgrounds']={}
for widget,quads in [(4,coil_active['coilQuads'][:10]),(5,coil_active['coilQuads'][10:])]:
 result['flashBackgrounds'][str(widget)]=[dict(x=q['position'][0],y=q['position'][1],w=q['right'][0]-q['position'][0],h=q['bottom'][1]-q['position'][1],u=q['uv'][0],v=q['uv'][1],uw=q['rightUV'][0]-q['uv'][0],vh=q['bottomUV'][1]-q['uv'][1]) for q in quads]
letters_path=root/'local/browser-ui/hud/boost-letters.json'
letters=json.loads(letters_path.read_text())['cases']
assert len(letters)==27
for case in letters:
 assert case['mode'] in (0,1,2) and 0<=case['letter']<9
 assert case['draws'][0]['text']=='UBERSUPER'[case['letter']]
 assert len(case['draws'])==(1 if case['mode']==0 else 2)
result['letterSubmissions']=letters
result['provenance']['letters_sha256']=hashlib.sha256(letters_path.read_bytes()).hexdigest()
(root/'web/public/assets/UI/boost-gauge.json').write_text(json.dumps(result,separators=(',',':')))
