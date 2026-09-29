import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import {createRiderLightingUpdate} from './rider-material.js';
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
let lightUpdate;
if(json(riderPackage+'/rider.json').source_skin){
 const environment=json('ARA1/environment.json'),env=str('ARA1/environment.json'),bytes=read('ARA1/environment.bin'),data=put(bytes),catalog=str('ARA1/local-lights.json'),tree=str('ARA1/light-tree.json');
 core._init_environment(env,data,bytes.length);core._init_rider_lighting(catalog,tree);for(const p of [env,data,catalog,tree])core._free(p);
 lightUpdate=createRiderLightingUpdate(core,environment.irradiance);
}



// Exercise camera inputs through the same motion -> pose -> camera order as main.js.
const sample=(p,n)=>new Float32Array(core.HEAPF32.buffer,p,n).slice();
const transitionTrace=[];
for(const held of [false,true]){
 core._reset_animation();assert.equal(core._rider_skin_matrices(),0,'Reset retained stale source skin matrices');assert.equal(core._rider_skin_palette(),0,'Reset retained stale source palette');core._reset_race();core._reset_rider(...start.position,start.heading);
 core._reset_fog();core._reset_rider_lighting();const fogRegions=new Set();let previousPalette=null,cachedPaletteFrames=0;
 let departed=false,landed=false,airFrames=0,priorMode=0,priorTick=null;
 for(let tick=0;tick<900;tick++){
  const jump=held?1:+(tick>=90&&tick<150);
  core._race_begin();const state=sample(core._step_rider(0,jump,0,0),16);
  core._animation_tick(state[7],0,0,state[9],state[8],jump,0,0,0,0,state[15],0);
  const pose=sample(core._pose_physical(),12);
  const skinCount=core._rider_skin_matrix_count(),skinPointer=core._rider_skin_matrices();
  assert.equal(skinCount,riderPackage==='RIDER_ZOE'?27:26);
  if(skinCount){const matrices=sample(skinPointer,skinCount*16);assert(matrices.every(Number.isFinite),'Invalid live source skin matrices');assert.deepEqual(sample(core._rider_skin_matrices(),skinCount*16),matrices,'Repeated draw changes cached matrices');}
  else assert.equal(skinPointer,0);
  const paletteCount=core._rider_skin_palette_count(),skinPalettePointer=core._rider_skin_palette();assert.equal(paletteCount,riderPackage==='RIDER_ZOE'?161:168);if(paletteCount){const data=sample(skinPalettePointer,paletteCount*16);assert(data.every(Number.isFinite));assert.deepEqual(sample(core._rider_skin_palette(),paletteCount*16),data,'Repeated draw changes weighted palette');}else assert.equal(skinPalettePointer,0);

  const camera=sample(core._step_camera_head(...pose.slice(9,12)),9),input=sample(core._camera_inputs(),22);
  if(lightUpdate){lightUpdate.update();const light=sample(core._rider_lighting_info(),5);assert.equal(light[1],tick+1);assert.equal(light[2],tick+1);assert(sample(core._rider_lighting_gpu_coefficients(),40).every(Number.isFinite),'Invalid live rider light bank');}
  const view=sample(core._camera_render_view(),16);
  assert(view.every(Number.isFinite),'nonfinite original render view');
  const sourceEye=[camera[0]*100,-camera[2]*100,camera[1]*100,1];
  const sourceTarget=[camera[3]*100,-camera[5]*100,camera[4]*100,1];
  const transform=v=>Array.from({length:4},(_,row)=>v.reduce((sum,x,col)=>sum+x*view[col*4+row],0));
  const eyeInView=transform(sourceEye),targetInView=transform(sourceTarget);
  assert(Math.hypot(...eyeInView.slice(0,3))<.1,'view translation must place its source eye at origin');
  assert(targetInView[2]>0,'original render view must face the camera target');
  // Float32 view words far from the course origin carry absolute rounding: allow 16 ULPs of the largest source coordinate.
  const ulp=2**(Math.floor(Math.log2(Math.max(1,...sourceEye.slice(0,3).map(Math.abs),...sourceTarget.slice(0,3).map(Math.abs))))-23);
  assert(Math.hypot(targetInView[0],targetInView[1])<Math.max(.1,16*ulp),'original view axes must align with final camera target');
  core._race_end();
  const fog=sample(core._fog_info(),11);assert.equal(fog[7],tick+1,'Fog must tick with camera, once per simulation frame');assert(fog.every(Number.isFinite),'Nonfinite spatial fog');assert(fog[2]>fog[1]&&fog[1]>=0,'Invalid authored fog distances');assert(Math.abs(fog[8]-camera[0]*100)<.1&&Math.abs(fog[9]+camera[2]*100)<.1,'Fog must sample final source camera X/Y');fogRegions.add(fog[6]);
  const paletteInfo=sample(core._fog_palette_info(),9),palette=core.HEAPU8.slice(core._fog_palette_rgba(),core._fog_palette_rgba()+1024);
  assert.equal(paletteInfo[0],1);assert.equal(paletteInfo[7],1);assert.equal(paletteInfo[8],0x31);assert(paletteInfo.every(Number.isFinite));
  for(let index=0;index<256;index++){assert(palette[index*4+3]<=128);assert.deepEqual(palette.slice(index*4,index*4+3),palette.slice(0,3));if(index>=paletteInfo[2])assert.equal(palette[index*4+3],128);}
  if(previousPalette&&previousPalette.info[1]===paletteInfo[1]){assert.deepEqual(palette,previousPalette.bytes,'Cached palette bytes changed without revision');cachedPaletteFrames++;}
  previousPalette={info:paletteInfo,bytes:palette};
  if(tick===0){const payload=json('ARA1/fog-tree.json').payloads[fog[6]];assert(payload,'Start camera must be inside authored fog');const values=[payload.mode,payload.near_cm,payload.far_cm,...payload.color].map(Math.fround);assert.deepEqual(Array.from(fog.slice(0,6)),values,'Initial fog must snap to authored payload');}
  if(process.argv.includes('--trace')){
   const head=[pose[9]/100,pose[11]/100,-pose[10]/100];
   transitionTrace.push({held,tick,mode:input[12],posedGrounded:!!pose[8],head,
    eye:Array.from(camera.slice(0,3)),target:Array.from(camera.slice(3,6)),
    relativeEye:Array.from(camera.slice(0,3),(v,i)=>v-head[i]),
    filteredCrouch:input[21],launch:input[13],prediction:Array.from(input.slice(0,8))});
  }
  const controls=sample(core._animation_inputs(),17);
  assert.equal(input[21],controls[9],'camera must read rider+220 filtered crouch, not jump-button charge');
  if(!held&&tick===150){assert.equal(state[9],0,'release fixture must clear the button accumulator');assert(input[21]>.5,'filtered crouch must survive jump release');}
  assert(camera.every(Number.isFinite),'non-finite camera during jump/landing');
  assert(camera[6]>0&&camera[6]<Math.PI&&camera[7]>0&&camera[8]>camera[7],'invalid camera projection');
  if(priorTick!==null)assert.equal(input[11],priorTick+1,'camera did not advance one simulation tick');
  priorTick=input[11];
  if(input[12]===1){departed=true;airFrames++;assert(Math.abs(Math.hypot(input[18],input[19])-1)<1e-5&&input[20]===0,"takeoff wall-camera direction missing");if(!held&&tick===150)assert(input[13]>300,'charged launch camera amplitude missing');}
  if(departed&&priorMode===1&&input[12]===0)landed=true;
  priorMode=input[12];
 }
 assert(departed&&landed&&airFrames>1,'fixture must cover flight and a grounded landing');
 assert(fogRegions.size>=1,'Fixture must sample authored fog');assert(cachedPaletteFrames>0,'Unchanged palette was rebuilt every frame');
 console.log('Posed gameplay camera:',{held,airFrames,landed,fogRegions:[...fogRegions]});
}

