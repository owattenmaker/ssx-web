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
let drawn=0,airClosed=0,maxVertices=0,lightingGaps=0;
for(let tick=0;tick<600;tick++){
 const frame=step(+(tick>90&&tick<150)),info=f(c._trail_info(),6);
 const lighting=f(c._environment_info(),7);assert.equal(lighting[0],1);assert(lighting.every(Number.isFinite));lightingGaps+=lighting[1];
 assert(info.every(Number.isFinite));assert(info[0]<=2000&&info[1]<=2000);assert.equal(info[0],info[1]*5,'six-band ribbon/roof topology differs');
 if(!frame.state[8]&&!f(c._crash_info(),12)[0]){assert.equal(info[5],0,'ordinary air motion continues making snow tracks');airClosed++;}
 if(info[0]){
  drawn++;maxVertices=Math.max(maxVertices,info[0]);const data=f(c._trail_ribbon(),info[0]*9);assert(data.every(Number.isFinite));
  for(let i=0;i<info[0];i++){assert(data[i*9+8]>=0&&data[i*9+8]<=2);const position=[data[i*9]/100,data[i*9+2]/100,-data[i*9+1]/100];assert(Math.hypot(...position.map((x,k)=>x-frame.state[k]))<250,'trail has an uninitialized or disconnected origin');}
 }
}
assert(drawn>100&&airClosed>0);c._reset_animation();assert.equal(f(c._trail_info(),6)[0],0,'restart retains old trail geometry');
console.log('Original six-band tracks generate bounded geometry, cap on takeoff and clear on reset:',{drawn,airClosed,maxVertices,lightingGaps,lighting:Array.from(f(c._environment_info(),7))});
