"""Package only the private course/rider inputs needed by the browser demo.

python3 prepare.py                  Snow Jam (ARA1), riders, sky and shared FX (historical default)
python3 prepare.py --location BRA2  another course into web/public/assets/BRA2 (docs/locations.md)
"""
import json,struct,zlib,shutil,array,subprocess,sys,argparse,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];src=root/'local/assets/native';out=root/'web/public/assets';out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'tools'))
import importlib.util
from locations import location as location_info,activation_dir,pickups_dir,web_manifest_entry,LOCATIONS
def png(w,h,rgba):
 def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
 raw=b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,3))+chunk(b'IEND',b'')
parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--location',default='ARA1',help='Course location code (tools/locations.py); ARA1 also packages riders, sky and shared FX')
parser.add_argument('--out',type=Path,help='Output asset root (default web/public/assets); use a scratch folder to verify byte identity')
parser.add_argument('--batches-only',action='store_true',help='Only re-split the course draw batches (world.json batches, indices.bin, vertex-alpha.bin, route.json) from the source package; textures, rails, riders, sky and courses.json are left as they are')
args=parser.parse_args();LOCATION=args.location;location_info(LOCATION)
if args.out:out=args.out;out.mkdir(parents=True,exist_ok=True)
ARA1=LOCATION=='ARA1'
def package_world(name):
 a=src/name;dest=out/name;dest.mkdir(parents=True,exist_ok=True)
 d=json.loads((a/'world.json').read_text())
 if name.startswith('RIDER_'):
  for key,t in d['textures'].items():
   target=dest/(key+'.png');target.write_bytes(png(t['width'],t['height'],(a/t['path']).read_bytes()));t['path']=target.name
 else:
  # World textures: references into the shared world texture library (TEXTURES/world.tex), the package's GameCube
  # lightmaps in lightmaps.tex (tools/export_world_textures.py); no per-location PNG copies.
  from export_world_textures import package_textures
  package_textures(dest,d['textures'],lambda key,t:(a/t['path']).read_bytes())
 for f in ['vertices.bin','indices.bin','colors.bin']:
  shutil.copy2(a/f,dest/f)
 if not name.startswith('RIDER_'):
  for file in ['terrain.json','world_collision.json','rails.json']:shutil.copy2(a/file,dest/file)
  flags_state=root/'local/reference/pcsx2'/location_info(name)['states']['glide']
  if flags_state.exists():subprocess.run([sys.executable,str(root/'tools/export_rail_runtime_flags.py'),'--location',name,str(dest/'rails.json')],check=True)
  else:print(f'WARNING {name}: no glide savestate {flags_state.name}; rails keep disc flags (runtime query flags unknown)')
 if name.startswith('RIDER_'):
  for f in ['rider.json','animation-samples.json']:shutil.copy2(a/f,dest/f)
 else:
  vb=array.array('f');vb.frombytes((a/'vertices.bin').read_bytes());ib=array.array('I');ib.frombytes((a/'indices.bin').read_bytes());tris=array.array('f')
  for b in d['batches']:
   if b['instance']:continue
   for i in ib[b['first_index']:b['first_index']+b['index_count']]:tris.extend(vb[i*10:i*10+3])
  (dest/'collision.bin').write_bytes(tris.tobytes())
  if (a/'riding-start.json').exists():start=json.loads((a/'riding-start.json').read_text())['native']['initial'];start={k:start[k] for k in ['position','heading']}
  else:
   # No riding checkpoint yet: provisional spawn at the human's grid slot (event-start.json) or the
   # origin of the first race path, facing along its first segment (flagged in the package).
   start=provisional_start(a)
  (dest/'start.json').write_text(json.dumps(start))
 (dest/'world.json').write_text(json.dumps(d,separators=(',',':')))
