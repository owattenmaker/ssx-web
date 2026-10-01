// The career lodge's Buy Attributes (cFEStateBuyAttrib, state 0x29, vtable 0x473908; FE.LUI 33buyattribs), buyAttribs.
// docs/career-events.md "Buy Attributes (cFEStateBuyAttrib)" has the spec with its addresses and the PS2 runs that confirm it.
//
// The screen keeps its own copy of the rider (constructor 0x1F4728): raw[7] (+0x4C, the profile bytes read through 0x148158),
// level[7] (+0x68, raw / 5), pending[7] (+0x84, raw points not bought yet), total (+0xA0) and bank (+0xA4, the cash on entry).
// One Right is ONE raw point (+0.2 level) at 0x150E50 = 0x440550[level - 1] ($250 .. $5,000 a point, 5 points a level), and the
// pending points stop at the next whole level (0x1F4DA8). Cross opens the buy popup; its Yes buys every pending point through
// 0x150C20 (cash - cost, byte + 1, runtime bank 0x535538 synced by 0x148098) and the screen stays open with the new values.
// Nothing is saved by the screen (the econ interface's +0xC call is the empty 0x15C7D8; the port autosaves as everywhere).
import { LuiScreen } from './lui-player.js';
import { menuModel } from './fe-screens.js';
import { money } from './trick-hud.js';
import { originalAttributeBytes } from './career.js';
import { LuiFlash, FLASH_IN, FLASH_OUT, flashAlpha } from './lui-flash.js';   // the lodge's shared TransitionOut flash

export const RAW_MAX = 55, LEVEL_MAX = 11;
export const BAR_UNIT = Math.fround(37 / 11);            // gp-0x554C (3.3636): bar pixels per raw point, 185 px at raw 55
export const BAR_HEIGHT = 12;
export const levelOf = (raw) => Math.trunc(raw / 5);
// lvl%d (updateLevels 0x1F4EA8): "%d.%d" of raw / 5 and (raw % 5) * 2 -- the bought value; pending points never show here
export const levelText = (raw) => `${levelOf(raw)}.${(raw - 5 * levelOf(raw)) * 2}`;
// 0x150E50(level - 1): the price of one raw point at a whole level (table 0x440550, stride 8, value +4)
export const pointCost = (costs, level) => (level >= 1 && level <= costs.length ? costs[level - 1] : null);

// The screen's rules, row order = the menu's (0x4780B0: Acceleration, Edging, Speed, Spin, Stability, Toughness, Tricks).
export class BuyAttribSession {
  constructor(raw, cash, costs) {
    this.raw = raw.slice(); this.level = this.raw.map(levelOf); this.pending = this.raw.map(() => 0);
    this.total = 0; this.bank = cash; this.costs = costs;
  }
  cost(row) { return pointCost(this.costs, this.level[row]); }
  // Right (0x1F4D28): 'error' (kind 4) at level 11 or when the bank cannot pay total + cost; 'none' (silent) at the next
  // whole level; 'add' (frame 'hl right', kind 1) otherwise
  right(row) {
    const cost = this.cost(row);
    if (this.level[row] >= LEVEL_MAX || cost == null) return 'error';
    if (this.bank < this.total + cost) return 'error';
    if (this.raw[row] + this.pending[row] >= (this.level[row] + 1) * 5) return 'none';
    this.pending[row]++; this.total += cost; return 'add';
  }
  // Left (0x1F4CB8): a pending point back ('hl left', kind 1), else kind 4
  left(row) {
    if (this.pending[row] <= 0) return 'error';
    this.pending[row]--; this.total -= this.cost(row); return 'remove';
  }
  // Cross (0x1F4AC8): the buy popup, or kind 4 with nothing to pay or too little cash
  cross() { return this.bank < this.total || this.total <= 0 ? 'error' : 'popup'; }
  // The popup's Yes (0x1CAF00 -> ev 0x16 -> 0x1F5300): buyPoint(row) once per pending point (0x150C20 on the profile), then
  // the screen's copy moves on and every display refreshes
  confirm(buyPoint) {
    this.bank -= this.total; this.total = 0;
    this.pending.forEach((n, row) => { for (let k = 0; k < n; k++) buyPoint(row); });
    this.pending.forEach((n, row) => { this.raw[row] += n; this.pending[row] = 0; this.level[row] = Math.max(this.level[row], levelOf(this.raw[row])); });
  }
  // What the rows show: the level text, the cost column (hidden at level 11, 0x1F5060), the orange bought bar Vector%d and the
  // light pending bar under%d (updateExperienceDisplay 0x1F50B0: widths raw x 3.36 and (raw + pending) x 3.36, height 12)
  view(row) {
    const raw = this.raw[row], cost = this.level[row] >= LEVEL_MAX ? null : this.cost(row);
    return { level: levelText(raw), cost: cost == null ? null : money(cost), vector: raw * BAR_UNIT, under: (raw + this.pending[row]) * BAR_UNIT };
  }
}

