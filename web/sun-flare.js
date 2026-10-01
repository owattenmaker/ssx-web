import {
  Vector3,
  Vector2,
  Vector4,
  DataTexture,
  RGBAFormat,
  UnsignedByteType,
  LinearFilter,
  NearestFilter,
  ClampToEdgeWrapping,
  NoColorSpace,
  RenderTarget,
  HalfFloatType,
  MeshBasicNodeMaterial,
  QuadMesh
} from 'three/webgpu';
import {Fn,uniform,texture,vec2,vec4,float,int,floor,min,max,select,screenUV,Loop,perspectiveDepthToViewZ} from 'three/tsl';
import {followRegion,followWorldLoad} from './painter-regions.js';

// Original sun glow + lens flare (SLUS_207.72), see terrain-render-fidelity.md.
//  tWPIGD_Sun painter (factory 9, ctor 2BC910, vtable 484700): current/sample pairs
//   +8 elevation deg, +10 azimuth deg, +18/+20/+28 RGB, +30 glow alpha,
//   +38 texture index (int), +40 flare alpha, +48 glow half size. Blend 2BD378
//   (w = weight^2, texture lerped as float then truncated), compare 2BDD38, reset 2BE1A8.
//  2F4DB8 (camera update): no sun if texture index == -1; direction
//   (cos az cos el, sin az cos el, sin el) (defaults 107/16 deg when both zero),
//   point = eye + dir*(far-500cm); outcode test 37DBE8 hides it unless the
//   point projects inside the viewport; 16x16 query rect centred on it.
//  2EC478 via 2E3130: Z readback of the rect; vis = open/256 when the whole
//   rect is on screen, else max((open-128)/128, 0). Stored at sun+204.
//  2F4A08: glow sprite (renderer sprite 377CF0, centre +- half size, ARGB*128
//   vertex colour) with FX texture 51+index (sun1/sun2), ARGB (1,R,G,B),
//   A *= vis*glowAlpha, half size 320 when +48 == 1 else +48.
//  2F4690: nine 'lens' (FX 45) quadrant sprites at centre+(sun-centre)*t,
//   half size 300*s, A *= vis*flareAlpha.
//  Both use ALPHA 0x48 (Cd + Cs*As>>7), TFX MODULATE, priority 8: drawn after
//  the fog composite (layer threshold 6) and before ScreenTint (threshold 9).
const f=Math.fround;
export const SUN_FIELDS=['elevation','azimuth','r','g','b','glowAlpha','texture','flareAlpha','size'];
const payloadValues=p=>[p.elevation,p.azimuth,...p.colour,p.glow_alpha,p.texture,p.flare_alpha,p.size];
export function painterPayloadIndex(tree,x,y){
 let px=f(f(x-tree.origin[0])*tree.scale),py=f(f(y-tree.origin[1])*tree.scale),leaf=null;
 if(px>-1&&px<32768&&py>-1&&py<32768){
  let a=((Math.trunc(px)<<1)&0xffff)>>>0,b=((Math.trunc(py)<<1)&0xffff)>>>0,index=tree.root;
  for(let steps=0;steps<=tree.nodes.length;steps++){
   const node=tree.nodes[index];if(!node)throw Error('Sun painter node outside package');
   if(!(node[0]&1)){leaf=node;break;}
   index=node[((a>>15)<<1)|(b>>15)]>>1;a=(a<<1)&0xfffc;b=(b<<1)&0xfffc;
  }
  if(!leaf)throw Error('Cyclic Sun painter tree');
 }
 const value=leaf?((leaf[2]|(leaf[3]<<16))>>>0):tree.outside_words[1];
 return value===0xffffffff?-1:value;
}
// Painter state and driver (2C0778), same X/Y source as Fog/ScreenTint.
export function createSunPainter(pkg){
 let tree=pkg.painter,payloads=(tree?.payloads??[]).map(p=>payloadValues(p).map(f));const defaults=payloadValues(pkg.defaults).map(f);
 let current=defaults.slice(),distance=-99999,lastX=0,lastY=0,lastTicks=-1,selected=-1;
 const region={track:-1,located:false};let missing=false; // regionTick (web/painter-regions.js): gp+0x770's record; missing: not loaded
 const reset=()=>{current=defaults.slice();distance=0;};
 const blend=(values,weight)=>{const w=f(weight*weight),c=f(1-w);current=current.map((v,i)=>i===6?Math.trunc(f(f(w*values[i])+f(c*v))):f(f(w*values[i])+f(c*v)));};
 function step(x,y){
  if(missing){lastX=x;lastY=y;current=defaults.slice();distance=0;selected=-1;return;} // 0x2C09D8: the class defaults, +0 = 0
  const initial=distance===-99999;
  if(!initial){const dx=f(lastX-x),dy=f(lastY-y);distance=f(distance+f(Math.sqrt(f(f(dx*dx)+f(dy*dy)))));}
  lastX=x;lastY=y;selected=tree?painterPayloadIndex(tree,x,y):-1; // no section in the region: missing-section reset
  if(selected<0){reset();return;}
  const values=payloads[selected],rate=tree.payloads[selected].rate;
  if(values.every((v,i)=>v===current[i]))distance=0;
  if(initial){blend(values,-1);distance=0;return;}
  if(rate>=0&&rate<=distance){blend(values,1);distance=0;}else blend(values,-rate);
 }
 function useTree(next){tree=next??null;payloads=(tree?.payloads??[]).map(p=>payloadValues(p).map(f));}
 return {step,
  tick(core){const info=new Float32Array(core.HEAPF32.buffer,core._fog_info(),11),ticks=info[7];
   if(!info[10])return;if(ticks<lastTicks){current=defaults.slice();distance=-99999;selected=-1;}
   if(followWorldLoad(region,core)){current=defaults.slice();distance=0;selected=-1;} // the class defaults, +0 = 0
   if(ticks>0&&ticks!==lastTicks){const r=followRegion(region,core,'sun');if(r){missing=!r.record;if(r.record)useTree(r.doc?.painter??null);}step(info[8],info[9]);}lastTicks=ticks;},
  // Streamed Peak 1 world (web/free-ride.js): the painter region's Sun section (null: none); the blend state is kept. Ignored
  // while the located records rule.
  setTree(next){if(!region.located)useTree(next);},
  get values(){return Object.fromEntries(SUN_FIELDS.map((k,i)=>[k,current[i]]));},
  get state(){return {current:current.slice(),selected,distance};}};
}
// Original-space sun direction (Z up) and its web (Y up) equivalent.
export function sunDirection(values,constants){
 let el=f(values.elevation*constants.degrees_to_radians),az=f(values.azimuth*constants.degrees_to_radians);
 if(el===0&&az===0){az=constants.default_azimuth_rad;el=constants.default_elevation_rad;}
 const ce=Math.cos(el),ps2=[Math.cos(az)*ce,Math.sin(az)*ce,Math.sin(el)];
 return {ps2,web:[ps2[0],ps2[2],-ps2[1]]};
}
// Sprites in 512x448 viewport pixels with GS vertex bytes (trunc(x*128)).
export function sunSprites(pkg,values,screen,vis){
 const c=pkg.constants,[w,h]=c.viewport,cx=w*.5,cy=h*.5,byte=v=>Math.trunc(f(v*128));
 const half=values.size===1?c.default_glow_half_size:values.size;
 const glow={texture:pkg.sun_textures[values.texture],centre:screen,half,uv:[0,0,1,1],rgb:[values.r,values.g,values.b].map(byte),a:byte(f(1*f(vis*values.glowAlpha)))};
 const quad=[[0,0,.5,.5],[.5,0,1,.5],[0,.5,.5,1],[.5,.5,1,1]];
 const flares = pkg.flares.map((e) => ({
   texture: 'lens',
   centre: [cx + (screen[0] - cx) * e.position, cy + (screen[1] - cy) * e.position],
   half: e.size * c.flare_size_scale,
   uv: quad[e.quadrant] ?? [0, 0, 0.5, 1],
   rgb: e.argb.slice(1).map(byte),
   a: byte(f(e.argb[0] * f(vis * values.flareAlpha)))
 }));
 return [glow,...flares];
}
// 2EC478 counting rule for the 16x16 rect centred on the sun (viewport pixels).
export function sunQueryRect(screen,constants){
 const n=constants.query_size,[w,h]=constants.viewport,ox=Math.trunc(screen[0]-n/2),oy=Math.trunc(screen[1]-n/2);
 const clamp=(v,limit)=>v>=0?Math.min(v,limit-n):0;
 return {origin:[ox,oy],read:[clamp(screen[0]-n/2,w),clamp(screen[1]-n/2,h)].map(Math.trunc),size:n};
}
export function sunVisibility(counted,open,size=16){
 const full=size*size;return counted===full?open/full:Math.max((open-full/2)/(full/2),0);
}

