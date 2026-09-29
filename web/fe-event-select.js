// The original Single Event track selector: Select Peak -> Select Mode -> Select Event (FE.LUI 'Map' over
// 'femap_template', tools/export_fe_menus.py -> UI/fe-menus.json), played by web/lui-player.js.
// docs/characters.md "Single Event track selector".
//
//   fe-peak   Select Peak    SP_PeakList: Peak 3 / Peak 2 / Peak 1 (All Mountain hidden), the whole-mountain picture
//                            (MAPGFX_map_mtn) with the focused peak's outline, locks on the peaks without a pass
//   fe-mode   Select Mode    SPG_Peak<n>Goals: Race / Freestyle (Freeride and Earnings are career goals), the peak
//                            map (MAPGFX_map_peak<A|B|C>) with every route of the focused mode red, the rest orange
//   fe-event  Select Event   Peak<n>RaceLocations / Peak<n>FreestyleLocations: the events, the focused route red with
//                            the start indicator (the Map state of that course), its type on the map tab and the
//                            original help text (kT_HELP<code>); Square toggles the INFO table (Run / Event / Medal /
//                            Top time or score: the SE_Info widgets the MCOMM Transport shows) and the medal column
//                            shows the rider's best medal from the career save
//
// Every ported peak opens (Peak 1; Peak 2 docs/peak2.md; Peak 3 docs/peak3.md); a peak that is not would stay as in the
// original, locked: Cross does nothing and the help reads "LOCKED:  Get a Peak N pass in Conquer the Mountain mode."
// (kT_10HELPUnlockPeakN).
// The last peak / mode / event is remembered (web/save-store.js selection) so a returning player lands on it.
import { LuiScreen } from './lui-player.js';
import { pv } from './pv-flags.js';
import { MEDAL, MEDAL_NAMES, isTimed } from './career.js';
import { raceTime } from './race-time.mjs';
import { loadSelection, saveSelection } from './save-store.js';

const ROOT = '/assets/UI/', CAREER = '/assets/CAREER/';
const SY = 448 / 480, FPS = 60, FLASH = 10, DISABLED_ALPHA = 128;
export const EVENT_SCREENS = ['fe-peak', 'fe-mode', 'fe-event'];
export const MODES = ['race', 'freestyle'];
// Map widget positions (480-line LUI space): list rows at SP_PeakList / SPG / Locations (202,103) + group (52,80),
// 25 px apart, right-aligned 20 px left of the menu; the focus bar (phl / pghl / ehl) at (40, 103 + 25 i) in the group.
const ROW0 = 183, ROW = 25;
// Route colours: the LUI paths carry the focus red (227,42,9); the code turns the others orange (PS2 frames
// menus/single/05-select-mode.png, 06-race-events.png: (222,42,6) and (216..222,126..132,30)).
export const PATH_RED = [227, 42, 9], PATH_ORANGE = [224, 134, 32];
export const PATHS = { ARA1: 'ara1path', BRA2: 'bra0path', ABC1: 'abc1path', ASS1: 'ass1path', ABA1: 'aba1path', BHP1: 'bhp1path',
  CRA3: 'cra3path', DRA4: 'dra4path', DBC2: 'dbc2path', DSS2: 'dss2path', CBA2: 'cba2path', CHP2: 'chp2path',
  ERA5: 'era5path', EBC3: 'ebc3path', ESS3: 'ess3path', EBA3: 'eba3path', EHP3: 'ehp3path' };
// Map states that put the start indicator on a course (frames 200..490, matched by the indicator position against
// each route's first vertex; 210/250, 320/350 and 430 are the base-station markers, not in Single Event).
export const INDICATOR_STATE = { ABC1: 200, ASS1: 220, ARA1: 230, ABA1: 240, BRA2: 260, BHP1: 270, 'peak1-race': 280, 'peak1-jam': 290,
  DBC2: 310, DSS2: 330, DRA4: 340, CBA2: 360, CRA3: 370, CHP2: 380, 'peak2-race': 390, 'peak2-jam': 400,
  EBC3: 420, ERA5: 440, EBA3: 450, EHP3: 460, ESS3: 470, 'peak3-race': 480, 'peak3-jam': 490 };
