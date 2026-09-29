import assert from 'node:assert/strict';import fs from 'node:fs';import createCore from './runtime/core.js';
const c=await createCore(),read=p=>fs.readFileSync(new URL('./public/assets/'+p,import.meta.url));
const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;};
const collision=read('ARA1/collision.bin'),p=put(collision);c._init_world(p,collision.length/4);c._free(p);
const t=put(Buffer.concat([read('ARA1/terrain.json'),Buffer.from([0])]));c._init_terrain(t);c._free(t);
const start=JSON.parse(read('ARA1/start.json')),right=[-Math.cos(start.heading),Math.sin(start.heading)];
const runs=[];
for(const steering of [-1,0,1]){c._reset_rider(...start.position,start.heading);let state;for(let i=0;i<45;i++){state=new Float32Array(c.HEAPF32.buffer,c._step_rider(i>=15?steering:0,0,0,0),16).slice();assert.equal(state[8],1,'steering fixture left the ground');}runs.push([state[0],state[2]]);}
const lateral=r=>(r[0]-runs[1][0])*right[0]+(r[1]-runs[1][1])*right[1];
const left=lateral(runs[0]),rightward=lateral(runs[2]);console.log({leftMeters:left,rightMeters:rightward});
assert(left<-.1,'Left input must move left relative to neutral riding');assert(rightward>.1,'Right input must move right relative to neutral riding');
