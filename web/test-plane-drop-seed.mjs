// pv coreEventStart (docs/core-migration.md M4b): the core's plane_drop_seed gives the CTM plane drop's seeds exactly as web/main.js
// resetPhysics makes them call by call (web/plane-drop.js: the rider head first, the start velocity, the air sequences, the camera's
// words, the air streamers): two identical cores, one seeded each way, ride 600 ticks with the same ?simtrace hash on every tick.
// CORE_JS=<scratch core.js>; skipped when the core has no plane_drop_seed.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { createGameTick, gameTrace, traceOut, TRACE_OUT_FLOATS } from './game-tick.js';
import { createAiRacers } from './ai-racers.js';
import { PLANE_DROP } from './plane-drop.js';

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
  if (!human._plane_drop_seed) return null;
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

const velocity = [120.5, -310.25, 40];
// main.js resetPhysics's plane-drop calls, one by one
function seedPage(core) {
  new Uint32Array(core.HEAPU8.buffer, core._rider_orientation(), 4).set(PLANE_DROP.quaternion);
  core._set_rider_velocity(...velocity);
  const b = new TextEncoder().encode(PLANE_DROP.animation + '\0'),
    p = core._malloc(b.length);
  core.HEAPU8.set(b, p);
  core._drop_air_seed(p);
  core._free(p);
  const w = core._malloc(271 * 4),
    k = core._malloc(271);
  new Uint32Array(core.HEAPU8.buffer, w, 271).set(PLANE_DROP.camera);
  core.HEAPU8.fill(1, k, k + 271);
  core._camera_seed_words(w, k);
  core._free(w);
  core._free(k);
  const s = core._malloc(PLANE_DROP.streamer.length * 4);
  core.HEAPF32.set(PLANE_DROP.streamer, s >> 2);
  core._rider_fx_streamer_seed(s);
  core._free(s);
}
function seedCore(core) {
  const t = new TextEncoder().encode(PLANE_DROP.animation + '\0');
  const q = core._malloc(16),
    a = core._malloc(t.length),
    w = core._malloc(271 * 4),
    f = core._malloc(PLANE_DROP.streamer.length * 4);
  new Uint32Array(core.HEAPU8.buffer, q, 4).set(PLANE_DROP.quaternion);
  core.HEAPU8.set(t, a);
  new Uint32Array(core.HEAPU8.buffer, w, 271).set(PLANE_DROP.camera);
  core.HEAPF32.set(PLANE_DROP.streamer, f >> 2);
  core._plane_drop_seed(q, 1, velocity[0], velocity[1], velocity[2], a, w, f);
  for (const p of [q, a, w, f]) core._free(p);
}
const a = await makeGame({ ai: false });
if (!a) {
  console.log('plane-drop seed: this core has no plane_drop_seed (skipped)');
  process.exit(0);
}
const b = await makeGame({ ai: false });
seedPage(a.host.core);
seedCore(b.host.core);
for (const g of [a, b]) g.host.state = f32(g.host.core, g.host.core._rider_state(), 16).slice();
for (let t = 0; t < 600; t++) {
  const pad = new Float32Array(24);
  pad[23] = t % 200 < 100 ? 1 : 0;
  const x = a.step(pad), y = b.step(pad);
  assert.equal(y.hash, x.hash, `plane drop tick ${t}`);
}
console.log('plane-drop seed: 600 ticks identical after the core\'s seeding and the page\'s');
