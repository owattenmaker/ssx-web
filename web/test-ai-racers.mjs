// Snow Jam six-rider race smoke test (web/ai-racers.js): the five original computer riders start from
// their countdown-grid seeds, hold the grid through the countdown (control 6), push off, follow their
// AI routes and are ranked by the original 0x10F998 ranking. Exactness against the PS2 is gated by
// test-ps2-captures.mjs (event-race-ai, ai-idle, event-race-ai-pairs); this test checks the browser
// integration without captures. Full-race completion: docs/ai-racers.md (node script in the doc).
import assert from 'node:assert/strict';
import { createNodeRace } from './ai-race-node.mjs';
import { estimateFinishTicks, orderByTime } from './ai-race.js';

const race = await createNodeRace({});
race.start();
const pad = new Float32Array(24);
const pos = (c) => Array.from(new Float32Array(c.HEAPF32.buffer, c._rider_world_state(), 16)).slice(0, 3);
const start = race.racers.npcs.map((n) => pos(n.core));
// 11C298 channel-1 masks per character (rider+0x8C0/+0x8C8 = bone lists | +0x8D0), read from the countdown savestate.
const masks = (c) => Array.from(new Float32Array(c.HEAPF32.buffer, c._upper_request_info(), 12)).slice(6, 11);
const expectedMasks = { psymon: [0x4000, 0xfffe, 0x4000, 0xfff8, 0x870], luther: [0x4000, 0xfffe, 0x4000, 0xfff8, 0x870],
  allegra: [0x20000, 0xfffe, 0x20000, 0xfff8, 0x870], moby: [0x20000, 0xfffe, 0x20000, 0xfff8, 0x870], griff: [0x8000, 0xfffe, 0x8000, 0xfff8, 0x870] };
race.racers.npcs.forEach((n) => assert.deepEqual(masks(n.core), expectedMasks[n.character], `${n.character} channel-1 masks`));
const controls = race.racers.npcs.map(() => new Set());
for (let t = 0; t < 1500; t++) {
  race.tick(pad);
  race.racers.npcs.forEach((n, k) => controls[k].add(new Float32Array(n.core.HEAPF32.buffer, n.core._rider_world_state(), 16)[8]));
  if (t === 150) race.racers.npcs.forEach((n, k) => assert.deepEqual(pos(n.core), start[k], `${n.character} left the grid during the countdown`));
}
race.racers.npcs.forEach((n, k) => {
  const p = pos(n.core), moved = Math.hypot(p[0] - start[k][0], p[1] - start[k][1], p[2] - start[k][2]);
  assert(moved > 20000, `${n.character} moved only ${moved.toFixed(0)} cm in 1500 ticks`);
  assert(controls[k].has(6) && controls[k].has(0), `${n.character} controls ${[...controls[k]]}`);
});
const rows = race.racers.standings();
assert.deepEqual(rows.map((r) => r.rank).sort(), [0, 1, 2, 3, 4, 5], 'ranking is not a permutation of 6 places');
const byRank = rows.slice().sort((a, b) => a.rank - b.rank);
for (let i = 1; i < byRank.length; i++) assert(byRank[i - 1].remaining <= byRank[i].remaining + 200, 'ranking disagrees with course progress beyond the 20 cm/place hysteresis window');
// 0x122D78 estimate and 0x238BF8 order.
assert.equal(estimateFinishTicks(6000, 353496.15625, 153496.15625, 1), 6000 + Math.trunc(Math.fround(153496.15625 / Math.fround(Math.fround(200000) / 6000))));
assert.equal(estimateFinishTicks(100, 1000, 900, 5), 100 + Math.trunc(Math.fround(900 / 25)), 'minimum average speed 30 - slot');
assert.deepEqual(orderByTime([300, 100, 200, 100]), [1, 3, 2, 0]);
console.log(`AI racers: 5 computer riders raced 1500 ticks, ranks ${rows.map((r) => `${r.character}:${r.rank}`).join(' ')}, pairs ${race.racers.pairCounts.join('/')}`);
