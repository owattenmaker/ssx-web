import {Quaternion,Vector3} from 'three';
import fs from 'node:fs';
import createCore from './runtime/core.js';
const railCatalog=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/rails.json',import.meta.url))),railSegment=railCatalog.rails.find(r=>r.name==='spline_ARA1_RAIL_3007').segments[0].native;
const railDelta=railSegment.end.map((v,i)=>v-railSegment.start[i]),railLength=Math.hypot(railDelta[0],railDelta[2]),railDirection=railDelta.map(v=>v/railLength);
const railFixture={position:railSegment.start.map((v,i)=>v-railDirection[i]*3.5),heading:Math.atan2(railDirection[0],railDirection[2]),velocity:[railDirection[0]*900,-railDirection[2]*900,0]};
const core=await createCore();
process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else console.error(core.getExceptionMessage(e));process.exit(1);});
const root=new URL('public/assets/',import.meta.url),read=p=>fs.readFileSync(new URL(p,root)),json=p=>JSON.parse(read(p));
const put=bytes=>{const p=core._malloc(bytes.length);core.HEAPU8.set(bytes,p);return p;};
const str=path=>put(Buffer.concat([read(path),Buffer.from([0])]));
const meta=str('ANIMATIONS/animation-packets.json'),rigPtr=str('RIDER_SAM/rider.json'),cfg=str('ANIMATIONS/initial.json'),packets=read('ANIMATIONS/animation-packets.bin'),packetsPtr=put(packets);
core._init_animation(meta,rigPtr,cfg,packetsPtr,packets.length);core._init_race(cfg);core._animation_use_physics(1);
const mesh=read('ARA1/collision.bin'),mp=put(mesh);core._init_world(mp,mesh.length/4);
const terrain=str('ARA1/terrain.json'),world=str('ARA1/world_collision.json'),hash=put(Buffer.from(json('ARA1/terrain.json').source_sha256+'\0'));
core._init_terrain(terrain);core._init_world_collision(world,hash);core._init_body_terrain(terrain);const rails=str('ARA1/rails.json');core._init_rails(rails,hash);core._free(rails);
for(const p of [meta,rigPtr,cfg,packetsPtr,mp,terrain,world,hash])core._free(p);
const start=json('ARA1/start.json');


for(const fixture of [start,railFixture]){
const release=fixture.velocity?90:150;
for(const prewind of [false,true]){const runs=[];let right;const sign=1;
for(const steer of [-1,0,1]){core._reset_animation();core._reset_race();core._reset_rider(...fixture.position,fixture.heading);if(fixture.velocity)core._set_rider_velocity(...fixture.velocity);let q;
for(let tick=0;tick<release+11;tick++){const jump=+(tick>=release-60&&tick<release),turn=tick>=(prewind?release-10:release)?steer:0;core._race_begin();const state=new Float32Array(core.HEAPF32.buffer,core._step_rider(turn,jump,0,0),16).slice();core._animation_tick(state[7],sign*turn,0,state[9],state[8],jump,0,0,0,0,state[15],0);const p=new Float32Array(core.HEAPF32.buffer,core._pose_physical(),12);const info=new Float32Array(core.HEAPF32.buffer,core._animation_info(),19);q=new Quaternion(p[3],p[5],-p[4],p[6]).multiply(new Quaternion(info[6],info[8],-info[7],info[9]));const cam=new Float32Array(core.HEAPF32.buffer,core._step_camera_head(...p.slice(9,12)),9);core._race_end();if(tick===release+10&&state[8]!==0)throw Error('Air steering fixture must be airborne at measurement');if(tick===release){const launch=new Float32Array(core.HEAPF32.buffer,core._camera_inputs(),14)[13];if(!(launch>300))throw Error('Charged jump did not reach original camera launch ramp');}if(tick===release-1&&steer===0){const dx=cam[3]-cam[0],dz=cam[5]-cam[2],n=Math.hypot(dx,dz);right=[-dz/n,dx/n];}}
const f=new Vector3(0,0,-1).applyQuaternion(q);runs.push([f.x,f.z]);}
const side=f=>(f[0]-runs[1][0])*right[0]+(f[1]-runs[1][1])*right[1];const left=side(runs[0]),rightward=side(runs[2]);console.log({afterRail:!!fixture.velocity,prewind,leftNose:left,rightNose:rightward});if(!(left<-.01&&rightward>.01))throw Error('Airborne rotation reversed relative to chase camera');}

}
