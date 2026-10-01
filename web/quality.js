// Presentation quality for phones and desktops (docs/mobile.md). Nothing here reaches the simulation: it only decides
// how big the drawing buffer is, whether MSAA is on, how often a frame is drawn and which presentation-only passes
// run. The fixed 60 Hz game clock (fixed-step-clock.js) advances the same ticks whatever the render rate.
//
// Tiers: low (phones), medium (tablets / small laptops), high (desktop). `?quality=low|medium|high` overrides the
// detected tier for one visit; Options > Display & Touch (fe-options.js DISPLAY_ROWS) saves a choice in localStorage `ssx3.quality`.
//
// Render scale (the drawing buffer of the 3D canvas; the HUD canvas is always the PS2's 640x448):
//   native    = the PS2 frame buffer, 640x448 for the 4:3 frame (640x336 inside the 16:9 letterbox band), stretched
//               to the frame like the PS2's non-square pixels on a TV;
//   native512 = 512x448 (the 3D viewport the PS2 renders at: context+0x5930 GS scale is for 512 wide);
//   css       = one buffer pixel per CSS pixel;
//   full      = devicePixelRatio, capped at 1.5 (the desktop default before this module).
export const RENDER_SCALES = Object.freeze(['native', 'native512', 'css', 'full']);
export const RENDER_SCALE_LABELS = Object.freeze({ native: 'PS2 640x448', native512: 'PS2 512x448', css: '1x', full: 'Full' });
export const UPSCALES = Object.freeze(['smooth', 'pixelated']);
export const TIERS = Object.freeze(['low', 'medium', 'high']);
const KEY = 'ssx3.quality';

// Per-tier defaults. passes: presentation passes main.js may skip on a tier (true = draw it). presentationFast: the core
// builds the skin palettes and snow sprites with plain float arithmetic instead of the exact EE rounding emulation
// (web/presentation_fast.hpp; presentation only, the simulation is the same; `?fastfx=0|1` overrides).
export const TIER_DEFAULTS = Object.freeze({
  low: Object.freeze({
    renderScale: 'native',
    upscale: 'smooth',
    antialias: false,
    fps: 'auto',
    maxFrameDt: 4 / 60,
    presentationFast: true,
    passes: Object.freeze({ sun: true, glow: true, glare: true })
  }),
  medium: Object.freeze({
    renderScale: 'css',
    upscale: 'smooth',
    antialias: true,
    fps: 60,
    maxFrameDt: Infinity,
    presentationFast: false,
    passes: Object.freeze({ sun: true, glow: true, glare: true })
  }),
  high: Object.freeze({
    renderScale: 'full',
    upscale: 'smooth',
    antialias: true,
    fps: 60,
    maxFrameDt: Infinity,
    presentationFast: false,
    passes: Object.freeze({ sun: true, glow: true, glare: true })
  })
});

// Device class from what the browser exposes. env: {navigator, matchMedia, screen, devicePixelRatio} (globalThis).
export function detectDevice(env = globalThis) {
  const nav = env.navigator || {}, ua = String(nav.userAgent || ''), mm = (q) => { try { return !!env.matchMedia?.(q).matches; } catch { return false; } };
  const coarse = mm('(pointer:coarse)'), touchPoints = +nav.maxTouchPoints || 0;
  const ios = /iPhone|iPad|iPod/.test(ua) || (/Macintosh/.test(ua) && touchPoints > 1);
  const android = /Android/.test(ua);
  const sw = +env.screen?.width || +env.innerWidth || 0, sh = +env.screen?.height || +env.innerHeight || 0;
  const short = Math.min(sw, sh), long = Math.max(sw, sh);
  const touch = coarse || touchPoints > 0;
  const phone = touch && short > 0 && short <= 540 && (ios || android || coarse);
  const tablet = touch && !phone && short > 540 && short <= 1100 && (ios || android || coarse);
  return { touch, coarse, ios, android, phone, tablet, short, long, dpr: +env.devicePixelRatio || 1,
    memory: +nav.deviceMemory || 0, cores: +nav.hardwareConcurrency || 0, webgpu: !!nav.gpu };
}

export function detectTier(device) {
  if (device.phone) return 'low';
  if (device.tablet) return (device.memory && device.memory <= 4) || (device.cores && device.cores <= 4) ? 'low' : 'medium';
  if (device.touch && ((device.memory && device.memory <= 4) || (device.cores && device.cores <= 4))) return 'medium';
  return 'high';
}

export function loadSaved(storage = globalThis.localStorage) { try { const v = JSON.parse(storage?.getItem(KEY) || 'null'); return v && typeof v === 'object' ? v : {}; } catch { return {}; } }
export function saveSaved(value, storage = globalThis.localStorage) { try { storage?.setItem(KEY, JSON.stringify(value)); } catch {} }

