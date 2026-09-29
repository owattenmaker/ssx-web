import fs from 'node:fs';import assert from 'node:assert/strict';import {boostOrb} from './boost-orb.js';
const profile=JSON.parse(fs.readFileSync(new URL('./public/assets/UI/boost-gauge.json',import.meta.url)));
const source=JSON.parse(fs.readFileSync(new URL('../local/browser-ui/hud/boost-orb.json',import.meta.url)));
let error=0;
for(const c of source.cases){const a=boostOrb(profile,c.phase,[0,5,10,11][c.palette]),d=c.draw;assert.deepEqual(a.argb,d.argb);assert.deepEqual([a.uv[1],a.uv[0],a.uv[2],a.uv[3]],d.uv);for(let i=0;i<2;i++){error=Math.max(error,Math.abs(a.position[i]-d.position[i]),Math.abs(a.size[i]-d.size[i]));}assert.deepEqual(d.scale,[1,1]);assert.equal(d.order,10);}
assert.equal(source.cases.length,808);assert(error<.0001);console.log({originalOrbCases:source.cases.length,maxPixelError:error});
