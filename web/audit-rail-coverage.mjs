import fs from 'node:fs';
import createCore from './runtime/core.js';
const root=new URL('./',import.meta.url),data=JSON.parse(fs.readFileSync(new URL('public/assets/ARA1/rails.json',root))),core=await createCore();
const put=text=>{const bytes=Buffer.from(text+'\0'),p=core._malloc(bytes.length);core.HEAPU8.set(bytes,p);return p;};
const catalog=put(JSON.stringify(data)),hash=put(data.source_sha256);core._init_rails(catalog,hash);core._free(catalog);core._free(hash);
const report={scope:'Authored spline query coverage, not gameplay latch acceptance or dynamic-object rails',sourceSha256:data.source_sha256,rails:data.rails.length,segments:0,samples:0,misses:[],distant:[],competing:[],maxDistanceCm:0};
for(const rail of data.rails)for(const segment of rail.segments){
 report.segments++;
 for(let step=0;step<=20;step++){
  const t=step/20,c=segment.source.coefficients;
  const point=[0,1,2].map(k=>Math.fround(((c[0][k]*t+c[1][k])*t+c[2][k])*t+c[3][k]));
  const out=new Float32Array(core.HEAPF32.buffer,core._rail_query(...point),14).slice();
  const row={rail:rail.name,localSegment:segment.index,t,point,selectedId:out[2],selectedSegment:out[3],distanceCm:out[5]};
  report.samples++;
  if(!out[1])report.misses.push(row);
  else{
   report.maxDistanceCm=Math.max(report.maxDistanceCm,out[5]);
   if(out[5]>2)report.distant.push(row);
   if(out[2]!==rail.packed_id)report.competing.push(row);
  }
 }
}
fs.writeFileSync(new URL('../local/browser-validation/rail-coverage.json',root),JSON.stringify(report,null,2)+'\n');
console.log({...report,misses:report.misses.length,distant:report.distant.length,competing:report.competing.length});
