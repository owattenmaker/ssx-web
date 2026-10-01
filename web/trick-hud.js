// Original single-player trick-scoring HUD (docs/tricks-scoring.md).
// Ports the slot cases of the HUD draw 0x1EC3F8 (slot loop in 0x1E9A30, dispatch table 0x46EC70) for the
// score object's message bank (engine/score_object.hpp): layout descriptors at 0x4768B0, text 0x1F1B30,
// interpolated text 0x1F1E28, wrapped text 0x1F2AA0, grade stars 0x21F338, lost-point digits 0x1F14B0,
// measure 0x391FB0 and alignment 0x21E750/0x21E7A8. Output is a draw list in PS2 640x480 HUD space:
// text draws carry the font state that 0x391CB0 sees (position, scale, ARGB, shadow), sprites the
// 0x1F1190 arguments, rects the 0x1F1338 flat quads (the freestyle standings' human row). Float32 arithmetic via Math.fround; `web/test-trick-hud.mjs` checks the list
// against original draws captured by tools/probe_trick_hud.py.
import { keyCapWidth, drawKeyCap, LUI_STRETCH } from './input-glyphs.js';
import { SPRITE_2D } from './sprite-canvas.js'; // offscreen sprite canvas kind (software in Firefox, docs/firefox-load.md)
const FREE = 0x34;
const HUD_DT = Math.fround(0.01666666753590107); // 1/60 s (0x3C888889: 116FB8 slot clocks, gp-0x5744 the big-message clock)
// EE COP1 rounds toward zero: products/sums of float32 values are exact in double, then chopped to float32.
const buf = new Float32Array(1), bits = new Uint32Array(buf.buffer);
function chop(v) { buf[0] = v; if (Math.abs(buf[0]) > Math.abs(v)) bits[0] -= 1; return buf[0]; }
const f = (v) => chop(v);
// Fonts: owner+0x428 = 0x5A0100 (data/fonts/fefont.sfn, height 29), owner+0x42C = 0x5A0200 (hudfont.sfn, 21).
export const HUD_FONTS = { FEFONT: { height: 29 }, HUDFONT: { height: 21 } };
// A font draw's vertex RGB byte (renderer 0x378808: trunc(colour x 204.0), 0x434C0000) over texels of unity 0x80.
export const hudTextByte = (v) => Math.max(0, Math.min(255, Math.trunc(Math.fround(v * 204))));
// 0x198AF0 (English): cash as "$ n" with thousands separators (the career cash popups 0x2E/0x2F and 'Collect +%s' 0x31).
export const money = (n) => '$ ' + Math.max(0, Math.trunc(n)).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');

