// Start: when it pauses and when it is a menu's accept (pv startRules; web/start-rules.js). The matrix is the PS2 pad captures'
// (local/ps2-capture/menus/startprobe): the game update's pause gates 0x230A34 and the LUI's UINext (Cross or Start).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { sourceOf } from './test-source.mjs';
const { startOpensPause, startAccepts } = await import('./start-rules.js');
const { createPadMenus } = await import('./gamepad-menus.js');
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
{ // the pad (the PS2 pad history (60 Hz updates, 3-update edge window: web/gamepad-menus.js)
  const sent = []; let menu = false, accepts = false, t = 0;
  const m = createPadMenus({ menu: () => menu, running: () => true, send: (ty, c) => sent.push(ty + ' ' + c), startAccepts: () => accepts });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  const f = (n = 1) => { for (let k = 0; k < n; k++) { t += 1000 / 60; m.step(pad, t); } };
  f(4); set(9, 1); f(); menu = true; accepts = true; f(4); assert.deepEqual(sent, [], 'held from the ride: nothing');
  set(9, 0); f(4); set(9, 1); f(); assert.deepEqual(sent.splice(0), ['keydown Enter'], 'a new Start in the menu = Enter');
  set(9, 0); f(4); sent.length = 0; accepts = false; set(9, 1); f(4); assert.deepEqual(sent, [], 'switch off: the old rule');
}
{ // startConsume: the card's Start is spent until released
  const sent = []; let screen = 'ctm-objectives', t = 0;
  const m = createPadMenus({ menu: () => screen !== 'game', running: () => true, send: (ty, c) => { sent.push(ty + ' ' + c); if (ty === 'keydown' && c === 'Enter') screen = 'game'; }, startAccepts: () => startAccepts(screen) });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  const f = (n = 1) => { for (let k = 0; k < n; k++) { t += 1000 / 60; m.step(pad, t); } };
  f(4); set(9, 1); f(); assert.deepEqual(sent.splice(0), ['keydown Enter'], 'the card takes Start (Enter)');
  assert.equal(m.taken('start'), true, 'spent while held'); f(4); assert.equal(m.taken('start'), true);
  set(9, 0); f(4); assert.equal(m.taken('start'), false, 'released: no longer spent');
}
const main = sourceOf('main.js');
assert.match(main, /const padMenus=installPadMenus\(/, 'main.js keeps the pad menus');
assert.match(main, /!padMenus\?\.taken\?\.\('start'\)/, 'pad path: a spent Start does not pause');
assert.match(main, /&&running&&!e\.ssxPadMenu\)/, 'keyboard path: the pad menus\' keys do not pause');
assert.match(main, /frameScreen==='game'&&startOpensPause\(/, 'pad Start pauses from the ride only (the screen of the previous frame)');
assert.match(main, /window\.addEventListener\('keydown',\(\)=>\{keyScreen=ui\.screen;keyPaused=isPaused\(\);\},true\);/, 'keyboard: the screen before any menu handled the key');
console.log('start rules ok');
