// The front end's popup box (cFEPopup, class 0x46CBD8: ctor 0x1C58E8, setup 0x1C6B08) on FE.LUI 'popup': how the code sizes and places the
// box, the message, the options and the Cross icon. Decoded from SLUS_207.72 and checked against the live popup objects of the PS2 captures
// (local/ps2-capture/menus/save-s3 / s4 / s7 / s8 final states: +0x260 size, +0x278 scale, the elements' +0x44 positions).
//
//   0x1C6B08  +0x260 = (desc +0xB0 width, or 300 when compact (+0x2CC, desc +0xBC), else 472; 0); title 0x1C8678, message 0x1C7258,
//             options 0x1C7388, menu 0x1C8758, buttons 0x1C88C8, post 0x1C7040; type (+0x31C) 0x1C66E8; placement 0x1C7C20.
//   message   compact: W = the widest prefix of the text within the element's 472 (0x1C94B0: advance x scale per character); if W > 300 the
//             text elements take width W (0x1C70F0); the text is word-wrapped at W (0x1C9938) and measured line by line (0x1C9038: each
//             line the font box of 0x3921F0, the lines 17.4 + 3 apart at 60 %); +0x260 = (max(w, widest line), h + lines); then h += 10.
//   options   h += EnterTextBox's 22 (0x1C6D30, even when hidden), then 20 per option (the Option elements' height) and 10 (0x1C7388);
//             +0x29C = the widest option text; the menu at x (640 - that) / 2 - 270, y = h after the message (0x1C8758).
//   box       type 1 (framed, the memory-card popups): scale ((w + 20) / 504 x 1.05, (h + 15) / 204) on Big / Small shape / outline and Shadow
//             (0x1C7620; 504 x 204 = Small shape's size), y = 92 + h / 2 - 7.5 (0x1C78C0); type 0: whitefg / redbg at (w + 15) / 516,
//             (h + 10) / 296, y = 92 + h / 2 - 5. Compact: no horizontal shift, the texts centred on x 320 (0x1C7BE0 / 0x1C7C20).
//   colours   type 1 texts black (0x4C6798 = 1, 0, 0, 0), types 0 / 2 white (0x4C6788); the veil is 'grey background' with its own 8-frame
//             fade to vertex A 175 (a gradient (111,177,210) -> (200,225,238), LUI animation 00438b41), drawn at its vertex alpha.
export const FONT_H = 29;                                      // FEFONT line height (font +0x14): 17.4 at the popups' 60 %

// 0x3921F0 mode 0: the glyph box of a string: x from min(pen + dx) to max(pen + dx + w), pen += advance; width = x1 + x0 (x0 <= 0).
export function textBox(glyphs, str, s) {
  let pen = 0, x0 = 0, x1 = 0;
  for (const ch of str) { const g = glyphs[ch] || { advance: 10, dx: 0, w: 10 }; x0 = Math.min(x0, pen + g.dx * s); x1 = Math.max(x1, pen + (g.dx + g.w) * s); pen += g.advance * s; }
  return x1 + x0;
}
// The EE's single-precision sums (a text that exactly fills its width stays on one line: 580 x 0.6 = 347.99997 twice).
const f32 = Math.fround;
// 0x1C94B0: the widest prefix within `limit` (per character: advance x scale, summed in single precision)
export function prefixWidth(glyphs, str, s, limit) {
  let w = 0; const k = f32(s);
  for (const ch of str) { const a = f32((glyphs[ch]?.advance ?? 10) * k), next = f32(w + a); if (limit < next) return w; w = next; }
  return w;
}
// 0x1C9938: per character, advance x scale summed in single precision; past the width the line breaks at its last space (the text
// from there starts the next line's sum).
export function wrap(glyphs, str, s, width) {
  const chars = [...String(str)], k = f32(s), out = []; let start = 0, sum = 0, space = -1;
  for (let i = 0; i < chars.length; i++) {
    const ch = chars[i]; if (ch === ' ') space = i;
    sum = f32(sum + f32((glyphs[ch]?.advance ?? 10) * k));
    if (width < sum && space >= 0) { out.push(chars.slice(start, space).join('')); start = space + 1; i = space; sum = 0; space = -1; }
  }
  out.push(chars.slice(start).join(''));
  return out;
}

