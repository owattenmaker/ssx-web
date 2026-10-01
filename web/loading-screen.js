// Original SSX 3 event loading screen (GL.LUI "110ctrl_load" = "Basic Controls"), played from the disc's own
// layout: tools/export_loading_screen.py writes web/public/assets/LOADING/loading.json (elements, frame-0
// properties, animations, timeline events, the GL_1 atlas UVs and the load hints). See docs/loading-screen.md.
//
// The original picks the load screen in cGameLoadState init 0x232E20: Conquer the Mountain loads show this
// controller screen, Single Event loads the course card (99QPEvent). The browser shows the controller screen for
// every event load (the controls are the point), adds the port's extra DualShock labels (Hand plant on Circle, Board
// press, Pause: the screen's own "Pro" label records) or a keyboard panel for the current Simple/Classic layout, and one
// of the fifteen original load hints (kT_FEHINT*, rotated like 0x245950). The original cannot be skipped; it
// stays until the load completes, so the browser keeps a minimum display time and never skips early.
import { downloadProgress } from './downloads.js';
import { inputDevice, drawKeyCap, LUI_STRETCH } from './input-glyphs.js';
import { SPRITE_2D } from './sprite-canvas.js'; // offscreen sprite canvas kind (software in Firefox, docs/firefox-load.md)
const ROOT = '/assets/LOADING/';
export const LOADING_FPS = 60;
export const HINT_COUNT = 15;
export const HINT_SKIP = 12;
const TEXT_COLOR = [37, 7, 5];
// LUI text scale 100% draws FEFONT at 0.79 of its native size in PS2 pixels (measured on the ARMSX2 frames).
const FONT_K = 0.79;
const PS2_X = 640 / 512, PS2_Y = 480 / 448;       // one PS2 pixel in the 640x480 LUI frame
const FADE_FRAMES = 20;                            // black -> screen and screen -> black (ARMSX2: ~20 frames each)
const DONE_FRAMES = 12;                            // 100% stays up this long before the fade out
const HINT_KEY = 'ssx3.loadingHint';

// 0x245950: n = counter + 1 (hint 12, Bragging Rights, is skipped), then counter = (counter + 1) % 15.
export function nextHint(counter) {
  let c = Number.isInteger(counter) && counter >= 0 && counter < HINT_COUNT ? counter : 0;
  let hint = c + 1;
  if (hint === HINT_SKIP) { c += 1; hint = c + 1; }
  return { hint, counter: (c + 1) % HINT_COUNT };
}

// LUI track: [[value, frames to next], ..., [last, 0]], linear between keys, held after the last.
export function trackValue(keys, frame) {
  if (!keys.length) return 0;
  let t = Math.max(0, frame);
  for (let i = 0; i < keys.length - 1; i++) {
    const [v, d] = keys[i];
    if (t < d) return v + (keys[i + 1][0] - v) * (t / d);
    t -= d;
  }
  return keys[keys.length - 1][0];
}

// Animation properties at a frame since it started. Mode bit 8 loops over the animation length; otherwise it holds.
export function animationProps(anim, frame, mode) {
  const length = Math.max(1, anim.frames);
  const t = mode & 8 ? frame % length : Math.min(frame, length);
  const out = {};
  for (const [prop, keys] of Object.entries(anim.tracks)) out[prop] = trackValue(keys, t);
  return out;
}

// Displayed percentage: the original Snow Jam load curve (frames -> percent, 0..98) stretched over the minimum
// display time; it holds at 98% until the work is done (the original also sits at 98% while the event starts).
export function loadingPercent(curve, frame, minFrames, originalFrames, done) {
  const hold = curve.filter(([, p]) => p <= 98);
  const lastFrame = hold[hold.length - 1][0];
  const f = frame * lastFrame / Math.max(1, minFrames * lastFrame / originalFrames);
  let p = hold[hold.length - 1][1];
  for (let i = 0; i < hold.length - 1; i++) {
    const [f0, p0] = hold[i], [f1, p1] = hold[i + 1];
    if (f < f1) { p = p0 + (p1 - p0) * (f - f0) / (f1 - f0); break; }
  }
  return done ? 100 : Math.max(0, Math.min(98, Math.floor(p)));
}

