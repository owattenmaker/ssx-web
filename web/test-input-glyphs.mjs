// Control hints follow the input in use (web/input-glyphs.js, docs/input-glyphs.md): PS2 button -> key per context
// (menus / race, Classic / Simple), the keys really do that (touch deck MENU_KEYS, pad-input.js buildPad), the FE atlas
// icons are recognised, key caps replace them in place, and the last device used decides.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  keyFor, keyCodeFor, keyLabel, MENU_BUTTON_KEYS, glyphButton, glyphRect, glyphKeyRect, drawGlyphAsKey, keyCapWidth, LUI_STRETCH, PS2_X, PS2_Y, MIN_CAP, CAP_HEIGHT,
  inputDevice, noteInput, noteTouchShown, pollGamepads, forceInputDevice, resetInputDevice, onInputDevice,
} from './input-glyphs.js';
import { MENU_KEYS, PAD_KEYS } from './touch-controls.js';
import { buildPad, createKeyboardContext, PAD_BUTTONS } from './pad-input.js';
import { LoadingScreen, keyboardRows } from './loading-screen.js';
import { TrickHud } from './trick-hud.js';

const ui = new URL('public/assets/UI/', import.meta.url);
const FEFONT = JSON.parse(fs.readFileSync(new URL('FEFONT-glyphs.json', ui)));

// ---- mapping -------------------------------------------------------------------------------------------------------
const menu = (b) => keyFor(b, { context: 'menu' }), race = (b, mode = 'Classic') => keyFor(b, { context: 'race', mode });
assert.deepEqual(['cross', 'triangle', 'square', 'circle', 'start'].map(menu), ['Space', 'Esc', 'Shift', 'Backspace', 'Enter'], 'menu face buttons');
assert.deepEqual(['l1', 'r1', 'l2', 'r2'].map(menu), ['Q', 'E', 'Z', 'X']);
assert.deepEqual(['up', 'down', 'left', 'right'].map(menu), ['↑', '↓', '←', '→']);
for (const [b, code] of Object.entries(MENU_KEYS)) assert.equal(MENU_BUTTON_KEYS[b], code, `menu ${b} = the touch deck's menu key`);
for (const mode of ['Classic', 'Simple']) {
  assert.deepEqual(['cross', 'square', 'circle', 'triangle', 'select', 'start', 'r3'].map((b) => race(b, mode)), ['Space', 'Shift', 'C', 'Y', 'Backspace', 'Esc', 'V'], `race face buttons (${mode})`);
  assert.deepEqual(['l1', 'r1', 'l2', 'r2'].map((b) => race(b, mode)), ['Q', 'E', 'Z', 'X']);
  assert.deepEqual(['lstick', 'rstick'].map((b) => race(b, mode)), ['W', 'T']);
}
assert.deepEqual(['up', 'left', 'down', 'right'].map((b) => race(b, 'Classic')), ['I', 'J', 'K', 'L'], 'Classic: IJKL D-pad');
assert.deepEqual(['up', 'left', 'down', 'right'].map((b) => race(b, 'Simple')), ['W', 'A', 'S', 'D'], 'Simple: WASD are the D-pad in the air');
assert.equal(keyFor('X'), 'Space'); assert.equal(keyFor('DPadUp', { context: 'race' }), 'I'); assert.equal(keyFor('Cross', { context: 'race' }), 'Space');
assert.equal(keyLabel('CapsLock'), 'Caps Lock'); assert.equal(keyLabel('ShiftRight'), 'Shift'); assert.equal(keyLabel(null), null);

// The race key really presses that PS2 button (buildPad; Simple D-pad keys pressed in the air).
const channel = { cross: 'Cross', square: 'Square', circle: 'Circle', triangle: 'Triangle', select: 'Select', r3: 'R3', l1: 'L1', r1: 'R1', l2: 'L2', r2: 'R2', up: 'DPadUp', down: 'DPadDown', left: 'DPadLeft', right: 'DPadRight' };
for (const mode of ['Classic', 'Simple']) for (const [b, name] of Object.entries(channel)) {
  const code = keyCodeFor(b, { context: 'race', mode }), kb = createKeyboardContext(mode); kb.air = true;
  const values = buildPad((c) => c === code, null, kb);
  assert.ok(values[PAD_BUTTONS.indexOf(name)] > 0, `${mode}: ${code} presses ${name}`);
}
// The touch deck's pad keys agree with the race keys for its buttons.
for (const [b, code] of Object.entries(PAD_KEYS)) if (!['up', 'down', 'left', 'right'].includes(b)) assert.equal(keyCodeFor(b, { context: 'race' }), code, `deck ${b}`);
// Same key names as the load screen's keyboard panel (its Jump / Boost / Hand plant / Reset / Pause rows).
const s = { turn: 'Turn', turn_spin_flip: 'Spin', jump: 'Jump', boost_tweak: 'Boost', grab_board: 'Grab', hand_plant: 'Plant', board_press: 'Press', reset: 'Reset', pause: 'Pause' };
const row = (label) => keyboardRows('Classic', s).find((r) => r.label === label).keys[0];
assert.deepEqual([race('cross'), race('square'), race('circle'), race('select'), race('start'), race('l1'), race('rstick')], [row('Jump'), row('Boost'), row('Plant'), row('Reset'), row('Pause'), row('Grab'), row('Press')]);
assert.equal(row('Spin'), race('up'), 'Classic D-pad row starts with the D-pad key');

