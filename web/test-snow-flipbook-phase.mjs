import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),root='public/assets/';
const read=p=>JSON.parse(fs.readFileSync(root+p));
const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;};
const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const metadata=str(read('ANIMATIONS/animation-packets.json')),rig=str(read('RIDER_SAM/rider.json')),settings=str(read('ANIMATIONS/initial.json'));
const raw=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin'),packets=put(raw);
c._init_animation(metadata,rig,settings,packets,raw.length);for(const p of [metadata,rig,settings,packets])c._free(p);
const mesh=fs.readFileSync(root+'ARA1/collision.bin'),meshPtr=put(mesh);c._init_world(meshPtr,mesh.length/4);c._free(meshPtr);
const terrain=str(read('ARA1/terrain.json'));c._init_terrain(terrain);c._free(terrain);
const world=read('ARA1/world_collision.json'),source=read('ARA1/terrain.json').source_sha256;
const wp=str(world),hp=put(new TextEncoder().encode(source+'\0'));c._init_world_collision(wp,hp);c._free(wp);c._free(hp);
c._animation_use_physics(1);const start=read('ARA1/start.json');
const bt=str(read('ARA1/terrain.json'));c._init_body_terrain(bt);c._free(bt);
const cfg=read('ANIMATIONS/initial.json'),raceConfig=str(cfg);c._init_race(raceConfig);c._free(raceConfig);
const f=(ptr,n)=>new Float32Array(c.HEAPF32.buffer,ptr,n).slice();

const profiles=read('SNOW_FX/snow-fx.json').profiles,seen=Array.from({length:10},()=>new Set());
c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
const initialPhases=Float32Array.from(cfg.original_snow.state.emitter_flipbook_phases);
assert.deepEqual(f(c._snow_flipbook_info(),20).slice(10),initialPhases);
assert(initialPhases.some(x=>x!==0),"Fixture must expose nonzero saved phases");
for(let tick=0;tick<64;tick++){
 const before=f(c._snow_flipbook_info(),20);c._race_begin();const r=f(c._step_rider(0,0,0,0),16);c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);c._race_end();const after=f(c._snow_flipbook_info(),20);
 for(const profile of profiles){const i=profile.emitter_index,p=profile.parameters,increment=Math.fround(p.FlipTextureRate/60),candidate=Math.fround(before[10+i]+increment),expected=Math.trunc(candidate)<p.NumFlipTextures?candidate:0;assert.equal(after[10+i],expected);assert.equal(after[i],p.TextureId+Math.trunc(expected));seen[i].add(after[i]);}
 assert.deepEqual(f(c._snow_flipbook_info(),20),after,'Reading draw state advanced the flipbook');
}
for(const i of [1,2,3])assert.deepEqual([...seen[i]].sort((a,b)=>a-b),[14,15,16,17,18,19,20,21]);
c._reset_animation();assert.deepEqual(f(c._snow_flipbook_info(),20).slice(10),initialPhases);console.log('Snow flipbooks follow original shared-emitter phase and reset, reaching all eight frames.');
// Source visibility mode2 disables the trail/chunks/breath/cloud/kicker/body
// emitters. Disabled births retain phase and do not publish their old history.
const disabledConfig=structuredClone(cfg);disabledConfig.original_snow.initial_rider.visibility_mode=2;
const again=[str(read('ANIMATIONS/animation-packets.json')),str(read('RIDER_SAM/rider.json')),str(disabledConfig),put(raw)];
c._init_animation(again[0],again[1],again[2],again[3],raw.length);for(const p of again)c._free(p);
c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
for(let tick=0;tick<20;tick++){
 const before=f(c._snow_flipbook_info(),20);
 c._race_begin();const r=f(c._step_rider(0,0,0,0),16);c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);c._race_end();
 const phases=f(c._snow_flipbook_info(),20),particles=f(c._snow_info(),23);
 const active=profiles.find(p=>p.emitter_index===5).parameters,candidate=Math.fround(before[15]+Math.fround(active.FlipTextureRate/60));assert.equal(phases[15],Math.trunc(candidate)<active.NumFlipTextures?candidate:0,'Enabled impact emitter must keep updating');
 for(const index of [0,1,2,3,4,7,8,9]){assert.equal(phases[10+index],initialPhases[index],'Disabled emitter advanced its phase');assert.equal(particles[index],0,'Disabled emitter published particles');}
}
console.log('Disabled snow emitters retain phase and publish no particles across gameplay ticks.');
