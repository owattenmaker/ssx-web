import {planTerrainDetail} from './terrain-detail.js';
import {createTerrainOverlays} from './terrain-overlays.js';
import {createGuardedWorker,workerUrl} from './worker-guard.js';import {createTerrainWorkerHandler} from './terrain-worker-core.js';

// the worker script of this build (web/worker-guard.js: build handshake; the same handler on the main thread if it cannot start,
// docs/workers.md)
const TERRAIN_WORKER=workerUrl(Worker=>new Worker(new URL('./terrain-worker.js',import.meta.url),{type:'module'}));
export async function createTerrainRefinement(parent,origin,root='/assets/ARA1/'){
 const response=await fetch(root+'terrain-render.json');if(!response.ok)throw Error('Missing terrain render package');const data=await response.json();
 if(data.version!==1||data.source_sha256!==parent.userData.courseHash)throw Error('Terrain render package/source mismatch');
 const overlays=createTerrainOverlays(parent,parent.children.filter(m=>m.userData.terrainBase),data.patches,origin);
 if(overlays.mappedPatches!==data.patches.length)throw Error('Some terrain patches cannot be replaced');
 const worker=createGuardedWorker({name:'terrain',url:TERRAIN_WORKER,raw:true,local:send=>createTerrainWorkerHandler(send)});
 const client=await createTerrainRefinementClient({patches:data.patches,worker,overlays,
  onState:state=>{document.body.dataset.terrainRefinement=String(state.count);document.body.dataset.terrainResources=state.resources.join(',');},
  onError:error=>{document.body.dataset.terrainError=error;console.error('Terrain refinement:',error);}});
 // The worker has its own copy of the patches (the init message); the main thread only plans (bounds, neighbors,
 // resource) and maps (baseline_vertex_start): drop the per-patch surface arrays (~60k small arrays kept live all race).
 for(const patch of data.patches){delete patch.coefficients;delete patch.textureUv;delete patch.lightUv;delete patch.color;}
 return client;
}
// Transport and rendering are injected so reply ordering can be tested directly.
export async function createTerrainRefinementClient({patches,worker,overlays,onState=()=>{},onError=()=>{}}){
 let serial=0,current=0,key='',requestedPlan=[],previousRefined=[],pending=null,lastTime=-Infinity,lastFocus=-1,disposed=false,failed=null,stats={refined:0,affected:0,commits:0};
 let resolveReady,rejectReady;const ready=new Promise((resolve,reject)=>{resolveReady=resolve;rejectReady=reject;});
 function fail(error){failed=String(error?.message||error);onError(failed);rejectReady(Error(failed));}
 worker.onerror=event=>fail(event.message);
 worker.onmessage=({data:message})=>{
  if(disposed)return;if(message.type==='ready'){resolveReady();return;}
  if(message.id!==current)return;
  if(message.type==='error'){fail(message.error);return;}
  if(message.type==='complete')pending=message;
 };
 worker.postMessage({type:'init',id:0,patches});
 try{await ready;}catch(error){worker.terminate();overlays.dispose();throw error;}
 return {
   update(position, focusResource, seconds) {
     if (disposed || failed || (seconds - lastTime < 0.1 && focusResource === lastFocus)) return;
     lastTime = seconds;
     lastFocus = focusResource;
     const detail = planTerrainDetail(patches, position, { focusResource, previousRefined }),
       next = JSON.stringify(detail.plan);
     if (next === key) return;
     key = next;
     requestedPlan = detail.plan;
     current = ++serial;
     pending = null;
     worker.postMessage({ type: 'build', id: current, plan: detail.plan });
   },
   commit() {
     if (!pending || disposed || failed) return;
     const result = pending;
     pending = null;
     if (result.id !== current) return;
     try {
       if (
         result.meshes.length !== requestedPlan.length ||
         result.meshes.some((mesh, i) => {
           const expected = requestedPlan[i];
           return (
             mesh.index !== expected.index ||
             mesh.resource !== expected.resource ||
             mesh.resolution !== expected.resolution ||
             mesh.edges?.length !== 4 ||
             mesh.edges.some((n, j) => n !== expected.edges[j])
           );
         })
       )
         throw Error('Terrain worker returned an incomplete or mismatched plan');
       overlays.commit(result.meshes);
       previousRefined = result.meshes.filter((m) => m.resolution > 8).map((m) => m.index);
       stats.refined = result.meshes.filter((m) => m.resolution > 8).length;
       stats.affected = overlays.count;
       stats.commits++;
       stats.cacheBytes = result.stats.bytes;
       onState({ count: overlays.count, resources: result.meshes.filter((m) => m.resolution > 8).map((m) => m.resource) });
     } catch (error) {
       fail(error);
     }
   },
   reset() {
     if (disposed) return;
     current = ++serial;
     key = '';
     requestedPlan = [];
     previousRefined = [];
     pending = null;
     lastTime = -Infinity;
     lastFocus = -1;
     overlays.clear();
     stats.affected = stats.refined = 0;
     onState({ count: 0, resources: [] });
     worker.postMessage({ type: 'build', id: current, plan: [] });
   },
   warmProxies() {
     return overlays.warmProxies?.() ?? [];
   },
   get stats() {
     return { ...stats, error: failed };
   },
   dispose() {
     if (disposed) return;
     disposed = true;
     current = ++serial;
     pending = null;
     worker.terminate();
     overlays.dispose();
   }
 };
}
