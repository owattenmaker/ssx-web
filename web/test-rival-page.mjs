// Backcountry rival challenges through the REAL game path (docs/backcountry.md "Rival computer rider"), not the capture
// gate's own set-up: headless Chrome loads the page (?qa=1&course=<X>&rider=zoe), the Single Event menu's call starts the
// event (ui.startSingleEvent: career-ui single -> the load -> web/ai-race.js prepareRival -> the objectives card), the card's
// Continue (careerUI.play -> startRun) starts the race, and ssxQA.advance replays the PS2 capture's pad tick by tick. The
// human, the rival and the shared game RNG must stay exact as long as they do in web/test-ps2-captures.mjs (the gate sets
// the rival up itself from npc-riders.json and the capture's RNG; the page must reach the same start on its own).
// 2026-09-26: the page raced from the core seed's game RNG (a glide state's, 436..928 draws) instead of the ready state's
// (seed 0 + 4 load draws): Nate left the PS2 at DBC2 tick 132, never crashed at 1413 nor made the 1656 reset placement, and
// the human followed at ~2650 (docs/weather.md section 10).
//   node test-rival-page.mjs [--only dbc2,abc1,...]
// Skipped without Chrome / WebGPU or without the captures (local/ps2-capture/runs).
import fs from 'node:fs';
import { startBrowser, startServer, sleep } from './headless-chrome.mjs';

const runs = new URL('../local/ps2-capture/runs/', import.meta.url).pathname;
// Baselines = the gate's (web/test-ps2-captures.mjs aiCases): the last exact tick of the human, the rival and the RNG.
const CASES = [
  // dbc2 rival: exact to 7000 since the crash frame fix (2026-09-28, the detached board's +0x160..+0x190 frame; was human 4435 / Nate 5141 /
  // RNG 4581).
  { key: 'dbc2', capture: 'peak2/dbc2-race-tuck', course: 'DBC2', mode: 4, ticks: 7000, human: 7000, rival: 7000, rng: 7000, rivalName: 'nate', resets: 1,
    why: 'Ruthless Rival Time vs Nate (Peak 2 stats): Nate lands in a crash at 1413 and makes the reset placement at 1656 (painter re-seed); the human crashes on surface 18 at 3911' },
  { key: 'abc1', capture: 'bc/bc-race-tuck', course: 'ABC1', mode: 4, ticks: 6700, human: 6700, rival: 6700, rng: 6700, rivalName: 'mac', why: 'Happiness Rival Time vs Mac' },
  // pv rivalRelations: Mac's soft attacks raise his record of Zoe to level 2 at 599 (0x155BF0), so 10F560 flags the pair and the
  // human's 115D48 picks the peer reaction 319 at 1161 (was 317: the RNG left at 1302 and the human at 2009).
  { key: 'abc1-tuck2', capture: 'bc/bc-race-tuck2', course: 'ABC1', mode: 4, ticks: 2100, human: 2100, rival: 2100, rng: 2100, rivalName: 'mac', why: 'Happiness Rival Time vs Mac, second line: in-race relationships' },
  { key: 'ebc3', capture: 'peak3/the-throne-race-idle', course: 'EBC3', mode: 4, ticks: 2900, human: 2900, rival: 2900, rng: 2900, rivalName: 'psymon', why: 'The Throne Rival Time vs Psymon' },
  { key: 'abc1-jam', capture: 'bc/bc-jam-tricks2', course: 'ABC1', mode: 5, ticks: 1000, human: 389, rival: 389, rng: 401, rivalName: 'mac',
    why: 'Happiness Jam (Rival Points) vs Mac: 390 a human/Mac pair contact in board press (physics, docs/backcountry.md)' },
];
const only = process.argv.includes('--only') ? new Set(process.argv[process.argv.indexOf('--only') + 1].split(',')) : null;