def provisional_start(a):
 import math
 if (a/'event-start.json').exists():
  human=next(p for p in json.loads((a/'event-start.json').read_text())['participants'] if p['race']['human'])
  x,y,z=human['race']['position'];w,qx,qy,qz=human['race']['quaternion'][3],*human['race']['quaternion'][:3]
  forward=[1-2*(qy*qy+qz*qz),2*(qx*qy+w*qz),2*(qx*qz-w*qy)]
  return dict(position=[x/100,z/100,-y/100],heading=math.atan2(forward[0],-forward[1]),provisional='event-start human grid slot')
 from course_painters import start_grid
 grid=start_grid(a.name)
 if grid:
  # Disc start grid (kind-0 rows of the course AIP region table; slot 0 is the human, Snow Jam-verified).
  x,y,z=grid[0]['position'];dx,dy=grid[0]['direction'][:2]
  return dict(position=[x/100,z/100,-y/100],heading=math.atan2(dx,-dy),provisional='disc AIP start grid slot 0')
 if (a/'race-event.json').exists():paths=json.loads((a/'race-event.json').read_text())['original_race_event']['paths'];why='race path 0 origin';p=paths[0]
 else:
  # Disc-only: the course's authored AIP track paths (SSB kind 14, rid 0; the runtime race paths are
  # byte-identical copies, events remapped). Start at the highest path origin (the downhill start).
  from world_assets import world_chunks,records,locations
  from race_event_assets import decode_aip
  track=next(i for i,l in enumerate(locations(root/'local/assets/source/ps2/bam.sdb')) if l['name']==a.name);aip=None
  for chunk in world_chunks(root/'local/assets/source/ps2/bam.ssb'):
   aip=next((decode_aip(d) for k,t,r,d in records(chunk) if k==14 and t==track and r==0),None)
   if aip:break
  p=max(aip['track_paths'],key=lambda q:q['position'][2]);p=dict(origin=p['position'],segments=p['segments']);why='highest authored AIP track path origin'
 x,y,z=p['origin'];s=p['segments'][0]
 return dict(position=[x/100,z/100,-y/100],heading=math.atan2(s[0],-s[1]),provisional=why)
if args.batches_only:
 # The source layout (collision_sources index the source triangle order) with the packaged texture names.
 source_world=json.loads((src/LOCATION/'world.json').read_text())
 source_world['textures']=json.loads((out/LOCATION/'world.json').read_text())['textures']  # the packaged texture table (library references)
 (out/LOCATION/'world.json').write_text(json.dumps(source_world,separators=(',',':')));shutil.copy2(src/LOCATION/'indices.bin',out/LOCATION/'indices.bin')
elif ARA1:
 for name in ['ARA1','RIDER_SAM','RIDER_ZOE']:package_world(name)
 print('Prepared private ARA1 / Sam assets')
from prepare_course_sky import prepare_course_sky  # SKY (ARA1) or <LOC>/sky/: the area's camera-anchored dome
if args.batches_only:pass
elif ARA1:prepare_course_sky('ARA1',out/'SKY')
else:
 package_world(LOCATION);print('Prepared private',LOCATION,'course assets');prepare_course_sky(LOCATION,out/LOCATION/'sky')
# Spatially split draw batches so the browser can cull distant course sections.
dest=out/LOCATION;d=json.loads((dest/'world.json').read_text());vs=array.array('f');vs.frombytes((dest/'vertices.bin').read_bytes());inds=array.array('I');inds.frombytes((dest/'indices.bin').read_bytes());groups={}
spec=importlib.util.spec_from_file_location('world_batches',root/'web/world-batches.py');batch_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(batch_module)
# Event evidence captured from the location's own PS2 event savestates (docs/locations.md):
# pickup runtime bindings (glide), countdown instance audit and scripted instances (countdown).
bindings_path=pickups_dir(LOCATION)/'runtime-bindings.json';audit_path=activation_dir(LOCATION)/'countdown-instances.json';scripted_path=activation_dir(LOCATION)/'scripted-instances.json'
event_path=src/LOCATION/'event-start.json'
have_event=all(p.exists() for p in (bindings_path,audit_path,scripted_path,event_path))
if ARA1 or have_event:
 # Entity-owned pickups (a live moving set piece, e.g. BRA2 speedboost_1000) stay ordinary scenery batches (tools/export_browser_pickups.py).
 pickups=[p for p in json.loads(bindings_path.read_text())['instances'] if not p.get('entity')]
 audit=json.loads(audit_path.read_text())
 if hashlib.sha256((src/LOCATION/'world_collision.json').read_bytes()).hexdigest()!=audit['world_package_sha256']:raise ValueError('Event visibility audit/world mismatch')
 event=json.loads(event_path.read_text())
 if audit['ee_sha256']!=event['provenance']['ee_sha256'] or audit['course_sha256']!=event['course_sha256']:raise ValueError('Event visibility snapshot mismatch')
