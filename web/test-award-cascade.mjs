// Career awards the PS2 way (pv awardCascade; docs/ctm-parity.md "Awards, the PS2 way"): 159CD0's grant and its cascade, 1591E8's
// event order, 1599A0 / 159B08 (the Freeride goal at a medal), 159818 (earnings), 158F60 (the passes) and the reward record
// 0x4C3EF0 that the reward list (0x1FF7B8) reads. PS2 facts: local/ps2-capture/ctm-parity/runs/sd-goal (the record after a
// free-ride goal), race-f95 (the earnings goal at the Snow Jam final), the asm of 159CD0 / 1577E0 / 157920 / 1591E8 / 158F60.
import fs from 'node:fs';
import { execFileSync } from 'node:child_process';
import assert from 'node:assert/strict';
import { Career, MODE, MEDAL, eventKey } from './career.js';
import { setBigChallengeTable, statusWords, challengeStatus } from './big-challenges.js';
import { COLLECTIBLE_TOTALS } from './free-ride-hud.js';

const root = new URL('public/assets/', import.meta.url);
const read = (p) => JSON.parse(fs.readFileSync(new URL(p, root)));
if (!fs.existsSync(new URL('CAREER/career.json', root)) || !fs.existsSync(new URL('BIGCHAL/big-challenges.json', root))) { console.log('career tables not exported; skipped'); process.exit(0); }
const data = read('CAREER/career.json'), shop = fs.existsSync(new URL('CAREER/shop.json', root)) ? read('CAREER/shop.json') : null;
const rows = read('BIGCHAL/big-challenges.json').challenges;
setBigChallengeTable(rows);
class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }
const fresh = () => new Career(data, { storage: new Memory(), shop, rosterSeed: () => 0x1234 });
const id = 'zoe', ZOE = 4;
const peakOf = (course) => data.courses[course].field54 + 1;
const goalEvents = (peak, goal) => data.rules.goal_events.find((g) => g.peak === peak && g.goal === goal).events;
// every course row of the peak full / every Big Challenge of the peak done (the platinum explore medals)
function allCollected(c, peak) { const r = c.rider(id); r.collected ??= {}; data.courses.forEach((co, i) => { if (i < 22 && co.field54 === peak - 1) { const n = COLLECTIBLE_TOTALS[i]; r.collected[i] = [n >= 32 ? 0xffffffff : (2 ** n - 1) >>> 0, n > 32 ? (2 ** (n - 32) - 1) >>> 0 : 0]; } }); }
function allChallenges(c, peak) { const w = statusWords(c, id); rows.forEach((x, i) => { if (x.course >= 0 && peakOf(x.course) === peak) w[i] = (w[i] | 8) >>> 0; }); }
function medal(c, mode, course, m) { const r = c.rider(id); r.medals[eventKey(mode, course ?? null)] = m; r.golds ??= {}; if (m !== MEDAL.NONE && m <= MEDAL.GOLD) r.golds[eventKey(mode, course ?? null)] = 1; }
// an event's final result: the active event at its final, placed 1st (gold), through the career's result entry point
function winFinal(c, mode, course, ticks = 9500) { const ev = c.startEvent(id, mode, course, true); ev.round = 3; return c.completeEvent(0, { ticks, score: 999999 }); }
const counts = (c, award) => Array.from(c.rewardRecord?.counts.get(award) ?? Array(8).fill(0));

