// pv sharedParse (docs/first-load.md "The event's riders and the course parse"): main.js feeds the human core the course
// packages' own text (terrain.json, world_collision.json), as the node gates do (web/ai-race-node.mjs), so the computer riders'
// contexts, which pass the same text, copy that parse (web/world_bridge.cpp parse cache) instead of parsing again. Before, the human
// got the re-serialized package (JSON.stringify(JSON.parse(text))). Both set-ups race the same pad here: every tick's human
// rider state, every rider's reference motion and the game RNG words must be bit-identical (a pad change shows at tick ~230).
//   node test-shared-parse.mjs [ticks] [courses]     (skips without the game data)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import createCore from './runtime/core.js';
import { createAiRacers } from './ai-racers.js';
const root = new URL('public/assets/', import.meta.url);
if (!fs.existsSync(new URL('ARA1/world_collision.json', root))) { console.log('shared parse: skipped (no game data)'); process.exit(0); }
const read = (p) => fs.readFileSync(new URL(p, root)), text = (p) => read(p).toString('utf8');
const TICKS = +(process.argv[2] || 1200), COURSES = (process.argv[3] || 'ARA1,ASS1,DBC2').split(',');

async function race(course, reserialized) {
  const human = await createCore();
  const put = (bytes) => { const p = human._malloc(bytes.length); human.HEAPU8.set(bytes, p); return p; };
  const str = (s) => put(Buffer.from(s + '\0')), humanText = (s) => (reserialized ? JSON.stringify(JSON.parse(s)) : s);
  const resources = {
    packetsJson: text('ANIMATIONS/animation-packets.json'), packetsBin: read('ANIMATIONS/animation-packets.bin'), initialText: text(course === 'ARA1' ? 'ANIMATIONS/initial.json' : `${course}/initial.json`),
    collision: read(`${course}/collision.bin`), terrainText: text(`${course}/terrain.json`), worldCollisionText: text(`${course}/world_collision.json`), railsText: text(`${course}/rails.json`), riderText: {},
  };
  resources.terrainHash = JSON.parse(resources.terrainText).source_sha256;
  const doc = JSON.parse(text(`${course}/npc-riders.json`));
  for (const r of doc.riders) resources.riderText[r.package] = text(`${r.package}/rider.json`);
  human._init_animation(str(resources.packetsJson), str(text('RIDER_ZOE/rider.json')), str(resources.initialText), put(resources.packetsBin), resources.packetsBin.length);
  human._init_race(str(resources.initialText)); human._animation_use_physics(1);
  human._init_world(put(resources.collision), resources.collision.length / 4);
  const hash = str(resources.terrainHash);
  human._init_terrain(str(humanText(resources.terrainText)));
  human._init_world_collision(str(humanText(resources.worldCollisionText)), hash);
  human._init_body_terrain(str(humanText(resources.terrainText)));
  human._init_rails(str(resources.railsText), hash);
  const t0 = performance.now(), racers = await createAiRacers({ human, resources, document: doc }), setupMs = performance.now() - t0;
  const padPtr = human._malloc(96), f32 = (ptr, n) => new Float32Array(human.HEAPF32.buffer, ptr, n);
  human._reset_pad_history(); human._start_event(); racers.start();
  let seed = 12345; const rnd = () => ((seed = (seed * 1103515245 + 12345) >>> 0) / 4294967296);
  const trace = [];
  for (let t = 0; t < TICKS; t++) {
    const pad = new Float32Array(24);
    if (t % 180 < 120) pad[20 + (t % 360 < 180 ? 0 : 1)] = 0.6 + 0.4 * rnd();   // left stick x
    if (t % 97 < 20) pad[10] = 1; if (t % 211 < 40) pad[11] = 1; if (t % 301 > 280) pad[12] = 1;   // Cross, Square, L1
    racers.beginTick();
    human.HEAPF32.set(pad, padPtr >> 2);
    const o = Array.from(f32(human._pad_tick(padPtr), 24)); human._race_begin();
    const state = f32(human._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16);
    human._animation_tick(state[7], o[10], o[11], state[9], state[8], o[6], o[8], o[12], o[7], 0, state[15], o[13]);
    const pose = f32(human._pose_physical(), 12); human._race_end(); human._step_camera_head(pose[9], pose[10], pose[11]);
    racers.endTick();
    const words = [...f32(human._rider_state(), 16), ...f32(human._reference_motion(), 6), ...racers.npcs.flatMap((n) => [...new Float32Array(n.core.HEAPF32.buffer, n.core._reference_motion(), 6)])];
    trace.push(Buffer.from(Float32Array.from(words).buffer).toString('base64') + Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)).join(','));
  }
  return { trace, setupMs };
}
for (const course of COURSES) {
  const a = await race(course, true), b = await race(course, false), first = a.trace.findIndex((x, i) => x !== b.trace[i]);
  assert.equal(first, -1, `${course}: the re-serialized and the file-text set-ups differ at tick ${first}`);
  console.log(`${course}: identical through ${TICKS} ticks; the computer riders' set-up ${a.setupMs.toFixed(0)} -> ${b.setupMs.toFixed(0)} ms`);
}
console.log('shared parse OK: the file text and the re-serialized packages race identically; the contexts copy the human\'s parse');
