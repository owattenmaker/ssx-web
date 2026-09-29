import {BufferGeometry,BufferAttribute,InterleavedBuffer,InterleavedBufferAttribute,Mesh} from 'three';
// Merge new edits with pending uploads: reset and replacement can both occur
// before a draw. Dropping old pending ranges would leave stale GPU triangles.
export function markTerrainIndexRanges(attribute,offsets){
 const ranges=[...attribute.updateRanges.map(r=>({...r})),...offsets.map(start=>({start,count:3}))].sort((a,b)=>a.start-b.start),merged=[];
 for(const range of ranges){const last=merged.at(-1);if(last&&range.start<=last.start+last.count)last.count=Math.max(last.start+last.count,range.start+range.count)-last.start;else merged.push({...range});}
 attribute.clearUpdateRanges();for(const range of merged)attribute.addUpdateRange(range.start,range.count);attribute.needsUpdate=true;
 return merged.reduce((n,r)=>n+r.count*attribute.array.BYTES_PER_ELEMENT,0);
}
// Reversible renderer-only replacement. The supplied coarse meshes must contain
// terrain triangles only. Collision buffers and source files are never modified.
export function createTerrainOverlays(parent,coarseMeshes,patches,origin){
 const ranges=patches.map((p,index)=>({start:p.baseline_vertex_start,end:p.baseline_vertex_start+81,index})).sort((a,b)=>a.start-b.start),references=new Map(),records=[];let active=new Map(),uploadStats={rangeBytes:0,fullBufferBytes:0,ranges:0,buffers:0};
 const find=vertex=>{let low=0,high=ranges.length-1;while(low<=high){const mid=(low+high)>>1,r=ranges[mid];if(vertex<r.start)high=mid-1;else if(vertex>=r.end)low=mid+1;else return r;}return null;};
 for(const mesh of coarseMeshes){
  const attribute=mesh.geometry.index;if(!attribute)throw Error('Missing coarse terrain indices');const original=attribute.array.slice(),record={attribute,original};records.push(record);
  for(let offset=0;offset<original.length;offset+=3){const range=find(original[offset]);if(!range||[1,2].some(k=>original[offset+k]<range.start||original[offset+k]>=range.end))throw Error('Coarse triangle crosses patch provenance');
   if(!references.has(range.index))references.set(range.index,{material:mesh.material,records:[],offsets:[]});const entry=references.get(range.index);if(entry.material!==mesh.material)throw Error('Patch spans incompatible materials');entry.records.push(records.length-1);entry.offsets.push(offset);
  }
 }
 // Per patch: its coarse triangles as (record index, index offset) in typed arrays -- ~290k small objects on ARA1
 // otherwise stay live for the whole run (garbage-collector marking work every full collection).
 for(const entry of references.values()){entry.records=Uint32Array.from(entry.records);entry.offsets=Uint32Array.from(entry.offsets);}
 const key=item=>[item.resolution,...item.edges].join(':');
 function commit(items){
  const next=new Map(),created=[];
  try{
   for(const item of items){
    if(!patches[item.index]||patches[item.index].resource!==item.resource||next.has(item.index)||!references.has(item.index))throw Error('Invalid terrain replacement identity');
    const previous=active.get(item.index);if(previous?.key===key(item)){next.set(item.index,previous);continue;}
    const count=item.vertices.length/10;if(!Number.isInteger(count)||item.colors.length!==count*4||item.indices.length%3||!item.vertices.every(Number.isFinite)||!item.colors.every(Number.isFinite)||item.indices.some(i=>i>=count))throw Error('Invalid terrain replacement geometry');
    const geometry=new BufferGeometry();created.push(geometry);const buffer=new InterleavedBuffer(item.vertices,10);
    for(const [name,size,offset]of [['position',3,0],['normal',3,3],['uv',2,6],['uv1',2,8]])geometry.setAttribute(name,new InterleavedBufferAttribute(buffer,size,offset));
    geometry.setAttribute('color',new BufferAttribute(item.colors,4));references.get(item.index).material.userData.prepareTerrainGeometry?.(geometry,patches[item.index]);geometry.setIndex(new BufferAttribute(item.indices,1));geometry.computeBoundingBox();geometry.computeBoundingSphere();
    const mesh=new Mesh(geometry,references.get(item.index).material);mesh.position.copy(origin).negate();mesh.updateMatrix();mesh.matrixAutoUpdate=false;mesh.userData.terrainRefinement=item.resource;next.set(item.index,{key:key(item),mesh});
   }
  }catch(error){for(const geometry of created)geometry.dispose();throw error;}
  // All replacement geometry exists before any coarse triangles are suppressed.
  const dirty=new Map();for(const index of new Set([...active.keys(),...next.keys()])){
   if(active.has(index)===next.has(index))continue;
   const ref=references.get(index);for(let t=0;t<ref.offsets.length;t++){const record=records[ref.records[t]],offset=ref.offsets[t];const a=record.original[offset];record.attribute.array[offset]=a;record.attribute.array[offset+1]=next.has(index)?a:record.original[offset+1];record.attribute.array[offset+2]=next.has(index)?a:record.original[offset+2];if(!dirty.has(record))dirty.set(record,[]);dirty.get(record).push(offset);}
  }
  uploadStats={rangeBytes:0,fullBufferBytes:0,ranges:0,buffers:0};
  for(const [record,offsets]of dirty){uploadStats.buffers++;uploadStats.rangeBytes+=markTerrainIndexRanges(record.attribute,offsets);uploadStats.fullBufferBytes+=record.attribute.array.byteLength;uploadStats.ranges+=record.attribute.updateRanges.length;}
  // The 'dispose' event on the mesh releases its three.js render objects (bindings, uniform buffers, material listeners):
  // disposing only the geometry leaves them registered on the shared terrain material, one set per replaced patch
  // (~0.7 MB/s of JS heap and GPU buffers kept for the whole race). Pipelines stay (warm proxies still use them).
  for(const [index,entry]of active)if(next.get(index)!==entry){parent.remove(entry.mesh);entry.mesh.geometry.dispose();entry.mesh.dispatchEvent({type:'dispose'});}
  for(const [index,entry]of next)if(active.get(index)!==entry)parent.add(entry.mesh);
  active=next;
 }
 // Loading-screen warm-up (main.js warmupRender): one degenerate replacement-layout mesh per terrain material, so the
 // node builds/pipelines of the refined patches (interleaved layout, own ps2LightUv) happen before the race.
 function warmProxies(){
  const out=[],seen=new Set();
  for(const [index,{material}]of references){if(seen.has(material))continue;seen.add(material);
   const geometry=new BufferGeometry(),buffer=new InterleavedBuffer(new Float32Array(30),10);
   for(const [name,size,offset]of [['position',3,0],['normal',3,3],['uv',2,6],['uv1',2,8]])geometry.setAttribute(name,new InterleavedBufferAttribute(buffer,size,offset));
   geometry.setAttribute('color',new BufferAttribute(new Float32Array(12),4));material.userData.prepareTerrainGeometry?.(geometry,patches[index]);geometry.setIndex(new BufferAttribute(new Uint32Array([0,1,2]),1));
   const mesh=new Mesh(geometry,material);mesh.position.copy(origin).negate();mesh.frustumCulled=false;mesh.userData.terrainWarmProxy=true;out.push(mesh);}
  return out;
 }
 return {get uploadStats(){return {...uploadStats};},commit,warmProxies,clear:()=>commit([]),get count(){return active.size;},dispose(){commit([]);},get mappedPatches(){return references.size;}};
}