// Resolve the effective settings: tier defaults < saved choices < URL (?quality=, ?renderScale=, ?upscale=, ?fps=, ?aa=).
export function resolveQuality(device, saved = {}, params = new URLSearchParams()) {
  const auto = detectTier(device);
  const qTier = params.get('quality');
  const tier = TIERS.includes(qTier) ? qTier : TIERS.includes(saved.tier) ? saved.tier : auto;
  const base = TIER_DEFAULTS[tier];
  const pick = (name, list) => { const q = params.get(name); if (list.includes(q)) return q; if (!qTier && list.includes(saved[name])) return saved[name]; return base[name]; };
  const renderScale = pick('renderScale', RENDER_SCALES), upscale = pick('upscale', UPSCALES);
  const okFps = (v) => v === 30 || v === 60 || v === 'auto', fpsQ = params.get('fps') === 'auto' ? 'auto' : +params.get('fps');
  const fps = okFps(fpsQ) ? fpsQ : !qTier && okFps(saved.fps) ? saved.fps : base.fps;
  const aaQ = params.get('aa'), antialias = aaQ === '0' ? false : aaQ === '1' ? true : base.antialias;
  // maxFrameDt: the most game time one drawn frame may advance. The fixed clock keeps its debt so a slow frame never
  // loses game time; on a device that cannot run 60 ticks/s that debt grows every frame (12 catch-up ticks per frame,
  // ~5 fps at 4x CPU throttling with six riders). Low tier caps it: past 4 ticks per frame the game runs slower than real
  // time instead. Tick order and inputs per tick are unchanged (same simulation), only wall-clock pacing differs.
  const fastQ = params.get('fastfx'), presentationFast = fastQ === '0' ? false : fastQ === '1' ? true : base.presentationFast;
  // PS2 softness (fog-renderer.js setSoftness; Options > Display & Touch): Off by default on every tier; ?ps2soft=0|1 for one visit.
  const softQ = params.get('ps2soft'), ps2Output = softQ === '1' ? true : softQ === '0' ? false : saved.ps2Output === true;
  // Rider textures ('Texture set'; docs/xbox-textures.md section 8): ?riders=ps2|xbox for one visit, else the saved choice, else
  // defaultRiderTextures.
  const ridersQ = params.get('riders');
  const riderTextures = RIDER_TEXTURE_SETS.includes(ridersQ) ? ridersQ : RIDER_TEXTURE_SETS.includes(saved.riderTextures) ? saved.riderTextures : defaultRiderTextures(device, tier);
  return { tier, auto, renderScale, upscale, fps, antialias, maxFrameDt: base.maxFrameDt, presentationFast, ps2Output, riderTextures, passes: { ...base.passes } };
}

// Rider texture set: 'ps2' = the PS2 TXP textures (WARDROBE/<ID>/textures.tex, gear.tex), 'xbox' = the Xbox HD set (textures-xbox.tex,
// gear-xbox.tex: the Xbox's own DXT blocks at 2x; web/texture-archive.js picks the archive). Default: Xbox HD on a
// desktop, PS2 on a phone (iOS / Android) or the low tier (memory and download).
export const RIDER_TEXTURE_SETS = Object.freeze(['ps2', 'xbox']);
export function defaultRiderTextures(device, tier, hd = true) { return hd && !device.ios && !device.android && tier !== 'low' ? 'xbox' : 'ps2'; }

// Drawing-buffer size for a CSS box of cssW x cssH (the 3D canvas: the stage, or the 16:9 letterbox band of it).
// band = view.band[1] (height fraction of the 448-line buffer). Returns {pixelRatio, width, height, fixed}: three's
// renderer.setPixelRatio(pixelRatio) + setSize(width, height, !fixed); fixed = the canvas keeps its CSS box from the
// stylesheet (100% of the stage) and the buffer is a set number of pixels.
// css / full never draw more than MAX_RENDER_LINES lines (2x the PS2's 448): the textures and UI are PS2 resolution, so
// more pixels only cost GPU time (a 2560x1440 buffer held Safari at 30 fps on a 60 Hz 27" display).
export const MAX_RENDER_LINES = 896;
export function bufferSize(scale, cssW, cssH, band = 1, dpr = 1) {
  const w = Math.max(1, Math.round(cssW)), h = Math.max(1, Math.round(cssH));
  if (scale === 'native' || scale === 'native512') return { pixelRatio: 1, width: scale === 'native' ? 640 : 512, height: Math.max(1, Math.round(448 * band)), fixed: true };
  const cap = MAX_RENDER_LINES * band / h;
  if (scale === 'css') return { pixelRatio: Math.min(1, cap), width: w, height: h, fixed: false };
  return { pixelRatio: Math.min(dpr || 1, 1.5, cap), width: w, height: h, fixed: false };
}

