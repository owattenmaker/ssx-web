import fs from 'node:fs';
import assert from 'node:assert/strict';
import {boostLetters} from './boost-letters.js';
const profile=JSON.parse(fs.readFileSync(new URL('./public/assets/UI/boost-gauge.json',import.meta.url)));
const rows=fs.readFileSync(new URL('../local/boost-letter-draw-reference.csv',import.meta.url),'utf8').trim().split('\n');assert.equal(rows.shift(),'count,fraction,letter,mode,order,scale_x,scale_y');
const groups=new Map();for(const row of rows){const r=row.split(',').map(Number);assert(r.every(Number.isFinite));const k=r.slice(0,2).join(',');if(!groups.has(k))groups.set(k,[]);groups.get(k).push(r);}
let maxScaleError=0;
for(const group of groups.values()){
 const [count,fraction]=group[0],actual=boostLetters(profile,count,Math.fround(fraction),false).filter(x=>x.draws.length===2);
 assert.equal(actual.length,group.length,`active count at ${count}/${fraction}`);
 group.forEach(([, ,letter,mode,order,sx,sy],i)=>{assert.equal(actual[i].letter,letter);assert.equal(mode,2);assert.equal(sx,sy);maxScaleError=Math.max(maxScaleError,Math.abs(actual[i].scale-sx));});
}
assert.equal(groups.size,9*181);assert(maxScaleError<.000001,`scale error ${maxScaleError}`);
assert(boostLetters(profile,9,1,true).every(x=>x.draws.length===1));
console.log({sourceDrawCases:groups.size,maxScaleError});
