// The original FE screens behind Options and Rider Details > Rewards (FE.LUI via tools/export_character_select.py ->
// UI/character-select.json), played by web/lui-player.js for web/fe-screens.js (which owns the routing and the
// whitefade). docs/characters.md "Setup Character, Rider Details, Options, Load game".
//
//   fe-game     19game_opt      Game Options: Speed units (Mph / Km/h), Widescreen (Off / 16:9 / Anamorphic),
//                               Screen position (24screen_position popup, SCREEN_POS), Video calibration (Language is
//                               hidden in the US build)
//   fe-control  22control       Controller Settings: Vibration 1P/2P (On/Off), Controller 1P/2P (Default/Pro), the
//                               DualShock picture with the Default or Pro labels; with the keyboard as the input
//                               device the picture gives way to the keyboard table and Controller 1P is Simple/Classic
//   fe-hud      21hud_opt       HUD Options: Full / Minimal / None with its HUD picture (FE_1-15/16/17)
//   fe-credits  26credits       Credits: the kT_CREDITS lines (CRAMER.LOC, order of 0x185C54..0x185F04) scrolling up
//                               1 px per frame; '^' lines are headings (60%, light, 40 px line), others 50% black
//   fe-rewards  128rewardsroom  Rewards: Art, Posters, Toys, Trading Cards, Cheat Characters, Videos with owned/total
//   fe-gallery  129 rewardgallery  a category's pages of 8 x 4 thumbnails ('?' until owned), name, You have, Cost
//   fe-poster   130rewardposter one owned reward (a Video plays full screen instead: playMovie)
//   fe-display  (19game_opt)    Display & Touch (the port's own settings, built from Game Options' rows, focus bar,
//                               arrows, help line and legend; displayScreen()): Resolution, Upscaling, PS2 softness (Off / On), Texture set
//                               (PS2 / Xbox HD, xboxRiders), Frame rate,
//                               Quality (Yes/No before the graphics rebuild), Touch controls, Touch layout, Touch stick,
//                               Touch vibration (greyed while the touch controller is not up). Options gets a
//                               "Display & Touch" item above DONE (optionsWithDisplay()); the deck's ≡ opens it.
//
// Option values are kept in localStorage 'ssx3.feOptions' (loadFeOptions()) for the HUD / input owners; Widescreen
// and the keyboard mode are the browser's own (ui.cycleWidescreen, ui.cycleKeyboard).
import { LuiScreen } from './lui-player.js';
import { REWARD_CATEGORIES } from './lodge-ui.js';
import { keyboardRows } from './loading-screen.js';
import { loadMovieIndex, startMovie } from './fe-movie.js';
import { TIERS, TIER_DEFAULTS, quality as liveQuality, setQuality, defaultRiderTextures, device as liveDevice } from './quality.js';
import { TOUCH_DEFAULTS, loadTouchSettings, saveTouchSettings } from './touch-controls.js';
import { inputDevice } from './input-glyphs.js';
import { PAD_SCREEN, PAD_TITLE, PAD_ROWS, CONTROL_PAD, controlWithPad, PadSetup, padName } from './fe-gamepad.js';   /* Controller Settings > Gamepad (pad test, remap) */
import { activeEntry } from './gamepad.js';

// Pro (INPUT2.MAP) keyboard rows: the direction keys turn and, with Space held, spin/flip (late spin / prewind);
// Y (Triangle) is the hand plant; C (Circle) has no function (the 22control Pro labels).
export function keyboardProRows(rows, s) {
  const out = rows.filter((r) => !(r.keys[0] === 'I' && r.keys[1] === 'J'));   // IJKL: Spin/Flip = 0 in the air
  return out.map((r) => {
    if (r.keys[0] === 'W') return { ...r, label: s.turn_spin_flip, note: 'Steer; spin/flip with Space held' };
    if (r.keys[0] === 'Space') return { ...r, note: 'Hold with a direction to spin/flip' };
    if (r.keys[0] === 'C') return { ...r, keys: ['Y'], note: 'Near anything you can grind' };
    return r;
  });
}

const SY = 448 / 480, FPS = 60, DISABLED_ALPHA = 128;
const OPTIONS_KEY = 'ssx3.feOptions';
export const OPTION_DEFAULTS = Object.freeze({ speedUnits: 0, vibration1: 0, vibration2: 0, controller1: 0, controller2: 0, hud: 0, screenX: 0, screenY: 0 });
// Screen position (cFEPopupScreenPos, 24screen_position; input 0x1DF050): x / y are the profile's signed bytes 0x535616 /
// 0x535615 (0 after a reset, 0x14F5D0). Each D-pad press moves one step (Right/Up +1, Left/Down -1) and pulses that arrow
// ('hl right' ...), clamped to -20..20 (the error sound at a limit); Cross keeps it, Triangle cancels. The popup sets the
// display live (vtable +0x60 -> 0x393FB8 floats), and the NTSC display set-up (0x382DC0) turns them into
// DISPLAY DX = 636 + 5 + 8x VCK (of 2560 per line) and DY = 50 - 2y (of 480 lines): one step moves the picture 1/320 of
// its width right and 1/240 of its height up. The browser moves #stage by the same fractions (CSS translate).
export const SCREEN_POS = Object.freeze({ limit: 20, stepX: 1 / 320, stepY: 1 / 240, arrows: { up: 70, down: 75, left: 80, right: 85 }, intro: 30 });
export function screenTranslate(x, y) { return x || y ? `${+(x * SCREEN_POS.stepX * 100).toFixed(4)}% ${+(-y * SCREEN_POS.stepY * 100).toFixed(4)}%` : ''; }
export function applyScreenPosition(stage, x, y) { if (stage?.style) stage.style.translate = screenTranslate(x | 0, y | 0); }
export const SCREEN_KEYS = { 'fe-game': '19game_opt', 'fe-control': '22control', 'fe-hud': '21hud_opt', 'fe-credits': '26credits',
  'fe-rewards': '128rewardsroom', 'fe-gallery': '129rewardgallery', 'fe-poster': '130rewardposter' };
export const EXTRA_SCREENS = Object.keys(SCREEN_KEYS);
// Options menu row -> screen (18options items 0 Game, 2 Controller, 3 HUD, 6 Credits; Sound is web/audio-menu.js).
export const OPTIONS_TARGETS = { 0: 'fe-game', 2: 'fe-control', 3: 'fe-hud', 6: 'fe-credits' };
const QUESTMARK = { page: 'FE_1-11', sx: 164.5, sy: 2.5, sw: 36, sh: 41 };
const GALLERY_COLS = 8, GALLERY_ROWS = 4, PAGE = GALLERY_COLS * GALLERY_ROWS;
// 129 rewardgallery: "<category>:" label per category (FEAMER), in REWARD_CATEGORIES order
const ITEM_LABELS = ['0e9cb77e', '0116945e', '01ccb51e', '09a722be', '0b3f18be', '0af1123e'];

function storage() { try { return localStorage; } catch { return null; } }
const rowDisabled = (r) => (typeof r?.disabled === 'function' ? r.disabled() : !!r?.disabled);
export function loadFeOptions() {
  try { const v = JSON.parse(storage()?.getItem(OPTIONS_KEY) || 'null'); return { ...OPTION_DEFAULTS, ...(v && typeof v === 'object' ? v : {}) }; }
  catch { return { ...OPTION_DEFAULTS }; }
}
export function saveFeOptions(v) { try { storage()?.setItem(OPTIONS_KEY, JSON.stringify(v)); } catch {} }

