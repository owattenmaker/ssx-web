import * as T from 'three/webgpu';
import {createSnowRenderer} from './snow-renderer.js';import {encodedFrame} from './frame-space.js';
const result=document.querySelector('#result'),originalFetch=globalThis.fetch;let renderer,target;
try{
 const asset=await (await originalFetch('/assets/SNOW_FX/snow-fx.json')).json();
 // Controlled white texture isolates the actual material's colour combination
 // from authored texture patterns. Keep original texture extents/alpha encoding.
 const textures=new Map(asset.textures.map(t=>['/assets/SNOW_FX/'+(t.gs_alpha_file||t.file),t]));
 globalThis.fetch=async url=>{const entry=textures.get(String(url));if(!entry)return originalFetch(url);const bytes=new Uint8Array(entry.width*entry.height*4).fill(255);if(entry.gs_alpha_file)for(let i=3;i<bytes.length;i+=4)bytes[i]=128;return new Response(bytes);};
 const memory=new Float32Array(4096),info=memory.subarray(4,27);for(let i=0;i<10;i++)info[10+i]=8;info[1]=1;info[20]=1;
 for(const p of asset.profiles)memory[800+p.emitter_index]=p.parameters.TextureId;
 memory.set([0,0,0,50,1,1,1,.5],192);
 const core={HEAPF32:memory,_snow_info:()=>16,_snow_particles:i=>(128+i*64)*4,_snow_flipbook_info:()=>3200};
 const snow=await createSnowRenderer(new T.Vector3(),core);globalThis.fetch=originalFetch;
 const scene=new T.Scene();scene.background=new T.Color(0);scene.add(snow.group);const camera=new T.PerspectiveCamera(60,1,.1,100);camera.position.z=2;
 renderer=new T.WebGPURenderer({antialias:false,forceWebGL:new URL(location.href).searchParams.get('backend')==='webgl'});renderer.setSize(64,64);await renderer.init();target=new T.RenderTarget(64,64,{depthBuffer:false});renderer.setRenderTarget(target);
 const samples=[];
 for(const multiplier of [.5,1,255/128]){
  memory.set([multiplier,multiplier,multiplier],196);info[20]++;snow.update(core,camera);renderer.render(scene,camera);
  const pixels=await renderer.readRenderTargetPixelsAsync(target,0,0,64,64),at=(32*64+32)*4;const rgb=Array.from(pixels.slice(at,at+3));
  const c=Math.min(multiplier,1),linear=c<=.04045?c/12.92:((c+.055)/1.055)**2.4,expected=Math.round((encodedFrame?c:linear)*.5*255);   // pv encodedBlend: the frame holds the encoded value (web/frame-space.js)
  if(rgb.some(v=>Math.abs(v-expected)>1))throw Error(`Colour ${multiplier}: ${rgb} expected ${expected}`);samples.push({multiplier,rgb,expected});
 }
 if(samples[1].rgb.some((v,i)=>v!==samples[2].rgb[i]))throw Error('Overrange colour leaked into blending');
 result.textContent=JSON.stringify({passed:true,backend:renderer.backend.isWebGPUBackend?'WebGPU':'WebGL2',samples,frame:encodedFrame?'encoded':'linear',scope:'Actual snow material clamps RGB before blending in the frame space (web/frame-space.js). Excludes original encoded-space blend, GS sampling/rounding and full-scene parity.'},null,2);
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{globalThis.fetch=originalFetch;target?.dispose();renderer?.dispose();}
