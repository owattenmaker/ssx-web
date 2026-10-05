// web/diag-bail.js: the field bail reports (docs/crash-motion.md "Field bail reports"), on a fake core.
import assert from 'node:assert/strict';
import { createBailWatch, packPad, BAIL_FOLLOW, BAIL_PADS } from './diag-bail.js';

function fakeCore() {
  const buffer = new ArrayBuffer(4096);
  const core = {
    HEAPF32: new Float32Array(buffer),
    HEAPU8: new Uint8Array(buffer),
    tick: 0,
    _crash_info: () => 0,
    _reset_info: () => 64,
    _score_object_dump: () => 1024,
    _game_tick: () => core.tick
  };
  return core;
}
const crash = (core) => new Float32Array(core.HEAPF32.buffer, 0, 12);
const reset = (core) => new Float32Array(core.HEAPF32.buffer, 64, 9);
const score = (core) => new Uint32Array(core.HEAPU8.buffer, 1024, 0x1d0 / 4);

// packPad: the buttons as a mask, sticks as -100..100 (left x = channels 21 - 20, left y = 23 - 22)
{
  const input = new Float32Array(24);
  input[10] = 1;
  input[15] = 0.5;
  input[21] = 1;
  input[22] = 0.5;
  assert.deepEqual(packPad(input, 2), [(1 << 10) | (1 << 15), 100, -50, 0, 0, 2]);
}

// A crash: the velocity into it, the launch after it, the angle between them, the pads and the attacked flag.
{
  const events = [];
  const core = fakeCore();
  const watch = createBailWatch((kind, data) => events.push({ kind, ...data }));
  const state = new Float32Array(16);
  const input = new Float32Array(24);
  let x = 0;
  let z = 0;
  const step = (dx, dz, ticksLeft = 1) => {
    x += dx;
    z += dz;
    state[0] = x;
    state[2] = z;
    core.tick++;
    watch.tick(core, state, input, ticksLeft);
  };
  for (let i = 0; i < 10; i++) step(0.5, 0, i % 2 ? 1 : 2);
  // the crash enters on this tick, attacked; the rider then goes sideways (+z)
  crash(core)[5] = 1;
  crash(core)[3] = 360;
  crash(core)[11] = 812.5;
  score(core)[0x12c / 4] = 1;
  step(0, 0.5);
  for (let i = 1; i < BAIL_FOLLOW; i++) step(0, 0.5);
  assert.equal(events.length, 1);
  const e = events[0];
  assert.equal(e.kind, 'bail');
  assert.equal(e.sem, 360);
  assert.equal(e.impact, 812.5);
  assert.equal(e.attacked, true);
  assert.deepEqual(e.vIn, [30, 0, 0]);
  assert.deepEqual(e.vOut, [0, 0, 30]);
  assert.equal(e.turnDeg, 90);
  assert.equal(e.turnHDeg, 90);
  assert.equal(e.pads.length, BAIL_PADS);
  assert.equal(e.multi, 3);
  assert.equal(e.tick, 11);
  // a forced reset
  reset(core)[2] = 1;
  reset(core)[4] = 2;
  step(0, 0);
  assert.equal(events.at(-1).kind, 'reset');
  assert.equal(events.at(-1).reason, 2);
  assert.ok(JSON.stringify(events[0]).length < 400, 'the bail event stays small');
}

// A placement inside the follow window: no launch direction.
{
  const events = [];
  const core = fakeCore();
  const watch = createBailWatch((kind, data) => events.push({ kind, ...data }));
  const state = new Float32Array(16);
  const input = new Float32Array(24);
  for (let i = 0; i < 4; i++) watch.tick(core, state, input, 1);
  crash(core)[5] = 1;
  watch.tick(core, state, input, 1);
  state[13] = 1;
  for (let i = 1; i < BAIL_FOLLOW; i++) watch.tick(core, state, input, 1);
  assert.equal(events.length, 1);
  assert.equal(events[0].placed, true);
  assert.equal(events[0].vOut, undefined);
}

// The session cap.
{
  const events = [];
  const core = fakeCore();
  const watch = createBailWatch((kind, data) => events.push({ kind, ...data }), { max: 3, follow: 1 });
  const state = new Float32Array(16);
  const input = new Float32Array(24);
  for (let i = 0; i < 10; i++) {
    crash(core)[5] = i;
    watch.tick(core, state, input, 1);
  }
  assert.equal(events.length, 3);
}
console.log('diag-bail: ok');
