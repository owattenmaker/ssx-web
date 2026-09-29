import fs from 'node:fs';import assert from 'node:assert/strict';import {boostCoilGlows,boostOrb} from './boost-orb.js';
const p=JSON.parse(fs.readFileSync(new URL('./public/assets/UI/boost-gauge.json',import.meta.url)));
const source=JSON.parse(fs.readFileSync(new URL('../local/browser-ui/hud/boost-coil-glow.json',import.meta.url)));
let alphaError=0;
for(const c of source.cases){const tier=[0,5,10,11][c.palette],glows=boostCoilGlows(p,c.phase,tier);assert.equal(glows.length,c.phase<0?0:4);glows.forEach((g,i)=>{const d=c.glow[i+1];assert.deepEqual(g.position,d.position);assert.deepEqual(g.size,d.size);assert.deepEqual(g.uv,d.uv);assert.deepEqual(g.argb.slice(1),d.argb.slice(1));alphaError=Math.max(alphaError,Math.abs(g.argb[0]-d.argb[0]));});if(c.phase<0)continue;
 const argb=boostOrb(p,c.phase,tier).argb,rgba=[...argb.slice(1),argb[0]].map(x=>Math.trunc(x*128));
 for(const [widget,start] of [[4,0],[5,10]])p.flashBackgrounds[widget].forEach((r,i)=>{const q=c.coilQuads[start+i];assert.deepEqual([r.x,r.y],q.position.slice(0,2));assert.deepEqual([r.u,r.v],q.uv);assert.deepEqual([r.u+r.uw,r.v+r.vh],[q.rightUV[0],q.bottomUV[1]]);assert.deepEqual(q.rgba128,rgba);});
}
assert.equal(source.cases.length,808);assert(alphaError<1e-6);console.log({coilCases:808,maxAlphaError:alphaError});
