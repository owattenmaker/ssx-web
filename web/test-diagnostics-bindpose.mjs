// web/diagnostics.js bind-pose probe (pv bindPoseProbe): the rigid-skeleton test it reports on. A skeleton at its bind pose gives a
// zero spread on the CPU (bone.matrixWorld x boneInverse) and, once three has updated it, in the bone matrices it uploads; a posed
// skeleton does not, and a posed skeleton whose buffer was not updated since its bind pose shows the stale upload (gpu 0, cpu not).
import assert from 'node:assert/strict';
import * as T from 'three/webgpu';
import { bindPoseSpreads } from './diagnostics.js';
import { PV_DEFAULTS } from './pv-flags.js';
const group = new T.Group(), bones = [];
for (let i = 0; i < 12; i++) { const b = new T.Bone(); b.position.set(0, i ? 0.2 : 1, 0); (i ? bones[i - 1] : group).add(b); bones.push(b); }
group.updateMatrixWorld(true);
const skeleton = new T.Skeleton(bones), mesh = new T.SkinnedMesh(new T.BufferGeometry(), new T.MeshBasicNodeMaterial());
group.add(mesh); mesh.bind(skeleton); group.position.set(3, 0, -2); group.updateMatrixWorld(true);
skeleton.update();
let s = bindPoseSpreads(mesh);
assert.ok(s.cpu < 1e-6 && s.gpu < 1e-6, `bind pose: cpu ${s.cpu} gpu ${s.gpu}`);
bones[3].rotation.z = 0.8; bones[7].rotation.x = -0.5; group.updateMatrixWorld(true);
s = bindPoseSpreads(mesh);
assert.ok(s.cpu > 0.1 && s.gpu < 1e-6, `posed bones, buffer not updated yet (stale): cpu ${s.cpu} gpu ${s.gpu}`);
skeleton.update(); s = bindPoseSpreads(mesh);
assert.ok(s.cpu > 0.1 && Math.abs(s.gpu - s.cpu) < 1e-5, `posed and uploaded: cpu ${s.cpu} gpu ${s.gpu}`);
assert.equal(typeof PV_DEFAULTS.bindPoseProbe, 'boolean');
console.log('diagnostics bind-pose probe: bind pose, stale upload and posed skeleton told apart');
