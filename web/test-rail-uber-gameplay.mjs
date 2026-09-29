import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const riderPackage=process.argv.includes('--zoe')?'RIDER_ZOE':'RIDER_SAM';
const c=await createCore(),root='public/assets/';
const read=p=>JSON.parse(fs.readFileSync(root+p));
const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;};
const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const boneCount=read(riderPackage+'/rider.json').bones.length;
const metadata=str(read('ANIMATIONS/animation-packets.json')),rig=str(read(riderPackage+'/rider.json')),settings=str(read('ANIMATIONS/initial.json'));
const raw=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin'),packets=put(raw);
c._init_animation(metadata,rig,settings,packets,raw.length);for(const p of [metadata,rig,settings,packets])c._free(p);
const mesh=fs.readFileSync(root+'ARA1/collision.bin'),meshPtr=put(mesh);c._init_world(meshPtr,mesh.length/4);c._free(meshPtr);
const terrain=str(read('ARA1/terrain.json'));c._init_terrain(terrain);c._free(terrain);
const world=read('ARA1/world_collision.json'),source=read('ARA1/terrain.json').source_sha256;
const wp=str(world),hp=put(new TextEncoder().encode(source+'\0'));c._init_world_collision(wp,hp);c._free(wp);c._free(hp);
c._animation_use_physics(1);const start=read('ARA1/start.json');
const bt=str(read('ARA1/terrain.json'));c._init_body_terrain(bt);c._free(bt);
const cfg=read('ANIMATIONS/initial.json'),raceConfig=str(cfg);c._init_race(raceConfig);c._free(raceConfig);
const f=(ptr,n)=>new Float32Array(c.HEAPF32.buffer,ptr,n).slice();
let score=0;
function step(jump=0,grab=0){
 c._race_begin();const motion=f(c._step_rider(0,jump,0,0),16);
 c._animation_tick(motion[7],0,0,motion[9],motion[8],jump,grab,0,0,4,motion[15],0);
 const state=f(c._rider_state(),16),animation=f(c._animation_info(),19),reset=f(c._reset_info(),9),race=f(c._race_end(),8);score+=animation[13];
 const pose=f(c._pose_physical(),12);const camera=f(c._step_camera_head(...pose.slice(9,12)),9);assert(camera.every(Number.isFinite));
 return {state,animation,reset,race};
}
const env=read('ARA1/environment.json'),envRaw=fs.readFileSync(root+'ARA1/environment.bin'),envText=str(env),envPointer=put(envRaw);c._init_environment(envText,envPointer,envRaw.length);c._free(envText);c._free(envPointer);
c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();

const catalog=read('ARA1/rails.json'),railText=str(catalog),railHash=put(new TextEncoder().encode(source+'\0'));c._init_rails(railText,railHash);c._free(railText);c._free(railHash);
const rail=catalog.rails.find(r=>r.name==='spline_ARA1_RAIL_3007'),segment=rail.segments[0].native;
const delta=segment.end.map((x,i)=>x-segment.start[i]),length=Math.hypot(delta[0],delta[2]),direction=delta.map(x=>x/length);
const spawn=segment.start.map((x,i)=>x-direction[i]*3.5);spawn[1]=-4132.04+.05;
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900, -direction[2]*900,0);

for(const mask of [1,2,4,8]){
 c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
 const semantics=new Set(),names=new Set();let cycled=0,released=false,seen=false,last;
 for(let tick=0;tick<360;tick++){
  if(tick===20){assert.equal(f(c._rail_gameplay_info(),8)[0],1);c._award_trick_meter(1);assert(f(c._boost_info(),8)[5]>0);}
  const shoulder=tick>=30&&!released?mask:0;c._rail_shoulder_input(shoulder);c._race_begin();const r=f(c._step_rider(0,0,0,0),16);
  const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,shoulder,0,0,0,r[15],0);assert(f(pp,boneCount*7).every(Number.isFinite));c._race_end();
  {const p=c._trick_name();const name=new TextDecoder().decode(c.HEAPU8.subarray(p,c.HEAPU8.indexOf(0,p)));if(name)names.add(name);}
  last=f(c._rail_uber_info(),6);const info=f(c._animation_info(),19);if(info[15]===12){seen=true;semantics.add(info[0]);}
  if(last[1]===1&&++cycled>=10)released=true;
 }
 console.log('Uber grind gameplay', {mask,semantics:[...semantics],names:[...names],state:[...last],boost:[...f(c._boost_info(),8)]});
 assert(seen&&released,'Uber grind never reached its original cycle');assert.equal(last[2],1);assert.equal(last[3],1,'Uber grind never committed');assert.notEqual(last[5],12,'Uber controller failed to exit');assert.equal(f(c._boost_info(),8)[3],2,'completed Uber must increment earned tier exactly once');
}

// Without Tricky the held identity notifies once, and a shoulder chord is not
// a valid original UberGrind action even when the meter subsequently fills.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
for(let tick=0;tick<90;tick++){
 if(tick===60)c._award_trick_meter(1);
 c._rail_shoulder_input(tick>=60?3:tick>=20?1:0);c._race_begin();const r=f(c._step_rider(0,0,0,0),16);
 c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);c._race_end();
 assert.equal(f(c._rail_uber_info(),6)[2],0,'locked or chorded input entered Uber grind');
}
assert.equal(f(c._rail_uber_info(),6)[4],1,'held unavailable identity repeatedly notified');
console.log('Original Tricky gate and exclusive shoulder mapping pass.');

// Keep the trick held beyond the rail, exercising motion1/0 under control12.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
let edgeRelease=false,offRailUber=0,edgeCompleted=false,edgeCrash=false;
for(let tick=0;tick<600;tick++){
 if(tick===20)c._award_trick_meter(1);
 const shoulder=tick>=30&&!edgeRelease?1:0;c._rail_shoulder_input(shoulder);c._race_begin();const r=f(c._step_rider(0,0,0,0),16);
 const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,shoulder,0,0,0,r[15],0);assert(f(pp,boneCount*7).every(Number.isFinite));c._race_end();
 const uber=f(c._rail_uber_info(),6),rail=f(c._rail_gameplay_info(),8);
 if(uber[5]===12&&!rail[0])offRailUber++;
 if(f(c._crash_info(),12)[0]){edgeCrash=true;edgeRelease=true;assert.equal(rail[1],0,'crash retains airborne Uber ownership');}
 if(uber[1]===2)edgeRelease=true;
 edgeCompleted||=uber[3]>0;
}
console.log('Held Uber past rail edge',{offRailUber,edgeCompleted,edgeCrash});
assert(offRailUber>0&&(edgeCompleted||edgeCrash),'held Uber did not continue beyond the rail into a resolved outcome');
if(edgeCrash)assert.equal(f(c._rail_uber_info(),6)[3],0,'interrupted Uber incorrectly counted as completed');
assert.notEqual(f(c._rail_uber_info(),6)[5],12);
