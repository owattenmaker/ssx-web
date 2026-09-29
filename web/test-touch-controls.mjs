// Touch deck (touch-controls.js) -> PS2 pad channels: every touch control must give buildPad() exactly the channels the
// keyboard (or a DualShock) gives for the same PS2 control, alone and in multi-finger combinations; plus the pure
// layout/hit-testing helpers and the quality module's buffer sizes.
import { buildPad, createKeyboardContext, stickChannels, axisByte, GAMEPAD_BUTTONS } from './pad-input.js';
import { TouchPad, PAD_KEYS, MENU_KEYS, stickResponse, STICK_DEAD, computeLayout, faceHits, dpadHits, CRAMPED, TOUCH_DEFAULTS, loadTouchSettings } from './touch-controls.js';
import { promptMessage, wrapLines } from './phone-prompts.js';
import { bufferSize, resolveQuality, detectDevice, detectTier, frameGate } from './quality.js';
const fail = (m) => { console.error('FAIL', m); process.exit(1); };
const arr = (v) => Array.from(v);
const same = (a, b, m) => { if (a.length !== b.length || arr(a).some((x, k) => !Object.is(x, b[k]))) fail(`${m}\n touch ${arr(a)}\n other ${arr(b)}`); };
const touchPad = (t, kb = null, real = null) => buildPad((c) => t.held(c), t.pad(real), kb);
const keyPad = (codes, kb = null, real = null) => buildPad((c) => codes.includes(c), real, kb);
const gamepad = (axes = [0, 0, 0, 0], pressed = []) => ({ axes, buttons: Array.from({ length: 17 }, (_, i) => ({ pressed: pressed.includes(i), value: pressed.includes(i) ? 1 : 0 })) });
const PS2 = ['cross', 'square', 'circle', 'triangle', 'l1', 'l2', 'r1', 'r2', 'up', 'down', 'left', 'right', 'select'];
const channel = { select: 0, right: 4, left: 5, up: 6, down: 7, triangle: 8, circle: 9, cross: 10, square: 11, l1: 12, r1: 13, l2: 14, r2: 15 };

