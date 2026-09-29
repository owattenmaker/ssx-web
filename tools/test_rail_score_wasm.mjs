import fs from 'node:fs';
import create from '../local/browser-validation/rail-score.mjs';
const core=await create(),read=name=>fs.readFileSync(new URL(`../local/browser-validation/${name}`,import.meta.url));
const input=read('rail-score-input.bin'),expected=read('rail-score-expected.bin'),p=core._malloc(input.length),q=core._malloc(expected.length),count=input.length/60;
if(count!==20000||expected.length!==count*48)throw Error('Rail score corpus extent');
core.HEAPU8.set(input,p);core._rail_score_rows(p,q,count);
const words=new Uint32Array(expected.buffer,expected.byteOffset,expected.length/4);for(let i=0;i<words.length;i++)if(core.HEAPU32[q/4+i]!==words[i])throw Error(`Rail score row ${Math.floor(i/12)} field ${i%12} differs`);
core._free(p);core._free(q);console.log('20000 WASM rail scoring stages match original executable output bits, including threshold events.');
