import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const core=await createCore();
process.on('uncaughtException',e=>{if(e instanceof Error)console.error(e);else console.error(core.getExceptionMessage(e));process.exit(1);});
const riderPackage=process.argv.includes('--zoe')?'RIDER_ZOE':'RIDER_SAM';
const root=new URL('public/assets/',import.meta.url),read=p=>fs.readFileSync(new URL(p,root)),json=p=>JSON.parse(read(p));
const put=bytes=>{const p=core._malloc(bytes.length);core.HEAPU8.set(bytes,p);return p;};
const str=path=>put(Buffer.concat([read(path),Buffer.from([0])]));
const meta=str('ANIMATIONS/animation-packets.json'),rigPtr=str(riderPackage+'/rider.json'),cfg=str('ANIMATIONS/initial.json'),packets=read('ANIMATIONS/animation-packets.bin'),packetsPtr=put(packets);
core._init_animation(meta,rigPtr,cfg,packetsPtr,packets.length);core._init_race(cfg);core._animation_use_physics(1);
const mesh=read('ARA1/collision.bin'),mp=put(mesh);core._init_world(mp,mesh.length/4);
const terrain=str('ARA1/terrain.json'),world=str('ARA1/world_collision.json'),hash=put(Buffer.from(json('ARA1/terrain.json').source_sha256+'\0'));
core._init_terrain(terrain);core._init_world_collision(world,hash);core._init_body_terrain(terrain);const rails=str('ARA1/rails.json');core._init_rails(rails,hash);core._free(rails);
for(const p of [meta,rigPtr,cfg,packetsPtr,mp,terrain,world,hash])core._free(p);
const poseCount=json(riderPackage+'/rider.json').bones.length*7,start=json('ARA1/start.json');
const fogPackage=str('ARA1/fog-tree.json');core._init_fog(fogPackage);core._free(fogPackage);