// ---- 1. sd-goal: the Freeride goal completes in free ride; the record keeps award 11 {pass 1, poster 1, cards 4} -------------------
{
  const c = fresh(); c.rider(id);
  c.clearRewardRecord();                                        // world state 10 at the free ride's location
  for (let i = 0; i < 40; i++) c.markCollected(id, 14, i, 0);    // collectible bronze (40), no Big Challenge medal yet
  const w = statusWords(c, id); let done = 0;
  rows.forEach((x, i) => { if (done < 11 && x.course >= 0 && peakOf(x.course) === 1) { w[i] = (w[i] | 8) >>> 0; done++; } });
  assert.ok(!c.goalComplete(id, 1, 'freeride'));
  const twelfth = rows.findIndex((x, i) => x.course >= 0 && peakOf(x.course) === 1 && !(w[i] & 8));
  challengeStatus(c, id, twelfth, (w[twelfth] | 8) >>> 0);        // 307308 -> 159B08: the 12th challenge = the bronze
  assert.ok(c.granted(id, 11) && c.rider(id).peaks[1], 'award 11 and the Peak 2 pass (158F60 arg 1)');
  const ps2 = [0, 0, 1, 1, 4, 0, 0, 0];                          // PS2 sd-goal end.p2s: +6 + 11*10 = {0, 0, 1, 1, 4, 0...}
  const p2s = new URL('../local/ps2-capture/ctm-parity/runs/sd-goal/end.p2s', import.meta.url);
  assert.deepEqual(counts(c, 11), ps2, 'the record holds award 11: the pass, a poster, four trading cards');
  assert.ok(c.rewardRecord.items[2].has(ZOE * 3 + 1), 'the pass item is char * 3 + 1 (PS2 bit 13 of the pass set for Zoe)');
  assert.equal(c.rewardRecordCount(), 6);
  if (fs.existsSync(p2s)) {   // the PS2 bytes themselves: +6 + award*10 + category, and the pass set's bit char*3+1
    const mem = execFileSync('unzip', ['-p', p2s.pathname, 'eeMemory.bin'], { maxBuffer: 64 << 20 }), rec = 0x4C3EF0;
    assert.deepEqual(Array.from(mem.subarray(rec + 6 + 110, rec + 6 + 118)), ps2, 'PS2 sd-goal: the record row of award 11');
    for (let a = 0; a < 32; a++) if (a !== 11) assert.ok(mem.subarray(rec + 6 + a * 10, rec + 16 + a * 10).every((b) => b === 0), `PS2 sd-goal: award ${a} empty`);
    const passes = mem.readUInt32LE(rec + 0x174 + 8) & 0x1FFFFFF, bit = ZOE * 3 + 1;
    assert.ok((mem[passes + (bit >> 3)] >> (bit & 7)) & 1, 'PS2 sd-goal: pass bit char*3+1');
  }
  // the next event reached without a location change lists it: a Snow Jam qualifier (no cash) still opens the reward list (20A8F8)
  const ev = c.startEvent(id, MODE.RACE, 0, true); assert.equal(ev.round, 1);
  assert.ok(c.rewardRecordCount() > 0, 'the free-ride award is still in the record at the next results');
  c.clearRewardRecord();                                        // a crossing / world load (158E30 at WS10)
  assert.equal(c.rewardRecordCount(), 0);
  console.log('sd-goal: the Freeride goal award in the record {pass 1, poster 1, cards 4}; kept to the next results, cleared at world state 10');
}

// ---- 2. the cascade in free ride: the last Peak 1 goal completed by a Big Challenge grants "Peak 1 conquered!" at once ---------------
{
  const c = fresh(); c.rider(id);
  for (const g of ['race', 'freestyle']) for (const e of goalEvents(1, g)) medal(c, e.mode, e.course, MEDAL.GOLD);
  c.rider(id).earned = 100000; c.rider(id).awards = [5, 8, 14]; c.rider(id).peaks[1] = true;   // race, freestyle, earnings already granted
  for (let i = 0; i < 40; i++) c.markCollected(id, 14, i, 0);
  const w = statusWords(c, id); let done = 0;
  rows.forEach((x, i) => { if (done < 11 && x.course >= 0 && peakOf(x.course) === 1) { w[i] = (w[i] | 8) >>> 0; done++; } });
  c.clearRewardRecord();
  const twelfth = rows.findIndex((x, i) => x.course >= 0 && peakOf(x.course) === 1 && !(w[i] & 8));
  challengeStatus(c, id, twelfth, (w[twelfth] | 8) >>> 0);
  assert.ok(c.granted(id, 11), 'award 11 (the Freeride goal)');
  assert.ok(c.granted(id, 2), 'award 2 "Peak 1 conquered!" in the same grant (159CD0 tail 0x15A1C4: 157A78(peak 1))');
  assert.equal(c.rewardRecord.cheats.get(2), 0x12, 'award 2 records its cheat character (Jurgen, 0x12)');
  assert.ok(c.owned(id, 'cheat_character').includes(c.rewardItems('cheat_character').findIndex((x) => x.character === 0x12)));
  assert.ok(!c.granted(id, 1) && !c.granted(id, 0));
  console.log('cascade: the Freeride goal that completes Peak 1 grants "Peak 1 conquered!" (Jurgen) at once, in free ride');
}

// ---- 3. award 0 "Mountain conquered!" (1577E0): the last gold with everything else in place ---------------------------------------
{
  const c = fresh(); c.rider(id);
  for (let p = 1; p <= 3; p++) {
    for (const g of ['race', 'freestyle']) for (const e of goalEvents(p, g)) medal(c, e.mode, e.course, e.mode >= 4 ? MEDAL.BRONZE : MEDAL.GOLD);
    allCollected(c, p); allChallenges(c, p);
  }
  for (let p = 1; p <= 3; p++) { assert.equal(c.collectMedal(id, p), MEDAL.PLATINUM, `peak ${p} collectible platinum`); assert.equal(c.challengeMedal(id, p), MEDAL.PLATINUM, `peak ${p} challenge platinum`); }
  c.monster(id).medals = Array(8).fill(3);
  medal(c, MODE.RACE, 0, MEDAL.SILVER);                         // Snow Jam only silver: 157920 needs gold or platinum
  assert.ok(!c.mountainConquered(id));
  c.rider(id).medals[eventKey(MODE.RACE, 0)] = MEDAL.SILVER; delete c.rider(id).golds[eventKey(MODE.RACE, 0)];
  c.clearRewardRecord();
  const out = winFinal(c, MODE.RACE, 0);
  assert.equal(out.medal, MEDAL.GOLD);
  assert.ok(c.granted(id, 0), 'award 0 (1591E8 0x1594B4 -> 159CD0(0))');
  assert.equal(c.rewardRecord.cheats.get(0), 0x1D, 'Far East Myth (0x1D) recorded under award 0');
  assert.ok(c.owned(id, 'cheat_character').includes(c.rewardItems('cheat_character').findIndex((x) => x.character === 0x1D)));
  // the highlight levels short of 24: no award
  const d = fresh(); d.rider(id);
  for (let p = 1; p <= 3; p++) { for (const g of ['race', 'freestyle']) for (const e of goalEvents(p, g)) medal(d, e.mode, e.course, e.mode >= 4 ? MEDAL.BRONZE : MEDAL.GOLD); allCollected(d, p); allChallenges(d, p); }
  d.monster(id).medals = [3, 3, 3, 3, 3, 3, 3, 2];
  assert.ok(!d.mountainConquered(id), 'the eight highlight levels must sum to 24');
  d.monster(id).medals[7] = 3; allCollected(d, 2); d.rider(id).collected[2] = [0, 0];
  assert.ok(!d.mountainConquered(id), 'platinum collectibles on every peak');
  console.log('award 0: golds everywhere, rival / peak medals, platinum explore medals and 24 highlight levels grant Far East Myth');
}

