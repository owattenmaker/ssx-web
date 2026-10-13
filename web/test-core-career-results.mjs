// pv coreCareerResults (docs/core-migration.md M5c): the career's result kernel (engine/career_rules.hpp) against the JS. Two careers
// in lockstep get the same events and rounds (race, freestyle with and without the live opponent, rival, peak runs, records), one
// with the switch off and one with it on: every result, every save and the kernel's call count are checked. The kernel runs in the
// rules module (web/runtime/rules.js; RULES_JS=<scratch rules.js>), or with CORE_JS=<core.js> in that course core (the page's
// setRulesCore); skipped when the module or core has no kernel.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { setPv } from './pv-flags.js';
import { loadRulesModule } from './rules-module.js';
import { setRulesCore } from './core-rules.js';
import { Career, MODE, MEDAL, RIDER_CHARACTER } from './career.js';

const data = JSON.parse(fs.readFileSync(new URL('./public/assets/CAREER/career.json', import.meta.url)));
const courseCore = process.env.CORE_JS ? await (await import((await import('node:url')).pathToFileURL(process.env.CORE_JS).href)).default() : null;
const module = courseCore ?? (await loadRulesModule());
if (!module) throw new Error('core-career-results: the rules module did not load');
let evals = 0;
const inner = module._rules_eval;
module._rules_eval = (fn, io, n) => {
  if (fn >= 28 && fn <= 32) evals++;
  return inner(fn, io, n);
};
// a kernel probe: an id the module does not have answers -1
{
  const io = module._malloc(64 * 8);
  const view = new Float64Array(module.HEAPU8.buffer, io, 64);
  view.set([1, 1, 1, 0, 1, 2, 3]);
  const has = inner(30, io, 7) >= 0;
  module._free(io);
  if (!has) {
    console.log('core-career-results: this rules module has no result kernel (skipped)');
    process.exit(0);
  }
}
if (courseCore) setRulesCore(courseCore);
else setPv('rulesModule', true);

let seed = 0x2545f491;
const rand = () => {
  seed ^= seed << 13;
  seed >>>= 0;
  seed ^= seed >>> 17;
  seed ^= seed << 5;
  seed >>>= 0;
  return seed;
};
const pick = (a) => a[rand() % a.length];
const clone = (v) => JSON.parse(JSON.stringify(v));
const draws = (s) => {
  let x = s;
  return () => {
    x = (Math.imul(x, 1664525) + 1013904223) >>> 0;
    return x;
  };
};
const make = () => {
  const c = new Career(data, { storage: null, save: null, rosterSeed: draws(7), presentation: draws(11) });
  c.persist = () => true;
  return c;
};
const off = make();
const on = make();
// one call on both careers: the switch off for the first, on for the second
function both(fn) {
  setPv('coreCareerResults', false);
  const a = fn(off);
  setPv('coreCareerResults', true);
  const b = fn(on);
  setPv('coreCareerResults', null);
  assert.deepEqual(b, a);
  const sa = JSON.stringify(off.save);
  const sb = JSON.stringify(on.save);
  if (sa !== sb) assert.fail(`save differs at ${firstDiff(JSON.parse(sa), JSON.parse(sb), 'save')}`);
  return a;
}
// the first path where two JSON values differ (the key order included)
function firstDiff(a, b, at) {
  if (typeof a !== 'object' || typeof b !== 'object' || a === null || b === null) return JSON.stringify(a) === JSON.stringify(b) ? null : `${at}: ${JSON.stringify(a)} / ${JSON.stringify(b)}`;
  const ka = Object.keys(a);
  const kb = Object.keys(b);
  if (ka.join() !== kb.join()) return `${at}: keys ${ka.join()} / ${kb.join()}`;
  for (const k of ka) {
    const d = firstDiff(a[k], b[k], `${at}.${k}`);
    if (d) return d;
  }
  return null;
}
const courses = data.courses.map((c, i) => i).filter((i) => off.standardMode(i) != null);
const riders = Object.keys(RIDER_CHARACTER);
const odd = () => pick([undefined, null, 0, 1e9, 0.5, Infinity, NaN]);
const steps = Number(process.env.STEPS ?? 3000);
let rounds = 0;
for (let n = 0; n < steps; n++) {
  const id = pick(riders);
  const course = pick(courses);
  let mode = off.standardMode(course);
  const roll = rand() % 10;
  if (roll === 0) mode = mode === MODE.RACE ? MODE.RIVAL_TIME : MODE.RIVAL_POINTS;
  const isCareer = rand() % 4 !== 0;
  both((c) => {
    c.startEvent(id, mode, course, isCareer);
    return null;
  });
  if (mode === MODE.RACE) {
    for (let round = 0; round < 3; round++) {
      const count = 1 + (rand() % 6);
      const human = rand() % count;
      const standings = Array.from({ length: count }, (_, k) => {
        const s = { name: `r${k}`, character: k, human: k === human };
        if (rand() % 12 === 0) s.dnf = true;
        if (rand() % 3) s.finishTicks = 4000 + (rand() % 5000);
        else if (rand() % 2) s.finishTicks = null;
        s.remaining = rand() % 25 === 0 ? odd() : rand() % 30000;
        if (rand() % 3) s.place = rand() % 6;
        if (rand() % 5 === 0) s.origin = rand() % 3 ? 30000 + (rand() % 20000) : odd();
        return s;
      });
      const raceTicks = 3000 + (rand() % 6000);
      const origin = rand() % 30 === 0 ? undefined : 30000 + (rand() % 30000);
      const out = both((c) => c.raceResult(clone(standings), { raceTicks, origin }));
      rounds++;
      if (!out.advance) break;
    }
  } else if (mode === MODE.RIVAL_TIME || mode === MODE.RIVAL_POINTS) {
    const win = rand() % 2 === 0;
    const ticks = rand() % 9 === 0 ? odd() : 3000 + (rand() % 6000);
    const score = rand() % 300000;
    both((c) => c.rivalResult({ win, values: [1, 2], place: win ? 0 : 1 }, { ticks, score }));
    rounds++;
  } else {
    if (rand() % 3 === 0) {
      const scores = [rand() % 90000, rand() % 90000, rand() % 90000];
      both((c) => {
        c.active.ev.opponent ??= { character: 1, scores: [...scores] };
        return null;
      });
    }
    for (let round = 0; round < 3; round++) {
      const score = rand() % 9 === 0 ? odd() : rand() % 400000;
      const opponent = rand() % 2 ? { finished: !!(rand() % 2), score: rand() % 300000, estimate: rand() % 300000 } : null;
      const out = both((c) => c.freestyleResult(score, opponent && { ...opponent }));
      rounds++;
      if (out.medal !== undefined || !out.advance) break;
    }
  }
  if (rand() % 4 === 0) {
    const pm = 6 + (rand() % 6);
    const run = { ticks: 2000 + (rand() % 20000), score: rand() % 900000 + (rand() % 2 ? 0.5 : 0), dnf: rand() % 8 === 0 };
    both((c) => {
      c.startEvent(id, pm, null, true);
      return c.peakRunResult({ ...run });
    });
    rounds++;
  }
}
assert.ok(evals > rounds, `the kernel answered (${evals} calls for ${rounds} rounds)`);
console.log(`core-career-results: ${rounds} rounds over ${steps} events identical off / on (${evals} kernel calls)`);
void MEDAL;