const nativeMeta=JSON.parse(fs.readFileSync(new URL('../local/assets/native/ARA1/world.json',import.meta.url))),nativeIndexBytes=fs.readFileSync(new URL('../local/assets/native/ARA1/indices.bin',import.meta.url));
const indices=new Uint32Array(nativeIndexBytes.buffer,nativeIndexBytes.byteOffset,nativeIndexBytes.length/4),vertexBytes=read('ARA1/vertices.bin'),vertices=new Float32Array(vertexBytes.buffer,vertexBytes.byteOffset,vertexBytes.length/4);
assert(vertexBytes.equals(fs.readFileSync(new URL('../local/assets/native/ARA1/vertices.bin',import.meta.url))),'Audited render vertex buffers differ');
const ranges=new Map(nativeMeta.collision_sources.filter(x=>x.kind==='terrain').map(x=>[(x.rid<<8)|x.track,x]));
const sample=(p,n)=>Array.from(new Float32Array(core.HEAPF32.buffer,p,n));
const native=p=>[p[0]/100,p[2]/100,-p[1]/100];
function renderHeights(resource,point){
 const entry=ranges.get(resource);if(!entry)return [];
 const heights=[];
 for(let i=entry.first_triangle*3;i<(entry.first_triangle+entry.triangle_count)*3;i+=3){
  const a=indices[i]*10,b=indices[i+1]*10,c=indices[i+2]*10;
  const ax=vertices[b]-vertices[a],az=vertices[b+2]-vertices[a+2],bx=vertices[c]-vertices[a],bz=vertices[c+2]-vertices[a+2],dx=point[0]-vertices[a],dz=point[2]-vertices[a+2],det=ax*bz-az*bx;
  if(Math.abs(det)<1e-8)continue;const s=(dx*bz-dz*bx)/det,t=(ax*dz-az*dx)/det;
  if(Math.min(s,t,1-s-t)<-1e-6)continue;
  heights.push({height:vertices[a+1]+s*(vertices[b+1]-vertices[a+1])+t*(vertices[c+1]-vertices[a+1]),triangle:i/3});
 }
 return heights.sort((a,b)=>Math.abs(a.height-point[1])-Math.abs(b.height-point[1]));
}
const patches=new Map(json('ARA1/terrain.json').patches.map(p=>[p.resource_id,p.coefficients]));
function analyticAtXZ(resource,point,initial){
 const c=patches.get(resource);if(!c)return null;let [u,v]=initial;
 const evaluate=(du=0,dv=0)=>Array.from({length:3},(_,k)=>{let sum=0;for(let j=dv;j<4;j++)for(let i=du;i<4;i++)sum+=c[j*4+i][k]*(du?i:1)*(dv?j:1)*u**(i-du)*v**(j-dv);return sum;});
 for(let step=0;step<20;step++){
  const p=evaluate(),dx=point[0]-p[0],dz=point[2]-p[2];
  if(Math.hypot(dx,dz)<1e-7)return u>=0&&u<=1&&v>=0&&v<=1?{height:p[1],uv:[u,v]}:null;
  const a=evaluate(1),b=evaluate(0,1),det=a[0]*b[2]-a[2]*b[0];if(Math.abs(det)<1e-10)return null;
  u+=(dx*b[2]-dz*b[0])/det;v+=(a[0]*dz-a[2]*dx)/det;if(!Number.isFinite(u+v)||Math.abs(u)>4||Math.abs(v)>4)return null;
 }
 return null;
}
core._reset_animation();core._reset_race();core._reset_rider(...start.position,start.heading);core._start_event();
const materialCatalog=json('ANIMATIONS/initial.json').original_ground.surface_catalog;const bodyScale=json('ANIMATIONS/initial.json').original_animation.scale[0];
const rows=[];let sampled=0;
for(let tick=0;tick<4500;tick++){
 core._start_input(0);core._rail_preinput(0);core._rail_rotation_input(0);core._race_begin();const state=sample(core._step_rider(0,0,0,0),16);
 core._animation_tick(state[7],0,0,state[9],state[8],0,0,0,0,0,state[15],0);
 const physical=sample(core._pose_physical(),12);core._step_camera_head(...physical.slice(9,12));core._race_end();
 const contact=sample(core._terrain_contact_info(),12),volume=sample(core._body_volume_info(),108);
 if(tick%3||!contact[9]||contact[0]<0||!volume[0])continue;
 sampled++;const hit=native(contact.slice(3,6)),board=native(volume.slice(5,8)),surface=renderHeights(contact[0],hit)[0],atBoard=renderHeights(contact[0],board)[0];
 if(!surface)continue;const analytic=analyticAtXZ(contact[0],board,contact.slice(1,3)),motion=sample(core._rider_state(),16);
 rows.push({tick,seconds:tick/60,resource:contact[0],surface:sample(core._physics_info(),6)[1],uv:contact.slice(1,3),contact:hit,board,physicsPosition:motion.slice(0,3),contactDistanceCm:sample(core._ground_contact_info(),6)[5],currentDepth1Cm:contact[10],currentDepth3Cm:contact[11],analyticAboveBoard:analytic?analytic.height-board[1]:null,renderErrorAtBoard:analytic&&atBoard?atBoard.height-analytic.height:null,boardNormalSeparation:(board[0]-hit[0])*contact[6]+(board[1]-hit[1])*contact[8]-(board[2]-hit[2])*contact[7],renderAboveContact:surface.height-hit[1],renderAboveBoard:atBoard?atBoard.height-board[1]:null,triangle:surface.triangle,mode:sample(core._animation_info(),19)[15]});
}
const worst=rows.filter(r=>r.renderAboveBoard!==null).sort((a,b)=>b.renderAboveBoard-a.renderAboveBoard).slice(0,30);
const bySurface={};for(const row of rows){const entry=bySurface[row.surface]??={samples:0,above10cm:0,maxRenderAboveBoard:0,maxRenderErrorAtBoard:0,maxNormalPenetration:0,depthTargetCm:materialCatalog.find(m=>m.surface.id===row.surface).depth_target3*bodyScale};entry.samples++;entry.above10cm+=+(row.renderAboveBoard>.1);entry.maxRenderAboveBoard=Math.max(entry.maxRenderAboveBoard,row.renderAboveBoard??0);entry.maxRenderErrorAtBoard=Math.max(entry.maxRenderErrorAtBoard,row.renderErrorAtBoard??0);entry.maxNormalPenetration=Math.max(entry.maxNormalPenetration,-row.boardNormalSeparation);}
const worstBySurface=Object.fromEntries(Object.keys(bySurface).map(id=>[id,rows.filter(r=>r.surface===Number(id)&&r.renderAboveBoard!==null).sort((a,b)=>b.renderAboveBoard-a.renderAboveBoard).slice(0,8)]));
const report={bySurface,worstBySurface,rider:riderPackage,ticks:4500,sampled,matched:rows.length,above10cm:rows.filter(r=>r.renderAboveBoard>.1).length,above50cm:rows.filter(r=>r.renderAboveBoard>.5).length,worst,scope:'Neutral Snow Jam replay; same contact patch vertical intersections, not all overlapping surfaces or frame-image proof'};
fs.writeFileSync(new URL('../local/browser-validation/rider-terrain-'+riderPackage+'.json',import.meta.url),JSON.stringify(report,null,2));
console.log(JSON.stringify({...report,worstBySurface:undefined,worst:worst.slice(0,3)},null,2));
