import {
  Vector3,
  Color,
  DoubleSide,
  DataTexture,
  RGBAFormat,
  UnsignedByteType,
  FloatType,
  HalfFloatType,
  LinearFilter,
  NearestFilter,
  ClampToEdgeWrapping,
  NoColorSpace,
  RenderTarget,
  MeshBasicNodeMaterial,
  QuadMesh,
  Mesh,
  Scene,
  OrthographicCamera,
  BufferGeometry,
  BufferAttribute,
  DynamicDrawUsage,
  CustomBlending,
  AddEquation,
  OneFactor
} from 'three/webgpu';
import {Fn,uniform,texture,uv,attribute,vec2,vec4,float,int,floor,min,max,select,screenUV,Loop,perspectiveDepthToViewZ} from 'three/tsl';

// Original light glow halos (SLUS_207.72), see terrain-render-fidelity.md "Light glow halos".
//  Sources: world records kind 7 (runtime entity type 8) collected per visible cell
//   (22A4A8 -> +7BC0) and re-listed every frame by 22C790 (2E2F98/2E2FF8/2E2FA8).
//  2E2B00 (update): 37DBE8 outcode test (hidden unless inside the view volume),
//   37DD20 integer screen x/y, zf = Z*2^-24. zf <= 0.005: far glow (no query,
//   visibility 1, sprite depth-tested GEQUAL at the light). Otherwise a count rect
//   clamp(trunc(zf*800),1,16) x clamp(trunc(zf*400),1,8) centred on the light, a
//   16x8 read rect at (x-8, y-4) clamped to the viewport, the reference Z of the
//   light pulled toward the camera by 100/80/200 cm (class 0x10/0x20/0x40) and a
//   rotation ((x - vx)*2/vw - 1)*pi/2.
//  2E3130 -> 2EC478 (renderer callback after the world): Z readback,
//   vis = open/(w*h), or max((open - wh/2)/(wh/2), 0) when the rect is clipped.
//  2E2868 (draw): FX 53 'shal' (0x10/0x40) or 54 'mhal' (0x20), half size
//   180/100/350 cm, vertex RGB trunc(128*normalize(colour)), A trunc(128*vis);
//   3781A0 builds a camera-facing quad (right/up rotated by the angle, corners from
//   projected integer offsets) and emits it twice. Material 2E3578: ALPHA 0x48
//   (Cd + Cs*As>>7), MODULATE, ATST ALWAYS, word2 priority 7 -> after the fog
//   composite (36AC00) and before the sun (priority 8) and ScreenTint.
const f=Math.fround;
// PS2 Z (2^24 at the near plane, 0 at the far plane) as a 0..1 fraction.
export function glowDepthFraction(depthCm,nearCm,farCm){return f(f(1/depthCm-1/farCm)/f(1/nearCm-1/farCm));}
// 37DD20 integer screen coordinate. Five live projections (four glows, the sun) all
// agree with the continuous three.js pixel coordinate minus half a pixel, truncated.
// EE FPU mul.s rounds toward zero (the f32 x f32 product is exact in a double).
const ulpBuffer=new DataView(new ArrayBuffer(4));
export function ps2Mul(a,b){
 const p=f(a)*f(b),r=f(p);if(!(Math.abs(r)>Math.abs(p)))return r;
 ulpBuffer.setFloat32(0,r);ulpBuffer.setUint32(0,ulpBuffer.getUint32(0)-1);return ulpBuffer.getFloat32(0);
}
export function glowPixel(v){return Math.trunc(v-.5);}
// 2E2B00 record fields from the integer screen position and zf.
export function glowRecord(screen,zf,constants,viewport=[0,0,...constants.viewport]){
 const [x,y]=screen,[vx,vy,vw,vh]=viewport,[rw,rh]=constants.read_size;
 if(!(f(zf)>constants.far_threshold))return {far:true,angle:0};
 const clamp=(v,hi)=>v<=0?1:Math.min(v,hi);
 const w=clamp(Math.trunc(f(zf*constants.count_scale[0])),constants.count_max[0]),h=clamp(Math.trunc(f(zf*constants.count_scale[1])),constants.count_max[1]);
 const read=(v,lo,extent,n)=>v>=0?Math.min(v,lo+extent-n):0;
 return {far:false,count:[x-(w>>1),y-(h>>1),w,h],read:[read(x-rw/2,vx,vw,rw),read(y-rh/2,vy,vh,rh)],
  angle:ps2Mul(f(f(f(f(x-vx)*2)/vw)-1),constants.rotation_scale)};
}
export function glowVisibility(counted,open,area){return counted===area?open/area:Math.max((open-area/2)/(area/2),0);}
export function glowColour(colour){
 const [r,g,b]=colour.map(f),n=f(Math.sqrt(f(f(f(r*r)+f(g*g))+f(b*b))));
 return (n===0?[r,g,b]:[r,g,b].map(v=>f(v*f(1/n)))).map(v=>Math.trunc(f(v*128)));
}
// 3781A0 corner order (triangle strip, ST 0/1): P-dx-dy, P+dx-dy, P-dx+dy, P+dx+dy.
export function glowQuad(P,dx,dy){
 return [[P[0]-dx[0]-dy[0],P[1]-dx[1]-dy[1],0,0],[P[0]+dx[0]-dy[0],P[1]+dx[1]-dy[1],1,0],
  [P[0]-dx[0]+dy[0],P[1]-dx[1]+dy[1],0,1],[P[0]+dx[0]+dy[0],P[1]+dx[1]+dy[1],1,1]];
}

