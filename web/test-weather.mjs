// Weather (docs/weather.md): packages and the snowfall draw model.
//  1. Every event course and every streamed-world location with a world painter has weather.json (tools/export_weather.py):
//     19-value payloads, a point tree, the ready state for event courses; the known wind / snowfall / lightning figures.
//  2. The flake / fluff positions of web/weather-renderer.js equal the original VU1 program 5 output
//     (tools/test_snowfall_vu_native.py -> local/reference/weather/snowfall-vu.json; skipped when absent).
//  3. The core loads every course's weather.json and reports the camera painter and the six snowfall layers.
// The simulation itself (wind push, painters, snowfall counts) is gated against the PS2 by test-ps2-captures.mjs
// (peak2/dbc2-race-tuck, weather/dbc2-weather) and instruction-exact by tools/test_weather_native.py.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { flakeSeeds, layerBox, flakePosition } from './weather-renderer.js';

const assets = new URL('public/assets/', import.meta.url);
const read = (p) => JSON.parse(fs.readFileSync(new URL(p, assets), 'utf8'));
const F = (w) => new Float32Array(new Uint32Array([w]).buffer)[0];
let checked = 0;

// ---- 1. packages ----------------------------------------------------------------------------------------------------
const courseList = read('courses.json').courses, courses = [...new Set((Array.isArray(courseList) ? courseList.map((c) => c.code) : Object.keys(courseList)).filter((c) => /^[A-E](RA|SS|BA|HP|BC)\d$/.test(c)))];
assert.ok(courses.length >= 17, 'the event courses of courses.json');
const expect = { DBC2: { wind: 40, snowfall: 6 }, EBC3: { wind: 14.5 }, ERA5: { wind: 8, snowfall: 3 }, EHP3: { wind: 7.5 }, EBA3: { lightning: 0.06 }, ESS3: { lightning: 0.02 }, ARA1: { wind: 0 }, ABC1: { wind: 0 } };
for (const code of courses) {
  const file = new URL(`${code}/weather.json`, assets); if (!fs.existsSync(file)) { console.log(`skip ${code}: no weather.json (tools/export_weather.py)`); continue; }
  const w = JSON.parse(fs.readFileSync(file, 'utf8'));
  assert.equal(w.version, 1); assert.equal(w.location, code);
  assert.ok(w.payloads.length > 0 && w.payloads.every((p) => p.values.length === 19 && Number.isFinite(p.transition)), `${code}: payloads`);
  assert.ok(w.regions.nodes.length > 0 && w.regions.scale > 0, `${code}: point tree`);
  assert.equal(w.defaults.length, 19);
  assert.ok(w.ready?.rider && w.ready?.camera && w.ready.layers?.length === 6 && w.ready.splash, `${code}: ready state (rider / camera painters, 6 layers, splash)`);
  const max = (k) => Math.max(...w.payloads.map((p) => p.values[k]));
  const e = expect[code]; if (e) { if (e.wind !== undefined) assert.equal(Math.fround(max(4)), Math.fround(e.wind), `${code} wind`); if (e.snowfall !== undefined) assert.equal(Math.fround(max(0)), Math.fround(e.snowfall), `${code} snowfall`); if (e.lightning !== undefined) assert.equal(Math.fround(max(10)), Math.fround(e.lightning), `${code} lightning`); }
  checked++;
}
for (const world of ['PEAK1', 'PEAK2', 'PEAK3']) {
  const dir = new URL(`${world}/`, assets); if (!fs.existsSync(dir)) continue;
  for (const loc of fs.readdirSync(dir)) {
    if (!fs.existsSync(new URL(`${world}/${loc}/fog-tree.json`, assets))) continue;
    const file = new URL(`${world}/${loc}/weather.json`, assets); if (!fs.existsSync(file)) { console.log(`skip ${world}/${loc}: no weather.json`); continue; }
    const w = JSON.parse(fs.readFileSync(file, 'utf8')); assert.equal(w.location, loc); assert.ok(w.payloads.every((p) => p.values.length === 19)); checked++;
  }
}
console.log(`weather packages: ${checked} locations`);

