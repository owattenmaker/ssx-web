// The Conquer the Mountain PDA screens drawn from the original OV.LUI records (tools/export_ctm_screens.py ->
// /assets/UI/ctm-screens.json; docs/ctm-parity.md): the MCOMM frame every PDA overlay sits in (PDATemplate + the bganim1
// snowflakes, 0x20A778), the MCOMM menu (31paus_freeride, cOVTemplate_PauseMenu 0x1F8168) and the Yes / No popup
// (87yndialog). web/career-ui.js calls it for the free-ride MCOMM, the career pause and their prompts; without the export
// it keeps its own drawing.
import { LuiScreen } from './lui-player.js';
import { nameHash } from './locale.js';
import { pv } from './pv-flags.js';   // the LUI widget name hash (tools/sam_ps2/loc_file.py name_hash): "%s group" lookups

const SY = 448 / 480;
// Row icons (0x1F8448): "<icon> group" of table 0x441C30 by item id, shown at y = 40 x row; the other groups are hidden.
export const ICON = Object.freeze({ return: 'conticon', transport: 'transporticon', restart: 'rstarticon', options: 'opticon', audio: 'radioicon',
  quit: 'hexicon', session: 'sessionicon', map: 'mapicon', messages: 'messicon' });
const ICON_GROUPS = ['transporticon', 'conticon', 'rstarticon', 'opticon', 'radioicon', 'hexicon', 'sessionicon', 'mapicon', 'messicon'];
const ROWS = ['00000030', '00000031', '00000032', '00000033', '00000034', '00000035', '00000036', '00000037'];
const INTRO = 60;          // 31paus_freeride: the menu and the icon strip slide in by frame 60
const FOCUS_LEN = 90;      // an icon's focus state: its colour at F, its animations from F + 1 / F + 10, reset at F + 90

// The badge temperature 0x20A854: -10 - (G+0x1C / 3600) % 15 (G+0x1C counts frames; now in 60 Hz frames).
export const pdaTemperature = (frames) => -10 - (Math.floor(frames / 3600) % 15);

