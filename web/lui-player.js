// A small player for original front-end LUI screens (DATA/UI/FE.LUI via tools/export_character_select.py; the
// same record format web/loading-screen.js plays for GL.LUI): frame-0 properties, timeline property/animation
// events, sprites from the FE_1 atlas pages, gouraud shapes (vertex/triangle lists), bars and FEFONT text.
// The caller decides which timeline events are live (`events`) and patches runtime fields through `override`.
import { animationProps } from './loading-screen.js';
import { drawGlyphAsKey, glyphButton, inputDevice, LUI_STRETCH } from './input-glyphs.js';
import { SPRITE_2D } from './sprite-canvas.js'; // offscreen sprite canvas kind (software in Firefox, docs/firefox-load.md)

// LUI text at 100% is FEFONT at 0.79 of its native size in PS2 pixels (docs/loading-screen.md).
const FONT_K = 0.79;
const PS2_X = 640 / 512, PS2_Y = 480 / 448;
const LINE = 40;                                 // wrapped line pitch in font units (the card text: 17 px at 50%, measured)
// FEFONT on the PS2 front end (local/ps2-capture/menus/fe-texture, 20 screens): the glyphs keep their aspect in the
// 640x480 LUI frame (they were drawn 1/1.17 as tall: heights 13-17 vs 11-15 px, widths equal), and the text is the
// LUI colour x 0.8 (the font's texels are 0xCC: white text peaks at 204, the title's 37,7,5 shows as 29,5,4).
// ?luitext=old draws the previous glyphs (full colour, squashed) for comparisons.
const OLD_TEXT = /[?&]luitext=old\b/.test(globalThis.location?.search ?? '');
const TEXT_TEXEL = OLD_TEXT ? 1 : 0.8, TEXT_TALL = OLD_TEXT ? 1 : PS2_X / PS2_Y;   // sweep 1.0..1.2 vs the PS2 text regions: best at PS2_X / PS2_Y

export class LuiScreen {
  static order = /[?&]luiorder=flat\b/.test(globalThis.location?.search ?? '') ? 'flat' : 'group';   // see draw()
  constructor(screen, images, ui) {
    this.screen = screen; this.images = images; this.ui = ui; this.tints = new Map();
    this.byName = new Map(screen.elements.map((e) => [e.name, e]));
    this.bars = new Map();
    for (const e of screen.elements) if (e.bar) { this.bars.set(e.bar.background, { bar: e, role: 'bg' }); this.bars.set(e.bar.fill, { bar: e, role: 'fill' }); }
  }

  // events: [{ev, start}] in application order; an animation event plays from `start` (a frame on `now`'s clock).
  props(e, events, now) {
    let p = e.props || null, anim = e.anim ? { ...e.anim, start: 0 } : null;
    for (const { ev, start } of events) {
      if (ev.element !== e.name) continue;
      if (ev.props) { p = ev.props; anim = null; } else anim = { hash: ev.anim, mode: ev.mode, start };
    }
    const a = anim && this.screen.animations[anim.hash];
    if (a) p = { ...(p || {}), ...animationProps(a, now - anim.start, anim.mode) };
    return p || {};
  }

