// Control hints for the input the player is using (docs/input-glyphs.md). The last input used decides: a real key
// press = keyboard, a gamepad button / stick = gamepad, a touch = touch (the deck shows PS2 buttons). Keyboard hints
// are key caps in the style of the load screen's keyboard panel (web/loading-screen.js), drawn in place of the PS2
// button icon of the FE atlas (FE_1-14 / OV_1-2: the same controller page); gamepad and touch keep the PS2 icons.
// Presentation only: nothing here reaches the simulation.
import { KEYBOARD_BUTTONS } from './pad-input.js';
import { pollPads, onPads, connectedPads } from './gamepad.js';

export const INPUT_DEVICES = Object.freeze(['keyboard', 'gamepad', 'touch']);

// ---- active input device ------------------------------------------------------------------------------------------
const state = { last: null, forced: null, touchShown: false, padsConnected: false, padActive: new Map(), polledAt: -1, listeners: new Set(), current: null };
try { const q = new URL(globalThis.location?.href).searchParams.get('glyphs'); if (INPUT_DEVICES.includes(q)) state.forced = q; } catch {}   // QA: ?glyphs=keyboard|gamepad|touch

function changed() {
  const now = currentDevice();
  if (now === state.current) return;
  state.current = now;
  for (const fn of state.listeners) { try { fn(now); } catch {} }
}
function currentDevice() { return state.forced || state.last || (state.touchShown ? 'touch' : state.padsConnected ? 'gamepad' : 'keyboard'); }
// The device the hints show. Polls the gamepads (menus do not run main.js inputs()), at most once per millisecond.
export function inputDevice() {
  const now = typeof performance !== 'undefined' ? Math.floor(performance.now()) : 0;
  if (now !== state.polledAt) { state.polledAt = now; pollGamepads(); }
  return currentDevice();
}
export function noteInput(device) { if (!INPUT_DEVICES.includes(device)) return; state.last = device; changed(); }
// The touch deck is up (it shows PS2 buttons): the default until another device is used.
export function noteTouchShown(on) { state.touchShown = !!on; if (on) state.last = 'touch'; else if (state.last === 'touch') state.last = null; changed(); }
export function onInputDevice(fn) { state.listeners.add(fn); return () => state.listeners.delete(fn); }
// QA / tests: force a device (null = follow the input again).
export function forceInputDevice(device) { state.forced = INPUT_DEVICES.includes(device) ? device : null; changed(); }
export function resetInputDevice() { state.last = null; state.forced = null; state.touchShown = false; state.padsConnected = false; state.padActive.clear(); state.polledAt = -1; changed(); }

// A gamepad counts when a button goes down or a stick crosses half travel (edge-triggered, so a pad resting with an
// axis off centre does not take the hints back from the keyboard every frame).
// Without an argument the shared pad reader (web/gamepad.js: every slot, not just slot 0) is polled; its 'use' events
// (the same edges, on the raw controls) mark the gamepad as the last device. With a list (tests), the pads given.
export function pollGamepads(pads) {
  if (pads === undefined) { pollPads(); return; }
  const list = [...(pads || [])].filter(Boolean);
  state.padsConnected = list.length > 0;
  let used = false;
  for (const p of list) {
    const active = new Set();
    (p.buttons || []).forEach((b, i) => { if (b && (b.pressed || b.value > 0.5)) active.add('b' + i); });
    (p.axes || []).forEach((a, i) => { if (Math.abs(a) > 0.5) active.add('a' + i + (a > 0 ? '+' : '-')); });
    const before = state.padActive.get(p.index ?? 0);
    for (const k of active) if (before ? !before.has(k) : k[0] === 'b') { used = true; break; }   // first sight: a held button counts, a resting axis does not
    state.padActive.set(p.index ?? 0, active);
  }
  if (used) state.last = 'gamepad';
  changed();
}

onPads((type) => {
  if (type === 'use') state.last = 'gamepad';
  state.padsConnected = connectedPads().length > 0;
  changed();
});

if (typeof addEventListener === 'function') {
  // Real key presses only: the touch deck drives the menus with synthesized (untrusted) key events.
  addEventListener('keydown', (e) => { if (e.isTrusted) noteInput('keyboard'); }, true);
  addEventListener('pointerdown', (e) => { if (e.pointerType === 'touch' || e.pointerType === 'pen') noteInput('touch'); }, { capture: true, passive: true });
}

// ---- PS2 button -> keyboard key ----------------------------------------------------------------------------------
// Menus (ui.js, fe-screens.js, career-ui.js, ... keydown handlers; web/touch-controls.js MENU_KEYS is the same table):
// Cross = Space (Enter also chooses), Triangle = Escape, Square = Shift, Circle = Backspace, Start = Enter,
// L1/R1/L2/R2 = Q/E/Z/X, D-pad / stick = arrows.
export const MENU_BUTTON_KEYS = Object.freeze({
  cross: 'Space', triangle: 'Escape', square: 'ShiftLeft', circle: 'Backspace', start: 'Enter', select: 'Backspace',
  l1: 'KeyQ', r1: 'KeyE', l2: 'KeyZ', r2: 'KeyX',
  up: 'ArrowUp', down: 'ArrowDown', left: 'ArrowLeft', right: 'ArrowRight',
});
const PAD_NAMES = { cross: 'Cross', triangle: 'Triangle', square: 'Square', circle: 'Circle', start: 'Start', select: 'Select',
  l1: 'L1', r1: 'R1', l2: 'L2', r2: 'R2', l3: 'L3', r3: 'R3', up: 'DPadUp', down: 'DPadDown', left: 'DPadLeft', right: 'DPadRight' };