// The capture's pad as the page's 24 channels (compare-ai-capture.mjs decodePad).
const BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown', 'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
function towardZero(x) { let r = Math.fround(x); if (Math.abs(r) > Math.abs(x)) { const b = new Float32Array([r]); new Uint32Array(b.buffer)[0] -= 1; r = b[0]; } return r; }
const axisByte = (v) => Math.floor((Math.max(-1, Math.min(1, v)) + 1) * 127.5 + 0.5);
function decodePad(seg) {
  const values = new Array(24).fill(0), held = new Set(seg.buttons || []);
  BUTTONS.forEach((name, i) => { const on = held.has(name); values[i] = i >= 4 ? towardZero((on ? 255 : 0) * R255) : (on ? 1 : 0); });
  [axisByte(seg.rx || 0), axisByte(-(seg.ry || 0)), axisByte(seg.lx || 0), axisByte(-(seg.ly || 0))].forEach((b, axis) => {
    const neg = Math.max(Math.trunc((79 - b) * 255 / 79), 0), pos = Math.max(Math.trunc((b - 176) * 255 / 79), 0);
    values[16 + 2 * axis] = towardZero(neg * R255); values[17 + 2 * axis] = towardZero(pos * R255);
  });
  return values;
}
// Record N (tick N): the human (+0x110 position, +0x1E0 velocity), the first computer rider, the RNG at the start of tick N.
function readCapture(name, ticks) {
  const bin = runs + name + '.bin', manifest = JSON.parse(fs.readFileSync(runs + name + '.capture.json', 'utf8'));
  const RECORD = manifest.record, fd = fs.openSync(bin, 'r'), n = Math.min(Math.floor(fs.fstatSync(fd).size / RECORD), ticks + 1), buf = Buffer.alloc(8920);
  const records = [];
  for (let i = 0; i < n; i++) {
    fs.readSync(fd, buf, 0, buf.length, i * RECORD); const f = (o) => buf.readFloatLE(o), u = (o) => buf.readUInt32LE(o);
    records.push({ tick: u(4), index: u(28), human: [0x10, 0x14, 0x18, 0xE0, 0xE4, 0xE8].map((o) => f(32 + o)), rival: [3008, 3012, 3016, 3024, 3028, 3032].map(f),
      rng: Array.from({ length: 6 }, (_, k) => u(8896 + 4 * k)) });
  }
  fs.closeSync(fd);
  const ends = []; let end = 0; for (const seg of manifest.segments) { end += seg.frames; ends.push({ end, pad: decodePad(seg) }); }
  const padFor = (index) => (ends.find((e) => index < e.end) || ends[ends.length - 1]).pad;
  return { records, padFor, isolated: !!manifest.isolated_from_computer_riders };
}

