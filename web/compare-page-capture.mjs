// The REAL page flow against a PS2 capture (docs/HANDOFF.md "Page = PS2 at Crow's Nest"): headless Chrome loads the page, the
// Single Event menu call starts the capture's course, the objectives card's Continue starts the run, the countdown lead-in runs
// to the capture's first tick, then ssxQA.advance replays the capture's pad. Per tick: the human's motion (+0x110 / +0x1E0),
// the shared game RNG 0x4FF030 and the visual RNG words; printed: the first tick each differs from the capture.
// Use it when the page and compare-ps2-capture.mjs disagree (the comparer copies the PS2's RNG each tick with --sync-rng).
//   node compare-page-capture.mjs CAPTURE_REL [--extra '&pv=..'] [--out OUT.json] [--ticks N] [--pro] [--rival 4|5]
// The course is the manifest's location; ?isolate=1 follows the capture (tools/ps2_capture.py --isolate); a course with lineups.json
// races its anchor lineup (?lineupSeed = lineups.json anchor.seed, the savestate's roster); --pro sets Controller Settings Pro
// (INPUT2.MAP); --rival picks the backcountry Single Event entry of that mode (4 Rival Time, 5 Rival Points).
import fs from 'node:fs';
const WEB = new URL('./', import.meta.url).pathname;
const { startServer, startBrowser, sleep } = await import(WEB + 'headless-chrome.mjs');
const argv = process.argv.slice(2), opt = (k) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : null; };
const cap = argv[0], outFile = opt('--out'), ticksArg = opt('--ticks'), pro = argv.includes('--pro'), rivalMode = opt('--rival') ? +opt('--rival') : 0;
const runs = new URL('../local/ps2-capture/runs/', import.meta.url).pathname;
const manifest = JSON.parse(fs.readFileSync(runs + cap + '.capture.json', 'utf8')), RECORD = manifest.record;
const course = opt('--course') || manifest.location || 'ARA1';
let extra = opt('--extra') || '';
if (manifest.isolated_from_computer_riders && !/[?&]isolate=/.test(extra)) extra += '&isolate=1';
let refExtra = 0;
{ const lp = new URL(`public/assets/${course}/lineups.json`, `file://${WEB}`).pathname; if (fs.existsSync(lp) && !/lineupSeed=/.test(extra)) { const d = JSON.parse(fs.readFileSync(lp, 'utf8')), a = d.anchor; if (a?.seed != null) extra += `&lineupSeed=${a.seed}`; refExtra = d.load_draws?.reference_anchor_extra ?? 0; } }
if (manifest.pokes?.length) console.warn(`${cap}: the capture pokes ${manifest.pokes.length} EE word(s) the page cannot set`);
const raw = fs.readFileSync(runs + cap + '.bin'), N = Math.min(Math.floor(raw.length / RECORD), (+ticksArg || 1e9) + 1);
const BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown', 'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
function towardZero(x) { let r = Math.fround(x); if (Math.abs(r) > Math.abs(x)) { const b = new Float32Array([r]); new Uint32Array(b.buffer)[0] -= 1; r = b[0]; } return r; }
const axisByte = (v) => Math.floor((Math.max(-1, Math.min(1, v)) + 1) * 127.5 + 0.5);
function decodePad(seg) { const values = new Array(24).fill(0), held = new Set(seg.buttons || []);
  BUTTONS.forEach((name, i) => { const on = held.has(name); values[i] = i >= 4 ? towardZero((on ? 255 : 0) * R255) : (on ? 1 : 0); });
  [axisByte(seg.rx || 0), axisByte(-(seg.ry || 0)), axisByte(seg.lx || 0), axisByte(-(seg.ly || 0))].forEach((b, axis) => { const neg = Math.max(Math.trunc((79 - b) * 255 / 79), 0), pos = Math.max(Math.trunc((b - 176) * 255 / 79), 0); values[16 + 2 * axis] = towardZero(neg * R255); values[17 + 2 * axis] = towardZero(pos * R255); });
  return values; }
