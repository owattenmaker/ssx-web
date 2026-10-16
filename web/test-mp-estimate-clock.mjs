// pv mpEstimateClock (docs/multiplayer.md "Results of racers still on course"): in an online race's Results a racer still on course
// (a human: the PS2 estimates only computer riders, 0x23A954 / 0x23AF74 after 12A250) has no time until its recorded time comes.
// Field report 2026-10-14: the old row ran down toward the finisher's own time, then jumped up to the real time at the other's finish.
import assert from 'node:assert/strict';
import { estimateFinishTicks } from './ai-race.js';
import { orderOnlineRows } from './net/result-rows.js';
import { sourceOf } from './test-source.mjs';

// The old rows (the bug): this page finished at race tick 10000 and its clock stopped; the other racer (slot 1) rides a 350000 cm
// course at 32 cm a tick and finishes at 10938. Its row was estimateFinishTicks(10000, ...) of its newest distance on every frame.
const origin = 350000;
const old = [];
for (let t = 10409; t < 10938; t += 3) old.push(estimateFinishTicks(10000, origin, Math.max(0, origin - 32 * t), 1));
assert.ok(old[0] > old[old.length - 1] + 300, `the old row runs down (${old[0]} -> ${old[old.length - 1]})`);
assert.ok(10938 - old[old.length - 1] > 700, 'and ends far under the real finish, then jumps up to it');

// The order: timed rows by time, then the racers still on course in slot order, then DNF; places by position.
const rows = [
  { slot: 0, ticks: null, dnf: false, onCourse: true },
  { slot: 1, ticks: 14561, dnf: false, onCourse: false },
  { slot: 2, ticks: 0, dnf: true, onCourse: false },
  { slot: 3, ticks: 12612, dnf: false, onCourse: false },
  { slot: 4, ticks: null, dnf: false, onCourse: true }
];
const ordered = orderOnlineRows(rows);
assert.deepEqual(ordered.map((r) => r.slot), [3, 1, 0, 4, 2]);
assert.deepEqual(ordered.map((r) => r.place), [0, 1, 2, 3, 4]);
for (const r of ordered.filter((x) => x.onCourse)) assert.equal(r.ticks, null, 'a racer on course has no time');
// equal times keep row order (0x238BF8)
assert.deepEqual(orderOnlineRows([{ slot: 0, ticks: 5 }, { slot: 1, ticks: 5 }]).map((r) => r.slot), [0, 1]);

// The wiring, behind the switch: web/net/mp-game.js resultRows and web/mp-ui.js drawResults
const game = sourceOf('net/mp-game.js');
assert.ok(game.includes("if(!latest||!Number.isFinite(latest.remaining))dnf=true;else if(pv('mpEstimateClock'))onCourse=true;else{"),
  'a racer still on course is onCourse with the switch, else the old estimate');
assert.ok(game.includes("if(pv('mpEstimateClock'))return orderOnlineRows(rows);"), 'the rows are ordered by result-rows.js');
const ui = sourceOf('mp-ui.js');
assert.ok(/if\(!r\.onCourse\)ui\.text\(c,[^;]*,418,y,18,color\);/.test(ui), 'its time is left empty');
console.log(`mp results on course: old row ${old[0]} -> ${old[old.length - 1]} (real 10938); on-course rows without a time, ordered OK`);
