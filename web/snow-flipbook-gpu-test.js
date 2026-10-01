import * as T from 'three/webgpu';import {createSnowRenderer} from './snow-renderer.js';
const result=document.querySelector('#result');let renderer,target,snow;
try{
 const asset=await (await fetch('/assets/SNOW_FX/snow-fx.json')).json(),memory=new Float32Array(4096),info=memory.subarray(4,27);for(let i=0;i<10;i++)info[10+i]=8;info[1]=1;info[20]=1;
 for(const p of asset.profiles)memory[800+p.emitter_index]=p.parameters.TextureId;memory.set([0,0,0,50,1,1,1,1],192);
 const core={HEAPF32:memory,_snow_info:()=>16,_snow_particles:i=>(128+i*64)*4,_snow_flipbook_info:()=>3200};snow=await createSnowRenderer(new T.Vector3(),core);
 const scene=new T.Scene();scene.background=new T.Color(0);scene.add(snow.group);const camera=new T.PerspectiveCamera(60,1,.1,100);camera.position.z=2;
 renderer = new T.WebGPURenderer({ antialias: false, forceWebGL: new URL(location.href).searchParams.get('backend') === 'webgl' });
 renderer.setSize(64, 64);
 await renderer.init();
 target = new T.RenderTarget(64, 64, { depthBuffer: false });
 renderer.setRenderTarget(target);
 const hashes=[];
 for (let id = 14; id <= 21; id++) {
   memory[801] = id;
   info[20]++;
   snow.update(core, camera);
   renderer.render(scene, camera);
   const pixels = await renderer.readRenderTargetPixelsAsync(target, 0, 0, 64, 64);
   let hash = 2166136261,
     nonblack = 0;
   for (let i = 0; i < pixels.length; i++) {
     hash = Math.imul(hash ^ pixels[i], 16777619) >>> 0;
     if (i % 4 < 3 && pixels[i]) nonblack++;
   }
   if (!nonblack) throw Error('Particle was not rendered');
   hashes.push(hash);
 }
 if (new Set(hashes).size !== 8) throw Error('Some frame changes did not reach the GPU texture binding');
 result.textContent = JSON.stringify(
   {
     passed: true,
     backend: renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2',
     frames: 8,
     distinctImages: new Set(hashes).size,
     scope: 'Actual snow renderer texture switching; not source framebuffer blend/clip parity'
   },
   null,
   2
 );
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{target?.dispose();renderer?.dispose();}