const cases = CASES.filter((c) => (!only || only.has(c.key)) && fs.existsSync(runs + c.capture + '.bin') && fs.existsSync(runs + c.capture + '.capture.json'));
if (!cases.length) { console.log('Rival page check SKIPPED: no backcountry rival capture in local/ps2-capture/runs'); process.exit(0); }
const browser = await startBrowser({ width: 640, height: 480 });
if (!browser) { console.log('Rival page check SKIPPED: no Chrome found (set CHROME=/path)'); process.exit(0); }
const server = await startServer();
const failures = [], rows = [];
try {
  if (!(await browser.hasWebGPU(server.origin))) { console.log('Rival page check SKIPPED: headless Chrome has no WebGPU adapter here'); }
  else for (const c of cases) {
    const t0 = Date.now(), { records, padFor, isolated } = readCapture(c.capture, c.ticks);
    await browser.goto(`${server.origin}/?qa=1&course=${c.course}&rider=zoe&cutscenes=0&quality=low${isolated ? '&isolate=1' : ''}`);
    await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
    const loadError = await browser.evaluate('document.body.dataset.loadError || ""');
    if (loadError) { failures.push(`${c.key}: load failed: ${loadError}`); continue; }
    // Single Event > Race|Freestyle > the rival entry (fe-event-select.js -> ui.startSingleEvent), then the objectives card.
    const entry = await browser.evaluate(`(()=>{const ui=ssxQA.ui(),e=ui.courses.find(x=>x.code===${JSON.stringify(c.course)}&&x.rivalMode===${c.mode});if(!e)return null;ui.startSingleEvent(e);return e.name})()`);
    if (!entry) { failures.push(`${c.key}: no Single Event entry for ${c.course} mode ${c.mode}`); continue; }
    await browser.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives' || !!document.body.dataset.loadError`, 300000);
    const lineup = await browser.evaluate('JSON.stringify(window.ssxAiRace?.lineup ?? null)');
    // The frame clock stops (dt 0 frames keep drawing): only ssxQA.advance ticks. Then the card's Continue.
    await browser.evaluate(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));
      ssxQA.ui().careerUI.play();
      window.__rivalRun=(pads)=>{const c=window.ssxEffects.core,a=window.ssxAiRace,n=a.racers.npcs[0].core,out=[];
        for(const p of pads){ssxQA.advance(1,p);out.push([...new Float32Array(c.HEAPF32.buffer,c._reference_motion(),6),...new Float32Array(c.HEAPF32.buffer,n._reference_motion(),6),...new Uint32Array(c.HEAPU8.buffer,c._animation_rng_words(),6)]);}
        return out;};return 1})()`);
    await sleep(300);
    const first = { human: null, rival: null, rng: null };
    for (let i = 0; i + 1 < records.length; i += 200) {
      const pads = []; for (let k = i; k < Math.min(i + 200, records.length - 1); k++) pads.push(padFor(records[k].index));
      const out = await browser.evaluate(`window.__rivalRun(${JSON.stringify(pads)})`);
      out.forEach((w, j) => { const rec = records[i + j + 1];
        if (!first.human && w.slice(0, 6).some((x, q) => Math.fround(x) !== rec.human[q])) first.human = rec.tick;
        if (!first.rival && w.slice(6, 12).some((x, q) => Math.fround(x) !== rec.rival[q])) first.rival = rec.tick;
        if (!first.rng && w.slice(12, 18).some((x, q) => x !== rec.rng[q])) first.rng = rec.tick; });
    }
    const resets = await browser.evaluate('(()=>{const a=window.ssxAiRace,c=window.ssxEffects.core;return new Float32Array(c.HEAPF32.buffer,a.racers.npcs[0].core._reset_info(),3)[2]})()');
    const who = JSON.parse(lineup);
    const through = (t) => (t == null ? `all ${records.length - 1}` : t - 1);
    rows.push(`${c.key} (${entry}, ${who?.rival}): human exact through ${through(first.human)}, rival ${through(first.rival)}, RNG ${through(first.rng)}; rival reset placements ${resets}; ${((Date.now() - t0) / 1000).toFixed(0)} s`);
    if (who?.rival !== c.rivalName) failures.push(`${c.key}: the rival is ${who?.rival}, not ${c.rivalName}`);
    if ((first.human ?? Infinity) <= c.human) failures.push(`${c.key}: the human left the PS2 at ${first.human} (the gate: exact through ${c.human}; ${c.why})`);
    if ((first.rival ?? Infinity) <= c.rival) failures.push(`${c.key}: ${c.rivalName} left the PS2 at ${first.rival} (the gate: exact through ${c.rival}; ${c.why})`);
    if ((first.rng ?? Infinity) <= c.rng) failures.push(`${c.key}: the game RNG differs at ${first.rng} (the gate: exact through ${c.rng})`);
    if (c.resets !== undefined && resets < c.resets) failures.push(`${c.key}: ${c.rivalName} made ${resets} reset placements (the PS2: ${c.resets} by tick ${c.ticks})`);
  }
} finally { await browser.close(); await server.close(); }
for (const r of rows) console.log(r);
if (failures.length) { console.error(`Rival page check FAILED:\n` + failures.join('\n')); process.exit(1); }
console.log(`Rival page check OK: ${rows.length} rival event(s) through the real Single Event path stay exact as far as the capture gate`);
process.exit(0);
