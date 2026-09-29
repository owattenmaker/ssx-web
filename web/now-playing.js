// Original in-game "EA RADIO BIG" now-playing popup (single-player HUD cOVStateHUD1P, ctor 0x1E9A30).
//
// Trigger: after PlaySong (0x2B35A0) the audio code 0x28F478 sends HUD command 5 (vfunc +0xC0 = 0x1EC1F0,
// case 0x1EC2E0) when the MUSIC channel is audible (0x287930 > 0) and a game is running. Case 5 clears flag
// 0x10000 (hud+0x3CC), takes the current song record (0x2B40F0, mgr+0x934 + 100 * index: TITLE; ARTIST at
// +0x2234 via 0x2B4120, ALBUM at +0x3B34 via 0x2B4150), sets 0x10000 when the title is not empty and, when
// the title hash (0x317670) differs from hud+0x3B4 (or the title is empty), stores it and restarts the popup
// (+0x190 time = -1, +0x194 slide = +0x198 text alpha = 0, +0x3A4 line count = +0x3A8 last title line = 0).
// The same title again while the popup runs is ignored; a new title restarts it from the slide-in (no slide-out).
//
// Update (0x1EA930, per 60 Hz frame, skipped while the HUD mask gp-0x6A0 bit 0 is set, i.e. pause/PDA menus):
// time < 0 builds the lines (0x1EABB0..0x1EB024, descriptor 60 of layout 0x4768B0 at 0x477120: anchor
// (20, 460), scale 0.8, left/bottom aligned, layer 19): "EA RADIO BIG" (string at 0x46EAE8 via gp-0xE74, not
// localized) in HUDFONT x0.8 with a fixed height 20.9988, the title word-wrapped by 0x392430 (300 px, at most
// 0x60 - 1 chars per line) in FEFONT x0.8 x 0.750017, one FEFONT line height (29 x 0.6000137 = 17.4004) per
// line plus 2.99987 below the last one, then artist and album (always two lines, even when empty). Widths are
// 0x391FB0 ink widths without shadow; box = (max width + 40) x (sum of heights + 28.0047). Otherwise the time
// advances by 1/60 (0.0166667): [0, 0.4) slide in (slide = 2.5 t), [0.4, 0.8) text fades in, hold to 7.2,
// [7.2, 7.6) text fades out, [7.6, 8.0) slide out, then flag 0x10000 is cleared and the stored hash is reset
// to 0xF0000000 (the same song shows again on the next command 5).
//
// Draw (0x1EC3F8 -> 0x2204A0, only when hud+0x3D8 == 0 (commands 3/4 from the Audio/Radio PDA 0x195FF0 and
// 0x20A380 raise/lower it), flag 0x10000 and time > 0): the box sits at (20, 460 - H) and slides in from the
// left by (1 - slide)^2 * (x + 30 + W) (two players: centred at 320 and slides up from y 510). It is three
// OV_1-4 ("hude", texture 0x656) slices at full vertex colour (no fade): left cap 42 px, the middle texel
// column stretched to W - 56, right cap 14 px, each the full box height (sprite records hud+0x4E4/+0x4E8/+0x4EC).
// Text starts at box y + 14.0024, x = box x + 20 (two players: centred), no shadow, each line then moves down by
// its stored height: header HUDFONT black (0x4C86A8), title lines FEFONT white (0x4C86C8), artist/album FEFONT
// black (0x4C86E8), alpha x text alpha.
// Checked against the popup state in the user's PCSX2 snapshots (widths/heights/times at hud+0x19C..0x3B4).
//
// API (640x448 UI canvas, PS2 HUD space 640x480 scaled by 448/480 like web/trick-hud.js):
//   const np = new NowPlayingHud(ui);  // ui = OriginalUI after load(): ui.fonts.{FEFONT,HUDFONT}, ui.images
//   np.show({ title, artist, album }); // HUD command 5 (MUSIC.INF TITLE/ARTIST/ALBUM keys are accepted too)
//   np.update(dtSeconds);              // runs whole 60 Hz HUD ticks (or np.tick() per game frame); skip while paused
//   np.draw(ctx);                      // on the 'game' screen, after the rest of the HUD
//   np.hidden = n;                     // command 3/4 counter (in-game Audio/Radio menus): suppresses drawing only
//   np.visible                         // true while drawn
// Pure helpers for tests: popupPhase, advanceTime, measureText, wrapLine, layoutNowPlaying, nowPlayingDrawList.
import { SPRITE_2D } from './sprite-canvas.js'; // offscreen sprite canvas kind (software in Firefox, docs/firefox-load.md)

