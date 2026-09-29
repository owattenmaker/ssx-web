import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore();
const bytes=fs.readFileSync('public/assets/ARA1/collision.bin'),p=c._malloc(bytes.length);c.HEAPU8.set(bytes,p);c._init_world(p,bytes.length/4);c._free(p);
const terrain=new TextEncoder().encode(fs.readFileSync('public/assets/ARA1/terrain.json','utf8')+'\0'),t=c._malloc(terrain.length);c.HEAPU8.set(terrain,t);c._init_terrain(t);c._free(t);
const start=JSON.parse(fs.readFileSync('public/assets/ARA1/start.json'));c._reset_rider(...start.position,start.heading);
let groundedTicks=0,unsnappedTicks=0,maxSeparation=0;
for(let tick=0;tick<240;tick++){
 const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,0,0,0),16);
 assert(r.every(Number.isFinite));assert.equal(r[13],0);
 if(!r[8])continue;
 groundedTicks++;
 const floor=c._height_at(r[0],r[1]+2,r[2]);assert(floor>-1e8);
 const separation=Math.abs(r[1]-floor);maxSeparation=Math.max(maxSeparation,separation);
 assert(separation<.5,'ground contact drifts far from the terrain');
 unsnappedTicks+=separation>.001;
}
assert(groundedTicks>50&&unsnappedTicks>50,'ground movement still teleports the actor to each query point');
console.log('Grounded integrated position retained:',{groundedTicks,unsnappedTicks,maxSeparation});
