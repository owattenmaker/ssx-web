import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import {raceTime,hudRaceTime} from './race-time.mjs';
const c=await createCore(),root='public/assets/';
process.on('uncaughtException',e=>{console.error(e instanceof Error?e:c.getExceptionMessage(e));process.exit(1);});
const read=p=>JSON.parse(fs.readFileSync(root+p));const put=b=>{const p=c._malloc(b.length);c.HEAPU8.set(b,p);return p;};const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const mesh=fs.readFileSync(root+'ARA1/collision.bin'),mp=put(mesh);c._init_world(mp,mesh.length/4);c._free(mp);const tp=str(read('ARA1/terrain.json'));c._init_terrain(tp);c._free(tp);
const settings=read('ANIMATIONS/initial.json'),event=settings.original_race_event.original_race_event;
const participant=event.participants.find(p=>p.human);participant.penalty_ticks=120;
const cfg=str(settings);c._init_race(cfg);c._free(cfg);c._reset_race();
const f=(p,n)=>new Float32Array(c.HEAPF32.buffer,p,n).slice();
const pointAt=(path,d)=>{const point=path.origin.slice();for(const segment of path.segments){const length=Math.min(d,segment[3]);for(let k=0;k<3;k++)point[k]+=segment[k]*length;if(d<segment[3])return point;d-=segment[3];}return point;};
let finish=null,finishPulses=0,resultPulses=0,ticks=0,lastPoint;
// Kinematic traversal of real authored paths/events, not a physics/AI replay.
outer:for(const index of [3,4,5,6,7]){
 const path=event.paths.find(p=>p.index===index);let length=path.segments.reduce((n,s)=>n+s[3],0);
 if(index===7)length=path.events.find(e=>e.type===1).start+500;
 for(let d=index===3?path.remaining_at_origin-participant.best_remaining:0;d<length;d+=50){
  const p=pointAt(path,d),q=pointAt(path,d+50);lastPoint=p;
  c._reset_rider(p[0]/100,p[2]/100,-p[1]/100,0);c._set_rider_velocity(...q.map((x,k)=>(x-p[k])*60));
  c._race_begin();const clock=f(c._race_end(),8),result=f(c._race_result_info(),6);ticks++;
  if(clock[2]){finishPulses++;assert.equal(result[0],1);assert.equal(result[1],result[4]+120,'finish omitted penalty ticks');finish=result;}
  if(clock[3]){resultPulses++;assert(finish);assert.equal(result[1],finish[1]);break outer;}
 }
}
assert(finish&&finishPulses===1&&resultPulses===1,'authored finish did not produce exactly one results transition');
const stopped=f(c._race_result_info(),6)[4];
for(let i=0;i<120;i++){c._race_begin();const clock=f(c._race_end(),8),result=f(c._race_result_info(),6);assert.equal(result[1],finish[1],'finish time changed after finish');assert.equal(result[4],stopped,'EndRace clock keeps running');assert.equal(clock[2],0);assert.equal(clock[3],0);}
c._reset_race();assert.equal(f(c._race_result_info(),6)[0],0,'new race retained finish record');
// Pause Give Up (0x20DA58 -> 1253D0): +0x480 = 1 on the unfinished human, then the next race tick's 125228 (branch 125368)
// runs the finish routine 125108 (penalty included) as a DNF; EndRace/results follow on the tick after, as for a finish.
{c._reset_race();const start=event.paths.find(p=>p.index===3),p=pointAt(start,start.remaining_at_origin-participant.best_remaining),q=pointAt(start,start.remaining_at_origin-participant.best_remaining+50);
 const tick=()=>{c._reset_rider(p[0]/100,p[2]/100,-p[1]/100,0);c._set_rider_velocity(...q.map((x,k)=>(x-p[k])*60));c._race_begin();return [f(c._race_end(),8),f(c._race_result_info(),6)];};
 for(let i=0;i<3;i++){const [clock,result]=tick();assert.equal(clock[2],0);assert.equal(result[0],0);assert.equal(result[5],5,'give-up test needs the Race phase');}
 assert.equal(c._race_timed_out(),0);c._race_give_up();assert.equal(c._race_timed_out(),1,'Give Up did not set +0x480');
 let [clock,result]=tick();assert.equal(clock[2],1,'Give Up did not finish on the next tick');assert.equal(result[1],result[4]+120,'Give Up finish omitted penalty ticks');
 [clock,result]=tick();assert.equal(clock[3],1,'Give Up did not request the results');assert.equal(c._race_timed_out(),1);
 const done=result[1];c._race_give_up();[clock,result]=tick();assert.equal(clock[2],0);assert.equal(result[1],done,'Give Up after the finish changed it');
 c._reset_race();assert.equal(c._race_timed_out(),0,'new race kept the Give Up DNF');}
assert.equal(raceTime(42),'00:00:70');assert.equal(raceTime(60),'00:01:00');assert.equal(raceTime(60*251,false),'04:11');
// In-race HUD 0x21F81C: hours:minutes:seconds (PS2 frames show 00:00:01 and 00:00:07 after GO).
assert.equal(hudRaceTime(59),'00:00:00');assert.equal(hudRaceTime(435),'00:00:07');assert.equal(hudRaceTime(60*3661),'01:01:01');
console.log('Authored race finish/results:',{ticks,finishTicks:finish[1],penaltyTicks:finish[2],time:raceTime(finish[1]),finishPulses,resultPulses});
