#!/usr/bin/env python3
"""Compile the verified event-grid human seed using the existing field mapping."""
import json,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from locations import LOCATIONS,activation_dir,state as location_state
import hashlib
def rolling_region_row(code):
 """The human's grid row of a backcountry course (disc kind 0, index 0 = runtime kind 1, slot 0) from its AIP variant 0."""
 from race_event_assets import decode_aip
 from world_assets import world_chunks,records,locations as sdb_locations
 SOURCE=ROOT/'local/assets/source/ps2';names={i:l['name'] for i,l in enumerate(sdb_locations(SOURCE/'bam.sdb'))};found={}
 for chunk in world_chunks(SOURCE/'bam.ssb'):
  for kind,track,rid,data in records(chunk):
   if kind==14 and names.get(track)==code:
    try:found[rid]=decode_aip(data)
    except ValueError:pass  # as tools/export_peak_world.py paths_packages
 if not found:raise ValueError(f'{code}: no AIP record')
 aip=found[min(found)]  # variant 0 (tools/export_peak_world.py paths_packages: courses take variant 0)
 row=next((r for r in aip['regions'] if r[1]==0 and r[0]==0),None)
 if row is None:raise ValueError(f'{code}: no grid region row (kind 0, index 0)')
 return row
def course_seed(code):
 """One course's start seed (C++ lines) and instance seed (declarations), from its countdown evidence."""
 event=json.loads((ROOT/f'local/assets/native/{code}/event-start.json').read_text());human=next(p for p in event['participants'] if p['race']['human'])
 lines=start_lines(event)
 def f(v):return 'std::bit_cast<float>(0x%08xu)'%struct.unpack('<I',struct.pack('<f',float(v)))[0]
 constants=f'inline constexpr int startDelay={human["start_delay_seconds"]};inline constexpr float progressOrigin='+f(human['progress_origin'])+';\n'
 return course_tail(code,event,lines,constants)
def start_lines(event):
 """The human's ground profile, ground state and race participant seed functions (C++ lines) from a countdown export."""
 human=next(p for p in event['participants'] if p['race']['human']);source=(ROOT/'engine/replay_io.mm').read_text();ground=human['original_ground']
 def f(v):return 'std::bit_cast<float>(0x%08xu)'%struct.unpack('<I',struct.pack('<f',float(v)))[0]
 def literal(v):
  if isinstance(v,bool):return str(v).lower()
  if isinstance(v,list):return '{'+','.join(literal(x) for x in v)+'}'
  if isinstance(v,dict):return '{'+','.join(f(v[k]) for k in ['current','rate','target'])+'}'
  return f(v)
 profile=ground['profile'];objects={'p':profile,'surface':profile['surface'],'h':profile['heading_profile']};lines=['inline ssx::OriginalGroundProfile browserEventGroundProfile(){ssx::OriginalGroundProfile profile;']
 fragment=source[source.index('ssx::OriginalGroundProfile profile;'):source.index('auto state=readOriginalPoseState(s);')]
 for field,kind,obj,key in re.findall(r'profile\.([\w.]+)=(value|curve)\((\w+),@"([^"]+)"\)',fragment):
  v=objects[obj][key];init=literal(v)
  if kind=='curve':init='{{'+','.join('{'+','.join(f(y) for y in x)+'}' for x in v)+'}}'
  lines.append(f'profile.{field}={init};')
 lines+=['profile.surface.id='+str(profile['surface']['id'])+';','profile.speedLimitTable='+literal(profile['speed_limit_table'])+';','return profile;}','inline ssx::OriginalGroundState browserEventGroundState(){ssx::OriginalGroundState state;']
 fragment=source[source.index('static ssx::OriginalGroundState readOriginalPoseState'):source.index('void initializeNativeReplay')];state=ground['state']
 for field,kind,key in re.findall(r'state\.(\w+)=(vec|value|boolean)\(s,@"([^"]+)"\)',fragment):lines.append(f'state.{field}={literal(state[key])};')
 for field,key in re.findall(r'state\.(\w+)=control\(@"([^"]+)"\)',fragment):lines.append(f'state.{field}={literal(state[key])};')
 for field,key in re.findall(r'state\.(\w+)=(?:signedInteger|unsigned32)\(s\[@"([^"]+)"\]',fragment):lines.append(f'state.{field}={int(state[key])};')
 lines+=['state.quaternion='+literal(state['quaternion'])+';','return state;}']
 r=human['race'];c=r['path_cache'];lines+=['inline ssx::OriginalRaceParticipant browserEventParticipant(){ssx::OriginalRaceParticipant p;p.human=true;',f'p.finish={{{f(r["finish_elapsed"])},{r["penalty_ticks"]},{r["finish_ticks"]}}};',f'p.progress.pathIndex={r["path_index"]};p.progress.remaining={f(r["remaining"])};p.progress.bestRemaining={f(r["best_remaining"])};',f'p.progress.cache={{{literal(c["origin"])},{f(c["distance"])},{c["segment"]}}};','return p;}']
 return lines
