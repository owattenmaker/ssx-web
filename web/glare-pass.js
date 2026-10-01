import {Vector2,Vector4,RenderTarget,UnsignedByteType,NearestFilter,NoColorSpace,MeshBasicNodeMaterial,QuadMesh,RenderPipeline} from 'three/webgpu';
import {Fn,uniform,texture,textureLoad,ivec2,vec2,vec4,float,floor,min,max,clamp,select,screenCoordinate,screenUV} from 'three/tsl';
import {mul,div,add,sub} from './ee-scalar-float.js';
import {followRegion,followWorldLoad} from './painter-regions.js';

// Original framebuffer glare pass (SLUS_207.72), see terrain-render-fidelity.md
// "Framebuffer glare pass (36C790)".
//  World painter type 6 (factory 2C0408, ctor 2BC830, vtable 484DA8): rate at +0, then
//   current/sample pairs +8/+C .. +38/+3C for seven values. Blend 2BD068 (w = weight^2,
//   current = w*payload + (1-w)*current), compare 2BDBD0, reset 2BE140 -> (1,1,1,1,1,0,0).
//   Getters 2C14F0..2C1520 via 2EEDB0..2EEF60 (environment slot +8); 2F00A0 copies them to
//   gp+12E4..12F4/1300/1304 (skipped when the debug "Override World Painter" gp+12DC is set),
//   renderer vfunc 386640 -> 36C740 to render context +6CD4 + view*0x1C (same timing as
//   ScreenTint 390458). Debug-menu names (249200): Minimum Intensity Cutoff, Post-Cutoff
//   Scale, Copy Intensity, Frame Source Intensity, Frame Blend Intensity, Blend Texture 2/3.
//  36C790 (363490, after layer 1 and before ScreenTint 3904A0), per view:
//   return unless gp+12D8 (Enable, 1) and one of A = trunc(127.5 * (gp+12F8 "Blend Texture 0",
//   gp+12FC "Blend Texture 1", BT2, BT3)) is non-zero (BT0/1 are debug values, 0 in retail).
//   L0 (2^gp+12E0 = 256 square, in the Z buffer at block ctx+5A80<<5): 36B9D8 bilinear copy
//    of the 512x448 viewport, MODULATE by trunc(FrameSource*127.5), no blend.
//   36C188: untextured sprites, ALPHA (Cd-Cs)*FIX>>7 with Cs = trunc(Cutoff*127.5),
//    FIX = trunc(PostCutoffScale*127.5).
//   for i in 0..2: if A[i]: composite L_i; L_{i+1} = 36B9D8 of L_i at half size: four
//    bilinear samples at +-J (J = trunc(Jitter*16) 1/16 texel, gp+1308 = 2.0) each MODULATE
//    trunc(Copy*127.5/4), first written, others added (ALPHA Cs+Cd, FIX 128).
//   if A[3]: composite L3. Composite 36C398: 16 column sprites over the viewport, ALPHA
//    Cd*FIX>>7 + Cs with FIX = trunc(FrameBlend*127.5), MODULATE by A[i] (RGBA A,A,A,A).
//   All GS math on bytes: MODULATE (T*C)>>7, blends clamp to 0..255, level buffers PSMCT24.
const FIELDS=['cutoff','post_cutoff_scale','copy_intensity','frame_source_intensity','frame_blend_intensity','blend_texture2','blend_texture3'];
export const GLARE_FIELDS=FIELDS;
export const GLARE_DEFAULTS=[1,1,1,1,1,0,0];
export const GLARE_RENDERER={bufferBlock:0xe0<<5,frameBlock:0,frameFbw:8,viewport:[0,0,512,448]};
const f=Math.fround,trunc=Math.trunc;
function painterPayloadIndex(tree,x,y){
 let px=f(f(x-tree.origin[0])*tree.scale),py=f(f(y-tree.origin[1])*tree.scale),leaf=null;
 if(px>-1&&px<32768&&py>-1&&py<32768){
  let a=((trunc(px)<<1)&0xffff)>>>0,b=((trunc(py)<<1)&0xffff)>>>0,index=tree.root;
  for(let steps=0;steps<=tree.nodes.length;steps++){
   const node=tree.nodes[index];if(!node)throw Error('Glare painter node outside package');
   if(!(node[0]&1)){leaf=node;break;}
   index=node[((a>>15)<<1)|(b>>15)]>>1;a=(a<<1)&0xfffc;b=(b<<1)&0xfffc;
  }
  if(!leaf)throw Error('Cyclic glare painter tree');
 }
 const value=leaf?((leaf[2]|(leaf[3]<<16))>>>0):tree.outside_words[1];
 return value===0xffffffff?-1:value;
}
// Painter state + driver 2C0778 (same X/Y source and step rule as Fog/ScreenTint/Sun).
export function createGlarePainter(pkg){
 let tree=pkg.painter,payloads=(tree?.payloads??[]).map(p=>FIELDS.map(k=>f(p[k])));const defaults=GLARE_DEFAULTS.map(f);
 let current=defaults.slice(),distance=-99999,lastX=0,lastY=0,lastTicks=-1,selected=-1;
 const region={track:-1,located:false};let missing=false; // regionTick (web/painter-regions.js): gp+0x770's record; missing: not loaded
 const reset=()=>{current=defaults.slice();distance=0;};
 // 2BD068: f1 = w*p, f0 = (1-w)*cur, cur = f1 + f0 (EE mul/add round toward zero).
 const blend=(values,weight)=>{const w=mul(weight,weight),c=sub(1,w);current=current.map((v,i)=>add(mul(w,values[i]),mul(c,v)));};
 function step(x,y){
  if(missing){lastX=x;lastY=y;current=defaults.slice();distance=0;selected=-1;return;} // 0x2C09D8: the class defaults, +0 = 0
  if(!tree){reset();return;}
  const initial=distance===-99999;
  if(!initial){const dx=f(lastX-x),dy=f(lastY-y);distance=f(distance+f(Math.sqrt(f(f(dx*dx)+f(dy*dy)))));}
  lastX=x;lastY=y;selected=painterPayloadIndex(tree,x,y);
  if(selected<0){reset();return;}
  const values=payloads[selected],rate=tree.payloads[selected].rate;
  if(values.every((v,i)=>v===current[i]))distance=0;
  if(initial){blend(values,-1);distance=0;return;}
  if(rate>=0&&rate<=distance){blend(values,1);distance=0;}else blend(values,-rate);
 }
 function useTree(next){tree=next??null;payloads=(tree?.payloads??[]).map(p=>FIELDS.map(k=>f(p[k])));}
 return {step,
  tick(core){const info=new Float32Array(core.HEAPF32.buffer,core._fog_info(),11),ticks=info[7];
   if(!info[10])return;if(ticks<lastTicks){current=defaults.slice();distance=-99999;selected=-1;}
   if(followWorldLoad(region,core)){current=defaults.slice();distance=0;selected=-1;} // the class defaults, +0 = 0
   if(ticks>0&&ticks!==lastTicks){const r=followRegion(region,core,'glare');if(r){missing=!r.record;if(r.record)useTree(r.doc?.painter??null);}step(info[8],info[9]);}lastTicks=ticks;},
  seed(values){current=values.map(f);},
  // Streamed Peak 1 world (web/free-ride.js): the painter region's glare section (null: none); the blend state is kept. Ignored
  // while the located records rule.
  setTree(next){if(!region.located)useTree(next);},
  get values(){return current.slice();},
  get state(){return {current:current.slice(),selected,distance};}};
}
// 36C790 byte parameters from the context values (+6CD4: the seven painter values) and
// the debug globals (gp+12E8 post-cutoff scale is read directly by 36C790, equal to +6CD4[1]
// in retail; gp+12F8/12FC blend textures 0/1; gp+1308 jitter; gp+12E0 capture log2).
export function glareBytes(values,debug={}){
 const q=v=>trunc(mul(f(v),127.5));
 const d={enable:1,capture_log2:8,blend_texture0:0,blend_texture1:0,jitter:2,post_cutoff_scale:values[1],...debug};
 const alphas=[q(d.blend_texture0),q(d.blend_texture1),q(values[5]),q(values[6])];
 const size=1<<d.capture_log2;
 return {enabled:!!d.enable&&alphas.some(a=>a!==0),alphas,cut:q(values[0]),cutScale:q(d.post_cutoff_scale),
  copy:trunc(div(mul(f(values[2]),127.5),4)),source:trunc(div(mul(f(values[3]),127.5),1)),frameBlend:q(values[4]),
  jitter:trunc(mul(f(d.jitter),16)),sizes:[size,size>>1,size>>2,size>>3]};
}
// 36B9D8 sprite list (UV/XY in 1/16 units): src rect -> dst (0,0,dw,dh), N samples jittered by J.
export function copySprites(src,dw,dh,J,N){
 const [x,y,w,h]=src,ub=x*16+J+8,U=w*16-2*J,vb=y*16+J+8,ve=(y+h)*16-J+8,cols=dw>>5,fp=dw*16; // cols: signed /32 (below 32 wide nothing is drawn)
 const offsets=[[-J,-J],[J,J],[J,-J],[-J,J]],out=[];
 for(let s=0;s<N;s++){const [du,dv]=offsets[s],sprites=[];
  for(let c=0;c<cols;c++){const X0=c*512,X1=Math.min(X0+512,fp);
   sprites.push([ub+Math.floor(X0*U/fp)+du,vb+dv,X0,0,ub+Math.floor(X1*U/fp)+du,ve+dv,X1,dh*16]);}
  out.push({abe:s>0,sprites});}
 return out;
}
// 36C398 composite sprites: 16 columns over the viewport, u step trunc(W/cols)*16, v 0..H*16.
export function compositeSprites(W,H,viewport=GLARE_RENDERER.viewport){
 const [vx,vy,vw,vh]=viewport,cols=vw>>5,step=trunc(W/cols)*16,out=[];
 for(let c=0;c<cols;c++)out.push([8+c*step,0,vx*16+c*512,vy*16,8+(c+1)*step,H*16,vx*16+(c+1)*512,(vy+vh)*16]);
 return out;
}
// The GS draw list 36C790 builds (same shape as the decoded original packets).
export function glarePlan(bytes,r=GLARE_RENDERER){
 if(!bytes.enabled)return [];
 const [vx,vy,vw,vh]=r.viewport,buffers=[r.bufferBlock,r.bufferBlock+0x400],draws=[];
 const tbw=w=>Math.max(1,w>>6),clampOf=(x,y,w,h)=>[2,2,x,x+w-1,y,y+h-1];
 const copy = (level, srcBlock, srcFbw, src, dst, dw, J, N, colour) =>
   copySprites(src, dw, dw, J, N).forEach(
     (p, sample) =>
       p.sprites.length &&
       draws.push({
         op: 'copy',
         level,
         sample,
         samples: N,
         frame: [dst >> 5, tbw(dw)],
         scissor: [0, dw - 1, 0, dw - 1],
         tex: [srcBlock, srcFbw],
         clamp: clampOf(...src),
         alpha: p.abe ? [0, 2, 2, 1, 128] : null,
         rgba: [colour, colour, colour, 128],
         sprites: p.sprites
       })
   );
 const S=bytes.sizes;let cur=buffers[0];
 copy(0,r.frameBlock,r.frameFbw,r.viewport,cur,S[0],0,1,bytes.source);
 const cols=S[0]>>5;
 draws.push({op:'cutoff',frame:[cur>>5,tbw(S[0])],scissor:[0,S[0]-1,0,S[0]-1],tex:null,clamp:null,alpha:[1,0,2,2,bytes.cutScale],rgba:[bytes.cut,bytes.cut,bytes.cut,128],
  sprites:Array.from({length:cols},(_,c)=>[c*512,0,Math.min((c+1)*512,S[0]*16),S[0]*16])});
 const composite = (i, block) => {
   const a = bytes.alphas[i];
   draws.push({
     op: 'composite',
     level: i,
     frame: [r.frameBlock >> 5, r.frameFbw],
     scissor: [vx, vx + vw - 1, vy, vy + vh - 1],
     tex: [block, tbw(S[i])],
     clamp: clampOf(0, 0, S[i], S[i]),
     alpha: [1, 2, 2, 0, bytes.frameBlend],
     rgba: [a, a, a, a],
     sprites: compositeSprites(S[i], S[i], r.viewport)
   });
 };
 for(let i=0;i<3;i++){
  if(bytes.alphas[i])composite(i,cur);
  const next=buffers[(i+1)&1];copy(i+1,cur,tbw(S[i]),[0,0,S[i],S[i]],next,S[i+1],bytes.jitter,4,bytes.copy);cur=next;
 }
 if(bytes.alphas[3])composite(3,cur);
 return draws;
}
// ---- Byte-exact CPU model of the GS work (reference for tests/GPU checks) ----
// GS bilinear: U (1/16) - 8 -> texel = U>>4, weight = U&15; (sum w*c)>>8; region clamp.
function bilinear(img,W,H,u,v,ch,maxU=W-1,maxV=H-1){
 const U=Math.floor(u)-8,V=Math.floor(v)-8,iu=U>>4,iv=V>>4,fu=U&15,fv=V&15;
 const at=(x,y)=>img[(Math.min(Math.max(y,0),maxV)*W+Math.min(Math.max(x,0),maxU))*4+ch];
 return (at(iu,iv)*(16-fu)*(16-fv)+at(iu+1,iv)*fu*(16-fv)+at(iu,iv+1)*(16-fu)*fv+at(iu+1,iv+1)*fu*fv)>>8;
}
const lerpSprite=(s,px,py)=>{const [u0,v0,x0,y0,u1,v1,x1,y1]=s;return [u0+(px*16-x0)*(u1-u0)/(x1-x0),v0+(py*16-y0)*(v1-v0)/(y1-y0)];};
// frame: Uint8Array 512x448 RGBA (encoded bytes before ScreenTint). Returns {frame, levels}.
export function glareReference(frame,bytes,r=GLARE_RENDERER){
 const [,,VW,VH]=r.viewport,out=frame.slice(),levels=[];const buffers={};
 const surface=block=>block===r.frameBlock?{img:out,w:VW,h:VH}:buffers[block];
 for(const d of glarePlan(bytes,r)){
  if(d.op==='composite'){
   const {img:L,w:LW,h:LH}=surface(d.tex[0]),a=d.rgba[0],fix=d.alpha[4];
   for(const s of d.sprites)for(let py=s[3]>>4;py<s[7]>>4;py++)for(let px=s[2]>>4;px<s[6]>>4;px++){
    const [u,v]=lerpSprite(s,px,py);
    for(let ch=0;ch<3;ch++){const i=(py*VW+px)*4+ch,cs=Math.min(255,(bilinear(L,LW,LH,u,v,ch)*a)>>7);out[i]=Math.min(255,((out[i]*fix)>>7)+cs);}
   }
   continue;
  }
  const dw=(d.scissor[1]+1),block=d.frame[0]<<5;
  if(!buffers[block]||buffers[block].w!==dw)buffers[block]={img:new Uint8Array(dw*dw*4),w:dw,h:dw};
  const dst=buffers[block].img;
  if(d.op==='cutoff'){for(let i=0;i<dst.length;i++)if(i%4!==3)dst[i]=Math.min(255,Math.max(0,((dst[i]-d.rgba[0])*d.alpha[4])>>7));levels[0]=dst.slice();continue;}
  const src=surface(d.tex[0]),c=d.rgba[0];
  for(const s of d.sprites)for(let py=0;py<dw;py++)for(let px=s[2]>>4;px<s[6]>>4;px++){
   const [u,v]=lerpSprite(s,px,py);
   for(let ch=0;ch<3;ch++){const i=(py*dw+px)*4+ch,cs=Math.min(255,(bilinear(src.img,src.w,src.h,u,v,ch)*c)>>7);dst[i]=d.alpha?Math.min(255,dst[i]+cs):cs;}
  }
  if(d.level>0&&d.sample===d.samples-1)levels[d.level]=dst.slice();
 }
 return {frame:out,levels};
}

