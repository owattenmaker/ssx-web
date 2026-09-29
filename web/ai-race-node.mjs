// Node setup of a six-rider Snow Jam race (one core: the human and five computer-rider contexts), shared by
// compare-ai-capture.mjs style tools and test-ai-racers.mjs.
import fs from 'node:fs';
import createCore from './runtime/core.js';
import { createAiRacers } from './ai-racers.js';

const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const text = (p) => read(p).toString('utf8');

export async function createNodeRace({ humanPackage = 'RIDER_ZOE', isolate = false, course = 'ARA1', onDraws = null, afterRider = null, document = null,  } = {}) {
  const human = await createCore();
  const put = (bytes) => { const p = human._malloc(bytes.length); human.HEAPU8.set(bytes, p); return p; };
  const str = (s) => put(Buffer.from(s + '\0'));
  const resources = {
    packetsJson: text('ANIMATIONS/animation-packets.json'), packetsBin: read('ANIMATIONS/animation-packets.bin'), initialText: text(course === 'ARA1' ? 'ANIMATIONS/initial.json' : `${course}/initial.json`),
    collision: read(`${course}/collision.bin`), terrainText: text(`${course}/terrain.json`), worldCollisionText: text(`${course}/world_collision.json`), railsText: text(`${course}/rails.json`), riderText: {},
  };
  resources.terrainHash = JSON.parse(resources.terrainText).source_sha256;
  const doc = document || JSON.parse(text(`${course}/npc-riders.json`)); // document: an assembled lineup (web/lineup.js, test-lineups.mjs)
  for (const r of doc.riders) resources.riderText[r.package] = text(`${r.package}/rider.json`);
  human._init_animation(str(resources.packetsJson), str(text(`${humanPackage}/rider.json`)), str(resources.initialText), put(resources.packetsBin), resources.packetsBin.length);
  human._init_race(str(resources.initialText));
  human._animation_use_physics(1);
  human._init_world(put(resources.collision), resources.collision.length / 4);
  const hash = str(resources.terrainHash);
  human._init_terrain(str(resources.terrainText));
  human._init_world_collision(str(resources.worldCollisionText), hash);
  human._init_body_terrain(str(resources.terrainText));
  human._init_rails(str(resources.railsText), hash);
  const racers = await createAiRacers({ human, resources, document: doc, isolate, onDraws, afterRider });
  const padPtr = human._malloc(96);
  const f32 = (ptr, n) => new Float32Array(human.HEAPF32.buffer, ptr, n);
  // The production pad path (main.js simTick): provider, race clock, motion, animation, course, camera.
  function humanTick(pad) {
    human.HEAPF32.set(pad, padPtr >> 2);
    const o = Array.from(f32(human._pad_tick(padPtr), 24));
    human._race_begin();
    const state = f32(human._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16);
    human._animation_tick(state[7], o[10], o[11], state[9], state[8], o[6], o[8], o[12], o[7], 0, state[15], o[13]);
    const pose = f32(human._pose_physical(), 12);
    const race = Array.from(f32(human._race_end(), 8));
    human._step_camera_head(pose[9], pose[10], pose[11]);
    return race;
  }
  const start = () => { human._reset_pad_history(); human._start_event(); racers.start(); };
  const tick = (pad) => { racers.beginTick(); const race = humanTick(pad); racers.endTick(); return race; };
  return { human, racers, doc, start, tick, humanTick, f32, resources }; // resources.riderText: packages for racers.setDocument (test-lineups.mjs)
}
