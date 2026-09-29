import fs from 'node:fs';
import assert from 'node:assert/strict';
import {BufferGeometry,Vector3} from 'three';
import {createOriginalRiderSkinning} from './rider-skinning.js';
const read=f=>fs.readFileSync(new URL('public/assets/'+f,import.meta.url));
for(const packageName of ['RIDER_ZOE','RIDER_SAM']){
const rig=JSON.parse(read(packageName+'/rider.json')),bytes=read(packageName+'/vertices.bin');
const vertices=new Float32Array(bytes.buffer,bytes.byteOffset,bytes.byteLength/4),before=vertices.slice();
const skin=createOriginalRiderSkinning(rig,vertices,new Vector3(100,-200,300)),geometry=new BufferGeometry();skin.attach(geometry);
assert.deepEqual(vertices,before,'Source attribute creation mutated unscaled vertices');
assert.equal(geometry.getAttribute('sourcePosition').count,rig.source_skin.length);
const groups=new Map();
for(let i=0;i<rig.source_skin.length;i++){
 const key=JSON.stringify(rig.source_skin[i]);if(!groups.has(key))groups.set(key,groups.size);
 assert.equal(geometry.getAttribute('sourceSkinGroup').getX(i),groups.get(key));
 const normal=geometry.getAttribute('sourceNormal'),quantize=x=>rig.source_normal_quantization===32768?Math.max(-32768,Math.min(32767,Math.round(x*32768)))/32768:x;assert.equal(normal.getX(i),quantize(vertices[i*10+3]));assert.equal(normal.getY(i),quantize(-vertices[i*10+5]));assert.equal(normal.getZ(i),quantize(vertices[i*10+4]));
 const p=geometry.getAttribute('sourcePosition');assert.equal(p.getX(i),Math.fround(vertices[i*10]*100));assert.equal(p.getY(i),Math.fround(-vertices[i*10+2]*100));assert.equal(p.getZ(i),Math.fround(vertices[i*10+1]*100));
}
assert.equal(skin.display(true,.5),false,'Uninitialized GPU skinning must not replace menu pose');
assert.equal(groups.size,packageName==='RIDER_ZOE'?161:168); // Sam: tools/sam_mesh.py derived-part weights + Mac's head weights (tools/sam_head.py) (docs: sam_character/model/README.md)
const stride=groups.size*16,memory=new Float32Array(stride+4+rig.source_skin.length),pointer=16,indexPointer=(stride+4)*4;
new Uint32Array(memory.buffer,indexPointer,rig.source_skin.length).set(geometry.getAttribute('sourceSkinGroup').array);
const core={HEAPF32:memory,HEAPU8:new Uint8Array(memory.buffer),_rider_skin_palette_count:()=>groups.size,_rider_skin_palette:()=>pointer,_rider_skin_palette_indices:()=>indexPointer};
for(let i=0;i<groups.size;i++)for(const j of [0,5,10,15])memory[4+i*16+j]=1;
skin.capture(core,true);assert.equal(skin.display(true,.5),true);assert.equal(skin.display(false,.5),false);
// Shadow snapshots survive heap growth, rotate without overwriting the preceding
// tick, and restart without interpolating back to a pose from the previous run.
const boneAt=memory.length,physicalAt=boneAt+1+7*rig.bones.length;
let heap=new Float32Array(physicalAt+12);heap.set(memory);
core.HEAPF32=heap;core.HEAPU8=new Uint8Array(heap.buffer);
core._world_pose_bones=()=>boneAt*4;core._pose_physical=()=>physicalAt*4;
const writePose=(value,n=rig.bones.length)=>{heap[boneAt]=n;for(let i=1;i<=7*n;i++)heap[boneAt+i]=value+i/8;heap.fill(value,physicalAt,physicalAt+12);};
writePose(10);skin.capture(core,true);skin.display(true,.25);
const firstShadow=skin.shadowPose(),storage=firstShadow.bones;
assert.equal(storage[1],10.125);
for(const value of [20,30,40]){
 writePose(value);skin.capture(core);skin.display(true,.25);
 const shadow=skin.shadowPose();assert.equal(shadow.bones,storage,'reuse interpolation storage');
 assert.equal(shadow.bones[1],value-10+2.5+.125);assert.equal(shadow.physical[0],value);
}
const grown=new Float32Array(heap.length*2);grown.set(heap);heap=grown;core.HEAPF32=heap;core.HEAPU8=new Uint8Array(heap.buffer);
writePose(50);skin.capture(core);skin.display(true,.5);assert.equal(skin.shadowPose().bones[1],45.125);
skin.reset();writePose(90);skin.capture(core);skin.display(true,0);assert.equal(skin.shadowPose().bones[1],90.125,'reset discards old shadow history');
writePose(100,22);skin.capture(core);skin.display(true,.5);assert.equal(skin.shadowPose().bones.length,155);assert.equal(skin.shadowPose().bones[1],100.125,'changed bone layout starts a new history');
skin.reset();assert.equal(skin.display(true,1),false,'Restart must invalidate previous GPU pose');
assert.equal(skin.capture({...core,_rider_skin_palette_count:()=>0,_rider_skin_palette:()=>0}),false,'a tick without a pose before any capture: nothing to draw');assert.equal(skin.display(true,1),false);
assert.throws(()=>skin.capture({...core,_rider_skin_palette_count:()=>groups.size+1}),/Missing live source/,'a palette of another rider still throws');
// A tick whose core has no pose (count 0, no palette: the cached world pose cleared by the mission teleport 300948 -> 1234D0, posed
// at the next tick) draws the last palette again (the PS2 draws rider+0x2C as it stands); a reset asked for then applies next.
{const noPose={...core,_rider_skin_palette_count:()=>0,_rider_skin_palette:()=>0};
 writePose(110);skin.capture(core,true);writePose(120);skin.capture(core);skin.display(true,.5);assert.equal(skin.shadowPose().bones[1],115.125);
 writePose(130);assert.equal(skin.capture(noPose),false);assert.equal(skin.display(true,.5),true,'the last pose stays drawn');assert.equal(skin.shadowPose().bones[1],120.125,'both ends the last pose');
 assert.equal(skin.capture(noPose,true),false);writePose(140);assert.equal(skin.capture(core),true);skin.display(true,0);assert.equal(skin.shadowPose().bones[1],140.125,'the pending reset: no blend from before the teleport');
 writePose(150);skin.capture(core);skin.display(true,.5);assert.equal(skin.shadowPose().bones[1],145.125,'then ticks blend as before');}
assert.equal(createOriginalRiderSkinning({bones:[]},vertices,new Vector3()),null);
skin.dispose();geometry.dispose();console.log('Source GPU skin attributes and lifecycle:',{vertices:rig.source_skin.length,bones:rig.bones.length,groups:groups.size});

}
