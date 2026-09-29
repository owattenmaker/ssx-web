// Conquer the Mountain collectibles (stage builtin38 -> 0x30C4A8; web/stage_script_gameplay.inc set_stage_collect_state):
// a single event (0x535C11 = 1) makes every listed collectible a DeadNode (the baked event-start state, unchanged
// physics); a career race (0) keeps the uncollected ones at their authored flags without entity (drawn, static route,
// section slot-1 program pending) and only the career-save bits dead. Plus the career save's collect row
// (web/career.js collectMask / markCollected) and web/stage-collect.js.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
import {Career} from './career.js';
import {collectStart, collectPoll} from './stage-collect.js';

const c = await createCore(), root = 'public/assets/';
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e : c.getExceptionMessage(e)); process.exit(1); });
const read = (p) => JSON.parse(fs.readFileSync(root + p));
const put = (b) => { const p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
const str = (x) => put(new TextEncoder().encode((typeof x === 'string' ? x : JSON.stringify(x)) + '\0'));
const metadata = str(read('ANIMATIONS/animation-packets.json')), rig = str(read('RIDER_SAM/rider.json')), settings = str(read('ANIMATIONS/initial.json'));
const raw = fs.readFileSync(root + 'ANIMATIONS/animation-packets.bin'), packets = put(raw);
c._init_animation(metadata, rig, settings, packets, raw.length);
const mesh = fs.readFileSync(root + 'ARA1/collision.bin'), meshPtr = put(mesh); c._init_world(meshPtr, mesh.length / 4); c._free(meshPtr);
const terrain = str(read('ARA1/terrain.json')); c._init_terrain(terrain);
const wp = str(read('ARA1/world_collision.json')), hp = put(new TextEncoder().encode(read('ARA1/terrain.json').source_sha256 + '\0')); c._init_world_collision(wp, hp);
c._animation_use_physics(1); const bt = str(read('ARA1/terrain.json')); c._init_body_terrain(bt); const rc = str(read('ANIMATIONS/initial.json')); c._init_race(rc);
const sections = str(fs.readFileSync(root + 'ARA1/SECTIONS/sections.json', 'utf8')); assert.ok(c._init_sections(sections) > 0);

const list = read('ARA1/STAGE/stage-world.json').collections['0'];
assert.equal(list.length, 30, 'Snow Jam lists 30 collectibles');
list.forEach((x, i) => assert.equal(x.index, i));
assert.ok(list.every((x) => /collecta_10\d\d$/.test(x.name) && Number(x.name.slice(-4)) - 1000 === x.index), 'list index = name suffix - 1000');
const U = () => new Uint32Array(c.HEAPU8.buffer);
const instances = () => { const at = c._stage_world_instances() >> 2, u = U(), n = u[at], out = new Map(); for (let k = 0; k < n; k++) out.set(u[at + 1 + 4 * k], {drawn: u[at + 2 + 4 * k], node: u[at + 3 + 4 * k], flags: u[at + 4 + 4 * k]}); return out; };
const skips = () => Array.from(new Int32Array(c.HEAPU8.buffer, c._world_collision_info(), 11)).slice(8);

// Single event (default path 1): every collectible dead, the event-start skip counts unchanged.
c._start_event();
const base = skips(); // the event-start dead nodes + type-16 nodes (test-event-start.mjs)
assert.ok(base[0] > 30 && base[0] === base[1] && base[1] === base[2], 'single event: dead nodes skip every collector');
let st = instances();
for (const x of list) { const s = st.get(x.resource); assert.ok(s && s.drawn === 0 && s.node === 6, `${x.name} is a DeadNode in a single event`); assert.equal(c._section_listed(x.resource) >> 4, 2, `${x.name}: section entity Dead`); }

// Career race, bits 0 and 2 collected: those two dead, the other 28 drawn at the authored flags (0x210023) without entity.
c._set_stage_collect_state(0, 0b101, 0); c._start_event();
st = instances();
for (const x of list) {
  const s = st.get(x.resource), dead = x.index === 0 || x.index === 2;
  if (dead) assert.ok(s.drawn === 0 && s.node === 6, `${x.name} (collected) stays dead`);
  else { assert.ok(s.drawn === 1 && s.node === 0 && s.flags === 0x210023, `${x.name} live in a career race: ${JSON.stringify(s)}`); assert.equal(c._section_listed(x.resource) >> 4, 0, `${x.name}: no entity until its section`); }
}
assert.deepEqual(skips(), base.map((n) => n - 28), 'career: the 28 uncollected collectibles take the static route');

// Back to a single event: dead again.
c._set_stage_collect_state(1, 0, 0); c._start_event();
assert.deepEqual(skips(), base);

// Career save rows (web/career.js) and the host glue (web/stage-collect.js).
const mem = new Map(), storage = {getItem: (k) => mem.get(k) ?? null, setItem: (k, v) => mem.set(k, v), removeItem: (k) => mem.delete(k)};
const career = new Career(JSON.parse(fs.readFileSync(root + 'CAREER/career.json', 'utf8')), {storage});
assert.deepEqual(career.collectMask('zoe', 0), [0, 0]);
career.markCollected('zoe', 0, 3, 500); career.markCollected('zoe', 1, 34, 500);
assert.deepEqual(career.collectMask('zoe', 0), [8, 0]); assert.deepEqual(career.collectMask('zoe', 1), [0, 4]);
assert.equal(career.rider('zoe').cash, 1000); assert.equal(career.collectCount('zoe', 1), 1);
const again = new Career(JSON.parse(fs.readFileSync(root + 'CAREER/career.json', 'utf8')), {storage});
assert.deepEqual(again.collectMask('zoe', 1), [0, 4], 'the collect rows persist in the career save');
collectStart(c, {careerMode: true, career: again, riderId: 'zoe', courseCode: 'ARA1'}); c._start_event();
st = instances(); assert.equal(st.get(list[3].resource).node, 6); assert.equal(st.get(list[4].resource).drawn, 1);
assert.equal(collectPoll(c, {careerMode: true, career: again, riderId: 'zoe', courseCode: 'ARA1'}), 0);
collectStart(c, {careerMode: false, career: again, riderId: 'zoe', courseCode: 'ARA1'}); c._start_event();
assert.deepEqual(skips(), base);
console.log('stage collectibles: single event 30/30 dead, career race 28 live + 2 collected dead, career save rows persist');
