import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),data=JSON.parse(fs.readFileSync('public/assets/ARA1/rails.json'));
const put=text=>{const bytes=Buffer.from(text+'\0'),p=c._malloc(bytes.length);c.HEAPU8.set(bytes,p);return p;};
const text=put(JSON.stringify(data)),hash=put(data.source_sha256);c._init_rails(text,hash);c._free(text);c._free(hash);
const view=(p,n)=>new Float32Array(c.HEAPF32.buffer,p,n).slice();assert.deepEqual(Array.from(view(c._rail_info(),3)),[1,data.rail_count,data.segment_count]); // the whole event residency (tools/import_rails.py: A_ARA1 7 + ARA1 171 + ARA1_B 4 rails)
const raw=fs.readFileSync('../local/browser-validation/rail-queries.bin'),values=new Float32Array(raw.buffer,raw.byteOffset,raw.byteLength/4);let count=0;
for(let offset=0;offset<values.length;offset+=17){const got=view(c._rail_query(...values.slice(offset,offset+3)),14),expected=values.slice(offset+3,offset+17);assert.deepEqual(got,expected,`Rail query ${count}`);++count;}
assert.equal(count,data.segment_count*5);
assert.equal(view(c._rail_query(0,0,0),14)[1],0,'far-away query must miss');
const bad=put(JSON.stringify(data)),wrong=put('wrong-world');let rejected=false;try{c._init_rails(bad,wrong);}catch{rejected=true;}finally{c._free(bad);c._free(wrong);}assert(rejected,'mixed course rail data accepted');assert.equal(view(c._rail_info(),3)[1],data.rail_count,'failed load destroyed the valid catalog');
console.log(`${count} authored rail queries match native exactly; provenance mismatch rejected without losing valid catalog.`);
