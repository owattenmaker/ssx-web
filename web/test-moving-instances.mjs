import assert from 'node:assert/strict';
import {Group, Mesh, BufferGeometry, MeshBasicMaterial, Matrix4, Vector3} from 'three';
import {movingDeltas, updateMovingInstances} from './moving-instances.js';
const memory=new WebAssembly.Memory({initial:1}),origin=new Vector3(10,20,30),parent=new Group();
const meshes=[new Mesh(new BufferGeometry(),new MeshBasicMaterial()),new Mesh(new BufferGeometry(),new MeshBasicMaterial())];
for(const m of meshes){m.userData.movingResource=12;parent.add(m);}
let entries=[];
const core={HEAPF32:new Float32Array(memory.buffer),_moving_instances(){const f=this.HEAPF32;f[0]=entries.length;entries.forEach(([id,x],i)=>{f[1+i*17]=id;f.set(new Matrix4().makeTranslation(x,2*x,3*x).elements,2+i*17);});return 0;}};
const check=(x,copy=0)=>{for(const m of parent.children.filter(m=>(m.userData.movingCopy??0)===copy))assert.deepEqual(m.matrix.elements,new Matrix4().makeTranslation(x-10,2*x-20,3*x-30).elements);};
entries=[[12,5],[12+1048576,7],[12+2*1048576,9]];updateMovingInstances(core,meshes,origin);
assert.equal(parent.children.length,6);check(5);check(7,1);check(9,2);
const snapshot=movingDeltas(core);
memory.grow(1);core.HEAPF32=new Float32Array(memory.buffer);
entries=[[12,13],[12+1048576,17]];updateMovingInstances(core,meshes,origin);check(13);check(17,1);
assert.equal(snapshot.get(12)[12],5,'public snapshots remain independent');
assert.ok(parent.children.filter(m=>m.userData.movingCopy===2).every(m=>!m.visible),'removed copies hidden');
meshes[0].visible=false;updateMovingInstances(core,meshes,origin);
assert.equal(parent.children.filter(m=>m.userData.movingCopy===1&&m.visible).length,1,'copy follows source visibility');
entries=[];updateMovingInstances(core,meshes,origin);
for(const m of meshes){assert.equal(m.matrixAutoUpdate,true);assert.deepEqual(m.position.toArray(),[-10,-20,-30]);}
assert.ok(parent.children.filter(m=>m.userData.movingCopy).every(m=>!m.visible));
entries=[[12,21],[12+1048576,23],[12+2*1048576,25]];meshes[0].visible=true;updateMovingInstances(core,meshes,origin);
assert.equal(parent.children.length,6,'restart reuses clones');check(21);check(23,1);check(25,2);
console.log('Moving instance matrices: copies, removal, visibility, reset and WASM growth passed');
