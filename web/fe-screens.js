// The original front-end screens around Select Character (FE.LUI via tools/export_character_select.py ->
// UI/character-select.json), played by web/lui-player.js. docs/characters.md "Setup Character, Rider Details,
// Options, Load game".
//
//   setup       09set_char        Setup Character (0x18293C): Continue / Equip Gear / Rider Details / Music
//                                 (widgets Option_Continue/_Equip/_Rider/_Radio = 0..3; Option_GBA is hidden)
//   details     154rider_details  Rider Details (0x1832BC): Rewards / Cheat Characters / Ubertrick Setup /
//                                 Player Name / Rider Profile (Option_1..5 = 5..9). Cheat Characters is disabled
//                                 unless the base rider owns one (0x1577A0 > 0, 0x183550 -> 0x194498).
//   fe-profile  14rid_prof        Rider Profile: Rider DNA / Rider Faves / Rider Q&A / Rider BIO, Left/Right wrap
//   fe-music    140audio          Music: Radio BIG / BIG Mountain Ambience / Custom Playlist [DJ] / [No DJ] / Edit Playlist
//   fe-options  18options         Options (FEOptions 0x1887A0; Square on Select/Setup Character and Rider Details)
//   fe-load     93profile_load    Load game (cFEStateProfileLoad; Circle on Select Character)
//   fe-uber     66ut_btnmap       Ubertrick Setup (cFEStateUberTrick): categories, then the category's ubers
//   keyboard    Fullkeyboard      cKeyboardPopup (0x1CB030) over Rider Details (Player Name) and Options (Enter Cheat)
//
// Menus follow the screens' own states: the focus state of item i (the frame that turns its text white and moves
// the orange bar), wrapping Up/Down that skips disabled items, disabled text at alpha 128 (0x194498; 0.5 on the PS2
// frames: Main Menu Multi Play / Online, Rider Details Cheat Characters, local/ps2-capture/menus/fe-texture). Every Cross/Triangle that changes screen plays the white fade (whitefade, TransitionOut).
// Rider screens (setup, details) put layers 0..9 on the UI background canvas, behind the 3D rider, like 08sel_char.
import { LuiScreen } from './lui-player.js';
import { drawBuyPopup } from './buy-popup.js';   // pv buyAttribs: the buy popups as PS2 139buy_popup
import { drawFePopup } from './fe-popup.js';     // pv fePopup: cFEPopup's own layout (0x1C6B08) for the lodge's Yes / No questions
import { pv } from './pv-flags.js';
import { FOCUS_LAG } from './lui-flash.js';   // pv introLead
import { UBER_ROWS, uberEntries, uberRow, uberChoiceRows } from './lodge.js';
import { loadSave } from './career-save.js';
import { setUberChoice } from './character-roster.js';
import { clipSample, channelValue, afb } from './fe-preview.js';
import { widescreenView } from './widescreen.js';
import { FeExtraScreens, OPTIONS_TARGETS, saveDateTime, optionsWithDisplay, DISPLAY_SCREEN } from './fe-options.js';

const FPS = 60;
const FLASH = 10;                                   // whitefade: 0 -> 255 over 10 frames, then the next screen
const DISABLED_ALPHA = 96;
const BACK_LAYER = 9;
const UBER_BACK_LAYER = 11;
const POPUP_DX = 23, POPUP_DY = 77, POPUP_SX = 0.826;
// The lodge's questions (web/career-ui.js, pv lodgeLui): PS2 menus/ctm/64-lodge-quit-confirm, 65 (save progress before quitting).
// veil: the popup's full-screen quad 0279dba7 as the PS2 frame shows it (fit of 63-lodge-cursor-8quit -> 64: light blue, A 0.72).
export const LODGE_POPUP = Object.freeze({ dx: -4, dy: -36, sx: 0.681, sy: 0.585, message: [318, 92], menu: [35, 29], veil: [160, 200, 225, 184] });   // popup box on the PS2 frame (leaving Options)
// The lodge's buy popup (Ubertrick Setup, 0x1854A0): text rows in 480-line pixels, the Yes/No menu group (PS2 lodge/runs/l2).
const BUY_POPUP = { x: 326, title: 155, name: 181, rows: 222, pitch: 21, split: 315, message: 274, menu: [125, 193], dx: 7, dy: 72, sx: 0.837, sy: 1.083 };
const DEG = Math.PI / 180;
// Ubertrick Setup preview (0x184A78 / 0x184D08): game cm, Z up; three.js = (x, z, -y) / 100 as web/character-select.js.
export const UBER_VIEW = Object.freeze({ eye: [0, 660, 80], target: [0, 0, 80], root: [-228, -205, 63], turn: 62, fov: 25 });
export function toThree([x, y, z]) { return [x / 100, z / 100, -y / 100]; }
const SY = 448 / 480;
const NAME_KEY = 'ssx3.playerName', MUSIC_KEY = 'ssx3.musicMode';
export const DEFAULT_PLAYER_NAME = 'PLAYER1';        // 0x147170 on a fresh profile (PS2 frame)
// pv playerName: an empty slot name reads as sprintf(kT_MEMPlayerName 'PLAYER %d', 1) (0x147170, which also writes it back), and
// the keyboard takes 8 characters (1CD088(kb, 8) at 0x1837A4 / 0x1F4494); at the limit a letter replaces the last one (1CD2F0
// moves the caret from 8 to 7): PS2 local/ps2-capture/lodge/runs/l4, 'a' x 3 on 'PLAYER 1' -> 'PLAYER a'.
export const PS2_PLAYER_NAME = 'PLAYER 1', PLAYER_NAME_MAX = 8;
export const defaultPlayerName = () => (pv('playerName') ? PS2_PLAYER_NAME : DEFAULT_PLAYER_NAME);
export const FE_LUI = { setup: '09set_char', details: '154rider_details', 'fe-profile': '14rid_prof', 'fe-music': '140audio',
  'fe-options': '18options', 'fe-load': '93profile_load', 'fe-uber': '66ut_btnmap' };
export const RIDER_SCREENS = ['setup', 'details'];
export const MUSIC_MODES = ['radio', 'ambience', 'custom-dj', 'custom-nodj'];
export const PROFILE_PAGES = ['dna', 'faves', 'qna', 'bio'];     // entry page DNA; Left/Right wrap (PS2 frames)
// Ubertrick Setup: "Hold <button> for 1 second while in air" per category row (PS2 frames), FE_1-14 icon boxes.
export const UBER_BUTTONS = { Mute: ['L2'], Indy: ['R2'], Stalefish: ['R1'], Method: ['L1'], 'Nose Grab': ['L1', 'L2'], 'Tail Grab': ['R1', 'R2'] };
const SHOULDER = { R1: [122, 157, 26, 14], L1: [151, 157, 25, 14], R2: [122, 174, 26, 14], L2: [151, 174, 25, 14] };
const SPR = { check: { page: 'FE_1-11', sx: 56.5, sy: 41.5, sw: 15, sh: 16 }, box: { page: 'FE_1-11', sx: 58.5, sy: 23.5, sw: 13, sh: 13 },
  dollar: { page: 'FE_1-11', sx: 57.5, sy: 3.5, sw: 16, sh: 15 } };