const EVENT_TAB = { race: ['kT_cmnRaceCaps', 'Race'], slopestyle: ['kT_cmnSlopeCaps', 'Slopestyle'], bigair: ['kT_cmnBigAirCaps', 'Big Air'],
  superpipe: ['kT_cmnPipeCaps', 'Super Pipe'], backcountry: ['kT_CMNBackCountry', 'Backcountry'] };
const MEDAL_CELL = { [MEDAL.GOLD]: [203, 0, 52, 42], [MEDAL.SILVER]: [203, 40, 52, 42], [MEDAL.BRONZE]: [203, 80, 52, 42], [MEDAL.PLATINUM]: [203, 120, 52, 42] };

// ---- pure helpers (web/test-fe-event-select.mjs) ----

// The Single Event list of a peak and mode, in the original goal-list order (0x45AAD8): the standard events, the
// rival run (Happiness / Happiness Jam, the courses.json rivalMode entries), then the peak event. Entries without a
// ported location stay listed but not playable; the peak events never (Conquer the Mountain only, as on the PS2).
// `order`: the Single Event rows of the Map LUI (Peak<n><Mode>Locations, their Map states -> course codes / peak keys,
// mapRowOrder below). Peak 2 Freestyle lists Style Mile, Launch Time, Schizophrenia (PS2 local/ps2-capture/nav/p2/
// out-lists/p2-freestyle-events.png) while its goal list is Launch Time, Schizophrenia, Style Mile; on Peak 1 they agree.
export function eventList({ rules, courses: careerCourses, playable = [], peak, mode, career = null, riderId = null, order = null }) {
  const goal = rules?.goal_events?.find((g) => g.peak === peak && g.goal === mode);
  if (!goal) return [];
  const rank = (e) => { const k = order ? order.indexOf(e.code ?? e.key) : -1; return k < 0 ? Infinity : k; };
  return goal.events.map((e, i) => {
    const c = e.course != null ? careerCourses[e.course] : null;
    if (e.mode >= 6) {   // the peak runs (web/peak-run.js): Conquer the Mountain only
      // PS2 (docs/peak3.md section 6 "Single Event"): the Map LUI's Single Event rows include them (states 280..490), but the
      // Select Event lists never show them, even with every lock word cleared (+0x278), every peak-run medal record set and
      // every award bit (+0xF28) set (local/ps2-capture/allpeak/nav/out-single-medalled, out-single-awarded; Peak 1 / 2 lists
      // in menus/single and nav/p2/out-lists). They are played from the career's Transport lists (web/career-ui.js).
      const open = false;
      return { mode: e.mode, course: null, code: null, key: `peak${peak}-${e.mode >= 9 ? 'jam' : 'race'}`, name: ['Peak 1 Race', 'Peak 2 Race', 'All Peak Race', 'Peak 1 Jam', 'Peak 2 Jam', 'All Peak Jam'][e.mode - 6],
        kind: 'peak', entry: null, hidden: !open, playable: open };
    }
    const rival = e.mode === 4 || e.mode === 5;
    const entry = playable.find((x) => x.code === c.code && (rival ? x.rivalMode === e.mode : x.rivalMode == null)) || null;
    return { mode: e.mode, course: e.course, code: c.code, key: rival ? `${c.code}:${e.mode}` : c.code, name: rival && e.mode === 5 ? `${c.short || c.name} Jam` : c.name,   /* short name: 'Happiness Jam', 'Throne Jam' (PS2 lists) */
      kind: rival ? 'backcountry' : c.kind, entry, hidden: false, playable: !!entry && entry.ready !== false };
  }).map((x, i) => ({ x, i })).sort((a, b) => (rank(a.x) - rank(b.x)) || a.i - b.i).map(({ x }) => x).filter((x) => !x.hidden);
}
// The Single Event row order of a peak and mode from the Map LUI: the rows' first words are the Map states that put the
// start indicator on their course (INDICATOR_STATE), so they name the course (or peak run) of each row.
export function mapRowOrder(mapScreen, peak, mode) {
  const label = `Peak${peak}${mode === 'race' ? 'Race' : 'Freestyle'}Locations`;
  const menu = mapScreen?.elements?.find((e) => e.label === label);
  if (!menu?.children) return null;
  const byState = new Map(Object.entries(INDICATOR_STATE).map(([k, v]) => [v, k]));
  const byName = new Map(mapScreen.elements.map((e) => [e.name, e]));
  return menu.children.map((n) => byState.get(byName.get(n)?.words?.[0])).filter(Boolean);
}
// Routes drawn red: the focused event's course, or on Select Mode every standard course of the mode (the backcountry
// belongs to Race there, PS2 05-select-mode / 05b-mode-down1).
export function redPaths(list, { screen, mode, index }) {
  if (screen === 'fe-event') { const e = list[index]; return new Set(e?.code ? [e.code] : []); }
  if (screen === 'fe-mode') return new Set(list.filter((e) => e.code && (mode === 'race' || e.kind !== 'backcountry')).map((e) => e.code));
  return new Set();
}
// Peaks whose events are ported. Peak 2 (docs/peak2.md) and Peak 3 (docs/peak3.md) open in Single Event without the
// Conquer the Mountain pass the PS2 asks for (kT_10HELPUnlockPeak2/3): the port lets its ported events be played directly;
// a row whose location is not ported stays unplayable (eventList playable: false).
export const SINGLE_EVENT_OPEN_PEAKS = [1, 2, 3];
export function peakUnlocked(peak, { career = null, riderId = null, ported = SINGLE_EVENT_OPEN_PEAKS } = {}) {
  if (!ported.includes(peak)) return false;   // a peak without ported courses stays locked
  if (peak === 1 || peak === 2 || peak === 3) return true;
  return !!career?.rider(riderId)?.peaks?.[peak - 1];
}
// The help line keeps the Map HelpText element's own box and size (375 wide, 50%): that reproduces every PS2 wrap
// (local/ps2-capture/menus/single 04..07; a 400 / 56% override wrapped Select Peak's help before "mode.").
export const helpOverride = (text) => (pv('help') ? { text } : { text, props: { 6: 400, 9: 56, 10: 56 } });   // off: the old 400 / 56% box
export function stepIndex(index, direction, n) { return n ? ((index + direction) % n + n) % n : 0; }

