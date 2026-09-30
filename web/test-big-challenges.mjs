// Big Challenges (docs/peak-mountain.md "Big Challenges"; web/mission_gameplay.inc, web/big-challenges.js):
//  1. the challenge table 0x43EE10 (tools/export_peak_missions.py): 88 rows, the Peak 1 chains, new-character status words
//     (151600), completion counts, the career save rows and the challenge clock format (1EB584);
//  2. the WScript mission state machine against PS2 Speed Demon captures (Snow Jam, challenge 21: 12 gates, 60 s timer,
//     "Count: n / 12"), made with tools/ps2_capture.py from derived savestates (local/ps2-capture/peak1/bc-sd-*.p2s):
//       bc-sd-offer: the arrival ride into the green offer volume (mdl_ARA1_bigchal_green_ext_04) -> the queued offer;
//       bc-sd-run:   Cross on the "63bc_start" prompt, then the run: every tick's stage-script context words C+0..+0x3C
//                    (count / timer HUD), C+0x2A0..+0x2B0, the mission object (+0 state, +8 task) and its 12 tasks.
//     The core replays the recorded rider (position +0x110, contact list +0x5B8) through the stage programs and must give
//     the same words on every tick. Skipped without the captures (git-ignored local/).
// CORE=path/to/core.js overrides the runtime (scratch builds).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { locationBatches } from './peak-world-batches.js';
import { initialStatus, setBigChallengeTable, bigChallengeCounts, bigChallengeInfo, statusWords, setStatusWord, challengeClock, heightText } from './big-challenges.js';
import { Career } from './career.js';

const root = new URL('public/assets/', import.meta.url);
const json = (p) => JSON.parse(fs.readFileSync(new URL(p, root)));
if (!fs.existsSync(new URL('BIGCHAL/big-challenges.json', root))) { console.log('Big Challenge table not exported (tools/export_peak_missions.py); skipped'); process.exit(0); }
const table = json('BIGCHAL/big-challenges.json');
const rows = table.challenges;

// ---- 1. table, status words, career save -------------------------------------------------------------------------------------
assert.equal(rows.length, 88);
const peak1 = rows.filter((r) => (r.flags & 0xff) === 1);
assert.equal(peak1.length, 40, 'Peak 1 has 40 Big Challenges');
const byId = new Map(rows.map((r) => [r.id >>> 0, r]));
for (const r of rows) {
  const first = !!(r.flags & 0x01000000), follow = !!(r.flags & 0x00010000);
  assert.ok(first !== follow, `row ${r.index}: first of a chain xor follow-on`);
  if (r.next !== 0xffffffff) { const n = byId.get(r.next >>> 0); assert.ok(n && (n.flags & 0x00010000), `row ${r.index}: next is a follow-on`); }
  assert.equal(initialStatus(r.flags), first ? 0x12 : 0x3, `row ${r.index}: 151600 status`);
}
const sd = rows[20];
assert.equal(sd.id >>> 0, 0x0472b867, 'row 20 is Speed Demon'); assert.equal(sd.course, 0); assert.equal(sd.misc >>> 16, 3, 'Speed Demon music type 3 (event 38)');
setBigChallengeTable(rows);
const mem = new Map(), storage = { getItem: (k) => mem.get(k) ?? null, setItem: (k, v) => mem.set(k, v), removeItem: (k) => mem.delete(k) };
const career = new Career(JSON.parse(fs.readFileSync(new URL('CAREER/career.json', root), 'utf8')), { storage });
assert.deepEqual(bigChallengeCounts(career, 'zoe', { peak: 1 }), { done: 0, count: 40 });
assert.equal(bigChallengeInfo(career, 'zoe', 0), '0 / 10', 'Snow Jam: 10 challenges');
assert.equal(bigChallengeInfo(career, 'zoe', 17), 'N/A', 'stations have none');
setStatusWord(career, 'zoe', 20, 0x0a);
const again = new Career(JSON.parse(fs.readFileSync(new URL('CAREER/career.json', root), 'utf8')), { storage });
assert.equal(statusWords(again, 'zoe')[20], 0x0a, 'status words persist in the career save');
assert.equal(bigChallengeInfo(again, 'zoe', 0), '1 / 10');
assert.equal(challengeClock(3000), '00:00:50.00'); assert.equal(challengeClock(2881), '00:00:48.01'); assert.equal(challengeClock(-5), '00:00:00.00');
assert.equal(heightText(2034.9), 'Height: 20.34m');
console.log('table: 88 rows, Peak 1 40 in chains, 151600 status words, career rows, clock format');

