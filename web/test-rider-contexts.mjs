// Rider contexts (web/rider_context.cpp, docs/ai-racers.md "One core, six riders"): a context created in a core whose
// human has already raced must behave exactly like a freshly instantiated core. Rider A runs in a fresh core; rider B
// runs in a new context of a core that raced 700 ticks first (its course geometry shared, the course package parses
// copied from the parse cache). Both ride the same pad script from the event start; every tick's rider state, race
// progress, game RNG, visual RNG and camera words must be equal.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import { riderContextCore } from './ai-racers.js';
import { scriptedPad } from './net/test-core.mjs';

const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root)), text = (p) => read(p).toString('utf8');
const course = 'ARA1', initial = text('ANIMATIONS/initial.json'), terrain = text(`${course}/terrain.json`), hash = JSON.parse(terrain).source_sha256;
const packetsJson = text('ANIMATIONS/animation-packets.json'), packetsBin = read('ANIMATIONS/animation-packets.bin'), riderJson = text('RIDER_ZOE/rider.json');
const collision = read(`${course}/collision.bin`), worldCollision = text(`${course}/world_collision.json`), rails = text(`${course}/rails.json`);
function loader(core) {
  const put = (b) => { const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; }, str = (s) => put(Buffer.from(s + '\0'));
  return {
    animation() { core._init_animation(str(packetsJson), str(riderJson), str(initial), put(packetsBin), packetsBin.length); },
    race() { core._init_race(str(initial)); core._animation_use_physics(1); },
    geometry() { core._init_world(put(collision), collision.length / 4); core._init_terrain(str(terrain)); },
    package() { const h = str(hash); core._init_world_collision(str(worldCollision), h); core._init_body_terrain(str(terrain)); core._init_rails(str(rails), h); },
  };
}
function tick(core, padPtr, pad) {
  core.HEAPF32.set(pad, padPtr >> 2);
  const o = Array.from(new Float32Array(core.HEAPF32.buffer, core._pad_tick(padPtr), 24));
  core._race_begin();
  const s = new Float32Array(core.HEAPF32.buffer, core._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16);
  core._animation_tick(s[7], o[10], o[11], s[9], s[8], o[6], o[8], o[7], o[7], 0, s[15], o[13]);
  const pose = Array.from(new Float32Array(core.HEAPF32.buffer, core._pose_physical(), 12));
  core._race_end(); core._fx_pass?.(-1); core._step_camera_head(pose[9], pose[10], pose[11]);
}
const words = (core) => {
  const f = (p, n) => Array.from(new Float32Array(core.HEAPF32.buffer, p, n)), u = (p, n) => Array.from(new Uint32Array(core.HEAPU8.buffer, p, n));
  return JSON.stringify([f(core._rider_world_state(), 16), f(core._race_progress_info(), 8), f(core._rider_state(), 16), u(core._animation_rng_words(), 6), u(core._visual_rng_words(), 6), u(core._camera_state_words(), 271)]);
};
// A: a fresh core.
const a = await createCore(); { const l = loader(a); l.animation(); l.race(); l.geometry(); l.package(); }
// B: a core whose human raced first, then a new rider context in it.
const host = await createCore(); { const l = loader(host); l.animation(); l.race(); l.geometry(); l.package(); }
const hostPad = host._malloc(96); host._reset_pad_history(); host._start_event();
for (let t = 0; t < 700; t++) tick(host, hostPad, scriptedPad(t, 3));
const b = riderContextCore(host, host._rider_context_create()); { const l = loader(b); l.animation(); l.race(); l.package(); }
host._rider_parse_cache_clear();
const padA = a._malloc(96), padB = b._malloc(96);
a._reset_pad_history(); a._start_event(); b._reset_pad_history(); b._start_event();
assert.equal(words(b), words(a), 'a new context differs from a fresh core at the event start');
for (let t = 0; t < 1500; t++) {
  const pad = scriptedPad(t, 1);
  tick(a, padA, pad); tick(b, padB, pad);
  assert.equal(words(b), words(a), `a rider context left the fresh core's path at tick ${t}`);
  if (t % 250 === 0) tick(host, hostPad, scriptedPad(700 + t, 3)); // the host's own context keeps running between them
}
console.log(`rider contexts: a context created after 700 host ticks rides 1500 ticks exactly like a fresh core (TLS block ${host._rider_context_bytes()} B)`);
