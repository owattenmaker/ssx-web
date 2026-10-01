// pv peakRelease (docs/ctm-parity.md "The PS2's location release"): a streamed location's collision freed in the core
// (peak_world_free_track: its terrain patches in both terrain systems, its instances' collision nodes, its grind rails) and
// fed again later answers every query as before, and the core keeps no copy of it while it is out.
//   node test-peak-release.mjs [CORE=path/to/core.js]
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { locationBatches } from './peak-world-batches.js';

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const core = await createCore();
if (!core._peak_world_free_track) { console.log('peak release: skipped (core without peak_world_free_track: web/build-core.sh)'); process.exit(0); }
const root = new URL('./public/assets/PEAK1/', import.meta.url);
const pj = (p) => JSON.parse(fs.readFileSync(new URL(p, root), 'utf8'));
const manifest = pj('peak.json');
const put = (t) => { const b = Buffer.from(t + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const codes = ['A', 'A_ARA1', 'ARA1', 'ARA1_B', 'B'];
const data = Object.fromEntries(codes.map((c) => [c, { terrain: pj(`${c}/terrain.json`), world: pj(`${c}/world_collision.json`), rails: pj(`${c}/rails.json`) }]));
const track = (c) => manifest.locations.find((l) => l.code === c).track;
let hash = 0, first = true;
function feed(code) {
  const { terrain, world, rails } = data[code];
  if (!hash) hash = put(terrain.source_sha256);
  const batches = locationBatches(terrain, world, rails, 'PEAK1', { railParts: true });
  if (first) {
    first = false; const r = batches.find((b) => b.kind === 'rails');
    core._init_terrain(put(JSON.stringify(terrain))); core._init_world_collision(put(JSON.stringify(world)), hash);
    core._init_body_terrain(put(JSON.stringify(terrain))); core._init_rails(put(r.whole ?? r.text), hash);
    core._peak_world_begin(); core._peak_world_reserve(65536, 4096);
  } else {
    for (const b of batches) {
      core._peak_world_append(1);
      try { if (b.kind === 'world') core._init_world_collision(put(b.text), hash); else if (b.kind === 'terrain') { core._init_terrain(put(b.text)); core._init_body_terrain(put(b.text)); } else core._init_rails(put(b.text), hash); }
      finally { core._peak_world_append(0); }
    }
    core._peak_world_commit();
  }
}
for (const c of codes) feed(c);
const resident = (c, on) => core._peak_world_set_resident(track(c), put(c), on ? 1 : 0);
for (const c of codes) resident(c, true);
const f32 = (p, n) => Array.from(new Float32Array(core.HEAPF32.buffer, p, n));
const info = () => Array.from(new Int32Array(core.HEAP8?.buffer ?? core.HEAPU8.buffer, core._world_collision_info(), 11));
// Probes of ARA1: every rail segment's start / end (PS2 cm) and the centres of its terrain patches (browser metres, from 5 m above).
const { terrain, rails } = data.ARA1;
const railProbes = rails.rails.filter((r) => r.runtime_flags).flatMap((r) => r.segments.flatMap((s) => [s.source.start, s.source.end]));
const heightProbes = terrain.patches.filter((_, k) => k % 7 === 0).map((p) => { const lo = p.authored_bounds_min, hi = p.authored_bounds_max; return [(lo[0] + hi[0]) / 2, hi[1] + 5, (lo[2] + hi[2]) / 2]; }); // (browser axes, y up)
const probe = () => ({
  rails: railProbes.map(([x, y, z]) => f32(core._rail_query(x, y, z), 14).join(',')),
  heights: heightProbes.map(([x, y, z]) => core._height_at(x, y, z)),
});
const before = probe(), infoBefore = info();
assert.ok(before.rails.some((r) => r.startsWith('1,1,')), 'ARA1 rails answer');
assert.ok(before.heights.filter((h) => h > -1e8).length > heightProbes.length / 2, 'ARA1 terrain answers');
// Refused while collidable; freed once the location is out of the world.
assert.equal(core._peak_world_free_track(track('ARA1')), -1, 'free refused while resident');
resident('ARA1', false);
const freed = core._peak_world_free_track(track('ARA1'));
assert.ok(freed > 0, 'ARA1 collision freed');
const infoOut = info();
assert.ok(infoOut[6] < infoBefore[6], `ARA1 instance nodes freed (${infoBefore[6]} -> ${infoOut[6]})`);
assert.equal(infoOut[0], infoBefore[0], 'instance slots kept');
for (const [x, y, z] of railProbes.slice(0, 20)) assert.ok(f32(core._rail_query(x, y, z), 14)[2] === -1 || (f32(core._rail_query(x, y, z), 14)[2] & 255) !== track('ARA1'), 'no ARA1 rail while out');
// Fed again (twice, to see the slots reused): the same answers, no growth.
for (let round = 0; round < 2; round++) {
  if (round) { resident('ARA1', false); assert.ok(core._peak_world_free_track(track('ARA1')) > 0, 'freed again'); }
  feed('ARA1'); resident('ARA1', true);
  const after = probe(), infoAfter = info();
  assert.deepEqual(infoAfter, infoBefore, `round ${round}: instance counts as loaded`);
  assert.deepEqual(after.heights, before.heights, `round ${round}: terrain heights`);
  for (let k = 0; k < before.rails.length; k++) assert.equal(after.rails[k], before.rails[k], `round ${round}: rail probe ${k}`);
}
// The other locations were untouched throughout.
console.log(`peak release: ARA1 freed (${freed} items) and fed again twice, ${railProbes.length} rail and ${heightProbes.length} terrain probes as loaded`);
