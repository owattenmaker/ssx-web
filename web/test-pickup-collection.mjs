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
const catalog=JSON.parse(fs.readFileSync('../local/browser-pickups/ara1-catalog.json'));
for(const definition of cfg.original_pickups.items){
 const target=catalog.items.find(x=>x.resource===definition.resource),pos=target.position_m;
 c._reset_rider(...pos,start.heading);c._reset_animation();c._reset_race();c._set_rider_velocity(0,0,0);
 let collected=-1,last;
 for(let tick=0;tick<150;tick++){
  c._race_begin();const r=f(c._step_rider(0,0,0,0),16);c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);c._race_end();last=f(c._pickup_info(),26);
  if(last[1]&&collected<0){collected=tick;assert.equal(last[2],definition.resource);const boost=f(c._boost_info(),8);assert.equal(boost[definition.effect_type===1?2:4],5,'authored reward was not applied');assert.equal(c._pickup_visible(definition.resource),0);const before=f(c._pickup_info(),26),reward=f(c._boost_info(),8);for(let pass=0;pass<8;pass++)c._pickup_contact_pass();assert.deepEqual(f(c._pickup_info(),26),before,'extra contact passes change lifetime or re-award');assert.deepEqual(f(c._boost_info(),8),reward,'extra contact passes advance reward timers');}
 }
 assert(collected>=0,'posed rider never collected '+target.name+' diagnostics '+JSON.stringify(Array.from(last)));
 assert.equal(last[1],1,'pickup awarded more than once');const row=cfg.original_pickups.items.findIndex(p=>p.resource===definition.resource);assert.equal(last[6+row*4+1],2,'pickup did not reach consumed replacement');
 c._reset_rider(...pos,start.heading);c._reset_animation();c._set_rider_velocity(0,0,0);assert.equal(c._pickup_visible(definition.resource),0,'animation reset resurrected a consumed pickup');
 for(let i=0;i<5;i++){const r=f(c._step_rider(0,0,0,0),16);c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);}
 assert.equal(f(c._pickup_info(),26)[1],1,'returning to a consumed pickup awarded again');
 c._reset_race();assert.equal(c._pickup_visible(definition.resource),1,'new race did not restore pickup');
 console.log('Collected authored pickup:',{resource:definition.resource,collected,type:definition.effect_type});
}