const buf = new Float32Array(1), bits = new Uint32Array(buf.buffer);
// EE COP1 rounds toward zero (same helper as web/trick-hud.js).
const f = (v) => { buf[0] = v; if (Math.abs(buf[0]) > Math.abs(v)) bits[0] -= 1; return buf[0]; };

export const NOW_PLAYING = Object.freeze({
  header: 'EA RADIO BIG',           // 0x46EAE8 (gp-0xE74), hardcoded English in all builds
  anchor: [20, 460], scale: 0.800000011920929, align: [-1, 1], layer: 19, // descriptor 60 (0x477120)
  anchor2P: [320, 460], align2P: [0, 1],                                  // descriptor 60 of layout 0x4773F0
  feScale: 0.7500171661376953,      // gp-0x5768 / gp-0x530C
  headerHeight: 20.99884033203125,  // gp-0x5764
  titleGap: 2.999871015548706,      // gp-0x5760
  padWidth: 40, padHeight: 28.00471305847168, // gp-0x575C
  textInset: [20, 14.00235652923584],         // 0x2204A0 f20 = 20, gp-0x5308
  wrapWidth: 300, wrapChars: 0x60,
  fontHeight: { FEFONT: 29, HUDFONT: 21 },
  frame: 0.01666666753590107,       // gp-0x5758
  times: [0.4000000059604645, 0.800000011920929, 7.199999809265137, 7.599999904632568, 8], // gp-0x5754.. , 8.0
  rate: 2.5,
  slideMargin: 30, slideFrom2P: 510,
  // hud+0x4E4/+0x4E8/+0x4EC sprite records (texture 0x656 = OV_1-4): [v0, u0, u1, v1] and the slice widths.
  slices: [{ uv: [0.689453125, 0.009765625, 0.169921875, 0.998046875], w: 42 },
           { uv: [0.689453125, 0.173828125, 0.173828125, 0.998046875], w: -56 }, // W - 56
           { uv: [0.689453125, 0.177734375, 0.228515625, 0.998046875], w: 14 }],
  page: 'OV_1-4',
  colours: { header: [1, 0, 0, 0], title: [1, 1, 1, 1], artist: [1, 0, 0, 0] }, // ARGB, 1.0 = 0x80
});

// 0x1EB02C..0x1EB128: time (+0x190) -> { slide (+0x194), alpha (+0x198), done } for time >= 0.
export function popupPhase(t) {
  const [a, b, c, d, e] = NOW_PLAYING.times, r = NOW_PLAYING.rate;
  if (t < a) return { slide: f(t * r), alpha: 0, done: false };
  if (t < b) return { slide: 1, alpha: f(f(t - a) * r), done: false };
  if (t < c) return { slide: 1, alpha: 1, done: false };
  if (t < d) return { slide: 1, alpha: f(1 - f(f(t - c) * r)), done: false };
  if (t < e) return { slide: f(1 - f(f(t - d) * r)), alpha: 0, done: false };
  return { slide: 0, alpha: 0, done: true };
}
export const advanceTime = (t) => f(t + NOW_PLAYING.frame);

const s8 = (v) => (v > 127 ? v - 256 : v);
const glyphOf = (font, ch) => { const g = font[ch]; return g ? { w: g.w, h: g.h, dx: s8(g.dx), dy: s8(g.dy), advance: s8(g.advance) } : null; };

// 0x391FB0 ink width (a3 = 0) with the font shadow at (0, 0), as while the popup is built.
export function measureText(font, text, sx, sy = sx) {
  let pen = 0, minX = 0, maxX = 0;
  for (const ch of text) {
    const g = glyphOf(font, ch); if (!g) continue;
    minX = Math.min(f(pen + f(g.dx * sx)), minX);
    maxX = Math.max(f(pen + f((g.dx + g.w) * sx)), maxX);
    pen = f(pen + f(g.advance * sx));
  }
  return f(maxX + minX);
}

