// Backcountry rival challenges (web/rival-mode.js, career.js rivalResult; docs/backcountry.md).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { RIVAL_TIME, RIVAL_POINTS, DNF_TICKS, CHARACTER_ID, rivalCharacter, rivalResult, estimateTicks, estimatePoints, hudFlags, timeLimitTicks, eventKind, handlerIndex, objectives } from './rival-mode.js';
import { Career, MODE, MEDAL } from './career.js';

// 0x145750: Peak 1 Mac / Griff for Mac, Peak 2 Nate / Zoe for Nate, Peak 3 Psymon / Elise for Psymon.
assert.equal(rivalCharacter(1, 4), 3); assert.equal(rivalCharacter(1, 3), 5);
assert.equal(rivalCharacter(2, 4), 7); assert.equal(rivalCharacter(2, 7), 4);
assert.equal(rivalCharacter(3, 4), 8); assert.equal(rivalCharacter(3, 8), 6);
// 0x238160 / 0x47C0F0: mode 4 -> handler 5, mode 5 -> handler 6; event kinds 5 / 6.
assert.equal(handlerIndex(RIVAL_TIME), 5); assert.equal(handlerIndex(RIVAL_POINTS), 6);
assert.equal(eventKind(RIVAL_TIME), 5); assert.equal(eventKind(RIVAL_POINTS), 6);
// HUD 0x478078[4] = [5] = 0x1530C047; 1EA930: 2 riders -> |1 &~0x30; kind 6 -> OPPONENT 0x08000000.
assert.equal(hudFlags(RIVAL_TIME), 0x1530C047 & ~0x30 | 1);
assert.equal(hudFlags(RIVAL_POINTS), (0x1530C047 & ~0x30 | 1 | 0x08000000) >>> 0);
// 1454F8: Rival Points 300 s (table 0x440B38 row 27), Rival Time untimed.
assert.equal(timeLimitTicks(RIVAL_POINTS, 14), 18000); assert.equal(timeLimitTicks(RIVAL_TIME, 14), 0);
assert.deepEqual(objectives(RIVAL_TIME).bullets, [0x0177555d]); assert.deepEqual(objectives(RIVAL_POINTS).bullets, [0x083e70d3]);

// 0x122D78 / 0x122E50 estimates.
assert.equal(estimateTicks(6000, 314023.75, 100000, 1), 6000 + Math.trunc(Math.fround(100000 / Math.fround(Math.fround(314023.75 - 100000) / 6000))));
assert.equal(estimateTicks(10, 1000, 999, 1), 10 + Math.trunc(Math.fround(999 / 29)), 'slow rider: floor 30 - slot');
assert.equal(estimatePoints(4999, 1000, 314023.75), 5000, 'no course left beyond 1000 cm: score + 1');
assert.equal(estimatePoints(9999, 101000, 201000), 10000 + Math.trunc(Math.fround(100000 * Math.fround(10000 / 100000))));

// Rival Time (23B8C8): the human finishes first -> win; rival still on course gets the estimate.
let r = rivalResult(RIVAL_TIME, [{ human: true, finishTicks: 11000 }, { human: false, finishTicks: null, remaining: 5000, origin: 314023.75 }], 11000);
assert.equal(r.win, true); assert.deepEqual(r.place, [0, 1]); assert.equal(r.estimated[1], true);
// PS2 nav/bc/out-tuck-finish2: Mac 03:18, Zoe 04:21 -> "Sorry, you didn't win."
r = rivalResult(RIVAL_TIME, [{ human: true, finishTicks: 15701 }, { human: false, finishTicks: 11900 }], 15701);
assert.equal(r.win, false); assert.deepEqual(r.place, [1, 0]);
// Give up: 360000 ticks, never a win even against an unfinished rival.
r = rivalResult(RIVAL_TIME, [{ human: true, dnf: true }, { human: false, finishTicks: null, remaining: 300000, origin: 314023.75 }], 900);
assert.equal(r.values[0], DNF_TICKS); assert.equal(r.win, false);
// Rival Points (23BDB8): scores descending; the rival's latched score, or the estimate when unfinished.
r = rivalResult(RIVAL_POINTS, [{ human: true, score: 70000 }, { human: false, finishTicks: 12315, score: 67761 }], 14027);
assert.equal(r.win, true);
r = rivalResult(RIVAL_POINTS, [{ human: true, score: 1030 }, { human: false, finishTicks: 12315, score: 67761 }], 14027);
assert.equal(r.win, false); assert.deepEqual(r.place, [1, 0]);
r = rivalResult(RIVAL_POINTS, [{ human: true, score: 20000 }, { human: false, finishTicks: null, score: 9999, remaining: 101000, origin: 201000 }], 9000);
assert.equal(r.values[1], estimatePoints(9999, 101000, 201000)); assert.equal(r.win, true);
r = rivalResult(RIVAL_POINTS, [{ human: true, dnf: true, score: 50000 }, { human: false, finishTicks: 100, score: 0 }], 18001);
assert.equal(r.values[0], 0); assert.equal(r.win, false, 'time up / give up scores 0 and never wins');

