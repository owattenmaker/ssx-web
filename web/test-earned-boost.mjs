import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import {captureRiderFrame} from './rider-frame.js';
import {Vector3} from 'three';
const c=await createCore();
const root='public/assets/';
const read=p=>JSON.parse(fs.readFileSync(root+p));
const allocations=[];
const put=bytes=>{const p=c._malloc(bytes.length);allocations.push(p);c.HEAPU8.set(bytes,p);return p;};
const string=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const raw=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin');
const rig=read('RIDER_SAM/rider.json'),settings=read('ANIMATIONS/initial.json');
c._init_animation(string(read('ANIMATIONS/animation-packets.json')),string(rig),string(settings),put(raw),raw.length);
const collision=fs.readFileSync(root+'ARA1/collision.bin');
c._init_world(put(collision),collision.length/4);
const terrain=read('ARA1/terrain.json');c._init_terrain(string(terrain));
c._init_world_collision(string(read('ARA1/world_collision.json')),put(new TextEncoder().encode(terrain.source_sha256+'\0')));c._init_body_terrain(string(terrain));
for(const p of allocations)c._free(p);
c._animation_use_physics(1);
const start=read('ARA1/start.json');
const runs=[];
for(const useBoost of [false,true]){
 c._reset_rider(...start.position,start.heading);c._reset_animation();
 let retainedContactSource=null,previousJump=false;
 let earned=0,used=0,landed=0,peakMeter=0,boostSpeed=0,predictedTicks=0,alignedTicks=0,bodyCandidates=0,bodyContacts=0,incompleteBodyTicks=0,liveLiftTicks=0,boardBounceTicks=0,slopedPoseTicks=0;
 // Use the captured safe early jump for reward/boost checks; crash lifecycle has its own suite.
 for(let tick=0;tick<180;tick++){
  const jump=+(tick>=1&&tick<30),pressed=+(useBoost&&tick>130);
  const previousState=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16).slice();
  let r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,jump,0,pressed),16).slice();
  const physicalOrientation=new Float32Array(c.HEAPF32.buffer,c._rider_orientation(),4).slice();
  assert(physicalOrientation.every(Number.isFinite));assert(Math.abs(Math.hypot(...physicalOrientation)-1)<.00002,'physical orientation lost normalization');
  const prediction=new Float32Array(c.HEAPF32.buffer,c._prediction_info(),19).slice();
  if(!r[8]){assert.equal(prediction[6],0,'course prediction hit incomplete collision');if(prediction[0]&&prediction[1]===1){assert(Number.isFinite(prediction[4]));predictedTicks++;}}
  if(!r[8]&&prediction[0]){
   const predictedPosition=[prediction[13]/100,prediction[15]/100,-prediction[14]/100];
   for(let k=0;k<3;k++)assert(Math.abs(r[k]-predictedPosition[k])<.0002,'rider was separately advanced after original trajectory integration');
   assert(Math.abs(r[14]-prediction[18]/100)<.0001,'vertical motion differs from original integrated state');
  }
  const before=new Float32Array(c.HEAPF32.buffer,c._boost_info(),8).slice();
  const floor=c._height_at(r[0],r[1]+.15,r[2]),gap=Math.max(0,r[1]-floor),estimate=(r[14]+Math.sqrt(r[14]*r[14]+38*gap))/19;
  const motionState=r;
  const pose=c._animation_tick(r[7],0,0,r[9],r[8],jump,tick>=35&&tick<55?1:0,0,pressed,estimate,r[15],0);
  r=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16).slice();
  assert(new Float32Array(c.HEAPF32.buffer,pose,rig.bones.length*7).every(Number.isFinite));
  const aligned=new Float32Array(c.HEAPF32.buffer,c._rider_orientation(),4);
  assert(aligned.every(Number.isFinite)&&Math.abs(Math.hypot(...aligned)-1)<.00002);
  if(!r[8]&&Math.hypot(...aligned.map((v,k)=>v-physicalOrientation[k]))>.0001)alignedTicks++;
  const camera=new Float32Array(c.HEAPF32.buffer,c._step_camera(0,.95,0),9);
  assert(camera.every(Number.isFinite));
  const cameraInput=new Float32Array(c.HEAPF32.buffer,c._camera_inputs(),12);
  const cameraPrediction=new Float32Array(c.HEAPF32.buffer,c._prediction_info(),19).slice();
  assert.equal(cameraInput[0],+(!!cameraPrediction[0]&&(cameraPrediction[1]===1||cameraPrediction[1]===3)));
  if(cameraPrediction[0]){assert.equal(cameraInput[1],cameraPrediction[2],'camera lost predicted airtime');assert.deepEqual(Array.from(cameraInput.slice(2,8)),Array.from(cameraPrediction.slice(7,13)),'camera trajectory basis differs from original predictor');}
  //13D218 updates history only when ground contact executes; charged release
  // enters air before that stage, and airborne frames retain the history.
  const crashContactOwner=new Float32Array(c.HEAPF32.buffer,c._crash_info(),4)[0];
  if(r[11]||crashContactOwner)retainedContactSource=r;
  else if(previousState[8]&&!(previousJump&&!jump))retainedContactSource=previousState;
  previousJump=!!jump;
  const contactSource=retainedContactSource;
  assert(contactSource,'Fixture must establish ground contact history');
  assert(Math.abs(cameraInput[8]-contactSource[4])<.00001&&Math.abs(cameraInput[9]+contactSource[6])<.00001&&Math.abs(cameraInput[10]-contactSource[5])<.00001,`camera contact history differs tick${tick} boost${useBoost} grounded${r[8]} landed${r[11]} expected${[contactSource[4],-contactSource[6],contactSource[5]]} actual${Array.from(cameraInput.slice(8,11))} crash${Array.from(new Float32Array(c.HEAPF32.buffer,c._crash_info(),4))}`);
  assert.equal(cameraInput[11],settings.original_landing.runtime.tick+tick+1,'camera is using its own clock');


  const visual=new Float32Array(c.HEAPF32.buffer,c._pose_controls_info(),16);
  assert(visual.every(Number.isFinite),'nonfinite live pose controls');
  liveLiftTicks+=Math.abs(visual[2])>.001;boardBounceTicks+=Math.abs(visual[5])>.001;
  slopedPoseTicks+=Math.hypot(visual[10],visual[11])>.001;
  assert(visual[6]>=0&&visual[6]<=1,'board alignment outside original filter bounds');
  const query=new Float32Array(c.HEAPF32.buffer,c._body_query_info(),14);
  assert(query.every(Number.isFinite));assert.equal(query[0],1,'body query lacks initialized world data');
  assert.equal(!!query[1],query[6]===0&&query[7]===0,'incomplete body query reported complete');
  bodyCandidates+=query[5];bodyContacts+=query[4];incompleteBodyTicks+=!query[1];
  const volume=new Float32Array(c.HEAPF32.buffer,c._body_volume_info(),108).slice();
  assert.equal(volume[0],10);assert(Math.abs(volume[1]-92*settings.original_ground.profile.body_scale)<.00001,'Body radius must use authored rider scale');
  const physicalPose=new Float32Array(c.HEAPF32.buffer,c._pose_physical(),12).slice();
  const displayState=r.slice();displayState[8]=physicalPose[8];
  const frame=captureRiderFrame(displayState,new Float32Array(c.HEAPF32.buffer,pose,rig.bones.length*7),new Float32Array(c.HEAPF32.buffer,c._animation_info(),19),rig.bones.map(b=>b.parent),new Vector3(),physicalPose.slice(3,7),physicalPose.slice(0,3));
  const worldBones=[];
  const worldBone=i=>{if(worldBones[i])return worldBones[i];const parent=rig.bones[i].parent;const base=parent<0?{position:frame.position,rotation:frame.rotation}:worldBone(parent);return worldBones[i]={position:frame.bonePositions[i].clone().applyQuaternion(base.rotation).add(base.position),rotation:base.rotation.clone().multiply(frame.boneRotations[i])};};
  const matches=(offset,bone)=>{const expected=worldBone(bone).position;const actual=new Vector3(volume[offset]/100,volume[offset+2]/100,-volume[offset+1]/100);assert(actual.distanceTo(expected)<.001,'body collision sphere differs from the rendered pose');};
  matches(2,1);matches(5,settings.original_animation.contact.board_root??22);
  for(let sphere=0;sphere<volume[0];sphere++){assert(volume[9+sphere*5]>0);matches(10+sphere*5,volume[8+sphere*5]);}
  const inputs=new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),10);
  assert(Math.abs(Math.hypot(...inputs.slice(0,3))/100-motionState[7])<.0001,`stale animation speed at ${tick}`);
  assert(Math.abs(inputs[2]/100-motionState[14])<.0001,`stale airborne/landing vertical velocity at ${tick}`);
  assert(Math.abs(inputs[3]-motionState[4])<.00001&&Math.abs(inputs[4]+motionState[6])<.00001&&Math.abs(inputs[5]-motionState[5])<.00001,'stale contact normal');
  const after=new Float32Array(c.HEAPF32.buffer,c._boost_info(),8).slice();
  assert(after[0]>=-settings.original_boost.profile.drain_per_tick&&after[0]<=1, `meter outside source drain bound at ${tick}: ${after[0]}`);
  if(after[0]>before[0]){assert(r[11],'meter awarded without a landing');earned++;}
  if(before[1]>0){used++;boostSpeed=Math.max(boostSpeed,r[7]);}
  peakMeter=Math.max(peakMeter,after[0]);landed+=r[11];
 }
 assert(liveLiftTicks>0&&boardBounceTicks>0&&slopedPoseTicks>0,'live body lift, board bounce or slope directions never reach the pose');
 assert(earned>0&&peakMeter>0,'real gameplay did not earn boost');
 assert(landed>0);assert(predictedTicks>0,'no original landing prediction produced');assert(alignedTicks>0,'original air alignment never changes the physical orientation');assert(bodyCandidates>0,'body queries never visit world geometry');
 assert.equal(used>0,useBoost,'earned boost was not dispatched correctly');
 runs.push({useBoost,earned,used,landed,peakMeter,boostSpeed,predictedTicks,alignedTicks,bodyCandidates,bodyContacts,incompleteBodyTicks,liveLiftTicks,boardBounceTicks,slopedPoseTicks});
}
console.log('Imported course jump/grab/landing earns meter; subsequent boost consumes it:',runs);
fs.writeFileSync('../local/browser-validation/earned-boost.json',JSON.stringify(runs,null,2));
// Place a test-only pose one meter into the intact body-query terrain.
// Empty reset/snapping geometry and the separate board-ray terrain for this
// body-response fixture. Authored body terrain/objects stay intact; no hit is injected.
// Isolate body response from the separate board-ray landing controller:
// remove only ray-query patches; original body-query terrain stays intact.
c._init_terrain(string({...terrain,patches:[]}));
const empty=c._malloc(4);c._init_world(empty,0);c._free(empty);
c._reset_rider(start.position[0],start.position[1]-1,start.position[2],start.heading);c._reset_animation();
const airSpawn=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,0,0,0),16);assert(Math.abs(airSpawn[0]-start.position[0])<2,'airborne spawn used an unseeded trajectory at the origin');
for(let tick=0;tick<60;tick++)c._animation_tick(18,0,0,1,1,1,0,0,0,5,0,1);
let contactTicks=0,maxPenetration=0,bounceTicks=0;
for(let tick=0;tick<180;tick++){
 const beforeResponse=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16).slice();
 c._animation_tick(18,0,0,0,0,0,0,0,0,5,0,1);
 const query=new Float32Array(c.HEAPF32.buffer,c._body_query_info(),14);
 assert(query.every(Number.isFinite));
 // Air body queries filter terrain against the pre-pose presentation up (13AA48), so this fixture's first
 // contact is deep enough to classify as a hard crash (reaction kind4); the crash entry then owns motion.
 if(query[2]&&new Float32Array(c.HEAPF32.buffer,c._crash_info(),12)[0]){contactTicks++;maxPenetration=Math.max(maxPenetration,query[3]);break;}
 if(query[2]){
  const poseFrame=new Float32Array(c.HEAPF32.buffer,c._pose_physical(),12);assert.equal(poseFrame[7],1);
  const posedPosition=[poseFrame[0]/100,poseFrame[2]/100,-poseFrame[1]/100];
  const response=new Float32Array(c.HEAPF32.buffer,c._body_response_info(),11),afterResponse=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16);
  assert.equal(response[0],1);assert.equal(response[3],1);bounceTicks+=response[2];
  const displacement=[response[4]/100,response[6]/100,-response[5]/100];
  for(let k=0;k<3;k++)assert(Math.abs(posedPosition[k]-beforeResponse[k]-displacement[k])<.001,'Geometry commit must translate the sampled pose without resampling');
  for(let k=0;k<3;k++)assert(Math.abs(afterResponse[k]-beforeResponse[k]-displacement[k])<.001,'body response advances motion instead of applying the source translation');
  assert(Math.abs(afterResponse[7]-Math.hypot(...response.slice(7,10))/100)<.0001);
  assert(Math.abs(afterResponse[14]-response[9]/100)<.0001);
  assert.equal(query[1],1);assert(query[3]>0);assert(Math.abs(Math.hypot(...query.slice(8,11))-1)<.001);contactTicks++;maxPenetration=Math.max(maxPenetration,query[3]);}
}
assert(contactTicks>0,'posed body query never detects a deliberate terrain intersection');
const crashedOnContact=new Float32Array(c.HEAPF32.buffer,c._crash_info(),12)[0]===1;
assert(bounceTicks>0||crashedOnContact,'incoming collision neither bounced nor classified as a crash');
const reaction=new Float32Array(c.HEAPF32.buffer,c._collision_reaction_info(),10).slice();if(bounceTicks>0){assert.equal(reaction[0],bounceTicks,'body bounce did not dispatch exactly one original collision classification');assert(reaction[1]>=0&&reaction[1]<=4);assert.equal(reaction[4],1,'ordinary airborne reaction did not request predictor reseed');assert(reaction[8]>0,'collision history did not retain impact speed');}else{assert.equal(reaction[0],1,'first deep contact must dispatch one classification');assert.equal(reaction[1],4,'first deep contact must classify as an original crash');}
c._reset_animation();assert.equal(new Float32Array(c.HEAPF32.buffer,c._body_query_info(),14)[0],0);
console.log('Posed body contact fixture:',{contactTicks,bounceTicks,maxPenetrationCm:maxPenetration,reactionKind:reaction[1],reactionAnimation:reaction[2]});
// Establish an inverted authored pose above terrain, then place that same
// continuing pose into the world for a deterministic crash-classification hit.
c._reset_rider(start.position[0],start.position[1]+100,start.position[2],start.heading);c._reset_animation();
for(let tick=0;tick<60;tick++)c._animation_tick(18,0,0,1,1,1,0,0,0,5,0,1);
let inverted=false;
for(let tick=0;tick<180;tick++){
 const posePointer=c._animation_tick(18,0,0,0,0,0,0,0,0,5,0,1);
 const physical=new Float32Array(c.HEAPF32.buffer,c._pose_physical(),12);
 const display=captureRiderFrame(new Float32Array(c.HEAPF32.buffer,c._rider_state(),16),new Float32Array(c.HEAPF32.buffer,posePointer,rig.bones.length*7),new Float32Array(c.HEAPF32.buffer,c._animation_info(),19),rig.bones.map(b=>b.parent),new Vector3(),physical.subarray(3,7),physical.subarray(0,3));
 if(new Vector3(0,1,0).applyQuaternion(display.rotation).y<-.5){inverted=true;break;}
}
assert(inverted,'fixture never reaches an inverted original pose');
c._reset_rider(start.position[0],start.position[1]-1,start.position[2],start.heading);
// reset_rider seeds an upright presentation up; the air query filters with the pre-pose up (13AA48),
// so the continuing inverted pose reaches the query on the second tick after the teleport.
for(let k=0;k<2&&!new Float32Array(c.HEAPF32.buffer,c._collision_reaction_info(),10)[0];k++){c._step_rider(0,0,0,0);c._animation_tick(18,0,0,0,0,0,0,0,0,5,0,1);}
const crashReaction=new Float32Array(c.HEAPF32.buffer,c._collision_reaction_info(),10).slice();
assert.equal(crashReaction[0],1);assert.equal(crashReaction[1],4,'inverted terrain impact is not classified as an original crash');
assert(crashReaction[2]>=328&&crashReaction[2]<=361);
const dispatchedCrash=new Float32Array(c.HEAPF32.buffer,c._crash_info(),12);assert.equal(dispatchedCrash[0],1,'classified body impact did not enter crash motion');assert.equal(dispatchedCrash[3],crashReaction[2]);assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_info(),19)[15],8);
console.log('Inverted posed collision selects original crash:',{semantic:crashReaction[2],resetPredictor:crashReaction[4]});
// Exercise the same event-entry API used by classified grounded soft hits.
const restoreMesh=c._malloc(collision.length);c.HEAPU8.set(collision,restoreMesh);c._init_world(restoreMesh,collision.length/4);c._free(restoreMesh);
c._init_terrain(string(terrain));
const softRuns=[];
for(const semantic of [55,56,57,58,59,60]){
 c._reset_rider(...start.position,start.heading);c._reset_animation();c._animation_use_physics(1);
 c._animation_tick(18,0,0,0,1,0,0,0,0,5,0,0);c._soft_collision_begin(semantic,0);
 let activeTicks=0,completed=false;
 for(let tick=0;tick<180;tick++){
  const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,1,1,0),16).slice();
  const pose=c._animation_tick(r[7],0,1,r[9],r[8],1,0,0,0,5,r[15],0);
  assert(new Float32Array(c.HEAPF32.buffer,pose,rig.bones.length*7).every(Number.isFinite));
  const info=new Float32Array(c.HEAPF32.buffer,c._animation_info(),19);
  if(info[15]===3){activeTicks++;assert.equal(r[9],0,'soft control accepts a charged jump');assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_inputs(),17)[8],0,'soft control accepts brake target');}
  else {assert.equal(info[15],0);completed=true;break;}
 }
 assert(activeTicks>0&&completed,`soft clip ${semantic} did not complete through source control`);
 const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,0,0,0),16);
 c._animation_tick(r[7],0,0,r[9],r[8],0,0,0,0,5,r[15],0);
 assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_info(),19)[15],0);
 softRuns.push({semantic,activeTicks});
}
console.log('All original soft-hit clips return to normal control:',softRuns);
// A test-only static box exercises query -> bounce -> classification -> soft
// entry. Its geometry is synthetic; no hit or reaction result is injected.
c._reset_rider(...start.position,start.heading);c._reset_animation();
for(let tick=0;tick<60;tick++){
 const r=new Float32Array(c.HEAPF32.buffer,c._step_rider(0,0,1,0),16);
 c._animation_tick(r[7],0,1,r[9],r[8],0,0,0,0,5,r[15],0);
}
const broad=new Float32Array(c.HEAPF32.buffer,c._body_volume_info(),108).slice();
const center=Array.from(broad.slice(2,5)),low=[center[0]-18,center[1]-200,center[2]-200],high=[center[0]-8,center[1]+200,center[2]+200];
const identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],fixtureHash='soft-collision-fixture';
const boxWorld={version:1,source_sha256:fixtureHash,bindings:{8:{descriptors:[{type:2,flags:0x200000,nodes:[{surface_id:0,flags:0,value:0}]}]}},instances:[{track:8,rid:1,collision_descriptor:0,scale:1,bounds_min_cm:low,bounds_max_cm:high,model_resource:264,matrix:identity}],render_model_nodes:{'8:1':{nodes:[{parent:0xffffffff,matrix:identity,draw_bounds_cm:[...low,...high],draw_flags:1}]}},collision_meshes:{}};
c._init_world_collision(string(boxWorld),put(new TextEncoder().encode(fixtureHash+'\0')));
c._init_body_terrain(string({...terrain,source_sha256:fixtureHash}));
const atWall=new Float32Array(c.HEAPF32.buffer,c._rider_state(),16);
c._animation_tick(atWall[7],0,0,atWall[9],atWall[8],0,0,0,0,5,0,0);
const softReaction=new Float32Array(c.HEAPF32.buffer,c._collision_reaction_info(),10);
assert.equal(softReaction[1],3,'box contact did not select original soft reaction');
assert.equal(new Float32Array(c.HEAPF32.buffer,c._animation_info(),19)[15],3,'classified soft hit did not enter control 3');
console.log('Synthetic box contact dispatches original soft control:',{semantic:softReaction[2]});
