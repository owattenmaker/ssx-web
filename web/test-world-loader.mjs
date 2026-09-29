import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore();
const text=fs.readFileSync('../local/assets/native/ARA1/world_collision.json','utf8');
const packageData=JSON.parse(text);
const scripted=new Set(JSON.parse(fs.readFileSync('../local/event-activation/scripted-instances.json','utf8')).instances.map(x=>x.resource));
const allocated=[];
const str=text=>{const data=new TextEncoder().encode(text+'\0'),p=c._malloc(data.length);c.HEAPU8.set(data,p);allocated.push(p);return p;};
const packagePointer=str(text);c._init_world_collision(packagePointer,str(packageData.source_sha256));
const info=()=>Array.from(new Int32Array(c.HEAPU8.buffer,c._world_collision_info(),8));
const terrain=JSON.parse(fs.readFileSync('public/assets/ARA1/terrain.json'));c._init_body_terrain(str(JSON.stringify(terrain)));
const counts=info();assert.equal(counts[7],terrain.patches.length);
assert.throws(()=>c._init_body_terrain(str(JSON.stringify({...terrain,source_sha256:'wrong-source'}))));assert.deepEqual(info(),counts,'rejected terrain replaced valid body world');assert.equal(counts[0],packageData.instances.length);
const types=[0,0,0,0];
for(const instance of packageData.instances){const type=packageData.bindings[instance.track].descriptors[instance.collision_descriptor].type;if(type<4)types[type]++;}
let expectedNodes=0,expectedUnsupported=0;
for(const instance of packageData.instances){const descriptor=packageData.bindings[instance.track].descriptors[instance.collision_descriptor];if(descriptor.type===0)continue;
 // Authored dynamic instances with a verified countdown state and contact class are static until contact.
 expectedUnsupported+=instance.scale===0||descriptor.type>3||!(descriptor.flags&0x200000)||(!!(descriptor.flags&0x40000000)&&!scripted.has((instance.rid<<8)|instance.track));
 const model=instance.model_resource,key=`${model&255}:${model>>>8}`;
 expectedNodes+=packageData.render_model_nodes[key].nodes.filter(n=>n.draw_bounds_cm!=null).length;
}
assert.equal(counts[6],expectedNodes,'sphere-tree model nodes were skipped');
assert.equal(counts[5],expectedUnsupported,'decoded static geometry remains unsupported');
assert.deepEqual(counts.slice(1,5),types);assert(counts[6]>0,'static collision nodes not loaded');
assert.throws(()=>c._init_world_collision(packagePointer,str('wrong-source')));
assert.deepEqual(info(),counts,'rejected package replaced valid collision world');
for(const p of allocated)c._free(p);
assert.deepEqual(info(),counts,'collision objects retain freed JSON pointers');
console.log('Actual-WASM world loader:',{instances:counts[0],types,nodes:counts[6],unsupportedForBody:counts[5],terrainPatches:counts[7]});
