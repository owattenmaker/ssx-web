import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),root='public/assets/';
const read=p=>JSON.parse(fs.readFileSync(root+p));
const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;};
const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const metadata=str(read('ANIMATIONS/animation-packets.json')),rig=str(read('RIDER_SAM/rider.json')),settings=str(read('ANIMATIONS/initial.json'));
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
let onRail=0,cycles=0,first=-1,left=-1,landedAfter=false,displayedPoints=0,peakPending=0;const positions=[];
for(let tick=0;tick<900;tick++){
 c._race_begin();const r=f(c._step_rider(0,0,0,0),16);const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);const pose=f(pp,26*7);assert(pose.every(Number.isFinite));c._race_end();
 const railState=f(c._rail_gameplay_info(),8),anim=f(c._animation_info(),19),state=f(c._rider_state(),16);
 displayedPoints+=anim[13];peakPending=Math.max(peakPending,anim[12]);assert.equal(f(c._rail_score_info(),8)[7],0,'score award was not delivered to animation output');
 if(railState[0]){onRail++;if(first<0)first=tick;if([18,19,20].includes(anim[0]))cycles++;if(left<0)positions.push(state.slice(0,3));assert.equal(anim[15],7,'rail motion must retain control7');}
 if(first>=0&&left<0&&!railState[0])left=tick;if(left>=0&&state[8]&&anim[15]===0)landedAfter=true;
 
}
const exitContacts=f(c._rail_exit_info(),4);console.log('Rail exit contacts',Array.from(exitContacts));assert(exitContacts[0]>0,'rail leave skipped exit queries');assert.equal(exitContacts[2],0,'rail exit world query incomplete');
const travel=Math.hypot(...positions.at(-1).map((v,i)=>v-positions[0][i]));console.log({first,left,onRail,cycles,landedAfter,firstGrindTravelMetres:travel});assert(first>=0&&first<30,'authored rail never attaches promptly');assert(onRail>=60,'grind ends too early');assert(cycles>onRail*.8,'original rail cycle missing');assert(left>first&&landedAfter,'rail exit does not return to riding');
assert(Math.hypot(...positions.at(-1).map((v,i)=>v-positions[0][i]))>10,'rail motion did not travel');
const paid=f(c._rail_score_info(),8);assert(peakPending>=1000);assert(paid[3]>=1000);assert.equal(displayedPoints,paid[3],'rail score must be delivered exactly once');assert(paid[6]>0);console.log({peakPending,displayedPoints,railMeterAward:paid[6]});
const earnedMeter=f(c._boost_info(),8)[0];assert(earnedMeter>0,'banked rail reward did not reach boost meter');c._race_begin();const boostMotion=f(c._step_rider(0,0,0,1),16);c._animation_tick(boostMotion[7],0,0,boostMotion[9],boostMotion[8],0,0,1,1,0,boostMotion[15],0);c._race_end();const spent=f(c._boost_info(),8);assert(spent[1]>0&&spent[0]<earnedMeter,'earned rail meter cannot power boost');


// Space requests control2, charges on subsequent held frames, then jumps once.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
let maxCharge=0,heldFrames=0,launchFrame=-1,jumps=0;
for(let tick=0;tick<220;tick++){
 const held=+(tick>=50&&tick<90);c._rail_preinput(0);c._race_begin();const r=f(c._step_rider(0,held,0,0),16);const pp=c._animation_tick(r[7],0,0,r[9],r[8],held,0,0,0,0,r[15],0);assert(f(pp,26*7).every(Number.isFinite));c._race_end();
 const rail=f(c._rail_gameplay_info(),8),jump=f(c._rail_jump_info(),3),anim=f(c._animation_info(),19),motion=f(c._rider_state(),16);
 if(tick===50)assert.equal(motion[9],0,'first rail jump press must not charge immediately');
 if(held&&rail[0]){heldFrames++;assert.equal(anim[15],2,'rail crouch must own control2');maxCharge=Math.max(maxCharge,motion[9]);}
 if(jump[2]>jumps){jumps=jump[2];launchFrame=tick;assert.equal(rail[0],0);assert.equal(motion[8],0);assert.equal(anim[15],5,'release must enter explicit airborne control5');}
}
console.log({heldFrames,maxCharge,launchFrame,jumps});assert(heldFrames>=30);assert(maxCharge>.8);assert.equal(launchFrame,90);assert.equal(jumps,1);

