import assert from 'node:assert/strict';import {PerspectiveCamera} from 'three';import {snowBillboardScale} from './snow-billboard.js';
let cases=0,clamped=0;
for(const fov of [35,66,100])for(const aspect of [1,640/448,2])for(const depth of [.01,.1,1,10,100])for(const radius of [.001,.1,1,10]){
 const camera=new PerspectiveCamera(fov,aspect,.01,1000),p=camera.projectionMatrix.elements,viewport=[512,448],size=snowBillboardScale(radius,depth,p[0],p[5],viewport,128);
 for(let axis=0;axis<2;axis++){const pixels=size[axis]*Math.abs(p[axis?5:0])*viewport[axis]/(2*depth),original=radius*Math.abs(p[axis?5:0])*viewport[axis]/(2*depth);assert(Math.abs(pixels-Math.min(original,128))<1e-9);if(original<=128)assert.equal(size[axis],radius);else clamped++;}cases++;
}
assert.deepEqual(snowBillboardScale(10,0,1,1,[512,448],128),[0,0]);assert.deepEqual(snowBillboardScale(10,-1,1,1,[512,448],128),[0,0]);
console.log('Snow billboard projected-size cap:',{cases,clampedAxes:clamped});
