// Touch controls for phones and tablets (docs/mobile.md): a DualShock 2 drawn around the PS2's 4:3 frame.
//
// Nothing here has a gameplay meaning. Every control is a PS2 pad control, and it reaches the core exactly the way
// the keyboard or a gamepad does: buttons hold the keyboard codes web/pad-input.js maps to that PS2 button, the stick
// is the left stick of a (virtual) standard gamepad, Start is gamepad button 9. So buildPad() produces the same 24
// channels for "touch ✕" as for "Space", and the original input path (pad_tick, INPUT.MAP) does the rest.
//
// Layouts (portrait is the main one; landscape keeps the frame centred with the controls at its sides):
//   split: L1 L2 · R2 R1 in a strip above the frame, d-pad / stick / Select Start / face buttons below it;
//   below: everything below the frame (the shoulder row first).
// Look: web/mobile.css (console case around the frame, DualShock-style deck), all drawn in CSS/SVG.
// Menus: while the front end, pause or results screen is up, the same controls also send the keys the menus read
// (d-pad/stick = arrows, ✕ = Space, △ = Escape (back), □ = Shift, ○ = Backspace, L1/R1 = Q/E, Start = Enter).
import { createPhonePrompts, promptMessage } from './phone-prompts.js';
import { noteTouchShown } from './input-glyphs.js';

// PS2 control -> the keyboard code pad-input.js maps to it (KEYBOARD_BUTTONS; IJKL = the D-pad in both keyboard modes).
export const PAD_KEYS = Object.freeze({
  cross: 'Space', square: 'ShiftLeft', circle: 'KeyC', triangle: 'KeyY',
  l1: 'KeyQ', l2: 'KeyZ', r1: 'KeyE', r2: 'KeyX',
  up: 'KeyI', down: 'KeyK', left: 'KeyJ', right: 'KeyL', select: 'Backspace',
});
// PS2 control -> the key the front-end screens read for it (ui.js, fe-screens.js, character-select.js, audio-menu.js).
export const MENU_KEYS = Object.freeze({
  cross: 'Space', triangle: 'Escape', square: 'ShiftLeft', circle: 'Backspace',
  l1: 'KeyQ', r1: 'KeyE', l2: 'KeyZ', r2: 'KeyX',
  up: 'ArrowUp', down: 'ArrowDown', left: 'ArrowLeft', right: 'ArrowRight', start: 'Enter',
});
const KEY_NAMES = { Space: ' ', Escape: 'Escape', ShiftLeft: 'Shift', Backspace: 'Backspace', KeyQ: 'q', KeyE: 'e', KeyZ: 'z', KeyX: 'x', ArrowUp: 'ArrowUp', ArrowDown: 'ArrowDown', ArrowLeft: 'ArrowLeft', ArrowRight: 'ArrowRight', Enter: 'Enter' };
const START_BUTTON = 9; // W3C standard gamepad Start (pad-input.js GAMEPAD_BUTTONS[1])
const DIRECTIONS = ['up', 'down', 'left', 'right'];

// Stick response: a small dead zone around the thumb's landing point, then the rest of the travel spread over the
// part of the PS2 range the game responds to (the original input stage ignores bytes 79..176, |value| < ~0.384;
// 0x327210). A real DualShock deflected to the returned value gives the same pad channels.
export const STICK_DEAD = 0.12, PS2_STICK_EDGE = 0.39;
export function stickResponse(v, dead = STICK_DEAD) {
  const a = Math.abs(v);
  if (!(a > dead)) return 0;
  return Math.sign(v) * Math.min(1, PS2_STICK_EDGE + (1 - PS2_STICK_EDGE) * (Math.min(a, 1) - dead) / (1 - dead));
}

// The pad state the touch controls hold (pure: tested in node by web/test-touch-controls.mjs).
export class TouchPad {
  constructor() {
    this.count = new Map();     // control -> number of fingers holding it
    this.codes = new Set();     // keyboard codes held (PAD_KEYS of the held controls)
    this.stick = { x: 0, y: 0, active: false };
    this.rstick = { x: 0, y: 0, active: false }; // right stick: BoardPress (up/down) / BoardPivot (left/right)
    this._pad = { id: 'touch', mapping: 'standard', connected: true, axes: [0, 0, 0, 0], buttons: Array.from({ length: 17 }, () => ({ pressed: false, value: 0 })) };
  }
  held(code) { return this.codes.has(code); }
  isHeld(control) { return (this.count.get(control) || 0) > 0; }
  // returns true when this is the first finger on the control
  press(control) {
    const n = (this.count.get(control) || 0) + 1; this.count.set(control, n);
    if (n === 1 && PAD_KEYS[control]) this.codes.add(PAD_KEYS[control]);
    return n === 1;
  }
  // returns true when the last finger left the control
  release(control) {
    const n = (this.count.get(control) || 0) - 1;
    if (n > 0) { this.count.set(control, n); return false; }
    if (!this.count.has(control)) return false;
    this.count.delete(control); if (PAD_KEYS[control]) this.codes.delete(PAD_KEYS[control]);
    return true;
  }
  // x, y: thumb offset / stick radius, +x right, +y down (Gamepad API = PS2 byte order)
  setStick(x, y) {
    const m = Math.hypot(x, y); if (m > 1) { x /= m; y /= m; }
    this.stick.x = stickResponse(x); this.stick.y = stickResponse(y); this.stick.active = true;
  }
  releaseStick() { this.stick.x = 0; this.stick.y = 0; this.stick.active = false; }
  setRStick(x, y) {
    const m = Math.hypot(x, y); if (m > 1) { x /= m; y /= m; }
    this.rstick.x = stickResponse(x); this.rstick.y = stickResponse(y); this.rstick.active = true;
  }
  releaseRStick() { this.rstick.x = 0; this.rstick.y = 0; this.rstick.active = false; }
  clear() { this.count.clear(); this.codes.clear(); this.releaseStick(); this.releaseRStick(); }
  get active() { return this.stick.active || this.rstick.active || this.count.size > 0; }
  // The gamepad buildPad() reads: the real pad with the touch sticks on its sticks and touch Start on button 9.
  // Returns the real pad untouched while no touch stick/Start is held. No allocation per call.
  pad(real = null) {
    const start = this.isHeld('start');
    if (!this.stick.active && !this.rstick.active && !start) return real;
    const p = this._pad, ra = real?.axes;
    p.axes[0] = this.stick.active ? this.stick.x : (Number.isFinite(ra?.[0]) ? ra[0] : 0);
    p.axes[1] = this.stick.active ? this.stick.y : (Number.isFinite(ra?.[1]) ? ra[1] : 0);
    p.axes[2] = this.rstick.active ? this.rstick.x : (Number.isFinite(ra?.[2]) ? ra[2] : 0);
    p.axes[3] = this.rstick.active ? this.rstick.y : (Number.isFinite(ra?.[3]) ? ra[3] : 0);
    for (let i = 0; i < p.buttons.length; i++) { const b = real?.buttons?.[i]; p.buttons[i].pressed = !!b?.pressed; p.buttons[i].value = b ? +b.value || 0 : 0; }
    if (start) { p.buttons[START_BUTTON].pressed = true; p.buttons[START_BUTTON].value = 1; }
    return p;
  }
}

