import * as T from 'three/webgpu';
import {Fn,ivec2,floor,screenCoordinate,textureLoad,vec4} from 'three/tsl';
import {riderTransformPositionNode} from './rider-lighting-nodes.js';
const result=document.querySelector('#result'),shaderErrors=[];
const log=console.error;console.error=(...args)=>{shaderErrors.push(args.map(String).join(' '));log(...args);};
let renderer,pipeline,target,inputs;
try{
 const response=await fetch('/test-data/rider-position-reference.bin');if(!response.ok)throw Error('Missing original skin reference');
 const records=new Float32Array(await response.arrayBuffer()),count=12000,width=256,height=Math.ceil(count/width);
 if(records.length!==count*24)throw Error('Unexpected skin reference size');
 const referenceWords=new Uint32Array(records.buffer),data=new Float32Array(width*height*24);data.set(records);
 inputs=new T.DataTexture(data,width*6,height,T.RGBAFormat,T.FloatType);inputs.minFilter=inputs.magFilter=T.NearestFilter;inputs.generateMipmaps=false;inputs.colorSpace=T.NoColorSpace;inputs.needsUpdate=true;
 renderer=new T.WebGPURenderer({antialias:false,forceWebGL:new URL(location.href).searchParams.get('backend')==='webgl'});renderer.setSize(width,height);renderer.toneMapping=T.NoToneMapping;await renderer.init();
 pipeline=new T.RenderPipeline(renderer);pipeline.outputColorTransform=false;
 pipeline.outputNode=Fn(()=>{const x=floor(screenCoordinate.x),y=floor(screenCoordinate.y);const sample=i=>textureLoad(inputs,ivec2(x.mul(6).add(i),y));return riderTransformPositionNode(sample(0),[sample(1),sample(2),sample(3),sample(4)]);})();
 target=new T.RenderTarget(width,height,{type:T.FloatType,depthBuffer:false});renderer.setRenderTarget(target);pipeline.render();
 const pixels=await renderer.readRenderTargetPixelsAsync(target,0,0,width,height);if(!(pixels instanceof Float32Array))throw Error('Skin reference requires float readback');
 const pixelWords=new Uint32Array(pixels.buffer,pixels.byteOffset,pixels.length);let mismatchedWords=0;const examples=[];
 for(let y=0;y<height;y++)for(let x=0;x<width;x++){
  const row=renderer.backend.isWebGPUBackend?y:height-1-y,index=row*width+x;if(index>=count)continue;
  for(let lane=0;lane<4;lane++){const actual=pixelWords[(y*width+x)*4+lane],expected=referenceWords[index*24+20+lane];if(actual!==expected){mismatchedWords++;if(examples.length<10)examples.push({index,lane,actual,expected});}}
 }
 if(shaderErrors.length)throw Error(shaderErrors.join('\n'));
 result.textContent=JSON.stringify({passed:mismatchedWords===0,backend:renderer.backend.isWebGPUBackend?'WebGPU':'WebGL2',cases:count,comparedWords:count*4,mismatchedWords,examples,scope:'Original float vertex/matrix transform before projection; supplied matrix, no live mesh/material claim'},null,2);
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{pipeline?.dispose();target?.dispose();inputs?.dispose();renderer?.dispose();}
