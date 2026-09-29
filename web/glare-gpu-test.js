import * as T from 'three/webgpu';
import {texture,screenUV,vec4} from 'three/tsl';
import {createGlarePass,glareBytes,glareReference} from './glare-pass.js';
// GPU glare pass (glare-pass.js) against its byte-exact CPU model (glareReference) on a
// synthetic 512x448 pre-tint frame with the live Metro-City glide-620 parameters.
// ?backend=webgl runs the WebGL2 backend.
const result=document.querySelector('#result');const errors=[];console.error=(...a)=>{errors.push(a.map(String).join(' '));};
let renderer;
try{
 const W=512,H=448,frame=new Uint8Array(W*H*4);let seed=1;const rnd=()=>((seed=(seed*1103515245+12345)>>>0)>>>16)&255;
 for(let y=0;y<H;y++)for(let x=0;x<W;x++){const i=(y*W+x)*4,sky=y<150,block=((x>>5)+(y>>5))%5===0;
  frame.set(sky?[200+(x&31),210+(y&31)%40,250,255]:block?[255,250,rnd()]:[40+(rnd()&31),60+(x&63),90+(y&63),255],i);frame[i+3]=255;}
 const context=[1.2406212091445923,1,1,1.2005192041397095,0.9598925709724426,0,0.4010432958602905];
 const pkg={version:1,location:'TEST',fields:['cutoff','post_cutoff_scale','copy_intensity','frame_source_intensity','frame_blend_intensity','blend_texture2','blend_texture3'],defaults:[1,1,1,1,1,0,0],debug:{enable:1,capture_log2:8,blend_texture0:0,blend_texture1:0,jitter:2},painter:null};
 renderer=new T.WebGPURenderer({antialias:false,forceWebGL:new URL(location.href).searchParams.get('backend')==='webgl'});renderer.setPixelRatio(1);renderer.setSize(W,H);renderer.toneMapping=T.NoToneMapping;await renderer.init();
 const glare=await createGlarePass(pkg);glare.painter.seed(context);glare.attach(renderer,b=>b);
 const source=new T.DataTexture(frame,W,H,T.RGBAFormat,T.UnsignedByteType);source.flipY=false;source.minFilter=source.magFilter=T.NearestFilter;source.colorSpace=T.NoColorSpace;source.needsUpdate=true;
 const copy=new T.MeshBasicNodeMaterial({depthTest:false,depthWrite:false,toneMapped:false,fog:false});copy.fragmentNode=vec4(texture(source,screenUV).rgb,1);const quad=new T.QuadMesh(copy);
 const out=new T.RenderTarget(W,H,{type:T.UnsignedByteType,depthBuffer:false});
 const target=glare.begin();if(!target)throw Error('glare inactive');renderer.setRenderTarget(target);quad.render(renderer);renderer.setRenderTarget(out);glare.end();
 const reference=glareReference(frame,glareBytes(context));
 const read=async(t,w,h)=>{let px=await renderer.readRenderTargetPixelsAsync(t,0,0,w,h);if(px.length>w*h*4){const row=Math.ceil(w*4/256)*256,u=new Uint8Array(w*h*4);for(let y=0;y<h;y++)u.set(px.subarray(y*row,y*row+w*4),y*w*4);px=u;}/* WebGPU rows are padded to 256 bytes */if(renderer.backend.isWebGPUBackend)return px;const f=new Uint8Array(px.length);for(let y=0;y<h;y++)f.set(px.subarray((h-1-y)*w*4,(h-y)*w*4),y*w*4);return f;};
 const compare=(a,b,w,h)=>{let mism=0,max=0,sum=0;for(let i=0;i<w*h*4;i++){if(i%4===3)continue;const e=Math.abs(a[i]-b[i]);if(e){mism++;max=Math.max(max,e);sum+=e;}}return {mismatchedLanes:mism,maxByteError:max,meanError:+(sum/(w*h*3)).toFixed(4)};};
 const levels=[];for(let k=0;k<4;k++){const s=256>>k;levels.push(compare(await read(glare.levels[k],s,s),reference.levels[k],s,s));}
 const final=compare(await read(out,W,H),reference.frame,W,H);
 if(errors.length)throw Error(errors.join('\n'));
 result.textContent=JSON.stringify({backend:renderer.backend.isWebGPUBackend?'WebGPU':'WebGL2',levels,final,passed:final.maxByteError<=1&&levels.every(l=>l.maxByteError<=1)},null,1);
}catch(e){result.textContent='FAILED: '+e.stack+'\n'+errors.join('\n');}
