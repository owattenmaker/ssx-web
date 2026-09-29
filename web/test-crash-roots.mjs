import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),base='public/assets/',allocated=[];
const read=p=>JSON.parse(fs.readFileSync(base+p));
const put=data=>{const p=c._malloc(data.length);allocated.push(p);c.HEAPU8.set(data,p);return p;};
const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const bytes=fs.readFileSync(base+'ANIMATIONS/animation-packets.bin');
c._init_animation(str(read('ANIMATIONS/animation-packets.json')),str(read('RIDER_SAM/rider.json')),str(read('ANIMATIONS/initial.json')),put(bytes),bytes.length);
for(const p of allocated)c._free(p);
assert.equal(c._sampled_local_root(),0,'unsampled pose claims a current crash root');
for(let tick=0;tick<20;tick++)c._animation_tick(18,0,0,0,1,0,0,0,0,4,0,0);
const root=()=>Array.from(new Float32Array(c.HEAPF32.buffer,c._sampled_local_root(),7));
const current=root(),state=Array.from(new Float32Array(c.HEAPF32.buffer,c._animation_info(),19));
for(let semantic=328;semantic<=361;semantic++){
 const p=c._crash_root_preview(semantic);assert(p,`missing crash root ${semantic}`);
 const preview=new Float32Array(c.HEAPF32.buffer,p,7);
 assert(preview.every(Number.isFinite));assert(Math.abs(Math.hypot(...preview.slice(3))-1)<.001);
 assert.deepEqual(root(),current,'preview changed the current sampled root');
 assert.deepEqual(Array.from(new Float32Array(c.HEAPF32.buffer,c._animation_info(),19)),state,'preview changed active animation');
}
assert.equal(c._crash_root_preview(999999),0);
c._reset_animation();assert.equal(c._sampled_local_root(),0);
console.log('All 34 original crash roots preview without changing current pose or playback; unsampled/invalid roots remain explicit.');