// ---- FE atlas icons ------------------------------------------------------------------------------------------------
const who = (...r) => glyphButton(...r)?.button ?? null;
assert.deepEqual([12.5, 34.5, 56.5, 78.5].map((x) => who('FE_1-14', x, 123.5, 20, 19)), ['triangle', 'square', 'cross', 'circle'], 'LUI legend sprites');
assert.deepEqual([[9, 122], [10, 122], [11, 122], [33, 122], [55, 122], [78, 122]].map(([u, v]) => who('OV_1-2', u, v, 24, 24)), ['triangle', 'triangle', 'triangle', 'square', 'cross', 'circle'], 'ui.sprite cells');
assert.deepEqual([[122, 157, 26, 14], [151, 157, 25, 14], [122, 174, 26, 14], [151, 174, 25, 14], [121.5, 157.5, 27, 15], [151.5, 157.5, 27, 15]].map((r) => who('FE_1-14', ...r)), ['r1', 'l1', 'r2', 'l2', 'r1', 'l1'], 'shoulder badges');
assert.equal(who('FE_1-14', 7.5, 5.5, 169, 106), null, 'the controller picture is not an icon');
assert.equal(who('OV_1-6', 55, 122, 24, 24), null, 'other pages');
assert.equal(who('OV_1-2', 99, 122, 24, 24), null, 'the grey Start/Select bar');
assert.deepEqual(glyphRect(glyphButton('OV_1-2', 55, 122, 24, 24), 55, 122, 24, 24, 100, 200, 24, 24), [101, 201, 20, 20], 'visible icon box');

// ---- key caps in place of the icon ---------------------------------------------------------------------------------
// Same cap as the load screen panel (its width rule at 16 high in the LUI frame).
const loading = new LoadingScreen({ fonts: { FEFONT } });
for (const label of ['Space', 'Esc', 'Shift', 'Backspace', 'Enter', 'C', '←']) assert.ok(Math.abs(keyCapWidth(FEFONT, label, 16, LUI_STRETCH) - loading.keycapWidth(label)) < 1e-9, `cap width ${label}`);
function mockCanvas() {
  const calls = [];
  const c = { calls, save() {}, restore() {}, translate() {}, scale() {}, rotate() {}, beginPath() {}, closePath() {}, fill() {}, stroke() {}, moveTo() {}, lineTo() {}, drawImage() { calls.push(['image']); },
    createLinearGradient: () => ({ addColorStop() {} }), roundRect(x, y, w, h) { calls.push(['cap', x, y, w, h]); } };
  return c;
}
const text = []; const drawer = { fonts: { FEFONT }, keyboardMode: 'Classic', text: (c, value) => text.push(value) };
let c = mockCanvas();
assert.equal(drawGlyphAsKey(c, drawer, 'OV_1-2', 55, 122, 24, 24, 437, 350, 16, 16, { device: 'gamepad' }), false, 'gamepad keeps the PS2 icon');
assert.equal(drawGlyphAsKey(c, drawer, 'OV_1-2', 55, 122, 24, 24, 437, 350, 16, 16, { device: 'touch' }), false, 'touch keeps the PS2 icon');
assert.equal(c.calls.length, 0);
assert.equal(drawGlyphAsKey(c, drawer, 'OV_1-2', 55, 122, 24, 24, 437, 350, 16, 16, { device: 'keyboard' }), true);
let [, x, y, w, h] = c.calls[0], box = glyphRect(glyphButton('OV_1-2', 55, 122, 24, 24), 55, 122, 24, 24, 437, 350, 16, 16);
assert.equal(text.pop(), 'Space');
assert.ok(Math.abs(x + w - (box[0] + box[2])) < 1e-9, 'the cap keeps the icon\'s right edge (text follows it)');
assert.ok(Math.abs(y + h / 2 - (box[1] + box[3] / 2)) < 1e-9 && h === MIN_CAP, 'centred on the icon, at least MIN_CAP high');
assert.ok(w > box[2] * 2, 'a long label grows to the left');
c = mockCanvas(); drawGlyphAsKey(c, drawer, 'FE_1-14', 56.5, 123.5, 20, 19, 428, 400, 18, 18, { device: 'keyboard', stretch: LUI_STRETCH });
[, x, y, w, h] = c.calls[0]; assert.ok(Math.abs(h - Math.max(18 * CAP_HEIGHT, MIN_CAP * PS2_Y)) < 1e-9, 'LUI legend cap height'); assert.equal(text.pop(), 'Space');
c = mockCanvas(); drawGlyphAsKey(c, drawer, 'FE_1-14', 121.5, 157.5, 27, 15, 410, 360, 28, 16, { device: 'keyboard', stretch: LUI_STRETCH, keys: { r1: 'CapsLock' } });
assert.equal(text.pop(), 'Caps Lock', 'per-screen key override (name-entry keyboard)');
assert.equal(glyphKeyRect(drawer, 'OV_1-2', 10, 122, 24, 24, 0, 0, 16, 16, { device: 'keyboard' }).label, 'Esc');
assert.equal(glyphKeyRect(drawer, 'OV_1-2', 10, 122, 24, 24, 0, 0, 16, 16, { device: 'gamepad' }), null);
assert.equal(glyphKeyRect(drawer, 'OV_1-2', 10, 122, 24, 24, 0, 0, 16, 16, { device: 'keyboard', context: 'race' }).label, 'Y');
assert.ok(Math.abs(PS2_X / PS2_Y - LUI_STRETCH) < 1e-12);

