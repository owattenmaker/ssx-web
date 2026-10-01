// Main menu > Previews: the original FE.LUI screen 146Bonusmat (0x063496A4; tools/export_fe_menus.py ->
// UI/fe-menus.json), played by web/lui-player.js like web/fe-screens.js plays its screens.
//
//   fe-previews  146Bonusmat  cFEStateBonusMaterial ("Previews"): Need for Speed(TM) Underground / NFL STREET /
//                             NBA STREET Vol. 2, help "View trailers of other EA(TM) games.", legend Select / Previous
//
// Input (0x195600): Cross (event 5) allocates a MoviePlayer (0x1D22F0, state 0x1D23E0) on the path table
// 0x441128[item] = data\movies\nfsxsell.mpc / nflxsell.mpc / st3xsell.mpc (strings 0x460178 / 0x460198 / 0x4601B8)
// and pushes it; the item index comes from the focused widget's label '1'..'3' (0x195540 -> +0x18). Triangle
// (event 6) goes back to the Main Menu. The movie plays full screen with the sound system paused and pops back to
// this screen when it ends or is skipped (as the rewards room Videos: web/fe-movie.js). Nothing is disabled in the
// original; here a trailer that tools/export_movies.py has not exported is drawn at alpha 96 (0x194498) and skipped.
//
// Menu states (as the other FE screens): intro up to frame 25, focus states 30 / 38 / 48 (text white, the orange
// 'hl' bar at y 158 + 20i), the bg_snow_loop overlay, the white fade (10 frames) when leaving.
import { LuiScreen } from './lui-player.js';
import { menuModel, stepMenu, firstEnabled } from './fe-screens.js';
import { loadMovieIndex, startMovie } from './fe-movie.js';

const SY = 448 / 480, FPS = 60, FLASH = 10, DISABLED_ALPHA = 96;
export const PREVIEWS_SCREEN = 'fe-previews';
export const PREVIEWS_LUI = '146Bonusmat';
export const MAIN_PREVIEWS_INDEX = 3;                         // 07main_men: Single Event, CTM, Multi Play, Previews, Online
// 0x441128: item -> disc movie (tools/export_movies.py keys; movies.json rows carry preview = the item)
export const PREVIEW_KEYS = Object.freeze(['NFSXSELL', 'NFLXSELL', 'ST3XSELL']);
const ROOT = '/assets/UI/';

// ---- pure helpers (web/test-fe-previews.mjs) ----
export function previewsModel(screen) { return screen ? menuModel(screen) : null; }
// movies.json rows for the three items (by `preview`, else by key); null where not exported.
export function previewMovies(index) {
  const list = index?.movies || [];
  return PREVIEW_KEYS.map((key, i) => list.find((m) => m.preview === i) || list.find((m) => m.key === key) || null);
}
export function previewsDisabled(movies, failed = new Set()) { return movies.map((m, i) => !m?.src || failed.has(i)); }

export class FePreviews {
  constructor(ui) {
    this.ui = ui; this.data = null; this.model = null; this.movies = PREVIEW_KEYS.map(() => null); this.failed = new Set();
    this.images = {}; this.luiScreen = null; this.enterAt = 0; this.focusAt = 0; this.flash = null; this.movie = null;
  }
  get ready() { return !!this.model; }
  get playing() { return !!this.movie; }
  now() { return performance.now() * FPS / 1000; }

