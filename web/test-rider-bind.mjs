import assert from 'node:assert/strict';
import fs from 'node:fs';
import {Bone,Group,Skeleton,SkinnedMesh,BufferGeometry,MeshBasicMaterial,Matrix4} from 'three';
import {originalRiderBoneInverses} from './rider-bind.js';
const rig=JSON.parse(fs.readFileSync(new URL('public/assets/RIDER_ZOE/rider.json',import.meta.url)));
const scale=JSON.parse(fs.readFileSync(new URL('public/assets/ANIMATIONS/initial.json',import.meta.url))).original_animation.scale[0];
const inverse=originalRiderBoneInverses(rig,scale);assert.equal(inverse.length,27);assert.deepEqual(rig.source_bone_slots.slice(24),[26,27,28]);
const root=new Group(),bones=rig.bones.map(b=>{const out=new Bone();out.position.fromArray(b.translation).multiplyScalar(scale);out.quaternion.fromArray(b.rotation).normalize();return out;});
rig.bones.forEach((b,i)=>{if(b.parent<0)root.add(bones[i]);else bones[b.parent].add(bones[i]);});root.updateMatrixWorld(true);
const saved=inverse.map(m=>m.elements.slice()),skeleton=new Skeleton(bones,inverse),mesh=new SkinnedMesh(new BufferGeometry(),new MeshBasicMaterial());root.add(mesh);root.updateMatrixWorld(true);mesh.bind(skeleton,mesh.matrixWorld);
assert.deepEqual(skeleton.boneInverses.map(m=>m.elements),saved,'Binding must not silently recompute source inverse matrices');
let maxRestError=0;const identity=new Matrix4().elements;
for(let i=0;i<bones.length;i++){const product=new Matrix4().multiplyMatrices(bones[i].matrixWorld,inverse[i]);for(let k=0;k<16;k++)maxRestError=Math.max(maxRestError,Math.abs(product.elements[k]-identity[k]));}
assert(maxRestError<.00001,`Original rest bind mapping is wrong: ${maxRestError}`);
bones[0].rotation.z+=.35;root.updateMatrixWorld(true);skeleton.update();assert(skeleton.boneMatrices.every(Number.isFinite));assert.deepEqual(skeleton.boneInverses.map(m=>m.elements),saved,'Animating must preserve source bind matrices');
const sam=JSON.parse(fs.readFileSync(new URL('public/assets/RIDER_SAM/rider.json',import.meta.url)));const samInverse=originalRiderBoneInverses(sam,scale);assert.equal(samInverse.length,26);assert(sam.source_bind_provenance.startsWith('Compiled from authored Sam'));assert.notDeepEqual(samInverse[24].elements,inverse[24].elements,'Sam hair bind must not borrow Zoe hair');
console.log('Original Zoe bind integration:',{bones:inverse.length,maxRestError,sourceHairSlots:rig.source_bone_slots.slice(24)});
mesh.geometry.dispose();mesh.material.dispose();skeleton.dispose();
