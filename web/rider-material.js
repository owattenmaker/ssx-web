import {Vector4,NoColorSpace} from 'three';
import {uniform,uniformArray,varying,texture,vec4,select,output,attribute,float} from 'three/tsl';
import {toFrame,encodedToLinear,linearToEncoded} from './frame-space.js';
import {riderIrradianceNode,riderHighlight2Node} from './rider-lighting-nodes.js';
import {createControllerLights} from './rider-controller-lights.js';
// Vertex lighting is quantized before interpolation, as in the original VU path.
// GS modulation uses encoded texture bytes; the final encoded RGB goes to the frame
// through web/frame-space.js toFrame. Menu keeps its map.
export function createRiderLightingMaterial(skin,authoredTint=false){
 const values=Array.from({length:10},()=>new Vector4()),coefficients=uniformArray(values,'vec4'),ready=uniform(false);
 const vertexLighting=varying(riderIrradianceNode(Array.from({length:10},(_,i)=>coefficients.element(i)),skin.lightingNormal),'ssxRiderLighting');
 return {
  attach(material){
   const raw=material.map.clone();raw.colorSpace=NoColorSpace;raw.needsUpdate=true;material.userData.sourceTexture=raw;
   const sample=texture(raw);
   // Sam's authored vertex tint was defined in linear space by the custom mesh.
   // Preserve it before the byte-domain game lighting; stock riders skip this.
   const source=authoredTint?vec4(linearToEncoded(encodedToLinear(sample.rgb).mul(attribute('color','vec3'))),sample.a):sample;
   // GS texels: the packages carry the PS2 TXP texels (128 = 1.0, tools/export_characters.py ps2_texel_pngs,
   // texel_domain 'ps2'); a GameCube-domain map (255 = 1.0: Sam, generated wardrobe outfits) is halved into it.
   // An Xbox HD entry (web/texture-archive.js, userData.entryDomain 'xbox', docs/xbox-textures.md section 8) is full intensity too:
   // halved here, so only the texels change (a BC texture cannot be halved on upload).
   const domain=material.map.userData?.entryDomain??material.map.userData?.texelDomain,ps2=domain==='ps2',xbox=domain==='xbox';
   const texel=vec4(source.rgb.mul(ps2?255:127.5),source.a.mul(255));
   const combined=riderHighlight2Node(texel,vertexLighting).div(255);
   // Unlit (menus, before the first lighting capture): texel x 1.0, i.e. the PS2 texels doubled.
   const unlit=ps2||xbox?vec4(toFrame(texel.rgb.mul(2/255).min(1)),sample.a):output;
   material.outputNode=select(skin.enabledNode.and(ready),vec4(toFrame(combined.rgb),combined.a),unlit);
  },
  capture(core){const p=core._rider_lighting_gpu_coefficients(),data=new Float32Array(core.HEAPF32.buffer,p,40);for(let i=0;i<10;i++)values[i].fromArray(data,i*4);ready.value=true;},
  reset(){ready.value=false;}
 };
}
// Caller supplies the fixed-tick query phase. This synchronous web integration
// refreshes each pose tick; original async job cadence remains a parity gap.
export function createRiderLightingUpdate(core,configuration){
 if(configuration.rim_constants?.length!==5||!Number.isFinite(configuration.rim_scale))throw Error('Missing original rider rim parameters');
 const point=core._malloc(16),constants=core._malloc(20),extra=createControllerLights(core);core.HEAPF32.set(configuration.rim_constants,constants/4);
 return {
  update(){
   const physical=new Float32Array(core.HEAPF32.buffer,core._pose_physical(),12);core.HEAPF32.set([physical[9],physical[10],physical[11],1],point/4);
   core._refresh_rider_lighting(core._rider_query_bounds(),point);
   // A spatial Lighting painter (Metro-City) owns the rim scalar (+0x50); uniform courses keep the packaged start value.
   const lighting=core._environment_lighting_info?new Float32Array(core.HEAPF32.buffer,core._environment_lighting_info(),8):null;
   const count=extra.count(); // the trick boost's extra light (rider-controller-lights.js, RFX+0xD30)
   core._shade_rider_lighting(core._environment_irradiance(),core._camera_render_view(),point,lighting&&lighting[0]?lighting[3]:configuration.rim_scale,constants,count?extra.pointer:0,count);
  },
  dispose(){core._free(point);core._free(constants);extra.dispose();}
 };
}

// riderDrawState (docs/xbox-textures.md section 9): the PS2 rider draw state per material, instead of one alphaTest 0.35 for all.
// 37A610 (rider draw) sets the material state 363C20 turns into GS registers (TEST builder 3626D8: word1 bits 12..19 AREF, 20..21 the
// alpha-test mode, 23..24 the depth mode; bits 2..6 the ALPHA enum):
//   - a material whose record has flag 0x8000 (set at bind, 0x386920, when the material name is 'alph' or starts 'ea': hair, hats and
//     the 'eat*' pieces) draws with ALPHA 0x44 = (Cs - Cd) x As >> 7 + Cd, TEST ATST GREATER AREF 92 with AFAIL FB_ONLY, ZTST GEQUAL:
//     every texel blends by its alpha; only those above 92 (of 128) also write Z;
//   - every other material draws with ALPHA 0x2A (Cs) and ATST ALWAYS: opaque, the texture's alpha unused (PS2 frame: Allegra's suit
//     CLUT alpha set to 0 in GS VRAM changes no pixel; her hair's set to 0 hides it).
// Three cannot write Z per fragment in one blended draw, so a blended material draws twice (a material array: two geometry groups of one
// mesh, in order): the texels above 92 blended with Z writes (alphaTest 185/255: texels hold the alpha doubled), then the others
// blended without Z. Colours, lighting and texels are unchanged.
export const RIDER_AREF=92;
export const blendedRiderMaterial=(name)=>name==='alph'||/^ea/.test(name??'');
export function riderDrawState(material,name){
 // opaque: the alpha is not used, and the pixel's alpha is 1 (the port composites its canvas: alpha 0 would show what is behind it)
 if (!blendedRiderMaterial(name)) {
   material.alphaTest = 0;
   material.transparent = false;
   const out = material.outputNode;
   if (out) material.outputNode = vec4(out.rgb, float(1));
   material.needsUpdate = true;
   return material;
 }
 const high=material,data=high.userData;high.userData={};const low=high.clone();high.userData=data;const limit=float((2*RIDER_AREF+0.5)/255);   // (clone would JSON-copy userData's texture)
 high.transparent=true;high.depthWrite=true;high.alphaTest=(2*RIDER_AREF+1)/255;high.needsUpdate=true;
 const out=high.outputNode;low.alphaTest=0;low.transparent=true;low.depthWrite=false;
 if(out)low.outputNode=vec4(out.rgb,select(out.a.greaterThan(limit),float(0),out.a));
 low.userData={...data,riderDrawPass:'low'};low.needsUpdate=true;
 return [high,low];
}
