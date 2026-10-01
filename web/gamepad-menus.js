// Gamepad -> the keys the front-end screens read (ui.js, fe-screens.js, character-select.js, career-ui.js, ...), the
// same table as the touch deck (web/touch-controls.js MENU_KEYS): D-pad / left stick = arrows, Cross = Space,
// Triangle = Escape (back), Square = Shift, Circle = Backspace, L1/R1/L2/R2 = Q/E/Z/X, Start = Enter.
// Off the race screen only; in a race the pad goes through main.js inputs() -> buildPad() and Start pauses there, so
// Start is not sent as Enter while a run is going. A key is sent on a new press made while a menu is up (a button
// still held from the race when the pause menu opens does nothing), directions repeat while held like a PS2 menu.
import { MENU_KEYS } from './touch-controls.js';
import { pollPads } from './gamepad.js';
import { startAccepts as startMenu } from './start-rules.js';

const KEY_NAMES = {
  Space: ' ',
  Escape: 'Escape',
  ShiftLeft: 'Shift',
  Backspace: 'Backspace',
  KeyQ: 'q',
  KeyE: 'e',
  KeyZ: 'z',
  KeyX: 'x',
  ArrowUp: 'ArrowUp',
  ArrowDown: 'ArrowDown',
  ArrowLeft: 'ArrowLeft',
  ArrowRight: 'ArrowRight',
  Enter: 'Enter'
};
// standard button index -> menu control
const BUTTON_CONTROLS = [[0, 'cross'], [1, 'circle'], [2, 'square'], [3, 'triangle'], [4, 'l1'], [5, 'r1'], [6, 'l2'], [7, 'r2'], [9, 'start'], [12, 'up'], [13, 'down'], [14, 'left'], [15, 'right']];
const DIRECTIONS = new Set(['up', 'down', 'left', 'right']);
// The PS2 pad history (docs/ctm-decomp-screens.md): 0x321298 0x321298 (INPUT.MAP UIUp / UIDown = DPad*.repeat ||
// LStick*.repeat) moves on the press, then after 24 frames, then every 12 (0 / 400 / 200 ms; ARMSX2 MCOMM capture: samples 31,
// 55, 67, 79, 91); the stick counts as held past raw < 79 / > 176 of 0..255 (about 0.38 of full deflection).
export const MENU_REPEAT_PS2 = Object.freeze({ first: 400, next: 200, stick: 0.38 });

// The menu controls held on a standard-layout pad.
export function menuControls(pad) {
  const out = new Set(); if (!pad) return out;
  for (const [i, c] of BUTTON_CONTROLS) { const b = pad.buttons?.[i]; if (b && (b.pressed || b.value > 0.5)) out.add(c); }
  const x = +pad.axes?.[0] || 0, y = +pad.axes?.[1] || 0, t = MENU_REPEAT_PS2.stick;
  if (y < -t) out.add('up'); if (y > t) out.add('down'); if (x < -t) out.add('left'); if (x > t) out.add('right');
  return out;
}

// Pure stepper (tested in node): feed it the pad each frame; send(type, code) dispatches.
// startAccepts(): startRules (web/start-rules.js): Start is the menu's accept (UINext = Cross or Start) on every menu screen, also
// while a run is going (the pause, MCOMM, results, the lodge); only the ride itself leaves it to main.js.
export function createPadMenus(options = {}) {
  const ps2 = createPs2PadMenus(options);
  return {
    get held() { return ps2.held; },
    // onFrame(): the UI frame counter's pass (web/screen-phases.js step), run before each 60 Hz pad update so a key meets the phase of
    // its own frame
    step(pad, now, onFrame = null) { ps2.step(pad, now, onFrame); },
    releaseAll() { ps2.releaseAll(); },
    taken(c) { return ps2.taken(c); },
  };
}