def course_tail(code,event,lines,constants):
 def f(v):return 'std::bit_cast<float>(0x%08xu)'%struct.unpack('<I',struct.pack('<f',float(v)))[0]
 # Backcountry rival events (tools/export_backcountry.py, docs/backcountry.md): no countdown, the ready state is the start.
 constants+='inline constexpr bool rollingStart=%s;\n'%('true' if event.get('rolling_start') else 'false')
 # The Continue's 11D390 placement (web/start_gameplay.inc): 11DE60(rider, slot 0, runtime kind 1) -> 26B5E0 on the course's
 # path bank (AIP variant 0, SSB kind 14; exported kind = runtime kind - 1): position and direction in PS2 cm.
 if event.get('rolling_start'):
  row=rolling_region_row(code)
  constants+='inline constexpr std::array<float,6> rollingPlacement={%s};\n'%','.join(f(v) for v in list(row[2:5])+list(row[5:8]))
 audit=json.loads((activation_dir(code)/'countdown-instances.json').read_text())
 if audit['ee_sha256']!=event['provenance']['ee_sha256'] or audit['course_sha256']!=event['course_sha256']:raise ValueError('Event instance audit mismatch')
 import hashlib
 if hashlib.sha256((ROOT/f'local/assets/native/{code}/world_collision.json').read_bytes()).hexdigest()!=audit['world_package_sha256']:raise ValueError('Event collision package changed')
 dead=[r for r in audit['instances'] if r['node_type']==6]
 if len({r['resource'] for r in dead})!=len(dead) or (code=='ARA1' and len(dead)!=81):raise ValueError('Unexpected DeadNode population')
 for r in dead:
  if any(r[k]!='skip' for k in ['body_route','ray_mode0_route','ray_mode2_route']) or r['runtime_flags']&0x60:raise ValueError('DeadNode collision routing mismatch')
 rows=','.join('{'+','.join(str(r[k])+'u' for k in ['resource','authored_flags','runtime_flags'])+'}' for r in dead)
 header='inline constexpr std::array<BrowserEventDeadNode,%d> dead={{'%len(dead)+rows+'}};\n'
 # Type-16 node entities (vtable 0x491800): the course script leaves them with flags
 # 0x210005/0x210305 (neither 0x20 nor 0x40), so every body/ray collector skips them.
 # Identical in the countdown audit and the mid-race glide savestate; applied at load.
 course=[r for r in audit['instances'] if r['node_type']==16]
 if (code=='ARA1' and not course) or any(r['node_vtable']!=0x491800 or r['runtime_flags']&0x60 or any(r[k]!='skip' for k in ['body_route','ray_mode0_route','ray_mode2_route']) for r in course):raise ValueError('Type-16 node routing mismatch')
 # Collision-bearing instances authored without the static (0x200000) or entity (0x40000000) bits, e.g. ARA1
 # mdl_ARA1_endmode_collide_*: runtime flags 2 at the countdown (no 0x20/0x40), so every body/ray collector
 # (3340F4, 335BB0, 336D64) skips them; without this seed they read as 'dynamic entity callbacks unavailable'.
 package=json.loads((ROOT/f'local/assets/native/{code}/world_collision.json').read_text())
 bodies={(i['rid']<<8|i['track']) for i in package['instances'] if package['bindings'][str(i['track'])]['descriptors'][i['collision_descriptor']]['type']!=0}
 inert=[r for r in audit['instances'] if r['resource'] in bodies and r['node_type'] is None and not r['authored_flags']&0x40200000]
 if any(r['runtime_flags']&0x60 or any(r[k]!='skip' for k in ['body_route','ray_mode0_route','ray_mode2_route']) for r in inert):raise ValueError('Inert instance routing mismatch')
 course=course+inert
 crows=','.join('{'+','.join(str(r[k])+'u' for k in ['resource','authored_flags','runtime_flags'])+'}' for r in course)
 header+='inline constexpr std::array<BrowserEventDeadNode,%d> skip={{'%len(course)+crows+'}};\n'
 # Authored dynamic instances (tools/export_scripted_instances.py): countdown runtime flags and the
 # verified slot-2 contact class (0 none, 1 RollerModifier program 49).
 scripted=json.loads((activation_dir(code)/'scripted-instances.json').read_text())
 if scripted['world_package_sha256']!=audit['world_package_sha256'] or scripted['source_sha256']!=audit['source_sha256']:raise ValueError('Scripted instance export mismatch')
 kinds={'none':0,'roller':1}
 srows=','.join('{%du,%du,%du,%d}'%(r['resource'],r['authored_flags'],r['runtime_flags'],kinds[r['contact']]) for r in scripted['instances'])
 header+='inline constexpr std::array<BrowserEventScriptedInstance,%d> scripted={{'%len(scripted['instances'])+srows+'}};\n'
 # RollerModifier tree mass properties (sphere-tree model header +0x1C/+0x28/+0x4C bits) per collision resource/model.
 trows=[]
 for resource,models in sorted(scripted['roller_trees'].items(),key=lambda x:int(x[0])):
  for index,m in enumerate(models):trows.append('{%du,%du,{%s},{%s},{%s}}'%(int(resource),index,*(','.join('0x%08xu'%w for w in m[k]) for k in ('center_of_mass_bits','inertia_bits','inverse_inertia_bits'))))
 header+='inline constexpr std::array<BrowserRollerTreeMass,%d> rollers={{'%len(trows)+','.join(trows)+'}};\n'
 # Live DEFAULT_3 camera (0x390 bytes) and the outer compositor words (original_camera_words.hpp compositorOffsets)
 # at the countdown anchor (web/core.cpp camera_seed_words layout). The anchor is game tick 18:
 # the browser applies these words before the camera step that follows its 18th event tick (docs/CAMERA_RECOVERY.md).
 header+=camera_seed_line(location_state(code,'countdown'),event['provenance']['ee_sha256'])
 return lines,header+constants,audit['source_sha256']

