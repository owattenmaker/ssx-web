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


process.on('uncaughtException',e=>{console.error(e instanceof Error?e:c.getExceptionMessage(e));process.exit(1);});

for(const kind of ['none','boost','pickup']){
 c._reset_animation();c._reset_race();c._reset_rider(...start.position,start.heading);
 let emitted=0,pickupPaletteFrames=0,maxVertices=0;
 for(let tick=0;tick<450;tick++){
  if(tick===30&&kind==='boost')c._award_trick_meter(1);
  if(tick===30&&kind==='pickup')c._award_boost_pickup(1,1);
  const boost=+(kind==='boost'&&tick>=30&&tick<90);
  c._race_begin();const r=f(c._step_rider(0,0,0,boost),16);
  c._animation_tick(r[7],0,0,r[9],r[8],0,0,boost,boost,0,r[15],0);c._race_end();
  const fx=f(c._boost_fx_info(),12),count=fx[6]+fx[7]+fx[8];assert(fx.every(Number.isFinite));assert.equal(fx[11],0,'non-finite boost vertices skipped');maxVertices=Math.max(maxVertices,count);
  if(count){emitted++;if(fx[0]===60)pickupPaletteFrames++;assert(kind!=='none','inactive rider emitted boost');}
  for(let strip=0;strip<3;strip++){
   const count=fx[6+strip],data=f(c._boost_fx_vertices(strip),count*9);assert(data.every(Number.isFinite));assert(count<=180&&count%3===0);
   for(let v=0;v<count;v++){const p=data.slice(v*9,v*9+3),distance=Math.hypot(p[0]/100-r[0],p[2]/100-r[1],-p[1]/100-r[2]);assert(distance<80,'boost geometry detached from rider');}
  }
  if(tick===449)assert.equal(count,0,'released boost did not fade out');
 }
 if(kind!=='none')assert(emitted>20,'boost ribbons never appeared');if(kind==='pickup')assert(pickupPaletteFrames>20,'speed pickup did not use original texture60');
 c._reset_animation();assert.equal(f(c._boost_fx_info(),12).slice(6,9).reduce((a,b)=>a+b,0),0,'reset kept old ribbon');
 console.log('Original boost FX gameplay',{kind,emitted,pickupPaletteFrames,maxVertices});
}
