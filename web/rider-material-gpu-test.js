import * as T from 'three/webgpu';
import {uniform,vec3} from 'three/tsl';
import {createRiderLightingMaterial} from './rider-material.js';
import {frameTextureSpace,configureFrameSpace} from './frame-space.js';   // pv encodedBlend: the target holds the frame's values
const result = document.querySelector('#result');
const tint = new URL(location.href).searchParams.has('tint') ? [0.4, 0.32, 0.24] : null;
const toLinear = (x) => (x <= 0.04045 ? x / 12.92 : ((x + 0.055) / 1.055) ** 2.4);
const toEncoded = (x) => (x <= 0.0031308 ? x * 12.92 : 1.055 * x ** (1 / 2.4) - 0.055);
let renderer, target, geometry, material, source;
const shaderErrors = [],
  log = console.error;
console.error = (...args) => {
  shaderErrors.push(args.map(String).join(' '));
  log(...args);
};
try{
 const size=64,data=new Uint8Array(size*size*4);
 for(let y=0;y<size;y++)for(let x=0;x<size;x++)data.set([x*17,y*17,(x*31+y*7)%256,255],(y*size+x)*4);
 source = new T.DataTexture(data, size, size, T.RGBAFormat, T.UnsignedByteType);
 source.colorSpace = frameTextureSpace;
 source.minFilter = source.magFilter = T.NearestFilter;
 source.generateMipmaps = false;
 source.needsUpdate = true;
 const active = uniform(false),
   lighting = createRiderLightingMaterial({ enabledNode: active, lightingNormal: vec3(0, 0, 1) }, !!tint),
   bank = new Float32Array(40);
 bank.set([127, 128, 200, 13]);
 lighting.capture({ HEAPF32: bank, _rider_lighting_gpu_coefficients: () => 0 });
 material = new T.MeshBasicNodeMaterial({ map: source, vertexColors: !!tint });
 geometry = new T.PlaneGeometry(2, 2);
 if (tint) geometry.setAttribute('color', new T.Float32BufferAttribute(Array.from({ length: 4 }, () => tint).flat(), 3));
 const scene = new T.Scene();
 scene.add(new T.Mesh(geometry, material));
 const camera = new T.OrthographicCamera(-1, 1, 1, -1, 0.1, 10);
 camera.position.z = 1;
 renderer = new T.WebGPURenderer({ antialias: false, forceWebGL: new URL(location.href).searchParams.get('backend') === 'webgl' });
 renderer.setSize(size, size);
 renderer.toneMapping = T.NoToneMapping;
 configureFrameSpace(renderer);
 await renderer.init();
 target=new T.RenderTarget(size,size,{type:T.UnsignedByteType,depthBuffer:false});
 // linear frame: an sRGB-encoding target; encoded frame: the bytes as written
target.texture.colorSpace=frameTextureSpace; renderer.setRenderTarget(target);
 renderer.render(scene,camera);const baseline=new Uint8Array(await renderer.readRenderTargetPixelsAsync(target,0,0,size,size));lighting.attach(material);material.needsUpdate=true;
 const reports=[];
 for(const enabled of [false,true]){
  active.value=enabled;renderer.render(scene,camera);const pixels=await renderer.readRenderTargetPixelsAsync(target,0,0,size,size);let mismatched=0;const examples=[];
  for(let y=0;y<size;y++)for(let x=0;x<size;x++){
   const row=renderer.backend.isWebGPUBackend?size-1-y:y,offset=(row*size+x)*4;
   for(let lane=0;lane<4;lane++){
     const tex = data[offset + lane],
       sample = tint && lane < 3 ? toEncoded(toLinear(tex / 255) * Math.fround(tint[lane])) * 255 : tex,
       expected =
         enabled && lane < 3
           ? // GameCube-domain map: halved into PS2 texels
             Math.min(255, Math.floor(((sample / 2) * [127, 128, 200][lane]) / 128) + 13)
           : baseline[(y * size + x) * 4 + lane],
       actual = pixels[(y * size + x) * 4 + lane];
     if (actual !== expected) {
       mismatched++;
       if (examples.length < 8) examples.push({ x, y, lane, actual, expected });
     }
   }
  }
  reports.push({enabled,mismatched,examples});
 }
 if(shaderErrors.length)throw Error(shaderErrors.join('\n'));
 result.textContent = JSON.stringify(
   {
     passed: reports.every((r) => !r.mismatched),
     backend: renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2',
     tint,
     reports,
     scope:
       'Actual MeshBasicNodeMaterial; menu compared to unchanged material, lit output compared to encoded HIGHLIGHT2 and optional authored tint; nearest RGBA8, no GS bilinear/blend equivalence'
   },
   null,
   2
 );
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{material?.userData.sourceTexture?.dispose();source?.dispose();material?.dispose();geometry?.dispose();target?.dispose();renderer?.dispose();}
