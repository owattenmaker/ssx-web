// Visual-parity fixes (docs/visual-parity.md) against PS2 frames.
// 1. Race HUD (pv raceHud): the race clock 0x1F16C0 -> 0x1F1840 at descriptor 0x1A and the speed widget 0x2200C0 at
//    descriptor 25, laid out from the original numbers.
// 2. HUD text colour (pv hudText): a font draw's vertex RGB is trunc(colour x 204) (renderer 0x378808).
// 3. Pixels: the two draw lists software-rendered (bilinear glyph blits, shadow then glyph, as the canvas renderer draws
//    them) over the PS2 screenshot local/ps2-capture/runs/peak2/cra3-full.tick418.png, which shows the PS2's own clock
//    "00:00:03" and speed "60 MPH"; on the pixels the drawn text covers completely, the PS2 pixels must match. The
//    page's old text path (ui.text: flat tint, other sizes and places) is rendered the same way for contrast.
import fs from 'node:fs'; import zlib from 'node:zlib';
import { TrickHud, hudTextByte } from './trick-hud.js';
import { PV_DEFAULTS } from './pv-flags.js';
import { sourceOf } from './test-source.mjs';

const ui = new URL('public/assets/UI/', import.meta.url);
const data = JSON.parse(fs.readFileSync(new URL('trick-hud.json', ui)));
const glyphs = { FEFONT: JSON.parse(fs.readFileSync(new URL('FEFONT-glyphs.json', ui))), HUDFONT: JSON.parse(fs.readFileSync(new URL('HUDFONT-glyphs.json', ui))) };
const hud = new TrickHud(data, glyphs);
let failed = 0; const check = (ok, what) => { if (!ok) { failed++; console.error('FAIL', what); } else console.log('ok', what); };
const near = (a, b, e = 1e-4) => Math.abs(a - b) <= e;

// 1a. The clock: 125 x 21 group centred on x 320, top 20; pairs every 39 px, colons 5 px, '1' digits 4.75 px right.
{ const out = []; hud.raceClock(out, 0, 1, 17);
  check(out.length === 5 && out.map((d) => d.text).join('') === '00:01:17', 'clock pieces 00 : 01 : 17');
  check(near(out[0].x, 257.5) && out.every((d) => d.y === 20), 'clock group at x 257.5, top 20 (320 - 125 / 2)');
  check(near(out[1].x, 257.5 + 39) && near(out[2].x, 257.5 + 44 + 4.75) && near(out[3].x, 257.5 + 83) && near(out[4].x, 257.5 + 88 + 4.75), 'clock pitches 39 / 5, one-digit shift 4.75');
  check(out.every((d) => d.font === 'HUDFONT' && d.shadow[0] === 2 && d.shadow[1] === 2 && d.argb.every((c) => c === 1)), 'clock: HUD font, shadow (2, 2), white');
  const red = []; hud.raceClock(red, 0, 0, 5, [1, 1, 0, 0]); check(red.every((d) => d.argb.join() === '1,1,0,0'), 'freestyle red override 0x4C8688'); }
// 1b. The speed: label bottom-left at (20, 460); the number above it at 1.3623675 (2 digits) / 1.121412 (3 digits).
{ const out = []; hud.speed(out, 60 / (0.036 * 0.621) * 1.0005, false);
  const [label, num] = out; const lsize = hud.measure('HUDFONT', 'MPH', 1, 1);
  check(label.text === 'MPH' && near(label.x, 20) && near(label.y, 460 - lsize.h), 'speed label MPH at (20, 460 - h)');
  check(num.text === '60' && near(num.scale[0], 1.3623675107955933), 'speed 60 mph at scale 1.3623675');
  const w = hud.measure('HUDFONT', '60', num.scale[0], num.scale[1]).w;
  check(near(num.x, 20 + lsize.w / 2 - w / 2, 1e-3) && near(num.y, label.y - (21 * num.scale[1] / 2 + 14.003287315368652), 1e-3), 'speed number centred over the label, 14.0033 above its middle');
  const kmh = []; hud.speed(kmh, 100 / 0.036 * 1.0005, true); check(kmh[0].text === 'KM/H' && kmh[1].text === '100' && near(kmh[1].scale[0], 1.1214120388031006), 'km/h: 100 at scale 1.121412');
  const slow = []; hud.speed(slow, 0.2, false); check(slow[1].text === '0', 'standing still reads 0'); }
// 2. Text colour bytes.
check(hudTextByte(1) === 204 && hudTextByte(0.7828530073165894) === 159 && hudTextByte(0) === 0, 'font vertex RGB = trunc(colour x 204): white 204, OPPONENT red 159');
// 2b. Finish / time-up banner 0x21F660 (pv finishBanner): a finish is the 'fini' sprite at descriptor 0x42 (320, 190, 213 x 41,
//     centred) with the value centred under it at scale 1.9075785; time up is 'timeup' at descriptor 0x1B (320, 180, 240 x 41).
{ const fin = []; hud.finishBanner(fin, false, '00:03:55'); const [spr, txt] = fin, tw = hud.measure('HUDFONT', '00:03:55', txt.scale[0], txt.scale[1]).w;
  check(spr.kind === 'sprite' && spr.sprite.page === 'OV_1-3' && near(spr.x, 213.5) && near(spr.y, 169.5) && spr.size.join() === '213,41', `finish sprite at (213.5, 169.5) 213 x 41: (${spr.x}, ${spr.y}) ${spr.size}`);
  check(spr.sprite.uv.map((v) => v * 256).join() === '1.5,1.5,132.5,25.5', "'fini' UV rows v0,u0,u1,v1 = 1.5, 1.5, 132.5, 25.5 (owner +0x4B4 record)");
  check(txt.text === '00:03:55' && near(txt.x + tw / 2, 320, 1e-3) && near(txt.y, 210.5) && near(txt.scale[0], 1.907578468322754) && txt.argb.every((c) => c === 1), 'finish value centred at x 320, top 210.5, scale 1.9075785, white');
  const up = []; hud.finishBanner(up, true, '00:03:55');
  check(up.length === 1 && near(up[0].x, 200) && near(up[0].y, 159.5) && up[0].size.join() === '240,41' && up[0].sprite.uv.map((v) => v * 256).join() === '127.5,1.5,170.5,150.5', "time up: 'timeup' at (200, 159.5) 240 x 41, no value");
  }