// ---- layout (pure: rectangles in CSS px) ------------------------------------------------------------------------------
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
// Too small to play (CSS px): a frame shorter than frameH, a portrait deck shorter than deckH, a landscape view shorter
// than landscapeH (the side controls overlap). The turn-your-device prompt then offers the other orientation when that
// one fits (relayout -> checkRotate, web/phone-prompts.js). Phones in either orientation are well above these.
export const CRAMPED = Object.freeze({ frameH: 150, deckH: 200, landscapeH: 260 });
// W, H: viewport; ins: safe-area insets {t,r,b,l}; aspect: frame height / width (3/4, or 9/16 for Anamorphic).
export function computeLayout({ W, H, ins = { t: 0, r: 0, b: 0, l: 0 }, aspect = 0.75, layout = 'split' }) {
  const landscape = W > H * 1.05, L = {};
  const bezel = clamp(Math.round(Math.min(W, H) * 0.014), 4, 8);
  const pad = clamp(Math.round(Math.min(W, H) * 0.03), 8, 18);
  L.landscape = landscape; L.bezel = bezel;
  if (!landscape) {
    const innerW = W - ins.l - ins.r;
    const stripH = Math.round(clamp(H * 0.075, 54, 76));
    const minDeck = 250;
    let fw = innerW - 2 * bezel, fh = fw * aspect;
    const avail = H - ins.t - ins.b - stripH - 2 * bezel - fh;
    if (avail < minDeck) { fh = Math.max(120, H - ins.t - ins.b - stripH - 2 * bezel - minDeck); fw = fh / aspect; }
    const fx = ins.l + (innerW - fw) / 2;
    let fy, stripY, deckY;
    if (layout === 'below') { fy = ins.t + bezel; stripY = fy + fh + bezel; deckY = stripY + stripH; }
    else { stripY = ins.t; fy = ins.t + stripH + bezel; deckY = fy + fh + bezel; }
    L.frame = { x: fx, y: fy, w: fw, h: fh };
    L.strip = { x: 0, y: layout === 'below' ? stripY : 0, w: W, h: layout === 'below' ? stripH : stripY + stripH };
    L.deck = { x: 0, y: deckY, w: W, h: H - deckY };
    const dx = ins.l, dw = innerW, dh = H - ins.b - deckY;
    // shoulder strip: L1 L2 [≡ Options] [fullscreen] R2 R1
    const sh = stripH - 2 * Math.round(pad * 0.6), sy = stripY + (stripH - sh) / 2, sg = clamp(dw * 0.02, 6, 12);
    const ib = clamp(sh * 0.8, 30, 40), mid = dx + dw / 2;
    const sw = Math.min(clamp(dw * 0.2, 64, 120), (dw / 2 - ib - 10 - pad * 0.6 - sg) / 2);
    L.l1 = { x: dx + pad * 0.6, y: sy, w: sw, h: sh }; L.l2 = { x: L.l1.x + sw + sg, y: sy, w: sw, h: sh };
    L.r1 = { x: dx + dw - pad * 0.6 - sw, y: sy, w: sw, h: sh }; L.r2 = { x: L.r1.x - sg - sw, y: sy, w: sw, h: sh };
    L.menu = { x: mid - ib - 4, y: sy + (sh - ib) / 2, w: ib, h: ib }; L.full = { x: mid + 4, y: sy + (sh - ib) / 2, w: ib, h: ib };
    // lower deck, anchored to the bottom where the thumbs are: left stick bottom-left with the d-pad above it,
    // face diamond bottom-right with the right stick above it, Select / Start between the d-pad and right stick.
    const bottom = deckY + dh - pad, gap = clamp(dh * 0.03, 6, 16);
    let D = clamp(Math.min(dw * 0.3, dh * 0.3), 88, 136), S = clamp(Math.min(dw * 0.37, dh * 0.38), 104, 168);
    const B = clamp(Math.min(dw * 0.165, dh * 0.16), 46, 76), spread = B * 1.02, cluster = 2 * spread + B;
    const top = deckY + pad;
    if (bottom - S - gap - D < top) D = Math.max(72, bottom - S - gap - top); // short deck: shrink the d-pad first
    L.stick = { cx: dx + pad + S / 2 + 4, cy: bottom - S / 2, size: S };
    L.dpad = { cx: dx + pad + Math.max(D, S) / 2 + 4, cy: Math.max(top + D / 2, bottom - S - gap - D / 2), size: D };
    const zoneTop = L.dpad.cy + D / 2 + 2;
    L.stickZone = { x: dx, y: zoneTop, w: dw * 0.52, h: deckY + dh - zoneTop };
    L.face = { cx: dx + dw - pad - cluster / 2 - 2, cy: bottom - cluster / 2 - clamp(dh * 0.035, 8, 16), B, spread }; // room for the ✕ caption
    const faceTop = L.face.cy - cluster / 2;
    const R = Math.min(D, faceTop - gap - top);
    L.rstick = R >= 72 ? { cx: L.face.cx, cy: faceTop - gap - R / 2, size: R } : null;
    const pw = clamp(dw * 0.15, 48, 66), ph = clamp(dh * 0.065, 22, 28);
    const blockTop = Math.min(L.dpad.cy - D / 2, L.rstick ? L.rstick.cy - R / 2 : faceTop);
    if (blockTop - gap - ph >= top) { // a row above the controls: SELECT  (o) ANALOG  START
      const y = blockTop - gap - ph;
      L.select = { x: mid - 44 - pw, y, w: pw, h: ph }; L.start = { x: mid + 44, y, w: pw, h: ph };
      L.analog = { x: mid - 42, y: y + (ph - 14) / 2, w: 84, h: 14 };
    } else { // short deck: stacked between the d-pad and the right stick
      const yc = Math.min(L.dpad.cy, L.rstick ? L.rstick.cy : L.dpad.cy), xc = (L.dpad.cx + D / 2 + (L.rstick ? L.rstick.cx - R / 2 : L.face.cx - cluster / 2)) / 2;
      L.select = { x: xc - pw / 2, y: yc - ph - 3, w: pw, h: ph }; L.start = { x: xc - pw / 2, y: yc + 3, w: pw, h: ph };
      L.analog = null;
    }
    L.cramped = L.frame.h < CRAMPED.frameH || dh < CRAMPED.deckH;
    return L;
  }
  // landscape: frame centred, controls at its sides
  const innerH = H - ins.t - ins.b;
  let fh = innerH - 2 * bezel, fw = fh / aspect;
  const minSide = 150;
  if ((W - ins.l - ins.r - fw - 2 * bezel) / 2 < minSide) { fw = W - ins.l - ins.r - 2 * bezel - 2 * minSide; fh = fw * aspect; }
  const side = (W - ins.l - ins.r - fw - 2 * bezel) / 2;
  const fx = ins.l + side + bezel, fy = ins.t + (innerH - fh) / 2;
  L.frame = { x: fx, y: fy, w: fw, h: fh };
  L.strip = { x: 0, y: 0, w: 0, h: 0 };
  L.deck = { x: 0, y: 0, w: W, h: H };
  const lx = ins.l, rx = fx + fw + bezel, top = ins.t + pad * 0.6, bottom = H - ins.b - pad * 0.6;
  const sh = clamp(innerH * 0.12, 36, 52), sw = (side - pad * 1.5) / 2;
  L.l1 = { x: lx + pad * 0.5, y: top, w: sw, h: sh }; L.l2 = { x: L.l1.x + sw + pad * 0.5, y: top, w: sw, h: sh };
  L.r1 = { x: rx + side - pad * 0.5 - sw, y: top, w: sw, h: sh }; L.r2 = { x: L.r1.x - pad * 0.5 - sw, y: top, w: sw, h: sh };
  const rowY = top + sh + pad * 0.6, ib = clamp(sh * 0.75, 28, 38);
  const pw = clamp(side * 0.36, 44, 64), ph = clamp(sh * 0.6, 22, 30);
  L.select = { x: rx + side / 2 - pw - 4, y: rowY, w: pw, h: ph }; L.start = { x: rx + side / 2 + 4, y: rowY, w: pw, h: ph };
  L.menu = { x: lx + side / 2 - ib - 4, y: rowY - (ib - ph) / 2, w: ib, h: ib }; L.full = { x: lx + side / 2 + 4, y: rowY - (ib - ph) / 2, w: ib, h: ib };
  const below = rowY + Math.max(ib, ph) + pad * 0.4, room = bottom - below;
  const S = clamp(Math.min(side - pad, room * 0.56), 90, 150), D = clamp(Math.min(side - pad * 1.5, room - S - pad * 0.5), 76, 120);
  L.dpad = { cx: lx + side / 2, cy: below + D / 2, size: D };
  L.stick = { cx: lx + side / 2, cy: bottom - S / 2, size: S };
  L.stickZone = { x: 0, y: L.dpad.cy + D / 2 + 2, w: fx - bezel, h: H - (L.dpad.cy + D / 2 + 2) };
  const B = clamp(Math.min((side - pad) / 2.95, room / 3.2), 40, 64), spread = B * 1.0;
  L.face = { cx: rx + side / 2, cy: bottom - spread - B / 2, B, spread };
  const R = Math.min(side - pad * 1.5, L.face.cy - spread - B / 2 - 6 - below);
  L.rstick = R >= 64 ? { cx: rx + side / 2, cy: below + R / 2, size: R } : null;
  L.analog = null;
  L.cramped = L.frame.h < CRAMPED.frameH || innerH < CRAMPED.landscapeH;
  return L;
}

