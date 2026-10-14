// pv mpTickLock (main.js frame, web/net/race-pace.js createRaceLock): an online race's ~60 Hz frames run one tick each, with the server
// clock as the reference. The frame loop is replayed against today's online path (FixedStepClock + paceSeconds) on simulated displays:
// rAF jitter, 59.94 / 60.06 / 120 Hz, 200 ms stalls and clock-offset jumps. Checked: no 0- / 2-tick judder at 60 Hz, the drift from the
// server clock stays within the pace slack, a stall is caught up as before, and no pad sample waits longer for its tick than today.
import assert from 'node:assert/strict';
import { FixedStepClock } from './fixed-step-clock.js';
import { paceSeconds, createRaceLock, SLACK, LOCK_STREAK } from './net/race-pace.js';
import { sourceOf } from './test-source.mjs';

// a small deterministic generator (the same frames for both paths)
function generator(seed) {
  let s = seed >>> 0;
  return () => {
    s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
    return s / 0x100000000;
  };
}
// The frame callbacks of one display: [wall seconds since GO] (the rAF time is the callback time, jittered).
function frames({ hz, seconds, jitter = 0, stallEvery = 0, stall = 0, seed = 1, phase = 0 }) {
  const random = generator(seed);
  const out = [];
  // the GO instant falls anywhere between two vsyncs
  let vsync = phase / hz;
  let nextStall = stallEvery || Infinity;
  while (vsync < seconds) {
    vsync += 1 / hz;
    if (vsync >= nextStall) {
      // the page's main thread is busy: the next callback comes at the first vsync after the stall
      vsync = Math.ceil((vsync + stall) * hz) / hz;
      nextStall += stallEvery;
    }
    out.push(vsync + (random() * 2 - 1) * jitter);
  }
  return out;
}
// The server clock as this page sees it (ticks since GO): the clock offset estimate jumps by `offsetJump` seconds every 30 s.
const expectedAt = (t, offsetJump) => (t + (Math.floor(t / 30) % 2 ? offsetJump : 0)) * 60;

