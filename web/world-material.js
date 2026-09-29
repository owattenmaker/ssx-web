import {MeshBasicNodeMaterial,BufferAttribute,NoColorSpace,LinearFilter,LinearMipmapLinearFilter,NearestFilter,ClampToEdgeWrapping,RepeatWrapping,DoubleSide,NormalBlending,AdditiveBlending,TextureNode,DataTexture,RGBAFormat,UnsignedByteType,CustomBlending,OneFactor,ZeroFactor,AddEquation} from 'three/webgpu';
import {texture,attribute,uv,vec2,vec3,vec4,floor,round,clamp,uniform,select,positionView,nodeObject,NodeUpdateType,normalWorld,varying,cameraPosition,modelWorldMatrixInverse,sin,cos,mod,float,normalize,normalLocal,modelWorldMatrix,cameraViewMatrix} from 'three/tsl';
import {toFrame,encodedFrame} from './frame-space.js';
import {fogScale} from './fog-shared.js';
import {registerEncodedEffect} from './snow-composite.js';
import {Vector2,Vector3} from 'three/webgpu';
import {riderShadowReceiver} from './rider-shadow.js';
import {pv} from './pv-flags.js';
const riderShadowsOn=new URL(globalThis.location?.href??'http://x/').searchParams.get('riderShadows')!=='0';
const worldWrapOn=()=>pv('worldWrap')&&new URL(globalThis.location?.href??'http://x/').searchParams.get('worldWrap')!=='0';   // ?worldWrap=0: world models repeat (comparisons)
// UVScrollModifier groups (builtin21 0x35F6E8/0x35F7D0/0x35FC20, web/uv-scroll.js): batch.uv_scroll_group
// adds the group's texture translation to every texture coordinate (uv' = uv + (u, v)).
export const worldUvScroll=new Map();
const scrollOffset=group=>{if(!worldUvScroll.has(group))worldUvScroll.set(group,uniform(new Vector2()));return worldUvScroll.get(group);};

// Original PS2 world combine (docs/terrain-render-fidelity.md, "Original PS2
// terrain/scenery combine"). All arithmetic is on raw GS bytes; the final
// encoded byte goes to the frame through web/frame-space.js toFrame: linear for
// three's sRGB output / the fog compositor's OETF, or the byte itself (pv encodedBlend).
//
// Terrain patch = two GS contexts over the same strip (VU GIF templates 364E90):
//  ctx1: base texture, TEX0 TFX0/TCC1 with vertex RGBA 0x80 -> Cs=T, ALPHA_1 enum1
//        =0x2A -> framebuffer = Cs (opaque, ATST ALWAYS).
//  ctx2: PS2 light page (PSMCT32, CLAMP), ALPHA_2 enum8 =0x81 ->
//        Cd = ((Cd - Cl) * Al) >> 7, clamped (Al is the raw 0..255 alpha).
// Static instances: TFX0 MODULATE with the VIF V4-5 unpacked colour (c5<<3):
//        Cs = (T * (c5<<3)) >> 7, As = Ta * (bit15<<7) >> 7. Batch blend (prepare.py, 37F2A4..37F6C8):
//        0 opaque ALPHA 0x2A-equivalent, ATST ALWAYS;
//        1 ALPHA 0x44, ATST GREATER 92, AFAIL FB_ONLY;
//        2 ALPHA 0x44, ATST GREATER 20, AFAIL FB_ONLY (depth-sorted class);
//        3 model flag 8: ALPHA 0x48 (Cd + Cs*As), ATST GREATER 20, AFAIL FB_ONLY.
//        FB_ONLY is two draws of the same range: texels passing the test write
//        depth, failing texels still blend without depth. The blend is on linear
//        light unless the frame is encoded (pv encodedBlend, docs/visual-parity.md 38).
const alphaReference=[0,92,20,20];
const frameBytes=bytes=>toFrame(clamp(bytes,0,255).div(255));
// pv terrainGlint (docs/visual-parity.md 41.6): the patch "reflection" pass 38D168 over the patches with word +0x0C & 0x600000
// (tools/export_terrain_glint.py -> terrain-glint.json; ps2Glint 1 grey, 2 blue). UV per vertex = (n, 1) x E: E = B(beta) A(alpha) M,
// 38B370 at terrain +0x360 (checked against RAM), alpha = -0.0005 camY, beta = -0.0005 camX (camera position, source cm),
// n = the patch's unit normal of 374518 (ds x dt: the opposite of the package normals, which point down). The pass re-kicks the
// base strips with the context-2 template (vertex RGBA 0x80: Cs = T), CLAMP, ALPHA_2 0x58: Cd + Cs x Ad >> 7 with Ad the base
// texel's alpha (the base pass writes it; FRAME_2 masks alpha), clamped; queued after the base record and before the rider
// shadow record (38CA70: 38D168 then 38D448).
const glintOn=pv('terrainGlint'),glintMaps=new Map();
// the two 32 x 32 glint images (the same in every package; black until a package brings its image)
const glintMap=(kind,image)=>{let t=glintMaps.get(kind);
 if(!t){t=new DataTexture(new Uint8Array(32*32*4),32,32,RGBAFormat,UnsignedByteType);t.colorSpace=NoColorSpace;t.flipY=false;t.minFilter=t.magFilter=LinearFilter;t.generateMipmaps=false;t.wrapS=t.wrapT=ClampToEdgeWrapping;t.name='ps2-glint-'+kind;t.needsUpdate=true;t.userData.loaded=false;glintMaps.set(kind,t);}
 if(image&&!t.userData.loaded&&image.width===32&&image.height===32){t.image.data.set(Uint8Array.from(atob(image.rgba),c=>c.charCodeAt(0)));t.userData.loaded=true;t.needsUpdate=true;}
 return t;};