// 0x392430: one line of `text` fitting `width` (shadow 0). Returns { line, rest } (rest null when done).
// The pen is the ink right edge of the last glyph; a line breaks at the last space seen while it still fit,
// a single word wider than the line is cut before the glyph that crosses, and after `maxChars - 1` stored
// characters a line that still fits ends the text.
export function wrapLine(font, text, sx, width = NOW_PLAYING.wrapWidth, maxChars = NOW_PLAYING.wrapChars) {
  const skip = (i) => { while (text[i] === ' ') i++; return i; };
  const breakAt = (end) => ({ line: text.slice(0, end + 1), rest: text.slice(skip(end + 1)) });
  if (!text.length) return { line: '', rest: null };
  let pen = 0, carry = 0, end = -1, i = 0;
  for (;;) {
    const ch = text[i];
    if (ch === ' ') {
      if (width <= pen) return breakAt(end);
      if (i > 0 && text[i - 1] !== ' ') end = i - 1;
    }
    const g = glyphOf(font, ch);
    if (g) { const ink = f((g.dx + g.w) * sx), adv = f(g.advance * sx); const step = f(carry + ink); carry = f(adv - ink); pen = f(pen + step); }
    if (width <= pen && end === -1) return { line: text.slice(0, i), rest: text.slice(i) };
    i++;
    if (i === maxChars - 1 || i >= text.length) break;
  }
  if (width <= pen) return breakAt(end);
  return { line: text.slice(0, i), rest: null };
}

// 0x1EABB0..0x1EB024: the popup lines and box size for a song (`fonts` = { FEFONT, HUDFONT } glyph tables).
export function layoutNowPlaying(fonts, song) {
  const N = NOW_PLAYING, sc = N.scale, fe = f(sc * N.feScale);
  const lineH = f(f(f(N.fontHeight.FEFONT * 1) * sc) * N.feScale);
  const lines = [{ text: N.header, font: 'HUDFONT', w: measureText(fonts.HUDFONT, N.header, sc), h: N.headerHeight, colour: 'header' }];
  let total = N.headerHeight, rest = song.title ?? '';
  do {
    const r = wrapLine(fonts.FEFONT, rest, fe); rest = r.rest;
    lines.push({ text: r.line, font: 'FEFONT', w: measureText(fonts.FEFONT, r.line, fe), h: lineH, colour: 'title' }); total = f(total + lineH);
  } while (rest);
  const lastTitle = lines.length - 1;
  total = f(total + N.titleGap); lines[lastTitle].h = f(lines[lastTitle].h + N.titleGap);
  for (const text of [song.artist ?? '', song.album ?? '']) { lines.push({ text, font: 'FEFONT', w: measureText(fonts.FEFONT, text, fe), h: lineH, colour: 'artist' }); total = f(total + lineH); }
  const maxW = lines.reduce((m, l) => Math.max(m, l.w), 0);
  return { lines, lastTitle, width: f(maxW + N.padWidth), height: f(total + N.padHeight), scale: { HUDFONT: sc, FEFONT: fe } };
}

// 0x2204A0: draw list in PS2 HUD space (640x480) for a layout at slide/alpha. players 1: slide from the left.
export function nowPlayingDrawList(layout, slide, alpha, players = 1) {
  const N = NOW_PLAYING, W = layout.width, H = layout.height;
  const [ax, ay] = players === 1 ? N.anchor : N.anchor2P; const alignH = players === 1 ? N.align[0] : N.align2P[0];
  let x = alignH < 0 ? ax : alignH > 0 ? f(ax - W) : f(ax - f(W * 0.5)), y = f(ay - H);
  if (slide < 1) {
    const e = f(f(1 - slide) * f(1 - slide));
    if (players === 1) x = f(x - f(e * f(f(x + N.slideMargin) + W)));
    else y = f(y + f(e * f(N.slideFrom2P - y)));
  }
  const out = []; let px = x;
  for (const s of N.slices) { const w = s.w < 0 ? f(W + s.w) : s.w; out.push({ kind: 'sprite', page: N.page, uv: s.uv, x: px, y, w, h: H, layer: N.layer }); px = f(px + w); }
  let ty = f(y + N.textInset[1]);
  layout.lines.forEach((l) => {
    const c = N.colours[l.colour].slice(); c[0] = f(c[0] * alpha);
    const tx = alignH < 0 ? f(x + N.textInset[0]) : alignH > 0 ? f(f(x - N.textInset[0]) - l.w) : f(x + f(f(W - l.w) * 0.5));
    out.push({ kind: 'text', font: l.font, text: l.text, x: tx, y: ty, scale: layout.scale[l.font], argb: c, layer: N.layer + 2 });
    ty = f(ty + l.h);
  });
  return { box: { x, y, w: W, h: H }, draws: out };
}

