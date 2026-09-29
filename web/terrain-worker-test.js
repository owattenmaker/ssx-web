import {planTerrainDetail} from './terrain-detail.js';
const result=document.querySelector('#result');let worker;
try{
 const [data,sites]=await Promise.all(['/assets/ARA1/terrain-render.json','/test-data/terrain-clipping-sites.json'].map(async url=>{const response=await fetch(url);if(!response.ok)throw Error('Missing terrain worker fixture');return response.json();}));
 const plan=planTerrainDetail(data.patches,sites.sites[0].point,{focusResource:sites.sites[0].resource}).plan;
 worker=new Worker(new URL('./terrain-worker.js',import.meta.url),{type:'module'});
 const pending=new Map();let id=0;
 worker.onmessage=({data:message})=>{const entry=pending.get(message.id);if(!entry)return;pending.delete(message.id);clearTimeout(entry.timer);if(message.type==='error')entry.reject(Error(message.error));else entry.resolve(message);};
 worker.onerror=error=>{for(const entry of pending.values()){clearTimeout(entry.timer);entry.reject(Error(error.message));}pending.clear();};
 const request=message=>new Promise((resolve,reject)=>{const next=++id,timer=setTimeout(()=>{pending.delete(next);reject(Error('Worker request timed out'));},15000);pending.set(next,{resolve,reject,timer});worker.postMessage({...message,id:next});});
 await request({type:'init',patches:data.patches,budget:8*1024*1024});
 const start=performance.now(),first=await request({type:'build',plan}),coldMs=performance.now()-start;
 const warmStart=performance.now(),second=await request({type:'build',plan}),warmMs=performance.now()-warmStart;
 if(first.type!=='complete'||second.type!=='complete'||first.meshes.length!==plan.length||second.stats.builds!==plan.length||second.stats.hits!==plan.length)throw Error('Worker did not reuse complete cached batch');
 const hash=mesh=>{let h=2166136261;for(const array of [mesh.vertices,mesh.indices,mesh.colors])for(const b of new Uint8Array(array.buffer,array.byteOffset,array.byteLength))h=Math.imul(h^b,16777619)>>>0;return h;};
 if(first.meshes.some((m,i)=>!m.vertices.byteLength||hash(m)!==hash(second.meshes[i])))throw Error('Cached data changed after transfer');
 if(second.stats.bytes>second.stats.budget)throw Error('Worker cache exceeded budget');
 await request({type:'clear'});const empty=await request({type:'build',plan:[]});if(empty.meshes.length||empty.stats.bytes)throw Error('Clear retained cached geometry');
 result.textContent=JSON.stringify({passed:true,patches:plan.length,cacheBytes:second.stats.bytes,coldMs,warmMs,scope:'Actual module worker/cache/transfer integration; timings are one QA request, not gameplay frame-time or power measurements'},null,2);
}catch(error){result.textContent='FAILED: '+error.stack;}
finally{worker?.terminate();}
