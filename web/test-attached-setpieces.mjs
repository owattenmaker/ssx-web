// Attached set pieces (web/attached-setpieces.js, tools/export_attached_setpieces.py) against PS2 memory
// (LIVECOMP/attached-snapshots.json, the location's kept race snapshots + its race-tick-0 state):
// 1. ParentModifier children on LiveComp parents (searchlightglowa <- searchlightbasea node 3): the
//    parent's node matrices recomputed from its LiveComp words (web/livecomp-animation.js) and the child
//    matrix (0x357108) from them, bit for bit, for every clean child/parent pair;
// 2. spline LiveComps (raven, blimp): node matrices composed on the snapshot's Spline matrix (0x356078
//    root) from the LiveComp words, bit for bit; children of spline LiveComps (blimpad/blimplights) equal
//    the parent's node matrix + offset.
// usage: node test-attached-setpieces.mjs [LOCATION] (default ARA1)
import { readFileSync, existsSync } from 'node:fs';
import { LiveCompAnimation, liveCompConstruct, liveCompMatrices } from './livecomp-animation.js';
import { parentMatrix, rowsFromBits, matrixBits } from './attached-setpieces.js';
import { fromBits, bitsOf } from './ee-scalar-float.js';

const LOC = process.argv[2] || 'ARA1';
const dir = new URL(LOC === 'ARA1' ? './public/assets/LIVECOMP/' : `./public/assets/${LOC}/LIVECOMP/`, import.meta.url);
const testDir = new URL(dir.href.replace('/public/assets/', '/public/test-data/'));   // PS2 snapshots: test data
if (!existsSync(new URL('attached.json', dir))) { console.log(`${LOC}: no attached.json (tools/export_attached_setpieces.py --location ${LOC})`); process.exit(0); }
const data = JSON.parse(readFileSync(new URL('attached.json', dir), 'utf8'));
const snaps = JSON.parse(readFileSync(new URL('attached-snapshots.json', testDir), 'utf8')).records;
const live = existsSync(new URL('livecomp.json', dir)) ? new LiveCompAnimation(JSON.parse(readFileSync(new URL('livecomp.json', dir), 'utf8')), { random: () => 0 }) : null;
let failures = 0; const fail = (m) => { if (failures++ < 12) console.error(m); };
const f = fromBits;
const stateFrom = (inst, h) => {
  const s = liveCompConstruct(inst, [0xffffffff, 1, 0, bitsOf(-1), bitsOf(-1), bitsOf(30), 0, bitsOf(-1), 0, 0, 0], () => 0);
  Object.assign(s, { mode: (h[0] << 16) >> 16, enabled: h[1] | 0, done: h[2] | 0, delay: h[3] | 0, rate: f(h[4]), low: f(h[5]), high: f(h[6]),
    time: f(h[7]), sampleTime: f(h[8]), previous: f(h[9]), unclamped: f(h[10]), dirty: true });
  return s;
};
const splineInst = new Map((data.splineLiveComps || []).map((x) => [x.resource, { ...x, matrixRows: rowsFromBits(x.matrix.map(bitsOf)), nodes: x.nodes.map((n) => ({ ...n, bindRows: rowsFromBits(n.bind.map(bitsOf)) })) }]));
const parentOf = new Map((data.parents || []).map((p) => [p.child, p]));
const stats = { parentChecked: 0, parentNodes: 0, splineChecked: 0, splineChildren: 0, skippedDirty: 0 };
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
for (const r of snaps) {
  if (r.kind === 'parent') {
    const p = parentOf.get(r.child); if (!p) { fail(`${r.snapshot}: unknown child ${r.child}`); continue; }
    if (r.node !== p.node) fail(`${r.snapshot} ${p.childName}: node ${r.node} != ${p.node}`);
    if (!same(r.offset, p.offset.map(bitsOf))) fail(`${r.snapshot} ${p.childName}: offset words differ`);
    if (!r.parentState) continue;
    if (r.dirty || r.parentState.dirty) { stats.skippedDirty++; continue; }
    let nodes;
    if (p.parentKind === 'livecomp') {
      const inst = live?.byResource.get(p.parent); if (!inst) { fail(`${p.parentName} not in livecomp.json`); continue; }
      nodes = liveCompMatrices(stateFrom(inst, r.parentState.words));
      if (!same(nodes.flatMap((m) => matrixBits(m)), r.parentState.matrices)) { fail(`${r.snapshot} ${p.parentName}: parent node matrices differ`); continue; }
      stats.parentNodes++;
    } else nodes = [0, 1, 2, 3, 4, 5, 6, 7].map((k) => r.parentState.matrices.length >= 16 * (k + 1) ? rowsFromBits(r.parentState.matrices.slice(16 * k, 16 * k + 16)) : null);
    const child = parentMatrix(nodes[p.node], p.offset);
    if (!same(matrixBits(child), r.matrix)) fail(`${r.snapshot} ${p.childName}: child matrix differs`);
    if (p.parentKind === 'spline-livecomp') stats.splineChildren++; else stats.parentChecked++;
  } else if (r.kind === 'spline-livecomp') {
    const x = splineInst.get(r.resource);
    if (r.dirty || r.modifierDirty) { stats.skippedDirty++; continue; }
    const inst = { ...x, matrixRows: rowsFromBits(r.modifier) };   // 0x356078: the Spline matrix is the root
    const s = stateFrom(inst, r.words);
    if (!same(liveCompMatrices(s).flatMap((m) => matrixBits(m)), r.matrices)) fail(`${r.snapshot} ${x.name}: node matrices on the spline root differ`);
    else stats.splineChecked++;
  }
}
console.log(`${LOC}: ${JSON.stringify(stats)} (${snaps.length} snapshot records)`);
if (failures) { console.error(`FAILED ${failures}`); process.exit(1); }
