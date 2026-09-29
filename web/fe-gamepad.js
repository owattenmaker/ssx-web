// Options > Controller Settings > Gamepad: the pad the game reads, a live pad test and a button remap, in the game's own
// screens (web/fe-options.js). Controller Settings (22control) gets a fifth row, "Gamepad", under Controller 2P
// (controlWithPad: the Controller 2P row's look one row lower, its own focus state); it opens 'fe-pad', a Game Options
// (19game_opt) copy with the rows below (fe-options.js displayScreen), the Previous / Reset options legend kept.
//   Gamepad      the active pad (web/gamepad.js: every slot, the last one used); Left / Right picks another connected pad
//   Layout       how its buttons are read: Standard (the browser's mapping), a known layout by vendor / product, Generic,
//                "Custom" after a remap
//   Buttons      the PS2 buttons the game sees right now; Cross starts a test (the menu ignores the pad until it has
//                been left alone for TEST_IDLE_MS, so Triangle / the D-pad can be tried without leaving the screen)
//   Left / Right stick   the stick values the game sees
//   Remap        Cross starts "press the button for ..." through every PS2 control; saved per pad (vendor:product)
// Square (Reset options) clears this pad's remap after the game's Yes/No box.
import { pollPads, activeEntry, connectedPads, selectPad, rawControls, saveRemap, clearRemap, padRemap } from './gamepad.js';
import { parsePadId, STD_BUTTONS } from './gamepad-map.js';

export const PAD_SCREEN = 'fe-pad';
export const PAD_TITLE = 'Gamepad';
export const PAD_ROWS = Object.freeze([
  { label: 'Gamepad', help: 'The pad the game reads. Left / Right: another pad.' },
  { label: 'Layout', help: 'How the pad\'s buttons are read. Custom after a remap.' },
  { label: 'Buttons', help: 'Cross tests every button without moving the menu.' },
  { label: 'Left stick', help: 'Move the left stick: the values the game sees.' },
  { label: 'Right stick', help: 'Move the right stick: the values the game sees.' },
  { label: 'Remap buttons', help: 'Press the pad\'s button for each PS2 button in turn.' },
]);
export const PAD_ENTRY = Object.freeze({ label: 'Gamepad', help: 'Test the pad and set which button is which.' });
// Controller Settings row names (22control) and its focus state.
export const CONTROL_PAD = Object.freeze({ option: '0d16ff00', label: '0d16ff01', value: '0d16ff02', help: '0d16ff03', frame: 55 });
const C22 = { menu: '00053c55', option: '00000034', label: '0b7e72b0', value: '00003b22', lastFrame: 50, frames: [35, 40, 45, 50],
  arrows: '046fd665', bar: '000006ec', helps: ['0d60c562', '03656bc5', '01e46e1e', '00c57d40'], pitch: 20 };

const LAYOUT_LABELS = { standard: 'Standard', generic: 'Generic', 'dualshock4-dinput': 'DualShock 4', dualshock3: 'DualShock 3', 'logitech-dinput': 'Logitech D',
  'xbox-bt-rawinput': 'Xbox Bluetooth', 'xbox-one-s-bt': 'Xbox One S', '8bitdo-bt': '8BitDo', horipad: 'HORIPAD', 'smartjoy-ps2': 'PS2 adapter',
  'xgear-ps2': 'PS2 adapter', 'boom-psx': 'PSX adapter', dragonrise: 'USB gamepad', 'snes-usb': 'SNES pad' };
