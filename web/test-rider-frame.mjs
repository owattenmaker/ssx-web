import assert from 'node:assert/strict';
import {Vector3,Object3D,Bone} from 'three';
import {captureRiderFrame,applyRiderFrame} from './rider-frame.js';
const state=new Float32Array([0,0,0,Math.PI,0,1,0,0,1]);
const pose=new Float32Array([0,0,100,0,0,0,1,0,0,200,0,0,0,1]);
const parents=[-1,0],origin=new Vector3();
const first=captureRiderFrame(state,pose,null,parents,origin);
state[0]=10;state[3]=Math.PI/2;
const second=captureRiderFrame(state,pose,null,parents,origin);
const saved=JSON.stringify([first,second]);
const rider=new Object3D(),bones=[new Bone(),new Bone()];
for(const alpha of [0,.25,.5,.75,1]){
 applyRiderFrame(rider,bones,first,second,alpha);
 assert(Math.abs(rider.position.x-10*alpha)<1e-9);
 assert(Math.abs(bones[1].position.length()-1)<1e-9,'interpolation changes bone length');
 const expected=rider.quaternion.clone();
 for(let i=0;i<20;i++)applyRiderFrame(rider,bones,first,second,alpha);
 assert(expected.angleTo(rider.quaternion)<1e-7,'render calls introduce extra smoothing');
}
applyRiderFrame(rider,bones,null,second,0);
assert.equal(rider.position.x,10,'reset blends against stale pose history');
assert.equal(JSON.stringify([first,second]),saved,'rendering mutates simulation snapshots');
// A flip about a one-meter authored pivot translates the root by two meters.
state[0]=0;state[3]=Math.PI;state[8]=0;
const info=new Float32Array(19);info[6]=1;info[18]=200;
const flip=captureRiderFrame(state,pose,info,parents,origin);
applyRiderFrame(rider,bones,null,flip,1);
const hip=bones[0].position.clone().applyQuaternion(rider.quaternion).add(rider.position);
assert(hip.distanceTo(new Vector3(0,1,0))<1e-6,'renderer drops the authored air pivot offset');
console.log('Rider rendering: shared tick interpolation, rigid local bones, pivot offset, reset snap, and no render-rate-dependent smoothing verified.');

state[8]=1;state[3]=0;
const physical=captureRiderFrame(state,pose,null,parents,origin,new Float32Array([0,0,0,1]));
assert(Math.abs(physical.rotation.w-1)<1e-9,'renderer rebuilt physical orientation from heading');
console.log('Rider rendering honors the physics-owned source quaternion.');

const cached=captureRiderFrame(state,pose,null,parents,origin,new Float32Array([0,0,0,1]),new Float32Array([100,200,300]));
assert.deepEqual(cached.position.toArray(),[1,3,-2],'rendering ignores the cached pre-response pose position');
console.log('Rider rendering preserves the posed physical frame independently of current physics.');