// Face buttons around the cluster centre; d-pad arms.
const FACE = [['triangle', 0, -1], ['circle', 1, 0], ['cross', 0, 1], ['square', -1, 0]];
// Which face buttons a finger at (x, y) presses. On a DualShock one thumb presses two neighbours by landing between
// them (✕+□ = jump + boost is the common one), so between any two neighbouring buttons there is a chord zone: where the
// finger is about as close to both (their distances differ by less than CHORD_BAND of a button) and near both, both
// fire. Sliding from ✕ toward □ adds □ without letting go of ✕. Opposite buttons (△/✕, □/○) never chord.
// Otherwise: the button under the finger, or the nearest one inside the cluster.
export const CHORD_BAND = 0.5, CHORD_REACH = 1.2;
const OPPOSITE = { triangle: 'cross', cross: 'triangle', square: 'circle', circle: 'square' };
export function faceHits(face, x, y) {
  const d = FACE.map(([name, fx, fy]) => [name, Math.hypot(x - face.cx - fx * face.spread, y - face.cy - fy * face.spread)]).sort((p, q) => p[1] - q[1]);
  const [a, b, c] = d, B = face.B;
  // (the third button must be clearly farther: the middle of the diamond is no chord, just the nearest button)
  if (OPPOSITE[a[0]] !== b[0] && a[1] <= B * CHORD_REACH && b[1] <= B * CHORD_REACH && b[1] - a[1] < B * CHORD_BAND && c[1] - b[1] > B * 0.3) return [a[0], b[0]];
  return a[1] <= B * 1.05 ? [a[0]] : [];
}
// Centres of the chord zones (the midpoints of neighbouring buttons), for the small markers drawn in the gaps.
export function faceChords(face) {
  const out = [];
  for (let i = 0; i < FACE.length; i++) { const [n1, x1, y1] = FACE[i], [n2, x2, y2] = FACE[(i + 1) % FACE.length]; out.push({ pair: [n1, n2], x: face.cx + (x1 + x2) / 2 * face.spread, y: face.cy + (y1 + y2) / 2 * face.spread }); }
  return out;
}
// D-pad: 8-way from the centre (diagonals press two arms), a small dead centre.
export function dpadHits(dpad, x, y) {
  const dx = x - dpad.cx, dy = y - dpad.cy, r = Math.hypot(dx, dy);
  if (r < dpad.size * 0.12) return [];
  const a = Math.atan2(dy, dx) * 180 / Math.PI; // 0 = right, 90 = down
  const out = [];
  if (a > -67.5 && a < 67.5) out.push('right');
  if (a > 22.5 && a < 157.5) out.push('down');
  if (a > 112.5 || a < -112.5) out.push('left');
  if (a > -157.5 && a < -22.5) out.push('up');
  return out;
}

