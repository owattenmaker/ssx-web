import * as T from 'three/webgpu';
import {Fn,ivec2,floor,screenCoordinate,textureLoad,vec4} from 'three/tsl';
import {riderTransformNormalNode} from './rider-lighting-nodes.js';
const result=document.querySelector('#result'),shaderErrors=[];
const log=console.error;console.error=(...args)=>{shaderErrors.push(args.map(String).join(' '));log(...args);};
let renderer,pipeline,target,inputs;
try{
 const response=await fetch('/test-data/rider-normal-reference.bin');if(!response.ok)throw Error('Missing original normal reference');
 const records=new Float32Array(await response.arrayBuffer()),count=12000,width=256,height=Math.ceil(count/width);
 if(records.length!==count*20)throw Error('Unexpected normal reference size');
 const referenceWords=new Uint32Array(records.buffer),data=new Float32Array(width*height*20);data.set(records);
 inputs = new T.DataTexture(data, width * 5, height, T.RGBAFormat, T.FloatType);
 inputs.minFilter = inputs.magFilter = T.NearestFilter;
 inputs.generateMipmaps = false;
 inputs.colorSpace = T.NoColorSpace;
 inputs.needsUpdate = true;
 renderer = new T.WebGPURenderer({ antialias: false, forceWebGL: new URL(location.href).searchParams.get('backend') === 'webgl' });
 renderer.setSize(width, height);
 renderer.toneMapping = T.NoToneMapping;
 await renderer.init();
 pipeline=new T.RenderPipeline(renderer);pipeline.outputColorTransform=false;
 pipeline.outputNode = Fn(() => {
   const x = floor(screenCoordinate.x),
     y = floor(screenCoordinate.y);
   const sample = (i) => textureLoad(inputs, ivec2(x.mul(5).add(i), y));
   return vec4(riderTransformNormalNode(sample(0).rgb, [sample(1).rgb, sample(2).rgb, sample(3).rgb]), 1);
 })();
 target=new T.RenderTarget(width,height,{type:T.FloatType,depthBuffer:false});renderer.setRenderTarget(target);pipeline.render();
 const pixels=await renderer.readRenderTargetPixelsAsync(target,0,0,width,height);if(!(pixels instanceof Float32Array))throw Error('Normal reference requires float readback');
 const pixelWords=new Uint32Array(pixels.buffer,pixels.byteOffset,pixels.length);let mismatchedWords=0;const examples=[];
 for(let y=0;y<height;y++)for(let x=0;x<width;x++){
  const row=renderer.backend.isWebGPUBackend?y:height-1-y,index=row*width+x;if(index>=count)continue;
  for (let lane = 0; lane < 3; lane++) {
    const actual = pixelWords[(y * width + x) * 4 + lane],
      expected = referenceWords[index * 20 + 16 + lane];
    if (actual !== expected) {
      mismatchedWords++;
      if (examples.length < 10) examples.push({ index, lane, actual, expected });
    }
  }
 }
 if(shaderErrors.length)throw Error(shaderErrors.join('\n'));
 result.textContent = JSON.stringify(
   {
     passed: mismatchedWords === 0,
     backend: renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2',
     cases: count,
     comparedWords: count * 3,
     mismatchedWords,
     examples,
     scope: 'Original packed normal decode and supplied-matrix transform; skin matrix selection/blending and full material not covered'
   },
   null,
   2
 );
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{pipeline?.dispose();target?.dispose();inputs?.dispose();renderer?.dispose();}
