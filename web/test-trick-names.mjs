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
const cfg=read('ANIMATIONS/initial.json'),raceConfig=str(cfg);c._init_race(raceConfig);c._free(raceConfig);
const f=(ptr,n)=>new Float32Array(c.HEAPF32.buffer,ptr,n).slice();


process.on('uncaughtException',e=>{console.error(e instanceof Error?e:c.getExceptionMessage(e));process.exit(1);});
const decoder=new TextDecoder(),name=()=>{const p=c._trick_name();return decoder.decode(c.HEAPU8.subarray(p,c.HEAPU8.indexOf(0,p)));};
const runs=[];
for(const [spin,flip] of [[1,0],[0,1],[1,1]]){
 c._reset_rider(...start.position,start.heading);c._reset_animation();c._reset_race();const names=new Set();let maxFlip=0,maxSpin=0;
 for(let tick=0;tick<250;tick++){
  const jump=+(tick>=90&&tick<150),turn=tick>=90&&tick<210?spin:0,rotation=tick>=90&&tick<210?flip:0;
  if(tick===149&&flip)c._award_boost_pickup(2,5);
  c._race_begin();const r=f(c._step_rider(turn,jump,0,0),16);c._animation_tick(r[7],turn,0,r[9],r[8],jump,0,0,0,0,r[15],rotation);c._race_end();
  const before=f(c._animation_info(),19),label=name();maxFlip=Math.max(maxFlip,Math.abs(before[11]));maxSpin=Math.max(maxSpin,Math.abs(before[10]));assert.deepEqual(f(c._animation_info(),19),before,'formatting mutates gameplay scoring');assert.equal(name(),label);
  if(label)names.add(label.trim());
 }
 runs.push([...names]);console.log({spin,flip,maxSpin,maxFlip,names:[...names]});
}
assert(runs[0].some(s=>/\b(180|360|540|720|900|1080)\b/.test(s)),'original spin degrees missing from gameplay names');
assert(runs[1].some(s=>/Flip|Rodeo|Misty/.test(s)),'original flip naming missing');