export class TrickHud {
  constructor(data, glyphs) {
    this.d = data.descriptors;
    this.sprites = data.sprites || {};
    this.colours = data.colours || {};
    this.sin = data.sin || [];
    this.strings = data.strings || {};
    this.flags = data.flags >>> 0;
    this.glyphs = {};
    this.clock = data.clock || null;
    for (const [name, table] of Object.entries(glyphs)) {
      const t = {};
      for (const [ch, g] of Object.entries(table))
        t[ch] = {
          ...g,
          dy: g.dy > 127 ? g.dy - 256 : g.dy,
          dx: g.dx > 127 ? g.dx - 256 : g.dx,
          advance: g.advance > 127 ? g.advance - 256 : g.advance
        };
      this.glyphs[name] = t;
    }
    this.font = { FEFONT: { shadow: [2, 2] }, HUDFONT: { shadow: [2, 2] } };
    this.big = { id: 0x50, t: 0, cash: 0 };
    this.prepass = null;
  }
  // The host hands over the bank as the next game tick starts (web/game-tick.js present): the HUD update 0x1EA930 of that frame.
  get prepassSlots() {
    return this.prepass;
  }
  set prepassSlots(slots) {
    this.prepass = slots;
    this.tickBigMessage(slots);
  }
  // Big centre message (owner +0x3C4 id, +0x3C0 time, +0x3C8 cash; 0x50 = none), per 60 Hz HUD update 0x1EA930:
  // 0x1EB3E4 advances a live message by 1/60 (clamped at 1; a message at >= 1 ends: 0x50), then the pre-pass 0x1EC07C starts
  // one when a score slot was just posted (value / maximum == 0): slot 0x1A (11A0E0 from 10F2B8: a Big Challenge starts,
  // 3071F0) -> 8 "GO!", else slot 0x1B (11A110: MISSION SUCCESS popup) -> 0x3A with its cash. HUD command 6 (the race
  // countdown's GO, 234C68 -> 0x1EC378) also starts 8: command(6).
  // Tick phase: the port runs the WScript tick (the posts) in race_begin, before the rider pass whose 117C28 advances the
  // slot clocks, so the bank read before the next tick carries one clock already (value 1/60) where the PS2's HUD update,
  // which runs between its WScript tick and the next rider pass (PS2 runs/bigchal: 0x1A / 0x1B recorded at 0 on the post's
  // record), sees 0. A post seen for the first time at 0 or 1/60 is that pre-pass's "ratio == 0": the same drawn frame.
  tickBigMessage(slots) {
    const m = this.big,
      seen = (this.bigSeen ??= {});
    if (m.id !== 0x50) {
      if (1 <= m.t) {
        m.id = 0x50;
        m.t = 0;
      } else {
        const t = f(m.t + HUD_DT);
        m.t = 1 < t ? 1 : t;
      }
    }
    if (!slots) {
      seen[0x1a] = seen[0x1b] = -1;
      return;
    }
    const fresh = (k) => {
      const s = slots[k],
        v = s && s.type !== FREE ? s.value : -1,
        before = seen[k] ?? -1;
      seen[k] = v;
      return v >= 0 && v <= HUD_DT && !(before >= 0 && before <= v);
    };
    const go = fresh(0x1a),
      success = fresh(0x1b);
    if (go) {
      m.id = 8;
      m.t = 0;
      m.cash = 0;
    } else if (success) {
      m.id = 0x3a;
      m.t = 0;
      m.cash = slots[0x1b].points | 0;
    }
  }
  command(k) {
    if (k === 6) {
      this.big.id = 8;
      this.big.t = 0;
      this.big.cash = 0;
    }
  }
  // 0x1F07EC after the players (no flag test): 8 = the GO! sprite (owner +0x434) at descriptor 8 growing 0.4 -> 1 and fading
  // out after 0.6; 0x3A = "MISSION SUCCESS" (0x46EC18) at descriptor 58 and the cash (0x198AF0) at 59, the same grow and fade.
  bigMessage(out) {
    const m = this.big,
      t = m.t,
      grow = f(f(t * f(0.600000024)) + f(0.400000006));
    if (m.id === 8 && this.sprites.go) {
      const d = this.desc(8),
        w = f(d.w * grow),
        h = f(d.h * grow),
        argb = d.argb.slice();
      argb[0] = f(1 - f(f(t - f(0.600000024)) * f(1.66666663)));
      out.push({
        kind: 'sprite',
        sprite: this.sprites.go,
        x: TrickHud.align(d.x, w, d.scale[0], d.align[0]),
        y: TrickHud.align(d.y, h, d.scale[1], d.align[1]),
        size: [w, h],
        scale: d.scale.slice(),
        argb,
        order: d.order
      });
    } else if (m.id === 0x3a) {
      const fade = f(0.600000024) < t ? f(1 - f(f(t - f(0.600000024)) * f(1.66666663))) : 1;
      const c1 = this.desc(0x3a).argb.slice();
      c1[0] = f(c1[0] * fade);
      this.text(out, 'HUDFONT', 0x3a, this.strings.missionSuccess ?? 'MISSION SUCCESS', { scale: [grow, grow], colour: c1 });
      if (m.cash > 0) {
        const c2 = this.desc(0x3b).argb.slice();
        c2[0] = f(c2[0] * fade);
        this.text(out, 'HUDFONT', 0x3b, money(m.cash), { scale: [grow, grow], colour: c2 });
      }
    }
  }
  // 0x391FB0: bounding box of `text` at scale (sx, sy) including the font shadow offset.
  measure(font, text, sx, sy, advanceOnly = false) {
    const g = this.glyphs[font];
    let pen = 0,
      minX = 0,
      minY = 0,
      maxX = 0,
      maxY = 0;
    for (const ch of text) {
      const q = g[ch];
      if (!q) continue;
      minX = Math.min(f(pen + f(q.dx * sx)), minX);
      const adv = f(q.advance * sy === 0 ? 0 : q.advance * sx);
      minY = Math.min(f(q.dy * sy), minY);
      maxY = Math.max(f((q.dy + q.h) * sy), maxY);
      if (!advanceOnly) {
        const next = f(pen + adv);
        maxX = Math.max(f(pen + f((q.dx + q.w) * sx)), maxX);
        pen = next;
      } else {
        pen = f(pen + adv);
        maxX = Math.max(pen, maxX);
      }
    }
    let w = f(maxX + minX),
      h = f(maxY + minY);
    const [shx, shy] = this.font[font].shadow;
    w = shx < 0 ? f(w - shx) : f(w + shx);
    h = shy < 0 ? f(h - shy) : f(h + shy);
    return { w, h };
  }
  // Switch-stance icon (the race HUD 0x1EC3F8 at 0x1F0288..0x1F0368, HUD flags 0x10000000; free rides too): owner +0x470's 'S'
  // sprite (OV_1-4) at descriptor 0x4F (583, 408, 24 x 22, centred), its alpha x 0.2 (gp-0x55B0) while rider +0x320 == +0x324
  // (the regular stance: a faint S under the meter on every PS2 frame), full while riding switch (PS2 peak2/dss2-full 4018).
  switchIcon(out, regular) {
    const sprite = this.sprites.switchIcon;
    if (!sprite) return;
    const d = this.desc(0x4f),
      argb = d.argb.slice();
    if (regular) argb[0] = f(argb[0] * f(0.200000003));
    const w = f(d.w * d.scale[0]),
      h = f(d.h * d.scale[1]);
    out.push({
      kind: 'sprite',
      sprite,
      x: TrickHud.align(d.x, w, 1, d.align[0]),
      y: TrickHud.align(d.y, h, 1, d.align[1]),
      size: [w, h],
      scale: [1, 1],
      argb,
      order: d.order
    });
  }
  static align(x, size, scale, a) {
    if (a === 0) return f(x - f(f(scale * size) * 0.5));
    if (a > 0) return f(x - f(scale * size));
    return x;
  }
  desc(k) {
    return this.d[k];
  }
  // 0x1F1B30: text at descriptor `k` (optional scale multiplier, offset, colour override, alignment overrides).
  text(out, font, k, text, o = {}) {
    const d = this.desc(k);
    const scale = o.scale ? [f(d.scale[0] * o.scale[0]), f(d.scale[1] * o.scale[1])] : [d.scale[0], d.scale[1]];
    const size = this.measure(font, text, scale[0], scale[1]);
    const ah = o.alignH ?? d.align[0],
      av = o.alignV ?? d.align[1];
    let x = TrickHud.align(d.x, size.w, 1, ah),
      y = TrickHud.align(d.y, size.h, 1, av);
    if (o.offset) {
      x = f(x + o.offset[0]);
      y = f(y + o.offset[1]);
    }
    const argb = o.colour ? o.colour.slice() : d.argb.slice();
    out.push({
      kind: 'text',
      font,
      text,
      x,
      y,
      scale,
      argb,
      shadow: this.font[font].shadow.slice(),
      shadowArgb: [argb[0], 0, 0, 0],
      order: d.order
    });
    return { x, y, w: size.w, h: size.h };
  }
  // 0x1F1E28: text interpolated from descriptor `a` to `b` by t (colour lerp, or override colour fading by 1-t).
  textLerp(out, font, a, b, t, text, o = {}) {
    const size = this.measure(font, text, 1, 1);
    const da = this.desc(a),
      db = this.desc(b);
    const sa = o.scale ? [f(da.scale[0] * o.scale[0]), f(da.scale[1] * o.scale[1])] : da.scale.slice();
    const sb = o.scale ? [f(db.scale[0] * o.scale[0]), f(db.scale[1] * o.scale[1])] : db.scale.slice();
    const ah = (d) => o.alignH ?? d.align[0],
      av = (d) => o.alignV ?? d.align[1];
    const pa = [TrickHud.align(da.x, f(size.w * sa[0]), 1, ah(da)), TrickHud.align(da.y, f(size.h * sa[1]), 1, av(da))];
    const pb = [TrickHud.align(db.x, f(size.w * sb[0]), 1, ah(db)), TrickHud.align(db.y, f(size.h * sb[1]), 1, av(db))];
    const scale = [f(f(f(sb[0] - sa[0]) * t) + sa[0]), f(f(f(sb[1] - sa[1]) * t) + sa[1])];
    let x = f(f(f(pb[0] - pa[0]) * t) + pa[0]),
      y = f(f(f(pb[1] - pa[1]) * t) + pa[1]);
    if (o.offset) {
      x = f(x + o.offset[0]);
      y = f(y + o.offset[1]);
    }
    let argb;
    if (o.colour) {
      argb = o.colour.slice();
      argb[0] = f(argb[0] * f(1 - t));
    } else {
      const to = [0, db.argb[1], db.argb[2], db.argb[3]];
      argb = da.argb.map((c, i) => f(f(f(to[i] - c) * t) + c));
    } // 0x1F2148: target alpha 0
    out.push({
      kind: 'text',
      font,
      text,
      x,
      y,
      scale,
      argb,
      shadow: this.font[font].shadow.slice(),
      shadowArgb: null,
      order: this.d[0x1a].order
    });
    return { x, y, w: f(size.w * scale[0]), h: f(size.h * scale[1]) };
  }
  // 0x1F2AA0: word-wrapped lines (0x392430 breaks at spaces within the descriptor width) stacked from the descriptor.
  wrapped(out, font, k, text) {
    const d = this.desc(k);
    const [sx, sy] = d.scale;
    const limit = f(d.w - Math.abs(this.font[font].shadow[0]));
    const lines = [];
    let line = '';
    for (const word of text.split(' ')) {
      const next = line ? line + ' ' + word : word;
      if (line && this.measure(font, next, sx, sy).w > limit) {
        lines.push(line);
        line = word;
      } else line = next;
    }
    if (line || !lines.length) lines.push(line);
    const lineH = f(f(HUD_FONTS[font].height * 1) * sy);
    const total = f(lineH * lines.length);
    let y = d.align[1] === 0 ? f(d.y - f(total * 0.5)) : d.align[1] > 0 ? f(d.y - total) : d.y;
    for (const l of lines) {
      const w = this.measure(font, l, sx, sy).w;
      const x = d.align[0] === 0 ? f(d.x - f(w * 0.5)) : d.align[0] > 0 ? f(d.x - w) : d.x;
      out.push({
        kind: 'text',
        font,
        text: l,
        x,
        y,
        scale: [sx, sy],
        argb: d.argb.slice(),
        shadow: this.font[font].shadow.slice(),
        shadowArgb: [d.argb[0], 0, 0, 0],
        order: d.order
      });
      y = f(y + lineH);
    }
  }
  // 0x21F338: `grade` stars at descriptor 0x10 (or centred on `at`), colour table 0x4C8568, alpha multiplier.
  stars(out, grade, scale, alpha, at) {
    if (grade <= 0) return;
    const d = this.desc(0x10);
    const sprite = this.sprites.star;
    const sc = scale ? [f(d.scale[0] * scale[0]), f(d.scale[1] * scale[1])] : d.scale.slice();
    const total = f(f(d.w * grade) + f(grade - 1 + (grade - 1)));
    let x, y;
    if (!at) {
      x = TrickHud.align(d.x, total, sc[0], d.align[0]);
      y = TrickHud.align(d.y, d.h, sc[1], d.align[1]);
    } else {
      x = f(at[0] - f(f(total * sc[0]) * 0.5));
      y = f(at[1] - f(f(d.h * sc[1]) * 0.5));
    }
    const colour = this.colours.stars[grade - 1].slice();
    colour[0] = f(colour[0] * alpha);
    const step = f(f(d.w + 2) * sc[0]);
    for (let k = 0; k < grade; k++) {
      out.push({ kind: 'sprite', sprite, x, y, w: d.w, h: d.h, scale: sc, argb: colour.slice(), order: d.order });
      x = f(x + step);
    }
  }
  pulse(t, amplitude = 0.2, rate = 12.566371) {
    const i = Math.trunc(f(f(t * f(rate)) * 81.4873276)) & 511;
    return f(f(f(this.sin[i] * amplitude) * f(1 - f(t * t))) + 1);
  }
  // Draw list for one frame. slots[k] = {type, maximum, value, arg, field10, points, text}; state = {players, pulse70, extraPoints7C}.
  frame(slots, state = {}) {
    const out = [];
    const flags = state.flags ?? this.flags;
    const players = state.players ?? 1;
    const ratio = (s) => s.ratio ?? Math.fround(s.value / s.maximum); // DIV.S rounds to nearest
    const flash = state.flash ?? (slots[0x18] && slots[0x18].type !== FREE ? ratio(slots[0x18]) : -1);
    const f25 = state.pulse25 ?? (flash >= 0 ? this.pulse(flash) : 1);
    let label = null,
      f26 = -1,
      f28 = 0,
      recovered = 0;
    for (let k = 0; k < slots.length; k++) {
      const s = slots[k];
      if (!s || s.type === FREE || s.type >= 0x34) continue;
      const r = ratio(s);
      const t = s.type;
      const text = s.text ?? String(s.points);
      if (t === 0) {
        if (flags & 0x4000) this.wrapped(out, 'FEFONT', 0x1d, text);
      } else if (t === 1) {
        if (flags & 0x4000000) {
          let sc = 1;
          if ((state.pulse70 ?? -1) >= 0) sc = this.pulse(state.pulse70);
          this.text(out, 'HUDFONT', 0x12, text, { scale: [sc, sc] });
          this.stars(out, s.arg, [sc, sc], 1);
        }
      } else if (t === 2) {
        if (flags & 0x4000000) this.bonus(out, s, r);
      } else if (t === 4) {
        if (flags & 0x4000000) {
          let sc = 1,
            colour = null;
          if (r < 1) {
            sc = this.pulse(r, f(0.150000006));
            colour = this.colours.multiplier;
          }
          this.text(out, 'HUDFONT', 0x1c, 'X' + text, { scale: [sc, sc], colour });
        }
      } else if (t === 3) {
        if (flags & 0x4000000) this.combo(out, s, r);
      } else if (t === 7) {
        if (flags & 0x402) {
          const value = String((s.points + (state.extraPoints7C ?? 0)) | 0);
          if (flash < 0) this.text(out, 'HUDFONT', 0x11, value);
          else this.text(out, 'HUDFONT', 0x11, value, { scale: [f25, f25], colour: this.colours.green });
        }
      } else if (t === 0xb) {
        if (f26 < 0) {
          f28 = r;
          f26 = 0;
        }
      } else if (t === 0xe) {
        if (players === 1 && flags & 0x4000000) this.text(out, 'FEFONT', 0x46, text);
      } else if (t >= 0x1c && t <= 0x20) label = this.strings.bonus[t - 0x1c];
      else if (t === 0x21) {
        this.text(out, 'FEFONT', 0x35, this.strings.recovered);
        f26 = r;
        f28 = 1;
        recovered = 1;
      } else if (t === 0x23 || t === 0x24) {
        if (flags & 0x4000000 && !(flags & 0x200)) this.popup(out, s, r);
      } else if (t === 0x25 || t === 0x27 || t === 0x28) {
        if (flags & 0x4000000) this.lost(out, s, r);
      } else if (t === 0x26) {
        if (flags & 0x4000000 && !(flags & 0x200)) this.comboBonus(out, s, r);
      }
      // 0x1EE690: case 0x30 is 0x26 for Conquer the Mountain cash (119EF8 kind 2: a paid-out combo, flags 0x200): the money text.
      else if (t === 0x30) {
        if (flags & 0x4000000 && flags & 0x200) this.comboBonus(out, s, r, money(s.points));
      } else if (t === 0x2e || t === 0x2f) {
        if (flags & 0x4000000 && flags & 0x200) this.cashPopup(out, s, r);
      }
      // 0x1EF624: with flags 0x100000 and the pre-pass display mode +0x88 = 3 (0x1EBB00: set while a 0x31 slot lives, unless the
      // finish / time-up modes 1/2 hold; a checkpoint split 0x29..0x2B under flags 0x4 clears 0x100000 instead).
      else if (t === 0x31) {
        const mode = state.collectMode ?? (state.finished ? 1 : 3);
        if (flags & 0x100000 && mode === 3 && !(flags & 0x4 && slots.some((x) => x && x.type >= 0x29 && x.type <= 0x2b)))
          this.text(out, 'HUDFONT', 1, 'Collect +' + money(s.points));
      } else if (t === 0x29 || t === 0x2a) {
        if (flags & 0x4) this.split(out, s, r);
      } else if (t === 0x2b) {
        if (flags & 0x2) this.split(out, s, r);
      }
    }
    if (label && players === 1 && flags & 0x4000000) {
      this.text(out, 'FEFONT', 0, label);
      out[out.length - 1].epilogue = true;
    }
    this.uberHint(out, slots, flags, state);
    // The original meter 0x21D9A0 (needs the trick-hud.json records of tools/probe_trick_hud.py); else the placeholder bar.
    if (f26 >= 0) {
      if (this.sprites.recoverBar) this.recoverMeter(out, f28, f26, !recovered && (flags & 0x1000000) !== 0, state);
      else out.push({ kind: 'recover', fill: f28, fade: f26, recovered });
    }
    if (state.bigMessage !== false && this.sprites.go) this.bigMessage(out); // trick-hud.json with the GO! record (tools/probe_trick_hud.py)
    return out;
  }
  // Uber trick hint (docs/tricks-scoring.md "Uber trick hint"): owner+0x560, the "UBER TRICK = @l1 + @square" / "@r1" string
  // (gp-0xF20[owner+0x55C]) laid out by 0x1E92A8 at FEFONT x 0.7 and drawn by 0x1E95A0 at descriptor 73 after the slot
  // loop (0x1F0228) when the per-player flags have bits 21 and 24 and the pre-pass (0x1EBCA4) grants bit 25: Tricky slot 9
  // live with value/maximum != 1 and no live slot of type 0, 0x21 or 0xB. owner+0x55C starts as rand() & 1 (0x3177F0 on the
  // visual RNG at HUD init) and flips each pre-pass that sees the slot-9 ratio at exactly 1 (a Tricky run ending).
  static HINT_ICONS = {
    l1: { w: 28, h: 16, uv: [0.681640625, 0.591796875, 0.697265625, 0.740234375] },
    r1: { w: 28, h: 16, uv: [0.615234375, 0.474609375, 0.580078125, 0.673828125] },
    square: { w: 21, h: 20, uv: [0.482421875, 0.134765625, 0.212890625, 0.556640625] }
  }; // table 0x4C8980 (bsl1 / bsr1 / square, OV_1-2)
  resetUberHint(bit) {
    this.hint55C = bit & 1;
  }
  uberHint(out, current, flags, state) {
    const slots = state.prepassSlots ?? this.prepassSlots ?? current; // the pre-pass reads the bank one tick earlier than the draws
    const live = (k) => slots[k] && slots[k].type !== FREE;
    const s9 = live(9) ? slots[9] : null;
    if (s9 && Math.fround(s9.value / s9.maximum) === 1) this.hint55C = (this.hint55C ?? 1) ^ 1;
    if (state.finished || !(flags & 0x200000) || !(flags & 0x1000000) || !s9 || Math.fround(s9.value / s9.maximum) === 1) return;
    if (slots.some((x, k) => x && x.type !== FREE && (x.type === 0 || x.type === 0x21 || x.type === 0xb) && k >= 0)) return;
    const icon = (this.hint55C ?? 1) ? 'r1' : 'l1',
      sc = f(0.7),
      runs = ['UBER TRICK = ', ' + ', ''],
      icons = [icon, 'square'];
    // The 0x1E92A8 record itself (iconLayout: widths without the font shadow, no empty last piece; PS2 owner +0x560:
    // W 209.99992, pieces 144.9 / 28 / 16.1 / 21), so the text starts at x 215 where this layout put it at 212. Key caps (keyboard)
    // are wider than the icons they replace: that layout stays on the port's own measure below.
    if (!(state.keys?.(icon) || state.keys?.('square'))) {
      this.iconLabel(out, ((this.uberLayouts ??= {})[icon] ??= this.iconLayout(`UBER TRICK = @${icon} + @square`, sc)), 73, state, {
        hint: true
      });
      return;
    }
    // state.keys(button) -> key cap label while the keyboard is in use (web/input-glyphs.js): the cap takes the icon's
    // place in the layout (16 high, as wide as the label), drawn by TrickHudRenderer.
    const dims = (k) => {
      const label = state.keys?.(k);
      return label ? { w: f(keyCapWidth(this.glyphs.FEFONT, label, 16, LUI_STRETCH)), h: 16, label } : TrickHud.HINT_ICONS[k];
    };
    const widths = runs.map((t, k) => this.measure('FEFONT', t, sc, sc, k < runs.length - 1).w);
    let W = 0;
    runs.forEach((_, k) => {
      W = f(W + widths[k]);
      if (k < icons.length) W = f(W + dims(icons[k]).w);
    });
    const textH = f(f(HUD_FONTS.FEFONT.height * 1) * sc);
    const H = Math.max(textH, ...icons.map((k) => dims(k).h));
    const offY = f(f(H - textH) * 0.5);
    const d = this.desc(73);
    let x = TrickHud.align(d.x, W, d.scale[0], d.align[0]);
    const y = TrickHud.align(d.y, H, d.scale[1], d.align[1]);
    const cy = f(y + f(f(H * d.scale[1]) * 0.5));
    runs.forEach((t, k) => {
      if (t)
        out.push({
          kind: 'text',
          font: 'FEFONT',
          text: t,
          x,
          y: f(y + f(offY * d.scale[1])),
          scale: [sc, sc],
          argb: d.argb.slice(),
          shadow: [2, 2],
          shadowArgb: [d.argb[0], 0, 0, 0],
          order: d.order,
          hint: true
        });
      x = f(x + f(widths[k] * d.scale[0]));
      if (k < icons.length) {
        const ic = dims(icons[k]);
        out.push({
          kind: 'sprite',
          sprite: { page: 'OV_1-2', uv: TrickHud.HINT_ICONS[icons[k]].uv },
          ...(ic.label ? { key: ic.label } : {}),
          x,
          y: f(cy - f(f(ic.h * d.scale[1]) * 0.5)),
          size: [ic.w, ic.h],
          scale: d.scale.slice(),
          argb: d.argb.slice(),
          order: d.order,
          hint: true
        });
        x = f(x + f(ic.w * d.scale[0]));
      }
    });
  }
  // 0x1E92A8: an "@icon" string laid out in FEFONT at `sc` (owner +0x560 / +0x6AC / +0x7F8 records, 0x14C bytes: +0 width, +4 height,
  // +0xC text y offset, +0x18 pieces {width, text or icon record 0x4C8980}). A text piece is measured by 0x3921F0 advance-only when an
  // icon follows (a3 = 1); the widths carry no font shadow (the snapshot's layout: "RECOVER = " 124.59997 at 0.7, where 391FB0 with
  // the draw-time shadow (2, 2) gives 126.59997); an '@name' ends at a space or the end, the space starts the next text piece, and
  // an empty last piece is dropped. Height: max(font height x sc, the tallest icon); offset (height - font height x sc) / 2.
  iconLayout(source, sc) {
    const pieces = [],
      parts = source.split(/@(\w+)/),
      shadow = this.font.FEFONT.shadow;
    this.font.FEFONT.shadow = [0, 0];
    parts.forEach((p, k) => {
      if (k & 1) {
        const ic = TrickHud.HINT_ICONS[p];
        if (ic) pieces.push({ icon: p, w: ic.w, h: ic.h });
      } else if (p) pieces.push({ text: p, w: this.measure('FEFONT', p, sc, sc, k < parts.length - 1).w });
    });
    this.font.FEFONT.shadow = shadow;
    let W = 0,
      H = f(f(HUD_FONTS.FEFONT.height * 1) * sc);
    const textH = H;
    for (const q of pieces) {
      W = f(W + q.w);
      if (q.icon && H < q.h) H = q.h;
    }
    return { pieces, sc, W, H, offY: f(f(H - textH) * 0.5) };
  }
  // 0x1E95A0: draws a layout at descriptor `k` (position, scale S, colour, alignment; 1F10F8-style on W x S / H x S): text pieces at
  // (pen, y + offY x S.y) in the font at sc x S with the (2, 2) shadow (shadow alpha x colour alpha), icons (vertex colour x 128)
  // centred on y + H x S.y / 2 at their size x S; the pen advances by width x S. state.keys(button) -> a key cap in the icon's place.
  iconLabel(out, layout, k, state = {}, tag = {}) {
    const d = this.desc(k),
      S = d.scale,
      argb = d.argb.slice(),
      sc = [f(layout.sc * S[0]), f(layout.sc * S[1])];
    let x = TrickHud.align(d.x, layout.W, S[0], d.align[0]);
    const y = TrickHud.align(d.y, layout.H, S[1], d.align[1]),
      cy = f(y + f(f(layout.H * S[1]) * 0.5));
    for (const q of layout.pieces) {
      if (q.text)
        out.push({
          kind: 'text',
          font: 'FEFONT',
          text: q.text,
          x,
          y: f(y + f(layout.offY * S[1])),
          scale: sc.slice(),
          argb: argb.slice(),
          shadow: [2, 2],
          shadowArgb: [argb[0], 0, 0, 0],
          order: d.order,
          ...tag
        });
      else {
        const label = state.keys?.(q.icon),
          w = label ? f(keyCapWidth(this.glyphs.FEFONT, label, 16, LUI_STRETCH)) : q.w,
          h = label ? 16 : q.h;
        out.push({
          kind: 'sprite',
          sprite: { page: 'OV_1-2', uv: TrickHud.HINT_ICONS[q.icon].uv },
          ...(label ? { key: label } : {}),
          x,
          y: f(cy - f(f(h * S[1]) * 0.5)),
          size: [w, h],
          scale: S.slice(),
          argb: argb.slice(),
          order: d.order,
          ...tag
        });
        x = f(x + f(w * S[0]));
        continue;
      }
      x = f(x + f(q.w * S[0]));
    }
  }
  // Crash recover meter 0x21D9A0, called after the slot loop (0x1EFB3C: f26 >= 0) at 0x1EFC34 with a1 the layout
  // table (0x4768B0; 0x4773F0 with two players), a2 = owner +0x54C (+0x554) {+0 end-cap count, +4 middle flag}, a3 the player (0: no
  // 21E750 mirroring), t0 = the label flag (no 0x21 slot this frame, sp+0x34C == 0, and per-player flags bit 24), t1 = (p, p), f12 =
  // f28 the fill (slot 0xB ratio, 1 under 0x21), f13 the fade. `r` = f26 (the 0x21 ratio, 0 for a lone 0xB):
  //   p = 1 + sin[trunc(t x 12.566371 x 81.487328) & 511] x 0.1500345 x (1 - t^2), t = r x 3.3330898, while r < 0.3000219 (0x1EFB50)
  //   fade = 1, 1 - (r - 0.2999631) x 4.9981575 past 0.2999631, 0 past 0.5000368 (0x1EFBB8)
  // Layout (descriptor 54: 320, 448, 154 x 12, centred, order 10; S = scale x p, W x H = 154 x 12 x S aligned by 1F10F8):
  //   label: "RECOVER = @square" (gp-0xF18, laid out at init 0x1EA828 / 0x1EA83C into owner +0x6AC and +0x7F8; +0x7F8 when the pad
  //     byte 0x534FE0[0x1C n + 0x13] of the player's port is set: the same string) through 0x1E95A0 at descriptor 55 (320, 435, bottom),
  //     not faded;
  //   the button: owner +0x4DC (+0x4E0 for that pad byte; the same 'square' record, OV_1-2) 20 x 20 x S at the bar's left, centred
  //     on its height; then 3 x S.x gap;
  //   the frame: +0x4D4 end cap (OV_1-4, 8 x 11 texels) count x 9 x S.x wide, +0x4D8 middle (only with the +4 flag) over the rest,
  //     the end cap again mirrored (u1 -> u0), all H high; textured vertex colour (128, 128, 128, trunc(fade x 128));
  //   the fill (fill > 0): the flat quad 0x1F1338 (0x4C8668 A,R,G,B = 1,1,0,0: vertex RGB x 255, alpha fade x 128) inset 3 x S
  //     (the static gp+0x2A38 = (3, 3)) from the frame after the button, (frame width - 6 x S.x) x fill wide, H - 6 x S.y high, drawn
  //     one layer up (render state +0xE84 word 8 bits 5..9 = descriptor order +1, 0x21E0D0; the frame at the order, 0x21DB60).
  // Init 0x1EA5C8: count 1 and the middle when the descriptor width - 23 (button + gap) - 18 (two caps) >= 0, else count =
  // (width - 23) / 18 (gp-0x5774) and no middle.
  recoverMeter(out, fill, r, label, state = {}) {
    let p = 1,
      fade = 1;
    if (r < f(0.3000219166278839)) p = this.pulse(f(r * f(3.333089828491211)), f(0.15003453195095062), f(12.566370964050293));
    if (f(0.5000368356704712) < r) fade = 0;
    else if (f(0.299963116645813) < r) fade = f(1 - f(f(r - f(0.299963116645813)) * f(4.998157501220703)));
    if (label)
      this.iconLabel(
        out,
        (this.recoverLabel ??= this.iconLayout(this.strings.recover ?? 'RECOVER = @square', f(0.699999988079071))),
        55,
        state,
        { recover: true }
      );
    const d = this.desc(54),
      rest = f(d.w - 23),
      full = f(rest - 18) >= 0,
      count = full ? 1 : f(rest * f(0.0555555559694767));
    const sx = f(d.scale[0] * p),
      sy = f(d.scale[1] * p),
      W = f(d.w * sx),
      H = f(d.h * sy);
    const x = TrickHud.align(d.x, W, 1, d.align[0]),
      y = TrickHud.align(d.y, H, 1, d.align[1]),
      argb = [fade, 1, 1, 1];
    const quad = (sprite, uv, qx, qy, w, h) =>
      out.push({
        kind: 'sprite',
        sprite: { page: sprite.page, uv },
        x: qx,
        y: qy,
        size: [w, h],
        scale: [1, 1],
        argb: argb.slice(),
        order: d.order,
        recover: true
      });
    const bw = f(sx * 20),
      bh = f(sy * 20),
      button = this.sprites.recoverButton?.[0] ?? { page: 'OV_1-2', uv: TrickHud.HINT_ICONS.square.uv };
    quad(button, button.uv, x, f(y + f(f(H - bh) * 0.5)), bw, bh);
    const lead = f(bw + f(sx * 3)),
      x0 = f(x + lead),
      inner = f(W - lead),
      cap = f(sx * f(count * 9)),
      end = this.sprites.recoverEnd,
      [v0, u0, u1, v1] = end.uv;
    let px = x0;
    quad(end, end.uv, px, y, cap, H);
    px = f(px + cap);
    if (full) {
      const mid = f(inner - f(cap + cap));
      quad(this.sprites.recoverBar, this.sprites.recoverBar.uv, px, y, mid, H);
      px = f(px + mid);
    }
    quad(end, [v0, u1, u0, v1], px, y, cap, H);
    if (0 < fill) {
      const ix = f(3 * sx),
        iy = f(3 * sy);
      out.push({
        kind: 'quad',
        x: f(x0 + ix),
        y: f(y + iy),
        size: [f(f(inner - f(ix + ix)) * fill), f(H - f(iy + iy))],
        argb: [f(1 * fade), 1, 0, 0],
        order: d.order + 1,
        recover: true
      });
    }
  }
  // Case 2 (0x1EEE44): "+bonus" (bonus84 + inverted points) and its box, pulsing green while the value settles.
  bonus(out, s, r) {
    let sc = 1,
      colour = null;
    if (r < 1) {
      sc = this.pulse(r);
      colour = this.colours.bonusGreen;
    }
    this.text(out, 'HUDFONT', 0xe, '+' + s.text, { scale: [sc, sc], colour });
    const d = this.desc(0xf);
    const scale = [f(d.scale[0] * sc), f(d.scale[1] * sc)];
    const w = f(d.w * scale[0]),
      h = f(d.h * scale[1]);
    out.push({
      kind: 'sprite',
      sprite: this.sprites.bonusBox,
      x: TrickHud.align(d.x, w, 1, d.align[0]),
      y: TrickHud.align(d.y, h, 1, d.align[1]),
      size: [w, h],
      scale: [1, 1],
      argb: (colour || d.argb).slice(),
      order: d.order
    });
  }
  // Case 3 (0x1EE8A0): combo. From two tricks "NX" (desc 0x0D at 2 x 2.4 scale, right-aligned) with the
  // stopwatch left of it; then "+points" and "Combo" above it (left-aligned from two tricks, centred for one,
  // where the stopwatch sits left of "Combo" at 2.4 x its height). The quarter hand shows the combo clock
  // (slot +0x10 = trunc(A4 * 3.2667): 0 -> +0x4C8, 1 -> +0x4C4, 2 -> +0x4C0, 3 -> +0x4BC, 4 none).
  // While the displayed value settles (ratio < 1) everything pulses (sine table) in green.
  combo(out, s, r) {
    let pulse = null,
      sc = 1;
    if (r < 1) {
      sc = this.pulse(r);
      pulse = this.colours.comboGreen;
    }
    const d = this.desc(0xd);
    const hands = { 0: this.sprites.hands[3], 1: this.sprites.hands[2], 2: this.sprites.hands[1], 3: this.sprites.hands[0] };
    const watch = (size, x) => {
      const px = TrickHud.align(x, size, 1, 1),
        py = TrickHud.align(d.y, size, 1, 0);
      out.push({
        kind: 'sprite',
        sprite: this.sprites.watch,
        x: px,
        y: py,
        size: [size, size],
        scale: [1, 1],
        argb: d.argb.slice(),
        order: d.order
      });
      const hand = hands[s.field10];
      if (hand)
        out.push({
          kind: 'sprite',
          sprite: hand,
          x: px,
          y: py,
          size: [size, size],
          scale: [1, 1],
          argb: d.argb.slice(),
          order: d.order + 1
        });
    };
    let align = 0;
    if (s.arg >= 2) {
      const box = this.text(out, 'HUDFONT', 0xd, `${s.arg}X`, {
        scale: [f(sc * 2), f(sc * f(2.4000001))],
        colour: pulse,
        offset: [0, -1],
        alignH: 1,
        alignV: 0
      });
      watch(box.h, f(d.x + f(-4 - box.w)));
      align = -1;
    }
    const plus = this.text(out, 'HUDFONT', 0xd, '+' + s.text, { scale: [sc, sc], colour: pulse, alignH: align, alignV: -1 });
    const label = this.text(out, 'HUDFONT', 0xd, this.strings.combo, {
      scale: [sc, sc],
      colour: pulse,
      alignH: align,
      alignV: -1,
      offset: [0, f(-plus.h)]
    });
    if (s.arg === 1) watch(f(label.h * f(2.4000001)), f(d.x + f(-16 - f(label.w * 0.5))));
  }
  // Cases 0x23/0x24 (0x1EE200): points fly from 0x15 (0x16 for a repeat) to the total 0x11. Before ratio
  // 0.2072 they pulse (sine table, t*4.827) in green (red for a repeat) fading by 1-t; after, the descriptor
  // colours interpolate at unit scale. Grade stars follow the text centre, fading with 1-t.
  popup(out, s, r) {
    const shadow = this.font.HUDFONT.shadow;
    this.font.HUDFONT.shadow = [0, 0];
    const from = s.type === 0x24 ? 0x16 : 0x15;
    let sc = 1,
      box;
    if (f(0.207151964) <= r) box = this.textLerp(out, 'HUDFONT', from, 0x11, r, s.text);
    else {
      sc = this.pulse(f(r * f(4.82737398)));
      const colour = (s.type === 0x24 ? this.colours.repeatPopup : this.colours.popup).slice();
      box = this.textLerp(out, 'HUDFONT', from, 0x11, r, s.text, { scale: [sc, sc], colour });
    }
    let alpha = 1;
    if (1 < r) alpha = 0;
    else if (0 < r) alpha = f(1 - r);
    this.stars(out, s.arg, [sc, sc], alpha, [f(box.x + f(box.w * 0.5)), f(box.y + f(box.h * 0.5))]);
    this.font.HUDFONT.shadow = shadow;
  }
  // Cases 0x29 / 0x2A / 0x2B (0x1ED990 / 0x1EDC6C): the checkpoint time bonus (0x29, slope style) and the peak-run station split
  // (0x2A time, 0x2B points; 23B5F8 / 23C560 -> 1195A8 / 1195D8, docs/peak3.md): the "CHECKPOINT" sprite (owner +0x4AC; +0x4B0
  // for 0x29) centred on descriptor 0x40 (0x41), pulsing in the first half (sine table, 2t, amplitude 0.1), and under it the
  // value: H:MM:SS through 1F1840 (sign '+' / '-', hour / minute / second pairs of 37 + 2 px, colons 3 + 2 px, a '1' digit
  // 4.75 px right, the group of 125 x 21 centred) for 0x29 / 0x2A, "+%d" / "%d" centred for 0x2B. Colours: 0x29 white;
  // a value >= 0 is red (0x4C88C8: behind the split time) for 0x2A and green (0x4C8888: ahead of the split score) for 0x2B.
  split(out, s, r) {
    const t = s.type,
      d = this.desc(t === 0x29 ? 0x41 : 0x40),
      sprite = t === 0x29 ? this.sprites.timeBonus : this.sprites.checkpoint;
    const pulse = r < 0.5 ? this.pulse(f(r + r), f(0.100000001)) : 1;
    const sc = [f(d.scale[0] * pulse), f(d.scale[1] * pulse)],
      w = f(d.w * sc[0]),
      h = f(d.h * sc[1]);
    const x = TrickHud.align(d.x, w, 1, d.align[0]),
      y = TrickHud.align(d.y, h, 1, d.align[1]);
    out.push({ kind: 'sprite', sprite, x, y, size: [w, h], scale: [1, 1], argb: d.argb.slice(), order: d.order });
    const cx = f(x + f(w * 0.5)),
      top = f(y + h),
      k = f(1.00012457),
      ts = [f(sc[0] * k), f(sc[1] * k)],
      value = s.points | 0;
    const colour = (
      t === 0x29 ? this.colours.timeBonus : value >= 0 === (t === 0x2a) ? this.colours.splitPositive : this.colours.splitNegative
    ).slice();
    const shadow = this.font.HUDFONT.shadow.slice(),
      piece = (text, px) =>
        out.push({
          kind: 'text',
          font: 'HUDFONT',
          text,
          x: px,
          y: top,
          scale: ts.slice(),
          argb: colour.slice(),
          shadow: shadow.slice(),
          shadowArgb: [colour[0], 0, 0, 0],
          order: d.order
        });
    if (t === 0x2b) {
      const text = value >= 0 ? `+${value}` : String(value),
        m = this.measure('HUDFONT', text, ts[0], ts[1]);
      piece(text, f(cx - f(m.w * 0.5)));
      return;
    }
    const c = this.clock || { gap: 3, pair: 37, sign: 11, one: 4.75 },
      a = Math.abs(value),
      two = (n) => String(n).padStart(2, '0');
    const width = f(f(c.pair * 3) + f(f(f(c.gap + 2) + 2) * 2));
    let px = f(cx - f(f(width * ts[0]) * 0.5));
    piece(value >= 0 ? (this.strings.plus ?? '+') : (this.strings.minus ?? '-'), px);
    px = f(px + c.sign);
    const pair = f(f(c.pair + 2) * ts[0]),
      colon = f(f(c.gap + 2) * ts[0]),
      one = f(c.one * ts[0]);
    const digits = (text) => {
      let o = 0;
      if (text[0] === '1') o = one;
      if (text[1] === '1') o = f(o + one);
      piece(text, f(px + o));
    };
    digits(two(Math.trunc(a / 3600)));
    for (const n of [Math.trunc(a / 60) % 60, a % 60]) {
      px = f(px + pair);
      piece(this.strings.colon ?? ':', px);
      px = f(px + colon);
      digits(two(n));
    }
  }
  // Race HUD clock (0x1F03A0 at 0x1F0F34 -> 0x1F16C0 -> 0x1F1840, raceHud, docs/visual-parity.md): "%02d" of the owner's
  // +0x17C / +0x180 / +0x184 (hours, minutes, seconds of 0x21F81C) at descriptor 0x1A (320, 20, centred, top), HUD font at the
  // descriptor scale, shadow (2, 2). 0x1F1840 with flags 0: the group is pair x 3 + (gap + 2 + 2) x 2 wide (125) and the font
  // height (21) high, aligned on the descriptor with its scale; a pair is drawn at pen + one per '1' digit, then pen += (pair + 2)
  // x scale; each colon (owner +0x430) at pen, then pen += (gap + 2) x scale. colour: the owner +0x3D0 >= 0.5 override (0x4C8688
  // A,R,G,B = 1,1,0,0, the freestyle limit's last seconds), else the descriptor's.
  raceClock(out, h, m, s, colour = null, k = 0x1a) {
    const c = this.clock || { gap: 3, pair: 37, sign: 11, one: 4.75 },
      d = this.desc(k),
      sc = d.scale,
      two = (n) => String(n).padStart(2, '0');
    const width = f(f(c.pair * 3) + f(f(f(c.gap + 2) + 2) * 2)),
      height = f(HUD_FONTS.HUDFONT.height * 1);
    let px = TrickHud.align(d.x, width, sc[0], d.align[0]);
    const py = TrickHud.align(d.y, height, sc[1], d.align[1]);
    const argb = (colour || d.argb).slice(),
      pair = f(f(c.pair + 2) * sc[0]),
      colon = f(f(c.gap + 2) * sc[0]),
      one = f(c.one * sc[0]);
    const piece = (text, x) =>
      out.push({
        kind: 'text',
        font: 'HUDFONT',
        text,
        x,
        y: py,
        scale: sc.slice(),
        argb: argb.slice(),
        shadow: this.font.HUDFONT.shadow.slice(),
        shadowArgb: [argb[0], 0, 0, 0],
        order: d.order
      });
    const digits = (text) => {
      let o = 0;
      if (text[0] === '1') o = one;
      if (text[1] === '1') o = f(o + one);
      piece(text, f(px + o));
      px = f(px + pair);
    };
    digits(two(h));
    for (const n of [m, s]) {
      piece(this.strings.colon ?? ':', px);
      px = f(px + colon);
      digits(two(n));
    }
  }
  // Freestyle standings rows (hudStandings; super pipe / big air / career slope style, owner +0x3CC bit 0x10). 0x1EB160 (in the
  // HUD update 0x1EA930, every frame) builds three rows at owner +0x3DC (stride 0x18: "%d" text, +0x10 rider id or -1, +0x14 x
  // offset): row k takes the next posted heat score (0x536640 in the ranking order 0x536708 of 238B70, best first; the player's
  // own slot is in that order too, unfilled slots read 0) unless the player's score (rider +0x198, plus 238590's heat-1 carry in
  // round 2) is at least it, then the player's row takes that place once. rows: [{rank, score, human, name}] in drawing order.
  standingsRows(posted, mine, name = '') {
    const rows = [];
    let j = 0,
      s = mine | 0;
    for (let k = 0; k < 3; k++) {
      const p = (typeof posted[j] === 'object' ? posted[j]?.score : posted[j]) ?? 0;
      if (s < p) {
        rows.push({ rank: k + 1, score: p | 0, human: false });
        j++;
      } else {
        rows.push({ rank: k + 1, score: s, human: true, name });
        s = -1;
      }
    }
    return rows;
  }
  // The name 14EE58 gives the human's row: the profile's first name (0x530990 + slot x 0x88), for the cheat characters 10..29 the
  // 8-byte table 0x43FA38 (these differ from the port's display names: 'Svelte Luther' -> 'Luther', 'NW Legend' -> 'Legend', ...).
  static CHEAT_NAMES = {
    10: 'Brodi',
    11: 'Eddie',
    12: 'JP',
    13: 'Luther',
    14: 'Marisol',
    15: 'Marty',
    16: 'Seeiah',
    17: 'Hiro',
    18: 'Jurgen',
    19: 'Luther',
    20: 'Stretch',
    21: 'Cudmore',
    22: 'Bun San',
    23: 'Churchl',
    24: 'Gutless',
    25: 'Snowbal',
    26: 'Legend',
    27: '? Rider',
    28: 'Canhuck',
    29: 'Myth'
  };
  riderName(rider) {
    return (rider?.kind === 'cheat' && TrickHud.CHEAT_NAMES[rider.character]) || rider?.name || '';
  }
  // The rows as the type-7 (score total) case draws them after the total, 0x1ED104..0x1ED4AC (flags 0x10; the case needs 0x402).
  // Descriptor 0x17 (20, 20, scale 1, white, order 10) read without alignment (1E91F8 position, 1E9290 scale, 1E91A8 colour into
  // the HUD font owner +0x42C, whose scale becomes its base scale x the descriptor's); every draw is 0x391CB0 with the font's
  // shadow (2, 2):
  //  - value column: each row's "%d" at x + sx x 66.997185 (gp-0x56D0) + (widest - own width) (0x391FB0 widths at the descriptor
  //    scale, shadow included: right-aligned on the widest row); rows every 21 x sy (font height x base scale x sy) from y.
  //  - posted row: the row number '1'..'3' (s2 + '1': the row index, not a stored rank) at (x, row y), then 'ST' / 'ND' / 'RD'
  //    (pointers 0x46EB40) at half the scale, at x + 1 + the digit's width (0x391FB0 at the full scale, shadow included) and
  //    row y + 1.5995806 (gp-0x56CC, not scaled): the superscript.
  //  - human row: the rider's name cut to three characters (strcpy, then byte 3 = 0) at (x, row y) over a flat box (0x1F1338:
  //    untextured quad, layer order - 1) at (x, row y) of (sx x 66.997185 + widest) x the row pitch, scale (1, 1), colour 0x4C8628
  //    (A,R,G,B 1,1,0,0: opaque red, vertex RGB x 255, alpha x 128). No PS2 capture shows this row (the recorded runs never
  //    passed a posted score); it is the code path.
  standings(out, rows, k = 0x17) {
    const d = this.desc(k),
      [sx, sy] = d.scale,
      argb = d.argb.slice(),
      shadow = this.font.HUDFONT.shadow.slice();
    const pitch = f(f(HUD_FONTS.HUDFONT.height * 1) * sy),
      col = f(d.x + f(sx * f(66.99718475341797))),
      half = [f(sx * 0.5), f(sy * 0.5)];
    const put = (text, x, y, scale) =>
      out.push({
        kind: 'text',
        font: 'HUDFONT',
        text,
        x,
        y,
        scale,
        argb: argb.slice(),
        shadow: shadow.slice(),
        shadowArgb: [argb[0], 0, 0, 0],
        order: d.order
      });
    const texts = rows.slice(0, 3).map((r) => String(r.score | 0)),
      widths = texts.map((t) => this.measure('HUDFONT', t, sx, sy).w);
    let widest = 0;
    for (const w of widths) if (widest < w) widest = w;
    let y = d.y;
    texts.forEach((t, i) => {
      put(t, f(col + f(widest - widths[i])), y, [sx, sy]);
      y = f(y + pitch);
    });
    y = d.y;
    rows.slice(0, 3).forEach((r, i) => {
      if (!r.human) {
        const digit = String.fromCharCode(0x31 + i);
        put(digit, d.x, y, [sx, sy]);
        put(['ST', 'ND', 'RD'][i], f(f(d.x + 1) + this.measure('HUDFONT', digit, sx, sy).w), f(y + f(1.5995806455612183)), half.slice());
      } else {
        put(String(r.name ?? '').slice(0, 3), d.x, y, [sx, sy]);
        out.push({ kind: 'rect', x: d.x, y, w: f(f(col - d.x) + widest), h: pitch, scale: [1, 1], argb: [1, 1, 0, 0], order: d.order - 1 });
      }
      y = f(y + pitch);
    });
  }
  // Finish / time-up banner 0x21F660 (the race HUD 0x1EC3F8 at 0x1ECB48 / 0x1ECB70, display mode +0x88 = 1 finish / 2 time up): the
  // 'fini' (owner +0x4B4) sprite at descriptor 0x42 (320, 190, 213 x 41, centred; 0x21F6B0) or the 'timeup' (+0x49C) sprite at
  // descriptor 0x1B (320, 180, 240 x 41, centred; 0x21F6C8, the time-up call passes t0 = 1), then for a finish the value
  // under it: races "%02d:%02d:%02d" (0x46ED68) of the finish time's hours / minutes / seconds, freestyle "%d" of the score,
  // centred (0x392908) at the sprite's middle x and bottom y, HUD font at the descriptor scale x 1.9075785 (gp-0x5340), colour
  // 0x4C8808 (white), shadow (2, 2). The sprite rectangles are OV_1-3 (1.5, 1.5, 131 x 24) and (1.5, 127.5, 169 x 23).
  finishBanner(out, timeUp, text = null, k = timeUp ? 0x1b : 0x42) {
    const d = this.desc(k),
      w = f(d.w * d.scale[0]),
      h = f(d.h * d.scale[1]);
    const x = TrickHud.align(d.x, w, 1, d.align[0]),
      y = TrickHud.align(d.y, h, 1, d.align[1]);
    const r = timeUp ? [1.5, 127.5, 169, 23] : [1.5, 1.5, 131, 24],
      P = 256;
    out.push({
      kind: 'sprite',
      sprite: { page: 'OV_1-3', uv: [r[1] / P, r[0] / P, (r[0] + r[2]) / P, (r[1] + r[3]) / P] },
      x,
      y,
      size: [w, h],
      scale: [1, 1],
      argb: d.argb.slice(),
      order: d.order
    });
    if (timeUp || text == null) return;
    const k2 = f(1.907578468322754),
      sc = [f(d.scale[0] * k2), f(d.scale[1] * k2)],
      tw = this.measure('HUDFONT', text, sc[0], sc[1]).w;
    out.push({
      kind: 'text',
      font: 'HUDFONT',
      text,
      x: f(f(x + f(w * 0.5)) - f(tw * 0.5)),
      y: f(y + h),
      scale: sc,
      argb: [1, 1, 1, 1],
      shadow: this.font.HUDFONT.shadow.slice(),
      shadowArgb: [1, 0, 0, 0],
      order: d.order
    });
  }
  // Speed widget 0x2200C0 (race HUD 0x1F03A0 at 0x1F0274; its layout = the HUD descriptors +0x384 = descriptor 25 at (20, 460),
  // left / bottom, scale 1): the label "MPH" / "KM/H" (profile 0x535610 bit 19) measured at the descriptor scale (391FB0) and
  // aligned (1F10F8, scale 1), drawn with the shadow; above it trunc(|v| x 0.036 (x 0.621 for mph) + 0.5) as "%d" at the
  // descriptor scale x 1.3623675 (fewer than 3 digits) or 1.121412, centred on the label (392908: x - width / 2), its top at the
  // label top - (21 x scale / 2 + 14.003287).
  speed(out, cmPerSec, kmh = false, k = 25) {
    const d = this.desc(k),
      label = kmh ? 'KM/H' : 'MPH',
      sc = d.scale.slice();
    let v = f(f(cmPerSec) * f(0.035999998450279236));
    if (!kmh) v = f(v * f(0.6209999918937683));
    const size = this.measure('HUDFONT', label, sc[0], sc[1]);
    const x = TrickHud.align(d.x, size.w, 1, d.align[0]),
      y = TrickHud.align(d.y, size.h, 1, d.align[1]);
    const shadow = this.font.HUDFONT.shadow.slice(),
      argb = d.argb.slice();
    out.push({
      kind: 'text',
      font: 'HUDFONT',
      text: label,
      x,
      y,
      scale: sc,
      argb: argb.slice(),
      shadow: shadow.slice(),
      shadowArgb: [argb[0], 0, 0, 0],
      order: d.order
    });
    const text = String(Math.max(0, Math.trunc(f(v + 0.5)))),
      ns = text.length < 3 ? f(1.3623675107955933) : f(1.1214120388031006);
    const nsc = [f(sc[0] * ns), f(sc[1] * ns)],
      w = this.measure('HUDFONT', text, nsc[0], nsc[1]).w;
    const top = f(y - f(f(f(f(HUD_FONTS.HUDFONT.height * 1) * nsc[1]) * 0.5) + f(14.003287315368652)));
    out.push({
      kind: 'text',
      font: 'HUDFONT',
      text,
      x: f(f(x + f(size.w * 0.5)) - f(w * 0.5)),
      y: top,
      scale: nsc,
      argb: argb.slice(),
      shadow: shadow.slice(),
      shadowArgb: [argb[0], 0, 0, 0],
      order: d.order
    });
  }
  // Cases 0x2E/0x2F (0x1EF924, flags 0x4000000 and 0x200: Conquer the Mountain cash for a point pickup / trick, posted by
  // 119EF8 kinds 1 / 0): the popup of 0x23 as money (0x198AF0) flying from 0x12 to the cash 0x38, shadow off, no stars.
  cashPopup(out, s, r) {
    const shadow = this.font.HUDFONT.shadow;
    this.font.HUDFONT.shadow = [0, 0];
    if (f(0.207151964) <= r) this.textLerp(out, 'HUDFONT', 0x12, 0x38, r, money(s.points));
    else {
      const sc = this.pulse(f(r * f(4.82737398)));
      this.textLerp(out, 'HUDFONT', 0x12, 0x38, r, money(s.points), { scale: [sc, sc], colour: this.colours.popup.slice() });
    }
    this.font.HUDFONT.shadow = shadow;
  }
  // Case 0x26 (0x1EE46C): combo payout flies from 0x02 to the total (colour override fading by 1-t; green
  // pulse below ratio 0.3), under an "NX Combo" label that fades out between 0.3 and 0.6.
  comboBonus(out, s, r, value = s.text) {
    const shadow = this.font.HUDFONT.shadow;
    this.font.HUDFONT.shadow = [0, 0];
    let colour = this.desc(0x2).argb.slice();
    let sc = 1;
    if (r < f(0.300000012)) {
      sc = this.pulse(f(r * f(3.33333325)));
      colour = this.colours.comboGreen.slice();
    }
    const box = this.textLerp(out, 'HUDFONT', 0x2, 0x11, r, value, { scale: [sc, sc], colour });
    if (r < f(0.600000024)) {
      if (f(0.300000012) < r) colour[0] = f(colour[0] * f(1 - f(f(r - f(0.300000012)) * f(3.33333325))));
      this.text(out, 'HUDFONT', 0x2, `${s.arg}X ${this.strings.combo}`, { scale: [sc, sc], colour, offset: [0, f(-box.h)] });
    }
    this.font.HUDFONT.shadow = shadow;
  }
  // Cases 0x25/0x27/0x28 (0x1EF084): lost pending (0x13) / combo (0x0D) / bonus (0x0E) points: the text
  // moves toward 0x14 by t^2 while colour and scale interpolate by t, and 0x1F14B0 scatters each digit.
  lost(out, s, r) {
    const shadow = this.font.HUDFONT.shadow;
    this.font.HUDFONT.shadow = [0, 0];
    const k = s.type === 0x27 ? 0xd : s.type === 0x28 ? 0xe : 0x13;
    const d = this.desc(k),
      e = this.desc(0x14);
    const w = this.measure('HUDFONT', s.text, 1, 1).w;
    const r2 = f(r * r);
    const a = [TrickHud.align(d.x, w, 1, d.align[0]), TrickHud.align(d.y, 0, 1, d.align[1])],
      b = [TrickHud.align(e.x, w, 1, e.align[0]), TrickHud.align(e.y, 0, 1, e.align[1])];
    const origin = [f(f(f(b[0] - a[0]) * r2) + a[0]), f(f(f(b[1] - a[1]) * r2) + a[1])];
    const argb = [0, 1, 2, 3].map((i) => f(f(f(e.argb[i] - d.argb[i]) * r) + d.argb[i]));
    const scale = [f(f(f(e.scale[0] - d.scale[0]) * r) + d.scale[0]), f(f(f(e.scale[1] - d.scale[1]) * r) + d.scale[1])];
    const g = this.glyphs.HUDFONT;
    const chars = [...s.text];
    let x = origin[0];
    chars.forEach((ch, i) => {
      let n = i - (chars.length >> 1),
        neg = false;
      if (n === 0) n = 1;
      else if (n < 0) {
        neg = true;
        n = -n;
      }
      const code = ch.charCodeAt(0) & 0xff;
      const m = 2 * n * ((code & 0x7f) >> 1);
      const up = (m * 4 + m) & 0x7f,
        side = (n * (code & 0xf) * 4) & 0x7f;
      const fx = neg ? -side : side,
        fy = -up;
      out.push({
        kind: 'text',
        font: 'HUDFONT',
        text: ch,
        x: f(x + f(r * fx)),
        y: f(origin[1] + f(r * fy)),
        scale,
        argb: argb.slice(),
        shadow: [0, 0],
        shadowArgb: null,
        order: d.order
      });
      const q = g[ch];
      if (q) x = f(x + f((q.dx + q.w) * scale[0]));
    });
    this.font.HUDFONT.shadow = shadow;
  }
}