// ---- screens ----
async function loadImage(src) { const im = new Image(); im.src = src; await im.decode(); return im; }

export class FeEventSelect {
  constructor(ui) {
    this.ui = ui; this.data = null; this.images = {}; this.flash = null; this.enterAt = {}; this.focusAt = 0; this.info = false;
    this.peak = 1; this.mode = 'race'; this.cursor = { 'fe-peak': 2, 'fe-mode': 0, 'fe-event': 0 };
    const s = loadSelection();
    if (s) { if ([1, 2, 3].includes(s.peak)) this.peak = s.peak; if (MODES.includes(s.mode)) this.mode = s.mode; this.eventKey = s.event || null; }
  }
  get ready() { return !!this.map; }
  now() { return performance.now() * FPS / 1000; }
  async load() {
    try {
      const data = await (await fetch(ROOT + 'fe-menus.json')).json();
      if (!data?.screens?.Map || !data.screens.femap_template) return false;
      const pages = [...new Set([...data.pages, 'FE_1-7', 'FE_1-11', 'FE_1-14', 'FE_1-18'])];
      await Promise.all(pages.map(async (p) => { this.images[p] = this.ui.characterSelect?.images?.[p] || await loadImage(ROOT + p + '.png'); }));
      await Promise.all(['MAPGFX_map_mtn', 'MAPGFX_map_peakA', 'MAPGFX_map_peakB', 'MAPGFX_map_peakC'].map(async (p) => { try { this.images[p] = await loadImage(CAREER + p + '.png'); } catch {} }));
      const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop;
      this.snow = snow?.events || [];
      const merge = (a) => (snow ? { ...a, elements: [...a.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000, snow: true }))], animations: { ...a.animations, ...snow.animations } } : a);
      this.data = data;
      this.template = new LuiScreen(merge(data.screens.femap_template), this.images, this.ui);
      this.map = new LuiScreen(data.screens.Map, this.images, this.ui);
      const by = this.map.byName, label = new Map(data.screens.Map.elements.filter((e) => e.label).map((e) => [e.label, e]));
      this.byLabel = label;
      // every element's ancestor chain (group or menu), to show whole widget groups per screen
      this.chain = new Map();
      for (const e of data.screens.Map.elements) { const up = []; let p = e.parent || e.menu; while (p && by.has(p)) { up.push(p); const q = by.get(p); p = q.parent || q.menu; } this.chain.set(e.name, up); }
      return true;
    } catch (error) { console.warn('Single Event selector unavailable', error); this.map = null; return false; }
  }
  owns(screen) { return this.ready && EVENT_SCREENS.includes(screen); }
  t(key, fallback = '') { return this.ui.careerUI?.t?.(key, fallback) || fallback; }
  el(label) { return this.byLabel?.get(label); }

  // ---- state ----
  get career() { return this.ui.careerUI?.career || null; }
  get riderId() { return this.ui.characterSelect?.base?.id || this.ui.rider?.id || 'zoe'; }
  get careerData() { return this.ui.careerUI?.data || null; }
  unlocked(peak) { return peakUnlocked(peak, { career: this.career, riderId: this.riderId }); }
  events(peak = this.peak, mode = this.mode) {
    const d = this.careerData;
    if (d) return eventList({ rules: d.rules, courses: d.courses, playable: this.ui.courses || [], peak, mode, career: this.career, riderId: this.riderId, order: mapRowOrder(this.data?.screens?.Map, peak, mode) });
    // no career tables: the courses.json events of Peak 1 by kind
    const list = (this.ui.courses || []).filter((c) => (c.peak ?? 1) === peak && (mode === 'race' ? (c.event === 'race' || c.rivalMode === 4) : (c.event !== 'race' && c.rivalMode !== 4)));
    return list.map((c) => ({ mode: c.rivalMode ?? (c.event === 'race' ? 0 : 1), course: null, code: c.code, key: c.rivalMode ? `${c.code}:${c.rivalMode}` : c.code, name: c.name, kind: c.event, entry: c, playable: c.ready !== false }));
  }
  items(screen = this.ui.screen) {
    if (screen === 'fe-peak') return [1, 2, 3].reverse().map((p) => this.t(`kT_Peak${p}name`, `Peak ${p}`));
    if (screen === 'fe-mode') return MODES.map((m) => (m === 'race' ? this.t('kT_cmnRaceCaps', 'Race') : this.t(0x0293c95c, 'Freestyle')));
    if (screen === 'fe-event') return this.events().map((e) => e.name);
    return [];
  }
  disabledList(screen = this.ui.screen) {
    if (screen === 'fe-peak') return [3, 2, 1].map((p) => !this.unlocked(p));
    if (screen === 'fe-event') return this.events().map((e) => !e.playable);
    return this.items(screen).map(() => false);
  }
  disabled(i) { return !this.ui.ready; }   // locked rows stay focusable (their help says why), like the PS2 peak list
  layout(i) { return [54, (ROW0 - 4 + ROW * i) * SY, 206, 24 * SY]; }

  enter(screen, from) {
    if (!this.owns(screen)) return;
    const now = this.now();
    if (from !== screen) this.enterAt[screen] = now;
    this.focusAt = now;
    if (screen !== 'fe-event') this.info = false;
    const n = this.items(screen).length;
    let i = this.cursor[screen] ?? 0;
    if (screen === 'fe-peak') i = 3 - this.peak;
    if (screen === 'fe-mode') i = Math.max(0, MODES.indexOf(this.mode));
    if (screen === 'fe-event' && this.eventKey) { const k = this.events().findIndex((e) => e.key === this.eventKey); if (k >= 0) i = k; }
    this.pendingIndex = Math.min(Math.max(0, i), Math.max(0, n - 1));
  }
  focus() {
    if (this.pendingIndex != null) { this.ui.index = this.pendingIndex; this.pendingIndex = null; }
    const n = this.items().length; if (this.ui.index >= n) this.ui.index = Math.max(0, n - 1);
    return this.ui.index;
  }
  go(screen, index = null) {
    this.flash = { at: this.now(), to: () => { this.ui.set(screen); if (index != null) { this.ui.index = index; this.ui.sync(); } } };
  }
  remember() { saveSelection({ peak: this.peak, mode: this.mode, event: this.eventKey }); }

  key(e) {
    const s = this.ui.screen; if (!this.owns(s)) return false;
    const codes = ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Enter', 'Space', 'Escape', 'ShiftLeft', 'ShiftRight'];
    if (codes.includes(e.code)) e.preventDefault();
    if (this.flash) return true;
    if (e.repeat && !['ArrowUp', 'ArrowDown'].includes(e.code)) return true;
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') { this.move(e.code === 'ArrowUp' ? -1 : 1); this.ui.sync(); return true; }
    if (e.code === 'Enter' || e.code === 'Space') { this.choose(this.ui.index); return true; }
    if (e.code === 'Escape') { this.back(); return true; }   // Triangle (touch deck: Escape)
    if ((e.code === 'ShiftLeft' || e.code === 'ShiftRight') && s === 'fe-event') { this.info = !this.info; return true; }   // Square: Show INFO / Show MAP
    return false;
  }
  move(direction) {
    const s = this.ui.screen, n = this.items(s).length;
    this.ui.index = stepIndex(this.focus(), direction, n); this.focusAt = this.now();
    if (s === 'fe-peak') this.peak = 3 - this.ui.index;
    if (s === 'fe-mode') this.mode = MODES[this.ui.index];
    if (s === 'fe-event') this.eventKey = this.events()[this.ui.index]?.key ?? null;
    if (s !== 'fe-peak') this.remember();   // the cursor itself is the returning player's 'last course' (a peak only once chosen)
  }
  choose(i) {
    const ui = this.ui, s = ui.screen; if (!ui.ready || this.flash) return;
    ui.index = i; this.focusAt = this.now();
    if (s === 'fe-peak') {
      const peak = 3 - i; if (!this.unlocked(peak)) return;   // locked: the help explains, nothing happens
      this.peak = peak; this.remember(); this.go('fe-mode'); return;
    }
    if (s === 'fe-mode') { this.mode = MODES[i] || 'race'; this.remember(); this.go('fe-event'); return; }
    if (s === 'fe-event') {
      const e = this.events()[i]; if (!e?.playable) return;
      this.eventKey = e.key; this.remember();
      this.flash = { at: this.now(), to: () => { if (e.entry) ui.startSingleEvent(e.entry); else if (e.mode >= 6) ui.careerUI?.singlePeak?.(e.mode); } };
    }
  }
  back() {
    const s = this.ui.screen; if (this.flash) return;
    if (s === 'fe-event' && this.info) { this.info = false; return; }
    if (s === 'fe-peak') this.go('setup', 0);
    else if (s === 'fe-mode') this.go('fe-peak');
    else if (s === 'fe-event') this.go('fe-mode');
  }

  // ---- drawing ----
  // Which Map widgets a screen shows (FE art, not the MCOMM 'pda' art; no confirm popup).
  visible(e, s) {
    const up = this.chain.get(e.name) || [], has = (label) => { const x = this.el(label); return x && (x.name === e.name || up.includes(x.name)); };
    if (has('pdaart') || has('ConfirmPopup') || has('SP_Info') || has('SP_GameInfo') || has('SPG_RaceFSEvents') || has('SPG_FreerideGoals') || has('SPG_EarningsGoals')) return false;
    if (has('feart') || has('HelpText')) return true;
    if (has('maptab') || (e.label === 'femaptab back')) return s === 'fe-event';
    if (e.label === 'MapPic' || e.name === '0c2f12c2') return true;
    if (e.label === 'PEAKno') return s !== 'fe-peak';
    if (has('SelectPeak')) return s === 'fe-peak' && !has('SP_PeakList Art') && !/you are here/.test(this.labelOf(up));
    if (has('SelectPeakGoal')) return s === 'fe-mode' && (has(`SPG_Peak${this.peak}Goals`) || e.label === 'pghl');
    if (has('SelectEvent')) {
      if (s !== 'fe-event') return false;
      if (has('SE_Info')) return this.info;
      if (has('Peak Locations Art')) return !this.info;
      return has(`Peak${this.peak}${this.mode === 'race' ? 'Race' : 'Freestyle'}Locations`) || e.label === 'ehl';
    }
    if (has('Locks')) return s === 'fe-peak';
    if (has('Indicators')) return s !== 'fe-peak' && !this.info && (has(`peak${this.peak - 1}indicators`) || (e.label === 'indicator' && s === 'fe-event'));
    return false;
  }
  labelOf(up) { return up.map((n) => this.map.byName.get(n)?.label || '').join('|'); }

  draw(c, b) {
    const ui = this.ui, s = ui.screen, now = this.now();
    if (this.flash && now - this.flash.at >= FLASH) { const f = this.flash; this.flash = null; f.to(); return; }
    if (this.enterAt[s] == null) this.enterAt[s] = now;
    this.focus();
    const frame = now - this.enterAt[s];
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    // femap_template: the orange '3' frame, its dashes (slide in over frames 1..20), the mountain art; the FE snow
    const tev = [], sf = frame % 600;
    for (const ev of this.template.screen.events) if (ev.frame <= frame) tev.push({ ev, start: ev.frame });
    for (const ev of this.snow) if (ev.frame <= sf) tev.push({ ev, start: frame - sf + ev.frame });
    this.template.draw(c, tev, frame);   // incl. the shape the executable names 'whitefade': the white ramp on the right (PS2 frames: single/04..07), not the screen transition
    const events = this.stateEvents(s);
    this.map.draw(c, events, frame, this.override(s));
    if (this.flash) { c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash.at) / FLASH)})`; c.fillRect(0, 0, 640, 480); }
    c.restore();
  }
  // Timeline states in effect: the event screen's indicator state of the focused course.
  stateEvents(s) {
    const out = [];
    if (s !== 'fe-event') return out;
    const e = this.events()[this.ui.index], frame = e ? INDICATOR_STATE[e.code ?? e.key] : null;
    if (frame != null) for (const ev of this.map.screen.events) if (ev.frame === frame) out.push({ ev, start: 0 });
    return out;
  }
  override(s) {
    const list = s === 'fe-event' ? this.events() : [], index = this.ui.index, dis = this.disabledList(s), items = this.items(s);
    const mode = s === 'fe-mode' ? MODES[index] || this.mode : this.mode;   // the focused row (keys or mouse hover)
    const red = redPaths(s === 'fe-mode' ? this.events(this.peak, mode) : list, { screen: s, mode, index });
    const menuLabel = s === 'fe-peak' ? 'SP_PeakList' : s === 'fe-mode' ? `SPG_Peak${this.peak}Goals` : `Peak${this.peak}${this.mode === 'race' ? 'Race' : 'Freestyle'}Locations`;
    const menu = this.el(menuLabel), rows = menu ? menu.children.map((n) => this.map.byName.get(n)).filter(Boolean).sort((a, b) => (a.props?.[1] ?? 0) - (b.props?.[1] ?? 0)) : [];
    const rowOf = new Map(rows.map((e, i) => [e.name, i]));
    const pathCode = new Map(Object.entries(PATHS).map(([code, label]) => [label, code]));
    const focused = list[index];
    const white = { 13: 255, 14: 255, 15: 255, 16: 255 }, black = { 13: 255, 14: 0, 15: 0, 16: 0 };
    return (e) => {
      if (e.kind === 'group' || e.kind === 'menu') {   // groups pass their offset / alpha; leaves decide
        const outline = /^peak([123])outline/.exec(this.map.byName.get(e.children?.[0])?.label || '');   // 005336ec.. start at A=0
        return outline ? (s === 'fe-peak' && +outline[1] === 3 - index ? { alpha: 255 } : { hidden: true }) : null;
      }
      if (!this.visible(e, s)) return { hidden: true };
      const L = e.label;
      if (rowOf.has(e.name)) {   // menu rows: the code writes the names; focus = white text on the orange bar
        const i = rowOf.get(e.name); if (i >= items.length) return { hidden: true };
        const p = { ...(e.props || {}), ...(i === index ? white : black), 9: 70, 10: 70 };
        return { text: items[i], props: p, alpha: dis[i] && i !== index && s !== 'fe-peak' ? DISABLED_ALPHA : 255 };   // locked peaks keep black text + lock (PS2)
      }
      if (L === 'phl' || L === 'pghl' || L === 'ehl') return { props: { 1: 103 + ROW * index } };
      if (L === 'title_title') return { text: s === 'fe-peak' ? this.t('kT_240SelectPeak', 'Select Peak') : s === 'fe-mode' ? this.t('kT_FEHelpSelectMode', 'Select Mode') : this.t('kT_240SelectEvent', 'Select Event') };
      if (L === 'HelpText') return helpOverride(this.help(s, index, focused));
      if (L === 'fePEAKno back') return s === 'fe-peak' ? { hidden: true } : null;
      if (/^peak[123]outline/.test(L || '')) return { props: this.recolour(e, [255, 128, 0], 255) };   // vertex A = 0 in the data; the code shows the focused peak's
      if (L === 'PEAKno') return { text: s === 'fe-mode' ? this.t(`kT_Peak${this.peak}name`, `Peak ${this.peak}`) : this.tab(focused) };
      if (L === 'maptab text') return { text: this.info ? this.t('kT_BTNShowMap', 'Show MAP') : this.t('kT_BTNShowInfo', 'Show INFO') };
      if (L === 'MapPic') {
        const page = s === 'fe-peak' ? 'MAPGFX_map_mtn' : `MAPGFX_map_peak${'ABC'[this.peak - 1]}`;
        return this.images[page] && !(s === 'fe-event' && this.info) ? { sprite: { page, sx: 0, sy: 0, sw: 256, sh: 256 } } : { hidden: true };
      }
      if (e.name === '0c2f12c2') return { props: this.scaled(e) };   // the map frame shape at 110%
      if (pathCode.has(L)) { const rgb = red.has(pathCode.get(L)) ? PATH_RED : PATH_ORANGE; return { props: this.recolour(e, rgb) }; }
      // Select Peak: the focused peak's outline on the mountain; locks on the peaks without a pass
      if (this.chain.get(e.name)?.includes(this.el('Locks')?.name)) {
        const lock = ['007359e3', '007359e2', '007359e1', '007359e0'].indexOf(e.name);   // rows Peak 3 / 2 / 1 / Mountain
        return lock >= 0 && lock < 3 && !this.unlocked(3 - lock) ? null : { hidden: true };
      }
      // Select Event: the medal column (career best of this rider), boxes only where a medal is shown
      const box = ['07b59610', '07b59611', '07b59612', '07b59613', '07b59614', '07b59615', '07b59616', '07b59617'].indexOf(e.name);
      if (box >= 0) return { hidden: true };
      const check = ['059e1680', '059e1681', '059e1682', '059e1683', '059e1684', '059e1685', '059e1686', '059e1687'].indexOf(e.name);
      if (check >= 0) { const m = this.medalOf(list[check]); return m != null && m !== MEDAL.NONE ? { sprite: { page: 'FE_1-11', sx: MEDAL_CELL[m][0], sy: MEDAL_CELL[m][1], sw: MEDAL_CELL[m][2], sh: MEDAL_CELL[m][3] }, props: { 0: -3, 1: -3 + ROW * check, 6: 20, 7: 19 } } : { hidden: true }; }
      if (s === 'fe-event' && this.info) { const r = this.infoRow(e, focused); if (r) return r; }
      return null;
    };
  }
  // Shape props with the element's own scale applied to its vertices (lui-player draws shape vertices unscaled).
  scaled(e) { const p = e.props || {}, sx = (p[9] ?? 100) / 100, sy = (p[10] ?? 100) / 100, o = {}; for (let k = 0; k < (e.shape?.[0] ?? 4); k++) { o[21 + 9 * k] = (p[21 + 9 * k] || 0) * sx; o[22 + 9 * k] = (p[22 + 9 * k] || 0) * sy; } return o; }
  recolour(e, rgb, a = null) { const o = {}; for (let k = 0; k < (e.shape?.[0] ?? 4); k++) { o[27 + 9 * k] = rgb[0]; o[28 + 9 * k] = rgb[1]; o[29 + 9 * k] = rgb[2]; if (a != null) o[26 + 9 * k] = a; } return o; }
  tab(e) { if (!e) return ''; if (e.kind === 'peak') return e.mode >= 9 ? 'Jam' : this.t('kT_cmnRaceCaps', 'Race'); const [k, f] = EVENT_TAB[e.kind] || EVENT_TAB.race; return this.t(k, f); }
  help(s, i, e) {
    if (s === 'fe-peak') { const p = 3 - i; return this.unlocked(p) ? this.t('kT_FEChoosePeakSelMode', 'Choose this peak and continue to select mode.') : this.t(`kT_10HELPUnlockPeak${p}`, `LOCKED:  Get a Peak ${p} pass in Conquer the Mountain mode.`); }
    if (s === 'fe-mode') return this.t('kT_FEChooseModeSelEvent', 'Choose a mode and continue to select event.');
    if (!e) return '';
    if (e.kind === 'peak') return this.t(e.mode >= 9 ? 'kT_HELPBeatChallPeakJam' : 'kT_HELPBeatChallengePeakRace', '');
    // kT_HELPBRA2 hashes like kT_HELPCBA2 (Launch Time); Metro-City's text is kT_HELPBRA2Blah, so that name goes first
    const text = this.t('kT_HELP' + e.code + 'Blah', '') || this.t('kT_HELP' + e.code, '');
    return e.playable ? text : `${text}${text ? '  ' : ''}(This location is not ported yet.)`;
  }
  medalOf(e) { try { return e && e.course != null && this.career ? this.career.medal(this.riderId, e.mode, e.course) : null; } catch { return null; } }
  // INFO (Square): Run / Event / Medal / Top time or Top score for the focused event (SE_Info, as the MCOMM Transport).
  infoRow(el, e) {
    if (!e) return null;
    const L = el.label, name = el.name, medal = this.medalOf(e), top = e.course != null ? this.career?.topRecord?.(e.mode, e.course) : null, timed = isTimed(e.mode);
    if (L === 'SE_Run') return { text: e.course != null ? this.careerData?.courses?.[e.course]?.name || e.name : e.name };
    if (L === 'SE_Event') return { text: e.mode === 4 ? this.t('kT_cmnRaceCaps', 'Race') : e.mode === 5 ? 'Jam' : this.tab(e) };
    if (L === 'SE_Data1Label') return { text: this.t('kT_MAPMedal', 'Medal:') };
    if (L === 'SE_Data1') return { text: medal == null || medal === MEDAL.NONE ? this.t('kT_CMNNoMedal', 'No medal') : MEDAL_NAMES[medal] };
    if (L === 'SE_Data2Label') return top ? { text: timed ? this.t('kT_CMNTopTime', 'Top time:') : this.t('kT_CMNTopScore', 'Top score:') } : { hidden: true };
    if (L === 'SE_Data2') return top ? { text: timed ? raceTime(top.ticks, false) : String(top.value) } : { hidden: true };
    if (name === '00bef29b' && !top) return { hidden: true };
    if (el.kind === 'shape' && this.chain.get(name)?.includes(this.el('SE_Info')?.name)) return { props: this.scaled(el) };
    return null;
  }
}
