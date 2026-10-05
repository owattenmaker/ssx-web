// The event load screen's honest meter (pv loadMeter, web/load-meter.js, web/loading-screen.js): stages weighted by their time,
// creep capped below a stage's end, a later plan never takes the meter back, 100% only when the load is done.
import assert from 'node:assert/strict';
import { createLoadMeter, CREEP_MAX, STAGE_MS, PENDING } from './load-meter.js';
import { loadingPercent, LoadingScreen } from './loading-screen.js';
import { setPv } from './pv-flags.js';

let t = 0;
const clock = () => t;

// weights: a stage's share is its time over the plan's
{
  const m = createLoadMeter({ now: clock });
  m.plan([['a', 100], ['b', 300]]);
  assert.equal(m.fraction(), 0);
  m.done('a');
  assert.equal(m.fraction(), 0.25);
  m.step('b', 0.5);
  assert.equal(m.fraction(), 0.625);
  // a report never goes back
  m.step('b', 0.2);
  assert.equal(m.fraction(), 0.625);
  // unknown stages do nothing
  m.step('zzz', 1);
  m.done('zzz');
  assert.equal(m.fraction(), 0.625);
  m.done('b');
  assert.equal(m.fraction(), 1);
}

// a stage without reports creeps towards CREEP_MAX of itself over its typical time, never to its end
{
  t = 0;
  const m = createLoadMeter({ now: clock });
  m.plan([['a', 1000]]);
  m.begin('a');
  t = 500;
  assert.ok(Math.abs(m.fraction() - CREEP_MAX / 2) < 1e-9);
  t = 60000;
  assert.equal(m.fraction(), CREEP_MAX);
  m.done('a');
  assert.equal(m.fraction(), 1);
}

// a stage's first report below where its creep had got to does not take it back
{
  t = 0;
  const m = createLoadMeter({ now: clock });
  m.plan([['a', 1000]]);
  m.begin('a');
  t = 1000;
  const before = m.fraction();
  m.step('a', 0.1);
  assert.equal(m.fraction(), before);
  m.step('a', 0.95);
  assert.equal(m.fraction(), 0.95);
}

// between reports a stage creeps towards the next report's place (ceiling), never to it
{
  t = 0;
  const m = createLoadMeter({ now: clock });
  m.plan([['a', 1000]]);
  m.step('a', 0.2, 0.6);
  t = 200;
  const mid = m.fraction();
  assert.ok(mid > 0.2 && mid < 0.6);
  t = 100000;
  assert.ok(Math.abs(m.fraction() - (0.2 + 0.4 * CREEP_MAX)) < 1e-9);
  // the next report keeps what the creep had shown
  m.step('a', 0.3, 0.9);
  assert.ok(m.fraction() >= 0.2 + 0.4 * CREEP_MAX - 1e-9);
}

// a later plan (a world load going on into an event load) continues over what is left
{
  t = 0;
  const m = createLoadMeter({ now: clock });
  m.plan(['unload', 'course']);
  m.done('unload');
  m.done('course');
  assert.equal(m.fraction(), 1);
  m.plan(['rider', 'warm']);
  assert.ok(m.fraction() >= 0.99 && m.fraction() < 1, 'the new segment starts where the last ended, below 1');
  m.done('rider');
  m.done('warm');
  assert.equal(m.fraction(), 1);
  const half = createLoadMeter({ now: clock });
  half.plan([['a', 100], ['b', 100]]);
  half.done('a');
  half.plan([['c', 100]]);
  assert.equal(half.fraction(), 0.5);
  half.step('c', 0.5);
  assert.equal(half.fraction(), 0.75);
}

// a course switch's plan keeps a PENDING share for the event load: the event's plan takes its place, no jump
{
  t = 0;
  const m = createLoadMeter({ now: clock });
  m.plan([['unload', 100], ['course', 500], [PENDING, 400]]);
  m.done('unload');
  m.done('course');
  assert.equal(m.fraction(), 0.6);
  m.plan([['rider', 100], ['warm', 300]], { showing: 0.3 });
  assert.equal(m.fraction(), 0.6, 'the event stages share the pending weight');
  m.done('rider');
  assert.equal(m.fraction(), 0.7);
  m.done('warm');
  assert.equal(m.fraction(), 1);
  // a new segment starts where the screen is when the pace held it below the work
  const n = createLoadMeter({ now: clock });
  n.plan([['a', 100]]);
  n.done('a');
  n.plan([['b', 100]], { showing: 0.4 });
  assert.equal(n.fraction(), 0.4);
}

// the shown value eases towards the fraction and never goes backwards
{
  t = 0;
  const m = createLoadMeter({ now: clock });
  m.plan(['rider', 'warm']);
  let last = 0;
  for (let i = 0; i <= 200; i++) {
    t = i * 16;
    if (i === 20) m.done('rider');
    if (i > 20) m.step('warm', (i - 20) / 180);
    const v = m.value();
    assert.ok(v >= last && v <= 1);
    last = v;
  }
  assert.equal(last, 1);
  assert.ok(STAGE_MS.warm > STAGE_MS.rider);
}

