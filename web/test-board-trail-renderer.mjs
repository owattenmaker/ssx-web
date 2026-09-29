import assert from 'node:assert/strict';
import {Vector3} from 'three';
import {createBoardTrail} from './board-trail.js';
const originalFetch=globalThis.fetch;
globalThis.fetch=async url=>url.endsWith('.json')?{json:async()=>({texture:{file:'test.rgba',width:1,height:1,normalized_sample_alpha_multiplier:1}})}:{arrayBuffer:async()=>new Uint8Array([255,255,255,255]).buffer};
try{
 const renderer=await createBoardTrail(new Vector3(10,20,30)),heap=new Float32Array(40000);
 const core={HEAPF32:heap,_trail_info:()=>0,_trail_ribbon:()=>64,_trail_roof:()=>80000};
 const ribbon=renderer.group.children[3].geometry,roof=renderer.group.children[0].geometry;
 const step=(r,s)=>{heap[0]=r;heap[1]=s;heap[2]++;for(const [at,n] of [[16,r],[20000,s]])for(let i=0;i<n*9;i++)heap[at+i]=i+100;renderer.update(core);};
 step(30,6);
 for(const [g,n] of [[ribbon,30],[roof,6]])for(const a of Object.values(g.attributes))assert.deepEqual(a.updateRanges,[{start:0,count:n*a.itemSize}]);
 assert.deepEqual(Array.from(ribbon.attributes.position.array.slice(0,3)),[-9,Math.fround(-18.98),Math.fround(-31.01)]);
 const v=ribbon.attributes.position.version;renderer.update(core);assert.equal(ribbon.attributes.position.version,v,'same serial skips upload');
 step(10,2);assert.deepEqual(ribbon.attributes.position.updateRanges,[{start:0,count:30}],'shrink replaces queued prefix');
 step(0,0);assert.equal(ribbon.drawRange.count,0);assert.deepEqual(ribbon.attributes.position.updateRanges,[]);
 step(100,20);assert.deepEqual(ribbon.attributes.position.updateRanges,[{start:0,count:300}],'growth uploads all newly visible vertices');
 heap[0]=2001;heap[2]++;assert.throws(()=>renderer.update(core),/capacity/);
 console.log('Board trail renderer: active upload ranges, shrink, empty, growth and serial guard passed');
}finally{globalThis.fetch=originalFetch;}
