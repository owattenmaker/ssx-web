// pv worldWarm (docs/course-switch.md "World arrivals warm under the load screen"): a Conquer the Mountain world load waits under the load
// screen for the start row's pipeline compile (free-ride.js rewarm), then warms the rest of what the world pass draws with the race warm-up's
// frames. Headless Chrome + a private Vite server: a new career from the menus (cutscenes off, the frame clock frozen so only ssxQA.advance
// ticks; the desktop tier's fixed 60 fps: the auto tier's frame gate drops to 30 fps after heavy frames and a frozen clock then never
// draws again), the switch off and on:
//  - on: both steps ran under the load screen (world:rewarm / world:warm measures);
//  - the ride is the same tick for tick (the human's reference motion, rider state and game RNG, 300 ticks of a pad);
//  - the frame after those ticks is the same picture, up to the rider's real-time interpolation noise that two runs with the switch off
//    show as well (tens of pixels, a few levels: docs/web-render-performance.md "Comparison pitfalls").
//   node test-world-warm.mjs     (skips without Chrome / WebGPU or without the game data)
import assert from 'node:assert/strict';
import fs from 'node:fs';
import zlib from 'node:zlib';
import { startBrowser, startServer } from './headless-chrome.mjs';

if (!fs.existsSync(new URL('./public/assets/courses.json', import.meta.url))) { console.log('world warm: skipped (no game data)'); process.exit(0); }
function png(b) { // RGBA of a PNG (8-bit, RGB / RGBA, not interlaced)
  let at = 8, w, h, ct; const idat = [];
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
function pixelDiff(a, b) { const A = png(Buffer.from(a, 'base64')), B = png(Buffer.from(b, 'base64')); if (A.w !== B.w || A.h !== B.h) return { n: Infinity, max: 255 };
  let n = 0, max = 0; for (let i = 0; i < A.d.length; i += 4) { const d = Math.max(Math.abs(A.d[i] - B.d[i]), Math.abs(A.d[i + 1] - B.d[i + 1]), Math.abs(A.d[i + 2] - B.d[i + 2])); if (d) { n++; if (d > max) max = d; } }
  return { n, max };
}
const browser = await startBrowser({ width: 960, height: 720 });
if (!browser) { console.log('world warm: skipped (no Chrome)'); process.exit(0); }
const server = await startServer();
const E = (x) => browser.evaluate(x);
async function ride(flags) {
  await browser.goto(`${server.origin}/manifest.webmanifest`); await E('(()=>{localStorage.clear();sessionStorage.clear();return 1})()');   // a new career
  await browser.goto(`${server.origin}/?qa=1&mute=1&cutscenes=0&presentationSeed=0x182200&rider=zoe&pv=${flags}`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  await E(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));const ui=ssxQA.ui();ui.careerMode=true;ui.onlineMode=false;ui.careerUI.enter();return 1})()`);
  await browser.waitFor(`(document.getElementById('stage').dataset.screen==='game'&&window.demoState?.running)||!!document.body.dataset.loadError`, 300000);
  assert.equal(await E('document.body.dataset.loadError || ""'), '');
  const info = JSON.parse(await E(`JSON.stringify({course:ssxQA.ui().course?.code,warm:performance.getEntriesByType('measure').filter(m=>m.name.startsWith('world:')).map(m=>m.name)})`));
  const ticks = JSON.parse(await E(`(()=>{const c=window.ssxEffects.core,out=[];for(let i=0;i<300;i++){const pad=new Array(24).fill(0);if(i%150<90)pad[20+(i%300<150?0:1)]=0.8;if(i%80<14)pad[10]=1;if(i%211<30)pad[11]=1;ssxQA.advance(1,pad);out.push([...new Float32Array(c.HEAPF32.buffer,c._reference_motion(),6),...new Float32Array(c.HEAPF32.buffer,c._rider_state(),16),...new Uint32Array(c.HEAPU8.buffer,c._animation_rng_words(),6)].join(','));}return JSON.stringify(out)})()`));
  await E('new Promise(r=>setTimeout(r,1500))');   // a few drawn frames of the tick reached
  const shot = await browser.screenshot('#stage');
  return { info, ticks, shot };
}
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('world warm: skipped (no WebGPU in headless Chrome)'); process.exit(0); }
  const A = process.env.WW_A ?? '-worldWarm', B = process.env.WW_B ?? 'worldWarm';
  const off = await ride(A), on = await ride(B);
  if (process.env.WW_SHOTS) { fs.writeFileSync(process.env.WW_SHOTS + '-a.png', Buffer.from(off.shot, 'base64')); fs.writeFileSync(process.env.WW_SHOTS + '-b.png', Buffer.from(on.shot, 'base64')); }
  assert.equal(off.info.course, on.info.course);
  if (!process.env.WW_A) { assert.deepEqual(off.info.warm, [], 'off: no world warm-up'); assert.deepEqual(on.info.warm.sort(), ['world:rewarm', 'world:warm'], 'on: the world warmed under the load screen'); }
  const first = off.ticks.findIndex((x, i) => x !== on.ticks[i]);
  assert.equal(first, -1, `the ride differs at tick ${first}`);
  const px = pixelDiff(off.shot, on.shot);
  assert.ok(px.n <= 1000 && px.max <= 16, `the frame after 300 ticks differs: ${px.n} pixels, up to ${px.max} levels`);
  console.log(`world warm: ${on.info.course}, 300 ticks identical, the frame after them within the rider noise (${px.n} pixels, up to ${px.max} levels)`);
} finally { await browser.close(); await server.close(); }
console.log('world warm OK: the world arrival warmed under the load screen, the ride and its picture unchanged');
process.exit(0);