// Leaving a rail while still holding Space hands control2 to ordinary air;
// release after the edge must use ordinary air/ground control, not rail takeoff.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
let releaseAt=Infinity,sawGrind=false,edge=-1;
for(let tick=0;tick<900;tick++){
 const held=+(tick>=50&&tick<releaseAt);c._rail_preinput(0);c._race_begin();const r=f(c._step_rider(0,held,0,0),16);const pp=c._animation_tick(r[7],0,0,r[9],r[8],held,0,0,0,0,r[15],0);assert(f(pp,26*7).every(Number.isFinite));c._race_end();const rail=f(c._rail_gameplay_info(),8);
 if(rail[0])sawGrind=true;if(sawGrind&&!rail[0]&&edge<0){edge=tick;releaseAt=tick+1;assert.equal(rail[1],0,'held edge must hand ownership to ordinary control2');}
 assert.equal(f(c._rail_jump_info(),3)[2],0,'airborne rail release added another impulse');
}
assert(edge>=50&&edge<850);console.log({heldRailEdge:edge,releaseAt});

// Independent rotation input uses source transitions and marker gates.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
const railRotationClips=new Set();let rotations=0;
for(let tick=0;tick<240;tick++){
 const rotate=tick>=20&&tick<23?-1:tick>=70&&tick<73?1:0;c._rail_rotation_input(rotate);c._rail_preinput(0);c._race_begin();const r=f(c._step_rider(0,0,0,0),16);const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);assert(f(pp,26*7).every(Number.isFinite));c._race_end();
 const anim=f(c._animation_info(),19),rotation=f(c._rail_rotation_info(),4);railRotationClips.add(anim[0]);rotations=rotation[0];
}
c._rail_rotation_input(0);console.log({rotations,railRotationClips:[...railRotationClips]});assert.equal(rotations,2,'short rotation pulses must trigger exactly one rotation each');assert(railRotationClips.has(49)&&railRotationClips.has(51),'original rail rotation clips are missing');assert(railRotationClips.has(19),'frontside balance cycle is missing');

c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
let activeAirExit=false;
for(let tick=0;tick<900;tick++){
 c._rail_rotation_input(tick>=20?1:0);c._rail_preinput(0);c._race_begin();const r=f(c._step_rider(0,0,0,0),16);const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);assert(f(pp,26*7).every(Number.isFinite));c._race_end();
 const rail=f(c._rail_gameplay_info(),8),transition=f(c._rail_jump_info(),3),anim=f(c._animation_info(),19);// Only the fixture's first exit is airborne: 13C5A0 restarts the attachment tolerance ramp, so the rider can
 // later re-attach from the snow and slide off the rail end onto the ground (control0), which is also original behaviour.
 if(rail[3]===1&&transition[1]&&!rail[1]){assert.equal(anim[15],5,'held rotation exit must select active air control5');activeAirExit=true;}
}
c._rail_rotation_input(0);assert(activeAirExit,'held rotation fixture never exercised active air exit');console.log('Held rotation exits into control5.');

// A scenery crash while grinding enters motion2 from motion4, not ground0.
// Use the public original collision-entry boundary after a real authored attach.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));
c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
let railCrash=false,railFramesAtCrash=0;
for(let tick=0;tick<80;tick++){
 c._race_begin();const r=f(c._step_rider(0,0,0,0),16);
 const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);
 assert(f(pp,26*7).every(Number.isFinite));c._race_end();
 const rail=f(c._rail_gameplay_info(),8);
 if(!railCrash&&rail[0]&&tick>=20){
  railFramesAtCrash=rail[4];c._hard_crash_begin(350,900);railCrash=true;
  const crash=f(c._crash_info(),8),after=f(c._rail_gameplay_info(),8);
  assert.equal(crash[0],1);assert.equal(crash[2],1,'rail crash incorrectly enters ground sliding');
  assert.equal(after[0],0,'crash retains rail motion');assert.equal(after[1],0,'crash retains rail control ownership');
 }
 if(railCrash){assert.equal(f(c._rail_gameplay_info(),8)[4],railFramesAtCrash,'rail motion advances during crash');}
}
assert(railCrash,'fixture did not reach a rail crash');
console.log('Authored grind transfers to airborne crash motion and releases rail ownership.');

// Original control3 can play a light impact while retaining rail motion4.
for(const semantic of [55,56,57,58,59,60]){
 c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));
 c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
 let injected=false,softTicks=0,recovered=false;
 for(let tick=0;tick<160;tick++){
  c._race_begin();const r=f(c._step_rider(0,0,0,0),16);
  const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);assert(f(pp,26*7).every(Number.isFinite));c._race_end();
  const rail=f(c._rail_gameplay_info(),8),anim=f(c._animation_info(),19);
  if(!injected&&rail[0]&&tick>=20){c._soft_collision_begin(semantic,0);injected=true;}
  else if(injected&&anim[15]===3){softTicks++;assert.equal(rail[0],1,'light hit lost the rail');assert.equal(anim[0],semantic,'impact clip was overwritten');}
  else if(injected&&softTicks&&anim[15]===7){recovered=true;assert.equal(rail[0],1);break;}
 }
 assert(injected&&softTicks>0&&recovered,'rail light impact failed to return to grinding');
 console.log('Rail light collision recovers:',{semantic,softTicks});
}