export class NowPlayingHud {
  constructor(ui, { players = 1 } = {}) {
    this.ui = ui; this.players = players; this.hidden = 0; this.tints = new Map();
    this.song = null; this.active = false; this.key = null; this.time = -1; this.slide = 0; this.alpha = 0; this.layout = null; this.acc = 0;
  }
  // HUD command 5 (0x1EC2E0).
  show(song) {
    const s = { title: song?.title ?? song?.TITLE ?? '', artist: song?.artist ?? song?.ARTIST ?? '', album: song?.album ?? song?.ALBUM ?? '' };
    const was = this.active; this.active = !!s.title;
    if (this.active && was && this.key === s.title) return;
    this.key = s.title; this.song = s; this.time = -1; this.slide = 0; this.alpha = 0; this.layout = null;
  }
  clear() { this.active = false; this.key = null; this.time = -1; this.slide = 0; this.alpha = 0; this.layout = null; }
  // One 60 Hz HUD update (0x1EABB0..0x1EB15C).
  tick() {
    if (!this.active) return;
    if (this.time < 0) { this.time = 0; this.slide = 0; this.alpha = 0; this.layout = layoutNowPlaying(this.ui.fonts, this.song); return; }
    this.time = advanceTime(this.time); const p = popupPhase(this.time);
    if (p.done) { this.key = null; this.clear(); return; }
    this.slide = p.slide; this.alpha = p.alpha;
  }
  update(dt) {
    if (!this.active) { this.acc = 0; return; }
    this.acc = Math.min(this.acc + Math.max(0, dt || 0), 0.25);
    while (this.acc >= 1 / 60 - 1e-9) { this.acc -= 1 / 60; this.tick(); }
  }
  get visible() { return this.active && this.hidden === 0 && this.time > 0 && !!this.layout; }
  tinted(name, argb) {
    const q = (v) => Math.max(0, Math.min(255, Math.round(Math.trunc(v * 128) * 255 / 128)));
    const key = `${name}:${q(argb[1])},${q(argb[2])},${q(argb[3])}`; let c = this.tints.get(key);
    if (!c) {
      const im = this.ui.images[name]; if (!im) return null;
      c = document.createElement('canvas'); c.width = im.width; c.height = im.height; const g = c.getContext('2d', SPRITE_2D);
      g.drawImage(im, 0, 0); g.globalCompositeOperation = 'multiply'; g.fillStyle = `rgb(${q(argb[1])},${q(argb[2])},${q(argb[3])})`; g.fillRect(0, 0, c.width, c.height);
      g.globalCompositeOperation = 'destination-in'; g.drawImage(im, 0, 0); this.tints.set(key, c);
    }
    return c;
  }
  draw(ctx) {
    if (!this.visible) return;
    const { draws } = nowPlayingDrawList(this.layout, this.slide, this.alpha, this.players);
    ctx.save(); ctx.scale(1, 448 / 480);
    for (const d of draws) {
      if (d.kind === 'sprite') {
        const im = this.ui.images[d.page]; if (!im) continue; const [v0, u0, u1, v1] = d.uv;
        // A zero-width UV span (the stretched middle column) samples the texel under u0.
        const sx = u1 > u0 ? u0 * im.width : Math.floor(u0 * im.width), sw = u1 > u0 ? (u1 - u0) * im.width : 1;
        ctx.globalAlpha = 1; ctx.drawImage(im, sx, v0 * im.height, sw, (v1 - v0) * im.height, d.x, d.y, d.w, d.h);
      } else {
        const a = Math.max(0, Math.min(1, Math.trunc(d.argb[0] * 128) / 128)); if (a <= 0) continue;
        const im = this.tinted(d.font + '-0', d.argb), font = this.ui.fonts[d.font]; if (!im || !font) continue;
        ctx.globalAlpha = a; let x = d.x;
        for (const ch of d.text) { const g = glyphOf(font, ch); if (!g) continue; const src = font[ch];
          ctx.drawImage(im, src.x, src.y, g.w, g.h, x + g.dx * d.scale, d.y + g.dy * d.scale, g.w * d.scale, g.h * d.scale); x += g.advance * d.scale; }
      }
    }
    ctx.restore();
  }
}
