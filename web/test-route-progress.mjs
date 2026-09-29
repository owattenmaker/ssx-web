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
c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
const initial=f(c._route_info(),14),indices=new Set();let moved=0,headingChanges=0,previous=initial;
for(let tick=0;tick<600;tick++){
 const frame=step(),route=f(c._route_info(),14);
 assert(route.every(Number.isFinite));assert.equal(route[5],tick+1,'route progress must update once after each race tick');
 assert(route[0]>=0&&route[0]<cfg.original_reset.paths.length);indices.add(route[0]);
 moved+=Math.abs(route[2]-previous[2])>.01;headingChanges+=Math.abs(route[4]-previous[4])>.001;
 const dx=route[11]-route[8],dy=route[12]-route[9];
 if(Math.hypot(dx,dy)>1){const expected=Math.atan2(dy,dx);assert(Math.abs(Math.atan2(Math.sin(route[4]-expected),Math.cos(route[4]-expected)))<.001,'route heading differs from its authored lookahead');}
 assert.equal(frame.state[13],0,'route progress fixture needed an emergency respawn');previous=route;
}
assert(moved>500,'retained reset-route distance stayed at its capture value');assert(indices.size>1,'course run did not exercise a route switch');assert(headingChanges>0);
const updates=previous[5];c._request_rider_reset(2);for(let tick=0;tick<41;tick++){step();assert.equal(f(c._route_info(),14)[5],updates+tick+1,'reset stopped the original shared-frame route stage');}
c._reset_rider(...start.position,start.heading);c._reset_animation();const restored=f(c._route_info(),14);assert.equal(restored[5],0);assert.equal(restored[0],initial[0]);assert.equal(restored[2],initial[2]);assert.equal(restored[4],initial[4]);
console.log('Human route progress follows authored paths across junctions and reset frames:',{moved,headingChanges,pathIndices:[...indices]});