// The UV the shader below computes (web/test-terrain-glint.mjs checks it against the PS2's E at terrain +0x360): n = package normal
// (native), cam = native world camera (m).
export function glintUv([nx,ny,nz],[cx,,cz]){
 const n=[-nx,nz,-ny],a=(cz*0.05)%(Math.PI*2),b=(cx*-0.05)%(Math.PI*2),sa=Math.sin(a),ca=Math.cos(a),sb=Math.sin(b),cb=Math.cos(b);
 return [(n[0]*sb+n[2]*cb)*0.5+0.5,0.5-(n[1]*sa+n[0]*cb*ca-n[2]*sb*ca)*0.5];
}
function glintBytes(lit,baseAlpha){
 const n0=attribute('normal','vec3'),n=vec3(n0.x.negate(),n0.z,n0.y.negate());                  // package normal (native) -> 374518's, source axes
 const cam=modelWorldMatrixInverse.mul(vec4(cameraPosition,1)).xyz,TAU=Math.PI*2;               // world meshes sit at -origin: native world camera
 const a=mod(cam.z.mul(0.05),TAU),b=mod(cam.x.mul(-0.05),TAU);                                  // -0.0005 x source Y (= -z*100), -0.0005 x source X
 const sa=sin(a),ca=cos(a),sb=sin(b),cb=cos(b);
 const st=varying(vec2(n.x.mul(sb).add(n.z.mul(cb)).mul(0.5).add(0.5),float(0.5).sub(n.y.mul(sa).add(n.x.mul(cb).mul(ca)).sub(n.z.mul(sb).mul(ca)).mul(0.5))),'ps2GlintSt');
 const g=attribute('ps2Glint','float'),grey=texture(glintMap('grey'),st),blue=texture(glintMap('blue'),st);
 const ct=round(select(g.greaterThan(1.5),blue,grey).rgb.mul(255)),ad=select(baseAlpha.greaterThanEqual(255),float(128),baseAlpha.mul(0.5)); // texture_rgba: GS alpha doubled
 return clamp(lit.add(select(g.greaterThan(0.5),floor(ct.mul(ad).div(128)),vec3(0))),0,255);
}
// pv byteBlend (section 17) moved the additive class to the encoded pass; with an encoded frame the world pass blends it as the GS does, before
// the fog composite fogs Cd + Cs x As, so it stays in the world pass.
const byteBlendOn=()=>pv('byteBlend')&&!encodedFrame;
const modelColour=(map,alphaScale=null,uvNode=uv())=>{
 map.colorSpace=NoColorSpace;const base=round(texture(map,uvNode).mul(255)),colour=attribute('color','vec4');
 const alpha=alphaScale?base.a.div(255).mul(alphaScale):base.a.div(255);
 const bytes=floor(base.rgb.mul(round(colour.rgb.mul(31)).mul(8)).div(128));
 return {rgb:frameBytes(bytes),alpha,bytes};
};
// Material word+12 (flags halfword +14 in the upper half) -> blend class, as
// selected by 37F494: &0x660000 = 0 opaque, 0x20000 -> 1, 0x40000/0x60000 -> 2.
export const originalModelBlend=(flags,groupFlags=0)=>({0:0,0x20000:1,0x40000:2,0x60000:2})[((flags<<16)|(groupFlags&8?0x40000:0))&0x60000];
// Texture addressing, same material word (37F2BC..37F354): word & 0x180000 -> [renderer+E84] word0 bits 2..3,
// which 363C20 hands to 3625C0 as GS CLAMP_1: 0x80000 (flags bit3) = WMS CLAMP (u), 0x100000 (flags bit4) = WMT
// CLAMP (v), neither = REPEAT. Every ASKY..ESKY material has both bits (flags 25/27/31), and the sky's own
// render-state push (353B10, word0 |= 0xC) is clamp/clamp as well. World static models: batch.wrap (below).
export const originalModelWrap=flags=>({wrapS:flags&8?ClampToEdgeWrapping:RepeatWrapping,wrapT:flags&16?ClampToEdgeWrapping:RepeatWrapping});

