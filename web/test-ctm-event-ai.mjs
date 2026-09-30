// Conquer the Mountain events in the streamed world, stage 4 (pv eventInWorldAi, docs/ctm-events-in-world.md) core checks:
//  1. a computer rider's context of a streamed world takes the event package's world in parts (web/load-slices.js feedContextWorld,
//     feedEventRails): its body collision and rails equal a normal event's rider context (the whole documents), and the human's
//     streamed world (camera terrain, collision, rails) is untouched;
//  2. the other contexts copy it by key (init_*_cached);
//  3. world_node_states / world_node_states_apply: the context's instances take the human's node states and flags (a DeadNode seeded
//     in the human reaches the context); a kind 9 event (web/shared_world.inc) replays one node;
//  4. a context set up again (reuse, no destroy) equals a fresh one, and the heap stays flat over the cycles.
// The event package's collision is exactly its resident locations' (web/test-ctm-event-world.mjs), so 1 is the resident set.
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches, eventTerrainParts, eventWorldParts, eventRailParts, parseKey } from './peak-world-batches.js';
import { riderContextCore } from './ai-racers.js';
import { feedContextWorld, feedEventRails } from './load-slices.js';

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const root = new URL('public/assets/', import.meta.url);
if (!fs.existsSync(new URL('PEAK1/peak.json', root))) { console.log('ctm event ai: skipped (no Peak 1 world)'); process.exit(0); }
const probe = await createCore();
if (!probe._event_world_bodies_only || !probe._world_node_states) { console.log('ctm event ai: skipped (core without event_world_bodies_only / world_node_states)'); process.exit(0); }
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
const manifest = json('PEAK1/peak.json');
const put = (core, text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const call = (core, fn, text, ...rest) => { const p = put(core, text); try { return fn(p, ...rest); } finally { core._free(p); } };
const u32 = (core, p, n) => Array.from(new Uint32Array(core.HEAPU8.buffer, p, n));
const hashes = (c) => { c._rider_parse_cache_clear(); return { world: c._world_load_hash() >>> 0, body: c._body_load_hash() >>> 0, rails: c._rail_load_hash() >>> 0 }; };
const states = (c) => { const p = c._world_instance_states(), n = u32(c, p, 1)[0]; const out = new Map(), w = u32(c, p + 4, n * 3); for (let k = 0; k < n; k++) out.set(w[3 * k], w[3 * k + 1]); return out; };
const noYield = async () => {};

// The human: Snow Jam's resident row streamed location by location (as web/peak-world.js feeds it).
const snow = ['A_ARA1', 'ARA1', 'ARA1_B'];
const human = await createCore(); let hashPtr = 0, first = true;
for (const code of snow) {
  const r = `PEAK1/${code}/`, terrain = json(r + 'terrain.json'), world = json(r + 'world_collision.json'), rails = json(r + 'rails.json');
  if (!hashPtr) hashPtr = put(human, terrain.source_sha256);
  if (first) {
    call(human, human._init_terrain, JSON.stringify(terrain)); call(human, human._init_world_collision, JSON.stringify(world), hashPtr);
    call(human, human._init_body_terrain, JSON.stringify(terrain)); call(human, human._init_rails, JSON.stringify(rails), hashPtr);
    human._peak_world_begin(); human._peak_world_reserve(65536, 4096); first = false;
  } else {
    for (const b of locationBatches(terrain, world, rails)) {
      human._peak_world_append(1);
      if (b.kind === 'world') call(human, human._init_world_collision, b.text, hashPtr);
      else if (b.kind === 'terrain') { call(human, human._init_terrain, b.text); call(human, human._init_body_terrain, b.text); }
      else call(human, human._init_rails, b.text, hashPtr);
      human._peak_world_append(0);
    }
    human._peak_world_commit();
  }
  human._peak_world_set_resident(manifest.locations.find((l) => l.code === code).track, put(human, code), 1);
}

// The event package, cut as the page cuts it (web/load-slices.js cutEventWorld's worker: web/peak-world-batches.js).
const terrainBytes = read('ARA1/terrain.json'), worldBytes = read('ARA1/world_collision.json'), railBytes = read('ARA1/rails.json');
const t = eventTerrainParts(terrainBytes.toString('utf8')), w = eventWorldParts(worldBytes.toString('utf8')), rr = eventRailParts(railBytes.toString('utf8'));
const cut = { hash: t.hash, keys: { world: parseKey(worldBytes, t.hash), body: parseKey(terrainBytes, t.hash), rails: parseKey(railBytes, t.hash) },
  terrain: { head: t.head, parts: t.parts }, world: { head: w.head, parts: w.parts }, rails: { head: rr.head, parts: rr.parts, count: rr.count } };

// A normal Snow Jam event's rider context (the whole documents in a fresh core): the reference.
const event = await createCore();
{ const h = put(event, t.hash); event._init_terrain(put(event, terrainBytes.toString('utf8'))); event._init_world_collision(put(event, worldBytes.toString('utf8')), h);
  event._init_body_terrain(put(event, terrainBytes.toString('utf8'))); event._init_rails(put(event, railBytes.toString('utf8')), h); }
const eventRider = riderContextCore(event, event._rider_context_create());
{ const h = put(eventRider, t.hash); eventRider._init_world_collision(put(eventRider, worldBytes.toString('utf8')), h); eventRider._init_body_terrain(put(eventRider, terrainBytes.toString('utf8'))); eventRider._init_rails(put(eventRider, railBytes.toString('utf8')), h); }
const reference = hashes(eventRider);

// 1. the first context, in parts
const before = hashes(human);
async function prepare(ctx) {
  const h = put(ctx, cut.hash);
  await feedContextWorld(ctx, cut, h, { yieldFn: noYield, budgetMs: 1e9 }); await feedEventRails(ctx, cut, h, { yieldFn: noYield, budgetMs: 1e9 });
  ctx._free(h);
}
const c1 = riderContextCore(human, human._rider_context_create());
await prepare(c1);
const got1 = hashes(c1);
assert.equal(got1.body, reference.body, 'context body collision (parts, streamed core) == a normal event rider context');
assert.equal(got1.rails, reference.rails, 'context rails == a normal event rider context');
assert.deepEqual(hashes(human), before, "the human's streamed world is untouched by a context's load");
console.log(`context world: the event package in ${w.parts.length} world, ${t.parts.length} terrain, ${rr.parts.length} rail parts == a normal event's rider; the human's world unchanged`);

// 2. the other contexts by key
await prepare(c1); // (the seal's caches again: hashes() cleared them)
const c2 = riderContextCore(human, human._rider_context_create());
assert.equal(call(c2, c2._init_world_collision_cached, cut.keys.world), 1); assert.equal(call(c2, c2._init_body_terrain_cached, cut.keys.body), 1); assert.equal(call(c2, c2._init_rails_cached, cut.keys.rails), 1);
assert.deepEqual(hashes(c2), hashes(c1), 'a context from the caches == the one loaded in parts');
assert.deepEqual(hashes(human), before);
console.log('context caches: the next context copies the first by key');

// 3. node states: a DeadNode seeded in the human's world reaches the context; kind 9 replays one node
const instances = json('ARA1/world_collision.json').instances, resourceOf = (name) => { const i = instances.find((x) => x.name.includes(name)); return (i.rid << 8) | i.track; };
const fence = resourceOf('mode_fenceb_start_1001'), volume = resourceOf('RaceRideState');
call(human, human._peak_world_seed, JSON.stringify({ entities: [], nodes: [[fence, 6, 0x10023], [volume, 16, 0x210021]] }));
const humanStates = states(human), list = [...states(c2).keys()];
const lp = human._malloc(list.length * 4); new Uint32Array(human.HEAPU8.buffer, lp, list.length).set(list);
const dump = u32(human, human._world_node_states(lp, list.length), 1 + 6 * list.length); human._free(lp);
const dp = c2._malloc(dump.length * 4); new Uint32Array(c2.HEAPU8.buffer, dp, dump.length).set(dump);
assert.equal(c2._world_node_states_apply(dp), list.length, 'every context instance is one of the human\'s'); c2._free(dp);
const after = states(c2); let differ = 0;
for (const r of list) { assert.equal(after.get(r), humanStates.get(r), `instance ${r}: the human's flags`); differ += states(c1).get(r) !== after.get(r); }
const nodeOf = (c, r) => { const p = c._malloc(4); new Uint32Array(c.HEAPU8.buffer, p, 1)[0] = r; const v = u32(c, c._world_node_states(p, 1), 7); c._free(p); return v; };
assert.equal(nodeOf(c2, fence)[2], 6, 'the fence is a DeadNode in the context'); assert.equal(nodeOf(c2, volume)[2], 16, 'the gate volume is hidden in the context');
// kind 9: [9, 6, resource, type, has flags, flags, has stage flags, stage flags]
const ev = [9, 6, fence, 0, 1, 0x10003, 0, 0], ep = c2._malloc(ev.length * 4); new Uint32Array(c2.HEAPU8.buffer, ep, ev.length).set(ev); c2._world_event_apply(ep); c2._free(ep);
assert.equal(nodeOf(c2, fence)[2], 0); assert.equal(states(c2).get(fence), 0x10003, 'kind 9 replays the node state and flags');
console.log(`node states: ${list.length} instances take the human's state (${differ} differed from the event load); kind 9 replays a node`);

// 4. reuse: the same block set up again equals a fresh context; the heap stays flat
const fresh = hashes(c1);
const sizes = [];
for (let cycle = 0; cycle < 4; cycle++) {
  await prepare(c1); c1._rider_parse_cache_clear();
  const again = hashes(c1); assert.deepEqual({ body: again.body, rails: again.rails }, { body: fresh.body, rails: fresh.rails }, `cycle ${cycle}: a reused context == a fresh one`);
  sizes.push(human.HEAPU8.length);
}
assert.equal(sizes[3], sizes[1], `the heap grows across reuse cycles: ${sizes.map((s) => s >> 20).join(' ')} MB`);
console.log(`reuse: 4 cycles of one context equal a fresh load, heap ${sizes[1] >> 20} MB flat`);
console.log('ctm event ai ok');