def streamed_scripted(course_resources):
 """Authored dynamic instances (descriptor flag 0x40000000, a collision type) of the streamed peak worlds that no course event
 seeds: the stations' mdl_<X>_os609_full_version_depart models (A, B, C, D, E). Their free-ride state comes from the PS2
 free-ride audits (tools/export_peak_instances.py, local/event-activation/PEAK<n>/<LOC>/freeride-instances.json): no entity
 (node type none) and runtime flags 0x50210023 (bit 0x20: the static body / ray route). Their contact row (descriptor
 +0x08, the location's stage handler table) has no slot-2 program (-1: 30A060 runs nothing), the seed's contact class 0,
 so a crash flight, crash body query or reset placement meeting one collides with it statically, as on the PS2."""
 from world_assets import world_chunks,records,locations as sdb_locations
 locs=sdb_locations(ROOT/'local/assets/source/ps2/bam.sdb');track={l['name']:i for i,l in enumerate(locs)};chunks=None;rows=[]
 for audit_path in sorted((ROOT/'local/event-activation').glob('PEAK[123]/*/freeride-instances.json')):
  name,code=audit_path.parts[-3],audit_path.parts[-2]
  world_path=ROOT/f'local/assets/native/{name}/{code}/world_collision.json'
  if not world_path.exists():continue
  audit=json.loads(audit_path.read_text());world=json.loads(world_path.read_text());state={r['resource']:r for r in audit['instances']}
  for inst in world['instances']:
   d=world['bindings'][str(inst['track'])]['descriptors'][inst['collision_descriptor']];resource=inst['rid']<<8|inst['track']
   if not d['flags']&0x40000000 or d['type']==0 or resource in course_resources or any(r[0]==resource for r in rows):continue
   r=state.get(resource)
   if r is None:raise ValueError(f'{name}/{code} {inst["name"]}: dynamic instance missing from the free-ride audit')
   if r['node_type'] is not None or not r['runtime_flags']&0x20 or r['runtime_flags']&0xFFFF0000!=d['flags']&0xFFFF0000:raise ValueError(f'{inst["name"]}: unexpected free-ride state {r}')
   if d['resource08']&255!=track[code]:raise ValueError(f'{inst["name"]}: handler table of another location')
   if chunks is None:chunks=list(world_chunks(ROOT/'local/assets/source/ps2/bam.ssb'))
   T=track[code];stage=next(data for kind,t,_,data in records(chunks[locs[T]['chunk_end']]) if kind==16 and t==T)
   slot2=struct.unpack_from('<6I',stage,struct.unpack_from('<I',stage,0x1C)[0]+(d['resource08']>>8)*24)[2]
   if slot2!=0xFFFFFFFF:raise ValueError(f'{inst["name"]}: unported slot-2 contact program {slot2>>8}')
   rows.append((resource,d['flags'],r['runtime_flags'],0,f'{name}/{code} {inst["name"]}'))
 return rows