// 3. Pixels against the PS2 frame.
function png(file) {
  const b = fs.readFileSync(file); let at = 8, w, h, ct; const idat = [];
  while (at < b.length) { const n = b.readUInt32BE(at), t = b.toString('latin1', at + 4, at + 8), d = b.subarray(at + 8, at + 8 + n); at += 12 + n; if (t === 'IHDR') { w = d.readUInt32BE(0); h = d.readUInt32BE(4); ct = d[9]; } else if (t === 'IDAT') idat.push(d); }
  const ch = ct === 6 ? 4 : 3, stride = w * ch, raw = zlib.inflateSync(Buffer.concat(idat)), out = new Uint8Array(w * h * 4); let prev = new Uint8Array(stride);
  for (let y = 0; y < h; y++) { const f = raw[y * (stride + 1)], line = Uint8Array.from(raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1)));
    for (let i = 0; i < stride; i++) { const a = i >= ch ? line[i - ch] : 0, u = prev[i], c = i >= ch ? prev[i - ch] : 0; let p = 0;
      if (f === 1) p = a; else if (f === 2) p = u; else if (f === 3) p = (a + u) >> 1; else if (f === 4) { const q = a + u - c, pa = Math.abs(q - a), pb = Math.abs(q - u), pc = Math.abs(q - c); p = pa <= pb && pa <= pc ? a : pb <= pc ? u : c; }
      line[i] = (line[i] + p) & 255; }
    for (let x = 0; x < w; x++) { for (let k = 0; k < ch; k++) out[(y * w + x) * 4 + k] = line[x * ch + k]; if (ch === 3) out[(y * w + x) * 4 + 3] = 255; }
    prev = line; }
  return { w, h, d: out };
}
const ps2File = new URL('../local/ps2-capture/runs/peak2/cra3-full.tick418.png', import.meta.url);
if (!fs.existsSync(ps2File)) console.log('skip pixel check: PS2 frame not present (local/ps2-capture/runs/peak2/cra3-full.tick418.png)');
else {
  const ps2 = png(ps2File), atlas = { HUDFONT: png(new URL('HUDFONT-0.png', ui)), FEFONT: png(new URL('FEFONT-0.png', ui)) };
  const sample = (A, u, v) => { const x0 = Math.floor(u - 0.5), y0 = Math.floor(v - 0.5), fx = u - 0.5 - x0, fy = v - 0.5 - y0, px = (x, y) => { x = Math.max(0, Math.min(A.w - 1, x)); y = Math.max(0, Math.min(A.h - 1, y)); return (y * A.w + x) * 4; };
    const o = [0, 0, 0, 0]; for (const [x, y, wgt] of [[x0, y0, (1 - fx) * (1 - fy)], [x0 + 1, y0, fx * (1 - fy)], [x0, y0 + 1, (1 - fx) * fy], [x0 + 1, y0 + 1, fx * fy]]) { const i = px(x, y), a = A.d[i + 3] / 255; for (let k = 0; k < 3; k++) o[k] += A.d[i + k] * a * wgt; o[3] += a * wgt; } return o; };
  // one 640x480 layer (premultiplied RGB + coverage) per draw list: the glyph quads of each text, shadow first
  function render(items) {
    const L = new Float32Array(640 * 480 * 4);
    const blit = (it, dx, dy, rgb) => { const g = hud.glyphs[it.font], A = atlas[it.font]; let pen = it.x + dx;
      for (const ch of it.text) { const q = g[ch]; if (!q) continue; const x0 = pen + q.dx * it.scale[0], y0 = it.y + dy + q.dy * it.scale[1], w = q.w * it.scale[0], h = q.h * it.scale[1];
        for (let y = Math.max(0, Math.floor(y0)); y < Math.min(480, Math.ceil(y0 + h)); y++) for (let x = Math.max(0, Math.floor(x0)); x < Math.min(640, Math.ceil(x0 + w)); x++) {
          const u = q.x + (x + 0.5 - x0) / it.scale[0], v = q.y + (y + 0.5 - y0) / it.scale[1]; if (u < q.x || v < q.y || u > q.x + q.w || v > q.y + q.h) continue;
          const s = sample(A, u, v), i = (y * 640 + x) * 4; for (let k = 0; k < 3; k++) L[i + k] = s[k] * rgb[k] / 255 + L[i + k] * (1 - s[3]); L[i + 3] = s[3] + L[i + 3] * (1 - s[3]); }
        pen += q.advance * it.scale[0]; } };
    // sprites (0x1F1190: the quad over UV rows v0,u0,u1,v1, vertex colour unity) under the text of the same list
    const sprite = (it) => { const A = atlas[it.sprite.page] ??= png(new URL(`${it.sprite.page}.png`, ui)), [v0, u0, u1, v1] = it.sprite.uv, w = it.size[0] * it.scale[0], h = it.size[1] * it.scale[1];
      for (let y = Math.max(0, Math.floor(it.y)); y < Math.min(480, Math.ceil(it.y + h)); y++) for (let x = Math.max(0, Math.floor(it.x)); x < Math.min(640, Math.ceil(it.x + w)); x++) {
        const fu = (x + 0.5 - it.x) / w, fv = (y + 0.5 - it.y) / h; if (fu < 0 || fv < 0 || fu > 1 || fv > 1) continue;
        const s = sample(A, (u0 + (u1 - u0) * fu) * A.w, (v0 + (v1 - v0) * fv) * A.h), i = (y * 640 + x) * 4; for (let k = 0; k < 3; k++) L[i + k] = s[k] + L[i + k] * (1 - s[3]); L[i + 3] = s[3] + L[i + 3] * (1 - s[3]); } };
    for (const it of items) if (it.kind === 'sprite') sprite(it);
    for (const it of items) if (it.kind !== 'sprite') { blit(it, it.shadow?.[0] ?? 0, it.shadow?.[1] ?? 0, [0, 0, 0]); blit(it, 0, 0, it.rgb); }
    return L;
  }
  // The drawn text composited over the PS2 frame, then the PS2 display's horizontal resample (the GS draws the HUD into a
  // 512-wide buffer shown 640 wide: a 3-tap [1/4, 1/2, 1/4] stand-in), against the PS2 pixels over the text's box, best of
  // +/-1 px (the GS display offset).
  function error(L, rect, ref = ps2) { let best = Infinity; const comp = (x, y, k) => { const i = (y * 640 + x) * 4; return L[i + k] + ref.d[i + k] * (1 - L[i + 3]); };
    for (const [ox, oy] of [[0, 0], [0, 1], [1, 0], [0, -1], [-1, 0], [1, 1], [-1, 1]]) { let s = 0, n = 0;
      for (let y = rect[1]; y < rect[1] + rect[3]; y++) for (let x = rect[0]; x < rect[0] + rect[2]; x++) {
        const p = ((y + oy) * 640 + x + ox) * 4; for (let k = 0; k < 3; k++) { s += Math.abs(0.25 * comp(x - 1, y, k) + 0.5 * comp(x, y, k) + 0.25 * comp(x + 1, y, k) - ref.d[p + k]); n++; } }
      best = Math.min(best, s / n); }
    return best; }
  const clock = [], speed = []; hud.raceClock(clock, 0, 0, 3); hud.speed(speed, 60 / (0.036 * 0.621) * 1.0005, false);
  const withRgb = (list, byte) => list.map((d) => ({ ...d, rgb: d.argb.slice(1).map(byte) }));
  const clockRect = [250, 15, 140, 30], speedRect = [10, 405, 90, 60];
  const eClock = error(render(withRgb(clock, hudTextByte)), clockRect), eSpeed = error(render(withRgb(speed, hudTextByte)), speedRect);
  // the page's old text (web/ui.js ui.text: HUDFONT at size / 17, colour flat, no shadow): clock size 21 centred at 320, top 20;
  // speed size 27 centred at x 32, y 391, "MPH" size 11 centred at x 32, y 418 (UI canvas y x 480 / 448)
  const old = (text, x, y, size, rgb) => { const sc = size / 17, g = hud.glyphs.HUDFONT; let w = 0; for (const ch of text) w += (g[ch]?.advance || 10) * sc;
    return { font: 'HUDFONT', text, x: x - w / 2, y: y * 480 / 448, scale: [sc, sc * 480 / 448], shadow: [0, 0], rgb }; };
  const flat = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
  const oClock = error(render([old('00:00:03', 320, 20, 21, flat('#eef5ee'))]), clockRect), oSpeed = error(render([old('60', 32, 391, 27, flat('#edf5e8')), old('MPH', 32, 418, 11, flat('#e1e9e5'))]), speedRect);
  if (process.env.VP_DEBUG) { const L = render(withRgb(clock.concat(speed), hudTextByte)), o = Buffer.alloc(640 * 480 * 4); for (let i = 0; i < 640 * 480; i++) { const a = L[i * 4 + 3]; for (let k = 0; k < 3; k++) o[i * 4 + k] = Math.round(L[i * 4 + k] + ps2.d[i * 4 + k] * (1 - a)); o[i * 4 + 3] = 255; } fs.writeFileSync(process.env.VP_DEBUG, JSON.stringify({ w: 640, h: 480 })); fs.writeFileSync(process.env.VP_DEBUG + '.rgba', o); }
  console.log(`PS2 cra3-full 418, mean |d| over the text boxes: clock ${eClock.toFixed(1)} (old path ${oClock.toFixed(1)}), speed ${eSpeed.toFixed(1)} (old path ${oSpeed.toFixed(1)})`);
  const white = (v) => Math.max(0, Math.min(255, Math.round(Math.trunc(v * 128) * 255 / 128))); // hudText off: the 128-unity tint
  const wClock = error(render(withRgb(clock, white)), clockRect), wSpeed = error(render(withRgb(speed, white)), speedRect);
  console.log(`  the same layout at the 128-unity colour (hudText off): clock ${wClock.toFixed(1)}, speed ${wSpeed.toFixed(1)}`);
  check(eClock < 14 && eSpeed < 10, 'clock and speed text match the PS2 frame (box error < 14 / 10)');
  // R7 pixels: the Uber hint over PS2 uber-chain tick 929 ("UBER TRICK = [R1] + [square]"; the capture's hint draw of owner +0x560)
  const ucFile = new URL('../local/ps2-capture/runs/uber-chain.tick929.png', import.meta.url);
  if (fs.existsSync(ucFile)) { const uc = png(ucFile), rect = [190, 425, 260, 45], nl = [], ol = [];
    hud.iconLabel(nl, hud.iconLayout('UBER TRICK = @r1 + @square', Math.fround(0.7)), 73, {}, { hint: true });
    const eN = error(render(withRgb(nl, hudTextByte)), rect, uc);
    console.log(`PS2 uber-chain 929 Uber hint box error ${eN.toFixed(1)}`);
    check(eN < 8, 'Uber hint (0x1E92A8 record) matches the PS2 frame'); }
  check(oClock > eClock + 10 && oSpeed > eSpeed + 3, 'the old text path is measurably further from the PS2 frame');
  check(wClock > eClock + 1 && wSpeed > eSpeed + 1, 'the 204 text colour is closer than white');
  // The slope style OPPONENT line (career-ui.js opponentLine, pv raceHud): descriptors 0x4A / 0x4B in the HUD font with the
  // yellow 0x4C88A8 (A,R,G,B 1,1,0.798,0) for a tie, over PS2 peak2/dss2-full.tick418.png ('OPPONENT' / '+0' at 203,161,0).
  const dssFile = new URL('../local/ps2-capture/runs/peak2/dss2-full.tick418.png', import.meta.url);
  if (fs.existsSync(dssFile)) {
    const dss = png(dssFile), opp = []; hud.text(opp, 'HUDFONT', 0x4A, 'OPPONENT', { colour: [1, 1, 0.7977359890937805, 0] }); hud.text(opp, 'HUDFONT', 0x4B, '+0', { colour: [1, 1, 0.7977359890937805, 0] });
    const rect = [15, 60, 110, 35], eOpp = error(render(withRgb(opp, hudTextByte)), rect, dss);
    const oldLeft = (text, y) => ({ ...old(text, 0, y, 12, [255, 255, 204]), x: 20 });   // ui.text left-aligned at x 20, size 12, rgb(255,255,204)
    const oldOpp = error(render([oldLeft('OPPONENT', 62), oldLeft('+0', 74)]), rect, dss);
    console.log(`PS2 dss2-full 418 OPPONENT line box error ${eOpp.toFixed(1)} (old text ${oldOpp.toFixed(1)})`);
    check(eOpp < 12 && oldOpp > eOpp + 5, 'the OPPONENT line matches the PS2 frame');
  }
  // The finish banners over PS2 frames: a race (ctm-parity/runs/race-f/fin.png, FINISH! 00:03:55) and a freestyle run
  // (runs/pipe-finishov.tick4408.png, FINISH! 2000), against the same banner at descriptor 0x1B (the time-up place).
  for (const [rel, value] of [['ctm-parity/runs/race-f/fin.png', '00:03:55'], ['runs/pipe-finishov.tick4408.png', '2000']]) {
    const f = new URL(`../local/ps2-capture/${rel}`, import.meta.url); if (!fs.existsSync(f)) { console.log(`skip finish banner: ${rel} not present`); continue; }
    const ref = png(f), rect = [195, 158, 250, 90], banner = (k) => { const l = []; hud.finishBanner(l, false, value, k); return render(withRgb(l, hudTextByte).map((d) => d.kind === 'sprite' ? { ...d, rgb: [255, 255, 255] } : d)); };
    const eFin = error(banner(0x42), rect, ref), e1B = error(banner(0x1B), rect, ref);
    console.log(`PS2 ${rel} finish banner box error ${eFin.toFixed(1)} (at descriptor 0x1B ${e1B.toFixed(1)})`);
    check(eFin < 14 && e1B > eFin + 8, `the finish banner matches ${rel}`);
  }
  const upFile = new URL('../local/ps2-capture/runs/pipe-brake.tick7519.png', import.meta.url);   // TIME'S UP (2:00 timeout)
  if (fs.existsSync(upFile)) { const ref = png(upFile), rect = [195, 150, 250, 60], up = (k) => { const l = []; hud.finishBanner(l, true, null, k); return render(l.map((d) => ({ ...d, rgb: [255, 255, 255] }))); };
    const eUp = error(up(0x1B), rect, ref), e42 = error(up(0x42), rect, ref);
    console.log(`PS2 runs/pipe-brake.tick7519.png time-up banner box error ${eUp.toFixed(1)} (at descriptor 0x42 ${e42.toFixed(1)})`);
    check(eUp < 14 && e42 > eUp + 8, 'the time-up banner matches runs/pipe-brake.tick7519.png'); }
}
// 4. UI atlas alpha (FE_1 / OV_1 / SU_1 palette images): the PS2 alpha has unity 0x80, so a texel's opacity is raw / 128 and
//    the PNG alpha must be min(255, 2 x raw). The first export doubled it twice (4 x raw, tools/sam_ps2 Program.cs): every
//    translucent UI texel 2x too opaque (the Uber coil, menu panels). Checked against the original SSH palettes when present.
{ const ssh = (n) => new URL(`../local/browser-ui/${n}.SSH`, import.meta.url);
  if (!fs.existsSync(ssh('OV_1'))) console.log('skip UI atlas alpha: local/browser-ui SSH files not present');
  else for (const [file, img] of [['OV_1', 4], ['OV_1', 3], ['FE_1', 11], ['FE_1', 20]]) {
    const d = fs.readFileSync(ssh(file)), off = d.readUInt32LE(20 + img * 8), size = d[off + 1] | (d[off + 2] << 8) | (d[off + 3] << 16), w = d.readUInt16LE(off + 4), h = d.readUInt16LE(off + 6);
    const sw = (i) => (i & ~0x18) | ((i & 0x08) << 1) | ((i & 0x10) >> 1), pal = off + size + 16, P = png(new URL(`${file}-${img}.png`, ui));
    let x2 = 0, x4 = 0; for (let k = 0; k < w * h; k++) { const raw = d[pal + sw(d[off + 16 + k]) * 4 + 3], a = P.d[k * 4 + 3]; if (a === Math.min(255, 2 * raw)) x2++; if (a === Math.min(255, 4 * raw)) x4++; }
    if (x2 === w * h) console.log(`ok ${file}-${img}: alpha = 2 x PS2 alpha (unity 0x80) on all ${w * h} texels`);
    else if (x4 === w * h) console.log(`PENDING ${file}-${img}: still the 4 x export; copy local/browser-validation/visual-parity/ui-alpha/new/*.png into web/public/assets/UI`);
    else check(false, `${file}-${img}: alpha matches neither rule (${x2} / ${x4} of ${w * h})`); } }
// 5. Results panel (pv luiResults, web/results-lui.js): 43final_standings laid out from its own records lands where the PS2
//    draws it (ctm-parity/runs/after-final/results.png: 'Rank' left edge 127, 'Riders' 203, the rows 20 px apart from 185, the
//    menu at 461, 'Zoe' at 203,163,122 = 0.8 x the human row colour).
{ const file = process.env.VP_RESULTS_JSON || new URL('public/assets/UI/results-screens.json', import.meta.url);   // VP_RESULTS_JSON: a staged export
  if (!fs.existsSync(file)) console.log('PENDING results panels: copy the scratch export of tools/export_results_screens.py to web/public/assets/UI/results-screens.json');
  else {
    const { LuiScreen } = await import('./lui-player.js'); const { HUMAN_ROW } = await import('./results-lui.js');
    const scr = JSON.parse(fs.readFileSync(file, 'utf8')).screens['43final_standings'], lui = new LuiScreen(scr, {}, {});
    const events = scr.events.filter((ev) => ev.frame === 30 || ev.frame === 40).map((ev) => ({ ev, start: ev.frame }));
    const by = new Map(scr.elements.map((e) => [e.name, e]));
    const abs = (name) => { const e = by.get(name), p = lui.props(e, events, 1000), up = e.parent || e.menu; const q = up && by.has(up) ? abs(up) : [0, 0]; return [q[0] + (p[0] || 0), q[1] + (p[1] || 0)]; };
    const near2 = (a, b, e = 2) => Math.abs(a[0] - b[0]) <= e && Math.abs(a[1] - b[1]) <= e;
    check(near2(abs('0007884b'), [126, 161]) && near2(abs('078fac93'), [202, 161]), `results heads at the PS2 places: Rank ${abs('0007884b')}, Riders ${abs('078fac93')}`);
    check(abs('078fac52')[1] - abs('078fac51')[1] === 20 && near2(abs('078fac51'), [202, 185]), `rider rows from y 185 every 20: ${abs('078fac51')}, ${abs('078fac52')}`);
    check(near2(abs('0263fe50'), [461, 320]), `menu first item at ${abs('0263fe50')} (PS2 461)`);
    check(HUMAN_ROW.map((v) => Math.round(v * 0.8)).every((v, k) => Math.abs(v - [203, 163, 122][k]) <= 2), 'human row colour x 0.8 = PS2 203,163,122');
    // 61toptimes / 70peakchal_results (PS2 menus/race/18-records.png: 'Rank' at 57, rows 26 px apart; allpeak/flow/out-p2r-timeup:
    // 'Event Results' at 99, 165; out-apr-results: 'PLAYER 1' at 200,108,14 = 0.8 x the record row colour)
    const all = JSON.parse(fs.readFileSync(file, 'utf8')).screens;
    if (!all['61toptimes'] || !all['70peakchal_results']) console.log('PENDING records / peak results panels: copy the newer results-screens.json export');
    else {
      const { RECORD_ROW } = await import('./results-lui.js');
      const place = (key, name, frames = [30]) => { const sc = all[key], L = new LuiScreen(sc, {}, {}), ev = sc.events.filter((e) => frames.includes(e.frame)).map((e) => ({ ev: e, start: e.frame })), by = new Map(sc.elements.map((e) => [e.name, e]));
        const a = (n) => { const e = by.get(n), p = L.props(e, ev, 1000), up = e.parent || e.menu; const q = up && by.has(up) ? a(up) : [0, 0]; return [q[0] + (p[0] || 0), q[1] + (p[1] || 0)]; }; return a(name); };
      check(near2(place('61toptimes', '0007884b'), [57, 168]) && place('61toptimes', '07483832')[1] - place('61toptimes', '07483831')[1] === 26, `records: Rank at ${place('61toptimes', '0007884b')}, rows 26 apart`);
      check(near2(place('70peakchal_results', '04b25345'), [98, 165]) && near2(place('70peakchal_results', '08feed45'), [328, 215]), `peak results: Event Results at ${place('70peakchal_results', '04b25345')}, the target label at ${place('70peakchal_results', '08feed45')}`);
      check(RECORD_ROW.map((v) => Math.round(v * 0.8)).every((v, k) => Math.abs(v - [200, 108, 14][k]) <= 2), 'record row colour x 0.8 = PS2 200,108,14');
    }
    // 40race_pre (PS2 menus/race/state-round-objectives.png: the objective at 98, 159; 'Riders' right-aligned at 243; names 23 px
    // apart; 'Zoe' 202,142,81 = 0.8 x the card's human colour)
    if (!all['40race_pre']) console.log('PENDING race card: copy the newer results-screens.json export');
    else {
      const { CARD_HUMAN } = await import('./results-lui.js');
      const sc = all['40race_pre'], L = new LuiScreen(sc, {}, {}), ev = sc.events.filter((e) => e.frame === 30).map((e) => ({ ev: e, start: e.frame })), by = new Map(sc.elements.map((e) => [e.name, e]));
      const a = (n) => { const e = by.get(n), p = L.props(e, ev, 1000), up = e.parent || e.menu; const q = up && by.has(up) ? a(up) : [0, 0]; return [q[0] + (p[0] || 0), q[1] + (p[1] || 0)]; };
      check(near2(a('01233101'), [98, 159]) && near2(a('01a8f887'), [243, 227]) && a('078fac52')[1] - a('078fac51')[1] === 23, `race card: objective ${a('01233101')}, Riders ${a('01a8f887')}, rows 23 apart`);
      check(CARD_HUMAN.map((v) => Math.round(v * 0.8)).every((v, k) => Math.abs(v - [202, 142, 81][k]) <= 2), 'card human colour x 0.8 = PS2 202,142,81');
    }
    // 62reward_list (PS2 ctm-parity/runs/race-f/final.png: 'Cash: $10,000' at x 121, rows 22 px apart) and 68rival_pre (the headline
    // at 92, 165; the first objective at 113, 215)
    const at = (key, frames, name) => { const sc = all[key], L = new LuiScreen(sc, {}, {}), ev = sc.events.filter((e) => frames.includes(e.frame)).map((e) => ({ ev: e, start: e.frame })), by = new Map(sc.elements.map((e) => [e.name, e]));
      const a = (n) => { const e = by.get(n), p = L.props(e, ev, 1000), up = e.parent || e.menu; const q = up && by.has(up) ? a(up) : [0, 0]; return [q[0] + (p[0] || 0), q[1] + (p[1] || 0)]; }; return a(name); };
    if (!all['62reward_list'] || !all['68rival_pre']) console.log('PENDING rewards / rival cards: copy the newer results-screens.json export');
    else {
      check(near2(at('62reward_list', [30, 40], '00000030'), [120, 184]) && at('62reward_list', [30, 40], '00000031')[1] - at('62reward_list', [30, 40], '00000030')[1] === 22, `rewards rows from ${at('62reward_list', [30, 40], '00000030')}, 22 apart`);
      check(near2(at('68rival_pre', [30], '00074835'), [92, 165]) && near2(at('68rival_pre', [30], '000758d1'), [113, 215]), `rival card headline ${at('68rival_pre', [30], '00074835')}, first objective ${at('68rival_pre', [30], '000758d1')}`);
    }
  } }