// Keyboard rows (web/pad-input.js KEYBOARD_BUTTONS and the Simple/Classic routing), labelled with the screen's strings.
export function keyboardRows(mode, s) {
  const simple = mode !== 'Classic';
  const rows = [
    { keys: ['W', 'A', 'S', 'D'], alt: ['←', '↑', '↓', '→'], label: simple ? s.turn_spin_flip : s.turn, note: simple ? 'Steer; spin/flip in the air' : 'Steer' },
  ];
  if (!simple) rows.push({ keys: ['I', 'J', 'K', 'L'], label: s.turn_spin_flip, note: 'Spin/flip in the air' });
  rows.push(
    { keys: ['Space'], label: s.jump, note: 'Hold to crouch, release to jump' },
    { keys: ['Shift'], label: s.boost_tweak, note: 'Boost; tweak a grab in the air' },
    { keys: ['Q', 'Z', 'E', 'X'], label: s.grab_board, note: 'In the air (L1 L2 R1 R2)' },
    { keys: ['C'], label: s.hand_plant, note: 'Near anything you can grind' },
    { keys: ['T', 'F', 'G', 'H'], label: s.board_press, note: 'V: ollie out of a press' },
    { keys: ['Backspace'], label: s.reset, note: 'Back onto the course' },
    { keys: ['Esc'], label: s.pause },
  );
  return rows;
}

// Last used input device (web/input-glyphs.js): the keyboard on a key press, a gamepad once a button / stick moves,
// touch on the deck; the pad picture shows for gamepad and touch (the deck is a DualShock).
function pollGamepad() { return inputDevice() === 'keyboard' ? 'keyboard' : 'gamepad'; }

function loadImage(src) { const im = new Image(); im.src = src; return im.decode().then(() => im); }
function storage() { try { return sessionStorage; } catch { return null; } }

export class LoadingScreen {
  constructor(ui) { this.ui = ui; this.data = null; this.images = {}; this.session = null; this.tints = new Map(); this.lastDraw = 0; this.forceDevice = null; }
  get available() { return !!this.data; }
  get active() { return !!this.session; }

  async load() {
    try {
      const data = await (await fetch(ROOT + 'loading.json')).json();
      await Promise.all(Object.keys(data.pages).map(async (name) => { this.images[name] = await loadImage(ROOT + name + '.png'); }));
      this.data = data;
      this.byName = new Map(data.elements.map((e, i) => [e.name, { ...e, index: i }]));
      this.byText = new Map(data.elements.filter((e) => e.kind === 'text').map((e) => [e.text, e]));
    } catch (error) { console.warn('Loading screen assets unavailable (tools/export_loading_screen.py)', error); this.data = null; }
  }

  params() {
    const q = typeof location !== 'undefined' ? new URL(location.href).searchParams : new URLSearchParams();
    const ms = Number(q.get('loadingMs'));
    return { disabled: q.get('loading') === '0', minMs: Number.isFinite(ms) && ms >= 0 && q.has('loadingMs') ? ms : 7000 };
  }

  // Show the screen now (e.g. while main.js init loads a course); run() attaches the continuation.
  open(work = []) {
    if (!this.data || this.params().disabled) return false;
    if (!this.session) {
      const st = storage(), saved = Number(st?.getItem(HINT_KEY));
      const { hint, counter } = nextHint(Number.isInteger(saved) ? saved : 0);
      try { st?.setItem(HINT_KEY, String(counter)); } catch {}
      // World mode (`this.world` = a draw function, set by web/ctm-event.js for the in-world Conquer the Mountain event
      // load): the PS2 has no load screen there (ps2b/intro: the fly-over with "Loading..."), so the session only
      // covers the in-page course switch with the world's caption, without the 7 s minimum or the fades.
      this.session = {
        start: performance.now(),
        pending: 0,
        next: null,
        doneAt: 0,
        hint: this.data.hints.find((h) => h.index === hint),
        minMs: this.world ? 0 : this.params().minMs,
        world: this.world || null
      };
      // The race canvas is hidden under the load screen (style.css), except in world mode where the world shows: the warm-up
      // frames drawn there never reach the compositor (Firefox held its frames behind every pipeline build, docs/firefox-load.md).
      if (this.ui.stage) this.ui.stage.dataset.loadWorld = this.session.world ? '1' : '';
      this.ui.set('loading');
      // Own frame loop while main.js is still loading (its render loop starts at the end of init); a screen change
      // away from 'loading' (e.g. a load error) abandons the session.
      const session = this.session;
      const tick = () => {
        if (this.session !== session) return;
        if (this.ui.screen !== 'loading') {
          this.session = null;
          return;
        }
        if (performance.now() - this.lastDraw > 40) this.ui.draw(this.ui.lastState || {});
        requestAnimationFrame(tick);
      };
      requestAnimationFrame(tick);
    }
    for (const w of work) this.wait(w);
    return true;
  }
  // Drop the current session without calling its continuation (main.js: a course change during an event load, or the
  // load screen closing into a menu / the online lobby after an in-page course load).
  cancel() { this.session = null; }   // (a pending world mode carries into the next session: main.js switchCourse cancels, then opens)
  wait(promise) {
    const s = this.session; if (!s || !promise?.then) return;
    s.pending++; promise.catch(() => {}).finally(() => { s.pending--; });
  }
  // Load-event entry point: show the screen (if not already up) and call next() when it has finished.
  run(next, work = []) {
    if (!this.open(work)) { next(); return; }
    this.session.next = next;
  }

