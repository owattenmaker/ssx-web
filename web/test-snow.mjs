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
const seen=new Set(),peaks=Array(10).fill(0);let impactFrames=0;
for(let tick=0;tick<600;tick++){
 c._race_begin();const steering=tick<80?.15:0,jump=+(tick>110&&tick<170),brake=+(tick>=80&&tick<110);
 const r=f(c._step_rider(steering,jump,brake,0),16);c._animation_tick(r[7],-steering,brake,r[9],r[8],jump,0,0,0,4,r[15],0);c._race_end();
 const info=f(c._snow_info(),23);assert(info.every(Number.isFinite));
 for(let i=0;i<10;i++){
  assert(info[i]<=info[10+i]);peaks[i]=Math.max(peaks[i],info[i]);if(info[i])seen.add(i);
  const particles=f(c._snow_particles(i),info[i]*8);assert(particles.every(Number.isFinite));
  for(let n=0;n<info[i];n++){assert(particles[n*8+3]>=0);assert(particles[n*8+7]>0&&particles[n*8+7]<=2);}
 }
 if(info[5]||info[6])impactFrames++;
 const snapshot=info.slice(),first=f(c._snow_particles(0),info[0]*8);assert.deepEqual(f(c._snow_info(),23),snapshot);assert.deepEqual(f(c._snow_particles(0),info[0]*8),first,'render reads advance particle state');
}
console.log({seen:[...seen],peaks,impactFrames});
assert(seen.has(0),'board spray emitter did not run');assert.equal(peaks[7],0,'packed snow incorrectly emits loose-snow clouds');assert(impactFrames>0,'braking/landing never produces original impact snow');assert.equal(peaks[3],0,'packed snow incorrectly emits rocks');assert(peaks[8]>0,'snow departure never produces original kicker particles');
c._reset_animation();assert(f(c._snow_info(),10).every(n=>n===0),'restart retains old particle histories');
console.log('Original snow emission and particle kernels run in gameplay:',{seen:[...seen],peaks,impactFrames});

for(const surface of [1,4]){
 const terrain=read('ARA1/terrain.json');for(const patch of terrain.patches)patch.authored_surface_id=surface;
 const pointer=str(terrain);c._init_terrain(pointer);c._free(pointer);c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();let maximum=0,clouds=0;const surfacePeaks=Array(10).fill(0);
 for(let tick=0;tick<120;tick++){
  c._race_begin();const r=f(c._step_rider(.4,0,0,0),16);c._animation_tick(r[7],-.4,0,r[9],r[8],0,0,0,0,4,r[15],0);c._race_end();
  const counts=f(c._snow_info(),10);for(let i=0;i<10;i++)surfacePeaks[i]=Math.max(surfacePeaks[i],counts[i]);maximum=Math.max(maximum,counts.reduce((a,b)=>a+b,0));clouds=Math.max(clouds,counts[7]);
 }
 if(surface===1)assert(clouds>0,'original loose-snow surface never emits clouds');else {assert.equal(surfacePeaks[0],0,'surface4 enables disabled spray');assert.equal(clouds,0,'surface4 enables disabled clouds');assert(surfacePeaks[2]>0,'surface4 suppresses its authored small chunks');}
 console.log('Original snow material gate:',{surface,maximum,clouds});
}