// ---- 4. the event's order: earnings (159818) before the race goal, so the Peak 2 pass lies under "Earnings goal complete!" -----
{
  const c = fresh(); c.rider(id);
  medal(c, MODE.RACE, 1, MEDAL.GOLD); medal(c, MODE.RIVAL_TIME, 14, MEDAL.GOLD); medal(c, 6, null, MEDAL.GOLD);   // Metro-City, Happiness Race, Peak 1 Race
  c.rider(id).earned = 95000; c.clearRewardRecord();
  const out = winFinal(c, MODE.RACE, 0);                        // the Snow Jam gold: $10,000 -> 105,000, and the race goal
  assert.ok(c.granted(id, 14) && c.granted(id, 5) && c.granted(id, 17), 'earnings (14), race goal (5), first gold (17)');
  assert.deepEqual(counts(c, 14).slice(2, 5), [1, 1, 4], 'the pass under award 14 (158F60 runs first in the earnings grant)');
  assert.deepEqual(counts(c, 5).slice(2, 5), [0, 1, 4], 'no pass under award 5 (already open)');
  assert.equal(c.rewardRecord.cash, 10000); assert.equal(c.rewardRecord.medal, MEDAL.GOLD);
  assert.deepEqual(out.opened, [2]);
  console.log('event order: the first gold award, 159818 (earnings award with the pass), then the race goal award');
}

// ---- 5. collection bonuses are not recorded (1580F8 / 157FD0 / 158220 / 158348 -> 158618) -------------------------------------------
{
  const c = fresh(); c.rider(id);
  const cards = c.rewardItems('trading_card').length;
  for (let i = 0; i < cards - 2; i++) c.grantReward(id, 'trading_card', i);   // two cards left: the goal award's four picks complete it
  c.clearRewardRecord();
  c.grantAward(id, 5);
  const hiro = c.rewardItems('cheat_character').findIndex((x) => x.character === 0x11);
  assert.ok(c.owned(id, 'cheat_character').includes(hiro), 'the trading-card collection bonus (Hiro) is granted');
  assert.equal(c.rewardRecord.cheats.size, 0, 'but not recorded: the reward list does not show it');
  assert.deepEqual(counts(c, 5).slice(2, 5), [1, 1, 2], 'the two cards left, the poster, the pass');
  console.log('collection bonus: granted silently');
}
// ---- pv rewardRng: the picks draw 0x3177F0 (the presentation generator 0x4FF018), r = draw % unowned, the r-th unowned --------------
{
  const { seededWords, nextWord } = await import('./lineup.js');
  const gen = (seed) => { const w = seededWords(seed); return () => nextWord(w); };
  {
    const c = new Career(data, { storage: new Memory(), shop, rosterSeed: () => 0x1234, presentation: gen(0xC0FFEE) }); c.rider(id);
    const seed0 = c.save.seed;
    // the expected picks with the same stream: award 5 (Peak 1 race goal): four cards, then a poster (0x159F50 .. 0x159FA8)
    const draw = gen(0xC0FFEE), expect = [], owned = { trading_card: [], poster: [] };
    for (const [cat, n] of [['trading_card', 4], ['poster', 1]]) for (let k = 0; k < n; k++) {
      const all = c.rewardItems(cat).length, r = draw() % (all - owned[cat].length); let i = -1, left = r;
      for (let j = 0; j < all; j++) if (!owned[cat].includes(j) && left-- === 0) { i = j; break; }
      owned[cat].push(i); expect.push([cat, i]);
    }
    const got = c.grantAward(id, 5).filter((g) => g.category === 'trading_card' || g.category === 'poster').map((g) => [g.category, g.index]);
    assert.deepEqual(got, expect, 'picks from the presentation stream (0x157080)');
    assert.equal(c.save.seed, seed0, 'the xorshift stand-in is not drawn');
  }
  console.log('rewardRng: the picks draw 0x3177F0');
}
console.log('test-award-cascade: ok');
