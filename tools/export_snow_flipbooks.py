#!/usr/bin/env python3
"""Recover authored tumble texture sequence; does not infer playback phase."""
import hashlib,json,struct
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
from export_snow_assets import member,png
from rider_assets import decode_rider_texture
ROOT=Path(__file__).resolve().parents[1]
def export():
 elf=(ROOT/'local/disc/SLUS_207.72').read_bytes()
 if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Wrong texture-ID executable')
 path=ROOT/'local/gamecube/disc/files/data/textures/particle.gsh';container=path.read_bytes();frames=[];records=[]
 for i in range(8):
  id=14+i;name=f'tmb{i+1}'
  if elf[0x4891b0+12*id-0xff000:0x4891b0+12*id-0xff000+4]!=name.encode():raise ValueError('Original texture ID sequence differs')
  offset,data=member(container,name);w,h,rgba=decode_rider_texture(data);frames.append((name,w,h,rgba));records.append(dict(id=id,name=name,width=w,height=h,file=name+'.rgba',sha256=hashlib.sha256(rgba).hexdigest(),source=str(path.relative_to(ROOT)),source_sha256=hashlib.sha256(container).hexdigest(),source_offset=offset,encoding=data[0],alpha_conversion='Original GX RGB5A3 toRGBA'))
 packages=[]
 for directory in ['local/assets/native/SNOW_FX','web/public/assets/SNOW_FX']:
  folder=ROOT/directory;manifest=json.loads((folder/'snow-fx.json').read_text());existing=next(t for t in manifest['textures']if t['id']==14)
  if existing['sha256']!=records[0]['sha256'] or (folder/'tmb1.rgba').read_bytes()!=frames[0][3]:raise ValueError('Existing first-frame asset differs')
  textures={t['id']:t for t in manifest['textures']};textures.update({t['id']:t for t in records});bindings=[]
  for profile in manifest['profiles']:
   p=profile['parameters'];ids=list(range(p['TextureId'],p['TextureId']+p['NumFlipTextures']))
   if any(id not in textures for id in ids):raise ValueError('Missing authored flipbook texture')
   bindings.append(dict(emitter_index=profile['emitter_index'],texture_ids=ids,authored_rate=p['FlipTextureRate']))
  manifest['textures']=list(textures.values());manifest['flipbooks']=bindings;packages.append((folder,manifest))
 for folder,manifest in packages:
  for name,w,h,rgba in frames:(folder/(name+'.rgba')).write_bytes(rgba);png(folder/(name+'.png'),w,h,rgba)
  (folder/'snow-fx.json').write_text(json.dumps(manifest,indent=2)+'\n')
 report=dict(texture_ids=list(range(14,22)),distinct_frames=len({t['sha256']for t in records}),animated_emitters=[b for b in bindings if len(b['texture_ids'])>1],source_sha256=hashlib.sha256(container).hexdigest(),scope='Authored frames and rate parameters only; original phase/index calculation and renderer switching remain unimplemented')
 (ROOT/'local/browser-validation/snow-flipbook-assets.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':export()