def generate(output):
 """Every registered course with countdown evidence gets a namespaced seed; browser_select_event_course
 (called by init_world_collision with world_collision.json 'location') points the historical names at it."""
 codes=[code for code in LOCATIONS if (ROOT/f'local/assets/native/{code}/event-start.json').exists() and (activation_dir(code)/'countdown-instances.json').exists() and (activation_dir(code)/'scripted-instances.json').exists()] # a location whose evidence is still being exported (tools/prepare_location.py writes scripted-instances.json last) waits for its next run
 if 'ARA1' not in codes:raise ValueError('Snow Jam countdown evidence missing')
 seeds={code:course_seed(code) for code in codes}
 if len({h for _,_,h in seeds.values()})!=1:raise ValueError('Courses from different world sources')
 header='#pragma once\n#include "../rider_local.hpp"\n#include <array>\n#include <cstdint>\n#include <span>\n#include <string_view>\nstruct BrowserEventDeadNode {uint32_t resource,authoredFlags,runtimeFlags;};\n'
 header+='struct BrowserEventScriptedInstance {uint32_t resource,authoredFlags,runtimeFlags;int contact;};\n'
 header+='struct BrowserRollerTreeMass {uint32_t resource,model;std::array<uint32_t,3> centerOfMass;std::array<uint32_t,9> inertia,inverseInertia;};\n'
 for code,(_,body,_) in seeds.items():header+=f'namespace browser_event_{code} {{\n'+body+'}\n'
 # Streamed peak worlds: the dynamic instances no course event seeds (web/world_bridge.cpp scripted_instance).
 course_resources={r['resource'] for code in seeds for r in json.loads((activation_dir(code)/'scripted-instances.json').read_text())['instances']}
 streamed=streamed_scripted(course_resources)
 header+='namespace browser_streamed {\n// '+'; '.join(r[4] for r in streamed)+'\ninline constexpr std::array<BrowserEventScriptedInstance,%d> scripted={{'%len(streamed)+','.join('{%du,%du,%du,%d}'%r[:4] for r in streamed)+'}};\n}\n'
 # bam.ssb hash: identical for every location, so the course is selected by its location code.
 header+='inline constexpr const char* browserEventWorldHash='+json.dumps(seeds['ARA1'][2])+';\n'
 header+='RIDER_LOCAL inline const char* browserEventLocation="ARA1";RIDER_LOCAL inline bool browserEventCourseSeeded=true;\n'
 header+='RIDER_LOCAL inline std::span<const BrowserEventDeadNode> browserEventDeadNodes=browser_event_ARA1::dead;\nRIDER_LOCAL inline std::span<const BrowserEventDeadNode> browserCourseSkipNodes=browser_event_ARA1::skip;\n'
 header+='RIDER_LOCAL inline const std::array<uint32_t,271>* browserEventCamera=&browser_event_ARA1::camera;RIDER_LOCAL inline uint32_t browserEventCameraAnchorTick=browser_event_ARA1::cameraAnchorTick;\n'
 header+='RIDER_LOCAL inline std::span<const BrowserEventScriptedInstance> browserEventScriptedInstances=browser_event_ARA1::scripted;\nRIDER_LOCAL inline std::span<const BrowserRollerTreeMass> browserRollerTreeMass=browser_event_ARA1::rollers;\n'
 header+='RIDER_LOCAL inline bool browserEventRolling=false;\n'
 header+='RIDER_LOCAL inline const std::array<float,6>* browserEventRollingPlacement=nullptr; // the Continue\'s 11D390 region row (PS2 cm position, direction)\n'
 header+='RIDER_LOCAL inline int browserEventStartDelay=browser_event_ARA1::startDelay;RIDER_LOCAL inline float browserEventProgressOrigin=browser_event_ARA1::progressOrigin;\n'
 header+='// Unknown/unseeded locations get empty instance tables and no event start (docs/locations.md).\ninline void browser_select_event_course(std::string_view location){\n'
 for code in seeds:header+=f' if(location=="{code}"){{browserEventLocation="{code}";browserEventCourseSeeded=true;browserEventDeadNodes=browser_event_{code}::dead;browserCourseSkipNodes=browser_event_{code}::skip;browserEventScriptedInstances=browser_event_{code}::scripted;browserRollerTreeMass=browser_event_{code}::rollers;browserEventStartDelay=browser_event_{code}::startDelay;browserEventProgressOrigin=browser_event_{code}::progressOrigin;browserEventCamera=&browser_event_{code}::camera;browserEventCameraAnchorTick=browser_event_{code}::cameraAnchorTick;browserEventRolling=browser_event_{code}::rollingStart;browserEventRollingPlacement={"&browser_event_"+code+"::rollingPlacement" if "rollingPlacement" in seeds[code][1] else "nullptr"};return;}}\n'
 header+=' browserEventRolling=false;browserEventRollingPlacement=nullptr;browserEventLocation="";browserEventCourseSeeded=false;browserEventDeadNodes={};browserCourseSkipNodes={};browserEventScriptedInstances={};browserRollerTreeMass={};browserEventStartDelay=0;browserEventProgressOrigin=0;browserEventCamera=nullptr;browserEventCameraAnchorTick=0;\n}\n'
 Path(output).with_name('event_instance_seed.hpp').write_text(header)
 start=['#pragma once','#include "ground_motion.hpp"','#include "race_session.hpp"','#include "event_instance_seed.hpp"','#include <bit>','#include <stdexcept>','#include <string_view>']
 for code,(lines,_,_) in seeds.items():start+=[f'namespace browser_start_{code} {{']+lines+['}']
 for kind,name in [('ssx::OriginalGroundProfile','browserEventGroundProfile'),('ssx::OriginalGroundState','browserEventGroundState'),('ssx::OriginalRaceParticipant','browserEventParticipant')]:
  start.append(f'inline {kind} {name}(){{const std::string_view l=browserEventLocation;')
  start+=[f' if(l=="{code}")return browser_start_{code}::{name}();' for code in seeds]
  start.append(' throw std::runtime_error("No event start seed for this course");}')
 Path(output).write_text('\n'.join(start)+'\n')
