// Per-location set pieces (web/set_piece_gameplay.inc): The Junction (BHP1) traffic MultiSplines against
// PS2 snapshots of local/ps2-capture/runs/setpieces-bhp1 (tools: local/event-activation/BHP1/
// multispline-snapshots.json), and Metro-City (BRA2) bins staying inactive until their section loads.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const fixture = '../local/event-activation/BHP1/multispline-snapshots.json';
if (!fs.existsSync(fixture) || !fs.existsSync('public/assets/BHP1/world_collision.json')) { console.log('Set-piece locations: fixtures missing, skipped'); process.exit(0); }
const load = async (loc) => {
  const c = await createCore();
  const str = (text) => { const data = new TextEncoder().encode(text + '\0'), p = c._malloc(data.length); c.HEAPU8.set(data, p); return p; };
  const hash = JSON.parse(fs.readFileSync(`public/assets/${loc}/terrain.json`, 'utf8')).source_sha256;
  c._init_world_collision(str(fs.readFileSync(`public/assets/${loc}/world_collision.json`, 'utf8')), str(hash));
  c._init_rails(str(fs.readFileSync(`public/assets/${loc}/rails.json`, 'utf8')), str(hash));
  return c;
};
const multi = (c) => { const p = c._set_piece_multi_bits(), u = new Uint32Array(c.HEAPU8.buffer); let i = p >> 2; const n = u[i++], out = new Map();
  for (let k = 0; k < n; k++) { const resource = u[i], active = u[i + 1], cars = u[i + 2], distance = u[i + 3]; i += 4; const matrices = []; for (let j = 0; j < cars; j++) { matrices.push(Array.from(u.subarray(i, i + 16))); i += 16; } out.set(resource, { active, distance, matrices }); }
  return out; };
// BHP1: race tick 0 = the-junction-ready; snapshot tick T = state after T entity passes (savestates are taken before tick T+1 runs).
{
  const c = await load('BHP1'), snaps = JSON.parse(fs.readFileSync(fixture, 'utf8')).snapshots;
  assert.equal(multi(c).size, 8, 'eight Junction traffic modifiers');
  let tick = 0, compared = 0;
  for (const snap of snaps) {
    while (tick < snap.tick) { c._race_begin(); tick++; }
    const web = multi(c);
    for (const m of snap.modifiers) { const w = web.get(m.resource); assert.ok(w && w.active, `modifier ${m.resource}`);
      assert.equal(w.distance, m.distance >>> 0, `tick ${snap.tick} ${m.resource} distance`);
      m.matrices.forEach((mat, k) => assert.deepEqual(w.matrices[k], mat.map((x) => x >>> 0), `tick ${snap.tick} ${m.resource} car ${k}`)); compared++; }
  }
  const p = c._moving_instances(); assert.ok(c.HEAPF32[p >> 2] >= 24, 'traffic cars (and the blimp) have draw deltas');
  console.log('Set-piece locations: BHP1 traffic bit-exact vs', compared, 'PS2 modifier snapshots;', c.HEAPF32[p >> 2], 'moving draws');
}
// BRA2: bins exist but stay inactive without course progress (section streaming proxy).
if (fs.existsSync('public/assets/BRA2/world_collision.json')) {
  const c = await load('BRA2');
  for (let t = 0; t < 120; t++) c._race_begin();
  const web = multi(c); assert.equal(web.size, 3); for (const w of web.values()) assert.equal(w.active, 0);
  console.log('Set-piece locations: BRA2 bins wait for their section');
}