// Rows of an option screen: menu children (option rows or plain texts) with the state that turns them (or their
// label) white; `skip` drops rows the code hides (19game_opt Language).
export function rowModel(screen, skip = []) {
  const by = new Map(screen.elements.map((e) => [e.name, e])), menu = screen.elements.find((e) => e.kind === 'menu' && e.label === 'Menu');
  const white = (p) => p && p[14] === 255 && p[15] === 255 && p[16] === 255 && p[13] !== 0;
  // focus = the state that turns the row white; 21hud_opt's rows keep their colour and are shown one at a time (A 255)
  const focusOf = (n) => { const e = by.get(n), label = e?.kind === 'option' ? e.children[0] : n;
    return screen.events.find((ev) => ev.element === label && white(ev.props))?.frame ?? screen.events.find((ev) => ev.element === label && ev.props?.[13] > 0 && ev.frame > 25)?.frame ?? null; };
  const rows = menu.children.filter((n) => !skip.includes(n) && focusOf(n) != null).sort((a, b) => (by.get(a).props?.[1] ?? 0) - (by.get(b).props?.[1] ?? 0));
  const frames = rows.map(focusOf);
  const label = (n) => (by.get(n).kind === 'option' ? by.get(by.get(n).children[0]) : by.get(n));
  return { menu: menu.name, items: rows, frames, texts: rows.map((n) => label(n)?.text || ''),
    values: rows.map((n) => (by.get(n).kind === 'option' ? by.get(n).children[1] : null)),
    intro: Math.max(0, ...screen.events.filter((ev) => ev.frame < Math.min(...frames)).map((ev) => ev.frame)) };
}

// Credits text (0x185C54: the lines are appended to the TextScroll widget): '\\' breaks a line, '^' starts a heading.
export function creditLines(strings) {
  const out = []; let y = 0, line = '';
  // a heading keeps a 20 px gap above it (merged with a blank line or a heading before it) and a 40 px line
  // (fitted to the PS2 roll: EA line 0, headings 40/80, Larry LaPierre 200 -> Co-Producers 240 -> J. David Elton 280)
  const flush = () => { const t = line.trim(), head = t.startsWith('^'), prev = out[out.length - 1];
    if (head && prev && prev.text && !prev.heading) y += 20;
    out.push({ text: head ? t.slice(1) : t, heading: head, y }); y += head ? 40 : 20; line = ''; };
  for (const s of strings) {
    const parts = String(s).split('\\\\');
    parts.forEach((p, i) => { if (i > 0) flush(); line += p; });
  }
  if (line.trim()) flush();
  return { lines: out, height: y };
}
export const CREDITS = Object.freeze({ box: [62, 95, 525, 300], speed: 1, start: 218, name: { 9: 50, rgb: [20, 26, 30] }, heading: { 9: 60, rgb: [203, 203, 203] } });

// Load game date / time (0x2C7038 '%02d:%02d:%04d' month first (US), 0x2C6F78 '%02d:%02d:%02d': the 24-hour branch,
// the one that fits the 70-wide right-aligned time1 widget; the AM/PM forms wrap there).
export function saveDateTime(ms) {
  if (!Number.isFinite(ms)) return null;
  const d = new Date(ms), p2 = (v) => String(v).padStart(2, '0');
  return { date: `${p2(d.getMonth() + 1)}:${p2(d.getDate())}:${d.getFullYear()}`, time: `${p2(d.getHours())}:${p2(d.getMinutes())}:${p2(d.getSeconds())}` };
}

// ---- Display & Touch (fe-display) --------------------------------------------------------------------------------------
// The browser's presentation settings (web/quality.js) and the touch controller's (web/touch-controls.js), as original
// option rows. `q` rows are quality settings (applied at once, no reload; a tier changes MSAA and re-warms the pipelines:
// main.js applyAntialias via onQualityChange), `t` rows touch settings (saved in ssx3.touch). Values: [value, label]
// (a kT_ key is the game's own word).
export const DISPLAY_SCREEN = 'fe-display';
export const DISPLAY_TITLE = 'Display & Touch';
export const DISPLAY_ENTRY = Object.freeze({ text: 'Display & Touch', help: 'Change the picture, frame rate and touch controller.' });
// Texture set (quality.riderTextures; docs/xbox-textures.md section 8): the riders' PS2 textures or the Xbox HD set.
// Riders shown already keep theirs until they load again (the next course, Select Character / Equip Gear rebuild them).
export const TEXTURE_SET_ROW = Object.freeze({ q: 'riderTextures', label: 'Texture set', help: 'Xbox HD gives the riders sharper textures. Riders update when they next load.',
  values: [['ps2', 'PS2'], ['xbox', 'Xbox HD']] });
export const DISPLAY_ROWS = Object.freeze([
  {
    q: 'renderScale',
    label: 'Resolution',
    help: 'PS2 draws the original 640x448 picture. Screen matches your display.',
    values: [
      ['native', 'PS2 640x448'],
      ['native512', 'PS2 512x448'],
      ['css', 'Screen 1x'],
      ['full', 'Screen full']
    ]
  },
  {
    q: 'upscale',
    label: 'Upscaling',
    help: 'Smooth softens the picture like a TV. Sharp keeps hard pixel edges.',
    values: [
      ['smooth', 'Smooth'],
      ['pixelated', 'Sharp']
    ]
  },
  {
    q: 'ps2Output',
    label: 'PS2 softness',
    help: 'On softens the 3D picture by one PS2 pixel, like the PS2 on a TV. The HUD stays sharp.',
    values: [
      [false, 'kT_CMNOff'],
      [true, 'kT_CMNOn']
    ]
  },
  TEXTURE_SET_ROW,
  {
    q: 'fps',
    label: 'Frame rate',
    help: 'Auto drops to 30 frames a second when the device falls behind.',
    values: [
      ['auto', 'Auto'],
      [60, '60'],
      [30, '30']
    ]
  },
  {
    q: 'tier',
    label: 'Quality',
    help: 'Low suits phones, High suits computers. Changing it rebuilds the graphics.',
    values: TIERS.map((t) => [t, t[0].toUpperCase() + t.slice(1)]),
    confirm: 'Rebuild the graphics for this quality?'
  },
  {
    t: 'show',
    label: 'Touch controls',
    help: 'Auto shows the touch controller when you touch the screen.',
    values: [
      ['auto', 'Auto'],
      ['on', 'Always'],
      ['off', 'kT_CMNOff']
    ],
    confirmOff: 'Hide the touch controller?'
  },
  {
    t: 'layout',
    deck: true,
    label: 'Touch layout',
    help: 'Shoulders on top puts L1 L2 R2 R1 above the picture.',
    values: [
      ['split', 'Shoulders on top'],
      ['below', 'All below']
    ]
  },
  {
    t: 'stick',
    deck: true,
    label: 'Touch stick',
    help: 'Floating centres the stick where your thumb lands.',
    values: [
      ['floating', 'Floating'],
      ['fixed', 'Fixed']
    ]
  },
  {
    t: 'haptics',
    deck: true,
    label: 'Touch vibration',
    help: 'Vibrate on each button press where the device allows it.',
    values: [
      [true, 'kT_CMNOn'],
      [false, 'kT_CMNOff']
    ]
  }
]);

