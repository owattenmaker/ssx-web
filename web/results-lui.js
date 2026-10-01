// In-race cards and results from the original OV.LUI records (tools/export_results_screens.py -> /assets/UI/results-screens.json;
// pv luiResults, docs/visual-parity.md section 9). web/career-ui.js draws through it when the export is present and the switch
// is on; otherwise it keeps its own drawing.
//   OV_darkblue        the panel every card / result sits in (cut-corner frame, header bar with the OV_1-6 picture, blinking
//                      lights): open animation frames 13..60 from the first frame the panel is drawn, then the lights (65..499)
//   43final_standings  the results: track_event (the event, y 93, 85 %) and title (the round, y 116, 75 %), Rank / Riders /
//                      Time|Score (white) over six rows at 20 px (names / values teal 101,184,201), the medal of the human's
//                      row, helptext, the overlayMenu (Continue / Restart / Replay / Records / Quit, focus frames 40 + 10 i:
//                      the focused row white, ps2x beside it), the multiplayer chat (hidden)
// The human's row is 255,204,153 (PS2 ctm-parity/runs/after-final/results.png: 'Zoe' / '03:55' at 203,163,122 = 0.8 x, the
// LUI text colour factor of web/lui-player.js).
import { LuiScreen } from './lui-player.js';
import { pv } from './pv-flags.js';

const SY = 448 / 480;
const OPEN = 60, LIGHTS = 65, CLOSE = 500;           // OV_darkblue: open done at 60, the lights loop from 65 until the close at 500
const HEADER_LIGHTS = ['0760d5c2', '0760d5c3'];
// pv luiLights (docs/visual-parity.md section 9, header lights). OV_darkblue's six header lights, slanted 11 x 7 shapes along
// the header's top edge at y 73: 0760d5c2 / 0760d5c3 in their own groups 06c3ae11 (433, 57) / 06c3ae12 (449, 63) at
// (13, 16) / (12, 10) -> x 446 / 461, and 0760d5c4..c7 in the panel group 00e91994 (18, 7) at x 480 / 495 / 531 / 546 ->
// 498 / 513 / 549 / 564. The runtime draws a child at the sum of its groups' x / y (group draw 0x398868 hands its origin plus
// its own x / y to each child, shape draw 0x3A39F8 adds the element's x / y to its vertices), as lui-player's abs() does.
// The groups' layout was lost by the export: OV_darkblue's frame 0 sets 00e91994 twice, so pairing the records with the
// definitions by index (export_character_select decode) gave every later element its predecessor's record; the runtime
// applies them by name (0x39CD30 -> 0x39D860). tools/export_results_screens.py pairs them by name (screen.state0 'byName');
// with an older export the two groups take their layout from LIGHT_GROUPS (OV.LUI state-0 records 47 / 49; the savestates'
// group elements hold 433,57 / 449,63).
export const LIGHT_SHAPES = ['0760d5c2', '0760d5c3', '0760d5c4', '0760d5c5', '0760d5c6', '0760d5c7'];
export const LIGHT_GROUPS = { '06c3ae11': { 0: 433, 1: 57 }, '06c3ae12': { 0: 449, 1: 63 } };
// OV_darkblue's timeline as the overlay steps it (0x39D778 -> 0x39C978, one frame a tick): the main playback runs 0..60 (the
// 0x10 record at 60 stops it); 0x12 records start two more playbacks (0x39CC38 -> 0x39C870): at 35 the 560 label (560..688,
// no element records) and at 50 the 65 label, the lights loop 65..500, where the 0x11 record jumps back to 65 (0x39CBE0):
// 436 frames a turn. A playback started by a record is appended to the screen's list (0x397910) and first stepped when the
// update pass (0x39D778, which reads each node's next before stepping it) reaches it: the 560 one at tick 36, the 65 one,
// appended behind it, in tick 50 itself. So loop frame 65 plays at tick 50, 15 frames before the main timeline's 65.
const MAIN_STOP = 60, LOOP_AT = 50, LOOP_FROM = 65, LOOP_TO = 500, TURN = LOOP_TO - LOOP_FROM + 1;
// [{ev, start}] in application order at frame t since the panel opened, each start on t's clock (an animation's frame counter
// starts when its record is applied, 0x397468); the loop's records of the previous turn that are still the latest stay.
export function panelTimeline(events, t) {
  const out = [];
  for (const ev of events) if (ev.frame <= MAIN_STOP && ev.frame <= t) out.push({ ev, start: ev.frame });
  if (t >= LOOP_AT) {
    const turn = Math.floor((t - LOOP_AT) / TURN), at = LOOP_FROM + (t - LOOP_AT) - turn * TURN;   // the loop's frame at t
    const tick = (frame, k) => LOOP_AT + (frame - LOOP_FROM) + k * TURN;
    const loop = events.filter((ev) => ev.frame >= LOOP_FROM && ev.frame < LOOP_TO);
    if (turn > 0) for (const ev of loop) if (ev.frame > at) out.push({ ev, start: tick(ev.frame, turn - 1) });
    for (const ev of loop) if (ev.frame <= at) out.push({ ev, start: tick(ev.frame, turn) });
  }
  return out.sort((a, b) => a.start - b.start);        // stable: the main playback steps first within a tick
}
const EL = {
  track: '01abc714', title: '007b0b25', help: '0c37a134', data: '0bcd9da1', chat: '088e0500', ps2x: '00077698',
  menu: '0263a575', items: ['0263fe50', '0263fe51', '0263fe52', '0263fe53', '0263fe54'],
  medals: ['0006e624', '07a03cc2', '06996605'], medalGroup: '0800e180',
  rankHead: '0007884b', ranks: ['007884e1', '007884e2', '007884e3', '007884e4', '007884e5', '007884e6'],
  riderHead: '078fac93', riders: ['078fac51', '078fac52', '078fac53', '078fac54', '078fac55', '078fac56'],
  valueHead: '061a88b5', values: ['07aa5c21', '07aa5c22', '07aa5c23', '07aa5c24', '07aa5c25', '07aa5c26'],
};
export const HUMAN_ROW = Object.freeze([255, 204, 153]);
// medal hex cells of OV_1-1 (the LUI gold / silver / bronze sprites; platinum is the cell under bronze, as career-ui MEDAL_CELL)
const MEDAL_SY = [120.5, 0.5, 40.5, 80.5];           // MEDAL.PLATINUM 0, GOLD 1, SILVER 2, BRONZE 3