function run(times, { locked, offsetJump = 0 }) {
  const clock = new FixedStepClock();
  const lock = createRaceLock();
  let tick = 0;
  let last = 0;
  const rows = [];
  for (const t of times) {
    const dt = Math.max(0, t - last);
    last = t;
    const expected = expectedAt(t, offsetJump);
    let ticks = null;
    let age = 0;
    if (locked) {
      ticks = lock.frame(dt, expected - tick);
      if (ticks != null) {
        clock.reset();
        for (let k = 0; k < ticks; k++) tick++;
      }
    }
    if (ticks == null) {
      ticks = clock.advance(paceSeconds({ expected, tick, dt, pending: clock.pending }), () => tick++);
      // the view is drawn between the last two ticks at alpha = leftover x 60: (1 - alpha) ticks older than the newest one
      age = 1 - clock.pending * 60;
    }
    rows.push({ t, dt, ticks, tick, drift: expectedAt(t, offsetJump) - tick, age });
  }
  return rows;
}
// Pad-to-tick: a pad sample changes at time e; the first frame callback after e that runs a tick consumes it.
function padLatency(rows, events) {
  const out = [];
  let i = 0;
  for (const e of events) {
    while (i < rows.length && rows[i].t < e) i++;
    let j = i;
    while (j < rows.length && rows[j].ticks === 0) j++;
    if (j < rows.length) out.push(rows[j].t - e);
  }
  return out;
}
const mean = (a) => a.reduce((x, y) => x + y, 0) / a.length;
const pct = (a, p) => [...a].sort((x, y) => x - y)[Math.min(a.length - 1, Math.floor(a.length * p))];
const report = [];
const cases = [
  { name: '60 Hz, jitter 2 ms', hz: 60, jitter: 0.002 },
  { name: '60 Hz, jitter 2 ms, offset jumps 20 ms', hz: 60, jitter: 0.002, offsetJump: 0.02 },
  { name: '60 Hz, jitter 2 ms, 200 ms stall every 10 s', hz: 60, jitter: 0.002, stallEvery: 10, stall: 0.2 },
  { name: '59.94 Hz, jitter 1 ms', hz: 59.94, jitter: 0.001 },
  { name: '60.06 Hz, jitter 1 ms', hz: 60.06, jitter: 0.001 },
  { name: '120 Hz, jitter 1 ms', hz: 120, jitter: 0.001 },
  { name: '120 Hz, 200 ms stall every 10 s', hz: 120, jitter: 0.001, stallEvery: 10, stall: 0.2 }
];
for (const base of cases)
for (const phase of [0.02, 0.25, 0.5, 0.75, 0.98]) {
  const c = { ...base, name: `${base.name}, GO at vsync phase ${phase}` };
  // a full race: 4 minutes
  const times = frames({ seconds: 240, seed: 7, phase, ...base });
  const before = run(times, { locked: false, offsetJump: c.offsetJump });
  const after = run(times, { locked: true, offsetJump: c.offsetJump });
  const random = generator(99);
  const events = Array.from({ length: 4000 }, () => 1 + random() * 238).sort((a, b) => a - b);
  const latency = [padLatency(before, events), padLatency(after, events)];
  // 60 Hz frames (the ones tickLock locks) that ran 0 or 2 ticks
  const judder = (rows) => rows.filter((r) => r.dt >= 0.75 / 60 && r.dt <= 1.25 / 60 && r.ticks !== 1).length;
  // drift past the first 2 s after a stall (the catch-up runs then)
  const settled = (rows) => rows.filter((r) => !c.stallEvery || r.t % c.stallEvery > 2 || r.t < c.stallEvery);
  const maxDrift = (rows) => Math.max(...settled(rows).map((r) => Math.abs(r.drift)));
  // behind the server clock (a tick that runs late against it)
  const maxBehind = (rows) => Math.max(...settled(rows).map((r) => r.drift));
  const row = {
    case: c.name,
    judder: [judder(before), judder(after)],
    maxDrift: [maxDrift(before), maxDrift(after)].map((x) => +x.toFixed(2)),
    maxBehind: [maxBehind(before), maxBehind(after)].map((x) => +x.toFixed(2)),
    endDrift: [settled(before).at(-1).drift, settled(after).at(-1).drift].map((x) => +x.toFixed(2)),
    padMs: latency.map((l) => +(mean(l) * 1000).toFixed(2)),
    padP99Ms: latency.map((l) => +(pct(l, 0.99) * 1000).toFixed(2)),
    viewAgeTicks: [mean(before.map((r) => r.age)), mean(after.map((r) => r.age))].map((x) => +x.toFixed(3))
  };
  report.push(row);
  console.log(JSON.stringify(row));
  // the server clock: drift bounded by the pace slack (+1: within a frame) after any catch-up, and back after every stall
  assert.ok(row.maxDrift[1] <= SLACK + 1.01, `${c.name}: drift ${row.maxDrift[1]}`);
  assert.ok(Math.abs(row.endDrift[1]) <= SLACK + 1.01, `${c.name}: drift at the end ${row.endDrift[1]}`);
  // a locked tick does not run later against the server clock than today's (within the 2 ms jitter)
  assert.ok(row.maxBehind[1] <= row.maxBehind[0] + 0.15, `${c.name}: behind ${row.maxBehind}`);
  // no pad sample waits longer for its first tick than on today's path (mean and tail)
  assert.ok(row.padMs[1] <= row.padMs[0] + 0.05, `${c.name}: pad-to-tick ${row.padMs}`);
  assert.ok(row.padP99Ms[1] <= row.padP99Ms[0] + 0.05, `${c.name}: pad-to-tick p99 ${row.padP99Ms}`);
  if (c.hz === 120) {
    // not a 60 Hz frame: every frame keeps today's pacing
    assert.deepEqual(after.map((r) => r.ticks), before.map((r) => r.ticks), `${c.name}: 120 Hz frames are unchanged`);
  }
  // at 60 Hz the only 0- / 2-tick frames are the first unlocked ones and those around a stall's catch-up
  const stalls = c.stallEvery ? Math.floor(240 / c.stallEvery) : 0;
  if (c.hz === 60 && !c.offsetJump) assert.ok(row.judder[1] <= LOCK_STREAK + stalls * (LOCK_STREAK + 2), `${c.name}: judder ${row.judder}`);
}
// main.js: the online lock runs only with the switch, on the race pace's lag (mpGame.behind)
const main = sourceOf('main.js');
assert.ok(main.includes("if(online&&pv('tickLock')&&pv('mpTickLock'))lockedTicks=raceLock.frame(dt,mpGame.behind())"), 'main.js locks online frames behind pv mpTickLock');
assert.ok(sourceOf('net/mp-game.js').includes('return(Date.now()-goLocal)*0.06-pairNet.tick'), 'mpGame.behind is the race pace\'s lag');
console.log('mp tick lock: one tick per 60 Hz online frame, server clock kept, no later pad ticks OK');
