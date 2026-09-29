import fs from 'node:fs';import assert from 'node:assert/strict';import createCore from './runtime/core.js';
const riderPackage=process.argv.includes('--zoe')?'RIDER_ZOE':'RIDER_SAM';
const c=await createCore();process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else{try{console.error(c.getExceptionMessage(e));}catch{console.error(e);}}process.exit(1);});const root='public/assets/';const read=p=>JSON.parse(fs.readFileSync(root+p));const str=x=>{const d=new TextEncoder().encode(JSON.stringify(x)+'\0'),p=c._malloc(d.length);c.HEAPU8.set(d,p);return p;};
const meta=read('ANIMATIONS/animation-packets.json'),rig=read(riderPackage+'/rider.json'),cfg=read('ANIMATIONS/initial.json'),raw=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin');const packet=c._malloc(raw.length);c.HEAPU8.set(raw,packet);c._init_animation(str(meta),str(rig),str(cfg),packet,raw.length);console.log('loaded',meta.clips.length,'clips',meta.state_properties.length,'properties');
for(let i=0;i<240;i++){const p=c._animation_tick(15,Math.sin(i/60),0,i<60?0:1,i<100?1:0,i>=60&&i<100?1:0,i>130&&i<200?1:0,0,0,3);const pose=new Float32Array(c.HEAPF32.buffer,p,rig.bones.length*7);assert(pose.every(Number.isFinite));if(i%30===0)console.log('animation',i,Array.from(new Float32Array(c.HEAPF32.buffer,c._animation_info(),6)));}
const b=fs.readFileSync(root+'ARA1/collision.bin'),v=new Float32Array(b.buffer,b.byteOffset,b.byteLength/4),mem=c._malloc(b.length);c.HEAPF32.set(v,mem/4);c._init_world(mem,v.length);const start=read('ARA1/start.json');c._reset_rider(...start.position,start.heading);let peak=0,landings=0;
for(let i=0;i<600;i++){const s=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,+(i>90&&i<150),0,0),16);assert(s.every(Number.isFinite));assert(s[7]<=new Float32Array(c.HEAPF32.buffer,c._physics_info(),6)[5]+.002);peak=Math.max(peak,s[7]);landings+=s[11];if(i%60===0)console.log('physics',i/60,Array.from(s.slice(0,11)));}
console.log({peak,landings});assert(peak<=33.334);
const masks=[1,2,4,8,3,5,9,6,10,12,7,11,13,14,15];
for(const mask of masks){c._reset_animation();let entered=false;for(let i=0;i<200;i++){const p=c._animation_tick(18,0,0,0,i<20?1:0,0,i>=60&&i<150?mask:0,0,0,3,0,0);assert(new Float32Array(c.HEAPF32.buffer,p,rig.bones.length*7).every(Number.isFinite));const info=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16);entered||=info[1]===18;}assert(entered,'grab '+mask+' did not enter its original animation');}
console.log('All 15 original shoulder-button grab chords entered and sampled.');
for(const [spin,flip] of [[-1,0],[1,0],[0,-1],[0,1],[1,1],[-1,-1]]){c._reset_animation();for(let i=0;i<240;i++){const p=c._animation_tick(18,spin,0,i<60?1:0,i<60?1:0,i<60?1:0,0,0,0,3,0,flip);assert(new Float32Array(c.HEAPF32.buffer,p,rig.bones.length*7).every(Number.isFinite));}console.log('prewind/air direction',spin,flip,'passed');}
assert.equal(c._animation_audit(),497);console.log('All 497 gameplay clips sampled at start/middle/end with finite transforms:',riderPackage);
for(const mask of masks){c._reset_animation();let tweaked=false;for(let i=0;i<260;i++){const p=c._animation_tick(18,0,0,0,i<20?1:0,0,i>=60&&i<220?mask:0,i>=130&&i<220?1:0,0,5,0,0);assert(new Float32Array(c.HEAPF32.buffer,p,rig.bones.length*7).every(Number.isFinite));const info=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16);tweaked||=info[1]===19;}assert(tweaked,'tweak chord '+mask+' failed');}console.log('All 15 original tweak triggers passed.');
const results=[];for(const fps of [30,60,120,144,240]){c._reset_rider(...start.position,start.heading);let accumulator=0,ticks=0,last;for(let frame=0;frame<fps*10;frame++){accumulator+=1/fps;while(accumulator+1e-10>=1/60){last=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,+(ticks>90&&ticks<150),0,0),16).slice();ticks++;accumulator-=1/60;}}assert.equal(ticks,600);results.push(Array.from(last));}for(const result of results)assert.deepEqual(result,results[0]);console.log('30/60/120/144/240 render FPS: identical 600-tick movement and jump results.');
c._reset_animation();for(let i=0;i<90;i++)c._animation_tick(18,0,0,1,1,1,0,0,0,4,0,0);const heldPrewindSemantic=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16)[0];for(let i=0;i<90;i++){c._animation_tick(18,0,0,0,0,1,0,0,0,4,0,0);const state=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16);assert.equal(state[15],2);assert.equal(state[0],heldPrewindSemantic);}for(let i=0;i<60;i++)c._animation_tick(18,0,0,0,0,0,0,0,0,4,0,0);assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_info(),16)[15],5);console.log('Held-jump ledge departure and release: original held control 2 -> 5 transitions passed.');
c._init_terrain(str(read('ARA1/terrain.json')));c._reset_rider(...start.position,start.heading);for(let i=0;i<600;i++){c._step_rider(0,+(i>100&&i<160),0,0);const view=new Float32Array(c.HEAPF32.buffer,c._step_camera(0,.95,0),9);assert(view.every(Number.isFinite));assert(view[6]>.7&&view[6]<.9);}console.log('Original chase-camera fixed-tick run: finite output and original FOV.');
c._init_race(str(cfg));c._reset_rider(...start.position,start.heading);let raceState;for(let i=0;i<120;i++){c._race_begin();c._step_rider(0,0,0,0);raceState=new Float32Array(c.HEAPF32.buffer,c._race_end(),8);}assert(Math.abs(raceState[0]-(158+120)/60)<.02);assert(raceState[1]>=0&&raceState[1]<100);console.log('Original race clock/path projection: elapsed ticks and course progress verified.');
c._animation_use_physics(1);c._reset_animation();c._reset_rider(...start.position,start.heading);c._reset_race();for(let i=0;i<600;i++){c._race_begin();const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,+(i>90&&i<150),0,0),16);const floor=c._height_at(r[0],r[1]+.15,r[2]),gap=Math.max(0,r[1]-floor),estimate=(r[14]+Math.sqrt(r[14]*r[14]+38*gap))/19;const pp=c._animation_tick(r[7],0,0,r[9],r[8],+(i>90&&i<150),i>160&&i<210?1:0,0,0,estimate,r[15],0),pose=new Float32Array(c.HEAPF32.buffer,pp,rig.bones.length*7);assert(pose.every(Number.isFinite));const camera=new Float32Array(c.HEAPF32.buffer,c._step_camera(pose[0]/100,pose[2]/100,-pose[1]/100),9);assert(camera.every(Number.isFinite));assert(new Float32Array(c.HEAPF32.buffer,c._race_end(),8).every(Number.isFinite));}console.log('Combined original physics + filtered animation inputs + procedural pose + camera + race pipeline: 600 ticks passed.');
// Isolate passive-air filter ownership from movement: once air owns the
// controller, an unchanged ground snapshot must not reset its filter each tick.
c._reset_rider(...start.position,start.heading);c._reset_animation();
for(let i=0;i<24;i++)c._step_rider(-1,0,0,0);
c._animation_tick(18,0,0,0,1,0,0,0,0,4,0,0);
const passiveTurns=[];
for(let i=0;i<40;i++){
 c._animation_tick(18,0,0,0,0,0,0,0,0,4,0,0);
 passiveTurns.push(new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),10)[6]);
}
assert(Math.abs(passiveTurns[0])>.001,'fixture must enter air with nonzero filtered turn');
assert(Math.abs(passiveTurns.at(-1))<.00001,'passive turn is repeatedly overwritten by the old ground state');
console.log('Attached passive-air turn filter converges without stale ground overwrites.');

