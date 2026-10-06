// Six-rider Snow Jam simulation benchmark (docs/sim-performance.md): CPU ms per 60 Hz tick of the browser's per-tick core
// work (web/ai-racers.js stages, the human's pad path, the FX passes, the camera) plus what the renderers read: the
// presentation buffers once per drawn frame (every --read-fx ticks) and the skin palettes on the last two ticks of a frame.
//   node web/bench-sim.mjs [--ticks 2000] [--read-fx 4] [--pad tuck|script] [CORE_DIR[:fast] ...]
// CORE_DIR: a directory with core.js/core.wasm (default web/runtime); ":fast" turns the presentation fast mode on.
// ":exact" runs every rider on the console arithmetic (a core built with SSX_PS2_EXACT_FPU=1, docs/ps2-float.md).
// With several cores, one race per core runs in this process in alternating 50-tick chunks, so machine load hits them
// alike; the median per-chunk ratio to the first core is printed.
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { createAiRacers } from './ai-racers.js';
import { scriptedPad } from './net/test-core.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2), opt = (k, d) => { const i = argv.indexOf('--' + k); if (i < 0) return d; const v = argv[i + 1]; argv.splice(i, 2); return v; };
const TICKS = +opt('ticks', 2000), READ_FX = +opt('read-fx', 4), PAD = opt('pad', 'tuck'), CHUNK = 50;
const specs = argv.length ? argv : [path.join(here, 'runtime')];
const read = (p) => fs.readFileSync(path.join(here, 'public/assets', p)), text = (p) => read(p).toString('utf8');

async function race(dir, mode) {
  const fast = mode === 'fast';
  const createCore = (await import(pathToFileURL(path.resolve(dir, 'core.js')).href + '?' + dir)).default;
  const human = await createCore(), course = 'ARA1';
  const put = (bytes) => { const p = human._malloc(bytes.length); human.HEAPU8.set(bytes, p); return p; }, str = (s) => put(Buffer.from(s + '\0'));
  const resources = { packetsJson: text('ANIMATIONS/animation-packets.json'), packetsBin: read('ANIMATIONS/animation-packets.bin'), initialText: text('ANIMATIONS/initial.json'),
    collision: read(`${course}/collision.bin`), terrainText: text(`${course}/terrain.json`), worldCollisionText: text(`${course}/world_collision.json`), railsText: text(`${course}/rails.json`), riderText: {} };
  resources.terrainHash = JSON.parse(resources.terrainText).source_sha256;
  const doc = JSON.parse(text(`${course}/npc-riders.json`)); for (const r of doc.riders) resources.riderText[r.package] = text(`${r.package}/rider.json`);
  human._init_animation(str(resources.packetsJson), str(text('RIDER_ZOE/rider.json')), str(resources.initialText), put(resources.packetsBin), resources.packetsBin.length);
  human._init_race(str(resources.initialText)); human._animation_use_physics(1); human._init_world(put(resources.collision), resources.collision.length / 4);
  const hash = str(resources.terrainHash);
  human._init_terrain(str(resources.terrainText)); human._init_world_collision(str(resources.worldCollisionText), hash); human._init_body_terrain(str(resources.terrainText)); human._init_rails(str(resources.railsText), hash);
  const racers = await createAiRacers({ human, resources, document: doc, sharedVisual: true });
  const padPtr = human._malloc(96), f32 = (ptr, n) => new Float32Array(human.HEAPF32.buffer, ptr, n);
  human._reset_pad_history(); human._start_event(); racers.start();
  const cores = [human, ...racers.npcs.map((n) => n.core)];
  if (fast) for (const c of cores) c._set_presentation_fast?.(1);
  if (mode === 'exact') {
    if (!human._ps2_arith_exact) throw new Error(`${dir}: no ps2_arith_exact (build with SSX_CORE_CFLAGS=-DSSX_PS2_EXACT_FPU=1)`);
    for (const c of cores) c._ps2_arith_exact(1);
  }
  let t = 0;
  return () => {
    const pad = new Float32Array(24); if (PAD === 'script') pad.set(scriptedPad(t, 1)); else pad[22] = 1;
    racers.beginTick(); human.HEAPF32.set(pad, padPtr >> 2);
    const o = f32(human._pad_tick(padPtr), 24).slice(); human._race_begin();
    const s = f32(human._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16).slice();
    human._animation_tick(s[7], o[10], o[11], s[9], s[8], o[6], o[8], o[7], o[7], 0, s[15], o[13]);
    const pose = f32(human._pose_physical(), 12).slice(); human._race_end(); racers.endTick(); human._step_camera_head(pose[9], pose[10], pose[11]);
    if (t % READ_FX >= READ_FX - 2) for (const c of cores) if (c._rider_skin_palette_count()) c._rider_skin_palette();
    if (t % READ_FX === READ_FX - 1) for (const c of cores) { c._snow_info(); c._trail_info(); c._wake_info(); }
    t++;
  };
}
const ticks = []; for (const spec of specs) { const [dir, mode] = spec.split(':'); ticks.push(await race(dir, mode)); }
for (const tick of ticks) for (let i = 0; i < 250; i++) tick(); // countdown and warm-up
const cpu = specs.map(() => 0), chunks = specs.map(() => []);
for (let done = 0, round = 0; done < TICKS; done += CHUNK, round++) for (let j = 0; j < ticks.length; j++) {
  const k = (j + round) % ticks.length, c0 = process.cpuUsage(); for (let i = 0; i < CHUNK; i++) ticks[k](); const c = process.cpuUsage(c0), ms = (c.user + c.system) / 1000;
  cpu[k] += ms; chunks[k].push(ms / CHUNK);
}
const median = (a) => [...a].sort((x, y) => x - y)[a.length >> 1];
specs.forEach((spec, k) => console.log(`${spec}: ${(cpu[k] / TICKS).toFixed(3)} CPU ms/tick` + (k ? `, ${median(chunks[k].map((v, i) => v / chunks[0][i])).toFixed(3)}x of the first` : '')));
