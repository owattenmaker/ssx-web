// Career attributes -> original stat getters (web/rider_attributes.hpp, web/attribute_bridge.cpp).
// Defaults must stay bit-identical to the captured level-1 riders; bought levels must change the physics.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import {originalAttributeBytes} from './career.js';

const root='public/assets/',read=p=>JSON.parse(fs.readFileSync(root+p));
async function boot(){
 const c=await createCore();
 const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;},str=x=>put(new TextEncoder().encode((typeof x==='string'?x:JSON.stringify(x))+'\0'));
 const metadata=str(read('ANIMATIONS/animation-packets.json')),rig=str(read('RIDER_SAM/rider.json')),settings=str(read('ANIMATIONS/initial.json'));
 const raw=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin'),packets=put(raw);
 c._init_animation(metadata,rig,settings,packets,raw.length);
 const mesh=fs.readFileSync(root+'ARA1/collision.bin'),mp=put(mesh);c._init_world(mp,mesh.length/4);
 c._init_terrain(str(read('ARA1/terrain.json')));
 c._init_world_collision(str(read('ARA1/world_collision.json')),str(read('ARA1/terrain.json').source_sha256));
 c._animation_use_physics(1);c._init_body_terrain(str(read('ARA1/terrain.json')));c._init_race(str(read('ANIMATIONS/initial.json')));
 process.on('uncaughtException',e=>{console.error(e instanceof Error?e:c.getExceptionMessage(e));process.exit(1);});
 return c;
}
const f=(c,ptr,n)=>new Float32Array(c.HEAPF32.buffer,ptr,n).slice();
const bits=x=>new Uint32Array(new Float32Array([x]).buffer)[0];
function setAttributes(c,raw){const p=c._malloc(28);new Int32Array(c.HEAPU8.buffer,p,7).set(raw);c._set_rider_attributes(p,0);c._free(p);}
// Event start, then a straight tuck down Snow Jam: returns per-tick [x,y,z,speed] and the peak speed.
function ride(c,ticks=900){
 c._start_event();const full=c._original_axis(1);const trace=[];let peak=0;
 for(let tick=0;tick<ticks;tick++){
  c._ride_command(0,full,0,0,0,0,0,0,0,1);c._race_begin();
  const s=f(c,c._step_rider(0,0,0,0),16);c._animation_tick(s[7],0,0,s[9],s[8],0,0,0,0,0,s[15],0);c._race_end();
  trace.push([s[0],s[1],s[2],s[7]]);peak=Math.max(peak,s[7]);
 }
 return {trace,peak,end:trace.at(-1)};
}

const base=await boot();
let stats=f(base,base._rider_attribute_stats(),12);
assert.equal(stats[11],0,'attributes are untouched until the career sets them');
for(let k=0;k<11;k++)assert.equal(bits(stats[k]),0x3dba2e8c,`level-1 stat ${k} must be the captured 1/11 (0x3DBA2E8C)`);
const unset=ride(base);

// Explicit level-1 attributes: every stat field keeps its seeded bits, so the run is bit-identical.
const same=await boot();setAttributes(same,[5,5,5,5,5,5,5]);
stats=f(same,same._rider_attribute_stats(),12);assert.equal(stats[11],1);for(let k=0;k<11;k++)assert.equal(bits(stats[k]),0x3dba2e8c);
const level1=ride(same);
assert.deepEqual(level1.trace.map(r=>r.map(bits)),unset.trace.map(r=>r.map(bits)),'setting level-1 attributes changed the physics');

// Maxed speed/acceleration (raw 55 -> 11/11): 0x11B3F8 raises the speed limit, the rider goes faster and further.
const fast=await boot();setAttributes(fast,[55,55,5,5,5,5,5]);
stats=f(fast,fast._rider_attribute_stats(),12);assert.equal(stats[0],1);assert.equal(stats[1],1);assert.equal(stats[7],1);assert.equal(stats[8],1);
const quick=ride(fast);
const dist=r=>Math.hypot(r.end[0]-r.trace[0][0],r.end[2]-r.trace[0][2]);
console.log('Rider attributes: level-1 peak',unset.peak.toFixed(2),'m/s dist',dist(unset).toFixed(1),'| speed+accel 11.0 peak',quick.peak.toFixed(2),'dist',dist(quick).toFixed(1));
assert(quick.peak>unset.peak,'top speed attribute did not raise the speed limit');
assert(dist(quick)>dist(unset),'faster rider did not travel further');

// Stability 11.0 reaches the landing / rail-balance stat (0x149120 / 0x149208).
const steady=await boot();setAttributes(steady,[5,5,5,5,55,5,55]);stats=f(steady,steady._rider_attribute_stats(),12);
assert.equal(stats[4],1);assert.equal(stats[6],1);assert.equal(stats[10],1,'landing stat');assert.equal(bits(stats[7]),0x3dba2e8c,'top speed untouched');

// Career bytes: UI rows (Acceleration, Edging, Speed, Spin, Stability, Toughness, Tricks) -> original order.
assert.deepEqual(originalAttributeBytes([10,15,20,25,30,35,40]),[20,10,40,15,25,35,30]);
console.log('Rider attributes: defaults bit-exact, bought levels reach the original stat getters');
