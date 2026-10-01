// Conquer the Mountain leftovers against the PS2 (docs/ctm-parity.md "Collectibles and Big Challenges", docs/peak3.md section 6):
//  1. free-ride cash: 119EF8 -> 1597B0 -> 159818 in the career (web/career.js earnCash): the cash, the lifetime earnings and an
//     earnings goal completed at once (award 14 + peak - 1 with the next peak's pass); the core's score cash queue
//     (web/stage-collect.js scorePoll); the capture gate peak2/fr-d-glide checks the core side (2371: a 690-point trick = $1);
//  2. records: every heat's time or score enters the top 5 under "PLAYER 1" (0x534FE0), and a new record opens "Top 5 Record
//     Times" before the rewards / results in a standard event too (20A8F8(7); PS2 local/ps2-capture/ctm-left/runs final-top
//     and qual-top: the Snow Jam final at 03:55 and the qualifier at 03:45 with the records poked slower);
//  4. the big HUD messages (web/trick-hud.js): "GO!" when a Big Challenge starts (score slot 0x1A, 11A0E0), "MISSION SUCCESS"
//     with the cash (slot 0x1B), their grow / fade; the CTM combo cash popup 0x30;
//  5. world state 15 (Session): which placements post the WScript reset event (web/free-ride.js sessionPlacement).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { Career, MODE, PLAYER_NAME, recordSlot } from './career.js';
import { CareerScreens } from './career-ui.js';
import { scorePoll } from './stage-collect.js';
import { TrickHud } from './trick-hud.js';
import { sessionPlacement } from './free-ride.js';

globalThis.location ??= { href: 'http://localhost/' };
const data = JSON.parse(fs.readFileSync(new URL('./public/assets/CAREER/career.json', import.meta.url)));
class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }
const tick = () => new Promise((r) => setTimeout(r, 0));

// ---- 1. the cash of an award in the world -------------------------------------------------------------------------------------
{
  const c = new Career(data, { storage: new Memory() }), id = 'zoe', r = c.rider(id), goal = data.rules.earnings_goal[0];
  assert.deepEqual(c.earnCash(id, 1), [], 'a $1 trick: no goal'); assert.equal(r.cash, 1); assert.equal(r.earned, 1);
  assert.ok(!r.peaks[1], 'Peak 2 still locked');
  r.earned = goal - 3; r.cash = 500;
  const got = c.earnCash(id, 20);
  assert.equal(r.cash, 520); assert.equal(r.earned, goal + 17);
  assert.ok(r.awards.includes(14), '159818: the peak 1 earnings goal completes at once (159170 -> award 5 + 3 * 3)');
  assert.ok(got.length > 0 && r.peaks[1], 'its items and the Peak 2 pass');
  const again = c.earnCash(id, 5); assert.deepEqual(again, [], 'the next threshold is peak 2\'s goal');
  // a collectible's cash goes the same way (30B9A0: 153B00, then 10F338 -> 119EF8 kind 3)
  const before = r.cash; c.markCollected(id, 14, 3, 500); assert.equal(r.cash, before + 500);
  // the core's score cash queue (score_career_events: [count, (kind, amount)...]) -> the career
  const buf = new ArrayBuffer(64), H = new Int32Array(buf); H.set([3, 0, 1, 2, 20, 1, 4], 4);
  const core = { HEAPU8: new Uint8Array(buf), _score_career_events: () => 16 };
  const cash = r.cash; assert.equal(scorePoll(core, { career: c, riderId: id }), 3); assert.equal(r.cash, cash + 25, 'kinds 0 / 2 / 1: $1 + $20 + $4');
  console.log(`free-ride cash: earnCash adds cash and earnings, the peak 1 earnings goal (${goal}) completes at once with the pass; the score queue pays $25`);
}

