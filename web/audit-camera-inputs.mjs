import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const core=await createCore();
process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else console.error(core.getExceptionMessage(e));process.exit(1);});
const riderPackage='RIDER_ZOE';
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


// Match the verified accepted-command stream: one neutral frame, jump held
// on frames2..30, then release (the movie input has one frame of latency).
const sample=(p,n)=>new Float32Array(core.HEAPF32.buffer,p,n).slice();
core._reset_animation();core._reset_race();core._reset_rider(...start.position,start.heading);
const rows=[],motionRows=[],cameraTrace=[];let precedingContactNormal;
const source=(v)=>[v[0]*100,-v[2]*100,v[1]*100];
const delta=(a,b)=>Math.hypot(...a.map((x,i)=>x-b[i]));
const floats=words=>Array.from(new Float32Array(new Uint32Array(words).buffer));
for(let frame=1;frame<=152;frame++){
 const jump=+(frame>=2&&frame<=30);
 const before=Array.from(sample(core._source_motion_audit(),20));
 core._race_begin();let state=sample(core._step_rider(0,jump,0,0),16);
 const afterMotion=Array.from(sample(core._source_motion_audit(),20));
 const posePointer=core._animation_tick(state[7],0,0,state[9],state[8],jump,0,0,0,0,state[15],0);
 if([31,60,90].includes(frame))fs.writeFileSync(new URL('../local/camera-continuous/browser-pose-'+frame+'.json',import.meta.url),JSON.stringify({local:Array.from(sample(core._sampled_local_pose(),poseCount)),world:Array.from(sample(posePointer,poseCount)),info:Array.from(sample(core._animation_info(),19)),physical:Array.from(sample(core._pose_physical(),12))}));
 state=sample(core._rider_state(),16);const pose=sample(core._pose_physical(),12);
 if(frame===29)precedingContactNormal=[state[4],-state[6],state[5]];
 if(frame===31){const takeoff=Array.from(sample(core._jump_takeoff_info(),21));assert.deepEqual(takeoff.slice(9,12),precedingContactNormal,'Takeoff must retain the preceding contact normal');fs.writeFileSync(new URL('../local/camera-continuous/browser-takeoff.json',import.meta.url),JSON.stringify(takeoff));}
 const camera=sample(core._step_camera_head(...pose.slice(9,12)),9),input=sample(core._camera_inputs(),22);
 core._race_end();const tick=input[11];
 cameraTrace.push({tick,input:Array.from(sample(core._camera_source_input(),38)),algorithm:Array.from(sample(core._camera_algorithm_info(),6))});
 motionRows.push({frame,tick,before,afterMotion,afterPose:Array.from(sample(core._source_motion_audit(),20)),poseControls:Array.from(sample(core._pose_controls_info(),17)),physics:Array.from(sample(core._physics_info(),6)),contact:Array.from(sample(core._ground_contact_info(),6)),head:Array.from(pose.slice(9,12))});
 const path=new URL('../local/camera-continuous/jump/tick-'+String(tick).padStart(8,'0')+'.json',import.meta.url);
 if(!fs.existsSync(path))continue;
 const original=JSON.parse(fs.readFileSync(path)).cameras.find(c=>c.address===0x1a58650);
 const rider=floats(original.rider_words),trajectory=floats(original.trajectory_words);
 const algorithm=sample(core._camera_algorithm_info(),6),reference=floats(original.algorithm_words);
 rows.push({frame,tick,browserMode:input[12],originalMode:original.mode,
  algorithmEyeErrorCm:delta(Array.from(algorithm.slice(0,3)),reference.slice(0x60/4,0x60/4+3)),
  algorithmTargetErrorCm:delta(Array.from(algorithm.slice(3,6)),reference.slice(0x20/4,0x20/4+3)),
  queryBoundsErrorCm:Math.max(...sample(core._rider_query_bounds(),12).map((v,i)=>Math.abs(v-rider[0x400/4+i]))),
  positionErrorCm:delta(source(state.slice(0,3)),rider.slice(0x110/4,0x110/4+3)),
  headErrorCm:delta(Array.from(pose.slice(9,12)),original.head.slice(0,3)),
  launch:[input[13],rider[0x5a4/4]],crouch:[input[21],rider[0x220/4]],
  predictionTime:[input[1],trajectory[0x98/4]],
  predictionHeadingError:delta(Array.from(input.slice(2,5)),trajectory.slice(4,7)),
  predictionNormalError:delta(Array.from(input.slice(5,8)),trajectory.slice(8,11))});
}
fs.writeFileSync(new URL('../local/camera-continuous/browser-camera-trace.json',import.meta.url),JSON.stringify(cameraTrace,null,2)+'\n');
fs.writeFileSync(new URL('../local/camera-continuous/browser-motion-phases.json',import.meta.url),JSON.stringify(motionRows,null,2)+'\n');
assert.equal(rows.length,122,'Must compare every original checkpoint');
for(const row of rows){
 assert.equal(row.crouch[0],row.crouch[1],`Original crouch differs at tick ${row.tick}`);
 assert(row.headErrorCm<.1,`Original posed head differs by over1mm at tick ${row.tick}`);
 assert.equal(row.browserMode,row.originalMode,`Original motion transition differs at tick ${row.tick}`);
 assert(row.positionErrorCm<.1,`Original rider position differs by over1mm at tick ${row.tick}`);
 assert(Math.abs(row.launch[0]-row.launch[1])<.001,'Original launch strength differs');
 if(row.originalMode===0)assert.equal(row.predictionTime[0],row.predictionTime[1],'Landing must clear predicted time');
}
const output=new URL('../local/camera-continuous/browser-input-comparison.json',import.meta.url);
fs.writeFileSync(output,JSON.stringify(rows,null,2)+'\n');
// This is an independent output comparison, not covered by input parity above.
// The browser starts a fresh chase camera; the reference retains pre-jump history.
// Report that difference explicitly instead of treating passing inputs as a
// full camera match. These samples precede compositor lift/shake and rendering.
const outputSummary={scope:'DEFAULT_3 output before compositor; fresh browser camera versus retained PS2 history',
 checkpoints:rows.length,outputWithin1mm:rows.every(r=>r.algorithmEyeErrorCm<.1&&r.algorithmTargetErrorCm<.1),
 maximumEyeErrorCm:Math.max(...rows.map(r=>r.algorithmEyeErrorCm)),
 maximumTargetErrorCm:Math.max(...rows.map(r=>r.algorithmTargetErrorCm)),
 first:rows[0],touchdown:rows.find(r=>r.originalMode===0),last:rows.at(-1)};
fs.writeFileSync(new URL('../local/camera-continuous/browser-output-comparison.json',import.meta.url),JSON.stringify(outputSummary,null,2)+'\n');
console.log('Camera output comparison (separate from passing input assertions):',outputSummary);
console.log('Camera input comparison:',{first:rows[0],originalTouchdown:rows.find(r=>r.originalMode===0),browserTouchdown:rows.find(r=>r.browserMode===0),last:rows.at(-1)});