// 0x150C20 on the port's career record (web/career.js buyAttributePoint): the byte's own level prices the point.
export function buyPoint(career, id, row) { return career.buyAttributePoint(id, row); }

// The attribute bytes of the run about to start (main.js startRun, buyAttribs): the runtime bank 0x535538 always holds the
// profile's bytes (0x148098 after every purchase and load), and the stat getters (0x1494C0 ..) read it live as int(raw / 5) / 11,
// so free rides and events alike ride with the bought levels. Online: the defaults every client simulates. Else null (unchanged).
export function runAttributes(ui, riderId) {
  if (ui?.onlineMode) return Array(7).fill(5);
  const rows = ui?.careerMode ? ui.careerUI?.career?.save?.riders?.[riderId]?.attributes : null;
  return Array.isArray(rows) && rows.length === 7 ? originalAttributeBytes(rows) : null;
}

const SY = 448 / 480, FPS = 60;
// TransitionOut (33buyattribs label frame 75, 28lodge frame 85: control 0x30 plays screen 0x00AB3C45 transition_flash, anim 02ed6393):
// a white quad over everything, A 0 -> 255 in 10 frames over the old screen, then the new screen (its intro from frame 0) under
// A 255 -> 0 in 9 frames. PS2 lodge/attrs/ba5: Triangle at sample 22, full white at 32, the lodge's intro under 0.79 white at 34.
export { FLASH_IN, FLASH_OUT, flashAlpha };   // web/lui-flash.js (the one TransitionOut flash of every lodge screen)
// The buy popup's veil: 139buy_popup (FE.LUI, cUIStateBuyPopup 0x1CAC30) shape 0885e124, layer 13, anim 0f5016c0: a 642 x 481
// quad, top (111,177,210) -> bottom (200,225,238), A 0 -> 175 in 8 frames. The GS alpha is 175 >> 1 = 87/128: fit over the
// PS2 frames ba1 speed-hold / popup outside the box: A 0.681, top (109.5,175.5,208.6), bottom (199.6,224.3,237.2).
export const POPUP_VEIL = Object.freeze({ top: [111, 177, 210], bottom: [200, 225, 238], height: 481, alpha: 87 / 128, frames: 8 });
const LVL = [0, 1, 2, 3, 4, 5, 6].map((k) => (0x73cf0 + k).toString(16).padStart(8, '0'));   // lvl0..6 (00073cf0..)
const COST = [0, 1, 2, 3, 4, 5, 6].map((k) => (0x6a6a70 + k).toString(16).padStart(8, '0'));  // cost0..6
const VECTOR = ['0cbab6f0', '0cbab6f1', '0cbab6f2', '0cbab6f3', '0cbab6f4', '0cbab6f5', '0cbab6f6'];
const UNDER = ['07c4ac50', '07c4ac51', '07c4ac52', '07c4ac53', '07c4ac54', '07c4ac55', '07c4ac56'];
const TOTAL = '0ddeb8a4', BANK = '0ccd9549';                        // AttributeCost, CurrentMoney
const HL = { left: { frame: 90, element: '0883c100' }, right: { frame: 95, element: '08386d40' } };   // 'hl left' / 'hl right'
const rect = (w, h) => ({ 21: 0, 22: 0, 30: w, 31: 0, 39: w, 40: h, 48: 0, 49: h });

