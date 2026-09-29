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
let score=0;
function step(jump=0,grab=0){
 c._race_begin();const motion=f(c._step_rider(0,jump,0,0),16);
 c._animation_tick(motion[7],0,0,motion[9],motion[8],jump,grab,0,0,4,motion[15],0);
 const state=f(c._rider_state(),16),animation=f(c._animation_info(),19),reset=f(c._reset_info(),9),race=f(c._race_end(),8);score+=animation[13];
 const pose=f(c._pose_physical(),12);const camera=f(c._step_camera_head(...pose.slice(9,12)),9);assert(camera.every(Number.isFinite));
 return {state,animation,reset,race};
}
const env=read('ARA1/environment.json'),envRaw=fs.readFileSync(root+'ARA1/environment.bin'),envText=str(env),envPointer=put(envRaw);c._init_environment(envText,envPointer,envRaw.length);c._free(envText);c._free(envPointer);
c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
let maximumRows=0,maximumTip=0,maximumVertices=0,firstRowSeen=false;
function drive(turn){c._race_begin();const r=f(c._step_rider(turn,0,0,0),16);c._animation_tick(r[7],-turn,0,r[9],r[8],0,0,0,0,4,r[15],0);c._race_end();return f(c._wake_info(),13);}
for(let tick=0;tick<120;tick++){
 const wake=drive(1);assert(wake.every(Number.isFinite));assert.equal(wake[0],1);assert(wake[1]>=0&&wake[1]<=32);assert.equal(f(c._snow_info(),23)[21],0,'implemented wake still reports unavailable');if(wake[1]&&!firstRowSeen){firstRowSeen=true;assert.equal(Math.hypot(...wake.slice(7,10)),0,'initial zero-velocity cap row was skipped');}
 maximumVertices=Math.max(maximumVertices,wake[11]);assert(wake[11]<=696);assert(f(c._wake_vertices(),wake[11]*9).every(Number.isFinite),'wake ribbon has nonfinite vertices');
 maximumRows=Math.max(maximumRows,wake[1]);maximumTip=Math.max(maximumTip,Math.hypot(...wake.slice(7,10)));
}
console.log('Carving wake:',{maximumRows,maximumTip,maximumVertices});assert(maximumVertices>0);assert(maximumRows>0&&maximumTip>1,'live board input never generates wake velocity');
let wake;for(let tick=0;tick<180;tick++)wake=drive(0);assert.equal(wake[1],0,'wake rows do not expire after carving stops');assert.equal(wake[11],0,'expired wake remains drawn');
// Restart while geometry is populated: the renderer must see a new generation
// even if the next run reaches the same resettable visual tick number.
c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
for(let tick=0;tick<90;tick++)wake=drive(1);
const beforeReset=f(c._wake_info(),13);
assert(beforeReset[11]>0,'restart fixture must contain visible wake geometry');
c._reset_animation();
const afterReset=f(c._wake_info(),13);
assert.equal(afterReset[1],0,'restart retains stale wake rows');
assert.equal(afterReset[11],0,'restart retains stale drawable vertices');
assert(afterReset[12]>beforeReset[12],'restart does not invalidate the renderer cache');
const afterTick=drive(0);
assert(afterTick[12]>afterReset[12],'first post-restart tick reuses the renderer generation');
console.log('Live wake cache supplies carving velocity, expires and resets.');
