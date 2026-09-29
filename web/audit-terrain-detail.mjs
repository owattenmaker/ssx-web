import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const core=await createCore();
process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else console.error(core.getExceptionMessage(e));process.exit(1);});
const riderPackage=process.argv.includes('--zoe')?'RIDER_ZOE':'RIDER_SAM';
const root=new URL('public/assets/',import.meta.url),read=p=>fs.readFileSync(new URL(p,root)),json=p=>JSON.parse(read(p));
const put=bytes=>{const p=core._malloc(bytes.length);core.HEAPU8.set(bytes,p);return p;};
const str=path=>put(Buffer.concat([read(path),Buffer.from([0])]));
const meta=str('ANIMATIONS/animation-packets.json'),rigPtr=str(riderPackage+'/rider.json'),cfg=str('ANIMATIONS/initial.json'),packets=read('ANIMATIONS/animation-packets.bin'),packetsPtr=put(packets);
core._init_animation(meta,rigPtr,cfg,packetsPtr,packets.length);core._init_race(cfg);core._animation_use_physics(1);
const mesh=read('ARA1/collision.bin'),mp=put(mesh);core._init_world(mp,mesh.length/4);
const terrain=str('ARA1/terrain.json'),world=str('ARA1/world_collision.json'),hash=put(Buffer.from(json('ARA1/terrain.json').source_sha256+'\0'));
core._init_terrain(terrain);core._init_world_collision(world,hash);core._init_body_terrain(terrain);const rails=str('ARA1/rails.json');core._init_rails(rails,hash);core._free(rails);
for(const p of [meta,rigPtr,cfg,packetsPtr,mp,terrain,world,hash])core._free(p);
const poseCount=json(riderPackage+'/rider.json').bones.length*7,start=json('ARA1/start.json');
const fogPackage=str('ARA1/fog-tree.json');core._init_fog(fogPackage);core._free(fogPackage);




const {planTerrainDetail}=await import('./terrain-detail.js');
const patches=json('ARA1/terrain-render.json').patches,sample=(p,n)=>Array.from(new Float32Array(core.HEAPF32.buffer,p,n));
core._reset_animation();core._reset_race();core._reset_rider(...start.position,start.heading);core._start_event();
let lastTime=-Infinity,lastFocus=-1,key='',previous=new Map();const events=[],positions=[];
for(let tick=0;tick<4500;tick++){
 core._start_input(0);core._rail_preinput(0);core._rail_rotation_input(0);core._race_begin();const state=sample(core._step_rider(0,0,0,0),16);core._animation_tick(state[7],0,0,state[9],state[8],0,0,0,0,0,state[15],0);core._race_end();
 const position=sample(core._rider_state(),16).slice(0,3),focus=sample(core._terrain_contact_info(),12)[0],seconds=tick/60;
 if(seconds-lastTime<.1&&focus===lastFocus)continue;lastTime=seconds;lastFocus=focus;positions.push({tick,position,focus});
 const detail=planTerrainDetail(patches,position,{focusResource:focus}),next=JSON.stringify(detail.plan);if(next===key)continue;key=next;
 const current=new Map(detail.plan.map(p=>[p.index,[p.resolution,...p.edges].join(':')]));events.push({tick,meshes:current.size,changed:detail.plan.filter(p=>previous.get(p.index)!==current.get(p.index)).length,removed:[...previous.keys()].filter(i=>!current.has(i)).length});previous=current;
}
const counts=new Map();for(const e of events)counts.set(Math.floor(e.tick/60),(counts.get(Math.floor(e.tick/60))||0)+1);
const report={simulationTicks:4500,plannerChecks:positions.length,planChanges:events.length,newOrChangedMeshes:events.reduce((n,e)=>n+e.changed,0),removedMeshes:events.reduce((n,e)=>n+e.removed,0),maxChangesPerSimulationSecond:Math.max(...counts.values()),events,scope:'Neutral event replay; selection churn, not GPU time or visual pop measurement'};
fs.writeFileSync(new URL('../local/browser-validation/terrain-detail-route.json',import.meta.url),JSON.stringify({source_sha256:json('ARA1/terrain-render.json').source_sha256,positions},null,2));
fs.writeFileSync(new URL('../local/browser-validation/terrain-detail-churn.json',import.meta.url),JSON.stringify(report,null,2));console.log(JSON.stringify({...report,events:undefined},null,2));
