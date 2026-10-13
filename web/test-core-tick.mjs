// pv coreTick (docs/core-migration.md M2): web/game-tick.js simulate with the switch off (the page's own core calls) and on (the
// core's tick_begin / tick_mid / tick_end / tick_finish / tick_results) gives the same game on two identical cores: every
// tick's ?simtrace hash (web/game-tick.js gameTrace: every rider's state, pose and world state, both RNG streams, the score
// object, the human's tick outputs) and the tick record the page presents (rescue, placement, camera, trick, finish, results).
// Snow Jam with its five computer riders through the finish, the results and the live stop, and Snow Jam alone, both on a pad
// script with jumps, grabs, tucks and forced resets (Select), then a pause Give Up (the DNF finish, the results, the live stop). CORE_JS=<scratch core.js>; skipped when the core has no tick_begin.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { setPv } from './pv-flags.js';
import { createGameTick, gameTrace, traceOut, TRACE_OUT_FLOATS } from './game-tick.js';
import { createAiRacers } from './ai-racers.js';

const coreUrl = process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js';
const createCore = (await import(coreUrl)).default;
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const text = (p) => read(p).toString('utf8');
const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);

// One page-like game: the human's core (FX deferred as main.js sets it), optionally the course's computer riders, and the
// game-tick host over it (the fields web/game-tick.js reads and writes).
async function makeGame({ ai, course = 'ARA1', humanPackage = 'RIDER_ZOE' }) {
  const human = await createCore();
  if (!human._tick_begin) return null;
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
    racers = await createAiRacers({ human, resources, document: doc, sharedVisual: true });
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

async function runPair(label, opts, maxTicks, pads = padAt, giveUpAt = -1) {
  const a = await makeGame(opts);
  if (!a) return false;
  const b = await makeGame(opts);
  let ticks = 0, finishTick = -1, resultsTick = -1, rescues = 0, placements = 0;
  for (let t = 0; t < maxTicks && a.host.running && b.host.running; t++) {
    const pad = pads(t);
    // pause Give Up (main.js gameHost giveUp -> core race_give_up between two ticks): the DNF finish, TIME'S UP results, the stop
    if (t === giveUpAt) {
      a.host.core._race_give_up();
      b.host.core._race_give_up();
    }
    setPv('coreTick', false);
    const x = a.step(pad);
    setPv('coreTick', true);
    const y = b.step(pad);
    setPv('coreTick', null);
    assert.equal(y.hash, x.hash, `${label} tick ${t}: trace ${y.hash} != ${x.hash}`);
    assert.deepEqual(y.rec, x.rec, `${label} tick ${t}: tick record`);
    if (x.rec.finish && finishTick < 0) finishTick = t;
    if (x.rec.results && resultsTick < 0) resultsTick = t;
    if (x.rec.rescue) rescues++;
    if (x.rec.placement) placements++;
    ticks++;
  }
  assert.equal(a.host.running, b.host.running, `${label}: the live stop`);
  console.log(`${label}: ${ticks} ticks identical (finish ${finishTick}, results ${resultsTick}, rescues ${rescues}, placements ${placements}, running ${a.host.running})`);
  return { finishTick, resultsTick, rescues, placements };
}

const solo = await runPair('Snow Jam alone', { ai: false }, 5000, padAt, 3000);
if (solo === false) {
  console.log('core-tick: this core has no tick_begin (skipped)');
  process.exit(0);
}
assert(solo.placements >= 2, 'the forced resets (Select) placed the rider');
assert(solo.finishTick >= 0 && solo.resultsTick > solo.finishTick, 'the solo run gave up and showed its results');
const race = await runPair('Snow Jam, five computer riders', { ai: true }, 6000, padAt, 4500);
assert(race.finishTick >= 0 && race.resultsTick > race.finishTick, 'the race finished and showed its results');
console.log('core-tick: the core tick gives the page\'s game tick for tick');