// The PS2 pad history 0x321298 (engine/original_input.cpp originalUpdatePad, bit-exact there), one update per
// 60 Hz frame on 24 channels (buttons, d-pad and left-stick directions each their own): after any edge (press or release) the next
// three updates ignore the input (edgeAge < 3: pressed = released = 0, held kept), so a tap inside that window is seen only if the
// button is still down when the channel looks again; a held channel repeats on the press, 24 updates later, then every 12. The menus'
// UIUp / UIDown / ... = DPad*.repeat || LStick*.repeat, UINext = Cross.pressed || Start.pressed, UIBack = Triangle.pressed
// (docs/ctm-decomp-screens.md; ARMSX2 MCOMM capture: 1-2 frame taps 3-5 frames apart give one move per pair). The channels keep
// updating outside the menus, so a button still held from the race does not press when a menu opens.
const FRAME_MS = 1000 / 60;
const stickRaw = (a) => Math.max(0, Math.min(255, Math.round(128 + a * 127.5)));   // browser axis -> the pad's analog byte
function ps2Held(pad) {
  const out = new Map(); if (!pad) return out;
  for (const [i, c] of BUTTON_CONTROLS) { const b = pad.buttons?.[i]; out.set(c, !!(b && (b.pressed || b.value > 0))); }
  const x = stickRaw(+pad.axes?.[0] || 0), y = stickRaw(+pad.axes?.[1] || 0);   // originalDecodePad: negative raw < 79, positive raw > 176
  out.set('s-left', x < 79); out.set('s-right', x > 176); out.set('s-up', y < 79); out.set('s-down', y > 176);
  return out;
}
function createPs2PadMenus({ menu = () => true, running = () => false, send = () => {}, startAccepts = () => false } = {}) {
  const channels = new Map();   // channel -> { held, pressed, released, repeat, timer, age }
  const held = new Map();       // control -> { code } keys sent down (buttons)
  const spent = new Set();
  let last = null;
  const channel = (c) => { let ch = channels.get(c); if (!ch) { ch = { held: 0, pressed: 0, released: 0, repeat: 0, timer: 0, age: 0 }; channels.set(c, ch); } return ch; };
  function update(values) {
    for (const [c, v] of values) {
      const ch = channel(c);
      if (ch.age < 3) { ch.age++; ch.pressed = ch.released = 0; }
      else { const now = v ? 1 : 0; if (now === ch.held) { ch.pressed = ch.released = 0; } else { ch.held = now; ch.pressed = now; ch.released = 1 - now; ch.age = 0; } }
      if (ch.held) { if (ch.timer === 0) { ch.repeat = 1; ch.timer = 24; } else { ch.timer--; if (ch.timer > 0) ch.repeat = 0; else { ch.repeat = 1; ch.timer = 12; } } }
      else { ch.repeat = 0; ch.timer = 0; }
    }
  }
  function release(c) { const h = held.get(c); if (!h) return; held.delete(c); send('keyup', h.code); }
  function emit(inMenu) {
    for (const c of [...spent]) if (!channel(c).held && !(DIRECTIONS.has(c) && channel('s-' + c).held)) spent.delete(c);
    if (!inMenu) { for (const c of [...held.keys()]) release(c); return; }
    for (const [, c] of BUTTON_CONTROLS) {
      const code = MENU_KEYS[c]; if (!code) continue;
      if (DIRECTIONS.has(c)) {
        if (channel(c).repeat || channel('s-' + c).repeat) { send('keydown', code); send('keyup', code); spent.add(c); }
        continue;
      }
      const ch = channel(c);
      if (ch.released) release(c);
      if (ch.pressed) {
        if (c === 'start' && running() && !startAccepts()) continue;   // Start pauses / resumes a run (main.js frame)
        send('keydown', code); held.set(c, { code }); spent.add(c);
      }
    }
  }
  return {
    held,
    step(pad, now, onFrame = null) {
      if (last === null || now - last > 250) last = now - FRAME_MS;   // first call or a long gap (background tab): one update
      const values = ps2Held(pad), inMenu = !!menu();
      const n = Math.min(4, Math.round((now - last) / FRAME_MS));   // 60 updates a second whatever the display rate (120 Hz: 1, 0, 1, ...)
      for (let k = 0; k < n; k++) { last += FRAME_MS; onFrame?.(); update(values); emit(inMenu); }
    },
    releaseAll() { for (const c of [...held.keys()]) release(c); },
    taken(c) { return spent.has(c); },
  };
}

// Browser: a frame loop of its own, so the menus answer the pad before the game data has loaded.
// phases: the UI frame counter (web/screen-phases.js): one pass per PS2 pad update, before its keys, so each key meets its own frame's
// phase.
export function installPadMenus({ screen = () => 'title', isRunning = () => false, phases = null } = {}) {
  if (typeof window === 'undefined' || typeof requestAnimationFrame !== 'function') return null;
  // ssxPadMenu: a menu key made from the pad (main.js: never the keyboard's pause)
  const send = (type, code) => {
    try {
      const e = new KeyboardEvent(type, { code, key: KEY_NAMES[code] || code, bubbles: true, cancelable: true });
      e.ssxPadMenu = true;
      window.dispatchEvent(e);
    } catch {}
  };
  const menus = createPadMenus({ menu: () => screen() !== 'game', running: isRunning, send, startAccepts: () => startMenu(screen()) });
  // A held arrow key repeats like the pad's directions (the PS2 keyboard-less menus repeat UIUp / UIDown at 24 / 12
  // frames); the browser's own auto-repeat keydowns (e.repeat) stay ignored by the screens, these arrive as fresh presses.
  const ARROWS = new Set(['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight']), keys = new Map(); // code -> next repeat (ms)
  const keySend = (type, code) => {
    try {
      const e = new KeyboardEvent(type, { code, key: KEY_NAMES[code] || code, bubbles: true, cancelable: true });
      e.ssxKeyRepeat = true;
      window.dispatchEvent(e);
    } catch {}
  };
  addEventListener('keydown', (e) => {
    if (e.ssxKeyRepeat || e.ssxPadMenu || e.repeat || !ARROWS.has(e.code) || screen() === 'game') return;
    keys.set(e.code, performance.now() + MENU_REPEAT_PS2.first);
  });
  addEventListener('keyup', (e) => { if (!e.ssxKeyRepeat) keys.delete(e.code); });
  const stepKeys = (t) => {
    if (!keys.size) return; if (screen() === 'game') { keys.clear(); return; }
    for (const [code, next] of keys) if (t >= next) { keys.set(code, t + MENU_REPEAT_PS2.next); keySend('keyup', code); keySend('keydown', code); }
  };
  const pass = phases ? () => phases.step() : null;
  const loop = (t) => { try { menus.step(pollPads(), t, pass); stepKeys(t); } catch (e) { console.warn('pad menus', e); } requestAnimationFrame(loop); };
  requestAnimationFrame(loop);
  addEventListener('blur', () => { menus.releaseAll(); keys.clear(); });
  return menus;
}
