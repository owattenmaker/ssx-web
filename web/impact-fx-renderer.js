import * as T from 'three/webgpu';
import {attribute,texture,vec4,select} from 'three/tsl';import {toFrame} from './frame-space.js';
import {snowBillboardScale} from './snow-billboard.js';
import {pv} from './pv-flags.js';

// Rider contact/impact sprites from the core (web/impact_fx_gameplay.inc):
//  - board sparks, RFX+0x470 draw 2DB478: grind chunks (371688, snow program, GS 0x44, tmb1..8),
//    the spark kernel (380518 -> VU1 program 4 entry 0: `sprk`, GS 0x48 additive, 64 px cap,
//    centre-clipped sprites skipped without stepping the slot's 1/8 alpha fade) and three glints
//    (377CF0: `sprk`, colour 128, no cap);
//  - the attack fist sparkle RFX+0xC70 draw 2F1510 (377CF0: `ospk`, GS 0x48, no cap).
// All draw at layer priority 7 in the encoded composite: chunks/sparks/glints after the wake
// (650) and before the boost strips (660), the fist sparkle after the snow emitters (700..709).
export const SPARK_HALF_EXTENT_LIMIT=64;
const SPARK_FLOATS=9,GLINT_FLOATS=4,CHUNK_FLOATS=8,FIST_FLOATS=8;

// Per-slot alpha fade of VU program 4: accepted sprites only (sprites whose centre is
// outside the clip volume are dropped and do not step the fade).
export function sparkSpriteAlphas(sprites,count,accept){
 const out=new Float32Array(count);let slot=-1,drawn=0;
 for(let j=0;j<count;j++){const n=j*SPARK_FLOATS,s=sprites[n+8];if(s!==slot){slot=s;drawn=0;}
  if(!accept(j)){out[j]=-1;continue;}
  out[j]=sprites[n+7]*(1-drawn/8);drawn++;}
 return out;
}

