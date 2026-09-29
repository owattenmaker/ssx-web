// Gamepad -> the keys the front-end screens read (ui.js, fe-screens.js, character-select.js, career-ui.js, ...), the
// same table as the touch deck (web/touch-controls.js MENU_KEYS): D-pad / left stick = arrows, Cross = Space,
// Triangle = Escape (back), Square = Shift, Circle = Backspace, L1/R1/L2/R2 = Q/E/Z/X, Start = Enter.
// Off the race screen only; in a race the pad goes through main.js inputs() -> buildPad() and Start pauses there, so
// Start is not sent as Enter while a run is going. A key is sent on a new press made while a menu is up (a button
// still held from the race when the pause menu opens does nothing), directions repeat while held like a PS2 menu.
import { MENU_KEYS } from './touch-controls.js';
import { pollPads } from './gamepad.js';
import { pv } from './pv-flags.js';
import { startAccepts as startMenu } from './start-rules.js';

const KEY_NAMES = { Space: ' ', Escape: 'Escape', ShiftLeft: 'Shift', Backspace: 'Backspace', KeyQ: 'q', KeyE: 'e', KeyZ: 'z', KeyX: 'x', ArrowUp: 'ArrowUp', ArrowDown: 'ArrowDown', ArrowLeft: 'ArrowLeft', ArrowRight: 'ArrowRight', Enter: 'Enter' };
// standard button index -> menu control
const BUTTON_CONTROLS = [[0, 'cross'], [1, 'circle'], [2, 'square'], [3, 'triangle'], [4, 'l1'], [5, 'r1'], [6, 'l2'], [7, 'r2'], [9, 'start'], [12, 'up'], [13, 'down'], [14, 'left'], [15, 'right']];
const DIRECTIONS = new Set(['up', 'down', 'left', 'right']);
export const MENU_REPEAT = Object.freeze({ first: 400, next: 110, stick: 0.5 });

// The menu controls held on a standard-layout pad.
export function menuControls(pad) {
  const out = new Set(); if (!pad) return out;
  for (const [i, c] of BUTTON_CONTROLS) { const b = pad.buttons?.[i]; if (b && (b.pressed || b.value > 0.5)) out.add(c); }
  const x = +pad.axes?.[0] || 0, y = +pad.axes?.[1] || 0, t = MENU_REPEAT.stick;
  if (y < -t) out.add('up'); if (y > t) out.add('down'); if (x < -t) out.add('left'); if (x > t) out.add('right');
  return out;
}

// Pure stepper (tested in node): feed it the pad each frame; send(type, code) dispatches.
// startAccepts(): pv startRules (web/start-rules.js): Start is the menu's accept (UINext = Cross or Start) on every menu screen, also
// while a run is going (the pause, MCOMM, results, the lodge); only the ride itself leaves it to main.js.
export function createPadMenus({ menu = () => true, running = () => false, send = () => {}, startAccepts = () => false } = {}) {
  const held = new Map();         // control -> { code, next }
  const spent = new Set();        // pv startConsume: controls whose press a menu took, until released (main.js pause: taken('start'))
  let prev = new Set();
  function release(c) { const h = held.get(c); if (!h) return; held.delete(c); send('keyup', h.code); }
  return {
    held,
    step(pad, now) {
      const controls = menuControls(pad), inMenu = !!menu();
      for (const c of [...spent]) if (!controls.has(c)) spent.delete(c);
      for (const c of [...held.keys()]) if (!controls.has(c) || !inMenu) release(c);
      if (inMenu) for (const c of controls) {
        if (prev.has(c) || held.has(c)) continue;
        if (c === 'start' && running() && !startAccepts()) continue;   // Start pauses / resumes a run (main.js frame)
        const code = MENU_KEYS[c]; if (!code) continue;
        send('keydown', code); held.set(c, { code, next: DIRECTIONS.has(c) ? now + MENU_REPEAT.first : Infinity }); spent.add(c);
      }
      for (const [c, h] of held) if (now >= h.next) { send('keyup', h.code); send('keydown', h.code); h.next = now + MENU_REPEAT.next; }   // menus ignore key repeats: fresh presses
      prev = controls;
    },
    releaseAll() { for (const c of [...held.keys()]) release(c); },
    // pv startConsume: this control's current press was a menu key (a card / prompt accept): the game must not also act on it
    taken(c) { return spent.has(c); },
  };
}

// Browser: a frame loop of its own, so the menus answer the pad before the game data has loaded.
export function installPadMenus({ screen = () => 'title', isRunning = () => false } = {}) {
  if (typeof window === 'undefined' || typeof requestAnimationFrame !== 'function') return null;
  const send = (type, code) => { try { const e = new KeyboardEvent(type, { code, key: KEY_NAMES[code] || code, bubbles: true, cancelable: true }); e.ssxPadMenu = true; window.dispatchEvent(e); } catch {} };   // ssxPadMenu: a menu key made from the pad (main.js: never the keyboard's pause)
  const menus = createPadMenus({ menu: () => screen() !== 'game', running: isRunning, send, startAccepts: () => pv('startRules') && startMenu(screen()) });
  const loop = (t) => { try { menus.step(pollPads(), t); } catch (e) { console.warn('pad menus', e); } requestAnimationFrame(loop); };
  requestAnimationFrame(loop);
  addEventListener('blur', () => menus.releaseAll());
  return menus;
}