  // Draw on a 640x480 LUI frame. override(e, p) may return {hidden, text, props, sprite, fill (0..1), alpha};
  // layers(layer) picks the draw layers (the 3D rider sits between the background and the menu layers).
  draw(c, events, now, override = () => null, layers = null) {
    const cache = new Map();
    const abs = (e) => {
      if (cache.has(e.name)) return cache.get(e.name);
      const o = override(e) || {};
      const p = { ...this.props(e, events, now), ...(o.props || {}) };
      const up = e.parent || e.menu, parent = up && this.byName.has(up) ? abs(this.byName.get(up)) : { x: 0, y: 0, a: 1 };   // group or menu offset
      const r = { x: parent.x + (p[0] || 0), y: parent.y + (p[1] || 0), a: parent.a * ((o.alpha ?? p[13] ?? 255) / 255) * (o.hidden ? 0 : 1), p, o };
      cache.set(e.name, r); return r;
    };
    const items = [];
    for (const e of this.screen.elements) {
      if (e.kind === 'group' || e.kind === 'menu' || e.kind === 'bar') continue;
      let r = abs(e);
      const bar = this.bars.get(e.name);
      if (bar) { const br = abs(bar.bar); r = { ...r, x: br.x + (r.p[0] || 0), y: br.y + (r.p[1] || 0), a: r.a * br.a, fill: bar.role === 'fill' ? (br.o.fill ?? 0) : 1 }; }
      if (r.a <= 0) continue;
      if (layers && !layers(r.o.layer ?? e.layer)) continue;
      items.push({ e, r });
    }
    // Draw order: by layer (flags & 0x3F); within a layer, an element under a top-level group draws with that group's
    // layer as the tie-break (a top-level element uses its own), then by definition order. Found against the PS2 frames of
    // 20 front-end screens (local/ps2-capture/menus/fe-texture): the Main Menu's dark help panel (layer 7, in a layer-8
    // group) covers the mountain (layer 7, in a layer-0 group) as on the PS2; every other screen draws as before.
    // ?luiorder=flat = layer, then definition order only (the previous order, for comparisons).
    const topLayer = (e) => { let x = e, l = e.layer; while (x) { l = x.layer; const up = x.parent || x.menu; x = up ? this.byName.get(up) : null; } return l; };
    for (const it of items) it.t = LuiScreen.order === 'flat' ? 0 : it.e.parent || it.e.menu ? topLayer(it.e) : (it.r.o.layer ?? it.e.layer);
    items.sort((a, b) => (a.r.o.layer ?? a.e.layer) - (b.r.o.layer ?? b.e.layer) || a.t - b.t || a.e.index - b.e.index);
    // Keyboard: a button icon drawn over another at the same spot (19game_opt keeps its Cross under the Triangle) hid it;
    // key caps differ in width, so the covered one is dropped instead of peeking out.
    if (inputDevice() === 'keyboard') {
      const seen = new Set();
      for (let i = items.length - 1; i >= 0; i--) {
        const { e, r } = items[i], sp = e.kind === 'sprite' && (r.o.sprite || e.sprite);
        if (!sp || !glyphButton(sp.page, sp.sx, sp.sy, sp.sw, sp.sh)) continue;
        const at = [r.x, r.y, r.p[6], r.p[7], r.p[9], r.p[10], r.p[12]].map((v) => Math.round((v ?? 0) * 4)).join();
        if (seen.has(at)) items.splice(i, 1); else seen.add(at);
      }
    }
    for (const { e, r } of items) {
      if (e.kind === 'sprite') this.sprite(c, r.o.sprite || e.sprite, r, r.p);
      else if (e.kind === 'shape') this.shape(c, e, r, r.p);
      else if (e.kind === 'text') { const text = r.o.text ?? e.text; if (text) this.text(c, text, r.x, r.y, r.p, r.a, !this.flagWrap || !!(e.flags & 0x80) || !!r.o.wrap); }
    }
  }

  anchor(p, w, h) {
    const f = p[12] ?? 9;
    return [-w * (f & 16 ? 0.5 : f & 32 ? 1 : 0), -h * (f & 2 ? 0.5 : f & 4 ? 1 : 0)];
  }

  // Source rectangle; a zero-height (or zero-width) UV box is a one-texel line (the dash rows of FE_1-11 at
  // v = 253.5, h = 0: sample texel row 253 itself rather than blending it with the empty row below).
  static source(sprite) {
    const sx = sprite.sw < 1 ? Math.floor(sprite.sx) : sprite.sx, sy = sprite.sh < 1 ? Math.floor(sprite.sy) : sprite.sy;
    return [sx, sy, Math.max(1, sprite.sw), Math.max(1, sprite.sh)];
  }