export class CtmPda {
  constructor(ui) { this.ui = ui; this.data = null; this.lui = {}; this.opened = 0; this.focus = { row: -1, at: 0 }; this.popupAt = 0; this.popupKey = null; }
  async load() {
    try {
      const data = await (await fetch('/assets/UI/ctm-screens.json')).json(), images = this.ui.images;
      await Promise.all(data.pages.filter((p) => !images[p]).map(async (p) => { const im = new Image(); im.src = `/assets/UI/${p}.png`; await im.decode(); images[p] = im; }));
      for (const [key, screen] of Object.entries(data.screens)) { this.lui[key] = new LuiScreen(screen, images, this.ui); this.lui[key].shapeScale = true; }
      this.groups = this.iconGroups(data.screens['31paus_freeride']);
      this.data = data;
    } catch (e) { console.warn('CTM PDA screens missing (python3 tools/export_ctm_screens.py)', e); }
  }
  get ready() { return !!this.data; }
  now() { return performance.now() * 60 / 1000; }
  // the icon group of each table name, its descendants and its focus frames (the frames of the events touching them)
  iconGroups(screen) {
    const kids = new Map(); for (const e of screen.elements) { const p = e.parent; if (!kids.has(p)) kids.set(p, []); kids.get(p).push(e.name); }
    const desc = (n) => [n, ...(kids.get(n) || []).flatMap(desc)];
    const out = {};
    for (const icon of ICON_GROUPS) {
      const name = nameHash(`${icon} group`).toString(16).padStart(8, '0'); if (!screen.elements.some((e) => e.name === name)) continue;
      const set = new Set(desc(name)), frames = screen.events.filter((ev) => set.has(ev.element)).map((ev) => ev.frame);
      out[icon] = { name, set, first: Math.min(...frames) };
    }
    return out;
  }
  // PDATemplate at the time the PDA opened (0x20A778: 39C870(template, 0, 0)), bganim1 looping behind it.
  frame(c, b) {
    const t = this.now() - this.opened, tmpl = this.lui.PDATemplate, snow = this.lui.bganim1; if (!tmpl) return false;
    // 0x20A854: -10 - (G+0x1C / 3600) % 15, '%d°%C'. pv mcommIcons: set once when the PDA template is built (0x20A778, the PDA
    // opening), as the PS2 writes the text there and never again; before, it was worked out every frame.
    const temperature = pv('mcommIcons') && this.temperature != null ? this.temperature : pdaTemperature(this.now());
    const live = (screen, f) => screen.screen.events.filter((ev) => ev.frame <= f).map((ev) => ({ ev, start: ev.frame }));
    b.save(); b.scale(1, SY);
    const last = Math.max(...snow.screen.events.map((ev) => ev.frame)) + 1, sf = t % last;
    const text = (e) => (e.kind === 'text' && /%d/.test(e.text || '') ? { text: `${temperature}°C` } : null);
    tmpl.draw(b, live(tmpl, t), t, text, (layer) => layer < 11);
    snow.draw(b, live(snow, sf), sf);
    b.restore();
    c.save(); c.scale(1, SY); tmpl.draw(c, live(tmpl, t), t, text, (layer) => layer >= 11); c.restore();
    return true;
  }
  opening() { this.opened = this.now(); this.focus = { row: -1, at: 0 }; this.temperature = pdaTemperature(this.opened); }
  // The MCOMM menu: rows [{label, icon, disabled}], the focused row, the help text of that row, the legend (Select / Previous).
  menu(c, rows, index, help, { legend = true } = {}) {
    const lui = this.lui['31paus_freeride']; if (!lui) return false;
    // pv mcommIcons: each row icon is one flat colour; filled triangle by triangle, the canvas leaves anti-aliased seams along
    // the internal edges (the hexagon, the arrows), which the GS does not have: fill each flat shape as one path.
    lui.unionFlat = pv('mcommIcons');
    const now = this.now(), t = Math.max(0, now - this.opened);
    if (this.focus.row !== index) this.focus = { row: index, at: now };
    const focusIcon = rows[index]?.icon, g = focusIcon && this.groups[focusIcon], ft = now - this.focus.at;
    const events = [];
    for (const ev of lui.screen.events) {
      if (ev.frame <= INTRO) { if (ev.frame <= t) events.push({ ev, start: ev.frame }); continue; }
      // the focused row's icon plays its focus state (colour + animations) from the focus change on; the others stay at rest
      if (g && g.set.has(ev.element) && ev.frame >= g.first && ev.frame < g.first + FOCUS_LEN && ev.frame - g.first <= ft) events.push({ ev, start: this.focus.at - this.opened + (ev.frame - g.first) });
    }
    const shown = new Map(rows.map((r, i) => [r.icon, i]));
    const override = (e) => {
      if (this.qaHide?.has(e.name)) return { hidden: true };   // QA (?qa=1): hide widgets by name
      const k = ROWS.indexOf(e.name);
      if (k >= 0) { const r = rows[k]; if (!r) return { hidden: true }; return { text: r.label, props: k === index ? { 14: 255, 15: 255, 16: 255 } : r.disabled ? { 13: 96 } : null }; }
      for (const [icon, grp] of Object.entries(this.groups)) if (e.name === grp.name) return shown.has(icon) ? { props: { 0: 0, 1: 40 * shown.get(icon), 13: 255 } } : { hidden: true };
      if (e.label === 'help0') return { text: help || '' };
      if (e.label === 'ps2L1' || e.name === '08657f34') return { hidden: true };   // the online Chat entry
      if (!legend && (e.label === 'btext1' || e.name === '069acf72' || e.label === 'ps2x' || e.label === 'ps2tri')) return { hidden: true };
      return null;
    };
    c.save(); c.scale(1, SY);
    lui.draw(c, events, t, override);
    c.restore();
    return true;
  }
  // An OV.LUI popup with a menu (63bc_start: rest 60, Yes 65 / No 70; 90bc_fail: rest 60, Yes 65 / No 70 / Challenge Info 75):
  // its intro up to the rest frame, then the focused item's frame; `texts` replaces a text element's string by label or name.
  popup(c, key, rest, focus, index, texts = {}) {
    const lui = this.lui[key]; if (!lui) return false;
    const tag = key + '|' + JSON.stringify(texts); if (tag !== this.popupKey) { this.popupKey = tag; this.popupAt = this.now(); }
    const t = this.now() - this.popupAt, events = [];
    for (const ev of lui.screen.events) if (ev.frame <= rest ? ev.frame <= t : ev.frame === focus[index]) events.push({ ev, start: ev.frame });
    // one line per item, as on the PS2 ('Challenge Info' runs past its 75-unit box: bigchal/sd-run-stop.png)
    const override = (e) => { const v = texts[e.label] ?? texts[e.name]; return v == null ? null : typeof v === 'object' ? v : { text: v }; };
    c.save(); c.scale(1, SY); lui.draw(c, events, t, override); c.restore();
    return true;
  }
  // The Yes / No popup (87yndialog): up to two title lines, Yes / No with the Cross on the focused one.
  dialog(c, lines, index) {
    const lui = this.lui['87yndialog']; if (!lui) return false;
    const key = lines.join('|'); if (key !== this.popupKey) { this.popupKey = key; this.popupAt = this.now(); }
    const t = this.now() - this.popupAt, model = lui.screen;
    const focusFrame = index === 0 ? 35 : 41, events = [];
    for (const ev of model.events) if (ev.frame <= 30 ? ev.frame <= t : ev.frame === focusFrame) events.push({ ev, start: ev.frame });
    const [a, b2] = lines.length > 1 ? lines : [lines[0], ''];
    const override = (e) => {
      // one line each, as on the PS2 ('Save progress before quitting?' is not wrapped: quit-ctm s480)
      if (e.label === 'title_quit') return { text: a, props: { 6: 440 } };
      if (e.label === 'title_quitchallenge') return b2 ? { text: b2, props: { 6: 440 } } : { hidden: true };
      if (e.label === 'yes') return { text: this.ui.careerUI?.t('kT_CMNYes', 'Yes') ?? 'Yes' };
      if (e.label === 'no') return { text: this.ui.careerUI?.t('kT_CMNNo', 'No') ?? 'No' };
      return null;
    };
    c.save(); c.scale(1, SY); lui.draw(c, events, t, override); c.restore();
    return true;
  }
}
