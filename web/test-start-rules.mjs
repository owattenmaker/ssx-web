// Start: when it pauses and when it is a menu's accept (pv startRules; web/start-rules.js). The matrix is the PS2 pad captures'
// (local/ps2-capture/menus/startprobe): the game update's pause gates 0x230A34 and the LUI's UINext (Cross or Start).
import assert from 'node:assert/strict';
import fs from 'node:fs';
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
{ // the pad: Start in a menu while a run is going is Enter (accept); a Start held into the menu from the ride is not
  const sent = []; let menu = false, accepts = false;
  const m = createPadMenus({ menu: () => menu, running: () => true, send: (t, c) => sent.push(t + ' ' + c), startAccepts: () => accepts });
  const pad = { buttons: Array.from({ length: 17 }, () => ({ value: 0, pressed: false })), axes: [0, 0, 0, 0] };
  const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  set(9, 1); m.step(pad, 0); menu = true; accepts = true; m.step(pad, 16); assert.deepEqual(sent, [], 'held from the ride: nothing');
  set(9, 0); m.step(pad, 32); set(9, 1); m.step(pad, 48); assert.deepEqual(sent.splice(0), ['keydown Enter'], 'a new Start in the menu = Enter');
  set(9, 0); m.step(pad, 64); sent.length = 0; accepts = false; set(9, 1); m.step(pad, 80); assert.deepEqual(sent, [], 'switch off: the old rule');
}
const main = fs.readFileSync(new URL('main.js', import.meta.url), 'utf8');
assert.match(main, /frameScreen==='game'&&startOpensPause\(/, 'pad Start pauses from the ride only (the screen of the previous frame)');
assert.match(main, /window\.addEventListener\('keydown',\(\)=>\{keyScreen=ui\.screen;keyPaused=paused;\},true\);/, 'keyboard: the screen before any menu handled the key');
const { PV_DEFAULTS } = await import('./pv-flags.js');
assert.equal(PV_DEFAULTS.startRules, true, 'startRules on');
console.log('start rules ok');