// 19game_opt parts the Display & Touch rows are built from (row template = Speed units, focused at frame 40).
const GAME = { menu: '00053c55', option: '00000032', label: '0cb4c474', value: '00003922', help: '034f7174', helpGroup: '03253174',
  title: '094dea53', arrows: '046fd665', arrowL: '0da87c07', arrowR: '0fa786c7', bar: '000006ec', marks: ['000007b1', '000007b2'],
  focus: 40, rest: 35, first: 35, step: 5, pitch: 20, fadeIn: '07ac0cc9' };
const hex = (n) => n.toString(16).padStart(8, '0');
const PAD_REMAP_HELP = hex(0x0d150003 + 5 * 16), PAD_TEST_HELP = hex(0x0d150003 + 2 * 16);   // displayScreen help lines of the Gamepad screen's Remap / Buttons rows
// A 19game_opt copy whose menu holds `rows` (label + value text per row, the help line per row, focus state k at frame
// 35 + 5k: that row white, the others orange, its help shown, the arrows and the orange bar on it). Nothing else of
// Game Options changes (background, frame, title position, legend: Previous / Reset options).
export function displayScreen(base, rows = DISPLAY_ROWS, title = DISPLAY_TITLE) {
  const by = new Map(base.elements.map((e) => [e.name, e]));
  const oldRows = by.get(GAME.menu).children, drop = new Set([...GAME.marks, ...by.get(GAME.helpGroup).children]);
  for (const r of oldRows) { drop.add(r); for (const c of by.get(r)?.children || []) drop.add(c); }
  const clone = (o) => JSON.parse(JSON.stringify(o));
  const elements = base.elements.filter((e) => !drop.has(e.name)).map(clone);
  const find = (n) => elements.find((e) => e.name === n);
  find(GAME.title).text = title;
  const names = rows.map((_, k) => ({ option: hex(0x0d150000 + k * 16), label: hex(0x0d150001 + k * 16), value: hex(0x0d150002 + k * 16), help: hex(0x0d150003 + k * 16) }));
  let index = 3000;
  const at = (ev) => base.events.find((x) => x.frame === ev.frame && x.element === ev.element);
  const focusProps = (el) => at({ frame: GAME.focus, element: el })?.props, restProps = (el) => at({ frame: GAME.rest, element: el })?.props;
  rows.forEach((row, k) => {
    const n = names[k];
    const option = { ...clone(by.get(GAME.option)), name: n.option, index: index++, label: String(k + 1), children: [n.label, n.value], props: { ...by.get(GAME.option).props, 1: k * GAME.pitch } };
    const label = { ...clone(by.get(GAME.label)), name: n.label, index: index++, parent: n.option, text: row.label };
    const value = { ...clone(by.get(GAME.value)), name: n.value, index: index++, parent: n.option, text: '' };
    const help = { ...clone(by.get(GAME.help)), name: n.help, index: index++, parent: GAME.helpGroup, text: row.help || '' };
    elements.push(option, label, value, help);
  });
  const menu = find(GAME.menu); menu.children = names.map((n) => n.option); menu.props = { ...menu.props, 7: rows.length * GAME.pitch };
  find(GAME.helpGroup).children = names.map((n) => n.help);
  const frames = rows.map((_, k) => GAME.first + k * GAME.step);
  const events = base.events.filter((ev) => !drop.has(ev.element) && !frames.includes(ev.frame) && !(ev.frame >= GAME.first && ev.frame < 85)).map(clone);
  for (const n of names) for (const el of [n.label, n.value]) events.push({ frame: 15, anim: GAME.fadeIn, element: el, mode: 1 });
  const white = { 14: 255, 15: 255, 16: 255 }, orange = { 14: 37, 15: 7, 16: 5 };
  frames.forEach((frame, k) => {
    names.forEach((n, j) => {
      events.push({ frame, element: n.label, props: { ...focusProps(GAME.label), ...(j === k ? white : orange) } });
      events.push({ frame, element: n.value, props: { ...focusProps(GAME.value), ...(j === k ? white : orange) } });
      events.push({ frame, element: n.help, props: { ...focusProps(GAME.help), 13: j === k ? 255 : 0 } });
    });
    events.push({ frame, element: GAME.arrows, props: { ...focusProps(GAME.arrows), 1: k * GAME.pitch } });
    for (const a of [GAME.arrowL, GAME.arrowR]) events.push({ frame, element: a, props: { ...focusProps(a), 13: 255 } });
    events.push({ frame, element: GAME.bar, props: { ...focusProps(GAME.bar), 1: k * GAME.pitch - 2 } });
    for (const x of base.events.filter((ev) => ev.frame === GAME.focus && ev.element === '069acf73')) events.push({ ...clone(x), frame });
  });
  return { ...base, elements, events };
}

// 18options with one more item above DONE (the Credits item's look and help slot; DONE and its focus state move down a
// row). Its focus state is frame 72 (the orange bar one row below Credits'), the other states show it orange.
const OPTS = { menu: '00053c55', after: '00000037', afterHelp: '0753da33', done: '00000038', bar: '000006ec', helpGroup: '03253174', pitch: 20 };
export const DISPLAY_ENTRY_NAMES = Object.freeze({ item: '0d15ff01', help: '0d15ff02', frame: 72 });
export function optionsWithDisplay(base, entry = DISPLAY_ENTRY) {
  const by = new Map(base.elements.map((e) => [e.name, e])), clone = (o) => JSON.parse(JSON.stringify(o));
  const menu = by.get(OPTS.menu); if (!menu || !by.get(OPTS.after) || !by.get(OPTS.done)) return base;
  const N = DISPLAY_ENTRY_NAMES, y0 = by.get(OPTS.done).props?.[1] ?? 0;
  const down = (props) => ({ ...props, 1: (props?.[1] ?? 0) + OPTS.pitch });
  const elements = base.elements.map((e) => {
    const c = clone(e);
    if (e.name === OPTS.menu) { c.children = [...e.children.filter((n) => n !== OPTS.done), N.item, OPTS.done]; c.props = { ...c.props, 7: (c.props?.[7] ?? 0) + OPTS.pitch }; }
    if (e.name === OPTS.done) c.props = down(c.props);
    if (e.name === OPTS.helpGroup) c.children = [...e.children, N.help];
    return c;
  });
  const item = { ...clone(by.get(OPTS.done)), name: N.item, index: 3100, label: '9', text: entry.text, props: { ...by.get(OPTS.done).props, 1: y0 } };
  const help = { ...clone(by.get(OPTS.afterHelp)), name: N.help, index: 3101, text: entry.help };
  elements.push(item, help);
  const doneFrame = base.events.find((ev) => ev.element === OPTS.done && ev.props?.[14] === 255 && ev.props?.[13] > 0)?.frame;
  const afterFrame = base.events.find((ev) => ev.element === OPTS.after && ev.props?.[14] === 255 && ev.props?.[13] > 0)?.frame;
  const events = [];
  for (const ev of base.events) {
    const c = clone(ev);
    if (ev.element === OPTS.done && ev.props) c.props = down(ev.props);
    if (ev.frame === doneFrame && ev.element === OPTS.bar && ev.props) c.props = down(ev.props);
    events.push(c);
    // every state: the new item orange (as Credits unfocused), its help hidden
    if (ev.element === OPTS.after && ev.props) events.push({ frame: ev.frame, element: N.item, props: { ...ev.props, 1: y0, 14: 37, 15: 7, 16: 5 } });
    if (ev.element === OPTS.afterHelp && ev.props) events.push({ frame: ev.frame, element: N.help, props: { ...ev.props, 13: 0 } });
  }
  // its own focus state: Credits' state with the roles swapped and the bar one row lower
  for (const ev of base.events.filter((x) => x.frame === afterFrame)) {
    const c = { ...clone(ev), frame: N.frame };
    if (ev.element === OPTS.after && ev.props) c.props = { ...ev.props, 14: 37, 15: 7, 16: 5 };
    else if (ev.element === OPTS.afterHelp && ev.props) c.props = { ...ev.props, 13: 0 };
    else if (ev.element === OPTS.done && ev.props) c.props = down(ev.props);
    else if (ev.element === OPTS.bar && ev.props) c.props = down(ev.props);
    events.push(c);
    if (ev.element === OPTS.after && ev.props) events.push({ frame: N.frame, element: N.item, props: { ...ev.props, 1: y0 } });
    if (ev.element === OPTS.afterHelp && ev.props) events.push({ frame: N.frame, element: N.help, props: { ...ev.props, 13: 255 } });
  }
  return { ...base, elements, events };
}

