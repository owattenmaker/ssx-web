// In-race progress meter 0x20EDA0 (web/progress-meter-hud.js, docs/slopestyle-bigair.md "Progress meter").
// Needs web/public/assets/{ASS1,ARA1,BRA2}/progress-meter.json (tools/export_progress_meter.py). With the local captures:
//   * local/ps2-capture/runs/progress-meter/rnb-meter (R&B tuck/weave run, --isolate, --watch 0x4C8BC8:0x90 + Moby +0x4D0/+0x110):
//     the port fed by the capture's riders reproduces the six-rider entries on every frame; fed by the browser core
//     (compare-ps2-capture.mjs TICK_HOOK=progress-meter-compare.mjs, main.js's inputs) through the physics-exact span;
//   * local/ps2-capture/runs/event-race-ai (Snow Jam, five computer riders): the percent on the PS2 frames of event-race.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { ProgressMeter, DESC, SPRITES } from './progress-meter-hud.js';
import { add, sub, mul } from './ee-scalar-float.js';

const f = Math.fround;
const read = (p) => JSON.parse(fs.readFileSync(new URL('public/assets/' + p, import.meta.url)));
const runs = new URL('../local/ps2-capture/runs/', import.meta.url).pathname;

// --- the SSB kind-21 records: the same totals/markers as gp-0x9A0 in the R&B, Snow Jam and Metro-City savestates ------------
const rnb = read('ASS1/progress-meter.json'), jam = read('ARA1/progress-meter.json'), metro = read('BRA2/progress-meter.json');
assert.deepEqual([rnb.segments.length, rnb.total, rnb.markers], [248, 246217.46875, [[0, 0], [2, 246217.46875], [1, 69760.5], [1, 170814.921875]]], 'R&B path (start, finish, the two +60 checkpoints)');
assert.deepEqual([jam.segments.length, jam.total, jam.markers.length], [325, 353496.09375, 2], 'Snow Jam path');
assert.deepEqual([metro.segments.length, metro.total, metro.markers.length], [323, 349050.625, 2], 'Metro-City path');
assert.equal(f(rnb.total - 69760.5), f(246217.46875 - 69760.5)); // checkpoint 1 at remaining 176456.97 (0x4D33B8)
assert.ok([rnb, jam, metro].every((p) => p.segments.every((s, k) => k === 0 || s[4] >= p.segments[k - 1][4])), 'segment distances ascend');

// --- descriptor owner+0x50 from 0x20EC18 of the layout rect (20,110,80,240) and the fefont (height 29, scale 1) --------------
{
  const [x, y, w, h] = [20, 110, 80, 240], text = mul(f(29 * 1), f(0.666700005531311)), off = SPRITES.offup.h;
  const bottom = sub(sub(add(y, h), text), off);
  assert.deepEqual([add(x, mul(w, 0.5)), sub(add(y, h), text), bottom, add(y, off), mul(sub(sub(bottom, y), off), f(9.999999747378752e-05))],
    [DESC.textX, DESC.textY, DESC.bottom, DESC.barTop, DESC.scale], '0x20EC18 descriptor = the savestate words');
}

// --- draw list geometry at the start (progress 0, window -5000..5000) ---------------------------------------------------------
{
  const m = new ProgressMeter(rnb); m.tick([{ remaining: 246217.5, x: -113690.1875, y: 72750.765625, human: true }, { remaining: 246217.5, x: -113644.203125, y: 72896.6953125 }]);
  assert.deepEqual([m.entries[0].lateral, m.entries[1].lateral, m.entries[1].smooth], [61.61873245239258, -91.23715209960938, -152.8558807373047], 'anchor entries (R&B countdown savestate)');
  const list = m.frame(), bar = list.find((d) => d.name === 'radarline'), start = list.find((d) => d.name === 'chkstart');
  assert.equal(list[0].text, '0%');
  assert.deepEqual([bar.y0, bar.y1], [122, add(DESC.barBottom, mul(-5000, DESC.scale))], 'bar shortened below the start');
  assert.ok(start && list.filter((d) => d.name === 'encpu').length === 1 && list.filter((d) => d.name === 'ply').length === 1, 'start line, human and opponent arrows');
  assert.deepEqual(list.map((d) => d.order), [10, 9, 10, 12, 14, 11], 'GS priorities: text, bar, fill, ply, encpu, chkstart');
}

