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

c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();
let peak=0,airborneEmissionFrames=0;
for(let tick=0;tick<300;tick++){
 const jump=+(tick>=90&&tick<150);c._race_begin();const r=f(c._step_rider(0,jump,0,0),16);c._animation_tick(r[7],0,0,r[9],r[8],jump,0,0,0,0,r[15],0);c._race_end();
 const info=f(c._snow_info(),23);peak=Math.max(peak,info[8]);assert(f(c._snow_particles(8),info[8]*8).every(Number.isFinite));
 if(!r[8]&&info[8]>0)airborneEmissionFrames++;
}
assert(peak>0&&airborneEmissionFrames>0,'Original kicker snow did not follow the rider into airtime');
console.log('Original kicker in gameplay:',{peak,airborneEmissionFrames});

// pv sprayReset (web/animation_bridge.cpp set_fx_reset_kicker / rider_fx_reset): the DynamicSpray reset 2DF3B0 (constructor, and the
// rider FX reset 111890 of every placement) writes the buildup FX+0x10 = 0. PS2 menus/fr/ctmstart: 0 through the plane drop's fall
// (f02700 / f02747 / f02793), so no kicker snow rises from the falling rider; the port's runs start from the seed's 1.37.
{
 const ps2='../local/ps2-capture/menus/fr/ctmstart.f02';
 if(fs.existsSync(ps2+'700.p2s')){
  const py=`import sys,struct,zipfile\nsys.path.insert(0,'../tools')\nfrom locations import human_rider\nout=[]\nfor f in ('700','730','760'):\n m=zipfile.ZipFile('${ps2}'+f+'.p2s').read('eeMemory.bin');r=human_rider(m);fx=struct.unpack_from('<I',m,r+0x77c)[0]+0xb40\n out.append(struct.unpack_from('<f',m,fx+0x10)[0])\nprint(out)`;
  const {execFileSync}=await import('node:child_process');const buildup=JSON.parse(execFileSync('python3',['-c',py],{encoding:'utf8'}));
  assert.deepEqual(buildup,[0,0,0],'PS2 plane drop: kicker buildup FX+0x10 = 0 through the fall');
 }else console.log('skip PS2 ctmstart buildup: local capture not present');
 const drop=(on,reset)=>{c._set_fx_reset_kicker(on?1:0);const s=f(c._rider_state(),16);c._reset_animation();c._reset_rider(s[0],s[1]+40,s[2],0.3);c._set_rider_velocity(0,0,-253);if(reset)c._rider_fx_reset();
  let frames=0;for(let tick=0;tick<60;tick++){c._race_begin();const r=f(c._step_rider(0,0,0,0),16);c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);c._race_end();if(!r[8]&&f(c._snow_info(),23)[8]>0)frames++;}return frames;};
 const before=drop(false,false),after=drop(true,true);c._set_fx_reset_kicker(0);
 assert(before>0&&after===0,`sprayReset: a rider placed in the air sprays no kicker snow (${before} -> ${after} frames)`);
 console.log('Kicker at a placement (sprayReset):',{before,after});
}