// Last input device: web/input-glyphs.js (keyboard / gamepad / touch; the pad picture shows unless it is the keyboard).
export { inputDevice } from './input-glyphs.js';

// Option rows per screen: values (locale keys or literal labels), getter/setter on the options / the browser.
function optionSpecs(x) {
  const o = () => x.options, set = (k) => (v) => { x.options = { ...x.options, [k]: v }; x.dirty = true; };   // kept on 'Yes' (commit)
  const kb = () => x.device() === 'keyboard';
  return {
    'fe-game': [
      { values: ['kT_CMNMPH', 'kT_CMNKPH'], get: () => o().speedUnits, set: set('speedUnits') },
      { values: ['kT_CMNOff', 'kT_19Widescreen169', 'kT_19WidescreenAnimorphic'], get: () => x.ui.widescreen ?? 0,
        set: (v) => { const d = ((v - (x.ui.widescreen ?? 0)) % 3 + 3) % 3; if (d) { x.ui.cycleWidescreen?.(d === 1 ? 1 : -1); x.dirty = true; } } },
      { popup: 'screen-position' },                              // Screen position (cFEPopupScreenPos): openScreenPosition
      { disabled: true, cross: true },                           // Video calibration (cFEPopupVideoCalibration)
    ],
    'fe-control': [
      { values: ['kT_CMNOn', 'kT_CMNOff'], get: () => o().vibration1, set: set('vibration1') },
      { values: ['kT_CMNOn', 'kT_CMNOff'], get: () => o().vibration2, set: set('vibration2') },
      // Controller 1P: Default = INPUT.MAP, Pro = INPUT2.MAP for the race (main.js startRun -> core set_input_map),
      // for the gamepad and the keyboard alike (the keyboard's Simple/Classic routing stays in the pause options).
      { values: ['kT_20PresetDefault', 'kT_OVRCMNPro'], get: () => o().controller1, set: set('controller1') },
      { values: ['kT_20PresetDefault', 'kT_OVRCMNPro'], get: () => o().controller2, set: set('controller2') },
      { open: PAD_SCREEN },                                      // Gamepad (web/fe-gamepad.js controlWithPad): the pad test / remap screen
    ],
    [PAD_SCREEN]: x.padSetup.specs(),
    // Display & Touch: live values from web/quality.js / web/touch-controls.js; saved at once in their own keys (not
    // part of the "save your Options?" question, like Widescreen). Touch layout / stick / vibration are greyed while
    // the touch controller is not up (keyboard or gamepad play), as the original greys what is unavailable.
    [DISPLAY_SCREEN]: DISPLAY_ROWS.map((row) => ({ row, values: row.values.map(([, label]) => label),
      get: () => { const cur = row.q ? x.quality[row.q] : x.touchSettings()[row.t]; return Math.max(0, row.values.findIndex(([v]) => String(v) === String(cur))); },
      set: (i) => x.setDisplay(row, row.values[i][0]),
      disabled: () => !!row.deck && !x.touchShown() })),
  };
}

export class FeExtraScreens {
  constructor(fe) {
    this.fe = fe;
    this.ui = fe.ui;
    this.padSetup = new PadSetup(this);
    this.quality = liveQuality;
    this.displayFrom = null;
    this.lui = {};
    this.models = {};
    this.options = loadFeOptions();
    this.category = 0;
    this.item = 0;
    this.enterAt = {};
    this.arrow = null;
    this.applyScreenPosition();
  }
  stage() { return this.ui.stage || (typeof document !== 'undefined' ? document.querySelector('#stage') : null); }
  applyScreenPosition(x = this.options.screenX, y = this.options.screenY) { applyScreenPosition(this.stage(), x, y); }
  get data() { return this.fe.data; }
  init(data, images, merge) {
    for (const [id, key] of Object.entries(SCREEN_KEYS)) {
      const screen = data.screens?.[key]; if (!screen) continue;
      const shown = id === 'fe-control' ? controlWithPad(screen) : screen;   // + the Gamepad row (web/fe-gamepad.js)
      this.lui[id] = new LuiScreen(merge(shown), images, this.ui);
      if (id === 'fe-game' && data.screens['24screen_position']) this.screenPosLui = new LuiScreen(data.screens['24screen_position'], images, this.ui);
      if (id === 'fe-game') this.models[id] = rowModel(screen, ['00000031']);
      else if (id === 'fe-control') this.models[id] = rowModel(shown);
      else if (id === 'fe-hud' || id === 'fe-rewards') this.models[id] = rowModel(screen);
    }
    if (data.screens?.['19game_opt']) {                                        // Display & Touch (displayScreen)
      const screen = displayScreen(data.screens['19game_opt']);
      this.lui[DISPLAY_SCREEN] = new LuiScreen(merge(screen), images, this.ui); this.models[DISPLAY_SCREEN] = rowModel(screen);
      const pad = displayScreen(data.screens['19game_opt'], PAD_ROWS, PAD_TITLE);   // Gamepad (web/fe-gamepad.js)
      this.lui[PAD_SCREEN] = new LuiScreen(merge(pad), images, this.ui); this.models[PAD_SCREEN] = rowModel(pad);
    }
    this.specs = optionSpecs(this);
    this.credits = data.credits ? creditLines(data.credits) : { lines: [], height: 0 };
    this.creditBitmaps = new Set(['038a1250', '038a1251']);                   // Bitmap0000 / Bitmap0001 (0x185B18)
    return true;
  }
  owns(s) { return !!this.lui[s]; }
  device() { return this.forcedDevice || inputDevice(); }         // forcedDevice: QA / tests
  now() { return this.fe.now(); }
  t(key, fallback = '') { return this.data?.strings?.[key] || fallback; }
  label(v) { return this.data?.strings?.[v] ?? v; }
  get career() { return this.ui.careerUI?.career || null; }
  get riderId() { return this.fe.baseId; }

