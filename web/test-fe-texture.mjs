// Front-end pictures against the PS2 (docs/first-load.md "Front-end texture fixes"): headless Chrome + a private Vite
// server walk title -> Main Menu -> Options -> Select Character -> Setup Character -> Select Peak and check, on each
// screen, what the PS2 frames of local/ps2-capture/menus/fe-texture show and the port used to get wrong:
//  - title: the original 06title (FE blue top, white ramp down to the last line, the logo), "Press START button" at
//    the LUI colour x 0.8;
//  - Main Menu: the dark help panel over the mountain (draw order), Multi Play greyed at alpha 0.5;
//  - Options: white LUI text at 204 (the font's 0xCC texels), glyphs as tall as on the PS2;
//  - Select Peak: the white ramp on the right ('whitefade' shape);
//  - with the PS2 frames present (they are not in the repo): the mean difference of each screen under a bound.
// Snow flakes move with the clock, so the checks sample places they do not reach and the bounds leave room for them.
//   node test-fe-texture.mjs     (skips when no Chrome is installed or the game data is missing)
import assert from 'node:assert/strict';
import fs from 'node:fs'; import path from 'node:path'; import zlib from 'node:zlib';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';

const web = path.dirname(new URL(import.meta.url).pathname);
const PS2 = path.join(web, '../local/ps2-capture/menus/fe-texture');
if (!fs.existsSync(path.join(web, 'public/assets/UI/fe-menus.json'))) { console.log('fe texture: skipped (no game data)'); process.exit(0); }

function png(buf) { // RGBA8 / RGB8 PNG -> {w, h, px(x, y) -> [r, g, b]}
  let at = 8, w, h, ct; const idat = [];
  while (at < buf.length) { const len = buf.readUInt32BE(at), type = buf.toString('ascii', at + 4, at + 8), d = buf.subarray(at + 8, at + 8 + len); if (type === 'IHDR') { w = d.readUInt32BE(0); h = d.readUInt32BE(4); ct = d[9]; } if (type === 'IDAT') idat.push(d); at += 12 + len; }
  const raw = zlib.inflateSync(Buffer.concat(idat)), bpp = ct === 6 ? 4 : 3, stride = w * bpp, out = Buffer.alloc(h * stride);
  for (let y = 0; y < h; y++) { const f = raw[y * (stride + 1)], line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let x = 0; x < stride; x++) { const a = x >= bpp ? out[y * stride + x - bpp] : 0, up = y ? out[(y - 1) * stride + x] : 0, c = x >= bpp && y ? out[(y - 1) * stride + x - bpp] : 0; let v = line[x];
      if (f === 1) v += a; else if (f === 2) v += up; else if (f === 3) v += (a + up) >> 1; else if (f === 4) { const p = a + up - c, pa = Math.abs(p - a), pb = Math.abs(p - up), pc = Math.abs(p - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? up : c; }
      out[y * stride + x] = v & 255; } }
  return { w, h, px: (x, y) => { const o = y * stride + x * bpp; return [out[o], out[o + 1], out[o + 2]]; } };
}
const region = (img, [x0, y0, x1, y1], pick) => { let best = null; for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) { const p = img.px(x, y); if (!best || pick(p, best)) best = p; } return best; };
const brightest = (img, r) => region(img, r, (p, b) => p[0] + p[1] + p[2] > b[0] + b[1] + b[2]);
const darkest = (img, r) => region(img, r, (p, b) => p[0] + p[1] + p[2] < b[0] + b[1] + b[2]);
const near = (a, b, tol, what) => assert.ok(a.every((v, i) => Math.abs(v - b[i]) <= tol), `${what}: ${a} vs ${b}`);
function meanDiff(a, b) { let s = 0; for (let y = 0; y < a.h; y++) for (let x = 0; x < a.w; x++) { const p = a.px(x, y), q = b.px(x, y); s += (Math.abs(p[0] - q[0]) + Math.abs(p[1] - q[1]) + Math.abs(p[2] - q[2])) / 3; } return s / (a.w * a.h); }
function textHeight(img, [x0, y0, x1, y1], bright) { let top = 1e9, bottom = -1; for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) { const p = img.px(x, y), l = (p[0] + p[1] + p[2]) / 3; if (bright ? l > 150 : l < 80) { top = Math.min(top, y); bottom = Math.max(bottom, y); } } return bottom - top + 1; }