  frame(now = performance.now()) { return this.session ? (now - this.session.start) * LOADING_FPS / 1000 : 0; }
  update(now) {
    const s = this.session; if (!s) return null;
    const frame = this.frame(now), minFrames = s.minMs * LOADING_FPS / 1000;
    if (!s.doneAt && s.next && s.pending === 0 && frame >= minFrames) s.doneAt = frame;
    const t = this.data.timing;
    // The original curve paces the number; while game data is still downloading it cannot run ahead of the transfer
    // (up to 90% for the download, the rest is the warm-up the curve already covers), and it never goes backwards.
    let percent = loadingPercent(t.percent_curve, frame, minFrames, t.original_frames, !!s.doneAt);
    const dl = downloadProgress();
    if (!s.doneAt && dl.active) percent = Math.min(percent, Math.floor(dl.fraction * 90));
    // the page's first course still loading behind the menus (pv lazyCourse, web/main.js backgroundCourse): nor ahead of its work
    const work = s.doneAt ? null : this.workFraction?.();
    if (work != null) percent = Math.min(percent, Math.floor(work * 90));
    percent = s.shownPercent = Math.max(s.shownPercent ?? 0, percent);
    let black = Math.max(0, 1 - frame / FADE_FRAMES);
    if (s.doneAt && s.world) { const next = s.next; this.session = null; this.world = null; next(); return null; }
    if (s.doneAt) {
      const out = (frame - s.doneAt - DONE_FRAMES) / FADE_FRAMES;
      black = Math.max(black, Math.min(1, Math.max(0, out)));
      if (out >= 1) { const next = s.next; this.session = null; next(); return null; }
    }
    return { frame, percent, black };
  }

  // ---- drawing (640x448 canvases, the LUI frame is 640x480) ----
  draw(c, b) {
    this.lastDraw = performance.now();
    const world = this.session?.world;
    const st = this.update(this.lastDraw);
    if (world) { b.save(); b.fillStyle = '#000'; b.fillRect(0, 0, 640, 448); b.restore(); world(c, b); return; }
    if (!st || !this.data) return;
    b.save(); b.fillStyle = '#000'; b.fillRect(0, 0, 640, 448); b.restore();
    c.save(); c.scale(1, 448 / 480);
    const pad = (this.forceDevice || pollGamepad()) === 'gamepad';
    this.drawScreen(c, st, pad);
    if (!pad) this.drawKeyboard(c);
    this.drawHint(c, pad);
    if (st.black > 0) { c.globalAlpha = st.black; c.fillStyle = '#000'; c.fillRect(0, 0, 640, 480); }
    c.restore();
  }

  // Element properties at a frame: frame-0 properties, later property events, and the element's current animation.
  elementProps(e, frame) {
    let props = e.props || null, anim = e.anim ? { ...e.anim, start: 0 } : null;
    for (const ev of this.data.events) {
      if (ev.frame > frame || ev.element !== e.name) continue;
      if (ev.props) { props = ev.props; anim = null; } else anim = { hash: ev.anim, mode: ev.mode, start: ev.frame };
    }
    if (anim && this.data.animations[anim.hash]) props = { ...(props || {}), ...animationProps(this.data.animations[anim.hash], frame - anim.start, anim.mode) };
    return props;
  }

  visibleFor(e, pad) {
    // Keyboard: the DualShock picture and its labels give way to the keyboard panel.
    const hidden = new Set(['0917a214']);                                     // "Pro" controller-config title
    const pro = this.byName.get('05f515af');                                  // "Pro" label group: keep the port's extras
    // Kept from the Pro set (with their leader lines): the Circle label (Pro 'No function', relabelled Hand plant:
    // Default INPUT.MAP Circle = handplant), Board press (right stick) and Pause (Start).
    const keep = new Set(['0514b410', '07730486', '08bc5120', '07730481', '08bc5121', '07730485']);
    for (let n = e; n; n = n.parent ? this.byName.get(n.parent) : null) {
      if (hidden.has(n.name)) return false;
      if (!pad && ['0a6b545c', '02c8befe', '08976274', '05f515af', '0e303d74'].includes(n.name)) return false;
      if (n === pro && !keep.has(e.name)) return false;
    }
    return true;
  }