// Area sky dome (353B10, depth writes off): same model combine as static
// instances. With no depth writes, AFAIL FB_ONLY makes the alpha test moot, so
// every texel of a blended class is blended.
// Addressing: the ring (mountain band, alpha 0 on its top rows, opaque on its bottom rows) and the bands below it
// end exactly at v = 0 / 1 and each 45-degree segment at u = 0 / 1. Repeat (the package default in main.js asset())
// made the bilinear filter / mip chain mix each edge with the opposite edge of the same texture: a half-transparent
// line along the bottom of the ring, a dark line at the top of the band below it, and vertical seams between the
// segments. The PS2 clamps (originalModelWrap), so the sky textures are set to its CLAMP_1 here.
export function createOriginalSkyMaterials({textures}){
 const cache=new Map();
 return {prepareGeometry(){},material(batch){
  const flags=batch.material_flags??1,map=textures['9-'+batch.texture];
  if(map){const {wrapS,wrapT}=originalModelWrap(flags);if(map.wrapS!==wrapS||map.wrapT!==wrapT){map.wrapS=wrapS;map.wrapT=wrapT;map.needsUpdate=true;}}
  const blend=originalModelBlend(flags),key=blend+':'+batch.texture;if(cache.has(key))return cache.get(key);
  const {rgb,alpha}=modelColour(map),m=new MeshBasicNodeMaterial({side:DoubleSide});
  m.colorNode=vec4(rgb,blend?alpha:1);if(blend){m.transparent=true;m.blending=NormalBlending;}
  m.userData.originalWorldCombine='sky-blend'+blend;cache.set(key,m);return m;
 }};
}

// Static-model texture addressing (batch.wrap = material word bits 0x180000 >> 19, web/world-batches.py; pv worldWrap):
// the package textures repeat; a clamped batch samples its own copy of the texture with the GS CLAMP_1 sampler (the CRA3
// rock polys, whose rock-over-snow textures bled a light line along their edges, and an ABA1 log mesh; the CHP2
// volumes are never drawn). cache: Map shared by one package's materials.
export function staticModelTexture(map,wrap,cache){
 if(!wrap||!map||!worldWrapOn())return map;const key=map.uuid+':'+wrap;let t=cache.get(key);
 if(!t){const {wrapS,wrapT}=originalModelWrap(wrap<<3);t=map.clone();t.wrapS=wrapS;t.wrapT=wrapT;t.needsUpdate=true;cache.set(key,t);}
 return t;
}

// pv sharedWorldMaterials: one colour graph per structure for every package's world materials. three keys a built node program by
// node ids, so the per-texture graphs above built one program per material (Peak 1: 75-165 builds per location for 0 new WGSL
// programs, 16-80 ms each at 4x CPU). Here each structure (terrain; instance modulate / blend class, per UV-scroll group) is built
// once, and its texture nodes take the drawn material's own textures (userData.ps2Textures) in the object update, before that
// render object's bindings update, as three's materialReference does. Same arithmetic, same samples: only binding names change.
class MaterialTexture extends TextureNode{
 static get type(){return 'MaterialTexture';}
 // updateType is OBJECT whatever TextureNode's setup assigns (it sets NONE for a node with its own uv and no flip / matrix uniform)
 constructor(slot,placeholder,uvNode){super(placeholder,uvNode);this.slot=slot;this.placeholder=placeholder;Object.defineProperty(this,'updateType',{get:()=>NodeUpdateType.OBJECT,set(){},configurable:true});}
 // the code is generated from the placeholder, whatever texture the last object update left (sharedTextureOk: every texture drawn
 // through it generates the same code: filterable 2D RGBA8, no colour-space conversion, no flip)
 setup(builder){const v=this._value;this._value=this.placeholder;try{return super.setup(builder);}finally{this._value=v;}}
 generate(builder,output){const v=this._value;this._value=this.placeholder;try{return super.generate(builder,output);}finally{this._value=v;}}
 getUniformHash(){return this.placeholder.uuid;}
 update(frame){const t=frame.material?.userData?.ps2Textures?.[this.slot];if(t&&t!==this.value)this.value=t;return super.update(frame);}
}
const placeholderTexture=name=>{const t=new DataTexture(new Uint8Array([255,255,255,255]),1,1,RGBAFormat,UnsignedByteType);t.name=name;t.colorSpace=NoColorSpace;t.flipY=false;t.magFilter=LinearFilter;t.minFilter=LinearMipmapLinearFilter;t.needsUpdate=true;return t;};
// A texture the shared graphs can draw: the properties three's texture code generation reads are the placeholder's.
const sharedTextureOk=t=>!!t?.isTexture&&!t.isRenderTargetTexture&&!t.isDepthTexture&&!t.isCubeTexture&&!t.isArrayTexture&&!t.isDataArrayTexture&&!t.isData3DTexture&&!t.isCompressedTexture&&!t.isVideoTexture&&!t.isFramebufferTexture&&!t.isStorageTexture&&
 (t.format??RGBAFormat)===RGBAFormat&&(t.type??UnsignedByteType)===UnsignedByteType&&t.colorSpace===NoColorSpace&&!t.flipY&&!(t.minFilter===NearestFilter&&t.magFilter===NearestFilter);
