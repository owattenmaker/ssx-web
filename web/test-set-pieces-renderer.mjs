// set-pieces-renderer.js wiring (no GPU): the renderer follows the core's set-piece tick counter,
// fires GO at tick 181 before the entity pass, fires contact starts from the core's contact log,
// and moves LiveComp node meshes by the livecomp-animation.js deltas; flags redraw from the hidden
// batches and UV scroll groups follow uv-scroll.js.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import * as T from 'three/webgpu';
import {createSetPieceRenderer} from './set-pieces-renderer.js';
import {LiveCompAnimation} from './livecomp-animation.js';
import {UvScroll} from './uv-scroll.js';
import {worldUvScroll} from './world-material.js';
const read = (p) => JSON.parse(fs.readFileSync('public/assets/' + p, 'utf8'));
for (const p of ['LIVECOMP/livecomp.json', 'FLAGS/flags.json', 'UVSCROLL/uv-scroll.json']) if (!fs.existsSync('public/assets/' + p)) { console.log('Set-piece renderer: assets missing, skipped'); process.exit(0); }
const live = read('LIVECOMP/livecomp.json'), flags = read('FLAGS/flags.json'), uv = read('UVSCROLL/uv-scroll.json');
// Mock core: tick counter + contact log in a small heap.
const heap = new ArrayBuffer(1 << 16), f32 = new Float32Array(heap), u8 = new Uint8Array(heap), u32 = new Uint32Array(heap);
const core = { HEAPF32: f32, HEAPU8: u8, ticks: 0, contacts: [],
  _set_piece_info() { f32[0] = 2; f32[1] = this.ticks; return 0; },
  _set_piece_contacts() { u32[256] = this.contacts.length / 2; this.contacts.forEach((v, i) => { u32[257 + i] = v; }); return 1024; } };
const origin = new T.Vector3(-1300, 0, 2300);
const group = new T.Group();
const door = live.instances.find((x) => x.name === 'mdl_ARA1_startgatedoorbig_1007');
const rock = live.instances.find((x) => x.name === 'mdl_ARA1_rock_roll_fall_1000');
const trigger = rock.starts.find((s) => s.trigger === 'contact').ownerResource;
const liveMesh = (inst, node) => { const m = new T.Mesh(new T.BufferGeometry(), new T.MeshBasicMaterial()); m.position.copy(origin).negate(); m.userData.liveComp = [inst.resource, node]; group.add(m); return m; };
group.userData.liveCompMeshes = [liveMesh(door, 0), liveMesh(rock, 0)];
const flagInst = flags.instances[0];
const hiddenFlag = new T.Mesh(new T.BufferGeometry(), new T.MeshBasicMaterial()); hiddenFlag.position.copy(origin).negate();
hiddenFlag.userData.hiddenResource = flagInst.resource; hiddenFlag.userData.batch = { texture: 138 };
group.userData.hiddenMeshes = [hiddenFlag];
const renderer = await createSetPieceRenderer({ core, group, load: async (p) => read(p.replace('/assets/', '')) });
assert.equal(renderer.state.flags, 1, 'the flag is redrawn from its hidden batch');
renderer.update();
const flagMesh=group.children.find(m=>m.userData.flagResource===flagInst.resource),flagPosition=flagMesh.geometry.attributes.position;
const initialVersion=flagPosition.version;
renderer.update();assert.equal(flagPosition.version,initialVersion,'unchanged tick does not upload cloth');
let uploads=0;
for(let t=1;t<=10;t++){const v=flagPosition.version;core.ticks=t;renderer.update();uploads+=flagPosition.version-v;const after=flagPosition.version;renderer.update();assert.equal(flagPosition.version,after,'one upload at most per changed grid');}
assert.equal(uploads,5,'30 Hz cloth uploads only on its parity across 60 Hz ticks');
core.ticks=0;renderer.update();
// Reference players driven the documented way.
const ref = new LiveCompAnimation(live, { random: () => 0 }), refScroll = new UvScroll(uv);
for (const inst of ref.instances) if (inst.starts.some((s) => s.trigger === 'section')) ref.fire('section', inst.resource);
const contactTick = 400;
for (let t = 0; t < 520; t++) {
  if (t === live.go_tick) ref.fire('go');
  ref.tick(); if (t === contactTick) ref.fire('contact', trigger);
  refScroll.tick();
}
// Core: 520 race_begin ticks, the rock-slide trigger contacted in tick 400 (plus a repeat and a pickup).
core.contacts = [contactTick, trigger, contactTick + 1, trigger, contactTick + 2, 480776];
core.ticks = 186; renderer.update();
const doorMesh = group.userData.liveCompMeshes[0];
assert.equal(doorMesh.matrixAutoUpdate, false, 'the door is animated after GO');
core.ticks = 520; renderer.update();
assert.deepEqual(renderer.state.fired, [trigger], 'the slot-2 trigger fires once');
for (const [mesh, inst] of [[doorMesh, door], [group.userData.liveCompMeshes[1], rock]]) {
  const d = ref.nodeDeltas(inst.resource)[0], e = mesh.matrix.elements;
  for (let i = 0; i < 12; i++) assert.equal(e[i], d[i], `${inst.name} rotation`);
  for (let k = 0; k < 3; k++) assert.ok(Math.abs(e[12 + k] - (d[12 + k] - origin.getComponent(k))) < 1e-3, `${inst.name} translation`);
}
// UV scroll uniforms follow the group's representative instance.
const first = uv.instances[0], offset = worldUvScroll.get(0);
if (offset) assert.deepEqual([offset.value.x, offset.value.y].map(Math.fround), refScroll.offsetOf(first.resource).map(Math.fround));
// New race: counter reset restarts everything.
core.ticks = 0; core.contacts = []; renderer.update();
assert.equal(renderer.state.ticks, 0); assert.deepEqual(renderer.state.fired, []);
console.log('Set-piece renderer: GO doors, contact-fired rock slide, flags and UV scroll follow the core tick counter');