export async function createLightGlow(origin,root='/assets/LIGHT_GLOW/'){
 const pkg=await (await fetch(root+'light-glow.json')).json();
 if(pkg.version!==1||!pkg.lights?.length)throw Error('Unsupported light glow package');
 const maps={};
 await Promise.all(Object.entries(pkg.textures).map(async([name,t])=>{
  const bytes=new Uint8Array(await (await fetch(root+t.file)).arrayBuffer());if(bytes.length!==t.width*t.height*4)throw Error('Light glow texture extent');
  const map = new DataTexture(bytes, t.width, t.height, RGBAFormat, UnsignedByteType);
  map.minFilter = map.magFilter = LinearFilter;
  map.wrapS = map.wrapT = ClampToEdgeWrapping;
  map.colorSpace = NoColorSpace;
  map.generateMipmaps = false;
  map.needsUpdate = true;
  maps[name] = map;
 }));
 const c=pkg.constants,[VW,VH]=c.viewport,MAX=pkg.lights.length;
 const lights=pkg.lights.map((l,i)=>{const cls=c.classes[l.flags&0x70];if(!cls)throw Error('Light glow class');
  return {index:i,track:l.track,world:new Vector3(l.position[0]/100-origin.x,l.position[2]/100-origin.y,-l.position[1]/100-origin.z),cls,rgb:glowColour(l.colour),mhal:cls.texture==='mhal'?1:0};});
 let tracks=null; // streamed Peak 1 world (setTracks): only the drawn locations' lights
 let band=[0,1],frameState={count:0,queried:0};
 const bandTop=uniform(0),bandHeight=uniform(VH),near=uniform(.1),far=uniform(300),enabled=uniform(0);
 // Query parameters: row 0 count rect (x, y, w, h), row 1 read origin + reference depth (m).
 const params=new DataTexture(new Float32Array(MAX*2*4),MAX,2,RGBAFormat,FloatType);params.minFilter=params.magFilter=NearestFilter;params.generateMipmaps=false;params.needsUpdate=true;
 const visTarget=new RenderTarget(MAX,1,{type:HalfFloatType,depthBuffer:false});visTarget.texture.minFilter=visTarget.texture.magFilter=NearestFilter;
 const glowTarget=new RenderTarget(1,1,{type:HalfFloatType,depthBuffer:false});glowTarget.texture.minFilter=glowTarget.texture.magFilter=NearestFilter;
 // Sprite geometry in NDC of the 3D band, two triangles per glow.
 const geometry=new BufferGeometry(),index=new Uint16Array(MAX*6);
 for(let i=0;i<MAX;i++)index.set([i*4,i*4+1,i*4+2,i*4+1,i*4+3,i*4+2],i*6);
 for(const [name,n] of [['position',3],['uv',2],['glowRgb',3],['glowInfo',4]])geometry.setAttribute(name,new BufferAttribute(new Float32Array(MAX*4*n),n).setUsage(DynamicDrawUsage));
 geometry.setIndex(new BufferAttribute(index,1));geometry.setDrawRange(0,0);
 const glowScene=new Scene(),glowCamera=new OrthographicCamera(-1,1,1,-1,-1,1);
 let glowMaterial=null,queryMaterial=null,queryQuad=null;const clearColour=new Color();
 const eye=new Vector3(),right=new Vector3(),up=new Vector3(),view=new Vector3(),ndc=new Vector3(),tmp=new Vector3(),dir=new Vector3();
 const api={pkg,enabled:true,warm:false,warmQuery:false,
  // Encoded 0..255 RGB after the fog/priority-7 composite, before the sun and ScreenTint.
  apply(bytes){const g=texture(glowTarget.texture,screenUV).rgb;return select(enabled.greaterThan(.5),min(bytes.add(g),255),bytes);},
  attach(renderer,worldPass){
   const depthTexture=worldPass.renderTarget.depthTexture;if(!depthTexture)throw Error('Light glow query requires scene depth');
   const sceneDepth=d=>perspectiveDepthToViewZ(d,near,far).negate();
   queryMaterial=new MeshBasicNodeMaterial({depthTest:false,depthWrite:false,toneMapped:false,fog:false});
   queryMaterial.fragmentNode=Fn(()=>{
    const u=floor(screenUV.x.mul(MAX)).add(.5).div(MAX),rect=texture(params,vec2(u,.25)),read=texture(params,vec2(u,.75));
    const open=float(0).toVar(),counted=float(0).toVar();
    Loop({start:int(0),end:int(c.read_size[1]),name:'gy'},({gy})=>{Loop({start:int(0),end:int(c.read_size[0]),name:'gx'},({gx})=>{
     const px=read.x.add(float(gx)),py=read.y.add(float(gy));
     const inRect=px.greaterThanEqual(rect.x).and(px.lessThan(rect.x.add(rect.z))).and(py.greaterThanEqual(rect.y)).and(py.lessThan(rect.y.add(rect.w)));
     const v=py.add(.5).sub(bandTop).div(bandHeight),depth=texture(depthTexture,vec2(px.add(.5).div(VW),v.clamp(0,1))).r;
     // 2EC478 counts pixels whose Z does not exceed the pulled reference Z (nothing nearer).
     const open1=sceneDepth(depth).greaterThanEqual(read.z).or(v.lessThan(0)).or(v.greaterThan(1));
     counted.addAssign(select(inRect,1,0));open.addAssign(select(inRect.and(open1),1,0));
    });});
    const area=rect.z.mul(rect.w);
    return vec4(select(counted.equal(area),open.div(area),max(open.sub(area.mul(.5)).div(area.mul(.5)),0)),0,0,1);
   })();
   queryQuad=new QuadMesh(queryMaterial);
   glowMaterial = new MeshBasicNodeMaterial({
     side: DoubleSide,
     depthTest: false,
     depthWrite: false,
     toneMapped: false,
     fog: false,
     transparent: true,
     blending: CustomBlending,
     blendEquation: AddEquation,
     blendSrc: OneFactor,
     blendDst: OneFactor,
     blendSrcAlpha: OneFactor,
     blendDstAlpha: OneFactor
   });
   glowMaterial.fragmentNode=Fn(()=>{
    const info=attribute('glowInfo','vec4'),rgb=attribute('glowRgb','vec3'),st=uv();
    const t=select(info.w.greaterThan(.5),texture(maps.mhal,st),texture(maps.shal,st)).mul(255).round();
    const vis=texture(visTarget.texture,vec2(info.x.add(.5).div(MAX),.5)).r;
    const farGlow=info.y.greaterThan(.5),va=select(farGlow,float(128),floor(vis.mul(128)));
    // Far glows: ZTST GEQUAL of the sprite (light depth) against the world Z.
    const pass=farGlow.not().or(sceneDepth(texture(depthTexture,screenUV).r).greaterThanEqual(info.z));
    const cs=min(floor(t.rgb.mul(rgb).div(128)),255),as=floor(t.a.mul(va).div(128));
    return vec4(select(pass,floor(cs.mul(as).div(128)).mul(c.copies),vec4(0).rgb),1);
   })();
   const mesh=new Mesh(geometry,glowMaterial);mesh.frustumCulled=false;glowScene.add(mesh);
   const update=worldPass.updateBefore.bind(worldPass);
   worldPass.updateBefore=frame=>{
    /* warm (main.js loading-screen warm-up): the query and glow pipelines build even with no glow in view (one degenerate sprite) */
    const warm=api.warm&&!frameState.count;update(frame);if(!frameState.count&&!warm)return;
    const prior = renderer.getRenderTarget(),
      priorClear = renderer.autoClear,
      rt = worldPass.renderTarget,
      clearAlpha = renderer.getClearAlpha(),
      range = geometry.drawRange.count;
    renderer.getClearColor(clearColour);
    try{
     renderer.autoClear=true;renderer.setClearColor(0x000000,0);if(warm)geometry.setDrawRange(0,6);
     // warmQuery (main.js warmPost): with glows counted but none queried the query built on the first queried race / cutscene frame; a
     // texel the glows read is always written the same frame
     if(frameState.queried||warm||api.warmQuery){renderer.setRenderTarget(visTarget);queryQuad.render(renderer);}
     glowTarget.setSize(rt.width,rt.height);renderer.setRenderTarget(glowTarget);renderer.render(glowScene,glowCamera);
    }finally{if(warm)geometry.setDrawRange(0,range);renderer.setRenderTarget(prior);renderer.autoClear=priorClear;renderer.setClearColor(clearColour,clearAlpha);}
   };
  },
  // Once per rendered frame with the displayed camera (2E2B00 + 2E2868 setup, view 0).
  update(camera){
   camera.updateMatrixWorld();frameState={count:0,queried:0,glows:[]};
   if(!api.enabled){enabled.value=0;geometry.setDrawRange(0,0);return frameState;}
   const vh=band[1]*VH,vy=band[0]*VH,viewport=[0,vy,VW,vh],e=camera.matrixWorld.elements;
   eye.setFromMatrixPosition(camera.matrixWorld);right.set(e[0],e[1],e[2]).normalize();up.set(e[4],e[5],e[6]).normalize();
   bandTop.value=vy;bandHeight.value=vh;near.value=camera.near;far.value=camera.far;
   const project=(p,out)=>{ndc.copy(p).project(camera);out[0]=glowPixel((ndc.x+1)*.5*VW);out[1]=glowPixel(vy+(1-ndc.y)*.5*vh);return out;};
   const P = [0, 0],
     A = [0, 0],
     B = [0, 0],
     pos = geometry.attributes.position.array,
     uvs = geometry.attributes.uv.array,
     rgbs = geometry.attributes.glowRgb.array,
     infos = geometry.attributes.glowInfo.array,
     q = params.image.data;
   let n=0,queried=0;
   for(const light of lights){
    if(tracks&&!tracks.has(light.track))continue;
    view.copy(light.world).applyMatrix4(camera.matrixWorldInverse);const depth=-view.z;
    if(!(depth>camera.near&&depth<camera.far))continue;
    ndc.copy(light.world).project(camera);if(Math.abs(ndc.x)>1||Math.abs(ndc.y)>1)continue;
    project(light.world,P);
    const record=glowRecord(P,glowDepthFraction(depth*100,camera.near*100,camera.far*100),c,viewport);
    if(!record.far){q.set(record.count,light.index*4);const o=(MAX+light.index)*4;q[o]=record.read[0];q[o+1]=record.read[1];q[o+2]=depth-light.cls.pull_cm/100;q[o+3]=1;queried++;}
    const a=record.angle,cs=Math.cos(a),sn=Math.sin(a),size=light.cls.half_size/100;
    dir.copy(right).multiplyScalar(cs).addScaledVector(up,sn);project(tmp.copy(light.world).addScaledVector(dir,size),A);
    dir.copy(right).multiplyScalar(-sn).addScaledVector(up,cs);project(tmp.copy(light.world).addScaledVector(dir,size),B);
    const quad=glowQuad(P,[A[0]-P[0],A[1]-P[1]],[B[0]-P[0],B[1]-P[1]]);
    // direct writes (no per-vertex arrays)
    for (let k = 0; k < 4; k++) {
      const c = quad[k],
        v = n * 4 + k;
      pos[v * 3] = (c[0] / VW) * 2 - 1;
      pos[v * 3 + 1] = 1 - ((c[1] - vy) / vh) * 2;
      pos[v * 3 + 2] = 0;
      uvs[v * 2] = c[2];
      uvs[v * 2 + 1] = c[3];
      rgbs.set(light.rgb, v * 3);
      infos[v * 4] = light.index;
      infos[v * 4 + 1] = record.far ? 1 : 0;
      infos[v * 4 + 2] = depth;
      infos[v * 4 + 3] = light.mhal;
    }
    frameState.glows.push({index:light.index,screen:P.slice(),far:record.far,count:record.count,read:record.read,angle:a,depth});n++;
   }
   geometry.setDrawRange(0,n*6);for(const k of ['position','uv','glowRgb','glowInfo'])geometry.attributes[k].needsUpdate=true;
   if(queried)params.needsUpdate=true;
   enabled.value=n?1:0;frameState.count=n;frameState.queried=queried;return frameState;
  },
  setBand(value){band=[value[0],value[1]];},
  // Streamed Peak 1 world (web/free-ride.js): the SDB tracks whose lights are drawn (null: all).
  setTracks(set){tracks=set;},
  get state(){return {frame:frameState,near:near.value,far:far.value};},
  dispose(){visTarget.dispose();glowTarget.dispose();params.dispose();geometry.dispose();glowMaterial?.dispose();queryMaterial?.dispose();Object.values(maps).forEach(m=>m.dispose());}
 };
 (globalThis.ssxEffects??={}).lightGlow=api;return api;
}