def camera_seed_line(state,ee_sha256):
 """The DEFAULT_3 camera block (0x390 bytes) and the outer compositor words (original_camera_words.hpp compositorOffsets) of a
 countdown anchor state (web/core.cpp camera_seed_words layout), as the camera / cameraAnchorTick constants."""
 import zipfile
 from ps2_capture import discover,GP
 memory=zipfile.ZipFile(state).read('eeMemory.bin')
 if hashlib.sha256(memory).hexdigest()!=ee_sha256:raise ValueError('Camera seed state differs from the event seed state')
 u=lambda a:struct.unpack_from('<I',memory,a&0x1FFFFFF)[0];found=discover(memory);cam,outer=found['camera'],found['outer']
 anchor=u(u(u(u(GP-0x848)+0x84)+0x0C)+8)
 offsets=[int(x,16) for x in re.search(r'compositorOffsets\{([^}]*)\}',(ROOT/'engine/original_camera_words.hpp').read_text()).group(1).split(',')]
 words=[u(cam+o) for o in range(0,0x390,4)]+[u(outer+o) for o in offsets]
 return 'inline constexpr std::array<uint32_t,%d> camera='%(len(words))+'{'+','.join('0x%08xu'%w for w in words)+'};inline constexpr uint32_t cameraAnchorTick=%du;\n'%anchor