  // UI/fe-menus.json + its FE_1 pages + MOVIES/movies.json; never throws (ready stays false on a failure).
  async load({ fetchFn = globalThis.fetch } = {}) {
    try {
      const [data, index] = await Promise.all([
        fetchFn(ROOT + 'fe-menus.json').then((r) => (r.ok ? r.json() : null)),
        loadMovieIndex(fetchFn),
      ]);
      if (!this.init(data, index)) { console.info('Previews: UI/fe-menus.json not exported (python3 tools/export_fe_menus.py)'); return false; }
      if (typeof Image !== 'undefined') {
        const have = this.ui.characterSelect?.images || {};
        await Promise.all((data.pages || []).filter((p) => !have[p]).map(async (p) => {
          try { const im = new Image(); im.src = ROOT + p + '.png'; await im.decode(); this.images[p] = im; } catch {}
        }));
      }
      return true;
    } catch (e) { console.warn('Previews unavailable', e); this.model = null; return false; }
  }
  init(data, index) {
    const screen = data?.screens?.[PREVIEWS_LUI], model = previewsModel(screen);
    if (!model || model.items.length !== PREVIEW_KEYS.length) return false;
    this.data = data; this.model = model; this.luiScreen = null;
    this.movies = previewMovies(index);
    this.movies.forEach((m, i) => { if (!m) console.info(`Previews: trailer ${PREVIEW_KEYS[i]} not exported (python3 tools/export_movies.py ${PREVIEW_KEYS.join(' ')}); item disabled`); });
    return true;
  }
  // Built on first use, when Select Character's data (the bg_snow_loop overlay and the shared FE_1 pages) is loaded.
  lui() {
    if (this.luiScreen) return this.luiScreen;
    const screen = this.data.screens[PREVIEWS_LUI], snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop;
    const merged = snow
      ? {
          ...screen,
          elements: [...screen.elements, ...snow.elements.map((e) => ({ ...e, index: e.index + 1000 }))],
          animations: { ...screen.animations, ...snow.animations }
        }
      : screen;
    const images = { ...(this.ui.characterSelect?.images || {}), ...this.images };
    const lui = new LuiScreen(merged, images, this.ui);
    if (snow && Object.keys(images).length) this.luiScreen = lui;   // keep it once the shared data is in
    return lui;
  }
  owns(screen) { return this.ready && screen === PREVIEWS_SCREEN; }

  // ---- menu state ----
  items() { return this.model?.texts || []; }
  disabledList() { return previewsDisabled(this.movies, this.failed); }
  disabled(i) { return !this.ui.ready || !!this.disabledList()[i]; }
  // Pointer rows on the 640x448 UI: the menu (190,160) + the right-aligned 300-wide texts at y 20i.
  layout(i) {
    const screen = this.data?.screens?.[PREVIEWS_LUI], by = screen && new Map(screen.elements.map((e) => [e.name, e]));
    const e = by?.get(this.model?.items[i]); if (!e) return [0, -100, 1, 1];
    const mp = by.get(this.model.menu)?.props || {}, p = e.props || {}, w = p[6] || 300, h = p[7] || 20;
    const x = (mp[0] || 0) + (p[0] || 0) - ((p[12] ?? 9) & 32 ? w : 0), y = (mp[1] || 0) + (p[1] || 0);
    return [x, y * SY, w, h * SY];
  }
  focus() {
    const dis = this.disabledList();
    if (!(this.ui.index >= 0 && this.ui.index < dis.length)) this.ui.index = 0;
    if (dis[this.ui.index]) this.ui.index = firstEnabled(dis, this.ui.index);
    return this.ui.index;
  }

  enter(screen, from) {
    if (screen !== PREVIEWS_SCREEN) { this.stopMovie(false); this.flash = null; return; }
    const now = this.now();
    if (from !== screen) this.enterAt = now;
    this.focusAt = now;
  }

