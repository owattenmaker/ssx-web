import fs from 'node:fs';import assert from 'node:assert/strict';import {PerspectiveCamera,Vector3} from 'three';import {createSnowRenderer} from './snow-renderer.js';
const originalFetch=globalThis.fetch;globalThis.fetch=async url=>{const bytes=fs.readFileSync(new URL('public'+url,import.meta.url));return {json:async()=>JSON.parse(bytes),arrayBuffer:async()=>bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength)};};
const memory=new Float32Array(4096),info=memory.subarray(4,27);for(let i=0;i<10;i++)info[10+i]=8;info[0]=2;info[20]=1;
const asset=JSON.parse(fs.readFileSync(new URL('public/assets/SNOW_FX/snow-fx.json',import.meta.url)));for(const p of asset.profiles)memory[800+p.emitter_index]=p.parameters.TextureId;
const core={_snow_flipbook_info:()=>3200,HEAPF32:memory,_snow_info:()=>16,_snow_particles:i=>(128+i*64)*4};memory.set([0,1000,0,10,1,.5,.25,.5,100,1500,0,20,.5,1,.25,1],128);
let snow;try{snow=await createSnowRenderer(new Vector3(),core);}finally{globalThis.fetch=originalFetch;}
const camera=new PerspectiveCamera(60,640/448,.1,1000);camera.position.z=5;snow.update(core,camera);
const mesh=snow.group.children[0],colours=mesh.geometry.attributes.snowColour;
assert.equal(mesh.count,2);assert.deepEqual(mesh.instanceMatrix.updateRanges,[{start:0,count:32}]);assert.deepEqual(colours.updateRanges,[{start:0,count:8}]);
const emitters=snow.group.children.filter(o=>o.isInstancedMesh);assert.equal(emitters.length,10);
for(const inactive of emitters.slice(1))assert.equal(inactive.instanceMatrix.version,0,'Empty emitter requested an upload');
const matrixVersion=mesh.instanceMatrix.version,colorVersion=colours.version;snow.update(core,camera);assert.equal(mesh.instanceMatrix.version,matrixVersion);assert.equal(colours.version,colorVersion);
camera.position.z=6;snow.update(core,camera);assert(mesh.instanceMatrix.version>matrixVersion);assert.equal(colours.version,colorVersion,'Camera movement reuploaded unchanged colors');
info[0]=1;info[20]++;snow.update(core,camera);assert.deepEqual(mesh.instanceMatrix.updateRanges,[{start:0,count:16}]);assert.deepEqual(colours.updateRanges,[{start:0,count:4}]);
info[0]=0;info[20]++;const before=mesh.instanceMatrix.version;snow.update(core,camera);assert.equal(mesh.count,0);assert.equal(mesh.instanceMatrix.version,before);
info[0]=3;info[20]++;memory.set([200,1500,0,30,.25,.5,1,.75],144);snow.update(core,camera);assert.equal(mesh.count,3);assert.deepEqual(mesh.instanceMatrix.updateRanges,[{start:0,count:48}]);assert.deepEqual(colours.updateRanges,[{start:0,count:12}]);assert.deepEqual(Array.from(colours.array.slice(8,12)),[.25,.5,1,.75]);
info[1]=1;info[20]++;memory[801]=15;memory.set([0,1000,0,10,1,1,1,1],192);snow.update(core,camera);assert.equal(snow.group.children[1].userData.snowTextureId,15,'Animated emitter failed to change texture');
console.log('Snow renderer uploads only active prefixes; empty, unchanged, camera-only, shrink and regrow cases pass.');
for(const m of emitters){m.geometry.dispose();m.material.dispose();m.dispose();}

// pv snowBuckets: the PS2 flush order (0x364240 key, 0x364050 radix sort): descending texture handle = ascending FX texture id.
{
  const { snowDrawOrder } = await import('./snow-renderer.js');
  const ids = [5, 6, 13, 16, 19, 21, 25], orders = ids.map(snowDrawOrder);
  if (!orders.every((o, k) => k === 0 || o > orders[k - 1])) throw Error('snowDrawOrder: not ascending by texture id');
  if (!(orders[0] > 690 && orders.at(-1) < 710)) throw Error('snowDrawOrder: outside the snow band (after startfire 690, before fist sparkle 710)');
  // renderer +0xF50 FX handle table (PS2 RAM, setpieces-abc1 tick 2000), entries 0..25
  const FX = [1525, -1, -1, -1, 1524, 1523, 1522, 1521, 1520, -1, 1519, 1518, -1, 1517, 1516, 1515, 1514, 1513, 1512, 1511, 1510, 1509, 1508, 1507, 1506, 1505];
  const handle = (id) => FX[id], key = (id) => ~((31 - 7) << 26 | (handle(id) & 0x3ff) << 3) >>> 0; // same modes: key order = handle order
  const byKey = [...ids].sort((a, b) => key(a) - key(b));
  if (JSON.stringify(byKey) !== JSON.stringify(ids)) throw Error('snowDrawOrder: differs from the 0x364240 key order');
  console.log('snow draw order: the PS2 key order for', ids.join(', '));
}
