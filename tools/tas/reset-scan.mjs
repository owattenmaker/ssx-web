// Where does a reset put the rider? (docs/tas.md "Resets"). Along a run, at every --step ticks: from that tick's state press
// Select (manual reset, the provider's ResetPath) for two ticks, then let the policy's default macro ride for --after ticks,
// and compare the route's remaining distance with the run itself --after ticks later. A positive gain means the reset placed
// the rider ahead (112D58 picks the reset route among the nearest candidate paths; 11D660 places).
//   node tools/tas/reset-scan.mjs --start START.json --guide GUIDE.json PAD.tas [--from 300] [--to 9000] [--step 50] [--after 180]
import fs from 'node:fs';
import { createTasRace } from './race.mjs';
import { parse, toChannels, neutralFrame, withButton } from './pad-format.mjs';
import { makeGuide } from './guide.mjs';
import { DEFAULT_MACRO, cloneMemory, newPolicyMemory, policyFrame } from './policy.mjs';

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const padPath = args.find((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
const start = JSON.parse(fs.readFileSync(opt('start'), 'utf8'));
const guide = makeGuide(JSON.parse(fs.readFileSync(opt('guide'), 'utf8')).points);
const { frames } = parse(fs.readFileSync(padPath, 'utf8'));
const FROM = +opt('from', 300);
const TO = Math.min(+opt('to', 9000), frames.length - 1);
const STEP = +opt('step', 50);
const AFTER = +opt('after', 180);
const race = await createTasRace({ start });
const core = race.core;
const f32 = (p, n) => new Float32Array(core.HEAPF32.buffer, p, n);
const boost = () => f32(core._boost_info(), 6);
const mem = newPolicyMemory();
// one pass: the run, and at each sampled tick a branch (save, reset, ride, restore). The run's own remaining distance AFTER ticks
// later is read when the run gets there.
const pending = [];
const rows = [];
const end = Math.min(TO + AFTER, frames.length - 1);
for (let t = 0; t <= end; t++) {
  if (t >= FROM && t <= TO && (t - FROM) % STEP === 0) {
    const save = race.save();
    const m = cloneMemory(mem);
    const resetsBefore = f32(core._reset_info(), 3)[2];
    for (let k = 0; k < AFTER; k++) {
      let f = policyFrame(guide, DEFAULT_MACRO, m, race.state(), boost());
      if (k < 2) f = withButton(neutralFrame(), 'Select');
      race.tick(toChannels(f));
    }
    pending.push({ tick: t, branch: race.progress()[0], resets: f32(core._reset_info(), 3)[2] - resetsBefore });
    race.restore(save);
  }
  policyFrame(guide, DEFAULT_MACRO, mem, race.state(), boost());
  race.tick(toChannels(frames[t]));
  for (const p of pending) {
    if (p.tick + AFTER - 1 === t) rows.push({ tick: p.tick, gainMetres: +((race.progress()[0] - p.branch) / 100).toFixed(1), resets: p.resets });
  }
}
rows.sort((a, b) => b.gainMetres - a.gainMetres);
console.log(JSON.stringify(rows.slice(0, 15)));
