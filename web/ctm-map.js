// The Conquer the Mountain Transport map (MCOMM > Transport: Select Peak / Select Peak Goal / Select ... Event) drawn from the
// original Map LUI (0x5380; OV.LUI holds the same screen as FE.LUI with its sprites on the OV_1 pages at the same UVs), as
// the PS2 draws it over the PDA frame (docs/ctm-parity.md "Transport map"): the 'pdaart' title (the mountain icon,
// "Transport", the sub title), the map picture MapPic and its tab, "Show INFO" (maptab), and on the map
//   Select Peak       the focused peak's outline (peak<n>outline, drawn orange) and "You are here" on the peak being ridden
//                     (P<n>you are here), the lock icons of the peaks without a pass (Locks);
//   Select Peak Goal  every route of the peak (peak<n-1>indicators: route shapes, node dots, station icons), those of the
//                     focused goal's list red, the rest orange (0x206690 'peak<n>racegroup' / 'fsgroup' / 'freeridegroup':
//                     the goal list's courses; Freeride: all red);
//   Select ... Event  the focused event's route red (0x206690 '<code>path'; a peak run 'peak<n>racepath' / 'fspath' = its
//                     table 0x4714A0 / 0x4A26A8..: Peak 1 Race abc1 + ara1 + bra0, Peak 1 Jam abc1 + ass1, ...) and the start
//                     indicator at its Map state (web/fe-event-select.js INDICATOR_STATE).
// web/career-ui.js keeps the lists, the INFO views and the help line; pv transportMap switches this on.
import { LuiScreen } from './lui-player.js';
import { pv } from './pv-flags.js';
import { PATHS, PATH_RED, PATH_ORANGE, INDICATOR_STATE } from './fe-event-select.js';

const SY = 448 / 480;
// FE.LUI's Map sprites -> the same cells on the OV pages (tools/export_ctm_screens.py pages; identical UVs in OV.LUI's Map).
export const OV_PAGE = { 'FE_1-11': 'OV_1-1', 'FE_1-14': 'OV_1-2', 'FE_1-18': 'OV_1-5', 'FE_1-19': 'OV_1-6' };
// The routes of each peak (the per-peak tables 0x4713A8 / 0x471420 / 0x471488) and of the peak runs (0x4714A0, 0x4A26A8..).
export const PEAK_PATHS = [['ABC1', 'ASS1', 'ARA1', 'ABA1', 'BRA2', 'BHP1'], ['CRA3', 'CBA2', 'CHP2', 'DBC2', 'DSS2', 'DRA4'], ['EBC3', 'ERA5', 'EBA3', 'EHP3', 'ESS3']];
// The Map states that put a base station's icon on the map (the indicator hidden): the freeride list's stations. The markers
// sit on the mountain in the order of the All Peak Race splits (E, C, D, A, B top to bottom): 210 at y 52 / 250 at y 179 on
// Peak 1 = Green (A, course 17) / Blue (B, 18); 350 at y 45 / 320 at y 126 on Peak 2 = Yellow (C, 19) / Red (D, 20); 430 = Black (E, 21).
export const STATION_INDICATOR = Object.freeze({ 17: 210, 18: 250, 19: 350, 20: 320, 21: 430 });
// Select Peak: the Map state of the focused row (the focus bar phl, the focused peak's outline, "You are here" shown or not):
// Peak 3 80, Peak 2 70, Peak 1 60, All Mountain 90.
export const PEAK_ROW_STATE = [80, 70, 60, 90];
export const INTRO = 30;   // the Map's intro: the title pieces slide / fade in over frames 5..25, the list at 30
export const PEAK_RUN_PATHS = { 'peak1-race': ['ABC1', 'ARA1', 'BRA2'], 'peak1-jam': ['ABC1', 'ASS1'], 'peak2-race': ['DBC2', 'DRA4'], 'peak2-jam': ['DBC2', 'DSS2'],
  'peak3-race': ['EBC3', 'ERA5'], 'peak3-jam': ['EBC3', 'ESS3'] };

