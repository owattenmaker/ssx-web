"""Pack the original CPU lighting lattice without expanding it into JSON texels.

Usage: python3 prepare-environment.py [--location ARA1|BRA2|BHP1] [--output DIR]
Inputs: local/assets/native/<X>/environment-lighting.json (tools/export_environment_lighting.py),
local/assets/native/IRRADIANCE/irradiance.json (tools/export_irradiance.py), and the
captured original_environment (ARA1: web/public/assets/ANIMATIONS/initial.json, other
locations: web/public/assets/<X>/initial.json when it exists; otherwise initial_ambient,
initial_ratio and force_next are left out and a warning is printed).
Outputs (web/public/assets/<X>/): environment.json/.bin, terrain-lighting.json,
terrain-light-atlas.png, screen-tint.json.
"""
import argparse,json,struct,hashlib,sys,zlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))

def initial_environment(location):
 path=root/('web/public/assets/ANIMATIONS/initial.json' if location=='ARA1' else f'web/public/assets/{location}/initial.json')
 if location!='ARA1' and not path.exists():
  print(f'WARNING: {path} missing; environment.json has no initial_ambient/initial_ratio/force_next (re-run after the savestate export)',file=sys.stderr);return None
 return json.loads(path.read_text())['original_environment']

def default_lighting_reference(location,irr):
 """2EE010 resolves an empty (reset, 2BE1F8) Lighting reference to the IRR index at gp+0x12D4, a runtime
 value set per course. Read it from the course glide savestate; None when the savestate is absent."""
 import zipfile
 from locations import state
 path=state(location,'glide')
 if not path.exists():print(f'WARNING: {path} missing; no default Lighting bank (gp+0x12D4)',file=sys.stderr);return None
 with zipfile.ZipFile(path) as z:memory=z.read('eeMemory.bin')
 index=struct.unpack_from('<i',memory,0x4a30f0+0x12d4)[0]
 names=[name for name,record in irr['records'].items() if record['index']==index]
 if len(names)!=1:raise ValueError(f'{location} default Lighting index {index} is not one IRR record')
 return names[0]

def irradiance_package(location,irr,elf):
 # 2EDA4C uses terrain ratio; 2EEFF0 reads Lighting scalar+48. ARA1's payloads agree, so
 # their bank/gain selection does not vary spatially. Other locations: the Lighting
 # payload that the course painter tree selects at the AIP start grid.
 if not irr['all_coefficient_bits_match']:raise ValueError('Cross-console light banks differ')
 if location=='ARA1':
  entries=irr['ara1_lighting'][0]['entries']
  if not entries or any((e['references'],e['scalars'])!=(entries[0]['references'],entries[0]['scalars'])for e in entries):raise ValueError('ARA1 lighting needs spatially varying bank selection')
  entry=entries[0];painter=None
 else:
  rows=irr.get('lighting_by_location',{}).get(location)
  if not rows:raise ValueError(f'irradiance.json has no Lighting references for {location}; re-run tools/export_irradiance.py')
  if len(rows)!=1:raise ValueError(f'Expected one {location} Lighting section')
  row=rows[0];entries=row['entries']
  if row['uniform']:entry=entries[0]
  else:
   if not row['start'] or row['start']['entry'] is None:raise ValueError(f'{location} Lighting varies spatially and no start payload was resolved')
   entry=entries[row['start']['entry']]
  names={n for e in entries for n in e['references']}
  default=default_lighting_reference(location,irr)
  if default:names.add(default)
  names=sorted(names)
  painter=dict(spatially_varying=not row['uniform'],start_entry=entry['index'],start=row['start'],chunk=row['chunk'],track=row['track'],rid=row['rid'],
               entries=[dict(index=e['index'],blend_rate=e['blend_rate'],references=e['references'],scalars=e['scalars'])for e in entries],
               tree=row['tree'],banks={n:irr['records'][n]['rows'] for n in names},
               note='irradiance.bright/dark/alternate/gain/rim_scale are the start payload; web/environment_bridge.cpp runs the spatial Lighting driver (2C0778) over entries/tree when spatially_varying')
  if default:painter['default_reference']=default
 refs=entry['references']
 incoming=struct.unpack_from('<f',elf,0x4a30f0+0xa80-0xff000)[0]
 result=dict(references=refs,bright=irr['records'][refs[0]]['rows'],dark=irr['records'][refs[1]]['rows'],alternate=irr['records'][refs[2]]['rows'],gain=entry['scalars'][0],incoming=incoming,source_sha256=irr['ps2_sha256'])
 result['rim_scale']=entry['scalars'][1]
 result['rim_constants']=[struct.unpack_from('<f',elf,a-0xff000)[0]for a in [0x4a09f0,0x4a43c0,0x4a09f4,0x4a09f8,0x4a0a08]]
 if painter:result['painter']=painter
 return result

