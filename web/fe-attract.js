// The front end's boot and attract movies (docs/intro-movies.md). The FE update 0x1A27A0 plays a movie while the movie
// mask gp-0x1724 is not empty, lowest bit first, and freezes the FE screens meanwhile (their update is skipped; 0x1A2208
// draws the movie instead):
//   bit 0  data\movies\eabig.mpc   EA SPORTS BIG          not skippable (+0x5AD8 = 0)
//   bit 1  data\movies\thx.mpc     THX                    not skippable
//   bit 2  the intro               Start (UIStart 0x70) or Cross (UINext 0x7A) on either pad skips it (320C48)
// The mask starts at 7 (0x4A19CC): a power-on plays EA SPORTS BIG, THX and the intro before the title. The intro is
// intro_dj.mpc (the DJ's voice on the centre channel) when the language is 0 and the audio object's one-shot +0x6C88
// (set at its init, read-and-cleared by 289DE0) is still set, so the boot intro; every later one is intro.mpc. Between
// two movies 0x5ADC holds the FE for one frame; the sound system is paused (2B3A70) from the first movie to the end of
// the last (2B3A98).
// The title's update 0x1948A8 counts frames (+0x48) from the moment "Press START button" shows; any button event on
// either pad resets it, and at 1801 (0x709) it resets and sets bit 2: the attract, intro.mpc over the frozen title,
// which comes back as it was when the movie ends or is skipped.
// Browser: the files are tools/export_movies.py EABIG THX INTRO_DJ INTRO (/assets/MOVIES, streamed only when played),
// drawn by web/fe-movie.js. pv `attract` runs the idle attract. pv `bootMovies` runs the power-on intro once per page load:
// after boot:ready (web/boot-screen.js; nothing streams before, so it never competes with the first load), over the title,
// with the mask DEFAULT_BOOT_MASK = the intro only (Owen, 2026-09-27: no EA SPORTS BIG / THX; ?bootlogos=1 plays the PS2's
// full mask 7). A boot movie stays transparent until its first frame plays and gives up after BOOT_START_MS (a slow link, a
// decode error, a refused autoplay): the title then simply stays. No boot intro for an event or online link
// (?course=..&autostart=1, ?online=1, ?lobby=..), ?qa=1, a page whose first screen after the load is not the title, or a
// later return to the title (a race's quit). Automated browsers (web/audio-engine.js testMuted) get neither switch unless
// the page asks with ?femovies=1 (which also lifts the ?qa=1 skip); headless Chrome counts as automated (automatedBrowser).
import { pv } from './pv-flags.js';
import { testMuted } from './audio-engine.js';
import { loadMovieIndex, startMovie } from './fe-movie.js';

export const IDLE_FRAMES = 0x709;                 // 1801: slti v1, v0, 0x709 (0x1948C4)
export const NTSC_HZ = 60000 / 1001;              // the PS2 counts NTSC frames
export const MASK = Object.freeze({ EABIG: 1, THX: 2, INTRO: 4 });
export const BOOT_MASK = 7;                       // gp-0x1724 = 7 in the executable's data
export const DEFAULT_BOOT_MASK = 4;               // the port's power-on: the (DJ) intro only
export const BOOT_START_MS = 2000;                // a boot movie that has not started playing by then is dropped
export const SKIP_KEYS = new Set(['Enter', 'Space']);   // Start = Enter, Cross = Space (web/gamepad-menus.js, touch deck)
const MAX_STEP_MS = 100;                          // a hidden tab or a stalled frame does not count as idle time

// The movie of the lowest set bit, and the mask without it. djOnce: 289DE0's one-shot flag (the first intro is the DJ cut).
export function nextMovie(mask, djOnce) {
  if (mask & MASK.EABIG) return { key: 'EABIG', skippable: false, mask: mask & ~MASK.EABIG, djOnce };
  if (mask & MASK.THX) return { key: 'THX', skippable: false, mask: mask & ~MASK.THX, djOnce };
  if (mask & MASK.INTRO) return { key: djOnce ? 'INTRO_DJ' : 'INTRO', fallback: 'INTRO', skippable: true, mask: mask & ~MASK.INTRO, djOnce: false };
  return null;
}