  // Each sprite drawn from its own canvas, cut once from the atlas page. Stretching a sprite straight from the page
  // (the menu focus bar is a narrow slice scaled across the row) lets bilinear filtering read the neighbouring atlas
  // pixels just outside the slice: Safari does (as the canvas spec allows) and showed a faint line along the bar's
  // left edge; Chrome clamps. Outside its own canvas the filter clamps to the edge in every browser.
  cut(sprite) {
    const key = sprite.page + ':' + sprite.sx + ',' + sprite.sy + ',' + sprite.sw + ',' + sprite.sh;
    let im = this.cuts?.get(key);
    if (!im) {
      const src = this.images[sprite.page], [sx, sy, sw, sh] = LuiScreen.source(sprite);
      im = document.createElement('canvas'); im.width = Math.max(1, Math.ceil(sw)); im.height = Math.max(1, Math.ceil(sh));
      im.getContext('2d', SPRITE_2D).drawImage(src, sx, sy, sw, sh, 0, 0, sw, sh);
      (this.cuts ??= new Map()).set(key, im);
    }
    return im;
  }

  tinted(sprite, rgb) {
    const key = sprite.page + sprite.sx + ',' + sprite.sy + rgb.join(',');
    let im = this.tints.get(key);
    if (!im) {
      const src = this.cut(sprite);   // the sprite's own canvas: no atlas neighbours at the edges
      im = document.createElement('canvas'); im.width = src.width; im.height = src.height;
      const t = im.getContext('2d', SPRITE_2D);
      t.drawImage(src, 0, 0);
      t.globalCompositeOperation = 'multiply'; t.fillStyle = `rgb(${rgb})`; t.fillRect(0, 0, im.width, im.height);
      t.globalCompositeOperation = 'destination-in'; t.drawImage(src, 0, 0);
      this.tints.set(key, im);
    }
    return im;
  }

  sprite(c, sprite, r, p) {
    if (!sprite || !this.images[sprite.page]) return;
    const [sx, sy, sw, sh] = LuiScreen.source(sprite);
    const w = (p[6] || sw) * (p[9] ?? 100) / 100, h = (p[7] || sh) * (p[10] ?? 100) / 100;
    const rgb = [p[14] ?? 255, p[15] ?? 255, p[16] ?? 255].map(Math.round);
    const [ox, oy] = this.anchor(p, w, h);
    // A PS2 button icon (FE_1-14 legends, prompts) becomes the key cap while the keyboard is in use (web/input-glyphs.js);
    // this.keys: per-screen key overrides (fe-screens.js name-entry keyboard).
    if (!p[5]) { c.save(); c.globalAlpha = Math.min(1, r.a); const key = drawGlyphAsKey(c, this.ui, sprite.page, sx, sy, sw, sh, r.x + ox, r.y + oy, w, h, { stretch: LUI_STRETCH, keys: this.keys }); c.restore(); if (key) return; }
    c.save(); c.globalAlpha = Math.min(1, r.a); c.translate(r.x, r.y);
    if (p[5]) c.rotate(p[5] * Math.PI / 180);
    if (rgb.every((v) => v >= 255)) c.drawImage(this.cut(sprite), 0, 0, sw, sh, ox, oy, w, h);
    else c.drawImage(this.tinted(sprite, rgb), 0, 0, sw, sh, ox, oy, w, h);
    c.restore();
  }

