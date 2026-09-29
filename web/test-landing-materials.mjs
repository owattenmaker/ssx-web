import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),root='public/assets/';
const bytes=fs.readFileSync(root+'ARA1/collision.bin'),p=c._malloc(bytes.length);c.HEAPU8.set(bytes,p);c._init_world(p,bytes.length/4);c._free(p);
const originalTerrain=JSON.parse(fs.readFileSync(root+'ARA1/terrain.json'));
const settings=JSON.parse(fs.readFileSync(root+'ANIMATIONS/initial.json'));
const start=JSON.parse(fs.readFileSync(root+'ARA1/start.json'));
const results=[];
for(const surface of [0,2]){
 // Test-only material substitution on the same imported geometry; source
 // files and the production course's authored materials remain untouched.
 const terrain=structuredClone(originalTerrain);for(const patch of terrain.patches)patch.authored_surface_id=surface;
 const data=new TextEncoder().encode(JSON.stringify(terrain)+'\0'),ptr=c._malloc(data.length);c.HEAPU8.set(data,ptr);c._init_terrain(ptr);c._free(ptr);
 c._reset_rider(...start.position,start.heading);let landed=false;
 for(let tick=0;tick<600;tick++){
  const rider=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,+(tick>90&&tick<150),0,0),16);
  if(!rider[11])continue;
  assert.equal(new Float32Array(c.HEAPF32.buffer,c._physics_info(),6)[1],surface);
  const response=new Float32Array(c.HEAPF32.buffer,c._landing_info(),6).slice();
  const material=settings.original_landing.profile.materials[surface];
  const expected=Math.max(response[4]*(1-material.normal_impulse_factor),-material.maximum_normal_speed);
  assert(Math.abs(response[5]-expected)<.01,'landing bypasses the source material impulse/clamp');
  if(surface===2)assert(response[5]<-.001,`soft material response at ${tick}: ${response[4]} -> ${response[5]}`);
  const next=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,0,0,0),16);assert(next[8],'fixture must stay grounded after touchdown');
  const ground=new Float32Array(c.HEAPF32.buffer,c._ground_contact_info(),5).slice();
  assert.equal(ground[4],surface);assert(Math.abs(ground[1]-ground[0])<.0001,'ground contact loses speed by projecting velocity');
  if(surface===2)assert(Math.abs(ground[3]-ground[2])<.001,'soft ground incorrectly applies hard-surface direction correction');
  results.push({surface,tick,before:response[4],after:response[5],groundSpeedBefore:ground[0],groundSpeedAfter:ground[1]});landed=true;break;
 }
 assert(landed,`material ${surface} fixture never landed`);
}
console.log('Original hard/soft landing velocity responses:',results);