  // ---- input ----
  key(e) {
    if (!this.owns(this.ui.screen)) return false;
    if (['ArrowUp', 'ArrowDown', 'Enter', 'Space', 'Escape'].includes(e.code)) e.preventDefault?.();
    if (this.movie) { if (!e.repeat && ['Enter', 'Space', 'Escape'].includes(e.code)) this.stopMovie(); return true; }   // skip (0x1D2518); modal while it plays
    if (this.flash) return true;
    if (e.repeat && e.code !== 'ArrowUp' && e.code !== 'ArrowDown') return true;
    if (e.code === 'ArrowUp' || e.code === 'ArrowDown') {
      this.ui.index = stepMenu(this.focus(), e.code === 'ArrowUp' ? -1 : 1, this.disabledList()); this.focusAt = this.now(); this.ui.sync?.(); return true;
    }
    if (e.code === 'Enter' || e.code === 'Space') { this.choose(this.ui.index); return true; }
    if (e.code === 'Escape') { this.back(); return true; }
    return false;
  }
  choose(i) {
    if (!this.ui.ready || this.flash || this.movie || this.disabled(i)) return;
    this.ui.index = i; this.focusAt = this.now();
    this.play(i);
  }
  back() {
    if (this.flash || this.movie) return;
    this.flash = { at: this.now(), to: () => { this.ui.set('main'); this.ui.index = MAIN_PREVIEWS_INDEX; this.ui.sync?.(); } };
  }

  // ---- trailer (MoviePlayer, web/fe-movie.js) ----
  play(i) {
    const m = this.movies[i]; if (!m || this.movie) return false;
    const token = this.movie = { index: i };
    token.handle = startMovie(this.ui, m, (reason) => {
      if (reason === 'error') { this.failed.add(i); console.warn(`Previews: ${m.src} failed to play (python3 tools/export_movies.py ${PREVIEW_KEYS[i]})`); }
      if (this.movie === token) this.stopMovie();
    });
    if (!token.handle) { this.movie = null; return false; }
    this.ui.sync?.();
    return true;
  }
  stopMovie(sync = true) {
    const t = this.movie; if (!t) return; this.movie = null;
    t.handle?.stop();
    this.focusAt = this.now();
    if (sync) this.ui.sync?.();
  }

  // Clickable legend: Previous (Triangle) under the 'buttons' group (430,370) + (34,45).
  legend(nav) {
    if (!this.owns(this.ui.screen) || this.movie || typeof document === 'undefined') return;
    const b = document.createElement('button'); b.textContent = 'Previous'; b.setAttribute('aria-label', 'Previous'); b.disabled = !this.ui.ready;
    b.style.cssText = `left:${440 / 6.4}%;top:${410 * SY / 4.48}%;width:${110 / 6.4}%;height:${18 * SY / 4.48}%;`;
    b.onclick = () => this.back(); nav.appendChild(b);
  }

  // ---- drawing ----
  events(frame) {
    const lui = this.lui(), model = this.model, out = [], focus = model.frames[this.ui.index];
    const start = this.focusAt - this.enterAt;
    for (const ev of lui.screen.events) {
      if (ev.frame <= model.intro && ev.frame <= frame) out.push({ ev, start: ev.frame });
      else if (ev.frame === focus) out.push({ ev, start });
    }
    const snow = this.ui.characterSelect?.data?.screens?.bg_snow_loop?.events || [], sf = frame % 600;
    for (const ev of snow) if (ev.frame <= sf) out.push({ ev, start: frame - sf + ev.frame });
    return out;
  }
  draw(c, b) {
    const now = this.now();
    if (this.flash && now - this.flash.at >= FLASH) { const f = this.flash; this.flash = null; f.to(); return; }
    if (!this.ready) return;
    this.focus();
    const lui = this.lui(), frame = now - this.enterAt, dis = this.disabledList();
    const items = new Map(this.model.items.map((n, i) => [n, i]));
    b.fillStyle = '#75a9cb'; b.fillRect(0, 0, 640, 448);
    c.save(); c.scale(1, SY);
    lui.draw(c, this.events(frame), frame, (e) => (items.has(e.name) && dis[items.get(e.name)] ? { alpha: DISABLED_ALPHA } : null));
    if (this.flash) { c.fillStyle = `rgba(255,255,255,${Math.min(1, (now - this.flash.at) / FLASH)})`; c.fillRect(0, 0, 640, 480); }
    c.restore();
  }
}
