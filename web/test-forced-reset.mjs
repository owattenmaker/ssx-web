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
c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
let frame;
// Bank the verified early jump before testing reset ownership and score retention.
for(let tick=0;tick<180;tick++)frame=step(+(tick>=1&&tick<30),+(tick>=35&&tick<55));
assert(score>0,'reset fixture did not bank a clean trick');
const before=frame,banked=score,rows=[];let placedPosition;c._request_rider_reset(2);let placed=0,completed=0,peakFade=0;
for(let tick=1;tick<=60;tick++){
 const previousMeter=f(c._boost_info(),8)[0];frame=step();rows.push({tick,reset:Array.from(frame.reset),position:Array.from(frame.state.slice(0,3)),speed:frame.state[7],control:frame.animation[15]});
 assert(frame.reset[8]>=0&&frame.reset[8]<=1);peakFade=Math.max(peakFade,frame.reset[8]);
 assert.equal(frame.state[13],before.state[13],'forced reset restarted the demo');assert.equal(score,banked,'forced reset discarded banked score');
 assert(Math.abs(frame.race[0]-before.race[0]-tick/60)<.0001,'race time restarted or stopped during reset');
 if(frame.reset[0]){assert.equal(frame.animation[15],9);assert.equal(f(c._camera_inputs(),13)[12],3);}
 if(!placed&&frame.reset[2]){placed=tick;placedPosition=Array.from(frame.state.slice(0,3));assert(Math.abs(f(c._boost_info(),8)[0]-Math.max(0,previousMeter*.9-cfg.original_boost.profile.tick_seconds*cfg.original_boost.profile.normal_decay))<.000001,'forced reset boost penalty differs from119368');assert(Math.hypot(...frame.state.slice(0,3).map((v,i)=>v-start.position[i]))>30,'reset teleported back to the demo start');}
 if(tick<21)assert.deepEqual(frame.state.slice(0,3),before.state.slice(0,3),'reset motion advanced before placement');
 if(tick>21&&tick<41)assert.deepEqual(Array.from(frame.state.slice(0,3)),placedPosition,'motion3 integrates while reset controller owns the frame');
 if(frame.reset[3]){completed=tick;assert.equal(frame.animation[15],4);break;}
}
console.log({placed,completed,banked,beforePosition:Array.from(before.state.slice(0,3)),afterPosition:Array.from(frame.state.slice(0,3))});
fs.writeFileSync('../local/browser-validation/forced-reset.json',JSON.stringify(rows,null,2));
assert(peakFade>.99,'original reset fade never covers the placement transition');assert.equal(frame.reset[8],0,'fade survives reset completion');
assert.equal(placed,21,'reset placement timing differs from original');assert.equal(completed,41,'reset handoff timing differs from original');
let landed=false;for(let tick=0;tick<180;tick++){frame=step();assert.equal(frame.state[13],0);if(frame.state[11]){landed=true;break;}}
assert(landed,'reset rider never lands back on the course');
console.log('Course-preserving forced reset keeps banked score/time and returns through passive-air landing.');

const priorPlacements=frame.reset[2],priorCompletions=frame.reset[3];c._request_rider_reset(0);let manualPlaced=0,manualComplete=0;
for(let tick=1;tick<=60;tick++){
 const previousMeter=f(c._boost_info(),8)[0];frame=step();
 if(!manualPlaced&&frame.reset[2]>priorPlacements){manualPlaced=tick;assert(previousMeter>0,'manual reset penalty fixture has no meter');assert(Math.abs(f(c._boost_info(),8)[0]-Math.max(0,previousMeter*.3-cfg.original_boost.profile.tick_seconds*cfg.original_boost.profile.normal_decay))<.000001,'manual reset penalty differs from119368');}
 if(frame.reset[3]>priorCompletions){manualComplete=tick;break;}
}
assert.equal(manualPlaced,21);assert.equal(manualComplete,41);assert.equal(score,banked);assert.equal(frame.state[13],0);
console.log('Manual reason0 reset applies its original70-percent meter penalty and preserves the race.');
