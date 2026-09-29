// Every prepared course/event location (public/assets/courses.json, tools/locations.py, docs/locations.md) loads
// through the production init path and runs its original event start: files present, the core accepts
// the course packages, the grid countdown/push-off runs and the rider moves along the course.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';

const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p.replace(/^\/assets\//, ''), root));
const manifest = JSON.parse(read('courses.json'));
assert.equal(manifest.default, 'ARA1');
assert(manifest.courses.some((c) => c.code === 'ARA1'), 'Snow Jam missing from courses.json');
let checked = 0;
for (const course of manifest.courses) {
  const files = ['world.json', 'vertices.bin', 'indices.bin', 'colors.bin', 'vertex-alpha.bin', 'collision.bin', 'start.json', 'route.json', 'terrain.json',
    'world_collision.json', 'rails.json', 'fog-tree.json', 'environment.json', 'environment.bin', 'screen-tint.json', 'terrain-render.json',
    'terrain-lighting.json', 'terrain-light-atlas.png', 'local-lights.json', 'light-tree.json'];
  for (const f of files) assert(fs.existsSync(new URL(course.root.replace(/^\/assets\//, '') + f, root)), `${course.code}: missing ${course.root}${f}`);
  for (const f of [course.sky + 'world.json', course.sunFlare + 'sun-flare.json', course.lightGlow + 'light-glow.json', course.initial])
    assert(fs.existsSync(new URL(f.replace(/^\/assets\//, ''), root)), `${course.code}: missing ${f}`);
  const world = JSON.parse(read(course.root + 'world_collision.json'));
  assert.equal(world.location, course.code, `${course.code}: world_collision.json belongs to ${world.location}`);
  if (course.code === 'ARA1') continue; // Snow Jam is covered by the rest of npm test
  const core = await createCore();
  const put = (bytes) => { const p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); return p; };
  const str = (path) => put(Buffer.concat([read(path), Buffer.from([0])]));
  core._init_animation(str('/assets/ANIMATIONS/animation-packets.json'), str('/assets/RIDER_ZOE/rider.json'), str(course.initial), put(read('/assets/ANIMATIONS/animation-packets.bin')), read('/assets/ANIMATIONS/animation-packets.bin').length);
  core._init_race(str(course.initial)); core._animation_use_physics(1);
  const mesh = read(course.root + 'collision.bin'); core._init_world(put(mesh), mesh.length / 4);
  const terrain = JSON.parse(read(course.root + 'terrain.json')); const hash = put(Buffer.from(terrain.source_sha256 + '\0'));
  core._init_terrain(str(course.root + 'terrain.json')); core._init_world_collision(str(course.root + 'world_collision.json'), hash);
  core._init_body_terrain(str(course.root + 'terrain.json')); core._init_rails(str(course.root + 'rails.json'), hash);
  core._init_fog(str(course.root + 'fog-tree.json'));
  core._init_rider_lighting(str(course.root + 'local-lights.json'), str(course.root + 'light-tree.json'));
  const start = JSON.parse(read(course.root + 'start.json'));
  core._reset_animation(); core._reset_race(); core._reset_rider(...start.position, start.heading);
  const pad = core._malloc(96); const neutral = new Float32Array(24);
  const step = () => { core.HEAPF32.set(neutral, pad >> 2); const o = new Float32Array(core.HEAPF32.buffer, core._pad_tick(pad), 24).slice();
    core._race_begin(); const s = new Float32Array(core.HEAPF32.buffer, core._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16).slice();
    core._animation_tick(s[7], o[10], o[11], s[9], s[8], o[6], o[8], o[12], o[7], 0, s[15], o[13]); core._race_end(); return s; };
  let first = null, last = null;
  if (course.eventStart) { core._reset_pad_history(); core._start_event(); }
  for (let t = 0; t < 600; t++) { last = step(); first ??= last; for (const v of last.slice(0, 8)) assert(Number.isFinite(v), `${course.code}: non-finite rider state at tick ${t}`); }
  const moved = Math.hypot(last[0] - first[0], last[1] - first[1], last[2] - first[2]);
  assert(moved > 20, `${course.code}: rider moved only ${moved.toFixed(2)} m in 10 s`);
  assert(last[1] < first[1], `${course.code}: rider did not descend`);
  console.log(`${course.code} (${course.name}): loads, ${course.eventStart ? 'event start' : 'free ride'} ran 600 ticks, moved ${moved.toFixed(1)} m`);
  checked++;
}
console.log(`Course locations: ${checked} non-Snow-Jam course(s) checked.`);