  // Shape: byte 0 vertex count, byte 1 triangle count, triangles from byte 4; vertex k at props 21+9k.. (x, y, A R G B).
  // Bar fills scale x by the bar value (0x39C428: background width * value / max).
  shape(c, e, r, p) {
    const bytes = e.shape || [];
    const nv = bytes[0] ?? 4, nt = bytes.length ? bytes[1] : 2;
    const tris = bytes.length >= 4 + 3 * nt ? Array.from({ length: nt }, (_, t) => bytes.slice(4 + 3 * t, 7 + 3 * t)) : [[0, 1, 2], [0, 2, 3]];
    // shapeScale (opt-in, web/ctm-pda.js): the element's scale props 9 / 10 (%) apply to its vertices, as for sprites (the MCOMM
    // icons are authored at 50..160%); the other screens keep the unscaled vertices they were matched with.
    const kx = this.shapeScale ? (p[9] ?? 100) / 100 : 1, ky = this.shapeScale ? (p[10] ?? 100) / 100 : 1;
    const sx = (r.fill ?? 1) * kx;
    const V = Array.from({ length: nv }, (_, k) => ({ x: (p[21 + 9 * k] || 0) * sx, y: (p[22 + 9 * k] || 0) * ky, a: (p[26 + 9 * k] ?? 255) / 255, rgb: [p[27 + 9 * k] ?? 255, p[28 + 9 * k] ?? 255, p[29 + 9 * k] ?? 255] }));
    c.save();
    if (nt === 0) {                                  // no triangles (byte 2 = 2): a closed line loop (e.g. the name bar frame)
      c.strokeStyle = `rgba(${V[0].rgb},${V[0].a * r.a})`; c.lineWidth = 1.5; c.beginPath();
      V.forEach((v, i) => c[i ? 'lineTo' : 'moveTo'](r.x + v.x, r.y + v.y)); c.closePath(); c.stroke(); c.restore(); return;
    }
    // unionFlat (opt-in, web/results-lui.js): a shape of one flat colour fills its triangles as one path. Filled one by one,
    // a translucent fan shows its internal edges where the anti-aliased coverage of two neighbours sums below 1 (WebKit's
    // CoreGraphics draws them as light lines radiating from the fan's corner); the GS has no such seams.
    if (this.unionFlat && V.length && V.every((v) => v.a === V[0].a && v.rgb.every((x, k) => x === V[0].rgb[k]))) {
      c.fillStyle = `rgba(${V[0].rgb},${V[0].a * r.a})`; c.beginPath();
      for (const tri of tris) { let T = tri.map((i) => V[i]); if (T.some((v) => !v)) continue; if ((T[1].x - T[0].x) * (T[2].y - T[0].y) - (T[2].x - T[0].x) * (T[1].y - T[0].y) < 0) T = [T[0], T[2], T[1]];   // one winding: the nonzero union
        T.forEach((v, i) => c[i ? 'lineTo' : 'moveTo'](r.x + v.x, r.y + v.y)); c.closePath(); }
      c.fill('nonzero'); c.restore(); return;
    }
    for (const tri of tris) {
      const T = tri.map((i) => V[i]); if (T.some((v) => !v)) continue;
      // Gouraud: alpha and colour are linear over the triangle. The gradient follows the plane of the channel that
      // varies most (alpha, else a colour channel), from its lowest vertex along the slope, so a quad split in two
      // triangles keeps a straight (e.g. vertical) ramp; its stops are the two extreme vertices' colours.
      const channel = (v, k) => (k === 3 ? v.a * 255 : v.rgb[k]);
      let key = 3, range = 0;
      for (const k of [3, 0, 1, 2]) { const vals = T.map((v) => channel(v, k)), d = Math.max(...vals) - Math.min(...vals); if (d > range + 1e-9) { range = d; key = k; } }
      const lo = T.reduce((m, v) => (channel(v, key) < channel(m, key) ? v : m)), hi = T.reduce((m, v) => (channel(v, key) > channel(m, key) ? v : m));
      const avg = [0, 1, 2].map((ch) => Math.round(T.reduce((s, v) => s + v.rgb[ch], 0) / 3)), aa = T.reduce((s, v) => s + v.a, 0) / 3;
      let fill = `rgba(${avg},${aa * r.a})`;
      if (range > 0) {
        const f = (v) => channel(v, key), [p0, q, w] = T, dxq = q.x - p0.x, dyq = q.y - p0.y, dxr = w.x - p0.x, dyr = w.y - p0.y, det = dxq * dyr - dxr * dyq;
        let ex = hi.x, ey = hi.y;
        if (det) { const A = ((f(q) - f(p0)) * dyr - (f(w) - f(p0)) * dyq) / det, B = (dxq * (f(w) - f(p0)) - dxr * (f(q) - f(p0))) / det, k = (f(hi) - f(lo)) / (A * A + B * B);
          if (Number.isFinite(k)) { ex = lo.x + A * k; ey = lo.y + B * k; } }
        const g = c.createLinearGradient(r.x + lo.x, r.y + lo.y, r.x + ex, r.y + ey);
        g.addColorStop(0, `rgba(${lo.rgb},${lo.a * r.a})`); g.addColorStop(1, `rgba(${hi.rgb},${hi.a * r.a})`); fill = g;
      }
      c.fillStyle = fill; c.beginPath(); T.forEach((v, i) => c[i ? 'lineTo' : 'moveTo'](r.x + v.x, r.y + v.y)); c.closePath(); c.fill();
    }
    c.restore();
  }

