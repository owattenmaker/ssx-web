// Every selectable rider (riders.json, docs/characters.md) initialises as the human with its own package and
// per-character settings (web/character-roster.js over the course initial.json, tools/export_characters.py)
// and completes the Snow Jam grid start: countdown, release, riding away with finite poses. Zoe/Sam keep the
// course initial.json exactly (their capture gates run elsewhere).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import { humanSettings, mergeCharacterSettings, composeCheat } from './character-roster.js';
import { frontEndSpeechBank } from './rider-speech.js';

const root = 'public/assets/';
const read = (p) => JSON.parse(fs.readFileSync(root + p));
const initial = read('ANIMATIONS/initial.json');
const riders = read('riders.json');
const metadataText = fs.readFileSync(root + 'ANIMATIONS/animation-packets.json', 'utf8');
const packets = fs.readFileSync(root + 'ANIMATIONS/animation-packets.bin');
const mesh = fs.readFileSync(root + 'ARA1/collision.bin');
const terrain = read('ARA1/terrain.json'), world = read('ARA1/world_collision.json');

assert.deepEqual(mergeCharacterSettings({ a: { b: 1, c: [1, 2] }, d: 2 }, { a: { c: [3] } }), { a: { b: 1, c: [3] }, d: 2 });
const doc = (pkg) => { const file = `${root}${pkg}/settings.json`; return fs.existsSync(file) ? JSON.parse(fs.readFileSync(file)) : null; };
const selectable = riders.filter((r) => fs.existsSync(`${root}${r.package}/rider.json`));
assert(selectable.length >= 31, 'the ten original riders, Sam and the twenty cheat characters are packaged');
// A cheat skin rides on a base rider: Zoe (its own savestates), and Stretch's Nose Grab uber over Psymon's table.
const stretchOnPsymon = composeCheat(doc('RIDER_PSYMON'), doc('RIDER_STRETCH'));
const psymonUber = humanSettings(initial, doc('RIDER_PSYMON')).original_grab_control.profile.uber, mixed = humanSettings(initial, stretchOnPsymon).original_grab_control.profile.uber;
assert.equal(mixed[0][4].semantic, 160, 'Stretch Nose Grab uber (0x150198)');
assert.deepEqual(mixed[0][9], psymonUber[0][9], 'the base rider keeps its other uber rows');
assert.equal(humanSettings(initial, stretchOnPsymon).original_trick_identity.rider_stance, humanSettings(initial, doc('RIDER_PSYMON')).original_trick_identity.rider_stance, 'stance follows the base rider');
selectable.push({ ...riders.find((r) => r.id === 'stretch'), id: 'stretch+psymon', composed: stretchOnPsymon });

