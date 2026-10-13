// The wasm build's per-tick ?simtrace hashes of native/core/race_driver.cpp's script (docs/core-migration.md "Native core") (web/test-core-tick.mjs makeGame, pv coreTick and
// the core's rider manager): node native/tools/ref-trace.mjs [--ai] [--ticks N] [--give-up T] > out.txt. CORE_JS=<core.js>, SSX_ASSETS=<dir>.
import fs from 'node:fs';
const WEB = new URL('../../web/', import.meta.url);
import { setPv } from '../../web/pv-flags.js';
import { createGameTick, gameTrace, traceOut, TRACE_OUT_FLOATS } from '../../web/game-tick.js';
import { createAiRacers } from '../../web/ai-racers.js';
const coreUrl = (await import('node:url')).pathToFileURL(process.env.CORE_JS || new URL('runtime/core.js', WEB).pathname).href;
const createCore = (await import(coreUrl)).default;
// SSX_ASSETS=<dir>: the exported game data (default web/public/assets)
const root = process.env.SSX_ASSETS ? (await import('node:url')).pathToFileURL(process.env.SSX_ASSETS.replace(/\/?$/, '/')) : new URL('public/assets/', WEB);
const read = (p) => fs.readFileSync(new URL(p, root));
const text = (p) => read(p).toString('utf8');
const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
const args = process.argv.slice(2), opt = (k, d) => (args.includes(k) ? +args[args.indexOf(k) + 1] : d);
const ai = args.includes('--ai'), ticks = opt('--ticks', 3000), giveUp = opt('--give-up', -1);
setPv('coreTick', true);
setPv('coreRiderManager', true);
setPv('coreEventStart', true);
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

const g = await makeGame({ ai });
const lines = [];
for (let t = 0; t < ticks && g.host.running; t++) {
  if (t === giveUp) g.host.core._race_give_up();
  const { hash } = g.step(padAt(t));
  lines.push(`${t} ${hash}`);
}
process.stdout.write(lines.join('\n') + '\n');