export function layoutLabel(name = '') { const base = String(name).replace(/\+remap$/, ''); return /\+remap$/.test(name) ? 'Custom' : LAYOUT_LABELS[base] || base || '-'; }
const VENDORS = { '054c': 'PlayStation', '045e': 'Xbox', '057e': 'Nintendo', '046d': 'Logitech', '2dc8': '8BitDo' };
export function padName(entry, max = 18) {
  if (!entry) return 'None';
  const id = parsePadId(entry.id), maker = VENDORS[id.vendor];
  let n = id.name.replace(/\(R\)|\(TM\)/gi, '').replace(/\s+/g, ' ').trim() || 'Gamepad';
  if (/^xinput$/i.test(n)) return 'Xbox pad';                                   // Firefox on Windows names every XInput pad "xinput"
  if (/^wireless controller$/i.test(n) && maker) return `${maker} pad`.length > max ? maker : `${maker} pad`;   // Firefox: DualShock 4 / DualSense
  if (n.length > max) { const short = n.replace(/\s+(Wireless\s+)?(Controller|Gamepad|Game Pad|Joystick)$/i, '').trim(); if (short.length >= 4) n = short; }
  return n.length > max ? n.slice(0, max - 1).trimEnd() + '.' : n;
}
// PS2 names (the Controller Settings picture's own words where it has them).
const SHORT = { DPadUp: 'Up', DPadDown: 'Down', DPadLeft: 'Left', DPadRight: 'Right' };
export function heldText(pad, max = 3) {
  if (!pad) return '-';
  const on = []; for (let i = 0; i < 16; i++) if (pad.buttons[i]?.pressed || pad.buttons[i]?.value > 0.5) on.push(SHORT[STD_BUTTONS[i]] || STD_BUTTONS[i]);
  if (!on.length) return '-';
  return on.length > max ? on.slice(0, max).join(' ') + ` +${on.length - max}` : on.join(' ');
}
const num = (v) => (Math.abs(v) < 0.005 ? 0 : v).toFixed(2);
export function stickText(pad, k) { if (!pad) return '-'; return `${num(+pad.axes[k] || 0)}  ${num(+pad.axes[k + 1] || 0)}`; }

// ---- Controller Settings + Gamepad row ---------------------------------------------------------------------------------
export function controlWithPad(base, entry = PAD_ENTRY) {
  const by = new Map(base.elements.map((e) => [e.name, e])), clone = (o) => JSON.parse(JSON.stringify(o));
  const menu = by.get(C22.menu), row = by.get(C22.option); if (!menu || !row || !by.get(C22.label) || !by.get(C22.value)) return base;
  const N = CONTROL_PAD, orange = { 14: 37, 15: 7, 16: 5 }, white = { 14: 255, 15: 255, 16: 255 };
  const elements = base.elements.map((e) => {
    const c = clone(e);
    if (e.name === C22.menu) c.children = [...e.children, N.option];
    return c;
  });
  elements.push({ ...clone(row), name: N.option, index: 3200, label: '5', children: [N.label, N.value], props: { ...row.props, 1: (row.props?.[1] ?? 0) + C22.pitch } },
    { ...clone(by.get(C22.label)), name: N.label, index: 3201, parent: N.option, text: entry.label },
    { ...clone(by.get(C22.value)), name: N.value, index: 3202, parent: N.option, text: '' },
    { ...clone(by.get(C22.helps[0])), name: N.help, index: 3203, text: entry.help });
  const events = [];
  for (const ev of base.events) {
    events.push(clone(ev));
    // the new row fades in with the others and is orange in every other row's focus state
    if (ev.element === C22.label || ev.element === C22.value) {
      const name = ev.element === C22.label ? N.label : N.value;
      if (ev.frame < C22.frames[0]) events.push({ ...clone(ev), element: name });
      else if (C22.frames.includes(ev.frame) && ev.props) events.push({ frame: ev.frame, element: name, props: { ...ev.props, ...orange } });
    }
    if (C22.frames.includes(ev.frame) && ev.element === C22.helps[0] && ev.props) events.push({ frame: ev.frame, element: N.help, props: { ...ev.props, 13: 0 } });
  }
  // its focus state: Controller 2P's with the roles swapped, the arrows and the bar a row lower, its help line shown
  for (const ev of base.events.filter((x) => x.frame === C22.lastFrame)) {
    const c = { ...clone(ev), frame: N.frame };
    if ((ev.element === C22.label || ev.element === C22.value) && ev.props) c.props = { ...ev.props, ...orange };
    else if ((ev.element === C22.arrows || ev.element === C22.bar) && ev.props) c.props = { ...ev.props, 1: (ev.props[1] ?? 0) + C22.pitch };
    else if (C22.helps.includes(ev.element) && ev.props) c.props = { ...ev.props, 13: 0 };
    events.push(c);
    if (ev.element === C22.label && ev.props) events.push({ frame: N.frame, element: N.label, props: { ...ev.props, ...white } });
    if (ev.element === C22.value && ev.props) events.push({ frame: N.frame, element: N.value, props: { ...ev.props, ...white } });
    if (ev.element === C22.helps[0] && ev.props) events.push({ frame: N.frame, element: N.help, props: { ...ev.props, 13: 255 } });
  }
  return { ...base, elements, events };
}

