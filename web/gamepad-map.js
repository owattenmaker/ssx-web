// Gamepad API snapshot -> the W3C "standard" layout the game reads (web/pad-input.js GAMEPAD_BUTTONS, axes 0..3).
// Pure: no DOM, tested in node (web/test-gamepad.mjs). web/gamepad.js polls the pads and calls mapPad() on each.
//
// A pad the browser already reports as mapping 'standard' is copied as it is (the PS2 control semantics of the
// existing standard mapping are unchanged). Anything else (DirectInput / RawInput pads on Windows, Firefox without a
// built-in remap, PS2-to-USB adapters) gets a layout: a known one by vendor / product id, else a generic heuristic
// (face and shoulder buttons in the standard order, triggers from buttons or rest-at-minus-one axes, the D-pad from a
// hat axis or buttons 12..15, sticks on the first axes). A player remap (Options > Controller, saved per pad) is
// applied on top of either.
//
// Layout sources, one string per standard button / axis ('' = nothing):
//   bN       raw button N (its value; pressed counts as at least 1)
//   aN+ aN-  half of raw axis N as a button (0..1)
//   tN       trigger axis N: -1 (rest) .. 1 once it has been seen below -0.5, else 0 .. 1
//   hN:up / hN:down / hN:left / hN:right   D-pad direction from hat axis N (8 positions in steps of 2/7, see hatBits)
//   x|y      the larger of several sources
// Axes: 'aN' or '-aN' (inverted), or 'bN-bM' (button M positive, N negative).

export const STD_BUTTONS = Object.freeze(['Cross', 'Circle', 'Square', 'Triangle', 'L1', 'R1', 'L2', 'R2',
  'Select', 'Start', 'L3', 'R3', 'DPadUp', 'DPadDown', 'DPadLeft', 'DPadRight', 'Home']);
export const STD_BUTTON_COUNT = 17, STD_AXIS_COUNT = 4;
const PRESSED = 0.5;

// ---- pad ids ----------------------------------------------------------------------------------------------------
// Chromium: "DUALSHOCK 4 Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)",
//           "USB Gamepad (Vendor: 0079 Product: 0006)", "Xbox 360 Controller (XInput STANDARD GAMEPAD)".
// Firefox:  "54c-9cc-Wireless Controller" (hex, not always zero padded), "xinput" on Windows.
// Safari:   the product name only ("DUALSHOCK 4 Wireless Controller"), normally mapping 'standard'.
export function parsePadId(id) {
  const s = String(id ?? '');
  const m = /Vendor:\s*([0-9a-f]{1,4})\s*Product:\s*([0-9a-f]{1,4})/i.exec(s) || /^([0-9a-f]{1,4})-([0-9a-f]{1,4})-/i.exec(s);
  const vendor = m ? m[1].toLowerCase().padStart(4, '0') : null, product = m ? m[2].toLowerCase().padStart(4, '0') : null;
  const name = s.replace(/^[0-9a-f]{1,4}-[0-9a-f]{1,4}-/i, '').replace(/\s*\([^()]*(?:STANDARD GAMEPAD|Vendor:)[^()]*\)\s*$/i, '').trim() || s;
  return { vendor, product, name, xinput: /xinput/i.test(s) };
}
// Key a remap is saved under: vendor:product when the id has one (the same pad in Chrome and Firefox), else the id.
export function padKey(pad) {
  const { vendor, product, name } = parsePadId(pad?.id);
  return vendor ? `${vendor}:${product}` : (name || 'pad').slice(0, 80);
}

// ---- hat switch ----------------------------------------------------------------------------------------------------
// One axis for the whole D-pad (Chromium RawInput / Firefox on Windows): up = -1, then clockwise in steps of 2/7 to
// up-left = 1; neutral is outside -1..1 (9/7 ~ 1.2857, some drivers 3.28) or 0 before the first report.
export const HAT = Object.freeze({ up: 1, down: 2, left: 4, right: 8 });
const HAT_POSITIONS = [HAT.up, HAT.up | HAT.right, HAT.right, HAT.down | HAT.right, HAT.down, HAT.down | HAT.left, HAT.left, HAT.up | HAT.left];
export function hatBits(v) {
  if (!Number.isFinite(v) || v === 0 || v < -1.05 || v > 1.05) return 0;
  const n = Math.round((v + 1) * 3.5);
  return n >= 0 && n <= 7 && Math.abs(v - (-1 + n * 2 / 7)) < 0.1 ? HAT_POSITIONS[n] : 0;
}