  drawScreen(c, st, pad) {
    const items = [];
    const cache = new Map();
    const abs = (e) => {
      if (cache.has(e.name)) return cache.get(e.name);
      const p = this.elementProps(e, st.frame) || {};
      const parent = e.parent ? abs(this.byName.get(e.parent)) : { x: 0, y: 0, a: 1 };
      const r = { x: parent.x + (p[0] || 0), y: parent.y + (p[1] || 0), a: parent.a * ((p[13] ?? 255) / 255), p };
      cache.set(e.name, r); return r;
    };
    for (const e of this.byName.values()) {
      if (e.kind === 'group' || !this.visibleFor(e, pad)) continue;
      const r = abs(e);
      if (r.a <= 0) continue;
      items.push({ e, r });
    }
    items.sort((a, b) => a.e.layer - b.e.layer || a.e.index - b.e.index);
    for (const { e, r } of items) {
      const p = r.p;
      if (e.kind === 'sprite') this.drawSprite(c, e.sprite, r, p);
      else if (e.kind === 'shape') this.drawShape(c, r, p);
      else if (e.kind === 'text') {
        const text = e.name === '00000025' ? `${st.percent}%` : e.name === '0514b410' ? this.data.strings.hand_plant : e.text;
        if (text) this.drawText(c, text, r.x, r.y, p, r.a);
      }
    }
  }

  anchor(p, w, h) {
    const f = p[12] ?? 9;
    const ax = f & 16 ? 0.5 : f & 32 ? 1 : 0, ay = f & 2 ? 0.5 : f & 4 ? 1 : 0;
    return [-w * ax, -h * ay];
  }

  tinted(sprite, rgb) {
    const key = sprite.hash + rgb.join(',');
    let im = this.tints.get(key);
    if (!im) {
      im = document.createElement('canvas'); im.width = Math.ceil(sprite.sw); im.height = Math.ceil(sprite.sh);
      const t = im.getContext('2d', SPRITE_2D);
      t.drawImage(this.images[sprite.page], sprite.sx, sprite.sy, sprite.sw, sprite.sh, 0, 0, sprite.sw, sprite.sh);
      t.globalCompositeOperation = 'multiply'; t.fillStyle = `rgb(${rgb})`; t.fillRect(0, 0, im.width, im.height);
      t.globalCompositeOperation = 'destination-in'; t.drawImage(this.images[sprite.page], sprite.sx, sprite.sy, sprite.sw, sprite.sh, 0, 0, sprite.sw, sprite.sh);
      this.tints.set(key, im);
    }
    return im;
  }

  drawSprite(c, sprite, r, p) {
    if (!sprite) return;
    const w = (p[6] || sprite.sw) * (p[9] ?? 100) / 100, h = (p[7] || sprite.sh) * (p[10] ?? 100) / 100;
    const rgb = [p[14] ?? 255, p[15] ?? 255, p[16] ?? 255].map(Math.round);
    const [ox, oy] = this.anchor(p, w, h);
    c.save(); c.globalAlpha = Math.min(1, r.a); c.translate(r.x, r.y);
    if (p[5]) c.rotate(p[5] * Math.PI / 180);
    if (rgb.every((v) => v >= 255)) c.drawImage(this.images[sprite.page], sprite.sx, sprite.sy, sprite.sw, sprite.sh, ox, oy, w, h);
    else c.drawImage(this.tinted(sprite, rgb), 0, 0, sprite.sw, sprite.sh, ox, oy, w, h);
    c.restore();
  }

  // Four gouraud vertices; the screen only uses flat quads and two-colour (vertical or horizontal) ramps.
  drawShape(c, r, p) {
    const v = [0, 1, 2, 3].map((k) => ({ x: p[21 + 9 * k] || 0, y: p[22 + 9 * k] || 0, a: (p[26 + 9 * k] ?? 255) / 255, rgb: [p[27 + 9 * k] ?? 255, p[28 + 9 * k] ?? 255, p[29 + 9 * k] ?? 255] }));
    const css = (q) => `rgba(${q.rgb.join(',')},${q.a * r.a})`;
    const same = (a, b) => a.a === b.a && a.rgb.every((x, i) => x === b.rgb[i]);
    let fill = css(v[0]);
    if (!same(v[0], v[2])) {
      const [a, b] = same(v[0], v[1]) ? [v[0], v[3]] : [v[0], v[1]];
      const g = c.createLinearGradient(r.x + a.x, r.y + a.y, r.x + b.x, r.y + b.y);
      g.addColorStop(0, css(a)); g.addColorStop(1, css(b)); fill = g;
    }
    c.save(); c.fillStyle = fill; c.beginPath(); v.forEach((q, i) => c[i ? 'lineTo' : 'moveTo'](r.x + q.x, r.y + q.y)); c.closePath(); c.fill(); c.restore();
  }