// ---- 1b. Called tricks and the trick event (PS2 ctm-parity/runs/dizzy, web/ctm-flow-ps2.json) --------------------------------
// Dizzy Spells (Happiness #9): builtin 83 packs the called trick into C+0x34 / +0x38 (the HUD's red "180" at the bottom), and a
// trick the human's score object names (118FF8 -> 309918, kind 3) is read back by builtin 78 (30AF08): the called one counts.
{
  const ps2 = JSON.parse(fs.readFileSync(new URL('./ctm-flow-ps2.json', import.meta.url)));
  const want = ps2.memory?.['dizzy-s200'];
  const createCore1 = (await import(process.env.CORE || './runtime/core.js')).default, k1 = await createCore1();
  if (want && k1._mission_called_trick && k1._mission_test_trick) {
    const put1 = (text) => { const b = Buffer.from(text + '\0'); const p = k1._malloc(b.length); k1.HEAPU8.set(b, p); return p; };
    const terrain = json('PEAK1/ABC1/terrain.json'), world = json('PEAK1/ABC1/world_collision.json'), rails = json('PEAK1/ABC1/rails.json');
    const hash = put1(terrain.source_sha256), worlds = locationBatches(terrain, world, rails).filter((x) => x.kind === 'world');
    k1._init_world_collision(put1(worlds[0].text), hash); k1._peak_world_begin(); k1._peak_world_reserve(65536, 4096);
    for (const b of worlds.slice(1)) { k1._peak_world_append(1); k1._init_world_collision(put1(b.text), hash); k1._peak_world_append(0); }
    k1._peak_world_commit();
    const tp1 = k1._malloc(4); new Int32Array(k1.HEAPU8.buffer, tp1, 1).set([6]); k1._mission_test_setup(4, tp1, 1);
    const trig1 = k1._malloc(256), step1 = () => { k1._mission_test_rider(0, 0, 0, trig1, 0); k1._mission_test_tick(); };
    const words = () => Array.from(new Int32Array(k1.HEAPU8.buffer, k1._mission_hud(), 24).slice(5, 21));
    const unsupported = () => new Int32Array(k1.HEAPU8.buffer, k1._mission_info(), 10)[6];
    for (let k = 0; k < 70; k++) step1();
    const dizzy = rows[8]; assert.equal(dizzy.id >>> 0, 0x0672b927, 'row 8 is Dizzy Spells');
    k1._mission_prompt(1, dizzy.id | 0); for (let k = 0; k < 5; k++) step1();
    const w = words();
    assert.deepEqual(w.slice(13, 16), want.hud.slice(13, 16), 'the called trick record (C+0x34..+0x3F) as on the PS2: 180');
    assert.deepEqual(w.slice(4, 7), want.hud.slice(4, 7), '"COUNT: 0 / 5"'); assert.equal(w[7], want.hud[7], 'the challenge clock runs');
    k1._mission_test_trick(0x1000, 0); step1(); step1();   // the called 180, as 11A8C8 packs it
    const w2 = words();
    assert.equal(w2[5], 1, 'a trick event with the called record counts (builtin 78 reads it back)');
    assert.ok(w2[14] !== w[14] || w2[15] !== w[15], 'the next trick is called');
    k1._mission_test_trick(0x1000 * 7, 0); step1(); step1();   // not the called one: no count
    assert.equal(words()[5], 1, 'another trick does not count');
    assert.equal(unsupported(), 0, 'no unsupported builtin on the way');
    console.log(`called tricks: Dizzy Spells calls {${w.slice(14, 16)}} (PS2 {${want.hud.slice(14, 16)}}), the called trick event counts 1 / 5, the next call {${w2.slice(14, 16)}}`);
  } else console.log('called tricks: core without mission_called_trick / the PS2 trace without dizzy; skipped');
}