c._animation_use_physics(0);c._reset_animation();
let blendedAxis=false;
for(let tick=0;tick<240;tick++){
 c._animation_tick(18,1,0,+(tick<60),+(tick<60),+(tick<60),0,0,0,5,0,1);
 const input=new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),11);
 assert(Number.isFinite(input[10]));
 if(tick>=60)blendedAxis ||= input[10]>.001;
}
assert(blendedAxis,'combined spin/flip never advances original presentation axis blend');
const settled=new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),11)[10];
for(let i=0;i<20;i++)assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),11)[10],settled,'reading animation inputs advances presentation');
console.log('Combined spin/flip advances original airborne axis blend; diagnostic reads do not advance it.');

// Source13A2E0 preserves held crouch/prewind across soft touchdown.
c._animation_use_physics(0);c._reset_animation();
for(let i=0;i<30;i++)c._animation_tick(18,0,0,1,1,1,0,0,0,3,0,0);
for(let i=0;i<10;i++)c._animation_tick(18,0,0,0,0,1,0,0,0,3,0,0);
const beforeHeldLanding=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16)[0];
c._animation_tick(18,0,0,1,1,1,0,0,0,0,-100,0);
let heldLandingInfo=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16);
assert.equal(heldLandingInfo[15],2);assert.equal(heldLandingInfo[0],beforeHeldLanding,'soft held landing incorrectly forced a landing clip');
console.log('Soft held-jump landing preserves original crouch/prewind animation.');

