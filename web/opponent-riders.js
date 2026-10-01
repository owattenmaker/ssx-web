import {createOriginalRiderSkinning} from './rider-skinning.js';
import {createRiderLightingMaterial,riderDrawState} from './rider-material.js';import {frameTextureSpace} from './frame-space.js';
import {createControllerLights} from './rider-controller-lights.js';
import {packageTexture} from './texture-archive.js';
/*
 Snow Jam computer opponents drawn with their own original models, skins and textures.

 Each opponent is simulated by its own browser core instance (one `await createCore()`
 each), initialised with its own rider.json (`_init_animation(metadata, RIDER_X/rider.json,
 settings, packets, n)`). This module only reads the pose/skin exports the human path
 reads, and draws with the human's source path (rider-skinning.js source palette
 vertex node + rider-material.js original rider lighting), not the Three.js skeleton:

   per 60 Hz tick  capture(): core._rider_skin_palette() (pose x body scale x authored
                   inverse bind x source weights, one matrix per distinct weight group)
                   is copied into the rider's palette texture, previous tick kept
                   (exactly sam.userData.sourceSkin.capture(core, reset)).
   per frame       update(opponents, alpha): the vertex node mixes previous/current palette
                   with the same fixed-step renderAlpha main.js passes to
                   sam.userData.sourceSkin.display(); no rider-frame.js bone interpolation is
                   needed because the source path ignores the Three.js skeleton.

 Assets: web/public/assets/RIDER_{PSYMON,ALLEGRA,MOBY,GRIFF,LUTHER} (tools/export_opponent_packages.py),
 grid roster web/public/assets/opponents.json (event-start participants 1..5, slot order).

 Opponent core initialisation
   physics + pose (required): _init_world, _init_animation (opponent rider.json; settings
     from opponentAnimationSettings(initial.json, RIDER_X/animation-start.json) for the
     opponent's authored body scale), _init_race, _animation_use_physics(1), _init_terrain,
     _init_world_collision, _init_body_terrain, _init_rails, then _start_event and per-tick
     _pad_tick/_race_begin/_step_rider/_animation_tick/_race_end as for the human.
   rendering: nothing else for skin/pose. For the original rider lighting also call
     initOpponentLighting(core, {environment, lights}) once per opponent core
     (= _init_environment + _init_rider_lighting, the same packages main.js gives the human);
     the environment irradiance is then updated by that core's own _step_rider (its own
     ground patch) and local lights are selected around its own bounds. The rim term needs
     the camera view: capture() copies it from `lighting.viewCore` (the human core's
     _camera_render_view()). Cores without lighting draw the unlit texture (menu path).
   not needed for rendering: _init_fog, camera stepping, snow/wake/trail/boost FX.

 WIRING (lines for web/main.js; the lead integrates):
   import {createOpponentRiders,opponentAnimationSettings,initOpponentLighting} from './opponent-riders.js'
   // after `scene.add(sam)` and the environment/light assets are loaded, with `opponents`
   // = [{core, visible:true}, ...] in opponents.json slot order:
   opponentRiders=await createOpponentRiders({T,scene,load,loader,origin,
     packages:(await load('/assets/opponents.json')).opponents.map(o=>o.package),
     lighting:{configuration:environmentMeta.irradiance,viewCore:core}})
   // (per opponent core, before its first tick:)
   initOpponentLighting(o.core,{environment:{meta:environmentMeta,bytes:environmentBytes},lights:lightAssets})
   // simTick(), right after sam.userData.sourceSkin?.capture(core,!previousRiderFrame) and
   // after every opponent core stepped this tick (the human camera has stepped already):
   opponentRiders.capture(opponents)
   // frame(), next to sam.userData.sourceSkin?.display(...):
   opponentRiders.update(opponents,renderAlpha)
   // startRun() (next to sam.userData.sourceSkin?.reset()):
   opponentRiders.reset(opponents)
   // an opponent teleported this tick (reset placement / rescue): set opponents[i].reset=true
   // for that capture so the palette does not interpolate across the jump.
 If the original fog renderer is created after this (main.js clears m.fog on every scene
 mesh then), nothing else is needed; created later than that, pass sceneFog:false (default).
*/
export const OPPONENT_SETTING_FIELDS=['character','scale'];
// initial.json is the human's (Zoe, snow-jam-glide) seed. Overlay the opponent's authored
// fields from its animation-start.json (same snapshot, tools/test_opponent_poses.py
// --export-initialization). Only character + body scale by default: its `layers` use an
// older schema the core rejects (no completed/seek_pending/raised_flags), and the other
// fields (contact, default_mirror, bone_mask, limited_bones, pose_state...) change the
// physics seed, which is the caller's decision. The human's secondary_motion profile is
// dropped: the opponent's own +86C profile is not exported.
export function opponentAnimationSettings(initial,start,fields=OPPONENT_SETTING_FIELDS){
 const original={...initial.original_animation};delete original.secondary_motion;
 for(const key of fields){if(!(key in start))throw Error(`Opponent animation seed lacks ${key}`);original[key]=start[key];}
 if(original.scale?.length!==3||!original.scale.every(x=>Number.isFinite(x)&&x>0))throw Error('Invalid opponent body scale');
 return {...initial,original_animation:original};
}
// Environment + local light world on an opponent core (same packages as the human).
// environment: {meta: ARA1/environment.json object, bytes: ARA1/environment.bin Uint8Array};
// lights: [ARA1/local-lights.json object, ARA1/light-tree.json object].
export function initOpponentLighting(core,{environment,lights}){
 const pointers=[],put=bytes=>{const p=core._malloc(bytes.length);pointers.push(p);core.HEAPU8.set(bytes,p);return p;},text=value=>put(new TextEncoder().encode(JSON.stringify(value)+'\0'));
 try{
  core._init_environment(text(environment.meta),put(environment.bytes),environment.bytes.length);
  core._init_rider_lighting(text(lights[0]),text(lights[1]));
 }finally{for(const p of pointers)core._free(p);}
}
function lightingAvailable(core){
 return !!core._rider_lighting_info&&new Float32Array(core.HEAPF32.buffer,core._rider_lighting_info(),5)[0]===1&&new Float32Array(core.HEAPF32.buffer,core._environment_irradiance_info(),10)[0]===1;
}
// rider-material.js createRiderLightingUpdate with the view supplied by the human core
// (opponent cores never step a camera).
function createLightingUpdate(core,configuration){
 if(configuration.rim_constants?.length!==5||!Number.isFinite(configuration.rim_scale))throw Error('Missing original rider rim parameters');
 const point=core._malloc(16),constants=core._malloc(20),view=core._malloc(64),extra=createControllerLights(core);core.HEAPF32.set(configuration.rim_constants,constants/4);
 return {
  update(viewMatrix){
   const physical=new Float32Array(core.HEAPF32.buffer,core._pose_physical(),12);core.HEAPF32.set([physical[9],physical[10],physical[11],1],point/4);core.HEAPF32.set(viewMatrix,view/4);
   core._refresh_rider_lighting(core._rider_query_bounds(),point);
   const count=extra.count(); // the trick boost's extra light, as the human's (rider-controller-lights.js)
   core._shade_rider_lighting(core._environment_irradiance(),view,point,configuration.rim_scale,constants,count?extra.pointer:0,count);
  },
  dispose(){core._free(point);core._free(constants);core._free(view);extra.dispose();}
 };
}
async function loadOpponent({T,load,loader,origin,root,renderOrder,sceneFog},name){
 const base=root+name+'/';
 const [manifest,rig,vb,ib,cb]=await Promise.all([load(base+'world.json'),load(base+'rider.json'),...['vertices.bin','indices.bin','colors.bin'].map(f=>load(base+f,'buffer'))]);
 if(!rig.source_skin||!rig.source_bind_matrix_words)throw Error(`${name}: rider.json lacks source skin/bind rows (tools/export_opponent_packages.py)`);
 const vertices=new Float32Array(vb),indices=new Uint32Array(ib),colors=new Float32Array(cb),count=vertices.length/10;
 if(!Number.isInteger(count)||rig.source_skin.length!==count||colors.length!==count*4)throw Error(`${name}: vertex/skin/colour counts differ`);
 const skin=createOriginalRiderSkinning(rig,vertices,origin),lighting=createRiderLightingMaterial(skin,colors.some((v,i)=>i%4<3&&v!==1));
 const textures={};
 await Promise.all(Object.entries(manifest.textures).map(async([key,t])=>{const texture=await packageTexture(loader,base,t);
 // rider texture archive entry (web/texture-archive.js) or PNG file
texture.flipY=false;texture.wrapS=texture.wrapT=T.RepeatWrapping;texture.colorSpace=frameTextureSpace;texture.anisotropy=4;texture.userData.texelDomain=t.texel_domain;textures[key]=texture;}));
 // Same per-texture batch merge and material setup as main.js asset(..., true).
 const byTexture=new Map();
 for(const b of manifest.batches){if(!byTexture.has(b.texture))byTexture.set(b.texture,[]);byTexture.get(b.texture).push(indices.subarray(b.first_index,b.first_index+b.index_count));}
 const inter=new T.InterleavedBuffer(vertices,10),colorAttribute=new T.BufferAttribute(colors,4),group=new T.Group(),materials=[];
 group.name=name;group.visible=false;
 for(const [texture,ranges] of byTexture){
  const map=textures['9-'+texture];if(!map)throw Error(`${name}: missing texture 9-${texture}`);
  const material=new T.MeshBasicNodeMaterial({map,vertexColors:true,side:T.DoubleSide,alphaTest:.35});
  material.vertexNode=skin.vertexNode;lighting.attach(material);material.fog=sceneFog;
  // riderDrawState (web/rider-material.js): the PS2 draw state of the texture's material ('alph' / 'ea*' blended in two passes)
  const drawn=riderDrawState(material,manifest.batches.find(b=>b.texture===texture)?.material);materials.push(...[].concat(drawn));
  const geometry=new T.BufferGeometry(),merged=new Uint32Array(ranges.reduce((n,r)=>n+r.length,0));let at=0;for(const r of ranges){merged.set(r,at);at+=r.length;}
  geometry.setAttribute('position',new T.InterleavedBufferAttribute(inter,3,0));geometry.setAttribute('normal',new T.InterleavedBufferAttribute(inter,3,3));
  geometry.setAttribute('uv',new T.InterleavedBufferAttribute(inter,2,6));geometry.setAttribute('uv1',new T.InterleavedBufferAttribute(inter,2,8));geometry.setAttribute('color',colorAttribute);
  skin.attach(geometry);geometry.setIndex(new T.BufferAttribute(merged,1));if(Array.isArray(drawn))drawn.forEach((_,i)=>geometry.addGroup(0,merged.length,i));
  const mesh=new T.Mesh(geometry,drawn);mesh.frustumCulled=false;mesh.renderOrder=renderOrder;group.add(mesh);
 }
 group.userData.shadowRider={skin,batches:manifest.batches,indices,core:()=>group.userData.shadowCore}; // web/rider-shadow.js

 // the parsed rider.json (~16k small arrays per rider) is only needed to build the skin: not kept on the entry
return {name,group,skin,lighting,materials,textures,paletteGroups:new Set(rig.source_skin.map(g=>JSON.stringify(g))).size,update:null,updateCore:null,captured:false};
}
/*
 createOpponentRiders({T, scene, load, loader, origin, packages, root='/assets/', renderOrder=600,
                       sceneFog=false, lighting:{configuration, viewCore}?})
   T        three/webgpu namespace; scene: the human's scene; load(path,'json'|'buffer') and
   loader (T.TextureLoader) as in main.js; origin: the scene origin Vector3 main.js subtracts.
   packages ['RIDER_PSYMON', ...] in the same order as the `opponents` array given later.
 returns {entries, capture(opponents), update(opponents, alpha), reset(opponents?), dispose()}
   opponents[i] = {core, visible=true, reset=false}; a missing entry/core hides rider i.
*/
export async function createOpponentRiders({T,scene,load,loader,origin,packages,root='/assets/',renderOrder=600,sceneFog=false,lighting=null}){
 const entries=await Promise.all(packages.map(name=>loadOpponent({T,load,loader,origin,root,renderOrder,sceneFog},name)));
 for(const entry of entries){entry.group.userData.opponentRider=true;scene.add(entry.group);}   // main.js hides them under a menu
 const view=new Float32Array(16);
 return {
  entries,
  capture(opponents){
   let viewReady=false;
   if(lighting?.viewCore){const p=lighting.viewCore._camera_render_view();if(p){view.set(new Float32Array(lighting.viewCore.HEAPF32.buffer,p,16));viewReady=view.every(Number.isFinite);}}
   entries.forEach((entry,i)=>{
    const o=opponents[i],core=o?.core;if(!core||!core._rider_skin_palette_count())return;
    entry.skin.capture(core,!!o.reset);entry.captured=true;entry.group.userData.shadowCore=core;
    if(!lighting||!viewReady)return;
    if(entry.updateCore!==core){entry.update?.dispose();entry.update=null;entry.updateCore=core;if(lightingAvailable(core))entry.update=createLightingUpdate(core,lighting.configuration);}
    else if(!entry.update&&lightingAvailable(core))entry.update=createLightingUpdate(core,lighting.configuration);
    if(entry.update){entry.update.update(view);entry.lighting.capture(core);}
   });
  },
  update(opponents,alpha){
   entries.forEach((entry,i)=>{const o=opponents[i];entry.group.visible=entry.skin.display(!!o?.core&&o.visible!==false&&entry.captured,alpha);});
  },
  reset(opponents=[]){
   entries.forEach((entry, i) => {
     entry.skin.reset();
     entry.lighting.reset();
     entry.captured = false;
     entry.group.visible = false;
     const core = opponents[i]?.core;
     if (core && entry.update && entry.updateCore === core) core._reset_rider_lighting();
   });
  },
  dispose(){
   for(const entry of entries){
    scene.remove(entry.group);entry.update?.dispose();entry.skin.dispose();
    entry.group.traverse(o=>{if(o.isMesh)o.geometry.dispose();});
    for(const m of entry.materials){m.userData.sourceTexture?.dispose();m.dispose();}
    for(const t of Object.values(entry.textures))t.dispose();
   }
  }
 };
}