  enter(s, from) {
    if (from !== s) this.enterAt[s] = this.now();
    this.focusAt = this.now(); this.arrow = null;
    if (s === 'fe-hud') this.fe.pendingIndex = this.options.hud;
    if (s === 'fe-gallery' && from === 'fe-rewards') this.item = 0;
  }
  // ---- menu state ----
  disabledList(s) {
    if (s === 'fe-game' || s === 'fe-control' || s === DISPLAY_SCREEN || s === PAD_SCREEN) return this.specs[s].map((r) => rowDisabled(r));
    if (s === 'fe-hud') return [false, false, false];
    if (s === 'fe-rewards') return REWARD_CATEGORIES.map(() => !this.career);
    return [false];
  }
  items(s) {
    if (s === 'fe-game' || s === 'fe-control' || s === DISPLAY_SCREEN || s === PAD_SCREEN) return this.models[s].texts;
    if (s === 'fe-hud') return [this.t(['kT_CMNHUDFull', 'kT_CMNHUDMinimal', 'kT_CMNHUDNone'][this.options.hud])];
    if (s === 'fe-rewards') return this.models[s].texts;
    if (s === 'fe-gallery') return (this.rewardItems().slice(this.page() * PAGE, this.page() * PAGE + PAGE)).map((i) => i.name);
    return ['Previous'];
  }
  layout(s, i) {
    const lui = this.lui[s], model = this.models[s];
    if (s === 'fe-gallery') { const k = i, c = k % GALLERY_COLS, r = Math.floor(k / GALLERY_COLS); return [35 + 24 + c * 68, (75 + 25 + r * 66) * SY, 45, 45 * SY]; }
    if (!model) return [430, 410 * SY, 150, 20 * SY];
    const name = model.items[i], e = lui.byName.get(name); if (!e) return [0, -100, 1, 1];
    const abs = lui.props(e, [], 0), menu = lui.byName.get(model.menu)?.props || {};
    return [(menu[0] || 0) + (abs[0] || 0), ((menu[1] || 0) + (abs[1] || 0)) * SY, 460, 20 * SY];
  }
  value(s, i) { const r = this.specs[s]?.[i]; if (!r?.values) return null; const vals = typeof r.values === 'function' ? r.values() : r.values; return { vals, index: r.get() }; }
  change(s, i, dir) {
    if (s === 'fe-hud') { this.options = { ...this.options, hud: (this.options.hud + dir + 3) % 3 }; this.dirty = true; this.ui.index = this.options.hud; this.focusAt = this.now(); }
    else {
      const r = this.specs[s]?.[i], v = this.value(s, i); if (!r || !v || rowDisabled(r)) return;
      if (r.step) r.step(dir); else if (r.set) r.set((v.index + dir + v.vals.length) % v.vals.length); else return;
    }
    this.arrow = { side: dir < 0 ? 'hll' : 'hlr', at: this.now() };
    this.ui.cb?.feOptions?.(this.options);                      // hook for the HUD / input owners
  }
  // Leaving Options after a change asks "Would you like to save your Options?" (PS2 frame): Yes keeps them on the
  // browser's card (localStorage), No keeps them for this session only (the race still uses them, as on the PS2).
  commit() { saveFeOptions(this.options); this.dirty = false; }
  discard() { this.dirty = false; }
  reset(s) {
    if (s === DISPLAY_SCREEN) { this.resetDisplay(); return true; }
    if (s === PAD_SCREEN) { this.padSetup.reset(); return true; }   // Square: this pad's remap (Yes/No)
    if (s === 'fe-game') { this.options = { ...this.options, speedUnits: 0, screenX: 0, screenY: 0 }; this.applyScreenPosition(); if (this.ui.widescreen) this.specs['fe-game'][1].set(0); }
    else if (s === 'fe-control') { this.options = { ...this.options, vibration1: 0, vibration2: 0, controller1: 0, controller2: 0 }; }
    else if (s === 'fe-hud') { this.options = { ...this.options, hud: 0 }; this.ui.index = 0; }
    else return false;
    this.dirty = true; this.ui.cb?.feOptions?.(this.options); return true;
  }

  // ---- input (from FeScreens.key / choose / back) ----
  key(e, s) {
    if (this.movie) { if (['Enter', 'Space', 'Escape'].includes(e.code)) this.stopMovie(); return true; }   // skip (0x1D2518); nothing else while it plays
    if (this.screenPos) return this.screenPosKey(e);
    if (s === PAD_SCREEN && this.padSetup.key(e)) return true;   // remap running: modal (web/fe-gamepad.js)
    const lr = e.code === 'ArrowLeft' ? -1 : e.code === 'ArrowRight' ? 1 : 0;
    if (s === 'fe-gallery') {
      const n = this.rewardItems().length; if (!n) return false;
      if (lr) { this.item = Math.max(0, Math.min(n - 1, this.item + lr)); this.focusAt = this.now(); return true; }
      if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { const d = e.code === 'ArrowUp' ? -GALLERY_COLS : GALLERY_COLS, v = this.item + d; if (v >= 0 && v < n) this.item = v; return true; }
      if (e.code === 'KeyQ' || e.code === 'PageUp' || e.code === 'KeyE' || e.code === 'PageDown') {   // L1 / R1: pages
        const d = e.code === 'KeyQ' || e.code === 'PageUp' ? -1 : 1, pages = Math.ceil(n / PAGE), p = this.page() + d;
        if (p >= 0 && p < pages) this.item = Math.min(n - 1, p * PAGE + (this.item % PAGE)); return true; }
      return false;
    }
    if (lr && (s === 'fe-game' || s === 'fe-control' || s === 'fe-hud' || s === DISPLAY_SCREEN || s === PAD_SCREEN)) { this.change(s, this.ui.index, lr); return true; }
    if ((e.code === 'ShiftLeft' || e.code === 'ShiftRight') && this.reset(s)) return true;   // Square: Reset options
    return false;
  }
  choose(s, i) {
    if (s === 'fe-rewards') { if (!this.career) return; this.category = i; this.item = 0; this.fe.returnTo['fe-rewards'] = { index: i }; this.fe.go('fe-gallery'); return; }
    if (s === 'fe-gallery') {
      const st = this.status();
      if (this.lodge() && st?.state === 'buy') {
        this.askBuy(st, this.item);
        return;
      }
      if (st?.state !== 'owned') return;
      if (this.section() === 'video') this.playMovie(st.item);
      else this.fe.go('fe-poster');
      return;
    }
    if (s === 'fe-game' && this.specs[s][i]?.popup === 'screen-position') { this.openScreenPosition(); return; }
    if (s === 'fe-control' && this.specs[s][i]?.open) { this.fe.returnTo['fe-control'] = { index: i }; this.fe.go(this.specs[s][i].open); return; }   // Gamepad
    if (s === PAD_SCREEN) { const r = this.specs[s][i]; if (r?.choose && !rowDisabled(r)) r.choose(); return; }
    if (s === 'fe-game' || s === 'fe-control' || s === DISPLAY_SCREEN) { const r = this.specs[s][i]; if (r?.values && !rowDisabled(r)) this.change(s, i, 1); }
  }
  back(s) {
    if (s === 'fe-gallery') return 'fe-rewards';
    if (s === 'fe-poster') return 'fe-gallery';
    if (s === 'fe-rewards') return 'details';
    if (s === PAD_SCREEN) { this.padSetup.remap = null; this.padSetup.test = null; return 'fe-control'; }
    if (s === DISPLAY_SCREEN && this.displayFrom) { const to = this.displayFrom; this.displayFrom = null; return to; }   // opened by ≡ (pause menu, a menu)
    return 'fe-options';
  }

