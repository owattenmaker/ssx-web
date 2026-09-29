import * as T from 'three/webgpu';
import {createFogRenderer} from './fog-renderer.js';
import {createEncodedSnowComposite} from './snow-composite.js';
import {createSnowRenderer} from './snow-renderer.js';
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
 renderer=new T.WebGPURenderer({antialias:new URL(location.href).searchParams.has('msaa'),forceWebGL:new URL(location.href).searchParams.get('backend')==='webgl'});renderer.setSize(64,64);await renderer.init();
 target=new T.RenderTarget(64,64,{depthTexture:new T.DepthTexture(64,64),depthBuffer:true});
 const composite=createEncodedSnowComposite(renderer,target,scene,camera,snow),samples=[];
 const blocker=new T.Mesh(new T.PlaneGeometry(2,2),new T.MeshBasicNodeMaterial({color:0x00ff00,toneMapped:false}));blocker.position.z=1;blocker.visible=false;scene.add(blocker);
 for(const test of [{name:'grey particle',colour:.5,count:1,expected:[64,64,64]},{name:'grey destination',background:0x808080,count:1,expected:[192,192,192]},{name:'one particle',count:1,expected:[128,128,128]},{name:'overlapping particles',count:2,expected:[192,192,192]},{name:'opaque occlusion',count:2,blocked:true,expected:[0,255,0]},{name:'resize',count:2,size:128,expected:[192,192,192]}]){
  if(test.size)target.setSize(test.size,test.size);
  scene.background.setHex(test.background||0);memory.set([test.colour??1,test.colour??1,test.colour??1],196);info[2]=test.count-1;memory.set([0,0,0,50,1,1,1,.5],256);info[20]++;snow.update(core,camera);blocker.visible=!!test.blocked;
  snow.group.visible=false;renderer.setRenderTarget(target);renderer.autoClear=true;renderer.render(scene,camera);snow.group.visible=true;
  const mask=camera.layers.mask;composite.render();if(renderer.getRenderTarget()!==target||!renderer.autoClear||camera.layers.mask!==mask)throw Error('Render state was not restored');
  const size=target.width,pixels=await renderer.readRenderTargetPixelsAsync(composite.target,0,0,size,size),at=((size/2)*size+size/2)*4,rgb=Array.from(pixels.slice(at,at+3));
  if(rgb.some((v,i)=>Math.abs(v-test.expected[i])>1))throw Error(`${test.name}: ${rgb} expected ${test.expected}`);samples.push({name:test.name,rgb,size});
 }
 composite.dispose();
 const sky=new T.Scene();sky.background=new T.Color(0);const fog=createFogRenderer(renderer,scene,sky,camera,'',snow);
 fog.update({HEAPF32:new Float32Array(9),_fog_palette_info:()=>0});
 const lifecycle=[];
 for(const test of [{name:'empty first frame',count:0,background:0x808080},{name:'emitter starts',count:2},{name:'emitter clears',count:0},{name:'empty resize',count:0,resize:true,background:0x808080},{name:'emitter restarts',count:2},{name:'hidden snow',count:2,hidden:true},{name:'visible again',count:2}]){
  if(test.resize)renderer.setSize(128,128);
  sky.background.setHex(test.background||0);
  info[1]=test.count?1:0;info[2]=test.count>1?1:0;info[20]++;snow.update(core,camera);snow.group.visible=!test.hidden;
  // PassNode updates once per animation frame, not once per render() call.
  await new Promise(requestAnimationFrame);
  const before=fog.snowCompositeStats;renderer.setRenderTarget(target);fog.render();
  const integrated=await renderer.readRenderTargetPixelsAsync(target,0,0,target.width,target.height),centre=(target.width/2*target.width+target.width/2)*4;
  const rgb=Array.from(integrated.slice(centre,centre+3)),active=test.count&&!test.hidden,expected=active?191:test.background?128:0,after=fog.snowCompositeStats;
  if(rgb.some(v=>Math.abs(v-expected)>1))throw Error(test.name+': integrated pixels '+rgb+' expected '+expected);
  if(after.renders-before.renders!==(active?1:0)||after.skipped-before.skipped!==(active?0:1))throw Error(test.name+': wrong composite execution count '+JSON.stringify({before,after}));
  lifecycle.push({name:test.name,rgb,...after});
 }
 samples.push({name:'fog pipeline lifecycle',lifecycle});fog.dispose();
 result.textContent=JSON.stringify({passed:true,backend:renderer.backend.isWebGPUBackend?'WebGPU':'WebGL2',samples,scope:'Actual snow material, shared encoded destination and copied scene depth. Hardware UNORM blending may differ from GS truncation by a byte; original particle ordering remains separately unverified.'},null,2);
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{globalThis.fetch=originalFetch;target?.dispose();renderer?.dispose();}