// ---- GPU pass (WebGPU + WebGL2 through three.js TSL) ----
export async function createGlarePass(source){
 let pkg=source;if(typeof source==='string'){const response=await fetch(source);if(!response.ok)return null;pkg=await response.json();}
 if(pkg.version!==1||!Array.isArray(pkg.fields)||pkg.fields.join()!==FIELDS.join())throw Error('Unsupported glare package');
 const painter=createGlarePainter(pkg),debug={...pkg.debug};
 let bytes=glareBytes(painter.values,debug),renderer=null,finalPipeline=null,frameTarget=null,frameState={enabled:false};
 const S=bytes.sizes,VP=GLARE_RENDERER.viewport,J=uniform(bytes.jitter);
 const capturing=uniform(0),cut=uniform(0),cutScale=uniform(0),source_=uniform(0),copy=uniform(0),frameBlend=uniform(0),alphas=uniform(new Vector4());
 const target = (size) => {
   const t = new RenderTarget(size, size, { type: UnsignedByteType, depthBuffer: false });
   t.texture.minFilter = t.texture.magFilter = NearestFilter;
   t.texture.generateMipmaps = false;
   t.texture.colorSpace = NoColorSpace;
   return t;
 };
 const levels=S.map(target);
 const gsPixel=()=>vec2(floor(screenCoordinate.x),floor(screenCoordinate.y));
 // Manual GS bilinear (see bilinear() above) of a level or of the frame target scaled to its
 // resolution: u,v in 1/16 texels of the GS surface, maxX/maxY the last texel (region clamp).
 const bilinearNode=(map,maxX,maxY,u,v,sx=1,sy=1)=>{
  const U=floor(u.mul(sx)).sub(8),V=floor(v.mul(sy)).sub(8),iu=floor(U.div(16)),iv=floor(V.div(16)),fu=U.sub(iu.mul(16)),fv=V.sub(iv.mul(16)),gu=float(16).sub(fu),gv=float(16).sub(fv);
  const at=(x,y)=>textureLoad(map,ivec2(clamp(x,0,maxX),clamp(y,0,maxY))).rgb.mul(255).round();
  return floor(at(iu,iv).mul(gu.mul(gv)).add(at(iu.add(1),iv).mul(fu.mul(gv))).add(at(iu,iv.add(1)).mul(gu.mul(fv))).add(at(iu.add(1),iv.add(1)).mul(fu.mul(fv))).div(256));
 };
 const frameW=uniform(512),frameH=uniform(448);
 const materials=[];const quad=node=>{const m=new MeshBasicNodeMaterial({depthTest:false,depthWrite:false,toneMapped:false,fog:false});m.fragmentNode=node;materials.push(m);return new QuadMesh(m);};
 // L0 = copy of the viewport (N=1, J=0) + 36C188 cutoff, fused.
 let l0Quad=null,levelQuads=[],frameNode=null;
 function build(frame){
  l0Quad=quad(Fn(()=>{
   const p=gsPixel(),W=S[0],u=float(8).add(p.x.mul(VP[2]*16/W)),v=float(8).add(p.y.mul(VP[3]*16/W));
   const t=bilinearNode(frame,frameW.sub(1),frameH.sub(1),u,v,frameW.div(VP[2]),frameH.div(VP[3]));
   const l0=min(floor(t.mul(source_).div(128)),255),cutoff=clamp(floor(l0.sub(cut).mul(cutScale).div(128)),0,255);
   return vec4(cutoff.div(255),1);
  })());
  levelQuads=[1,2,3].map(k=>quad(Fn(()=>{
   const p=gsPixel(),W=S[k],Sp=S[k-1],src=levels[k-1].texture;
   // 36B9D8: u = x*16+J+8 + x_dst*(Sp*16-2J)/W, samples at (-J,-J),(J,J),(J,-J),(-J,J).
   const base=J.add(8),slope=float(Sp*16).sub(J.mul(2)).div(W);
   const u=base.add(p.x.mul(slope)),v=base.add(p.y.mul(slope));
   let acc=vec4(0).rgb;
   for(const [du,dv] of [[-1,-1],[1,1],[1,-1],[-1,1]]){
    const t=bilinearNode(src,Sp-1,Sp-1,u.add(J.mul(du)),v.add(J.mul(dv)));
    acc=min(acc.add(min(floor(t.mul(copy).div(128)),255)),255);
   }
   return vec4(acc.div(255),1);
  })()));
  // 36C398 composites onto the frame, in level order; web pixel -> GS continuous pixel.
  frameNode=Fn(()=>{
   // F in a shader var per level: select() emits its input in both branches, so chaining four levels (and the tint after)
   // without vars multiplied the frame graph ~16x past WebKit's 8 KB private-memory limit (fog-renderer.js). Same values.
   let F=texture(frame,screenUV).rgb.mul(255).round().toVar();
   const x=screenUV.x.mul(VP[2]).sub(.5),y=screenUV.y.mul(VP[3]).sub(.5);
   S.forEach((W,i)=>{
    const step=trunc(W/(VP[2]>>5))*16,a=alphas[['x','y','z','w'][i]];
    const t=bilinearNode(levels[i].texture,W-1,W-1,x.mul(step/32).add(8),y.mul(W*16/VP[3]));
    const blended=min(floor(F.mul(frameBlend).div(128)).add(min(floor(t.mul(a).div(128)),255)),255);
    F=select(a.greaterThan(0),blended,F).toVar();
   });
   return F;
  })();
 }
 if(debug.capture_log2!==8)throw Error('Glare GPU pass implements the retail 2^8 capture size');
 const api={painter,pkg,debug,capturing,enabled:true,
  tick(core){painter.tick(core);},
  setTree(next){painter.setTree(next);},
  // Called by the fog renderer: finish(bytes) = ScreenTint etc. applied to the glared frame.
  attach(r,finish){
   renderer = r;
   frameTarget = new RenderTarget(1, 1, { type: UnsignedByteType, depthBuffer: false });
   frameTarget.texture.minFilter = frameTarget.texture.magFilter = NearestFilter;
   frameTarget.texture.colorSpace = NoColorSpace;
   frameTarget.texture.generateMipmaps = false;
   build(frameTarget.texture);
   finalPipeline = new RenderPipeline(r);
   finalPipeline.outputColorTransform = false;
   if (finalPipeline._quadMesh?.material) finalPipeline._quadMesh.material.name = 'GlareFinal';
   finalPipeline.outputNode = Fn(() => vec4(finish(frameNode.toVar()).div(255), 1))();
  },
  // Per rendered frame: parameters from the painter; returns the frame target when the pass runs.
  // warm (warmPost, main.js warmupRender): the race warm-up's frames run the pass whatever the values (a disabled glare draws
  // its levels and composites with zero alphas: the frame unchanged), so its pipelines and the fog composite's into the frame target
  // exist before the race; the pass reads nothing from earlier frames.
  // Every other warm frame, so the fog composite's pipelines for the canvas (glare off) are built as well.
  warm:false,warmFrames:0,
  begin(){
   bytes=glareBytes(painter.values,debug);frameState={enabled:api.enabled&&bytes.enabled,bytes};capturing.value=0;
   const forced=api.warm&&api.enabled&&(api.warmFrames++&1)===0;
   if(!(frameState.enabled||forced)||!renderer)return null;capturing.value=1;
   cut.value=bytes.cut;cutScale.value=bytes.cutScale;source_.value=bytes.source;copy.value=bytes.copy;frameBlend.value=bytes.frameBlend;J.value=bytes.jitter;alphas.value.set(...bytes.alphas);
   const size=renderer.getDrawingBufferSize(new Vector2());
   if(frameTarget.width!==size.x||frameTarget.height!==size.y)frameTarget.setSize(size.x,size.y);
   frameW.value=size.x;frameH.value=size.y;return frameTarget;
  },
  // After the pre-tint frame is in the frame target: levels, then composite + finish to the canvas.
  end(){
   const prior=renderer.getRenderTarget();
   try{renderer.setRenderTarget(levels[0]);l0Quad.render(renderer);levelQuads.forEach((q,i)=>{renderer.setRenderTarget(levels[i+1]);q.render(renderer);});}
   finally{renderer.setRenderTarget(prior);}
   finalPipeline.render();
  },
  get state(){return {...painter.state,values:painter.values,frame:frameState};},
  get levels(){return levels;},
  dispose(){levels.forEach(t=>t.dispose());frameTarget?.dispose();materials.forEach(m=>m.dispose());finalPipeline?.dispose();}
 };
 (globalThis.ssxEffects??={}).glare=api;return api;
}
