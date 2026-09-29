import fs from 'node:fs';import assert from 'node:assert/strict';import {createHash} from 'node:crypto';import {buildTerrainPatch} from './terrain-mesh.js';
const read=f=>fs.readFileSync(new URL('public/assets/ARA1/'+f,import.meta.url)),data=JSON.parse(read('terrain-render.json')),bytes=read('vertices.bin'),vertices=new Float32Array(bytes.buffer,bytes.byteOffset,bytes.byteLength/4);
assert.equal(createHash('sha256').update(bytes).digest('hex'),data.baseline_vertex_sha256);assert.equal(data.patches.length,2238); // event residency: A_ARA1 132 + ARA1 1913 + ARA1_B 193 patches
let positionError=0,uvError=0,checked=0,edges=0;
const edgeUv=(edge,t)=>[[t,0],[1,t],[t,1],[0,t]][edge];
function point(p,u,v){return Array.from({length:3},(_,k)=>p.coefficients.reduce((s,c,i)=>s+c[k]*u**(i%4)*v**Math.floor(i/4),0));}
for(const [index,patch]of data.patches.entries()){
 const mesh=buildTerrainPatch(patch);assert.equal(mesh.indices.length,384);
 for(let i=0;i<mesh.parameters.length/2;i++){
  const u=mesh.parameters[i*2],v=mesh.parameters[i*2+1],old=(patch.baseline_vertex_start+Math.round(v*8)*9+Math.round(u*8))*10;
  for(let k=0;k<3;k++)positionError=Math.max(positionError,Math.abs(mesh.vertices[i*10+k]-vertices[old+k]));
  for(let k=6;k<10;k++)uvError=Math.max(uvError,Math.abs(mesh.vertices[i*10+k]-vertices[old+k]));checked++;
 }
 for(const [edge,neighbor]of patch.neighbors.entries())if(neighbor){
  const other=data.patches[neighbor.patch],back=other.neighbors[neighbor.edge];assert(back&&back.patch===index&&back.edge===edge&&back.reverse===neighbor.reverse,'Nonreciprocal adjacency');
  for(let j=0;j<=64;j++){const t=j/64,a=point(patch,...edgeUv(edge,t)),b=point(other,...edgeUv(neighbor.edge,neighbor.reverse?1-t:t));assert(Math.hypot(...a.map((v,i)=>v-b[i]))<.0021,'Matched edge curves diverge');}edges++;
 }
}
assert(positionError<=.001,'Exported polynomial differs from installed render vertices');assert(uvError<=.00003,'UV reconstruction differs from installed asset');
console.log('Authored terrain render package:',{vertices:checked,sharedEdges:edges/2,maxPositionErrorM:positionError,maxUvError:uvError});
const {planTerrainDetail}=await import('./terrain-detail.js');
const sites=JSON.parse(fs.readFileSync(new URL('public/test-data/terrain-clipping-sites.json',import.meta.url)));
for(const site of sites.sites){
 const detail=planTerrainDetail(data.patches,site.point,{focusResource:site.resource}),byIndex=new Map(detail.plan.map(p=>[p.index,p]));
 assert(detail.refined<=8);assert(detail.plan.length<=40);assert.equal(detail.plan.find(p=>p.resource===site.resource)?.resolution,32,'Contact patch was not prioritized');
 for(const p of detail.plan)for(let edge=0;edge<4;edge++){const neighbor=data.patches[p.index].neighbors[edge];if(neighbor)assert.equal(p.edges[edge],byIndex.get(neighbor.patch)?.edges[neighbor.edge]??8,'Detail plan leaves mismatched shared edge');}
}
assert.equal(planTerrainDetail(data.patches,[0,0,0],{radius:0,maxRefined:0}).plan.length,0);
console.log('Bounded terrain detail plans prioritize contact and reconcile neighbor edge resolutions.');
