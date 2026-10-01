import * as T from 'three/webgpu';
import {Fn,ivec2,floor,screenCoordinate,textureLoad,vec4,float} from 'three/tsl';
import {riderIrradianceNode,chopAdd,chopMul} from './rider-lighting-nodes.js';
const result = document.querySelector('#result');
const shaderErrors = [];
const originalConsoleError = console.error;
console.error = (...args) => {
  shaderErrors.push(args.map(String).join(' '));
  result.textContent = 'FAILED shader: ' + shaderErrors.join('\n');
  originalConsoleError(...args);
};
let renderer, pipeline, target, inputs;
try{
 const response=await fetch('/test-data/irradiance-reference.bin');if(!response.ok)throw Error('Missing original VU reference');
 const records=new Float32Array(await response.arrayBuffer()),count=12000,width=256,height=Math.ceil(count/width);
 if(records.length!==count*48)throw Error('Reference extent differs');
 const data=new Float32Array(width*height*48);data.set(records);
 inputs=new T.DataTexture(data,width*12,height,T.RGBAFormat,T.FloatType);
 inputs.minFilter=inputs.magFilter=T.NearestFilter;inputs.generateMipmaps=false;inputs.colorSpace=T.NoColorSpace;inputs.needsUpdate=true;
 renderer=new T.WebGPURenderer({antialias:false,forceWebGL:new URL(location.href).searchParams.get('backend')==='webgl'});
 renderer.setSize(width,height);renderer.toneMapping=T.NoToneMapping;await renderer.init();
 pipeline=new T.RenderPipeline(renderer);pipeline.outputColorTransform=false;
 pipeline.outputNode=Fn(()=>{
  const x=floor(screenCoordinate.x),y=floor(screenCoordinate.y);
  const sample=i=>textureLoad(inputs,ivec2(x.mul(12).add(i),y));
  return riderIrradianceNode(Array.from({length:10},(_,i)=>sample(i+1)),sample(0).rgb).div(255);
 })();
 if(new URL(location.href).searchParams.has('diagnose')){
  pipeline.outputNode=vec4(chopAdd(float(1),float(-1e-8)),chopAdd(float(-1),float(1e-8)),chopMul(float(.1),float(.1)),1);
  target=new T.RenderTarget(width,height,{type:T.FloatType,depthBuffer:false});renderer.setRenderTarget(target);pipeline.render();
  const values=await renderer.readRenderTargetPixelsAsync(target,0,0,1,1);
  const expected=[0.9999999403953552,-0.9999999403953552,0.009999999776482582,1];if(Array.from(values).some((v,i)=>v!==expected[i]))throw Error('GPU rounding primitive differs: '+Array.from(values));
  if (shaderErrors.length) throw Error(shaderErrors.join('\n'));
  result.textContent = JSON.stringify({
    passed: true,
    diagnostic: Array.from(values),
    backend: renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2'
  });
 }else{
 target=new T.RenderTarget(width,height,{type:T.UnsignedByteType,depthBuffer:false});renderer.setRenderTarget(target);pipeline.render();
 const pixels=await renderer.readRenderTargetPixelsAsync(target,0,0,width,height);
 let mismatchedPixels=0,mismatchedLanes=0,maxByteError=0;const examples=[];
 for(let y=0;y<height;y++)for(let x=0;x<width;x++){
  const row=renderer.backend.isWebGPUBackend?y:height-1-y,index=row*width+x;if(index>=count)continue;
  let mismatch=false;
  for(let lane=0;lane<4;lane++){
   const actual=pixels[(y*width+x)*4+lane],expected=records[index*48+44+lane],error=Math.abs(actual-expected);
   maxByteError=Math.max(maxByteError,error);
   if(error){mismatch=true;mismatchedLanes++;if(examples.length<10)examples.push({index,lane,actual,expected});}
  }
  mismatchedPixels+=+mismatch;
 }
 if (shaderErrors.length) throw Error(shaderErrors.join('\n'));
 result.textContent = JSON.stringify(
   {
     passed: mismatchedLanes === 0,
     backend: renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2',
     cases: count,
     mismatchedPixels,
     mismatchedLanes,
     maxByteError,
     examples,
     scope: 'Coefficient evaluation and quantization from original transformed normals; no material or full-image parity claim'
   },
   null,
   2
 );
}
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{pipeline?.dispose();target?.dispose();inputs?.dispose();renderer?.dispose();}
