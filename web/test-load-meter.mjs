// The load screens' meter (pv loadMeter, web/load-meter.js, web/loading-screen.js): stages weighted by their expected time (work by
// its measured time, downloads by bytes at the measured bandwidth), creep capped below a stage's end, a later plan never takes the
// meter back, the stage line and its reports, no minimum, 100% only when the load is done.
import assert from 'node:assert/strict';
import { createLoadMeter, CREEP_MAX, STAGE_MS, PENDING, DEFAULT_BANDWIDTH } from './load-meter.js';
import { LoadingScreen, stageText } from './loading-screen.js';
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

// download stages: bytes since the plan over the expected total, weighted at the measured bandwidth; an unknown file grows them
{
  t = 0;
  let bytes = 0;
  const m = createLoadMeter({ now: clock, received: () => bytes });
  m.plan([['files', { bytes: 1000000 }], ['warm', 1000]]);
  assert.equal(m.fraction(), 0);
  assert.deepEqual(m.current(0)?.id, 'files');
  // 1 MB at the default bandwidth weighs 1e6 / DEFAULT_BANDWIDTH ms
  bytes = 500000;
  t = 100;
  const w = 1000000 / DEFAULT_BANDWIDTH;
  assert.ok(Math.abs(m.fraction() - (w * 0.5) / (w + 1000)) < 0.02, String(m.fraction()));
  // more bytes than expected: the stage grows, it never reaches its end by bytes alone
  bytes = 3000000;
  t = 200;
  assert.ok(m.current(t).got === 3000000 && m.current(t).bytes > 3000000, JSON.stringify(m.current(t)));
  const f = m.fraction();
  assert.ok(f < 1);
  // the bandwidth: 3 MB arrived over 200 ms
  assert.ok(Math.abs(m.bandwidth - 15000) < 1);
  m.done('files');
  m.begin('warm');
  assert.equal(m.current(t).id, 'warm');
  // over the manifest's total, the stage counts what is still in flight: it keeps moving as those bytes arrive
  let coming = 2000000;
  const o = createLoadMeter({ now: clock, received: () => bytes, inflight: () => coming });
  bytes = 0;
  o.plan([['files', { bytes: 1000 }]]);
  bytes = 1000000;
  const f1 = o.fraction();
  bytes = 2000000;
  coming = 1000000;
  const f2 = o.fraction();
  bytes = 3000000;
  coming = 0;
  const f3 = o.fraction();
  assert.ok(f1 < f2 && f2 < f3 && f3 < 1, `${f1} ${f2} ${f3}`);
  // not downloading: the stage line skips the download stage
  const n = createLoadMeter({ now: clock, received: () => bytes });
  n.plan([['files', { bytes: 10 }], 'warm']);
  n.begin('warm');
  assert.equal(n.current(t, { downloading: false }).id, 'warm');
  assert.equal(n.current(t, { downloading: true }).id, 'files');
}

// a fraction standing still: the shown value keeps moving, at most 2% ahead of it
{
  t = 0;
  const m = createLoadMeter({ now: clock });
  m.plan([['a', 1000]]);
  m.step('a', 0.5);
  let v = 0;
  for (let k = 0; k <= 100; k++) {
    t = k * 100;
    v = m.value();
  }
  assert.ok(v > 0.5 && v <= 0.52 + 1e-9, String(v));
}

// the stage line
{
  assert.equal(stageText({ id: 'files', got: 42e6, bytes: 118e6 }), 'Downloading course 42 / 118 MB');
  assert.equal(stageText({ id: 'eventFiles', got: 1.25e6, bytes: 4e6 }), 'Downloading riders 1.3 / 4.0 MB');
  assert.equal(stageText({ id: 'warm', got: null, bytes: null }, { done: 31, total: 80 }), 'Building shaders 31 / 80');
  assert.equal(stageText({ id: 'lineup', got: null, bytes: null }), 'Preparing riders');
}

// the screen: with the switch, no minimum, its percentage is the work's (<= 99) until every promise has settled, then 100
{
  setPv('loadMeter', true);
  const reports = [];
  const ui = { set() {}, draw() {}, screen: 'loading', stage: null };
  const screen = new LoadingScreen(ui);
  screen.report = (kind, data) => reports.push([kind, data]);
  screen.data = { timing: { original_frames: 815, percent_curve: [[0, 0], [20, 2], [120, 17], [240, 27], [400, 52], [520, 97], [540, 98], [800, 98], [810, 100]] }, hints: [] };
  globalThis.requestAnimationFrame ??= () => 0;
  globalThis.location = new URL('http://x/');
  let resolve;
  const work = new Promise((r) => (resolve = r));
  screen.run(() => {}, [work]);
  assert.equal(screen.session.minMs, 0, 'no minimum with the switch');
  screen.plan(['rider', 'warm']);
  const s0 = screen.session.start;
  let st = screen.update(s0 + 16);
  assert.equal(st.percent, 0);
  screen.done('rider');
  screen.begin('warm');
  screen.step('warm', 0.5, 0, { done: 40, total: 80 });
  for (let k = 0; k < 40; k++) st = screen.update(s0 + 100 + k * 16);
  const expect = Math.floor((99 * (STAGE_MS.rider + 0.5 * STAGE_MS.warm)) / (STAGE_MS.rider + STAGE_MS.warm));
  assert.ok(st.percent >= expect - 1 && st.percent <= expect, `${st.percent} ~ ${expect}`);
  assert.equal(screen.session.stage.text, 'Building shaders 40 / 80');
  assert.equal(globalThis.ssxLoadStage, `warm ${st.percent}%`);
  // a stage over 5 s: one 'load-stage' report
  for (let k = 0; k < 10; k++) screen.update(s0 + 6000 + k * 100);
  assert.equal(reports.filter(([k]) => k === 'load-stage').length, 1);
  screen.done('warm');
  for (let k = 0; k < 60; k++) st = screen.update(s0 + 7100 + k * 16);
  assert.equal(st.percent, 99, 'every stage done, a promise still pending: 99');
  resolve();
  await work;
  await new Promise((r) => setTimeout(r, 0));
  st = screen.update(s0 + 8100);
  assert.equal(st.percent, 100);
  assert.equal(reports.filter(([k, d]) => k === 'load-stage' && d.ended).length, 1, 'the long stage reports its end');
  // ?loadingMs= still sets a minimum
  globalThis.location = new URL('http://x/?loadingMs=3000');
  const forced = new LoadingScreen(ui);
  forced.data = screen.data;
  forced.open();
  assert.equal(forced.session.minMs, 3000);
  globalThis.location = new URL('http://x/');
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
console.log('load meter: stage weights, creep, downloads, segments, monotonic value, stage line and reports, no minimum, 100% only when done OK');
