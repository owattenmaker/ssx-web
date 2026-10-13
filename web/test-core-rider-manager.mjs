// pv coreRiderManager (docs/core-migration.md M3): a race's computer riders on the core's rider manager (web/rider_manager.cpp) give
// the same game as web/ai-racers.js's choreography, tick for tick, on two identical cores: every tick's ?simtrace hash (every
// rider's state, pose and world state, both RNG streams, the score object), the tick record, the shared world counts, every
// computer rider's provider words and finish, the ranking world and the rider-pair counts. Snow Jam with its five computer riders
// on a pad script with jumps, grabs, tucks and resets, then a pause Give Up through the results; then the QA hooks (draw counts,
// injected world draws) on both. CORE_JS=<scratch core.js>; skipped when the core has no manager_configure.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { setPv } from './pv-flags.js';
import { createGameTick, gameTrace, traceOut, TRACE_OUT_FLOATS } from './game-tick.js';
import { createAiRacers } from './ai-racers.js';
import { ageRelationships, applyRelationshipEvent, relationshipScores, relationshipRecord } from './lineup.js';

const coreUrl = process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js';
const createCore = (await import(coreUrl)).default;
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const text = (p) => read(p).toString('utf8');
const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);

// One page-like game: the human's core (FX deferred as main.js sets it), optionally the course's computer riders, and the
// game-tick host over it (the fields web/game-tick.js reads and writes).
async function makeGame({ ai, course = 'ARA1', humanPackage = 'RIDER_ZOE', hooks = null }) {
  const human = await createCore();
  if (!human._manager_configure) return null;
  human._set_fx_deferred(1);
  const put = (bytes) => {
    const p = human._malloc(bytes.length);
    human.HEAPU8.set(bytes, p);
    return p;
  };
  const str = (s) => put(Buffer.from(s + '\0'));
  const resources = {
    packetsJson: text('ANIMATIONS/animation-packets.json'),
    packetsBin: read('ANIMATIONS/animation-packets.bin'),
    initialText: text(course === 'ARA1' ? 'ANIMATIONS/initial.json' : `${course}/initial.json`),
    terrainText: text(`${course}/terrain.json`),
    worldCollisionText: text(`${course}/world_collision.json`),
    railsText: text(`${course}/rails.json`),
    riderText: {}
  };
  resources.terrainHash = JSON.parse(resources.terrainText).source_sha256;
  const collision = read(`${course}/collision.bin`);
  const rig = text(`${humanPackage}/rider.json`);
  human._init_animation(str(resources.packetsJson), str(rig), str(resources.initialText), put(resources.packetsBin), resources.packetsBin.length);
  human._init_race(str(resources.initialText));
  human._animation_use_physics(1);
  human._init_world(put(collision), collision.length / 4);
  const hash = str(resources.terrainHash);
  human._init_terrain(str(resources.terrainText));
  human._init_world_collision(str(resources.worldCollisionText), hash);
  human._init_body_terrain(str(resources.terrainText));
  human._init_rails(str(resources.railsText), hash);
  let racers = null;
  if (ai) {
    const doc = JSON.parse(text(`${course}/npc-riders.json`));
    for (const r of doc.riders) resources.riderText[r.package] = text(`${r.package}/rider.json`);
    racers = await createAiRacers({ human, resources, document: doc, sharedVisual: true, ...(hooks ?? {}) });
  }
  const boneCount = JSON.parse(rig).bones.length;
  const padPtr = human._malloc(96);
  human._reset_pad_history();
  human._start_event();
  racers?.start();
  const posePtr = human._animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
  const aiRace = racers && {
    racers,
    beginTick: () => racers.beginTick(),
    endTick: () => racers.endTick(),
    hud: () => ({ place: racers.worldState ? racers.worldState[360] : 0 }),
    results: () => racers.standings(),
    standings: () => racers.standings()
  };
  const host = {
    core: human,
    padPtr,
    boneCount,
    animationReady: true,
    aiActive: !!racers,
    aiRace,
    running: true,
    clock: 0,
    score: 0,
    pending: 0,
    trickUntil: 0,
    lastResetPlacement: 0,
    state: f32(human, human._rider_state(), 16).slice(),
    animPose: f32(human, posePtr, boneCount * 7).slice(),
    animInfo: f32(human, human._animation_info(), 19).slice(),
    posePhysicalFrame: f32(human, human._pose_physical(), 12).slice(),
    finished: false,
    resultsPending: false,
    raceInfo: null
  };
  host.lastRescues = host.state[13];
  const out = new Float32Array(TRACE_OUT_FLOATS);
  const tick = createGameTick(host);
  const npcCores = racers ? racers.npcs.map((n) => n.core) : [];
  return {
    host,
    // one tick: the record (without its core) and the trace hash
    racers,
    step(pad) {
      const rec = tick.simulate(pad);
      const h = gameTrace(human, npcCores, traceOut(out, { state: host.state, posePhysicalFrame: host.posePhysicalFrame, raceInfo: host.raceInfo, cameraPose: rec?.camera ?? null }));
      const plain = {};
      for (const [k, v] of Object.entries(rec ?? {})) if (k !== 'core') plain[k] = ArrayBuffer.isView(v) ? Array.from(v) : v;
      return { hash: h, rec: plain };
    }
  };
}

