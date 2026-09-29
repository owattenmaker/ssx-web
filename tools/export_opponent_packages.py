#!/usr/bin/env python3
"""Package the five Snow Jam computer opponents for the browser, like RIDER_ZOE.

For each rider_assets.NPC_PRESETS opponent (native package local/assets/native/RIDER_*,
built by tools/rider_assets.py --rider NAME):
  1. raw source skin weights      (export_rider_skin_weights.export(name), GC MNF, checked
                                   against the installed normalized skin)
  2. static bind matrices / slots (export_rider_bind_matrices.export(name), live PS2 actor
                                   in snow-jam-countdown-anchor + snow-jam-glide)
  3. web/public/assets/RIDER_*    textures as PNG, vertices/indices/colors.bin, rider.json,
                                   animation-samples.json, animation-start.json, world.json
                                   (the same file set web/prepare.py writes for RIDER_ZOE,
                                   plus the opponent's original animation seed)
and web/public/assets/opponents.json: the countdown-anchor grid roster (ARA1 event-start.json
participants 1..5 -> render_package).

python3 tools/export_opponent_packages.py [--rider psymon ...] [--skip-derive]
"""
import argparse,json,shutil,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from rider_assets import NPC_PRESETS
import export_rider_skin_weights,export_rider_bind_matrices
SRC=ROOT/'local/assets/native';OUT=ROOT/'web/public/assets'
def png(w,h,rgba):  # identical to web/prepare.py
 def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
 raw=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,3))+chunk(b'IEND',b'')
def package(name):
 a=SRC/name;dest=OUT/name;dest.mkdir(parents=True,exist_ok=True)
 d=json.loads((a/'world.json').read_text())
 for key,t in d['textures'].items():
  rgba=(a/t['path']).read_bytes()
  if len(rgba)!=t['width']*t['height']*4:raise ValueError(f'{name} {key}: texture size mismatch')
  target=dest/(key+'.png');target.write_bytes(png(t['width'],t['height'],rgba));t['path']=target.name
 for f in ['vertices.bin','indices.bin','colors.bin','rider.json','animation-samples.json','animation-start.json']:shutil.copy2(a/f,dest/f)
 (dest/'world.json').write_text(json.dumps(d,separators=(',',':')))
 from export_characters import ps2_texel_pngs;ps2_texel_pngs(dest)  # PS2 texel domain (web/rider-material.js HIGHLIGHT2)
 rig=json.loads((dest/'rider.json').read_text());vertices=(dest/'vertices.bin').stat().st_size//40
 missing=[k for k in ('source_skin','source_bind_matrix_words','source_bone_slots','source_bone_slot_count') if k not in rig]
 if missing:raise ValueError(f'{name}: rider.json lacks {missing}')
 if len(rig['source_skin'])!=vertices or len(rig['skin'])!=vertices:raise ValueError(f'{name}: skin/vertex count mismatch')
 return dict(package=name,bones=len(rig['bones']),source_slots=rig['source_bone_slot_count'],vertices=vertices,
             triangles=(dest/'indices.bin').stat().st_size//12,textures=len(d['textures']),
             hair=[b['name'] for b in rig['bones'] if b['name'].startswith('sec_')],
             bytes=sum(p.stat().st_size for p in dest.iterdir()))
def main():
 parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
 parser.add_argument('--rider',action='append',choices=list(NPC_PRESETS),help='limit to these opponents (default all five)')
 parser.add_argument('--skip-derive',action='store_true',help='only copy/convert; native rider.json already carries source skin + bind rows')
 args=parser.parse_args();riders=args.rider or list(NPC_PRESETS);rows=[]
 for rider in riders:
  if not args.skip_derive:export_rider_skin_weights.export(rider);export_rider_bind_matrices.export(rider)
  rows.append(package(f'RIDER_{rider.upper()}'));print(json.dumps(rows[-1]))
 event=json.loads((SRC/'ARA1/event-start.json').read_text())
 roster=[dict(slot=p['slot'],character=p['character'],package=p['render_package'],gameplay_character_id=p['gameplay_character_id'],
              resource_prefix=NPC_PRESETS[p['character']]['prefix'],actor=hex(NPC_PRESETS[p['character']]['actor']))
         for p in event['participants'] if not p['race']['human']]
 if [r['slot'] for r in roster]!=[1,2,3,4,5]:raise ValueError('Unexpected Snow Jam opponent grid')
 (OUT/'opponents.json').write_text(json.dumps(dict(location='ARA1',source='local/assets/native/ARA1/event-start.json participants (countdown anchor)',opponents=roster),indent=2)+'\n')
 from export_rider_textures import pack_all;pack_all()  # the packages' PNGs -> the riders' texture archives (WARDROBE/<ID>/textures.tex)
if __name__=='__main__':main()
