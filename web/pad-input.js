// Host devices -> the 24 decoded PS2 pad channels consumed by the original input path
// (engine/original_input.cpp originalDecodePad, pressure + analog mode). The core's
// pad_tick runs the original button history, INPUT.MAP and provider on these values, so
// no gameplay meaning is assigned here: this file only says which physical control is
// which DualShock 2 control.
//
// Channel order: 0 Select, 1 Start, 2 L3, 3 R3, 4 D-right, 5 D-left, 6 D-up, 7 D-down,
// 8 Triangle, 9 Circle, 10 Cross, 11 Square, 12 L1, 13 R1, 14 L2, 15 R2,
// 16/17 right stick left/right, 18/19 right stick up/down, 20/21 left stick left/right,
// 22/23 left stick up/down.
export const PAD_BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown',
  'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];

// Keyboard: arrows/WASD = left stick, IJKL = D-pad, TFGH = right stick (board press/pivot),
// Space = Cross, Shift = Square, C = Circle (handplant), Y = Triangle, Q/Z/E/X = L1/L2/R1/R2,
// V = R3 (ollie from a board press), Backspace = Select (reset to course).
export const KEYBOARD_BUTTONS = {
  Select: ['Backspace'], Start: [], L3: [], R3: ['KeyV'],
  DPadRight: ['KeyL'], DPadLeft: ['KeyJ'], DPadUp: ['KeyI'], DPadDown: ['KeyK'],
  Triangle: ['KeyY'], Circle: ['KeyC'], Cross: ['Space'], Square: ['ShiftLeft', 'ShiftRight'],
  L1: ['KeyQ'], R1: ['KeyE'], L2: ['KeyZ'], R2: ['KeyX'],
};
// W3C standard gamepad layout index for each PS2 button.
export const GAMEPAD_BUTTONS = [8, 9, 10, 11, 15, 14, 12, 13, 3, 1, 0, 2, 4, 5, 6, 7];

const RECIPROCAL_255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
// MUL.S runs with round-toward-zero on the EE.
export function towardZero(x) {
  const scratch = new Float32Array([x]);
  if (Math.abs(scratch[0]) > Math.abs(x)) new Uint32Array(scratch.buffer)[0] -= 1;
  return scratch[0];
}
// Nearest byte on the symmetric 0..255 range; 0 is left/up (engine/input_adapter.cpp).
export function axisByte(value) {
  if (!Number.isFinite(value)) throw new Error('Nonfinite stick value');
  return Math.floor((Math.max(-1, Math.min(1, value)) + 1) * 127.5 + 0.5);
}
// 0x327210..0x3276BC axial response: 79..176 is dead; the rest ramps to full scale.
export function stickChannels(byte) {
  const negative = Math.max(Math.trunc((79 - byte) * 255 / 79), 0);
  const positive = Math.max(Math.trunc((byte - 176) * 255 / 79), 0);
  return [towardZero(negative * RECIPROCAL_255), towardZero(positive * RECIPROCAL_255)];
}
export function pressureChannel(amount) {
  const byte = Math.floor(Math.max(0, Math.min(1, amount)) * 255 + 0.5);
  return towardZero(byte * RECIPROCAL_255);
}

// Keyboard "Simple" mode: arrows/WASD are the left stick on the ground (carving, rails) and become the
// D-pad (spins/flips) in the air, so a keyboard alone can trick. Each direction keeps the role it got when it
// was pressed until it is released:
//   - pressed on the ground: left stick (a direction held through the takeoff keeps steering = air adjust);
//   - pressed in the air: D-pad (a spin/flip held through the landing stays a D-pad press, so a D-pad spin
//     can be buffered into a crouch/prewind exactly like on a DualShock);
//   - pressed in the air while the opposite direction is held as a D-pad spin: left stick. This gives the
//     DualShock "D-pad spin + opposite stick" combination (540 spinboost) that one set of keys otherwise
//     cannot express (D-pad left+right would only cancel the spin, INPUT.MAP Spin = DPadR - DPadL).
// On the ground the original CruiseTurn/CruiseCrouch/CruiseBrake read max(D-pad, stick), so a D-pad-role key
// still carves. "Classic" keeps the fixed layout (IJKL D-pad). Gamepads are unaffected.
export const KEYBOARD_MODES = Object.freeze(['Simple', 'Classic']);
const DIRECTIONS = Object.freeze({ left: ['KeyA', 'ArrowLeft'], right: ['KeyD', 'ArrowRight'], up: ['KeyW', 'ArrowUp'], down: ['KeyS', 'ArrowDown'] });
const OPPOSITE = Object.freeze({ left: 'right', right: 'left', up: 'down', down: 'up' });
const DPAD_INDEX = Object.freeze({ right: 4, left: 5, up: 6, down: 7 });
// steering: direction -> { role: 'stick' | 'dpad', order } for every held Simple-mode direction.
export function createKeyboardContext(mode = 'Classic') { return { mode, air: false, steering: new Map(), serial: 0 }; }
const KEYBOARD_KEY = 'ssx3.keyboard';
// Classic (the fixed layout) is the default
export function loadKeyboardMode() { try { const v = localStorage.getItem(KEYBOARD_KEY); return KEYBOARD_MODES.includes(v) ? v : 'Classic'; } catch { return 'Classic'; } }
export function saveKeyboardMode(mode) { try { localStorage.setItem(KEYBOARD_KEY, mode); } catch {} }