else:
 missing=[str(p.relative_to(root)) for p in (bindings_path,audit_path,scripted_path,event_path) if not p.exists()]
 print(f'WARNING {LOCATION}: event evidence missing ({", ".join(missing)}); drawing every imported instance (helpers/free-ride props not hidden, pickups static)')
 pickups=[];audit=dict(instances=[],dead_resources=[],course_sha256=None)
# Original static-model GS state (37E238 model draw, 37F2A4..37F6C8): model header
# +10 bit3 -> additive ALPHA 0x48 (blend 3); otherwise material word+12 (|0x40000 for
# group flag bit3) & 0x660000: 0 opaque (1), 0x20000 ALPHA 0x44 + ATST GREATER 92
# (blend 1), 0x40000/0x60000 ALPHA 0x44 + ATST GREATER 20, depth sorted (blend 2).
# Env-map second-texture variants (0x200000/0x400000) keep their base class.
from world_assets import world_chunks,records,locations,event_locations
from world_models import decode_model
ps2=root/'local/assets/source/ps2';locs=locations(ps2/'bam.sdb')
# Race-event residency (the course and its connectors, tools/import_world.py --event).
resident=[(b,e) for _,_,b,e in event_locations(locs,LOCATION)]
if [x['name'] for x in d['event_locations']]!=[n for _,n,_,_ in event_locations(locs,LOCATION)]:raise ValueError('World package residency differs')
model_records,material_records,instance_records={},{},{}
for index,chunk in enumerate(world_chunks(ps2/'bam.ssb')):
 if index>max(e for _,e in resident):break
 if index!=0 and not any(b<=index<=e for b,e in resident):continue
 for kind,track,rid,data in records(chunk):
  if kind==2:model_records[track,rid]=data
  elif kind==0:material_records[track,rid]=data
  elif kind==3 and index!=0:instance_records[track,rid]=data
model_blends={}
def mesh_blend(model,mesh):
 key=tuple(model)
 if key not in model_blends:
  data=model_records[key];header=struct.unpack_from('<I',data,16)[0];classes=[]
  for m in decode_model(data):
   word=struct.unpack_from('<I',material_records[m['material']],12)[0]|(0x40000 if m['group_flags']&8 else 0)
   classes.append(3 if header&8 else {0:0,0x20000:1,0x40000:2,0x60000:2}[word&0x60000])
  model_blends[key]=classes
 return model_blends[key][mesh]
# Texture addressing, same material word (37F2BC..37F354): & 0x180000 -> GS CLAMP_1 (0x80000 u, 0x100000 v; neither =
# REPEAT). World static models carry the bits like the sky (CRA3 rock polys, an ABA1 log mesh, CHP2 volumes): batches
# are split by them (batch.wrap = 1 u / 2 v / 3 both, web/world-material.js).
model_wraps={}
def mesh_wrap(model,mesh):
 key=tuple(model)
 if key not in model_wraps:model_wraps[key]=[(struct.unpack_from('<I',material_records[m['material']],12)[0]>>19)&3 for m in decode_model(model_records[key])]
 return model_wraps[key][mesh]
# Env-map second pass (37F2A4..37FD2C; the global gp+0x1404 that would strip the bits is 0 in every race savestate): the material word
# (with group flag bit3 -> 0x40000) & 0x660000 in 0x200000 / 0x220000 / 0x260000 draws the triangles again in GS context 2 with the record's
# second texture (halfword +2) and ALPHA_2 enum 2 (Cs + Cd); 0x600000 / 0x620000 / 0x660000 with enum 17 (Cs x Ad + Cd). Any other value
# with 0x400000 / 0x200000 takes the default state (no second pass). web/world-batches.py tags the batches env = [texture, mode].
ENV_MODES={0x200000:0x200000,0x220000:0x200000,0x260000:0x200000,0x600000:0x600000,0x620000:0x600000,0x660000:0x600000}
model_envs={}
def mesh_env(model,mesh):
 key=tuple(model)
 if key not in model_envs:
  out=[]
  for m in decode_model(model_records[key]):
   rec=material_records[m['material']];word=struct.unpack_from('<I',rec,12)[0]|(0x40000 if m['group_flags']&8 else 0);mode=ENV_MODES.get(word&0x660000)
   out.append((struct.unpack_from('<h',rec,2)[0],mode) if mode else 0)
  model_envs[key]=out
 return model_envs[key][mesh]