// type 1 = framed (memory card, confirmations), compact = the 300 base width (desc +0xBC); scale = the text elements' 60 %.
export function fePopupLayout(glyphs, { message = '', options = [], type = 1, compact = true, width = 0, scale = 0.6 } = {}) {
  let w = width || (compact ? 300 : 472), h = 0, textWidth = 472;
  const lineH = FONT_H * scale + 3;
  if (compact) { const W = prefixWidth(glyphs, message, scale, 472); if (W > w) textWidth = W; else textWidth = 472; }
  const lines = wrap(glyphs, message, scale, textWidth);
  for (const l of lines) { w = Math.max(w, textBox(glyphs, l, scale)); h += lineH; }
  h += 10;
  const menuY = h, optW = Math.max(0, ...options.map((o) => textBox(glyphs, o, scale)));
  w = Math.max(w, 138); h += 22;                               // EnterTextBox (138 x 22), hidden
  for (const _ of options) { w = Math.max(w, 100); h += 20; }  // the Option elements (100 x 20)
  h += 10;
  const menuX = (640 - optW) * 0.5 - 270;
  const box = type === 1 ? { sx: (w + 20) / 504 * 1.05, sy: (h + 15) / 204, y: 92 + h * 0.5 - 7.5 } : { sx: (w + 15) / 516, sy: (h + 10) / 296, y: 92 + h * 0.5 - 5 };
  const xoff = compact ? 0 : (640 - w) * 0.5 - 80;
  return { w, h, lines, textWidth, lineH, menu: [menuX + xoff, menuY], box, xoff, textX: compact ? 320 + xoff : 80 + xoff, textY: 92 };
}

// Draw a type-1 popup on FE.LUI 'popup' (0x1C66E8 type 1: the frame shapes and the grey background shown, whitefg / redbg / black bg
// hidden) with the layout above: `lui` a LuiScreen of the 'popup' screen with shapeScale (the shapes scaled by props 9 / 10), `frame`
// the frames since the popup opened (the grey background's own 8-frame fade to A 175). The message's lines centred on x 320 from y 92
// in black, 17.4 + 3 apart; the options in Menu0000 at the layout's point (Option k at 270, 120 + 20 k), the focused one white, and
// the Cross icon (group 'buttons', its focus frames 30 / 35 / 45 / 55 / 65) moved with them. Draws in the 640 x 480 LUI frame.
const PARTS = new Set(['065adea5', '09797985', '02bb75c5', '0ecd9825', '059e7b67']);   // Big outline / shape, Small outline / shape, Shadow
const OPTIONS = ['067b06b0', '067b06b1', '067b06b2', '067b06b3', '067b06b4'];         // Option0..4
const HIDDEN = ['0df0ab27', '0078ba87', '0279dba7', '05cbab62', '07c54234', '0c55e7c5', '07c59864', '0344d188', '09d8fb02', '0121a25c', '046556e4'];
// `animated` (cFEPopupConfirm, the lodge's questions: 0x1C5C30 called once a frame): the frame shapes grow from 0.2, step k (1 at the
// popup's first frame) at 0.2 + k (target - 0.2) / 24 up to k = 25, where they stay (PS2 lodge-quitpop88 / 96 / 108: A4 8 -> 0.336,
// 16 -> 0.492, 25 -> 0.686 for a 0.667 target); the texts and the icon (0x1C5F20) are clear until the LUI's 'Start' label (frame 25).
// Not animated (the memory-card manager's popup, 0x1A9BC0 -> 0x1C5DD8): at the target scale at once.
export function drawFePopup(c, lui, glyphs, { message = '', options = [], index = 0, frame = 100, animated = false } = {}) {
  const L = fePopupLayout(glyphs, { message, options, type: 1, compact: true });
  if (animated) { const k = Math.min(Math.floor(frame) + 1, 25); L.box = { ...L.box, sx: 0.2 + k * (L.box.sx - 0.2) / 24, sy: 0.2 + k * (L.box.sy - 0.2) / 24 }; }
  const texts = !animated || frame >= 25;
  const focus = options.length ? [30, 35, 45, 55, 65][index] : -1, events = lui.screen.events.filter((ev) => ev.frame === focus).map((ev) => ({ ev, start: 0 }));
  const text = lui.byName.get('07c54234'), tp = { ...(text?.props || {}), 12: 17, 14: 0, 15: 0, 16: 0 };
  lui.draw(c, events, Math.max(0, frame), (e) => {
    const n = e.name;
    if (PARTS.has(n)) { const q = e.props || {}; return { alpha: 255, props: { 0: (q[0] || 0) + L.xoff, 1: L.box.y, 9: L.box.sx * 100, 10: L.box.sy * 100 } }; }
    if (n === '0885e124') return { alpha: 255 };                                         // grey background: its own fade, vertex A only
    if (HIDDEN.includes(n)) return { hidden: true };
    if (n === '0c583950' || n === '09cbb693') return options.length && texts ? { props: { 0: L.menu[0], 1: L.menu[1] } } : { hidden: true };
    const r = OPTIONS.indexOf(n);
    if (r >= 0) { if (r >= options.length || !texts) return { hidden: true }; const on = r === index ? 255 : 0; return { text: options[r], props: { 13: 255, 14: on, 15: on, 16: on } }; }
    return null;
  });
  if (texts) L.lines.forEach((line, i) => lui.text(c, line, L.textX, L.textY + i * L.lineH, tp, 1, false));
  return L;
}
