import fs from 'node:fs';
import assert from 'node:assert/strict';
import {originalModelBlend} from './world-material.js';
// Package checks for the original PS2 terrain/model combine (world-material.js).
const root='public/assets/ARA1/',json=f=>JSON.parse(fs.readFileSync(root+f));
const lighting=json('terrain-lighting.json'),render=json('terrain-render.json'),environment=json('environment.json'),world=json('world.json');
assert.equal(lighting.source_sha256,render.source_sha256);
const vertices=new Float32Array(fs.readFileSync(root+'vertices.bin').buffer.slice(0)),env=new Map(environment.patches.map(p=>[p.resource,p]));
let checked=0,maxError=0;
for(const patch of render.patches){
 const [page,p0,p1,p2,p3]=lighting.patches[patch.resource],e=env.get(patch.resource),g=patch.lightUv;
 assert.deepEqual([p0,p1,p2,p3],e.light_uv,'PS2 light UV comes from the original patch +10');
 const kinds=[0,1,2].map(s=>(e.flags>>(3*s))&7),slot=kinds.indexOf(5);
 assert.equal(environment.textures[e.textures[slot]].source_id,lighting.pages[page].source_id,'layer type5 page');
 // Same mapping as world-material.js: GC uv1 -> PS2 page UV, checked on all 81 vertices.
 for(let k=0;k<81;k++){
  const u=(k%9)/8,v=Math.floor(k/9)/8,at=(patch.baseline_vertex_start+k)*10+8;
  const pu=p0+(vertices[at]-g[0])*p2/g[2],pv=p1+(vertices[at+1]-g[1])*p3/g[3];
  maxError=Math.max(maxError,Math.abs(pu-(p0+u*p2)),Math.abs(pv-(p1+v*p3)));checked++;
 }
}
assert(maxError<2e-6,`GC->PS2 light UV mapping error ${maxError}`);
for(const page of lighting.pages)assert(page.x>=1&&page.y>=1&&page.x+page.width<lighting.atlas.width&&page.y+page.height<lighting.atlas.height);
const alpha=fs.readFileSync(root+'vertex-alpha.bin');assert.equal(alpha.length,vertices.length/10);
assert.equal(alpha.filter(v=>v===0).length,1420,'V4-5 alpha-bit-clear static model vertices');
assert(alpha.every(v=>v===0||v===128));
const classes=world.batches.reduce((c,b)=>{if(b.instance)c[b.blend||0]=(c[b.blend||0]||0)+1;else assert.equal(b.blend,undefined);return c;},{});
assert(classes[0]&&classes[1]&&classes[2]&&classes[3],'all four static-model blend classes present');
assert.deepEqual([1,3,7,0x21,0x27,0x61,25,27,31].map(f=>originalModelBlend(f)),[0,1,2,0,2,0,0,1,2]);
assert.equal(originalModelBlend(1,8),2,'group flag bit3 selects the sorted class');
console.log('Original world combine package:',{patches:render.patches.length,uvChecks:checked,maxError,pages:lighting.pages.length,modelBatches:classes});
