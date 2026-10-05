// web/pad-ring.js (pv padRing): each catch-up tick of a drawn frame reads the pad as of its own tick time, and the frame's last tick
// the live pad, so the ring adds no latency. Synthetic timestamps, no browser.
import assert from 'node:assert/strict';
import { createPadRing, padForTick, startPadPoller, PAD_RING_POLL_MS, PAD_RING_IDLE_MS } from './pad-ring.js';
import { buildPad } from './pad-input.js';
import { newStandardPad } from './gamepad-map.js';
import { sourceOf } from './test-source.mjs';

let clock = 0;
const live = newStandardPad();
const ring = createPadRing({ read: () => live, now: () => clock });
const setStick = (x) => {
  live.axes[0] = x;
};
const lx = (pad) => {
  const v = buildPad(() => false, pad);
  return Math.round((v[21] - v[20]) * 100) / 100;
};

// The stick moves every 4 ms poll; the ring keeps each change with the time it was seen.
for (let t = 0; t <= 36; t += PAD_RING_POLL_MS) {
  clock = t;
  setStick(t < 10 ? 0 : t < 20 ? 1 : -1);
  ring.sample();
}
assert.equal(ring.size, 3, 'only changes are kept');

// A 33.3 ms frame at 33.3 ms after the previous one: two ticks. Tick 0 is at 16.7 ms, tick 1 at the frame time.
const frameMs = 33.3;
const start = frameMs - 33.3;
const tickTime = (k) => start + ((k + 1) * 1000) / 60;
assert.equal(lx(padForTick(ring, tickTime(0), frameMs, live)), 1, 'the catch-up tick reads the pad of its own time (16.7 ms: right)');
assert.equal(lx(padForTick(ring, tickTime(1), frameMs, live)), -1, 'the last tick reads the live pad: no latency added');
// the live pad wins even when the ring has nothing newer yet (a change the poller has not seen)
setStick(0.9);
assert.equal(lx(padForTick(ring, tickTime(1), frameMs, live)), lx(live), 'the last tick is the live read');
assert.notEqual(lx(live), -1);
setStick(-1);
// without the ring (pv padRing off) every tick reads the frame's sample
assert.equal(lx(padForTick(null, tickTime(0), frameMs, live)), -1);
// before the oldest kept sample: the oldest one
assert.equal(lx(padForTick(ring, -5, frameMs, live)), 0);
// an empty ring falls back to the live pad
assert.equal(padForTick(createPadRing({ read: () => null, now: () => 0 }), 1, 33, live), live);

// The kept sample is a copy: the gamepad module reuses its standard pad object.
{
  const r = createPadRing({ read: () => live, now: () => 0 });
  setStick(0.5);
  r.sample();
  setStick(-0.5);
  assert.equal(r.at(0).axes[0], 0.5);
}

// The poller: every 4 ms while active, every 100 ms otherwise (and the ring cleared), stop() cancels.
{
  const timers = [];
  let active = true;
  let samples = 0;
  const r = { sample: () => samples++, clear: () => {}, size: 0 };
  const stop = startPadPoller(r, () => active, (fn, ms) => (timers.push([fn, ms]), timers.length), () => {});
  assert.equal(samples, 1);
  assert.equal(timers.at(-1)[1], PAD_RING_POLL_MS);
  active = false;
  timers.at(-1)[0]();
  assert.equal(samples, 1);
  assert.equal(timers.at(-1)[1], PAD_RING_IDLE_MS);
  stop();
  const n = timers.length;
  timers.at(-1)[0]();
  assert.equal(timers.length, n, 'a stopped poller does not re-arm');
}

// main.js: the multi-tick frame's ticks take padForTick behind pv padRing; the poller runs only during a visible ride.
{
  const main = sourceOf('main.js');
  assert.ok(main.includes("padForTick(pv('padRing')?padRing:null,t,ms,pollPads())"), 'stallKeyInput reads the ring per tick');
  assert.ok(main.includes("startPadPoller(padRing,()=>running&&!document.hidden&&pv('padRing'))"), 'the poller is gated on the ride and the switch');
}
console.log('pad-ring: ok');
