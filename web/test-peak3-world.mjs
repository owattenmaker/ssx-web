// Streamed Peak 3 world (docs/peak3.md; tools/export_peak_world.py --peak 3, web/peak_world.inc, web/peak-world-batches.js):
//  1. append equivalence: every Peak 3 event residency row (Gravitude E_ERA5 / ERA5 / ERA5_C, Kick Doubt, Much-2-Much,
//     Perpendiculous, The Throne) streamed location by location from web/public/assets/PEAK3/<LOC>/ answers ground height
//     and rail queries exactly like the event package that holds the row together (web/public/assets/<EVENT>/);
//  2. residency: a non-resident location is invisible to the queries;
//  3. the whole Peak 3 world loads through the slices; its manifest carries the residency rows of courses 4, 7, 10, 13,
//     16 and 21, the streaming rows and every location's paths / painters.
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches } from './peak-world-batches.js';
import { regionRow, arrivalRow } from './free-ride.js';
import { SESSION_POINTS } from './career-ui.js';
import { PEAK3_COLLECT_TRACKS } from './stage-collect.js';

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
if (!fs.existsSync(new URL('PEAK3/peak.json', root))) { console.log('Peak 3 world not exported (tools/export_peak_world.py --peak 3); skipped'); process.exit(0); }
const manifest = json('PEAK3/peak.json');
const put = (core, text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const call = (core, fn, text, ...rest) => { const p = put(core, text); try { return fn(p, ...rest); } finally { core._free(p); } };

async function streamed(codes, { resident = true } = {}) {
  const core = await createCore(); let hash = 0; let first = true; const tracks = {};
  for (const code of codes) {
    const r = `PEAK3/${code}/`, terrain = json(r + 'terrain.json'), world = json(r + 'world_collision.json'), rails = json(r + 'rails.json');
    if (!hash) hash = put(core, terrain.source_sha256);
    const batches = locationBatches(terrain, world, rails, 'PEAK3');
    if (first) {
      core._init_terrain(put(core, JSON.stringify(terrain))); core._init_world_collision(put(core, JSON.stringify(world)), hash);
      core._init_body_terrain(put(core, JSON.stringify(terrain))); call(core, core._init_rails, JSON.stringify(rails), hash);
      core._peak_world_begin(); core._peak_world_reserve(65536, 4096); first = false;
    } else {
      for (const b of batches) {
        core._peak_world_append(1);
        if (b.kind === 'world') call(core, core._init_world_collision, b.text, hash);
        else if (b.kind === 'terrain') { call(core, core._init_terrain, b.text); call(core, core._init_body_terrain, b.text); }
        else call(core, core._init_rails, b.text, hash);
        core._peak_world_append(0);
      }
      core._peak_world_commit();
    }
    const entry = manifest.locations.find((l) => l.code === code); tracks[code] = entry.track;
    if (resident) core._peak_world_set_resident(entry.track, put(core, code), 1);
  }
  return { core, tracks };
}
async function eventCore(code) {
  const core = await createCore(); const r = `${code}/`, terrain = json(r + 'terrain.json'); const hash = put(core, terrain.source_sha256);
  core._init_terrain(put(core, JSON.stringify(terrain))); core._init_world_collision(put(core, JSON.stringify(json(r + 'world_collision.json'))), hash);
  core._init_body_terrain(put(core, JSON.stringify(terrain))); call(core, core._init_rails, read(r + 'rails.json').toString(), hash);
  return core;
}
// Deterministic sample points over a package's terrain (native metres), heights probed from above.
function samples(code, n) {
  const w = json(`PEAK3/${code}/world.json`), [[x0, y0, z0], [x1, y1, z1]] = w.bounds; let s = 12345; const rnd = () => ((s = (s * 1103515245 + 12345) >>> 0) / 4294967296);
  return Array.from({ length: n }, () => [x0 + (x1 - x0) * rnd(), y1 + 5, z0 + (z1 - z0) * rnd()]);
}

// 0. The manifest (ELF tables): the Peak 3 rows and their locations.
assert.equal(manifest.peak, 3); assert.equal(manifest.name, 'PEAK3');
const rows = Object.fromEntries(manifest.residency.map((r) => [r.course, r.locations]));
assert.deepEqual(Object.keys(rows).map(Number).sort((x, y) => x - y), [4, 7, 10, 13, 16, 21]);
assert.deepEqual(rows[21], ['E', 'E_ERA5', 'E_ESS3', 'E_EHP3', 'E_EBA3', 'EBC3_E']);
assert.deepEqual(rows[4], ['ERA5', 'E_ERA5', 'ERA5_C']);
for (const l of manifest.locations) for (const f of ['world.json', 'terrain.json', 'world_collision.json', 'rails.json', 'paths.json', 'lighting.json'])
  assert.ok(fs.existsSync(new URL(`PEAK3/${l.code}/${f}`, root)), `PEAK3/${l.code}/${f}`);
for (const l of manifest.locations) assert.ok(json(`PEAK3/${l.code}/rails.json`).rails.every((r) => r.runtime_flags_source !== 'default'), `${l.code}: rail runtime flags from a PS2 state`);

// 1. Append equivalence with each event package (its residency row, loaded in SDB track order as the streamer reads them).
let compared = 0, hits = 0;
for (const [event, course] of [['ERA5', 4], ['ESS3', 7], ['EBA3', 10], ['EHP3', 13], ['EBC3', 16]]) {
  const codes = rows[course].slice().sort((x, y) => manifest.locations.find((l) => l.code === x).track - manifest.locations.find((l) => l.code === y).track);
  const a = await eventCore(event), { core: b } = await streamed(codes);
  for (const code of codes) for (const [x, y, z] of samples(code, 150)) {
    const ha = a._height_at(x, y, z), hb = b._height_at(x, y, z);
    assert.equal(Object.is(ha, hb) || ha === hb, true, `${event} height ${code} (${x},${y},${z}): event ${ha} streamed ${hb}`); compared++; hits += ha > -1e8;
    const ra = Array.from(new Float32Array(a.HEAPF32.buffer, a._rail_query(x * 100, -z * 100, (ha > -1e8 ? ha : y) * 100 + 50), 14));
    const rb = Array.from(new Float32Array(b.HEAPF32.buffer, b._rail_query(x * 100, -z * 100, (hb > -1e8 ? hb : y) * 100 + 50), 14));
    assert.deepEqual(rb.slice(1), ra.slice(1), `${event} rail query ${code}`);
  }
  // 2. Residency gates the queries (the event's own location).
  // (a probe the neighbouring connector also covers keeps its height, so count the probes the hidden location answered)
  const track = manifest.locations.find((l) => l.code === event).track, probes = samples(event, 3000).filter(([x, y, z]) => b._height_at(x, y, z) > -1e8);
  b._peak_world_set_resident(track, put(b, event), 0); const changed = probes.filter((p) => b._height_at(...p) !== a._height_at(...p)).length;
  b._peak_world_set_resident(track, put(b, event), 1);
  assert.ok(changed > 0 || !probes.length, `${event}: hiding the location changed no query (${probes.length} probes)`); assert.ok(probes.every((p) => b._height_at(...p) === a._height_at(...p)), `${event}: restored`);
}
console.log(`append equivalence: ${compared} height + rail probes identical to the five Peak 3 event packages (${hits} on terrain); residency gates each`);

// 3. The whole Peak 3 world streams in.
const t0 = performance.now(); const { core: c } = await streamed(manifest.locations.map((l) => l.code));
const info = new Int32Array(c.HEAPU8.buffer, c._world_collision_info(), 11);
console.log(`whole Peak 3 world: ${manifest.locations.length} locations, ${info[0]} instances, core memory ${(c.HEAPU8.buffer.byteLength / 1048576).toFixed(0)} MB, ${((performance.now() - t0) / 1000).toFixed(1)} s`);

// 4. Session points (0x440770 +0x18) and arrival rows against each course's own AIP bank (as test-peak-mountain.mjs for Peak 1);
//    the collectible lists of the merged stage world = the HUD totals of table 0x43FA70.
const bankOf = (course) => json(`PEAK3/${manifest.residency.find((r) => r.course === course).code}/paths.json`).variants['0'];
const sessions = Object.entries(SESSION_POINTS).filter(([c]) => rows[c]);
assert.deepEqual(Object.fromEntries(sessions), { 4: 7, 7: 6, 10: 2, 13: 2, 16: 5, 21: 1 });
for (const [course, n] of sessions) { const bank = bankOf(Number(course));
  for (let k = 0; k < n; k++) { const row = regionRow(bank, k + 1, 2); assert.ok(row.kind === 1 && row.index === k + 1, `session ${course} item ${k}`); }
  assert.ok(regionRow(bank, ...arrivalRow(Number(course))), `arrival row ${course}`); }
const stage = json('PEAK3/SETPIECES/stage-world.json'), TOTALS = { 4: 30, 7: 30, 10: 5, 13: 8, 16: 44, 21: 5 };
for (const [track, course] of Object.entries(PEAK3_COLLECT_TRACKS)) {
  const n = Object.entries(stage.collections).filter(([k]) => Number(k.split(':').pop()) === Number(track)).reduce((a, [, v]) => a + v.length, 0);
  assert.equal(n, TOTALS[course], `collectibles of track ${track} (course ${course})`);
}
console.log('Peak 3 session points (6 courses), arrival rows and the 6 collectible lists (122) match the tables');
