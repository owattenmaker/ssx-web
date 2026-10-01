// Offscreen 2D canvases for UI sprites (glyph tints, LUI sprite caches, HUD atlases) that are drawn into the software UI
// canvases (ui.js UI_CANVAS) every frame (docs/firefox-load.md).
//
// Firefox: software, like the UI canvases. An accelerated sprite canvas lives in Firefox's GPU process, and each drawImage
// from it into a software canvas waits for a readback there. While the GPU process builds render pipelines (the race
// warm-up), one load screen frame waited 0.1-0.7 s. Chrome and Safari keep the default kind: their accelerated sprites
// give slightly different edge pixels than software ones (up to 57 levels on the HUD, measured in Chrome), so they stay
// as they were. ?uicanvas=gpu (accelerated UI canvases) keeps the default everywhere.
// WebKit keeps the default kind too (software tints differ by up to 64 levels); its glyph atlases go through softGlyphs below.
const ua = globalThis.navigator?.userAgent ?? '';
const firefox = /\bFirefox\//.test(ua), webkit = /AppleWebKit\//.test(ua) && !/\b(Chrome|Chromium|Edg|OPR)\//.test(ua);
const gpuUi = /[?&]uicanvas=gpu\b/.test(globalThis.location?.search ?? '');
export const SPRITE_2D = firefox && !gpuUi ? { willReadFrequently: true } : undefined;
// WebKit: a finished glyph tint atlas is drawn from a software copy of it (drawImage 1:1: the same bytes), so each glyph
// draw into the software UI canvas reads memory instead of locking and converting the atlas's IOSurface. `softGlyphs.on` can be
// flipped in a page (QA comparisons); the copy is made once per atlas.
// The accelerated original is released once copied (its IOSurface: no memory growth), except in QA pages (?qa=1), which compare both.
export const softGlyphs = { on: webkit && !firefox && !gpuUi, keep: /[?&]qa=1\b/.test(globalThis.location?.search ?? '') };
export function glyphSource(canvas) {
  if (!softGlyphs.on || !canvas) return canvas;
  let copy = canvas.__softCopy;
  if (!copy) {
    copy = document.createElement('canvas'); copy.width = canvas.width; copy.height = canvas.height;
    copy.getContext('2d', { willReadFrequently: true }).drawImage(canvas, 0, 0); canvas.__softCopy = copy;
    if (!softGlyphs.keep) canvas.width = canvas.height = 0;
  }
  return copy;
}