// ---- layouts ----------------------------------------------------------------------------------------------------------
const hat = (i) => [`h${i}:up`, `h${i}:down`, `h${i}:left`, `h${i}:right`];
const range = (from, n) => Array.from({ length: n }, (_, k) => `b${from + k}`);
// Standard order written out, for layouts that only differ in a few places.
export const IDENTITY = Object.freeze({ buttons: Object.freeze(range(0, 17)), axes: Object.freeze(['a0', 'a1', 'a2', 'a3']) });

// Known raw layouts by vendor:product. Chromium maps most of these to 'standard' itself; they matter for browsers and
// drivers that do not (Firefox, older Chromium, Safari HID). Filled from Chromium gamepad_standard_mappings_win.cc and
// Firefox GamepadRemapping.cpp.
// Raw indexing is the same in both (and in Chromium's macOS table): button = HID usage - 1, axis = usage - 0x30, so the
// hat switch (usage 0x39) is axes[9] and a pad's right stick is usually Z / Rz (axes 2 and 5).
export const KNOWN_LAYOUTS = new Map();
export function defineLayout(ids, layout) { for (const id of ids) KNOWN_LAYOUTS.set(id, Object.freeze({ ...layout, known: true })); }
const RZ = ['a0', 'a1', 'a2', 'a5'];
// DualShock 4 / DualSense / SCUF over RawInput: Square Cross Circle Triangle = b0..b3, analog L2/R2 on Rx/Ry (the digital
// b6/b7 copies are left out, they would cut the pressure short), Share/Options b8/b9, PS b12.
defineLayout(['054c:05c4', '054c:09cc', '054c:0ba0', '054c:0ce6', '054c:0df2', '054c:0e5f', '2e95:7725'],
  { name: 'dualshock4-dinput', buttons: ['b1', 'b2', 'b0', 'b3', 'b4', 'b5', 't3', 't4', 'b8', 'b9', 'b10', 'b11', ...hat(9), 'b12'], axes: RZ });
// DualShock 3 (Sixaxis): its own button order, D-pad as buttons 4..7, PS b16.
defineLayout(['054c:0268'], { name: 'dualshock3', buttons: ['b14', 'b13', 'b15', 'b12', 'b10', 'b11', 'b8', 'b9', 'b0', 'b3', 'b1', 'b2', 'b4', 'b6', 'b7', 'b5', 'b16'], axes: RZ });
// Logitech F310 / F510 / F710 with the switch on D.
defineLayout(['046d:c216', '046d:c218', '046d:c219'], { name: 'logitech-dinput', buttons: ['b1', 'b2', 'b0', 'b3', ...range(4, 8), ...hat(9), ''], axes: RZ });
// Xbox Wireless over Bluetooth when read through RawInput (Windows normally uses XInput / Windows.Gaming.Input: standard).
defineLayout(['045e:0b20', '045e:0b21', '045e:0b22', '045e:0b13', '045e:0b12'], {
  name: 'xbox-bt-rawinput',
  buttons: ['b0', 'b1', 'b3', 'b4', 'b6', 'b7', 't3', 't4', 'b10', 'b11', 'b13', 'b14', ...hat(9), 'b12'],
  axes: RZ
});
defineLayout(['045e:02e0', '045e:02fd'], {
  name: 'xbox-one-s-bt',
  buttons: ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 't2', 't5', 'b6', 'b7', 'b8', 'b9', ...hat(9), 'b10'],
  axes: ['a0', 'a1', 'a3', 'a4']
});
// 8BitDo over Bluetooth (X-input style order, triggers swapped).
defineLayout(['2dc8:301b', '2dc8:6012'], { name: '8bitdo-bt', buttons: ['b0', 'b1', 'b3', 'b4', 'b6', 'b7', 't4', 't3', 'b10', 'b11', 'b13', 'b14', ...hat(9), 'b12'], axes: RZ });
// HORIPAD (Switch).
defineLayout(['0f0d:00c1'], { name: 'horipad', buttons: ['b1', 'b2', 'b0', 'b3', ...range(4, 8), ...hat(9), 'b12'], axes: RZ });
// PS2 to USB adapters. SmartJoy PLUS: Triangle Circle Cross Square = b0..b3, L2 R2 L1 R1 = b4..b7, Start b8, Select b9.
defineLayout(['0925:0005'], { name: 'smartjoy-ps2', buttons: ['b2', 'b1', 'b3', 'b0', 'b6', 'b7', 'b4', 'b5', 'b9', 'b8', 'b10', 'b11', ...hat(9), ''], axes: RZ });
defineLayout(['0e6f:0003'], { name: 'xgear-ps2', buttons: ['b2', 'b1', 'b3', 'b0', 'b6', 'b7', 'b4', 'b5', 'b8', 'b9', 'b10', 'b11', ...hat(9), ''], axes: ['a0', 'a1', 'a5', 'a2'] });
defineLayout(['6666:0667'], { name: 'boom-psx', buttons: ['b2', 'b1', 'b3', 'b0', 'b6', 'b7', 'b4', 'b5', 'b8', 'b11', 'b9', 'b10', 'b12', 'b14', 'b15', 'b13', ''], axes: RZ });
// DragonRise generic USB pads (many PS-style pads and PS1/PS2 adapters): Chromium keeps the buttons as they come.
defineLayout(['0079:0006'], { name: 'dragonrise', buttons: [...range(0, 12), ...hat(9), ''], axes: RZ });
// SNES-style "2Axes 8Keys" (0079:0011): no sticks, the D-pad is axes 0/1 (the game's ground turn reads the D-pad too).
defineLayout(['0079:0011'], { name: 'snes-usb', buttons: ['b2', 'b1', 'b3', 'b0', 'b4', 'b5', '', '', 'b8', 'b9', '', '', 'a1-', 'a1+', 'a0-', 'a0+', ''], axes: ['', '', '', ''] });

