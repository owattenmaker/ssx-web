// pv eventSlices (docs/ctm-flow.md "Start and streaming", web/load-slices.js initEventWorldSliced / initWorldStepped): an event course's
// core world loaded in parts ends in the state of the whole-document calls, bit for bit (core hashes):
//   - init_world's triangle tree built across frames (init_world_begin / init_world_step) == init_world (world_load_hash);
//   - the terrain, collision world and body terrain from their heads, parts and seal == init_terrain / init_world_collision /
//     init_body_terrain of the whole documents, parse caches included (world_load_hash, body_load_hash);
//   - the rail catalog from its head, parts and rails_seal == init_rails (rail_load_hash);
//   - a computer rider's context set up from the caches by key (init_*_cached) == one set up from the texts.
// CORE=path/to/core.js overrides the runtime (scratch builds). Courses: argv, else ARA1 BRA2 ASS1 ABC1 BHP1 (Peak 1) + CRA3 EBC3.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { eventTerrainParts, eventWorldParts, eventRailParts, parseKey } from './peak-world-batches.js';
import { riderContextCore } from './ai-racers.js';

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const probe = await createCore();
if (!probe._event_world_seal || !probe._init_world_step) { console.log('event slices: skipped (core without event_world_seal: web/build-core.sh)'); process.exit(0); }
const root = new URL('./public/assets/', import.meta.url);
const courses = process.argv.slice(2).length ? process.argv.slice(2) : ['ARA1', 'BRA2', 'ASS1', 'ABC1', 'BHP1', 'CRA3', 'EBC3'];
const put = (c, bytes) => { const p = c._malloc(bytes.length + 1); c.HEAPU8.set(bytes, p); c.HEAPU8[p + bytes.length] = 0; return p; };
const str = (c, t) => put(c, new TextEncoder().encode(t));
const hashes = (c) => ({ world: c._world_load_hash() >>> 0, body: c._body_load_hash() >>> 0, rails: c._rail_load_hash() >>> 0 });
for (const code of courses) {
  const dir = new URL(`${code}/`, root); if (!fs.existsSync(new URL('world_collision.json', dir))) continue;
  const raw = (f) => fs.readFileSync(new URL(f, dir));
  const terrainBytes = raw('terrain.json'), worldBytes = raw('world_collision.json'), railBytes = raw('rails.json'), tris = raw('collision.bin');
  const terrainText = terrainBytes.toString('utf8'), worldText = worldBytes.toString('utf8'), railText = railBytes.toString('utf8');
  const floats = new Float32Array(tris.buffer.slice(tris.byteOffset, tris.byteOffset + tris.byteLength));
  const t = eventTerrainParts(terrainText), w = eventWorldParts(worldText), r = eventRailParts(railText);
  const keys = { world: parseKey(worldBytes, t.hash), body: parseKey(terrainBytes, t.hash), rails: parseKey(railBytes, t.hash) };
  // the whole documents (main.js loadCourse, pv sharedParse: the files' own bytes)
  const a = await createCore();
  { const p = a._malloc(floats.byteLength); a.HEAPF32.set(floats, p >> 2); a._init_world(p, floats.length); a._free(p); }
  const ha = str(a, t.hash);
  a._init_terrain(put(a, terrainBytes)); a._init_world_collision(put(a, worldBytes), ha); a._init_body_terrain(put(a, terrainBytes)); a._init_rails(put(a, railBytes), ha);
  // the parts
  const b = await createCore();
  { const p = b._malloc(floats.byteLength); b.HEAPF32.set(floats, p >> 2); b._init_world_begin(p, floats.length); b._free(p); let steps = 0; while (!b._init_world_step(60000)) steps++; assert.ok(steps > 3, `${code}: the tree takes several steps`); }
  const hb = str(b, t.hash);
  b._event_world_begin(); b._init_terrain(str(b, t.head)); b._init_world_collision(str(b, w.head), hb); b._init_body_terrain(str(b, t.head));
  b._peak_world_append(1); for (const part of w.parts) b._init_world_collision(str(b, part), hb); for (const part of t.parts) b._terrain_part(str(b, part)); b._peak_world_append(0);
  b._event_world_seal(str(b, keys.world), str(b, keys.body));
  b._init_rails(str(b, r.head), hb); b._peak_world_reserve(0, r.count); b._peak_world_append(1); for (const part of r.parts) b._init_rails(str(b, part), hb); b._peak_world_append(0); b._rails_seal(str(b, keys.rails));
  // the keys are the core's own parse keys of the whole texts
  assert.equal(a.UTF8ToString?.(a._parse_key_of(put(a, worldBytes), ha)) ?? keys.world, keys.world, `${code}: world parse key`);
  const A = hashes(a), B = hashes(b);
  assert.deepEqual(B, A, `${code}: the core world in parts == the whole documents`);
  // a computer rider's context: from the texts (a) and from the caches by key (b)
  const ca = riderContextCore(a, a._rider_context_create()), cb = riderContextCore(b, b._rider_context_create());
  const hca = str(ca, t.hash);
  ca._init_world_collision(put(ca, worldBytes), hca); ca._init_body_terrain(put(ca, terrainBytes)); ca._init_rails(put(ca, railBytes), hca);
  assert.equal(cb._init_world_collision_cached(str(cb, keys.world)), 1, `${code}: world cache by key`);
  assert.equal(cb._init_body_terrain_cached(str(cb, keys.body)), 1, `${code}: body terrain cache by key`);
  assert.equal(cb._init_rails_cached(str(cb, keys.rails)), 1, `${code}: rails cache by key`);
  assert.deepEqual(hashes(cb), hashes(ca), `${code}: a computer rider's context from the caches == from the texts`);
  console.log(`event slices: ${code} OK (${w.parts.length} world, ${t.parts.length} terrain, ${r.parts.length} rail parts)`);
}

