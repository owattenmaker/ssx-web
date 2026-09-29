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
 const state=f(c._rider_state(),16),animation=f(c._animation_info(),19),reset=f(c._reset_info(),8),race=f(c._race_end(),8);score+=animation[13];
 const pose=f(c._pose_physical(),12);const camera=f(c._step_camera_head(...pose.slice(9,12)),9);assert(camera.every(Number.isFinite));
 return {state,animation,reset,race};
}
const results=[];
for(const [semantic,quick] of [[360,true],[361,false]]){
 c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
 for(let tick=0;tick<240;tick++)step();
 c._hard_crash_begin(semantic,1000);let detached=false,phase4=false,reset=false,done=false;const clips=new Set();
 for(let tick=0;tick<2400;tick++){
  const frame=step(+quick),crash=f(c._crash_info(),12);detached||=!!crash[6];phase4||=crash[1]===4;reset||=!!frame.reset[0];clips.add(frame.animation[0]);
  assert(frame.state.every(Number.isFinite));assert.equal(frame.state[13],0,'detached reset restarted the demo');
  if(frame.reset[3]){done=true;break;}
 }
 assert(detached&&reset&&done,'detached crash never completed source reset');if(!quick)assert(phase4,'automatic detached recovery bypassed reset clip phase');
 results.push({semantic,quick,phase4,clips:[...clips]});
}
console.log('Detached board crashes reach on-course forced reset:',results);
