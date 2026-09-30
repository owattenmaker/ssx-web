import * as T from 'three/webgpu';
import {snowBillboardScale} from './snow-billboard.js';
import {createStartfireRenderer} from './startfire-renderer.js';
import {createImpactFxRenderer} from './impact-fx-renderer.js';
import {attribute,texture,vec4,uniform,select} from 'three/tsl';import {toFrame} from './frame-space.js';
import {pv} from './pv-flags.js';import {drawOrder, EFFECT, SUBMIT} from './ps2-draw-order.js';
import {setUpdateRange} from './heap-views.js';
// pv snowBuckets (docs/visual-parity.md 41.8): the PS2's render-list order for the rider snow emitters. The snow component draw 0x2E24D0
// submits the ten emitters in index order (allocated, count > 0) through 0x371688 -> 0x380CE0, one record each, textured with the
// emitter's current flipbook frame. The flush merges records of equal material state and textures (0x362DE8, 0x394ED0 / 0x395000
// buckets), keys each bucket with 0x364240 = ~((31 - priority) << 26 | modes | (texture handle & 0x3FF) << 3) and radix-sorts the
// keys ascending, stable (0x364050): within priority 7 the draws go by DESCENDING texture handle, ties in submission order. The snow
// textures are FX-table entries 4..25, whose handles (renderer +0xF50: 1524 .. 1505, entries 9 and 12 empty) fall as the entry rises,
// so the order is ASCENDING texture id: the SnowTrail / CloudySpray / BodySnow cloud (5), the impacts (6), then the chunky sprays
// (tmb1..tmb8, 14..21) on top
// (the dark chunks over the cloud in the forest, ABC1 2000). renderOrder 700 + 0.02 x id (three keeps equal renderOrders in creation
// order: the riders, then the emitter index, as the submission order).
export const snowDrawOrder=(textureId)=>700+0.02*textureId;
export async function createSnowRenderer(origin,core,options={}){
 const skipEmpty=pv('skipEmpty'),effectOrder=pv('effectOrder'),buckets=pv('snowBuckets')||effectOrder;
 const encodedOutput=uniform(false);
 const asset=await (await fetch('/assets/SNOW_FX/snow-fx.json')).json(),maps=new Map(),alpha=new Map();
 await Promise.all(asset.textures.map(async t=>{const bytes=new Uint8Array(await (await fetch('/assets/SNOW_FX/'+(t.gs_alpha_file||t.file))).arrayBuffer());if(bytes.length!==t.width*t.height*4)throw Error('Original snow texture extent');const map=new T.DataTexture(bytes,t.width,t.height,T.RGBAFormat);map.wrapS=map.wrapT=T.RepeatWrapping;map.minFilter=map.magFilter=T.LinearFilter;map.colorSpace=T.NoColorSpace;map.needsUpdate=true;maps.set(t.id,map);alpha.set(t.id,t.gs_alpha_file?t.gs_alpha_scale:1);}));
 const view=asset.view;if(view?.source_viewport?.length!==2||view.max_projected_half_extent!==128)throw Error('Missing original snow view constraints');
 const info=new Float32Array(core.HEAPF32.buffer,core._snow_info(),23).slice(),group=new T.Group(),meshes=[],textureNodes=[],bindings=[];group.userData.gameplayOnly=true;
 /* One instance-matrix capacity for every emitter (the largest): three sizes the instance array in the vertex shader by the
    capacity, so ten capacities were ten shaders and pipelines (docs/firefox-load.md). The authored capacity still bounds each emitter. */
 const sharedCapacity=Math.max(...Array.from({length:10},(_,i)=>info[10+i]));
 for(let i=0;i<10;i++){
  const entry=asset.profiles.find(p=>p.emitter_index===i),id=entry.parameters.TextureId,capacity=info[10+i];
  if(!maps.has(id)||!capacity)throw Error('Original snow emitter binding');
  const geometry=new T.PlaneGeometry(2,2);for(let v=0;v<geometry.attributes.uv.count;v++)geometry.attributes.uv.setY(v,1-geometry.attributes.uv.getY(v));
  geometry.setAttribute('snowColour',new T.InstancedBufferAttribute(new Float32Array(capacity*4),4).setUsage(T.DynamicDrawUsage));
  const binding=asset.flipbooks.find(b=>b.emitter_index===i);if(!binding||binding.texture_ids.some(t=>!maps.has(t)||alpha.get(t)!==alpha.get(id)))throw Error('Invalid original snow flipbook binding');bindings.push(binding.texture_ids);
  const texel=texture(maps.get(id)),colour=attribute('snowColour','vec4');
  const material=new T.MeshBasicNodeMaterial({transparent:true,depthWrite:false,depthTest:true,side:T.DoubleSide,forceSinglePass:true,fog:false,toneMapped:false});
  const encodedColour=texel.rgb.mul(colour.rgb).clamp(0,1);
  material.fragmentNode=vec4(select(encodedOutput,encodedColour,toFrame(encodedColour)),texel.a.mul(alpha.get(id)).mul(colour.a).clamp(0,1));
  textureNodes.push(texel);
  const mesh=new T.InstancedMesh(geometry,material,sharedCapacity);mesh.userData.snowTextureId=id;mesh.userData.capacity=capacity;mesh.count=0;mesh.frustumCulled=false;mesh.renderOrder=700+i;mesh.instanceMatrix.setUsage(T.DynamicDrawUsage);group.add(mesh);meshes.push(mesh);
 }
 group.userData.warmTextures=[...maps.values()]; // every flipbook frame uploads during the loading-screen warm-up (main.js), not on its first race frame
 // World start-gate sparks share the particle pipeline (encoded composite, order 690).
 // Only Snow Jam has the startfirePop fountains (courses.json startfire=null elsewhere).
 const startfire=options.startfire===false?{group:new T.Group(),update(){}}:await createStartfireRenderer(origin,encodedOutput);group.add(startfire.group);
 // Rider board sparks/glints/grind chunks and the attack fist sparkle (impact-fx-renderer.js).
 const impact=options.impact===false?null:await createImpactFxRenderer(origin,encodedOutput,new Map([...maps].map(([id,map])=>[id,{map,scale:alpha.get(id)}])),view);if(impact)group.add(impact.group);
 let serial=-1;const extentScratch=[0,0],lastCamera=new T.Matrix4(),lastProjection=new T.Matrix4(),viewPosition=new T.Vector3(),matrix=new T.Matrix4(),position=new T.Vector3(),scale=new T.Vector3();
 const api={group,startfire,impact,setEncodedOutput(value){encodedOutput.value=!!value;},update(core,camera){
  startfire.update(core,camera);impact?.update(core,camera);
  const si=core._snow_info()>>2,H=core.HEAPF32; // core records read in place (no per-call views): info [23], flipbook [20], particles [count x 8]
  if(buckets){ // 0x364240 / 0x364050 order by the current flipbook frame, before the unchanged-state early return
   const fp=core._snow_flipbook_info()>>2;
   for(let i=0;i<10;i++)if(meshes[i])meshes[i].renderOrder=effectOrder?drawOrder(EFFECT.snow(H[fp+i]),SUBMIT.snow):snowDrawOrder(H[fp+i]); // pv effectOrder: the shared key (web/ps2-draw-order.js)
  }
  camera.updateMatrixWorld();if(serial===H[si+20]&&lastCamera.equals(camera.matrixWorld)&&lastProjection.equals(camera.projectionMatrix))return;const changed=serial!==H[si+20];serial=H[si+20];lastCamera.copy(camera.matrixWorld);lastProjection.copy(camera.projectionMatrix);
  const frames=core._snow_flipbook_info()>>2;
  for(let i=0;i<10;i++){
   const frame=H[frames+i];if(!bindings[i].includes(frame))throw Error('Snow texture frame outside authored sequence');if(meshes[i].userData.snowTextureId!==frame){textureNodes[i].value=maps.get(frame);meshes[i].userData.snowTextureId=frame;}
   const mesh=meshes[i],count=H[si+i];if(!mesh){if(count)throw Error('Unimplemented snow emitter became active');continue;}if(count>mesh.userData.capacity)throw Error('Snow history exceeded authored birth capacity');mesh.count=count;if(skipEmpty)mesh.visible=count>0;/* pv skipEmpty: an empty emitter draws nothing, and hidden it skips three's per-object work (render object, bindings, pipeline) */if(!count)continue;
   const data=H,dp=core._snow_particles(i)>>2,colours=mesh.geometry.attributes.snowColour;
   for(let j=0;j<count;j++){const n=dp+j*8;position.set(data[n]/100-origin.x,data[n+2]/100-origin.y,-data[n+1]/100-origin.z);viewPosition.copy(position).applyMatrix4(camera.matrixWorldInverse);const extent=snowBillboardScale(data[n+3]/100,-viewPosition.z,camera.projectionMatrix.elements[0],camera.projectionMatrix.elements[5],view.source_viewport,view.max_projected_half_extent,extentScratch);scale.set(extent[0],extent[1],1);matrix.compose(position,camera.quaternion,scale);mesh.setMatrixAt(j,matrix);if(changed)for(let k=0;k<4;k++)colours.array[j*4+k]=data[n+4+k];}
   // The whole active prefix was rewritten. Older pending tail updates may be
   // discarded because those instances are no longer drawn; growth rewrites them.
   setUpdateRange(mesh.instanceMatrix,0,count*16);
   if(changed)setUpdateRange(colours,0,count*4);
  }
 }};
 (globalThis.ssxEffects??={}).snow=api;return api;
}
