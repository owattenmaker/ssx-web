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

for(const pose of [0,.1,.31,.62,1,1.2]){
 const result=new Float32Array(c.HEAPF32.buffer,c._start_animation_probe(pose),4).slice();
 assert.equal(result[0],8192,'wrong original start clip');
 assert(Math.abs(result[1]-Math.fround(pose)*result[2])<1e-6,'start pose did not select the source frame');
 assert.equal(result[1],result[3],'start pose advanced by elapsed time');
}
console.log('Original start-pose animation enters and seeks correctly in WASM.');
