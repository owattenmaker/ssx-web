// Core attached set pieces (web/attached_setpieces.inc, engine/parent_modifier.hpp) against PS2 memory:
// The Junction (BHP1) blimp LiveComp on its looping Spline (resident, seeded from the-junction-ready) and
// its ParentModifier children (blimpad_1000/1001, blimplights_1000), stepped through race_begin to the
// setpieces-bhp1 snapshot ticks (LIVECOMP/attached-snapshots.json, tools/export_attached_setpieces.py):
// LiveComp time, clean node matrices and clean child matrices bit for bit. Skips until the core exports
// set_piece_attached_bits (tools/../local/integration/setpieces_attached_integration.py).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const fixture = 'public/test-data/BHP1/LIVECOMP/attached-snapshots.json';
if (!fs.existsSync(fixture) || !fs.existsSync('public/assets/BHP1/world_collision.json')) { console.log('Attached core: fixtures missing, skipped'); process.exit(0); }
const c = await createCore();
if (!c._set_piece_attached_bits) { console.log('Attached core: core without set_piece_attached_bits, skipped'); process.exit(0); }
const str = (text) => { const data = new TextEncoder().encode(text + '\0'), p = c._malloc(data.length); c.HEAPU8.set(data, p); return p; };
const hash = JSON.parse(fs.readFileSync('public/assets/BHP1/terrain.json', 'utf8')).source_sha256;
c._init_world_collision(str(fs.readFileSync('public/assets/BHP1/world_collision.json', 'utf8')), str(hash));
c._init_rails(str(fs.readFileSync('public/assets/BHP1/rails.json', 'utf8')), str(hash));
const bits = () => { const u = new Uint32Array(c.HEAPU8.buffer); let i = c._set_piece_attached_bits() >> 2; const lc = new Map(), children = new Map();
  const n = u[i++]; for (let k = 0; k < n; k++) { const res = u[i], active = u[i + 1], time = u[i + 2], nodes = u[i + 3]; i += 4; lc.set(res, { active, time, nodes: Array.from(u.subarray(i, i + 16 * nodes)) }); i += 16 * nodes; }
  const m = u[i++]; for (let k = 0; k < m; k++) { const res = u[i], active = u[i + 1]; i += 2; children.set(res, { active, matrix: Array.from(u.subarray(i, i + 16)) }); i += 16; }
  return { lc, children }; };
// snapshot tick T = state after T entity passes (race tick 0 = the-junction-ready)
const records = JSON.parse(fs.readFileSync(fixture, 'utf8')).records.filter((r) => r.snapshot.startsWith('full.tick')).sort((a, b) => a.tick - b.tick);
let tick = 0, times = 0, nodes = 0, children = 0, skipped = 0;
for (const r of records) {
  while (tick < r.tick) { c._race_begin(); tick++; }
  const web = bits();
  if (r.kind === 'spline-livecomp') {
    const w = web.lc.get(r.resource); assert.ok(w && w.active, `spline LiveComp ${r.resource} active`);
    assert.equal(w.time, r.words[7] >>> 0, `tick ${r.tick} ${r.resource} LiveComp time`); times++;
    if (!r.dirty && !r.modifierDirty) { assert.deepEqual(w.nodes, r.matrices.map((x) => x >>> 0), `tick ${r.tick} ${r.resource} node matrices`); nodes++; } else skipped++;
  } else if (r.kind === 'parent' && web.children.has(r.child)) {
    const w = web.children.get(r.child); assert.ok(w.active, `child ${r.child}`);
    if (!r.dirty) { assert.deepEqual(w.matrix, r.matrix.map((x) => x >>> 0), `tick ${r.tick} child ${r.child}`); children++; } else skipped++;
  }
}
console.log(`Attached core: BHP1 blimp LiveComp time ${times}x, node matrices ${nodes}x, ParentModifier children ${children}x bit-exact (${skipped} dirty records skipped)`);