# Original PS2 terrain light pages for rendering (see docs/terrain-render-fidelity.md,
# "Original PS2 terrain/scenery combine"). Patch layer slot type5 is the light
# layer drawn in GS context 2 with ALPHA_2=0x81, (Cd-Cs)*As>>7. Pages keep raw
# PSMCT32 bytes (alpha 0..255 is the blend factor); the CPU lattice's guard
# row/column is dropped and each page gets a 1-texel replicated gutter so
# bilinear atlas sampling equals the original CLAMP_2 (WMS/WMT=CLAMP) edges.
def _png(w,h,rgba):
 def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
 rows=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows,6))+chunk(b'IEND',b'')

def terrain_lighting(j,raw,out):
 light_textures={}
 for p in j['patches']:
  kinds=[(p['flags']>>(3*s))&7 for s in range(3)];slots=[s for s in range(3)if kinds[s]==5]
  if len(slots)!=1 or p['textures'][slots[0]]<0:raise ValueError(f"Patch {p['resource']} has no single PS2 light layer")
  light_textures.setdefault(p['textures'][slots[0]],[]).append(p)
 pages=[];atlas_w=1024;x=y=shelf=0
 for index in sorted(light_textures,key=lambda i:(-j['textures'][i]['height'],i)):
  t=j['textures'][index]
  if t['source_kind']!=10 or t['source_format']!=5:raise ValueError('Light layer is not a raw PSMCT32 page')
  w,h=t['width'],t['height']
  if x+w+2>atlas_w:x,y,shelf=0,y+shelf,0
  pages.append(dict(texture=index,source_id=t['source_id'],x=x+1,y=y+1,width=w,height=h));x+=w+2;shelf=max(shelf,h+2)
 atlas_h=1
 while atlas_h<y+shelf:atlas_h*=2
 atlas=bytearray(atlas_w*atlas_h*4)
 for page in pages:
  t=j['textures'][page['texture']];w,h=page['width'],page['height'];src=raw[t['rgba_offset']:t['rgba_offset']+(w+1)*(h+1)*4]
  for yy in range(-1,h+1):
   sy=min(max(yy,0),h-1)
   for xx in range(-1,w+1):
    sx=min(max(xx,0),w-1);o=((page['y']+yy)*atlas_w+page['x']+xx)*4;atlas[o:o+4]=src[(sy*(w+1)+sx)*4:(sy*(w+1)+sx)*4+4]
 page_of={p['texture']:i for i,p in enumerate(pages)}
 lighting=dict(version=1,source_sha256=j['source_sha256'],atlas=dict(path='terrain-light-atlas.png',width=atlas_w,height=atlas_h),pages=pages,
  combine='GS ctx1 base MODULATE (vertex RGBA 0x80), ALPHA_1=0x2A; ctx2 light page MODULATE, ALPHA_2=0x81: (Cd-Cs)*As>>7',
  patches={str(p['resource']):[page_of[idx]]+list(p['light_uv'])for idx,ps in light_textures.items()for p in ps})
 (out/'terrain-light-atlas.png').write_bytes(_png(atlas_w,atlas_h,bytes(atlas)));(out/'terrain-lighting.json').write_text(json.dumps(lighting,separators=(',',':')))
 print('PS2 terrain light pages',len(pages),'atlas',atlas_w,'x',atlas_h)

