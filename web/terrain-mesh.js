// Renderer-only bicubic mesh. Edge resolutions may exceed the interior grid so
// a coarse patch can meet a refined neighbor without a T-junction. UV edge order:
// v=0, u=1, v=1, u=0. Geometry comes from the authored polynomial, never physics.
export function buildTerrainPatch(patch,resolution=8,edges=Array(4).fill(resolution)){
 const valid=n=>Number.isInteger(n)&&n>=1&&n<=64&&(n&(n-1))===0;
 if(!valid(resolution)||edges.length!==4||edges.some(n=>!valid(n)||n<resolution))throw Error('Invalid terrain grid/edge resolution');
 const c=patch.coefficients;if(c.length!==16||c.some(v=>v.length!==3||!v.every(Number.isFinite)))throw Error('Invalid terrain polynomial');
 const uv=patch.textureUv,light=patch.lightUv,color=patch.color||[1,1,1,1];
 if(uv?.length!==4||uv.some(v=>v.length!==2||!v.every(Number.isFinite))||light?.length!==4||!light.every(Number.isFinite))throw Error('Invalid authored terrain UVs');
 const values=[],parameters=[],colors=[],indices=[],lookup=new Map();
 const at=(u,v,du=0,dv=0)=>Array.from({length:3},(_,k)=>{let sum=0;for(let j=dv;j<4;j++)for(let i=du;i<4;i++)sum+=c[j*4+i][k]*(du?i:1)*(dv?j:1)*u**(i-du)*v**(j-dv);return sum;});
 const vertex=(u,v)=>{
  const key=Math.round(u*128)+129*Math.round(v*128);if(lookup.has(key))return lookup.get(key);
  const p=at(u,v),a=at(u,v,1),b=at(u,v,0,1),normal=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]],length=Math.hypot(...normal);
  const n=length>1e-10?normal.map(v=>v/length):[0,1,0];
  const tex=[0,1].map(k=>uv[0][k]*(1-u)*(1-v)+uv[1][k]*(1-u)*v+uv[2][k]*u*(1-v)+uv[3][k]*u*v);
  const id=values.length/10;lookup.set(key,id);values.push(...p,...n,...tex,light[0]+u*light[2],light[1]+v*light[3]);parameters.push(u,v);colors.push(...color);return id;
 };
 for(let y=0;y<resolution;y++)for(let x=0;x<resolution;x++){
  const u=x/resolution,v=y/resolution,h=1/resolution;
  const segments=[y===0?edges[0]/resolution:1,x===resolution-1?edges[1]/resolution:1,y===resolution-1?edges[2]/resolution:1,x===0?edges[3]/resolution:1],boundary=[];
  for(let j=0;j<segments[0];j++)boundary.push(vertex(u+h*j/segments[0],v));
  for(let j=0;j<segments[1];j++)boundary.push(vertex(u+h,v+h*j/segments[1]));
  for(let j=0;j<segments[2];j++)boundary.push(vertex(u+h-h*j/segments[2],v+h));
  for(let j=0;j<segments[3];j++)boundary.push(vertex(u,v+h-h*j/segments[3]));
  if(boundary.length===4)indices.push(boundary[0],boundary[1],boundary[3],boundary[1],boundary[2],boundary[3]);
  else {const center=vertex(u+h/2,v+h/2);for(let i=0;i<boundary.length;i++)indices.push(center,boundary[i],boundary[(i+1)%boundary.length]);}
 }
 return {vertices:new Float32Array(values),indices:new Uint32Array(indices),colors:new Float32Array(colors),parameters:new Float32Array(parameters)};
}