// A late light impact can leave the rail and touch down in the same posed
// frame. The departed rail must not keep ownership through that landing.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));
c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
let lateHit=false,sawLateImpact=false,leftDuringImpact=false,lateRecovered=false;
for(let tick=0;tick<320;tick++){
 c._race_begin();const r=f(c._step_rider(0,0,0,0),16);
 const pp=c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);assert(f(pp,26*7).every(Number.isFinite));c._race_end();
 const rail=f(c._rail_gameplay_info(),8),anim=f(c._animation_info(),19),motion=f(c._rider_state(),16);
 if(!lateHit&&tick===175){assert.equal(rail[0],1);c._soft_collision_begin(58,0);lateHit=true;}
 if(lateHit&&rail[0]&&anim[15]===3)sawLateImpact=true;
 if(sawLateImpact&&!rail[0]){leftDuringImpact=true;assert.equal(rail[1],0,'departed rail retains light-collision ownership');}
 if(leftDuringImpact&&motion[8]&&anim[15]===0)lateRecovered=true;
}
assert(lateHit&&sawLateImpact&&leftDuringImpact&&lateRecovered,'late rail impact did not hand off to landing');
console.log('Late rail light collision releases ownership and returns to riding after landing.');


// Synthetic solid geometry, real masked rail-exit query and source response.
c._reset_animation();c._reset_race();c._reset_rider(...spawn,Math.atan2(direction[0],direction[2]));c._set_rider_velocity(direction[0]*900,-direction[2]*900,0);
for(let tick=0;tick<90;tick++){
 const held=+(tick>=50);c._race_begin();const r=f(c._step_rider(0,held,0,0),16);
 c._animation_tick(r[7],0,0,r[9],r[8],held,0,0,0,0,r[15],0);c._race_end();
}
assert.equal(f(c._rail_gameplay_info(),8)[0],1);
const body=f(c._body_volume_info(),108),center=Array.from(body.slice(10,13)),radius=body[9];
const velocity=f(c._source_motion_audit(),20).slice(3,6),axis=Math.abs(velocity[0])>Math.abs(velocity[1])?0:1,sign=Math.sign(velocity[axis]);
const low=center.map(x=>x-200),high=center.map(x=>x+200);
low[axis]=center[axis]+(sign>0?radius-5:-radius-5);high[axis]=low[axis]+10;
const identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],fixtureHash='rail-exit-solid-fixture';
const box={version:1,source_sha256:fixtureHash,bindings:{8:{descriptors:[{type:2,flags:0x200000,nodes:[{surface_id:0,flags:0,value:0}]}]}},instances:[{track:8,rid:1,collision_descriptor:0,scale:1,bounds_min_cm:low,bounds_max_cm:high,model_resource:264,matrix:identity}],render_model_nodes:{'8:1':{nodes:[{parent:0xffffffff,matrix:identity,draw_bounds_cm:[...low,...high],draw_flags:1}]}},collision_meshes:{}};
c._init_world_collision(str(box),put(new TextEncoder().encode(fixtureHash+'\0')));
c._init_body_terrain(str({...read('ARA1/terrain.json'),source_sha256:fixtureHash}));
// 0x13BFA8 is motion 4's post stage: its contact passes run every rail tick, so count only the release tick's pass.
const exitPassesBefore=f(c._rail_exit_info(),4)[0];
c._race_begin();const exit=f(c._step_rider(0,0,0,0),16),contact=f(c._rail_exit_info(),4),reaction=f(c._collision_reaction_info(),10);
assert.equal(contact[0]-exitPassesBefore,1);assert(contact[1]>0,'masked rail-exit query missed solid box');assert.equal(contact[2],0);assert(contact[3]>0,'rail contact response never notified collision controller');
assert.equal(reaction[1],3,'rail obstacle failed to select the source light impact');
c._animation_tick(exit[7],0,0,exit[9],exit[8],0,0,0,0,0,exit[15],0);c._race_end();
assert.equal(f(c._animation_info(),19)[15],3,'jump release overwrote contact-selected soft control');
assert.equal(f(c._rail_gameplay_info(),8)[1],0);
console.log('Real box query on rail exit:',{contact:Array.from(contact),semantic:reaction[2]});
