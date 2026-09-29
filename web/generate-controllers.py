"""Reuse the native sequence implementation; strip only the Mac asset adapter."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'web/generated';out.mkdir(exist_ok=True)
s=(root/'engine/rider_animation_player.mm').read_text();s=s[s.index('float OriginalRiderAnimation::duration'):s.index('void OriginalRiderAnimation::consumeEvent')];s=s.replace('OriginalRiderAnimation::','BrowserAnimationGraph::')
(out/'animation_graph.cpp').write_text('#include "../animation_graph.hpp"\nnamespace ssx {\n'+s+'\n}\n')
# WASM has only nearest rounding. Keep this difference explicit rather than
# pretending fesetround(FE_TOWARDZERO) worked. Native implementation untouched.
s=(root/'engine/jump_motion.cpp').read_text();s=s.replace('if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("Jump rounding");','/* WebAssembly: nearest rounding; numerical parity tested separately. */')
(out/'jump_motion.cpp').write_text(s)
import json,re,struct
for file in ['ground_motion.cpp','air_motion.cpp']:
 s=(root/'engine'/file).read_text();s=s.replace('if(previous<0||std::fesetround(FE_TOWARDZERO))throw std::runtime_error("Ground rounding unavailable");','/* WASM nearest rounding; original operation order preserved. */');s=s.replace('if (previous<0||std::fesetround(mode)) throw std::runtime_error("Cannot set native floating-point rounding");','/* WASM nearest rounding; original operation order preserved. */');(out/file).write_text(s)
start=json.loads((root/'local/assets/native/ARA1/riding-start.json').read_text())['native']['initial']['original_ground'];source=(root/'engine/replay_io.mm').read_text()
def f(v):return 'std::bit_cast<float>(0x%08xu)'%struct.unpack('<I',struct.pack('<f',float(v)))[0]
def literal(v):
 if isinstance(v,bool):return str(v).lower()
 if isinstance(v,list):return '{'+','.join(literal(x) for x in v)+'}'
 if isinstance(v,dict):return '{'+','.join(f(v[k]) for k in ['current','rate','target'])+'}'
 return f(v)
profile=start['profile'];objects={'p':profile,'surface':profile['surface'],'h':profile['heading_profile']};lines=['#pragma once','#include "../rider_local.hpp" // per-rider course seeds (web/rider_local.hpp)','#include "ground_motion.hpp"','#include <bit>','inline ssx::OriginalGroundProfile browserGroundProfile(){ssx::OriginalGroundProfile profile;']
fragment=source[source.index('ssx::OriginalGroundProfile profile;'):source.index('auto state=readOriginalPoseState(s);')]
for field,kind,obj,key in re.findall(r'profile\.([\w.]+)=(value|curve)\((\w+),@"([^"]+)"\)',fragment):
 v=objects[obj][key];init=literal(v);init='{'+','.join('{'+','.join(f(y) for y in x)+'}' for x in v)+'}' if kind=='curve' else init
 if kind=='curve':init='{'+init+'}'
 lines.append(f'profile.{field}={init};')
lines+=['profile.surface.id='+str(profile['surface']['id'])+';','profile.speedLimitTable='+literal(profile['speed_limit_table'])+';','return profile;}','inline ssx::OriginalGroundState browserGroundState(){ssx::OriginalGroundState state;']
fragment=source[source.index('static ssx::OriginalGroundState readOriginalPoseState'):source.index('void initializeNativeReplay')];state=start['state'];seen=set()
for field,kind,key in re.findall(r'state\.(\w+)=(vec|value|boolean)\(s,@"([^"]+)"\)',fragment):lines.append(f'state.{field}={literal(state[key])};');seen.add(field)
for field,key in re.findall(r'state\.(\w+)=control\(@"([^"]+)"\)',fragment):lines.append(f'state.{field}={literal(state[key])};');seen.add(field)
for field,key in re.findall(r'state\.(\w+)=(?:signedInteger|unsigned32)\(s\[@"([^"]+)"\]',fragment):lines.append(f'state.{field}={int(state[key])};')
lines+=['state.quaternion='+literal(state['quaternion'])+';','return state;}'];(out/'physics_seed.hpp').write_text('\n'.join(lines))
for file in ['air_control.cpp','air_entry.cpp','air_animation_selector.cpp','orientation_motion.cpp','landing_motion.cpp','upper_reaction.cpp','grab_score.cpp','passive_air_control.cpp','race_event.cpp','boost_control.cpp','boost_award.cpp','trick_identity.cpp','trick_commit.cpp','air_trajectory.cpp','air_alignment.cpp','ground_pose_motion.cpp','soft_collision_control.cpp','stance_restore.cpp','reset_route.cpp','reset_placement.cpp','npc_path.cpp','environment_lighting.cpp','snow_particles.cpp','snow_context.cpp','snow_emission.cpp','wake_row.cpp','wake_input.cpp','wake_control.cpp','wake_render.cpp']:
 s=(root/'engine'/file).read_text();s=re.sub(r'if\(std::fesetround\(FE_TOWARDZERO\)\)throw std::runtime_error\("[^\"]+"\);','/* WASM nearest rounding; operation order retained. */',s);(out/file).write_text(s)
lines=['inline std::array<ssx::OriginalGroundProfile,19> browserGroundMaterials(){std::array<ssx::OriginalGroundProfile,19> table;']
for index,entry in enumerate(start['surface_catalog']):
 lines.append('{auto p=browserGroundProfile();')
 surf=entry['surface'];lines.append('p.surface.id='+str(surf['id'])+';')
 for field,key in [('gravity','gravity'),('lateralDrag','lateral_drag'),('powderDamping','powder_damping')]:lines.append(f'p.surface.{field}={f(surf[key])};')
 lines.append('p.surface.slipFriction={{'+','.join('{'+','.join(f(y) for y in x)+'}' for x in surf['slip_friction'])+'}};')
 for field,key in [('depthTarget1','depth_target1'),('depthTarget3','depth_target3'),('maxTurnAngle','max_turn_angle'),('airHeight','air_height'),('autoBoostSpeed','auto_boost_speed'),('autoBoostFactor','auto_boost_factor'),('alignmentRate','alignment_rate'),('surfaceTerminalVelocity','surface_terminal_velocity')]:lines.append(f'p.{field}={f(entry[key])};')
 for field,key in [('surface28','surface28'),('surface2C','surface2c'),('surface30','surface30'),('surface34','surface34')]:lines.append(f'p.headingProfile.{field}={f(entry["heading_profile"][key])};')
 lines.append(f'table[{index}]=p;}}')
lines+=['return table;}'];header=out/'physics_seed.hpp';header.write_text(header.read_text()+'\n'+'\n'.join(lines))

boost=json.loads((root/'local/assets/native/ARA1/riding-start.json').read_text())['native']['initial']['original_boost']
lines=['#include "boost_control.hpp"','inline ssx::OriginalBoostProfile browserBoostProfile(){return {'+','.join(f(boost['profile'][k]) for k in ['full_threshold','medium_threshold','drain_per_tick','tick_seconds','modifier_threshold','super_timer_floor','normal_decay','fast_decay'])+'};}', 'inline ssx::OriginalBoostState browserBoostState(){ssx::OriginalBoostState s;']
for field,key in [('meter','meter'),('amount','amount'),('window','window'),('modifier','modifier'),('superTime','super_time')]:lines.append(f's.{field}={f(boost["state"][key])};')
for field,key in [('tier','tier'),('drainEnabled','drain_enabled'),('feedbackFlags','feedback_flags')]:lines.append(f's.{field}={int(boost["state"][key])};')
lines+=['return s;}'];header.write_text(header.read_text()+'\n'+'\n'.join(lines))

landing=json.loads((root/'local/assets/native/ARA1/riding-start.json').read_text())['native']['initial']['original_landing']
lines=['#include "landing_motion.hpp"','inline ssx::OriginalLandingProfile browserLandingProfile(){ssx::OriginalLandingProfile p;']
for n,m in enumerate(landing['profile']['materials']):lines.append('p.materials[%d]={'%n+','.join(f(m[k]) for k in ['depth1','depth3','normal_impulse_factor','maximum_normal_speed'])+','+str(m['recovery'])+'};')
lines+=['p.bodyScale='+f(landing['profile']['body_scale'])+';','p.landingStat='+f(landing['profile']['landing_stat'])+';','return p;}', 'inline constexpr uint32_t browserLandingTick='+str(landing['runtime']['tick'])+'u;', 'inline constexpr uint32_t browserLastGroundLeave='+str(landing['runtime']['last_ground_leave_tick'])+'u;', 'inline constexpr uint32_t browserGroundFocusTick='+str(landing['runtime']['ground_focus_tick'])+'u;']
header.write_text(header.read_text()+'\n'+'\n'.join(lines))

header.write_text(header.read_text()+"\ninline constexpr std::array<int,19> browserSurfaceProperties44={"+",".join(str(int(x)) for x in start["surface_properties44"])+"};\n")

# Per-course glide seeds (docs/locations.md): every location with a riding-start.json (its grounded
# glide savestate, tools/locations.py) gets its ground profile/state, boost state and landing
# runtime ticks; browser_select_glide_course() (called by init_world_collision with the loaded
# location) points the historical names at it. Snow Jam's values are the ones above, unchanged.
def course_ground(code):
 initial=json.loads((root/f'local/assets/native/{code}/riding-start.json').read_text())['native']['initial'];g=initial['original_ground']
 replay=(root/'engine/replay_io.mm').read_text();prof=g['profile'];objs={'p':prof,'surface':prof['surface'],'h':prof['heading_profile']}
 ground=[f'inline ssx::OriginalGroundProfile profile(){{ssx::OriginalGroundProfile profile;']
 frag=replay[replay.index('ssx::OriginalGroundProfile profile;'):replay.index('auto state=readOriginalPoseState(s);')]
 for field,kind,obj,key in re.findall(r'profile\.([\w.]+)=(value|curve)\((\w+),@"([^"]+)"\)',frag):
  v=objs[obj][key];init=literal(v);init='{'+','.join('{'+','.join(f(y) for y in x)+'}' for x in v)+'}' if kind=='curve' else init
  if kind=='curve':init='{'+init+'}'
  ground.append(f'profile.{field}={init};')
 ground+=['profile.surface.id='+str(prof['surface']['id'])+';','profile.speedLimitTable='+literal(prof['speed_limit_table'])+';','return profile;}','inline ssx::OriginalGroundState state(){ssx::OriginalGroundState state;']
 frag=replay[replay.index('static ssx::OriginalGroundState readOriginalPoseState'):replay.index('void initializeNativeReplay')];st=g['state']
 for field,kind,key in re.findall(r'state\.(\w+)=(vec|value|boolean)\(s,@"([^"]+)"\)',frag):ground.append(f'state.{field}={literal(st[key])};')
 for field,key in re.findall(r'state\.(\w+)=control\(@"([^"]+)"\)',frag):ground.append(f'state.{field}={literal(st[key])};')
 for field,key in re.findall(r'state\.(\w+)=(?:signedInteger|unsigned32)\(s\[@"([^"]+)"\]',frag):ground.append(f'state.{field}={int(st[key])};')
 ground+=['state.quaternion='+literal(st['quaternion'])+';','return state;}']
 b=initial['original_boost'];boost=['inline ssx::OriginalBoostState boost(){ssx::OriginalBoostState s;']
 for field,key in [('meter','meter'),('amount','amount'),('window','window'),('modifier','modifier'),('superTime','super_time')]:boost.append(f's.{field}={f(b["state"][key])};')
 for field,key in [('tier','tier'),('drainEnabled','drain_enabled'),('feedbackFlags','feedback_flags')]:boost.append(f's.{field}={int(b["state"][key])};')
 boost.append('return s;}');r=initial['original_landing']['runtime']
 # Course-independent tables must agree with Snow Jam's (the rider/board and surface data are global).
 strip=lambda cat:[{k:v for k,v in x.items() if k!='source_address'} for x in cat] # heap placement of the catalog (differs by savestate)
 if strip(g['surface_catalog'])!=strip(start['surface_catalog']) or g['surface_properties44']!=start['surface_properties44'] or b['profile']!=boost_ara1['profile'] or initial['original_landing']['profile']!=landing['profile']:raise ValueError(f'{code}: glide seed tables differ from Snow Jam')
 return ground,boost,(r['tick'],r['last_ground_leave_tick'],r['ground_focus_tick'])
boost_ara1=boost
import sys;sys.path.insert(0,str(root/'tools'));from locations import LOCATIONS
courses=[c for c in LOCATIONS if c!='ARA1' and (root/f'local/assets/native/{c}/riding-start.json').exists()]
# The streamed Peak 1 world (world location 'PEAK1', docs/peak-mountain.md): the free-ride capture baseline (tools/export_peak_seed.py).
if (root/'local/assets/native/PEAK1/riding-start.json').exists():courses.append('PEAK1')
# The streamed Peak 3 world (docs/peak3.md): a CTM free-ride state at The Throne (tools/export_peak_seed.py --peak 3).
if (root/'local/assets/native/PEAK2/riding-start.json').exists():courses.append('PEAK2') # the streamed Peak 2 world (docs/peak2.md): Red station seed
if (root/'local/assets/native/PEAK3/riding-start.json').exists():courses.append('PEAK3')
# The whole mountain (world location 'MOUNTAIN', docs/peak3.md section 6): the All Peak runs' objectives-card state at The Throne
# (tools/export_peak_seed.py --world MOUNTAIN); MOUNTAIN<x> = the same world seeded for another run (MOUNTAIN2: the Peak 2 Race at Ruthless).
courses+=sorted(d.name for d in (root/'local/assets/native').glob('MOUNTAIN*') if (d/'riding-start.json').exists())
text=header.read_text()
for old,new in [('inline ssx::OriginalGroundProfile browserGroundProfile(){','inline ssx::OriginalGroundProfile browserGroundProfileARA1(){'),('inline ssx::OriginalGroundState browserGroundState(){','inline ssx::OriginalGroundState browserGroundStateARA1(){'),('inline ssx::OriginalBoostState browserBoostState(){','inline ssx::OriginalBoostState browserBoostStateARA1(){')]:
 if text.count(old)!=1:raise ValueError('Unexpected physics seed layout')
 text=text.replace(old,new)
seeds={c:course_ground(c) for c in courses}
ground_block=['#include <string_view>','RIDER_LOCAL inline const char* browserGlideLocation="ARA1";']
for c,(g,_,_) in seeds.items():ground_block+=[f'namespace browser_glide_{c} {{']+g+['}']
ground_block.append('inline ssx::OriginalGroundProfile browserGroundProfile(){const std::string_view l=browserGlideLocation;'+''.join(f'if(l=="{c}")return browser_glide_{c}::profile();' for c in seeds)+'return browserGroundProfileARA1();}')
ground_block.append('inline ssx::OriginalGroundState browserGroundState(){const std::string_view l=browserGlideLocation;'+''.join(f'if(l=="{c}")return browser_glide_{c}::state();' for c in seeds)+'return browserGroundStateARA1();}')
cut=text.index('inline std::array<ssx::OriginalGroundProfile,19> browserGroundMaterials()');text=text[:cut]+'\n'.join(ground_block)+'\n'+text[cut:]
boost_block=[]
for c,(_,bl,_) in seeds.items():boost_block+=[f'namespace browser_glide_{c} {{']+bl+['}']
boost_block.append('inline ssx::OriginalBoostState browserBoostState(){const std::string_view l=browserGlideLocation;'+''.join(f'if(l=="{c}")return browser_glide_{c}::boost();' for c in seeds)+'return browserBoostStateARA1();}')
cut=text.index('#include "landing_motion.hpp"');text=text[:cut]+'\n'.join(boost_block)+'\n'+text[cut:]
for name in ['browserLandingTick','browserLastGroundLeave','browserGroundFocusTick']:
 old=f'inline constexpr uint32_t {name}='
 if text.count(old)!=1:raise ValueError('Unexpected landing seed layout')
 text=text.replace(old,f'RIDER_LOCAL inline uint32_t {name}=')
ticks={'ARA1':(landing['runtime']['tick'],landing['runtime']['last_ground_leave_tick'],landing['runtime']['ground_focus_tick']),**{c:t for c,(_,_,t) in seeds.items()}}
text+='// Unknown locations keep the Snow Jam glide seed (a disc-only course rides from start.json).\ninline void browser_select_glide_course(std::string_view location){\n'
for c,(a,b_,g_) in ticks.items():text+=f' if(location=="{c}"){{browserGlideLocation="{c}";browserLandingTick={a}u;browserLastGroundLeave={b_}u;browserGroundFocusTick={g_}u;return;}}\n'
a,b_,g_=ticks['ARA1'];text+=f' browserGlideLocation="ARA1";browserLandingTick={a}u;browserLastGroundLeave={b_}u;browserGroundFocusTick={g_}u;\n}}\n'
header.write_text(text)

source=(root/'engine/rider_animation_player.mm').read_text()
fragment=source[source.index('std::optional<AnimationTransform> OriginalRiderAnimation::previewRoot'):source.rfind('\n}')]
fragment=fragment.replace('OriginalRiderAnimation::','BrowserAnimationGraph::').replace('Round round;','OriginalRounding round;')
graph=out/'animation_graph.cpp';graph.write_text(graph.read_text()+'\nnamespace ssx {\n'+fragment+'\n}\n')

# Graph host calculations can run under an original controller callback scope.
# Preserve that policy instead of inheriting WASM's hardware-nearest operators.
text=graph.read_text()
for before,after in {
 'def.blendSeconds*originalScalarSubtract(1.f,next.weight)':'[&]{OriginalRounding chop;return terrain_original::mul(def.blendSeconds,originalScalarSubtract(1.f,next.weight));}()', # native enter runs under playSemantic's Round (311F00 mul.s chop); browser callers often have no scope (air-steer-passive 712 revived landing 61)
 'state.timeScale*std::bit_cast<float>(0x3c888889u)':'terrain_original::mul(state.timeScale,std::bit_cast<float>(0x3c888889u))',
 'root.position[k]*scale[k]':'terrain_original::mul(root.position[k],scale[k])',
}.items():
 assert text.count(before)==1,('Graph arithmetic source changed',before)
 text=text.replace(before,after)
graph.write_text('#include "../../engine/terrain_contact_math.hpp"\n'+text)

import sys
sys.path.insert(0,str(root/"tools"))
from generate_event_seed import generate
generate(out/"event_start_seed.hpp")
# Computer-rider seeds are read at run time (npc-riders.json) with the same ground field mapping.
from generate_npc_seed import generate as generate_npc_seed
generate_npc_seed(out/"npc_seed_reader.hpp")
# NPC provider leaves: the native Round guard sets hardware toward-zero rounding. WASM has only
# nearest, so select the emulated policy (original_rounding.hpp): the scalar helpers then use
# software toward-zero; plain operators in these files stay nearest (docs/ai-racers.md).
for file,guard in [('npc_input.cpp','struct Round{int previous=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("NPC input rounding");}~Round(){std::fesetround(previous);}};'),
                   ('npc_air.cpp','struct Round {int old=std::fegetround();Round(){if(std::fesetround(FE_TOWARDZERO))throw std::runtime_error("NPC airborne rounding");}~Round(){std::fesetround(old);}};')]:
 s=(root/'engine'/file).read_text()
 if s.count(guard)!=1:raise ValueError('NPC rounding guard changed: '+file)
 (out/file).write_text('#include "../../engine/original_rounding.hpp"\n'+s.replace(guard,'using Round=OriginalRounding;'))
