// Gamepads without Windows hardware (web/gamepad.js, gamepad-map.js, gamepad-menus.js): synthetic Gamepad API
// snapshots for the cases players hit on Windows — the pad in slot 1/2 with slot 0 empty or held by another device,
// DirectInput layouts, the hat D-pad (Firefox / RawInput axis 9, steps of 2/7), triggers on axes, two pads,
// hot-plug, Chromium snapshots vs Firefox live objects — plus the menu keys, rumble and the control hints.
import assert from 'node:assert/strict';

const store = new Map();
globalThis.localStorage = { getItem: (k) => (store.has(k) ? store.get(k) : null), setItem: (k, v) => store.set(k, String(v)), removeItem: (k) => store.delete(k) };

const G = await import('./gamepad.js');
const M = await import('./gamepad-map.js');
const { createPadMenus, menuControls } = await import('./gamepad-menus.js');
const { setPv } = await import('./pv-flags.js');
const { buildPad, GAMEPAD_BUTTONS, PAD_BUTTONS } = await import('./pad-input.js');
const { Rumble } = await import('./rumble.js');
const glyphs = await import('./input-glyphs.js');

// ---- synthetic devices ----------------------------------------------------------------------------------------------
// A device: its current raw state; snapshot() gives a Chromium-style fresh object every call.
function device({ id, mapping = '', buttons = 17, axes = 4, index = 0, rest = null, vibration = false }) {
  const d = { id, mapping, index, connected: true, timestamp: 0, b: Array(buttons).fill(0), a: rest ? [...rest] : Array(axes).fill(0), vibration };
  d.press = (i, v = 1) => { d.b[i] = v; d.timestamp++; return d; };
  d.release = (i) => { d.b[i] = 0; d.timestamp++; return d; };
  d.axis = (i, v) => { d.a[i] = v; d.timestamp++; return d; };
  d.snapshot = () => ({ id: d.id, index: d.index, mapping: d.mapping, connected: d.connected, timestamp: d.timestamp,
    buttons: d.b.map((v) => ({ pressed: v > 0.5 || v === 1, touched: v > 0, value: v })), axes: [...d.a],
    ...(d.vibration ? { vibrationActuator: d.vibration } : {}) });
  return d;
}
let slots = [], now = 1000;
const tick = (ms = 16) => { now += ms; };
G.setPadSource(() => slots.map((d) => (d ? d.snapshot() : null)), () => now);
const poll = () => { tick(); return G.pollPads(); };
const xinput = (index) => device({ id: 'Xbox 360 Controller (XInput STANDARD GAMEPAD)', mapping: 'standard', index });
const ds4 = (index) => device({ id: 'DUALSHOCK 4 Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)', mapping: 'standard', index });
// A generic DirectInput pad as Chromium / Firefox show it on Windows: axes indexed by HID usage (X Y Z Rx Ry Rz ...,
// hat = usage 0x39 -> axis 9), Rx/Ry triggers resting at -1, the hat neutral at 9/7.
const HAT_NEUTRAL = 9 / 7;
const dinput = (index, id = 'USB Gamepad (Vendor: 1234 Product: 5678)') => device({ id, index, buttons: 12, rest: [0.0039, -0.0039, 0.0039, -1, -1, -0.0039, 0, 0, 0, HAT_NEUTRAL] });
function reset(list) { slots = list; G.resetPads(); store.clear(); glyphs.resetInputDevice(); }
const channel = (pad, name) => buildPad(() => false, pad)[PAD_BUTTONS.indexOf(name)];

{ // ids: Chromium, Firefox (not zero padded), XInput
  assert.deepEqual(M.parsePadId('DUALSHOCK 4 Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)'), { vendor: '054c', product: '09cc', name: 'DUALSHOCK 4 Wireless Controller', xinput: false });
  assert.deepEqual(M.parsePadId('54c-9cc-Wireless Controller'), { vendor: '054c', product: '09cc', name: 'Wireless Controller', xinput: false });
  assert.deepEqual(M.parsePadId('Xbox 360 Controller (XInput STANDARD GAMEPAD)'), { vendor: null, product: null, name: 'Xbox 360 Controller', xinput: true });
  assert.equal(M.padKey({ id: '79-6-Generic   USB  Joystick  ' }), '0079:0006');
  assert.equal(M.padKey({ id: 'USB Gamepad (Vendor: 0079 Product: 0006)' }), '0079:0006', 'the same pad keys the same in Chrome and Firefox');
}
{ // hat: 8 directions in steps of 2/7, neutral outside -1..1 or 0
  const H = M.HAT;
  const want = [H.up, H.up | H.right, H.right, H.down | H.right, H.down, H.down | H.left, H.left, H.up | H.left];
  for (let n = 0; n < 8; n++) { assert.equal(M.hatBits(-1 + n * 2 / 7), want[n], 'position ' + n); assert.equal(M.hatBits(Math.fround(-1 + n * 2 / 7)), want[n]); }
  for (const v of [HAT_NEUTRAL, 3.2857, 0, NaN, -1.3]) assert.equal(M.hatBits(v), 0, 'neutral ' + v);
}

