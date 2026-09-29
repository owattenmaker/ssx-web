// Node helpers for the online tests (web/test-remote-riders.mjs, web/test-mp-pairs.mjs): a human core on a course,
// set up and ticked like web/main.js simTick (pad_tick, race_begin, step_rider, animation_tick, race_end, camera).
import fs from 'node:fs';
import createCore from '../runtime/core.js';
const root = new URL('../public/assets/', import.meta.url);
export const file = (p) => fs.readFileSync(new URL(p, root));
export const json = (p) => JSON.parse(file(p));
export async function createTestRider({ course = 'ARA1', rider = 'RIDER_ZOE', settings = null, gridSeed = '', lighting = false } = {}) {
  const core = await createCore();
  const explain = (e) => (e instanceof Error ? e : new Error(core.getExceptionMessage ? core.getExceptionMessage(e).join(': ') : String(e)));
  const pointers = [], put = (b) => { const p = core._malloc(b.length); pointers.push(p); core.HEAPU8.set(b, p); return p; };
  const text = (s) => put(Buffer.concat([Buffer.from(s), Buffer.from([0])]));
  const initialPath = course === 'ARA1' ? 'ANIMATIONS/initial.json' : course + '/initial.json';
  const settingsText = settings ? JSON.stringify(settings) : file(initialPath).toString();
  const packets = file('ANIMATIONS/animation-packets.bin');
  try {
    const s = text(settingsText);
    core._init_animation(text(file('ANIMATIONS/animation-packets.json').toString()), text(file(rider + '/rider.json').toString()), s, put(packets), packets.length);
    core._init_race(s); core._animation_use_physics(1);
    const mesh = file(course + '/collision.bin'); core._init_world(put(mesh), mesh.length / 4);
    const hash = text(json(course + '/terrain.json').source_sha256), terrain = text(file(course + '/terrain.json').toString());
    core._init_terrain(terrain); core._init_world_collision(text(file(course + '/world_collision.json').toString()), hash); core._init_body_terrain(terrain); core._init_rails(text(file(course + '/rails.json').toString()), hash);
    if (lighting) {
      const env = file(course + '/environment.bin'); core._init_environment(text(file(course + '/environment.json').toString()), put(env), env.length);
      core._init_rider_lighting(text(file(course + '/local-lights.json').toString()), text(file(course + '/light-tree.json').toString()));
    }
  } catch (e) { throw explain(e); } finally { for (const p of pointers.splice(0)) core._free(p); }
  let rescues = 0;
  const start = json(course + '/start.json'), pad = core._malloc(96), f32 = (p, n) => new Float32Array(core.HEAPF32.buffer, p, n);
  const rider0 = {
    core, f32, explain,
    startEvent() {
      core._reset_animation(); core._reset_race(); core._reset_rider(...start.position, start.heading); core._reset_pad_history();
      if (core._human_grid_seed && gridSeed) { const b = Buffer.from(gridSeed + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); core._human_grid_seed(p); core._free(p); }
      core._start_event(); rescues = f32(core._rider_state(), 16)[13];
    },
    // One 60 Hz tick with a 24-channel pad; hooks.begin/end wrap it like main.js aiRace/mpGame begin/end.
    tick(input, hooks = {}) {
      try {
        hooks.begin?.();
        core.HEAPF32.set(input, pad >> 2); const o = f32(core._pad_tick(pad), 24).slice(); core._race_begin();
        let s = f32(core._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16).slice();
        if (s[13] !== rescues) { rescues = s[13]; core._reset_animation(); } // main.js simTick: a rescue restarts the rider animation
        core._animation_tick(s[7], o[10], o[11], s[9], s[8], o[6], o[8], o[7], o[7], 0, s[15], o[13]);
        const pose = f32(core._pose_physical(), 12).slice(); const race = f32(core._race_end(), 8).slice();
        hooks.end?.(); core._section_pass?.(); core._step_camera_head(pose[9], pose[10], pose[11]);
        return { state: f32(core._rider_state(), 16).slice(), race };
      } catch (e) { throw explain(e); }
    },
  };
  return rider0;
}
// A deterministic "played" pad script: carving, tucks, jumps with spins/flips and grabs.
export function scriptedPad(t, seed = 1) {
  const p = new Float32Array(24), phase = (t + seed * 97) % 600;
  const steer = Math.sin((t + seed * 31) / 45);
  if (steer > 0.3) p[21] = Math.min(1, steer); else if (steer < -0.3) p[20] = Math.min(1, -steer);
  if (phase > 100 && phase < 160) p[10] = 1;                     // hold Cross (crouch), release -> jump
  if (phase > 170 && phase < 230) { p[5 + (seed & 1)] = 1; p[12] = phase > 190 ? 1 : 0; } // spin + grab in the air
  if (phase > 300 && phase < 340) p[22] = 1;                       // tuck
  if (phase > 400 && phase < 440) p[10] = 1;
  if (phase > 445 && phase < 500) { p[6] = 1; p[13] = 1; }
  return p;
}