  // ---- Display & Touch ----
  touchSettings() { return this.ui.touchControls?.settings || (this.storedTouch ??= loadTouchSettings()); }
  touchShown() { return !!this.ui.touchControls?.shown; }
  setTouch(key, value) {
    if (this.ui.touchControls?.setSetting) this.ui.touchControls.setSetting(key, value);
    else { const t = this.touchSettings(); t[key] = value; saveTouchSettings(t); }
  }
  // The game's Yes/No box (web/fe-screens.js drawPrompt) before a change that needs it.
  ask(message, yes, focus = 0) {
    const fe = this.fe, done = (ok) => { fe.prompt = null; if (ok) yes(); this.focusAt = this.now(); this.ui.sync?.(); };
    fe.prompt = { at: fe.now(), index: focus, message, yes: () => done(true), no: () => done(false) }; this.ui.sync?.();
  }
  setDisplay(row, value) {
    if (row.q) {
      if (String(this.quality[row.q]) === String(value)) return;
      const apply = () => setQuality({ [row.q]: value });   // MSAA / re-warm in main.js (onQualityChange -> applyAntialias)
      if (row.confirm) this.ask(row.confirm, apply); else apply();
      return;
    }
    if (row.t === 'show' && value === 'off' && this.touchShown()) { this.ask(row.confirmOff, () => this.setTouch('show', 'off'), 1); return; }
    this.setTouch(row.t, value);
  }
  // Square (Reset options): the detected tier's picture settings and the touch defaults (Yes/No when the tier changes).
  resetDisplay() {
    const auto = this.quality.auto || this.quality.tier, d = TIER_DEFAULTS[auto];
    const apply = () => {
      setQuality({
        tier: auto,
        renderScale: d.renderScale,
        upscale: d.upscale,
        fps: d.fps,
        ps2Output: false,
        riderTextures: defaultRiderTextures(liveDevice, auto)
      });
      for (const [k, v] of Object.entries(TOUCH_DEFAULTS)) if (String(this.touchSettings()[k]) !== String(v)) this.setTouch(k, v);
    };
    if (auto !== this.quality.tier) this.ask(DISPLAY_ROWS.find((r) => r.q === 'tier').confirm, apply); else apply();
  }

  // ---- rewards ----
  section() { return REWARD_CATEGORIES[this.category][0]; }
  rewardItems() { try { return this.career?.rewardItems(this.section()) || []; } catch { return []; } }
  page() { return Math.floor(this.item / PAGE); }
  // Opened from the lodge's Rider Details (web/fe-screens.js openFromLodge) the room sells, at this lodge's peak
  lodge() { return !!this.fe.lodgeBack; }
  status(i = this.item) { try { return this.career?.rewardStatus(this.riderId, this.section(), i, this.lodge() ? (this.ui.careerUI?.lodgePeak || 1) : 0) || null; } catch { return null; } }
  // the lodge's buy popup (PS2 local/ps2-capture/lodge/runs/l11 cheat-buy-popup): '<category>:' / name / Cost / You have / 'Buy item?', Yes
  askBuy(st, index) {
    const fe = this.fe, c = this.career, cu = this.ui.careerUI, t = (k, f) => cu?.t?.(k, f) || f, id = this.riderId, section = this.section(), peak = cu?.lodgePeak || 1;
    const done = (buy) => { fe.prompt = null; if (buy) c.buyReward(id, section, index, peak);
    // 0x15A818 rules, the collection bonus (web/career.js)
this.focusAt = this.now(); this.ui.sync?.(); };
    fe.prompt = { at: fe.now(), index: 0, message: t('kT_OVRCMNBuyItem', 'Buy item?'), buy: { title: this.data.strings?.[`label_${this.category}`] || this.galleryLabel(), name: st.item.name,
      rows: [[t(0x05f1a304, 'Cost:'), `$${st.item.price.toLocaleString('en-US')}`], [t(0x0918c5a5, 'You have:'), `$ ${(c.rider(id).cash ?? 0).toLocaleString('en-US')}`]] },
      yes: () => done(true), no: () => done(false) };
    this.ui.sync?.();
  }
  thumb(item) {
    if (this.section() === 'cheat_character') { const face = this.data.faces?.[this.ui.riders?.find((r) => r.character === item.character)?.face]; if (face) return { sprite: face }; }
    const im = item?.picture && this.ui.careerUI?.picture?.('REWARDS/' + item.picture);
    return im ? { image: im } : null;
  }
  help() {
    const st = this.status(), cu = this.ui.careerUI; if (!st) return '';
    if (this.lodge()) return st.help ? cu?.t?.(st.help, '') || '' : '';   // PS2 l11: 'This item is for sale.', 'Select this rider in the Cheat Character screen.'
    if (st.state === 'owned') return st.help ? cu?.t?.(st.help, '') || '' : '';
    return st.item.price ? this.t('buy_item_ctm') : (cu?.t?.(st.item.help || st.help, '') || '');
  }

