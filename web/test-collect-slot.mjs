// The open collection slot (ctx+0x2C0) across a crossing, with the PS2's rule (pv collectSlotReset; web/stage_script_gameplay.inc
// stage_collect_slot_rule; docs/core-migration.md "Collectible slot"; local/core-migration/decomp/popup-17303.md).
// PS2: the unload (230360 -> 308FE0) never frees the slot; 309F18 at the world-state-10 enter after the crossing's Load (2355C0 at 0x2356B0)
// resets it and runs the loaded locations' global programs again, so the first one with a collection takes it. 30C4A8's not-dead branch
// (1538E8 = 0 -> 0x30C688) only counts the resource: an instance already built keeps its entity and node state.
// The old port rule (switch off) freed the slot at the unload, and a next location that had read first never took it.
// Peak 1 hub A (track 1, 5 collectibles) holds the slot; Peak 1 ABA1 (track 5, 2 collectibles) reads; A unloads; the WS10 enter.
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches } from './peak-world-batches.js';

const root = new URL('public/assets/', import.meta.url);
const json = (p) => JSON.parse(fs.readFileSync(new URL(p, root)));
if (!fs.existsSync(new URL('PEAK1/SETPIECES/stage-world.json', root))) {
  console.log('Peak 1 stage world not exported; skipped');
  process.exit(0);
}
const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
{
  const probe = await createCore();
  if (!probe._stage_collect_slot_rule) {
    console.log('collect slot: the core has no stage_collect_slot_rule (an older build); skipped');
    process.exit(0);
  }
}
const manifest = json('PEAK1/peak.json');
const hub = manifest.locations.find((l) => l.code === 'A');
const next = manifest.locations.find((l) => l.code === 'ABA1');
const names = new Map();
for (const code of [hub.code, next.code]) {
  for (const x of json(`PEAK1/${code}/world_collision.json`).instances) names.set((x.rid << 8) | x.track, x.name);
}
const collections = Object.values(json('PEAK1/SETPIECES/stage-world.json').collections);
const listOf = (code) => collections.find((l) => l.some((x) => names.get(x.resource)?.startsWith(`mdl_${code}_collecta`)));
const hubList = listOf(hub.code);
const nextList = listOf(next.code);
assert.ok(hubList && nextList, 'A and ABA1 list collectibles');

