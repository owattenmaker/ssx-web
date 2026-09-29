"""Read-only export of the original renderer-owned HUD glow texture."""
from pathlib import Path
import sys,struct,zipfile,zlib,json,hashlib
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from inspect_disc import Disc
with zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s') as z: m=z.read('eeMemory.bin')
u=lambda p:struct.unpack_from('<I',m,p)[0]
renderer=u(0x4a30f0-0x854);handle=u(renderer+0xf50);descriptor=u(u(renderer+0x18f4)+handle*4+8)
assert m[descriptor+4:descriptor+8]==b'part'
tex0=struct.unpack_from('<Q',m,descriptor+0x38)[0];width=1<<((tex0>>26)&15);height=1<<((tex0>>30)&15)
assert (width,height,(tex0>>20)&63)==(64,64,27)
pixels=m[u(descriptor+0x1c):u(descriptor+0x1c)+width*height];palette=m[u(descriptor+0x20):u(descriptor+0x20)+1024]
from disc_paths import ps2_iso;disc=Disc(ps2_iso())
try: archive=disc.file('DATA/TEXTURES/PARTICLE.SSH')
finally: disc.close()
entries=[struct.unpack_from('<4sI',archive,16+8*i) for i in range(struct.unpack_from('<I',archive,8)[0])]
offset=next(o for name,o in entries if name==b'part');record=archive[offset:];size=int.from_bytes(record[1:4],'little')
assert record[0]==2 and struct.unpack_from('<HH',record,4)==(width,height)
assert record[16:size]==pixels,'Disc and runtime glow texels differ'
assert record[size+16:size+16+1024]==palette,'Disc and runtime glow palette differ'
rgba=bytearray()
for index in pixels:
 source=(index&0xe7)|((index&8)<<1)|((index&16)>>1)
 r,g,b,a=palette[source*4:source*4+4];rgba+=bytes((r,g,b,min(255,a*2)))
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
scan=b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')
out=root/'web/public/assets/UI';(out/'part-glow.png').write_bytes(png)
(out/'part-glow.json').write_text(json.dumps(dict(source='DATA/TEXTURES/PARTICLE.SSH',entry='part',archive_sha256=hashlib.sha256(archive).hexdigest(),runtime_handle=handle,runtime_descriptor=descriptor,texels_sha256=hashlib.sha256(pixels).hexdigest(),palette_sha256=hashlib.sha256(palette).hexdigest(),width=width,height=height,layout='linear 8-bit texels; CSM1 palette index bit3/4 swap; alpha128 to255'),indent=2)+'\n')
print('Exported original64x64 part glow; disc texels and palette exactly match captured renderer resource')
