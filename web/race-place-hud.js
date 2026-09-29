// In-race place display 0x21E1B0 ("3RD/6", top-left) from the HUD atlas OV_1-3 (texture 0x657).
// Layout record 0x4768B0+0x360: position (20,20), anchor top-left. Number nm{place+1}w at (x,y), size
// 26x42 for 1st else 40x42 (0x477F30 + place*0x24); suffix st/nd/rd/th at (x+W,y) 40x22 (0x478008);
// slash at (x+W,y+22) 20x20 (0x47802C); rider count nm{N}w at (x+W+20,y+22) 20x20 (0x478050).
// Colours (A,R,G,B floats -> vertex colour x128, GS modulate): 1st (1,1,.8,0) gold, others
// (1,0,.494,.7) blue; slash/count white. Change animation (timer t from web/ai-race.js):
//   1st: number+suffix scaled from the top-left by 1 + sin(4*pi*t)*0.19963631*(1-t*t);
//   other places: number+suffix alpha x (0.5t+0.5).
// 1st-place glow (0x21E1B0 -> 0x21E7E0 twice, draw order 10: behind the number and suffix): while the rider
// leads, a second timer g (place object +0x4C, 0x1EBC10) runs 0 -> 2 and wraps (+0.026782159 per tick; -1
// otherwise). The renderer-owned part texture (hud+0x474 = handle 0x5F5, UI/part-glow.png, UV 0,0,1,1) in
// the number colour covers each rect grown about its centre by 1.8101751 (gp-0x535C), with alpha
// A x (0.5 + 0.5 x tri(g)), tri = g or 2 - g; a rect wider than 72 is drawn as a 36-wide U 0..0.5 end, a
// constant-U 0.5 middle and a 36-wide U 0.5..1 end (as the boost coil glow).
import { SPRITE_2D } from './sprite-canvas.js'; // offscreen sprite canvas kind (software in Firefox, docs/firefox-load.md)
const RECTS = { // source pixels in the 256x256 atlas (UV half-texel insets as authored)
  nm1w: [0.5, 84.5, 25.5, 125.5], nm2w: [30.5, 84.5, 73.5, 125.5], nm3w: [77.5, 84.5, 120.5, 125.5], nm4w: [125.5, 84.5, 168.5, 125.5],
  nm5w: [176.5, 84.5, 219.5, 125.5], nm6w: [211.5, 175.5, 254.5, 216.5],
  st_w: [216.5, 62.5, 255.5, 83.5], nd_w: [215.5, 129.5, 255.5, 150.5], rd_w: [216.5, 39.5, 255.5, 60.5], th_w: [214.5, 152.5, 254.5, 173.5],
  slash: [228.5, 85.5, 251.5, 125.5],
};
const SUFFIX = ['st_w', 'nd_w', 'rd_w', 'th_w'];
const GOLD = [1, 0.8, 0], BLUE = [0, 0.494, 0.7];
const tinted = new Map();
export const GLOW_STEP = Math.fround(0.026782158762216568), GLOW_SCALE = Math.fround(1.8101750612258911);
// 0x1EBC10: the glow timer of the place object for this tick's place (0 = first).
export function placeGlowStep(g, place) {
  if (place !== 0) return -1;
  if (!(g >= 0)) return 0;
  let t = Math.fround(g + GLOW_STEP); while (t > 2) t = Math.fround(t - 2); return t;
}
const glowTints = new Map();
function tintedGlow(image, rgb) { // GS modulate of the part texture by the vertex colour (x128 >> 7)
  const key = rgb.join(','); let canvas = glowTints.get(key); if (canvas) return canvas;
  canvas = document.createElement('canvas'); canvas.width = image.width; canvas.height = image.height; const c = canvas.getContext('2d', SPRITE_2D); c.drawImage(image, 0, 0);
  const data = c.getImageData(0, 0, canvas.width, canvas.height); const k = rgb.map((v) => Math.max(0, Math.min(128, Math.trunc(v * 128))));
  for (let i = 0; i < data.data.length; i += 4) for (let j = 0; j < 3; j++) data.data[i + j] = data.data[i + j] * k[j] / 128;
  c.putImageData(data, 0, 0); glowTints.set(key, canvas); return canvas;
}
// 0x21E7E0: one glow submission (float32 geometry as the original).
function drawGlow(ctx, image, g, alpha, x, y, w, h) {
  const f = Math.fround, dw = f(w * f(GLOW_SCALE - 1)), dh = f(h * f(GLOW_SCALE - 1));
  x = f(x - f(dw * 0.5)); y = f(y - f(dh * 0.5)); w = f(w + dw); h = f(h + dh);
  const pulse = f(0.5 + f(0.5 * (g > 1 ? f(2 - g) : g)));
  ctx.globalAlpha = Math.trunc(f(alpha * pulse) * 128) / 128;
  const iw = image.width, ih = image.height;
  if (w > 72) {
    ctx.drawImage(image, 0, 0, iw / 2, ih, x, y, 36, h);
    ctx.drawImage(image, iw / 2 - 0.5, 0, 1, ih, x + 36, y, f(w - 72), h);
    ctx.drawImage(image, iw / 2, 0, iw / 2, ih, f(x + f(w - 72)) + 36, y, 36, h);
  } else ctx.drawImage(image, 0, 0, iw, ih, x, y, w, h);
}
function tintedAtlas(image, rgb) {
  const key = rgb.join(','); let canvas = tinted.get(key);
  if (canvas) return canvas;
  canvas = document.createElement('canvas'); canvas.width = image.width; canvas.height = image.height;
  const c = canvas.getContext('2d', SPRITE_2D); c.drawImage(image, 0, 0);
  c.globalCompositeOperation = 'multiply'; c.fillStyle = `rgb(${rgb.map((v) => Math.round(v * 255)).join(',')})`; c.fillRect(0, 0, canvas.width, canvas.height);
  c.globalCompositeOperation = 'destination-in'; c.drawImage(image, 0, 0);
  tinted.set(key, canvas); return canvas;
}
function quad(ctx, image, name, x, y, w, h) { const [u0, v0, u1, v1] = RECTS[name]; ctx.drawImage(image, u0, v0, u1 - u0, v1 - v0, x, y, w, h); }
export function drawRacePlace(ui, ctx, hud) {
  const atlas = ui.images['OV_1-3'];
  if (!atlas || !hud || hud.place < 0 || hud.place > 5) return;
  const place = hud.place, first = place === 0, W = first ? 26 : 40, x = 20, y = 20, t = hud.timer ?? -1;
  const tint = tintedAtlas(atlas, first ? GOLD : BLUE);
  const glow = ui.images['part-glow'];
  if (first && glow && hud.glow >= 0) { ctx.save(); const im = tintedGlow(glow, GOLD); drawGlow(ctx, im, hud.glow, 1, x, y, W, 42); drawGlow(ctx, im, hud.glow, 1, x + W, y, 40, 22); ctx.restore(); }
  ctx.save();
  if (t >= 0 && !first) ctx.globalAlpha = 0.5 * t + 0.5;
  if (t >= 0 && first) { const s = 1 + Math.sin(4 * Math.PI * t) * 0.19963631 * (1 - t * t); ctx.translate(x, y); ctx.scale(s, s); ctx.translate(-x, -y); }
  quad(ctx, tint, `nm${place + 1}w`, x, y, W, 42);
  quad(ctx, tint, SUFFIX[place < 4 ? place : 3], x + W, y, 40, 22);
  ctx.restore();
  if (hud.total >= 1 && hud.total <= 6) { quad(ctx, atlas, 'slash', x + W, y + 22, 20, 20); quad(ctx, atlas, `nm${hud.total}w`, x + W + 20, y + 22, 20, 20); }
}
