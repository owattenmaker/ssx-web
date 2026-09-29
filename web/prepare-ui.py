"""Read-only import of original PS2 UI/font data and decoded animation banks."""
import json,struct,shutil,sys,subprocess,hashlib,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from inspect_disc import Disc
import argparse
from locations import state as location_state,human_rider,web_dir
LOCATION=(lambda p:(p.add_argument('--location',default='ARA1',help='Non-ARA1: only write web/public/assets/<LOC>/initial.json from its savestates'),p.parse_args().location)[1])(argparse.ArgumentParser(description=__doc__))
a=root/'local/assets/native'
def build_initial(location='ARA1'):
 """initial.json for a course from its own savestates (tools/locations.py): riding-start (glide),
 the glide rider state and the countdown-anchor event route. Snow Jam: ANIMATIONS/initial.json."""
 initial=json.loads((a/location/'riding-start.json').read_text())['native']['initial']
 phoff=struct.unpack_from('<I',elf,28)[0];stride,count=struct.unpack_from('<HH',elf,42);table=None
 for i in range(count):
  typ,off,addr,_,size,_,_,_=struct.unpack_from('<8I',elf,phoff+i*stride)
  if typ==1 and addr<=0x459e20 and 0x459e40<=addr+size:table=elf[off+0x459e20-addr:off+0x459e40-addr]
 assert table is not None,'Original grab hold threshold table not mapped'
 thresholds=[dict(seconds=s,points=p) for s,p in struct.iter_unpack('<ff',table)]
 initial['original_grab_score']=dict(profile=dict(normal=initial['original_grab_control']['profile']['grabs'],hold_thresholds=thresholds),provenance=dict(elf_sha256=hashlib.sha256(elf).hexdigest(),table='0x459e20'))
 from reference_trick_identity import extract_trick_identity
 from reference_boost import extract_boost
 with zipfile.ZipFile(location_state(location,'glide')) as snapshot: memory=snapshot.read('eeMemory.bin')
 rider=human_rider(memory)  # Snow Jam 0x14701A0
 initial['original_trick_identity']=extract_trick_identity(memory,rider)
 initial['original_boost']['award_context']=extract_boost(memory,rider)['award_context']
 from reference_reset import extract_reset
 initial['original_reset']=extract_reset(memory,rider)
 # The race start (grid countdown anchor) retains its own human route: path 2 at distance 0,
 # not the glide savestate's mid-course progress. 0x4CC heading feeds groundForwardDrive.
 with zipfile.ZipFile(location_state(location,'anchor')) as snapshot: anchor=snapshot.read('eeMemory.bin')
 event_reset=extract_reset(anchor,human_rider(anchor))
 if event_reset['paths']!=initial['original_reset']['paths']:raise ValueError('Event and glide reset path banks differ')
 initial['original_reset']['event_route']=event_reset['route']
 initial['original_board_trail']['profile']=json.loads((a/'BOARD_TRAIL/board_trail.json').read_text())['profile']
 initial['original_snow']['emitter_profiles']=json.loads((a/'SNOW_FX/snow-fx.json').read_text())['profiles']
 from reference_snow_context import extract_memory as extract_snow
 snow_snapshot=extract_snow(memory,rider)
 initial['original_snow']['state']['kicker_buildup']=snow_snapshot['state']['kicker_buildup']
 initial['original_snow']['state']['emitter_flipbook_phases']=snow_snapshot['state']['emitter_flipbook_phases']
 initial['original_snow']['wake_noise_table']=list(struct.unpack_from('<161f',memory,0x445ab0))
 breath_path=root/('local/browser-validation/breath-context.json' if location=='ARA1' else f'local/browser-validation/{location}/breath-context.json')
 subprocess.run([sys.executable,str(root/'tools/export_breath_context.py'),'--output',str(breath_path)]+([] if location=='ARA1' else ['--location',location,'--state',str(location_state(location,'glide')),'--rider',hex(rider)]),cwd=root,check=True)
 initial['original_breath']=json.loads(breath_path.read_text())
 from export_browser_pickups import extract as extract_pickups
 initial['original_pickups']=extract_pickups(location)
 from reference_rail_context import extract_memory as extract_rail_context
 initial['original_rail_context']=extract_rail_context(memory,rider)
 from export_trick_names import extract as extract_trick_names
 initial['original_trick_names']=extract_trick_names()
 from reference_secondary_motion import extract_secondary_motion
 initial['original_animation']['secondary_motion']=extract_secondary_motion(memory,rider)
 return initial
if LOCATION!='ARA1':
 elf=(root/'local/disc/SLUS_207.72').read_bytes();(web_dir(LOCATION)/'initial.json').write_text(json.dumps(build_initial(LOCATION)));print('Course initial state',LOCATION);sys.exit(0)
source=root/'local/browser-ui';source.mkdir(exist_ok=True);out=root/'web/public/assets/UI';out.mkdir(parents=True,exist_ok=True)
from disc_paths import ps2_iso;disc=Disc(ps2_iso())
try:
 for name in ['DATA/UI/FE_1.SSH','DATA/UI/OV_1.SSH','DATA/UI/SU_1.SSH','DATA/FONTS/FEFONT.SFN','DATA/FONTS/HUDFONT.SFN','DATA/FONTS/MENU.SSH']:(source/Path(name).name).write_bytes(disc.file(name))
 elf=disc.file('SLUS_207.72')
finally:disc.close()
for name in ['FEFONT','HUDFONT']:
 data=(source/(name+'.SFN')).read_bytes();offset=struct.unpack_from('<I',data,28)[0];tail=data[offset:];(source/(name+'.SSH')).write_bytes(b'SHPS'+struct.pack('<II',24+len(tail),1)+b'GIMXfont'+struct.pack('<I',24)+tail)
 glyphs={}
 for i in range(struct.unpack_from('<H',data,10)[0]):
  ch,w,h,x,y,advance,dx,dy,_=struct.unpack_from('<HBBHHBbBB',data,128+i*12);glyphs[chr(ch)]=dict(w=w,h=h,x=x,y=y,advance=advance,dx=dx,dy=dy)
 (out/(name+'-glyphs.json')).write_text(json.dumps(glyphs))
subprocess.run([str(root/'local/dotnet/dotnet'),'run','--project',str(root/'tools/sam_ps2'),'--',str(root),'--export-browser-ui'],check=True)
a=root/'local/assets/native';dest=root/'web/public/assets/ANIMATIONS';dest.mkdir(exist_ok=True)
for file in ['library.json','samples.f32']:shutil.copy2(a/'ANIMATIONS'/file,dest/file)
for file in ['animation-packets.json','animation-packets.bin']:shutil.copy2(a/'RIDER_ZOE'/file,dest/file)
initial=build_initial('ARA1')
(dest/'initial.json').write_text(json.dumps(initial))
print('Original UI/fonts, 497 gameplay + 67 frontend clips, and authored scoring thresholds imported')