export class ResultsLui {
  constructor(ui) { this.ui = ui; this.data = null; this.lui = {}; this.openAt = 0; this.lastDraw = -1e9; this.names = new Map(); }
  async load() {
    try {
      const data = await (await fetch('/assets/UI/results-screens.json')).json(), images = this.ui.images;
      await Promise.all(data.pages.filter((p) => !images[p]).map(async (p) => { const im = new Image(); im.src = `/assets/UI/${p}.png`; await im.decode(); images[p] = im; }));
      for (const [key, screen] of Object.entries(data.screens)) {
        this.lui[key] = new LuiScreen(screen, images, this.ui); this.lui[key].shapeScale = true; this.lui[key].unionFlat = true; this.lui[key].keepLead = true;
        // pv resultsMenu: a text wraps only when its element has flag 0x80 (lui-player flagWrap, 0x3A0528): the menu items (0x24c)
        // stay on one line, so 43final_standings' 'Next event' (about 88 px in its 80 px item) no longer breaks onto Restart
        // (PS2 menus/replay/bhp1-neutral, nav/bc/out-jam-finish: 'Next event' on one line, x 461..549)
        Object.defineProperty(this.lui[key], 'flagWrap', { get: () => pv('resultsMenu') });
        // pv luiWrap: a wrapping text breaks where 0x3A0D00 does (lui-player ps2Wrap: font advances x scale % against the width)
        Object.defineProperty(this.lui[key], 'ps2Wrap', { get: () => pv('luiWrap') });
      }
      this.data = data;
    } catch (e) { console.warn('Results panels missing (python3 tools/export_results_screens.py)', e); }
  }
  get ready() { return !!this.data && !!this.lui.OV_darkblue && !!this.lui['43final_standings']; }
  now() { return performance.now() * 60 / 1000; }
  // frames since the panel opened: a panel not drawn for 15 frames (250 ms) opens again, as the overlay's 39C870 restart
  clock() { const now = this.now(); if (now - this.lastDraw > 15) this.openAt = now; this.lastDraw = now; return now - this.openAt; }
  opened() { return this.now() - this.openAt; }
  // OV_darkblue at frame t: the open events up to t, then the light loop 65..499 over the settled panel
  panelFrame(c, t) {
    if (pv('luiLights')) return this.panelLights(c, t);
    const lui = this.lui.OV_darkblue, f = t < LIGHTS ? t : LIGHTS + ((t - LIGHTS) % (CLOSE - LIGHTS));
    const events = lui.screen.events.filter((ev) => ev.frame < CLOSE && ev.frame <= f).map((ev) => ({ ev, start: ev.frame }));
    // The two header lights (shapes 0760d5c2 / 0760d5c3, whose events carry their groups' layout: x 12, y 10 / x -280..13) land
    // in the top-left corner (24, 20) and off screen, where the PS2 shows nothing (they blink at the header's top right, x 448 /
    // 463, y 73: PS2 race-f95 / peak1-race-objectives); left out until that layout is traced.
    c.save(); c.scale(1, SY); lui.draw(c, events, f, (e) => (HEADER_LIGHTS.includes(e.name) ? { hidden: true } : null)); c.restore();
  }
  // pv luiLights: OV_darkblue on its own timeline (panelTimeline), with the header lights. A light rests dark blue 4,45,74 at
  // vertex alpha 125 and ramps to white at 100 over 40 frames and back (0e7d0442 / 0ead0443 ...): 0760d5c2 peaks at t 92 and
  // 172, 0760d5c3 at 140 and 220, then c4 190, c5 240 / 320, c6 290, c7 340 / 420, and again every 436 frames. The shape draw
  // (0x3A39F8) sends the vertex colours alone (x 0.5 to GS alpha): the element's own alpha (prop 13, animated with the vertex
  // alphas) is not applied, so the lights' override alpha 255 keeps lui-player from applying it twice (PS2 race-f95/final: the
  // lit light R 111 = 0.39 white over the header, the dark one R 16-17 = 0.49 of 4,45,74; squared they would be 60 / 22).
  panelLights(c, t) {
    const lui = this.lui.OV_darkblue, groups = lui.screen.state0 === 'byName' ? null : LIGHT_GROUPS;
    const events = panelTimeline(lui.screen.events, t);
    c.save(); c.scale(1, SY);
    lui.draw(c, events, t, (e) => (LIGHT_SHAPES.includes(e.name) ? { alpha: 255 } : groups?.[e.name] ? { props: groups[e.name] } : null));
    c.restore();
  }
  // the content screen with the frame-30 layout plus the focus frame of `index`; override(e) -> {hidden, text, props, sprite, alpha}
  content(c, index, override) {
    const lui = this.lui['43final_standings'], focus = 40 + 10 * Math.max(0, Math.min(4, index));
    const events = lui.screen.events.filter((ev) => ev.frame === 30 || ev.frame === focus).map((ev) => ({ ev, start: ev.frame }));
    c.save(); c.scale(1, SY); lui.draw(c, events, 1000, override); c.restore();
  }
  // A card or a result's panel with its two header lines (track_event / title), e.g. the objectives card, records, rewards.
  panel(c, title, sub, { openOnly = false } = {}) {
    const t = this.clock(); this.panelFrame(c, t);
    if (openOnly) return t;
    this.content(c, 0, (e) => e.name === EL.track ? { text: title } : e.name === EL.title ? { text: sub } : { hidden: true });
    return t;
  }
  // The results (43final_standings): heads [rank, riders, value], rows [{rank, name, value, human}] in rank order, medal
  // {row, kind} or null, message, items (menu labels), index (focus), disabled(i), noItems.
  results(c, { title, sub, heads, rows, medal, message, items, index, disabled = () => false, noItems = false }) {
    this.panelFrame(c, this.clock());
    const at = (list, name) => list.indexOf(name);
    this.content(c, index, (e) => {
      const n = e.name;
      if (n === EL.track) return { text: title };
      if (n === EL.title) return { text: sub };
      if (n === EL.data) return { alpha: 255 };                 // 0x1E78F4 shows screenData (authored at alpha 0)
      if (n === EL.chat) return { hidden: true };
      if (n === EL.help) return message ? { text: message } : { hidden: true };
      if (n === EL.rankHead) return { text: heads[0] };
      if (n === EL.riderHead) return { text: heads[1] };
      if (n === EL.valueHead) return { text: heads[2] };
      let k = at(EL.ranks, n); if (k >= 0) return rows[k] ? { text: String(rows[k].rank) } : { hidden: true };
      k = at(EL.riders, n); if (k >= 0) return rows[k] ? { text: rows[k].name, ...(rows[k].human ? { props: colour(HUMAN_ROW) } : {}) } : { hidden: true };
      k = at(EL.values, n); if (k >= 0) return rows[k] ? { text: rows[k].value, ...(rows[k].human ? { props: colour(HUMAN_ROW) } : {}) } : { hidden: true };
      k = at(EL.medals, n);
      if (k >= 0) {   // one hex, left of the human's rank, of the medal earned (the gold element moved to that row)
        if (k || !medal || medal.kind == null || medal.row < 0) return { hidden: true };
        const sp = e.sprite; return { sprite: { ...sp, sy: MEDAL_SY[medal.kind] ?? sp.sy }, props: { 1: -8 + 20 * medal.row } };
      }
      k = at(EL.items, n);
      if (k >= 0) { if (noItems || k >= items.length) return { hidden: true }; return { text: items[k], ...(disabled(k) && k !== index ? { alpha: 128 } : {}) }; }
      if (n === EL.ps2x || n === '069d01a0') return noItems || index >= items.length ? { hidden: true } : null;
      return null;
    });
  }
  // any content screen at its frame-30 layout plus one focus frame
  screen(c, key, focus, override) {
    const lui = this.lui[key]; if (!lui) return false;
    const events = lui.screen.events.filter((ev) => ev.frame === 30 || ev.frame === focus).map((ev) => ({ ev, start: ev.frame }));
    c.save(); c.scale(1, SY); lui.draw(c, events, 1000, override); c.restore(); return true;
  }
  // 61toptimes: title, timed (Top 5 Record Times / Scores), rows [{name, rider, value, player}] (5), message (a new top time /
  // score, or null), items (Continue / Save Records, or Return), index. The player's row is 250,135,18 (PS2 out-apr-results
  // 'PLAYER 1' / 'Zoe' / '22:10' at 200,108,14 = 0.8 x).
  // pv onlineRecords (web/online-records-ui.js): a third menu item (61toptimes has two: Continue / Save Records; the copy adds one
  // below at the same 15-line pitch with its own focus frame 50, the cross glyph 14 lower again), the rank column's text (row.rank),
  // a focused row in the focus white (row.focus), and the subtitle's text (subtitle).
  records(c, { title, timed, rows, message, items, index, disabled = () => false, subtitle = null }) {
    if (!this.lui['61toptimes']) return false;
    this.panelFrame(c, this.clock());
    const three = items.length > 2 && this.topTimesThree(), key = three ? '61toptimes+' : '61toptimes', itemNames = three ? [...TOP.items, TOP_THIRD] : TOP.items;
    const R = TOP.rows, pick = (list, n) => list.indexOf(n);
    return this.screen(c, key, index === 2 && three ? 50 : index === 1 ? 45 : 40, (e) => {
      const n = e.name;
      if (n === TOP.track) return { text: title };
      if (n === TOP.times) return timed ? (subtitle != null ? { text: subtitle } : null) : { hidden: true };
      if (n === TOP.scores) return timed ? { hidden: true } : (subtitle != null ? { text: subtitle } : null);
      if (n === TOP.time) return timed ? null : { hidden: true };
      if (n === TOP.points) return timed ? { hidden: true } : null;
      if (n === TOP.help1) return message && timed ? { text: message } : { hidden: true };
      if (n === TOP.help2) return message && !timed ? { text: message } : { hidden: true };
      if (n === TOP.xgroup) return items.length ? { alpha: 255 } : { hidden: true };   // 0x1FE1D8 shows ps2x beside the menu
      let k = pick(itemNames, n); if (k >= 0) return k < items.length ? { text: items[k], props: colour(k === index ? [255, 255, 255] : [101, 184, 201]), ...(disabled(k) && k !== index ? { alpha: 128 } : {}) } : { hidden: true };
      for (const [col, field] of [[R.rank, 'rank'], [R.name, 'name'], [R.rider, 'rider'], [R.value, 'value']]) {
        k = pick(col, n); if (k < 0) continue; const row = rows[k];
        if (!row) return { hidden: true };
        const text = field === 'rank' ? (row.rank != null ? { text: String(row.rank) } : {}) : { text: row[field] };
        return { ...text, ...(row.focus ? { props: colour([255, 255, 255]) } : row.player ? { props: colour(RECORD_ROW) } : {}) };
      }
      return null;
    });
  }
  // 61toptimes with a third menu item (pv onlineRecords): built once from the screen's own Save Records element and its events.
  topTimesThree() {
    if (this.lui['61toptimes+']) return true;
    const base = this.lui['61toptimes']; if (!base) return false;
    const screen = JSON.parse(JSON.stringify(base.screen)), [first, second] = TOP.items, PITCH = 15, GLYPH = 14;
    const src = screen.elements.find((e) => e.name === second), menu = screen.elements.find((e) => (e.children || []).includes(second));
    if (!src || !menu) return false;
    screen.elements.push({ ...JSON.parse(JSON.stringify(src)), name: TOP_THIRD, index: Math.max(...screen.elements.map((e) => e.index ?? 0)) + 1, label: 'onlinerecords',
      props: { ...src.props, 1: src.props[1] + PITCH, 7: 15 } });
    menu.children.push(TOP_THIRD);
    const blue = { 14: 101, 15: 184, 16: 201 }, white = { 14: 255, 15: 255, 16: 255 }, added = [];
    for (const ev of screen.events) if (ev.element === second && (ev.frame === 30 || ev.frame === 40 || ev.frame === 45)) added.push({ ...ev, element: TOP_THIRD, props: { ...ev.props, 1: ev.props[1] + PITCH, 7: 15, ...blue } });
    for (const ev of screen.events) if (ev.frame === 40 && [first, second, TOP.xgroup].includes(ev.element)) {
      const p = { ...ev.props, ...(ev.element === TOP.xgroup ? { 1: ev.props[1] + 2 * GLYPH } : blue) };
      added.push({ ...ev, frame: 50, props: p });
    }
    const third = added.find((ev) => ev.element === TOP_THIRD && ev.frame === 40); if (third) added.push({ ...third, frame: 50, props: { ...third.props, ...white } });
    screen.events.push(...added); screen.labels = [...(screen.labels || []), { frame: 50, name: TOP_THIRD, control: ['10000400'] }];
    const lui = new LuiScreen(screen, this.ui.images, this.ui); lui.shapeScale = true; lui.unionFlat = true; lui.keepLead = true;
    Object.defineProperty(lui, 'flagWrap', { get: () => pv('resultsMenu') }); Object.defineProperty(lui, 'ps2Wrap', { get: () => pv('luiWrap') });
    this.lui['61toptimes+'] = lui; return true;
  }
  // 70peakchal_results: title1 / title2, 'Event Results', the target and the player's value (labels right-aligned), the
  // message, items (Transport / Restart / Quit), index, noItems.
  peakResults(c, { title, sub, targetLabel, target, yourLabel, yours, message, items, index, disabled = () => false, noItems = false }) {
    if (!this.lui['70peakchal_results']) return false;
    this.panelFrame(c, this.clock());
    return this.screen(c, '70peakchal_results', 50 + 10 * Math.max(0, Math.min(2, index)), (e) => {
      const n = e.name, P = PEAK;
      if (n === P.title1) return { text: title };
      if (n === P.title2) return { text: sub };
      if (n === P.targetLabel) return { text: targetLabel };
      if (n === P.target) return { text: target };
      if (n === P.yourLabel) return { text: yourLabel };
      if (n === P.yours) return { text: yours };
      if (n === P.help) return message ? { text: message } : { hidden: true };
      const k = P.items.indexOf(n);
      if (k >= 0) { if (noItems || k >= items.length) return { hidden: true }; return { text: items[k], ...(disabled(k) && k !== index ? { alpha: 128 } : {}) }; }
      if (n === '069d01a0' || (n === EL.ps2x && e.parent === '069d01a0')) return noItems || index >= items.length ? { hidden: true } : null;
      return null;
    });
  }
  // 40race_pre (the race round card): title, sub (the round), objective (objTextLine1, wrapped to 480, with its bullet),
  // riders [{name, human, note}] (six rows at 23 px; the human's 252,177,101: PS2 menus/race/state-round-objectives.png 'Zoe'
  // 202,142,81 = 0.8 x), record (the 'Record time:' value, or null: both hidden), continueLabel.
  raceCard(c, { title, sub, objective, riders, recordLabel, record, continueLabel }) {
    if (!this.lui['40race_pre']) return false;
    this.panelFrame(c, this.clock());
    return this.screen(c, '40race_pre', 30, (e) => {
      const n = e.name, R = CARD;
      if (n === R.track) return { text: title };
      if (n === R.title) return { text: sub };
      if (R.tabs.includes(n)) return { hidden: true };
      if (n === R.objective) return objective ? { text: objective } : { hidden: true };
      if (n === R.bullet) return objective ? null : { hidden: true };
      if (n === R.recordLabel) return record != null ? { text: recordLabel } : { hidden: true };
      if (n === R.record) return record != null ? { text: record } : { hidden: true };
      if (n === R.item) return { text: continueLabel };
      const k = R.rows.indexOf(n);
      if (k >= 0) { const r = riders[k]; if (!r) return { hidden: true }; return { text: r.name, ...(r.human ? { props: colour(CARD_HUMAN) } : {}), ...(r.note ? { alpha: 128, props: { 6: 400 } } : {}) }; }   // the port's note on one line
      return null;
    });
  }
  // 62reward_list (overlay 0x10): title (title_rewardlisting, 85 %), 'Rewards', the intro line, nine list rows 22 px apart from
  // (120, 184) (the focused row white: focus frames 40 + 5 k), the up / down scroll arrows (OV_1-1 sprites at (71, 179) / (71, 382))
  // and Continue with ps2x. lines [{text, indent}] from `top`; `at` the focused line; an indented line is "      %s" (0x1FF7B8).
  awards(c, { title, sub, intro, lines, top, at, cont }) {
    if (!this.lui['62reward_list']) return false;
    this.panelFrame(c, this.clock());
    // 0x1FFD08: a list of fewer than 10 lines paints every row teal (A,R,G,B 1, 101,184,201 / 255: gp-0x553C..) and takes no
    // Up / Down (0x1FF798 returns 0), so no row is focused (PS2 ctm-parity/runs/race-f/final.png); a longer list scrolls with the
    // focused row white (race-f95/final.png)
    const k0 = Math.max(0, Math.min(8, at - top)), still = lines.length < 10;
    return this.screen(c, '62reward_list', still ? 30 : 40 + 5 * k0, (e) => {
      const n = e.name, W = REWARD;
      if (n === W.title) return { text: title };
      if (n === W.sub) return { text: sub };
      if (n === W.intro) return { text: intro };
      if (n === W.cont) return { text: cont };
      if (n === W.up) return top > 0 ? null : { hidden: true };
      if (n === W.down) return lines.length > top + 9 ? null : { hidden: true };
      const k = W.rows.indexOf(n);
      if (k >= 0) { const l = lines[top + k]; return l ? { text: (l.indent ? '      ' : '') + l.text, ...(still ? { props: colour([101, 184, 201]) } : {}) } : { hidden: true }; }
      return null;
    });
  }
  // 68rival_pre (the rival / peak run card, 0x1FD190): title1 / title2, the headline (name, 73 %, wrapped to 475), up to two
  // objective lines (obj1 / obj2 at y 215 / 280, 60 %, teal) with their bullets, the target (timescoretobeat) and Continue.
  rivalCard(c, { title, sub, headline, bullets, target, cont }) {
    if (!this.lui['68rival_pre']) return false;
    this.panelFrame(c, this.clock());
    return this.screen(c, '68rival_pre', 30, (e) => {
      const n = e.name, V = RIVAL;
      if (n === V.title1) return { text: title };
      if (n === V.title2) return { text: sub };
      if (n === V.headline) return { text: headline };
      if (n === V.target) return target ? { text: target } : { hidden: true };
      if (n === V.cont) return { text: cont };
      let k = V.objs.indexOf(n); if (k >= 0) return bullets[k] ? { text: bullets[k] } : { hidden: true };
      k = V.bullets.indexOf(n); if (k >= 0) return bullets[k] ? null : { hidden: true };
      return null;
    });
  }
}
const RIVAL = { title1: '07b0b281', title2: '07b0b282', headline: '00074835', target: '0b26aa84', cont: '0a6a1cf1', objs: ['000758d1', '000758d2'], bullets: ['09c32cb1', '09c32cb2'] };
const REWARD = { title: '0ebd1347', sub: '09edbde4', intro: '0038acf4', cont: '0fbd0534', up: '000007c0', down: '0006b6de',
  rows: ['00000030', '00000031', '00000032', '00000033', '00000034', '00000035', '00000036', '00000037', '00000038'] };