// ---- Round 2 (docs/visual-parity.md sections 12..17) ----
// R1. Crash recover meter 0x21D9A0 (pv recoverMeter): "RECOVER = @square" by 0x1E92A8 / 0x1E95A0 at descriptor 55 (FEFONT x 0.7), the
//     button (owner +0x4DC) and the bar at descriptor 54 (ends +0x4D4, middle +0x4D8, right end mirrored), the fill an untextured red
//     quad 0x1F1338 one layer up. The owner records come from trick-hud.json when the export carries them, else these (PS2
//     snow-jam-glide memory, owner 0x18AA570; local/browser-validation/visual-parity/round2/recover/trick-hud.sprites.json).
{ const R1 = { recoverEnd: { page: 'OV_1-4', uv: [0.552734375, 0.591796875, 0.623046875, 0.595703125] }, recoverBar: { page: 'OV_1-4', uv: [0.552734375, 0.638671875, 0.669921875, 0.595703125] },
    recoverButton: [{ page: 'OV_1-2', uv: [0.482421875, 0.134765625, 0.212890625, 0.556640625] }] };
  if (!data.sprites.recoverBar) console.log('PENDING recover meter records in trick-hud.json (round2/recover/trick-hud.merged.json): using the fixture');
  const h = new TrickHud({ ...data, sprites: { ...R1, ...data.sprites }, strings: { recover: 'RECOVER = @square', ...data.strings } }, glyphs), out = [];
  h.recoverMeter(out, 0.5, 0, true, {});
  const [label, icon, button, left, mid, right, fill] = out, tex = (d) => d.sprite.uv.map((v) => v * 256).join();
  check(label.text === 'RECOVER = ' && label.font === 'FEFONT' && near(label.x, 247.2, 1e-3) && near(label.y, 414.7, 1e-3) && near(label.scale[0], 0.7), `recover label "RECOVER = " FEFONT x 0.7 at (247.2, 414.7): (${label.x}, ${label.y})`);
  check(icon.kind === 'sprite' && near(icon.x, 371.8, 1e-3) && near(icon.y, 414.85, 1e-3) && icon.size.join() === '21,20', `label icon at (371.8, 414.85) 21 x 20: (${icon.x}, ${icon.y})`);
  check(button.x === 243 && button.y === 438 && button.size.join() === '20,20' && left.x === 266 && left.y === 442 && left.size.join() === '9,12'
    && mid.x === 275 && mid.size.join() === '113,12' && right.x === 388 && right.size.join() === '9,12', 'bar: button (243, 438) 20 x 20, ends 9 x 12 at 266 / 388, middle 113 x 12 at 275 (descriptor 54: 320, 448, 154 x 12)');
  check(tex(left) === '141.5,151.5,159.5,152.5' && tex(right) === '141.5,159.5,151.5,152.5' && tex(mid) === '141.5,163.5,171.5,152.5', 'bar UV rows: the left end u 151.5..159.5, the right end mirrored, the middle u 163.5..171.5');
  check(fill.kind === 'quad' && fill.x === 269 && fill.y === 445 && fill.size.join() === '62.5,6' && fill.argb.join() === '1,1,0,0' && fill.order === button.order + 1, 'fill 0.5: red quad (269, 445) 62.5 x 6, one layer above the bar');
  const faded = []; h.recoverMeter(faded, 1, 0.4, false, {}); check(faded.every((d) => near(d.argb[0], 0.5, 1e-3)) && !faded.some((d) => d.kind === 'text'), 'RECOVERED! at ratio 0.4: no label, everything at half alpha');
  }
// R2. Freestyle standings 0x1ED104 (pv hudStandings): descriptor 0x17 (20, 20), rows 21 apart, the value column at x + 66.997 right-aligned
//     on the widest row, 'ST' / 'ND' / 'RD' at half scale 1.6 px down; PS2 pipe-brake tick 2018: 162880 / 98220 / 44820.
{ const out = []; hud.standings(out, hud.standingsRows([162880, 98220, 44820], 0, 'Zoe')); const at = (t) => out.find((d) => d.text === t);
  check(near(at('162880').x, 86.99718) && near(at('98220').x, 97.99718) && at('162880').y === 20 && at('98220').y === 41 && at('44820').y === 62, 'standings values right-aligned on the widest row at x + 66.997, rows 21 apart');
  check(at('1').x === 20 && at('ST').x === 30 && at('ND').x === 40 && near(at('ST').y, 21.5995788) && at('ST').scale[0] === 0.5 && at('ST').font === 'HUDFONT', "rank digits at 20, 'ST' at 30 / 'ND' at 40, half scale, 1.6 px down");
  const mine = []; hud.standings(mine, hud.standingsRows([162880, 98220, 44820], 100000, 'Zoe'));
  check(mine.some((d) => d.text === 'Zoe' && d.y === 41) && mine.some((d) => d.kind === 'rect' && d.y === 41 && d.argb.join() === '1,1,0,0') && mine.some((d) => d.text === '98220' && d.y === 62), "the player's row (score >= the posted one) takes the place once: its name over the red 0x4C8628 box");
  // PS2 menus/transport-map/heats/h2-scorepoke sample00303 (the human's score poked): 'ZOE 402000' over the red box, pixels x 20..205.
  const top = []; hud.standings(top, hud.standingsRows([328320, 186960, 1000], 402000, 'Zoe')); const box = top.find((d) => d.kind === 'rect');
  check(box && box.x === 20 && box.y === 20 && Math.round(box.x + box.w) === 206 && box.h === 21 && top.some((d) => d.text === 'Zoe' && d.x === 20 && d.y === 20) && top.some((d) => d.text === '328320' && d.y === 41), "the player's first row: the red box over x 20..205 as on the PS2 (h2-scorepoke 'ZOE 402000')");
  }
// R3. OV_darkblue header lights (pv luiLights): the six lights at y 73 (groups 06c3ae11 (433, 57) / 06c3ae12 (449, 63) + their shapes),
//     the light loop 65..500 started at panel frame 50 (436 frames a turn).
{ const { panelTimeline, LIGHT_SHAPES, LIGHT_GROUPS } = await import('./results-lui.js'); const { LuiScreen } = await import('./lui-player.js');
  const all = fs.existsSync(new URL('results-screens.json', ui)) ? JSON.parse(fs.readFileSync(new URL('results-screens.json', ui))).screens : null;
  if (!all?.OV_darkblue) console.log('skip header lights: results-screens.json not present');
  else {
    const sc = all.OV_darkblue, L = new LuiScreen(sc, {}, {}), by = new Map(sc.elements.map((e) => [e.name, e])), t = 300, ev = panelTimeline(sc.events, t);
    const pos = (n) => { const e = by.get(n), p = { ...L.props(e, ev, t), ...(sc.state0 === 'byName' ? {} : LIGHT_GROUPS[n] || {}) }, up = e.parent || e.menu; const q = up && by.has(up) ? pos(up) : [0, 0]; return [q[0] + (p[0] || 0), q[1] + (p[1] || 0)]; };
    const at = LIGHT_SHAPES.map(pos);
    check(at.map((p) => p.join()).join(' ') === '446,73 461,73 498,73 513,73 549,73 564,73', `header lights at ${at.map((p) => p.join()).join(' ')} (PS2 savestates: 446 / 461 / 498 / 513 / 549 / 564, y 73)`);
    const starts = (tt) => panelTimeline(sc.events, tt).filter((x) => x.ev.frame === 65).map((x) => x.start);
    check(starts(49).length === 0 && starts(50).every((s) => s === 50) && starts(486).every((s) => s === 486), 'the lights loop starts at panel frame 50 and turns every 436 frames');
  }
  }
// R4. "Cash: $ 10,000" (pv cashGap): 0x198AF0 puts '$ ' before the digits (PS2 race-f final: '$' -> '1' 11.4 px).
{ const { money } = await import('./trick-hud.js'); const src = sourceOf('career-ui.js');
  check(money(10000) === '$ 10,000' && /'Cash: %S'\),money\(rec\.cash\)/.test(src), 'rewards cash line "$ 10,000" (0x198AF0)'); }
// R5. Air streamers 2EF950 (pv streamers): the GS samples strm with its bright rows at T = 1: over the ribbon's T = scroll + k / count
//     (to ~1.8, clamped) the flipped texture is bright for any scroll, the exported order dark past T = 0.75 (PS2 setpieces-bra2 418).
{ const fxDir = new URL('public/assets/FX/', import.meta.url);
  if (!fs.existsSync(new URL('strm.gs.rgba', fxDir))) console.log('skip streamer texture: FX export not present');
  else {
    const b = fs.readFileSync(new URL('strm.gs.rgba', fxDir)), w = 32, h = b.length / 128, rowG = (y) => { let s = 0; for (let x = 0; x < w; x++) s += b[(y * w + x) * 4 + 1]; return s / w; };
    const at = (T, flip) => { const y = Math.min(h - 1, Math.max(0, Math.floor(T * h))); return rowG(flip ? h - 1 - y : y); };
    const mean = (flip, s) => { let m = 0; for (let k = 0; k < 25; k++) m += at(s + k / 25, flip); return m / 25; };
    check([0.35, 0.77].every((s) => mean(true, s) > 100 && mean(false, s) < 40), `strm over T = s..s+1: flipped ${mean(true, 0.77).toFixed(0)} / exported ${mean(false, 0.77).toFixed(0)} (s 0.77)`);
    check(/tag==='strm'\)\{/.test(sourceOf('boost-renderer.js')), 'boost-renderer flips strm');
  } }
// R6. Results menu (pv resultsMenu): a LUI text wraps only when its element has flag 0x80 (0x3A0528 -> 0x3A0D00 / 0x3A0EB0), so the
//     43final_standings items (flags 0x24c, 80 px at 50 %) keep 'Next event' / 'Final Round' on one line (PS2 bhp1-neutral:
//     'Next event' at x 462..546); a Single Event opens on Restart (0x1E7558: 39B960(menu, 1) when 0x535C11 == 1), the career on item 0.
{ const all = fs.existsSync(new URL('results-screens.json', ui)) ? JSON.parse(fs.readFileSync(new URL('results-screens.json', ui))).screens : null;
  if (!all?.['43final_standings']) console.log('skip results menu: results-screens.json not present');
  else {
    const els = all['43final_standings'].elements, items = ['0263fe50', '0263fe51', '0263fe52', '0263fe53', '0263fe54'].map((n) => els.find((e) => e.name === n));
    const wrapped = els.filter((e) => e.kind === 'text' && e.flags & 0x80).map((e) => e.name);
    check(items.every((e) => e && !(e.flags & 0x80)) && wrapped.includes('0c37a134'), `menu items without flag 0x80 (${items.map((e) => e?.flags?.toString(16)).join(' ')}), helptext wraps`);
  }
  const lui = fs.readFileSync(new URL('lui-player.js', import.meta.url), 'utf8'), cu = sourceOf('career-ui.js');
  check(/!this\.flagWrap \|\| !!\(e\.flags & 0x80\)/.test(lui) && /resultsFocus\(was\)\{return this\.peakResults\(\)\|\|this\.active\?\.career\?0:1;\}/.test(cu), 'flagWrap wraps flag-0x80 texts only; resultsFocus: Single Event -> Restart (1), career / peak runs -> 0');
  // Up / Down (0x39AB50 / 0x39AC48): greyed items (flags 0x20) skipped, round the ends when the menu element has 0x80
  if (all) { const wraps = ['43final_standings', '70peakchal_results', '61toptimes'].map((k) => all[k]?.elements.find((e) => e.kind === 'menu')?.flags ?? 0);
    check(wraps.every((f) => f & 0x80) && /\(s==='ctm-results'\|\|s==='ctm-records'\)&&\(e\.code==='ArrowUp'\|\|e\.code==='ArrowDown'\)\)/.test(cu), `results / records menus wrap (menu flags ${wraps.map((f) => f.toString(16)).join(' ')} have 0x80), career-ui key() wraps them`); }
  }
// R7. Uber hint layout (pv uberLayout): the 0x1E92A8 record (PS2 owner +0x560: W 209.99992, pieces 144.9 / 28 / 16.1 / 21) drawn by
//     0x1E95A0 at descriptor 73: the text from x 215 (the port's own measure put it at 212).
{ const out = []; hud.iconLabel(out, hud.iconLayout('UBER TRICK = @r1 + @square', Math.fround(0.7)), 73, {}, { hint: true });
  const [t0, i0, t1, i1] = out;
  check(near(hud.iconLayout('UBER TRICK = @r1 + @square', Math.fround(0.7)).W, 209.99992, 1e-3) && near(t0.x, 215, 1e-3) && near(i0.x, 359.9, 1e-3) && near(t1.x, 387.9, 1e-3) && near(i1.x, 404, 1e-3) && near(i1.y + 20, 459.85, 1e-2),
    `uber hint: text at 215, icons at 359.9 / 404 (record W 209.99992): ${out.map((d) => d.x.toFixed(2)).join(' ')}`);
  }
// R8. The static-model additive class (model flag 8, ALPHA 0x48) writes no depth (ZBUF ZMSK 1 in the PS2 render queue, metro-event-race
//     tick 318; TEST 0x5114d): it draws as one additive pass with depthWrite off, so the cones' back faces add too.
{ const src = sourceOf('world-material.js');
  check(/if\(blend===3\)\{m\.alphaTest=0;m\.depthWrite=false;cache\.set\(key,m\);return m;\}/.test(src), 'additive static models: one pass, no depth write'); }
