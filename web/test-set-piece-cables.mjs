// MultiSpline cables (web/set-piece-cables.js, docs/visual-parity.md 41):
//  1. every CABLES path is a rails.json rail of its location, identical in the event package and the streamed package;
//  2. every sample point lies in its segment's box (+0x6C/+0x78, 1 cm): 0x345638's per-segment AABB cull then drops only what the
//     GPU clips anyway;
//  3. the sample points and the clip rule against the original VU1 program (local/reference/cables/cable-vu.json from
//     tools/test_cable_vu_native.py, when present): count vertices at the VU's t sequence, screen position within 2/16 px, ADC when
//     the vertex or the one before it is outside the clip volume;
//  4. CableLife: section starts / leaves, the tick-0 scan, chunk residency, the core's own MultiSpline state first.
import { readFileSync, existsSync } from 'node:fs';
import { CABLES, CableLife, segmentPoints, streamedCableRoot, cableGeometry, POINTS } from './set-piece-cables.js';

let failures = 0, checks = 0;
const fail = (m) => { if (failures++ < 12) console.error(m); };
const assets = new URL('./public/assets/', import.meta.url);
const json = (p) => JSON.parse(readFileSync(new URL(p, assets), 'utf8'));
let segments = 0, points = 0;
for (const [code, list] of Object.entries(CABLES)) {
  const event = json(`${code}/rails.json`), streamed = json(streamedCableRoot(code).replace('/assets/', '') + 'rails.json');
  for (const c of list) {
    const a = event.rails.find((r) => r.packed_id === c.rail), b = streamed.rails.find((r) => r.packed_id === c.rail);
    if (!a || !b) { fail(`${code}: rail ${c.rail} missing (event ${!!a}, streamed ${!!b})`); continue; }
    if (JSON.stringify(a.segments.map((s) => s.source.coefficients)) !== JSON.stringify(b.segments.map((s) => s.source.coefficients))) fail(`${code} ${a.name}: event and streamed paths differ`);
    for (const s of a.segments) {
      segments++;
      const lo = s.source.bounds_min, hi = s.source.bounds_max;
      for (const p of segmentPoints(s.source.coefficients)) { points++; if (p.some((v, i) => v < lo[i] - 1 || v > hi[i] + 1)) fail(`${code} ${a.name} segment ${s.index}: point ${p.map((v) => v.toFixed(1))} outside its box`); }
    }
    const g = cableGeometry(a, [0, 0, 0]); checks++;
    if (g.index.count !== a.segments.length * (POINTS - 1) * 6) fail(`${code} ${a.name}: ${g.index.count / 6} quads`);
  }
}
console.log(`paths: ${Object.values(CABLES).flat().length} cables, ${segments} segments, ${points} points in their boxes`);

// VU1 program 3 at 0x3BF0 against the model
const oracle = new URL('../local/reference/cables/cable-vu.json', import.meta.url);
if (existsSync(oracle)) {
  const f = (w) => new Float32Array(new Uint32Array([w]).buffer)[0], i32 = (w) => (w | 0);
  const cases = JSON.parse(readFileSync(oracle, 'utf8')).cases; let worst = 0, adc = 0, verts = 0;
  for (const c of cases) {
    const m = c.matrix.map(f), r = c.rows.map(f), rows = [r.slice(0, 3), r.slice(3, 6), r.slice(6, 9), r.slice(9, 12)];
    const pts = segmentPoints(rows, c.count);
    if (pts.length !== c.verts.length) { fail(`VU case: ${c.verts.length} vertices, model ${pts.length}`); continue; }
    let prevOut = true;
    pts.forEach((p, k) => {
      const clip = [0, 1, 2, 3].map((j) => m[j] * p[0] + m[4 + j] * p[1] + m[8 + j] * p[2] + m[12 + j]);
      const out = [0, 1, 2].some((j) => Math.abs(clip[j] / clip[3]) > 1), v = c.verts[k];
      if (((v[3] & 0x8000) !== 0) !== (out || prevOut)) adc++;
      if (!out) worst = Math.max(worst, Math.abs(i32(v[0]) - (clip[0] / clip[3] * 1024 + 2047.5) * 16), Math.abs(i32(v[1]) - (clip[1] / clip[3] * 1024 + 2047.5) * 16));
      prevOut = out; verts++;
    });
  }
  if (adc) fail(`VU ADC mismatches: ${adc}`);
  if (worst > 2) fail(`VU screen positions: worst ${worst.toFixed(2)}/16 px`);
  console.log(`VU1 0x3BF0: ${cases.length} cases, ${verts} vertices, worst ${worst.toFixed(2)}/16 px, ADC mismatches ${adc}`);
} else console.log('VU oracle absent (python3 tools/test_cable_vu_native.py): skipped');

// CableLife
{
  const doc = {initial: {active_after_tick0_scan: [407560]}, instances: [{resource: 407560, chunk: [8, 32], activation: true}, {resource: 711688, chunk: [8, 32], activation: true}]};
  const life = new CableLife('ARA1', doc), no = () => undefined;
  const expect = (what, got, want) => { checks++; if (got !== want) fail(`CableLife ${what}: ${got}, want ${want}`); };
  expect('tick-0 scan', life.lives(407560, no), true);
  expect('not started', life.lives(711688, no), false);
  life.section(711688, 1); expect('started', life.lives(711688, no), true);
  expect('chunk not resident', life.lives(711688, no, (c) => (c === 32 ? false : undefined)), false);
  life.section(711688, 5); expect('leave keeps it (0x35AAE0 counts +0x34 down)', life.lives(711688, no), true);
  life.section(711688, 3); expect('destroyed', life.lives(711688, no), false);
  expect('core MultiSpline first', life.lives(711688, () => true), true);
  life.reset(); expect('reset', life.lives(711688, no), false);
  const bare = new CableLife('EBA3', null); expect('no section data', bare.lives(91433, no), true);
}
if (failures) { console.error(`test-set-piece-cables: ${failures} failures`); process.exit(1); }
console.log(`test-set-piece-cables: ok (${checks} checks)`);
