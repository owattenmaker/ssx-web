import * as T from 'three/webgpu';
import {Fn,uniform,vec4,floor,float,uint,bitAnd,bitXor,screenCoordinate} from 'three/tsl';
import {riderHighlight2Node} from './rider-lighting-nodes.js';
const result=document.querySelector('#result');
let renderer,target,pipeline;
try{
 const response=await fetch('/test-data/rider-texture-reference.bin');
 if(!response.ok)throw Error('Missing generated original GS texture-combine reference');
 const reference=new Uint8Array(await response.arrayBuffer()),rims=[0,1,127,128,254,255],size=256;
 if(reference.length!==rims.length*size*size*4)throw Error('Unexpected reference extent');
 renderer=new T.WebGPURenderer({antialias:false,forceWebGL:new URL(location.href).searchParams.get('backend')==='webgl'});
 renderer.setSize(size,size);renderer.toneMapping=T.NoToneMapping;await renderer.init();
 const rim=uniform(0);
 pipeline=new T.RenderPipeline(renderer);pipeline.outputColorTransform=false;
 pipeline.outputNode=Fn(()=>{
  const x=floor(screenCoordinate.x),y=floor(screenCoordinate.y);
  const tex=vec4(x,float(bitXor(uint(x),uint(0x55))),float(255).sub(x),float(bitAnd(uint(x.add(y)),uint(255))));
  const light=vec4(y,float(255).sub(y),float(bitXor(uint(y),uint(0xaa))),rim);
  return riderHighlight2Node(tex,light).div(255);
 })();
 target=new T.RenderTarget(size,size,{type:T.UnsignedByteType,depthBuffer:false});
 const cases=[];
 for(let batch=0;batch<rims.length;batch++){
  rim.value=rims[batch];renderer.setRenderTarget(target);pipeline.render();
  const actual=await renderer.readRenderTargetPixelsAsync(target,0,0,size,size);
  for(let y=0;y<size;y++)for(let x=0;x<size;x++)for(let lane=0;lane<4;lane++){
   // ScreenNode uses top-left coordinates; raw WebGL readback starts at bottom-left.
   const row=renderer.backend.isWebGPUBackend?y:size-1-y;
   const expected=reference[(batch*size*size+row*size+x)*4+lane];
   if(actual[(y*size+x)*4+lane]!==expected)throw Error(JSON.stringify({rim:rims[batch],x,y,lane,actual:actual[(y*size+x)*4+lane],expected}));
  }
  cases.push({rim:rims[batch],pixels:size*size});
 }
 result.textContent = JSON.stringify(
   {
     passed: true,
     backend: renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2',
     pixels: rims.length * size * size,
     lanes: 4,
     cases,
     scope: 'HIGHLIGHT2 texture/light combination only; no live rider lighting or framebuffer blend claim'
   },
   null,
   2
 );
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{pipeline?.dispose();target?.dispose();renderer?.dispose();}
