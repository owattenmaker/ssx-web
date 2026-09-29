import fs from 'node:fs';
import createModule from '../local/browser-validation/software-float.mjs';
const core=await createModule();
const scopeFailure=core._rounding_scope_test();
if(scopeFailure)throw Error(`Rounding scope/EE scalar exception failure ${scopeFailure}`);
const bytes=fs.readFileSync(new URL('../local/browser-validation/float-corpus.bin',import.meta.url));
const expected=new Uint32Array(bytes.buffer,bytes.byteOffset,bytes.byteLength/4);
const pointer=core._malloc(bytes.length);core.HEAPU8.set(bytes,pointer);
const count=bytes.length/28;
core._evaluate(pointer,count);
const actual=core.HEAPU32.subarray(pointer/4,pointer/4+expected.length);
const nan=bits=>(bits&0x7f800000)===0x7f800000&&(bits&0x7fffff)!==0;
for(let i=0;i<count;i++)for(let op=0;op<5;op++){
 const j=i*7+2+op,a=actual[j],e=expected[j];
 if(a!==e&&!(nan(a)&&nan(e)))throw Error(`WASM pair ${i} operation ${op}: ${a.toString(16)} != ${e.toString(16)}`);
}
core._free(pointer);
console.log(`${count} operand pairs: WASM software arithmetic matches native directed-rounding corpus (${count*5} results).`);
