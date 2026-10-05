// web/tick-lock.js (pv tickLock): one tick per ~60 Hz drawn frame, the real-time debt paid one tick at a time, other frames unlocked.
import assert from 'node:assert/strict';
import { createTickLock } from './tick-lock.js';

const run = (dts) => {
  const lock = createTickLock();
  const out = [];
  for (const dt of dts) out.push(lock.frame(dt));
  return { out, lock };
};
const count = (a, v) => a.filter((x) => x === v).length;

// a steady 60 Hz display, with rAF jitter: one tick every frame
{
  const dts = Array.from({ length: 6000 }, (_, i) => 1 / 60 + (i % 2 ? 0.0004 : -0.0004));
  const { out, lock } = run(dts);
  assert.equal(count(out, 1), out.length, 'a 60 Hz frame runs one tick');
  assert(Math.abs(lock.debt) < 1e-6);
}
// 59.94 Hz: game time follows real time within the debt bound (an extra tick now and then)
{
  const n = 59.94 * 600;
  const { out } = run(Array.from({ length: n }, () => 1 / 59.94));
  const ticks = out.reduce((a, b) => a + b, 0);
  assert(Math.abs(ticks - 600 * 60) <= 4, `10 minutes at 59.94 Hz ran ${ticks} ticks`);
  assert(count(out, 0) === 0 && count(out, 2) > 0);
}
// 60.06 Hz: the other way (a frame of no tick now and then)
{
  const n = Math.round(60.06 * 600);
  const { out } = run(Array.from({ length: n }, () => 1 / 60.06));
  const ticks = out.reduce((a, b) => a + b, 0);
  assert(Math.abs(ticks - 600 * 60) <= 5, `10 minutes at 60.06 Hz ran ${ticks} ticks`);
  assert(count(out, 2) === 0 && count(out, 0) > 0);
}
// not a 60 Hz frame: a 120 / 144 Hz display, a missed vsync, a stall -> the caller's own pacing, and the debt starts again
for (const dt of [1 / 120, 1 / 144, 2 / 60, 0.25, 0]) {
  const { out, lock } = run([1 / 60 + 0.002, dt]);
  assert.equal(out[1], null, `dt ${dt}`);
  assert.equal(lock.debt, 0);
}
{
  const { out, lock } = run([1 / 60, 1 / 60]);
  assert.deepEqual(out, [1, 1]);
  lock.reset();
  assert.equal(lock.debt, 0);
}
console.log('tick lock: ok');
