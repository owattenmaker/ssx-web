// Phone prompts as in-game message boxes (docs/mobile.md "Prompts"): the Add to Home Screen / full screen hint and the
// turn-your-device card. Both are the original FE Yes/No popup box (FE.LUI 'popup': the blue panel with the burnt
// orange frame and its dark shadow, black FEFONT message, white focused option with the Cross icon, as drawn by
// web/fe-screens.js drawPrompt), laid out for the message instead of Yes/No, with one option ("OK" / "Continue").
//
// They are drawn on their own canvas above the page (the game frame can be tiny or not laid out yet when the device
// is turned the wrong way) and load what they need themselves (FEFONT, the Cross icon, the popup screen), so they work
// before the 3D, the UI or the first course have loaded. Cross (✕ on the deck, Space/Enter), Triangle or a tap closes
// them. Nothing here reaches the game: while a prompt is up it swallows the menu keys.
import { LuiScreen } from './lui-player.js';
import { drawGlyphAsKey, LUI_STRETCH } from './input-glyphs.js';
import { SPRITE_2D } from './sprite-canvas.js'; // offscreen sprite canvas kind (software in Firefox, docs/firefox-load.md)

const UI = '/assets/UI/';
const BOX = ['059e7b67', '0ecd9825', '09797985', '02bb75c5', '065adea5'];   // popup frame shapes (group 00376566)
const CROSS = { page: 'FE_1-14', sx: 56.5, sy: 123.5, sw: 20, sh: 19 };      // the Cross icon of the Yes/No menu (00077698)
const FE_BLUE = '#75a9cb';                                                  // FE background (web/fe-screens.js draw)
const TEXT_SCALE = 88;       // message FEFONT % (the popup's own 60% is ~8 px on a phone; this keeps the box's look, readable)
const PITCH = 23;            // message line pitch (480-line px) at that size
const WRAP = 420;            // message width (LUI px)