const gridSpots = new Map();
for (const rider of selectable) {
  const character = rider.composed ?? (rider.kind === 'cheat' ? composeCheat(null, doc(rider.package)) : doc(rider.package));
  if (rider.id === 'zoe') assert.equal(character, null, 'zoe must stay on the course initial.json');
  if (rider.id === 'sam') assert.equal(character.settings.original_animation.scale[0], Math.fround(0.96), 'Sam: 5\'11" = model size 96 (Elise)');
  const settings = humanSettings(initial, character);
  if (!character) assert.equal(settings, initial);
  const rig = JSON.parse(fs.readFileSync(`${root}${rider.package}/rider.json`));
  const c = await createCore();
  const put = (b) => { const p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
  const str = (x) => put(new TextEncoder().encode((typeof x === 'string' ? x : JSON.stringify(x)) + '\0'));
  const explain = (fn) => { try { return fn(); } catch (e) { throw e instanceof Error ? e : new Error(`${rider.id}: ${c.getExceptionMessage(e)}`); } };
  explain(() => {
    { const p = put(mesh); c._init_world(p, mesh.length / 4); c._free(p); }
    const m = str(metadataText), r = str(rig), s = str(settings), p = put(packets); c._init_animation(m, r, s, p, packets.length); c._init_race(s); for (const x of [m, r, s, p]) c._free(x);
    c._animation_use_physics(1);
    { const t = str(terrain); c._init_terrain(t); c._free(t); }
    { const w = str(world), h = str(terrain.source_sha256); c._init_world_collision(w, h); c._free(w); c._free(h); }
    { const t = str(terrain); c._init_body_terrain(t); c._free(t); }
  });
  const f = (ptr, n) => new Float32Array(c.HEAPF32.buffer, ptr, n).slice();
  // channel-1 masks rider+0x8C0/+0x8D0 as init_animation took them (upper_request_info [6..10])
  const masks = f(c._upper_request_info(), 12);
  const want = BigInt(character?.identity?.upper_mask8c0 ?? '0x8000fffe');
  assert.equal(masks[6] * 65536 + masks[7], Number(want), `${rider.id}: channel-1 mask +0x8C0`);
  explain(() => c._start_event());
  // Grid spot: the rider's own countdown ground state (settings.original_event_start), else Zoe's compiled seed.
  {
    const seed = settings.original_event_start?.state?.position, grid = f(c._rider_state(), 16);
    const want = seed ? [seed[0] / 100, seed[2] / 100, -seed[1] / 100] : null;
    if (want) for (let k = 0; k < 3; k++) assert(Math.abs(grid[k] - Math.fround(want[k])) < 2e-5, `${rider.id}: grid spot ${grid.slice(0, 3)} vs ${want}`);
    gridSpots.set(rider.id, grid.slice(0, 3));
  }
  let released = -1, origin = null, far = 0;
  explain(() => {
    for (let tick = 0; tick < 700; tick++) {
      const tuck = released >= 0 && tick < released + 200;
      c._start_input(0);
      c._ride_command(0, tuck ? c._original_axis(1) : 0, 0, 0, 0, 0, 0, 0, 0, 1);
      c._race_begin();
      const r = f(c._step_rider(0, 0, 0, 0), 16);
      const pose = f(c._animation_tick(r[7], 0, 0, r[9], r[8], 0, 0, 0, 0, 0, r[15], 0), rig.bones.length * 7);
      assert(pose.every(Number.isFinite), `${rider.id}: finite pose at tick ${tick}`);
      c._race_end();
      const start = f(c._start_info(), 7);
      if (released < 0 && !start[1]) { released = tick; origin = r.slice(0, 3); }
      if (origin) far = Math.max(far, Math.hypot(r[0] - origin[0], r[2] - origin[2]));
    }
  });
  assert(released >= 180 && released < 240, `${rider.id}: grid release at ${released}`);
  assert(far > 20, `${rider.id}: rode ${far.toFixed(1)} m`);
  console.log('character', rider.id, { package: rider.package, bones: rig.bones.length, released, metres: +far.toFixed(1), scale: settings.original_animation.scale[0], overrides: Object.keys(character?.settings || {}).length });
}
// Zoe keeps the compiled seed; Mac (same scale and stance) sits on the same spot, Psymon elsewhere (his own countdown).
assert.deepEqual(gridSpots.get('mac'), gridSpots.get('zoe'));
assert.notDeepEqual(gridSpots.get('psymon'), gridSpots.get('zoe'));
// FE speech (1A0358): Post_Selection outside career, Customize always, with the base rider's voice; the banks exist.
for (const r of riders.filter((x) => x.kind !== 'custom')) for (const kind of ['select', 'customize']) {
  const bank = frontEndSpeechBank(kind, r.kind === 'cheat' ? { ...r, base: 'psymon' } : r);
  assert(bank && fs.existsSync(`public/assets/AUDIO/speech/${bank}.json`), `${r.id} ${kind}: ${bank}`);
}
assert.equal(frontEndSpeechBank('select', riders.find((r) => r.id === 'zoe'), { career: true }), null);
assert.equal(frontEndSpeechBank('select', riders.find((r) => r.id === 'sam')), null);
// Sam in the roster row: silhouettes from his own model (tools/export_sam_roster.py), in the originals' style and group.
{
  const sel = read('UI/character-select.json'), [x, y, w, h] = sel.sam.silhouette;
  assert.equal(h, 78, 'Sam: 5\'11" like Elise'); assert(Math.abs(y + 12 + h * 1.1 - 364.5) < 1, 'feet on the originals\' line');
  assert(x - 4 < 481 - 4 + 27 * 1.3, 'overlaps Kaori (inside the group)'); assert(sel.sam.right_arrow_x > x - 4 + w * 1.3, 'right arrow after him');
  assert(/export_sam_roster/.test(sel.sam.source), 'made from his model, not the Sam PS2 build mask');
}
console.log('test-characters: ok', selectable.length);