// ---- 1c. Builtin 59: the Kick Doubt Grinder distance (pv bcSpeed; PS2 local/ps2-capture/ctm-parity/grinder, docs/ctm-parity.md) ----
// Grinder / Pepper Grinder / Meat Grinder (rows 71-73) add stage builtin 59 (0x3032C0: |rider+0x1E0| x G+0x14) to their distance while
// the rider is on a rail (122EE8 key 4); the VM stores int + float as int + cvt.w.s. The PS2 run c-grind (the Grinder accepted from a
// derived Kick Doubt free ride, stick up + left 700 frames): every record's distance word (the LUN table node at 0x54390C) grows by
// trunc(builtin 59 of that record's velocity) on each rail tick. The core's builtin must give the same increment on all of them.
{
  const createCore2 = (await import(process.env.CORE || './runtime/core.js')).default, k2 = await createCore2();
  const bin = new URL('../local/ps2-capture/ctm-parity/grinder/runs/c-grind', import.meta.url);
  if (!k2._mission_test_speed) console.log('builtin 59: core without mission_test_speed (web/build-core.sh); skipped');
  else if (!fs.existsSync(bin)) console.log('builtin 59: PS2 capture missing (local/ps2-capture/ctm-parity/grinder); skipped');
  else {
    const data = fs.readFileSync(bin), R = 16384, W = 10752, recs = [];
    for (let k = 0; k * R < data.length; k++) {
      const dv = new DataView(data.buffer, data.byteOffset + k * R, R);
      if (!dv.getUint32(0, true) && !dv.getUint32(4, true)) continue;
      recs.push({ tick: dv.getUint32(4, true), mm: dv.getInt32(16, true), vel: [0, 1, 2].map((i) => dv.getFloat32(32 + 0xE0 + 4 * i, true)), count: dv.getInt32(W + 20, true), dist: dv.getInt32(W + 64 + 16 + 12, true) });
    }
    const f = new Float32Array(1), u = new Uint32Array(f.buffer);
    let rail = 0, total = 0;
    for (let i = 1; i < recs.length; i++) {
      const a = recs[i - 1], b = recs[i], d = b.dist - a.dist;
      if (b.mm !== 4 && !d) continue;
      u[0] = k2._mission_test_speed(...b.vel);
      assert.equal(Math.trunc(f[0]), d, `tick ${b.tick}: distance +${d} (motion ${b.mm})`);
      assert.equal(b.count, Math.trunc(b.dist / 100), `tick ${b.tick}: COUNT = distance / 100`);
      rail++; total += d;
    }
    assert.ok(rail >= 200 && total >= 2000, `the capture grinds (${rail} ticks, ${total} cm)`);
    console.log(`builtin 59: ${rail} rail ticks of the PS2 Grinder run, the distance (${total} cm) and COUNT equal the core's builtin per tick`);
  }
}

// ---- 2. PS2 captures ------------------------------------------------------------------------------------------------------------
const repo = new URL('../', import.meta.url);
// Records: local/ps2-capture/runs/bigchal/NAME.bin (runcap2 incremental writer); layout: the capture state's manifest.
const capture = (name) => { const bin = new URL(`local/ps2-capture/runs/bigchal/${name}.bin`, repo), man = new URL(`local/ps2-capture/peak1/${name}.capture.capture.json`, repo); return fs.existsSync(bin) && fs.existsSync(man) && fs.statSync(bin).size ? { data: fs.readFileSync(bin), manifest: JSON.parse(fs.readFileSync(man)) } : null; };
const run = capture('bc-sd-run');
if (!run) { console.log('PS2 Big Challenge captures missing (local/ps2-capture/runs/bigchal); state machine replay skipped'); process.exit(0); }

// Record layout (tools/ps2_capture.py): 0 seq, 4 tick; rider +0x100.. at 32; watches from layout.watch_offset in build order.
function records({ data, manifest }) {
  const R = manifest.record, off = {}; let o = manifest.layout.watch_offset;
  for (const w of manifest.layout.watches) { off[w.address.toLowerCase()] = o; o += w.length; }
  const out = [];
  for (let k = 0; k * R < data.length; k++) {
    const r = data.subarray(k * R, (k + 1) * R), dv = new DataView(r.buffer, r.byteOffset, r.byteLength);
    const i32 = (at) => dv.getInt32(at, true), f32 = (at) => dv.getFloat32(at, true), w = (a) => off[a];
    const trig = []; for (let t = 0; t < 64; t++) { const v = dv.getUint32(w('0x14572f8') + 4 * t, true); if (v === 0xffffffff) break; trig.push(v); }
    const m = (x) => i32(w('0x586c00') + x), c = (x) => i32(w('0xad0370') + x), q = (x) => i32(w('0xad0534') + x);
    out.push({ tick: dv.getUint32(4, true), pos: [f32(32 + 0x10), f32(32 + 0x14), f32(32 + 0x18)], trig,
      hud: Array.from({ length: 16 }, (_, i) => c(4 * i)), active: dv.getInt32(w('0xad0600') + 0x10, true) >>> 0,
      pendingOp: dv.getInt32(w('0xad0600') + 0x1c, true), mstate: m(0), mtask: m(8), queue: [q(0), q(4)],
      tasks: off['0x157f020'] != null ? Array.from({ length: 12 }, (_, i) => i32(off['0x157f020'] + 0x44 * i)) : null });
  }
  return out;
}

