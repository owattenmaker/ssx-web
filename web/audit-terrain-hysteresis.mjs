import fs from 'node:fs';import assert from 'node:assert/strict';import {planTerrainDetail} from './terrain-detail.js';
const root=new URL('../local/browser-validation/',import.meta.url),data=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/terrain-render.json',import.meta.url))),route=JSON.parse(fs.readFileSync(new URL('terrain-detail-route.json',root)));assert.equal(data.source_sha256,route.source_sha256);
const results=[];
for(const hysteresis of [0,2]){
 let previousRefined=[],previous=new Map(),key='',planChanges=0,changed=0,removed=0;
 for(const sample of route.positions){
  const detail=planTerrainDetail(data.patches,sample.position,{focusResource:sample.focus,previousRefined,hysteresis}),next=JSON.stringify(detail.plan);assert(detail.refined<=8&&detail.plan.length<=40);
  const focus=data.patches.find(p=>p.resource===sample.focus);if(focus?.neighbors.every(Boolean))assert(detail.plan.some(p=>p.resource===sample.focus&&p.resolution===32),'Contact patch lost detail');
  if(next===key)continue;key=next;planChanges++;const current=new Map(detail.plan.map(p=>[p.index,[p.resolution,...p.edges].join(':')]));changed+=detail.plan.filter(p=>previous.get(p.index)!==current.get(p.index)).length;removed+=[...previous.keys()].filter(i=>!current.has(i)).length;previous=current;previousRefined=detail.plan.filter(p=>p.resolution>8).map(p=>p.index);
 }
 results.push({hysteresis,planChanges,newOrChangedMeshes:changed,removedMeshes:removed});
}
const report={samples:route.positions.length,results,scope:'Selection-only replay assuming immediate commit; not actual asynchronous request counts, GPU time or measured visual popping'};fs.writeFileSync(new URL('terrain-hysteresis-comparison.json',root),JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));
