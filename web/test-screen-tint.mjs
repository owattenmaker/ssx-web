import fs from 'node:fs';
import assert from 'node:assert/strict';
import {createScreenTint} from './screen-tint.js';
// Original tWPIGD_ScreenTint state from owned PCSX2 snapshots (instance at
// *(*(0x4FA370+0xC)), vtable 484B70): driver X/Y, current scale and last sample.
const tint=JSON.parse(fs.readFileSync('public/assets/ARA1/screen-tint.json'));
const snapshots=[
 {name:'countdown-1',x:-131865.234375,y:13858.5,sample:[1.1,.85,1]},
 {name:'glide',x:-134845.671875,y:13353.677734375,sample:[.9,.85,1.15]},
 {name:'jump-60',x:-135739.53125,y:13188.7822265625,sample:[.9,.85,1.15]},
 {name:'glide-120',x:-138274.546875,y:12746.0927734375,sample:[1,1,1]},
 {name:'600',x:-141435.453125,y:12299.552734375,sample:[1,1,1]},
];
const f=Math.fround,info=new Float32Array(16),core={HEAPF32:info,_fog_info:()=>0};
let ticks=0;
const step=(state,x,y)=>{info[7]=++ticks;info[8]=x;info[9]=y;info[10]=1;state.step(core);return state.state;};
for(const s of snapshots){
 const state=await createScreenTint(tint);ticks=0;const result=step(state,s.x,s.y);
 assert.deepEqual(result.current.slice(0,3),s.sample.map(f),`${s.name}: initial application selects the captured payload`);
 assert.equal(result.enabled,s.sample.every(v=>v===1)?0:1);
}
// Captured glide (tick 338) instance, then one driver step at the captured
// glide+1 camera X/Y must reproduce the glide+1 snapshot (weight 0.16 squared).
const state=await createScreenTint(tint);
state.seed({current:[0.9766084551811218,.85,1.092537522315979,0,0,0],distance:3022.961181640625,x:-134845.671875,y:13353.677734375,ticks:1});ticks=1;
const inside=step(state,-134874.640625,13348.283203125);assert.equal(inside.selected,6);
for(const [i,v] of [[0,0.9746472835540771],[1,.85],[2,1.094008445739746]])assert(Math.abs(inside.current[i]-v)<2e-7,`glide+1 channel ${i}: ${inside.current[i]} vs ${v}`);
assert(Math.abs(inside.distance-3052.427734375)<1e-3,`glide+1 travel ${inside.distance}`);
// Encoded pass bytes: trunc(scale*127.5) as in 3905E8.
assert.deepEqual(inside.scaleBytes,inside.current.slice(0,3).map(v=>Math.trunc(f(v*127.5))));
console.log('ScreenTint tree/driver matches',snapshots.length,'original snapshot payloads and the captured glide transition.');
