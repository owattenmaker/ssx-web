import assert from 'node:assert/strict';
import fs from 'node:fs';
import {gaugeRects} from './boost-gauge.js';
const p=JSON.parse(fs.readFileSync(new URL('./public/assets/UI/boost-gauge.json',import.meta.url)));
const captured=JSON.parse(fs.readFileSync(new URL('../local/browser-ui/hud/boost-draws.json',import.meta.url)));
let count=0,maxPosition=0,maxUV=0;
for(const c of captured.cases){
 const actual=gaugeRects(p,p.layers.find(l=>l.widget===c.widget),c.fraction);
 const expected=c.quads.filter(q=>q.vertices[2].position[1]>q.vertices[0].position[1]);
 assert.equal(actual.length,expected.length);
 actual.forEach((r,i)=>{const [a,b,d]=expected[i].vertices;for(const [got,want] of [[r.x,a.position[0]],[r.y,a.position[1]],[r.x+r.w,b.position[0]],[r.y+r.h,d.position[1]]])maxPosition=Math.max(maxPosition,Math.abs(got-want));for(const [got,want] of [[r.u,a.uv[0]],[r.v,a.uv[1]],[r.u+r.uw,b.uv[0]],[r.v+r.vh,d.uv[1]]])maxUV=Math.max(maxUV,Math.abs(got-want));count++;});
}
// Canvas uses double arithmetic; these bounds are subpixel, not bit-exact EE claims.
assert.ok(maxPosition<.0001,`position error ${maxPosition}`);
assert.ok(maxUV<.000001,`UV error ${maxUV}`);
console.log({cases:captured.cases.length,quads:count,maxPosition,maxUV});
