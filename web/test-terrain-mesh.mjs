import assert from 'node:assert/strict';import fs from 'node:fs';import {buildTerrainPatch} from './terrain-mesh.js';
const make=()=>({coefficients:Array.from({length:16},()=>[0,0,0]),textureUv:[[0,0],[0,1],[1,0],[1,1]],lightUv:[.1,.2,.3,.4],color:[.5,.5,.5,1]});
const left=make();left.coefficients[1][0]=1;left.coefficients[4][2]=1;left.coefficients[8][1]=.25;const right=structuredClone(left);right.coefficients[0][0]=1;
const a=buildTerrainPatch(left,8,[8,32,8,8]),b=buildTerrainPatch(right,32);
const edge=(mesh,u)=>{const points=[];for(let i=0;i<mesh.parameters.length/2;i++)if(mesh.parameters[i*2]===u)points.push([mesh.parameters[i*2+1],...mesh.vertices.slice(i*10,i*10+3)]);return points.sort((a,b)=>a[0]-b[0]);};
assert.deepEqual(edge(a,1),edge(b,0));assert.equal(edge(a,1).length,33);
for(const mesh of [a,b]){
 const counts=new Map();let area=0;
 for(let i=0;i<mesh.indices.length;i+=3){const ids=Array.from(mesh.indices.slice(i,i+3));const p=ids.map(i=>mesh.parameters.slice(i*2,i*2+2));const signed=((p[1][0]-p[0][0])*(p[2][1]-p[0][1])-(p[1][1]-p[0][1])*(p[2][0]-p[0][0]))/2;assert(signed>0,'Reversed or degenerate triangle');area+=signed;
 for(let j=0;j<3;j++){const x=ids[j],y=ids[(j+1)%3],key=[Math.min(x,y),Math.max(x,y)].join(',');counts.set(key,(counts.get(key)||0)+1);}}
 assert.equal(area,1,'Patch has overlap or missing area');
 for(const [key,count]of counts){assert(count===1||count===2);if(count===1){const [i,j]=key.split(',').map(Number),u=mesh.parameters[i*2],v=mesh.parameters[i*2+1],x=mesh.parameters[j*2],y=mesh.parameters[j*2+1];assert((u===x&&(u===0||u===1))||(v===y&&(v===0||v===1)),'Interior crack');}}
 for(let i=0;i<mesh.parameters.length/2;i++){const u=mesh.parameters[i*2],v=mesh.parameters[i*2+1];assert.equal(mesh.vertices[i*10+6],u);assert.equal(mesh.vertices[i*10+7],v);assert(Math.abs(mesh.vertices[i*10+8]-(.1+u*.3))<1e-7);}
}
const terrain=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/terrain.json',import.meta.url))),report=JSON.parse(fs.readFileSync(new URL('public/test-data/terrain-clipping-sites.json',import.meta.url)));
function height(mesh,p){let best;for(let i=0;i<mesh.indices.length;i+=3){const [a,b,c]=Array.from(mesh.indices.slice(i,i+3),i=>mesh.vertices.slice(i*10,i*10+3));const ax=b[0]-a[0],az=b[2]-a[2],bx=c[0]-a[0],bz=c[2]-a[2],dx=p[0]-a[0],dz=p[2]-a[2],d=ax*bz-az*bx;if(Math.abs(d)<1e-8)continue;const s=(dx*bz-dz*bx)/d,t=(ax*dz-az*dx)/d;if(Math.min(s,t,1-s-t)<-1e-6)continue;const y=a[1]+s*(b[1]-a[1])+t*(c[1]-a[1]);if(best===undefined||Math.abs(y-p[1])<Math.abs(best-p[1]))best=y;}return best;}
assert.equal(report.source_sha256,terrain.source_sha256);
const results=[];for(const row of report.sites){
 const patch={...make(),coefficients:terrain.patches.find(p=>p.resource_id===row.resource).coefficients},point=[...row.point];
 const coarse=Math.abs(height(buildTerrainPatch(patch,8),point)-point[1]),fine=Math.abs(height(buildTerrainPatch(patch,32),point)-point[1]);assert(fine<coarse&&fine<.03);results.push({resource:row.resource,tick:row.tick,coarseErrorM:coarse,refinedErrorM:fine});
}
console.log('Stitched terrain geometry and replay-site refinement:',results);
