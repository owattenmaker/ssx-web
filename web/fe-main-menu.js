// The original Main Menu screen 07main_men (FE.LUI 0x08065FDE; tools/export_fe_menus.py -> UI/fe-menus.json),
// played by web/lui-player.js. web/ui.js keeps the menu logic ('main' items, which ones are enabled, Cross/Triangle,
// Square = Options); this draws the screen: intro to frame 21, the focus states 35 / 40 / 50 / 60 / 70 (text white,
// the orange 'hl' bar, that item's help text on the left panel), greyed items at alpha 96 (0x194498), the
// bg_snow_loop overlay. PS2 reference: local/ps2-capture/menus/ctm/01-main-menu.png.
import { LuiScreen } from './lui-player.js';
import { menuModel } from './fe-screens.js';

const SY = 448 / 480, FPS = 60, DISABLED_ALPHA = 128, ROOT = '/assets/UI/';
export const MAIN_LUI = '07main_men';

export class FeMainMenu {
  constructor(ui) { this.ui = ui; this.data = null; this.model = null; this.images = {}; this.lui = null; this.enterAt = 0; this.focusAt = 0; this.lastIndex = -1; }
  get ready() { return !!this.model; }
  now() { return performance.now() * FPS / 1000; }
  async load() {
    try {
      const data = await (await fetch(ROOT + 'fe-menus.json')).json(), screen = data?.screens?.[MAIN_LUI], model = screen && menuModel(screen);
      if (!model || model.items.length !== 5) return false;
      const have = this.ui.characterSelect?.images || {};
      await Promise.all((data.pages || []).filter((p) => !have[p]).map(async (p) => { try { const im = new Image(); im.src = ROOT + p + '.png'; await im.decode(); this.images[p] = im; } catch {} }));
      const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop;
      const merged = snow ? { ...screen, elements: [...screen.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))], animations: { ...screen.animations, ...snow.animations } } : screen;
      this.lui = new LuiScreen(merged, { ...have, ...this.images }, this.ui);
      this.data = data; this.model = model;
      return true;
    } catch (e) { console.warn('Main Menu screen unavailable', e); this.model = null; return false; }
  }
  owns(screen) { return this.ready && screen === 'main'; }
  enter(screen, from) { if (screen !== 'main') return; const now = this.now(); if (from !== screen) this.enterAt = now; this.focusAt = now; this.lastIndex = this.ui.index; }
  // Pointer rows on the 640x448 UI: menu (-18,-7) + the texts (348, 144 + 25 i), right-anchored 300 wide.
  layout(i) {
    const by = this.lui?.byName, e = by?.get(this.model?.items[i]); if (!e) return [0, -100, 1, 1];
    const mp = by.get(this.model.menu)?.props || {}, p = e.props || {}, w = p[6] || 300, h = p[7] || 20;
    const x = (mp[0] || 0) + (p[0] || 0) - ((p[12] ?? 9) & 32 ? w : 0), y = (mp[1] || 0) + (p[1] || 0);
    return [x, y * SY, w, h * SY];
  }
  draw(c, b) {
    if (!this.ready) return false;
    const now = this.now(), frame = now - this.enterAt, dis = this.ui.mainDisabled();
    if (this.ui.index !== this.lastIndex) { this.lastIndex = this.ui.index; this.focusAt = now; }
    const focus = this.model.frames[this.ui.index], start = this.focusAt - this.enterAt, events = [];
    for (const ev of this.lui.screen.events) {
      if (ev.frame <= this.model.intro && ev.frame <= frame) events.push({ ev, start: ev.frame });
      else if (ev.frame === focus) events.push({ ev, start });
    }
    const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) events.push({ ev, start: frame - sf + ev.frame });
    const items = new Map(this.model.items.map((n, i) => [n, i]));
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    this.lui.draw(c, events, frame, (e) => (items.has(e.name) && dis[items.get(e.name)] ? { alpha: DISABLED_ALPHA } : null));
    c.restore();
    return true;
  }
}
