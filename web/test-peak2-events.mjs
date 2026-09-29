// Peak 2 events (docs/peak2.md): the six locations' prepared data against what the PS2 savestates hold, and every course's
// stage world / sections / flags loading in the production core. Physics / score / AI exactness is gated by
// test-ps2-captures.mjs (peak2/*); loading and the event start of every course by test-locations.mjs.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';

const root = new URL('public/assets/', import.meta.url);
const has = (p) => fs.existsSync(new URL(p, root));
const json = (p) => JSON.parse(fs.readFileSync(new URL(p, root), 'utf8'));
const PEAK2 = { CRA3: 'race', DRA4: 'race', DSS2: 'slopestyle', CBA2: 'bigair', CHP2: 'superpipe', DBC2: 'backcountry' };

// courses.json: every Peak 2 location prepared, with its compiled event start (grid countdown or rolling start)
const courses = json('courses.json').courses;
for (const [code, event] of Object.entries(PEAK2)) {
  const c = courses.find((x) => x.code === code);
  assert(c, `${code} missing from courses.json (tools/prepare_location.py ${code})`);
  assert.equal(c.peak, 2); assert.equal(c.event, event); assert.equal(c.eventStart, true, `${code}: no compiled event start`);
}

// Freestyle configuration from the countdown anchors (tools/export_freestyle_event.py): Style Mile's +90 s / +60 s checkpoints
// and 75 s limit, Launch Time 60 s, Schizophrenia 120 s; handler 0 with the game mode as the kind.
const fe = (code) => json(`${code}/freestyle-event.json`);
{ const s = fe('DSS2');
  assert.deepEqual([s.game_mode, s.handler, s.freestyle_kind, s.posted_riders, s.time_limit_ticks, s.event_riders], [1, 0, 1, 4, 4500, 2]);
  assert.deepEqual(s.bonus.filter((b) => b.value).map((b) => b.value), [90, 60]);
  assert.deepEqual([fe('CBA2').game_mode, fe('CBA2').time_limit_ticks, fe('CBA2').posted_riders], [3, 3600, 5]);
  assert.deepEqual([fe('CHP2').game_mode, fe('CHP2').time_limit_ticks, fe('CHP2').posted_riders], [2, 7200, 5]); }

// Computer riders: the race anchors' five riders and Style Mile's opponent at stat level 4 (attribute bank 2, raw 20: the
// stat getters 0x1494C0..), Peak 1 riders at raw 5.
for (const code of ['CRA3', 'DRA4', 'DSS2']) {
  const d = json(`${code}/npc-riders.json`);
  assert.equal(d.riders.length, code === 'DSS2' ? 1 : 5, code);
  for (const r of d.riders) { assert.deepEqual(r.attributes.raw, [20, 20, 20, 20, 20, 20, 20], `${code} ${r.character}`); assert.equal(r.attributes.bank, 2);
    assert.equal(Math.fround(r.ground.profile.top_speed_stat), Math.fround(4 / 11)); }
}
// Ruthless rival documents: Nate, or Zoe when the player is Nate (0x145750 peak 1), Rival Time (mode 4) and Rival Points (5).
{ const d = json('DBC2/rivals.json').documents;
  for (const [mode, gm] of [['race', 4], ['jam', 5]]) for (const [rival, human] of [['nate', 'zoe'], ['zoe', 'nate']]) {
    const doc = d[mode]?.[rival]; assert(doc, `DBC2 rivals ${mode}/${rival}`);
    assert.equal(doc.game_mode.mode, gm); assert.equal(doc.game_mode.course, 15); assert.equal(doc.human, human); assert.equal(doc.rolling_start, true);
    assert.deepEqual(doc.riders[0].attributes.raw, [20, 20, 20, 20, 20, 20, 20]); } }

// Stage world, sections, flags: each course's packages load in the production core like set-pieces-renderer.js does.
for (const code of Object.keys(PEAK2)) {
  for (const f of ['PARTICLES/particles.json', 'LIVECOMP/livecomp.json', 'STAGE/stage-world.json', 'FLAGS/flags.json', 'UVSCROLL/uv-scroll.json',
    'SECTIONS/sections.json', 'SECTIONS/ready-state.json', ...(['bigair', 'superpipe'].includes(PEAK2[code]) ? [] : ['progress-meter.json'])])
    assert(has(`${code}/${f}`), `${code}: missing ${f}`);
  const course = courses.find((x) => x.code === code), dir = course.root.replace(/^\/assets\//, '');
  const core = await createCore();
  const read = (p) => fs.readFileSync(new URL(p, root)); const put = (b) => { const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const text = (t) => put(Buffer.from(t + '\0')); const file = (p) => text(read(dir + p).toString());
  const call = (what, f) => { try { return f(); } catch (e) { throw new Error(`${code} ${what}: ${core.getExceptionMessage ? core.getExceptionMessage(e).join(': ') : e}`); } };
  core._init_animation(file('initial.json'), put(Buffer.concat([read('RIDER_ZOE/rider.json'), Buffer.from([0])])), file('initial.json'), put(read('ANIMATIONS/animation-packets.bin')), read('ANIMATIONS/animation-packets.bin').length);
  core._init_race(file('initial.json')); const mesh = read(dir + 'collision.bin'); core._init_world(put(mesh), mesh.length / 4);
  core._init_terrain(file('terrain.json')); call('world collision', () => core._init_world_collision(file('world_collision.json'), text(JSON.parse(read(dir + 'terrain.json')).source_sha256)));
  const sections = call('sections', () => core._init_sections(file('SECTIONS/sections.json')));
  const stage = call('stage world', () => core._init_stage_world(file('PARTICLES/particles.json'), file('LIVECOMP/livecomp.json'), file('STAGE/stage-world.json')));
  call('flags', () => core._init_stage_flags(file('FLAGS/flags.json'), file('SECTIONS/ready-state.json')));
  assert(sections > 0 && stage > 0, `${code}: sections ${sections}, stage world ${stage}`);
  console.log(`${code}: ${sections} section instances, stage world ${stage}`);
}

// Single Event: Peak 2 opens and its Freestyle rows follow the Map LUI (Style Mile, Launch Time, Schizophrenia).
{ globalThis.performance ??= { now: () => 0 };
  const { peakUnlocked, mapRowOrder } = await import('./fe-event-select.js');
  assert.equal(peakUnlocked(2), true);
  if (has('UI/fe-menus.json')) { const map = json('UI/fe-menus.json').screens.Map;
    assert.deepEqual(mapRowOrder(map, 2, 'race'), ['CRA3', 'DRA4', 'DBC2', 'peak2-race']);
    assert.deepEqual(mapRowOrder(map, 2, 'freestyle'), ['DSS2', 'CBA2', 'CHP2', 'DBC2', 'peak2-jam']); } }
console.log('Peak 2 events: courses, freestyle configuration, computer-rider stats, rival documents, stage worlds and the selector OK');