const ends = []; let end = 0; for (const seg of manifest.segments) { end += seg.frames; ends.push({ end, pad: decodePad(seg) }); }
const padFor = (index) => (ends.find((e) => index < e.end) || ends[ends.length - 1]).pad;
const records = []; for (let i = 0; i < N; i++) { const o = i * RECORD; records.push({ tick: raw.readUInt32LE(o + 4), index: raw.readUInt32LE(o + 28) }); }
const server = await startServer(), b = await startBrowser({ width: 640, height: 480 });
const out = { rows: [] };
try {
  await b.goto(`${server.origin}/?qa=1&course=${course}&rider=zoe&cutscenes=0&quality=low&mute=1${extra}`);
  await b.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  console.log('event', await b.evaluate(`(()=>{const ui=ssxQA.ui();${pro ? 'const o=ui.feScreens?.extra?.options;if(o)o.controller1=1;' : ''}const e=ui.courses.find(x=>x.code===${JSON.stringify(course)}&&(${rivalMode}?x.rivalMode===${rivalMode}:!x.rivalMode));if(!e)return null;ui.startSingleEvent(e);return e.name})()`), extra);
  await b.waitFor(`document.getElementById('stage')?.dataset.screen === 'ctm-objectives' || !!document.body.dataset.loadError`, 300000);
  out.lineup = await b.evaluate('JSON.stringify(window.ssxAiRace?.lineup ?? null)');
  // A lineup course whose reference captures were reached through the peak-1-selection path hold one load draw more than the
  // direct path the page follows (lineups.json load_draws.reference_anchor_extra, docs/characters.md): start from the capture's words.
  if (refExtra) { const w = Array.from({ length: 6 }, (_, k) => raw.readUInt32LE(8896 + 4 * k)); await b.evaluate(`(window.ssxAiRace?.racers?.setAnchorRng(${JSON.stringify(w)}),1)`); console.log('reference anchor path: +' + refExtra + ' load draw'); }
  await b.evaluate(`(()=>{const raf=window.requestAnimationFrame.bind(window),at=performance.now();window.requestAnimationFrame=cb=>raf(()=>cb(at));
    ssxQA.ui().careerUI.play();
    window.__run=(pads)=>{const c=window.ssxEffects.core,out=[];for(const p of pads){ssxQA.advance(1,p);
      out.push([...new Float32Array(c.HEAPF32.buffer,c._reference_motion(),6),...new Uint32Array(c.HEAPU8.buffer,c._animation_rng_words(),6),...(c._visual_rng_words?new Uint32Array(c.HEAPU8.buffer,c._visual_rng_words(),6):[])]);}return out;};return 1})()`);
  await sleep(300);
  await b.evaluate(`(()=>{for(let t=0;t<${records[0].tick};t++)ssxQA.advance(1,new Array(24).fill(0));return 1})()`); // the comparer's --event lead-in to the countdown anchor
  for (let i = 0; i + 1 < records.length; i += 200) {
    const pads = []; for (let k = i; k < Math.min(i + 200, records.length - 1); k++) pads.push(padFor(records[k].index));
    const rows = await b.evaluate(`window.__run(${JSON.stringify(pads)})`);
    rows.forEach((r, j) => out.rows.push({ tick: records[i + j + 1].tick, motion: r.slice(0, 6), rng: r.slice(6, 12), vis: r.slice(12, 18) }));
  }
} finally { await b.close(); await server.close(); }
if (outFile) fs.writeFileSync(outFile, JSON.stringify(out));
const u = (o) => raw.readUInt32LE(o), f = (o) => raw.readFloatLE(o), at = new Map(records.map((r, i) => [r.tick, i * RECORD]));
// The record's RNG words are sampled at the human's provider exit, mid-tick: draws of the tick's earlier passes (the group-1 entity
// pass, e.g. a stage program) are in record N but in the page's words only after tick N. Such one-tick blips resync; the
// report names the first difference that persists over two records.
let motion = null, rng = null, blips = 0; const hasRng = RECORD >= 16384;
out.rows.forEach((r, j) => { const o = at.get(r.tick); if (o == null) return;
  const m = [0x110, 0x114, 0x118, 0x1E0, 0x1E4, 0x1E8].map((x) => f(o + 32 + x - 0x100));
  if (motion == null && r.motion.some((x, k) => Math.fround(x) !== m[k])) motion = r.tick;
  if (!hasRng || rng) return;
  const differs = (row) => { const q = row && at.get(row.tick); return q != null && row.rng.some((x, k) => x !== u(q + 8896 + 4 * k)); };
  if (differs(r)) { if (differs(out.rows[j + 1])) rng = { tick: r.tick, drawsAhead: (r.rng[5] - u(o + 8896 + 20)) | 0 }; else blips++; } });
console.log(`${cap}: ${out.rows.length} ticks; human motion exact through ${motion == null ? 'the end' : motion - 1}; shared RNG ${!hasRng ? 'not recorded' : rng ? `first persistent difference at ${rng.tick} (${rng.drawsAhead} draws ahead)` : 'exact'}${blips ? ` (${blips} one-tick blips before it)` : ''}`);
process.exit(0);