// the climb curve: the PS2 curve to its first 98% over the minimum, a pace limit (98 exactly as the minimum ends)
{
  const curve = [[0, 0], [20, 2], [120, 17], [240, 27], [400, 52], [520, 97], [540, 98], [800, 98], [810, 100]];
  let prev = 0;
  for (let f = 0; f <= 420; f += 5) {
    const p = loadingPercent(curve, f, 420, 815, false, true);
    assert.ok(p >= prev && p <= 98);
    prev = p;
  }
  assert.ok(loadingPercent(curve, 410, 420, 815, false, true) < 98);
  assert.equal(loadingPercent(curve, 420, 420, 815, false, true), 98);
  // the old pacing reaches 98% at two thirds of the minimum
  assert.equal(loadingPercent(curve, 280, 420, 815, false), 98);
}

// the screen: with the switch, its percentage is the work's (<= 98) until every promise has settled, then 100
{
  setPv('loadMeter', true);
  const ui = { set() {}, draw() {}, screen: 'loading', stage: null };
  const screen = new LoadingScreen(ui);
  screen.data = { timing: { original_frames: 815, percent_curve: [[0, 0], [20, 2], [120, 17], [240, 27], [400, 52], [520, 97], [540, 98], [800, 98], [810, 100]] }, hints: [] };
  globalThis.requestAnimationFrame ??= () => 0;
  globalThis.location = new URL('http://x/?loadingMs=1000');
  let resolve;
  const work = new Promise((r) => (resolve = r));
  screen.run(() => {}, [work]);
  screen.plan(['rider', 'warm']);
  const s0 = screen.session.start;
  // well past the minimum: the curve allows 98, the work holds the number
  let st = screen.update(s0 + 5000);
  assert.equal(st.percent, 0);
  screen.done('rider');
  screen.step('warm', 0.5);
  for (let k = 0; k < 40; k++) st = screen.update(s0 + 5000 + k * 16);
  const expect = Math.floor(99 * (STAGE_MS.rider + 0.5 * STAGE_MS.warm) / (STAGE_MS.rider + STAGE_MS.warm));
  assert.ok(st.percent > 0 && st.percent <= expect, `${st.percent} <= ${expect}`);
  screen.done('warm');
  for (let k = 0; k < 60; k++) st = screen.update(s0 + 6000 + k * 16);
  assert.equal(st.percent, 98, 'every stage done, a promise still pending: the curve top, 98');
  resolve();
  await work;
  await new Promise((r) => setTimeout(r, 0));
  st = screen.update(s0 + 7000);
  assert.equal(st.percent, 100);
  // a world load (no continuation yet) shows its work; a continuation attached later paces on from the number shown
  const later = new LoadingScreen(ui);
  later.data = screen.data;
  later.open();
  later.plan([['course', 100]]);
  const l0 = later.session.start;
  later.step('course', 0.6);
  for (let k = 0; k < 40; k++) st = later.update(l0 + 100 + k * 16);
  const shownBefore = st.percent;
  assert.ok(shownBefore >= 55, 'no pace before a continuation: ' + shownBefore);
  later.done('course');
  later.run(() => {}, []);
  later.plan(['rider']);
  later.done('rider');
  let lastPct = shownBefore;
  for (let k = 0; k < 70; k++) {
    st = later.update(l0 + 800 + k * 2);
    assert.ok(st.percent >= lastPct, 'never back');
    lastPct = st.percent;
  }
  assert.ok(lastPct > shownBefore && lastPct < 98, 'paced on from the number shown towards 98 at the minimum: ' + lastPct);
  st = later.update(l0 + 1000);
  assert.ok(st.percent >= 98);
  // a world load ends on 100% and the fade as soon as its work has settled, then its continuation (finish: no minimum)
  const world = new LoadingScreen(ui);
  world.data = screen.data;
  world.open();
  world.plan(['unload', 'course']);
  const w0 = world.session.start;
  world.done('unload');
  world.done('course');
  let went = 0;
  assert.equal(world.finish(() => went++), true);
  assert.equal(world.finish(() => went++), false, 'one continuation');
  st = world.update(w0 + 100);
  assert.equal(st.percent, 100);
  for (let k = 0; k < 40 && world.session; k++) world.update(w0 + 100 + k * 17);
  assert.equal(went, 1);
  assert.equal(world.session, null);
  setPv('loadMeter', null);
  assert.equal(new LoadingScreen(ui).finish(() => {}), false, 'off or no session: nothing to finish');
  delete globalThis.location;
}
console.log('load meter: stage weights, creep, segments, monotonic value, climb curve, 100% only when done OK');