for (const mode of ['Classic', 'Simple']) for (const air of [false, true]) {
  const ctx = () => { const k = createKeyboardContext(mode); k.air = air; return k; };
  // 1. every button alone == its keyboard key, and lands on the PS2 channel of that button
  for (const c of PS2) {
    const t = new TouchPad(); t.press(c);
    const v = touchPad(t, ctx());
    same(v, keyPad([PAD_KEYS[c]], ctx()), `${mode}/${air ? 'air' : 'ground'} ${c} vs key ${PAD_KEYS[c]}`);
    if (v[channel[c]] !== 1 || arr(v).filter((x) => x).length !== 1) fail(`${c} should be channel ${channel[c]} only: ${arr(v)}`);
  }
  // 2. Start = DualShock Start (gamepad button 9 -> channel 1); no keyboard key exists for it
  { const t = new TouchPad(); t.press('start'); const v = touchPad(t, ctx()); same(v, keyPad([], ctx(), gamepad([0, 0, 0, 0], [GAMEPAD_BUTTONS[1]])), 'start'); if (v[1] !== 1) fail('start channel'); }
  // 3. stick: full deflection == the arrow keys / WASD in Classic (== a DualShock pushed fully), partial == the same
  //    value on a real stick
  for (const [x, y, key] of [[-1, 0, 'ArrowLeft'], [1, 0, 'ArrowRight'], [0, -1, 'ArrowUp'], [0, 1, 'ArrowDown']]) {
    const t = new TouchPad(); t.setStick(x, y);
    same(touchPad(t, ctx()), keyPad([], ctx(), gamepad([x, y, 0, 0])), `stick ${key} vs full DualShock stick`);
    if (mode === 'Classic') same(touchPad(t, ctx()), keyPad([key], ctx()), `stick ${key} vs keyboard ${key}`);
  }
  for (const v of [0.2, 0.35, 0.5, 0.75, 0.95, -0.3, -0.6]) {
    const t = new TouchPad(); t.setStick(v, 0);
    same(touchPad(t, ctx()), keyPad([], ctx(), gamepad([stickResponse(v), 0, 0, 0])), `partial stick ${v}`);
  }
  // 4. multi-touch: stick + ✕ + R1 (normal play), grab chords with every shoulder combination, d-pad + face, ...
  const combos = [
    [['cross', 'r1'], [-1, 0], ['Space', 'KeyE', 'ArrowLeft']],
    [['l1', 'r2', 'cross'], [0, -1], ['KeyQ', 'KeyX', 'Space', 'ArrowUp']],
    [['l1', 'l2', 'r1', 'r2'], null, ['KeyQ', 'KeyZ', 'KeyE', 'KeyX']],
    [['square', 'cross', 'left', 'up'], null, ['ShiftLeft', 'Space', 'KeyJ', 'KeyI']],
    [['circle', 'l2', 'right'], [1, 0], ['KeyC', 'KeyZ', 'KeyL', 'ArrowRight']],
  ];
  for (const [controls, stick, keys] of combos) {
    const t = new TouchPad(); for (const c of controls) t.press(c); if (stick) t.setStick(...stick);
    const v = touchPad(t, ctx());
    if (mode === 'Classic' || !stick) same(v, keyPad(keys, ctx()), `combo ${controls} ${stick || ''}`);
    else same(v, keyPad(keys.filter((k) => !k.startsWith('Arrow')), ctx(), gamepad([stick[0], stick[1], 0, 0])), `combo ${controls} stick ${stick} vs DualShock`);
  }
  // every one of the 15 shoulder chords (Trick1..15 = L1/L2/R1/R2 held-flag masks)
  const sh = ['l1', 'l2', 'r1', 'r2'];
  for (let m = 1; m < 16; m++) { const t = new TouchPad(), keys = []; sh.forEach((c, i) => { if (m & (1 << i)) { t.press(c); keys.push(PAD_KEYS[c]); } }); same(touchPad(t, ctx()), keyPad(keys, ctx()), `shoulder chord ${m}`); }
}
// 5. fingers: two fingers on one button, one lifts -> still held; release of an unheld control is a no-op
{ const t = new TouchPad(); t.press('cross'); t.press('cross'); t.release('cross'); if (!t.held('Space')) fail('two fingers: one lift released ✕'); t.release('cross'); if (t.held('Space')) fail('✕ stuck'); if (t.release('cross')) fail('double release'); }
// 6. right stick = BoardPress / BoardPivot channels (16..19) exactly like a DualShock right stick / TFGH keys
{ const t = new TouchPad(); t.setRStick(0, -1); same(touchPad(t), keyPad(['KeyT']), 'right stick up = T'); t.setRStick(1, 0); same(touchPad(t), keyPad(['KeyH']), 'right stick right = H'); t.releaseRStick(); same(touchPad(t), keyPad([]), 'right stick released'); }
// 7. a real gamepad still works while touch is idle, and the touch stick overrides only the left stick
{ const t = new TouchPad(), g = gamepad([0, 0, 0.8, 0], [0]); same(touchPad(t, null, g), keyPad([], null, g), 'idle touch passes the gamepad'); t.setStick(-1, 0); const v = touchPad(t, null, g); same(v, keyPad([], null, gamepad([-1, 0, 0.8, 0], [0])), 'touch stick + gamepad right stick/buttons'); }
// 8. clear() releases everything
{ const t = new TouchPad(); t.press('l1'); t.setStick(1, 1); t.press('start'); t.clear(); same(touchPad(t), keyPad([]), 'clear'); }
// 9. stick response: dead zone, PS2 dead zone skipped (just past the touch dead zone already leaves 79..176), full at the rim
if (stickResponse(STICK_DEAD * 0.9) !== 0) fail('touch dead zone');
{ const [n, p] = stickChannels(axisByte(stickResponse(STICK_DEAD + 0.02))); if (!(p > 0) || n) fail('just past the touch dead zone must steer: ' + [n, p]); }
if (stickResponse(1) !== 1 || stickResponse(-1.4) !== -1) fail('stick rim');
for (let v = 0; v < 1; v += 0.01) if (stickResponse(v + 0.01) < stickResponse(v)) fail('stick response not monotonic');
// 10. menu keys: the front end's keys (ui.js Enter/Space choose, Escape back; fe-screens Shift/Backspace/Q/E)
same([MENU_KEYS.cross, MENU_KEYS.triangle, MENU_KEYS.square, MENU_KEYS.circle, MENU_KEYS.up, MENU_KEYS.start], ['Space', 'Escape', 'ShiftLeft', 'Backspace', 'ArrowUp', 'Enter'], 'menu keys');
// 11. hit tests: face buttons (one, two when rolled between them, nearest when off the caps) and 8-way d-pad
{
  const face = { cx: 100, cy: 100, B: 60, spread: 61 };
  same(faceHits(face, 100, 161), ['cross'], 'face ✕'); same(faceHits(face, 161, 100), ['circle'], 'face ○'); same(faceHits(face, 100, 39), ['triangle'], 'face △'); same(faceHits(face, 39, 100), ['square'], 'face □');
  same(faceHits(face, 70, 130).sort(), ['cross', 'square'], 'face roll ✕+□');
  // chord zones between neighbours: the whole gap between ✕ and □ fires both, a finger on a cap stays single,
  // opposite buttons never chord, and sliding from ✕ toward □ reaches the chord before leaving ✕'s cap
  for (const [x, y] of [[69.5, 130.5], [60, 125], [75, 140], [65, 138], [80, 120]]) same(faceHits(face, x, y).sort(), ['cross', 'square'], `✕+□ gap at ${x},${y}`);
  same(faceHits(face, 131, 131).sort(), ['circle', 'cross'], '✕+○ gap'); same(faceHits(face, 69, 69).sort(), ['square', 'triangle'], '□+△ gap'); same(faceHits(face, 131, 69).sort(), ['circle', 'triangle'], '△+○ gap');
  same(faceHits(face, 100, 150), ['cross'], '✕ cap edge stays single'); same(faceHits(face, 50, 100), ['square'], '□ cap edge stays single');
  same(faceHits(face, 100, 100).length, 1, 'the middle of the diamond presses one button');
  let slid = null; for (let t = 0; t <= 1.0001; t += 0.05) { const h = faceHits(face, 100 - 61 * t, 161 - 61 * t).sort().join('+'); if (h === 'cross+square') { slid = t; break; } if (h !== 'cross') break; }
  same(slid !== null && slid < 0.6, true, `sliding ✕ -> □ reaches the chord without dropping ✕ (at ${slid})`);
  const d = { cx: 0, cy: 0, size: 100 };
  same(dpadHits(d, 40, 0), ['right'], 'dpad right'); same(dpadHits(d, 0, -40), ['up'], 'dpad up'); same(dpadHits(d, 30, 30).sort(), ['down', 'right'], 'dpad diagonal'); same(dpadHits(d, 3, 2), [], 'dpad centre');
}
// 12. layouts: the frame keeps its aspect, controls stay inside the viewport and off the frame, both layouts, both orientations
const inside = (r, W, H) => r.x >= -0.5 && r.y >= -0.5 && r.x + r.w <= W + 0.5 && r.y + r.h <= H + 0.5;
const overlaps = (a, b) => a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
for (const [W, H, ins] of [[390, 844, { t: 47, r: 0, b: 34, l: 0 }], [375, 667, { t: 20, r: 0, b: 0, l: 0 }], [430, 932, { t: 59, r: 0, b: 34, l: 0 }], [844, 390, { t: 0, r: 47, b: 21, l: 47 }], [820, 1180, { t: 24, r: 0, b: 20, l: 0 }], [1180, 820, { t: 24, r: 0, b: 20, l: 0 }]])
  for (const layout of ['split', 'below']) for (const aspect of [0.75, 0.5625]) {
    const L = computeLayout({ W, H, ins, aspect, layout }), f = L.frame, tag = `${W}x${H} ${layout} ${aspect}`;
    if (Math.abs(f.h / f.w - aspect) > 1e-6) fail(`${tag}: frame aspect`);
    if (!inside(f, W, H) || f.y < ins.t - 0.5) fail(`${tag}: frame outside`);
    const rects = { l1: L.l1, l2: L.l2, r1: L.r1, r2: L.r2, select: L.select, start: L.start, menu: L.menu, full: L.full,
      dpad: { x: L.dpad.cx - L.dpad.size / 2, y: L.dpad.cy - L.dpad.size / 2, w: L.dpad.size, h: L.dpad.size },
      stick: { x: L.stick.cx - L.stick.size / 2, y: L.stick.cy - L.stick.size / 2, w: L.stick.size, h: L.stick.size },
      face: (() => { const e = 2 * L.face.spread + L.face.B; return { x: L.face.cx - e / 2, y: L.face.cy - e / 2, w: e, h: e }; })() };
    if (L.rstick) rects.rstick = { x: L.rstick.cx - L.rstick.size / 2, y: L.rstick.cy - L.rstick.size / 2, w: L.rstick.size, h: L.rstick.size };
    for (const [k, r] of Object.entries(rects)) {
      if (!inside(r, W, H)) fail(`${tag}: ${k} outside the viewport ${JSON.stringify(r)}`);
      if (overlaps(r, f)) fail(`${tag}: ${k} covers the frame`);
      if (r.y + r.h > H - ins.b + 0.5 || r.x < ins.l - 0.5 || r.x + r.w > W - ins.r + 0.5) fail(`${tag}: ${k} in the safe-area inset`);
    }
    const names = Object.keys(rects);
    for (let i = 0; i < names.length; i++) for (let j = i + 1; j < names.length; j++) if (overlaps(rects[names[i]], rects[names[j]])) fail(`${tag}: ${names[i]} overlaps ${names[j]}`);
    if (Math.min(L.face.B, L.l1.h) < 36) fail(`${tag}: buttons too small for a thumb`);
  }
