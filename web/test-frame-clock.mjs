import assert from 'node:assert/strict';
import {FixedStepClock} from './fixed-step-clock.js';
// Exercise the scheduler imported by main.js, including the slow render rates
// excluded by the original WASM-only 30..240 Hz fixture.
for (const hz of [5, 10, 15, 20, 30, 60, 120, 144, 240]) {
  const clock = new FixedStepClock();
  let ticks = 0;
  for (let i = 0; i < hz * 10; i++) clock.advance(1 / hz, () => ticks++);
  assert.equal(ticks, 600, `${hz} Hz lost simulation time`);
  assert(clock.pending < 1e-8);
}
const clock = new FixedStepClock();
let ticks = 0;
let elapsed = 0;
for (const dt of [.016, .130, .008, .080, .016, .500, .010, .100]) {
  elapsed += dt;
  const before = ticks;
  clock.advance(dt, () => ticks++);
  assert(ticks - before <= 12, 'unbounded catch-up work');
  assert(Math.abs(ticks / 60 + clock.pending - elapsed) < 1e-8, 'elapsed time discarded');
}
while(clock.pending >= clock.step) clock.advance(0, () => ticks++);
assert.equal(ticks, Math.floor(elapsed * 60 + 1e-8));
clock.reset();
assert.equal(clock.pending, 0);
console.log('Live frame scheduler: 5..240 Hz and uneven frames preserve elapsed time; catch-up work is bounded.');
// PS2 frame pacing after a slow frame (web/ps2-frame-pacing.js, pv stallCap): measured with stall savestates (Crow's Nest,
// tick 700 spun for K extra vblanks; docs/HANDOFF.md "Air release"): catch-up updates run back to back after the slow frame.
// K = 30 is left out: its ring write index lapped onto the read index (a sub-frame race; the PS2 ran 11, the lap model 0).
{
  const { ps2FrameTime } = await import('./ps2-frame-pacing.js');
  const PS2 = { 2: 2, 4: 4, 11: 11, 12: 11, 13: 11, 16: 11, 20: 11, 29: 11, 31: 1, 35: 5, 40: 10, 45: 11, 50: 11, 59: 11, 60: 0, 90: 0, 120: 0 };
  for (const [k, catchUp] of Object.entries(PS2)) {
    const c = new FixedStepClock(); let n = 0;
    for (let i = 0; i < 30; i++) c.advance(ps2FrameTime(1 / 60), () => n++);
    assert.equal(n, 30, 'steady 60 Hz: one update a frame');
    const before = n; c.advance(ps2FrameTime((+k + 1) / 60), () => n++);
    assert.equal(n - before - 1, catchUp, `K = ${k}: ${n - before - 1} catch-up updates, the PS2 ran ${catchUp}`);
    for (let i = 0; i < 5; i++) { const b = n; c.advance(ps2FrameTime(1 / 60), () => n++); assert.equal(n - b, 1, `K = ${k}: debt kept after the slow frame`); }
  }
  for (const hz of [30, 60, 120, 144]) { const c = new FixedStepClock(); let n = 0; for (let i = 0; i < hz * 10; i++) c.advance(ps2FrameTime(1 / hz), () => n++); assert.equal(n, 600, `${hz} Hz`); }
  console.log('PS2 frame pacing: at most 12 updates a drawn frame, the backlog dropped, 30-slot ring lap (17 stall savestates).');
}