let sharedPlaceholders=null;const sharedGraphs=new Map();
function sharedGraph(kind,blend,scroll,glint=false){
 const key=kind==='t'?(glint?'tg':'t'):'i'+blend+(scroll!==undefined?':s'+scroll:'');let g=sharedGraphs.get(key);if(g)return g;
 sharedPlaceholders??={map:placeholderTexture('ps2-world-map'),atlas:placeholderTexture('ps2-world-atlas')};
 const tex=(slot,uvNode)=>nodeObject(new MaterialTexture(slot,sharedPlaceholders[slot],uvNode));
 if(kind==='t'){
  const base=round(tex('map',uv()).mul(255)),light=round(tex('atlas',attribute('ps2LightUv','vec2')).mul(255));
  const lit0=clamp(floor(base.rgb.sub(light.rgb).mul(light.a).div(128)),0,255),lit=glint?glintBytes(lit0,base.a):lit0;
  g={colorNode:vec4(frameBytes(riderShadowsOn?riderShadowReceiver(lit):lit),1)};
 }else{
  // modelColour with the material's texture
  const base=round(tex('map',scroll!==undefined?uv().add(scrollOffset(scroll)):uv()).mul(255)),colour=attribute('color','vec4');
  const alpha=base.a.div(255).mul(attribute('ps2VertexAlpha','float'));
  const bytes=floor(base.rgb.mul(round(colour.rgb.mul(31)).mul(8)).div(128));
  g={colorNode:vec4(frameBytes(bytes),blend?alpha:1)};
  if(blend)g.maskNode=alpha.lessThanEqual(alphaReference[blend]/128);
 }
 sharedGraphs.set(key,g);return g;
}

// QA (pixel checks of pv sharedWorldMaterials in one page): give a shared world material the per-texture graph the switch replaces
// (legacy = true; the same nodes for a material and its fringe) or its shared graph back. The caller sets material.needsUpdate.
const legacyGraphs=new WeakMap();
export function worldMaterialGraph(material,legacy){
 const t=material.userData?.ps2Textures;if(!t)return false;const {kind,blend,scroll}=t.shape;
 let g=legacy?legacyGraphs.get(t):sharedGraph(kind,blend,scroll,t.shape.glint);
 if(!g){
  if(kind==='t'){const base=round(texture(t.map,uv()).mul(255)),light=round(texture(t.atlas,attribute('ps2LightUv','vec2')).mul(255));
   const lit0=clamp(floor(base.rgb.sub(light.rgb).mul(light.a).div(128)),0,255),lit=t.shape.glint?glintBytes(lit0,base.a):lit0;g={colorNode:vec4(frameBytes(riderShadowsOn?riderShadowReceiver(lit):lit),1)};}
  else{const {rgb,alpha}=modelColour(t.map,attribute('ps2VertexAlpha','float'),scroll!==undefined?uv().add(scrollOffset(scroll)):uv());g={colorNode:vec4(rgb,blend?alpha:1)};if(blend)g.maskNode=alpha.lessThanEqual(alphaReference[blend]/128);}
  legacyGraphs.set(t,g);
 }
 material.colorNode=g.colorNode;if(material.maskNode)material.maskNode=g.maskNode;return true;
}