async function loadImpactTextures(){
 const asset=await (await fetch('/assets/IMPACT_FX/impact-fx.json')).json(),maps=new Map();
 await Promise.all(asset.textures.map(async t=>{const bytes=new Uint8Array(await (await fetch('/assets/IMPACT_FX/'+t.file)).arrayBuffer());if(bytes.length!==t.width*t.height*4)throw Error('Original impact FX texture extent');
  const map=new T.DataTexture(bytes,t.width,t.height,T.RGBAFormat);map.minFilter=map.magFilter=T.LinearFilter;map.colorSpace=T.NoColorSpace;map.needsUpdate=true;maps.set(t.name,{map,scale:t.gs_alpha_scale,texture:t});}));
 for(const name of ['sprk','ospk'])if(!maps.has(name))throw Error('Missing original impact FX texture '+name);
 return maps;
}
function spriteMesh(capacity,map,alphaScale,encodedOutput,{additive,wrap,renderOrder}){
 const geometry=new T.PlaneGeometry(2,2);for(let v=0;v<geometry.attributes.uv.count;v++)geometry.attributes.uv.setY(v,1-geometry.attributes.uv.getY(v));
 geometry.setAttribute('fxColour',new T.InstancedBufferAttribute(new Float32Array(capacity*4),4).setUsage(T.DynamicDrawUsage));
 map.wrapS=map.wrapT=wrap?T.RepeatWrapping:T.ClampToEdgeWrapping;
 const texel=texture(map),colour=attribute('fxColour','vec4'),encodedColour=texel.rgb.mul(colour.rgb).clamp(0,1);
 // GS MODULATE with vertex colour/128; 0x48 = Cd + Cs*As>>7 (additive), 0x44 = normal alpha blend. Depth tested, no depth write.
 const material=new T.MeshBasicNodeMaterial({transparent:true,depthWrite:false,depthTest:true,side:T.DoubleSide,forceSinglePass:true,fog:false,toneMapped:false,...(additive?{blending:T.AdditiveBlending}:{})});
 material.fragmentNode=vec4(select(encodedOutput,encodedColour,toFrame(encodedColour)),texel.a.mul(alphaScale).mul(colour.a).clamp(0,1));
 const mesh=new T.InstancedMesh(geometry,material,capacity);mesh.count=0;mesh.frustumCulled=false;mesh.renderOrder=renderOrder;mesh.instanceMatrix.setUsage(T.DynamicDrawUsage);
 return {mesh,texel};
}
export async function createImpactFxRenderer(origin,encodedOutput,snowMaps,view){
 const maps=await loadImpactTextures(),group=new T.Group();group.userData.gameplayOnly=true;
 const sprk=maps.get('sprk'),ospk=maps.get('ospk'),chunkFrame=snowMaps.get(14);
 if(!chunkFrame)throw Error('Grind chunks need the tmb flipbook');
 const chunks=spriteMesh(240,chunkFrame.map,chunkFrame.scale,encodedOutput,{additive:false,wrap:true,renderOrder:655});
 const sparks=spriteMesh(240,sprk.map,sprk.scale,encodedOutput,{additive:true,wrap:true,renderOrder:656});
 const glintMap=sprk.map.clone();glintMap.needsUpdate=true; // material B clamps both axes
 const glints=spriteMesh(3,glintMap,sprk.scale,encodedOutput,{additive:true,wrap:false,renderOrder:657});
 const fist=spriteMesh(4,ospk.map,ospk.scale,encodedOutput,{additive:true,wrap:false,renderOrder:710});
 for(const m of [chunks,sparks,glints,fist])group.add(m.mesh);
 const matrix=new T.Matrix4(),position=new T.Vector3(),viewPosition=new T.Vector3(),scale=new T.Vector3(),clip=new T.Vector4();
 let chunkTexture=14;const state={sparks:0,glints:0,chunks:0,fist:0,skipped:0};
 const place=(mesh,i,cm,halfCm,camera,limit)=>{position.set(cm[0]/100-origin.x,cm[2]/100-origin.y,-cm[1]/100-origin.z);viewPosition.copy(position).applyMatrix4(camera.matrixWorldInverse);
  const e=snowBillboardScale(halfCm/100,-viewPosition.z,camera.projectionMatrix.elements[0],camera.projectionMatrix.elements[5],view.source_viewport,limit);scale.set(e[0],e[1],1);matrix.compose(position,camera.quaternion,scale);mesh.setMatrixAt(i,matrix);};
 const inside=(cm,camera)=>{clip.set(cm[0]/100-origin.x,cm[2]/100-origin.y,-cm[1]/100-origin.z,1).applyMatrix4(camera.matrixWorldInverse).applyMatrix4(camera.projectionMatrix);const w=clip.w;return w>0&&Math.abs(clip.x)<=w&&Math.abs(clip.y)<=w&&clip.z>=-w&&clip.z<=w;};
 const skipEmpty=pv('skipEmpty');/* pv skipEmpty: an empty batch hidden (it drew nothing; three still ran its per-object work) */
 const finish=(m,count)=>{m.mesh.count=count;if(skipEmpty)m.mesh.visible=count>0;if(count){m.mesh.instanceMatrix.needsUpdate=true;m.mesh.geometry.attributes.fxColour.needsUpdate=true;}};
 return {group,state,update(core,camera){
  if(!core._impact_fx_info)return;
  const info=new Float32Array(core.HEAPF32.buffer,core._impact_fx_info(),17);camera.updateMatrixWorld();
  const read=(kind,n,floats)=>new Float32Array(core.HEAPF32.buffer,core._impact_fx_sprites(kind),n*floats);
  // Sparks.
  const nSparks=info[1],spark=read(0,nSparks,SPARK_FLOATS),cm=[0,0,0];
  const alphas=sparkSpriteAlphas(spark,nSparks,j=>{cm[0]=spark[j*9];cm[1]=spark[j*9+1];cm[2]=spark[j*9+2];return inside(cm,camera);});
  let count=0;const sc=sparks.mesh.geometry.attributes.fxColour.array;
  for(let j=0;j<nSparks;j++){if(alphas[j]<0){state.skipped++;continue;}const n=j*9;cm[0]=spark[n];cm[1]=spark[n+1];cm[2]=spark[n+2];place(sparks.mesh,count,cm,spark[n+3],camera,SPARK_HALF_EXTENT_LIMIT);
   // ftoi0 bytes on the GS: RGB clamped 0..255 by the program, alpha col.a*fade.
   for(let k=0;k<3;k++)sc[count*4+k]=Math.trunc(spark[n+4+k])/128;sc[count*4+3]=Math.max(0,Math.trunc(alphas[j]))/128;count++;}
  finish(sparks,count);state.sparks=count;
  // Glints (colour 128 = x1).
  const nGlints=info[2],glint=read(1,nGlints,GLINT_FLOATS),gc=glints.mesh.geometry.attributes.fxColour.array;
  for(let i=0;i<nGlints;i++){const n=i*4;place(glints.mesh,i,[glint[n],glint[n+1],glint[n+2]],glint[n+3],camera,Infinity);gc.fill(1,i*4,i*4+4);}
  finish(glints,nGlints);state.glints=nGlints;
  // Grind chunks (tmb flipbook frame from the core).
  const frame=info[5];if(frame!==chunkTexture){const next=snowMaps.get(frame);if(!next)throw Error('Grind chunk frame outside tmb1..8');chunks.texel.value=next.map;chunkTexture=frame;}
  const nChunks=Math.min(info[3],240),chunk=read(2,nChunks,CHUNK_FLOATS),cc=chunks.mesh.geometry.attributes.fxColour.array;
  for(let i=0;i<nChunks;i++){const n=i*8;place(chunks.mesh,i,[chunk[n],chunk[n+1],chunk[n+2]],chunk[n+3],camera,view.max_projected_half_extent);for(let k=0;k<4;k++)cc[i*4+k]=chunk[n+4+k];}
  finish(chunks,nChunks);state.chunks=nChunks;
  // Fist sparkle.
  const nFist=info[4],f=read(3,nFist,FIST_FLOATS),fc=fist.mesh.geometry.attributes.fxColour.array;
  for(let i=0;i<nFist;i++){const n=i*8;place(fist.mesh,i,[f[n],f[n+1],f[n+2]],f[n+3],camera,Infinity);for(let k=0;k<4;k++)fc[i*4+k]=f[n+4+k];}
  finish(fist,nFist);state.fist=nFist;
  // QA: emit point (rider+0x460 + v*0.01) in NDC for screenshot crops.
  clip.set(info[14]/100-origin.x,info[16]/100-origin.y,-info[15]/100-origin.z,1).applyMatrix4(camera.matrixWorldInverse).applyMatrix4(camera.projectionMatrix);state.pointNdc=[clip.x/clip.w,clip.y/clip.w];
 }};
}
