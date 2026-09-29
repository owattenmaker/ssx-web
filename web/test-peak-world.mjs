// Streamed Peak 1 world (docs/peak-mountain.md, web/peak_world.inc, web/peak-world-batches.js):
//  1. append equivalence: Snow Jam's three locations streamed one by one from the per-location packages
//     (web/public/assets/PEAK1/<LOC>/, tools/export_peak_world.py) answer the world queries exactly like the
//     race-event package that holds them together (web/public/assets/ARA1/): ground height, rails;
//  2. residency: a location that is not resident is invisible to the queries, and becomes visible again;
//  3. every Peak 1 location package loads through the slices.
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches } from './peak-world-batches.js';

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
if (!fs.existsSync(new URL('PEAK1/peak.json', root))) { console.log('Peak 1 world not exported (tools/export_peak_world.py); skipped'); process.exit(0); }
const manifest = json('PEAK1/peak.json');
const put = (core, text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const call = (core, fn, text, ...rest) => { const p = put(core, text); try { return fn(p, ...rest); } finally { core._free(p); } };

async function streamed(codes, { resident = true } = {}) {
  const core = await createCore(); let hash = 0; let first = true; const tracks = {};
  for (const code of codes) {
    const r = `PEAK1/${code}/`, terrain = json(r + 'terrain.json'), world = json(r + 'world_collision.json'), rails = json(r + 'rails.json');
    if (!hash) hash = put(core, terrain.source_sha256);
    const batches = locationBatches(terrain, world, rails);
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
  const w = json(`PEAK1/${code}/world.json`), [[x0, y0, z0], [x1, y1, z1]] = w.bounds; let s = 12345; const rnd = () => ((s = (s * 1103515245 + 12345) >>> 0) / 4294967296);
  return Array.from({ length: n }, () => [x0 + (x1 - x0) * rnd(), y1 + 5, z0 + (z1 - z0) * rnd()]);
}

// 1. Append equivalence with the Snow Jam race-event package (A_ARA1, ARA1, ARA1_B in SDB/load order).
const snow = ['A_ARA1', 'ARA1', 'ARA1_B'];
const a = await eventCore('ARA1'), { core: b } = await streamed(snow);
let compared = 0, hits = 0;
for (const code of snow) for (const [x, y, z] of samples(code, 400)) {
  const ha = a._height_at(x, y, z), hb = b._height_at(x, y, z);
  assert.equal(Object.is(ha, hb) || ha === hb, true, `height ${code} (${x},${y},${z}): event ${ha} streamed ${hb}`); compared++; hits += ha > -1e8;
  const ra = Array.from(new Float32Array(a.HEAPF32.buffer, a._rail_query(x * 100, -z * 100, (ha > -1e8 ? ha : y) * 100 + 50), 14));
  const rb = Array.from(new Float32Array(b.HEAPF32.buffer, b._rail_query(x * 100, -z * 100, (hb > -1e8 ? hb : y) * 100 + 50), 14));
  assert.deepEqual(rb.slice(1), ra.slice(1), `rail query ${code}`);
}
console.log(`append equivalence: ${compared} height + rail probes identical to the ARA1 event package (${hits} on terrain)`);

// 2. Residency gates the queries.
const probe = samples('ARA1', 200).find(([x, y, z]) => b._height_at(x, y, z) > -1e8);
b._peak_world_set_resident(manifestTrack('ARA1'), put(b, 'ARA1'), 0);
const gone = b._height_at(...probe);
b._peak_world_set_resident(manifestTrack('ARA1'), put(b, 'ARA1'), 1);
assert.equal(b._height_at(...probe), a._height_at(...probe));
assert.notEqual(gone, a._height_at(...probe), 'a non-resident location must not answer queries');
console.log('residency: ARA1 hidden and restored');
function manifestTrack(code) { return manifest.locations.find((l) => l.code === code).track; }

// 3. Every exported Peak 1 location streams in.
const all = manifest.locations.filter((l) => fs.existsSync(new URL(`PEAK1/${l.code}/terrain.json`, root))).map((l) => l.code);
const t0 = performance.now(); const { core: c } = await streamed(all);
const info = new Int32Array(c.HEAPU8.buffer, c._world_collision_info(), 11);
console.log(`whole peak: ${all.length} locations, ${info[0]} instances, core memory ${(c.HEAPU8.buffer.byteLength / 1048576).toFixed(0)} MB, ${((performance.now() - t0) / 1000).toFixed(1)} s`);