// ---- 2. VU1 program 5 -----------------------------------------------------------------------------------------------
const fixture = new URL('../local/reference/weather/snowfall-vu.json', import.meta.url);
if (fs.existsSync(fixture)) {
  const { cases } = JSON.parse(fs.readFileSync(fixture, 'utf8')); let sprites = 0, worst = 0;
  for (const c of cases) {
    assert.equal(c.sprites.length, c.count, `kind ${c.kind}: every flake inside the synthetic view is drawn`);
    const lower = c.lower.map(F), box = { base: c.offsetN.map(F), lower, upper: lower.map((l) => Math.fround(l + 1)) };
    const r = flakeSeeds(c.seeds, c.count);
    c.sprites.forEach(([x0, y0, z0, x1, y1, z1], k) => {
      const n = flakePosition([r[3 * k], r[3 * k + 1], r[3 * k + 2]], box, 1), centre = [(x0 + x1) / 2 / 4096, (y0 + y1) / 2 / 4096];
      const err = Math.max(Math.abs(centre[0] - n[0]), Math.abs(centre[1] - n[1])); worst = Math.max(worst, err);
      assert.ok(err < 2 / 4096, `kind ${c.kind} flake ${k}: VU centre ${centre} vs renderer ${n.slice(0, 2)}`);
      const half = (x0 - x1) / 2 / 4096; assert.ok(Math.abs(half - F(c.size)) < 2 / 4096, `kind ${c.kind} flake ${k}: half size ${half} vs ${F(c.size)}`);
      sprites++;
    });
  }
  console.log(`snowfall VU1 program 5: ${cases.length} layers, ${sprites} sprites = web/weather-renderer.js positions and sizes (worst ${worst.toExponential(2)})`);
} else console.log('skip snowfall VU oracle: run tools/test_snowfall_vu_native.py');