const server = await startServer();
const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) { await server.close(); console.log('fe texture: skipped (no Chrome)'); process.exit(0); }
const failures = [], shots = {};
const key = async (code, key = code, vk = 0) => { for (const type of ['keyDown', 'keyUp']) await browser.send('Input.dispatchKeyEvent', { type, code, key, windowsVirtualKeyCode: vk }); };
const shot = async (name) => { shots[name] = png(Buffer.from(await browser.screenshot('#stage'), 'base64')); return shots[name]; };
const screen = () => browser.evaluate(`document.getElementById('stage').dataset.screen`);
const check = (name, fn) => { try { fn(); } catch (e) { failures.push(`${name}: ${e.message}`); } };
try {
  await browser.goto(server.origin + '/?perf=1&diag=0&glyphs=gamepad' + (process.env.FE_QUERY ? '&' + process.env.FE_QUERY : ''));   // FE_QUERY=luiorder=flat&luitext=old: the old drawing (must fail)
  await browser.waitFor('!!(window.ssxBoot && window.ssxBoot.state.ready) || !!document.body.dataset.loadError', 240000);
  assert.equal(await browser.evaluate('document.body.dataset.loadError || ""'), '');
  await sleep(1200);
  const title = await shot('title');
  check('title', () => {
    near(title.px(320, 10), [117, 169, 203], 4, 'FE blue at the top');
    for (const x of [5, 320, 635]) assert.ok(title.px(x, 479).every((v) => v >= 245), `white ramp down to the last line at x ${x}: ${title.px(x, 479)}`);
    near(darkest(title, [210, 370, 430, 390]), [29, 5, 4], 6, '"Press START button" (37, 7, 5) x 0.8');
    assert.ok(textHeight(title, [210, 366, 430, 395], false) >= 12, '"Press START button" glyph height (PS2: 13 px)');
  });
  await key('Enter', 'Enter', 13); await sleep(3000);
  const s1 = await screen(); check('main menu screen', () => assert.equal(s1, 'main'));
  const main = await shot('main');
  check('main menu', () => {
    // the help panel (layer 7 in a layer-8 group) covers the mountain (layer 7 in a layer-0 group): the peak is dark
    near(main.px(60, 330), [79, 123, 147], 10, 'mountain under the dark panel (PS2 79,123,147; drawn over it: 129,180,202)');
    near(main.px(150, 300), [47, 98, 125], 10, 'mountain under the dark panel (PS2 47,98,125; drawn over it: 46,122,150)');
    near(darkest(main, [290, 208, 400, 226]), [58, 84, 101], 8, 'Multi Play greyed at alpha 0.5');
    near(brightest(main, [45, 158, 225, 195]), [204, 204, 204], 3, 'white help text x 0.8');
  });
  await key('ShiftLeft', 'Shift', 16); await sleep(3000);
  const opts = await shot('options');
  check('options', () => {
    near(brightest(opts, [65, 42, 170, 65]), [204, 204, 204], 3, '"Options" title x 0.8');
    assert.ok(textHeight(opts, [62, 38, 200, 70], true) >= 21, `"Options" glyph height ${textHeight(opts, [62, 38, 200, 70], true)} (PS2: 22 px)`);
  });
  await key('Escape', 'Escape', 27); await sleep(2000);
  await key('Enter', 'Enter', 13); await sleep(4000);           // Single Event -> Select Character
  await key('Enter', 'Enter', 13); await sleep(3000);           // -> Setup Character
  await key('Enter', 'Enter', 13); await sleep(3500);           // Continue -> Select Peak
  const peak = await shot('peak');
  const s2 = await screen(); check('select peak screen', () => assert.equal(s2, 'fe-peak'));
  check('select peak', () => { for (const y of [20, 240]) assert.ok(peak.px(635, y).every((v) => v >= 240), `white ramp on the right at y ${y}: ${peak.px(635, y)}`); });
  // Whole screens against the PS2 frames (local only: they come from the user's disc)
  const refs = { title: ['11-title-press-start-state', 6.0], main: ['20-main-menu-single-event', 6.5], options: ['30-options-game-options-focused', 7.5], peak: ['57-select-peak', 8.5] };
  const means = {};
  for (const [name, [file, bound]] of Object.entries(refs)) {
    const f = path.join(PS2, file + '.png'); if (!fs.existsSync(f) || !shots[name]) continue;
    const m = meanDiff(png(fs.readFileSync(f)), shots[name]); means[name] = +m.toFixed(2);
    if (m > bound) failures.push(`${name}: mean difference to the PS2 frame ${m.toFixed(2)} > ${bound}`);
  }
  console.log('fe texture: vs PS2 mean', JSON.stringify(means));
} finally { await browser.close(); await server.close(); }
if (failures.length) { console.error(failures.join('\n')); process.exit(1); }
console.log('fe texture ok');