// Uber trick hint: the caps take the icons' places and the line stays centred on x = 320.
const hud = new TrickHud(JSON.parse(fs.readFileSync(new URL('trick-hud.json', ui))), { FEFONT, HUDFONT: JSON.parse(fs.readFileSync(new URL('HUDFONT-glyphs.json', ui))) });
hud.resetUberHint(1);
const slots = Array(44).fill(null); slots[9] = { type: 9, maximum: -1, value: -0.5, arg: 1, field10: 0, points: 0 };
const hint = hud.frame(slots, { prepassSlots: slots, keys: (b) => keyFor(b, { context: 'race' }) }).filter((d) => d.hint);
assert.deepEqual(hint.filter((d) => d.kind === 'sprite').map((d) => d.key), ['E', 'Shift'], 'R1 = E, Square = Shift');
const last = hint[hint.length - 1]; assert.ok(Math.abs((hint[0].x + last.x + last.size[0] * last.scale[0]) / 2 - 320) < 2, 'centred');
assert.ok(hud.frame(slots, { prepassSlots: slots }).filter((d) => d.hint).every((d) => !d.key), 'no keys: the PS2 icons');

// ---- the active device ---------------------------------------------------------------------------------------------
const seen = []; onInputDevice((d) => seen.push(d));
resetInputDevice();
assert.equal(inputDevice(), 'keyboard', 'nothing used, no pad: keyboard');
const btn = (pressed) => ({ pressed, value: pressed ? 1 : 0 });
const pad = (pressed = [], axes = [0, 0, 0, 0]) => ({ index: 0, buttons: Array.from({ length: 17 }, (_, i) => btn(pressed.includes(i))), axes });
pollGamepads([pad()]); assert.equal(seen.at(-1), 'gamepad', 'a connected pad is the default');
noteInput('keyboard'); pollGamepads([pad()]); assert.equal(seen.at(-1), 'keyboard', 'a key press takes it back');
pollGamepads([pad([0])]); assert.equal(seen.at(-1), 'gamepad', 'Cross pressed');
noteInput('keyboard'); pollGamepads([pad([0])]); assert.equal(seen.at(-1), 'keyboard', 'a held button does not take it again');
pollGamepads([pad([], [0, 0.9, 0, 0])]); assert.equal(seen.at(-1), 'gamepad', 'a stick pushed past half');
noteInput('keyboard'); pollGamepads([pad([], [0, 0.9, 0, 0])]); assert.equal(seen.at(-1), 'keyboard', 'a resting stick does not');
resetInputDevice(); pollGamepads([pad([], [-1, 0, 0, 0])]); noteInput('keyboard'); pollGamepads([pad([], [-1, 0, 0, 0])]); assert.equal(seen.at(-1), 'keyboard', 'an axis resting off centre from the start does not');
noteTouchShown(true); assert.equal(seen.at(-1), 'touch', 'the touch deck shows PS2 buttons');
noteInput('keyboard'); noteTouchShown(false); assert.equal(seen.at(-1), 'keyboard', 'deck hidden by a key press');
noteInput('touch'); assert.equal(seen.at(-1), 'touch');
forceInputDevice('gamepad'); noteInput('keyboard'); assert.equal(seen.at(-1), 'gamepad', 'forced (QA ?glyphs=)'); forceInputDevice(null); assert.equal(seen.at(-1), 'keyboard');
resetInputDevice();
console.log('input glyphs: menu/race keys (Classic, Simple) match the handlers and buildPad, atlas icons -> key caps in place, device switching OK');
