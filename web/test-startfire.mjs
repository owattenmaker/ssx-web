import fs from 'node:fs';import assert from 'node:assert/strict';
import {startfireKernel,startfireParticles,createStartfireSimulation} from './startfire-renderer.js';
const meta=JSON.parse(fs.readFileSync(new URL('public/assets/STARTFIRE/startfire.json',import.meta.url)));
assert.equal(meta.profile.TextureId,28);assert.equal(meta.profile.BlendMode,0);assert.deepEqual(meta.schedule.map(s=>s.gate),[1,20,29]);
assert.deepEqual(meta.schedule.map(s=>s.instances.map(i=>i.rid)),[[906,1973],[1120,1534],[1005,1119]]);
const k=startfireKernel(meta.profile);
assert.equal(k.count,150);assert(Math.abs(k.ageStep-.005)<1e-9,'Duration*Damp/NumParticles');assert.equal(k.remaining,2);
const sim=createStartfireSimulation(meta),box=[meta.trigger.bounds_min_cm,meta.trigger.bounds_max_cm];
const centre=box[0].map((x,i)=>(x+box[1][i])/2),outside=[box[1][0]+500,centre[1],centre[2]];
for(let i=0;i<30;i++)sim.step(outside);assert(!sim.triggered&&sim.emitters.length===0,'fired before the trigger');
sim.step(centre);assert(sim.triggered);
const fired=[];let peak=0,maxHalfExtent=0,firstColour=null;
for(let tick=1;tick<=240;tick++){
 sim.step([box[0][0]-3000,centre[1],centre[2]]);fired.push(sim.emitters.length);
 for(const e of sim.emitters)for(const p of startfireParticles(k,e)){peak=Math.max(peak,p.position[2]-e.origin[2]);maxHalfExtent=Math.max(maxHalfExtent,p.size);firstColour??=p.colour;assert(p.colour[3]<=128&&p.colour[3]>=0);}
}
assert.equal(fired[0],2,'first pair at gate 1/30s');assert.equal(fired[39],4,'second pair at 20/30s');assert.equal(fired[57],6,'third pair at 29/30s');
assert.equal(fired[239],0,'emitters expire after Duration+Life');
assert.deepEqual(firstColour,[128,128,128,128],"start colour ARGB 1 at emission");
assert(peak>800&&peak<1400,`fountain height ${peak}cm`);assert(maxHalfExtent>90&&maxHalfExtent<=107.5,`half extent ${maxHalfExtent}`);
sim.step([box[1][0]+2000,centre[1],centre[2]]);assert(!sim.triggered&&sim.emitters.length===0,"restart uphill re-arms the stage event");
console.log(`Startfire pairs fire at gates 1/20/29, rise ${Math.round(peak)}cm, grow to ${maxHalfExtent.toFixed(1)}cm half extent and expire after 2s.`);
