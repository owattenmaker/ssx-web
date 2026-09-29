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

c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();c._set_rider_velocity(0,0,0);
const fx=read('SNOW_FX/snow-fx.json'),profile=fx.profiles.find(p=>p.emitter_index===4),texture=fx.textures.find(t=>t.id===profile.parameters.TextureId);
assert(texture,'breath texture is absent from the renderer manifest');assert.equal(texture.name,'brth');
assert.equal(fs.statSync(root+'SNOW_FX/'+texture.gs_alpha_file).size,texture.width*texture.height*4);
let peak=0,slowTicks=0;
for(let tick=0;tick<360;tick++){
 c._race_begin();let r=f(c._step_rider(0,0,1,0),16);c._animation_tick(r[7],0,1,r[9],r[8],0,0,0,0,0,r[15],0);
 const posed=f(c._pose_physical(),12);c._step_camera_head(...posed.slice(9,12));c._race_end();r=f(c._rider_state(),16);
 const snow=f(c._snow_info(),23),count=snow[4];peak=Math.max(peak,count);if(r[7]<11.11111)slowTicks++;
 const particles=f(c._snow_particles(4),count*8);assert(particles.every(Number.isFinite));
 for(let i=0;i<count;i++){
  //Breath must remain near the rider; accidentally retaining matrix world
  //translation in the emission velocity launches it thousands of metres away.
  const native=[particles[i*8]/100,particles[i*8+2]/100,-particles[i*8+1]/100];
  assert(Math.hypot(...native.map((v,k)=>v-r[k]))<8,'breath particles escape rider vicinity');
 }
 assert.equal(r[13],0,'breath fixture required an emergency reset');
}
assert(slowTicks>60,'fixture did not sustain the original low-speed emission gate');assert(peak>0,'original breath never reached the particle buffers');
console.log('Original breath in gameplay:',{peak,slowTicks});
