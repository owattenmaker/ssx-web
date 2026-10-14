// pv frameGateRecover (web/quality.js frameGate recover): the low tier's 'auto' frame gate comes back to 60 after a load spike once
// the page can hold it, and a page that cannot stays at 30 without flapping. A port rule (the PS2 has no frame gate).
// The model: rAF at 60 Hz (real timestamps); each drawn frame's work = render + sim x the ticks it runs (one at 60, two at 30), plus a
// 2 s spike of 20 ms frames at the start.
//   node test-frame-gate.mjs
import assert from 'node:assert/strict';
import { frameGate } from './quality.js';

const FRAME = 1000 / 60;
// -> {rate at the end, the frame (ms) the gate went back to 60 after the spike, rate changes after that, time at 60 after the drop}
function ride({ recover, render, sim = 0, seconds = 300, spike = 2, frozen = false }) {
  const g = frameGate('auto', { recover });
  let t = 0, dropAt = null, backAt = null, changes = [], at60 = 0, last = g.rate;
  for (let f = 0; f < seconds * 60; f++) {
    if (!frozen || f < spike * 60 + 200) t = f * FRAME;
    const draw = g(t);
    if (draw) {
      const ticks = g.rate === 30 ? 2 : 1;
      g.done(f < spike * 60 ? 20 : render + sim * ticks);
    }
    if (g.rate !== last) {
      changes.push({ t: Math.round(f * FRAME), rate: g.rate });
      if (g.rate === 30 && dropAt == null) dropAt = f * FRAME;
      if (g.rate === 60 && dropAt != null && backAt == null) backAt = f * FRAME;
      last = g.rate;
    }
    if (dropAt != null && g.rate === 60) at60 += FRAME;
  }
  return { rate: g.rate, dropAt, backAt, changes, at60 };
}

// recover on: steady work of 7 / 9 / 12 ms comes back to 60 (12: the first probe, 10 s at 30) and stays there
for (const w of [7, 9, 10.9, 12]) {
  const r = ride({ recover: true, render: w });
  assert.equal(r.rate, 60, `${w} ms: back at 60`);
  assert.ok(r.dropAt != null && r.backAt - r.dropAt <= 12000, `${w} ms: back within 12 s of the drop (${r.backAt - r.dropAt} ms)`);
  assert.equal(r.changes.length, 2, `${w} ms: one drop, one recovery, no flapping ${JSON.stringify(r.changes)}`);
}
// a page that fits at 60 but reads above 11 ms at 30 (two ticks a frame): 6 ms render + 3 ms a tick = 9 at 60, 12 at 30 -> the probe
{
  const r = ride({ recover: true, render: 6, sim: 3 });
  assert.equal(r.rate, 60);
  assert.equal(r.changes.length, 2, `two-tick work: recovers by the probe and stays ${JSON.stringify(r.changes)}`);
}
// a true 15 ms device: stays at 30; only the probes leave it, each for one window, the wait from each drop doubling to 80 s
{
  const r = ride({ recover: true, render: 15 });
  assert.equal(r.rate, 30, '15 ms: stays at 30');
  const waits = [], windows = [];
  for (let k = 1; k < r.changes.length; k += 2) {
    waits.push(Math.round((r.changes[k].t - r.changes[k - 1].t) / 1000));
    if (r.changes[k + 1]) windows.push(r.changes[k + 1].t - r.changes[k].t);
  }
  assert.deepEqual(waits, [10, 20, 40, 80, 80], `15 ms: the waits before each probe (s) ${waits}`);
  assert.ok(windows.every((w) => w < 1000), `a probe lasts one 45-frame window ${windows}`);
  assert.ok(r.at60 < 0.02 * (300000 - r.dropAt), `15 ms: under 2% of the time at 60 (${Math.round(r.at60)} ms)`);
}
// a borderline 13-14 ms device: no threshold flapping (it never reads below 11 at 30)
{
  const r = ride({ recover: true, render: 12, sim: 1.5 });
  assert.ok(r.changes.filter((c) => c.rate === 60).length <= 5, `13.5 ms: only probes ${JSON.stringify(r.changes)}`);
}
// a held clock (a test's frozen rAF time): no probe ever starts
{
  const r = ride({ recover: true, render: 15, frozen: true });
  assert.equal(r.changes.length, 1, `frozen clock: the drop only ${JSON.stringify(r.changes)}`);
}
// recover off: as before, back to 60 only below 7 ms
for (const [w, rate] of [[6.9, 60], [7, 30], [9, 30], [12, 30]]) assert.equal(ride({ recover: false, render: w }).rate, rate, `off: ${w} ms ends at ${rate}`);
// fixed rates ignore recover
{
  const g = frameGate(30, { recover: true });
  let drawn = 0;
  for (let i = 0; i < 1200; i++) {
    if (g(i * FRAME)) {
      drawn++;
      g.done(2);
    }
  }
  assert.equal(g.rate, 30);
  assert.equal(drawn, 600, 'fixed 30 draws every other frame');
}
console.log('frame gate recover: OK');