// R9. LUI word wrap (pv luiWrap, web/lui-player.js ps2Wrap): 0x3A0D00 breaks a flag-0x80 text where the font's advances x the scale
//     +0x50 pass the width +0x60, both in PS2 units. 68rival_pre: Nate (652 x 0.73 = 476.0 > 475) and Psymon wrap after 'Rival', Griff
//     (464.3) stays on one line, the 60 % bullet (445.2 <= 450) never wraps (PS2 local/reference/pcsx2 ruthless / the-throne / happiness-mac-ready).
{ const { LuiScreen } = await import('./lui-player.js');
  const all = fs.existsSync(new URL('results-screens.json', ui)) ? JSON.parse(fs.readFileSync(new URL('results-screens.json', ui))).screens : null;
  const els = all?.['68rival_pre']?.elements, head = els?.find((e) => e.name === '00074835'), obj = els?.find((e) => e.name === '000758d1');
  if (!head || !obj) console.log('skip rival card wrap: results-screens.json not present');
  else { const lines = [], lui = new LuiScreen({ elements: [] }, {}, { fonts: glyphs, text: (c, ln) => lines.push(ln) }); lui.flagWrap = true; lui.ps2Wrap = true;
    const c = { save() {}, restore() {}, translate() {}, scale() {} }, wrap = (text, p) => { lines.length = 0; lui.text(c, text, 0, 0, p, 1, true); return lines.join('|'); };
    const face = (n) => `Face off against ${n} in a Rival Challenge!`, bullet = 'The first rider to the bottom of Backcountry wins.';
    check(wrap(face('Nate'), head.props) === 'Face off against Nate in a Rival|Challenge!' && wrap(face('Psymon'), head.props) === 'Face off against Psymon in a Rival|Challenge!'
      && wrap(face('Griff'), head.props) === face('Griff') && wrap(bullet, obj.props) === bullet && !!(head.flags & 0x80),
      "rival card: 'Face off against Nate in a Rival' / 'Challenge!' where the PS2 wraps it, Griff and the bullet on one line (luiWrap on)"); } }
// R10. The CTM plane drop (pv dropCamera, web/plane-drop.js): the rider lies head first (+0x120, forward +0x1B0 68 degrees down) and the
//      camera is 10 ticks into the lock of the drop's set-target 0x176FE0 (lockView = forward x 250), looking down the slope. On Happiness
//      with a neutral pad, the rider and camera must then follow the PS2 (menus/fr/ctmstart f02887 / f02935 = ticks 187 / 235) within 1 cm;
//      the old level start (heading only, the camera begun at the horizon) is 9..20 cm off.
{ const { PLANE_DROP } = await import('./plane-drop.js'), { createTestRider } = await import('./net/test-core.mjs'), { PEAK_STARTS } = await import('./free-ride.js');
  const drop = PEAK_STARTS[14], run = async (on) => {
    const r = await createTestRider({ course: 'ABC1' }), c = r.core, [x, y, z] = drop.position, [hx, hy] = drop.heading;
    c._reset_animation(); c._reset_race(); c._reset_rider(x / 100, z / 100, -y / 100, Math.atan2(hx, -hy)); c._reset_pad_history();
    if (on) new Uint32Array(c.HEAPU8.buffer, c._rider_orientation(), 4).set(PLANE_DROP.quaternion);
    c._set_rider_velocity(...drop.velocity);
    if (on) { const w = c._malloc(271 * 4), k = c._malloc(271); new Uint32Array(c.HEAPU8.buffer, w, 271).set(PLANE_DROP.camera); c.HEAPU8.fill(1, k, k + 271); c._camera_seed_words(w, k); c._free(w); c._free(k); }
    const at = {}; for (let tick = 1; tick <= 235; tick++) { r.tick(new Float32Array(24)); if (tick === 187 || tick === 235) {
      const w = new Uint32Array(c.HEAPU8.buffer, c._camera_state_words(), 271), fv = (o) => [0, 1, 2].map((i) => new Float32Array(new Uint32Array([w[o / 4 + i]]).buffer)[0]);
      at[tick] = { rider: Array.from(new Float32Array(c.HEAPF32.buffer, c._reference_motion(), 3)), eye: fv(0x40), look: fv(0x20) }; } }
    return at; };
  const PS2 = { 187: { rider: [-30993.9, 28388.9, -192657.8], eye: [-30962, 28369.9, -192204.3], look: [-31048, 28366.3, -192599.6] },
    235: { rider: [-31701.5, 28295.6, -194093.5], eye: [-31640.7, 28293.5, -193639.6], look: [-31753.6, 28283.8, -194028.5] } };
  const err = (at) => Math.max(...[187, 235].flatMap((k) => ['rider', 'eye', 'look'].map((n) => Math.hypot(...at[k][n].map((v, i) => v - PS2[k][n][i])))));
  const on = err(await run(true)), off = err(await run(false));
  check(on < 1 && off > 5 && drop.drop === true && PLANE_DROP.camera.length === 271, `plane drop: rider and camera within ${on.toFixed(2)} cm of the PS2 at ticks 187 / 235 (level start: ${off.toFixed(1)} cm)`);
  const main = sourceOf('main.js');
  check(/const drop=!!spawn\.drop;/.test(main) && /core\._camera_seed_words\(w,k\)/.test(main), 'plane drop: main.js resetPhysics sets the orientation before set_rider_velocity and seeds the camera');
  const state = new URL('../local/ps2-capture/menus/fr/ctmstart.f02700.p2s', import.meta.url);
  if (fs.existsSync(state)) { const { execFileSync } = await import('node:child_process');
    const py = `import sys,struct,zipfile,re\nsys.path.insert(0,'../tools')\nimport ps2_capture as pc\nfrom locations import human_rider\nm=zipfile.ZipFile('../local/ps2-capture/menus/fr/ctmstart.f02700.p2s').read('eeMemory.bin');d=pc.discover(m);r=human_rider(m)\noffs=[int(x,16) for x in re.search(r'compositorOffsets\\{([^}]*)\\}',open('../engine/original_camera_words.hpp').read()).group(1).split(',')]\nu=lambda a:struct.unpack_from('<I',m,a)[0]\nprint([u(r+0x120+4*i) for i in range(4)]+[u(d['camera']+4*i) for i in range(228)]+[u(d['outer']+k) for k in offs])`;
    const words = JSON.parse(execFileSync('python3', ['-c', py], { encoding: 'utf8', cwd: new URL('.', import.meta.url).pathname }));
    check(words.join() === [...PLANE_DROP.quaternion, ...PLANE_DROP.camera].join(), 'plane-drop.js equals the PS2 ctmstart f02700 rider +0x120 and camera words'); }
  else console.log('skip plane-drop.js vs PS2: local capture not present'); }