// Career: Happiness (course 14) rival race, won -> gold, cash 0x4405A0[14][gold], one round, event complete.
const data = JSON.parse(fs.readFileSync(new URL('public/assets/CAREER/career.json', import.meta.url)));
class Memory { constructor() { this.map = new Map(); } getItem(k) { return this.map.get(k) ?? null; } setItem(k, v) { this.map.set(k, String(v)); } removeItem(k) { this.map.delete(k); } }
const career = new Career(data, { storage: new Memory() });
const ev = career.startEvent('zoe', MODE.RIVAL_TIME, 14, true);
assert.equal(ev.round, 1); assert.equal(ev.rival, 3, 'Zoe meets Mac');
assert.equal(career.roundName(ev), 'Rival Challenge');
assert.equal(career.objectives(ev)[0].arg, 'Mac');
let out = career.rivalResult(rivalResult(RIVAL_TIME, [{ human: true, finishTicks: 12000 }, { human: false, finishTicks: null, remaining: 9000, origin: 314023.75 }], 12000), { ticks: 12000 });
assert.ok(out.medal === MEDAL.GOLD || out.medal === MEDAL.PLATINUM); assert.equal(out.win, true);
assert.equal(out.cash, data.rules.cash[14][out.medal]);
assert.equal(career.timeLimit({ mode: MODE.RIVAL_POINTS, course: 14 }), 18000);
const lose = new Career(data, { storage: new Memory() });
const ev2 = lose.startEvent('mac', MODE.RIVAL_POINTS, 14, true);
assert.equal(ev2.rival, 5, 'Mac meets Griff');
out = lose.rivalResult(rivalResult(RIVAL_POINTS, [{ human: true, score: 100 }, { human: false, finishTicks: 10, score: 500 }], 5000), { score: 100 });
assert.equal(out.medal, MEDAL.NONE); assert.equal(out.cash, 0); assert.equal(lose.medal('mac', MODE.RIVAL_POINTS, 14), MEDAL.NONE);
// Single Event: the rival challenge is one round too (not the forced final).
assert.equal(new Career(data, { storage: new Memory() }).startEvent('zoe', MODE.RIVAL_TIME, 14, false).round, 1);

