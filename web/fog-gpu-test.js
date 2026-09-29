import * as T from 'three/webgpu';
import {createFogRenderer} from './fog-renderer.js';
const result=document.querySelector('#result');
try{
 const renderer=new T.WebGPURenderer({antialias:false,forceWebGL:new URL(location.href).searchParams.get('backend')==='webgl'});renderer.setSize(16,16);renderer.outputColorSpace=T.SRGBColorSpace;renderer.toneMapping=T.NoToneMapping;await renderer.init();
 const scene=new T.Scene(),sky=new T.Scene();sky.background=new T.Color(0);
 const camera=new T.PerspectiveCamera(60,1,.1,10);camera.position.z=2;
 scene.add(new T.Mesh(new T.PlaneGeometry(4,4),new T.MeshBasicMaterial({color:0xff0000,fog:false})));
 const buffer=new ArrayBuffer(2048),bytes=new Uint8Array(buffer),floats=new Float32Array(buffer);
 const core={HEAPU8:bytes,HEAPF32:floats,_fog_palette_rgba:()=>0,_fog_palette_info:()=>1024};
 const info=new Float32Array(buffer,1024,9);info.set([1,1,255,0,0,0,2,1,49]);
 const fog=createFogRenderer(renderer,scene,sky,camera),target=new T.RenderTarget(16,16,{type:T.UnsignedByteType,depthBuffer:false});
 async function draw(alpha){for(let i=0;i<256;i++)bytes.set([64,128,192,alpha],i*4);info[1]++;fog.update(core);renderer.setRenderTarget(target);fog.render();return Array.from(await renderer.readRenderTargetPixelsAsync(target,8,8,1,1)).slice(0,3);}
 const source=await draw(128);if(source.join(',')!=='255,0,0')throw Error('Opaque scene color changed: '+source);const cases=[];
 for(const alpha of [0,1,32,64,96,127,128]){const actual=await draw(alpha);const expected=source.map((v,i)=>Math.max(0,Math.min(255,Math.floor((v-[64,128,192][i])*alpha/128)+[64,128,192][i])));if(actual.some((v,i)=>v!==expected[i]))throw Error(JSON.stringify({alpha,actual,expected}));cases.push({alpha,actual});}
 const indices=[];
 for(const rawDepth of [128,384,3968,4224,32640,32896,65152,65408,65534,65535,65536,65664,131071,16777215]){
  for(let i=0;i<256;i++)bytes.set([i,0,255-i,0],i*4);
  info[4]=0;info[5]=rawDepth*200;info[1]++;fog.update(core);renderer.setRenderTarget(target);fog.render();
  const actual=Array.from(await renderer.readRenderTargetPixelsAsync(target,8,8,1,1)).slice(0,3),index=(rawDepth>>>8)&255;
  const expected=rawDepth<65535?[index,0,255-index]:source;if(actual.join(',')!==expected.join(','))throw Error(JSON.stringify({rawDepth,actual,index}));indices.push(index);
 }
 const uploads=fog.uploads;fog.update(core);if(fog.uploads!==uploads)throw Error('Unchanged palette uploaded again');
 result.textContent=JSON.stringify({passed:true,indices,uploads,backend:renderer.backend.isWebGPUBackend?'WebGPU':'WebGL2',source,cases},null,2);
 fog.dispose();target.dispose();renderer.dispose();
}catch(error){result.textContent='FAILED: '+error.stack;}
