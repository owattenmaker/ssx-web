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
const results=[];
for(const [spin,flip] of [[0,0],[1,0],[-1,0]]){
 c._reset_rider(...start.position,start.heading);c._reset_animation();let previousGround=true,exits=0,maxRotationChange=0,maxPivotShift=0,landingReversals=0;
 for(let tick=0;tick<600;tick++){
  const jump=+(tick>90&&tick<150);
  const turn=previousGround?0:spin;
  let r=new Float32Array(c.HEAPF32.buffer,c._step_rider(turn,jump,0,0),16).slice();
  assert.equal(r[11],0,'gameplay landed through the pre-pose vertical fallback');
  const before=new Float32Array(c.HEAPF32.buffer,c._air_exit_info(),19).slice();
  const stanceBefore=new Float32Array(c.HEAPF32.buffer,c._rider_stance_info(),9).slice();
  c._animation_tick(r[7],turn,0,r[9],r[8],jump,0,0,0,4,r[15],flip);
  r=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16).slice();
  const drawn=new Float32Array(c.HEAPF32.buffer,c._pose_physical(),12);
  if(!drawn[8]){
   const probe=new Float32Array(c.HEAPF32.buffer,c._landing_probe_info(),17);
   if(!probe[0])assert(previousGround&&!r[11],'air motion lacks posed landing query');
   else {assert.equal(probe[1],1,'posed landing query incomplete on fixture');
   assert.equal(probe[3],r[11],'published touchdown differs from posed contact acceptance');
   const body=new Float32Array(c.HEAPF32.buffer,c._body_volume_info(),108);
   const translation=new Float32Array(c.HEAPF32.buffer,c._pose_translation(),3);
   for(let k=0;k<3;k++)assert(Math.abs(probe[4+k]+translation[k]-body[5+k])<.02,'landing ray must use the board before the late geometry translation');
   assert(Math.abs(Math.hypot(...probe.slice(7,10))-1)<.0001,'landing ray ignores normalized presentation up');
   if(r[11]){assert(probe[2]>=.5,'landing bypasses original coarse-fraction gate');assert.equal(new Float32Array(c.HEAPF32.buffer,c._landing_info(),6)[3],4);}
   }
  }
  const stanceAfter=new Float32Array(c.HEAPF32.buffer,c._rider_stance_info(),9).slice();
  if(stanceAfter[8]>stanceBefore[8]){
   assert(r[11],'animation reverses physics outside touchdown');assert.equal(stanceAfter[8],stanceBefore[8]+1);
   assert.notEqual(stanceAfter[0],stanceBefore[0]);assert.equal(stanceAfter[0],stanceAfter[1],'landing mirror differs from physical stance');
   const semantic=new Float32Array(c.HEAPF32.buffer,c._animation_info(),19)[0];
   assert([64,65].includes(semantic),'reversed landing played cruise reverse or ordinary landing clip');landingReversals++;
  }
  const after=new Float32Array(c.HEAPF32.buffer,c._air_exit_info(),19).slice();
  assert(after[0]-before[0]>=0&&after[0]-before[0]<=1,'contact phase baked air exit more than once');
  if(after[0]>exits){
   assert.equal(after[0],exits+1);assert(r[11]&&!previousGround,'air exit ran outside touchdown');
   assert(after.every(Number.isFinite));assert(Math.abs(Math.hypot(...after.slice(11,15))-1)<.00002);
   maxRotationChange=Math.max(maxRotationChange,Math.hypot(...after.slice(11,15).map((x,i)=>x-after[4+i])));
   maxPivotShift=Math.max(maxPivotShift,Math.hypot(...after.slice(8,11).map((x,i)=>x-after[1+i])));
   exits=after[0];
  }
  assert.equal(r[13],0,'air-exit fixture needed emergency rescue');previousGround=!!r[8];
 }
 assert(exits>0,'course did not execute original air exit');
 if(spin||flip)assert(maxRotationChange>.05,'flip presentation was discarded on landing');

 results.push({spin,flip,landingReversals,exits,maxRotationChange,maxPivotShift,reversals:new Float32Array(c.HEAPF32.buffer,c._rider_stance_info(),9)[8]});
 c._reset_animation();assert.equal(new Float32Array(c.HEAPF32.buffer,c._air_exit_info(),19)[0],0);
}

console.log('Original posed-contact air exit bakes presentation once on touchdown, for clean straight/spin landings:',results);

c._reset_rider(...start.position,start.heading);c._reset_animation();let heldLanded=false;
for(let tick=0;tick<1800;tick++){
 let r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,1,0,0),16).slice();
 c._animation_tick(r[7],0,0,r[9],r[8],1,0,0,0,4,r[15],0);
 r=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16).slice();
 assert.equal(new Float32Array(c.HEAPF32.buffer,c._air_exit_info(),19)[0],0,'held-ledge control2 incorrectly runs control5 exit');
 if(r[11]){heldLanded=true;break;}
}
assert(heldLanded,'held-ledge fixture did not land');
console.log('Held-ledge touchdown retains control2 without baking a released-air pose.');
