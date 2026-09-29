// Snow Jam opponent packages (tools/export_opponent_packages.py) load in their own core
// instances, produce finite source skin palettes after the event start, and drive
// opponent-riders.js (skin palette + original rider lighting) without a GPU.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import * as T from 'three/webgpu';
import createCore from './runtime/core.js';
import {createOpponentRiders,opponentAnimationSettings,initOpponentLighting} from './opponent-riders.js';
const root=new URL('public/assets/',import.meta.url),file=p=>fs.readFileSync(new URL(p,root)),json=p=>JSON.parse(file(p));
// {pack, id} texture entries (web/texture-archive.js): the PNG bytes of an archive entry; fetch reads /assets from public/
const packPng=(pack,id)=>{const d=fs.readFileSync(new URL('public'+pack,import.meta.url)),n=d.readUInt32LE(8),index=JSON.parse(d.toString('utf8',12,12+n)),start=12+n+((16-((12+n)%16))%16),e=index.entries.find(x=>x.id===id);assert(e,`${pack}: ${id}`);return d.subarray(start+e.offset,start+e.offset+e.size);};
globalThis.fetch=async url=>{const p=new URL(url,'http://localhost/').pathname,f=new URL('public'+decodeURIComponent(p),import.meta.url);return fs.existsSync(f)?new Response(fs.readFileSync(f)):new Response('',{status:404});};
const roster=json('opponents.json').opponents;
assert.deepEqual(roster.map(o=>[o.slot,o.package]),[[1,'RIDER_PSYMON'],[2,'RIDER_ALLEGRA'],[3,'RIDER_MOBY'],[4,'RIDER_GRIFF'],[5,'RIDER_LUTHER']]);
const sam=json('RIDER_SAM/rider.json').bones.slice(0,24).map(b=>b.name);
const initial=json('ANIMATIONS/initial.json'),metadata=file('ANIMATIONS/animation-packets.json'),packets=file('ANIMATIONS/animation-packets.bin');
const environment={meta:json('ARA1/environment.json'),bytes:new Uint8Array(file('ARA1/environment.bin'))},lights=[json('ARA1/local-lights.json'),json('ARA1/light-tree.json')];
const terrainHash=json('ARA1/terrain.json').source_sha256,start=json('ARA1/start.json');
const opponents=[],report=[];
for(const {package:name} of roster){
 // ---- package schema ----
 const rig=json(name+'/rider.json'),manifest=json(name+'/world.json'),count=file(name+'/vertices.bin').length/40,indices=new Uint32Array(new Uint8Array(file(name+'/indices.bin')).buffer);
 const bones=rig.bones.length;
 assert(bones>=26&&bones<=29,`${name}: ${bones} bones`);
 assert.deepEqual(rig.bones.slice(0,24).map(b=>b.name),sam,`${name}: body/board skeleton differs from Sam`);
 rig.bones.forEach((b,i)=>{assert(b.parent<i,`${name}: bone ${b.name} parent order`);if(i>=24)assert(b.name.startsWith('sec_')&&b.parent>=0,`${name}: extra bone ${b.name}`);});
 assert.equal(rig.source_bind_matrix_space,'source-centimeters-Z-up');
 assert.equal(rig.source_bind_matrix_words.length,bones);assert.equal(rig.source_bone_slots.length,bones);
 assert.equal(new Set(rig.source_bone_slots).size,bones);assert(rig.source_bone_slots.every(s=>Number.isInteger(s)&&s>=0&&s<rig.source_bone_slot_count));
 assert.equal(rig.source_bone_slot_count,bones+2,`${name}: expected only the two inactive eye slots`);
 assert.deepEqual(rig.source_bone_slots.slice(0,24),[...Array(24).keys()]);
 assert(rig.source_bind_matrix_words.every(row=>row.length===16&&new Float32Array(new Uint32Array(row).buffer).every(Number.isFinite)));
 assert.equal(rig.source_skin_weight_units,'integer-percent');assert.equal(rig.source_skin.length,count);assert.equal(rig.skin.length,count);
 assert(rig.source_skin.every(g=>g.length>=1&&g.length<=4&&g.every(([bone,w])=>bone<bones&&w>0)&&g.reduce((n,[,w])=>n+w,0)===100),`${name}: source weights`);
 assert(indices.every(i=>i<count));
 for(const b of manifest.batches){assert(b.first_index+b.index_count<=indices.length);assert(manifest.textures['9-'+b.texture],`${name}: batch texture ${b.texture}`);}
 for(const t of Object.values(manifest.textures))assert.equal((t.pack!==undefined?packPng(t.pack,t.id):file(name+'/'+t.path)).subarray(1,4).toString(),'PNG'); // rider texture archive entry (tools/export_rider_textures.py) or PNG file
 const seed=json(name+'/animation-start.json'),settings=opponentAnimationSettings(initial,seed);
 assert.equal(settings.original_animation.character,rig.character);
 // ---- core instance ----
 const began=performance.now(),core=await createCore();
 process.on('uncaughtException',e=>{console.error(e instanceof Error?e:core.getExceptionMessage(e));process.exit(1);});
 const pointers=[],put=b=>{const p=core._malloc(b.length);pointers.push(p);core.HEAPU8.set(b,p);return p;},text=s=>put(Buffer.concat([Buffer.from(s),Buffer.from([0])])),str=p=>text(file(p)),f32=(p,n)=>new Float32Array(core.HEAPF32.buffer,p,n);
 const settingsPtr=text(JSON.stringify(settings));
 core._init_animation(text(metadata),str(name+'/rider.json'),settingsPtr,put(packets),packets.length);
 core._init_race(settingsPtr);core._animation_use_physics(1);
 const mesh=file('ARA1/collision.bin');core._init_world(put(mesh),mesh.length/4);
 const hash=text(terrainHash);core._init_terrain(str('ARA1/terrain.json'));core._init_world_collision(str('ARA1/world_collision.json'),hash);core._init_body_terrain(str('ARA1/terrain.json'));core._init_rails(str('ARA1/rails.json'),hash);
 initOpponentLighting(core,{environment,lights});
 for(const p of pointers)core._free(p);
 const loaded=performance.now()-began;
 core._reset_animation();core._reset_race();core._reset_rider(...start.position,start.heading);core._reset_pad_history();core._start_event();
 const pad=core._malloc(96),neutral=new Float32Array(24);
 const tick=()=>{core.HEAPF32.set(neutral,pad>>2);const o=f32(core._pad_tick(pad),24).slice();core._race_begin();const s=f32(core._step_rider(o[0],o[6],o[2]?1:0,o[7]),16).slice();core._animation_tick(s[7],o[10],o[11],s[9],s[8],o[6],o[8],o[12],o[7],0,s[15],o[13]);const pose=f32(core._pose_physical(),12);core._race_end();core._step_camera_head(pose[9],pose[10],pose[11]);};
 for(let t=0;t<200;t++)tick();
 assert.equal(core._rider_skin_matrix_count(),bones,`${name}: skin matrix count`);
 const matrices=f32(core._rider_skin_matrices(),bones*16);assert(matrices.every(Number.isFinite),`${name}: nonfinite skin matrices`);
 // Pose x authored body scale x inverse bind: every column of a rigid bone's linear part has the opponent's scale.
 const scale=seed.scale[0];for(let b=0;b<24;b++)for(let c=0;c<3;c++)assert(Math.abs(Math.hypot(...matrices.subarray(b*16+c*4,b*16+c*4+3))-scale)<2e-3,`${name}: bone ${b} scale`);
 const groups=new Set(rig.source_skin.map(g=>JSON.stringify(g))).size;
 assert.equal(core._rider_skin_palette_count(),groups);assert(f32(core._rider_skin_palette(),groups*16).every(Number.isFinite));
 opponents.push({core,visible:true,tick});
 report.push({name,bones,slots:rig.source_bone_slot_count,vertices:count,groups,scale,loadMs:Math.round(loaded),heapMB:core.HEAPU8.length/1048576});
}
// ---- renderer module (no GPU: materials/nodes/textures are built, palette textures filled) ----
const scene=new T.Scene(),loads=[];
const load=async(path,type='json')=>{const bytes=fs.readFileSync(new URL('public'+path,import.meta.url));loads.push(path);return type==='json'?JSON.parse(bytes):bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength);};
const loader={loadAsync:async path=>{if(!path.startsWith('blob:'))assert(fs.existsSync(new URL('public'+path,import.meta.url)),path);return new T.Texture();}}; // blob: = an archive entry (web/texture-archive.js)
const riders=await createOpponentRiders({T,scene,load,loader,origin:new T.Vector3(...start.position),packages:roster.map(o=>o.package),lighting:{configuration:environment.meta.irradiance,viewCore:opponents[0].core}});
assert.equal(riders.entries.length,5);assert(riders.entries.every(e=>scene.children.includes(e.group)&&!e.group.visible));
riders.update(opponents,1);assert(riders.entries.every(e=>!e.group.visible),'opponents must stay hidden before their first palette');
riders.capture(opponents);for(const o of opponents)o.tick();riders.capture(opponents);
riders.update(opponents,.5);assert(riders.entries.every(e=>e.group.visible));
riders.update(opponents.map((o,i)=>i===2?{...o,visible:false}:o),.5);assert.equal(riders.entries[2].group.visible,false);assert.equal(riders.entries[1].group.visible,true);
riders.update([opponents[0]],.5);assert.equal(riders.entries[4].group.visible,false,'missing core must hide the rider');
for(const [i,o] of opponents.entries()){
 const info=new Float32Array(o.core.HEAPF32.buffer,o.core._rider_lighting_info(),5),coefficients=new Float32Array(o.core.HEAPF32.buffer,o.core._rider_lighting_gpu_coefficients(),40);
 assert(info[1]>=2&&info[2]>=2,`${report[i].name}: rider lighting refresh/draw`);assert(coefficients.every(Number.isFinite)&&coefficients.some(x=>x>0),`${report[i].name}: rider lighting coefficients`);
 report[i].lights=info[4];
}
riders.reset(opponents);assert(riders.entries.every(e=>!e.group.visible));riders.update(opponents,1);assert(riders.entries.every(e=>!e.group.visible),'reset must hide until the next capture');
riders.dispose();assert(riders.entries.every(e=>!scene.children.includes(e.group)));
console.table(report);
console.log('Opponent riders: 5 packages load, skin and light in their own cores');