// FEFONT text as web/ui.js OriginalUI.text (glyph atlas tinted per colour), for LuiScreen outside the UI.
function textDrawer(fonts, images) {
  const tints = new Map();
  return {
    fonts,
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
}

// Word wrap in FEFONT units (as LuiScreen.text: advance * 0.79 * scale%, width in 512-px PS2 units).
export function wrapLines(text, glyphs, scalePct = TEXT_SCALE, width = WRAP) {
  const s = 0.79 * scalePct / 100, max = width / (640 / 512), adv = (str) => [...str].reduce((w, ch) => w + (glyphs?.[ch]?.advance || 10), 0) * s;
  const lines = []; let line = '';
  for (const word of String(text).split(/\s+/).filter(Boolean)) { const next = line ? line + ' ' + word : word; if (line && adv(next) > max) { lines.push(line); line = word; } else line = next; }
  if (line) lines.push(line);
  return lines;
}

// The messages. kind 'home': iPhone/iPad Safari (no element full screen) or a browser without the Fullscreen API.
export function promptMessage(kind, { ios = false, want = 'portrait' } = {}) {
  if (kind === 'home') return ios
    ? { text: 'For full screen, tap Share, then Add to Home Screen, and start SSX 3 from the new icon.', option: 'OK' }
    : { text: 'Full screen is not available in this browser. Add SSX 3 to your home screen to play without the browser bars.', option: 'OK' };
  return { text: want === 'landscape' ? 'Turn your device sideways to play. The picture and the controller need more room.' : 'Turn your device upright to play. The picture and the controller need more room.', option: 'Continue', want };
}

export function createPhonePrompts({ ui = null } = {}) {
  const doc = document;
  const canvas = doc.createElement('canvas'); canvas.id = 'tc-prompt'; canvas.hidden = true; canvas.setAttribute('role', 'alertdialog');
  doc.body.appendChild(canvas);
  const ctx = canvas.getContext('2d', { willReadFrequently: true }); // software canvas: no diagonal triangle seams in Safari (web/ui.js UI_CANVAS)
  let assets = null, loading = null, current = null, raf = 0;

  // What the prompt needs, from the running UI when it has loaded, else fetched here (shared downloads, web/downloads.js).
  function load() {
    if (assets) return Promise.resolve(assets);
    const fromUi = () => { const fe = ui?.feScreens, im = fe?.images || ui?.characterSelect?.images;
      const snow = fe?.data?.screens?.bg_snow_loop;
      return fe?.popupLui && snow && ui?.fonts?.FEFONT && ui?.images?.['FEFONT-0'] && im?.['FE_1-14'] && im['FE_1-11'] ? { popup: fe.popupLui.screen, snow, fonts: { FEFONT: ui.fonts.FEFONT }, images: { 'FEFONT-0': ui.images['FEFONT-0'], 'FE_1-14': im['FE_1-14'], 'FE_1-11': im['FE_1-11'] } } : null; };
    const ready = fromUi(); if (ready) return Promise.resolve(finish(ready));
    const image = (name) => new Promise((resolve, reject) => { const im = new Image(); im.onload = () => resolve(im); im.onerror = reject; im.src = UI + name + '.png'; });
    loading ??= Promise.all([fetch(UI + 'character-select.json').then((r) => r.json()), fetch(UI + 'FEFONT-glyphs.json').then((r) => r.json()), image('FEFONT-0'), image('FE_1-14'), image('FE_1-11')])
      .then(([data, glyphs, font, fe14, fe11]) => finish({ popup: data.screens.popup, snow: data.screens.bg_snow_loop, fonts: { FEFONT: glyphs }, images: { 'FEFONT-0': font, 'FE_1-14': fe14, 'FE_1-11': fe11 } }));
    return loading;
  }
  function finish(a) {
    const drawer = textDrawer(a.fonts, a.images);
    assets = { ...a, lui: new LuiScreen(a.popup, a.images, drawer), snowLui: a.snow ? new LuiScreen(a.snow, a.images, drawer) : null };
    return assets;
  }

  // ---- layout / drawing -------------------------------------------------------------------------------------------
  function boxBounds(lui) {                        // the frame shapes' extent at their frame-0 state (480-line LUI px)
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const n of BOX) { const e = lui.byName.get(n); if (!e) continue; const p = e.props || {}, nv = e.shape?.[0] ?? 4;
      for (let k = 0; k < nv; k++) { const x = (p[0] || 0) + (p[21 + 9 * k] || 0), y = (p[1] || 0) + (p[22 + 9 * k] || 0); x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); } }
    return { x0, y0, x1, y1 };
  }
  function draw(now) {
    raf = 0; if (!current || !assets) return;
    const dpr = Math.min(3, window.devicePixelRatio || 1);
    const W = canvas.clientWidth || innerWidth, H = canvas.clientHeight || innerHeight;
    if (canvas.width !== Math.round(W * dpr) || canvas.height !== Math.round(H * dpr)) { canvas.width = Math.round(W * dpr); canvas.height = Math.round(H * dpr); }
    const { lui } = assets, msg = current.message, lines = wrapLines(msg.text, assets.fonts.FEFONT);
    // the message box in LUI units: text block + the option row, inside the frame's inner panel margins
    const inner = { w: WRAP + 40, h: 30 + lines.length * PITCH + 18 + 22 + 22 };
    const pic = current.kind === 'rotate' ? 150 : 0;                          // the turning device above the box
    const k = Math.min(1.35, (W * 0.94) / (inner.w + 40), (H * 0.92) / (inner.h + 40 + pic));
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (current.kind === 'rotate') {                                          // the FE background behind a full-screen card
      const g = ctx.createLinearGradient(0, 0, 0, canvas.height); g.addColorStop(0, '#8fbad6'); g.addColorStop(1, FE_BLUE);
      ctx.fillStyle = g; ctx.fillRect(0, 0, canvas.width, canvas.height);
      if (assets.snowLui) {                                                    // the FE's drifting snowflakes (bg_snow_loop)
        const frame = (now - current.at) * 60 / 1000, sf = frame % 600, cover = Math.max(canvas.width / 640, canvas.height / 480);
        const events = assets.snow.events.filter((ev) => ev.frame <= sf).map((ev) => ({ ev, start: frame - sf + ev.frame }));
        ctx.setTransform(cover, 0, 0, cover, (canvas.width - 640 * cover) / 2, (canvas.height - 480 * cover) / 2);
        assets.snowLui.draw(ctx, events, frame);
        ctx.setTransform(1, 0, 0, 1, 0, 0);
      }
    } else { ctx.fillStyle = 'rgba(0,0,0,0.45)'; ctx.fillRect(0, 0, canvas.width, canvas.height); }
    const t = Math.max(0, (now - current.at) / 1000);
    // over the game frame when it is on screen (the hint belongs to the game), else the middle of the page
    const frame = current.kind === 'home' ? doc.getElementById('stage')?.getBoundingClientRect() : null;
    const midY = frame && frame.height > 80 && frame.top >= 0 && frame.bottom <= H ? frame.top + frame.height / 2 : H / 2;
    const cx = W / 2, top = Math.max(4, Math.min(H - (inner.h + pic) * k - 4, midY - (inner.h + pic) * k / 2));
    ctx.setTransform(dpr * k, 0, 0, dpr * k, dpr * (cx - inner.w * k / 2), dpr * (top + pic * k));
    // the popup frame, stretched from its own extent to the message box (like drawPrompt narrows it to its question)
    const b = boxBounds(lui), sx = (inner.w + 36) / (b.x1 - b.x0), sy = (inner.h + 24) / (b.y1 - b.y0);
    const fadeIn = Math.min(1, t * 6);
    ctx.save(); ctx.translate(-18, -12); ctx.scale(sx, sy); ctx.translate(-b.x0, -b.y0);
    lui.draw(ctx, [], 0, (e) => (BOX.includes(e.name) ? { alpha: 255 * fadeIn } : e.kind === 'group' || e.kind === 'menu' ? { alpha: 255 } : { hidden: true }));
    ctx.restore();
    // message (black, as the popup's question) and the one option (white = focused, Cross icon at its left)
    ctx.globalAlpha = fadeIn;
    lui.text(ctx, lines.join(' '), 20, 26, { 6: WRAP, 9: TEXT_SCALE, 12: 9, 14: 0, 15: 0, 16: 0, pitch: PITCH }, 1);
    const oy = 26 + lines.length * PITCH + 14;
    const cross = assets.images[CROSS.page];
    // keyboard: the Space key cap in the icon's place (web/input-glyphs.js)
    if (!drawGlyphAsKey(ctx, lui.ui, CROSS.page, CROSS.sx, CROSS.sy, CROSS.sw, CROSS.sh, inner.w / 2 - 44, oy, 22, 21, { stretch: LUI_STRETCH }) && cross) ctx.drawImage(cross, CROSS.sx, CROSS.sy, CROSS.sw, CROSS.sh, inner.w / 2 - 44, oy, 22, 21);
    lui.text(ctx, msg.option, inner.w / 2 - 16, oy + 1, { 6: 300, 9: TEXT_SCALE, 12: 9, 14: 255, 15: 255, 16: 255 }, 1);
    ctx.globalAlpha = 1;
    if (pic) drawDevice(ctx, inner.w / 2, -pic / 2 - 4, t, msg.want);
    raf = requestAnimationFrame(draw);
  }
  // A device outline turning to the wanted orientation (white with the FE's navy edge), then holding, on a loop.
  function drawDevice(c, x, y, t, want) {
    const phase = (t % 2.4) / 2.4, turn = Math.min(1, Math.max(0, (phase - 0.25) / 0.35)), ease = turn * turn * (3 - 2 * turn);
    const from = want === 'landscape' ? 0 : Math.PI / 2, to = want === 'landscape' ? Math.PI / 2 : 0;
    c.save(); c.translate(x, y); c.rotate(from + (to - from) * ease);
    const w = 62, h = 112, r = 10;
    const body = () => { c.beginPath(); c.roundRect(-w / 2, -h / 2, w, h, r); };
    c.lineWidth = 7; c.strokeStyle = '#112634'; body(); c.stroke();
    c.lineWidth = 4; c.strokeStyle = '#ffffff'; body(); c.stroke();
    c.fillStyle = 'rgba(255,255,255,0.18)'; c.fillRect(-w / 2 + 7, -h / 2 + 12, w - 14, h - 26);
    c.fillStyle = '#ffffff'; c.beginPath(); c.arc(0, h / 2 - 7, 3, 0, Math.PI * 2); c.fill();
    c.restore();
    // the turn arrow
    c.save(); c.translate(x, y); c.lineWidth = 4; c.strokeStyle = '#d57b0b'; c.beginPath(); const a0 = -Math.PI * 0.95, a1 = -Math.PI * 0.55; c.arc(0, 0, 78, a0, a1); c.stroke();
    const ax = Math.cos(a1) * 78, ay = Math.sin(a1) * 78; c.fillStyle = '#d57b0b'; c.beginPath(); c.moveTo(ax + 9, ay + 2); c.lineTo(ax - 4, ay - 9); c.lineTo(ax - 5, ay + 9); c.closePath(); c.fill();
    c.restore();
  }

  // ---- input --------------------------------------------------------------------------------------------------------
  function onKey(e) {
    if (!current) return;
    if (['Space', 'Enter', 'Escape', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'ShiftLeft', 'Backspace'].includes(e.code)) { e.preventDefault(); e.stopImmediatePropagation(); }
    if (e.type === 'keydown' && !e.repeat && ['Space', 'Enter', 'Escape'].includes(e.code) && performance.now() - current.at > 250) hide(true);
  }
  addEventListener('keydown', onKey, { capture: true }); addEventListener('keyup', onKey, { capture: true });
  canvas.addEventListener('pointerdown', (e) => { e.preventDefault(); e.stopPropagation(); });
  canvas.addEventListener('pointerup', (e) => { e.preventDefault(); if (current && performance.now() - current.at > 250) hide(true); });

  function show(kind, message) {
    if (current?.kind === kind && current.message.text === message.text) return;
    current = { kind, message, at: performance.now() };
    canvas.hidden = false; canvas.setAttribute('aria-label', message.text);
    load().then(() => { if (current && !raf) raf = requestAnimationFrame(draw); }).catch((err) => { console.warn('Phone prompt assets', err); hide(); });
  }
  function hide(byPlayer = false) {
    const was = current; current = null; canvas.hidden = true; if (raf) cancelAnimationFrame(raf); raf = 0;
    if (was && byPlayer) was.onClose?.();
  }
  return {
    show(kind, message, onClose = null) { show(kind, message); if (current) current.onClose = onClose; },
    hide: () => hide(false),
    get kind() { return current?.kind || null; },
    load,
  };
}
