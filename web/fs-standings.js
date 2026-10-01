// The Conquer the Mountain freestyle heat results: OV.LUI 42freestyle_standings (tools/export_fs_standings.py ->
// /assets/UI/fs-standings.json; fsStandings, docs/ctm-parity.md "Freestyle standings") in the OV_darkblue panel of
// web/results-lui.js. web/career-ui.js draws the qualifying heats (rounds 1 and 2) through it; the final keeps
// 43final_standings.
//   0x1E5B80 (build): track_event '<course> - <event>', title kT_OVRCMNQFHeat1Stand / kT_OVRCMNQFHeat2Stand; the header "Total:"
//     hidden after heat 1; row k (rank order 0x536708) rider_k = the rider's name, run_1.k = its heat 1 score (238590 slot, 0),
//     'DNF' for the human who gave up in heat 1; after heat 1: run_2.k '- - -' and total_k hidden; after heat 2 (or when heat 1
//     already qualified for the final, GMM+0x70 = 3: every rider's heat 2 and total, the human's heat 2 '- - -'): run_2.k =
//     heat 2 (slot, 1), total_k = heat 1 + heat 2, the human's 'DNF' (total hidden) when given up.
//     Help: heat 1 'You are currently in Nth place.' (help_current0N, not after a give up; kT_CMNHELPEarnedEnoughPoints when
//     heat 1 qualified for the final), heat 2 help_advance (top three) or help_sorry.
//   0x1E64C8 (show): the human's rider_N and run_<round>.N pulse (@TextPulseWhiteOrange, mode 3: orange 255,128,0 to white
//     over 30 frames), UI sound 14.
// The heat card before each round is OV.LUI 41freestyle_pre (0x1FBD20 build, 0x1FC768 show): the round's tab (heat1_tab /
// heat2_tab / finalheat_tab), objText1 / objText2 (the second and its bullet hidden when there is one line), 'Current standings':
// the three best posted riders (ranking 0x536708 without the human) with Heat 1 (or the final's posted score and the header
// 'Score' in the final), Heat 2 and Total only before heat 2 (column_secondrun / column_total hidden in heat 1 and the final);
// 'Up next' the human (p0rider) with p0run1 = the heat 1 score before heat 2, else '- - -', and p0dashes under Heat 2; the
// record score; Continue. The show pulses p0rider and p0dashes (heat 2) or p0run1.
import { LuiScreen } from './lui-player.js';

const SY = 448 / 480;
export const DASHES = '- - -';   // 0x4A2168
export const DNF = 'DNF';        // 0x4A2160

// Rows and texts of 0x1E5B80. rows: [{name, human, rank (0-based), heat1, heat2, dnf}] (heat1 / heat2: numbers, heat2 null
// before it is ridden); round 1 | 2; qualified: heat 1 put the human in the final (GMM+0x70 = 3).
export function standingsModel({ round, rows, qualified = false }) {
  const full = round === 2 || (round === 1 && qualified);
  const human = rows.find((r) => r.human);
  const out = [...rows].sort((a, b) => a.rank - b.rank).map((r) => {
    const o = { name: r.name, human: !!r.human, rank: r.rank + 1 };
    o.heat1 = r.human && r.dnf && round === 1 ? DNF : String(r.heat1 ?? 0);
    if (!full) { o.heat2 = DASHES; o.total = null; }
    else if (r.human && r.dnf) { o.heat2 = DNF; o.total = null; }
    else if (r.human && round === 1) { o.heat2 = DASHES; o.total = String(r.heat1 ?? 0); }
    else { o.heat2 = String(r.heat2 ?? 0); o.total = String((r.heat1 ?? 0) + (r.heat2 ?? 0)); }
    return o;
  });
  const place = human ? human.rank : -1, dnf = !!human?.dnf;
  let help = null;
  if (!dnf && round === 1 && place >= 0) help = { key: qualified ? 'earned' : 'current', place: Math.min(place, 5) + 1 };   // help_current0N (its text replaced when qualified)
  if (!dnf && round === 2) help = { key: place < 3 ? 'advance' : 'sorry' };
  return { rows: out, totalHeader: round !== 1, help, pulse: out.findIndex((r) => r.human), pulseRun: round };
}