// ---- settings -------------------------------------------------------------------------------------------------------
// Shown and changed in the game's own Options (Options > Display & Touch, web/fe-options.js DISPLAY_ROWS); the ≡ button
// on the deck opens that screen (a race pauses first).
const SETTINGS_KEY = 'ssx3.touch';
export const TOUCH_DEFAULTS = Object.freeze({ layout: 'split', stick: 'floating', haptics: true, show: 'auto' });
export function loadTouchSettings() { try { return { ...TOUCH_DEFAULTS, ...(JSON.parse(globalThis.localStorage?.getItem(SETTINGS_KEY) || '{}') || {}) }; } catch { return { ...TOUCH_DEFAULTS }; } }
export function saveTouchSettings(s) { try { globalThis.localStorage?.setItem(SETTINGS_KEY, JSON.stringify(s)); } catch {} }
const loadSettings = loadTouchSettings, saveSettings = saveTouchSettings;

// ---- DOM ------------------------------------------------------------------------------------------------------------
const SVG = {
  triangle: '<svg viewBox="0 0 24 24"><path d="M12 4.6 20 18.4H4z" fill="none" stroke="currentColor" stroke-width="2.3" stroke-linejoin="round"/></svg>',
  circle: '<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="7" fill="none" stroke="currentColor" stroke-width="2.3"/></svg>',
  cross: '<svg viewBox="0 0 24 24"><path d="M5.5 5.5 18.5 18.5M18.5 5.5 5.5 18.5" stroke="currentColor" stroke-width="2.3" stroke-linecap="round"/></svg>',
  square: '<svg viewBox="0 0 24 24"><rect x="5.2" y="5.2" width="13.6" height="13.6" fill="none" stroke="currentColor" stroke-width="2.3"/></svg>',
  menu: '<svg viewBox="0 0 24 24"><path d="M4 7h16M4 12h16M4 17h16" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/></svg>',
  full: '<svg viewBox="0 0 24 24"><path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  arrow: '<svg viewBox="0 0 24 24"><path d="M12 6 18 15H6z" fill="currentColor"/></svg>',
};
const FACE_HINT = { cross: 'JUMP', square: 'BOOST', circle: 'PLANT', triangle: '' };