// pv envMap (docs/visual-parity.md 43): the static-model second pass. A material whose word +12 (with group flag bit3 -> 0x40000) & 0x660000
// is 0x200000 / 0x220000 / 0x260000 or 0x600000 / 0x620000 / 0x660000 (37F2A4..37FD2C; batch.env = [texture, mode] from web/world-batches.py)
// draws its triangles again in GS context 2 with the record's second texture: for these models (header +0x10 bit 1) 37E238 builds the UV
// matrix E = (view x node rotation, translation cleared) x the constant at 0x504760 (0.5, -0.5, 0, (0.5, 0.5)) and flags UV mode 256,
// VU1 program 3 at 0xCE8: u = 0.5 n.x + 0.5, v = -0.5 n.y + 0.5 per vertex (n: the camera-space normal), ST = uv x Q (0x3360, the first
// pass's packet re-kicked with these ST). TEX0_2 MODULATE with the packet's vertex colour: Cs = T x (c5 << 3) >> 7. ALPHA_2 enum 2
// (mode 0x200000): Cs x FIX 128 >> 7 + Cd = Cs + Cd; enum 17 (0x600000): Cs x Ad >> 7 + Cd, Ad = the first pass's alpha (Ta x Va >> 7).
// Clamped (COLCLAMP); ZTST GEQUAL: the same depth passes. Drawn in the world pass, so the fog composite fogs the sum as on the PS2.
const envOn=pv('envMap'),envTextures=new Map(),envMaterials=new Map();
export function envPassMaterial(batch,map,textures,uvNode){
 const [tex,mode]=batch.env,source=textures['9-'+tex];if(!source)return null;
 const key=tex+':'+mode+(mode===0x600000?':'+map.uuid+(batch.uv_scroll_group??''):'');let m=envMaterials.get(key);if(m)return m;
 let env=envTextures.get(source);if(!env){env=source.clone();env.wrapS=env.wrapT=ClampToEdgeWrapping;env.colorSpace=NoColorSpace;env.flipY=false;env.needsUpdate=true;envTextures.set(source,env);}
 const n=normalize(cameraViewMatrix.mul(modelWorldMatrix.mul(vec4(normalLocal,0))).xyz);          // camera space (x right, y up)
 const st=varying(vec2(n.x.mul(0.5).add(0.5),n.y.mul(-0.5).add(0.5)));                             // per vertex, as the VU writes it
 const T=round(texture(env,st).mul(255)),colour=attribute('color','vec4');
 let bytes=clamp(floor(T.rgb.mul(round(colour.rgb.mul(31)).mul(8)).div(128)),0,255);
 if(mode===0x600000){const Ad=round(texture(map,uvNode).a.mul(attribute('ps2VertexAlpha','float')).mul(128));bytes=floor(bytes.mul(Ad).div(128));}
 m=new MeshBasicNodeMaterial({side:DoubleSide,transparent:true,depthWrite:false});
 m.blending=CustomBlending;m.blendEquation=AddEquation;m.blendSrc=OneFactor;m.blendDst=OneFactor;m.blendSrcAlpha=ZeroFactor;m.blendDstAlpha=OneFactor;
 m.colorNode=vec4(frameBytes(bytes),1);m.userData.originalWorldCombine='instance-env-'+(mode===0x600000?'Ad':'add');m.userData.envTexture=tex;
 envMaterials.set(key,m);return m;
}
export const envPassKey=batch=>envOn&&batch?.instance&&batch.env?':e'+batch.env.join('_'):'';
export async function createOriginalWorldMaterials({root,textures,vertices,vertexCount,loader,load}){
 const [lighting,render,atlas,alphaBytes]=await Promise.all([load(root+'terrain-lighting.json'),load(root+'terrain-render.json'),loader.loadAsync(root+'terrain-light-atlas.png'),load(root+'vertex-alpha.bin','buffer')]);
 if(alphaBytes.byteLength!==vertexCount)throw Error('Static-model vertex alpha does not match the vertex buffer');
 const vertexAlpha=new BufferAttribute(Float32Array.from(new Uint8Array(alphaBytes),v=>v/128),1);
 // pv terrainGlint: the package's glint patches (terrain-glint.json; none -> the plain terrain graph, no attribute)
 const glint=glintOn?await fetch(root+'terrain-glint.json').then(r=>r.ok?r.json():null).catch(()=>null):null,glintOf=new Map(glint?.patches??[]);
 if(glint){for(const [kind,image] of Object.entries(glint.textures??{}))glintMap(kind,image);}
 if(lighting.version!==1||lighting.source_sha256!==render.source_sha256)throw Error('PS2 terrain lighting package/source mismatch');
 atlas.flipY=false;atlas.colorSpace=NoColorSpace;atlas.generateMipmaps=false;atlas.minFilter=atlas.magFilter=LinearFilter;atlas.wrapS=atlas.wrapT=ClampToEdgeWrapping;atlas.needsUpdate=true;
 const W=lighting.atlas.width,H=lighting.atlas.height,maps=new Map();
 // GC and PS2 light UVs are both affine in the patch parameters (u,v), so the
 // installed GC uv1 maps to the PS2 page by one affine transform per patch.
 for(const patch of render.patches){
  const entry=lighting.patches[patch.resource],g=patch.lightUv;if(!entry)throw Error(`Missing PS2 light layer for patch ${patch.resource}`);
  const [pageIndex,p0,p1,p2,p3]=entry,page=lighting.pages[pageIndex];if(!page||!g[2]||!g[3])throw Error(`Invalid PS2 light mapping for patch ${patch.resource}`);
  const sx=page.width*p2/g[2]/W,sy=page.height*p3/g[3]/H;
  maps.set(patch.resource,[(page.x+page.width*p0)/W-g[0]*sx,sx,(page.y+page.height*p1)/H-g[1]*sy,sy]);
 }
 const fill=(target,source,stride,offset,first,count,map)=>{for(let v=first;v<first+count;v++){target[v*2]=map[0]+source[v*stride+offset]*map[1];target[v*2+1]=map[2]+source[v*stride+offset+1]*map[3];}};
 const lightUv=new Float32Array(vertexCount*2);
 for(const patch of render.patches)fill(lightUv,vertices,10,8,patch.baseline_vertex_start,81,maps.get(patch.resource));
 const lightAttribute=new BufferAttribute(lightUv,2);
 let glintAttribute=null;
 if(glint&&glintOf.size){const g=new Float32Array(vertexCount);for(const patch of render.patches){const k=glintOf.get(patch.resource);if(k)g.fill(k,patch.baseline_vertex_start,patch.baseline_vertex_start+81);}glintAttribute=new BufferAttribute(g,1);}
 const prepareGeometry=geometry=>{geometry.setAttribute('ps2LightUv',lightAttribute);geometry.setAttribute('ps2VertexAlpha',vertexAlpha);if(glintAttribute)geometry.setAttribute('ps2Glint',glintAttribute);};
 const prepareTerrainGeometry=(geometry,patch)=>{
  const source=geometry.getAttribute('uv1'),count=source.count,out=new Float32Array(count*2),map=maps.get(patch.resource),flat=new Float32Array(count*2);
  for(let i=0;i<count;i++){flat[i*2]=source.getX(i);flat[i*2+1]=source.getY(i);}
  fill(out,flat,2,0,0,count,map);geometry.setAttribute('ps2LightUv',new BufferAttribute(out,2));
  if(glintAttribute)geometry.setAttribute('ps2Glint',new BufferAttribute(new Float32Array(count).fill(glintOf.get(patch.resource)??0),1)); // a refined patch: one value
 };
 const cache=new Map(),wrapped=new Map(),addressed=(map,wrap)=>staticModelTexture(map,wrap,wrapped);
 // pv envMap: the base pass(es), then the batch's env-map second pass (envPassMaterial) as the last material of the array
 function material(batch){
  // pv litInstances: a lit instance's batch (web/world-batches.py lighting: its light-cache rows) draws its texture lit per vertex from
  // those rows, not with its baked colours (litWorldMaterial below)
  const lit=batch.instance&&batch.lighting&&pv('litInstances'),base0=baseMaterial(batch),base=lit?litWorldMaterial(base0,batch.lighting):base0;if(!envPassKey(batch))return base;
  const key=(batch.instance?'i'+(batch.blend||0)+':':'t')+batch.texture+(batch.uv_scroll_group!==undefined?':s'+batch.uv_scroll_group:'')+(batch.wrap?':w'+batch.wrap:'')+envPassKey(batch)+(lit?':L'+batch.lighting.resource:'');
  if(cache.has(key))return cache.get(key);
  const map=addressed(textures['9-'+batch.texture],batch.wrap||0),env=envPassMaterial(batch,map,textures,batch.uv_scroll_group!==undefined?uv().add(scrollOffset(batch.uv_scroll_group)):uv());
  const out=env?[...[].concat(base),env]:base;cache.set(key,out);return out;
 }
 function baseMaterial(batch){
  const blend=batch.instance?batch.blend||0:0,scroll=batch.instance?batch.uv_scroll_group:undefined,wrap=batch.instance?batch.wrap||0:0,key=(batch.instance?'i'+blend+':':'t')+batch.texture+(scroll!==undefined?':s'+scroll:'')+(wrap?':w'+wrap:'');if(cache.has(key))return cache.get(key);
  const map=addressed(textures['9-'+batch.texture],wrap),m=new MeshBasicNodeMaterial({side:DoubleSide});
  if(pv('sharedWorldMaterials')&&!(blend===3&&byteBlendOn())){map.colorSpace=NoColorSpace;if(sharedTextureOk(map)&&(batch.instance||sharedTextureOk(atlas)))return sharedMaterial(batch,key,map,m,blend,scroll);}
  if(batch.instance){
   const {rgb,alpha}=modelColour(map,attribute('ps2VertexAlpha','float'),scroll!==undefined?uv().add(scrollOffset(scroll)):uv());
   m.colorNode=vec4(rgb,blend?alpha:1);
   if(blend){
    const reference=alphaReference[blend]/128,fringe=m.clone();
    // pv byteBlend: the additive class (ALPHA 0x48) adds its bytes to the encoded frame in the encoded pass after the fog composite
    // (registerWorldAdditive), scaled by the fog alpha at its depth; three's linear-light sum drew the Metro City / Kick Doubt
    // searchlight beams at 40-60 % of the PS2's (metro-event-race tick 318: sky + 61 -> 104 on the PS2, 68 in the browser).
    if(blend===3&&byteBlendOn()){const {bytes}=modelColour(map,attribute('ps2VertexAlpha','float'),scroll!==undefined?uv().add(scrollOffset(scroll)):uv());
     m.colorNode=vec4(select(worldEncodedOutput,clamp(bytes,0,255).div(255).mul(fogScale(positionView.z.negate().mul(100).max(.0001))),rgb),alpha);m.userData.byteBlend=fringe.userData.byteBlend=true;}
    for(const pass of [m,fringe]){pass.transparent=true;pass.blending=blend===3?AdditiveBlending:NormalBlending;pass.colorNode=m.colorNode;}
    m.alphaTest=reference;fringe.depthWrite=false;fringe.maskNode=alpha.lessThanEqual(reference);
    m.userData.originalWorldCombine=fringe.userData.originalWorldCombine='instance-blend'+blend;
    // pv additiveNoZ: the additive class (model flag 8, ALPHA 0x48) draws with ZBUF ZMSK = 1 (the render queue of metro-event-race
    // tick 318: every 0x48 static-model block has TEST 0x5114d = ATST GREATER 20 / AFAIL FB_ONLY / ZTST GEQUAL and ZBUF
    // 0x1010000e0), so no texel writes depth and the alpha test changes nothing: one additive pass, depth tested, no depth
    // write. The two-pass form wrote depth where alpha > 20 and hid the cones' back faces (the searchlight beams at 60 % of
    // the PS2's, glow 1000 sum 72k vs 119k).
    if(blend===3&&pv('additiveNoZ')){m.alphaTest=0;m.depthWrite=false;cache.set(key,m);return m;}
    cache.set(key,[m,fringe]);return [m,fringe];
   }
  }else{
   map.colorSpace=NoColorSpace;
   const base=round(texture(map,uv()).mul(255)),light=round(texture(atlas,attribute('ps2LightUv','vec2')).mul(255));
   // Light pass 0x81, then the rider shadow passes 38D448 (same blend with As 0x80: Cd = max(Cd - T, 0), web/rider-shadow.js).
   const lit0=clamp(floor(base.rgb.sub(light.rgb).mul(light.a).div(128)),0,255),lit=glintAttribute?glintBytes(lit0,base.a):lit0;
   m.colorNode=vec4(frameBytes(riderShadowsOn?riderShadowReceiver(lit):lit),1);
   m.userData.prepareTerrainGeometry=prepareTerrainGeometry;
  }
  m.userData.originalWorldCombine=batch.instance?'instance-modulate':'terrain-ctx2-0x81';
  cache.set(key,m);return m;
 }
 // pv sharedWorldMaterials: the same material properties as above, the colour graph of its structure (sharedGraph), its own textures
 function sharedMaterial(batch,key,map,m,blend,scroll){
  map.colorSpace=NoColorSpace;const ps2Textures={map,atlas,shape:{kind:batch.instance?'i':'t',blend,scroll,glint:!batch.instance&&!!glintAttribute}};
  // non-enumerable: Material.copy / clone JSON-copies userData (a Texture would serialise its image); set after the fringe clone
  const own=x=>Object.defineProperty(x.userData,'ps2Textures',{value:ps2Textures,enumerable:false,writable:true,configurable:true});
  if(batch.instance){
   const g=sharedGraph('i',blend,scroll);m.colorNode=g.colorNode;
   if(blend){
    const reference=alphaReference[blend]/128,fringe=m.clone();
    for(const pass of [m,fringe]){pass.transparent=true;pass.blending=blend===3?AdditiveBlending:NormalBlending;pass.colorNode=g.colorNode;}
    m.alphaTest=reference;fringe.depthWrite=false;fringe.maskNode=g.maskNode;
    m.userData.originalWorldCombine=fringe.userData.originalWorldCombine='instance-blend'+blend;own(m);own(fringe);
    if(blend===3&&pv('additiveNoZ')){m.alphaTest=0;m.depthWrite=false;cache.set(key,m);return m;}
    cache.set(key,[m,fringe]);return [m,fringe];
   }
  }else{m.colorNode=sharedGraph('t',undefined,undefined,!!glintAttribute).colorNode;m.userData.prepareTerrainGeometry=prepareTerrainGeometry;}
  m.userData.originalWorldCombine=batch.instance?'instance-modulate':'terrain-ctx2-0x81';own(m);
  cache.set(key,m);return m;
 }
 // pv gpuRestore (web/gpu-copies.js): the light UVs again, from a restored vertex array (main.js drops the arrays once uploaded)
 const lightSpans=render.patches.map(p=>[p.baseline_vertex_start,maps.get(p.resource)]);
 const lightUvFrom=v=>{const out=new Float32Array(vertexCount*2);for(const [first,map] of lightSpans)fill(out,v,10,8,first,81,map);return out;};
 return {material,prepareGeometry,atlas,lightUvFrom};
}