// The card's rows: posted [{name, heat1, heat2, final}] (the computer riders), round 1 | 2 | 3, human {name, heat1}.
export function cardModel({ round, posted, human }) {
  const value = (r) => (round === 3 ? r.final : round === 2 ? r.heat1 + r.heat2 : r.heat1);
  const rows = [...posted].sort((a, b) => value(b) - value(a)).slice(0, 3).map((r) => ({
    name: r.name, heat1: String(round === 3 ? r.final ?? 0 : r.heat1 ?? 0), heat2: round === 2 ? String(r.heat2 ?? 0) : null, total: round === 2 ? String(value(r)) : null }));
  return { round, rows, columns: round === 2, tab: ['heat1_tab', 'heat2_tab', 'finalheat_tab'][round - 1], scoreHeader: round === 3,
    human: { name: human.name, run1: round === 2 ? String(human.heat1 ?? 0) : DASHES }, pulse: round === 2 ? 'p0dashes' : 'p0run1' };
}

export class FsStandings {
  constructor(ui) { this.ui = ui; this.data = null; this.lui = null; this.shownAt = 0; this.lastDraw = -1e9; }
  async load() {
    try {
      const r = await fetch('/assets/UI/fs-standings.json'); if (!r.ok) return;
      const data = await r.json(), images = this.ui.images;
      await Promise.all(data.pages.filter((p) => !images[p]).map(async (p) => { const im = new Image(); im.src = `/assets/UI/${p}.png`; await im.decode(); images[p] = im; }));
      // flagWrap: only a text element with flag 0x80 wraps (0x3A0528), so 'Current standings' stays on one line as on the PS2
      const make = (screen) => { const l = new LuiScreen(screen, images, this.ui); l.shapeScale = true; l.unionFlat = true; l.keepLead = true; l.flagWrap = true; return l; };
      const screen = data.screens['42freestyle_standings'];
      this.lui = make(screen);
      this.by = new Map(screen.elements.filter((e) => e.label).map((e) => [e.label, e.name]));
      const pre = data.screens['41freestyle_pre'];
      if (pre) { this.pre = make(pre); this.preBy = new Map(pre.elements.filter((e) => e.label).map((e) => [e.label, e.name])); }
      this.data = data;
    } catch (e) { console.warn('Freestyle standings panel missing (python3 tools/export_fs_standings.py)', e); }
  }
  get ready() { return (!!this.data) && !!this.ui.careerUI?.resultsLui?.ready; }
  get cardReady() { return this.ready && !!this.pre; }
  now() { return performance.now() * 60 / 1000; }
  // m: standingsModel(...); title / sub: the two header lines; items: the menu labels; index: focus; strings: t(key, fallback)
  draw(c, { title, sub, model, items, index, disabled = () => false, noItems = false, t = (k, f) => f }) {
    const panel = this.ui.careerUI.resultsLui;
    panel.panelFrame(c, panel.clock());
    const now = this.now(); if (now - this.lastDraw > 15) this.shownAt = now; this.lastDraw = now;   // the pulse starts when it shows (0x1E64C8)
    const lui = this.lui, name = (label) => this.by.get(label), focus = 40 + 10 * Math.max(0, Math.min(4, index));
    const events = lui.screen.events.filter((ev) => ev.frame === 30 || ev.frame === focus).map((ev) => ({ ev, start: ev.frame }));
    const tp = now - this.shownAt;
    if (model.pulse >= 0)
      for (const l of [`rider_${model.pulse + 1}`, `run_${model.pulseRun}.${model.pulse + 1}`])
        events.push({ ev: { element: name(l), anim: this.data.pulse, mode: 3, frame: 0 }, start: 1000 });
    const helpShown = !model.help ? null : model.help.place ? `help_current0${model.help.place}` : `help_${model.help.key}`;
    const labelOf = new Map([...this.by].map(([l, n]) => [n, l]));
    const override = (e) => {
      const L = labelOf.get(e.name);
      if (L === 'track_event') return { text: title };
      if (L === 'title') return { text: sub };
      if (L === 'screenData') return { alpha: 255 };
      if (L === 'total') return model.totalHeader ? null : { hidden: true };
      let m = /^rider_(\d)$/.exec(L || ''); if (m) { const r = model.rows[m[1] - 1]; return r ? { text: r.name } : { hidden: true }; }
      m = /^run_([12])\.(\d)$/.exec(L || ''); if (m) { const r = model.rows[m[2] - 1]; return r ? { text: m[1] === '1' ? r.heat1 : r.heat2 } : { hidden: true }; }
      m = /^total_(\d)$/.exec(L || ''); if (m) { const r = model.rows[m[1] - 1]; return r && r.total != null ? { text: r.total } : { hidden: true }; }
      if (/^help_/.test(L || '')) {
        if (L !== helpShown) return { hidden: true };
        return model.help.key === 'earned' ? { text: t('kT_CMNHELPEarnedEnoughPoints', this.data.strings.kT_CMNHELPEarnedEnoughPoints) } : null;
      }
      m = /^overlayOpt(\d)$/.exec(L || '');
      if (m) { const k = +m[1]; if (noItems || k >= items.length) return { hidden: true }; return { text: items[k], ...(disabled(k) && k !== index ? { alpha: 128 } : {}) }; }
      if (L === 'ps2x' && noItems) return { hidden: true };
      // the rank numbers '1'..'6': only as many as there are rows
      const rank = ['07885121', '07885122', '07885123', '07885124', '07885125', '07885126'].indexOf(e.name);
      if (rank >= 0 && rank >= model.rows.length) return { hidden: true };
      return null;
    };
    c.save(); c.scale(1, SY); lui.draw(c, events, 1000 + tp, override); c.restore();   // the layout settled (frame 30), the pulse tp frames in
    return true;
  }
  // The heat card (41freestyle_pre): m = cardModel(...); title (track_event); objectives [line1, line2?]; record {label, value}
  // or null; continueLabel; t(key, fallback).
  card(c, { title, model, objectives = [], record = null, continueLabel = 'Continue', t = (k, f) => f }) {
    const panel = this.ui.careerUI.resultsLui;
    panel.panelFrame(c, panel.clock());
    const now = this.now(); if (now - this.lastDraw > 15) this.shownAt = now; this.lastDraw = now;
    const lui = this.pre, by = this.preBy, labelOf = new Map([...by].map(([l, n]) => [n, l]));
    const events = lui.screen.events.filter((ev) => ev.frame === 30).map((ev) => ({ ev, start: ev.frame }));
    const tp = now - this.shownAt;
    for (const l of ['p0rider', model.pulse]) events.push({ ev: { element: by.get(l), anim: this.data.pulse, mode: 3, frame: 0 }, start: 1000 });
    const override = (e) => {
      const L = labelOf.get(e.name);
      if (L === 'track_event') return { text: title };
      if (L === 'title' || L === 'p1rider' || L === 'p1dashes') return { hidden: true };   // Multi Play only
      if (L === 'screenData') return { alpha: 255 };
      if (/_tab$/.test(L || '')) return L === model.tab ? null : { hidden: true };
      if (L === 'objText1') return objectives[0] ? { text: objectives[0] } : { hidden: true };
      if (L === 'objText2' || L === 'bullet1') return objectives[1] ? (L === 'objText2' ? { text: objectives[1] } : null) : { hidden: true };
      if (e.name === '069c32c4') return objectives[0] ? null : { hidden: true };   // the first bullet
      if (L === 'column_secondrun' || L === 'column_total') return model.columns ? null : { hidden: true };
      if (L === 'heat1') return model.scoreHeader ? { text: t('kT_OVRCMNScoreColon', this.data.strings.kT_OVRCMNScoreColon) } : null;
      let m = /^rider_(\d)$/.exec(L || ''); if (m) { const r = model.rows[m[1] - 1]; return r ? { text: r.name } : { hidden: true }; }
      m = /^run_([12])\.(\d)$/.exec(L || ''); if (m) { const r = model.rows[m[2] - 1]; return r ? { text: m[1] === '1' ? r.heat1 : r.heat2 ?? '' } : { hidden: true }; }
      m = /^total_(\d)$/.exec(L || ''); if (m) { const r = model.rows[m[1] - 1]; return r?.total != null ? { text: r.total } : { hidden: true }; }
      const rank = ['07885121', '07885122', '07885123'].indexOf(e.name); if (rank >= 0 && rank >= model.rows.length) return { hidden: true };
      if (L === 'p0rider') return { text: model.human.name };
      if (L === 'p0run1') return { text: model.human.run1 };
      if (L === 'topScore') return record ? { text: record.value } : { hidden: true };
      if (L === 'topScoreLabel') return record ? { text: record.label } : { hidden: true };
      if (L === 'overlayOpt0') return { text: continueLabel };
      return null;
    };
    c.save(); c.scale(1, SY); lui.draw(c, events, 1000 + tp, override); c.restore();
    return true;
  }
}
