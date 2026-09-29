import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),root='public/assets/';
const raw=fs.readFileSync(root+'ARA1/collision.bin'),p=c._malloc(raw.length);
c.HEAPU8.set(raw,p);c._init_world(p,raw.length/4);c._free(p);
const start=JSON.parse(fs.readFileSync(root+'ARA1/start.json'));
const snapshot=()=>new Float32Array(c.HEAPF32.buffer,c._rider_state(),16).slice();
c._reset_rider(...start.position,start.heading);
const initial=snapshot();
assert.equal(initial[0],Math.fround(start.position[0]));
assert.equal(initial[2],Math.fround(start.position[2]));
assert.equal(initial[9],0);assert.equal(initial[10],0);assert.equal(initial[11],0);assert.equal(initial[12],0);assert.equal(initial[15],0);
for(let i=0;i<10;i++)assert.deepEqual(snapshot(),initial,'reading the starting state advances motion');
const first=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,0,0,0),16).slice();
assert.notEqual(first[0],initial[0],'first real tick should move the captured starting velocity');
for(let i=0;i<3;i++){
 for(let tick=0;tick<60;tick++)c._step_rider(.4,+(tick<30),0,0);
 c._reset_rider(...start.position,start.heading);
 assert.deepEqual(snapshot(),initial,'restart retains old motion or landing data');
 assert.deepEqual(new Float32Array(c.HEAPF32.buffer,c._step_rider(0,0,0,0),16).slice(),first);
}
console.log('Start/restart snapshots preserve exact X/Z spawn; reads do not advance motion; first real tick is repeatable.');