// ---- remap ---------------------------------------------------------------------------------------------------------------
// Steps: the 16 PS2 buttons in the Controller Settings order, then the two sticks (right and down give the sign).
export const REMAP_STEPS = Object.freeze([
  ...[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15].map((i) => ({ button: i, name: SHORT[STD_BUTTONS[i]] ? 'D-pad ' + SHORT[STD_BUTTONS[i]] : STD_BUTTONS[i] })),
  { axis: 0, name: 'Left stick right' }, { axis: 1, name: 'Left stick down' }, { axis: 2, name: 'Right stick right' }, { axis: 3, name: 'Right stick down' },
]);
export const REMAP_SKIP_MS = 5000, TEST_IDLE_MS = 4000;
// A raw control (gamepad.js rawControls) -> the layout source for this step, or null when it does not fit the step.
export function sourceFor(step, control, entry) {
  const m = /^a(\d+)([+-])$/.exec(control);
  if (step.axis != null) return m ? (m[2] === '+' ? `a${m[1]}` : `-a${m[1]}`) : null;
  if (m) return (entry?.rest?.[+m[1]] ?? 0) <= -0.5 && m[2] === '+' ? `t${m[1]}` : control;   // a trigger resting at -1
  return control;                                                                            // bN, hN:dir
}
// Pure remap state machine (tested in node): feed it rawControls() each frame.
export function createRemap(entry, now = 0) {
  return { entry, step: 0, at: now, before: null, release: null, map: { buttons: {}, axes: {} }, done: false };
}
export function remapStep(r, controls, now) {
  if (r.done) return r;
  if (r.before == null) { r.before = new Set(controls); r.at = now; return r; }   // held when it started: not a press
  if (r.release) { if (![...r.release].some((c) => controls.has(c))) { r.release = null; r.before = new Set(controls); r.at = now; } return r; }
  for (const c of [...r.before]) if (!controls.has(c)) r.before.delete(c);
  const step = REMAP_STEPS[r.step];
  const fresh = [...controls].filter((c) => !r.before.has(c));
  const pick = fresh.map((c) => [c, sourceFor(step, c, r.entry)]).find(([, src]) => src);
  if (pick) {
    const [control, src] = pick;
    if (step.axis != null) r.map.axes[step.axis] = src; else r.map.buttons[step.button] = src;
    r.release = new Set([control]); r.step++; r.at = now;
  } else if (now - r.at >= REMAP_SKIP_MS) { r.step++; r.at = now; }
  if (r.step >= REMAP_STEPS.length) r.done = true;
  return r;
}
// The finished map on top of the pad's layout: controls that were skipped lose any raw input taken by another control.
export function finishRemap(r, baseLayout) {
  const map = { buttons: { ...r.map.buttons }, axes: { ...r.map.axes } };
  const taken = new Set(Object.values(map.buttons).map(String));
  for (let i = 0; i < 16; i++) {
    if (map.buttons[i] != null) continue;
    const parts = String(baseLayout?.buttons?.[i] ?? '').split('|').filter(Boolean), keep = parts.filter((p) => !taken.has(p));
    if (keep.length !== parts.length) map.buttons[i] = keep.join('|');
  }
  return map;
}

