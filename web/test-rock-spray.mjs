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

const patch=read('ARA1/terrain.json').patches.find(p=>p.resource_id===100104);
const center=[0,0,0];for(let j=0;j<4;j++)for(let i=0;i<4;i++)for(let k=0;k<3;k++)center[k]+=patch.coefficients[j*4+i][k]*.5**(i+j);
c._reset_rider(center[0],center[1]+.04,center[2],start.heading);c._reset_animation();c._reset_race();
assert.equal(patch.authored_surface_id,7);
const peaks=Array(10).fill(0);
for(let tick=0;tick<360;tick++){
 const steer=tick<90?.5:0,brake=+(tick>=60&&tick<90),jump=+(tick>=110&&tick<170);c._race_begin();const r=f(c._step_rider(steer,jump,brake,0),16);c._animation_tick(r[7],steer,brake,r[9],r[8],jump,0,0,0,0,r[15],0);c._race_end();const info=f(c._snow_info(),23);for(let i=0;i<10;i++){peaks[i]=Math.max(peaks[i],info[i]);assert(f(c._snow_particles(i),info[i]*8).every(Number.isFinite));}
}
console.log({patch:patch.resource_id,center,peaks});assert(peaks[3]>0,'Authored surface7 fixture must emit rock spray');