// The pure state (tested in node, web/test-fe-attract.mjs). step(dtMs, {idle}) advances the title's idle counter while
// `idle` (the title with "Press START button", no movie) and returns the movie to start, if any; input() is a pad event.
export function createAttractState({ attract = true, boot = false } = {}) {
  const s = { mask: boot ? BOOT_MASK : 0, bootBits: boot ? BOOT_MASK : 0, djOnce: true, frames: 0, hold: 0, playing: null };
  // pv attract without the boot sequence: the power-on intro (the DJ cut) never played, but a PS2 player never sees the DJ
  // cut in the attract either (the boot intro always took it), so the attract plays intro.mpc.
  if (!boot) s.djOnce = false;
  return {
    get state() { return s; },
    // the power-on sequence: these bits play first, marked `boot`, the first intro the DJ cut
    boot(mask = DEFAULT_BOOT_MASK) { s.mask |= mask; s.bootBits |= mask; s.djOnce = true; },
    input() { if (!s.playing) s.frames = 0; },
    step(dtMs, { idle = false } = {}) {
      if (s.playing) return null;
      if (s.hold > 0) { s.hold--; return null; }   // 0x5ADC: one frame between two movies
      if (!s.mask && idle && attract) {
        s.frames += Math.min(Math.max(0, dtMs), MAX_STEP_MS) * NTSC_HZ / 1000;
        if (s.frames >= IDLE_FRAMES) { s.frames = 0; s.mask |= MASK.INTRO; }
      }
      if (!s.mask) return null;
      const before = s.mask, m = nextMovie(s.mask, s.djOnce), bit = before & ~m.mask;
      m.boot = !!(s.bootBits & bit); s.bootBits &= ~bit; s.mask = m.mask; s.djOnce = m.djOnce;
      s.playing = m; return m;
    },
    ended() { s.playing = null; s.hold = 1; s.frames = 0; },
    // a key while a movie plays: 'skip' (Start / Cross on the intro), else 'eat' (the FE is frozen); null when no movie
    key(code) { if (!s.playing) return null; return s.playing.skippable && SKIP_KEYS.has(code) ? 'skip' : 'eat'; },
  };
}

// Automated browsers: web/audio-engine.js testMuted (navigator.webdriver, ?mute=1: WebKit driver, the visual-parity driver) and
// headless Chrome (the npm test harness, web/headless-chrome.mjs, reports navigator.webdriver false but 'HeadlessChrome' in its
// user agent).
export const automatedBrowser = (nav = globalThis.navigator, muted = testMuted) => !!muted || nav?.webdriver === true || /HeadlessChrome/.test(nav?.userAgent || '');
// The page's own address as it was loaded (web/main.js strips ?course / autostart / online / lobby once it has read them).
const PAGE_URL = (() => { try { return new URL(globalThis.location?.href ?? 'http://x/'); } catch { return new URL('http://x/'); } })();
function wanted(q = PAGE_URL.searchParams, automated = automatedBrowser()) {
  if (q.get('femovies') === '1') return true; if (q.get('femovies') === '0') return false;
  return !automated;
}
// Why the power-on intro is skipped for this page (null: it plays). q: the page's search params.
export function bootSkip(q, { automated = false } = {}) {
  const asked = q.get('femovies') === '1';
  if (q.get('femovies') === '0') return 'femovies=0';
  if (automated && !asked) return 'automated';
  if (q.get('autostart') === '1') return 'autostart';
  if (q.get('online') === '1' || q.has('lobby')) return 'online';
  if (q.has('qa') && !asked) return 'qa';
  return null;
}

