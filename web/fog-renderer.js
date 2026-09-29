import {attachEncodedSnowComposite} from './snow-composite.js';import {copyFogState} from './fog-shared.js';
import {depthStencilPassOptions} from './depth-stencil-target.js';
import {pv} from './pv-flags.js';
import {DataTexture,RGBAFormat,UnsignedByteType,NearestFilter,NoColorSpace,RenderPipeline} from 'three/webgpu';
import {Fn,pass,texture,uniform,vec2,vec4,floor,round,mod,clamp,mix,select,screenUV} from 'three/tsl';import {fromFrame} from './frame-space.js';/* the frame's colour space (pv encodedBlend) */

// Source-derived depth/CLUT/encoded-byte blend. The scene's lighting and GS
// rasterization are not reproduced by this pass; see fog-painter-recovery.md.
export function createFogRenderer(renderer,scene,skyScene,camera,stage='',snow=null,tint=null,sun=null,glow=null,glare=null){
 const palette=new DataTexture(new Uint8Array(1024),256,1,RGBAFormat,UnsignedByteType);
 palette.minFilter=palette.magFilter=NearestFilter;palette.generateMipmaps=false;palette.colorSpace=NoColorSpace;palette.needsUpdate=true;
 const slope=uniform(0),offset=uniform(0),enabled=uniform(0);
 const skyPass=pass(skyScene,camera),worldPass=pass(scene,camera,depthStencilPassOptions());
 const skyBackground=skyPass.getTextureNode().sample(screenUV);
 const sceneColor=worldPass.getTextureNode().sample(screenUV),worldEncoded=fromFrame(sceneColor.rgb);
 const depthCm=worldPass.getViewZNode().negate().mul(100).max(.0001);
 const originalZ=floor(depthCm.mul(slope).add(offset).mul(depthCm.reciprocal())).max(0);
 const entry=texture(palette,vec2(mod(floor(originalZ.div(256)),256).add(.5).div(256),.5));
 // Encoded 0..1 colour -> fogged GS bytes (36AC00 CLUT blend on the world depth).
 const fogBytes=encoded=>{
  const source=round(clamp(encoded,0,1).mul(255)),fog=round(entry.rgb.mul(255)),alpha=round(entry.a.mul(255));
  const blended=clamp(floor(source.sub(fog).mul(alpha).div(128)).add(fog),0,255);
  return select(originalZ.lessThan(65535),mix(source,blended,enabled),source);
 };
 const worldFogged=fogBytes(worldEncoded);
 // Snow/wake/boost/start sparks are priority 7 (370950, 2DDC4C, 2E7BDC set word2 bits5..9):
 // 363490 draws them after the fog composite, so they blend onto the fogged world, unfogged.
 const snowComposite=snow?attachEncodedSnowComposite(renderer,worldPass,scene,camera,snow,worldFogged.div(255)):null;
 sun?.attach(renderer,worldPass);
 glow?.attach(renderer,worldPass);
 const pipeline=new RenderPipeline(renderer);pipeline.outputColorTransform=false;if(pipeline._quadMesh?.material)pipeline._quadMesh.material.name='FogComposite';/* pipeline label in field reports (web/diagnostics.js) */
 pipeline.outputNode=Fn(()=>{
  if(stage==='palette')return vec4(entry.rgb,1);
  if(stage==='color')return vec4(worldEncoded,1);
  // Each stage's result goes to a shader var (.toVar() on this Fn's stack) before the next stage selects on it: a
  // select() emits its input in both branches, so without the vars every nested select (snow, glow, sun, glare, tint,
  // lightning) doubled the whole upstream graph, and the WGSL (~230 KB, ~950 private vars, 10.6 KB) broke WebKit's
  // 8 KB private address space limit: the iPhone/Safari pipeline failed and the frame was not drawn. Same values.
  const fogged=(snowComposite?round(clamp(snowComposite.encodedColour,0,1).mul(255)):worldFogged).toVar();
  // Sun glow/lens flare (priority 8, additive 0x48) sit between the fog composite and
  // ScreenTint (3904A0) in 363490; see sun-flare.js and screen-tint.js.
  // Light glow halos (2E2868, priority 7, additive 0x48) precede the sun; see light-glow.js.
  const glowed=glow?glow.apply(fogged).toVar():fogged,lit=sun?sun.apply(glowed).toVar():glowed;
  // Glare 36C790 runs on this frame just before ScreenTint (see glare-pass.js): while it is
  // active the untinted frame goes to its target and the glare's final pass applies the tint.
  return vec4((glare?select(glare.capturing.greaterThan(0),lit,finish(lit)):finish(lit)).div(255),1);
 })();
 function finish(bytes){return tint?tint.apply(bytes):bytes;}
 // PS2 softness (Options > Display & Touch, quality.ps2Output; docs/visual-parity.md section 30): the scene colour horizontally
 // low-passed by one 640-frame pixel before the fog composite, like the PS2's 512 -> 640 picture on a TV: four bilinear taps at
 // +-0.25 / +-0.75 of a 640-frame pixel (exactly [1,2,1]/4 at 640 wide, [1,2,2,2,1]/8 at 1280). The HUD and menus are other canvases.
 // Built only when switched on, so the default composite's shader and pixels are today's.
 const baseOutput=pipeline.outputNode;let softOutput=null,softSeed=null,soft=false;
 function buildSoft(){
  const tex=worldPass.getTextureNode(),tap=k=>tex.sample(screenUV.add(vec2(k/640,0)));
  const softColour=tap(-.75).add(tap(-.25)).add(tap(.25)).add(tap(.75)).mul(.25),softEncoded=fromFrame(softColour.rgb),softFogged=fogBytes(softEncoded);
  softSeed=snowComposite?softFogged.div(255):null;const softSnow=snowComposite?snowComposite.seeded(softSeed):null;
  return Fn(()=>{
   if(stage==='palette')return vec4(entry.rgb,1);
   if(stage==='color')return vec4(softEncoded,1);
   const fogged=(softSnow?round(clamp(softSnow,0,1).mul(255)):softFogged).toVar();
   const glowed=glow?glow.apply(fogged).toVar():fogged,lit=sun?sun.apply(glowed).toVar():glowed;
   return vec4((glare?select(glare.capturing.greaterThan(0),lit,finish(lit)):finish(lit)).div(255),1);
  })();
 }
 function setSoftness(on){on=!!on;if(on===soft)return;soft=on;
  if(on&&!softOutput)softOutput=buildSoft();
  pipeline.outputNode=on?softOutput:baseOutput;snowComposite?.useSeed(on?softSeed:null);pipeline.needsUpdate=true;}
 glare?.attach(renderer,finish);
 let revision=-1,uploads=0;
 return {
  update(core){
   const info=new Float32Array(core.HEAPF32.buffer,core._fog_palette_info(),9);enabled.value=info[0];slope.value=info[4];offset.value=info[5];
   if(info[0]&&revision!==info[1]){palette.image.data.set(core.HEAPU8.subarray(core._fog_palette_rgba(),core._fog_palette_rgba()+1024));palette.needsUpdate=true;revision=info[1];uploads++;}
   copyFogState(info,info[0]?core.HEAPU8.subarray(core._fog_palette_rgba(),core._fog_palette_rgba()+1024):null);   // pv byteBlend: the encoded-pass world additives (web/fog-shared.js)
  },
  render(){
   sun?.update(camera);glow?.update(camera);const background=scene.backgroundNode;scene.backgroundNode=skyBackground;
   try{
    const glareTarget=glare?.begin();
    if(glareTarget){const prior=renderer.getRenderTarget();renderer.setRenderTarget(glareTarget);try{pipeline.render();}finally{renderer.setRenderTarget(prior);}glare.end();}
    else pipeline.render();
   }finally{scene.backgroundNode=background;}
  },
  // Race warm-up (main.js warmupRender): compile the world and sky passes into their own render targets
  // (MRT, sample count, output type as PassNode.setup sets them) so no pipeline links on a race frame.
  async compileAsync(){
   const background=scene.backgroundNode;scene.backgroundNode=skyBackground;
   try{for(const p of [worldPass,skyPass]){p.renderTarget.samples=p.options?.samples===undefined?renderer.samples:p.options.samples;p.renderTarget.texture.type=renderer.getOutputBufferType();await p.compileAsync(renderer);}}
   finally{scene.backgroundNode=background;}
  },
  // Compile one object for the world pass: its render target (formats, sample count) and MRT, as the race frame draws it
  // (cutscene actors and sets, web/cutscenes.js host.compile). renderer.compileAsync on its own compiles for the canvas,
  // whose pipelines the world pass never uses (docs/firefox-load.md). The context is taken synchronously, so the target is
  // restored before the compile yields.
  // depth: the render-context depth to compile at (pv rideWarm passes 1, the world pass's own; default: 1 with pv streamWarm, else 0)
  compileObject(object,{depth:want=null}={}){
   const target=worldPass.renderTarget,priorTarget=renderer.getRenderTarget(),priorMRT=renderer.getMRT();
   target.samples=worldPass.options?.samples===undefined?renderer.samples:worldPass.options.samples;target.texture.type=renderer.getOutputBufferType();
   // hidden parts shown later (the rider's board, the PDA prop) compile too: the object list is taken synchronously
   const hidden=[];object.traverse(o=>{if(!o.visible){hidden.push(o);o.visible=true;}});
   renderer.setRenderTarget(target);renderer.setMRT(worldPass.getMRT?.()??null);
   // pv streamWarm: three keys a render context by its call depth too, and the world pass draws nested in the post pipeline (depth 1)
   // while compileAsync takes depth 0: every material compiled here got another context id in its cache key and was built again on
   // its first world-pass draw (the stations of the streamed world, the NIS actors; docs/ctm-flow.md "Start and streaming").
   const contexts=renderer._renderContexts,get=contexts?.get,d=want??(pv('streamWarm')?1:0),depth=d&&get?d:0;if(depth)contexts.get=function(t,m,d){return get.call(this,t,m,d||depth);};
   try{return renderer.compileAsync(object,camera,scene);}finally{if(depth)delete contexts.get;renderer.setRenderTarget(priorTarget);renderer.setMRT(priorMRT);for(const o of hidden)o.visible=false;}
  },
  setSoftness,get softness(){return soft;},
  get uploads(){return uploads;},
  get snowCompositeStats(){return snowComposite?{...snowComposite.stats}:null;},
  dispose(){snowComposite?.dispose();pipeline.dispose();worldPass.dispose();skyPass.dispose();palette.dispose();}
 };
}