// In a race (pad-input.js KEYBOARD_BUTTONS for the current Simple/Classic layout). Where two keys do the same thing the
// key the load screen's keyboard panel shows wins (keyboardRows: Esc = Pause, W/A/S/D before the arrows).
const RACE_EXTRA = { start: 'Escape' };                       // main.js: Escape (or Enter) pauses / resumes
const SIMPLE_DPAD = { up: 'KeyW', left: 'KeyA', down: 'KeyS', right: 'KeyD' };   // Simple: WASD are the D-pad in the air
const STICKS = { lstick: 'KeyW', rstick: 'KeyT' };            // left stick W/A/S/D (+ arrows), right stick T/F/G/H

export function buttonName(button) {
  const b = String(button || '').toLowerCase().replace(/^dpad/, '');
  return b === 'x' ? 'cross' : b;
}
// Key code for a PS2 button: context 'menu' (front-end screens, pause / results menus, cards) or 'race' (read by the pad
// while riding: cutscene skip, in-race hints); mode = the keyboard layout ('Classic' default, 'Simple').
export function keyCodeFor(button, { context = 'menu', mode = 'Classic' } = {}) {
  const b = buttonName(button);
  if (context === 'menu') return MENU_BUTTON_KEYS[b] || null;
  if (STICKS[b]) return STICKS[b];
  if (mode === 'Simple' && SIMPLE_DPAD[b]) return SIMPLE_DPAD[b];
  if (RACE_EXTRA[b]) return RACE_EXTRA[b];
  return KEYBOARD_BUTTONS[PAD_NAMES[b]]?.[0] || null;
}
// Key cap label for a key code (the load screen panel's names).
export function keyLabel(code) {
  if (!code) return null;
  const named = { Space: 'Space', ShiftLeft: 'Shift', ShiftRight: 'Shift', Escape: 'Esc', Enter: 'Enter', Backspace: 'Backspace', CapsLock: 'Caps Lock', Tab: 'Tab',
    ArrowUp: '↑', ArrowDown: '↓', ArrowLeft: '←', ArrowRight: '→' }[code];
  if (named) return named;
  const m = /^(?:Key|Digit)(.)$/.exec(code);
  return m ? m[1] : code;
}
export function keyFor(button, opts) { return keyLabel(keyCodeFor(button, opts)); }

// ---- PS2 icon sprites in the FE atlas ----------------------------------------------------------------------------
// FE_1-14 and OV_1-2 are the same controller page: face icons 20x20 at x = 12 + 22k (Triangle, Square, Cross, Circle),
// y = 123; shoulder badges 28x15 at x 121 (R) / 151 (L), y 157 (1) / 174 (2). Source rects vary per call site
// (LUI 56.5,123.5,20,19; ui.sprite 55,122,24,24), so the button is told by the rect's centre.
export const GLYPH_PAGES = Object.freeze(['FE_1-14', 'OV_1-2']);
const FACE = ['triangle', 'square', 'cross', 'circle'];
export function glyphButton(page, sx, sy, sw, sh) {
  if (!GLYPH_PAGES.includes(page) || !(sw > 0) || !(sh > 0)) return null;
  const cx = sx + sw / 2, cy = sy + sh / 2;
  if (sw <= 26 && sh <= 26 && cy >= 118 && cy <= 148 && cx >= 12 && cx < 100) return { button: FACE[Math.floor((cx - 11) / 22)], box: [12 + 22 * Math.floor((cx - 11) / 22), 123, 20, 20] };
  if (sw <= 32 && sh <= 20 && cy >= 150 && cy <= 190 && cx >= 118 && cx < 182) {
    const left = cx >= 150, two = cy >= 170;
    return { button: (left ? 'l' : 'r') + (two ? '2' : '1'), box: [left ? 151 : 121, two ? 174 : 157, 28, 15] };
  }
  return null;
}
// Where the icon itself lands for a drawImage(page, sx, sy, sw, sh, x, y, w, h): [x, y, w, h] of its visible box.
export function glyphRect(g, sx, sy, sw, sh, x, y, w, h) {
  const kx = w / sw, ky = h / sh, [bx, by, bw, bh] = g.box;
  const x0 = Math.max(bx, sx), y0 = Math.max(by, sy), x1 = Math.min(bx + bw, sx + sw), y1 = Math.min(by + bh, sy + sh);
  return [x + (x0 - sx) * kx, y + (y0 - sy) * ky, (x1 - x0) * kx, (y1 - y0) * ky];
}

