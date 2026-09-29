// Peak challenges (web/peak-run.js, docs/peak-mountain.md): tiers, limits, splits and results of the time challenge
// (23B268/23B5F8/23B468) and points challenge (23C0D0/23C560/23C2D8) against table 0x440D18 (career.json), plus the
// PS2 Peak 1 Race values (fresh Zoe: GMM+0x78 = +0x7C = 48000 ticks, the HUD clock 00:13:20).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { MEDAL } from './career.js';
import { peakTier, peakSetup, peakResult, peakSplit, splitText, isPeakRun, peakKind, PEAK_RUNS } from './peak-run.js';
import { COLLECTIBLE_TOTALS, cashText } from './free-ride-hud.js';

const file = new URL('public/assets/CAREER/career.json', import.meta.url);
if (!fs.existsSync(file)) { console.log('career.json not exported; skipped'); process.exit(0); }
const rules = JSON.parse(fs.readFileSync(file)).rules;
assert.equal(rules.peak_challenges.length, 18);

// Tiers: time challenge gold/silver -> 2, bronze -> 1, none/platinum -> 0; points challenge platinum/gold/silver -> 2.
assert.deepEqual([MEDAL.NONE, MEDAL.PLATINUM, MEDAL.GOLD, MEDAL.SILVER, MEDAL.BRONZE].map((m) => peakTier(6, m)), [0, 0, 2, 2, 1]);
assert.deepEqual([MEDAL.NONE, MEDAL.PLATINUM, MEDAL.GOLD, MEDAL.SILVER, MEDAL.BRONZE].map((m) => peakTier(9, m)), [0, 2, 2, 2, 1]);

// Peak 1 Race, first attempt: 800 s -> 48000 ticks (PS2 GMM words), splits 220 / 508 s, $5,000 bronze row.
const race = peakSetup(rules, 6, MEDAL.NONE);
assert.equal(race.limitTicks, 48000); assert.equal(race.target, 48000); assert.deepEqual(race.splits.slice(0, 2), [220, 508]);
assert.equal(peakSetup(rules, 6, MEDAL.GOLD).limitTicks, 670 * 60);
// Medals: gold <= 670 s, silver <= 760 s, bronze <= the tier limit; above the limit or time up fails.
assert.deepEqual(peakResult(rules, race, { ticks: 670 * 60 }), { place: 0, fail: false, value: 40200 });
assert.equal(peakResult(rules, race, { ticks: 670 * 60 + 1 }).place, 1);
assert.equal(peakResult(rules, race, { ticks: 760 * 60 + 1 }).place, 2);
assert.equal(peakResult(rules, race, { ticks: 48001 }).fail, true);
assert.deepEqual(peakResult(rules, race, { dnf: true }), { place: 3, fail: true, value: 360000 });
// A silver holder rides against the 670 s limit: 700 s fails.
assert.equal(peakResult(rules, peakSetup(rules, 6, MEDAL.SILVER), { ticks: 700 * 60 }).fail, true);

// Peak 1 Jam: 12:00, 130,000 / 350,000 / 700,000 points.
const jam = peakSetup(rules, 9, MEDAL.NONE);
assert.equal(jam.limitTicks, 720 * 60); assert.equal(jam.target, 130000);
assert.equal(peakResult(rules, jam, { score: 129999 }).fail, true);
assert.equal(peakResult(rules, jam, { score: 130000 }).place, 2);
assert.equal(peakResult(rules, jam, { score: 350000 }).place, 1);
assert.equal(peakResult(rules, jam, { score: 700000 }).place, 0);

// Splits: int(raceTicks * 0.016666668) - split; zero splits advance the counter silently.
let s = peakSplit(race, 0, { raceTicks: 200 * 60 + 59 });
assert.deepEqual([s.counter, s.shown, s.value, s.ahead], [1, true, -20, true]); assert.equal(splitText(s, true), '-00:00:20');
s = peakSplit(race, 1, { raceTicks: 520 * 60 }); assert.deepEqual([s.value, s.ahead], [12, false]); assert.equal(splitText(s, true), '+00:00:12');
assert.equal(peakSplit(race, 2, { raceTicks: 1 }).shown, false);
s = peakSplit(jam, 0, { score: 60000 }); assert.deepEqual([s.value, s.ahead], [5000, true]);

assert(isPeakRun(6) && isPeakRun(11) && !isPeakRun(5) && !isPeakRun(12));
assert.equal(peakKind(6), 5); assert.equal(peakKind(9), 6);
assert.deepEqual([PEAK_RUNS[6].start, PEAK_RUNS[6].finish, PEAK_RUNS[9].finish], [14, 1, 5]);

// Free-ride HUD tables: collectible totals 0x43FA70 (Peak 1: 155), cash text.
assert.equal([0, 1, 5, 8, 11, 14, 17, 18].reduce((a, c) => a + COLLECTIBLE_TOTALS[c], 0), 30 + 35 + 30 + 2 + 4 + 44 + 5 + 5);
assert.equal(cashText(1508), '$ 1,508'); assert.equal(cashText(0), '$ 0');
console.log('Peak challenges: tiers, limits, splits, medals; free-ride HUD tables OK');