export async function createSunFlare(root='/assets/SUN_FLARE/'){
 const pkg=await (await fetch(root+'sun-flare.json')).json();
 if(pkg.version!==1||pkg.flares?.length!==9)throw Error('Unsupported sun flare package');
 const maps={};
 await Promise.all(Object.entries(pkg.textures).map(async([name,t])=>{
  const bytes=new Uint8Array(await (await fetch(root+t.file)).arrayBuffer());if(bytes.length!==t.width*t.height*4)throw Error('Sun texture extent');
  const map = new DataTexture(bytes, t.width, t.height, RGBAFormat, UnsignedByteType);
  map.minFilter = map.magFilter = LinearFilter;
  map.wrapS = map.wrapT = ClampToEdgeWrapping;
  map.colorSpace = NoColorSpace;
  map.generateMipmaps = false;
  map.needsUpdate = true;
  maps[name] = map;
 }));
 const painter=createSunPainter(pkg),c=pkg.constants,[VW,VH]=c.viewport;
 // Per-frame uniforms: sun in viewport pixels, glow bytes, flare alpha, query.
 const sunScreen=uniform(new Vector2()),visible=uniform(0),glowHalf=uniform(320),glowRgb=uniform(new Vector3(128,128,128)),glowAlpha=uniform(0),flareAlpha=uniform(0);
 const bandTop=uniform(0),bandHeight=uniform(VH);let band=[0,1];
 const sunDepth=uniform(1),near=uniform(.1),far=uniform(300),queryOrigin=uniform(new Vector2()),queryRead=uniform(new Vector2()),glowSun2=uniform(0);
 const visTarget=new RenderTarget(1,1,{type:HalfFloatType,depthBuffer:false});visTarget.texture.minFilter=visTarget.texture.magFilter=NearestFilter;
 let visQuad=null,visMaterial=null;const eye=new Vector3(),point=new Vector3(),ndc=new Vector3(),view=new Vector3();
 let frameState={visible:false};
 const visibility=texture(visTarget.texture,vec2(.5,.5)).r;
 // Sample the GS texture as bytes: Cs = T*Vc>>7, As = Ta*Va>>7, add = Cs*As>>7.
 const spriteAdd=(map,centre,half,uv,rgb,alpha)=>{
  const p=vec2(screenUV.x.mul(VW),screenUV.y.mul(bandHeight).add(bandTop)),local=p.sub(centre.sub(half)).div(half.mul(2));
  const inside=local.x.greaterThanEqual(0).and(local.x.lessThan(1)).and(local.y.greaterThanEqual(0)).and(local.y.lessThan(1));
  const st=vec2(uv.x.add(local.x.mul(uv.z.sub(uv.x))),uv.y.add(local.y.mul(uv.w.sub(uv.y))));
  const t=texture(map,st).mul(255).round();
  const cs=min(floor(t.rgb.mul(rgb).div(128)),255),as=floor(t.a.mul(alpha).div(128));
  return select(inside,floor(cs.mul(as).div(128)),vec4(0).rgb);
 };
 const flareNodes=pkg.flares.map(()=>({centre:uniform(new Vector2()),half:uniform(0),uv:uniform(new Vector4()),rgb:uniform(new Vector3()),alpha:uniform(0)}));
 const api={
  painter,pkg,enabled:true,warm:false,
  tick(core){painter.tick(core);},
  setTree(next){painter.setTree(next);},
  // Encoded 0..255 RGB after fog, before ScreenTint.
  apply(bytes){
   const va=v=>floor(v.mul(visibility).mul(128));
   let sum=select(glowSun2.greaterThan(.5),spriteAdd(maps.sun2,sunScreen,glowHalf,vec4(0,0,1,1),glowRgb,va(glowAlpha)),spriteAdd(maps.sun1,sunScreen,glowHalf,vec4(0,0,1,1),glowRgb,va(glowAlpha)));
   for(const n of flareNodes)sum=sum.add(spriteAdd(maps.lens,n.centre,n.half,n.uv,n.rgb,va(n.alpha)));
   return select(visible.greaterThan(.5),min(bytes.add(sum),255),bytes);
  },
  // Z query on the world depth after the opaque world pass.
  attach(renderer,worldPass){
   const depthTexture=worldPass.renderTarget.depthTexture;if(!depthTexture)throw Error('Sun query requires scene depth');
   visMaterial=new MeshBasicNodeMaterial({depthTest:false,depthWrite:false,toneMapped:false,fog:false});
   visMaterial.fragmentNode=Fn(()=>{
    const open=float(0).toVar(),counted=float(0).toVar();
    Loop({start:int(0),end:int(c.query_size),name:'qy'},({qy})=>{Loop({start:int(0),end:int(c.query_size),name:'qx'},({qx})=>{
     const px=queryRead.x.add(float(qx)),py=queryRead.y.add(float(qy));
     const inRect=px.greaterThanEqual(queryOrigin.x).and(px.lessThan(queryOrigin.x.add(c.query_size))).and(py.greaterThanEqual(queryOrigin.y)).and(py.lessThan(queryOrigin.y.add(c.query_size)));
     // Lines outside the 3D band (16:9 letterbox) hold no world depth: open.
     const v=py.add(.5).sub(bandTop).div(bandHeight),depth=texture(depthTexture,vec2(px.add(.5).div(VW),v.clamp(0,1))).r;
     const behind=perspectiveDepthToViewZ(depth,near,far).negate().greaterThanEqual(sunDepth).or(v.lessThan(0)).or(v.greaterThan(1));
     counted.addAssign(select(inRect,1,0));open.addAssign(select(inRect.and(behind),1,0));
    });});
    const full=c.query_size*c.query_size;
    return vec4(select(counted.equal(full),open.div(full),max(open.sub(full/2).div(full/2),0)),0,0,1);
   })();
   visQuad=new QuadMesh(visMaterial);
   const update=worldPass.updateBefore.bind(worldPass);
   worldPass.updateBefore=frame=>{update(frame);if(!frameState.visible&&!api.warm)return;
   // warm (main.js loading-screen warm-up): the Z query pipeline builds even with the sun out of view (its result is unused then)
const prior=renderer.getRenderTarget();try{renderer.setRenderTarget(visTarget);visQuad.render(renderer);}finally{renderer.setRenderTarget(prior);}};
  },
  // Once per rendered frame with the displayed camera (2F4DB8 + 2F4A08 setup).
  update(camera){
   const v=painter.values;camera.updateMatrixWorld();
   if(!api.enabled||v.texture===-1||!pkg.sun_textures[v.texture]){visible.value=0;frameState={visible:false};return frameState;}
   const dir=sunDirection(v,c).web;eye.setFromMatrixPosition(camera.matrixWorld);
   point.copy(eye).addScaledVector(new Vector3(...dir),camera.far-c.far_inset_cm/100);
   view.copy(point).applyMatrix4(camera.matrixWorldInverse);ndc.copy(point).project(camera);
   const inside=view.z<0&&Math.abs(ndc.x)<=1&&Math.abs(ndc.y)<=1&&ndc.z<=1;
   if(!inside){visible.value=0;frameState={visible:false};return frameState;}
   const screen=[(ndc.x+1)*.5*VW,band[0]*VH+(1-ndc.y)*.5*band[1]*VH],q=sunQueryRect(screen,c);bandTop.value=band[0]*VH;bandHeight.value=band[1]*VH;
   sunScreen.value.set(...screen);queryOrigin.value.set(...q.origin);queryRead.value.set(...q.read);sunDepth.value=-view.z;near.value=camera.near;far.value=camera.far;
   const sprites=sunSprites(pkg,v,screen,1);
   glowHalf.value=sprites[0].half;glowRgb.value.set(...sprites[0].rgb);glowAlpha.value=f(v.glowAlpha);glowSun2.value=v.texture===1?1:0;
   sprites.slice(1).forEach((s, i) => {
     const n = flareNodes[i];
     n.centre.value.set(...s.centre);
     n.half.value = s.half;
     n.uv.value.set(...s.uv);
     n.rgb.value.set(...s.rgb);
     n.alpha.value = f(pkg.flares[i].argb[0] * v.flareAlpha);
   });
   visible.value=1;frameState={visible:true,screen,query:q,depth:-view.z};return frameState;
  },
  // Widescreen band (widescreen.js view.band): 3D viewport lines of the 448-line buffer.
  setBand(value){band=[value[0],value[1]];},
  get state(){return {...painter.state,values:painter.values,frame:frameState};},
  dispose(){visTarget.dispose();visMaterial?.dispose();Object.values(maps).forEach(m=>m.dispose());}
 };
 (globalThis.ssxEffects??={}).sun=api;return api;
}