const createCore = (await import(process.env.CORE || './runtime/core.js')).default;
const core = await createCore();
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e : core.getExceptionMessage?.(e) ?? e); process.exit(1); });
const put = (text) => { const b = Buffer.from(text + '\0'); const p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
// The ARA1 location's instances (matrices for the gate checks) in the streamed-world format (location PEAK1).
{
  const terrain = json('PEAK1/ARA1/terrain.json'), world = json('PEAK1/ARA1/world_collision.json'), rails = json('PEAK1/ARA1/rails.json');
  const hash = put(terrain.source_sha256), worlds = locationBatches(terrain, world, rails).filter((x) => x.kind === 'world');
  core._init_world_collision(put(worlds[0].text), hash); core._peak_world_begin(); core._peak_world_reserve(65536, 4096);
  for (const b of worlds.slice(1)) { core._peak_world_append(1); core._init_world_collision(put(b.text), hash); core._peak_world_append(0); }
  core._peak_world_commit();
}
const tracks = [3, 8, 9]; // A_ARA1, ARA1, ARA1_B (the Snow Jam row)
const tp = core._malloc(4 * tracks.length); new Int32Array(core.HEAPU8.buffer, tp, tracks.length).set(tracks);
core._mission_test_setup(4, tp, tracks.length);
const trigPtr = core._malloc(256);
const hud = () => { const v = new Int32Array(core.HEAPU8.buffer, core._mission_hud(), 24); return { active: v[0] >>> 0, state: v[2], task: v[3], words: Array.from(v.slice(5, 21)) }; };
const step = (r) => { new Uint32Array(core.HEAPU8.buffer, trigPtr, 64).set(r.trig); core._mission_test_rider(r.pos[0], r.pos[1], r.pos[2], trigPtr, r.trig.length); core._mission_test_tick(); };
const OFFER = (746 << 8) | 8;

// Offer: the recorded arrival ride (ticks 2836..3609). The rider's contact with the green offer volume is in the rider pass
// of the last record's tick (3609): the next tick's queue read (230890) opens the prompt and the game pauses before any
// further record. Every earlier tick: no offer queued, no challenge HUD words.
const offer = capture('bc-sd-offer');
if (offer) {
  const recs = records(offer);
  for (let k = 0; k < recs.length; k++) {
    step(recs[k]);
    assert.equal(new Int32Array(core.HEAPU8.buffer, core._mission_offer_peek(), 3)[0], 0, `tick ${recs[k].tick}: nothing queued`);
    assert.deepEqual(hud().words.slice(1, 16), recs[k].hud.slice(1, 16), `tick ${recs[k].tick}: context words`);
  }
  core._mission_test_contact(OFFER); // rider pass of tick 3609
  const q = new Int32Array(core.HEAPU8.buffer, core._mission_offer_peek(), 3);
  assert.equal(q[0], 8, 'offer type 8 queued'); assert.equal(q[1] >>> 0, 0x0472b867);
  console.log(`offer: ${recs.length} ticks (${recs[0].tick}..${recs[recs.length - 1].tick}) without an offer, then the green volume's contact queues {Speed Demon, 8}`);
} else {
  for (let k = 0; k < 70; k++) step({ trig: [], pos: [0, 0, 0] }); // missions built, the offer volume's contact guard (1 s) run down
  core._mission_test_contact(OFFER);
}
const id = core._mission_offer_pop() >>> 0;
assert.equal(id, 0x0472b867, 'the prompt pops the Speed Demon offer');
core._mission_prompt(1, id); // Cross on "Yes" (1F75A0 -> 30B540: pending op 1)

// Run: every record's context words, active challenge, mission state / task and task states.
// The rider's contact list (+0x5B8) is emptied by 120F20 before the record is written (provider exit), so the recorded
// lists are empty: the gate contacts are taken from the PS2 task transitions (task i advancing at WScript tick T means gate
// i was in the list of rider pass T-1). Everything else (the not-yet / missed distance test of 30BFC0, the timer, the count,
// the HUD words, the fail conditions of the tick program) comes from the core.
const GATES = [1492, 2484, 969, 1709, 1626, 2688, 956, 2879, 220, 1970, 3050, 1639].map((rid) => (rid << 8) | 8); // step programs 665..676, key1
const recs = records(run);
for (let k = 1; k < recs.length; k++) if (recs[k].mtask > recs[k - 1].mtask && recs[k - 1].mstate === 2) recs[k].trig = [GATES[recs[k - 1].mtask]];
let compared = 0, firstState2 = -1, lastCount = 0, end = recs.length;
for (let k = 0; k < recs.length; k++) {
  const r = recs[k];
  step(r);
  const h = hud(), v = r;
  if (v.mstate === 3 || v.mstate === 0 && k > 0) { end = k; break; }
  assert.deepEqual(h.words.slice(1, 16), v.hud.slice(1, 16), `tick ${r.tick} (record ${k}): context words C+4..+0x3C`);
  assert.equal(h.active, v.active, `tick ${r.tick}: C+0x2A0`);
  assert.equal(h.state, v.mstate, `tick ${r.tick}: mission state`);
  assert.equal(h.task, v.mtask, `tick ${r.tick}: task index`);
  if (firstState2 < 0 && v.mstate === 2) firstState2 = r.tick;
  lastCount = v.hud[5]; compared++;
}
// The capture stops at record 4278: the next tick's WScript posts the fail (type 9: the rider is past gate 2 = 1011, 30BFC0
// returns -1, the tick program runs 30A868(id, 9)) and the game pauses on overlay 0x1E before its record. The same savestate
// replayed with PINE polling (local/ps2-capture/menus/bigchal/sd-run-stop.p2s) gives that tick's rider (-163466, 13649, -249340)
// and the queue {id, 9}; the core must fail there, not earlier.
if (end === recs.length && recs.length && recs[recs.length - 1].tick === 4278) {
  assert.equal(new Int32Array(core.HEAPU8.buffer, core._mission_offer_peek(), 3)[0], 0, 'no fail before tick 4279');
  step({ trig: [], pos: [-163466, 13649, -249340] });
  const q = Array.from(new Int32Array(core.HEAPU8.buffer, core._mission_offer_peek(), 3));
  assert.deepEqual([q[0], q[1] >>> 0], [9, 0x0472b867], 'tick 4279: missed gate 2 -> fail queued (type 9)');
  const info = Array.from(new Int32Array(core.HEAPU8.buffer, core._mission_info(), 10));
  const st = new Uint32Array(core.HEAPU8.buffer, core._mission_status(), 88)[20];
  assert.equal(st & 4, 4, 'the failed bit (bit 2, 1540F0) is set');
  console.log('fail: tick 4279 queues {Speed Demon, 9} and 3074C0 sets the failed bit, as on the PS2');
}
console.log(`run: ${compared} ticks identical (${recs[0].tick}..${recs[compared - 1].tick}): count ${lastCount} / 12, timer ${recs[compared - 1].hud[8]}; ${end < recs.length ? `mission stops at record ${end}` : 'capture ends while running'}`);

// Success: derived PS2 state bc-sd-lastgate (from the closed-loop run's savestate at the timer's end, count 8: poked to task 11,
// count 11, the rider in mdl_ARA1_bcvolume_1020 = the 12th gate). Neutral pad; the rider settles into the volume and the
// last gate completes the challenge at record 142 (tick 7376): 307240 stop + 307308 (status word 0x12 -> 0x1A: completed,
// cash 10F2D8 1511B0(peak 1) = $2,000), C+0x2A0 = -1. Watches: the status word (character block +0x118 + 4*20) and cash (+0xAC4).
const success = capture('bc-sd-success');
if (success) {
  const man = success.manifest, R = man.record; let o = man.layout.watch_offset; const off = {};
  for (const w of man.layout.watches) { off[w.address] = o; o += w.length; }
  const recs2 = records(success).map((r, k) => { const dv = new DataView(success.data.buffer, success.data.byteOffset + k * R, R); return { ...r, status: dv.getInt32(off['0x4aac30'], true), cash: dv.getInt32(off['0x4ab58c'], true) }; });
  const c2 = await createCore(); // a fresh core: the offer, the start, then the poked state
  const put2 = (text) => { const b = Buffer.from(text + '\0'); const p = c2._malloc(b.length); c2.HEAPU8.set(b, p); return p; };
  { const terrain = json('PEAK1/ARA1/terrain.json'), world = json('PEAK1/ARA1/world_collision.json'), rails = json('PEAK1/ARA1/rails.json');
    const hash = put2(terrain.source_sha256), worlds = locationBatches(terrain, world, rails).filter((x) => x.kind === 'world');
    c2._init_world_collision(put2(worlds[0].text), hash); c2._peak_world_begin(); c2._peak_world_reserve(65536, 4096);
    for (const b of worlds.slice(1)) { c2._peak_world_append(1); c2._init_world_collision(put2(b.text), hash); c2._peak_world_append(0); }
    c2._peak_world_commit(); }
  const tp2 = c2._malloc(12); new Int32Array(c2.HEAPU8.buffer, tp2, 3).set(tracks); c2._mission_test_setup(4, tp2, 3);
  const trig2 = c2._malloc(256);
  const step2 = (r) => { new Uint32Array(c2.HEAPU8.buffer, trig2, 64).set(r.trig); c2._mission_test_rider(r.pos[0], r.pos[1], r.pos[2], trig2, r.trig.length); c2._mission_test_tick(); };
  const hud2 = () => { const v = new Int32Array(c2.HEAPU8.buffer, c2._mission_hud(), 24); return { active: v[0] >>> 0, state: v[2], task: v[3], words: Array.from(v.slice(5, 21)) }; };
  for (let k = 0; k < 70; k++) step2({ trig: [], pos: recs2[0].pos });
  c2._mission_test_contact(OFFER); c2._mission_prompt(1, c2._mission_offer_pop());
  step2({ trig: [], pos: recs2[0].pos }); c2._mission_ui_events();
  c2._mission_test_set(11, 0x07a6c5b1, 11); c2._mission_test_set(-1, 0x08b03c93, recs2[0].hud[8] + 1);
  const done = recs2.findIndex((r) => r.mstate === 3);
  assert.ok(done > 0, 'the success capture completes');
  recs2[done].trig = [GATES[11]];
  for (let k = 0; k <= done; k++) {
    step2(recs2[k]); const h = hud2(), v = recs2[k];
    if (k > 0 && k < done) { assert.deepEqual(h.words.slice(1, 16), v.hud.slice(1, 16), `success tick ${v.tick}: context words`); assert.equal(h.task, v.mtask); } // record 0: the words of the pre-poke tick
  }
  const ev = (() => { const p = c2._mission_ui_events() >> 2, H = new Int32Array(c2.HEAPU8.buffer); return Array.from(H.slice(p + 1, p + 1 + 3 * H[p])); })();
  const status = new Uint32Array(c2.HEAPU8.buffer, c2._mission_status(), 88)[20];
  assert.equal(hud2().active, 0xffffffff, 'C+0x2A0 = -1 after 307308');
  assert.equal(status, recs2[done].status >>> 0, `status word ${recs2[done].status.toString(16)}`);
  const cashEv = []; for (let k = 0; k < ev.length; k += 3) if (ev[k] === 6) cashEv.push(ev[k + 1]);
  assert.deepEqual(cashEv, [recs2[done].cash - recs2[done - 1].cash], 'cash award (10F2D8)');
  console.log(`success: ${done - 1} ticks identical, completed at tick ${recs2[done].tick}: status 0x${recs2[done - 1].status.toString(16)} -> 0x${status.toString(16)}, cash +$${cashEv[0]}`);
}

// pv bcDecline (core mission_lifecycle): builtin 67's 30B7F8 (0x3021B8: an event gate ends a running challenge) and world state
// 10 enter's 309030 -> 308988 (0x2356A8, the tick after a Load trigger: C+0x2A0 = -1, the HUD words, the offer ring cleared).
{
  const c3 = await createCore();
  if (c3._mission_lifecycle && c3._mission_test_gate && c3._mission_test_load_trigger) {
    const put3 = (text) => { const b = Buffer.from(text + '\0'); const p = c3._malloc(b.length); c3.HEAPU8.set(b, p); return p; };
    const terrain = json('PEAK1/ARA1/terrain.json'), world = json('PEAK1/ARA1/world_collision.json'), rails = json('PEAK1/ARA1/rails.json');
    const hash = put3(terrain.source_sha256), worlds = locationBatches(terrain, world, rails).filter((x) => x.kind === 'world');
    c3._init_world_collision(put3(worlds[0].text), hash); c3._peak_world_begin(); c3._peak_world_reserve(65536, 4096);
    for (const b of worlds.slice(1)) { c3._peak_world_append(1); c3._init_world_collision(put3(b.text), hash); c3._peak_world_append(0); }
    c3._peak_world_commit();
    const tp3 = c3._malloc(12); new Int32Array(c3.HEAPU8.buffer, tp3, 3).set(tracks); c3._mission_test_setup(4, tp3, 3);
    const trig3 = c3._malloc(256), step3 = () => { c3._mission_test_rider(0, 0, 0, trig3, 0); c3._mission_test_tick(); };
    const active = () => new Int32Array(c3.HEAPU8.buffer, c3._mission_hud(), 24)[0] >>> 0, running = () => c3._mission_running();
    const start = () => { c3._mission_test_contact(OFFER); step3(); c3._mission_prompt(1, c3._mission_offer_pop()); for (let k = 0; k < 3; k++) step3(); };
    for (let k = 0; k < 70; k++) step3();
    start(); assert.equal(running(), 1, 'Speed Demon runs');
    c3._mission_lifecycle(0); c3._mission_test_gate(); step3();
    assert.equal(running(), 1, 'switch off: a gate leaves the challenge running (the old port)');
    c3._mission_lifecycle(2); c3._mission_test_gate();
    assert.equal(running(), 0, 'builtin 67: 30B7F8 stops the running challenge'); assert.equal(active(), 0xffffffff, '30B7F8: C+0x2A0 = -1');
    step3(); c3._mission_prompt(1, 0x0472b867 | 0); for (let k = 0; k < 3; k++) step3(); assert.equal(running(), 1, 'Speed Demon runs again (the prompt\'s Yes)');
    c3._mission_lifecycle(4); c3._mission_test_load_trigger(); step3();
    assert.equal(active(), 0x0472b867, 'the Load trigger tick itself keeps the challenge (world state 10 enters on the next tick)');
    step3();
    const words = Array.from(new Int32Array(c3.HEAPU8.buffer, c3._mission_hud(), 24).slice(5, 21));
    assert.equal(active(), 0xffffffff, '309030 / 308988: C+0x2A0 = -1'); assert.equal(running(), 0, 'no challenge runs after world state 10');
    assert.equal(words[1] | words[4] | words[7], 0, '308988: the goal / count / timer HUD flags cleared');
    assert.equal(new Int32Array(c3.HEAPU8.buffer, c3._mission_info(), 10)[9], 0, '30C760: the offer ring is empty');
    console.log('lifecycle: an event gate (builtin 67) ends the challenge; world state 10 enter clears C+0x2A0, the HUD words and the offers');
  } else console.log('lifecycle: core without mission_lifecycle (rebuild web/runtime)');
}

// World state 15 (236058: Session / a Transport to here / the post-event Transport to the same course; web/free-ride.js
// placeRegion -> core mission_world_session): 30B7F8 before the placement (every mission inactive, the running challenge
// stopped, the offers cleared), then 11DF18(rider, 1) -> 3099F8 posts WScript event kind 5 (docs/ctm-parity.md).
if (core._mission_world_session) {
  const info = () => Array.from(new Int32Array(core.HEAPU8.buffer, core._mission_info(), 10)); // [missions, inactive, active, pending, ticks, programs, unsupported, op, events, offers]
  const before = info(), running = hud().active !== 0xffffffff;
  core._mission_world_session(0);
  const after = info();
  assert.equal(after[2], 0, '30B7F8: no mission left in the active list'); assert.equal(after[1], before[1] + before[2], 'all back in the inactive list');
  assert.equal(after[7], 0, 'no pending op'); assert.equal(after[9], 0, 'no offer queued'); assert.equal(hud().active, 0xffffffff, 'C+0x2A0 = -1');
  core._mission_world_session(1);
  assert.equal(info()[8], after[8] + 1, '11DF18(rider, 1): WScript event kind 5 queued');
  console.log(`world state 15: ${running ? 'the running challenge stopped, ' : ''}${before[2]} active -> 0, the reset event queued`);
} else console.log('world state 15: core without mission_world_session (rebuild web/runtime)');