// 13. quality: render-scale buffers and tiers
same(Object.values(bufferSize('native', 390, 292, 1, 3)), [1, 640, 448, true], 'native 4:3');
same(Object.values(bufferSize('native', 390, 219, 0.75, 3)), [1, 640, 336, true], 'native 16:9 band');
same(Object.values(bufferSize('native512', 390, 292, 1, 3)), [1, 512, 448, true], 'native512');
same(Object.values(bufferSize('css', 390, 292, 1, 3)), [1, 390, 292, false], 'css');
same(Object.values(bufferSize('full', 1024, 768, 1, 2)), [896 / 768, 1024, 768, false], 'full (desktop default): capped at 896 lines');
same(Object.values(bufferSize('full', 400, 300, 1, 2)), [1.5, 400, 300, false], 'full below the cap: 1.5x');
same(Object.values(bufferSize('css', 1920, 1440, 1, 2)), [896 / 1440, 1920, 1440, false], 'css capped at 896 lines');
same(Object.values(bufferSize('full', 1920, 1080, 0.75, 2)), [672 / 1080, 1920, 1080, false], 'full 16:9 band: 0.75 x 896');
{
  const phone = detectDevice({ navigator: { userAgent: 'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X)', maxTouchPoints: 5, hardwareConcurrency: 6 }, matchMedia: (q) => ({ matches: q === '(pointer:coarse)' }), screen: { width: 390, height: 844 }, devicePixelRatio: 3 });
  const desk = detectDevice({ navigator: { userAgent: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)', maxTouchPoints: 0, hardwareConcurrency: 12 }, matchMedia: () => ({ matches: false }), screen: { width: 1728, height: 1117 }, devicePixelRatio: 2 });
  if (detectTier(phone) !== 'low' || detectTier(desk) !== 'high') fail('tiers ' + detectTier(phone) + ' ' + detectTier(desk));
  const q = resolveQuality(desk, {}, new URLSearchParams()); if (q.renderScale !== 'full' || !q.antialias || q.fps !== 60) fail('desktop defaults must be unchanged: ' + JSON.stringify(q));
  const p = resolveQuality(phone, {}, new URLSearchParams()); if (p.renderScale !== 'native' || p.antialias || p.fps !== 'auto') fail('phone defaults ' + JSON.stringify(p));
  const o = resolveQuality(phone, { renderScale: 'css' }, new URLSearchParams('quality=high')); if (o.tier !== 'high' || o.renderScale !== 'full') fail('?quality= override ' + JSON.stringify(o));
  const r = resolveQuality(phone, {}, new URLSearchParams('renderScale=native512&fps=30')); if (r.renderScale !== 'native512' || r.fps !== 30) fail('url overrides ' + JSON.stringify(r));
}
// 14. 30 fps presentation gate: draws every other 60 Hz rAF, never two in a row
{ const g = frameGate(30); let drawn = 0, prev = false; for (let i = 0; i < 120; i++) { const d = g(i * 1000 / 60); if (d && prev) fail('30 fps drew consecutive frames'); prev = d; drawn += d; } if (drawn !== 60) fail('30 fps drew ' + drawn); const g60 = frameGate(60); for (let i = 0; i < 10; i++) if (!g60(i * 16.7)) fail('60 fps skipped');
  const ga = frameGate('auto'); if (ga.rate !== 60) fail('auto starts at 60'); for (let i = 0; i < 45; i++) ga.done(18); if (ga.rate !== 30) fail('auto should drop to 30 on 18 ms frames'); for (let i = 0; i < 45; i++) ga.done(10); if (ga.rate !== 30) fail('auto hysteresis'); for (let i = 0; i < 45; i++) ga.done(5); if (ga.rate !== 60) fail('auto back to 60'); }
// 15. the turn-your-device prompt (web/phone-prompts.js): phones in either orientation fit; a view too short for the
//     side controls (or a tiny frame) is cramped; the prompt messages; settings come back with defaults
for (const [W, H] of [[390, 844], [844, 340], [667, 300], [375, 667], [1024, 768], [320, 480]]) if (computeLayout({ W, H }).cramped) fail(`${W}x${H} should fit`);
if (!computeLayout({ W: 740, H: 240 }).cramped || computeLayout({ W: 240, H: 740 }).cramped) fail('740x240 landscape is cramped, 240x740 portrait fits');
if (CRAMPED.landscapeH > 300) fail('landscape phones (>= 300 px tall) must not be told to rotate');
if (!/Add to Home Screen/.test(promptMessage('home', { ios: true }).text) || !/upright/.test(promptMessage('rotate', { want: 'portrait' }).text) || !/sideways/.test(promptMessage('rotate', { want: 'landscape' }).text)) fail('prompt messages');
{ const glyphs = { ' ': { advance: 10 } }; const lines = wrapLines('aaaa bbbb cccc dddd', glyphs, 100, 60); if (lines.length < 2 || lines.join(' ') !== 'aaaa bbbb cccc dddd') fail('wrap ' + lines); }
{ Object.defineProperty(globalThis, 'localStorage', { value: { getItem: () => '{"layout":"below"}', setItem() {} }, configurable: true, writable: true }); const t = loadTouchSettings(); delete globalThis.localStorage; if (t.layout !== 'below' || t.stick !== TOUCH_DEFAULTS.stick) fail('touch settings load'); }
console.log('Touch controls: every control, stick deflection and multi-touch combo gives the same PS2 channels as the keyboard / a DualShock; layouts and quality sizes OK.');
