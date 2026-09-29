import fs from 'node:fs';
import create from '../local/browser-validation/air-rounding.mjs';
const core=await create(),read=name=>fs.readFileSync(new URL(`../local/browser-validation/${name}`,import.meta.url));
const input=read('air-input.bin'),expected=read('air-expected.bin');
const words=new Uint32Array(expected.buffer,expected.byteOffset,expected.length/4),p=core._malloc(input.length);
core.HEAPU8.set(input,p);core._air_evaluate(p,input.length/36);
for(let i=0;i<words.length;i++)if(core.HEAPU32[p/4+i]!==words[i])throw Error(`Air step ${Math.floor(i/9)} field ${i%9} differs`);
core._free(p);console.log('20000 WASM airborne steps match native directed rounding exactly across position, velocity, speed and cap branch.');