// The screen's rows (fe-options.js optionSpecs) and its input / drawing hooks.
export class PadSetup {
  constructor(extra) { this.x = extra; this.remap = null; this.test = null; this.note = null; }
  entry() { pollPads(); return activeEntry(); }
  specs() {
    const pad = () => { const e = this.entry(); return e ? e.std : null; }, noPad = () => !this.entry();
    return [
      { values: () => [padName(this.entry())], get: () => 0, step: (dir) => this.cycle(dir) },
      { values: () => [this.entry() ? layoutLabel(this.entry().layout?.name) : '-'], get: () => 0, disabled: noPad },
      { values: () => [heldText(pad())], get: () => 0, disabled: noPad, choose: () => this.startTest() },
      { values: () => [stickText(pad(), 0)], get: () => 0, disabled: noPad },
      { values: () => [stickText(pad(), 2)], get: () => 0, disabled: noPad },
      { values: () => [this.remapValue()], get: () => 0, disabled: noPad, choose: () => this.startRemap() },
    ];
  }
  cycle(dir) {
    const pads = connectedPads(), cur = this.entry(); if (pads.length < 2) return;
    const i = pads.indexOf(cur), next = pads[((i < 0 ? 0 : i + dir) % pads.length + pads.length) % pads.length];
    selectPad(next);
  }
  remapValue() {
    if (this.remap) return REMAP_STEPS[this.remap.step]?.name ? 'Press ' + REMAP_STEPS[this.remap.step].name : '';
    if (this.note && performance.now() - this.note.at < 2500) return this.note.text;
    return padRemap(this.entry()) ? 'Custom' : 'Start';
  }
  startTest() { if (this.entry()) this.test = { at: performance.now(), before: null }; }
  // The help line of the Buttons row while a test runs.
  testHelp() {
    const t = this.test; if (!t) return null;
    return `Testing: press anything. Stops in ${Math.max(0, Math.ceil((TEST_IDLE_MS - (performance.now() - t.at)) / 1000))} s untouched.`;
  }
  // The help line of the Remap row while a remap runs.
  remapHelp() {
    const r = this.remap; if (!r) return null;
    const left = Math.max(0, Math.ceil((REMAP_SKIP_MS - (performance.now() - r.at)) / 1000));
    return r.release ? 'Let go of the button.' : `Press the pad's button for ${REMAP_STEPS[r.step].name}. Skips in ${left}.`;
  }
  startRemap() { const e = this.entry(); if (!e) return; this.remap = createRemap(e, performance.now()); this.note = null; }
  cancel(text = 'Cancelled') { this.remap = null; this.note = { text, at: performance.now() }; }
  tick() {
    if (this.test) {
      const e = this.entry(), held = e ? rawControls(e, 0.3) : new Set(), now = performance.now();
      if (!e) this.test = null;
      else { if (held.size) this.test.at = now; if (now - this.test.at >= TEST_IDLE_MS) this.test = null; }
    }
    const r = this.remap; if (!r) return;
    pollPads();
    if (!connectedPads().includes(r.entry)) { this.cancel('Pad lost'); return; }
    remapStep(r, rawControls(r.entry), performance.now());
    if (r.done) {
      this.remap = null;
      const map = finishRemap(r, r.entry.baseLayout);
      if (Object.keys(r.map.buttons).length || Object.keys(r.map.axes).length) { saveRemap(r.entry, map); this.note = { text: 'Saved', at: performance.now() }; }
      else this.note = { text: 'No change', at: performance.now() };
    }
  }
  // Modal while remapping: every key (the pad's own menu keys included) is swallowed; a real Escape cancels.
  key(e) {
    if (!this.remap && !this.test) return false;
    if (e.isTrusted && e.code === 'Escape') { if (this.remap) this.cancel(); this.test = null; }
    return true;
  }
  reset() {
    const e = this.entry(); if (!e || !padRemap(e)) return;
    this.x.ask('Reset this pad\'s buttons?', () => { clearRemap(e); this.note = { text: 'Reset', at: performance.now() }; }, 1);
  }
}