// ---- the rider's animation documents parsed ahead (core animation_prepare, pv eventSlices: main.js prepareRiderAnimation, ai-racers.js) ----
if (probe._animation_prepare) {
  const A = new URL('./public/assets/', import.meta.url), read = (p) => fs.readFileSync(new URL(p, A));
  const packetsJson = read('ANIMATIONS/animation-packets.json'), packetsBin = read('ANIMATIONS/animation-packets.bin'), initial = read('ANIMATIONS/initial.json');
  for (const riderPkg of ['RIDER_ZOE', 'RIDER_MAC', 'RIDER_PSYMON']) {
    const rider = read(`${riderPkg}/rider.json`);
    const run = async (prepare) => {
      const c = await createCore(); const bin = c._malloc(packetsBin.length); c.HEAPU8.set(packetsBin, bin);
      if (prepare) { for (const t of [initial, packetsJson, rider, initial]) c._animation_prepare(put(c, t)); }
      c._init_race(put(c, initial)); c._init_animation(put(c, packetsJson), put(c, rider), put(c, initial), bin, packetsBin.length); c._animation_use_physics(0);
      assert.equal(c._animation_prepared(), 0, `${riderPkg}: every prepared document taken`);
      const out = [];
      for (let t = 0; t < 90; t++) { const p = c._animation_tick(0.5, t % 30 < 10 ? 1 : 0, 0, 0, 1, 0, 0, 0, 0, 0); out.push(...new Uint32Array(c.HEAPU8.buffer, p, 28 * 7)); out.push(...new Uint32Array(c.HEAPU8.buffer, c._animation_info(), 19)); }
      out.push(...new Uint32Array(c.HEAPU8.buffer, c._animation_rng_words(), 6));
      let h = 0x811c9dc5; for (const w of out) h = Math.imul(h ^ w, 16777619) >>> 0; return h;
    };
    assert.equal(await run(true), await run(false), `${riderPkg}: init_animation / init_race from prepared parses == from the texts`);
  }
  console.log('event slices: animation documents prepared ahead OK (Zoe, Mac, Psymon)');
}
