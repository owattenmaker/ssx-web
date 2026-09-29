import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const riderPackage=process.argv.includes('--zoe')?'RIDER_ZOE':'RIDER_SAM';
const c=await createCore(),root='public/assets/';
const read=p=>JSON.parse(fs.readFileSync(root+p));
const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;};
const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const metadata=str(read('ANIMATIONS/animation-packets.json')),rig=str(read(riderPackage+'/rider.json')),settings=str(read('ANIMATIONS/initial.json'));
const raw=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin'),packets=put(raw);
c._init_animation(metadata,rig,settings,packets,raw.length);for(const p of [metadata,rig,settings,packets])c._free(p);
const mesh=fs.readFileSync(root+'ARA1/collision.bin'),meshPtr=put(mesh);c._init_world(meshPtr,mesh.length/4);c._free(meshPtr);
const terrain=str(read('ARA1/terrain.json'));c._init_terrain(terrain);c._free(terrain);
const world=read('ARA1/world_collision.json'),source=read('ARA1/terrain.json').source_sha256;
const wp=str(world),hp=put(new TextEncoder().encode(source+'\0'));c._init_world_collision(wp,hp);c._free(wp);c._free(hp);
c._animation_use_physics(1);const start=read('ARA1/start.json');
const bt=str(read('ARA1/terrain.json'));c._init_body_terrain(bt);c._free(bt);
const runs=[];
for(const [flip,quick] of [[1,true],[-1,true],[1,false]]){
 const transitions=[];let sawCameraShake=false;let seen=false,finished=false,prior='',bankedDuringCrash=0,entry=-1,exit=-1;
 c._reset_rider(...start.position,start.heading);c._reset_animation();
 for(let tick=0;tick<2400;tick++){
  const active=!!new Float32Array(c.HEAPF32.buffer,c._crash_info(),12)[0];
  const jump=active?+quick:+(tick>90&&tick<150);
  const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(active?1:0,jump,0,0),16).slice();
  c._animation_tick(r[7],active?1:0,0,r[9],r[8],jump,active?15:0,active?1:0,0,4,r[15],seen?0:flip);
  const info=new Float32Array(c.HEAPF32.buffer,c._crash_info(),12).slice();
  const state=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16).slice();
  const prepared=new Float32Array(c.HEAPF32.buffer,c._pose_physical(),12);
  const camera=new Float32Array(c.HEAPF32.buffer,c._step_camera_head(...prepared.slice(9,12)),9);
  assert(camera.every(Number.isFinite),'crash camera contains nonfinite values');
  const cameraInput=new Float32Array(c.HEAPF32.buffer,c._camera_inputs(),18);
  if(cameraInput[14]===4&&cameraInput[15]===1&&cameraInput[17]>0)sawCameraShake=true;
  assert.equal(cameraInput[12],info[0]?2:state[8]?0:1,'camera does not follow the active source motion mode');
  const animation=new Float32Array(c.HEAPF32.buffer,c._animation_info(),19).slice();
  assert(state.every(Number.isFinite)&&info.every(Number.isFinite));assert.equal(state[13],0,'crash recovery was replaced by an emergency respawn');
  const key=[info[0],info[1],info[2],animation[0]].join(',');
  if(key!==prior){transitions.push({tick,active:!!info[0],phase:info[1],motion:info[2],semantic:animation[0],speed:state[7],recovery:info[7]});prior=key;}
  if(info[0]){
   if(!seen)entry=tick;seen=true;assert.equal(animation[15],8,'crash animation runs under ordinary riding control');
   assert(animation[0]>=328&&animation[0]<=410,'grab/steer input overrides the crash clip');
   assert.equal(state[9],0,'crash controller charges a jump');assert.equal(animation[12],0,'crash retains pending trick points');bankedDuringCrash+=animation[13];
  }
  if(seen&&!info[0]){
   finished=true;exit=tick;assert([0,5].includes(animation[15]),'recovery dispatches another input controller on the same tick');
   assert.equal(state[9],0,'held recovery charges a jump on the exit tick');
   assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),17)[6],0,'recovery applies a second steering-control update');
   if(state[8]){
    assert.equal(animation[0],5,'recovery immediately replaces the original riding-entry animation');
    const next=new Float32Array(c.HEAPF32.buffer,c._step_rider(1,1,0,0),16).slice();
    c._animation_tick(next[7],1,0,next[9],next[8],1,0,0,0,4,next[15],0);
    assert(next[9]>0,'normal jump control did not resume on the following tick');
    assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_info(),19)[15],2);
   }
   break;
  }
 }
 assert(sawCameraShake,'original ground crash camera shake never reached the compositor');
 assert(seen&&finished,'original crash lifecycle did not complete');assert.equal(bankedDuringCrash,0,'failed trick banked score');
 assert(transitions.some(t=>t.active&&t.phase===0&&t.semantic>=328&&t.semantic<=361));
 assert(transitions.some(t=>t.active&&(t.phase===1||t.phase===2)&&t.semantic>=362&&t.semantic<=388));
 assert(transitions.some(t=>t.active&&t.phase===3&&t.semantic>=389&&t.semantic<=408));
 runs.push({flip,quick,entry,exit,transitions});
}
console.log(JSON.stringify(runs,null,2));
fs.writeFileSync('../local/browser-validation/'+(riderPackage==='RIDER_SAM'?'crash-gameplay.json':'crash-gameplay-'+riderPackage+'.json'),JSON.stringify(runs,null,2));

// Animation reset must also clear the one-frame recovery ownership flag.
c._reset_animation();c._animation_use_physics(0);
for(let tick=0;tick<60;tick++)c._animation_tick(18,0,0,1,1,1,0,0,0,4,0,0);
assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_info(),19)[15],2,'recovery ownership survives an animation reset');
