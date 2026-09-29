// The lodge transition load screens (docs/cutscenes.md "Inventory" 14): FL.LUI 117loadinlodge (cFELoadStateInLodge,
// Yes at 'Would you like to enter the lodge?') and GL.LUI 118loadoutlodge (cGameLoadStateOutLodge, Return to Game):
// the handheld with the load percentage and 'Loading...' over the ice background, faded in and out from black like
// the event load screen (web/loading-screen.js). Assets: tools/export_transition_screens.py -> CUTSCENES/TRANSITIONS.
// Not skippable (the original waits for its load). ui.js draws it while the screen is 'transition'.
import { LuiScreen } from './lui-player.js';

const ROOT = '/assets/CUTSCENES/TRANSITIONS/';
const SY = 448 / 480, FPS = 60, FADE = 20, DONE = 12;
const PERCENT = '00000025';

export class TransitionScreens {
  constructor(ui) { this.ui = ui; this.data = null; this.images = {}; this.lui = {}; this.active = null; this.loading = null; }
  load() {
    this.loading ??= (async () => {
      const r = await fetch(ROOT + 'screens.json'); if (!r.ok) throw Error(`${ROOT}screens.json: ${r.status}`);
      this.data = await r.json();
      await Promise.all(this.data.pages.map(async (p) => { const im = new Image(); im.src = `${ROOT}${p}.png`; await im.decode(); this.images[p] = im; }));
      for (const [k, s] of Object.entries(this.data.screens)) this.lui[k] = new LuiScreen(s, this.images, this.ui);
      return true;
    })().catch((e) => { console.warn('Transition screens unavailable (python3 tools/export_transition_screens.py)', e); return false; });
    return this.loading;
  }
  // Plays `name` for at least minMs and until `work` settles; the percentage runs to 98% over the minimum time, then
  // 100% and the fade out. Resolves when done (immediately when the assets are missing).
  async run(name, { minMs = 1500, work = null } = {}) {
    if (!(await this.load()) || !this.lui[name]) { await work; return false; }
    return new Promise((resolve) => {
      const state = this.active = { name, start: performance.now(), minMs, done: false, doneAt: 0, resolve };
      Promise.resolve(work).catch(() => {}).then(() => { state.done = true; });
      const tick = () => {
        if (this.active !== state) return;
        const now = performance.now(), frame = (now - state.start) / 1000 * FPS;
        if (state.done && now - state.start >= minMs && !state.doneAt) state.doneAt = frame;
        if (state.doneAt && frame - state.doneAt > DONE + FADE) { this.active = null; resolve(true); return; }
        requestAnimationFrame(tick);
      };
      requestAnimationFrame(tick);
    });
  }
  draw(c) {
    const s = this.active, lui = s && this.lui[s.name]; if (!lui) return;
    const frame = (performance.now() - s.start) / 1000 * FPS, minFrames = s.minMs / 1000 * FPS;
    const pct = s.doneAt ? 100 : Math.min(98, Math.floor(98 * frame / Math.max(1, minFrames)));
    const events = lui.screen.events.filter((ev) => ev.frame <= frame).map((ev) => ({ ev, start: ev.frame }));
    c.save(); c.fillStyle = '#000'; c.fillRect(0, 0, 640, 448); c.scale(1, SY);
    lui.draw(c, events, frame, (e) => (e.name === PERCENT ? { text: `${pct}%` } : null));
    c.restore();
    const fade = s.doneAt ? Math.min(1, (frame - s.doneAt - DONE) / FADE) : 1 - Math.min(1, frame / FADE);
    if (fade > 0) { c.save(); c.globalAlpha = fade; c.fillStyle = '#000'; c.fillRect(0, 0, 640, 448); c.restore(); }
  }
}
