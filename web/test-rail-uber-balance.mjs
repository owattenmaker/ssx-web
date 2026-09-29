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


for(let family=0;family<4;family++)for(const amount of [-1,-.75,-.25,0,.25,.75,1]){
 const semantic=218+family*8,bank=amount<0?353+family*9:352+family*9;
 const a=f(c._rail_uber_balance_probe(semantic,amount,0),7),b=f(c._rail_uber_balance_probe(semantic,amount,1),7);
 assert.equal(a[0],bank<<8);assert.equal(a[6],a[0]);assert.equal(a[5],a[1],'balance pose advanced with time rather than seeking by magnitude');
 assert(Math.abs(a[1]-Math.abs(amount)*a[2])<.000001,'balance magnitude did not select original frame');
 assert.equal(a[4],1);assert.deepEqual(a,b,'switch stance incorrectly reverses Uber balance poses');
}
console.log('All four original Uber grind balance drivers enter and seek correctly in WASM');
