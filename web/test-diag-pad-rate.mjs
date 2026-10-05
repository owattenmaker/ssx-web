// web/diag-pad-rate.js: the field pad update rate (docs/crash-motion.md "pv padRing (removed) and the pad report rate").
import assert from 'node:assert/strict';
import { createPadRateWatch, REPORT_AFTER_S } from './diag-pad-rate.js';

function run(stepEvery) {
  const events = [];
  let beat = null;
  const watch = createPadRateWatch((kind, data) => events.push({ kind, ...data }), (s) => (beat = s));
  const pad = { id: '045e-0b22-Xbox Wireless Controller', mapping: '', axes: [0, 0, 0, 0, -1, -1, -9 / 7] };
  const info = () => ({ name: 'Xbox Wireless Controller', vendor: '045e', product: '0b22', layout: 'xbox-bt-firefox' });
  // centred: nothing counts
  for (let f = 0; f < 120; f++) watch.frame(pad, info, 1000 / 60);
  assert.equal(events.length, 0);
  // a stick circled; the browser updates it every `stepEvery` frames
  for (let f = 0; f < 60 * (REPORT_AFTER_S + 2); f++) {
    if (f % stepEvery === 0) pad.axes[0] = Math.cos(f / 20) * 0.9;
    pad.axes[1] = 0.9;
    watch.frame(pad, info, 1000 / 60);
  }
  return { events, beat };
}
{
  const { events, beat } = run(6);
  assert.equal(events.length, 1, 'one report after the off-centre time');
  const e = events[0];
  assert.equal(e.kind, 'pad-rate');
  assert.equal(e.mapping, '-');
  assert.equal(e.layout, 'xbox-bt-firefox');
  assert.ok(Math.abs(e.hz - 10) < 1, `a 10 Hz pad reads ${e.hz} Hz`);
  assert.ok(Math.abs(e.changedPct - 17) <= 1, `${e.changedPct}% of frames changed`);
  assert.equal(e.longest, 5);
  assert.ok(beat && beat.frames > e.frames, 'the heartbeats carry the running numbers');
  assert.ok(JSON.stringify(e).length < 300);
}
{
  const { events } = run(1);
  assert.ok(events[0].hz > 55 && events[0].changedPct === 100, 'a pad changing every frame');
}
console.log('diag-pad-rate: ok');