  // ---- Screen position popup (SCREEN_POS) ----
  openScreenPosition() {
    if (!this.screenPosLui) return;
    const x = this.options.screenX | 0, y = this.options.screenY | 0;
    this.screenPos = { at: this.now(), x0: x, y0: y, x, y, arrow: null };
  }
  screenPosKey(e) {
    const p = this.screenPos, L = SCREEN_POS.limit;
    const d = { ArrowLeft: ['left', -1, 0], ArrowRight: ['right', 1, 0], ArrowUp: ['up', 0, 1], ArrowDown: ['down', 0, -1] }[e.code];
    if (d) {
      p.x = Math.max(-L, Math.min(L, p.x + d[1])); p.y = Math.max(-L, Math.min(L, p.y + d[2]));
      p.arrow = { frame: SCREEN_POS.arrows[d[0]], at: this.now() }; this.applyScreenPosition(p.x, p.y); return true;
    }
    if (e.code === 'Enter' || e.code === 'Space') {       // Cross: Confirm
      if (p.x !== p.x0 || p.y !== p.y0) { this.options = { ...this.options, screenX: p.x, screenY: p.y }; this.dirty = true; this.ui.cb?.feOptions?.(this.options); }
      this.screenPos = null; return true;
    }
    if (e.code === 'Escape') { this.applyScreenPosition(p.x0, p.y0); this.screenPos = null; return true; }   // Triangle: Cancel
    return true;                                          // modal
  }
  drawScreenPosition(c) {
    const p = this.screenPos, lui = this.screenPosLui, frame = this.now() - p.at, out = [];
    for (const ev of lui.screen.events) {
      if (ev.frame <= SCREEN_POS.intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (p.arrow && ev.frame === p.arrow.frame) out.push({ ev, start: p.arrow.at - p.at });
    }
    // the Confirm / Cancel legend (group 091bea93 and its menu start at A 0) is shown, both lines white (PS2 frame);
    // the message wraps after 'your' as on the PS2 (FEFONT there is ~3% wider than our 0.79 fit: width 300 -> 290)
    lui.draw(c, out, frame, (e) => (e.name === '091bea93' || e.name === '0c583950' ? { alpha: 255 }
      : e.name === '03bb2974' || e.name === '0554cbf4' ? { props: { 14: 255, 15: 255, 16: 255 } }
        : e.name === '0b7ee05e' ? { props: { 6: 290 } } : null));
  }

  // ---- reward videos (tools/export_movies.py: MOVIES/movies.json + INTRO.mp4 / MTNALIVE.mp4) ----
  // Choosing an owned video runs the MoviePlayer state (0x1D23E0): the file full screen, the sound system paused
  // (0x2B3A70) until it ends or is skipped (0x1D2518), then back to the gallery (0x1D2638).
  movieIndex() { return this.movies ??= loadMovieIndex(); }         // web/fe-movie.js (shared with Main menu > Previews)
  async playMovie(item) {
    if (this.movie || typeof document === 'undefined') return;
    const token = this.movie = { item };
    const m = (await this.movieIndex()).movies?.find((v) => v.reward === item.image);
    if (this.movie !== token) return;
    if (!m) { this.movie = null; console.warn(`Reward video ${item.image} not exported (python3 tools/export_movies.py)`); this.fe.go('fe-poster'); return; }
    token.handle = startMovie(this.ui, m, () => { if (this.movie === token) this.stopMovie(); });
    if (!token.handle) this.movie = null;
  }
  stopMovie() {
    const m = this.movie; if (!m) return; this.movie = null;
    m.handle?.stop();
    this.focusAt = this.now(); this.ui.sync?.();
  }

  // ---- drawing ----
  events(s, frame, focusFrames) {
    const screen = this.lui[s].screen, out = [], model = this.models[s];
    const intro = model?.intro ?? 25, start = this.focusAt - this.enterAt[s];
    for (const ev of screen.events) {
      if (ev.frame <= intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (focusFrames.includes(ev.frame)) out.push({ ev, start });
      else if (this.arrow && ev.frame === this.arrowFrame(screen, this.arrow.side)) out.push({ ev, start: this.arrow.at - this.enterAt[s] });
    }
    const snow = this.data.screens.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    return out;
  }
  arrowFrame(screen, side) { return screen.labels?.find((l) => l.label === side)?.frame ?? -1; }

  draw(c, b, s) {
    const now = this.now(); if (this.enterAt[s] == null) this.enterAt[s] = now;
    const frame = now - this.enterAt[s], lui = this.lui[s], model = this.models[s];
    if (s === PAD_SCREEN) this.padSetup.tick();                    // remap progress (web/fe-gamepad.js)
    const idx = this.ui.index, focus = model?.frames?.[s === 'fe-hud' ? this.options.hud : idx];
    const events = this.events(s, frame, focus != null ? [focus] : []);
    const override = this.override(s, frame);
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    lui.draw(c, events, frame, override);
    if (s === 'fe-control' && this.device() === 'keyboard' && this.ui.loading?.data) {
      // the keyboard table (web/loading-screen.js) in the pad's place, its 'Keyboard - mode' title on the subtitle line
      c.save(); c.translate(11, -1.4); c.scale(0.8, 0.8); this.drawKeyboardTable(c); c.restore();
    }
    if (s === 'fe-credits') this.drawCredits(c, frame);
    if (s === 'fe-gallery') this.drawGallery(c);
    if (s === 'fe-poster') this.drawPoster(c);
    if (s === 'fe-game' && this.screenPos) this.drawScreenPosition(c);
    c.restore();
  }

  override(s, frame) {
    const model = this.models[s], dis = this.disabledList(s);
    const rows = new Map((model?.items || []).map((n, i) => [n, i])), values = new Map((model?.values || []).map((n, i) => [n, i]));
    const lui = this.lui[s];
    const labelOf = new Map((model?.items || []).map((n, i) => [lui.byName.get(n)?.kind === 'option' ? lui.byName.get(n).children[0] : n, i]));
    return (e) => {
      if (s === 'fe-game' && ['00000031', '0bb6b1c4', '00003822', '03417174', '034d7174'].includes(e.name)) return { hidden: true };   // Language / Auto save (not in the US build)
      // under the Screen position popup the rows and their Cross marks are gone (PS2 frame: the box shows only its text)
      if (s === 'fe-game' && this.screenPos && (labelOf.has(e.name) || values.has(e.name) || e.name === '000007b1' || e.name === '000007b2')) return { hidden: true };
      if (labelOf.has(e.name) && dis[labelOf.get(e.name)]) return { alpha: DISABLED_ALPHA };
      if (s === PAD_SCREEN && e.name === PAD_REMAP_HELP && this.padSetup.remap) return { text: this.padSetup.remapHelp() };
      if (s === PAD_SCREEN && e.name === PAD_TEST_HELP && this.padSetup.test) return { text: this.padSetup.testHelp() };
      if (s === PAD_SCREEN && values.has(e.name) && dis[values.get(e.name)]) {
        const v = this.value(s, values.get(e.name));
        return { alpha: DISABLED_ALPHA, text: this.label(v?.vals[v.index] ?? '') };
      }
      if (s === DISPLAY_SCREEN && values.has(e.name) && dis[values.get(e.name)]) {
        const v = this.value(s, values.get(e.name));
        return { alpha: DISABLED_ALPHA, text: this.label(v?.vals[v.index] ?? '') };
      }
      if (values.has(e.name)) { const v = this.value(s, values.get(e.name)); if (v) return { text: this.label(v.vals[v.index] ?? '') }; }
      if (s === 'fe-control') return this.controlOverride(e);
      if (s === 'fe-rewards') return this.roomOverride(e);
      if (s === 'fe-gallery') return this.galleryOverride(e);
      if (s === 'fe-poster') return this.posterOverride(e);
      if (s === 'fe-credits' && e.name === '0991cfcc') return { hidden: true };   // TextScroll: drawn by drawCredits
      if (s === 'fe-credits' && this.creditBitmaps.has(e.name)) return { hidden: true };   // Bitmap0000/0001 (Dolby logos): scrolled in after the text
      return null;
    };
  }
  controlOverride(e) {
    if (e.name === CONTROL_PAD.value) return { text: padName(activeEntry(), 12) };   // Gamepad row: the pad the game reads
    const kb = this.device() === 'keyboard', pro = this.options.controller1 === 1;
    const lui = this.lui['fe-control'],
      pad = (this.padElements ??= new Set(
        lui.screen.elements
          .filter(
            (x) =>
              (x.kind === 'sprite' && x.sprite?.page === 'FE_1-14' && x.sprite.sw > 60) ||
              ['02c8befe', '08976274', '05f515af'].includes(x.parent) ||
              ['02c8befe', '08976274', '05f515af'].includes(x.name) ||
              (x.kind === 'shape' && x.layer >= 12 && ['02c8befe', '08976274', '05f515af'].includes(lui.byName.get(x.parent)?.parent))
          )
          .map((x) => x.name)
      ));
    if (kb && pad.has(e.name)) return { hidden: true };            // keyboard: the DualShock and its labels give way to the key table
    if (e.name === '08976274') return { alpha: pro ? 0 : 255 };    // Default labels
    if (e.name === '05f515af') return { alpha: pro ? 255 : 0 };    // Pro labels
    if (e.name === '0e303d74') return { alpha: kb || pro ? 0 : 255 };
    if (e.name === '0917a214') return { alpha: pro && !kb ? 255 : 0 };
    return null;
  }
  // Keyboard table in the pad's place (web/loading-screen.js keycaps and label style), with the Pro rows when
  // Controller 1P is Pro (INPUT2.MAP: spins/flips with the direction while Cross is held, hand plant on Triangle).
  drawKeyboardTable(c) {
    const L = this.ui.loading, pro = this.options.controller1 === 1; if (!L?.data) return;
    let rows = keyboardRows(this.ui.keyboardMode, L.data.strings);
    if (pro) rows = keyboardProRows(rows, L.data.strings);
    L.drawText(c, `Keyboard - ${this.ui.keyboardMode === 'Classic' ? 'Classic' : 'Simple'}${pro ? ' - ' + this.label('kT_OVRCMNPro') : ''}`, 55, 88, { 9: 60, 12: 9 });
    const right = 262, step = rows.length > 8 ? 19 : 21, top = 122;
    rows.forEach((row, i) => {
      const y = top + i * step, caps = [...row.keys, ...(row.alt || [])];
      const widths = caps.map((k) => L.keycapWidth(k)), gap = row.alt ? 8 : 0;
      let x = right - widths.reduce((a, w) => a + w + 3, 0) - gap;
      caps.forEach((k, j) => { if (j === row.keys.length) x += gap; L.keycap(c, k, x, y, widths[j]); x += widths[j] + 3; });
      c.fillStyle = '#ffffff'; c.fillRect(right + 3, y + 7, 14, 2);
      L.drawText(c, row.label, right + 22, y, { 9: 50, 12: 9 });
      if (row.note) L.drawText(c, row.note, right + 32 + L.textWidth(row.label, 50), y + 3, { 9: 38, 12: 9 }, 1, [44, 66, 88]);
    });
  }
  roomOverride(e) {
    const counts = ['0fb3c140', '0fb3c141', '0fb3c142', '0fb3c143', '0fb3c144', '0fb3c145'].indexOf(e.name);
    if (counts >= 0) {
      const [key] = REWARD_CATEGORIES[counts], c = this.career;
      let all = 0, have = 0; try { all = c?.rewardItems(key).length || 0; have = c?.owned(this.riderId, key).length || 0; } catch {}
      return { text: `${have} / ${all}`, alpha: 255 };
    }
    return null;
  }
  galleryOverride(e) {
    const items = this.rewardItems(), page = this.page(), cat = REWARD_CATEGORIES[this.category];
    const slot = /^06f92([6-9])2([0-7])$/.exec(e.name), box = /^068d2([6-9])2([0-7])$/.exec(e.name);
    const index = (m) => page * PAGE + (+m[1] - 6) * GALLERY_COLS + +m[2];
    if (box) return items[index(box)] ? null : { hidden: true };
    if (slot) {
      const i = index(slot), item = items[i]; if (!item) return { hidden: true };
      const st = this.status(i); if (st?.state === 'owned') return { hidden: true };  // thumbnail drawn by drawGallery
      if (this.lodge() && (st?.state === 'buy' || st?.state === 'short')) return { alpha: i === this.item ? 255 : 150 };   // for sale in this lodge: the slot's own '$' (PS2 lodge/runs/l11)
      return { sprite: QUESTMARK, alpha: i === this.item ? 255 : 150 };   // 'questmark' (0x0CABC9EB), bright when focused (PS2: 255 / 151)
    }
    // focus frame
    if (e.name === '009371e2') {
      const k = this.item % PAGE,
        p = this.lui['fe-gallery'].props(e, [], 0);
      return { props: { 0: (p[0] || 0) + (k % GALLERY_COLS) * 68, 1: (p[1] || 0) + Math.floor(k / GALLERY_COLS) * 66 } };
    }
    if (e.name === '0f480214') return { text: `Page ${page + 1}/${Math.max(1, Math.ceil(items.length / PAGE))}` };
    if (e.name === '0ca85a45') return { text: this.ui.careerUI?.t?.(cat[1], cat[2]) || cat[2], props: { 6: 400 } };   // one line, as the PS2 (flags without 0x80: no wrap)
    if (e.name === '06481d1e') return { text: this.data.strings?.[`label_${this.category}`] || this.galleryLabel() };
    if (e.name === '0896df35') return { text: items[this.item]?.name || '' };
    const st = this.status(), priced = st && st.state !== 'owned' && st.item.price;
    if (e.name === '0cb973f4' || e.name === '0a677834') return priced ? null : { hidden: true };
    if (e.name === '08df7534') return priced ? { text: `$ ${(this.career?.rider(this.riderId).cash ?? 0).toLocaleString('en-US')}` } : { hidden: true };
    if (e.name === '08973da4') return priced ? { text: `$ ${st.item.price.toLocaleString('en-US')}` } : { hidden: true };
    if (e.name === '0c37a134') return { text: this.help(), alpha: 255 };
    if (e.name === '04f76762' || e.name === '05576762') return items.length > PAGE ? null : { hidden: true };   // L1 / R1 page icons: only with more pages (PS2 Toys 1/1)
    return null;
  }
  galleryLabel() { return ({ art: 'Art:', poster: 'Poster:', toy: 'Toy:', trading_card: 'Card:', cheat_character: 'Cheat Character:', video: 'Video:' })[this.section()]; }
  posterOverride(e) {
    const item = this.rewardItems()[this.item], cat = REWARD_CATEGORIES[this.category];
    if (e.name === '0ca85a45') return { text: this.ui.careerUI?.t?.(cat[1], cat[2]) || cat[2], props: { 6: 400 } };
    if (e.name === '0896df35') return { text: item?.name || '', alpha: 255 };
    return null;
  }
  drawGallery(c) {
    const items = this.rewardItems(), page = this.page();
    for (let k = 0; k < PAGE; k++) {
      const i = page * PAGE + k, item = items[i]; if (!item || this.status(i)?.state !== 'owned') continue;
      const t = this.thumb(item), x = 35 + 24 + (k % GALLERY_COLS) * 68.3, y = 75 + 25 + Math.floor(k / GALLERY_COLS) * 66;
      if (t?.image) c.drawImage(t.image, x, y, 45, 45);
      else if (t?.sprite && this.fe.images[t.sprite.page]) c.drawImage(this.fe.images[t.sprite.page], t.sprite.sx, t.sprite.sy, t.sprite.sw, t.sprite.sh, x, y, 45, 45);
    }
  }
  drawPoster(c) {
    const item = this.rewardItems()[this.item], t = item && this.thumb(item); if (!t) return;
    const src = t.image, w0 = src ? src.naturalWidth : t.sprite.sw, h0 = src ? src.naturalHeight : t.sprite.sh;
    const k = Math.min(520 / w0, 300 / h0), w = w0 * k, h = h0 * k, x = 320 - w / 2, y = 95 + (300 - h) / 2;   // the 130rewardposter picture area
    if (src) c.drawImage(src, x, y, w, h); else c.drawImage(this.fe.images[t.sprite.page], t.sprite.sx, t.sprite.sy, t.sprite.sw, t.sprite.sh, x, y, w, h);
  }
  drawCredits(c, frame) {
    const [bx, by, bw, bh] = CREDITS.box, lines = this.credits.lines, total = this.credits.height;
    const tail = 20 + 64;                                        // the two logos (Bitmap0000/0001) follow the last line
    const top = CREDITS.start - (frame * CREDITS.speed) % (total + tail + CREDITS.start - by + 20);   // restarts once everything has left the box
    const lui = this.lui['fe-credits'];
    c.save(); c.beginPath(); c.rect(bx, by, bw, bh); c.clip();
    for (const l of lines) {
      const y = top + l.y; if (y < by - 40 || y > by + bh) continue; if (!l.text) continue;
      const st = l.heading ? CREDITS.heading : CREDITS.name;
      lui.text(c, l.text, 320, y, { 9: st[9], 12: 17, 14: st.rgb[0], 15: st.rgb[1], 16: st.rgb[2] }, 1);
    }
    for (const name of this.creditBitmaps) {
      const e = lui.byName.get(name); if (!e?.sprite) continue; const p = e.props;
      lui.sprite(c, e.sprite, { x: p[0], y: top + total + 20, a: 1 }, { ...p, 12: 9 });
    }
    c.restore();
  }
}