  // FEFONT text at scale% * FONT_K PS2 pixels; anchor bits as for sprites; word-wrapped to the element width
  // (an override may give props.pitch, the wrapped line pitch in 480-line pixels, and props.sy, a vertical scale %).
  // flagWrap (opt-in, web/results-lui.js pv resultsMenu): only a text element whose flags have bit 7 (0x80) wraps, as on the
  // PS2 (0x3A0528: element +0x14 bit 7 -> 0x3A0D00 word wrap at the width +0x60 with the scale +0x50; otherwise 0x3A0EB0
  // breaks only at explicit line breaks); `wrap` false keeps the text on its lines. The other screens wrap every text as before.
  text(c, text, x, y, p, alpha = 1, wrap = true) {
    const s = FONT_K * (p[9] ?? 100) / 100, f = p[12] ?? 9, align = f & 32 ? 'right' : f & 16 ? 'center' : 'left';
    const rgb = [p[14] ?? 0, p[15] ?? 0, p[16] ?? 0].map((v) => v * TEXT_TEXEL);
    const hex = '#' + rgb.map((v) => Math.round(v).toString(16).padStart(2, '0')).join('');
    const glyphs = this.ui.fonts.FEFONT || {};
    const advance = (str) => [...str].reduce((w, ch) => w + (glyphs[ch]?.advance || 10), 0) * s;
    const max = (p[6] || 1e6) / PS2_X, lines = []; let line = '';
    if (!wrap) lines.push(...String(text).split(/\r\n|\r|\n/));
    else {
      // ps2Wrap (opt-in, web/results-lui.js pv luiWrap): 0x3A0D00's measure, the font's advances (392C60) x the element scale +0x50
      // (props 9 %) against the width +0x60 (props 6), both in PS2 units: 68rival_pre 'Face off against Nate in a Rival Challenge!'
      // 652 x 0.73 = 476 > 475 breaks after 'Rival', its bullet 742 x 0.6 = 445.2 <= 450 stays on one line (PS2 ruthless-ready).
      const over = this.ps2Wrap ? (str) => [...str].reduce((w, ch) => w + (glyphs[ch]?.advance || 10), 0) * (p[9] ?? 100) / 100 > (p[6] || 1e6) : (str) => advance(str) > max;
      for (const word of String(text).split(' ')) { const next = line ? line + ' ' + word : word; if (line && over(next)) { lines.push(line); line = word; } else line = next; }
      if (line) lines.push(line);
    }
    // keepLead (opt-in, web/results-lui.js): the text's leading spaces stay on its first line, as the PS2 font draws them (the
    // rewards list indents an award's items as "      %s", 0x1FF7B8)
    const lead = this.keepLead && wrap ? /^ */.exec(String(text))[0] : ''; if (lead && lines.length) lines[0] = lead + lines[0];   // (unwrapped lines keep theirs)
    const sy = p.sy != null ? FONT_K * p.sy / 100 : s;                     // optional vertical scale % (override only)
    const pitch = p.pitch ?? LINE * sy * PS2_Y, dy = f & 2 ? -(lines.length - 1) * pitch / 2 - 11 * sy * PS2_Y * TEXT_TALL : 0;
    c.save(); c.globalAlpha = Math.min(1, alpha); c.translate(x, y + dy); c.scale(PS2_X / PS2_Y * s / sy / TEXT_TALL, 1);
    lines.forEach((ln, i) => this.ui.text(c, ln, 0, i * pitch, 22 * sy * PS2_Y * TEXT_TALL, hex, 'FEFONT', align));
    c.restore();
  }
}
