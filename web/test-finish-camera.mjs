// Finish camera: the course finish (rider finish 0x125108 -> 0x162258) fades the director to
// POST_RACE_1 in the same tick's camera update (0.016793445 per update, old node unlinked after
// 60 updates, as the original ARMSX2 finish capture shows); a race reset returns to DEFAULT_3.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c=await createCore(),root='public/assets/';
process.on('uncaughtException',e=>{console.error(e instanceof Error?e:c.getExceptionMessage(e));process.exit(1);});
const read=p=>JSON.parse(fs.readFileSync(root+p));const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;};const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const mesh=fs.readFileSync(root+'ARA1/collision.bin'),mp=put(mesh);c._init_world(mp,mesh.length/4);c._free(mp);const tp=str(read('ARA1/terrain.json'));c._init_terrain(tp);c._free(tp);
const settings=read('ANIMATIONS/initial.json'),event=settings.original_race_event.original_race_event;
const cfg=str(settings);c._init_race(cfg);c._free(cfg);c._reset_race();
const f=(p,n)=>new Float32Array(c.HEAPF32.buffer,p,n).slice();
const RATE=new Float32Array(new Uint32Array([0x3c899268]).buffer)[0];
const pointAt=(path,d)=>{const point=path.origin.slice();for(const segment of path.segments){const length=Math.min(d,segment[3]);for(let k=0;k<3;k++)point[k]+=segment[k]*length;if(d<segment[3])return point;d-=segment[3];}return point;};
const path=event.paths.find(p=>p.index===7),finishAt=path.events.find(e=>e.type===1).start;
const step=(p,q)=>{c._reset_rider(p[0]/100,p[2]/100,-p[1]/100,0);c._set_rider_velocity(...q.map((x,k)=>(x-p[k])*60));c._race_begin();const clock=f(c._race_end(),8);
  const cam=f(c._step_camera_head(p[0],p[1],p[2]+150),9);return {clock,cam,director:f(c._camera_director_info(),8)};};
let finishTick=-1,t=0,after=[];
// Ride original physics down path 7 through the authored finish (neutral input).
{const p=pointAt(path,finishAt-1200),q=pointAt(path,finishAt-1175);c._reset_rider(p[0]/100,p[2]/100,-p[1]/100,Math.atan2(q[0]-p[0],q[1]-p[1]));c._set_rider_velocity(...q.map((x,k)=>(x-p[k])*60));}
for(;t<900;t++){
  c._race_begin();const s=f(c._step_rider(0,0,0,0),16);const clock=f(c._race_end(),8);
  const cam=f(c._step_camera_head(s[0]*100,-s[2]*100,s[1]*100+150),9),director=f(c._camera_director_info(),8);
  if(finishTick<0&&!clock[2]){assert.equal(director[0],1,'extra camera node before the finish');assert.equal(director[1],0x3d);}
  if(clock[2]){finishTick=t;assert.equal(director[0],2,'finish did not push POST_RACE_1 in the same tick');assert.equal(director[1],0x44);
    assert.equal(director[2],RATE,'POST_RACE_1 weight after its first update');}
  if(finishTick>=0)after.push(director);
  assert(cam.every(Number.isFinite),'nonfinite finish camera');
  if(after.length>=80)break;
}
assert(finishTick>=0,'authored finish not reached');
const unlinkedAt=after.findIndex(x=>x[0]===1)+1;
assert.equal(unlinkedAt,60,'DEFAULT_3 must be unlinked on the 60th director update after the finish');
for(let k=0;k<59;k++)assert(Math.abs(after[k][3]+after[k][5]-1)<1e-5,'smoothstep weights do not sum to one');
assert.equal(after[79][1],0x44);assert.equal(after[79][2],1);
// Race reset (Restart): the next camera update begins the preferred DEFAULT_3 again.
c._reset_race();const p=pointAt(path,100),r=step(p,pointAt(path,125));// kinematic reset rider
assert.equal(r.director[0],1);assert.equal(r.director[1],0x3d);assert.equal(r.director[6],0);
console.log('Finish camera:',{finishTick,unlinkedAt,headWeight:after[0][2],postWeightAfter80:after[79][2]});