{ // slot 0 empty, the controller in slot 1 (the reported Windows failure: the game read slot 0 only)
  const pad = xinput(1); reset([null, pad]);
  pad.press(0); const std = poll();
  assert.ok(std, 'the pad in slot 1 is read'); assert.equal(std.index, 1);
  assert.ok(channel(std, 'Cross') > 0.99, 'Cross reaches the game');
  pad.release(0).axis(0, -1); assert.equal(buildPad(() => false, poll())[20] > 0.99, true, 'left stick left = channel 20');
}
{ // slot 0 held by another device (a headset / wheel driver, not gamepad shaped), the pad in slot 2
  const headset = device({ id: 'Headset (Vendor: 1b1c Product: 0a14)', buttons: 3, axes: 0 }), pad = ds4(2);
  reset([headset, null, pad]);
  assert.equal(poll().index, 2, 'before any input the standard pad wins over slot 0');
  pad.press(9); assert.ok(channel(poll(), 'Start') === 1);
}
{ // slot 0 is Steam Input's idle virtual pad (standard), the real pad in slot 1 is used: it becomes active
  const steam = xinput(0), pad = ds4(1); reset([steam, pad]);
  assert.equal(poll().index, 0, 'no input yet: lowest standard slot');
  pad.press(0); assert.equal(poll().index, 1, 'a press moves to the pad that was used');
  pad.release(0); poll(); assert.equal(poll().index, 1, 'and it stays there');
  steam.axis(1, 0.9); assert.equal(poll().index, 0, 'the other pad takes over when it is used');
  // Mirrors (Steam Input doubles a pad): both report the same press; the active one keeps it (no flapping)
  steam.axis(1, 0); poll(); pad.press(3); steam.press(3); assert.equal(poll().index, 0, 'mirrored press keeps the active pad');
}
{ // standard pads are copied unchanged: the PS2 channels are exactly those of the raw pad (no gameplay change)
  const pad = xinput(0); reset([pad]);
  pad.press(7, 0.63).press(4).press(13).axis(0, 0.52).axis(1, -0.97).axis(2, -0.4).axis(3, 0.2);
  const std = poll(), raw = pad.snapshot();
  assert.deepEqual([...buildPad(() => false, std)], [...buildPad(() => false, raw)]);
  for (let i = 0; i < 17; i++) assert.equal(std.buttons[i].value, raw.buttons[i].value);
  assert.deepEqual(std.axes, raw.axes);
}
{ // generic DirectInput pad (non-standard): hat D-pad on axis 9, Rx/Ry triggers resting at -1, right stick Z/Rz
  const pad = dinput(1); reset([null, pad]);
  pad.press(0); let std = poll(); assert.equal(std.layout, 'generic');
  assert.ok(channel(std, 'Cross') > 0.99, 'face buttons in the standard order');
  for (const name of ['L2', 'R2', 'DPadUp', 'DPadDown', 'DPadLeft', 'DPadRight']) assert.equal(channel(std, name), 0, name + ' at rest');
  pad.release(0).axis(9, -1); std = poll(); assert.ok(channel(std, 'DPadUp') > 0.99 && !channel(std, 'DPadLeft'), 'hat up');
  pad.axis(9, 1); std = poll(); assert.ok(channel(std, 'DPadUp') > 0.99 && channel(std, 'DPadLeft') > 0.99, 'hat up-left');
  pad.axis(9, -3 / 7); std = poll(); assert.ok(channel(std, 'DPadRight') > 0.99 && !channel(std, 'DPadUp'), 'hat right');
  pad.axis(9, HAT_NEUTRAL); std = poll(); assert.equal(channel(std, 'DPadRight'), 0, 'hat neutral');
  pad.axis(3, 1); std = poll(); assert.ok(channel(std, 'L2') > 0.99, 'L2 from the Rx axis');
  pad.axis(3, 0); std = poll(); assert.ok(Math.abs(std.buttons[6].value - 0.5) < 1e-6, 'half pulled');
  pad.axis(3, -1).axis(2, 1).axis(5, -1); std = poll(); assert.equal(std.axes[2], 1); assert.equal(std.axes[3], -1, 'right stick Y from Rz');
}
{ // known raw layouts (browsers / drivers that do not remap them): DualShock 4 in DirectInput, PS2 adapters, DualShock 3
  const raw = (id, buttons, axes) => ({ id, index: 0, mapping: '', connected: true, timestamp: 1, buttons: buttons.map((v) => ({ pressed: v > 0.5, value: v })), axes });
  const map = (r, seen = null) => { const cal = M.newCalibration(); if (seen) M.calibrate(cal, { axes: seen }); M.calibrate(cal, r); return M.mapPad(r, M.compileLayout(M.layoutFor(r)), cal); };
  const b = (n, on = []) => Array.from({ length: n }, (_, i) => (on.includes(i) ? 1 : 0));
  const ds4Rest = [0, 0, 0, -1, -1, 0, 0, 0, 0, 9 / 7];
  let std = map(raw('054c-09cc-Wireless Controller', b(14, [1]), ds4Rest));
  assert.equal(std.layout, 'dualshock4-dinput'); assert.ok(std.buttons[0].pressed, 'DS4 raw b1 = Cross'); assert.ok(!std.buttons[2].pressed);
  std = map(raw('054c-09cc-Wireless Controller', b(14, [0, 6]), [0, 0, 0, 0.5, -1, 0.7, 0, 0, 0, 1 / 7]), ds4Rest);
  assert.ok(std.buttons[2].pressed, 'b0 = Square'); assert.ok(Math.abs(std.buttons[6].value - 0.75) < 1e-6, 'analog L2 from Rx, not the digital b6');
  assert.ok(std.buttons[13].pressed && !std.buttons[12].pressed, 'hat down'); assert.equal(std.axes[3], 0.7, 'right stick Y = Rz');
  std = map(raw('Twin USB Joystick (Vendor: 0925 Product: 0005)', b(12, [2, 8]), [0, 0, 0, 0, 0, 0, 0, 0, 0, 9 / 7]));
  assert.equal(std.layout, 'smartjoy-ps2'); assert.ok(std.buttons[0].pressed && std.buttons[9].pressed && !std.buttons[8].pressed, 'SmartJoy: b2 Cross, b8 Start');
  std = map(raw('054c-268-PLAYSTATION(R)3 Controller', b(19, [14, 4, 5]), [0, 0, 0, 0, 0, 0]));
  assert.ok(std.buttons[0].pressed && std.buttons[12].pressed && std.buttons[15].pressed, 'DualShock 3: Cross, D-pad up/right buttons');
  std = map(raw('0079-0011-USB Gamepad', b(10, [2]), [-1, 1]));
  assert.ok(std.buttons[0].pressed && std.buttons[14].pressed && std.buttons[13].pressed, 'SNES-style: D-pad on axes');
  std = map({ ...raw('054c-09cc-Wireless Controller', b(17, [0]), [0, 0, 0, 0]), mapping: 'standard' });
  assert.equal(std.layout, 'standard', 'a pad the browser maps itself is left alone'); assert.ok(std.buttons[0].pressed);
}
{ // Firefox-style pad with the hat on axis 9 and a live (mutated in place) Gamepad object
  const live = { id: '79-6-Generic   USB  Joystick  ', index: 0, mapping: '', connected: true, timestamp: 0,
    buttons: Array.from({ length: 12 }, () => ({ pressed: false, value: 0 })), axes: [0, 0, 0, 0, 0, 0, 0, 0, 0, HAT_NEUTRAL] };
  reset([]); G.setPadSource(() => [live], () => now);
  live.buttons[2].pressed = true; live.buttons[2].value = 1;
  let std = poll(); assert.ok(std, 'seen after the first press');
  for (let n = 0; n < 8; n++) {
    live.axes[9] = -1 + n * 2 / 7; std = poll();
    const dirs = ['DPadUp', 'DPadDown', 'DPadLeft', 'DPadRight'].filter((d) => channel(std, d) > 0.99);
    assert.deepEqual(dirs, [['DPadUp'], ['DPadUp', 'DPadRight'], ['DPadRight'], ['DPadDown', 'DPadRight'], ['DPadDown'], ['DPadDown', 'DPadLeft'], ['DPadLeft'], ['DPadUp', 'DPadLeft']][n], 'hat position ' + n);
  }
  live.axes[9] = 3.2857; assert.equal(['DPadUp', 'DPadDown', 'DPadLeft', 'DPadRight'].map((d) => channel(poll(), d)).join(), '0,0,0,0');
  // live object: a press / release is still seen as an edge (nothing keeps the raw object's buttons)
  live.buttons[2].pressed = false; live.buttons[2].value = 0; poll();
  let used = 0; const off = G.onPads((t) => { if (t === 'use') used++; });
  live.buttons[2].pressed = true; live.buttons[2].value = 1; poll(); assert.equal(used, 1, 'edge on a live object');
  poll(); assert.equal(used, 1, 'held is not a new use'); off();
  G.setPadSource(() => slots.map((d) => (d ? d.snapshot() : null)), () => now);
}
{ // triggers on axes: rest -1 -> 0, full -> 1; an axis that still reads 0 before its first report is not half pressed
  const pad = device({ id: 'USB Gamepad (Vendor: 1234 Product: 5678)', buttons: 12, rest: [0, 0, 0, 0, 0, 0, 0, 0, 0, HAT_NEUTRAL] });
  reset([pad]); pad.press(0); let std = poll();
  assert.equal(std.buttons[6].value, 0, 'unreported trigger axis (0) is released');
  pad.axis(3, -1); std = poll(); assert.equal(std.buttons[6].value, 0, 'rest at -1');
  pad.axis(3, 1); std = poll(); assert.equal(std.buttons[6].value, 1); assert.ok(std.buttons[6].pressed);
}
{ // Chromium snapshots: every poll re-reads getGamepads (a kept object would never change)
  const pad = xinput(0); reset([pad]);
  pad.press(0); const a = poll(); assert.equal(a.buttons[0].pressed, true);
  pad.release(0); const b = poll(); assert.equal(b.buttons[0].pressed, false, 'the release is seen');
  let calls = 0; G.setPadSource(() => { calls++; return [pad.snapshot()]; }, () => now);
  tick(); G.pollPads(); G.pollPads(); G.activePad(); assert.equal(calls, 1, 'one read per poll interval'); tick(); G.pollPads(); assert.equal(calls, 2);
  G.setPadSource(() => slots.map((d) => (d ? d.snapshot() : null)), () => now);
}
{ // hot-plug: unplug the active pad, plug one in at another slot, a different pad reusing a slot
  const a = xinput(0), b = ds4(1); reset([a, b]);
  b.press(0); assert.equal(poll().index, 1);
  const events = []; const off = G.onPads((t, e) => events.push(t + ':' + e?.slot));
  slots = [a, null]; assert.equal(poll().index, 0, 'unplugged: falls back to the other pad'); assert.ok(events.includes('disconnect:1'));
  slots = [null, null]; assert.equal(poll(), null, 'nothing connected');
  const c = dinput(2); slots = [null, null, c]; c.press(0); assert.equal(poll().index, 2, 'plugged in at slot 2'); assert.ok(events.includes('connect:2'));
  const d = xinput(2); slots = [null, null, d]; assert.equal(poll().layout, 'standard', 'a new pad in the same slot gets its own layout'); off();
}
{ // two pads: the most recently used one drives the game; buttons held on the other do not leak in
  const a = xinput(0), b = dinput(1); reset([a, b]);
  a.press(5); assert.equal(poll().index, 0);
  b.press(0); const std = poll(); assert.equal(std.index, 1);
  assert.equal(channel(std, 'R1'), 0, 'R1 held on the other pad is not read'); assert.ok(channel(std, 'Cross') > 0.99);
}
{ // remap per pad (Options > Controller): saved under vendor:product, applied at once, used by every reader
  const pad = dinput(0, 'USB Gamepad (Vendor: 0810 Product: 0001)'); reset([pad]);
  pad.press(2); poll(); const e = G.activeEntry();
  assert.equal(M.padKey(pad.snapshot()), '0810:0001');
  pad.release(2); poll();
  assert.deepEqual([...G.rawControls(e)], []);
  pad.press(2); poll(); assert.deepEqual([...G.rawControls(e)], ['b2']);
  pad.release(2).axis(3, 1); poll(); assert.deepEqual([...G.rawControls(e)], ['a3+'], 'a trigger axis from its rest');
  pad.axis(3, -1).axis(9, 5 / 7); poll(); assert.deepEqual([...G.rawControls(e)], ['h9:left']);
  pad.axis(9, HAT_NEUTRAL); poll();
  G.saveRemap(e, { buttons: { 0: 'b2', 2: 'b0' } });
  assert.ok(JSON.parse(store.get('ssx3.padmap'))['0810:0001'], 'saved');
  pad.press(2); let std = poll(); assert.ok(channel(std, 'Cross') > 0.99 && !channel(std, 'Square'), 'raw 2 is Cross now');
  G.resetPads(); std = poll(); assert.ok(channel(std, 'Cross') > 0.99, 'loaded back from storage');
  G.clearRemap(G.activeEntry()); std = poll(); assert.ok(channel(std, 'Square') > 0.99, 'cleared');
}
{ // menus: the touch deck's keys, new presses only, Start left to the race while running, repeats (the port's pad model)
  setPv('ps2MenuInput', false);
  const sent = []; let menu = true, running = false;
  const m = createPadMenus({ menu: () => menu, running: () => running, send: (t, c) => sent.push(t + ' ' + c) });
  const pad = M.newStandardPad(); const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  set(0, 1); m.step(pad, 0); assert.deepEqual(sent, ['keydown Space']);
  set(0, 0); m.step(pad, 16); assert.deepEqual(sent.splice(0), ['keydown Space', 'keyup Space']);
  set(12, 1); m.step(pad, 32); m.step(pad, 300); assert.deepEqual(sent.splice(0), ['keydown ArrowUp'], 'no repeat before 400 ms');
  m.step(pad, 440); assert.deepEqual(sent.splice(0), ['keyup ArrowUp', 'keydown ArrowUp'], 'repeat'); set(12, 0); m.step(pad, 460); sent.length = 0;
  pad.axes[0] = 0.8; m.step(pad, 500); assert.deepEqual(sent.splice(0), ['keydown ArrowRight'], 'left stick = arrows'); pad.axes[0] = 0; m.step(pad, 516); sent.length = 0;
  for (const [i, code] of [[1, 'Backspace'], [2, 'ShiftLeft'], [3, 'Escape'], [4, 'KeyQ'], [5, 'KeyE'], [6, 'KeyZ'], [7, 'KeyX'], [9, 'Enter']]) { set(i, 1); m.step(pad, 600); set(i, 0); m.step(pad, 616); assert.equal(sent.splice(0)[0], 'keydown ' + code); }
  // in a race Start is the pad's Start (main.js frame pauses); opening the pause menu with it held does not press Enter
  menu = false; running = true; set(9, 1); m.step(pad, 700); assert.deepEqual(sent, []);
  menu = true; m.step(pad, 716); assert.deepEqual(sent, [], 'held into the pause menu: nothing');
  set(9, 0); m.step(pad, 732); set(9, 1); m.step(pad, 748); assert.deepEqual(sent, [], 'Start while running is not Enter');
  set(9, 0); set(0, 1); m.step(pad, 764); assert.deepEqual(sent.splice(0), ['keydown Space']);
  menu = false; m.step(pad, 780); assert.deepEqual(sent.splice(0), ['keyup Space'], 'keys are released when the race resumes');
  assert.deepEqual([...menuControls(null)], []);
  setPv('ps2MenuInput', null);
}
{ // pv ps2MenuInput: the same through the PS2 pad history 0x321298, stepped in 60 Hz updates (an edge is seen once the channel's last
  // edge is 3 updates old; directions repeat on the press, 24 updates later, then every 12)
  setPv('ps2MenuInput', true);
  const sent = []; let menu = true, running = false, t = 0;
  const m = createPadMenus({ menu: () => menu, running: () => running, send: (ty, c) => sent.push(ty + ' ' + c) });
  const pad = M.newStandardPad(); const set = (i, v) => { pad.buttons[i].value = v; pad.buttons[i].pressed = v > 0.5; };
  const f = (n = 1) => { for (let k = 0; k < n; k++) { t += 1000 / 60; m.step(pad, t); } };
  f(4); assert.deepEqual(sent, [], 'the channels start with an edge window');
  set(0, 1); f(); assert.deepEqual(sent.splice(0), ['keydown Space'], 'Cross on its update');
  set(0, 0); f(3); assert.deepEqual(sent, [], 'a release inside the edge window waits'); f(); assert.deepEqual(sent.splice(0), ['keyup Space']);
  f(3); set(12, 1); f(); assert.deepEqual(sent.splice(0), ['keydown ArrowUp', 'keyup ArrowUp'], 'Up moves on the press');
  f(23); assert.deepEqual(sent, [], 'no repeat before 24 updates'); f(); assert.deepEqual(sent.splice(0), ['keydown ArrowUp', 'keyup ArrowUp'], 'repeat at 24');
  f(11); assert.deepEqual(sent, []); f(); assert.equal(sent.splice(0).length, 2, 'then every 12');
  set(12, 0); f(4); sent.length = 0;
  pad.axes[0] = 0.8; f(); assert.deepEqual(sent.splice(0), ['keydown ArrowRight', 'keyup ArrowRight'], 'left stick = arrows'); pad.axes[0] = 0; f(4); sent.length = 0;
  for (const [i, code] of [[1, 'Backspace'], [2, 'ShiftLeft'], [3, 'Escape'], [4, 'KeyQ'], [5, 'KeyE'], [6, 'KeyZ'], [7, 'KeyX'], [9, 'Enter']]) { set(i, 1); f(4); set(i, 0); f(4); assert.equal(sent.splice(0)[0], 'keydown ' + code); }
  menu = false; running = true; set(9, 1); f(4); assert.deepEqual(sent, [], 'Start in the race: main.js pauses');
  menu = true; f(4); assert.deepEqual(sent, [], 'held into the pause menu: nothing');
  set(9, 0); f(4); set(9, 1); f(4); assert.deepEqual(sent, [], 'Start while running is not Enter');
  set(9, 0); f(4); set(0, 1); f(); assert.deepEqual(sent.splice(0), ['keydown Space']);
  menu = false; f(); assert.deepEqual(sent.splice(0), ['keyup Space'], 'keys are released when the race resumes');
  setPv('ps2MenuInput', null);
}
{ // rumble drives the active pad, not slot 0
  const calls = [], act = { playEffect: (t, p) => { calls.push(p); return Promise.resolve(); }, reset: () => Promise.resolve() };
  const pad = device({ id: 'Xbox 360 Controller (XInput STANDARD GAMEPAD)', mapping: 'standard', index: 1, vibration: act });
  reset([null, pad]); pad.press(0); poll();
  const r = new Rumble(); r.drive({ strong: 0.5, weak: 1 }); assert.equal(calls.length, 1, 'dual-rumble on slot 1');
  const pulses = []; const ff = { hapticActuators: [{ pulse: (v, ms) => { pulses.push([v, ms]); return Promise.resolve(true); } }] };
  const rf = new Rumble(() => ff); pulses.length = 0; rf.drive({ strong: 0.8, weak: 0 }); assert.deepEqual(pulses, [[0.8, 100]], 'Firefox pulse()');
  rf.stop(); assert.deepEqual(pulses.at(-1), [0, 0], 'stopped');
}
{ // control hints: a press on a pad in slot 2 switches them to the pad
  const pad = xinput(2); reset([null, null, pad]);
  glyphs.noteInput('keyboard'); poll(); assert.equal(glyphs.inputDevice(), 'keyboard', 'connected but unused');
  pad.press(0); poll(); assert.equal(glyphs.inputDevice(), 'gamepad', 'the use event reaches the hints');
}
{ // Options > Controller Settings > Gamepad (web/fe-gamepad.js): the added row, the screen, the remap state machine
  const F = await import('./fe-gamepad.js'), O = await import('./fe-options.js');
  const fs = await import('node:fs');
  const data = JSON.parse(fs.readFileSync(new URL('./public/assets/UI/character-select.json', import.meta.url)));
  if (data.screens?.['22control']) {
    const model = O.rowModel(F.controlWithPad(data.screens['22control']));
    assert.deepEqual(model.texts, ['Vibration 1P', 'Vibration 2P', 'Controller 1P', 'Controller 2P', 'Gamepad'], 'Controller Settings gets a Gamepad row');
    assert.deepEqual(model.frames, [35, 40, 45, 50, F.CONTROL_PAD.frame], 'with its own focus state');
    const pad = O.rowModel(O.displayScreen(data.screens['19game_opt'], F.PAD_ROWS, F.PAD_TITLE));
    assert.deepEqual(pad.texts, F.PAD_ROWS.map((r) => r.label));
  }
  // remap: a held button at the start does not count; each step takes a new press, waits for the release, skips after 5 s
  const entry = { rest: [0, 0, 0, -1, -1, 0] }, set = (...c) => new Set(c);
  const r = F.createRemap(entry, 0);
  F.remapStep(r, set('b0'), 0);                         // Cross still held from choosing the row
  F.remapStep(r, set('b0'), 100); assert.equal(r.step, 0);
  F.remapStep(r, set(), 200); F.remapStep(r, set('b2'), 300); assert.equal(r.map.buttons[0], 'b2', 'Cross = raw 2'); assert.equal(r.step, 1);
  F.remapStep(r, set('b2'), 400); F.remapStep(r, set('b1', 'b2'), 450); assert.equal(r.step, 1, 'waits for the release first');
  F.remapStep(r, set(), 500); F.remapStep(r, set('b1'), 600); assert.equal(r.map.buttons[1], 'b1');
  F.remapStep(r, set(), 700); F.remapStep(r, set(), 700 + F.REMAP_SKIP_MS); assert.equal(r.step, 3, 'Square skipped after 5 s'); assert.equal(r.map.buttons[2], undefined);
  for (const [c, want] of [['b3', 'b3'], ['b4', 'b4'], ['b5', 'b5'], ['a3+', 't3'], ['a4+', 't4'], ['b8', 'b8'], ['b9', 'b9'], ['b10', 'b10'], ['b11', 'b11'], ['h9:up', 'h9:up'], ['h9:down', 'h9:down'], ['h9:left', 'h9:left'], ['h9:right', 'h9:right']]) {
    const k = r.step; F.remapStep(r, set(c), 10000 + k * 100); F.remapStep(r, set(), 10050 + k * 100);
    assert.equal(r.map.buttons[F.REMAP_STEPS[k].button], want, F.REMAP_STEPS[k].name);
  }
  F.remapStep(r, set('b0'), 11700); assert.equal(r.step, 16, 'a button does not answer a stick step');
  F.remapStep(r, set('a0+'), 11800); F.remapStep(r, set(), 11900); assert.equal(r.map.axes[0], 'a0');
  F.remapStep(r, set('a1-'), 12000); F.remapStep(r, set(), 12100); assert.equal(r.map.axes[1], '-a1', 'pushed down reads negative: inverted');
  F.remapStep(r, set(), 12100 + F.REMAP_SKIP_MS); F.remapStep(r, set(), 12100 + 2 * F.REMAP_SKIP_MS); assert.ok(r.done);
  const map = F.finishRemap(r, M.genericLayout({ buttons: Array(12), axes: Array(10) }));
  assert.equal(map.buttons[2], '', 'skipped Square (generic b2) loses the raw button Cross took');
  const r2 = F.createRemap(entry, 0); r2.map.buttons[0] = 'b5'; r2.done = true;
  const m2 = F.finishRemap(r2, M.genericLayout({ buttons: Array(12), axes: Array(10) }));
  assert.equal(m2.buttons[2], undefined, 'a skipped control whose button is free keeps its layout'); assert.equal(m2.buttons[5], '', 'R1 gives up b5');
  assert.equal(F.layoutLabel('standard'), 'Standard'); assert.equal(F.layoutLabel('generic+remap'), 'Custom'); assert.equal(F.layoutLabel('dualshock4-dinput'), 'DualShock 4');
  assert.equal(F.padName({ id: 'Xbox 360 Controller (XInput STANDARD GAMEPAD)' }, 12), 'Xbox 360'); assert.equal(F.padName({ id: 'xinput' }), 'Xbox pad');
  assert.equal(F.padName({ id: '054c-0ce6-Wireless Controller' }), 'PlayStation pad'); assert.equal(F.padName(null), 'None');
  const p = M.newStandardPad(); p.buttons[0].pressed = true; p.buttons[12].pressed = true; assert.equal(F.heldText(p), 'Cross Up');
}
console.log('gamepad tests passed');
