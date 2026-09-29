import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),root='public/assets/';
const read=p=>JSON.parse(fs.readFileSync(root+p));
const rig=read('RIDER_SAM/rider.json'),metadata=read('ANIMATIONS/animation-packets.json'),settings=read('ANIMATIONS/initial.json');
const raw=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin');
const allocations=[];
const put=bytes=>{const p=c._malloc(bytes.length);c.HEAPU8.set(bytes,p);allocations.push(p);return p;};
const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const mp=str(metadata),rp=str(rig),packets=put(raw);
const masks=[1,2,4,8,3,5,9,6,10,12,7,11,13,14,15];
// Explicit captured-state fixtures for animation conformance, not injected
// meter in gameplay. Production uses the physics-owned earned state.
for(const tier of [1,5,10]){
 const config=structuredClone(settings);config.original_boost.state.super_time=20;config.original_boost.state.tier=tier;
 c._init_animation(mp,rp,str(config),packets,raw.length);
 c._animation_use_physics(0);
 const definitions=config.original_grab_control.profile;
 for(let index=0;index<15;index++){
  c._reset_animation();
  const definition=definitions.uber[tier>=5?1:0][index];
  const target=definition.semantic===438?definitions.tweak[index].semantic:definition.semantic;
  let entered=false;
  for(let tick=0;tick<300;tick++){
   const pose=c._animation_tick(18,0,0,0,+(tick<20),0,tick>=60&&tick<260?masks[index]:0,+(tick>=130&&tick<260),0,5,0,0);
   assert(new Float32Array(c.HEAPF32.buffer,pose,rig.bones.length*7).every(Number.isFinite));
   const info=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16);
   entered ||= info[0]===target;
  }
  assert(entered,`tier ${tier}, chord ${masks[index]} never entered original semantic ${target}`);
 }
}
for(const p of allocations)c._free(p);
console.log('All 15 original advanced shoulder chords enter their authored animations at tiers 1, 5 and 10; poses remain finite.');