// The screen for web/career-ui.js 'ctm-attributes'.
export class BuyAttribs {
  constructor(cu) { this.cu = cu; this.ui = cu.ui; this.session = null; this.popup = null; this.lui = null; this.model = null; }
  now() { return performance.now() * FPS / 1000; }
  load() {
    if (this.lui) return true;
    const cs = this.ui.characterSelect, data = cs?.data, screen = data?.screens?.['33buyattribs'];
    if (!screen) return false;
    const snow = data.screens.bg_snow_loop;
    const merged = snow
      ? {
          ...screen,
          elements: [...screen.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))],
          animations: { ...screen.animations, ...snow.animations }
        }
      : screen;
    this.lui = new LuiScreen(merged, cs.images || {}, this.ui); this.model = menuModel(screen);
    this.lui.unionFlat = true;  // the flat bars (Vector / under / background) as one path: no diagonal seam between their two triangles
    this.lui.flagWrap = true;   // only flag-0x80 texts wrap (the row help); 'You have:', the costs and the money stay on one line as on the PS2
    return !!this.model;
  }
  get career() { return this.cu.career; }
  get riderId() { return this.cu.riderId; }
  open() {
    const r = this.cu.me;
    this.session = new BuyAttribSession(r.attributes, r.cash, this.career.rules.attribute_cost);
    this.popup = null; this.hl = null; this.enterAt = this.cu.lodgeFlash?.introStart?.(this.now()) ?? this.now(); this.focusAt = this.enterAt; this.focusRow = 0;
  }
  sfx(name) { try { this.ui.audioMenus?.sfx?.(name, 'ctm-attributes'); } catch {} }
  // Triangle (0x1F4B64): TransitionOut, then the lodge (its intro again, Buy Attributes focused); pending dropped
  leave() { this.transition(() => { this.session = null; this.popup = null; const ui = this.ui; ui.set('ctm-lodge'); ui.index = 3; ui.sync(); }); }
  // The lodge's Buy Attributes item: the lodge's TransitionOut (28lodge frame 85), then this screen from its intro (PS2 ba5 Cross)
  enter() { this.transition(() => { this.open(); this.ui.set('ctm-attributes'); }); }
  // the lodge's shared flash (web/career-ui.js lodgeGo: cursor memory, the other lodge screens' changes); without the
  // career UI (tests) its own LuiFlash on this clock
  get flash() { return this.cu.lodgeFlash?.flash ?? this.ownFlash?.flash ?? null; }
  transition(to) { if (this.flash) return; if (this.cu.lodgeGo) { this.cu.lodgeGo(to); return; } (this.ownFlash ??= new LuiFlash(() => this.now())).go(to); }
  // Drawn after the lodge / this screen (web/career-ui.js draw): the switch happens at full white (the shared flash is career-ui's own draw)
  drawFlash(c) { this.ownFlash?.draw(c); }

  key(e) {
    const ui = this.ui, s = this.session; if (!s) return false;
    if (this.flash) { e.preventDefault(); return true; }   // no input while the transition runs
    const code = e.code;
    if (this.popup) {                                   // the buy popup: Up / Down wrap, Cross picks, Triangle does nothing (PS2 ba1)
      if (['ArrowUp', 'ArrowDown', 'Escape', 'ArrowLeft', 'ArrowRight'].includes(code)) e.preventDefault(); else return false;   // Enter: ui.choose
      if (code === 'ArrowUp' || code === 'ArrowDown') { ui.index = 1 - ui.index; ui.sync(); }   // the move sound: web/audio-menu.js watchSounds
      ui.draw(ui.lastState); return true;
    }
    if (code === 'ArrowLeft' || code === 'ArrowRight') {   // UILeft / UIRight = DPadL/R.repeat (input.map): held keys repeat
      e.preventDefault();
      const row = ui.index, out = code === 'ArrowRight' ? s.right(row) : s.left(row);
      if (out === 'add' || out === 'remove') { this.hl = { side: code === 'ArrowRight' ? 'right' : 'left', at: this.now(), row }; this.sfx('move'); }
      else if (out === 'error') this.sfx('error');
      ui.draw(ui.lastState); return true;
    }
    if (code === 'ArrowUp' || code === 'ArrowDown') {       // AttribMenu flags 0x2C0: wraps (PS2 ba2 'wrap': Up on Acceleration -> Tricks)
      e.preventDefault(); ui.index = (ui.index + (code === 'ArrowUp' ? 6 : 1)) % 7; ui.sync(); return true;
    }
    if (code === 'Escape') { e.preventDefault(); if (!e.repeat) this.leave(); return true; }   // Triangle: back, pending dropped
    return false;
  }
  cross() {
    const s = this.session; if (!s || this.popup) return;
    if (s.cross() === 'error') { this.sfx('error'); return; }
    // Yes focused (PS2 r3-attrs f600); while it is up the nav buttons are its Yes / No (items, layout), ui.index its cursor
    this.popup = { at: this.now(), row: this.ui.index }; this.ui.index = 0; this.ui.sync();
  }
  choose(i) { if (this.flash) return; if (this.popup) this.choosePopup(i); else this.cross(); }
  choosePopup(i = this.ui.index) {
    const s = this.session, row = this.popup.row; this.popup = null;
    if (i === 0) { s.confirm((k) => buyPoint(this.career, this.riderId, k)); this.career.persist(); }
    this.ui.index = row; this.focusRow = row; this.ui.sync();
  }
  items(rows) { return this.popup ? [this.cu.t('kT_CMNYes', 'Yes'), this.cu.t('kT_CMNNo', 'No')] : rows; }
  // Nav buttons (ui.js sync): the rows of AttribMenu (text at menu (50,120) + (153, 27 + 20 i), 20 high, in 480 lines)
  layout(i) { return this.popup ? [360, 283 + 20 * i, 110, 20] : [24, (147 + 20 * i) * SY, 440, 20 * SY]; }   // popup: Yes / No (448 lines)
  events(frame, row) {
    const lui = this.lui, model = this.model, ui = this.ui, out = [];
    if (this.focusRow !== row) { this.focusRow = row; this.focusAt = this.now(); if (this.hl?.row !== row) this.hl = null; }
    const focusStart = this.focusAt - this.enterAt;
    for (const ev of lui.screen.events) {
      if (ev.frame <= model.intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (ev.frame === model.frames[row]) out.push({ ev, start: focusStart });
    }
    if (this.hl) { const h = HL[this.hl.side]; for (const ev of lui.screen.events) if (ev.frame === h.frame && ev.element === h.element) out.push({ ev, start: this.hl.at - this.enterAt }); }
    const snow = ui.characterSelect?.data?.screens?.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    return out;
  }
  draw(c, b) {
    const s = this.session; if (!s || !this.load()) return false;
    const now = this.now(), frame = now - this.enterAt, lui = this.lui, rows = s.raw.map((_, k) => s.view(k)), row = this.popup ? this.popup.row : this.ui.index;
    const at = (list, name) => list.indexOf(name);
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    lui.draw(c, this.events(frame, row), frame, (e) => {
      let k;
      if ((k = at(LVL, e.name)) >= 0) return { text: rows[k].level };
      if ((k = at(COST, e.name)) >= 0) return rows[k].cost == null ? { hidden: true } : { text: rows[k].cost };
      if ((k = at(VECTOR, e.name)) >= 0) return { props: rect(rows[k].vector, BAR_HEIGHT) };
      if ((k = at(UNDER, e.name)) >= 0) return { props: rect(rows[k].under, BAR_HEIGHT) };
      if (e.name === TOTAL) return { text: money(s.total) };
      if (e.name === BANK) return { text: money(s.bank) };
      return null;
    });
    c.restore();
    if (this.popup) this.drawPopup(c);
    return true;
  }
  // cUIStateBuyPopup (0x1CABA8 / 0x1CAFC0): item type 'Buy attributes?' (kT_OVRCMNBuyAttributesColon), blank item name and
  // question, 'Cost:' money(total), 'You have:' money(bank); the lodge's buy popup layout (web/fe-screens.js drawPrompt)
  drawPopup(c) {
    const fe = this.ui.feScreens; if (!fe?.drawPrompt) return;
    const t = (k, f) => this.cu.t(k, f), s = this.session;
    // at: the popup's own intro plays from the Cross (with the 139buy_popup screen; the 'popup' fallback is drawn at rest)
    const prompt = { at: fe.buyPopupLui ? this.popup.at : this.popup.at - 40, index: this.ui.index, message: '', buy: { title: t('kT_OVRCMNBuyAttributesColon', 'Buy attributes?'), name: '',
      rows: [[t(0x05f1a304, 'Cost:'), money(s.total)], [t(0x0918c5a5, 'You have:'), money(s.bank)]] } };
    // The veil is the popup screen's own 0885e124 (drawPrompt draws FE.LUI 'popup', whose 0885e124 / anim 00438b41 is the same quad as
    // 139buy_popup's: POPUP_VEIL); nothing is added here.
    const saved = fe.prompt; fe.prompt = prompt;
    try { fe.drawPrompt(c, fe.now()); } finally { fe.prompt = saved; }
  }
}