// The red routes (0x206690): goals -> the courses of the focused goal's list (Freeride: every route of the peak), an event ->
// its course or its peak run's courses; Select Peak none. list: [{code?, key?, course?}], codes: course index -> code.
export function transportRedPaths(screen, { peak = 1, goal = 'race', focused = null, goalLists = {} } = {}) {
  if (screen === 'ctm-goals') {
    if (goal === 'freeride') return new Set(PEAK_PATHS[peak - 1] || []);
    return new Set((goalLists[goal] || []).map((e) => e.code).filter(Boolean));
  }
  if (screen === 'ctm-events' && focused) {
    if (focused.key && PEAK_RUN_PATHS[focused.key]) return new Set(PEAK_RUN_PATHS[focused.key]);
    return new Set(focused.code && !focused.station ? [focused.code] : []);   // a station has no route (its icon shows instead)
  }
  return new Set();
}

async function loadImage(src) { const im = new Image(); im.src = src; await im.decode(); return im; }

export class CtmMap {
  constructor(ui) { this.ui = ui; this.lui = null; this.loading = null; this.openedAt = 0; this.drawnAt = -1e9; }
  now() { return performance.now() * 60 / 1000; }
  // frames since the Transport opened: its intro plays when it is drawn after something else (as ctm-pda's PDA frame)
  clock() { const now = this.now(); if (now - this.drawnAt > 15) this.openedAt = now; this.drawnAt = now; return now - this.openedAt; }
  get on() { return pv('transportMap'); }
  get ready() { return this.on && !!this.lui; }
  // Built from the FE Map already loaded by web/fe-event-select.js, its sprites moved to the OV pages.
  ensure() {
    if (this.lui || this.loading || !this.on) return;
    const es = this.ui.eventSelect, screen = es?.data?.screens?.Map; if (!screen) return;
    this.loading = (async () => {
      const images = this.ui.images;
      await Promise.all(Object.values(OV_PAGE).filter((p) => !images[p]).map(async (p) => { images[p] = await loadImage(`/assets/UI/${p}.png`); }));
      for (const p of ['MAPGFX_map_mtn', 'MAPGFX_map_peakA', 'MAPGFX_map_peakB', 'MAPGFX_map_peakC']) if (es.images[p]) images[p] = es.images[p];
      const elements = screen.elements.map((e) => (e.sprite && OV_PAGE[e.sprite.page] ? { ...e, sprite: { ...e.sprite, page: OV_PAGE[e.sprite.page] } } : e));
      const lui = new LuiScreen({ ...screen, elements }, images, this.ui);
      const by = lui.byName, label = new Map(screen.elements.filter((e) => e.label).map((e) => [e.label, e.name]));
      this.chain = new Map();
      for (const e of screen.elements) { const up = []; let p = e.parent || e.menu; while (p && by.has(p)) { up.push(p); const q = by.get(p); p = q.parent || q.menu; } this.chain.set(e.name, up); }
      this.label = label; this.lui = lui;
    })().catch((e) => { console.warn('Transport map unavailable', e); this.loading = null; });
  }
  under(e, lab) { const n = this.label.get(lab); return !!n && (e.name === n || this.chain.get(e.name)?.includes(n)); }
  // The PDA title: the mountain icon, "Transport" and the sub title (pdaart / pdatitle).
  title(c, sub) { return this.draw(c, { part: 'title', sub }); }
  // The map region: s = 'ctm-peaks' | 'ctm-goals' | 'ctm-events'; o = {index, peak, herePeak, tab, info, red, indicator, locked}
  map(c, s, o) { return this.draw(c, { part: 'map', s, ...o }); }
  draw(c, o) {
    this.ensure(); if (!this.lui) return false;
    const t = o.part === 'title' ? this.clock() : Math.max(0, this.now() - this.openedAt), events = [];
    // the event list's indicator state; Select Peak's row states (PEAK_ROW_STATE) only move the focus bar, which career-ui
    // draws, and replace the outlines' vertex lists, so the focused outline is shown as in web/fe-event-select.js
    const state = o.part === 'map' && o.s === 'ctm-events' ? o.indicator : null;
    for (const ev of this.lui.screen.events) {
      if (ev.frame <= INTRO) { if (ev.frame <= t) events.push({ ev, start: ev.frame }); }
      else if (ev.frame === state) events.push({ ev, start: 0 });
    }
    c.save(); c.scale(1, SY);
    this.lui.draw(c, events, t, (e) => this.override(e, o));
    c.restore();
    return true;
  }
  override(e, o) {
    const L = e.label;
    if (e.kind === 'group' || e.kind === 'menu') {   // the outline groups start at A = 0: the code shows the focused peak's
      const outline = /^peak([123])outline/.exec(this.lui.byName.get(e.children?.[0])?.label || '');
      if (outline) return o.part === 'map' && o.s === 'ctm-peaks' && !o.info && +outline[1] === o.focusPeak ? { alpha: 255 } : { hidden: true };
      return null;
    }
    if (o.part === 'title') {
      if (!this.under(e, 'pdatitle')) return { hidden: true };
      if (L === 'SubTitle') return { text: o.sub || '' };
      return null;
    }
    const s = o.s, map = !o.info;
    if (e.label === 'MapPic') return map ? { sprite: { page: s === 'ctm-peaks' ? 'MAPGFX_map_mtn' : `MAPGFX_map_peak${'ABC'[o.peak - 1]}`, sx: 0, sy: 0, sw: 256, sh: 256 } } : { hidden: true };
    if (e.name === '0c2f12c2') return map ? null : { hidden: true };   // the map's frame
    if (L === 'PEAKno' || L === 'pdaPEAKno back') return s !== 'ctm-peaks' && o.tab ? (L === 'PEAKno' ? { text: o.tab } : null) : { hidden: true };
    if (this.under(e, 'maptab') || L === 'pdamaptab back') { if (!o.showTab) return { hidden: true }; return L === 'maptab text' ? { text: o.tabText } : null; }
    const here = this.youAreHere(e);
    if (here) return s === 'ctm-peaks' && map && +here[1] === o.herePeak ? null : { hidden: true };
    if (/^peak[123]outline/.test(L || '')) return { props: recolour(e, [255, 128, 0], 255) };   // vertex A = 0 in the data (state 60 / 70 / 80: 255,128,0 at 255)
    if (this.under(e, 'Locks')) {
      const lock = ['007359e3', '007359e2', '007359e1', '007359e0'].indexOf(e.name);   // rows Peak 3 / 2 / 1 / All Mountain
      return s === 'ctm-peaks' && lock >= 0 && lock < 3 && o.locked?.[lock] ? null : { hidden: true };
    }
    if (this.under(e, 'Indicators')) {
      if (s === 'ctm-peaks' || !map) return { hidden: true };
      if (L === 'indicator') return s === 'ctm-events' && o.indicator != null ? null : { hidden: true };   // a station state hides it and shows the station's icon
      if (!this.under(e, `peak${o.peak - 1}indicators`)) return { hidden: true };
      const code = PATH_CODE.get(L);
      if (code) return { props: recolour(e, o.red?.has(code) ? PATH_RED : PATH_ORANGE) };
      return null;
    }
    return { hidden: true };
  }
  youAreHere(e) { const up = this.chain.get(e.name) || []; for (const n of [e.name, ...up]) { const l = this.lui.byName.get(n)?.label; const m = l && /^P([123])you are here$/.exec(l); if (m) return m; } return null; }
}
const PATH_CODE = new Map(Object.entries(PATHS).map(([code, label]) => [label, code]));
function recolour(e, rgb, a = null) { const o = {}; for (let k = 0; k < (e.shape?.[0] ?? 4); k++) { o[27 + 9 * k] = rgb[0]; o[28 + 9 * k] = rgb[1]; o[29 + 9 * k] = rgb[2]; if (a != null) o[26 + 9 * k] = a; } return o; }
export { INDICATOR_STATE };
