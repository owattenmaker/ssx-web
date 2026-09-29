// First paint and load meter of the title (docs/first-load.md). web/boot-plugin.js bundles this file with
// web/boot-progress.js, web/title-screen.js and web/lui-player.js into a classic script at the end of index.html's body,
// with its data in window.__SSX_BOOT__ (the title's LUI screens, FEFONT metrics, course names and the boot manifest:
// every file the title waits for with its size on the wire, the compute steps and the phase names). It runs while the
// page is still parsing, so the first paint is already the title (FE blue and the white ramp, then the logo, the
// mountains, the snow and the meter as their pictures arrive, ~200 KB), long before the game code (2 MB) has arrived.
//
// It draws the title on the UI canvases every animation frame until web/main.js takes over (ssxBoot.handoff(), when
// the game's frame loop starts); web/ui.js then draws the title through ssxBoot.draw(), so it is one screen, one
// renderer and one clock from the first paint to "Press START button".
//   ssxBoot.bytes(url, got, total)   streamed bytes of a game file (web/downloads.js onDownloadBytes)
//   ssxBoot.begin(step) / step(step, fraction)   compute steps (main.js: core, gpu, world; ai from the ai-race marks)
//   ssxBoot.ready() / fail(text)     the title shows "Press START button" / the error
// pv lazyCourse (web/main.js lazyStart, docs/first-load.md): the title waits for the front end only (the code, the core, the
// menus, the GPU); the course and its riders load behind the menus, and ssxBoot.courseProgress counts them for the event load
// screen (web/loading-screen.js workFraction: its percentage never runs ahead of them) until ssxBoot.courseDone().
import { createBootProgress, bootKey } from './boot-progress.js';
import { TitleScreen, TITLE_PAGES, FE_BLUE } from './title-screen.js';
import { SPRITE_2D } from './sprite-canvas.js';
import { pv } from './pv-flags.js';

const D = globalThis.__SSX_BOOT__ || {};
const now = () => performance.now(), t0 = now();
// = web/ui.js UI_CANVAS: a canvas keeps the attributes of its first getContext, and this is the first.
const UI_CANVAS = /[?&]uicanvas=gpu\b/.test(location.search) ? {} : { willReadFrequently: true };
const EVENT_COURSE = /^[A-E][A-Z]{2}\d$/;       // event courses share the boot course's file names (ARA1 -> BRA2 ...)

const query = new URLSearchParams(location.search), course = query.get('course') || D.course || 'ARA1';
const name = D.courseNames?.[course] || (course === D.course ? 'Snow Jam' : 'course');
const base = D.manifest || {};
const files = (base.files || []).filter(([p, , , phase]) => phase !== 'course' || course === D.course || EVENT_COURSE.test(course) || !p.startsWith(`/assets/${D.course}/`));
const manifest = { ...base, files, course: [D.course, EVENT_COURSE.test(course) ? course : D.course], phases: (base.phases || []).map(([id, label]) => [id, label.replace('%s', name)]) };
const lazy = pv('lazyCourse') && query.get('autostart') !== '1', FRONT = new Set(['code', 'core', 'fe', 'gpu']);
const part = (keep) => ({ ...manifest, files: files.filter(([, , , ph]) => keep(ph)), steps: (base.steps || []).filter(([, , ph]) => keep(ph)) });
const progress = createBootProgress(lazy ? part((ph) => FRONT.has(ph)) : manifest, { now, start: t0 });
const courseProgress = lazy ? createBootProgress(part((ph) => !FRONT.has(ph)), { now, start: t0 }) : null;   // the course, behind the menus
for (const [path, , , phase] of files) if (phase === 'code') progress.begin(path, t0);   // the code bundle is already on its way

