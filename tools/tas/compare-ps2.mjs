// A PS2 capture of a TAS pad (tools/tas/ps2_tas_capture.py + tools/ps2_capture.py run) against the node harness
// (tools/tas/race.mjs, the page's race) on the same pad, record by record (docs/tas.md "Proof").
//   node tools/tas/compare-ps2.mjs --start START.json PAD.tas RUN.bin [--manifest STATE.capture.json] [--tol 0.01]
//
// Record k holds the riders' state after the tick before it and the shared RNG at the start of its tick (tools/ps2_capture.py:
// the provider exit 0x128630). The record's tick field is the game tick; the page's run tick t is game tick t (the countdown
// state is game tick 18 = the page's tick 18). Compared: the shared RNG words (exact), the human's position (rider +0x110, cm, the
// PS2 frame: native = (x, z, -y) / 100) and the five computer riders' positions (record 3008 + 32 k, roster order), and the finish
// times 0x536640 / places 0x536730 when the capture watched them.
import fs from 'node:fs';
import { createTasRace } from './race.mjs';
import { parse, toChannels, neutralFrame } from './pad-format.mjs';

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const files = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
const [padPath, binPath] = files;
const manifestPath = opt('manifest') ?? (fs.existsSync(binPath.replace(/\.bin$/, '.capture.json')) ? binPath.replace(/\.bin$/, '.capture.json') : null);
if (!manifestPath) throw new Error('no manifest: --manifest STATE.capture.json');
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
const start = JSON.parse(fs.readFileSync(opt('start'), 'utf8'));
const { frames } = parse(fs.readFileSync(padPath, 'utf8'));
const RECORD = manifest.record || 16384;
const raw = fs.readFileSync(binPath);
const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
const watchOffset = manifest.layout?.watch_offset;
const watches = manifest.layout?.watches || [];
const records = [];
for (let at = 0; at + RECORD <= raw.length; at += RECORD) {
  const u = (o) => dv.getUint32(at + o, true);
  const f = (o) => dv.getFloat32(at + o, true);
  const rec = { tick: u(4), index: u(28), position: [f(32 + 0x10), f(32 + 0x14), f(32 + 0x18)], rng: Array.from({ length: 6 }, (_, k) => u(8896 + 4 * k)),
    others: [0, 1, 2, 3, 4].map((k) => [f(3008 + 32 * k), f(3012 + 32 * k), f(3016 + 32 * k)]) };
  let o = watchOffset;
  for (const w of watches) {
    if (Number(w.address) === 0x536640) rec.times = Array.from({ length: w.length / 4 }, (_, k) => u(o + 4 * k));
    if (Number(w.address) === 0x536730) rec.places = Array.from({ length: w.length / 4 }, (_, k) => u(o + 4 * k));
    o += w.length;
  }
  records.push(rec);
}
console.log(`${records.length} records, game ticks ${records[0]?.tick}..${records[records.length - 1]?.tick}`);
// the node run: the RNG at the start of each tick and the states after each tick
const race = await createTasRace({ start });
const f32 = (c, p, n) => new Float32Array(c.HEAPF32.buffer, p, n);
const lastTick = records[records.length - 1].tick + 1;
const rngAt = [];
const human = [];
const npcs = [];
const neutral = toChannels(neutralFrame());
let finish = null;
for (let t = 0; t <= lastTick; t++) {
  rngAt.push(Array.from(race.rng()));
  const o = race.tick(t < frames.length ? toChannels(frames[t]) : neutral);
  if (o.finish && !finish) finish = o.finish;
  human.push(race.state().slice(0, 3));
  npcs.push(race.aiRace.racers.npcs.map((n) => f32(n.core, n.core._rider_state(), 3).slice()));
}
const native = (p) => [p[0] / 100, p[2] / 100, -p[1] / 100];
const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
let firstRng = -1;
let firstPos = -1;
let maxErr = 0;
let compared = 0;
for (const r of records) {
  const t = r.tick;
  // the countdown state's own record (game tick 18) already has the anchor RNG the page's race sets inside its tick 18
  if (t <= 18 || t > lastTick) continue;
  compared++;
  if (firstRng < 0 && r.rng.some((w, k) => w !== rngAt[t][k] >>> 0)) firstRng = t;
  const err = dist(native(r.position), human[t - 1]) * 100;
  maxErr = Math.max(maxErr, firstRng < 0 ? err : maxErr);
  if (firstPos < 0 && err > +opt('tol', 0.01)) firstPos = t;
}
const last = records[records.length - 1];
console.log(JSON.stringify({ compared, firstRngDifference: firstRng, firstPositionDifference: firstPos, maxHumanErrCmBeforeRng: +maxErr.toFixed(4),
  nodeFinish: finish, ps2Times: last.times ?? null, ps2Places: last.places ?? null }));
if (firstPos >= 0) {
  const r = records.find((x) => x.tick === firstPos);
  console.log('at', firstPos, 'ps2', native(r.position), 'node', Array.from(human[firstPos - 1]));
}
if (args.includes('--show')) {
  for (const r of records.slice(0, 6)) console.log(r.tick, r.index, r.rng.join(','), '| node start', rngAt[r.tick].join(','), '| node next', rngAt[r.tick + 1].join(','));
}