// ---- key caps (web/loading-screen.js keyboard panel style) -------------------------------------------------------
// The panel's caps are 16 high in its 640x480 LUI frame: rounded 3, a dark vertical gradient, a light 1.2 outline and
// FEFONT at 36% (0.79 * 0.36 * 22 PS2 px, widened to PS2 pixels), 3 below the top edge; single characters are 17 wide,
// longer labels the text width + 10. Everything scales with the cap height h. stretch = the horizontal FEFONT scale of
// the frame: PS2_X (640 / 512) on a 640x448 canvas, PS2_X / PS2_Y on a 640x480 LUI frame drawn at 448 / 480.
export const PS2_X = 640 / 512, PS2_Y = 480 / 448;
export const LUI_STRETCH = PS2_X / PS2_Y;
export const CAP_HEIGHT = 0.85, MIN_CAP = 14;
const TEXT_K = 22 * 0.79 * 0.36 * PS2_Y / 16;               // FEFONT size per unit of cap height (6.70 at h = 16)
export function keyCapWidth(glyphs, label, h, stretch = PS2_X) {
  if ([...label].length === 1) return 17 * h / 16;
  const advance = [...label].reduce((w, ch) => w + (glyphs?.[ch]?.advance || 10), 0);
  return advance * TEXT_K * h / 22 * stretch + 10 * h / 16;
}
// ui: anything with .text(ctx, value, x, y, size, colour, font, align) and .fonts.FEFONT (web/ui.js OriginalUI, the
// phone prompts' drawer, the trick HUD's adapter). Draws with the context's current alpha.
export function drawKeyCap(c, ui, label, x, y, w, h, stretch = PS2_X) {
  const k = h / 16;
  c.save();
  const g = c.createLinearGradient(0, y, 0, y + h); g.addColorStop(0, '#4a525a'); g.addColorStop(1, '#101316');
  c.fillStyle = g; c.strokeStyle = '#eef3f6'; c.lineWidth = 1.2 * k;
  c.beginPath(); c.roundRect(x, y, w, h, 3 * k); c.fill(); c.stroke();
  const arrow = { '←': 180, '↑': 270, '↓': 90, '→': 0 }[label];
  if (arrow !== undefined) {                                  // FEFONT has no arrow glyphs: a small white triangle
    c.translate(x + w / 2, y + h / 2); c.rotate(arrow * Math.PI / 180); c.scale(k, k); c.fillStyle = '#f0f5f8';
    c.beginPath(); c.moveTo(4, 0); c.lineTo(-3, -4); c.lineTo(-3, 4); c.closePath(); c.fill(); c.restore(); return;
  }
  c.translate(x + w / 2, y + 3 * k); c.scale(stretch, 1);
  ui?.text?.(c, label, 0, 0, TEXT_K * h, '#f0f5f8', 'FEFONT', 'center');
  c.restore();
}

// A PS2 icon draw (page, source rect, destination rect) as a key cap when the keyboard is the device: the cap sits on
// the icon's centre line at the height of its disc (CAP_HEIGHT of the 20x20 icon box: 16 in an 18-high FE legend
// slot, the load screen panel's cap height, so stacked legend rows do not overlap), at least MIN_CAP lines of the
// 448-line frame so the label stays readable beside the small career legend icons, and keeps the icon's right edge
// (hint text follows the icon on the right); long labels grow to the left. opts.keys: {button: key code} overrides for
// a screen whose keys differ (the name-entry keyboard). Returns true when it drew the cap (the caller skips the sprite).
export function drawGlyphAsKey(c, ui, page, sx, sy, sw, sh, x, y, w, h, opts = {}) {
  const cap = glyphKeyRect(ui, page, sx, sy, sw, sh, x, y, w, h, opts); if (!cap) return false;
  drawKeyCap(c, ui, cap.label, cap.x, cap.y, cap.w, cap.h, opts.stretch ?? PS2_X);
  return true;
}
// The cap drawGlyphAsKey would draw ({label, x, y, w, h}), or null (not the keyboard, not a button icon): lets a
// hand-laid legend make room for a wide cap (career-messages.js).
export function glyphKeyRect(ui, page, sx, sy, sw, sh, x, y, w, h, { stretch = PS2_X, context = 'menu', mode, keys = null, device = inputDevice() } = {}) {
  if (device !== 'keyboard') return null;
  const g = glyphButton(page, sx, sy, sw, sh); if (!g) return null;
  const label = keyLabel(keys?.[g.button] || keyCodeFor(g.button, { context, mode: mode ?? keyboardMode(ui) }));
  if (!label) return null;
  const [gx, gy, gw, gh] = glyphRect(g, sx, sy, sw, sh, x, y, w, h);
  const ch = Math.max(gh * CAP_HEIGHT, MIN_CAP * (stretch === LUI_STRETCH ? PS2_Y : 1)), cw = keyCapWidth(ui?.fonts?.FEFONT, label, ch, stretch);
  return { label, x: gx + gw - cw, y: gy + (gh - ch) / 2, w: cw, h: ch };
}
function keyboardMode(ui) { return ui?.keyboardMode || ui?.ui?.keyboardMode || 'Classic'; }
