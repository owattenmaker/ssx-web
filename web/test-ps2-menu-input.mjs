// The menu pad (web/gamepad-menus.js): the PS2 pad history 0x321298 against the ARMSX2 MCOMM captures
// (docs/ctm-decomp-screens.md; local/ctm-decomp/screens/caps/mcomm-repeat.json): a held Down moves at samples 31, 55, 67, 79, 91
// (press, +24, then every 12); 1-2 frame taps 3-5 frames apart move once per pair (200/203, 230-231/234-235, 260-261/265-266).
import assert from 'node:assert/strict';
const { createPadMenus } = await import('./gamepad-menus.js');
const FRAME = 1000 / 60;
const padWith = (down) => ({ buttons: Array.from({ length: 17 }, (_, i) => ({ pressed: i === 13 && down, value: i === 13 && down ? 1 : 0 })), axes: [0, 0, 0, 0] });
function run(downAt, frames) {
  const moves = [], m = createPadMenus({ menu: () => true, send: (t, c) => { if (t === 'keydown' && c === 'ArrowDown') moves.push(cur); } });
  let cur = 0;
  for (cur = 0; cur < frames; cur++) m.step(padWith(downAt(cur)), (cur + 1) * FRAME + 0.01);
  return moves;
}
// held from sample 31
assert.deepEqual(run((f) => f >= 31, 100), [31, 55, 67, 79, 91], 'held Down: press, +24, then every 12');
// the three tap pairs of the capture: one move each (at 200, 230, 260)
const taps = new Set([200, 203, 230, 231, 234, 235, 260, 261, 265, 266]);
assert.deepEqual(run((f) => taps.has(f), 300), [200, 230, 260], 'double taps inside the edge window count once');
// a button held when the menu opens does not press
{
  let inMenu = false; const sent = [];
  const m = createPadMenus({ menu: () => inMenu, send: (t, c) => sent.push(t + ' ' + c) });
  const cross = (on) => ({ buttons: Array.from({ length: 17 }, (_, i) => ({ pressed: i === 0 && on, value: i === 0 && on ? 1 : 0 })), axes: [0, 0, 0, 0] });
  for (let f = 0; f < 10; f++) m.step(cross(true), (f + 1) * FRAME + 0.01);
  inMenu = true;
  for (let f = 10; f < 20; f++) m.step(cross(true), (f + 1) * FRAME + 0.01);
  assert.deepEqual(sent, [], 'Cross held from before the menu does nothing');
  for (let f = 20; f < 30; f++) m.step(cross(false), (f + 1) * FRAME + 0.01);
  for (let f = 30; f < 40; f++) m.step(cross(true), (f + 1) * FRAME + 0.01);
  assert.deepEqual(sent.filter((s) => s.startsWith('keydown')), ['keydown Space'], 'a new press in the menu is Cross');
}
// the stick: past raw 176 (about 0.38) is held, 0.35 is not
{
  const moves = []; const m = createPadMenus({ menu: () => true, send: (t, c) => { if (t === 'keydown') moves.push(c); } });
  const stick = (y) => ({ buttons: Array.from({ length: 17 }, () => ({ pressed: false, value: 0 })), axes: [0, y, 0, 0] });
  for (let f = 0; f < 10; f++) m.step(stick(0.35), (f + 1) * FRAME + 0.01);
  assert.deepEqual(moves, [], '0.35 deflection: no move');
  for (let f = 10; f < 20; f++) m.step(stick(0.45), (f + 1) * FRAME + 0.01);
  assert.deepEqual(moves, ['ArrowDown'], '0.45 deflection moves once (the PS2 moved, the port at 0.5 did not)');
}
console.log('ps2 menu input: repeat 24/12, edge window, held-over buttons and the stick threshold match the PS2 captures');