// Generic fallback: what a DirectInput / HID pad without a known layout most often looks like in the browser.
//   buttons 0..11 in the standard order (face, shoulders, triggers, Select/Start, stick clicks);
//   D-pad: a hat on axis 9 when there are 10+ axes (Chromium / Firefox index axes by HID usage, the hat is usage 0x39),
//          else buttons 12..15;
//   triggers: buttons 6/7, or the Rx/Ry axes (3/4) when those rest at -1 (usage-indexed pads);
//   right stick: Z / Rz (axes 2 and 5) on usage-indexed pads, else axes 2 and 3.
export function genericLayout(raw) {
  const nb = raw?.buttons?.length ?? 0, na = raw?.axes?.length ?? 0, usage = na >= 10;
  const buttons = range(0, 12);
  if (usage) { buttons[6] = 'b6|t3'; buttons[7] = 'b7|t4'; }
  buttons.push(...(usage ? hat(9) : nb >= 16 ? range(12, 4) : ['', '', '', '']), nb > 16 ? 'b16' : '');
  return { name: 'generic', buttons, axes: ['a0', 'a1', 'a2', usage ? 'a5' : 'a3'] };
}

export function layoutFor(raw) {
  if (raw?.mapping === 'standard') return { name: 'standard', standard: true, ...IDENTITY };
  const { vendor, product } = parsePadId(raw?.id);
  const known = vendor && KNOWN_LAYOUTS.get(`${vendor}:${product}`);
  if (known) return known;
  return genericLayout(raw);
}
// A player remap on top of a layout: { buttons: { stdIndex: source }, axes: { stdIndex: source } }.
export function withRemap(layout, remap) {
  if (!remap) return layout;
  const buttons = [...layout.buttons], axes = [...layout.axes];
  for (const [k, v] of Object.entries(remap.buttons || {})) if (+k >= 0 && +k < STD_BUTTON_COUNT) buttons[+k] = String(v ?? '');
  for (const [k, v] of Object.entries(remap.axes || {})) if (+k >= 0 && +k < STD_AXIS_COUNT) axes[+k] = String(v ?? '');
  return { ...layout, name: layout.name + '+remap', standard: false, remapped: true, buttons, axes };
}