// R11. The plane drop's pose and air streamers (pv dropPose, dropStreamer; web/plane-drop.js animation / streamer): the PS2 rider is in the
//      air controller with its air clip 287 10 ticks in and the placement's ground-control triplets at 0 (core drop_air_seed), and its
//      streamer ring has scroll 0.81 and 4 rows (core rider_fx_streamer_seed). Tick 47 (PS2 f02747): the skeleton relative to the rider
//      and the camera within 1 cm; the ribbon's T starts at the PS2's scroll (0.872) and runs 25 rows (the level start: 23 from 0.065).
{ const { PLANE_DROP } = await import('./plane-drop.js'), { createTestRider } = await import('./net/test-core.mjs'), { PEAK_STARTS } = await import('./free-ride.js');
  const drop = PEAK_STARTS[14], r = await createTestRider({ course: 'ABC1' }), c = r.core, [x, y, z] = drop.position, [hx, hy] = drop.heading;
  c._set_rider_fx_render_scale(1); c._reset_animation(); c._reset_race(); c._reset_rider(x / 100, z / 100, -y / 100, Math.atan2(hx, -hy)); c._reset_pad_history();
  new Uint32Array(c.HEAPU8.buffer, c._rider_orientation(), 4).set(PLANE_DROP.quaternion); c._set_rider_velocity(...drop.velocity);
  { const b = new TextEncoder().encode(PLANE_DROP.animation + '\0'), p = c._malloc(b.length); c.HEAPU8.set(b, p); c._drop_air_seed(p); c._free(p); }
  { const w = c._malloc(271 * 4), k = c._malloc(271); new Uint32Array(c.HEAPU8.buffer, w, 271).set(PLANE_DROP.camera); c.HEAPU8.fill(1, k, k + 271); c._camera_seed_words(w, k); c._free(w); c._free(k); }
  { const p = c._malloc(PLANE_DROP.streamer.length * 4); c.HEAPF32.set(PLANE_DROP.streamer, p >> 2); c._rider_fx_streamer_seed(p); c._free(p); }
  for (let tick = 1; tick <= 47; tick++) r.tick(new Float32Array(24));
  const pos = Array.from(new Float32Array(c.HEAPF32.buffer, c._reference_motion(), 3)), bp = c._world_pose_bones(), bn = new Float32Array(c.HEAPF32.buffer, bp, 1)[0], bones = new Float32Array(c.HEAPF32.buffer, bp + 4, bn * 7);
  const names = JSON.parse(fs.readFileSync(new URL('public/assets/RIDER_ZOE/rider.json', import.meta.url))).bones.map((b) => b.name);
  const PS2 = { hips: [-82.6, -24.51, 33.41], head: [-102.56, 8.99, 38.34], handleft: [-100.21, -1.77, -12.3], footright: [-30.7, -10.11, 38.81] };   // f02747, relative to the rider
  const boneErr = Math.max(...Object.entries(PS2).map(([n, p]) => { const i = names.indexOf(n); return Math.hypot(...p.map((v, j) => bones[i * 7 + j] - pos[j] - v)); }));
  const w = new Uint32Array(c.HEAPU8.buffer, c._camera_state_words(), 271), fv = (o) => [0, 1, 2].map((i) => new Float32Array(new Uint32Array([w[o / 4 + i]]).buffer)[0]);
  const camErr = Math.max(Math.hypot(...fv(0x40).map((v, i) => v - [-29729.9, 28504.3, -186744.4][i])), Math.hypot(...fv(0x20).map((v, i) => v - [-29802.3, 28511.9, -187095.8][i])));
  const info = new Float32Array(c.HEAPF32.buffer, c._rider_fx_info(), 8), n = info[6], v = new Float32Array(c.HEAPF32.buffer, c._rider_fx_vertices(4), n * 9);
  const ts = [...new Set(Array.from({ length: n }, (_, i) => +v[i * 9 + 4].toFixed(3)))].sort((a, b) => a - b);
  check(boneErr < 0.5 && camErr < 1 && Math.abs(ts[0] - 0.872) < 0.01 && ts.length === 25,
    `plane drop pose: skeleton ${boneErr.toFixed(2)} cm and camera ${camErr.toFixed(2)} cm from the PS2 at f02747; streamer ${ts.length} rows from T ${ts[0]}`);
  const main = sourceOf('main.js');
  check(/if\(drop&&core\._drop_air_seed/.test(main) && /if\(drop&&core\._rider_fx_streamer_seed/.test(main), 'plane drop pose: main.js seeds the air controller and the streamers'); }
// R12. A LiveComp owner its Object player draws (pv liveCompObject): The Throne's summit flag pole (resource 282410) has runtime flags
//      0x210225 (the static collector skips it: flags & 3 != 3) and a type-1 Object entity (vtable 0x490B10) whose 0x356298 draws it
//      (flags & 4) while the player lives: PS2 the-throne-ready shows it at the card's left (projected x 162 -> 96, y 291 -> 66).
//      tools/export_livecomp.py classes it draw 'object'; web/set-pieces-renderer.js adds its hidden batch while the player runs.
{ const audit = new URL('../local/event-activation/EBC3/countdown-instances.json', import.meta.url);
  if (fs.existsSync(audit)) { const { execFileSync } = await import('node:child_process');
    const cls = execFileSync('python3', ['-c', "import sys,json\nsys.path.insert(0,'../tools')\nfrom export_livecomp import draw_class\na={x['resource']:x for x in json.load(open('../local/event-activation/EBC3/countdown-instances.json'))['instances']}\nprint(json.dumps([draw_class(a[282410],'mdl_EBC3_summit_flag_pole_1000'),draw_class(a[282410],'mdl_EBC3_os609_full_version_inair')]))"], { encoding: 'utf8', cwd: new URL('.', import.meta.url).pathname });
    check(JSON.parse(cls).join() === 'object,none', "LiveComp draw class: The Throne's flag pole is 'object' (its Object player draws it), the os609 heli keeps 'none'"); }
  else console.log('skip LiveComp draw class: countdown audit not present');
  const src = fs.readFileSync(new URL('set-pieces-renderer.js', import.meta.url), 'utf8');
  check(/\(live\.byResource\.get\(resource\)\?\.draw === 'object' \|\| heli\)/.test(src), 'set-pieces-renderer draws an object-class LiveComp while its player runs');
  const lc = JSON.parse(fs.readFileSync(new URL('public/assets/EBC3/LIVECOMP/livecomp.json', import.meta.url))).instances.find((x) => x.resource === 282410);
  check(lc?.draw === 'object', "EBC3 livecomp.json: the summit flag pole is drawn by its Object player (draw 'object')"); }
// R13. The rival card's rider (pv readyLight, web/main.js readyView): the PS2 draws it shaded with the environment irradiance its load
//      frames settled at the start spot (the-throne-ready: row 0 (0.220, 0.307, 0.508), 0.95 of the dark bank EPDK1), so it reads dark
//      through the panel; the port drew the unlit menu fallback (texels doubled). Core environment_settle at The Throne's event start.
{ const { createTestRider } = await import('./net/test-core.mjs'), r = await createTestRider({ course: 'EBC3', lighting: true }), c = r.core; r.startEvent();
  const before = Array.from(new Float32Array(c.HEAPF32.buffer, c._environment_irradiance(), 3)); c._environment_settle(60);
  const row0 = Array.from(new Float32Array(c.HEAPF32.buffer, c._environment_irradiance(), 3)), ps2 = [0.220, 0.307, 0.508];
  const main = sourceOf('main.js');
  check(before.every((v) => v === 0) && row0.every((v, i) => Math.abs(v - ps2[i]) < 0.02) && /if\(riderLightingUpdate&&sam\?\.userData\.sourceLighting\)/.test(main) && /core\._environment_settle\?\.\(60\)/.test(main)
    && /\|\|readyScreen\(\)\),renderAlpha/.test(main),
    `rival card rider: the environment settled at the start spot, row 0 ${row0.map((v) => v.toFixed(3)).join(', ')} (PS2 0.220, 0.307, 0.508)`); }
// R14. The switch-stance 'S' (pv switchIcon): 0x1EC3F8 draws owner +0x470's sprite at descriptor 0x4F under the meter when HUD flags
//      0x10000000 (the race and the free-ride HUDs both have it), alpha x 0.2 while rider +0x320 == +0x324. PS2: faint on every regular
//      frame (peak1-green-start 103, Snow Jam 418), full orange riding switch (Snow Jam 4419, CRA3 1218); icon box error 12.2 -> 6.3,
//      8.5 -> 4.1, 73.6 -> 19.8, 55.9 -> 17.3.
{ const h = new TrickHud(data, glyphs);
  const reg = [], sw = []; h.switchIcon(reg, true); h.switchIcon(sw, false); const d = reg[0];
  const ui = sourceOf('ui.js'), main = sourceOf('main.js');
  check(d && d.x === 571 && d.y === 397 && d.size.join() === '24,22' && near(d.argb[0], 0.2) && sw[0].argb[0] === 1 && d.sprite.page === 'OV_1-4'
    && (data.flags & 0x10000000) !== 0 && (0x1530C380 & 0x10000000) !== 0 && /if\(flags&0x10000000\)\{const l=\[\];this\.trickHud\.switchIcon\(l,s\.stanceRegular!==false\)/.test(ui)
    && /stanceRegular:core\?\._rider_stance_info/.test(main), "switch-stance 'S': descriptor 0x4F (571, 397, 24 x 22), alpha 0.2 regular / 1 switch, race and free-ride flags ");
  check(data.sprites.switchIcon?.owner_offset === '0x470' && data.sprites.switchIcon.page === 'OV_1-4', "trick-hud.json: the switch icon is owner +0x470's OV_1-4 sprite"); }
// R15. A world load's ride start (pv painterWorldLoad: web/free-ride.js placeRegion -> core environment_world_load). The PS2 places
//      the new rider during the load and the load's painter steps run in the stale region gp+0x770, whose record is not loaded:
//      Fog / Sun / ScreenTint / glare start at their class defaults (+0 = 0) and the location's record blends in at its rate.
//      PS2 peak1-green-start (the lodge's Return to Game, painters watched): Sun A elevation / azimuth 6.7397 / 79.6394 at
//      record 101 and 9.1679 / 108.3325 at 201, Fog A density 0.26807 at 101 (0 -> 1.2 at 0.25 %/tick). The port jumped to A's
//      values at tick 1: the sun shone beside the Green Station sign at t203, where the PS2's is still off screen. Frame MAD
//      14.4 / 17.7 / 24.4 -> 11.7 / 12.1 / 21.2 at t103 / 203 / 306 (Chrome; WebKit 14.4 / 17.7 / 24.4 -> 11.8 / 12.1 / 21.3).
{ const { createSunPainter } = await import('./sun-flare.js');
  const pkg = JSON.parse(fs.readFileSync(new URL('public/assets/SUN_FLARE/sun-flare.json', import.meta.url)));
  const A = JSON.parse(fs.readFileSync(new URL('public/assets/PEAK1/A/sun-painter.json', import.meta.url)));
  const run = (loads) => { const info = new Float32Array(11), p = createSunPainter(pkg), out = {}; p.setTree(A.painter);
    const core = { HEAPF32: info, _fog_info: () => 0, _environment_world_loads: () => loads }; info[8] = -65541.83; info[9] = 33258.8; info[10] = 1;
    for (let t = 1; t <= 201; t++) { info[7] = t; p.tick(core); if (t === 101 || t === 201) out[t] = p.values; }
    return out; };
  const on = run(1), off = run(0);
  check(near(on[101].elevation, 6.739697, 1e-4) && near(on[101].azimuth, 79.63936, 1e-3) && near(on[201].elevation, 9.167935, 1e-4) && near(on[201].azimuth, 108.3325, 1e-3)
    && off[101].azimuth === Math.fround(124.9), `world load: Sun A blends in from the class defaults (az ${on[101].azimuth.toFixed(3)} / ${on[201].azimuth.toFixed(3)} at 101 / 201; PS2 79.639 / 108.332)`);
  const fr = fs.readFileSync(new URL('free-ride.js', import.meta.url), 'utf8');
  check(/core\._place_rider_region\([^\n]*\n[^\n]*\n[^\n]*\n\s*if \(fresh\) core\._environment_world_load\?\.\(\);/.test(fr), 'free-ride placeRegion: the world load reset after the new rider\'s placement (its 0x2C03E8)');
  const { createTestRider } = await import('./net/test-core.mjs'), fog = async (load) => { const r = await createTestRider({ course: 'ARA1' }), c = r.core;
    if (!c._environment_world_load) return null;
    const b = Buffer.from(fs.readFileSync(new URL('public/assets/ARA1/fog-tree.json', import.meta.url)) + '\0'), p = c._malloc(b.length); c.HEAPU8.set(b, p); c._init_fog(p); c._free(p);
    c._reset_fog(); r.startEvent(); if (load) c._environment_world_load(); r.tick(new Float32Array(24)); return Array.from(new Float32Array(c.HEAPF32.buffer, c._fog_info(), 11)); };
  const f0 = await fog(false), f1 = await fog(true);
  check(f0 && f0[0] === 2 && f0[2] === 10000 && near(f1[0], 0.02, 1e-6) && near(f1[2], 29800, 0.01) && f1[6] === 0, `world load: core Fog blends from the class defaults (density ${f1[0].toFixed(4)}, far ${f1[2].toFixed(1)} after one tick; jump: ${f0[0]}, ${f0[2]})`); }
// R16. Terrain / world texture LOD K (analysed, not drawn: docs/visual-parity.md section 28). The streamer 0x37CA30 sets a world
//      texture's TEX1 K from its SSB kind-9 header s16 +8 (v): K = trunc(-16 log2(240 / (v / 16384))), clamped to -2047 .. -135,
//      v = 0 keeping the default -185 (0x367880); level = log2(depth cm) + K / 16. The live strm_tex descriptors of 8 PS2 states
//      (334 textures) all equal it; these are a few of them (v -> K).
{ const { execFileSync } = await import('node:child_process');
  let ks = null; try { ks = JSON.parse(execFileSync('python3', ['-c', 'import sys,json\nsys.path.insert(0,"../tools")\nfrom export_world_texture_lod import lod_k\nprint(json.dumps([lod_k(v) for v in (0,60,260,1727,2011,20698,32767)]))'], { cwd: new URL('.', import.meta.url).pathname, encoding: 'utf8' })); } catch {}
  if (!ks) console.log('skip world texture LOD K: python3 not available');
  else check(ks.join() === '-185,-256,-222,-178,-174,-135,-135', `world texture LOD K from the header (v 0 / 60 / 260 / 1727 / 2011 / 20698 / 32767 -> ${ks.join(' / ')}, PS2 strm_tex descriptors)`); }
// R17. Hints after Peak 1 (pv hudHints): 0x1EBA10 in the HUD prepass sets the hints bit 0x1000000 in the player's clear mask (HUD owner
//      +0x48 +0x80) when the rider's profile visited mask (+0xACC, 145E68 -> 1523E8) has a Peak 2 (0x45A7C4: 0x18924C) or Peak 3
//      (0x45A7C8: 0x21A490) course bit: no "RECOVER = button" label and no Uber hint. PS2 free-ride states fr-dbc2 2304 / 702,
//      fr-throne-tuck 4302, frc-1800, fr-era5-rode: clear mask 0x01000000; Peak 1 (fr-aara1-glide, fr-no-d, ctmstart, green-ride): 0.
//      PS2 frames fr-dbc2 2304 and fr-throne-neutral 2302 draw the recover bar without its label.
{ const { CareerScreens } = await import('./career-ui.js');
  const off = (visited) => CareerScreens.prototype.hintsOff.call({ visitedMask: CareerScreens.prototype.visitedMask }, { visited });
  const slots = Array(0x40).fill(null); slots[0] = { type: 0xB, value: 1, maximum: 2, points: 0 };
  const label = (flags) => new TrickHud(data, glyphs).frame(slots, { flags }).some((d) => d.kind === 'text' && /RECOVER/.test(d.text));
  const ui = sourceOf('ui.js');
  check(!off(1 << 17) && !off((1 << 14) | (1 << 8)) && off(1 << 15) && off(1 << 19) && off(1 << 21) && label(0x1530C380) && !label(0x1530C380 & ~0x1000000)
    && /hintsOff=\(fr\|\|this\.careerMode\)&&!!this\.careerUI\?\.hintsOff\?\.\(\)/.test(ui),
    'hints off after a Peak 2 / 3 visit: Peak 1 masks keep the RECOVER label, a Peak 2 / 3 bit removes it'); }
// R18. Single Event pause (pv singlePause): the MCOMM PDA with Return / Restart / Audio / Options / Quit, no Messages / Give Up (PS2
//      menus/single/11-single-pause, r3-pause-rows): help 'Get back to boarding.' / 'Restart the current competition.' / 'Change music
//      tracks.' / 'Modify game and sound options.' / 'Quit out to Title screen.'; Restart 'Are you sure?' (No) -> the round's card
//      (r3-pause-restartyes); Quit 'Quit Game' (No) -> Yes -> the title (r3-pause-quityes). The career pause keeps its six rows.
//      pv menuRiders: the computer / online riders hide while a menu covers the world (they drew over the pause).
{ const { CareerScreens } = await import('./career-ui.js');
  const items = (career) => CareerScreens.prototype.pauseItems.call({ active: { career }, t: (k, f) => f, singlePause: CareerScreens.prototype.singlePause });
  const cu = sourceOf('career-ui.js'), ui = sourceOf('ui.js');
  const main = sourceOf('main.js'), opp = fs.readFileSync(new URL('opponent-riders.js', import.meta.url), 'utf8');
  check(items(false).join() === 'Return,Restart,Audio,Options,Quit' && items(true).join() === 'Return,Restart,Messages,Audio,Options,Give Up'
    && /this\.singlePause\(\)\?\[this\.t\('kT_OVRHELPGetBoarding'\),this\.t\('kT_OVRHELPRestartComp'\),this\.t\('kT_OVRHELPChangeMusic'\),this\.t\('kT_OVRHELPOptions'\),this\.t\('kT_MAPHELPQuitGame'\)\]/.test(cu)
    && /case 'ctm-quit':if\(i===0&&this\.quitFrom==='ctm-pause'\)\{this\.quitToTitle\(\);return;\}/.test(cu) && /else if\(i===4\)\{this\.quitFrom='ctm-pause';this\.go\('ctm-quit',1\);\}/.test(cu)
    && /if\(this\.screen==='pause'&&this\.pdaPause\(\)\)/.test(ui),
    'Single Event pause: Return / Restart / Audio / Options / Quit with the PS2 help lines, Quit Game -> title; career pause unchanged (singlePause on)');
  check(/entry\.group\.userData\.opponentRider=true;scene\.add\(entry\.group\);/.test(opp)
    && /if\(!playing\)for\(const o of scene\.children\)if\(o\.userData\.opponentRider\)o\.visible=false;/.test(main),
    'computer / online riders hidden under a menu'); }
// R19. Transport arrival fade (pv arrivalFade): the PS2 world comes in from black over ~30 ticks after the arrival placement, the HUD
//      drawn over it (watched re-capture of menus/fr-courses/aba1-screen10: the fade's opaque first frames reset the painters, weight
//      -99999 at tick 2020; snow luma 6 % / 40 % / 84 % / 100 % at +1 / +11 / +24 / +29). main.js starts cutscenes.fadeFrom({ticks: 30,
//      colour: 'black', hud: true}) after a course arrival (not a station or backcountry, whose cuts fade themselves); ui.js draws that
//      overlay first and the HUD over it.
{ const main = sourceOf('main.js'), ui = sourceOf('ui.js'), cs = sourceOf('cutscenes.js');
  check(/else if\(s&&\(dest<14\|\|dest>=17\)\)cutscenes\?\.fadeFrom\?\.\(\{ticks:30,colour:'black',hud:true(,bars:pv\('transportFade'\))?\}\)/.test(main)
    && /if\(this\.cutscene\?\.overlayUnderHud\)this\.cutscene\.draw\(c,this\);/.test(ui) && /if\(this\.cutscene\?\.overlay&&!this\.cutscene\.overlayUnderHud\)\{/.test(ui)
    && /get overlayUnderHud\(\)\{return!!overlay\?\.hud;\}/.test(cs),
    'transport arrival: 30-tick fade from black under the HUD'); }
// R20. Generic scene fog (pv genericFogOff): with the original fog pass, three's linear scene fog is detached after the course load. The
//      load's traverse cleared m.fog only on the meshes present then; every material made later (streamed free-ride locations after a
//      transport or on the way down, static cells, set pieces) was fogged by it to the painter's colour at its far, on top of the PS2
//      fog (ABA1 arrival far 80 m: near-white trees and signs; the PS2 CLUT there peaks at alpha 104 / 128, 19 %).
{ const main = sourceOf('main.js');
  check(/if\(fogRenderer\)\{scene\.userData\.genericFog=scene\.fog;scene\.fog=null;\}/.test(main)
    && /const gf=scene\.fog\|\|scene\.userData\.genericFog;if\(gf&&fogValues\[10\]&&fogValues\[7\]>0\)/.test(main),
    'generic scene fog detached under the original fog pass'); }
// R21. In-game Options (pv pdaOptions): the PDA page OV.LUI 37beoptions (PS2 menus/single/r3-options: HUD Options, Camera 1, Camera 2
//      greyed, Music/MC / SFX / Character speech sliders, DJ Speech, Arcade SFX, Save game greyed in a Single Event; Down wraps past
//      the greyed rows; ctm/47-options: Save game in Conquer the Mountain). The port's Widescreen / Keyboard / Display & Touch sit
//      behind "More options" in the online-only EA Talk row (y 325, focus 120). ui.js routes every in-game 'options' there.
{ const am = await import('./audio-menu.js'), { stepMenu } = await import('./fe-screens.js');
  const ui = sourceOf('ui.js');
  const lui = JSON.parse(fs.readFileSync(new URL('public/assets/UI/audio-menus.json', import.meta.url), 'utf8')).screens['37beoptions'];
  const names = new Set((lui?.elements || []).map((e) => e.name));
  const rowsOk = !lui || am.PDA_ROWS.every((r) => [r.row, r.label, r.value, r.slider, r.help].filter(Boolean).every((n) => names.has(n)));
  const framesOk = !lui || am.PDA_ROWS.every((r) => lui.events.some((ev) => ev.frame === r.frame && ev.element === r.label && ev.props?.[14] === 255));
  const dis = am.PDA_ROWS.map((r) => !!r.disabled || r.kind === 'save'), walk = []; let i = 0; for (let k = 0; k < 9; k++) { i = stepMenu(i, 1, dis); walk.push(i); }
  check(am.PDA_ROWS.map((r) => r.y).join() === '100,125,150,175,200,225,250,275,300,325' && rowsOk && framesOk && walk.join() === '1,3,4,5,6,7,9,0,1'
    && am.PDA_MORE.map((r) => r.text).join() === 'Widescreen,Keyboard,Display & Touch'
    && /if\(screen==='options'&&this\.audioMenus\?\.owns\?\.\('pda-options'\)\)screen='pda-options';/.test(ui),
    `in-game Options is the 37beoptions PDA page${lui ? '' : ' (audio-menus.json without 37beoptions yet: rows not checked)'}`); }
// R22. Restarts (pv pauseRestart, loopFadeOnce). PS2 menus/race/r3-restart: the pause's Restart shows the lit start gate at once and the
//      card over it; r3-results-restart: the results' Restart rides the gondola first; r3-rr-dense: the gate under the card is steady
//      (luma 179.1 over 1600 samples). The port rode the gondola for both, and the idle under a heat card re-ran its fade-in (from the
//      start hut's fade-out record) on every loop: the card sat over a gate dipping to black.
{ const cs = await import('./cutscenes.js'), cu = sourceOf('career-ui.js');
  const idle = { fade_in: { type: 0 }, fade_out: { type: 0 } }, prev = { type: 1, colour: 'black', in_ticks: 30 };
  const a = (o) => cs.fadeAt(o).alpha;
  check(a({ script: idle, t: 15, duration: 60, prevFadeOut: prev, step: { idle: true } }) === 0.5 && a({ script: idle, t: 15, duration: 60, prevFadeOut: prev, step: { idle: true }, loops: 1 }) === 0
    && cs.restartSteps().length === 1 && /const kind=fromResults\?'heat':'restart'[;,]/.test(cu),
    "pause Restart: the gate idle under the card (no gondola); a looping step's fade-in on its first pass only"); }
// R23. The lodge (pv lodgeLui): the menu is FE.LUI 28lodge (PS2 menus/ctm/62-lodge, 63-lodge-cursor-*: the orange frame, rows at
//      508 x 132 + 20 i, 'Save your progress.'), its questions the FE popup sized 0.681 x 0.585, moved (-4, -36), with the veil
//      (64-lodge-quit-confirm; the veil fitted from 63-lodge-cursor-8quit -> 64: light blue, A 0.72).
{ const fe = await import('./fe-screens.js'), cu = sourceOf('career-ui.js'), fs2 = fs.readFileSync(new URL('fe-screens.js', import.meta.url), 'utf8');
  const lui = JSON.parse(fs.readFileSync(new URL('public/assets/UI/character-select.json', import.meta.url), 'utf8')).screens['28lodge'];
  const rowsOk = !lui || ['Return to Game', 'Equip Gear', 'Buy Gear', 'Buy Attributes', 'Rider Details', 'Music', 'Save Game', 'Quit'].every((t, i) => lui.elements.some((e) => e.text === t && e.props?.[1] === 132 + 20 * i));
  check(rowsOk && fe.LODGE_POPUP.sx === 0.681 && fe.LODGE_POPUP.veil[3] === 184 && /drawLodgeMenu\(c, b, \{ index = this\.ui\.index/.test(fs2)
    && /if\(s==='ctm-lodge'&&ui\.feScreens\?\.drawLodgeMenu\?\.\(/.test(cu) && /overLodge&&ui\.feScreens\?\.drawLodgePrompt\?\.\(/.test(cu), `lodge menu from 28lodge and its questions in the FE popup${lui ? '' : ' (character-select.json without 28lodge yet: rows not checked)'}`); }
// R24. The MCOMM Session map (pv sessionMap): OV.LUI 38session with the location's picture ses_<x> (table 0x440770), the points and the
//      rider at |(p - c0) / (c1 - c0)| x (356, 266), truncated, + (252, 97) - 5 (0x209A88 / 0x209754), the focus opening on the nearest
//      point (0x26B680). PS2 menus/ctm/r3-session-up (Happiness, the rider at the bottom): Bottom of run focused; the Top of run
//      highlight's top-left (489, 132), the rider's (604, 281).
{ const sm = await import('./session-map.js'), abc1 = sm.SESSION_MAP[14][2], rider = [-46106, 37351, -206688];
  const pts = [[1980, -45988, 96309], [10773, -23646, 77059], [45216, -9407, 50865], [64333, 30111, 7806], [44349, 48523, -64943], [29150, 36753, -90407], [-1792, 46153, -132981]];
  const cu = sourceOf('career-ui.js');
  check(sm.mapMarker(abc1, pts[0][0], pts[0][1]).join() === '489,132' && sm.mapMarker(abc1, rider[0], rider[1]).join() === '604,281'
    && sm.nearestSessionPoint(pts, rider) === 7 && Object.keys(sm.SESSION_MAP).length === 22 && sm.SESSION_MAP[1][1] === 8
    && /if\(this\.drawSessionMap\(c,b,s\)\)return;/.test(cu),
    'Session map: the Top of run highlight at (489, 132), the rider at (604, 281), Bottom of run focused, as PS2 r3-session-up (sessionMap on)'); }
// R25. PS2 softness (Options > Display & Touch, quality.ps2Output; section 30): Off by default and then the page builds today's composite
//      (Chrome: the 52 WGSL modules identical, frames identical; WebKit frames identical); On: the scene colour low-passed by four
//      bilinear taps at +-0.25 / +-0.75 of a 640-frame pixel before the fog composite (Snow Jam free ride 3946: high-frequency energy
//      12.49 -> 8.87, PS2 8.67; crawl 1.32 -> 0.99). Built only when switched on; switching back restores the first graph.
{ const { resolveQuality, detectDevice } = await import('./quality.js'), { DISPLAY_ROWS } = await import('./fe-options.js');
  const fr = fs.readFileSync(new URL('fog-renderer.js', import.meta.url), 'utf8'), main = sourceOf('main.js');
  const dev = detectDevice({}), row = DISPLAY_ROWS.find((r) => r.q === 'ps2Output');
  check(resolveQuality(dev, {}, new URLSearchParams()).ps2Output === false && resolveQuality(dev, { ps2Output: true }, new URLSearchParams()).ps2Output === true
    && resolveQuality(dev, { ps2Output: true }, new URLSearchParams('ps2soft=0')).ps2Output === false && row?.label === 'PS2 softness' && DISPLAY_ROWS.indexOf(row) === 2
    && /const baseOutput=pipeline\.outputNode;let softOutput=null/.test(fr) && /if\(on&&!softOutput\)softOutput=buildSoft\(\);/.test(fr) && /tap\(-\.75\)\.add\(tap\(-\.25\)\)\.add\(tap\(\.25\)\)\.add\(tap\(\.75\)\)\.mul\(\.25\)/.test(fr)
    && /fogRenderer\.setSoftness\?\.\(quality\.ps2Output\);/.test(main) && /fogRenderer\?\.setSoftness\?\.\(q\.ps2Output\);/.test(main),
    'PS2 softness option: Off by default, the filtered composite built only when on (Display & Touch row 3)'); }
// R26. A Transport to a station (pv stationArrival, section 33): world state 14 arg 1 queues no cinematic at a station and raises no
//      prompt (lists 22 / 23 and overlay 0x1F are arg 0's, the lodge door's: 236250 / 236418); the station fades in from black like a
//      course (PS2 stations-sj-to-green, stations-to-c). World state 15 (pv sessionFade): MCOMM Session Yes or a Transport to the current
//      location fades in from white over 60 ticks with the HUD over it (2E4370(mgr, 1, 0, white, 0, 0, 1.0), 2E47E8 1 - t / 1.0 at the
//      60 Hz timer; PS2 stations-ws15).
{ const fr = sourceOf('free-ride.js'), main = sourceOf('main.js');
  check(!/station-arrival/.test(fr) && /if\(s\)s\.sameLocation=true;return s;\}/.test(fr)
    && /if\(s\?\.sameLocation\)cutscenes\?\.fadeFrom\?\.\(\{ticks:60,colour:'white',hud:true\}\);else if\(s&&\(dest<14\|\|dest>=17\)\)/.test(main)
    && !/station-arrival/.test(main) && /freeRide\.afterReset\(\);cutscenes\?\.fadeFrom\?\.\(\{ticks:60,colour:'white',hud:true\}\);/.test(main),
    'station Transport: no walk-in or lodge prompt, a black fade-in; Session / same-location Transport: a 60-tick white fade under the HUD'); }
// R27. No rider at its bind pose (pv riderPoseGate, docs/ctm-flow.md "T-poses on the first load"): the placeholder RIDER_SAM of every course
//      load and a newly loaded rider start hidden (the held transport loop draws the scene across a world switch, where the frame's
//      visibility pass does not run: the field probe's 26-bone hit at 'loading', PEAK1/1), and in the ride screens the rider draws once a
//      tick has posed it (a switch's first ride frame drew the career rider at its bind pose, in view).
{ const main = sourceOf('main.js');
  check(/sam\.visible=false;scene\.add\(sam\);/.test(main) && /sam=model;sam\.visible=false;/.test(main)
    && /!\(!currentRiderFrame&&\(readyScreen\(\)\|\|/.test(main),
    'rider pose gate: the placeholder and a new rider start hidden, the ride shows the rider once posed'); }
// R28. The freestyle finish panel (pv finishLui): OV.LUI finishov as 0x1E8200 sets it up (the run label by round, 'place%d' / place6,
//      pointstotal, the medal of a final or Rival Points career run, new_record, Menu0000 hidden), frame 1 two ticks after the 3 s mark.
//      PS2 pipe-finishov2 4589..4634: panel MAD 12.6-39.7 -> 2.1-8.4 in Chrome and WebKit.
{ const cu = sourceOf('career-ui.js'), amp = new URL('public/assets/UI/audio-menus.json', import.meta.url);
  const lui = fs.existsSync(amp) ? JSON.parse(fs.readFileSync(amp, 'utf8')).screens?.finishov : null;
  const labels = lui ? new Set(lui.elements.map((e) => e.label)) : null;
  const need = ['Menu0000', 'firstrun', 'secondrun', 'finalrun', 'place1', 'place6', 'pointstotal', 'new_record', 'gold', 'silver', 'bronze', 'platinum'];
  check((!labels || need.every((n) => labels.has(n))) && /if\(this\.finishLui\(c,ev,score,t,place,rival\)\)return;/.test(cu)
    && /const frame=Math\.min\(45,t\*60-2\),events=\[\];if\(frame<1\)return true;/.test(cu) && /round===2\?'secondrun':round===3\?'finalrun':'firstrun'/.test(cu)
    && /if\(ev\.career&&\(round===3\|\|ev\.mode===MODE\.RIVAL_POINTS\)\)\{medal=placementMedal\(ev\.mode,place-1\);/.test(cu)
    && /n==='place'\+Math\.min\(6,place\)/.test(cu),
    `finish panel from OV.LUI finishov${labels ? '' : ' (audio-menus.json without finishov yet: elements not checked)'} (finishLui on)`); }
// R29. The lodge's Trophies (pv trophyLui, section 36): FE.LUI 125mountainroom / 126peakroom / 127trophyroom with their states' rules
//      (0x1D2990.., 0x1D3890.., 0x1D4368..): the goal rows of table 0x45AAD8 named as 0x1CE758 names them, the medals (0x1CED90), the
//      stats line (0x1CE9E0: mm:ss of the record's whole seconds, score, "$ 123,456", counts of 155 / 40), the markers (0x1D3340: one per
//      standard event, by mode, counted over the peaks), the help by focus and passes, and the flow (locked peaks open, incomplete goals do
//      not, Triangle closes the pass popup first). PS2 trophy-tour / medals / stats / earn / locked / pop*: MAD 5.0-7.7 (was 47-52).
{ const { TrophyRoom } = await import('./trophy-room.js'), { Career } = await import('./career.js');
  const root = new URL('public/assets/', import.meta.url), csp = new URL('UI/character-select.json', root);
  const fe = fs.existsSync(csp) ? JSON.parse(fs.readFileSync(csp, 'utf8')) : null, lui = !!fe?.screens?.['125mountainroom'];
  const c = new Career(JSON.parse(fs.readFileSync(new URL('CAREER/career.json', root), 'utf8')), { storage: null }), r = c.rider('zoe');
  const medals = { '0:0': 1, '0:1': 0, '4:14': 1, '6:-': 3, '1:5': 2, '2:11': 1 };   // PS2 trophy-medals (patched record bytes)
  r.medals = { ...medals, '3:8': 3, '5:14': 2, '9:-': 0 }; r.earned = 123456;       // + trophy-stats
  r.best = { '0:0': 125 * 60, '0:1': 61 * 60, '4:14': 200 * 60, '6:-': 754 * 60, '1:5': 12345, '3:8': 67890, '2:11': 1000000, '5:14': 5, '9:-': 250000 };
  const ui = { screen: 'ctm-trophies', index: 0, set(s) { this.screen = s; this.index = 0; }, sync() {}, feScreens: { data: fe, images: {}, t: (n, f) => fe?.strings?.[n] || f } };
  const t = new TrophyRoom({ ui, c, id: 'zoe', cs: { picture: () => null } }), rows = (p, g) => t.rows(p, g).map((x) => x.text).join('|');
  check(rows(1, 0) === 'Snow Jam|Metro-City|Happiness Race|Peak 1 Race' && rows(2, 0) === 'Ruthless Ridge|Intimidator|Ruthless Race|Peak 2 Race'
    && rows(3, 0) === 'Gravitude|Throne Race|All Peak Race' && rows(1, 1) === "R&B|Crow's Nest|The Junction|Happiness Jam|Peak 1 Jam"
    && rows(1, 2) === 'Collectibles|Big Challenges' && rows(1, 3) === 'Earnings' && t.rows(1, 0).map((x) => x.medal).join() === '1,0,1,3'
    && t.rows(1, 3)[0].medal === 1, 'trophy rows: the goal table\'s events by 0x1CE758 (PS2 trophy-medals, trophy-locked), medals, earnings gold');
  const st = (p, g, k) => t.stat(t.rows(p, g)[k], p);
  check(st(1, 0, 0) === 'Your best time: 02:05' && st(1, 0, 3) === 'Your best time: 12:34' && st(1, 1, 1) === 'Your best score: 67890'
    && st(1, 3, 0) === "You've earned: $ 123,456" && st(1, 2, 0) === 'Collectibles: 0/155' && st(1, 2, 1) === 'Challenges Complete: 0/40',
    'trophy stats line as 0x1CE9E0 writes it (PS2 trophy-stats 02:05 / 12:34 / 67890, trophy-earn $ 123,456)');
  t.choose(1); const locked = ui.screen === 'ctm-trophy-peak' && t.peak === 2; t.back(); const back1 = ui.screen === 'ctm-trophies' && ui.index === 1;
  t.choose(0); t.choose(2); const noEnter = ui.screen === 'ctm-trophy-peak'; t.choose(1); const room = ui.screen === 'ctm-trophy-room' && t.goal === 1 && t.items(ui.screen).length === 6;
  t.back(); const back2 = ui.screen === 'ctm-trophy-peak' && ui.index === 1; t.back(); ui.index = 3; t.choose(3); const pop = !!t.popup; t.back(); const closed = !t.popup && ui.screen === 'ctm-trophies'; t.back();
  check(locked && back1 && noEnter && room && back2 && pop && closed && ui.screen === 'ctm-details' && ui.index === 1,
    'trophy flow: a locked peak opens (trophy-locked), an incomplete goal does not, Triangle closes the pass popup first, back to Rider Details Trophies');
  if (lui) {
    r.medals = medals; ui.screen = 'ctm-trophies'; t.ready();
    const els = fe.screens['125mountainroom'].elements.filter((e) => /^mrk_/.test(e.label || '')), m = t.mountain();
    const on = els.filter((e) => m(e)?.sprite?.hash === fe.trophy_sprites['dot_visit arrow'].hash).map((e) => e.label).sort().join();
    const help = [0, 1, 2, 3].map((i) => { ui.index = i; return t.mountain()({ label: 'helptext' }).text; });
    check(els.length === 20 && on === 'mrk_race_0,mrk_race_1,mrk_racebackcountry_0,mrk_slopestyle_0,mrk_superpipe_0' && t.items('ctm-trophies').join('|') === 'Peak 1|Peak 2|Peak 3|Peak Pass'
      && help[1].startsWith('LOCKED:') && help[2].startsWith('LOCKED:') && help[3] === 'View current peak pass.',
      'Trophies markers (0x1D3340: medal earned on the five medalled standard events, PS2 trophy-medals f380), menu and help by focus');
  }
  const lodge = sourceOf('lodge-ui.js');
  check(/if\(this\.trophyLui\(s\)&&this\.trophies\.draw\(c,b\)\)return;/.test(lodge) && /trophyLui\(s\)\{return this\.trophies\.owns\(s\);\}/.test(lodge),
    `lodge Trophies drawn from FE.LUI 125 / 126 / 127${lui ? '' : ' (character-select.json without the trophy screens yet: markers not checked)'}`); }
// R30. The lodge's Save Game (pv lodgeSave, section 37): cFEStateProfileLoad mode 3 (0x1F3A38 menu value 8) on FE.LUI 93profile_load
//      with the browser's card (one save), the name keyboard, then the memory-card popups: Checking, overwrite? (No focused), Saving,
//      Save complete. / Continue back to the lodge on Save Game (or where Options > Save/Load opened it). The popups: cFEPopup's own
//      layout (web/fe-popup.js, R31). PS2 save-s6..s8 and lodge/35-save-game: MAD 2.6-6.0 (the old 'Save complete.' screen 55.3).
{ const { SaveGame } = await import('./save-game.js'), { fePopupLayout } = await import('./fe-popup.js');
  const glyphs = JSON.parse(fs.readFileSync(new URL('public/assets/UI/FEFONT-glyphs.json', import.meta.url), 'utf8'));
  let saved = true, persisted = 0, card = true, clock = 1000;
  const store = { setItem() { if (!card) throw Error('blocked'); }, removeItem() {}, getItem() { return null; } };
  const fe = { lui: { 'fe-load': {} }, popupLui: {}, keyboard: null, playerName: 'PLAYER 1', openKeyboard() { this.keyboard = { kind: 'name' }; },
    keyboardKey(e) { if (e.code === 'Enter') this.keyboard = null; }, saveInfo() { return saved ? { name: this.playerName } : null; }, t: (k, f) => f };
  const ui = { screen: 'ctm-lodge', index: 6, fonts: { FEFONT: glyphs.glyphs || glyphs }, feScreens: fe, set(s) { this.screen = s; this.index = 0; }, sync() {} };
  const cs = { career: { storage: store, persist() { persisted++; saved = true; return true; } }, t: (k, f) => f };
  const g = new SaveGame({ ui, cs }); g.now = () => clock;
  const N = '\u00a0', checkMsg = `Checking for memory${N}card${N}(PS2) in MEMORY${N}CARD${N}slot${N}1. Do not remove memory${N}card${N}(PS2) or the controller, reset or switch off the console.`;
  const G = glyphs.glyphs || glyphs, four = fePopupLayout(G, { message: checkMsg }), fmt = fePopupLayout(G, { message: `The memory${N}card${N}(PS2) inserted into MEMORY${N}CARD${N}slot${N}1 is unformatted. Do you wish to format?`, options: ['Yes', 'No'] });
  check(four.lines.length === 4 && four.lines[2] === `memory${N}card${N}(PS2) or the controller, reset or` && fmt.lines.length === 3,
    'save popups: the PS2 line breaks (Checking 4 lines, the format question 3: 0x1C94B0 / 0x1C9938)');
  const key = (code) => g.key({ code, preventDefault() {} });
  const opened = g.open(); const list = ui.screen === 'ctm-save' && g.phase === 'list' && g.items()[0] === 'PLAYER 1';
  key('Enter'); const naming = g.phase === 'name' && !!fe.keyboard; key('Enter'); const checking = g.phase === 'check';
  clock += 61; g.tick(); const asking = g.phase === 'ask' && g.ask === 1 && g.items().join() === 'Yes,No';
  key('ArrowUp'); key('Enter'); const saving = g.phase === 'saving' && persisted === 1;
  clock += 91; g.tick(); const complete = g.phase === 'done'; key('Enter');
  check(opened && list && naming && checking && asking && saving && complete && ui.screen === 'ctm-lodge' && ui.index === 6,
    'Save Game flow: rows, name keyboard, Checking, overwrite? (No focused) -> Yes, Saving (the career written), Save complete. -> the lodge on Save Game');
  g.open(); key('Enter'); key('Escape'); const cancel = g.phase === 'list' && !fe.keyboard; key('Enter'); key('Enter'); clock += 61; g.tick(); key('Enter'); const no = g.phase === 'list' && persisted === 1;
  card = false; const blocked = g.items().length === 0; key('Enter'); const stay = g.phase === 'list' && !fe.keyboard; key('Escape');
  check(cancel && no && blocked && stay && ui.screen === 'ctm-lodge', 'Save Game: Triangle leaves the keyboard, No keeps the save, no card (storage blocked) = no rows, Triangle back to the lodge');
  const lodge = sourceOf('lodge-ui.js'), cui = sourceOf('career-ui.js');
  check(/if\(this\.saveLui\(s\)&&this\.saveGame\.draw\(c,b\)\)return;/.test(lodge) && /else if\(i===6\)\{if\(this\.lodge\.saveGame\?\.ready\(\)\)this\.lodgeGo\(\(\)=>this\.lodge\.saveGame\.open\(\)\);/.test(cui),
    'lodge Save Game opens the Save game screen'); }
// R31. The front end's popup box (pv fePopup, section 39): cFEPopup's layout (0x1C6B08 .. 0x1C7C20, web/fe-popup.js) reproduces the live
//      popup objects of the PS2 captures (+0x260 size, +0x278 scale, the menu at +0x290 and the frame parts' y) for the overwrite question
//      (save-s7), Save complete. (s4 / s8), the format question (s3) and the lodge's Quit / Save progress questions; the lodge's questions
//      open as cFEPopupConfirm (0x1C5C30: steps 0.2 + k (t - 0.2) / 24 to k = 25: lodge-quitpop88 / 96 / 108). drawLodgePrompt uses it;
//      Options > Save/Load Save game opens the Save game screen. PS2 lodge-quitprompt / saveprompt: MAD 11.9 / 11.5 -> 2.6 / 2.4.
{ const { fePopupLayout, textBox } = await import('./fe-popup.js');
  const G = (() => { const g = JSON.parse(fs.readFileSync(new URL('public/assets/UI/FEFONT-glyphs.json', import.meta.url), 'utf8')); return g.glyphs || g; })();
  const N = ' ', near2 = (a, b, e = 0.006) => Math.abs(a - b) <= e;
  const cases = [['Would you like to overwrite PLAYER 1?', ['Yes', 'No'], [345.6, 102.4, 33.8, 30.4, 0.7617, 0.5755, 135.7]],
    ['Save complete.', ['Continue'], [300, 82.4, 9.2, 30.4, 0.6667, 0.4775, 125.7]],
    [`The memory${N}card${N}(PS2) inserted into MEMORY${N}CARD${N}slot${N}1 is unformatted. Do you wish to format?`, ['Yes', 'No'], [445.2, 143.2, 33.8, 71.2, 0.9692, 0.7755, 156.1]],
    ['Quit to Title screen?', ['Yes', 'No'], [300, 102.4, 33.8, 30.4, 0.6667, 0.5755, 135.7]]];
  const ok = cases.every(([message, options, [w, h, mx, my, sx, sy, y]]) => { const L = fePopupLayout(G, { message, options });
    return near2(L.w, w, 0.05) && near2(L.h, h, 0.05) && near2(L.menu[0], mx, 0.05) && near2(L.menu[1], my, 0.05) && near2(L.box.sx, sx, 0.0006) && near2(L.box.sy, sy, 0.0006) && near2(L.box.y, y, 0.05); });
  const step = (k, t) => 0.2 + k * (t - 0.2) / 24;
  check(ok && near2(textBox(G, 'Yes', 0.6), 32.4, 0.01) && near2(textBox(G, 'Continue', 0.6), 81.6, 0.01) && near2(step(7, 0.6667), 0.3361, 0.0002) && near2(step(15, 0.6667), 0.4917, 0.0002) && near2(step(25, 0.6667), 0.6861, 0.0002),
    'FE popup layout as cFEPopup computes it (the PS2 popup objects of save-s3/s4/s7/s8 and lodge-quitprompt; the confirm popup\'s growth steps)');
  const fes = sourceOf('fe-screens.js'), fsl = sourceOf('fe-saveload.js');
  check(/if\(this\.data\?\.screens\?\.popup\)\{/.test(fes) && /animated:true\}\);/.test(fes) && /if\(i===0&&this\.ui\.careerUI\?\.lodge\?\.saveGame\?\.ready\(\)\)/.test(fsl),
    'the lodge\'s Yes / No questions drawn as cFEPopup; Options > Save/Load Save game opens the Save game screen'); }
// R32. Lit LiveComp instances (pv litLiveComp, section 40): authored descriptor flag 0x40000000 is runtime flag 0x4000 in every countdown
//      audit, and 37E238 (0x37E3B8) -> 2F5148 -> 2F5400 lights such an instance per vertex from its light cache on the node-rotated normal
//      (VU1 program 3 0x8B8), over the baked colours. PS2 RAM: the ERA5 crash billboards' cache entries (grav-bb tick1209, 0xACC2D0 /
//      0xACC450) and the CRA3 falling tram's (cra3-full tick10418, 0xACC150) hold their object banks' rows unchanged (no local light);
//      the billboards' node matrices at 1209 / 1230 equal the page's. So the dark board at grav-bb 1208 is its poster lit near black
//      (L = (7, 17, 33)), not a culled face: VU1 program 3 has no facing test. grav-bb 1169 / 1192 / 1208 / 1229 luma MAD 20.2 / 24.0 /
//      28.1 / 23.3 -> 15.5 / 21.5 / 24.1 / 22.4 (WebKit within 0.3).
{ const EOBR1 = [[0.327142805, 0.402806133, 0.4941836], [0.0316557698, 0.0292963907, 0.0124205314], [0.0124609182, 0.0114212716, 0.00224322779],
    [-0.0756307542, -0.0825445578, -0.0738663226], [0.222393185, 0.24890326, 0.238380954], [-0.10126698, -0.116345614, -0.133048296],
    [-0.110914171, -0.113272473, -0.0835877433], [0.321016282, 0.347605199, 0.344127327], [0.25471741, 0.303694397, 0.319781601], [-0.117957979, -0.0909409001, -0.0416484922]];
  const COBR1 = [[0.517549932, 0.380353421, 0.375266999], [-0.128408358, -0.0805904195, -0.0542569272], [0.213144138, 0.119328506, 0.0485817678],
    [-0.141031757, -0.0862255394, -0.0527842902], [-0.0686803982, -0.0519338995, -0.0546181127], [-0.0135588208, 0.0110299876, 0.0452987961],
    [-0.0850561038, -0.0552985184, -0.0346335955], [0.0181690715, 0.00758956699, -0.03615411], [-0.49662897, -0.271267325, -0.0685320273], [0.000341122533, -0.00562910642, -0.0511714965]];
  const lc = (loc) => JSON.parse(fs.readFileSync(new URL(`public/assets/${loc}/LIVECOMP/livecomp.json`, import.meta.url), 'utf8')).instances;
  const rowsOk = (l, ps2) => l?.scale === 128 && l.rows.length === 10 && l.rows.every((r, i) => r.slice(0, 3).every((v, k) => Math.abs(v - ps2[i][k]) < 1e-6));
  const flagged = (list) => list.every((x) => !!x.lighting === !!(x.authoredFlags & 0x40000000));
  const era5 = lc('ERA5'), cra3 = lc('CRA3'), lit = (list) => list.filter((x) => x.lighting).map((x) => x.resource).join();
  check(flagged(era5) && flagged(cra3) && lit(era5) === '332333,669741' && lit(cra3) === '39960' && era5.filter((x) => x.lighting).every((x) => rowsOk(x.lighting, EOBR1)) &&
    rowsOk(cra3.find((x) => x.lighting).lighting, COBR1), 'lit LiveComps (authored flag 0x40000000) carry their object bank as the PS2 light cache holds it (ERA5 EOBR1, CRA3 COBR1)');
  const spr = fs.readFileSync(new URL('set-pieces-renderer.js', import.meta.url), 'utf8'), wm = sourceOf('world-material.js');
  check(/if \(liveData\?\.instances\)/.test(spr) && /mesh\.material = litWorldMaterial\(mesh\.material, l\)/.test(spr) &&
    /const L=varying\(floor\(clamp\(acc,0,255\)\)\)/.test(wm) && /frameBytes\(floor\(base\.rgb\.mul\(L\)\.div\(128\)\)\)/.test(wm),
    'lit LiveComp meshes drawn with Cs = T x L >> 7 from the per-vertex bank'); }
// R33. The lodge's screen changes (pv lodgeFlash, section 42): an FE state change (0x39F400) plays the old state's TransitionOut, whose
//      control 0x30 starts transition_flash (white A up over 10 frames, down over 9) and whose end label's 0x41 (0x39CE98, 9 frames on)
//      makes the switch; the new state's LUI plays its intro from frame 0, its menu focus from the intro's 0x42 label (frame 25). PS2
//      tout-troA (125mountainroom Triangle) / tout-detD (the lodge's Rider Details) / tout-saveC (Save Game): white 0.29 / 0.79 at 5 / 10
//      frames after the press, full at 12, Rider Details' intro under the fall at 16, clear by 22, its focus bar by 44.
{ const { LuiFlash } = await import('./lui-flash.js');
  let T = 0, switched = 0; const f = new LuiFlash(() => T), c = { save() {}, restore() {}, fillRect() { this.n = (this.n || 0) + 1; }, set fillStyle(v) { this.style = v; } };
  const a = (t) => { T = t; f.draw(c); return f.flash ? +(/,([\d.]+)\)$/.exec(c.style)?.[1] ?? 0) : 0; };
  f.go(() => switched++); const rise = [0, 3, 5, 8].map(a), before = switched, full = a(10), after = switched, fall = [14, 18].map(a); a(19);
  const second = f.go(() => switched++) && (f.go(() => switched += 10) === false);
  check(rise.join() === '0,0.3,0.5,0.8' && before === 0 && full === 1 && after === 1 && Math.abs(fall[0] - 5 / 9) < 1e-9 && Math.abs(fall[1] - 1 / 9) < 1e-9 && second,
    'lodge flash: A = t / 10 up, the switch at full white once, A down over 9, then idle; one change at a time');
  T = 100; f.flash = null; f.fall(); const fallOnly = a(100) === 1 && Math.abs(a(104.5) - 0.5) < 1e-9 && (a(109), !f.active);
  const src = (n) => fs.readFileSync(new URL(n, import.meta.url), 'utf8');
  const cui = sourceOf('career-ui.js'), lui = sourceOf('lodge-ui.js'), tro = sourceOf('trophy-room.js'), sav = sourceOf('save-game.js'), fes = sourceOf('fe-screens.js');
  check(fallOnly && /else if\(i===4\)this\.lodgeGo\(\(\)=>ui\.set\('ctm-details'\)\)/.test(cui) && /if\(this\.lodgeFlashing\(s\)\)\{e\.preventDefault\(\);return true;\}/.test(cui) &&
    /if\(s==='ctm-details'\)\{this\.cs\.lodgeGo\(/.test(lui) && /go\(to\)\{const cs=this\.lodge\.cs;if\(cs\?\.lodgeGo\)cs\.lodgeGo\(to\);else to\(\);\}/.test(tro) &&
    /const cs=this\.lodge\.cs;if\(cs\?\.lodgeGo\)cs\.lodgeGo\(to\);else to\(\);/.test(sav) && /if\(this\.lodgeFlash\(\)\)\{this\.ui\.careerUI\.lodgeGo\(to\);return;\}/.test(fes),
    'every lodge LUI screen change goes through the flash: the lodge, Rider Details, Trophies, Save Game, Highlights, the FE screens it opens'); }
// R34. The lodge's states (section 42): the menu cursor kept per state (pv stateCursor: 0x1865A8 -> 1A0708 on exit, 0x186518 ->
//      1A06F0 / 39B960 on activation for the lodge, Rider Details, Buy Attributes, Trophies' mountain room; PS2 tout-kbd: Rider Details
//      reopens on Trophies), the new state's intro 2 frames in when it shows (pv introLead: 0x39ED4C + the phase-4 tail 0x39EED4) and its
//      focus 2 frames after the intro's 0x42 (tout-detD: bar absent 26, present 28), Equip / Buy Gear and Buy Attributes on the one flash
//      (full white while the gear screen loads). The Player Name keyboard and Cheat Characters are child states (0x39F290): no flash
//      (PS2 tout-kbd: the keyboard 5 frames after Cross and the screen 5 after Triangle, no white).
{ const { LuiFlash, INTRO_LEAD, FOCUS_LAG, FLASH_IN } = await import('./lui-flash.js'), ba = await import('./buy-attribs.js');
  let T = 0; const f = new LuiFlash(() => T), c = { save() {}, restore() {}, fillRect() {}, fillStyle: '' };
  f.go(() => {}); for (T = 0; T <= 10; T++) f.draw(c); T = 11;
  const lead = f.introStart(11) === 10 - INTRO_LEAD && f.introStart(40) === 40 && INTRO_LEAD === 2 && FOCUS_LAG === 2;
  let resolve; T = 100; const g = new LuiFlash(() => T); g.go(() => new Promise((r) => { resolve = r; })); for (T = 100; T <= 110; T++) g.draw(c);
  T = 125; g.draw(c); const held = g.flash?.wait === true && c.fillStyle === 'rgba(255,255,255,1)'; resolve(); await Promise.resolve(); await Promise.resolve();
  const fell = !g.flash?.wait && Math.abs(g.alpha() - 1) < 1e-9 && g.switchedAt === 125;
  check(lead && held && fell && ba.FLASH_IN === FLASH_IN && ba.flashAlpha(5) === 0.5, 'lodge states: intro 2 frames in at the switch, full white while the next screen loads, Buy Attributes on lui-flash.js');
  const src = (n) => fs.readFileSync(new URL(n, import.meta.url), 'utf8'), cui = sourceOf('career-ui.js'), lui = sourceOf('lodge-ui.js'), war = sourceOf('wardrobe.js'), bat = sourceOf('buy-attribs.js');
  check(/const CURSOR_STATES=new Set\(\['ctm-lodge','ctm-details','ctm-trophies','ctm-attributes'\]\)/.test(cui) && /lodgeGo\(to\)\{const run=\(\)=>\{this\.memoCursor\(\);const r=to\(\);/.test(cui) &&
    /else if\(i===1\|\|i===2\)this\.lodgeGo\(\(\)=>this\.lodge\.openGear/.test(cui) && /onExit:\(\)=>this\.cs\.lodgeGo\(/.test(lui) && /this\.ui\.careerUI\?\.lodgeFlash\?\.draw\(c\)/.test(war) &&
    /if\(this\.cu\.lodgeGo\)\{this\.cu\.lodgeGo\(to\);return;\}/.test(bat) && !/openKeyboard\('name'\)\)|lodgeGo\(\(\)=>fe\.openKeyboard/.test(lui) &&
    true, 'lodge states: cursor memory, gear and Buy Attributes through the flash, the keyboard not'); }
// R35. Lit static-model instances (pv litInstances, section 40): each lit instance's light-cache rows (2F5400: the object bank of the
//      painter payload at its x/y plus up to 4 local lights ranked there, 2F5AF0 / 2F5B68 capacity 4 / 38A6A8; engine/lit_instance_lighting.hpp,
//      tools/export_lit_instances.py) on its own batches, drawn by one shared program with the rows as object uniforms. PS2 light caches in
//      the savestates under local/ps2-capture: 199 instances bit-exact at their instance position (ERA5 crashbag_0_00011 at grav-bb 1209:
//      EOBR1 + 4 lights, row 0 = (0.5109544, 0.6061056, 1.0957082)). BHP1 bag-bhp1 1141 / 1144: the crashbag's box MAD 27.3 / 27.1 -> 19.8 / 17.9.
{ const w = JSON.parse(fs.readFileSync(new URL('public/assets/ERA5/world.json', import.meta.url), 'utf8'));
  const bag = w.batches.find((b) => b.lighting?.resource === 46637), r0 = bag?.lighting?.rows?.[0] ?? [];
  const exact = [0.5109544, 0.6061056, 1.0957082].every((v, i) => Math.abs(r0[i] - v) < 1e-6) && bag.lighting.bank === 'EOBR1';
  const lit = new Set(w.batches.filter((b) => b.lighting).map((b) => b.lighting.resource));
  const wm = sourceOf('world-material.js'), wb = fs.readFileSync(new URL('world-batches.py', import.meta.url), 'utf8');
  check(exact && lit.size === 29 && /uniform\(new Vector3\(\)\)\.onObjectUpdate\(\(?frame\)?=>frame\.material\?\.userData\?\.litRows/.test(wm) &&
    /const lit=batch\.instance&&batch\.lighting,/.test(wm) && /if lit is not None:batch\['lighting'\]=lit_resources\[lit\]/.test(wb),
    'lit instances: the PS2 light-cache rows on their batches (ERA5: 29 instances), one shared lit program (litInstances on)'); }
// R36. The lodge's Equip Gear load (pv equipLoading, section 42; PS2 local/ps2-capture/menus/eqg-k*, eqg2-k*): no white hold, the screen is
//      up at the switch with "Loading..." over the list; phase 3 (the intro's 0x42 label + FOCUS_LAG: CharEquip vt+0x30 = 0x1993A0, 19E538
//      (slot, 1)) shows the rider once loaded, 36 frames after Cross on the PS2 (37 after the flash's 2-frame lead), "Loading..." one frame
//      later; a reload hides it again; the help line waits for the outfit's data (+0xA60).
{ const { EquipGearScreen } = await import('./wardrobe.js');
  let T = 0; const p = { failed: new Set(), model: null, shown: null, isReady: true, want() { return true; }, get ready() { return this.isReady; }, show(v) { this.shown = v; } };
  const g = new EquipGearScreen({ characterSelect: { preview3d: p } }); g.now = () => T; g.base = { id: 'zoe', package: 'RIDER_ZOE' }; g.enter = 8; g.T = {};
  g.data = { screen: { labels: [{ frame: 25, control: ['4200080000000000', '10000400'] }] } }; g.preparing = { promise: Promise.resolve() };
  T = 20; const building = g.showPreview() === true && p.shown === false && g.loadingText() && !g.gearReady();
  g.preparing = null; T = 34; const early = g.showPreview() === true && p.shown === false && g.loadingText() && !g.settled() && g.gearReady();
  T = 35; g.showPreview(); const first = p.shown === true && g.settled() && g.loadingText();
  T = 36; g.showPreview(); const next = p.shown === true && !g.loadingText();
  p.isReady = false; T = 50; g.showPreview(); const reload = p.shown === false && g.loadingText();
  const src = (n) => fs.readFileSync(new URL(n, import.meta.url), 'utf8'), lui = sourceOf('lodge-ui.js'), cui = sourceOf('career-ui.js'), war = sourceOf('wardrobe.js');
  check(building && early && first && next && reload && /preloadGear\(\)\{/.test(lui) && /this\.lodge\.preloadGear\?\.\(\)/.test(cui) &&
    /case '0b777dd4':return settled\?null:\{hidden:true\}/.test(war),
    'Equip Gear load: the screen at the switch, "Loading..." until the rider draws from phase 3, no help until the outfit is in'); }
// R37. The race HUD at the finish (pv finishHudHide, section 45): once rider+0x470 >= 0 (FINISH or TIME'S UP), 1EB9E8 sets the per-player
//      mask +0x80 = 0xFFEFFFFF (0x1EB9FC) and 12A250 cuts owner+0x3CC to 0x170000 (0x1EB91C): only the banner 0x100000 is drawn, a cut on the
//      finish tick (races and peak runs). PS2 setpieces/full (re-run, records byte-equal): snap 12297 full HUD, 12300 banner alone.
{ const slots = [{ type: 7, maximum: 1, value: 1, points: 1234 }, { type: 1, maximum: 1, value: 0.5, arg: 0, points: 0, text: 'BACKFLIP' },
    { type: 4, maximum: 1, value: 1, points: 2, text: '2' }];
  const live = hud.frame(slots, { flags: hud.flags, bigMessage: false });
  const done = hud.frame(slots, { flags: (hud.flags & 0x100000) >>> 0, finished: true, bigMessage: false });
  const u = sourceOf('ui.js'), m = sourceOf('career-messages.js');
  const gates = ['if(collecting&&!finishHide&&', '!collecting&&!finishHide)drawRacePlace', 'drawHud(c,true,!finishHide)', 'if(!fs&&!fr&&hudLevel<2&&!finishHide){',
    'hudLevel<2&&!finishHide)this.text(c,String(Math.round(s.score||0))', 'if(hudLevel<1&&!finishHide){if(raceHud)this.trickHud.speed(',
    '&&!fr&&hudLevel<1&&!finishHide){this.progressMeter?.draw(', 'if(hudLevel<2&&!finishHide){this.boostGauge?.drawOrb(',
    'if(hudLevel<2&&!finishHide&&this.trickHud&&this.trickHudRenderer){', 'if(finishHide)slotFlags=(slotFlags&0x100000)>>>0;', '{flags:slotFlags,finished:'];
  check('finishHudHide' in PV_DEFAULTS && live.length > 0 && done.length === 0 && /const finishHide=!fs&&!!s\.message&&pv\('finishHudHide'\);/.test(u)
    && u.includes('this.freeRideHud?.(c,hudLevel,finishHide)')
    && /freeRideHud=\(c,level,finishHide=false\)=>\{if\(!freeRide\|\|worldEvent\)return false;if\(course\.freeRide\.kind!==4\)\{if\(level<2&&!finishHide\)\{/
      .test(sourceOf('main.js'))
    && gates.every((g) => u.includes(g)) && /drawHud\(c,racing=true,draw=true\)\{/.test(m) && /if\(!color\)\{this\.hud=null;return;\}if\(!draw\)return;/.test(m),
    `finish HUD: every race element but the banner goes on the finish tick (slots ${live.length} -> ${done.length}; finishHudHide ${PV_DEFAULTS.finishHudHide ? 'on' : 'off'})`); }
if (failed) { console.error(`${failed} visual-parity check(s) failed`); process.exit(1); }
console.log('visual parity: all checks passed');
