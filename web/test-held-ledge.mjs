import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore();
const bytes=fs.readFileSync('public/assets/ARA1/collision.bin'),p=c._malloc(bytes.length);c.HEAPU8.set(bytes,p);c._init_world(p,bytes.length/4);c._free(p);
const terrain=new TextEncoder().encode(fs.readFileSync('public/assets/ARA1/terrain.json','utf8')+'\0'),t=c._malloc(terrain.length);c.HEAPU8.set(terrain,t);c._init_terrain(t);c._free(t);
const start=JSON.parse(fs.readFileSync('public/assets/ARA1/start.json'));c._reset_rider(...start.position,start.heading);
let departure=-1,landing=-1,airTicks=0;const reference=[];
for(let tick=0;tick<1800;tick++){
 const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,1,0,0),16).slice();reference.push(r);
 assert.equal(r[13],0,'held-ledge fixture required an emergency reset');
 if(!r[8]){if(departure<0)departure=tick;airTicks++;assert(r[9]>.99,'held jump charge lost while airborne');}
 if(r[11]&&departure>=0){landing=tick;assert(r[9]>.99,'soft touchdown lost held jump charge');break;}
}
assert(departure>=0&&landing>departure&&airTicks>1,'course did not exercise held ledge departure and touchdown');
console.log('Course held-ledge charge survives flight and landing:',{departure,landing,airTicks});

c._reset_rider(...start.position,start.heading);
for(let tick=0;tick<=landing;tick++){
 const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,+(tick<departure+10),0,0),16);
 if(tick>=departure+10){
  assert.deepEqual(Array.from(r.slice(0,8)),Array.from(reference[tick].slice(0,8)),'midair jump release injected a second impulse');
  assert.equal(r[14],reference[tick][14]);
 }
}
console.log('Releasing held jump after ledge departure leaves the airborne trajectory unchanged.');
