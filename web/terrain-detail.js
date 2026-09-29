// Bounded near-rider detail plan; worker/cache and atomic mesh swaps own execution.
// Only patches with unambiguous full-edge neighbors are eligible for refinement.
export function planTerrainDetail(patches,position,{radius=30,maxRefined=8,resolution=32,focusResource=-1,previousRefined=[],hysteresis=2}={}){
 if(!Number.isFinite(hysteresis)||hysteresis<0)throw Error('Invalid terrain hysteresis');
 const retained=new Set(previousRefined);
 if(position.length!==3||!position.every(Number.isFinite)||!Number.isFinite(radius)||radius<0||!Number.isInteger(maxRefined)||maxRefined<0||![16,32,64].includes(resolution))throw Error('Invalid terrain detail request');
 // Called ~10x a second over every patch (2238 on ARA1): distances without per-patch arrays/objects; only the
 // candidates get a record (same values and order as mapping every patch, then filtering).
 const px=position[0],py=position[1],pz=position[2],candidates=[];
 for(let index=0;index<patches.length;index++){
  const patch=patches[index],lo=patch.bounds[0],hi=patch.bounds[1];
  const distance=Math.hypot(Math.max(lo[0]-px,0,px-hi[0]),Math.max(lo[1]-py,0,py-hi[1]),Math.max(lo[2]-pz,0,pz-hi[2]));
  if(distance<=radius+(retained.has(index)?hysteresis:0)||patch.resource===focusResource)candidates.push({patch,index,distance});
 }
 const eligible=candidates.filter(x=>x.patch.neighbors.every(n=>n!==null));eligible.sort((a,b)=>(Number(b.patch.resource===focusResource)-Number(a.patch.resource===focusResource))||(a.distance-(retained.has(a.index)?hysteresis:0))-(b.distance-(retained.has(b.index)?hysteresis:0))||a.patch.resource-b.patch.resource);
 const selected=new Set(eligible.slice(0,maxRefined).map(x=>x.index)),affected=new Set(selected);
 for(const i of selected)for(const n of patches[i].neighbors)affected.add(n.patch);
 const plan=[...affected].sort((a,b)=>a-b).map(index=>{
  const n=selected.has(index)?resolution:8,edges=patches[index].neighbors.map(neighbor=>Math.max(n,neighbor&&selected.has(neighbor.patch)?resolution:8));
  return {index,resource:patches[index].resource,resolution:n,edges};
 });
 return {plan,refined:selected.size,excludedNear:candidates.length-eligible.length};
}