// down(code) -> bool for keyboard/touch; pad is a standard-mapping Gamepad or null; keyboard is an optional
// createKeyboardContext() whose .air flag the caller updates from the running controller (4/5 = airborne).
// Stick axes use the Gamepad API convention (+x right, +y down), which is the PS2 byte order.
export function buildPad(down, pad, keyboard = null) {
  if (keyboard && keyboard.mode === 'Simple') return buildSimplePad(down, pad, keyboard);
  return buildClassicPad(down, pad);
}
function buildSimplePad(down, pad, keyboard) {
  const held = Object.fromEntries(Object.entries(DIRECTIONS).map(([d, codes]) => [d, codes.some(down)]));
  if (!(keyboard.steering instanceof Map)) keyboard.steering = new Map();
  const roles = keyboard.steering;
  for (const d of [...roles.keys()]) if (!held[d]) roles.delete(d);
  for (const d of Object.keys(DIRECTIONS)) if (held[d] && !roles.has(d)) {
    const role = !keyboard.air ? 'stick' : roles.get(OPPOSITE[d])?.role === 'dpad' ? 'stick' : 'dpad';
    roles.set(d, { role, order: keyboard.serial = (keyboard.serial || 0) + 1 });
  }
  const moveCodes = new Set(Object.values(DIRECTIONS).flat());
  // Movement keys are routed below; everything else keeps the classic layout.
  const values = buildClassicPad((code) => !moveCodes.has(code) && down(code), pad);
  for (const [d, index] of Object.entries(DPAD_INDEX)) if (roles.get(d)?.role === 'dpad') values[index] = pressureChannel(1);
  // Stick axis: the most recently pressed stick-role direction of the pair wins (else the gamepad stick).
  const axis = (negative, positive, value) => {
    const n = roles.get(negative), p = roles.get(positive);
    const on = (r) => r && r.role === 'stick';
    if (on(p) && (!on(n) || p.order > n.order)) return 1;
    if (on(n)) return -1;
    return Number.isFinite(value) ? value : 0;
  };
  const lx = axis('left', 'right', pad?.axes[0]), ly = axis('up', 'down', pad?.axes[1]);
  [lx, ly].forEach((v, k) => { const [n, p] = stickChannels(axisByte(v)); values[20 + 2 * k] = n; values[21 + 2 * k] = p; });
  return values;
}
function buildClassicPad(down, pad) {
  const values = new Float32Array(24);
  PAD_BUTTONS.forEach((name, i) => {
    let amount = KEYBOARD_BUTTONS[name].some(down) ? 1 : 0;
    const button = pad?.buttons[GAMEPAD_BUTTONS[i]];
    if (button) amount = Math.max(amount, button.pressed ? Math.max(button.value, 1) : button.value);
    // Select/Start/L3/R3 are digital; D-pad, face and shoulder buttons are pressure bytes.
    values[i] = i < 4 ? (amount >= 0.5 ? 1 : 0) : pressureChannel(amount);
  });
  const key = (codes) => codes.some(down);
  const axis = (negative, positive, value) => key(positive) ? 1 : key(negative) ? -1 : (Number.isFinite(value) ? value : 0);
  const rx = axis(['KeyF'], ['KeyH'], pad?.axes[2]), ry = axis(['KeyT'], ['KeyG'], pad?.axes[3]);
  const lx = axis(['KeyA', 'ArrowLeft'], ['KeyD', 'ArrowRight'], pad?.axes[0]), ly = axis(['KeyW', 'ArrowUp'], ['KeyS', 'ArrowDown'], pad?.axes[1]);
  [rx, ry, lx, ly].forEach((v, k) => { const [n, p] = stickChannels(axisByte(v)); values[16 + 2 * k] = n; values[17 + 2 * k] = p; });
  return values;
}