// ui: web/ui.js OriginalUI. frame() is called from the title's draw; the capture-phase key listener swallows every key
// while a movie plays (the FE screens are frozen on the PS2) and resets the idle count otherwise.
export class FeAttract {
  constructor(ui) {
    this.ui = ui; this.on = { attract: pv('attract'), boot: pv('bootMovies') };
    const q = PAGE_URL.searchParams;
    this.enabled = (this.on.attract || this.on.boot) && wanted(q);
    this.logic = createAttractState({ attract: this.on.attract, boot: false });
    this.bootSkip = this.on.boot ? bootSkip(q, { automated: automatedBrowser() }) : 'off';
    this.bootMask = q.get('bootlogos') === '1' ? BOOT_MASK : DEFAULT_BOOT_MASK;
    this.bootDone = !!this.bootSkip; this.last = null; this.handle = null;
    if (!this.enabled || typeof addEventListener !== 'function') return;
    // Leaving the title before the power-on intro started (an event load, a menu, a race) ends its chance: a later return to
    // the title is not a boot.
    try { const st = ui.stage; if (st && typeof MutationObserver === 'function') { const mo = new MutationObserver(() => { if (st.dataset.screen !== 'title') { this.bootDone = true; mo.disconnect(); } }); mo.observe(st, { attributes: true, attributeFilter: ['data-screen'] }); } } catch {}
    const onKey = (e) => {
      const r = this.logic.key(e.code);
      if (!r) { if (this.ui.screen === 'title') this.logic.input(); return; }
      e.preventDefault(); e.stopImmediatePropagation();
      if (r === 'skip' && !e.repeat) this.stop('skip');
    };
    addEventListener('keydown', onKey, { capture: true });
    addEventListener('keyup', (e) => { if (this.logic.state.playing) { e.preventDefault(); e.stopImmediatePropagation(); } }, { capture: true });
    addEventListener('pointerdown', () => { if (this.ui.screen === 'title') this.logic.input(); }, { capture: true });
  }
  get playing() { return this.logic.state.playing; }
  // Called every drawn title frame (web/ui.js draw). `idle`: "Press START button" is up and nothing else is going on.
  frame(now = performance.now()) {
    if (!this.enabled) return;
    const ui = this.ui, dt = this.last == null ? 0 : now - this.last; this.last = now;
    const idle = ui.screen === 'title' && ui.ready && !(ui.error && ui.error !== 'Loading...') && !ui.titleOut;   // ui.error keeps 'Loading...' once loaded (the title's own line)
    // the power-on intro, once, when the first load is done (boot:ready): nothing streams before it
    const booted = !globalThis.ssxBoot || !!globalThis.ssxBoot.state?.ready;
    if (!this.bootDone && idle && booted) { this.bootDone = true; this.logic.boot(this.bootMask); }
    const m = this.logic.step(dt, { idle });
    if (m) this.play(m);
  }
  async play(m) {
    const token = this.token = {};
    const list = (await loadMovieIndex()).movies || [];
    if (this.token !== token) return;
    const entry = list.find((x) => x.key === m.key) || (m.fallback && list.find((x) => x.key === m.fallback));
    if (!entry) { console.warn(`FE movie ${m.key} not exported (python3 tools/export_movies.py ${m.key})`); this.logic.ended(); return; }
    // a boot movie: transparent until it plays; dropped when it has not started within BOOT_START_MS
    let guard = null;
    const opts = { skippable: m.skippable };
    if (m.boot) { opts.reveal = true; opts.onPlaying = () => { clearTimeout(guard); guard = null; }; }
    this.handle = startMovie(this.ui, entry, () => { clearTimeout(guard); if (this.token === token) this.finish(); }, opts);
    if (!this.handle) { this.finish(); return; }
    if (m.boot) guard = setTimeout(() => { if (this.token === token && this.handle) { this.dropped = (this.dropped || 0) + 1; this.stop(); } }, BOOT_START_MS);
  }
  finish() { this.handle = null; this.token = null; this.logic.ended(); }
  stop() { const h = this.handle; if (!h) return; h.stop(); this.finish(); }
}