// Frame pacing: draw every rAF (fps 60) or at most every other ~33 ms (fps 30). The game clock keeps its 60 Hz ticks
// (two per drawn frame at 30), so the simulation is the same; only presentation is skipped.
// 'auto' (low tier): starts at 60 and measures the main-thread work of each drawn frame (gate.done(ms), sim ticks +
// render): a median above 13 ms over 45 frames goes to 30 (steady two ticks per frame instead of uneven 1/2-tick
// frames), a median below 7 ms at 30 goes back to 60.
// refreshCap (pv refreshCap): on a display faster than 60 Hz (120 / 144 / 165 / 240 Hz; Safari keeps rAF at 60 by default, Chrome and
// Firefox do not) a 60 fps gate draws every n-th animation frame, n = floor(display Hz / 60 + 0.1) (120 -> every 2nd, 144 -> every 2nd =
// 72 fps, 90 Hz -> every frame): the game ticks at 60 Hz, so the frames in between drew the same ticks again, interpolated. The display
// rate is the median of the last 30 animation-frame intervals (all of them, drawn or not); a busy page whose frames already run at 60
// measures 16.7 ms and draws every frame. A skipped animation frame runs nothing (no ticks, no input read): the ticks run in the drawn
// frames, still 60 per second.
export function frameGate(fps, { refreshCap = false } = {}) {
  let lastDrawn = -Infinity, rate = fps === 'auto' ? 60 : fps, lastRaf = -1, refresh = 1000 / 60, every = 1, filled = 0;
  const intervals = new Float64Array(30);
  const measure = (ms) => {
    if (lastRaf >= 0) {
      const d = ms - lastRaf;
      if (d > 0 && d < 100) {
        intervals[filled++] = d;
        if (filled === intervals.length) {
          filled = 0;
          const s = Array.from(intervals).sort((a, b) => a - b);
          refresh = s[s.length >> 1];
          every = Math.max(1, Math.floor(1000 / refresh / 60 + 0.1));
        }
      }
    }
    lastRaf = ms;
  };
  const work = [], gate = (ms) => {
    if (refreshCap) measure(ms);
    if (rate >= 60) { if (!refreshCap || every <= 1) { lastDrawn = ms; return true; } if (ms - lastDrawn < every * refresh - refresh / 2) return false; lastDrawn = ms; return true; }
    if (ms - lastDrawn < 1000 / rate - 4) return false; lastDrawn = ms; return true; };
  gate.done = (ms) => {
    if (fps !== 'auto') return;
    work.push(ms); if (work.length < 45) return;
    const m = work.sort((a, b) => a - b)[work.length >> 1]; work.length = 0;
    if (rate === 60 && m > 13) rate = 30; else if (rate === 30 && m < 7) rate = 60;
  };
  Object.defineProperty(gate, 'rate', { get: () => rate });
  Object.defineProperty(gate, 'display', { get: () => ({ refresh, every }) }); // QA
  return gate;
}

// ---- browser singleton ---------------------------------------------------------------------------------------------
const hasWindow = typeof window !== 'undefined';
export const device = hasWindow ? detectDevice(window) : detectDevice({});
let saved = hasWindow ? loadSaved() : {};
const params = hasWindow ? new URL(location.href).searchParams : new URLSearchParams();
export const quality = resolveQuality(device, saved, params);
const listeners = new Set();
export function onQualityChange(f) { listeners.add(f); return () => listeners.delete(f); }
// Change a setting at run time (Options > Display & Touch). Everything applies at once: render scale / upscale / fps here, a tier's MSAA in
// main.js (applyAntialias: renderer.samples, then the pipelines re-warm), no page reload.
export function setQuality(patch) {
  saved = { ...saved, ...patch }; saveSaved(saved);
  if (patch.tier) {
    const next = resolveQuality(device, saved, new URLSearchParams());
    Object.assign(quality, {
      tier: next.tier,
      renderScale: next.renderScale,
      upscale: next.upscale,
      fps: next.fps,
      antialias: next.antialias,
      maxFrameDt: next.maxFrameDt,
      presentationFast: next.presentationFast
    });
  }
  for (const k of ['renderScale', 'upscale', 'fps', 'ps2Output', 'riderTextures']) if (patch[k] !== undefined) quality[k] = patch[k];
  applyUpscale();
  for (const f of listeners) try { f(quality); } catch (e) { console.warn(e); }
}
export function resetQuality() { saved = {}; saveSaved({}); }
function applyUpscale() { if (hasWindow) document.documentElement.dataset.upscale = quality.upscale; }
if (hasWindow) { applyUpscale(); window.__quality = quality; }
// Size the 3D canvas (main.js layoutStage): the renderer's buffer for this render scale.
export function sizeRenderer(renderer, cssW, cssH, band = 1) {
  const s = bufferSize(quality.renderScale, cssW, cssH, band, hasWindow ? window.devicePixelRatio : 1);
  if (renderer.getPixelRatio() !== s.pixelRatio) renderer.setPixelRatio(s.pixelRatio);
  renderer.setSize(s.width, s.height, !s.fixed);
  if (s.fixed && renderer.domElement) { renderer.domElement.style.width = ''; renderer.domElement.style.height = ''; } // stylesheet: 100% of the stage / band
  return s;
}