// ---- compile + map ------------------------------------------------------------------------------------------------------
const TERM = /^(?:b(\d+)|a(\d+)([+-])|t(\d+)|h(\d+):(up|down|left|right))$/;
function compileButton(spec) {
  const out = [];
  for (const part of String(spec || '').split('|')) {
    const m = TERM.exec(part.trim()); if (!m) continue;
    if (m[1] != null) out.push({ k: 0, i: +m[1] });
    else if (m[2] != null) out.push({ k: 1, i: +m[2], s: m[3] === '-' ? -1 : 1 });
    else if (m[4] != null) out.push({ k: 2, i: +m[4] });
    else out.push({ k: 3, i: +m[5], bit: HAT[m[6]] });
  }
  return out;
}
function compileAxis(spec) {
  const s = String(spec || '').trim();
  let m = /^(-?)a(\d+)$/.exec(s); if (m) return { k: 0, i: +m[2], s: m[1] ? -1 : 1 };
  m = /^b(\d+)-b(\d+)$/.exec(s); if (m) return { k: 1, n: +m[1], p: +m[2] };
  return null;
}
export function compileLayout(layout) {
  return { ...layout, bt: layout.buttons.slice(0, STD_BUTTON_COUNT).map(compileButton), ax: layout.axes.slice(0, STD_AXIS_COUNT).map(compileAxis) };
}
// Per pad axis history for 'tN' triggers: the lowest value seen.
export function newCalibration() { return { min: [] }; }
export function calibrate(cal, raw) {
  const axes = raw?.axes || [];
  for (let i = 0; i < axes.length; i++) { const v = +axes[i]; if (Number.isFinite(v) && !(cal.min[i] <= v)) cal.min[i] = v; }
}
function rawButton(raw, i) {
  const b = raw?.buttons?.[i]; if (b == null) return 0;
  if (typeof b === 'number') return b;          // very old Chromium: plain numbers
  const v = Number.isFinite(+b.value) ? +b.value : 0;
  return b.pressed ? Math.max(v, 1) : v;
}
function term(t, raw, cal) {
  if (t.k === 0) return rawButton(raw, t.i);
  const v = +raw?.axes?.[t.i]; if (!Number.isFinite(v)) return 0;
  if (t.k === 1) return Math.max(0, Math.min(1, v * t.s));
  if (t.k === 2) return Math.max(0, Math.min(1, cal?.min?.[t.i] <= -0.5 ? (v + 1) / 2 : v));
  return hatBits(v) & t.bit ? 1 : 0;
}
// A standard-layout pad object for buildPad(): { id, index, mapping, connected, timestamp, buttons[17], axes[4] }.
export function newStandardPad() {
  return { id: '', index: -1, mapping: 'standard', connected: true, timestamp: 0, layout: '',
    buttons: Array.from({ length: STD_BUTTON_COUNT }, () => ({ pressed: false, touched: false, value: 0 })), axes: [0, 0, 0, 0] };
}
// raw: a Gamepad snapshot; compiled: compileLayout(layout); out: newStandardPad() (reused, no allocation).
export function mapPad(raw, compiled, cal, out = newStandardPad()) {
  out.id = raw?.id ?? ''; out.index = raw?.index ?? -1; out.timestamp = raw?.timestamp ?? 0; out.layout = compiled.name;
  if (compiled.standard) {                              // the browser's own standard mapping, copied as it is
    for (let i = 0; i < STD_BUTTON_COUNT; i++) {
      const b = raw?.buttons?.[i], o = out.buttons[i];
      if (b == null) { o.pressed = false; o.touched = false; o.value = 0; continue; }
      if (typeof b === 'number') { o.value = b; o.pressed = b > PRESSED; o.touched = o.pressed; continue; }
      o.pressed = !!b.pressed; o.touched = !!(b.touched ?? b.pressed); o.value = Number.isFinite(+b.value) ? +b.value : 0;
    }
    for (let k = 0; k < STD_AXIS_COUNT; k++) { const v = +raw?.axes?.[k]; out.axes[k] = Number.isFinite(v) ? v : 0; }
    return out;
  }
  for (let i = 0; i < STD_BUTTON_COUNT; i++) {
    let v = 0, p = false;
    for (const t of compiled.bt[i] || []) {
      const x = term(t, raw, cal); if (x > v) v = x;
      // a raw button keeps the browser's pressed flag (trigger thresholds)
      if (t.k === 0) { const b = raw?.buttons?.[t.i]; if (b != null && (typeof b === 'number' ? b > PRESSED : b.pressed)) p = true; }
    }
    const o = out.buttons[i]; o.value = Math.min(1, v); o.pressed = p || v > PRESSED; o.touched = v > 0;
  }
  for (let k = 0; k < STD_AXIS_COUNT; k++) {
    const a = compiled.ax[k]; let v = 0;
    if (a?.k === 0) { const r = +raw?.axes?.[a.i]; v = Number.isFinite(r) && Math.abs(r) <= 1.0001 ? r * a.s : 0; }
    else if (a?.k === 1) v = (rawButton(raw, a.p) > PRESSED ? 1 : 0) - (rawButton(raw, a.n) > PRESSED ? 1 : 0);
    out.axes[k] = v;
  }
  return out;
}