// ---- 2. records --------------------------------------------------------------------------------------------------------------
const field = (me) => [{ human: true, finishTicks: me, remaining: 0, place: 0, origin: 1 }, ...[3, 8, 2, 1, 6].map((c, i) => ({ character: c, finishTicks: null, remaining: 50000, place: i + 1, origin: 600000, dnf: true }))];
const pokeSlow = (c, slot) => { c.save.records[slot].forEach((e, k) => { e.value = 900 + k; delete e.ticks; }); };   // the PS2 runs' poke: 15:00..15:04
{
  const c = new Career(data, { storage: new Memory() }), id = 'zoe', slot = recordSlot(data.rules, MODE.RACE, 0);
  assert.equal(slot, 12, 'Snow Jam race records');
  pokeSlow(c, slot); c.startEvent(id, MODE.RACE, 0, true);
  const q = c.raceResult(field(225 * 60), { raceTicks: 225 * 60, origin: 0 });
  assert.equal(q.record, 0, 'a qualifier heat enters the top 5 (PS2 qual-top: 03:45 first)');
  const rows = c.records(slot, true);
  assert.equal(rows[0].name, PLAYER_NAME); assert.equal(PLAYER_NAME, 'PLAYER 1'); assert.equal(rows[0].ticks, 225 * 60); assert.ok(rows[0].player);
  c.active.ev.round = 3;
  const f = c.raceResult(field(235 * 60), { raceTicks: 235 * 60, origin: 0 });
  assert.equal(f.record, 1, 'the final heat too (03:55 behind the qualifier\'s 03:45)');
  assert.equal(c.records(slot, true).filter((x) => x.player).length, 2, 'once per heat (the event completion adds none of its own)');
  // saves from before named the player's entries 'YOU'
  c.save.records[slot][0].name = 'YOU'; assert.equal(c.records(slot, true)[0].name, 'PLAYER 1');
  // a DNF enters nothing
  const d = field(0); d[0].dnf = true; d[0].finishTicks = null; c.active.ev.round = 1;
  assert.equal(c.raceResult(d, { raceTicks: 0, origin: 0 }).record, -1);
  console.log('records: every heat under PLAYER 1, once, not for a DNF');
}
{ // 0x154D58 on whole seconds (0x154CD8): 02:57.50 ties BOMBER's 177 s and goes above it; 02:58 goes below; a tie with the 5th enters
  const c = new Career(data, { storage: new Memory() }), slot = recordSlot(data.rules, MODE.RACE, 0);
  c.startEvent('zoe', MODE.RACE, 0, true);
  assert.equal(c.heatRecord(177 * 60 + 30), 0, 'a tie in seconds ranks the new run first');
  assert.equal(c.records(slot, true)[1].name, 'BOMBER');
  assert.equal(c.heatRecord(178 * 60), 2);
  assert.equal(c.heatRecord(191 * 60 + 59), 4, '03:11.98 = 191 s ties SIMON (now 5th) and goes above him');
  assert.equal(c.heatRecord(192 * 60), -1, 'slower than the 5th');
  console.log('records: whole seconds, a tie goes above (0x154D58)');
}
// the results flow: a new record opens Top 5 Record Times first, then the rewards / results (20A8F8 overlay 0xF)
async function flow(round, ticks) {
  const log = [], ui = {
    screen: 'game', index: 0, ready: true, careerMode: true, log, riders: [{ id: 'zoe', name: 'Zoe' }], rider: { id: 'zoe', name: 'Zoe', kind: 'rider' },
    courses: data.courses.map((c) => ({ code: c.code, ready: true })), loading: {}, cutscene: { fadeFrom() {}, drawCover() {}, preFade: async () => {}, clearOverlay() {} },
    set(s) { this.screen = s; this.index = 0; log.push(s); }, sync() {}, items() { return cs.owns(this.screen) ? cs.items(this.screen) : []; }, rememberRider() {},
    cb: { freeRide() { return false; }, start() {}, resume() {}, quit() {}, cutscene: () => Promise.resolve({ played: true }), freeRideRespawn() {}, attributes() {}, timeLimit() {}, standings: null, lineup: () => [] },
  };
  const cs = new CareerScreens(ui); ui.careerUI = cs; cs.data = data; cs.career = new Career(data, { storage: new Memory() }); cs.loc = new (cs.loc.constructor)(data.strings);
  cs.enter(); cs.resume(); await tick(); await tick();
  pokeSlow(cs.career, 12); cs.freeRide = { course: 0 }; cs.begin(MODE.RACE, 0, true); cs.career.active.ev.round = round;
  cs.finish({ ticks, raceTicks: ticks, standings: field(ticks) }); await tick(); await tick();
  const first = ui.screen; assert.deepEqual(cs.items('ctm-records'), ['Continue', 'Save Records']);
  cs.choose(0); return [first, ui.screen];
}
assert.deepEqual(await flow(1, 225 * 60), ['ctm-records', 'ctm-results'], 'qualifier: Top 5 Record Times, then the Qualifier Results (PS2 qual-top)');
assert.deepEqual(await flow(3, 235 * 60), ['ctm-records', 'ctm-award'], 'final: Top 5 Record Times, then the Rewards (PS2 final-top), then the results');
console.log('results flow: a top time opens the records first in the standard events (qualifier -> results, final -> rewards)');

