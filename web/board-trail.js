import * as T from 'three/webgpu';
import {attribute,texture,vec4,float} from 'three/tsl';import {toFrame} from './frame-space.js';
// Original386E78 pass order, matching the existing native Metal adapter.
export async function createBoardTrail(origin){
 const meta=await (await fetch('/assets/BOARD_TRAIL/board_trail.json')).json();
 const bytes=new Uint8Array(await (await fetch('/assets/BOARD_TRAIL/'+meta.texture.file)).arrayBuffer());
 if(bytes.length!==meta.texture.width*meta.texture.height*4)throw Error('Original board-trail texture extent');
 const map=new T.DataTexture(bytes,meta.texture.width,meta.texture.height,T.RGBAFormat);map.wrapS=map.wrapT=T.RepeatWrapping;map.minFilter=map.magFilter=T.LinearFilter;map.colorSpace=T.NoColorSpace;map.needsUpdate=true;
 const maxVertices=2000;
 const geometry=()=>{const g=new T.BufferGeometry();for(const [name,n] of [['position',3],['uv',2],['trailColor',4]])g.setAttribute(name,new T.BufferAttribute(new Float32Array(maxVertices*n),n).setUsage(T.DynamicDrawUsage));g.setDrawRange(0,0);return g;};
 const ribbon=geometry(),roof=geometry(),group=new T.Group();group.userData.gameplayOnly=true;
 const texel=texture(map),colour=attribute('trailColor','vec4');
 // Source FX target is BGRA8Unorm: retain encoded source multiplication through
 // Three's output conversion. PS2 texture and vertex alpha use separate scales.
 const fragment=vec4(toFrame(texel.rgb.mul(colour.rgb)),texel.a.mul(meta.texture.normalized_sample_alpha_multiplier).mul(colour.a).clamp(0,1));
 // Keep the source pass sequence after terrain and before rider geometry,
 // so clearing snow depth cannot erase an already-rendered character.
 for(let pass=0;pass<5;pass++){
  const m=new T.MeshBasicNodeMaterial({side:T.DoubleSide,forceSinglePass:true,transparent:false,blending:T.CustomBlending,blendEquation:T.AddEquation,blendSrc:T.SrcAlphaFactor,blendDst:T.OneMinusSrcAlphaFactor,blendSrcAlpha:T.OneFactor,blendDstAlpha:T.OneMinusSrcAlphaFactor,toneMapped:false,fog:false,colorWrite:pass>=3,depthWrite:pass>=2,depthTest:true,depthFunc:pass===0||pass===2?T.AlwaysDepth:T.LessEqualDepth,stencilWrite:true,stencilRef:[0,1,1,1,0][pass],stencilFunc:pass===2||pass===4?T.EqualStencilFunc:T.AlwaysStencilFunc,stencilFail:T.KeepStencilOp,stencilZFail:T.KeepStencilOp,stencilZPass:[T.ZeroStencilOp,T.ReplaceStencilOp,T.ZeroStencilOp,T.ReplaceStencilOp,T.KeepStencilOp][pass]});
  m.fragmentNode=pass<3?vec4(0):fragment;if(pass===2)m.depthNode=float(1);
  const mesh=new T.Mesh(pass===3?ribbon:roof,m);mesh.renderOrder=500+pass;mesh.frustumCulled=false;group.add(mesh);
 }
 let previousSerial=-1;
 const upload=(core,g,pointer,count)=>{
  if(count>maxVertices)throw Error('Original trail exceeded source ring capacity');g.setDrawRange(0,count);
  const data=new Float32Array(core.HEAPF32.buffer,pointer,count*9),p=g.attributes.position.array,uv=g.attributes.uv.array,c=g.attributes.trailColor.array;
  for(let i=0;i<count;i++){p[i*3]=data[i*9]/100-origin.x;p[i*3+1]=data[i*9+2]/100-origin.y;p[i*3+2]=-data[i*9+1]/100-origin.z;uv[i*2]=data[i*9+3];uv[i*2+1]=data[i*9+4];for(let k=0;k<4;k++)c[i*4+k]=data[i*9+5+k];}
  // Only the active prefix is drawn. Rewrites supersede pending ranges, including
  // a shrinking trail; later growth fills the newly visible vertices again.
  for(const a of Object.values(g.attributes)){a.clearUpdateRanges();if(count){a.addUpdateRange(0,count*a.itemSize);a.needsUpdate=true;}}
 };
 const api={group,update(core){(globalThis.ssxEffects??={}).core=core;const info=new Float32Array(core.HEAPF32.buffer,core._trail_info(),6);if(info[2]===previousSerial)return;previousSerial=info[2];upload(core,ribbon,core._trail_ribbon(),info[0]);upload(core,roof,core._trail_roof(),info[1]);}};
 (globalThis.ssxEffects??={}).boardTrail=api;return api;
}