for(const heldLedge of [false,true])for(const wind of [0,1]){
 c._animation_use_physics(0);c._reset_animation();
 for(let tick=0;tick<90;tick++)c._animation_tick(18,wind,0,1,1,1,0,0,0,4,0,wind);
 if(heldLedge)for(let tick=0;tick<20;tick++)c._animation_tick(18,wind,0,0,0,1,0,0,0,4,0,wind);
 c._animation_tick(18,wind,0,0,0,0,0,0,0,4,0,wind);
 const rate=new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),12)[11];
 assert(Number.isFinite(rate)&&rate>0);
 if(wind)assert(Math.abs(rate-1)>.001,'prewound release still hardcodes unit playback rate');
 else assert.equal(rate,1,'straight release must retain original unit rate');
}
console.log('Ground and midair release use original prewind-dependent playback rates; straight jumps retain rate 1.');

c._animation_use_physics(0);c._reset_animation();
for(let tick=0;tick<5;tick++)c._animation_tick(18,-1,0,1,1,1,0,0,0,5,0,0);
for(let tick=0;tick<1;tick++)c._animation_tick(18,-1,0,0,0,0,0,0,0,5,0,0);
const rawLandingSpin=new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),14)[12];
c._animation_tick(18,0,0,0,1,0,0,0,0,0,-100,0);
const landingSpin=new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),14)[13];
const sourceRateBits=[0,0x3fc90fdc,0x4096cbe5,0x40fb53d3];
const sourceRates=sourceRateBits.map(bits=>new Float32Array(new Uint32Array([bits]).buffer)[0]);
assert(sourceRates.some(rate=>Math.abs(landingSpin)===rate),'landing spin bypassed original quantization');
assert.notEqual(landingSpin,rawLandingSpin,'fixture must exercise the air-exit rate conversion');
const landingSemantic=new Float32Array(c.HEAPF32.buffer,c._animation_info(),16)[0];
assert.equal(landingSemantic,landingSpin < -Math.PI?66:landingSpin > Math.PI?67:61);
assert.notEqual(landingSemantic,rawLandingSpin < -Math.PI?66:rawLandingSpin > Math.PI?67:61,`fixture must expose the wrong clip chosen from raw spin: ${rawLandingSpin} -> ${landingSpin}`);
console.log('Landing clip selection uses original quantized air-exit spin:',{rawLandingSpin,landingSpin,landingSemantic});

c._animation_use_physics(0);c._reset_animation();let pivotOffsets=0;
for(let tick=0;tick<200;tick++){
 c._animation_tick(18,1,0,+(tick<60),+(tick<60),+(tick<60),0,0,0,4,0,1);
 const data=new Float32Array(c.HEAPF32.buffer,c._animation_info(),19),inputs=new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),17);
 assert(data.every(Number.isFinite));
 if(tick<60)continue;
 const p=Array.from(inputs.slice(14,17)),q=Array.from(data.slice(6,10));
 const cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
 const v=q.slice(0,3),uv=cross(v,p),uuv=cross(v,uv);
 for(let k=0;k<3;k++)assert(Math.abs(2*q[3]*uv[k]+2*uuv[k]+data[16+k])<.001,'original air presentation does not preserve its authored pivot');
 pivotOffsets+=Math.hypot(...data.slice(16,19))>1;
}
assert(pivotOffsets>20,'air rotations still use a zero pivot');
console.log('Original airborne presentation rotates about the authored pivot and exports its positional offset.');
