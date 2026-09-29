import fs from 'node:fs';
import assert from 'node:assert/strict';
import {Quaternion} from 'three';
import createCore from './runtime/core.js';
const c=await createCore(),root='public/assets/';
const read=p=>JSON.parse(fs.readFileSync(root+p));const pointers=[];
const put=bytes=>{const p=c._malloc(bytes.length);pointers.push(p);c.HEAPU8.set(bytes,p);return p;};
const str=x=>put(new TextEncoder().encode(JSON.stringify(x)+'\0'));
const packets=fs.readFileSync(root+'ANIMATIONS/animation-packets.bin');
c._init_animation(str(read('ANIMATIONS/animation-packets.json')),str(read('RIDER_SAM/rider.json')),str(read('ANIMATIONS/initial.json')),put(packets),packets.length);
const triangles=fs.readFileSync(root+'ARA1/collision.bin');c._init_world(put(triangles),triangles.length/4);
for(const p of pointers)c._free(p);
const start=read('ARA1/start.json');
for(const airborne of [false,true])for(const style of [0,1,2,3,4]){
 c._reset_rider(start.position[0],start.position[1]+(airborne?100:0),start.position[2],start.heading);c._reset_animation();
 const before=Array.from(new Float32Array(c.HEAPF32.buffer,c._rider_orientation(),4));
 c._restore_rider_stance(style);
 const result=Array.from(new Float32Array(c.HEAPF32.buffer,c._stance_restore_info(),6));
 const after=Array.from(new Float32Array(c.HEAPF32.buffer,c._rider_orientation(),4));
 if(style===0){assert.equal(result[0],0);assert.equal(result[5],0);assert.deepEqual(after,before);continue;}
 assert.equal(result[0],1);
 const sideways=style===3||style===4;
 assert.equal(result[5],airborne?(sideways?34:4):(sideways?1234:4),'stance callbacks ran in a different order');
 assert.equal(result[4],airborne?(style===3?282:style===4?277:268):5);
 if(!airborne&&sideways){const a=new Quaternion(...before).normalize(),b=new Quaternion(...after).normalize();assert(Math.abs(a.angleTo(b)-Math.PI/2)<.00001,'sideways restoration did not rotate physical orientation by a quarter turn');}
 else assert.deepEqual(after,before,'this restoration must preserve physical orientation');
}
console.log('Browser stance restoration: styles 0..4 in ground/air modes preserve original callback order, physical rotations and animation requests.');