// pv byteBlend: the static-model additive class drawn in the encoded pass (web/snow-composite.js): one shared output switch and one
// encoded effect over the course's class-3 meshes (their own visibility kept: the composite hides them for the world pass and
// restores each one's state). main.js registers the event course's world after it is built.
export const worldEncodedOutput=uniform(false);
export function registerWorldAdditive(group){
 const meshes=[];group.traverse(o=>{if(!o.isMesh)return;const m=Array.isArray(o.material)?o.material[0]:o.material;if(m?.userData?.byteBlend)meshes.push(o);});
 if(!meshes.length)return null;
 const object={get parent(){return meshes[0].parent;},traverse(fn){for(const m of meshes)fn(m);},
  get visible(){return meshes.map(m=>m.visible);},
  set visible(v){if(Array.isArray(v))meshes.forEach((m,i)=>{m.visible=v[i];});else for(const m of meshes)m.visible=!!v;}};
 return registerEncodedEffect({object,setEncodedOutput:v=>{worldEncodedOutput.value=!!v;},populated:camera=>meshes.some(m=>m.visible&&(!camera||m.layers.test(camera.layers)))});
}

// pv litLiveComp (docs/visual-parity.md 40): a lit static-model instance (runtime flag 0x4000 = authored descriptor flag 0x40000000 in
// every countdown audit). 37E238 tests it (0x37E3B8) and calls 2F5148 -> 2F5400, the instance's light cache (32 entries of 0x180
// bytes): the Lighting painter's object bank (reference 3) plus local lights within 10 m (2F5AF0). VU1 program 3 scales the ten rows
// by 128 (0x2170, MULi.xyz) and at 0x8B8 evaluates per vertex, on the ITOF15 normal through the node's rotation rows (VU 25..27),
// L = FTOI0(clamp(r0 + r1 x^2 + r2 y^2 + r3 z^2 + r4 xy + r5 zx + r6 yz + r7 x + r8 y + r9 z, 0, 255)) in source axes, stored over
// the baked vertex colour: TFX MODULATE Cs = T x L >> 7, As = Ta x 128 >> 7 = Ta. A LiveComp mesh's matrix is its node's rest ->
// animated rotation, so normalWorld is the PS2's rotated normal (native axes). material: a shared world instance material (or its
// [pass, fringe]); lighting: {bank, rows, scale} (LIVECOMP/livecomp.json, tools/export_livecomp.py). Without its textures (the
// per-texture graphs of pv sharedWorldMaterials off) the material is returned unchanged.
const litCache=new Map(),litGraphs=new Map(),ZERO3=new Vector3();
// One colour graph per blend class / UV-scroll group for every lit instance: the ten rows are object uniforms read from the drawn
// material (userData.litRows, x128 as 0x2170 scales them), so all of a course's lit instances share one program (WebKit compiles
// each program; per-instance constants built one per instance), and the texture comes through MaterialTexture as the shared graphs.
function litGraph(blend,scroll){
 const key='L'+blend+(scroll!==undefined?':s'+scroll:'');let g=litGraphs.get(key);if(g)return g;
 sharedPlaceholders??={map:placeholderTexture('ps2-world-map'),atlas:placeholderTexture('ps2-world-atlas')};
 const base=round(nodeObject(new MaterialTexture('map',sharedPlaceholders.map,scroll!==undefined?uv().add(scrollOffset(scroll)):uv())).mul(255));
 const rows=[...Array(10)].map((_,i)=>uniform(new Vector3()).onObjectUpdate((frame)=>frame.material?.userData?.litRows?.[i]??ZERO3));
 const n=normalWorld,x=n.x,y=n.z.negate(),z=n.y,w=[x.mul(x),y.mul(y),z.mul(z),x.mul(y),z.mul(x),y.mul(z),x,y,z];   // native (x, z, -y) -> source
 let acc=rows[0];for(let i=1;i<10;i++)acc=acc.add(rows[i].mul(w[i-1]));
 const L=varying(floor(clamp(acc,0,255))),alpha=base.a.div(255);   // per vertex (Gouraud), as the VU writes it
 g={colorNode:vec4(frameBytes(floor(base.rgb.mul(L).div(128))),blend?alpha:1)};
 if(blend)g.maskNode=alpha.lessThanEqual(alphaReference[blend]/128);
 litGraphs.set(key,g);return g;
}
// lighting: {rows: ten [r, g, b] (a bank, or a light-cache row set), scale? (128), resource? | bank?}. A material already lit keeps its rows.
export function litWorldMaterial(material,lighting){
 if(Array.isArray(material)){const out=material.map(m=>litWorldMaterial(m,lighting));return out.some((m,i)=>m===material[i])?material:out;}
 if(material?.userData?.litRows)return material;
 const t=material?.userData?.ps2Textures;if(!t||t.shape?.kind!=='i')return material;
 const key=material.uuid+':'+(lighting.resource??lighting.bank);let m=litCache.get(key);if(m)return m;
 const g=litGraph(t.shape.blend,t.shape.scroll),scale=lighting.scale??128;
 m=material.clone();m.colorNode=g.colorNode;if(material.maskNode)m.maskNode=g.maskNode;
 Object.defineProperty(m.userData,'ps2Textures',{value:t,enumerable:false,writable:true,configurable:true});   // the map (MaterialTexture), the static path
 m.userData.litRows=lighting.rows.map(r=>new Vector3(...r.slice(0,3).map(v=>Math.fround(v*scale))));
 m.userData.originalWorldCombine='instance-lit'+(t.shape.blend||'');m.userData.litBank=lighting.bank;m.userData.litResource=lighting.resource;
 litCache.set(key,m);return m;
}