// The title's pictures and FEFONT (preloaded by index.html); each one shows as soon as it has decoded.
const images = {}, fonts = { FEFONT: D.glyphs || null }, marks = new Set();
const mark = (n) => { if (marks.has(n)) return; marks.add(n); try { performance.mark(n); } catch {} };
for (const page of [...TITLE_PAGES, 'FEFONT-0']) {
  const im = new Image(); im.fetchPriority = 'high'; im.src = `/assets/UI/${page}.png`;
  im.decode().then(() => { images[page] = im; if (page === 'FE_1-20') mark('boot:logo'); }, () => {});
}
// FEFONT text exactly as web/ui.js OriginalUI.text (the glyph atlas tinted once per colour).
const tints = new Map();
const ui = {
  fonts, images,
  text(ctx, value, x, y, size = 19, color = '#101c28', font = 'FEFONT', align = 'left') {
    const glyphs = fonts[font], atlas = images[font + '-0']; if (!glyphs || !atlas) return;
    const scale = size / (font === 'FEFONT' ? 22 : 17);
    let total = 0; for (const ch of value) total += (glyphs[ch]?.advance || 10) * scale;
    if (align === 'right') x -= total; if (align === 'center') x -= total / 2;
    let im = tints.get(font + color);
    if (!im) { im = document.createElement('canvas'); im.width = atlas.width; im.height = atlas.height; const c = im.getContext('2d', SPRITE_2D); c.drawImage(atlas, 0, 0); c.globalCompositeOperation = 'source-in'; c.fillStyle = color; c.fillRect(0, 0, im.width, im.height); tints.set(font + color, im); }
    for (const ch of value) { const g = glyphs[ch]; if (!g) { x += 10 * scale; continue; } ctx.drawImage(im, g.x, g.y, g.w, g.h, x + g.dx * scale, y + g.dy * scale, g.w * scale, g.h * scale); x += g.advance * scale; }
  },
};
const title = D.title ? new TitleScreen(D.title, images, ui, { t0 }) : null;
const state = { ready: false, error: null, keyboard: false };

// Completed files (Resource Timing: images, the code bundle, fetches) and the computer riders' marks (web/ai-race.js).
const AI_RIDERS = 5; let aiDone = 0;
try { performance.setResourceTimingBufferSize?.(4000); } catch {}
try {
  const observer = new PerformanceObserver((list) => {
    for (const e of list.getEntries()) {
      if (e.entryType === 'resource') { if (progress.done(e.name, e.transferSize || 0) && bootKey(e.name) === D.mainScript) progress.step('js', 0, now(), { begin: true }); courseProgress?.done(e.name, e.transferSize || 0); continue; }
      if (!/^ai:.+:(create|done)$/.test(e.name)) continue;
      if (e.name.endsWith(':create')) { progress.step('ai', 0, now(), { begin: true }); courseProgress?.step('ai', 0, now(), { begin: true }); }
      else { aiDone++; for (const p of [progress, courseProgress]) { p?.step('ai', aiDone / AI_RIDERS); if (aiDone >= AI_RIDERS) p?.step('final', 0, now(), { begin: true }); } }
    }
  });
  for (const type of ['resource', 'mark']) observer.observe({ type, buffered: true });   // buffered: what finished before this script ran (the CSS)
} catch {}

let lastDraw = 0, handed = false, painted = false; const history = [];
function draw(c, b, opts = {}) {
  if (!c || !b) return false;
  const t = now(), dt = lastDraw ? Math.min(100, t - lastDraw) : 16; lastDraw = t;
  c.clearRect(0, 0, 640, 448);
  if (!title) { b.fillStyle = FE_BLUE; b.fillRect(0, 0, 640, 448); return true; }
  const shown = progress.value(t, dt);
  if (t - (history.at(-1)?.[0] ?? -1e9) >= 100 && history.length < 3000) history.push([Math.round(t), +shown.fraction.toFixed(4), shown.phase]);   // QA: what the meter showed
  title.draw(c, b, t, { ...state, ...opts, progress: shown });
  if (!painted) { painted = true; mark('boot:paint'); }
  return true;
}
const stage = document.getElementById('stage');
const fg = document.getElementById('ui')?.getContext('2d', UI_CANVAS), bg = document.getElementById('ui-bg')?.getContext('2d', UI_CANVAS);
function loop() {
  if (handed) return;
  const s = stage?.dataset.screen;
  if (!s || s === 'title') draw(fg, bg);
  requestAnimationFrame(loop);
}
draw(fg, bg); requestAnimationFrame(loop);

globalThis.ssxBoot = {
  t0, progress, images, fonts, state, history, lazy, courseProgress,
  bytes(url, got, total) { progress.bytes(url, got, total); courseProgress?.bytes(url, got, total); },
  begin(id) { progress.step(id, 0, now(), { begin: true }); courseProgress?.step(id, 0, now(), { begin: true }); },
  step(id, fraction = 1) { progress.step(id, fraction); courseProgress?.step(id, fraction); },
  courseDone() { courseProgress?.finish(); },
  ready() { if (state.ready) return; progress.finish(); state.ready = true; state.error = null; mark('boot:ready'); },
  fail(text) { state.error = text; },
  handoff() { handed = true; },
  draw(c, b, opts) { return draw(c, b, opts); },
  // Boot timeline for field diagnostics (web/main.js diagnose('boot')): ms since navigation.
  timeline() { const m = (n) => Math.round(performance.getEntriesByName(n)[0]?.startTime ?? -1); return { script: Math.round(t0), paint: m('boot:paint'), logo: m('boot:logo'), ready: m('boot:ready') }; },
};