export function createTouchControls({ ui, isRunning = () => false, pause = () => {} } = {}) {
  const doc = document, html = doc.documentElement, touch = new TouchPad(), settings = loadSettings();
  const root = doc.createElement('div'); root.id = 'touch-ui'; root.setAttribute('aria-hidden', 'true');
  const el = (cls, parent = root, tag = 'div') => { const e = doc.createElement(tag); e.className = cls; parent.appendChild(e); return e; };
  const stripBg = el('tc-strip'), deckBg = el('tc-deck'), bezel = el('tc-bezel');
  const buttons = {};
  const button = (name, label, cls = '') => { const b = el('tc-btn ' + cls); b.dataset.c = name; b.innerHTML = label; buttons[name] = b; return b; };
  for (const [n, t] of [['l1', 'L1'], ['l2', 'L2'], ['r1', 'R1'], ['r2', 'R2']]) button(n, `<span>${t}</span>`, 'tc-shoulder');
  button('select', '<span>SELECT</span>', 'tc-pill'); button('start', '<span>START</span>', 'tc-pill');
  const menuBtn = el('tc-icon'); menuBtn.dataset.act = 'menu'; menuBtn.innerHTML = SVG.menu; menuBtn.title = 'Options';
  const fullBtn = el('tc-icon'); fullBtn.dataset.act = 'full'; fullBtn.innerHTML = SVG.full; fullBtn.title = 'Full screen';
  const dpad = el('tc-dpad'); dpad.dataset.zone = 'dpad';
  const arms = {}; for (const d of DIRECTIONS) { const a = el('tc-arm tc-' + d, dpad); a.innerHTML = SVG.arrow; arms[d] = a; }
  el('tc-hub', dpad);
  const stickZone = el('tc-stick-zone'); stickZone.dataset.zone = 'stick';
  const stickBase = el('tc-stick-base', stickZone), nub = el('tc-stick-nub', stickBase);
  const rstick = el('tc-rstick'); rstick.dataset.zone = 'rstick';
  const rbase = el('tc-stick-base', rstick), rnub = el('tc-stick-nub', rbase);
  const analog = el('tc-analog'); analog.innerHTML = '<i></i>ANALOG';
  const face = el('tc-face'); face.dataset.zone = 'face';
  const faceBtns = {};
  for (const [name] of FACE) { const b = el('tc-face-btn tc-' + name, face); b.innerHTML = `<i class="tc-glyph">${SVG[name]}</i>${FACE_HINT[name] ? `<b>${FACE_HINT[name]}</b>` : ''}`; faceBtns[name] = b; }
  const chordDots = [0, 1, 2, 3].map(() => el('tc-chord', face)); // gap markers: press here for both neighbours
  doc.body.appendChild(root);

  // ---- mode ------------------------------------------------------------------------------------------------------
  const coarse = (() => { try { return matchMedia('(pointer:coarse)').matches; } catch { return false; } })();
  let shown = false, layout = null;
  function show(on) {
    on = !!on && settings.show !== 'off';
    if (on === shown) return;
    shown = on; html.dataset.touch = on ? '1' : '';
    if (ui?.loading) ui.loading.forceDevice = on ? 'gamepad' : null; // the "Basic Controls" load screen shows the pad layout
    noteTouchShown(on);                                              // control hints show the PS2 buttons the deck has (web/input-glyphs.js)
    if (!on) releaseAll();
    relayout();
  }
  const menuMode = () => ui?.screen !== 'game';

  // ---- key synthesis for menus -------------------------------------------------------------------------------------
  function sendKey(type, code, repeat = false) {
    if (!code) return;
    const e = new KeyboardEvent(type, { code, key: KEY_NAMES[code] || code, bubbles: true, cancelable: true, repeat });
    window.dispatchEvent(e);
  }
  const menuKeysDown = new Map(); // control -> {code, timer}
  function menuPress(control) {
    if (!menuMode()) return;
    const code = MENU_KEYS[control]; if (!code || menuKeysDown.has(control)) return;
    // Start while a run is going is the pad's Start (frame() pauses/resumes on it), not Enter (which also toggles pause)
    if (control === 'start' && isRunning()) return;
    sendKey('keydown', code);
    const entry = { code, timer: 0 }; menuKeysDown.set(control, entry);
    if (DIRECTIONS.includes(control)) { // held direction repeats as fresh presses (the menus ignore key repeats)
      const again = () => { if (menuKeysDown.get(control) !== entry) return; sendKey('keyup', code); sendKey('keydown', code); entry.timer = setTimeout(again, 110); };
      entry.timer = setTimeout(again, 400);
    }
  }
  function menuRelease(control) {
    const entry = menuKeysDown.get(control); if (!entry) return;
    clearTimeout(entry.timer); menuKeysDown.delete(control); sendKey('keyup', entry.code);
  }

  // ---- haptics -----------------------------------------------------------------------------------------------------
  // navigator.vibrate (Android Chrome). iOS Safari has none; iOS 18 gives a tick when a switch checkbox toggles.
  let hapticSwitch = null;
  function haptic() {
    if (!settings.haptics) return;
    if (typeof navigator.vibrate === 'function') { try { navigator.vibrate(8); } catch {} return; }
    if (!hapticSwitch) {
      const label = doc.createElement('label'), input = doc.createElement('input');
      input.type = 'checkbox'; input.setAttribute('switch', ''); label.style.cssText = input.style.cssText = 'position:fixed;left:-99px;top:0;width:1px;height:1px;opacity:0;pointer-events:none';
      label.appendChild(input); doc.body.appendChild(label); hapticSwitch = label;
    }
    try { hapticSwitch.click(); } catch {}
  }

  // ---- press / release -----------------------------------------------------------------------------------------------
  // A tap shorter than a frame would press and release between two pad reads (frame() samples the pad once per
  // drawn frame): every press lasts at least MIN_HOLD_MS, so a quick tap on ✕ or Start always reaches the game.
  const syncChordDots = () => { for (const dot of chordDots) { const [p1, p2] = (dot.dataset.pair || '').split('+'); dot.classList.toggle('on', !!(faceBtns[p1]?.classList.contains('on') && faceBtns[p2]?.classList.contains('on'))); } };
  const MIN_HOLD_MS = 70, pressedAt = new Map();
  function pressControl(c) {
    pressedAt.set(c, performance.now());
    if (touch.press(c)) { (buttons[c] || faceBtns[c] || arms[c])?.classList.add('on'); menuPress(c); haptic(); }
    syncChordDots();
  }
  function releaseControl(c) {
    const held = performance.now() - (pressedAt.get(c) ?? -1e9);
    if (held < MIN_HOLD_MS) { setTimeout(() => releaseNow(c), MIN_HOLD_MS - held); return; }
    releaseNow(c);
  }
  function releaseNow(c) {
    if (touch.release(c)) { (buttons[c] || faceBtns[c] || arms[c])?.classList.remove('on'); menuRelease(c); }
    syncChordDots();
  }
  // menus: the stick's direction acts like the d-pad
  const stickDirs = new Set();
  function stickMenu(x, y) {
    const want = new Set();
    if (!menuMode()) { for (const d of stickDirs) { const e = menuKeysDown.get('s' + d); if (e) { clearTimeout(e.timer); menuKeysDown.delete('s' + d); sendKey('keyup', e.code); } } stickDirs.clear(); return; }
    if (y < -0.5) want.add('up'); if (y > 0.5) want.add('down'); if (x < -0.5) want.add('left'); if (x > 0.5) want.add('right');
    for (const d of [...stickDirs]) if (!want.has(d)) { stickDirs.delete(d); const e = menuKeysDown.get('s' + d); if (e) { clearTimeout(e.timer); menuKeysDown.delete('s' + d); sendKey('keyup', e.code); } }
    for (const d of want) if (!stickDirs.has(d)) {
      stickDirs.add(d); const code = MENU_KEYS[d]; sendKey('keydown', code); const entry = { code, timer: 0 }; menuKeysDown.set('s' + d, entry);
      const again = () => { if (menuKeysDown.get('s' + d) !== entry) return; sendKey('keyup', code); sendKey('keydown', code); entry.timer = setTimeout(again, 130); };
      entry.timer = setTimeout(again, 420);
    }
  }
  const pointers = new Map(); // pointerId -> {kind, controls:Set, el, ox, oy}
  function setControls(p, next) {
    for (const c of p.controls) if (!next.includes(c)) { p.controls.delete(c); releaseControl(c); }
    for (const c of next) if (!p.controls.has(c)) { p.controls.add(c); pressControl(c); }
  }
  function stickMove(p, x, y) {
    const R = layout.stick.size * 0.42, dx = x - p.ox, dy = y - p.oy;
    const m = Math.hypot(dx, dy), k = m > R ? R / m : 1;
    nub.style.transform = `translate(${dx * k}px,${dy * k}px)`;
    touch.setStick(dx / R, dy / R);
    stickMenu(touch.stick.x, touch.stick.y);
  }
  function placeStickBase(cx, cy) {
    const S = layout.stick.size, z = layout.stickZone;
    stickBase.style.left = (cx - S / 2 - z.x) + 'px'; stickBase.style.top = (cy - S / 2 - z.y) + 'px';
  }
  function down(e) {
    const t = e.target.closest?.('[data-c],[data-zone],[data-act]'); if (!t || !layout) return;
    e.preventDefault();
    try { t.setPointerCapture(e.pointerId); } catch {}
    const p = { kind: t.dataset.act ? 'act' : t.dataset.zone || 'button', controls: new Set(), el: t, ox: 0, oy: 0, dead: false };
    pointers.set(e.pointerId, p);
    const x = e.clientX, y = e.clientY;
    if (p.kind === 'act') { t.classList.add('on'); haptic(); } // runs on release: fullscreen needs the activation of pointerup/touchend
    else if (p.kind === 'button') setControls(p, [t.dataset.c]);
    else if (p.kind === 'face') setControls(p, faceHits(layout.face, x, y));
    else if (p.kind === 'dpad') setControls(p, dpadHits(layout.dpad, x, y));
    else if (p.kind === 'stick') {
      if ([...pointers.values()].some((q) => q !== p && q.kind === 'stick' && !q.dead)) { p.kind = 'ignored'; return; }
      if (settings.stick === 'floating') { p.ox = x; p.oy = y; placeStickBase(x, y); } else { p.ox = layout.stick.cx; p.oy = layout.stick.cy; }
      stickBase.classList.add('on'); stickMove(p, x, y); haptic();
    } else if (p.kind === 'rstick') {
      if ([...pointers.values()].some((q) => q !== p && q.kind === 'rstick' && !q.dead)) { p.kind = 'ignored'; return; }
      p.ox = layout.rstick.cx; p.oy = layout.rstick.cy; rstick.classList.add('on'); rstickMove(p, x, y); haptic();
    }
  }
  function move(e) {
    const p = pointers.get(e.pointerId); if (!p || p.dead || !layout) return;
    e.preventDefault();
    const x = e.clientX, y = e.clientY;
    if (p.kind === 'face') setControls(p, faceHits(layout.face, x, y));
    else if (p.kind === 'dpad') setControls(p, dpadHits(layout.dpad, x, y));
    else if (p.kind === 'stick') stickMove(p, x, y);
    else if (p.kind === 'rstick') rstickMove(p, x, y);
  }
  function rstickMove(p, x, y) {
    const R = layout.rstick.size * 0.36, dx = x - p.ox, dy = y - p.oy, m = Math.hypot(dx, dy), k = m > R ? R / m : 1;
    rnub.style.transform = `translate(${dx * k}px,${dy * k}px)`;
    touch.setRStick(dx / R, dy / R);
  }
  function up(e) {
    const p = pointers.get(e.pointerId); if (!p) return;
    pointers.delete(e.pointerId);
    if (p.kind === 'act') { p.el.classList.remove('on'); const r = p.el.getBoundingClientRect(); if (e.type === 'pointerup' && e.clientX >= r.left && e.clientX <= r.right && e.clientY >= r.top && e.clientY <= r.bottom) action(p.el.dataset.act); return; }
    if (!p.dead) endPointer(p);
  }
  function endPointer(p) {
    for (const c of p.controls) releaseControl(c); p.controls.clear();
    if (p.kind === 'stick' && layout) {
      touch.releaseStick(); stickMenu(0, 0); stickBase.classList.remove('on'); nub.style.transform = '';
      placeStickBase(layout.stick.cx, layout.stick.cy);
    }
    if (p.kind === 'rstick') { touch.releaseRStick(); rstick.classList.remove('on'); rnub.style.transform = ''; }
  }
  // Logical release of everything (pause, rider switch, deck hidden). Fingers still down stay dead until lifted.
  function releaseAll() {
    pressedAt.clear(); // immediate releases
    for (const p of pointers.values()) { endPointer(p); p.dead = true; }
    for (const [c, entry] of menuKeysDown) { clearTimeout(entry.timer); sendKey('keyup', entry.code); } menuKeysDown.clear(); stickDirs.clear();
    for (const c of [...touch.count.keys()]) { (buttons[c] || faceBtns[c] || arms[c])?.classList.remove('on'); }
    touch.clear();
  }
  root.addEventListener('pointerdown', down, { passive: false });
  root.addEventListener('pointermove', move, { passive: false });
  for (const t of ['pointerup', 'pointercancel', 'lostpointercapture']) root.addEventListener(t, up);
  // no scrolling, pinch/double-tap zoom, long-press callout or selection from the controls
  for (const t of ['touchstart', 'touchmove', 'touchend']) root.addEventListener(t, (e) => { if (e.cancelable) e.preventDefault(); }, { passive: false });
  root.addEventListener('contextmenu', (e) => e.preventDefault());
  doc.addEventListener('gesturestart', (e) => { if (shown) e.preventDefault(); }, { passive: false }); // iOS pinch
  doc.addEventListener('dblclick', (e) => { if (shown) e.preventDefault(); }, { passive: false });
  doc.addEventListener('touchmove', (e) => { if (shown && e.cancelable && e.touches.length > 1) e.preventDefault(); }, { passive: false });

  // ---- show on touch, hide on keyboard / gamepad -------------------------------------------------------------------
  addEventListener('pointerdown', (e) => {
    if (e.pointerType === 'touch' || e.pointerType === 'pen') {
      if (!shown && settings.show !== 'off') show(true);
      // iOS: keep the game audible with the ring/silent switch on (Safari 17+ audio session API)
      try { if (navigator.audioSession && navigator.audioSession.type !== 'playback') navigator.audioSession.type = 'playback'; } catch {}
    }
  }, { capture: true, passive: true });
  addEventListener('keydown', (e) => { if (e.isTrusted && shown && settings.show === 'auto' && !['Shift', 'Control', 'Alt', 'Meta'].includes(e.key)) show(false); }, { capture: true });
  // gamepad activity (polled by main.js each frame through pad())
  function gamepadActive(real) {
    if (!real || !shown || settings.show !== 'auto') return false;
    if (real.buttons?.some((b) => b.pressed)) return true;
    return (real.axes || []).some((a) => Math.abs(a) > 0.6);
  }

  // ---- layout ------------------------------------------------------------------------------------------------------
  const probe = doc.createElement('div');
  probe.style.cssText = 'position:fixed;visibility:hidden;pointer-events:none;padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)';
  doc.body.appendChild(probe);
  const stage = doc.getElementById('stage');
  const place = (node, r) => { node.style.left = r.x + 'px'; node.style.top = r.y + 'px'; node.style.width = r.w + 'px'; node.style.height = r.h + 'px'; };
  function relayout() {
    html.dataset.touchLayout = settings.layout;
    if (!shown) { layout = null; for (const k of ['--fx', '--fy', '--fw', '--fh']) html.style.removeProperty(k); checkRotate(); return; }
    const vv = window.visualViewport, W = Math.round(vv?.width || innerWidth), H = Math.round(vv?.height || innerHeight);
    const cs = getComputedStyle(probe), ins = { t: parseFloat(cs.paddingTop) || 0, r: parseFloat(cs.paddingRight) || 0, b: parseFloat(cs.paddingBottom) || 0, l: parseFloat(cs.paddingLeft) || 0 };
    const aspect = stage?.dataset.widescreen === '2' ? 9 / 16 : 3 / 4;
    layout = computeLayout({ W, H, ins, aspect, layout: settings.layout });
    if (layout.cramped) { const other = computeLayout({ W: H, H: W, ins, aspect, layout: settings.layout }); layout.cramped = !other.cramped; layout.want = other.landscape ? 'landscape' : 'portrait'; }
    html.dataset.touchOrient = layout.landscape ? 'landscape' : 'portrait';
    const f = layout.frame;
    html.style.setProperty('--fx', f.x + 'px'); html.style.setProperty('--fy', f.y + 'px'); html.style.setProperty('--fw', f.w + 'px'); html.style.setProperty('--fh', f.h + 'px');
    place(bezel, { x: f.x - layout.bezel, y: f.y - layout.bezel, w: f.w + 2 * layout.bezel, h: f.h + 2 * layout.bezel });
    place(stripBg, layout.strip); place(deckBg, layout.deck);
    for (const n of ['l1', 'l2', 'r1', 'r2', 'select', 'start']) place(buttons[n], layout[n]);
    place(menuBtn, layout.menu); place(fullBtn, layout.full);
    const d = layout.dpad; place(dpad, { x: d.cx - d.size / 2, y: d.cy - d.size / 2, w: d.size, h: d.size });
    const z = layout.stickZone; place(stickZone, z);
    const S = layout.stick.size; stickBase.style.width = stickBase.style.height = S + 'px'; placeStickBase(layout.stick.cx, layout.stick.cy);
    const fc = layout.face, ext = fc.spread * 2 + fc.B;
    place(face, { x: fc.cx - ext / 2, y: fc.cy - ext / 2, w: ext, h: ext });
    for (const [name, fx, fy] of FACE) place(faceBtns[name], { x: ext / 2 + fx * fc.spread - fc.B / 2, y: ext / 2 + fy * fc.spread - fc.B / 2, w: fc.B, h: fc.B });
    faceChords({ cx: ext / 2, cy: ext / 2, spread: fc.spread }).forEach((c, k) => { const r = fc.B * 0.2; place(chordDots[k], { x: c.x - r, y: c.y - r, w: 2 * r, h: 2 * r }); chordDots[k].dataset.pair = c.pair.join('+'); });
    root.style.setProperty('--B', fc.B + 'px');
    const rs = layout.rstick; rstick.hidden = !rs; if (rs) place(rstick, { x: rs.cx - rs.size / 2, y: rs.cy - rs.size / 2, w: rs.size, h: rs.size });
    analog.hidden = !layout.analog; if (layout.analog) place(analog, layout.analog);
    checkRotate();
  }
  let pending = 0;
  const schedule = () => { if (!pending) pending = requestAnimationFrame(() => { pending = 0; relayout(); }); };
  addEventListener('resize', schedule); window.visualViewport?.addEventListener('resize', schedule);
  addEventListener('orientationchange', () => setTimeout(schedule, 60));
  if (stage) new MutationObserver(schedule).observe(stage, { attributes: true, attributeFilter: ['data-widescreen'] });
  // menus vs race: captions only in a race
  if (stage) new MutationObserver(() => { root.dataset.screen = stage.dataset.screen || ''; }).observe(stage, { attributes: true, attributeFilter: ['data-screen'] });

  // ---- fullscreen ----------------------------------------------------------------------------------------------------
  const standalone = matchMedia?.('(display-mode: standalone)').matches || matchMedia?.('(display-mode: fullscreen)').matches || navigator.standalone;
  const canFullscreen = !!(html.requestFullscreen || html.webkitRequestFullscreen) && !/iPhone|iPod/.test(navigator.userAgent);
  const ios = /iPhone|iPad|iPod/.test(navigator.userAgent) || (/Macintosh/.test(navigator.userAgent) && navigator.maxTouchPoints > 1);
  if (standalone) fullBtn.hidden = true;
  async function toggleFullscreen() {
    const fsEl = doc.fullscreenElement || doc.webkitFullscreenElement;
    if (fsEl) { (doc.exitFullscreen || doc.webkitExitFullscreen)?.call(doc); return; }
    if (canFullscreen) {
      try { await (html.requestFullscreen ? html.requestFullscreen({ navigationUI: 'hide' }) : html.webkitRequestFullscreen()); } catch { prompt('home'); return; }
      try { await screen.orientation?.lock?.(innerWidth > innerHeight ? 'landscape' : 'portrait'); } catch {} // not everywhere; fine
      return;
    }
    prompt('home'); // iPhone Safari: no element fullscreen, a home-screen app is the fullscreen version
  }

  // ---- prompts: in-game message boxes (web/phone-prompts.js; assets loaded on first use) -------------------------------------
  // 'home': the Add to Home Screen hint; 'rotate': the frame has no room in this orientation (layout.cramped), shown
  // until the device turns or the player carries on (Cross) — then not again for that orientation this visit.
  let prompts = null, rotateDismissed = null, rotateForced = false;
  function prompt(kind) {
    const want = kind === 'rotate' ? layout?.want : null;
    const message = promptMessage(kind, { ios, want });
    prompts ??= createPhonePrompts({ ui });
    prompts.show(kind, message, () => { if (kind === 'rotate') rotateDismissed = layout?.landscape ? 'landscape' : 'portrait'; });
  }
  function checkRotate() {
    if (rotateForced) return;
    const orient = layout?.landscape ? 'landscape' : 'portrait';
    if (layout?.cramped && rotateDismissed !== orient) { if (prompts?.kind !== 'rotate') prompt('rotate'); }
    else if (prompts?.kind === 'rotate') prompts.hide();
  }

  // ---- ≡: the game's own Options -----------------------------------------------------------------------------------
  // Options > Display & Touch (web/fe-options.js fe-display) holds these settings. In a race the game pauses first and
  // the screen opens over the pause menu (Triangle goes back to it); on the menus ≡ opens Options (Square's screen).
  const BUSY = new Set(['loading', 'cutscene', 'transition']);
  function openSettings() {
    const fe = ui?.feScreens, s = ui?.screen;
    if (!fe?.ready || !ui.ready || BUSY.has(s) || fe.flash || fe.prompt || fe.keyboard) return false;
    if (s === 'fe-display') { fe.back(); return true; }
    if (s === 'game') {
      if (!isRunning()) return false;
      pause(); if (ui.screen === 'game') return false;        // no pause (online race, finished run): nothing opens
      return fe.openDisplay(ui.screen);
    }
    if (isRunning() || s === 'fe-options' || fe.sub?.(s)) return fe.openDisplay(s);   // paused race, or already in Options
    fe.openOptions(s); return true;
  }
  function setSetting(key, value) {
    if (!(key in TOUCH_DEFAULTS)) return;
    settings[key] = value; saveSettings(settings);
    if (key === 'show') show(value === 'on' || (value === 'auto' && (coarse || shown)));
    relayout();
  }
  function action(name) { if (name === 'menu') openSettings(); else if (name === 'full') toggleFullscreen(); }

  // initial state
  root.dataset.screen = stage?.dataset.screen || '';
  relayout();
  if (settings.show === 'on' || (settings.show === 'auto' && coarse)) show(true);

  const api = {
    touch,
    get shown() { return shown; },
    get coarse() { return coarse; },
    settings,                // the live settings (read by Options > Display & Touch)
    setSetting,              // (key, value): saved and applied at once (show/hide, layout)
    openSettings,
    prompt,
    get promptKind() { return prompts?.kind || null; },
    held: (code) => shown && touch.codes.has(code),
    // the gamepad for buildPad (main.js inputs()): the real one merged with the touch stick / Start
    pad(real) { if (gamepadActive(real)) show(false); return shown ? touch.pad(real) : real; },
    clear: releaseAll,
    relayout,
    show,
  };
  if (ui) ui.touchControls = api;
  window.__touchControls = api;                                        // QA / diagnostics (read-only use)
  const qa = new URL(location.href).searchParams.get('phonePrompt');   // QA: ?phonePrompt=home|rotate shows a prompt at once
  if (qa === 'home' || qa === 'rotate') { rotateForced = qa === 'rotate'; prompt(qa); }
  return api;
}