// ---- 3. the core's weather state per course --------------------------------------------------------------------------
const createCore = (await import(process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js')).default;
const core = await createCore();
if (core._init_weather) {
  const put = (text) => { const b = Buffer.from(text + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  let loaded = 0;
  for (const code of courses) {
    const file = new URL(`${code}/weather.json`, assets); if (!fs.existsSync(file)) continue;
    const w = JSON.parse(fs.readFileSync(file, 'utf8')); const p = put(JSON.stringify(w)); assert.equal(core._init_weather(p), 1); core._free(p);
    const info = new Float32Array(core.HEAPF32.buffer, core._weather_info(), 47), layers = new Uint32Array(core.HEAPU8.buffer, core._weather_layers(), 1);
    assert.equal(info[0], 1, `${code}: weather ready`); assert.equal(layers[0], 6, `${code}: the snowfall object's six layers`);
    for (let k = 0; k < 19; k++) assert.equal(Math.fround(info[11 + k]), F(w.ready.camera.current[k]), `${code}: camera painter property ${k} from the ready state`);
    loaded++;
  }
  assert.equal(core._init_weather(0), 0, 'no weather.json: weather off');
  console.log(`core weather: ${loaded} courses loaded`);
}

// ---- 4..6. painter resets under the reset fade, streamed-world painter records, the streamed flag manager --------------------------
// A Snow Jam rider core (as web/test-forced-reset.mjs) with Snow Jam's weather.json.
if (core._weather_fade_reset && core._weather_location && core._init_streamed_flags) {
  const c = await createCore(), put = (b) => { const p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
  const text = (p) => fs.readFileSync(new URL(p, assets), 'utf8'), str = (t) => put(new TextEncoder().encode(t + '\0'));
  const f = (ptr, n) => new Float32Array(c.HEAPF32.buffer, ptr, n).slice(), i32 = (ptr, n) => new Int32Array(c.HEAPU8.buffer, ptr, n).slice();
  { const m = str(text('ANIMATIONS/animation-packets.json')), r = str(text('RIDER_SAM/rider.json')), s0 = str(text('ANIMATIONS/initial.json')), raw = fs.readFileSync(new URL('ANIMATIONS/animation-packets.bin', assets)), pk = put(raw);
    c._init_animation(m, r, s0, pk, raw.length); }
  { const mesh = fs.readFileSync(new URL('ARA1/collision.bin', assets)); c._init_world(put(mesh), mesh.length / 4); }
  c._init_terrain(str(text('ARA1/terrain.json'))); c._init_world_collision(str(text('ARA1/world_collision.json')), str(read('ARA1/terrain.json').source_sha256));
  c._animation_use_physics(1); c._init_body_terrain(str(text('ARA1/terrain.json'))); c._init_race(str(text('ANIMATIONS/initial.json')));
  const weatherText = text('ARA1/weather.json'); assert.equal(c._init_weather(str(weatherText)), 1);
  const start = read('ARA1/start.json');
  const step = () => { c._race_begin(); const m = f(c._step_rider(0, 0, 0, 0), 16); c._animation_tick(m[7], 0, 0, m[9], m[8], 0, 0, 0, 0, 4, m[15], 0); c._race_end(); const pose = f(c._pose_physical(), 12); c._step_camera_head(...pose.slice(9, 12));
    return { rider: f(c._weather_painter_info(), 40), camera: f(c._weather_info(), 30), region: i32(c._weather_region_info(), 5), reset: f(c._reset_info(), 9), patch: f(c._terrain_contact_info(), 1)[0] }; };
  c._reset_rider(...start.position, start.heading); c._reset_animation(); c._reset_race(); c._reset_weather();
  for (let t = 0; t < 120; t++) step();
  // 4. 0x2E47E8: the reset fade (progress 0.025 a tick; placement at 21) resets every painter on the renders of opacity >= 0.93:
  //    progress 0.475 / 0.5 / 0.525 = ticks 19, 20, 21 after the request. The rider painter's +0 is -99999 after those ticks
  //    only (the next step jumps to its payload); the placement's 0x111890 reset falls on tick 21 too.
  const fade0 = step().region[4]; c._request_rider_reset(2); const rows = [];
  for (let t = 1; t <= 45; t++) { const s = step(); rows.push({ t, fades: s.region[4] - fade0, riderDistance: s.rider[1], cameraDistance: s.camera[10], alpha: s.reset[8] }); }
  const fadeTicks = rows.filter((r, k) => r.fades > (k ? rows[k - 1].fades : 0)).map((r) => r.t);
  assert.deepEqual(fadeTicks, [19, 20, 21], 'painter resets under the reset fade: ticks of opacity >= 0.93');
  assert.deepEqual(rows.filter((r) => r.riderDistance === -99999).map((r) => r.t), [19, 20, 21], 'the rider Weather painter is reset (-99999) after exactly those ticks');
  assert.deepEqual(rows.filter((r) => r.cameraDistance === -99999).map((r) => r.t), [19, 20, 21], 'the camera Weather painter too');
  console.log(`fade painter resets: ticks ${fadeTicks.join(', ')} of the reset (opacity ${rows.filter((r) => fadeTicks.includes(r.t)).map((r) => r.alpha.toFixed(3)).join(' / ')})`);
  // 5. Streamed-world records (2C0778 / gp+0x770): with Snow Jam's section as the record of the rider's track the painters step
  //    exactly as the single-package path; a track without a loaded record gives the class defaults with distance 0.
  const run = (setup, ticks = 90) => { c._reset_rider(...start.position, start.heading); c._reset_animation(); c._reset_race(); c._init_weather(str(weatherText)); setup(); c._reset_weather(); const out = []; for (let t = 0; t < ticks; t++) out.push(step()); return out; };
  const single = run(() => {});
  const track = single.map((s) => s.patch).find((p) => p >= 0) & 0xFF;
  const located = run(() => { assert.equal(c._weather_location(track, str(weatherText)), 1); c._weather_region_seed(track); });
  single.forEach((s, k) => { assert.deepEqual(Array.from(located[k].rider.slice(1)), Array.from(s.rider.slice(1)), `tick ${k}: rider painter, located record = single package`); assert.deepEqual(Array.from(located[k].camera.slice(10, 30)), Array.from(s.camera.slice(10, 30)), `tick ${k}: camera painter`); });
  assert.equal(located[located.length - 1].region[0], track, 'gp+0x770 follows the contact patch track');
  const missing = run(() => { c._weather_location((track + 1) & 0xFF, str(weatherText)); c._weather_region_seed(track); });
  const defaults = JSON.parse(weatherText).defaults.map(Math.fround);
  assert.deepEqual(Array.from(missing[5].rider.slice(2, 21)), defaults, 'no record for the region track: the class defaults (2BE258)'); assert.equal(missing[5].rider[1], 0, '... with distance 0');
  assert.equal(c._init_weather(str(weatherText)), 1); assert.equal(i32(c._weather_region_info(), 5)[2], 0, 'a new world (init_weather) drops the located records');
  console.log(`streamed painter records: track ${track} record = the single package on ${single.length} ticks; a missing record = the class defaults`);
  // 6. The streamed flag manager (0x34C428 wind; mode of the current course 0x2D1BA0): init_streamed_flags.
  const flagText = fs.existsSync(new URL('PEAK3/SETPIECES/flags.json', assets)) ? text('PEAK3/SETPIECES/flags.json') : null;
  if (flagText) { assert.ok(c._init_streamed_flags(str(flagText)) > 0); const w = f(c._stage_flag_wind(), 5); assert.deepEqual(Array.from(w.slice(0, 4)), [0, 0.5, 0.25, 0], 'fresh manager wind');
    for (const [course, mode] of [[-1, 1], [0, 1], [3, 2], [16, 3], [21, 3], [27, 1]]) { c._peak_world_set_course(course); assert.equal(f(c._stage_flag_wind(), 5)[4], mode, `course ${course}: flag wind mode`); }
    c._peak_world_set_course(-1); console.log('streamed flag manager: fresh wind, mode by the current course'); }
}
console.log('weather: ok');
