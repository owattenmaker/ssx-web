// A Conquer the Mountain collectible stays collected when its streamed location unloads and reads again (web/stage_script_gameplay.inc
// stage_collectible_award; docs/pickup-recovery.md "Collected snowflakes stay removed").
// The original marks the career stats bit at the award (30B9A0 -> 30C3E0 -> 153B00), and a location's stage setup after a new read
// (30C4A8 -> 1538E8) turns the listed collectibles holding that bit into DeadNodes. The core keeps its copy of the career row
// (set_stage_collect_row, web/stage-collect.js) in step at the award, so the eviction (browser_stage_track_reset: the location's
// instances come back from the disc) and the next read's setup keep the collected snowflake dead for the rest of the session.
// Hub A (Green Base Station, track 1): mdl_A_collecta_0003 (list index 2) collected, A unloads (rows 5 -> 7 -> 0) and reads again.
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches } from './peak-world-batches.js';

const root = new URL('public/assets/', import.meta.url);
const json = (p) => JSON.parse(fs.readFileSync(new URL(p, root)));
if (!fs.existsSync(new URL('PEAK1/SETPIECES/stage-world.json', root))) { console.log('Peak 1 stage world not exported; skipped'); process.exit(0); }
const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const core = await createCore();
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e : core.getExceptionMessage?.(e) ?? e); process.exit(1); });
const put = (text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
const manifest = json('PEAK1/peak.json'), hub = manifest.locations.find((l) => l.code === 'A');
{ // hub A's instances in the streamed-world format (location PEAK1)
  const terrain = json('PEAK1/A/terrain.json'), world = json('PEAK1/A/world_collision.json'), rails = json('PEAK1/A/rails.json');
  const hash = put(terrain.source_sha256), worlds = locationBatches(terrain, world, rails).filter((x) => x.kind === 'world');
  core._init_world_collision(put(worlds[0].text), hash); core._peak_world_begin(); core._peak_world_reserve(65536, 4096);
  for (const b of worlds.slice(1)) { core._peak_world_append(1); core._init_world_collision(put(b.text), hash); core._peak_world_append(0); }
  core._peak_world_commit();
}
core._peak_world_manifest(put(JSON.stringify({ streaming: manifest.streaming, residency: manifest.residency })));
const tp = core._malloc(4); new Int32Array(core.HEAPU8.buffer, tp, 1).set([hub.track]);
core._mission_test_setup(4, tp, 1); // free ride, the stage VM of every Peak 1 stage, hub A resident
core._peak_world_manual(1); core._peak_world_row(hub.id, 2);
const setpieces = (name) => put(fs.readFileSync(new URL('PEAK1/SETPIECES/' + name, root), 'utf8'));
assert.ok(core._init_stage_world(setpieces('particles.json'), setpieces('livecomp.json'), setpieces('stage-world.json')) > 0);

const names = new Map(json('PEAK1/A/world_collision.json').instances.map((x) => [(x.rid << 8) | x.track, x.name]));
const list = json('PEAK1/SETPIECES/stage-world.json').collections, key = Object.keys(list).find((k) => list[k].some((x) => names.get(x.resource)?.startsWith('mdl_A_collecta')));
const hubList = list[key], picked = hubList.find((x) => names.get(x.resource) === 'mdl_A_collecta_0003');
assert.ok(picked, 'hub A lists mdl_A_collecta_0003');
const instance = (r) => { const U = new Uint32Array(core.HEAPU8.buffer), at = core._stage_world_instances() >> 2, n = U[at]; for (let k = 0; k < n; k++) if (U[at + 1 + 4 * k] === r) return { drawn: U[at + 2 + 4 * k], node: U[at + 3 + 4 * k] }; return null; };
const events = () => { const U = new Uint32Array(core.HEAPU8.buffer), at = core._stage_collect_events() >> 2; return Array.from(U.slice(at + 1, at + 1 + 2 * U[at])); };

// Free-ride run start (web/free-ride.js afterReset -> web/stage-collect.js collectStart): CTM path, nothing collected yet.
core._set_stage_collect_state(0, 0, 0); core._set_stage_collect_row(hub.track, 0, 0); core._peak_collect_refresh();
assert.equal(core._stage_collect_track(), hub.track, 'hub A holds the collectible slot');
for (const x of hubList) assert.equal(instance(x.resource)?.drawn, 1, `${names.get(x.resource)} drawn at the start`);

// The collect (slot-2 program: builtin39 award, collectabreak, Debounce DeadNode): removed at once, one career event.
core._mission_test_contact(picked.resource); core._mission_test_tick();
assert.deepEqual(events(), [picked.index, 500], 'one collect event: list index, the Peak 1 award');
assert.equal(instance(picked.resource).drawn, 0, 'the collected snowflake stops drawing at the collect');
for (let k = 0; k < 240; k++) core._mission_test_tick();
assert.deepEqual(instance(picked.resource), { drawn: 0, node: 6 }, 'DeadNode after the Debounce');

// Hub A unloads (230360: the stage teardown frees the slot) and is evicted (3AB498: its instances come back from the disc) ...
core._peak_world_row(hub.id, 5); core._peak_world_row(hub.id, 7);
assert.equal(core._stage_collect_track(), -1, 'the unload frees the slot');
core._peak_world_row(hub.id, 0);
assert.equal(instance(picked.resource), null, 'eviction: the instance state is gone with the location');
// ... and reads again: its stage setup takes the free slot and 30C4A8 kills the collected one again; the others stay.
core._peak_world_row(hub.id, 6); core._peak_world_row(hub.id, 2); core._peak_collect_refresh();
assert.equal(core._stage_collect_track(), hub.track);
assert.deepEqual(instance(picked.resource), { drawn: 0, node: 6 }, 'the collected snowflake stays dead after the location streams back in');
for (const x of hubList) if (x !== picked) assert.equal(instance(x.resource)?.drawn, 1, `${names.get(x.resource)} (not collected) drawn again`);
assert.deepEqual(events(), [], 'no award from the re-read');
console.log(`collect re-stream: hub A ${names.get(picked.resource)} collected (index ${picked.index}) stays a DeadNode across the unload / eviction / read; the other ${hubList.length - 1} draw`);
