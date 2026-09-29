import assert from 'node:assert/strict';import fs from 'node:fs';
import {planTerrainDetail} from './terrain-detail.js';import {createTerrainMeshCache,createTerrainWorkerHandler} from './terrain-worker-core.js';
const data=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/terrain-render.json',import.meta.url))),sites=JSON.parse(fs.readFileSync(new URL('public/test-data/terrain-clipping-sites.json',import.meta.url)));
const plan=planTerrainDetail(data.patches,sites.sites[0].point,{focusResource:sites.sites[0].resource}).plan,cache=createTerrainMeshCache(data.patches,1024*1024);
cache.validate(plan);for(const item of plan){cache.get(item);assert(cache.stats().bytes<=cache.stats().budget);}
const item=plan[0],first=cache.get(item),expected=first.vertices.slice();structuredClone(first,{transfer:[first.vertices.buffer,first.indices.buffer,first.colors.buffer]});assert.equal(first.vertices.byteLength,0);assert.deepEqual(cache.get(item).vertices,expected,'Transfer detached cached geometry');assert(cache.stats().hits>0);
assert.throws(()=>cache.validate([{...item,edges:[64,64,64,64]}]),/edge/);
const messages=[];let resume;const handle=createTerrainWorkerHandler((message,transfers=[])=>messages.push(structuredClone(message,{transfer:transfers})),()=>new Promise(resolve=>{resume=resolve;}));
await handle({data:{type:'init',id:1,patches:data.patches,budget:1024*1024}});
const pending=handle({data:{type:'build',id:2,plan}});assert(resume,'Fixture must yield before publishing');
await handle({data:{type:'build',id:3,plan:[]}});resume();await pending;
assert.equal(messages.find(m=>m.id===2)?.type,'cancelled');assert.equal(messages.find(m=>m.id===3)?.type,'complete');assert(!messages.some(m=>m.id===2&&m.type==='complete'),'Obsolete batch was published');
const complete=[];const normal=createTerrainWorkerHandler((message,transfers=[])=>complete.push(structuredClone(message,{transfer:transfers})),async()=>{});
await normal({data:{type:'init',id:4,patches:data.patches}});await normal({data:{type:'build',id:5,plan}});await normal({data:{type:'build',id:6,plan}});
const result=complete.find(m=>m.id===6);assert.equal(result.type,'complete');assert.equal(result.meshes.length,plan.length);assert.equal(result.stats.builds,plan.length);assert.equal(result.stats.hits,plan.length);assert(result.meshes.every(m=>m.vertices.byteLength>0&&m.indices.byteLength>0));
await normal({data:{type:'clear',id:7}});await normal({data:{type:'build',id:8,plan:[{...item,resource:-1}]}});assert.equal(complete.at(-1).type,'error');
console.log('Terrain worker: bounded cache, transfer ownership, complete batches, latest-request cancellation and edge validation pass.',{patches:plan.length,cacheBytes:result.stats.bytes});