// cKeyboardPopup keys, rows = the LUI row groups left to right (labels from the PS2 frame; the code sets them).
export const KEY_ROWS = [
  ['~', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 'Back'],
  ['Tab', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '|'],
  ['Caps', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', 'Done'],
  ['Shift', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', 'Up'],
  ['Clear', 'Space', 'Left', 'Down', 'Right']];
const KEY_GROUPS = ['039115e3', '03e115e3', '03f115e3', '03c115e3', '03d115e3'];
const SHIFTED = { '1': '!', '2': '@', '3': '#', '4': '$', '5': '%', '6': '^', '7': '&', '8': '*', '9': '(', '0': ')' };
// Disabled keys: Tab always (0x1CE3C8 key 0x4B), Up/Down (single line); Player Name also the punctuation keys
// (0x1837C4..0x1838A0: ten more ids), as dimmed on the PS2 frame.
const KEYS_OFF = { cheat: ['Tab', 'Up', 'Down'], name: ['Tab', 'Up', 'Down', '~', '-', '=', '[', ']', '|', ':', '"', '<', '>', '?'] };

function storage() { try { return localStorage; } catch { return null; } }
function stored(key, fallback) { try { return storage()?.getItem(key) ?? fallback; } catch { return fallback; } }
function store(key, value) { try { storage()?.setItem(key, value); } catch {} }

// ---- pure helpers (web/test-fe-screens.mjs) ----

// The screen's menu: its text children in display order and each one's focus state frame.
export function menuModel(screen, menuName) {
  const by = new Map(screen.elements.map((e) => [e.name, e]));
  const menu = menuName ? by.get(menuName) : (screen.elements.find((e) => e.kind === 'menu' && e.label === 'Menu') || screen.elements.find((e) => e.kind === 'menu'));
  if (!menu) return null;
  const white = (p) => p && p[14] === 255 && p[15] === 255 && p[16] === 255 && p[13] !== 0;
  const focus = {};
  for (const ev of screen.events) if (menu.children.includes(ev.element) && white(ev.props) && focus[ev.element] == null) focus[ev.element] = ev.frame;
  const items = menu.children.filter((n) => by.get(n)?.kind === 'text' && focus[n] != null)
    .sort((a, b) => (by.get(a).props?.[1] ?? 0) - (by.get(b).props?.[1] ?? 0));
  const first = Math.min(...items.map((n) => focus[n]));
  return { menu: menu.name, items, focus, frames: items.map((n) => focus[n]), intro: Math.max(0, ...screen.events.filter((ev) => ev.frame < first).map((ev) => ev.frame)), texts: items.map((n) => by.get(n).text) };
}

// Up/Down: wraps and skips disabled items (Setup/Rider Details on the PS2: Down from the last item goes to the first).
// pv lodgeFlash: a lodge LUI's menu focus plays from its intro's end, the label whose control 0x42 (intro done, stop) makes the state
// active (28lodge / 155rider_details_conquer frame 25; PS2 tout-detD: Rider Details' focus bar is up 32 frames after the switch, not at 24).
const introEnd = (screen, model) => screen.labels?.find((l) => l.control?.some((c) => c.startsWith('42')))?.frame ?? model.intro;
export function stepMenu(index, direction, disabled) {
  const n = disabled.length;
  for (let k = 1; k <= n; k++) { const i = ((index + direction * k) % n + n) % n; if (!disabled[i]) return i; }
  return index;
}
export function firstEnabled(disabled, from = 0) { return disabled[from] ? stepMenu(from, 1, disabled) : from; }

// Setup Character: every item is enabled (0x182AA8..0x182BD8, Option_GBA hidden); Equip Gear needs the wardrobe.
export function setupDisabled({ wardrobe = false } = {}) { return [false, !wardrobe, false, false]; }
// Rider Details: Cheat Characters only when the base rider owns one; Ubertrick Setup needs the uber tables.
export function detailsDisabled({ cheats = 0, ubers = true, rewards = true } = {}) { return [!rewards, cheats <= 0, !ubers, false, false]; }
// Music: the custom playlists need a playlist (help: create one in Conquer the Mountain mode).
export function musicDisabled({ playlist = 0, edit = false } = {}) { return [false, false, playlist <= 0, playlist <= 0, !edit]; }
// Options: all eight items live in the original; the browser has Game Options (camera/widescreen/keyboard via the
// 'options' screen), Save/Load, Enter Cheat and DONE; Sound/Controller/HUD/Credits screens are not ported.
export function optionsDisabled({ sound = false, controller = false, hud = false, credits = false } = {}) { return [false, !sound, !controller, !hud, false, false, !credits, false]; }

// Ubertrick Setup choice -> grab-profile uber rows (set 1): web/lodge.js (uberRow / uberChoiceRows, 0x14FEA8).
export { uberRow, uberChoiceRows };

// On-screen keyboard (pure state): press a key label.
export function keyboardPress(state, label) {
  const s = { ...state };
  const put = (ch) => {
    if (s.overwriteFull && s.text.length >= s.max) { const at = Math.min(s.caret, s.max - 1); s.text = s.text.slice(0, at) + ch + s.text.slice(at + 1); s.caret = Math.min(at + 1, s.max - 1); return; }   // 1CD2F0
    if (s.text.length >= s.max) return; s.text = s.text.slice(0, s.caret) + ch + s.text.slice(s.caret); s.caret = s.overwriteFull ? Math.min(s.caret + 1, s.max - 1) : s.caret + 1; };
  switch (label) {
    case 'Back': if (s.caret > 0) { s.text = s.text.slice(0, s.caret - 1) + s.text.slice(s.caret); s.caret--; } break;
    case 'Clear': s.text = ''; s.caret = 0; break;
    case 'Space': put(' '); break;
    case 'Caps': s.caps = !s.caps; break;
    case 'Shift': s.shift = !s.shift; break;
    case 'Left': s.caret = Math.max(0, s.caret - 1); break;
    case 'Right': s.caret = Math.min(s.text.length, s.caret + 1); break;
    case 'Done': s.done = true; break;
    case 'Tab': case 'Up': case 'Down': break;
    default: {
      let ch = label;
      if (/^[a-z]$/.test(ch) && (s.caps !== s.shift)) ch = ch.toUpperCase();
      else if (s.shift && SHIFTED[ch]) ch = SHIFTED[ch];
      put(ch); s.shift = false;
    }
  }
  return s;
}

// ---- screens ----
export class FeScreens {
  constructor(ui) {
    this.ui = ui; this.data = null; this.lui = {}; this.models = {}; this.enterAt = {}; this.focusAt = 0; this.flash = null;
    this.page = 0; this.pageAt = -1e9; this.pageSide = null; this.uberCat = 0; this.uberList = false; this.uberIndex = 0;
    this.keyboard = null; this.notice = null; this.loadState = null; this.returnTo = {}; this.pendingIndex = null; this.wardrobe = null;
    this.playerName = stored(NAME_KEY, null) || defaultPlayerName();
    this.lodgeBack = null;   // pv lodgeDetails: the lodge's Rider Details opened fe-uber / fe-profile (openFromLodge)
    this.musicMode = MUSIC_MODES.includes(stored(MUSIC_KEY)) ? stored(MUSIC_KEY) : 'radio';
  }
  get ready() { return !!this.data; }
  now() { return performance.now() * FPS / 1000; }

  // data = UI/character-select.json (already loaded by CharacterSelect), images = its page images.
  init(data, images = {}) {
    if (!data?.screens?.['09set_char'] || !data.screens['154rider_details']) return false;
    this.data = data; this.images = images;
    const snow = data.screens.bg_snow_loop;
    const merge = (a) => (snow ? { ...a, elements: [...a.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))], animations: { ...a.animations, ...snow.animations } } : a);
    for (const [id, key] of Object.entries(FE_LUI)) {
      let screen = data.screens[key]; if (!screen) continue;
      if (id === 'fe-options') screen = optionsWithDisplay(screen);   // + Display & Touch above DONE (web/fe-options.js)
      this.lui[id] = new LuiScreen(merge(screen), images, this.ui);
      this.models[id] = id === 'fe-uber' ? menuModel(screen, '0c5a3d9e') : id === 'fe-load' ? menuModel(screen, '07653c55') : menuModel(screen);
    }
    if (data.screens['155rider_details_conquer']) { this.lodgeLui = new LuiScreen(merge(data.screens['155rider_details_conquer']), images, this.ui); this.lodgeModel = menuModel(data.screens['155rider_details_conquer']); }
    if (data.screens['28lodge']) { this.lodgeMenuLui = new LuiScreen(merge(data.screens['28lodge']), images, this.ui); this.lodgeMenuModel = menuModel(data.screens['28lodge']); }   // pv lodgeLui
    if (data.screens['66ut_btnmap']) this.uberModel = menuModel(data.screens['66ut_btnmap'], '05ab281b');
    if (data.screens.Fullkeyboard) this.kbLui = new LuiScreen(data.screens.Fullkeyboard, images, this.ui);
    // Key caps on the name-entry keyboard (keyboardKey: typing goes straight in, so L1 Shift / R1 Caps are the real keys)
    if (this.kbLui) this.kbLui.keys = { l1: 'ShiftLeft', r1: 'CapsLock', cross: 'Enter', circle: 'Backspace' };
    if (data.screens.popup) this.popupLui = new LuiScreen(data.screens.popup, images, this.ui);
    // pv buyAttribs: the PS2's buy popup screen (web/buy-popup.js); the box grows with its shapes' scale, only 'item name' wraps
    if (data.screens['139buy_popup']) { this.buyPopupLui = new LuiScreen(data.screens['139buy_popup'], images, this.ui); this.buyPopupLui.shapeScale = true; this.buyPopupLui.flagWrap = true; }
    this.extra = new FeExtraScreens(this); this.extra.init(data, images, merge);   // web/fe-options.js: Options sub-screens, Credits, Rewards
    return true;
  }
  async load() {
    try { const mods = import.meta.glob('./wardrobe.js'); const load = mods['./wardrobe.js']; if (load) this.wardrobe = await load(); } catch { this.wardrobe = null; }
    return this.init(this.ui.characterSelect?.data, this.ui.characterSelect?.images);
  }
  owns(screen) { return this.ready && ((!!FE_LUI[screen] && !!this.lui[screen]) || !!this.extra?.owns(screen)); }
  // Race HUD options (web/ui.js 'game' branch): profile 0x535610 bits 30..31 HUD level, bit 19 Km/h (FE Options).
  hudLevel() { return this.extra?.options?.hud ?? 0; }
  speedUnits() { return this.extra?.options?.speedUnits ?? 0; }
  // Controller 1P preset for the race input map (main.js startRun -> core set_input_map): 0 Default, 1 Pro (INPUT2.MAP).
  inputMap() { return this.extra?.options?.controller1 === 1 ? 1 : 0; }
  vibration() { return (this.extra?.options?.vibration1 ?? 0) === 0; }   // Controller Settings Vibration 1P: 0 On (default) / 1 Off (web/rumble.js)
  // Minimal: the HUD owner's flag word loses 0x0510C040 (owner+0x3CC 0x1530C047 -> 0x10200007 in the derived
  // Snow Jam states): trick names/popups (0x4000, 0x4000000) go, the score (0x402) stays.
  minimalHudFlags(flags) { return (flags & ~0x0510C040) >>> 0; }
  sub(screen = this.ui.screen) { return this.extra?.owns(screen) ? this.extra : null; }
  t(name, fallback = '') { return this.data?.strings?.[name] || fallback; }

  // ---- state ----
  get career() { return this.ui.careerUI?.career || null; }
  get baseId() { return this.ui.characterSelect?.base?.id || this.ui.rider?.id || 'zoe'; }
  cheatsOwned() { try { const cs = this.ui.characterSelect; return cs ? cs.unlocked(cs.base).length : 0; } catch { return 0; } }
  playlistSize() { try { return this.career?.songState(this.baseId)?.playlist?.length || 0; } catch { return 0; } }
  hasWardrobe() { return !!(this.wardrobe?.openEquipGear || this.ui.cb?.equipGear); }
  disabledList(screen = this.ui.screen) {
    if (this.sub(screen)) return this.extra.disabledList(screen);
    switch (screen) {
      case 'setup': return setupDisabled({ wardrobe: this.hasWardrobe() });
      case 'details': return detailsDisabled({ cheats: this.cheatsOwned(), ubers: !!this.career?.shop, rewards: !!this.career });
      case 'fe-music': return musicDisabled({ playlist: this.playlistSize(), edit: !!this.ui.cb?.music });
      case 'fe-options': { const d = optionsDisabled({ sound: !!this.ui.cb?.audioOptions, controller: !!this.extra?.owns('fe-control'), hud: !!this.extra?.owns('fe-hud'), credits: !!this.extra?.owns('fe-credits') });
        if (this.displayIndex() >= 0) d.splice(this.displayIndex(), 0, !this.extra?.owns(DISPLAY_SCREEN)); return d; }
      case 'fe-uber': return this.models['fe-uber'].items.map(() => false);
      case 'fe-load': return this.models['fe-load'].items.map((_, i) => i > 0 || !this.saveInfo());
      default: return (this.models[screen]?.items || []).map(() => false);
    }
  }
  items() {
    const s = this.ui.screen;
    if (this.sub(s)) return this.extra.items(s);
    if (s === 'fe-profile') return [this.t(['rider_dna', 'rider_faves', 'rider_qna', 'rider_bio'][this.page])];
    if (s === 'fe-uber') return [...UBER_ROWS.map(([n]) => n), 'DONE'];
    if (s === 'fe-load') return this.saveInfo() ? [this.saveInfo().name] : [this.t('empty', '<Empty>')];
    return this.models[s]?.texts || [];
  }
  disabled(i) { return !this.ui.ready || !!this.disabledList()[i]; }
  // Pointer targets on the 640x448 UI (from the menu text positions: right-aligned 250-wide rows).
  layout(i) {
    const s = this.ui.screen, lui = this.lui[s], model = this.models[s];
    if (this.sub(s)) return this.extra.layout(s, i);
    if (s === 'fe-profile') return [148, 94 * SY, 349, 35 * SY];
    const name = model?.items[i]; const e = name && lui?.byName.get(name); if (!e) return [0, -100, 1, 1];
    const menu = lui.byName.get(model.menu), mp = menu?.props || {}, p = e.props || {};
    const w = p[6] || 250, h = p[7] || 20, anchorRight = (p[12] ?? 9) & 32;
    const x = (mp[0] || 0) + (p[0] || 0) - (anchorRight ? w : 0), y = (mp[1] || 0) + (p[1] || 0);
    return [x, y * SY, w, h * SY];
  }

  enter(screen, from) {
    // pv lodgeFlash: the lodge's own LUI screens replay their intro on every entry (the new state's LUI from frame 0; PS2 tout-troA /
    // tout-detD: Rider Details' intro under the fall), also back from a screen that is not drawn here; its popups are child states.
    if (pv('lodgeFlash') && from !== screen) { if (screen === 'ctm-details') this.lodgeScreen = this.lodgeIndex = null; else if (screen === 'ctm-lodge' && !['ctm-saveprompt', 'ctm-quit', 'ctm-quitsave', 'ctm-saved'].includes(from)) this.lodgeMenuScreen = this.lodgeMenuIndex = null; }
    if (!this.owns(screen)) { if (this.keyboard && !this.owns(screen)) this.keyboard = null; return; }
    if (this.sub(screen)) { this.keyboard = null; this.extra.enter(screen, from); const want = this.returnTo[screen]?.index; if (want != null) { this.pendingIndex = want; this.returnTo[screen].index = null; } return; }
    const now = this.now();
    if (from !== screen) this.enterAt[screen] = this.ui.careerUI?.lodgeFlash?.introStart?.(now) ?? now;   // pv introLead (web/lui-flash.js)
    this.focusAt = now; this.keyboard = null;
    if (screen === 'fe-profile' && from !== screen) { this.page = 0; this.pageAt = -1e9; }
    if (screen === 'fe-uber' && from !== screen) { this.uberCat = 0; this.uberList = false; this.uberAnim = null; }   // 0x184C60: animator reset, 436
    // Setup Character / Rider Details replay the cheer on every entry, also back from a sub-screen (0x182DB8 -> 0x182EC0,
    // ARMSX2 snaps from Music / Options / Rider Details): web/character-select.js pose() restarts when lastScreen differs.
    if (RIDER_SCREENS.includes(screen) && from !== screen && this.ui.characterSelect) this.ui.characterSelect.lastScreen = null;
    if (RIDER_SCREENS.includes(screen) && from !== screen) this.ui.characterSelect?.hidePreviewFor?.(from === 'character' ? 26 : 30);   // PS2: model hidden, then the cheer from t=0
    if (screen === 'fe-load') this.loadState = null;
    const want = this.returnTo[screen]?.index; if (want != null) { this.pendingIndex = want; this.returnTo[screen].index = null; }
  }
  // ui.index is the focus; entering puts it on the remembered or first enabled item (after ui.set's own index=0).
  focus() {
    const s = this.ui.screen, dis = this.disabledList(s);
    if (this.pendingIndex != null) { this.ui.index = this.pendingIndex; this.pendingIndex = null; }
    if (this.ui.index >= dis.length) this.ui.index = 0;
    if (dis[this.ui.index]) this.ui.index = firstEnabled(dis, this.ui.index);
    return this.ui.index;
  }

  // A whitefade started from a screen another module draws (Square / the deck's ≡ on Main Menu, Select Character, the
  // pause menus): no FeScreens.draw runs there, so web/ui.js drawStreamingNote (after every frame's UI) calls this.
  drawForeignFlash(c) {
    if (!this.flash || !c || this.owns(this.ui.screen)) return;
    const now = this.now();
    if (now - this.flash.at >= FLASH) { const f = this.flash; this.flash = null; f.to(); return; }
    c.save(); c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash.at) / FLASH)})`; c.fillRect(0, 0, 640, 448); c.restore();
  }
  go(screen, index = null) {
    const to = () => { this.ui.set(screen); if (index != null) { this.ui.index = index; this.ui.sync(); } };
    if (this.lodgeFlash()) { this.ui.careerUI.lodgeGo(to); return; }   // opened from the lodge: its state change's flash (rise, switch, fall)
    this.flash = { at: this.now(), to };
  }
  // pv lodgeFlash (web/lui-flash.js): the screens the lodge opens (Rewards, Ubertrick Setup, Rider Profile) change through the lodge's flash
  lodgeFlash() { return !!(this.lodgeBack && pv('lodgeFlash') && this.ui.careerUI?.lodgeGo); }
  // Leave to a screen owned by someone else, remembering where to come back.
  away(screen, back, index) { this.returnTo[back] = { index }; this.go(screen); }

  // Options' Display & Touch item (optionsWithDisplay), -1 without it; DONE stays the last item.
  displayIndex() { return this.models['fe-options']?.texts.length > 8 ? this.models['fe-options'].texts.length - 2 : -1; }
  choose(i) {
    const ui = this.ui, s = ui.screen;
    if (!ui.ready || this.flash || this.prompt || (pv('lodgeFlash') && ui.careerUI?.lodgeFlash?.active)) return;
    if (this.keyboard) { this.keyPress(this.keyLabel()); return; }
    if (this.disabled(i)) return;
    ui.index = i; this.focusAt = this.now();
    if (s === 'setup') {
      if (i === 0) {                                                  // Continue: the event flow (Select Peak on the PS2)
        this.flash = { at: this.now(), to: () => { if (ui.onlineMode && ui.mpUI) ui.mpUI.enter(); else if (ui.careerMode && ui.careerUI?.ready) ui.careerUI.enter(); else ui.set('event'); } };
      } else if (i === 1) this.equipGear();
      else if (i === 2) { this.returnTo.setup = { index: 2 }; this.go('details'); }
      else if (i === 3) { this.returnTo.setup = { index: 3 }; this.go('fe-music'); }
      return;
    }
    if (s === 'details') {
      this.returnTo.details = { index: i };
      if (i === 0) { if (this.extra?.owns('fe-rewards')) this.go('fe-rewards'); else this.openRewards(); }   // 128rewardsroom (web/fe-options.js)
      else if (i === 1) ui.characterSelect?.openCheats();            // 131cheat_char over the page (web/character-select.js)
      else if (i === 2) this.go('fe-uber');
      else if (i === 3) this.openKeyboard('name');
      else if (i === 4) this.go('fe-profile');
      return;
    }
    if (s === 'fe-music') {
      if (i < 4) { this.musicMode = MUSIC_MODES[i]; store(MUSIC_KEY, this.musicMode); ui.cb?.music?.(this.musicMode); }
      else ui.cb?.music?.('edit');                                    // Edit Playlist (16radio): the audio agent's hook
      return;
    }
    if (s === 'fe-options') {
      this.returnTo['fe-options'] = { index: i };
      if (OPTIONS_TARGETS[i] && this.extra?.owns(OPTIONS_TARGETS[i])) this.go(OPTIONS_TARGETS[i]);   // 19game_opt / 22control / 21hud_opt / 26credits (web/fe-options.js)
      else if (i === 0) { ui.optionsReturn = 'fe-options'; this.go('options'); }      // fallback: the browser options screen
      else if (i === 1) ui.cb?.audioOptions?.();
      else if (i === 4) { if (this.ui.saveLoad?.ready) this.go('fe-saveload'); else { this.loadFrom = 'fe-options'; this.go('fe-load'); } }   // Save/Load: 25saveload (web/fe-saveload.js)
      else if (i === 5) this.openKeyboard('cheat');
      else if (i === this.displayIndex()) { this.extra.displayFrom = null; this.go(DISPLAY_SCREEN); }
      else if (i === (this.models['fe-options']?.texts.length ?? 8) - 1) this.back();   // DONE
      return;
    }
    if (s === 'fe-uber') { this.uberChoose(); return; }
    if (s === 'fe-load') { this.loadGame(); return; }
    if (this.sub(s)) this.extra.choose(s, i);
  }

  back() {
    const ui = this.ui, s = ui.screen;
    if (this.flash || (pv('lodgeFlash') && ui.careerUI?.lodgeFlash?.active)) return;
    if (this.keyboard) { this.keyboard = null; ui.sync(); return; }
    if (s === 'fe-uber' && this.uberList) { this.uberList = false; this.focusAt = this.now(); return; }
    if (this.lodgeBack && (s === 'fe-uber' || s === 'fe-profile' || s === 'fe-rewards')) { const back = this.lodgeBack, lodge = this.lodgeFlash(); this.lodgeBack = null; if (lodge) this.ui.careerUI.lodgeGo(back); else this.flash = { at: this.now(), to: back }; return; }   // pv lodgeDetails
    const to = { setup: 'character', details: 'setup', 'fe-profile': 'details', 'fe-music': 'setup', 'fe-uber': 'details' }[s];
    if (to) { this.go(to, to === 'character' ? null : this.returnTo[to]?.index ?? null); if (to !== 'character') this.returnTo[to] = { index: this.returnTo[to]?.index }; return; }
    if (this.sub(s)) { const to = this.extra.back(s); this.go(to, this.returnTo[to]?.index ?? null); return; }
    if (s === 'fe-options' && this.extra?.dirty && !this.prompt) { this.askSaveOptions(); return; }
    if (s === 'fe-options' || s === 'fe-load') {
      const back = (s === 'fe-load' && this.loadFrom === 'fe-options') ? 'fe-options' : (s === 'fe-options' ? this.optionsFrom : this.loadFrom) || 'setup';
      this.go(back, this.returnTo[back]?.index ?? null);
    }
  }
  // Square: Options (Setup Character, Rider Details); Select Character's Square/Circle come through openOptions/openLoad.
  openOptions(from = this.ui.screen) { this.optionsFrom = from; this.returnTo[from] = { index: this.ui.index }; this.go('fe-options'); }
  // The touch deck's ≡ (web/touch-controls.js openSettings): Display & Touch over a paused race or from Options; back returns there.
  openDisplay(from = this.ui.screen) {
    if (!this.extra?.owns(DISPLAY_SCREEN)) return false;
    this.extra.displayFrom = from === 'fe-options' ? null : from; this.returnTo[from] = { index: this.ui.index }; this.go(DISPLAY_SCREEN); return true;
  }
  openLoad(from = this.ui.screen) { this.loadFrom = from; this.returnTo[from] = { index: this.ui.index }; this.go('fe-load'); }

  // Clickable legend entries: Options (Square) on Select/Setup Character and Rider Details, Load game (Circle) on Select.
  legend(nav) {
    const s = this.ui.screen; if (!this.ready || !['character', 'setup', 'details'].includes(s) || this.keyboard) return;
    const add = (label, y, fn) => { const b = document.createElement('button'); b.textContent = label; b.setAttribute('aria-label', label); b.disabled = !this.ui.ready;
      b.style.cssText = `left:${440 / 6.4}%;top:${y * SY / 4.48}%;width:${150 / 6.4}%;height:${15 * SY / 4.48}%;`; b.onclick = fn; nav.appendChild(b); };
    add('Options', 430, () => this.openOptions(s));
    if (s === 'character') add('Load game', 445, () => this.openLoad(s));
  }
  equipGear() {
    const ui = this.ui, rider = ui.characterSelect?.human?.() || ui.rider;
    this.returnTo.setup = { index: 1 };
    if (this.wardrobe?.openEquipGear) { this.wardrobe.openEquipGear(ui, rider); return; }
    ui.cb?.equipGear?.(rider);
  }
  // Rewards: the career rewards viewer (web/lodge-ui.js ctm-rewards) for this rider, view-only as in the FE.
  openRewards() {
    const cu = this.ui.careerUI; if (!cu?.career || !cu.lodge) return;
    cu.lodge.feReturn = () => { cu.lodge.feReturn = null; this.returnTo.details = { index: 0 }; this.ui.set('details'); this.ui.index = 0; this.ui.sync(); };
    this.go('ctm-rewards');
  }

  key(e) {
    const s = this.ui.screen;
    // Select Character: Square = Options (FEOptions), Circle = Load game (cFEStateProfileLoad). Letters are Enter
    // Cheat typing there (web/character-select.js), so Circle is Backspace; Square is Shift as everywhere.
    if (s === 'character' && this.ready && !e.repeat && !this.ui.characterSelect?.flash) {
      if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') { e.preventDefault(); this.openOptions('character'); return true; }
      if (e.code === 'Backspace') { e.preventDefault(); this.openLoad('character'); return true; }
    }
    if (!this.owns(s)) return false;
    if (pv('lodgeFlash') && this.ui.careerUI?.lodgeFlash?.active) { e.preventDefault(); return true; }   // no input under the lodge's flash
    if (this.ui.characterSelect?.overlay?.(s)) return false;
    if (this.keyboard) return this.keyboardKey(e);
    if (this.prompt) return this.promptKey(e);
    if (this.sub(s) && !e.repeat && this.extra.key(e, s)) { e.preventDefault(); this.ui.sync(); return true; }
    const codes = ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Enter', 'Space', 'Escape', 'ShiftLeft', 'ShiftRight', 'KeyO', 'KeyL'];
    if (codes.includes(e.code)) e.preventDefault();
    if (e.repeat && !['ArrowUp', 'ArrowDown'].includes(e.code)) return true;
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { this.move(e.code === 'ArrowUp' ? -1 : 1); this.ui.sync(); return true; }
    if (s === 'fe-profile' && (e.code === 'ArrowLeft' || e.code === 'ArrowRight')) { this.turnPage(e.code === 'ArrowLeft' ? -1 : 1); return true; }
    if (s === 'fe-uber' && e.code === 'ArrowRight' && !this.uberList) return true;
    if (e.code === 'Enter' || e.code === 'Space') { this.choose(this.ui.index); return true; }
    if (e.code === 'Escape') { this.back(); return true; }
    // Square (Shift / O on the keyboard) = Options on Setup Character and Rider Details (0x182D58 / 0x183A2C).
    if ((e.code === 'ShiftLeft' || e.code === 'ShiftRight' || e.code === 'KeyO') && RIDER_SCREENS.includes(s)) { this.openOptions(s); return true; }
    if ((e.code === 'ShiftLeft' || e.code === 'ShiftRight') && s === 'fe-uber') { this.previewTrick(); return true; }   // Square: Preview trick (list only)
    return false;
  }
  move(direction) {
    const s = this.ui.screen;
    if (s === 'fe-profile') return;
    if (s === 'fe-uber' && this.uberList) { const n = this.uberEntries().length; if (n) this.uberIndex = (this.uberIndex + direction + n) % n; this.focusAt = this.now(); return; }
    this.ui.index = stepMenu(this.focus(), direction, this.disabledList(s)); this.focusAt = this.now();
  }
  turnPage(direction) { this.page = (this.page + direction + PROFILE_PAGES.length) % PROFILE_PAGES.length; this.pageAt = this.now(); this.pageSide = direction < 0 ? 'hll' : 'hlr'; this.ui.sync(); }

  // ---- Ubertrick Setup ----
  uberCategory() { return UBER_ROWS[this.uberCat]?.[1]; }
  uberEntries(category = this.uberCategory()) {
    const c = this.career; if (!c?.shop || category == null) return [];
    return uberEntries(c.shop, category).map((entry, index) => ({ index, entry, ...(c.uberStatus(this.baseId, category, index) || {}) })).filter((x) => x.state);
  }
  uberChoose() {
    const ui = this.ui;
    if (!this.uberList) {
      if (ui.index >= UBER_ROWS.length) { this.back(); return; }
      this.uberCat = ui.index; this.uberList = true; const list = this.uberEntries();
      this.uberIndex = 0; this.focusAt = this.now(); return;       // the list opens on its first row (PS2 frame), not on the selected uber
    }
    const x = this.uberEntries()[this.uberIndex]; if (!x) return;
    if (this.lodgeBack && x.state === 'buy') { this.askBuyUber(x); return; }   // the lodge (pv lodgeDetails): 0x1854A0, cash >= price -> the buy popup; short: nothing
    if (x.state !== 'owned' && x.state !== 'selected') return;   // buy only in the lodge
    this.career.selectUber(this.baseId, this.uberCategory(), x.index);
    this.applyUbers();
  }
  // The rider's chosen ubers go to the human's grab profile through web/character-roster.js (set 1 rows), then the
  // rider reloads with them (main.js selectRider -> loadCharacter -> humanSettings).
  applyUbers() {
    const c = this.career; if (!c?.shop) return;
    const character = c.rider(this.baseId).character, selection = {};
    for (const [cat, st] of Object.entries(c.uber(this.baseId) || {})) selection[cat] = st.selected;
    setUberChoice(this.baseId, uberChoiceRows(c.shop, this.data.uber_points?.table, character, selection));
    const human = this.ui.characterSelect?.human?.(); if (human && this.ui.cb?.rider) this.ui.cb.rider(human).catch?.(() => {});
  }

  // ---- the career lodge's Rider Details (pv lodgeDetails, web/lodge-ui.js): Ubertrick Setup and Rider Profile are these
  // screens (0x1F4064: cFEStateUberTrick with the buy handler 0x1854A0 / 0x184F40, 14rid_prof); back returns to the lodge.
  openFromLodge(screen, back) { this.lodgeBack = back; this.go(screen); }
  // 0x1854A0 -> popup 0x1CAF58 (PS2 lodge/runs/l2 uber-buy-popup): 'Ubertrick:' name, Cost, You have, 'Buy this ubertrick?',
  // Yes focused; Yes = 0x184F40 (cash - price, 0x14FE08 clears the lock bit); the entry is then owned, not selected.
  askBuyUber(x) {
    const c = this.career, id = this.baseId, cat = this.uberCategory(), t = (k, f) => this.ui.careerUI?.t?.(k, f) || f;
    const done = (buy) => { this.prompt = null; if (buy) c.buyUber(id, cat, x.index); this.focusAt = this.now(); this.ui.sync(); };
    this.prompt = { at: this.now(), index: 0, message: t(0x0249c7c2, 'Buy this ubertrick?'), buy: { title: t(0x0db6ea7b, 'Ubertrick:'), name: x.entry.name,
      rows: [[t(0x05f1a304, 'Cost:'), `$${x.entry.price.toLocaleString('en-US')}`], [t(0x0918c5a5, 'You have:'), `$ ${(c.rider(id).cash ?? 0).toLocaleString('en-US')}`]] },
      yes: () => done(true), no: () => done(false) };
    this.ui.sync();
  }

  // ---- Ubertrick Setup trick preview (cFEStateUberTrick 0x184C60, web/main.js frontEndRider) ----
  // The FE preview slot shows the rider on the board: camera eye (0,660,80) -> (0,0,80) (0x184A78), 25 degrees; root
  // (-228,-205,63) cm turned 2 x 31 degrees about Z (0x184D08, quaternion half-angle gp-0x59F0); the board is not
  // moved away (no 0x19F548). Semantic 436 = FE_A_CYC loops; Square plays the focused uber's trick id A as a
  // semantic (animator +8 = 0x7B for Indian, 0x9E for Pommel Me on the derived states), which the FE variant table
  // maps to fe:(id-93) (113 UBER_INDY_1_L1 .. 158 UBER_NOSE_ZOE); it plays once, then FE_A_CYC again.
  ownsRider(screen) { return this.ready && screen === 'fe-uber'; }
  previewEntry() { return this.ui.characterSelect?.previewEntry?.() ?? null; }
  showPreview(screen, playing) {
    const cs = this.ui.characterSelect, T = cs?.T; if (!cs?.preview3d) return false;
    const on = !playing && this.ownsRider(screen) && !!T && cs.preview3d.want(T, this.previewEntry());
    cs.preview3d.show(on);
    return on;
  }
  place(T, model, camera) {
    if (!this.ownsRider(this.ui.screen)) return false;
    const cs = this.ui.characterSelect; if (cs) cs.T = T;
    camera.clearViewOffset();
    camera.aspect = widescreenView(this.ui.widescreen, 0).cameraAspect;
    camera.fov = 2 * Math.atan(Math.tan(UBER_VIEW.fov * DEG) * 0.75) / DEG;
    camera.updateProjectionMatrix(); camera.up.set(0, 1, 0);
    camera.position.set(...toThree(UBER_VIEW.eye)); camera.lookAt(...toThree(UBER_VIEW.target));
    const q = new T.Quaternion().setFromAxisAngle(new T.Vector3(0, 1, 0), UBER_VIEW.turn * DEG), at = toThree(UBER_VIEW.root);
    if (cs?.preview3d?.want(T, this.previewEntry()) && model.parent && cs.preview3d.place(T, model.parent, 0)) {
      const g = cs.preview3d.model.group; g.position.set(...at); g.quaternion.copy(q); g.updateMatrixWorld(true);
      cs.preview3d.light(T, cs.core);
    }
    model.position.set(...at); model.scale.setScalar(1 / (model.userData.riderScale || 1)); model.quaternion.copy(q);
    return true;
  }
  uberClipName(clips, entry) {
    const sem = entry?.trick_ids?.[0], id = sem >= 113 && sem <= 158 ? `fe:${sem - 93}` : null;
    return (id && clips.find((c) => c.id === id)?.name) || null;
  }
  previewTrick() {
    const x = this.uberList && this.uberEntries()[this.uberIndex];
    if (x) this.uberAnim = { ...(this.uberAnim || {}), clip: null, entry: x.entry, t: 0, fade: null, restart: true };
  }
  pose(T, bones, rig, dt, { clips, samples, scale } = {}) {
    if (!this.ownsRider(this.ui.screen) || !clips || !samples) return false;
    const cs = this.ui.characterSelect, preview = !!cs?.preview3d?.want(T, this.previewEntry());
    if (preview && !cs.preview3d.ready) return true;
    const target = preview ? cs.preview3d.model : rig;
    let a = this.uberAnim;
    if (!a || a.rig !== target) a = this.uberAnim = { rig: target, clip: 'FE_A_CYC', t: 0, fade: null };
    if (a.restart) { a.rig = target; a.clip = this.uberClipName(clips, a.entry) || 'FE_A_CYC'; a.t = 0; a.fade = null; a.restart = false; }
    a.t += dt;
    const find = (name) => clips.find((c) => c.name === name);
    let main = find(a.clip); if (!main) return false;
    const once = a.clip !== 'FE_A_CYC', len = (c) => (c.frame_count - 1) / c.fps;
    if (once && a.t >= len(main) && !a.fade) a.fade = { t: 0 };
    let weight = 0, idle = null;
    if (a.fade) { a.fade.t += dt; idle = find('FE_A_CYC'); weight = Math.min(1, a.fade.t / 0.23);
      if (weight >= 1) { a.clip = 'FE_A_CYC'; a.t = a.fade.t; a.fade = null; main = idle; idle = null; weight = 0; } }
    const A = clipSample(main, a.t, a.clip === 'FE_A_CYC'), B = idle ? clipSample(idle, a.fade.t, true) : null;
    if (preview) return cs.preview3d.apply(T, A, B, weight, samples, { board: true });
    const v = new T.Vector3(), q = new T.Quaternion();
    rig.bones.forEach((b, k) => {                                 // the race rider (no FE package): same clips, board kept
      const bone = bones[k]; if (!bone) return;
      const local = (smp) => {
        const stream = smp.clip.streams?.[String(b.file)]; if (!stream) return null;
        const tc = b.animation_translation_channel, rc = b.animation_rotation_channel, x = (i) => channelValue(samples, smp, stream, i);
        return { p: tc >= 0 ? new T.Vector3(x(tc) / 100, x(tc + 2) / 100, -x(tc + 1) / 100).multiplyScalar(scale) : null, r: rc >= 0 ? afb(T, x(rc), x(rc + 1), x(rc + 2)) : null };
      };
      bone.position.fromArray(b.translation).multiplyScalar(scale); bone.quaternion.fromArray(b.rotation).normalize();
      const pa = local(A), pb = B ? local(B) : null;
      if (pa?.p) bone.position.copy(pa.p); if (pa?.r) bone.quaternion.copy(pa.r);
      if (pb) { if (pb.p) bone.position.lerp(v.copy(pb.p), weight); if (pb.r) bone.quaternion.slerp(q.copy(pb.r), weight); }
    });
    return true;
  }

  // ---- Yes/No popup (FE.LUI 'popup') ----
  askSaveOptions() {
    const done = (save) => { this.prompt = null; if (save) this.extra.commit(); else this.extra.discard(); this.back(); };
    this.prompt = { at: this.now(), index: 0, message: this.t('save_options_q', 'Would you like to save your Options?'), yes: () => done(true), no: () => done(false) };
    this.ui.sync();
  }
  promptKey(e) {
    const p = this.prompt;
    if (['ArrowUp', 'ArrowDown', 'Enter', 'Space', 'Escape'].includes(e.code)) e.preventDefault();
    if (e.repeat) return true;
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { p.index = 1 - p.index; p.at = this.now() - 30; }
    if (e.code === 'Enter' || e.code === 'Space') (p.index === 0 ? p.yes : p.no)();
    return true;
  }
  // Layout from the PS2 frame (leaving Options after a change): the box centred 62 px lower than its LUI frame
  // state, the question centred at y 193, the Yes/No menu and its Cross icon moved to (+95, +171).
  drawPrompt(c, now) {
    const p = this.prompt, lui = this.popupLui; if (!lui) return;
    // a buy popup over a sub screen (the lodge's rewards room) is laid out as over the main screens, which draw it inside their own scale
    if (p.buy && this.sub(this.ui.screen) && !p.nested) { c.save(); c.scale(1, SY); p.nested = true; try { this.drawPrompt(c, now); } finally { p.nested = false; c.restore(); } return; }
    // pv buyAttribs: cUIStateBuyPopup's own screen 139buy_popup (box, texts, focus, its intro from p.at) instead of the 'popup' fit below
    // (drawn in the UI canvas's own 640x448 frame: the FE screens call drawPrompt inside their LUI scale, the rewards room inside two)
    if (p.buy && pv('buyAttribs') && this.buyPopupLui) {
      c.save(); c.setTransform(1, 0, 0, 1, 0, 0);
      try { if (drawBuyPopup(c, this.buyPopupLui, { frame: now - p.at, index: p.index, type: p.buy.title, name: p.buy.name, question: p.message, cost: p.buy.rows?.[0]?.[1] ?? '', have: p.buy.rows?.[1]?.[1] ?? '' })) return; } finally { c.restore(); }
    }
    const frame = now - p.at, focus = p.index === 0 ? 30 : 35;
    const events = lui.screen.events.filter((ev) => ev.frame === focus).map((ev) => ({ ev, start: 0 }));
    const frameParts = new Set(['065adea5', '09797985', '02bb75c5', '0ecd9825', '059e7b67']);
    c.save(); c.scale(1, SY);
    lui.draw(c, events, frame, (e) => {
      // the box: its group moved (+13, +77) and its vertices narrowed to 0.826 (the code sizes it to the question)
      if (e.name === '00376566') { const q = lui.props(e, [], 0); return { props: { 0: (q[0] || 0) + (p.buy ? BUY_POPUP.dx : POPUP_DX), 1: (q[1] || 0) + (p.buy ? BUY_POPUP.dy : POPUP_DY) } }; }
      if (frameParts.has(e.name)) { const q = lui.props(e, [], 0), o = {}; for (let k = 0; k < (e.shape?.[0] ?? 4); k++) { o[21 + 9 * k] = (q[21 + 9 * k] || 0) * (p.buy ? BUY_POPUP.sx : POPUP_SX); if (p.buy) o[22 + 9 * k] = (q[22 + 9 * k] || 0) * BUY_POPUP.sy; } return { props: o }; }
      if (['0279dba7', '0df0ab27', '0078ba87', '05cbab62'].includes(e.name)) return { hidden: true };   // not on the PS2 frame
      // The buy popup's veil (PS2 139buy_popup 0885e124, the same quad as here): its vertices carry A 175 and the element alpha animates
      // to 175 too; the GS draws it at 175 >> 1 = 0.68 once (fit over lodge/attrs ba1: 0.681), not 0.68 x 0.68 (docs/career-events.md)
      if (e.name === '0885e124' && p.buy && pv('buyAttribs')) return { alpha: 255 };
      if (e.name === '07c54234') return { text: p.message, props: { 0: 330, 1: p.buy ? BUY_POPUP.message : 209, 12: 17 } };
      if (e.name === '0c583950' || e.name === '09cbb693') return { props: p.buy ? { 0: BUY_POPUP.menu[0], 1: BUY_POPUP.menu[1] } : { 0: 95, 1: 190 } };
      if (e.name === '067b06b0') return { text: this.t('yes', 'Yes'), props: { 13: 255, 14: p.index === 0 ? 255 : 0, 15: p.index === 0 ? 255 : 0, 16: p.index === 0 ? 255 : 0 } };
      if (e.name === '067b06b1') return { text: this.t('no', 'No'), props: { 13: 255, 14: p.index === 1 ? 255 : 0, 15: p.index === 1 ? 255 : 0, 16: p.index === 1 ? 255 : 0 } };
      if (['067b06b2', '067b06b3', '067b06b4', '0c55e7c5', '07c59864', '0121a25c', '0344d188', '046556e4'].includes(e.name)) return { hidden: true };
      return null;
    });
    if (p.buy) {   // label (dark) and value (light) lines above the question
      const dark = { 9: 60, 12: 17, 14: 0, 15: 0, 16: 0 }, light = { 9: 60, 12: 17, 14: 255, 15: 255, 16: 255 };
      lui.text(c, p.buy.title, BUY_POPUP.x, BUY_POPUP.title, dark, 1, false); lui.text(c, p.buy.name, BUY_POPUP.x, BUY_POPUP.name, light, 1, false);
      p.buy.rows.forEach(([label, value], i) => { const y = BUY_POPUP.rows + i * BUY_POPUP.pitch;
        lui.text(c, label, BUY_POPUP.split - 5, y, { ...dark, 12: 33 }, 1, false); lui.text(c, value, BUY_POPUP.split + 5, y, { ...light, 12: 1 }, 1, false); });
    }
    c.restore();
  }

  // ---- keyboard ----
  openKeyboard(kind) {
    const text = kind === 'name' ? this.playerName : '';
    const ps2Name = kind === 'name' && pv('playerName');
    this.keyboard = { kind, text, caret: ps2Name ? Math.min(text.length, PLAYER_NAME_MAX - 1) : text.length, caps: false, shift: false, max: kind === 'name' ? (ps2Name ? PLAYER_NAME_MAX : 15) : 24, overwriteFull: ps2Name, row: kind === 'name' ? 2 : 1, col: kind === 'name' ? 12 : 1, at: this.now() };
    this.ui.sync();
  }
  keyLabel() { const k = this.keyboard; return KEY_ROWS[k.row]?.[k.col]; }
  keyOff(label) { return KEYS_OFF[this.keyboard?.kind || 'cheat'].includes(label); }
  keyPress(label) {
    if (!label || this.keyOff(label)) return;
    const k = keyboardPress(this.keyboard, label);
    this.keyboard = k;
    if (k.done) this.keyboardDone();
  }
  keyboardDone() {
    const k = this.keyboard; this.keyboard = null;
    if (k.kind === 'name') { if (k.text.trim()) { this.playerName = pv('playerName') ? k.text : k.text.trim(); store(NAME_KEY, this.playerName); this.ui.playerName = this.playerName; if (pv('playerName') && this.career) this.career.playerName = this.playerName; } }
    else {
      const hit = this.ui.characterSelect?.enterCheat?.(k.text);    // 0x187D38: a character code unlocks it for every rider
      this.notice = { text: hit ? `${this.t('enter_cheat', 'Enter Cheat')}: ${hit.name}` : '', until: this.now() + 150 };
    }
    this.ui.sync();
  }
  keyboardKey(e) {
    const k = this.keyboard;
    if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Enter', 'Escape', 'Backspace', 'Space', 'Tab'].includes(e.code)) e.preventDefault();
    if (e.code === 'Escape') { this.back(); return true; }
    if (e.code === 'Enter') { this.keyPress(this.keyLabel()); return true; }
    if (e.code === 'Backspace') { this.keyPress('Back'); return true; }
    if (e.code.startsWith('Arrow')) {
      if (e.code === 'ArrowLeft' || e.code === 'ArrowRight') { const n = KEY_ROWS[k.row].length; k.col = (k.col + (e.code === 'ArrowLeft' ? -1 : 1) + n) % n; }
      else { const x = this.keyX(k.row, k.col); k.row = (k.row + (e.code === 'ArrowUp' ? -1 : 1) + KEY_ROWS.length) % KEY_ROWS.length; k.col = this.nearestKey(k.row, x); }
      return true;
    }
    if (e.key && e.key.length === 1 && !e.ctrlKey && !e.metaKey) {
      e.preventDefault();
      // typing on the computer keyboard: the character goes in and the highlight moves to Done (Enter accepts)
      if (k.overwriteFull) this.keyboard = { ...keyboardPress(k, e.key), row: 2, col: 12 };   // pv playerName: the pad keyboard's rule (1CD2F0)
      else if (k.text.length < k.max) this.keyboard = { ...k, text: k.text.slice(0, k.caret) + e.key + k.text.slice(k.caret), caret: k.caret + 1, row: 2, col: 12 };
      return true;
    }
    return true;
  }
  // The keyboard uses its state at frame 45 (the layout on the PS2 frames); key boxes from the shapes' vertices.
  kbEvents() { return (this.kbLui?.screen.events || []).filter((ev) => ev.frame === 45).map((ev) => ({ ev, start: 0 })); }
  keyShapes() {
    if (this.shapes) return this.shapes;
    const lui = this.kbLui; if (!lui) return [];
    const events = this.kbEvents(), props = (e) => lui.props(e, events, 0);
    this.shapes = KEY_GROUPS.map((g) => {
      const group = lui.byName.get(g); const gp = group ? props(group) : {};
      return (group?.children || []).map((n) => lui.byName.get(n)).filter(Boolean).sort((a, b) => (props(a)[0] || 0) - (props(b)[0] || 0)).map((e) => {
        const p = props(e), nv = e.shape?.[0] ?? 4, xs = [], ys = [];
        for (let k = 0; k < nv; k++) { xs.push(p[21 + 9 * k] || 0); ys.push(p[22 + 9 * k] || 0); }
        return { name: e.name, x: (gp[0] || 0) + (p[0] || 0) + Math.min(...xs), y: (gp[1] || 0) + (p[1] || 0) + Math.min(...ys), w: Math.max(...xs) - Math.min(...xs), h: Math.max(...ys) - Math.min(...ys), nv };
      });
    });
    return this.shapes;
  }
  keyX(row, col) { const s = this.keyShapes()[row]?.[col]; return s ? s.x + s.w / 2 : 0; }
  nearestKey(row, x) { const r = this.keyShapes()[row] || []; let best = 0; r.forEach((s, i) => { if (Math.abs(s.x + s.w / 2 - x) < Math.abs(r[best].x + r[best].w / 2 - x)) best = i; }); return best; }

  // ---- Load game ----
  saveInfo() {
    const c = this.career; if (!c) return null;
    const save = loadSave(c.storage); if (!save) return null;
    return { name: this.playerName, riders: Object.keys(save.riders || {}).length, save };
  }
  loadGame() {
    const info = this.saveInfo(); if (!info || this.loadState) return;
    this.loadState = { at: this.now(), done: false };
    this.career.save = info.save;                                    // the browser's memory card: localStorage ssx3.career.v1
    if (!this.career.save.records) this.career.save.records = this.career.rules.records.map((slot) => slot.map((r) => ({ ...r })));
  }

  // ---- drawing ----
  events(id, frame, now) {
    const screen = this.lui[id].screen, model = this.models[id], out = [], focus = [];
    const intro = id === 'fe-profile' ? 25 : id === 'fe-load' ? 25 : model?.intro ?? 25;
    if (model && id !== 'fe-profile') {
      const idx = id === 'fe-uber' ? (this.uberList ? this.uberCat : this.ui.index) : this.ui.index;
      if (id === 'fe-load') { if (this.saveInfo()) focus.push(model.frames[0]); }
      else if (model.frames[idx] != null) focus.push(model.frames[idx]);
      if (id === 'fe-uber' && this.uberList && this.uberModel?.frames[this.uberIndex] != null) focus.push(this.uberModel.frames[this.uberIndex]);
    }
    if (id === 'fe-profile') focus.push(35);
    const start = this.focusAt - this.enterAt[id];
    for (const ev of screen.events) {
      if (ev.frame <= intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (focus.includes(ev.frame)) out.push({ ev, start });
      else if (id === 'fe-profile' && this.pageSide && ev.frame === (this.pageSide === 'hll' ? 105 : 110)) out.push({ ev, start: this.pageAt - this.enterAt[id] });
    }
    const snow = this.data.screens.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    return out;
  }

  draw(c, b) {
    const ui = this.ui, id = ui.screen, now = this.now();
    if (this.flash && now - this.flash.at >= FLASH) { const f = this.flash; this.flash = null; f.to(); return; }
    if (this.sub(id)) { this.focus(); this.extra.draw(c, b, id); if (this.prompt) this.drawPrompt(c, now); if (this.flash) { c.save(); c.scale(1, SY); c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash.at) / FLASH)})`; c.fillRect(0, 0, 640, 480); c.restore(); } ui.careerUI?.lodgeFlash?.draw(c); return; }
    if (this.enterAt[id] == null) this.enterAt[id] = now;
    this.focus();
    const frame = now - this.enterAt[id], lui = this.lui[id];
    const events = this.events(id, frame, now), override = this.override(id, frame, events);
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    b.save(); b.scale(1, SY); c.save(); c.scale(1, SY);
    if (RIDER_SCREENS.includes(id)) { lui.draw(b, events, frame, override, (l) => l <= BACK_LAYER); lui.draw(c, events, frame, override, (l) => l > BACK_LAYER); }
    else if (id === 'fe-uber') { lui.draw(b, events, frame, override, (l) => l <= UBER_BACK_LAYER); lui.draw(c, events, frame, override, (l) => l > UBER_BACK_LAYER); }   // the panel (9..11) behind the preview rider
    else lui.draw(c, events, frame, override);                       // no 3D rider on these screens (PS2 frames)
    this.drawExtras(c, id, frame, events);
    if (this.keyboard) this.drawKeyboard(c, now);
    if (this.prompt) this.drawPrompt(c, now);
    if (this.flash) { c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash.at) / FLASH)})`; c.fillRect(0, 0, 640, 480); }
    c.restore(); b.restore();
    if (id === 'details') ui.characterSelect?.drawOverlay?.(c);
    ui.careerUI?.lodgeFlash?.draw(c);   // pv lodgeFlash: the lodge's flash over a screen it opened
  }

  override(id, frame, events) {
    const ui = this.ui, model = this.models[id], dis = this.disabledList(id), profile = this.profile();
    const human = ui.characterSelect?.base || ui.rider;
    const items = new Map((model?.items || []).map((n, i) => [n, i]));
    const pageGroups = { dna: ['06271a73', '0926ac74'], faves: ['08be95b3', '0cc835f4'], qna: ['06272873', '0626ac54'], bio: ['0e4317f4'] };
    const page = PROFILE_PAGES[this.page];
    return (e) => {
      if (items.has(e.name) && dis[items.get(e.name)]) return { alpha: DISABLED_ALPHA };
      if (e.name === '0c485b75') return { text: human?.name || '' };             // 'rider name' (0x182DB8 -> 0x14EEC8)
      if (id === 'details' && e.name === '0a7f2263') return { alpha: 255 };       // the frame group is A=0 in 154rider_details; the PS2 shows it (kept from Setup Character)
      if (id === 'fe-music') {
        const box = ['00036968', '00037968', '00038968', '00039968'].indexOf(e.name);
        if (box >= 0) return { sprite: MUSIC_MODES[box] === this.musicMode ? SPR.check : SPR.box, ...(dis[box] ? { props: { 14: 128, 15: 128, 16: 128 } } : {}) };   // disabled: grey box (PS2 frame)
        if (e.name === '0c37a134') return { text: this.t(['help_radio_big', 'help_ambience', dis[2] ? 'help_no_playlist' : 'help_custom_dj', dis[3] ? 'help_no_playlist' : 'help_custom_nodj', 'help_edit_playlist'][ui.index]) };
      }
      if (id === 'fe-options' && this.notice && this.now() < this.notice.until && this.notice.text && e.name === '056433e4') return { text: this.notice.text };
      if (id === 'fe-profile') {
        for (const [pg, groups] of Object.entries(pageGroups)) if (groups.includes(e.name) && pg !== page) return { hidden: true };
        if ((e.name === '0e75f658' || e.name === '05c5de78') && (page === 'qna' || page === 'bio')) return { hidden: true };   // the column dashes: DNA / Faves only (PS2 frames)
        if (e.name === '09cd2630') return { text: this.t(['rider_dna', 'rider_faves', 'rider_qna', 'rider_bio'][this.page]), alpha: 255, props: { 13: 255, 14: 0, 15: 0, 16: 0 } };
        if (!profile) return null;
        const dna = ['00049241', '00049242', '00049243', '00049244', '00049245', '00049246', '00049247', '00049248'].indexOf(e.name);
        if (dna >= 0) return { text: profile.dna?.[dna] ?? '' };
        const fav = ['04a6aa61', '04a6aa62', '04a6aa63', '04a6aa64', '04a6aa65', '04a6aa66', '04a6aa67', '04a6aa68', '04a6aa69', '0a6aa6c0', '0a6aa6c1', '0a6aa6c2'].indexOf(e.name);
        if (fav >= 0) return { text: profile.faves?.[fav] ?? '' };
        const qna = ['00056241', '00056242', '00056243', '00056244'].indexOf(e.name);
        if (qna >= 0) return { text: profile.qna?.[qna] ?? '' };
        // the BIO text box (kind 0x20, 450 wide at 50%) wraps between 436 and 445 px and steps 17.5 px per line on
        // the PS2 frame (line extents measured on out-profile/sample00941)
        if (e.name === '0e4317f4') return { text: profile.bio || human?.bio || '', props: { 6: 440, pitch: 17.5 } };
      }
      if (id === 'fe-uber') return this.uberOverride(e);
      if (id === 'fe-load') return this.loadOverride(e);
      return null;
    };
  }
  profile() {
    const cs = this.ui.characterSelect, base = cs?.base || this.ui.rider;
    const p = this.data.profiles?.[base?.id];
    if (p) return p;
    return base ? { dna: base.dna || [], faves: [], qna: [], bio: base.bio || '' } : null;
  }
  uberOverride(e) {
    const list = this.uberEntries(this.uberList ? this.uberCategory() : UBER_ROWS[Math.min(this.ui.index, UBER_ROWS.length - 1)]?.[1]);
    const cats = ['00074ca5', '000704b9', '082b25d8', '073caf54', '00075695', '0007a7fc'];
    const cat = cats.indexOf(e.name);
    if (cat >= 0) return { text: UBER_ROWS[cat][0], ...(this.uberList && cat === this.uberCat ? { props: { 14: 200, 15: 222, 16: 236 } } : {}) };
    if (e.name === '0aa92dd4' && this.uberList) return { hidden: true };             // category bar hides in the list
    if (e.name === '0a989cb4' && !this.uberList) return { hidden: true };            // list bar only in the list
    const row = ['0b8f9e90', '0b8f9e91', '0b8f9e92', '0b8f9e93', '0b8f9e94', '0b8f9e95', '0b8f9e96', '0b8f9e97', '0b8f9e98'].indexOf(e.name);
    if (row >= 0) { const x = list[row]; return x ? { text: x.entry.name, alpha: 255, props: { 14: 255, 15: 255, 16: 255 } } : { hidden: true }; }
    const icon = ['00069e30', '00069e31', '00069e32', '00069e33', '00069e34', '00069e35', '00069e36', '00069e37', '00069e38'].indexOf(e.name);
    if (icon >= 0) { const x = list[icon]; return x ? { sprite: x.state === 'selected' ? SPR.check : x.state === 'owned' ? SPR.box : SPR.dollar } : { hidden: true }; }
    const focused = this.uberList ? list[this.uberIndex] : null, buy = focused && (focused.state === 'buy' || focused.state === 'short');
    // Cost / You have: shown for every focused list row, owned ones too (the entry's price; PS2 frame on Indian)
    const cost = !!focused;
    if (e.name === '0aa67834' || e.name === '0cb671f4') return cost ? { alpha: 255, props: { 6: 200 } } : { hidden: true };   // one line (no wrap)
    if (e.name === '06aa68fd') return cost ? { text: `$ ${focused.entry.price.toLocaleString('en-US')}`, alpha: 255, props: { 14: 255, 15: 255, 16: 255 } } : { hidden: true };
    if (e.name === '08cb687d') return cost ? { text: `$ ${(this.career?.rider(this.baseId).cash ?? 0).toLocaleString('en-US')}`, alpha: 255, props: { 14: 255, 15: 255, 16: 255 } } : { hidden: true };
    if (e.name === '036b82d5') return { alpha: this.uberList ? 0 : 255 };
    const lodgeHelp = this.lodgeBack && buy && (focused.state === 'buy' ? this.ui.careerUI?.t?.(0x012de269, 'This trick is available to buy.') : this.ui.careerUI?.t?.(0x0e820e42, 'Save more cash to buy this trick.'));   // the lodge (PS2 lodge/runs/l1)
    if (e.name === '03588f1d') return this.uberList ? { alpha: 255, text: lodgeHelp || (buy ? this.t('buy_trick_ctm') : this.t('customize_ubers')) } : { alpha: 0 };
    if (e.name === '069acf75') return { alpha: this.uberList ? 255 : 0 };             // "Preview trick" (list only)
    if (e.name === '007769a1') return { alpha: this.uberList ? 255 : 0 };
    const buttons = UBER_BUTTONS[UBER_ROWS[this.uberList ? this.uberCat : Math.min(this.ui.index, UBER_ROWS.length - 1)]?.[0]] || [];
    const btn = ['05a89110', '05a89111'].indexOf(e.name);
    if (btn >= 0) { const b = buttons[btn]; if (!b || this.ui.index >= UBER_ROWS.length && !this.uberList) return { hidden: true };
      const [sx, sy, sw, sh] = SHOULDER[b]; return { sprite: { page: 'FE_1-14', sx, sy, sw, sh }, props: { 0: [112, 145][btn], 1: 355, 6: 19, 7: 11 } }; }
    // "for 1 second while in air" follows the icons (PS2 frames: x 146 after one, 177 after two)
    if (e.name === '00705a71') return { props: { 0: (buttons.length > 1 ? 177 : 146) - 25, 6: 300 } };
    if (e.name === '05a89112') return { sprite: { page: 'FE_1-14', sx: 34.5, sy: 123.5, sw: 20, sh: 19 }, props: { 0: 161, 1: 373, 6: 15, 7: 15 } };   // "Then press" Square
    return null;
  }
  loadOverride(e) {
    const info = this.saveInfo(), st = this.loadState;
    if (e.name === '006afdc5' || e.name === '02206ce5' || e.name === '069acf73' || e.name === '069acf75') return { hidden: true };   // Save-mode widgets
    if (e.name === '0224c154') return { alpha: 255 };
    if (e.name === '0f06f0f5') {                                     // 'Memory card text'
      if (st) return { text: this.now() - st.at < 60 ? this.t('loading', 'Loading...') : this.t('load_complete', 'Load complete.'), alpha: 255 };
      return info ? { hidden: true } : { text: this.t('empty', '<Empty>'), alpha: 255 };
    }
    if (!info || st) { if (['07653c55', '0006a8a5', '0007b035', '00000031', '000006ec', '05c5de78', '0e75f658'].includes(e.name)) return { hidden: true }; return null; }   // message mode: no table
    if (['07653c55', '0006a8a5', '0007b035'].includes(e.name)) return { alpha: 255 };
    if (e.name === '00000031') return { alpha: 255 };
    if (/^0000003[2-6]$/.test(e.name)) return { hidden: true };
    if (e.name === '0728fcb1') return { text: info.name };
    const when = saveDateTime(info.save?.savedAt);                                  // career-save.js savedAt (older saves: none)
    if (e.name === '006a8a81') return when ? { text: when.date, alpha: 255 } : { text: '', alpha: 0 };
    if (e.name === '007b0381') return when ? { text: when.time, alpha: 255 } : { text: '', alpha: 0 };
    if (/^0728fcb[2-6]$|^006a8a8[2-6]$|^007b038[2-6]$/.test(e.name)) return { hidden: true };
    if (e.name === '09acf751' || e.name === '00077698') return { alpha: 255 };     // "Load" + Cross
    return null;
  }
  drawExtras(c, id, frame) {
    if (id === 'fe-load' && this.loadState && this.now() - this.loadState.at > 120) { this.loadState = null; this.back(); }
  }

  // The career lodge's Rider Details (155rider_details_conquer, 0x1F4064): drawn for web/lodge-ui.js 'ctm-details'
  // with the lodge's own item rules (disabled(i)); same menu states, help texts and frame as 154rider_details.
  drawLodgeDetails(c, b, disabled = () => false) {
    const lui = this.lodgeLui, model = this.lodgeModel; if (!lui || !model) return false;
    const now = this.now(), ui = this.ui;
    if (this.lodgeScreen !== ui.screen) { this.lodgeScreen = ui.screen; this.lodgeEnter = ui.careerUI?.lodgeFlash?.introStart?.(now) ?? now; }
    if (this.lodgeIndex !== ui.index) { this.lodgeIndex = ui.index; this.lodgeFocus = now; }
    const frame = now - this.lodgeEnter, out = [], focusStart = Math.max((this.lodgeFocus ?? now) - this.lodgeEnter, pv('lodgeFlash') ? introEnd(lui.screen, model) + (pv('introLead') ? FOCUS_LAG : 0) : 0);
    for (const ev of lui.screen.events) {
      if (ev.frame <= model.intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (ev.frame === model.frames[ui.index] && frame >= focusStart) out.push({ ev, start: focusStart });
    }
    const snow = this.data.screens.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    const items = new Map(model.items.map((n, i) => [n, i])), base = ui.characterSelect?.base || ui.rider;
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    lui.draw(c, out, frame, (e) => {
      if (items.has(e.name) && disabled(items.get(e.name))) return { alpha: pv('lodgeFlash') ? Math.min(DISABLED_ALPHA, lui.props(e, out, frame)[13] ?? 255) : DISABLED_ALPHA };   // not above its intro's fade (pv lodgeFlash)
      if (e.name === '0c485b75') return { text: base?.name || '' };
      if (e.name === '0a7f2263' || e.name === '03253174') return { alpha: 255 };   // frame and help group start at A=0 in this screen
      return null;
    });
    c.restore();
    return true;
  }

  // The career lodge's menu (FE.LUI 28lodge, cFEStateLodge; PS2 menus/ctm/62-lodge, 63-lodge-cursor-*): drawn for web/career-ui.js
  // 'ctm-lodge' (pv lodgeLui) with the lodge's item rules, the title 'Lodge - Peak N' and the LUI's own help lines. `still`: the menu under
  // a popup (its Save / Quit questions), at rest with the popup's row focused, no intro replay.
  drawLodgeMenu(c, b, { index = this.ui.index, title = null, disabled = () => false, still = false } = {}) {
    const lui = this.lodgeMenuLui, model = this.lodgeMenuModel; if (!lui || !model) return false;
    const now = this.now();
    if (!still && this.lodgeMenuScreen !== this.ui.screen) { this.lodgeMenuScreen = this.ui.screen; this.lodgeMenuEnter = this.ui.careerUI?.lodgeFlash?.introStart?.(now) ?? now; }
    if (!still && this.lodgeMenuIndex !== index) { this.lodgeMenuIndex = index; this.lodgeMenuFocus = now; }
    if (still) this.lodgeMenuScreen = null;
    const frame = still ? 1000 : now - this.lodgeMenuEnter, focusStart = still ? 0 : Math.max((this.lodgeMenuFocus ?? now) - this.lodgeMenuEnter, pv('lodgeFlash') ? introEnd(lui.screen, model) + (pv('introLead') ? FOCUS_LAG : 0) : 0), out = [];
    for (const ev of lui.screen.events) {
      if (ev.frame <= model.intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (ev.frame === model.frames[index] && frame >= focusStart) out.push({ ev, start: focusStart });
    }
    const snow = this.data.screens.bg_snow_loop?.events || [], sf = (now - (this.lodgeMenuEnter ?? now)) % 600;
    for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    const items = new Map(model.items.map((n, i) => [n, i]));
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    lui.draw(c, out, frame, (e) => {
      if (items.has(e.name) && disabled(items.get(e.name))) return { alpha: pv('lodgeFlash') ? Math.min(DISABLED_ALPHA, lui.props(e, out, frame)[13] ?? 255) : DISABLED_ALPHA };
      if (e.name === '0bd54c95' && title) return { text: title };
      if (e.name === '00000687') return { alpha: 255 };   // the backdrop group (white-to-blue gradient, mountain) starts at A=0; the PS2 shows it
      return null;
    });
    c.restore();
    return true;
  }

  // The lodge's Yes / No questions (Save progress? / Quit to Title screen? / Save progress before quitting?): the FE popup (FE.LUI popup)
  // over the lodge, sized to the question as the PS2 draws it (menus/ctm/64-lodge-quit-confirm: box 135..500 x 68..205, the question
  // at y 92, Yes / No at 143 / 163) with its full-screen veil 0279dba7 over the menu. layout: {dx, dy, sx, sy, message, menu} (tests).
  drawLodgePrompt(c, { message, index = 0, layout = LODGE_POPUP } = {}) {
    // pv fePopup: the lodge's questions are cFEPopup type 1, compact (PS2 lodge-quitprompt / lodge-saveprompt: the live popup object's
    // +0x260 300 x 102.4, scale 0.667 x 0.576), laid out and drawn by web/fe-popup.js; each question fades its veil in from its first frame
    if (pv('fePopup') && this.data?.screens?.popup) {
      if (!this.fePopupLui) { this.fePopupLui = new LuiScreen(this.data.screens.popup, this.images, this.ui); this.fePopupLui.shapeScale = true; }
      const now = this.now(), seen = this.fePopupSeen;
      if (!seen || seen.message !== message || now - seen.drawn > 2) this.fePopupSeen = { message, at: now, drawn: now }; else seen.drawn = now;
      c.save(); c.scale(1, SY);
      drawFePopup(c, this.fePopupLui, this.ui.fonts?.FEFONT || {}, { message, options: [this.t('yes', 'Yes'), this.t('no', 'No')], index, frame: now - this.fePopupSeen.at, animated: true });
      c.restore();
      return true;
    }
    const lui = this.popupLui; if (!lui) return false;
    const focus = index === 0 ? 30 : 35, events = lui.screen.events.filter((ev) => ev.frame === focus).map((ev) => ({ ev, start: 0 }));
    const frameParts = new Set(['065adea5', '09797985', '02bb75c5', '0ecd9825', '059e7b67']), L = layout;
    c.save(); c.scale(1, SY);
    lui.draw(c, events, 100, (e) => {
      if (e.name === '00376566') { const q = lui.props(e, [], 0); return { props: { 0: (q[0] || 0) + L.dx, 1: (q[1] || 0) + L.dy } }; }
      if (frameParts.has(e.name)) { const q = lui.props(e, [], 0), o = {}; for (let k = 0; k < (e.shape?.[0] ?? 4); k++) { o[21 + 9 * k] = (q[21 + 9 * k] || 0) * L.sx; o[22 + 9 * k] = (q[22 + 9 * k] || 0) * L.sy; } return { props: o }; }
      if (['0df0ab27', '0078ba87', '05cbab62'].includes(e.name)) return { hidden: true };
      if (e.name === '0279dba7') { const o = {}; for (let k = 0; k < 4; k++) { o[26 + 9 * k] = L.veil[3]; o[27 + 9 * k] = L.veil[0]; o[28 + 9 * k] = L.veil[1]; o[29 + 9 * k] = L.veil[2]; } return { props: o }; }
      if (e.name === '07c54234') return { text: message, props: { 0: L.message[0], 1: L.message[1], 12: 17 } };
      if (e.name === '0c583950' || e.name === '09cbb693') return { props: { 0: L.menu[0], 1: L.menu[1] } };
      if (e.name === '067b06b0') return { text: this.t('yes', 'Yes'), props: { 13: 255, 14: index === 0 ? 255 : 0, 15: index === 0 ? 255 : 0, 16: index === 0 ? 255 : 0 } };
      if (e.name === '067b06b1') return { text: this.t('no', 'No'), props: { 13: 255, 14: index === 1 ? 255 : 0, 15: index === 1 ? 255 : 0, 16: index === 1 ? 255 : 0 } };
      if (['067b06b2', '067b06b3', '067b06b4', '0c55e7c5', '07c59864', '0121a25c', '0344d188', '046556e4'].includes(e.name)) return { hidden: true };
      return null;
    });
    c.restore();
    return true;
  }

  drawKeyboard(c, now) {
    const k = this.keyboard, lui = this.kbLui; if (!lui) return;
    const screen = lui.screen, frame = now - k.at;
    const events = this.kbEvents();
    const shapes = this.keyShapes(), focusName = shapes[k.row]?.[k.col]?.name;
    const labelEls = new Set(screen.elements.filter((e) => e.kind === 'text' && e.layer === 20 && !['0c7608f5', '07577ff4', '0978e014', '03277174'].includes(e.name)).map((e) => e.name));
    const glyphs = this.ui.fonts?.FEFONT || {}, caretX = [...k.text.slice(0, k.caret)].reduce((w, ch) => w + (glyphs[ch]?.advance || 10), 0) * 0.79 * 0.6 * 1.25;
    lui.draw(c, events, frame, (e) => {
      if (labelEls.has(e.name)) return { hidden: true };                           // key labels: drawn below
      if (e.name === '0c7608f5') return { text: k.text };
      if (e.name === '07a2ec42') { const p = lui.props(e, events, frame); return { props: { 0: (p[0] || 0) + caretX } }; }   // the caret under the next character
      if (['00928f83', '034e0b40', '02e54c54', '0e5e85b4'].includes(e.name)) return { hidden: true };   // alternate row, "Show help", title arrows: not on the PS2 frames
      if (pv('cheat') && (e.name === '07a151f0' || e.name === '07a165b7')) return { hidden: true };   // back_exp / back_reg: cKeyboardPopup 0x1CD348 shows only the compact mode's dimming shape back_com (07a1575d, alpha 153, frame 45); the three start hidden (flags bit 0x40), and drawing all three dimmed the screen behind 1 - 0.4^3
      if (e.name === '03aa3aa0' || e.name === '0a3bd19e') return { alpha: 100, props: { 14: 0, 15: 0, 16: 0 } };   // Up / Down arrow keys (disabled): a dark arrow, the key x 0.61 (PS2 frame 40-enter-cheat-keyboard: 17,57,66 on 28,94,109)
      if (e.name === focusName) { const p = {}; for (let v = 0; v < 6; v++) { p[26 + 9 * v] = 255; } return { props: p }; }
      return null;
    });
    // key labels (the code writes them into the LUI text slots): centred on each key, dimmed when disabled
    shapes.forEach((row, r) => row.forEach((s, i) => {
      const label = KEY_ROWS[r]?.[i]; if (!label) return;
      const text = { Up: '', Down: '', Left: '', Right: '' }[label] ?? (/^[a-z]$/.test(label) && (k.caps !== k.shift) ? label.toUpperCase() : label);
      const off = this.keyOff(label);
      lui.text(c, text, s.x + s.w / 2, s.y + 3, { 9: 60, 12: 17, 14: 255, 15: 255, 16: 255 }, off ? 0.35 : 1);
    }));
  }
}