triangle_env={}
for source in d['collision_sources']:
 if source['kind']!='instance':continue
 env=mesh_env(source['model'],source['mesh'])
 if env:
  for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):triangle_env[tri]=env
if __import__('os').environ.get('SSX_ENV_SPLIT')=='0':triangle_env={}  # comparisons: the batches without the env split
if triangle_env:
 # the second textures join the package's texture table as world texture library references (like every 9- entry)
 from export_world_textures import library_index,LIBRARY_URL
 for tex in sorted({e[0] for e in triangle_env.values()}):
  e=library_index()[tex];d['textures'].setdefault(f'9-{tex}',dict(width=e['width'],height=e['height'],source='ps2',fallback_reason=None,pack=LIBRARY_URL,id=tex))
print('Env-map static-model triangles',len(triangle_env))
triangle_blend={};triangle_wrap={}
for source in d['collision_sources']:
 if source['kind']!='instance':continue
 blend=mesh_blend(source['model'],source['mesh'])
 if blend:
  for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):triangle_blend[tri]=blend
 wrap=mesh_wrap(source['model'],source['mesh'])
 if wrap:
  for tri in range(source['first_triangle'],source['first_triangle']+source['triangle_count']):triangle_wrap[tri]=wrap
print('Blended static-model triangles',len(triangle_blend),'clamped',len(triangle_wrap))
# VIF UNPACK V4-5 vertex colour: A = bit15<<7, so As = Ta*Av>>7 is zero where
# bit15 is clear (fading beam/glow ends). The importer kept RGB only; restore
# the alpha bit per instance vertex (vertex-alpha.bin, one byte per vertex,
# 128 = 0x80) after checking the RGB matches; colors.bin stays byte-identical.
def instance_colour_words(data):
 at,words=160,[]
 while at+4<=len(data):
  word=struct.unpack_from('<I',data,at)[0];at+=4
  if word>>24 not in (0x6f,0x7f):continue
  count=((word>>16)&255) or 256;words.extend(struct.unpack_from(f'<{count}H',data,at));at=(at+count*2+3)&~3
 return words
