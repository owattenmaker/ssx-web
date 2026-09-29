#!/usr/bin/env python3
"""Record original snow texture-combine bindings from an owned race snapshot."""
from pathlib import Path
import hashlib,json,struct,zipfile
root=Path(__file__).resolve().parents[1]
path=root/'local/reference/pcsx2/snow-jam-glide.p2s'
with zipfile.ZipFile(path) as archive:ram=archive.read('eeMemory.bin')
u=lambda address:struct.unpack_from('<I',ram,address)[0]
renderer=u(0x4a30f0+0x2a90);manager=u(renderer+0x18f4)
asset=json.loads((root/'web/public/assets/SNOW_FX/snow-fx.json').read_text())
rows=[]
for texture in sorted({p['parameters']['TextureId'] for p in asset['profiles']}):
 handle=u(renderer+0xf50+texture*4);descriptor=u(manager+8+handle*4)
 tex0=struct.unpack_from('<Q',ram,descriptor+0x38)[0]
 row=dict(textureId=texture,handle=handle,descriptor=descriptor,tex0=hex(tex0),tfx=(tex0>>35)&3,tcc=(tex0>>34)&1)
 assert (row['tfx'],row['tcc'])==(0,1),row
 rows.append(row)
report=dict(snapshot=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),renderer=renderer,textures=rows,
 scope='Saved texture descriptor state: MODULATE/TCC1. Not proof of draw ordering, framebuffer blend or all-frame material state.')
out=root/'local/browser-validation/snow-material-bindings.json';out.write_text(json.dumps(report,indent=2)+'\n')
print(f'{len(rows)} snow base textures use MODULATE/TCC1 in the original saved bindings.')
