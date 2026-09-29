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


process.on('uncaughtException',e=>{console.error(e instanceof Error?e:c.getExceptionMessage(e));process.exit(1);});

const grid=JSON.parse(fs.readFileSync('../local/assets/native/ARA1/event-start.json')).participants.find(p=>p.race.human).race.position;
// The 74 type-16 node entities (course-script flags without 0x20/0x40) and the 5 endmode colliders (authored flags 0,
// countdown runtime flags 2) skip every collector from load.
assert.deepEqual(Array.from(new Int32Array(c.HEAPU8.buffer,c._world_collision_info(),11)).slice(8),[79,79,79]);
for(const lean of [0,1]){
c._start_event();assert.deepEqual(Array.from(new Int32Array(c.HEAPU8.buffer,c._world_collision_info(),11)).slice(8),[160,160,160],'event dead nodes, type-16 nodes and endmode colliders must skip body and both ray modes');let traveled=0,released=-1,normal=-1,firstRace=-1;
for(let tick=0;tick<420;tick++){
 c._start_input(lean);c._race_begin();const r=f(c._step_rider(0,0,0,0),16);const pose=c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,0,r[15],0);assert(f(pose,26*7).every(Number.isFinite));const race=f(c._race_end(),8),start=f(c._start_info(),7);
 traveled=Math.max(traveled,Math.hypot(r[0]-grid[0]/100,r[2]+grid[1]/100));
 if(start[1]){assert(Math.hypot(r[0]-grid[0]/100,r[1]-grid[2]/100,r[2]+grid[1]/100)<.002,'grid rider moved before release');assert.equal(r[7],0);}
 if(race[5]===4){assert.equal(race[0],0);assert(Math.abs(race[1])<.001,'grid progress does not start at zero');assert.equal(start[1],1,'countdown released rider early');}
 if(race[5]===5&&firstRace<0)firstRace=tick;
 if(!start[1]&&released<0)released=tick;
 if(!start[0]&&normal<0)normal=tick;
}
console.log('Original event start',{lean,firstRace,released,normal,traveled});assert(traveled>10,'rider failed to leave the starting area');assert.equal(firstRace,180);assert(released>=180&&released<240);assert(normal>released&&normal<280);
c._start_event();assert.equal(f(c._start_info(),7)[6],0,'restart retains elapsed race time');assert.equal(f(c._start_info(),7)[1],1);

}
const full=c._original_axis(1);
c._start_event();
let began=-1,origin,sample,leftSnow=false,crash=0;
for(let tick=0;tick<1200;tick++){
 const started=began>=0;
 const charging=started&&tick>=began+40&&tick<began+90;
 const tuck=started&&tick>=began&&tick<began+160;
 const rising=charging&&tick===began+40;
 c._ride_command(0,charging?1:tuck?full:0,0,0,0,0,charging?1:0,0,0,rising?0:1);
 c._race_begin();
 const state=f(c._step_rider(0,charging?1:0,0,0),16);
 c._animation_tick(state[7],0,0,state[9],state[8],charging?1:0,0,0,0,0,state[15],0);
 const after=f(c._rider_state(),16),info=f(c._animation_info(),19),start=f(c._start_info(),7);
 crash=Math.max(crash,f(c._crash_info(),1)[0]);
 c._race_end();
 if(began<0&&start[0]===0&&start[1]===0){began=tick;origin=after.slice(0,3);}
 if(began>=0&&after[8]===0)leftSnow=true;
 if(began>=0&&tick===began+200){sample=after;break;}
}
const moved=sample.slice(0,3).map((v,i)=>v-origin[i]);
assert(began>0&&sample,'event tuck never left the grid');
assert.equal(sample[8],1,'tuck-then-jump must be back on snow');
assert.equal(crash,0,'tuck-then-jump must not wipe out');
assert(leftSnow,'tuck-then-jump never left the snow');
assert(moved[1]<-5&&Math.hypot(moved[0],moved[2])>20,`tuck-then-jump must move downhill ${moved}`);
console.log('Event tuck-then-jump',{began,moved,speed:sample[7]});