// --- capture: every frame of the R&B run from the PS2 riders ------------------------------------------------------------------
function rows(bin) {
  const man = JSON.parse(fs.readFileSync(bin.replace(/\.bin$/, '.capture.json'))), R = man.record, L = man.layout; let o = L.watch_offset; const W = {};
  for (const w of L.watches || []) { W[Number(w.address)] = o; o += w.length; }
  const buf = fs.readFileSync(bin), dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength), out = [];
  for (let k = 0; k < Math.floor(buf.length / R); k++) {
    const b = k * R, F = (x) => dv.getFloat32(b + x, true), I = (x) => dv.getInt32(b + x, true), moby = Number(man.others[0]);
    out.push({ tick: I(4), human: { remaining: F(32 + 0x3D0), x: F(32 + 0x10), y: F(32 + 0x14), human: true }, moby: { remaining: F(W[moby + 0x4D0]), x: F(W[moby + 0x110]), y: F(W[moby + 0x110] + 4) },
      ent: [0, 1].map((i) => { const e = W[0x4C8BC8] + 0x18 * i; return [I(e), I(e + 4), F(e + 8), F(e + 12), F(e + 16), I(e + 20)]; }) });
  }
  return out;
}
const rnbBin = runs + 'progress-meter/rnb-meter.bin';
if (fs.existsSync(rnbBin)) {
  const rs = rows(rnbBin), m = new ProgressMeter(rnb);
  rs[0].ent.forEach((e, i) => Object.assign(m.entries[i], { reset: e[0], seg: e[1], progress: e[2], lateral: e[3], smooth: e[4], flag: e[5] }));
  const jumped = (a, b) => Math.hypot(a.x - b.x, a.y - b.y) > 400; // rider placement 0x11D660 (teleport) -> 0x2105B0
  let exact = 0;
  // record r: +0..+0xC after the frame's update (0x20ED20) with record r's riders, +0x10/+0x14 after the previous draw
  for (let r = 1; r + 1 < rs.length; r++) {
    m.tick([{ ...rs[r].human, placed: jumped(rs[r].human, rs[r - 1].human) }, { ...rs[r].moby, placed: jumped(rs[r].moby, rs[r - 1].moby) }], 0, true);
    const placedNext = [jumped(rs[r + 1].human, rs[r].human), jumped(rs[r + 1].moby, rs[r].moby)];
    const want = rs[r].ent.map((e, i) => [...e.slice(0, 4), rs[r + 1].ent[i][4], placedNext[i] ? 1 : rs[r + 1].ent[i][5]]);
    const got = m.entries.slice(0, 2).map((e) => [e.reset, e.seg, e.progress, e.lateral, e.smooth, e.flag]);
    assert.deepEqual(got, want, `R&B meter entries at tick ${rs[r].tick}`); exact++;
    const pct = { 119: 0, 597: 5, 1538: 12, 3038: 25, 4539: 43 }[rs[r].tick]; // the PS2 snapshots rnb-meter.tick*.png
    if (pct !== undefined) assert.equal(m.percent(), pct, `percent on the PS2 frame at ${rs[r].tick}`);
  }
  console.log(`R&B meter: ${exact} frames exact (both riders: progress, segment, lateral, smoothing)`);
  // browser core as main.js feeds it (race_progress_info / rider_world_state / reset_info), Moby from the capture
  const report = new URL('../local/ps2-capture/runs/progress-meter/rnb-meter.meter.json', import.meta.url).pathname;
  await new Promise((resolve, reject) => execFile(process.execPath, ['compare-ps2-capture.mjs', rnbBin, '--pad', '--sync-rng', '--report', report, '--zoe', '--event'],
    { cwd: new URL('.', import.meta.url).pathname, env: { ...process.env, STAGE_WORLD: '1', TICK_HOOK: 'progress-meter-compare.mjs' }, maxBuffer: 1 << 28 }, (e, _o, err) => e ? reject(new Error(err?.slice(-2000) || e.message)) : resolve()));
  const s = JSON.parse(fs.readFileSync(report, 'utf8')).summary.progressMeter;
  const PHYSICS_EXACT = 6671; // peak1/rnb-event-tuck (same baseline and script): 6672 a control-3 soft collision lands 0.03 cm off
  assert.ok(!s.firstUpdate || s.firstUpdate.tick > PHYSICS_EXACT, `browser-fed meter update differs at ${JSON.stringify(s.firstUpdate)}`);
  assert.ok(!s.firstSmooth || s.firstSmooth.tick > PHYSICS_EXACT, `browser-fed meter smoothing differs at ${JSON.stringify(s.firstSmooth)}`);
  console.log(`R&B meter fed by the browser core: exact through ${s.firstUpdate ? s.firstUpdate.tick - 1 : 'the end'} (${s.exactUpdate}/${s.checked} updates)`);
} else console.log('skip: no local/ps2-capture/runs/progress-meter/rnb-meter capture');

// --- Snow Jam race: five computer riders (event-race-ai), percent on the event-race PS2 frames -------------------------------
const aiBin = runs + 'event-race-ai.bin';
if (fs.existsSync(aiBin)) {
  const man = JSON.parse(fs.readFileSync(aiBin.replace(/\.bin$/, '.capture.json'))), R = man.record, A = man.layout.ai_state;
  const buf = fs.readFileSync(aiBin), dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength), m = new ProgressMeter(jam), seen = {};
  for (let r = 0; r < Math.floor(buf.length / R); r++) {
    const b = r * R, F = (o) => dv.getFloat32(b + o, true), tick = dv.getInt32(b + 4, true);
    const riders = [{ remaining: F(32 + 0x3D0), x: F(32 + 0x10), y: F(32 + 0x14), human: true }];
    for (let s = 0; s < A.slots; s++) { const o = A.base + s * A.stride; riders.push({ remaining: F(o + 0x4D0), x: F(o + 0x110), y: F(o + 0x114) }); }
    m.tick(riders, 0, true);
    if ([918, 1618, 2318].includes(tick)) { const l = m.frame(); seen[tick] = [m.percent(), l.filter((d) => d.name === 'encpu').length, l.filter((d) => d.name === 'offup').length]; }
  }
  assert.deepEqual(seen, { 918: [6, 5, 0], 1618: [10, 4, 1], 2318: [16, 4, 1] }, 'Snow Jam percent, computer arrows in the window and offup arrows (event-race.tick*.png)');
  console.log('Snow Jam meter:', JSON.stringify(seen));
}
console.log('progress meter ok');