  // LUI text: FEFONT at scale% * FONT_K PS2 pixels; anchor bits as for sprites (right = x is the right edge).
  drawText(c, text, x, y, p, alpha = 1, color = null) {
    const s = FONT_K * (p[9] ?? 100) / 100;
    const f = p[12] ?? 9, align = f & 32 ? 'right' : f & 16 ? 'center' : 'left';
    const rgb = color || [p[14] ?? TEXT_COLOR[0], p[15] ?? TEXT_COLOR[1], p[16] ?? TEXT_COLOR[2]];
    const hex = '#' + rgb.map((v) => Math.round(v).toString(16).padStart(2, '0')).join('');
    c.save(); c.globalAlpha = Math.min(1, alpha); c.translate(x, y); c.scale(PS2_X / PS2_Y, 1);
    this.ui.text(c, text, 0, 0, 22 * s * PS2_Y, hex, 'FEFONT', align);
    c.restore();
  }
  textWidth(text, scale) {
    const g = this.ui.fonts.FEFONT || {};
    return [...text].reduce((w, ch) => w + (g[ch]?.advance || 10), 0) * FONT_K * scale / 100 * PS2_X;
  }

  // Keyboard panel in the DualShock's place, in the screen's label style (FEFONT 50%, white leader lines):
  // keycaps right-aligned on a column, then the original control name and a short note.
  drawKeyboard(c) {
    const rows = keyboardRows(this.ui.keyboardMode, this.data.strings);
    this.drawText(c, `Keyboard - ${this.ui.keyboardMode === 'Classic' ? 'Classic' : 'Simple'}`, 55, 88, { 9: 60, 12: 9 });
    const right = 262, step = rows.length > 8 ? 19 : 21, top = 122;
    rows.forEach((row, i) => {
      const y = top + i * step, caps = [...row.keys, ...(row.alt || [])];
      const widths = caps.map((k) => this.keycapWidth(k)), gap = row.alt ? 8 : 0;
      let x = right - widths.reduce((a, w) => a + w + 3, 0) - gap;
      caps.forEach((k, j) => { if (j === row.keys.length) x += gap; this.keycap(c, k, x, y, widths[j]); x += widths[j] + 3; });
      c.fillStyle = '#ffffff'; c.fillRect(right + 3, y + 7, 14, 2);
      this.drawText(c, row.label, right + 22, y, { 9: 50, 12: 9 });
      if (row.note) this.drawText(c, row.note, right + 32 + this.textWidth(row.label, 50), y + 3, { 9: 38, 12: 9 }, 1, [44, 66, 88]);
    });
  }
  keycapWidth(label) { return label.length === 1 ? 17 : this.textWidth(label, 36) + 10; }
  // The key cap (16 high here) is shared with every keyboard control hint (web/input-glyphs.js drawKeyCap).
  keycap(c, label, x, y, w) { drawKeyCap(c, this.ui, label, x, y, w, 16, LUI_STRETCH); }

  // One original load hint (kT_FEHINTTitle/DES), below the controls and above the forest.
  drawHint(c, pad) {
    const h = this.session?.hint; if (!h) return;
    const y = 304, width = 500;
    c.fillStyle = '#e06a1c'; c.fillRect(56, y + 5, 4, 4);
    this.drawText(c, `${this.data.strings.hints_and_tips}: ${h.title}`, 66, y, { 9: 50, 12: 9 });
    const words = h.body.replace(/\\\\/g, ' ').split(/\s+/); let line = '', n = 0;
    const flush = () => { if (n < 3) this.drawText(c, line, 66, y + 16 + n * 13, { 9: 42, 12: 9 }, 1, [30, 44, 58]); n++; line = ''; };
    for (const w of words) { const next = line ? line + ' ' + w : w; if (line && this.textWidth(next, 42) > width) flush(); line = line ? line + ' ' + w : w; }
    if (line) flush();
  }
}