export const RECORD_ROW = Object.freeze([250, 135, 18]), CARD_HUMAN = Object.freeze([252, 177, 101]);
const CARD = { track: '01abc714', title: '007b0b25', tabs: ['085c309c', '04df1f42', '070c569c'], objective: '01233101', bullet: '069c32c4',
  recordLabel: '0d0ecebc', record: '0b65b0d5', item: '0263fe50', rows: ['078fac51', '078fac52', '078fac53', '078fac54', '078fac55', '078fac56'] };
const TOP_THIRD = '084d7134';   // pv onlineRecords: 61toptimes' added third item (topTimesThree)
const TOP = { track: '07a198f8', times: '0c850ae4', scores: '0c9904a3', time: '0007b035', points: '077605b3', help1: '07a12f51', help2: '07a12f52',
  xgroup: '0f55f740', items: ['065b08f5', '084d7133'],
  rows: { rank: ['00000331', '00000332', '00000333', '00000334', '00000335'], name: ['07483831', '07483832', '07483833', '07483834', '07483835'],
    rider: ['07e0ab31', '07e0ab32', '07e0ab33', '07e0ab34', '07e0ab35'], value: ['07b03831', '07b03832', '07b03833', '07b03834', '07b03835'] } };
const PEAK = { title1: '07b0b281', title2: '07b0b282', targetLabel: '08feed45', target: '085c1ea5', yourLabel: '024cbae5', yours: '03c99085', help: '0c37a134',
  items: ['0263fe50', '0263fe51', '0263fe52'] };
// an override's props patch: the LUI text colour (props 14..16); lui-player merges it over the element's props
function colour([r, g, b]) { return { 14: r, 15: g, 16: b }; }