// The pad of tick t: a tuck (left stick down), a charged jump every 240 ticks with a grab (R1) in the air, a turn, and a forced
// reset (Select) at 1500 and 2500.
function padAt(t) {
  const p = new Float32Array(24);
  p[23] = 1;
  const k = t % 240;
  if (k >= 150 && k < 180) p[10] = 1;
  if (k >= 185 && k < 215) p[13] = 1;
  if (t % 600 < 120) p[21] = 1;
  if (t === 1500 || t === 2500) p[0] = 1;
  return p;
}

const state = (g) => {
  const r = g.racers;
  return JSON.stringify({ tick: r.tick, pairs: r.pairCounts, events: r.worldEvents, kinds: r.worldEventKinds, world: r.worldState,
    npcs: r.npcs.map((n) => ({ command: n.command && Array.from(n.command), info: n.info && Array.from(n.info), finished: n.finished })) });
};
// hooks(log): the QA hooks of compare-ai-capture.mjs (draw counts per rider; a world-draw injection after riders 2 and -1)
async function runPair(label, ticks, giveUpAt, hooks = null) {
  const logs = [[], []];
  const make = (k) => (hooks ? hooks(logs[k]) : null);
  setPv('coreRiderManager', false);
  const a = await makeGame({ ai: true, hooks: make(0) });
  setPv('coreRiderManager', true);
  const b = await makeGame({ ai: true, hooks: make(1) });
  setPv('coreRiderManager', null);
  if (!a || !b) return false;
  let n = 0, finish = -1, results = -1;
  for (let t = 0; t < ticks && a.host.running && b.host.running; t++) {
    if (t === giveUpAt) {
      a.host.core._race_give_up();
      b.host.core._race_give_up();
    }
    const pad = padAt(t);
    const x = a.step(pad), y = b.step(pad);
    assert.equal(y.hash, x.hash, `${label} tick ${t}: trace ${y.hash} != ${x.hash}`);
    assert.deepEqual(y.rec, x.rec, `${label} tick ${t}: tick record`);
    assert.equal(state(b), state(a), `${label} tick ${t}: the manager's state`);
    assert.deepEqual(logs[1], logs[0], `${label} tick ${t}: the hooks' calls`);
    if (x.rec.finish && finish < 0) finish = t;
    if (x.rec.results && results < 0) results = t;
    n++;
  }
  console.log(`${label}: ${n} ticks identical (finish ${finish}, results ${results}, pairs ${a.racers.pairCounts.join('/')}, world events ${a.racers.worldEvents}, hook calls ${logs[0].length})`);
  return { finish, results };
}
const race = await runPair('Snow Jam, five computer riders', 6000, 4500);
if (race === false) {
  console.log('core-rider-manager: this core has no manager_configure (skipped)');
  process.exit(0);
}
assert(race.finish >= 0 && race.results > race.finish, 'the race gave up and showed its results');
await runPair('Snow Jam with the QA hooks', 1500, -1, (log) => ({
  onDraws: (slot, before, controller, motion) => log.push(`d${slot}:${before.join('.')}:${controller}:${motion}`),
  afterRider: (slot) => {
    log.push(`a${slot}`);
    return slot === 2 || slot === -1 ? 1 : 0;
  }
}));
// pv coreRelationships (M4a): the relationship records in the core against the page's rule in the tick (web/ai-race.js onReact,
// compare-ai-capture.mjs): both games on the core's rider manager; the same traces, reactions, notices and records.
async function relationshipPair(ticks) {
  const games = [];
  const logs = [[], []];
  setPv('coreRiderManager', true);
  for (let k = 0; k < 2; k++) {
    setPv('coreRelationships', k === 1);
    const g = await makeGame({ ai: true });
    const R = g.racers.document.relationships;
    const characters = R.characters,
      banks = R.target_banks,
      riders = g.racers.npcs.length + 1;
    const fresh = JSON.parse(fs.readFileSync(new URL('ARA1/lineup-sessions.json', root), 'utf8')).fresh;
    const tables = ageRelationships(fresh, characters.slice(0, riders), banks);
    const levels = () => relationshipScores(tables, characters, banks, R.rival_character);
    if (k === 0) {
      g.racers.onReact = (target, other, kind, attack) => {
        const [before, after, score] = applyRelationshipEvent(tables, characters, banks, target, other, (kind === 2 ? 1 : 0) + (attack ? 2 : 0));
        logs[0].push(`${target}:${other}:${kind}:${attack}:${other === 0 && before < after ? 1 : 0}:${score}`);
        g.racers.setRelationships(levels());
      };
    } else {
      const ok = g.racers.relationshipsInCore({ enabled: true, characters, banks, rival: R.rival_character, scores: levels(), kinds: null,
        tables: tables.map((bank) => bank.map((row) => row.map((h) => relationshipRecord(h)))) });
      assert(ok, 'the core took the relationships');
    }
    games.push({ g, tables });
  }
  setPv('coreRelationships', null);
  setPv('coreRiderManager', null);
  const [a, b] = games.map((x) => x.g);
  for (let t = 0; t < ticks; t++) {
    const pad = padAt(t);
    const x = a.step(pad), y = b.step(pad);
    for (const e of b.racers.relationshipEvents()) logs[1].push(`${e.slot}:${e.other}:${e.kind}:${e.attack}:${e.notice ? 1 : 0}:${e.score}`);
    assert.equal(y.hash, x.hash, `relationships tick ${t}: trace`);
    assert.deepEqual(logs[1], logs[0], `relationships tick ${t}: the reactions`);
  }
  const core = b.racers.relationshipState().tables;
  const js = games[0].tables.map((bank) => bank.map((row) => row.map((h) => relationshipRecord(h))));
  assert.deepEqual(core, js, 'the records after the race');
  console.log(`relationships in the core: ${ticks} ticks identical, ${logs[0].length} reactions (${logs[0].join(' ')}), the records equal`);
}
await relationshipPair(3000);
// pv coreEventStart (M4b): the computer riders' event start in the core against ai-racers.js's: both on the core's rider manager,
// started by makeGame (racers.start) and started again mid-run (a restart, and a grid start with a hold and the human's rank).
async function eventStartPair(ticks) {
  const games = [];
  setPv('coreRiderManager', true);
  for (let k = 0; k < 2; k++) {
    setPv('coreEventStart', k === 1);
    const g = await makeGame({ ai: true });
    if (k === 1 && !g.host.core._manager_event_start) return false;
    games.push(g);
  }
  const [a, b] = games;
  for (let t = 0; t < ticks; t++) {
    for (const [k, g] of games.entries()) {
      setPv('coreEventStart', k === 1);
      if (t === 700) g.racers.start();
      if (t === 1400) g.racers.start({ gridStart: true, hold: true, ranks: [2, 0, 0, 0, 0, 0] });
    }
    setPv('coreEventStart', null);
    const pad = padAt(t);
    const x = a.step(pad), y = b.step(pad);
    assert.equal(y.hash, x.hash, `event start tick ${t}: trace`);
    assert.equal(state(b), state(a), `event start tick ${t}: the manager's state`);
  }
  setPv('coreRiderManager', null);
  console.log(`event start in the core: ${ticks} ticks identical, with a restart at 700 and a held grid start at 1400`);
  return true;
}
await eventStartPair(2000);
console.log('core-rider-manager: the core\'s rider manager gives ai-racers.js\'s game tick for tick');
