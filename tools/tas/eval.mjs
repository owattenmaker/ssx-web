// Run a TAS pad file in the node harness and report the run (docs/tas.md): every --every ticks the remaining distance, speed,
// boost meter / Uber tier / Super time, crashes, resets and booth teleports; then the finish.
//   node tools/tas/eval.mjs --start START.json PAD.tas [--every 500] [--ticks N] [--trace out.json]
import fs from 'node:fs';
import { createTasRace } from './race.mjs';
import { parse, toChannels, neutralFrame } from './pad-format.mjs';

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const padPath = args.find((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
const start = JSON.parse(fs.readFileSync(opt('start'), 'utf8'));
const every = +opt('every', 500);
const { frames } = parse(fs.readFileSync(padPath, 'utf8'));
const limit = +opt('ticks', frames.length + 2000);
const race = await createTasRace({ start });
const core = race.core;
const f32 = (p, n) => new Float32Array(core.HEAPF32.buffer, p, n);
const neutral = toChannels(neutralFrame());
let crashes = 0;
let wasCrash = false;
let fin = null;
const trace = [];
for (let t = 0; t < limit && !fin; t++) {
  const o = race.tick(t < frames.length ? toChannels(frames[t]) : neutral);
  const crash = f32(core._crash_info(), 1)[0] !== 0;
  if (crash && !wasCrash) crashes++;
  wasCrash = crash;
  if (o.finish) fin = o.finish;
  const s = race.state();
  const b = f32(core._boost_info(), 6);
  const p = race.progress();
  const tele = core._stage_teleport_info ? f32(core._stage_teleport_info(), 1)[0] : 0;
  if (opt('trace')) trace.push([t, s[0], s[1], s[2], s[7], s[8], p[0], b[0], b[3], b[5], crash ? 1 : 0, s[13], tele]);
  if (t % every === 0 || o.finish) {
    console.log(`${t} rem ${p[0].toFixed(0)} v ${s[7].toFixed(1)} meter ${b[0].toFixed(2)} tier ${b[3]} super ${b[5].toFixed(1)} crashes ${crashes} resets ${s[13]} teleports ${tele}`);
  }
}
console.log('finish', JSON.stringify(fin), `(${fin ? (fin.ticks / 60).toFixed(2) + ' s' : 'none'}), pad ticks ${frames.length}`);
if (opt('trace')) fs.writeFileSync(opt('trace'), JSON.stringify(trace));