// ---- 4. the big HUD messages and the CTM combo cash popup -------------------------------------------------------------------
{
  const trick = JSON.parse(fs.readFileSync(new URL('./public/assets/UI/trick-hud.json', import.meta.url)));
  const glyphs = { FEFONT: JSON.parse(fs.readFileSync(new URL('./public/assets/UI/FEFONT-glyphs.json', import.meta.url))), HUDFONT: JSON.parse(fs.readFileSync(new URL('./public/assets/UI/HUDFONT-glyphs.json', import.meta.url))) };
  const go = { owner_offset: '0x434', handle: 1622, page: 'OV_1-4', uv: [0.005859375, 0.537109375, 0.990234375, 0.173828125] };   // tools/probe_trick_hud.py
  const hud = new TrickHud({ ...trick, sprites: { ...trick.sprites, go: trick.sprites.go ?? go }, strings: { missionSuccess: 'MISSION SUCCESS', ...trick.strings } }, glyphs);
  const FREE = 0x34, bank = () => Array.from({ length: 44 }, () => ({ type: FREE, maximum: 0, value: 0, arg: 0, points: 0 }));
  const one = Math.fround(1 / 60);
  // the bank read before the tick after the post carries one slot clock (the port runs the WScript tick before the rider pass)
  let b = bank(); b[0x1A] = { type: 0x1A, maximum: 1, value: one, arg: 0, points: 0 }; hud.prepassSlots = b;
  assert.equal(hud.big.id, 8, 'slot 0x1A just posted -> message 8'); assert.equal(hud.big.t, 0);
  let s = hud.frame(bank(), { flags: 0x1530C380 }).find((d) => d.kind === 'sprite' && d.sprite === hud.sprites.go);
  assert.ok(s, 'the GO! sprite'); assert.deepEqual(s.size.map((x) => +x.toFixed(3)), [65.6, 25.6], 'descriptor 8 (164 x 64) x 0.4');
  assert.equal(+s.x.toFixed(3), 287.2, 'centred on x 320'); assert.equal(s.y, 120); assert.ok(s.argb[0] >= 1);
  b = bank(); b[0x1A] = { type: 0x1A, maximum: 1, value: Math.fround(2 / 60), arg: 0, points: 0 }; hud.prepassSlots = b;
  assert.equal(hud.big.id, 8); assert.equal(hud.big.t, one, 'no restart for the same post');
  let n = 1; while (hud.big.t < 1 && n < 80) { hud.prepassSlots = bank(); n++; }
  assert.equal(hud.big.t, 1, 'clamped at 1'); assert.ok(n >= 60 && n <= 61, `about 60 updates (${n}: the chopped 1/60 sum)`);
  s = hud.frame(bank(), {}).find((d) => d.kind === 'sprite' && d.sprite === hud.sprites.go);
  assert.deepEqual(s.size, [164, 64]); assert.ok(Math.abs(s.argb[0] - 1 / 3) < 1e-6, `alpha 1 - (1 - 0.6) x 1.6666666 at the end (${s.argb[0]})`);
  hud.prepassSlots = bank(); assert.equal(hud.big.id, 0x50, 'then gone');
  // MISSION SUCCESS with the cash (slot 0x1B)
  b = bank(); b[0x1B] = { type: 0x1B, maximum: 1.5, value: one, arg: 0, points: 2000 }; hud.prepassSlots = b;
  assert.equal(hud.big.id, 0x3A); assert.equal(hud.big.cash, 2000);
  const texts = hud.frame(bank(), {}).filter((d) => d.kind === 'text').map((d) => d.text);
  assert.ok(texts.includes('MISSION SUCCESS') && texts.includes('$ 2,000'), JSON.stringify(texts));
  // 0x30: a paid-out combo in Conquer the Mountain (flags 0x200): the 0x26 animation with the money text
  const c30 = bank(); c30[0x23] = { type: 0x30, maximum: 2.5, value: 0.5, arg: 3, points: 4, text: '4' };
  const cash = hud.frame(c30, { flags: 0x1530C380, bigMessage: false }).filter((d) => d.kind === 'text').map((d) => d.text);
  assert.ok(cash.includes('$ 4') && cash.some((t) => /^3X /.test(t)), JSON.stringify(cash));
  assert.equal(hud.frame(c30, { flags: 0x1D31C047, bigMessage: false }).filter((d) => d.kind === 'text').length, 0, 'not without flag 0x200');
  console.log('big messages: GO! (slot 0x1A) grows 0.4 -> 1 and fades after 0.6 over 60 updates; MISSION SUCCESS + $ 2,000 (slot 0x1B); combo cash 0x30');
}

// ---- 5. world state 15 -------------------------------------------------------------------------------------------------------
{
  assert.equal(sessionPlacement({ session: { course: 0, index: 2, kind: 2 } }), true, 'MCOMM Session point 2');
  assert.equal(sessionPlacement({ session: { course: 0, index: 1, kind: 2 } }), true, 'Transport to the current location: point 1');
  assert.equal(sessionPlacement({ session: { course: 0, index: 1, kind: 2 }, entry: true }), false, 'a location entry (11D390) posts nothing');
  assert.equal(sessionPlacement({ session: { course: 17, index: 0, kind: 2 } }), false, 'the station ride-in (session point 0)');
  console.log('world state 15: Session points and a Transport to here post the reset event; entries and the station ride-in do not');
}
console.log('test-ctm-left: ok');