// Camera-position fixtures cover all authored fog payloads independently of the
// short riding fixture's route. They are not a full-course traversal.
const fogData=json('ARA1/fog-tree.json');core._reset_fog();let sampled=new Set(),blended=false;
for(const entry of fogData.representative_samples){
 const [x,y]=entry.position;core._reset_rider(x/100,2000,-y/100,start.heading);
 const before=sample(core._fog_info(),11);core._step_camera_head(x,y,200000);const after=sample(core._fog_info(),11);
 assert.equal(after[6],entry.payload,'Camera fixture chose wrong fog leaf');sampled.add(after[6]);
 const target=fogData.payloads[entry.payload];
 if(before[7]>0&&Math.abs(before[1]-target.near_cm)>1)blended ||= after[1]!==before[1]&&Math.abs(after[1]-target.near_cm)>1;
 if(before[7]>0&&Math.abs(before[2]-target.far_cm)>1)blended ||= after[2]!==before[2]&&Math.abs(after[2]-target.far_cm)>1;
}
assert.equal(sampled.size,fogData.payloads.length,'Some authored fog regions untested');assert(blended,'Fog region transition snapped instead of blending');
console.log('Camera fog region fixtures:',{payloads:sampled.size,blended});

if(transitionTrace.length){
 const output=new URL('../local/browser-validation/camera-transitions-'+riderPackage+'.json',import.meta.url);
 fs.mkdirSync(new URL('.',output),{recursive:true});
 fs.writeFileSync(output,JSON.stringify(transitionTrace,null,2)+'\n');
 const movements=[];
 for(let i=1;i<transitionTrace.length;i++){
  const a=transitionTrace[i-1],b=transitionTrace[i];if(a.held!==b.held)continue;
  movements.push({held:b.held,tick:b.tick,previousMode:a.mode,mode:b.mode,
   relativeEyeStep:Math.hypot(...b.relativeEye.map((v,k)=>v-a.relativeEye[k]))});
 }
 console.log('Largest camera movements relative to posed head (metres/tick):',movements.sort((a,b)=>b.relativeEyeStep-a.relativeEyeStep).slice(0,8));
 console.log('Camera transition trace:',output.pathname);
}

lightUpdate?.dispose();
