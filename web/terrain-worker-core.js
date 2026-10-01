import {buildTerrainPatch} from './terrain-mesh.js';
const meshBytes=m=>m.vertices.byteLength+m.indices.byteLength+m.colors.byteLength;
export function createTerrainMeshCache(patches,budget=8*1024*1024){
 if(!Number.isSafeInteger(budget)||budget<0)throw Error('Invalid terrain cache budget');
 const cache=new Map();let bytes=0,hits=0,builds=0;
 function validate(plan){
  if(!Array.isArray(plan)||plan.length>40)throw Error('Terrain batch exceeds planned patch limit');
  const byIndex=new Map();
  for (const item of plan) {
    if (!Number.isInteger(item.index) || !patches[item.index] || patches[item.index].resource !== item.resource || byIndex.has(item.index))
      throw Error('Invalid terrain patch identity');
    byIndex.set(item.index, item);
    if (
      ![8, 16, 32, 64].includes(item.resolution) ||
      item.edges?.length !== 4 ||
      item.edges.some((n) => ![8, 16, 32, 64].includes(n) || n < item.resolution)
    )
      throw Error('Invalid terrain detail resolution');
  }
  for(const item of plan)for(let edge=0;edge<4;edge++){
   const neighbor=patches[item.index].neighbors[edge];
   if(!neighbor){if(item.edges[edge]!==8)throw Error('Unresolved terrain edge cannot be refined');continue;}
   const back=patches[neighbor.patch]?.neighbors[neighbor.edge];if(!back||back.patch!==item.index||back.edge!==edge)throw Error('Nonreciprocal terrain neighbor');
   if(item.edges[edge] !== (byIndex.get(neighbor.patch)?.edges[neighbor.edge]??8))throw Error('Terrain batch would open a shared edge');
  }
 }
 function get(item){
  const key=[item.index,item.resolution,...item.edges].join(':');let mesh=cache.get(key);
  if(mesh){hits++;cache.delete(key);cache.set(key,mesh);}else{
   builds++;const generated=buildTerrainPatch(patches[item.index],item.resolution,item.edges);mesh={vertices:generated.vertices,indices:generated.indices,colors:generated.colors};
   const size = meshBytes(mesh);
   if (size <= budget) {
     while (bytes + size > budget && cache.size) {
       const oldest = cache.keys().next().value;
       bytes -= meshBytes(cache.get(oldest));
       cache.delete(oldest);
     }
     cache.set(key, mesh);
     bytes += size;
   }
  }
  // Transfer copies, never the cached buffers themselves.
  return {index:item.index,resource:item.resource,resolution:item.resolution,edges:[...item.edges],vertices:mesh.vertices.slice(),indices:mesh.indices.slice(),colors:mesh.colors.slice()};
 }
 return {validate,get,stats:()=>({bytes,budget,entries:cache.size,hits,builds}),clear(){cache.clear();bytes=0;}};
}
export function createTerrainWorkerHandler(send,yieldTask=()=>new Promise(resolve=>setTimeout(resolve,0))){
 let cache=null,generation=0;
 return async function receive({data:message}){
  const id=message?.id;
  try{
   if(message.type==='init'){generation++;cache=createTerrainMeshCache(message.patches,message.budget);send({type:'ready',id,stats:cache.stats()});return;}
   if(message.type==='clear'){generation++;cache?.clear();send({type:'cleared',id});return;}
   if(message.type!=='build'||!cache)throw Error('Terrain worker is not initialized or request is unsupported');
   const ticket=++generation,active=cache;active.validate(message.plan);const meshes=[];
   for(let i=0;i<message.plan.length;i++){
    if(ticket!==generation){send({type:'cancelled',id});return;}
    meshes.push(active.get(message.plan[i]));if(i%4===3)await yieldTask();
   }
   if(ticket!==generation){send({type:'cancelled',id});return;}
   const transfers=meshes.flatMap(m=>[m.vertices.buffer,m.indices.buffer,m.colors.buffer]);
   send({type:'complete',id,meshes,stats:active.stats()},transfers);
  }catch(error){send({type:'error',id,error:String(error?.message||error)});}
 };
}
