import * as T from 'three/webgpu';
import {attribute,texture,vec4,uniform,select} from 'three/tsl';import {toFrame} from './frame-space.js';
import {registerEncodedEffect} from './snow-composite.js';import {pv} from './pv-flags.js';import {drawOrder, EFFECT, SUBMIT} from './ps2-draw-order.js';
export async function createWakeRenderer(origin){
 const asset=await (await fetch('/assets/SNOW_FX/snow-fx.json')).json(),t=asset.textures.find(t=>t.id===56);
 if(!t)throw Error('Original wake texture missing');
 const bytes=new Uint8Array(await (await fetch('/assets/SNOW_FX/'+(t.gs_alpha_file||t.file))).arrayBuffer());if(bytes.length!==t.width*t.height*4)throw Error('Wake texture extent');
 const map=new T.DataTexture(bytes,t.width,t.height,T.RGBAFormat);map.wrapS=map.wrapT=T.RepeatWrapping;map.minFilter=map.magFilter=T.LinearFilter;map.colorSpace=T.NoColorSpace;map.needsUpdate=true;
 const geometry=new T.BufferGeometry(),capacity=1000;
 for(const [name,size] of [['position',3],['uv',2],['wakeColour',4]])geometry.setAttribute(name,new T.BufferAttribute(new Float32Array(capacity*size),size).setUsage(T.DynamicDrawUsage));geometry.setDrawRange(0,0);
 const material=new T.MeshBasicNodeMaterial({transparent:true,depthWrite:false,depthTest:true,side:T.DoubleSide,forceSinglePass:true,fog:false,toneMapped:false});
 const texel=texture(map),colour=attribute('wakeColour','vec4');const encodedOutput=uniform(false),encodedColour=texel.rgb.mul(colour.rgb).clamp(0,1);
 // GS MODULATE saturates at 255 (wake RGB is doubled environment colour); ALPHA 0x44 blends in encoded space.
 material.fragmentNode=vec4(select(encodedOutput,encodedColour,toFrame(encodedColour)),texel.a.mul(t.gs_alpha_file?t.gs_alpha_scale:1).mul(colour.a).clamp(0,1));
 const mesh=new T.Mesh(geometry,material);mesh.frustumCulled=false;mesh.renderOrder=pv('effectOrder')?drawOrder(EFFECT.wake,SUBMIT.wake):650;mesh.userData.gameplayOnly=true;let previous=-1;
 const api={mesh,update(core){const info=new Float32Array(core.HEAPF32.buffer,core._wake_info(),13);if(info[12]===previous)return;previous=info[12];const count=info[11];if(count>capacity)throw Error('Wake exceeded original ribbon capacity');geometry.setDrawRange(0,count);
  const data=new Float32Array(core.HEAPF32.buffer,core._wake_vertices(),count*9),p=geometry.attributes.position.array,uv=geometry.attributes.uv.array,c=geometry.attributes.wakeColour.array;
  for(let i=0;i<count;i++){p[i*3]=data[i*9]/100-origin.x;p[i*3+1]=data[i*9+2]/100-origin.y;p[i*3+2]=-data[i*9+1]/100-origin.z;uv[i*2]=data[i*9+3];uv[i*2+1]=data[i*9+4];for(let k=0;k<4;k++)c[i*4+k]=data[i*9+5+k];}
  for(const a of Object.values(geometry.attributes))a.needsUpdate=true;
 }};
 registerEncodedEffect({object:mesh,setEncodedOutput:v=>{encodedOutput.value=!!v;},populated:()=>geometry.drawRange.count>0});
 (globalThis.ssxEffects??={}).wake=api;return api;
}