// Canvas renderer for the draw list (UI canvas 640x448; PS2 HUD space is 640x480). Text follows 0x391CB0:
// the shadow pass (offset, shadow ARGB) then the glyph pass, each glyph at pen + (dx, dy) * scale. Colours
// are the GS vertex ARGB (1.0 = 0x80) modulating the white font/atlas texels. Sprites follow 0x1F1190
// (top-left position, size x scale, UV rows v0,u0,u1,v1 as the owner sprite records store them).
export class TrickHudRenderer {
  constructor(images, glyphs) { this.images = images; this.glyphs = glyphs; this.tints = new Map(); }
  // text: a font draw (0x391CB0 -> renderer 0x378808), whose vertex RGB is trunc(colour x 204.0) (0x434C0000) over font texels of
  // unity 0x80, so white text lands at 204 (PS2 HUD text peaks at 204,204,204; red OPPONENT 0x4C88C8 at 158,12,8 = 0.8 x its
  // colour); alpha stays trunc(a x 128) (docs/visual-parity.md). Sprites (0x1F1190) keep the 128 unity.
  tinted(name, argb, text = false) {
    const q = text ? hudTextByte : (v) => Math.max(0, Math.min(255, Math.round(Math.trunc(v * 128) * 255 / 128)));
    const key = `${name}:${q(argb[1])},${q(argb[2])},${q(argb[3])}`; let c = this.tints.get(key);
    if (!c) { const im = this.images[name]; if (!im) return null; c = document.createElement('canvas'); c.width = im.width; c.height = im.height; const g = c.getContext('2d', SPRITE_2D);
      g.drawImage(im, 0, 0); g.globalCompositeOperation = 'multiply'; g.fillStyle = `rgb(${q(argb[1])},${q(argb[2])},${q(argb[3])})`; g.fillRect(0, 0, c.width, c.height);
      g.globalCompositeOperation = 'destination-in'; g.drawImage(im, 0, 0); this.tints.set(key, c); }
    return c;
  }
  text(ctx, d, dx, dy, argb) {
    const im = this.tinted(d.font + '-0', argb, true); const g = this.glyphs[d.font]; if (!im || !g) return;
    ctx.globalAlpha = Math.max(0, Math.min(1, Math.trunc(argb[0] * 128) / 128)); let x = d.x + dx;
    for (const ch of d.text) { const q = g[ch]; if (!q) continue; const gy = q.dy > 127 ? q.dy - 256 : q.dy;
      ctx.drawImage(im, q.x, q.y, q.w, q.h, x + q.dx * d.scale[0], d.y + dy + gy * d.scale[1], q.w * d.scale[0], q.h * d.scale[1]); x += q.advance * d.scale[0]; }
  }
  // FEFONT text for drawKeyCap (the ui.text(ctx, value, x, y, size, colour, font, align) shape) on this renderer's atlas.
  keyText() {
    return this.keyUi ??= { fonts: this.glyphs, text: (ctx, value, x, y, size, colour, font, align) => {
      const g = this.glyphs[font]; if (!g) return; const sc = size / 22, rgb = [1, 3, 5].map((i) => parseInt(colour.slice(i, i + 2), 16) / 255);
      const w = [...value].reduce((a, ch) => a + (g[ch]?.advance || 10) * sc, 0), a = ctx.globalAlpha;
      this.text(ctx, { font, text: value, x: x - (align === 'center' ? w / 2 : align === 'right' ? w : 0), y, scale: [sc, sc] }, 0, 0, [a, ...rgb]); ctx.globalAlpha = a;
    } };
  }
  render(ctx, list) {
    ctx.save(); ctx.scale(1, 448 / 480);
    // Within a layer the font glyphs land above the sprites (PS2 frame score-air-tricks tick 482: the popup
    // digits cover its grade star although 0x1F1190 is submitted after 0x391CB0).
    const rank = (d) => (d.order ?? 0) * 2 + (d.kind === 'text' ? 1 : 0);
    for (const d of [...list].sort((a, b) => rank(a) - rank(b))) {
      if (d.kind === 'text') {
        if (d.shadowArgb && (d.shadow[0] || d.shadow[1]) && d.shadowArgb[0] > 0) this.text(ctx, d, d.shadow[0], d.shadow[1], [d.shadowArgb[0], 0, 0, 0]);
        this.text(ctx, d, 0, 0, d.argb);
      } else if (d.kind === 'sprite' && d.key) {       // keyboard: the key cap in the icon's place (web/input-glyphs.js)
        ctx.save(); ctx.globalAlpha = Math.max(0, Math.min(1, Math.trunc(d.argb[0] * 128) / 128));
        drawKeyCap(ctx, this.keyText(), d.key, d.x, d.y, d.size[0] * d.scale[0], d.size[1] * d.scale[1], LUI_STRETCH); ctx.restore();
      } else if (d.kind === 'sprite' && d.sprite?.page) {
        const im = this.tinted(d.sprite.page, d.argb); if (!im) continue; const [v0, u0, u1, v1] = d.sprite.uv;
        const size = d.size || [d.w, d.h]; const w = size[0] * d.scale[0], h = size[1] * d.scale[1];
        ctx.globalAlpha = Math.max(0, Math.min(1, Math.trunc(d.argb[0] * 128) / 128));
        ctx.save();
        ctx.translate(d.x + (u1 < u0 ? w : 0), d.y + (v1 < v0 ? h : 0));
        ctx.scale(u1 < u0 ? -1 : 1, v1 < v0 ? -1 : 1);
        ctx.drawImage(im, Math.min(u0, u1) * im.width, Math.min(v0, v1) * im.height, Math.abs(u1 - u0) * im.width, Math.abs(v1 - v0) * im.height, 0, 0, w, h); ctx.restore();
      } else if (d.kind === 'rect') {                  // 0x1F1338: untextured quad, vertex RGB trunc(c x 255), alpha trunc(a x 128) (unity 0x80)
        const q = (v) => Math.max(0, Math.min(255, Math.trunc(Math.fround(v * 255))));
        ctx.globalAlpha = Math.max(0, Math.min(1, Math.trunc(d.argb[0] * 128) / 128)); ctx.fillStyle = `rgb(${q(d.argb[1])},${q(d.argb[2])},${q(d.argb[3])})`;
        ctx.fillRect(d.x, d.y, d.w * (d.scale?.[0] ?? 1), d.h * (d.scale?.[1] ?? 1));
      } else if (d.kind === 'quad') {                  // 0x1F1338: untextured quad, vertex RGB trunc(c x 255), alpha trunc(a x 128)
        const q = (v) => Math.max(0, Math.min(255, Math.trunc(v * 255)));
        ctx.globalAlpha = Math.max(0, Math.min(1, Math.trunc(d.argb[0] * 128) / 128)); ctx.fillStyle = `rgb(${q(d.argb[1])},${q(d.argb[2])},${q(d.argb[3])})`;
        ctx.fillRect(d.x, d.y, d.size[0], d.size[1]);
      } else if (d.kind === 'recover') {
        // Crash recover bar (0x21D9A0 after the slot loop; not yet a source port): fill of descriptor 0x36.
        ctx.globalAlpha = 1; ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 2; ctx.strokeRect(243, 442, 154, 12);
        ctx.fillStyle = '#ff2020'; ctx.fillRect(245, 444, Math.max(0, Math.min(1, d.fill)) * 150, 8);
      }
    }
    ctx.restore();
  }
}