async function crossing(rule) {
  const core = await createCore();
  const put = (text) => {
    const b = Buffer.from(text + '\0');
    const p = core._malloc(b.length);
    core.HEAPU8.set(b, p);
    return p;
  };
  // Both locations' instances in the streamed-world format (location PEAK1).
  let hash = 0;
  let first = true;
  for (const code of [hub.code, next.code]) {
    const terrain = json(`PEAK1/${code}/terrain.json`);
    const world = json(`PEAK1/${code}/world_collision.json`);
    const rails = json(`PEAK1/${code}/rails.json`);
    if (!hash) hash = put(terrain.source_sha256);
    const worlds = locationBatches(terrain, world, rails).filter((x) => x.kind === 'world');
    let rest = worlds;
    if (first) {
      core._init_world_collision(put(worlds[0].text), hash);
      core._peak_world_begin();
      core._peak_world_reserve(65536, 4096);
      rest = worlds.slice(1);
      first = false;
    }
    for (const b of rest) {
      core._peak_world_append(1);
      core._init_world_collision(put(b.text), hash);
      core._peak_world_append(0);
    }
    core._peak_world_commit();
  }
  core._peak_world_manifest(put(JSON.stringify({ streaming: manifest.streaming, residency: manifest.residency })));
  const tp = core._malloc(8);
  new Int32Array(core.HEAPU8.buffer, tp, 2).set([hub.track, next.track]);
  // free ride, the stage VM of every Peak 1 stage, A and ABA1 resident
  core._mission_test_setup(4, tp, 2);
  core._peak_world_manual(1);
  core._peak_world_row(hub.id, 2);
  core._peak_world_row(next.id, 2);
  const setpieces = (name) => put(fs.readFileSync(new URL('PEAK1/SETPIECES/' + name, root), 'utf8'));
  assert.ok(core._init_stage_world(setpieces('particles.json'), setpieces('livecomp.json'), setpieces('stage-world.json')) > 0);
  // ABA1 unloads and is evicted before the run (rows 5 -> 7 -> 0): only A is in the world at the start.
  core._peak_world_row(next.id, 5);
  core._peak_world_row(next.id, 7);
  core._peak_world_row(next.id, 0);
  const instance = (r) => {
    const U = new Uint32Array(core.HEAPU8.buffer);
    const at = core._stage_world_instances() >> 2;
    for (let k = 0; k < U[at]; k++) {
      if (U[at + 1 + 4 * k] === r) return { drawn: U[at + 2 + 4 * k], node: U[at + 3 + 4 * k] };
    }
    return null;
  };
  const entityType = (r) => new Uint32Array(core.HEAPU8.buffer, core._stage_instance_flags(r), 6)[5];
  core._stage_collect_slot_rule(rule ? 1 : 0);

  // Run start (web/free-ride.js afterReset -> web/stage-collect.js collectStart): career path, ABA1's second collectible collected.
  core._set_stage_collect_state(0, 0, 0);
  core._set_stage_collect_row(hub.track, 0, 0);
  core._set_stage_collect_row(next.track, 0b10, 0);
  core._peak_collect_refresh();
  assert.equal(core._stage_collect_track(), hub.track, 'hub A holds the slot at the start');

  // ABA1 reads (rows 6 -> 2): its stage setup runs its global programs, and 30B928 opens no collection while the slot is held.
  core._peak_world_row(next.id, 6);
  core._peak_world_row(next.id, 2);
  assert.equal(core._stage_collect_track(), hub.track, 'the slot stays with A when ABA1 reads');
  const live = nextList.find((x) => x.index === 0);
  const collected = nextList.find((x) => x.index === 1);
  // 30C4A8 lists only the open collection's resources: ABA1's are left at their authored state (no stage state yet)
  assert.equal(instance(live.resource), null, `${names.get(live.resource)} untouched by the read`);
  assert.equal(instance(collected.resource), null, `${names.get(collected.resource)} untouched by the read`);
  // ABA1's live collectible as a built instance (an entity and a Hide node, as a section enter or a status program leaves one).
  core._peak_world_seed(put(JSON.stringify({ entities: [], nodes: [[live.resource, 16, 0x210021]] })));
  const built = { instance: instance(live.resource), entity: entityType(live.resource) };
  assert.equal(built.instance.node, 16, 'the seeded node');

  // A unloads (rows 5 -> 7) and is evicted.
  core._peak_world_row(hub.id, 5);
  core._peak_world_row(hub.id, 7);
  const afterUnload = core._stage_collect_track();
  core._peak_world_row(hub.id, 0);

  // The WS10 enter after the crossing's Load: 309F18 (browser_stage_ws10_enter -> peak_collect_refresh with the rule on).
  if (rule) core._peak_collect_refresh();
  return {
    afterUnload,
    afterEnter: core._stage_collect_track(),
    live: { instance: instance(live.resource), entity: entityType(live.resource) },
    built,
    collected: instance(collected.resource),
    names: [names.get(live.resource), names.get(collected.resource)],
  };
}

const off = await crossing(false);
assert.equal(off.afterUnload, -1, 'switch off: the unload frees the slot');
assert.equal(off.afterEnter, -1, 'switch off: the slot stays empty (ABA1 had read before A unloaded)');

const on = await crossing(true);
assert.equal(on.afterUnload, hub.track, 'rule: the unload does not free the slot (230360 -> 308FE0)');
assert.equal(on.afterEnter, next.track, 'rule: the WS10 enter (309F18) gives the slot to ABA1, the loaded location with a collection');
assert.deepEqual(on.live, on.built, `rule: 30C4A8's not-dead branch leaves ${on.names[0]} as it was (node, entity)`);
assert.deepEqual(on.collected, { drawn: 0, node: 6 }, `rule: ${on.names[1]} (collected) dead after the WS10 enter`);
console.log(
  `collect slot: A holds it across ABA1's read; off: freed at A's unload, empty after; rule: kept through the unload, ABA1's at the WS10 enter, ` +
  `${on.names[0]} untouched (node ${on.live.instance.node}), ${on.names[1]} dead`,
);