# Original tWPIGD_ScreenTint (factory 7, constructor 2BC890, vtable 484B70) point
# tree from the course's world painter record (ARA1: its single record, chunk 33 track 8).
# Payload = rate + RGB scale + RGB add (blend 2BD1B8 maps payload[1..6] to
# +8/+10/+18/+20/+28/+30). A course record without a ScreenTint section yields an
# explicit absent package (absent=true, nodes=[], payloads=[]): the driver would then
# always reset to the identity defaults, i.e. no tint pass (3905E8 skips identity).
def screen_tint(location,out):
 from world_assets import world_chunks,records,locations
 from import_sky import chunk_range,painter_sections,painted_entries
 painter_folder=root/'local/assets/source/ps2';_,tint_begin,tint_end=chunk_range(locations(painter_folder/'bam.sdb'),location);tint_found=[];records_seen=[]
 for chunk_index,chunk in enumerate(world_chunks(painter_folder/'bam.ssb')):
  if chunk_index>tint_end:break
  if chunk_index<tint_begin:continue
  for kind,track,rid,data in records(chunk):
   if kind==15 and len(data)>=64:records_seen.append((chunk_index,track,rid,data))
   if kind==15 and len(data)>=64 and 7 in painter_sections(data):tint_found.append((chunk_index,track,rid,data,painter_sections(data)[7]))
 if location!='ARA1' and not tint_found and len(records_seen)==1:
  chunk_index,track,rid,data=records_seen[0]
  (out/'screen-tint.json').write_text(json.dumps(dict(version=1,location=location,absent=True,chunk=chunk_index,track=track,rid=rid,record_sha256=hashlib.sha256(data).hexdigest(),
   scale=1.0,origin=[0.0,0.0],root=0,nodes=[],outside_words=[0,0xffffffff],payloads=[],defaults=dict(scale=[1,1,1],add=[0,0,0])),separators=(',',':')))
  print('ScreenTint absent in',location);return
 if len(tint_found)!=1:raise ValueError(f'Expected one {location} ScreenTint section')
 tint_chunk,tint_track,tint_rid,tint_record,tint_section=tint_found[0];tint_header=struct.unpack_from('<I',tint_section)[0]
 tint_scale,tint_x,tint_y=struct.unpack_from('<3f',tint_section,tint_header);tint_count=struct.unpack_from('<I',tint_section,tint_header+12)[0];tint_root=struct.unpack_from('<H',tint_section,tint_header+20)[0]
 if tint_header+40+tint_count*8>len(tint_section) or tint_root>=tint_count:raise ValueError('Invalid ScreenTint tree extent')
 tint_nodes=[list(struct.unpack_from('<4H',tint_section,tint_header+40+i*8)) for i in range(tint_count)]
 tint_payloads=[]
 for kind,values in painted_entries(tint_section,7):
  if kind!=7:raise ValueError('ScreenTint section entry with another type')
  tint_payloads.append(dict(rate=values[0],scale=list(values[1:4]),add=list(values[4:7])))
 for node in tint_nodes:
  if node[0]&1:
   if any(v>>1>=tint_count for v in node):raise ValueError('ScreenTint child outside tree')
  elif (node[2]|node[3]<<16)!=0xffffffff and (node[2]|node[3]<<16)>=len(tint_payloads):raise ValueError('ScreenTint leaf outside payloads')
 tint_outside=list(struct.unpack_from('<2I',tint_section,tint_header+24))
 if tint_outside[0]&1 or (tint_outside[1]!=0xffffffff and tint_outside[1]>=len(tint_payloads)):raise ValueError('Invalid ScreenTint outside leaf')
 package=dict(version=1,location=location,chunk=tint_chunk,track=tint_track,rid=tint_rid,record_sha256=hashlib.sha256(tint_record).hexdigest(),
  scale=tint_scale,origin=[tint_x,tint_y],root=tint_root,nodes=tint_nodes,outside_words=tint_outside,payloads=tint_payloads,defaults=dict(scale=[1,1,1],add=[0,0,0]))
 if location!='ARA1':
  from course_painters import start_selection
  selection=start_selection(location,dict(scale=tint_scale,origin=[tint_x,tint_y],root=tint_root,nodes=tint_nodes,outside_words=tint_outside))
  if selection is not None:package['start']=dict(entry=selection['entry'],uniform_near_start=selection['uniform'],position_cm=selection['position'])
 (out/'screen-tint.json').write_text(json.dumps(package,separators=(',',':')))
 print('ScreenTint nodes',len(tint_nodes),'payloads',len(tint_payloads),'chunk',tint_chunk,'track',tint_track)

def prepare(location='ARA1',output=None):
 source=root/'local/assets/native'/location;out=Path(output) if output else root/'web/public/assets'/location;out.mkdir(parents=True,exist_ok=True)
 j=json.loads((source/'environment-lighting.json').read_text());raw=bytearray()
 if j['location']!=location:raise ValueError(f'{source} environment-lighting.json belongs to {j["location"]}')
 for texture in j['textures']:
  for field in ['rgba','valid']:
   texture[field+'_offset']=len(raw);raw.extend((source/texture[field]).read_bytes())
 initial=initial_environment(location)
 if initial is not None:j.update(initial_ambient=initial['ambient'],initial_ratio=initial['ratio'],force_next=initial['force_next'])
 irr=json.loads((root/'local/assets/native/IRRADIANCE/irradiance.json').read_text())
 from inspect_disc import EXPECTED_SHA1
 elf=(root/'local/disc/SLUS_207.72').read_bytes()
 if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected environment executable')
 j['irradiance']=irradiance_package(location,irr,elf)
 (out/'environment.bin').write_bytes(raw);(out/'environment.json').write_text(json.dumps(j));print('Original lighting bytes',len(raw))
 terrain_lighting(j,raw,out)
 screen_tint(location,out)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
 p.add_argument('--location',default='ARA1');p.add_argument('--output',type=Path,help='Override web/public/assets/<location> (e.g. a scratch dir)')
 a=p.parse_args();prepare(a.location,a.output)