// The rival's start: the game RNG 0x4FF030 at the rolling start (the ready state; web/lineup.js rivalAnchorWords, set by
// web/ai-race.js prepareRival). Each document keeps its ready state's words: seeded 0 at the load + 4 or 5 load draws.
{
  const { rivalAnchorDraws, rivalAnchorWords, seededWords, nextWord } = await import('./lineup.js');
  const roster = JSON.parse(fs.readFileSync(new URL('public/assets/riders.json', import.meta.url)));
  const who = (id) => ({ base: CHARACTER_ID.indexOf(id), cheat: 0 });
  const words = (n) => { const w = seededWords(0); for (let k = 0; k < n; k++) nextWord(w); return w; };
  const runs = new URL('../local/ps2-capture/runs/', import.meta.url).pathname;
  const capturedRng = (name) => { const bin = runs + name + '.bin'; if (!fs.existsSync(bin)) return null; const fd = fs.openSync(bin, 'r'), b = Buffer.alloc(24); fs.readSync(fd, b, 0, 24, 8896); fs.closeSync(fd); return Array.from({ length: 6 }, (_, k) => b.readUInt32LE(4 * k)); };
  const CAPTURES = { ABC1: { race: ['bc/bc-race-tuck', 'bc/bc-race-idle', 'bc/bc-race-tuck2'], jam: ['bc/bc-jam-tricks2', 'bc/bc-jam-iso'] }, DBC2: { race: ['peak2/dbc2-race-tuck', 'weather/dbc2-weather'] }, EBC3: { race: ['peak3/the-throne-race-idle'] } };
  let captures = 0;
  const RIVAL_IDS_OF = (code, id) => CHARACTER_ID[rivalCharacter({ ABC1: 1, DBC2: 2, EBC3: 3 }[code], CHARACTER_ID.indexOf(id))];
  for (const code of ['ABC1', 'DBC2', 'EBC3']) {
    const rivals = JSON.parse(fs.readFileSync(new URL(`public/assets/${code}/rivals.json`, import.meta.url)));
    for (const [mode, docs] of Object.entries(rivals.documents)) for (const [rival, doc] of Object.entries(docs)) {
      assert.equal(doc.anchor_rng?.seed, 0, `${code} ${mode}/${rival}: the ready state's game RNG is seeded 0 at the load`);
      assert.deepEqual(doc.anchor_rng.words, words(doc.anchor_rng.draws), `${code} ${mode}/${rival}: anchor words = seed 0 + draws`);
      assert.deepEqual(rivalAnchorWords(rivals, doc, who(doc.human), roster), doc.anchor_rng.words, `${code} ${mode}/${rival}: its own human starts from the ready state`);
      if (doc.human !== 'zoe') continue;
      for (const name of CAPTURES[code]?.[mode] ?? []) { const rng = capturedRng(name); if (!rng) continue; captures++;
        assert.deepEqual(rivalAnchorWords(rivals, doc, who('zoe'), roster), rng, `${name}: the page's anchor RNG is the capture's (record 0)`); }
    }
    // Other humans: a player's direct menu path, the lineups' load-draw rule (Moby +1, Zoe -1, a cheat skin +4) around the
    // course's base. Both documents of a course (Zoe's and the rival-swap human's) must predict the same counts.
    const zoeDoc = Object.values(rivals.documents.race).find((d) => d.human === 'zoe'), swapDoc = Object.values(rivals.documents.race).find((d) => d.human !== 'zoe');
    for (const h of [who('moby'), who('kaori'), who('viggo'), { base: 4, cheat: 12 }, { base: 2, cheat: 20 }]) {
      const viaZoe = rivalAnchorDraws(rivals, zoeDoc, h, roster), viaSwap = rivalAnchorDraws(rivals, swapDoc, h, roster);
      assert.equal(viaZoe, viaSwap, `${code}: human ${JSON.stringify(h)} draws ${viaZoe} from the Zoe document, ${viaSwap} from the ${swapDoc.human} one`);
    }
    assert.equal(rivalAnchorDraws(rivals, swapDoc, who('zoe'), roster), zoeDoc.anchor_rng.draws - zoeDoc.anchor_rng.menu_extra, `${code}: Zoe's direct path`);
    // Ruthless: the direct-path PS2 states (characters/<human>/select.p2s -> ruthless-nate-ready's menu script).
    for (const [id, e] of Object.entries(rivals.direct_path ?? {})) {
      const doc = rivals.documents.race[RIVAL_IDS_OF(code, id)];
      assert.equal(rivalAnchorDraws(rivals, doc, who(id), roster) - (id === doc.human ? doc.anchor_rng.menu_extra : 0), e.draws, `${code}: ${id}'s direct path (${e.state})`);
    }
  }
  console.log(`Rival anchor RNG: every rival document's ready-state words; ${captures} PS2 captures start from the page's anchor`);
}
console.log('Rival challenges: handlers, rival table, HUD, time limit, estimates, results, career medals OK');