colour_values=array.array('f');colour_values.frombytes((dest/'colors.bin').read_bytes());vertex_alpha=bytearray([128])*(len(colour_values)//4);cleared=0;mesh_cache={}
for source in d['collision_sources']:
 if source['kind']!='instance':continue
 key=tuple(source['model'])
 if key not in mesh_cache:mesh_cache[key]=decode_model(model_records[key])
 mesh=mesh_cache[key][source['mesh']];words=instance_colour_words(instance_records[source['track'],source['rid']])
 tri=source['first_triangle'];base=min(inds[tri*3:(tri+source['triangle_count'])*3])
 for i in range(len(mesh['vertices'])):
  value=words[mesh['color_offset']+i];at=(base+i)*4
  if any(abs(colour_values[at+k]-((value>>(5*k))&31)/31)>1e-6 for k in range(3)):raise ValueError(f"Instance colour/vertex mismatch {source['track']}:{source['rid']}")
  if not value>>15:vertex_alpha[base+i]=0;cleared+=1
(dest/'vertex-alpha.bin').write_bytes(bytes(vertex_alpha));print('Static-model vertices with V4-5 alpha 0:',cleared)
if ARA1 or have_event:
 scripted=json.loads(scripted_path.read_text())
 if scripted['world_package_sha256']!=audit['world_package_sha256']:raise ValueError('Scripted instance export/world mismatch')
 rollers=[x['resource'] for x in scripted['instances'] if x['contact']=='roller']  # tools/export_scripted_instances.py
else:rollers=[]
# Chairlift cars (MultiSplineModifier, tools/export_set_pieces.py): the authored tramlores batches follow car 0
# and web/moving-instances.js clones them for cars 1.. (clone instances of the original).
set_pieces_path=activation_dir(LOCATION)/'set-pieces.json'  # ARA1: local/event-activation/set-pieces.json
if (ARA1 or have_event) and set_pieces_path.exists():
 set_pieces=json.loads(set_pieces_path.read_text())
 if set_pieces['world_package_sha256']!=audit['world_package_sha256']:raise ValueError('Set-piece export/world mismatch')
 rollers=rollers+[l['resource'] for l in set_pieces.get('chairlifts',[])]+set_pieces.get('drawn_spline_pieces',[])  # raven flyby (SplineModifier)
 # other locations (tools/export_set_pieces.py --location): MultiSpline cars (bins, traffic) and looping splines (blimp)
 rollers=rollers+[m['resource'] for m in set_pieces.get('multisplines',[])]+[m['resource'] for m in set_pieces.get('spline_modifiers',[])]
# Avalanche pieces (tools/export_avalanches.py -> <LOC>/avalanches.json, docs/avalanche.md): the group instances a builtin-95
# AvaSpline makes follow their tumbler (0x2D9C00) get their own batches, moved by web/moving-instances.js from the core's
# moving_instances() while a tumbler drives them.
avalanche_path=out/LOCATION/'avalanches.json'
if not ARA1 and avalanche_path.exists():
 rollers=rollers+[g['resource'] for a in json.loads(avalanche_path.read_text())['avalanches'] for g in a['groups'] if g['ava_spline'] and g['resource'] not in rollers]
# Instances the original never draws in the race event (countdown audit 'draw', tools/export_event_instances.py):
# static collector 22A5A0 needs runtime (flags&3)==3, dynamic entities draw through vtable+0x20. This hides
# the helpers with bit0 clear (RaceRideState, reset planes, fcollision/bcvolume, emitter/trigger placeholders,
# cameraflash; the PS2 event-start Z buffer holds the stadium where the RaceRideState box would write 3.3 m)
# and the free-ride Big Challenge gates/flags/arrows (type-16 nodes, flags 0x210005/0x210305, empty draw).
# DeadNodes keep their event_dead_resource batches (shown in the non-event QA fixtures).
hidden=[r['resource'] for r in audit['instances'] if r['draw']=='none' and r['resource'] not in set(audit['dead_resources'])]
# Waving flags (tools/export_flags.py, web/flag-animation.js) replace their static batches; UV-scrolling
# instances (tools/export_uv_scroll.py, web/uv-scroll.js) are batched per scroll group (identical initial state).
set_piece_assets=out if ARA1 else out/LOCATION  # Snow Jam: /assets/FLAGS etc.; other locations under their root
flags_path,scroll_path=set_piece_assets/'FLAGS/flags.json',set_piece_assets/'UVSCROLL/uv-scroll.json'
if flags_path.exists():hidden=sorted(set(hidden)|{x['resource'] for x in json.loads(flags_path.read_text())['instances']})
scroll_groups=None
if scroll_path.exists():
 keys={};scroll_groups={}
 for x in json.loads(scroll_path.read_text())['instances']:scroll_groups[x['resource']]=keys.setdefault(json.dumps(x['initial'],sort_keys=True),len(keys))
# LiveComp animation players (tools/export_livecomp.py, web/livecomp-animation.js): batches split per animated node.
livecomp_path=set_piece_assets/'LIVECOMP/livecomp.json';livecomp_nodes=None
if livecomp_path.exists():livecomp_nodes={(x['resource'],m):node for x in json.loads(livecomp_path.read_text())['instances'] for m,node in enumerate(x.get('meshNodes') or [])}
# Log teeters (tools/export_rail_teeters.py): node-1 meshes follow the AnimTeeter node (core set_piece_teeters);
# meshes are numbered in node order (meshGroups per node).
teeter_path=root/'local/event-activation/rail-teeters.json'
if ARA1 and teeter_path.exists():
 livecomp_nodes=livecomp_nodes or {}
 for t in json.loads(teeter_path.read_text())['teeters']:
  mesh=0
  for node,n in enumerate(t['animModel']['nodes']):
   for _ in range(n['meshGroups']):livecomp_nodes[(t['resource'],mesh)]=node;mesh+=1
# Attached set pieces (tools/export_attached_setpieces.py, web/attached-setpieces.js): drawn spline LiveComps (raven,
# blimp) split per node instead of whole-instance moving draws; ParentModifier children split (node 0) and drawn.
attached_path=set_piece_assets/'LIVECOMP/attached.json'
if attached_path.exists():
 attached=json.loads(attached_path.read_text());livecomp_nodes=livecomp_nodes or {}
 spline_drawn={x['resource'] for x in attached['splineLiveComps'] if x['drawn']}
 for x in attached['splineLiveComps']:
  if x['resource'] in spline_drawn:
   for m,node in enumerate(x['meshNodes']):livecomp_nodes[(x['resource'],m)]=node
 children={p['child'] for p in attached['parents'] if p['parentKind']!='object'}
 for p in attached['parents']:
  if p['child'] in children:
   for m in range(p['childMeshes']):livecomp_nodes[(p['child'],m)]=0
 rollers=[r for r in rollers if r not in spline_drawn]
 hidden=[r for r in hidden if r not in children]
# Stage-world instances (tools/export_stage_world.py): instances whose flags the stage programs change at run time
# (builtins 1/2/13/29/58: DeadNode / RestoreNode / Hide / breaking) get their own batches (script_resource) and follow
# the core's instance state; MeshAnim targets (builtin13 break pieces) are split per node like the LiveComps.
stage_world_path=out/LOCATION/'STAGE/stage-world.json';script_resources=set();meshanim_nodes=None;dead_batches=audit['dead_resources']
if stage_world_path.exists():
 stage_world=json.loads(stage_world_path.read_text())
 meshanim_nodes={(m['resource'],mesh):node for m in stage_world['meshanim'] for mesh,node in enumerate(m['meshNodes'])}
 # Conquer the Mountain collectibles (builtin38 lists): DeadNodes only in single events (30C4A8 reads 0x535C11), so their
 # batches follow the core's instance state (career races draw the uncollected ones) instead of the event_dead tag.
 collectibles={x['resource'] for v in stage_world.get('collections',{}).values() for x in v}
 dead_batches=[r for r in audit['dead_resources'] if r not in collectibles]
 owned=set(p['resource'] for p in pickups)|set(dead_batches)|set(rollers)
 script_resources=({x['resource'] for x in stage_world['script']}|collectibles)-owned
# Texture chunks (tools/export_sections.py): batches split per chunk so the renderer can skip instances whose
# chunk the original has not streamed in (0x22A5A0; web/section_gameplay.inc section_chunks()).
sections_path=out/LOCATION/'SECTIONS/sections.json';instance_chunks=None
if sections_path.exists():instance_chunks={int(k,16):(c[1] if c[0]!=255 else -1) for k,c in json.loads(sections_path.read_text())['instance_chunks'].items()}
# Lit instances (tools/export_lit_instances.py): each one's light-cache rows (37E238 -> 2F5400: the object bank plus up to 4 local lights
# at its position; 0x1000 ones relit as they move) on its own batches (web/world-material.js litWorldMaterial, pv litInstances).
lit_path=activation_dir(LOCATION)/'lit-instances.json';lit_resources=None
if lit_path.exists():
 lit_doc=json.loads(lit_path.read_text())
 if lit_doc['world_package_sha256']!=audit.get('world_package_sha256'):raise ValueError('Lit instance export/world mismatch')
 lit_resources={x['resource']:dict(resource=x['resource'],bank=x['bank'],rows=[r[:3] for r in x['rows']],**({'relight':True} if x['relight'] else {})) for x in lit_doc['instances']}
batches,new=batch_module.spatial_batches(d,vs,inds,[p['resource'] for p in pickups],dead_batches,triangle_blend,rollers,hidden,scroll_groups,livecomp_nodes,instance_chunks=instance_chunks,script_resources=script_resources,meshanim_nodes=meshanim_nodes,triangle_wrap=triangle_wrap,triangle_env=triangle_env,lit_resources=lit_resources)
d['event_dead_resources']=audit['dead_resources'];d['event_course_sha256']=audit['course_sha256']
d['batches']=batches;(dest/'indices.bin').write_bytes(new.tobytes());(dest/'world.json').write_text(json.dumps(d,separators=(',',':')))
race_path=src/LOCATION/'race-event.json'
if race_path.exists():
 paths=json.loads(race_path.read_text())['original_race_event']['paths'];points=[]
 # ARA1: the Snow Jam path chain; other locations list their paths in authored order (route.json is diagnostic only).
 for index in ([0,3,4,5,6,7,2] if ARA1 else [p['index'] for p in paths]):
  p=next(p for p in paths if p['index']==index);o=p['origin'][:]
  for s in p['segments']:
   points.append([o[0]/100,o[2]/100,-o[1]/100])
   for k in range(3):o[k]+=s[k]*s[3]
else:points=[];print(f'WARNING {LOCATION}: no race-event.json; empty route')
(dest/'route.json').write_text(json.dumps(points));print('Spatial batches:',len(batches),'route points:',len(points))

if args.batches_only:sys.exit(0)
if ARA1:
 trail=out/'BOARD_TRAIL';trail.mkdir(exist_ok=True)
 for file in ['board_trail.json','btrl.rgba']:shutil.copy2(src/'BOARD_TRAIL'/file,trail/file)

 snow=out/'SNOW_FX';snow.mkdir(exist_ok=True)
 fx=json.loads((src/'SNOW_FX/snow-fx.json').read_text());shutil.copy2(src/'SNOW_FX/snow-fx.json',snow/'snow-fx.json')
 for texture in fx['textures']:
  name=texture.get('gs_alpha_file',texture['file']);shutil.copy2(src/'SNOW_FX'/name,snow/name)

 # Original ARA1 spatial Fog painter package.
 from export_fog_tree import export as export_fog_tree
 export_fog_tree()
 shutil.copy2(root/"local/event-activation/fog-tree.json",out/"ARA1/fog-tree.json")

 # Original rider text from the owned English front-end locale.
 spec=importlib.util.spec_from_file_location('rider_locale',root/'tools/sam_ps2/loc_file.py');locale_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(locale_module)
 locale_bytes=(root/'local/sam-ps2/original/FEAMER.LOC').read_bytes();locale=locale_module.entries(locale_bytes)
 zoe_bio=locale[locale_module.name_hash('kT_FULLBIO1Zoe')]
 roster=[dict(id='sam',name='Sam',package='RIDER_SAM',card=['A chopped unc from Wisconsin. Prefers uphill','and keeps his head above his board.'],bio='From Wisconsin. Grew up riding the flat hills of the Midwest. Regular stance. Not keen on going inverted. These days he prefers uphill to down.'),dict(id='zoe',name='Zoe',package='RIDER_ZOE',card=zoe_bio.split('  ')[:2],bio=zoe_bio,locale_sha256=hashlib.sha256(locale_bytes).hexdigest())]
 (out/'riders.json').write_text(json.dumps(roster,indent=2)+'\n')
else:
 fog=activation_dir(LOCATION)/'fog-tree.json'
 if fog.exists():shutil.copy2(fog,dest/'fog-tree.json')
 else:print(f'WARNING {LOCATION}: no {fog.relative_to(root)} (tools/export_fog_tree.py --location {LOCATION})')
# Course/event manifest for the browser course select (courses.json): every registered location
# whose world package has been prepared, with its asset roots (tools/locations.web_manifest_entry).
# eventStart: the grid/countdown start is available (event-start.json exported and compiled into the core seeds,
# tools/generate_event_seed.py); otherwise the browser rides freely from start.json.
manifest=[dict(web_manifest_entry(code),ready=(out/code/'world.json').exists(),eventStart=(src/code/'event-start.json').exists()) for code in LOCATIONS]
(out/'courses.json').write_text(json.dumps(dict(version=1,default='ARA1',courses=[c for c in manifest if c['ready']]),indent=1)+'\n')
