// Streamed Peak 2 world (docs/peak2.md section 6; web/test-peak-world.mjs is the Peak 1 original of this test):
//  0. the manifest: 17 locations (C, D, the six courses, their connectors, DRA4_A, ERA5_C), residency rows 2, 3, 6, 9, 12,
//     15, 19, 20 with their skies, the stations' rows;
//  1. append equivalence: Ruthless Ridge's three locations streamed one by one from the per-location packages
//     (web/public/assets/PEAK2/<LOC>/) answer the world queries exactly like its race-event package (CRA3/);
//  2. residency: a location that is not resident is invisible to the queries, and becomes visible again;
//  3. every Peak 2 location package loads through the slices.
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches } from './peak-world-batches.js';

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
if (!fs.existsSync(new URL('PEAK2/peak.json', root))) { console.log('Peak 2 world not exported (tools/export_peak_world.py --peak 2); skipped'); process.exit(0); }
const manifest = json('PEAK2/peak.json');
{ const codes = manifest.locations.map((l) => l.code).sort();
  for (const c of ['C', 'D', 'CRA3', 'DRA4', 'DSS2', 'CBA2', 'CHP2', 'DBC2', 'C_CRA3', 'CRA3_D', 'D_DRA4', 'DRA4_A', 'D_DSS2', 'C_CBA2', 'C_CHP2', 'DBC2_D', 'ERA5_C']) assert(codes.includes(c), `PEAK2 manifest lacks ${c}`);
  const rows = new Map(manifest.residency.map((r) => [r.course, r]));
  assert.deepEqual([...rows.keys()].sort((x, y) => x - y), [2, 3, 6, 9, 12, 15, 19, 20]);
  assert.deepEqual(rows.get(19).locations, ['C', 'C_CRA3', 'C_CHP2', 'C_CBA2', 'ERA5_C']); assert.equal(rows.get(19).sky, 'CSKY');
  assert.deepEqual(rows.get(20).locations, ['D', 'D_DRA4', 'D_DSS2', 'CRA3_D', 'DBC2_D']); assert.equal(rows.get(20).sky, 'DSKY');
  assert.deepEqual(rows.get(3).locations, ['DRA4', 'D_DRA4', 'DRA4_A']);
  console.log(`manifest: ${codes.length} entries, residency rows ${[...rows.keys()].join(' ')}`); }
const put = (core, text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const call = (core, fn, text, ...rest) => { const p = put(core, text); try { return fn(p, ...rest); } catch (e) { throw new Error(core.getExceptionMessage ? core.getExceptionMessage(e).join(': ') : String(e)); } finally { core._free(p); } };

async function streamed(codes, { resident = true } = {}) {
  const core = await createCore(); let hash = 0; let first = true; const tracks = {};
  for (const code of codes) {
    const r = `PEAK2/${code}/`, terrain = json(r + 'terrain.json'), world = json(r + 'world_collision.json'), rails = json(r + 'rails.json');
    if (!hash) hash = put(core, terrain.source_sha256);
    const batches = locationBatches(terrain, world, rails);
    if (first) {
      call(core, core._init_terrain, JSON.stringify(terrain)); call(core, core._init_world_collision, JSON.stringify(world), hash);
      call(core, core._init_body_terrain, JSON.stringify(terrain)); call(core, core._init_rails, JSON.stringify(rails), hash);
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
  call(core, core._init_terrain, JSON.stringify(terrain)); call(core, core._init_world_collision, JSON.stringify(json(r + 'world_collision.json')), hash);
  call(core, core._init_body_terrain, JSON.stringify(terrain)); call(core, core._init_rails, read(r + 'rails.json').toString(), hash);
  return core;
}
// Deterministic sample points over a package's terrain (native metres), heights probed from above.
function samples(code, n) {
  const w = json(`PEAK2/${code}/world.json`), [[x0, y0, z0], [x1, y1, z1]] = w.bounds; let s = 12345; const rnd = () => ((s = (s * 1103515245 + 12345) >>> 0) / 4294967296);
  return Array.from({ length: n }, () => [x0 + (x1 - x0) * rnd(), y1 + 5, z0 + (z1 - z0) * rnd()]);
}

// 1. Append equivalence with the Ruthless Ridge race-event package (C_CRA3, CRA3, CRA3_D in SDB/load order).
const snow = ['C_CRA3', 'CRA3', 'CRA3_D'];
const a = await eventCore('CRA3'), { core: b } = await streamed(snow);
let compared = 0, hits = 0;
for (const code of snow) for (const [x, y, z] of samples(code, 400)) {
  const ha = a._height_at(x, y, z), hb = b._height_at(x, y, z);
  assert.equal(Object.is(ha, hb) || ha === hb, true, `height ${code} (${x},${y},${z}): event ${ha} streamed ${hb}`); compared++; hits += ha > -1e8;
  const ra = Array.from(new Float32Array(a.HEAPF32.buffer, a._rail_query(x * 100, -z * 100, (ha > -1e8 ? ha : y) * 100 + 50), 14));
  const rb = Array.from(new Float32Array(b.HEAPF32.buffer, b._rail_query(x * 100, -z * 100, (hb > -1e8 ? hb : y) * 100 + 50), 14));
  assert.deepEqual(rb.slice(1), ra.slice(1), `rail query ${code}`);
}
console.log(`append equivalence: ${compared} height + rail probes identical to the CRA3 event package (${hits} on terrain)`);

// 2. Residency gates the queries.
const probe = samples('CRA3', 200).find(([x, y, z]) => b._height_at(x, y, z) > -1e8);
b._peak_world_set_resident(manifestTrack('CRA3'), put(b, 'CRA3'), 0);
const gone = b._height_at(...probe);
b._peak_world_set_resident(manifestTrack('CRA3'), put(b, 'CRA3'), 1);
assert.equal(b._height_at(...probe), a._height_at(...probe));
assert.notEqual(gone, a._height_at(...probe), 'a non-resident location must not answer queries');
console.log('residency: CRA3 hidden and restored');
function manifestTrack(code) { return manifest.locations.find((l) => l.code === code).track; }

// 3. Every exported Peak 2 location streams in.
const all = manifest.locations.filter((l) => fs.existsSync(new URL(`PEAK2/${l.code}/terrain.json`, root))).map((l) => l.code);
const t0 = performance.now(); const { core: c } = await streamed(all);
const info = new Int32Array(c.HEAPU8.buffer, c._world_collision_info(), 11);
console.log(`whole peak: ${all.length} locations, ${info[0]} instances, core memory ${(c.HEAPU8.buffer.byteLength / 1048576).toFixed(0)} MB, ${((performance.now() - t0) / 1000).toFixed(1)} s`);
