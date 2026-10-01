// Start: when it pauses and when it is a menu's accept (pv startRules; web/start-rules.js). The matrix is the PS2 pad captures'
// (local/ps2-capture/menus/startprobe): the game update's pause gates 0x230A34 and the LUI's UINext (Cross or Start).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { sourceOf } from './test-source.mjs';
const { startOpensPause, startAccepts } = await import('./start-rules.js');
const { createPadMenus } = await import('./gamepad-menus.js');
const { setPv } = await import('./pv-flags.js');
setPv('ps2MenuInput', false);   // the port's pad model first; the PS2 pad history below
const ride = { screen: 'game', running: true, paused: false, finished: false, cutscene: false };
const rows = [
  ['riding / countdown / checkpoint', ride, true],
  ['FINISH! banner (finished)', { ...ride, finished: true }, false],
  ['a cutscene over the ride', { ...ride, cutscene: true }, false],
  ['transport ride (paused behind the loop)', { ...ride, paused: true }, false],
  ['the lodge', { ...ride, screen: 'ctm-lodge', paused: true }, false],
  ['the pause', { ...ride, screen: 'ctm-pause', paused: true }, false],
  ['results', { ...ride, screen: 'ctm-results', finished: true }, false],
  ['front end', { screen: 'main' }, false],
];
for (const [name, state, want] of rows) assert.equal(startOpensPause(state), want, name);
for (const s of ['pause', 'ctm-pause', 'ctm-mcomm', 'ctm-lodge', 'ctm-saveprompt', 'ctm-results', 'ctm-objectives', 'ctm-peaks', 'ctm-events', 'ctm-enterlodge', 'ctm-bcpause', 'pda-options', 'replay', 'main'])
  assert.equal(startAccepts(s), true, s + ': Start is the accept');
for (const s of ['game', 'loading', 'cutscene', 'transition']) assert.equal(startAccepts(s), false, s + ': no menu');
{ // the pad: Start in a menu while a run is going is Enter (accept); a Start held into the menu from the ride is not
  const sent = []; let menu = false, accepts = false;
  const m = createPadMenus({ menu: () => menu, running: () => true, send: (t, c) => sent.push(t + ' ' + c), startAccepts: () => accepts });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  set(9, 1); m.step(pad, 0); menu = true; accepts = true; m.step(pad, 16); assert.deepEqual(sent, [], 'held from the ride: nothing');
  set(9, 0); m.step(pad, 32); set(9, 1); m.step(pad, 48); assert.deepEqual(sent.splice(0), ['keydown Enter'], 'a new Start in the menu = Enter');
  set(9, 0); m.step(pad, 64); sent.length = 0; accepts = false; set(9, 1); m.step(pad, 80); assert.deepEqual(sent, [], 'switch off: the old rule');
}
{ // pv startConsume: the Start that accepts the card is spent until released, whichever loop sees it first (PS2 0x230A34 needs a new
  // press with no overlay / transition, 0x20CBE8): main.js's pad path asks taken('start'); a later, new press is the pause again
  const sent = []; let screen = 'ctm-objectives';
  const m = createPadMenus({ menu: () => screen !== 'game', running: () => true, send: (t, c) => { sent.push(t + ' ' + c); if (t === 'keydown' && c === 'Enter') screen = 'game'; }, startAccepts: () => startAccepts(screen) });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  // the game frame's own edge: pauses only on a Start it has not seen held, on the ride, and not taken by a menu
  let held = false, frameScreen = 'ctm-objectives'; const frame = () => { const down = pad.buttons[9].pressed; const p = down && !held && !m.taken('start') && frameScreen === 'game' && startOpensPause({ ...ride, screen }); held = down; frameScreen = screen; return p; };
  set(9, 1); m.step(pad, 0); assert.deepEqual(sent.splice(0), ['keydown Enter'], 'the card takes Start (Enter)');
  assert.equal(frame(), false, 'same frame'); assert.equal(m.taken('start'), true, 'spent while held');
  held = false; frameScreen = 'game'; assert.equal(frame(), false, 'the game frame sees the press only after the switch (Safari): still spent');
  m.step(pad, 16); set(9, 0); m.step(pad, 32); assert.equal(m.taken('start'), false, 'released: no longer spent');
  frame(); set(9, 1); m.step(pad, 48); assert.equal(frame(), true, 'a new Start on the ride pauses');
}
setPv('ps2MenuInput', true);
{ // pv ps2MenuInput: the same rules through the PS2 pad history (60 Hz updates, 3-update edge window: web/gamepad-menus.js)
  const sent = []; let menu = false, accepts = false, t = 0;
  const m = createPadMenus({ menu: () => menu, running: () => true, send: (ty, c) => sent.push(ty + ' ' + c), startAccepts: () => accepts });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  const f = (n = 1) => { for (let k = 0; k < n; k++) { t += 1000 / 60; m.step(pad, t); } };
  f(4); set(9, 1); f(); menu = true; accepts = true; f(4); assert.deepEqual(sent, [], 'held from the ride: nothing');
  set(9, 0); f(4); set(9, 1); f(); assert.deepEqual(sent.splice(0), ['keydown Enter'], 'a new Start in the menu = Enter');
  set(9, 0); f(4); sent.length = 0; accepts = false; set(9, 1); f(4); assert.deepEqual(sent, [], 'switch off: the old rule');
}
{ // pv ps2MenuInput + startConsume: the card's Start is spent until released
  const sent = []; let screen = 'ctm-objectives', t = 0;
  const m = createPadMenus({ menu: () => screen !== 'game', running: () => true, send: (ty, c) => { sent.push(ty + ' ' + c); if (ty === 'keydown' && c === 'Enter') screen = 'game'; }, startAccepts: () => startAccepts(screen) });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  const f = (n = 1) => { for (let k = 0; k < n; k++) { t += 1000 / 60; m.step(pad, t); } };
  f(4); set(9, 1); f(); assert.deepEqual(sent.splice(0), ['keydown Enter'], 'the card takes Start (Enter)');
  assert.equal(m.taken('start'), true, 'spent while held'); f(4); assert.equal(m.taken('start'), true);
  set(9, 0); f(4); assert.equal(m.taken('start'), false, 'released: no longer spent');
}
setPv('ps2MenuInput', null);
const main = sourceOf('main.js');
assert.match(main, /const padMenus=installPadMenus\(/, 'main.js keeps the pad menus');
assert.match(main, /!padMenus\?\.taken\?\.\('start'\)/, 'pad path: a spent Start does not pause');
assert.match(main, /&&running&&!e\.ssxPadMenu\)/, 'keyboard path: the pad menus\' keys do not pause');
assert.match(main, /frameScreen==='game'&&startOpensPause\(/, 'pad Start pauses from the ride only (the screen of the previous frame)');
assert.match(main, /window\.addEventListener\('keydown',\(\)=>\{keyScreen=ui\.screen;keyPaused=isPaused\(\);\},true\);/, 'keyboard: the screen before any menu handled the key');
console.log('start rules ok');