def generate_exact(output,native_root=ROOT/'local/assets/native-exact'):
 """The console-arithmetic start seeds (SSX_PS2_EXACT_FPU builds with exactArithmetic on): the same ground profile, ground state and
 participant functions, from countdown exports of the exact-derived anchors (tools/export_exact_event_starts.py ->
 <native_root>/<code>/event-start.json). A course without an exact export has no exact seed: the core keeps the mode-1 one."""
 codes=[code for code in LOCATIONS if (Path(native_root)/code/'event-start.json').exists()]
 start=['#pragma once','// Generated by tools/generate_event_seed.py --exact from exact-derived countdown anchors (local/reference-exact).',
  '#include "ground_motion.hpp"','#include "race_session.hpp"','#include "event_instance_seed.hpp"','#include <bit>','#include <stdexcept>','#include <string_view>']
 for code in codes:
  event=json.loads((Path(native_root)/code/'event-start.json').read_text())
  # The camera seed of the same exact anchor (the mode-1 one is event_instance_seed.hpp's browser_event_<code>::camera).
  camera=camera_seed_line(ROOT/event['provenance']['snapshot'],event['provenance']['ee_sha256']).rstrip('\n')
  start+=[f'namespace browser_start_exact_{code} {{']+start_lines(event)+[camera,'}']
 if 'ARA1' in codes:start.append('#define SSX_EXACT_SNOW_JAM_SEED 1 // browser_start_exact_ARA1 (web/event_start_select.hpp)')
 start.append('inline bool browserEventExactSeeded(){const std::string_view l=browserEventLocation;return '+('||'.join(f'l=="{c}"' for c in codes) or 'false')+';}')
 start.append('#define SSX_EXACT_EVENT_CAMERA_SEEDS 1 // browserEventCameraExact (web/event_start_select.hpp)')
 start.append('inline const std::array<uint32_t,271>* browserEventCameraExact(){const std::string_view l=browserEventLocation;')
 start+=[f' if(l=="{code}")return &browser_start_exact_{code}::camera;' for code in codes]
 start.append(' return nullptr;}')
 start.append('inline uint32_t browserEventCameraAnchorTickExact(){const std::string_view l=browserEventLocation;')
 start+=[f' if(l=="{code}")return browser_start_exact_{code}::cameraAnchorTick;' for code in codes]
 start.append(' return 0;}')
 for kind,name in [('ssx::OriginalGroundProfile','browserEventGroundProfile'),('ssx::OriginalGroundState','browserEventGroundState'),('ssx::OriginalRaceParticipant','browserEventParticipant')]:
  start.append(f'inline {kind} {name}Exact(){{const std::string_view l=browserEventLocation;')
  start+=[f' if(l=="{code}")return browser_start_exact_{code}::{name}();' for code in codes]
  start.append(' throw std::runtime_error("No exact event start seed for this course");}')
 Path(output).write_text('\n'.join(start)+'\n')
 return codes
if __name__=='__main__':
 if '--exact' in sys.argv:print('exact seeds:',generate_exact(ROOT/'web/generated/event_start_seed_exact.hpp'))
 else:generate(ROOT/'web/generated/event_start_seed.hpp')
