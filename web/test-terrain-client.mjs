import fs from 'node:fs';import assert from 'node:assert/strict';import {createTerrainRefinementClient} from './terrain-refinement.js';
const patches=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/terrain-render.json',import.meta.url))).patches,sites=JSON.parse(fs.readFileSync(new URL('public/test-data/terrain-clipping-sites.json',import.meta.url))).sites;
async function fixture(){
 const sent=[],commits=[],errors=[];let count=0,terminated=false,clears=0;
 const worker={postMessage(m){sent.push(structuredClone(m));if(m.type==='init')queueMicrotask(()=>worker.onmessage({data:{type:'ready',id:0}}));},terminate(){terminated=true;}};
 const overlays={commit(meshes){commits.push(meshes);count=meshes.length;},clear(){count=0;clears++;},dispose(){count=0;},get count(){return count;}};
 const client=await createTerrainRefinementClient({patches,worker,overlays,onError:e=>errors.push(e)});
 const reply=(request,meshes=request.plan)=>worker.onmessage({data:{type:'complete',id:request.id,meshes:structuredClone(meshes),stats:{bytes:123}}});
 return {client,sent,commits,errors,worker,reply,get terminated(){return terminated;},get clears(){return clears;}};
}
const f=await fixture(),site=sites[0];f.client.update(site.point,site.resource,0);const first=f.sent.at(-1);assert(first.plan.length);assert.equal(f.client.stats.refined,0,'Wanted detail reported as committed');
f.reply(first);f.client.reset();f.reply(first);f.client.commit();assert.equal(f.commits.length,0,'Pre-reset result committed');assert.equal(f.client.stats.refined,0);
f.client.update(site.point,site.resource,0);const second=f.sent.at(-1);f.reply(second);f.client.commit();assert.equal(f.commits.length,1);assert(f.client.stats.refined>0);
f.client.update([0,0,0],-1,1);const empty=f.sent.at(-1);f.reply(second);f.client.commit();assert.equal(f.commits.length,1,'Superseded result committed');f.reply(empty);f.client.commit();assert.equal(f.client.stats.affected,0);
f.client.dispose();f.reply(second);f.client.commit();assert.equal(f.commits.length,2);assert(f.terminated);
const invalid=await fixture();invalid.client.update(site.point,site.resource,0);const request=invalid.sent.at(-1);invalid.reply(request,request.plan.slice(1));invalid.client.commit();assert.equal(invalid.commits.length,0);assert.match(invalid.errors[0],/incomplete or mismatched/);invalid.client.dispose();
console.log('Terrain client rejects stale/reset/disposed replies and incomplete batches; committed stats remain accurate.');
