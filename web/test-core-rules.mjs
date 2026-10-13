// pv coreRules (docs/core-migration.md slice M1): every rule web/core-rules.js sends to the core (engine/game_rules.hpp,
// web/rules_bridge.cpp) gives the JS version's result bit for bit (Object.is on every number, NaN and -0 included), over
// generated inputs and the game's own tables (career.json, every lineups.json). Each case runs the public function twice:
// switch off (the JS version) and switch on with the core registered, and checks the core answered (its call count).
// CORE_JS=<scratch core.js> tests a scratch build. Skipped when the core has no rules_eval.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { setPv } from './pv-flags.js';
import { setRulesCore } from './core-rules.js';
import * as lineup from './lineup.js';
import * as career from './career.js';
import * as rival from './rival-mode.js';
import { estimateFinishTicks as raceEstimate, orderByTime } from './ai-race.js';
import { eventAnchorWords, EVENT_ANCHOR_DRAWS } from './event-anchor-rng.js';

const coreUrl = process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js';
const createCore = (await import(coreUrl)).default;
const core = await createCore();
if (!core._rules_eval) {
  console.log('core-rules: this core has no rules_eval (skipped)');
  process.exit(0);
}
let coreCalls = 0;
const evalRaw = core._rules_eval;
core._rules_eval = (fn, io, n) => {
  coreCalls++;
  return evalRaw(fn, io, n);
};
setRulesCore(core);

// ---- comparison --------------------------------------------------------------------------------------------------------
function same(a, b, path = '') {
  if (typeof a === 'number' || typeof b === 'number') {
    if (!Object.is(a, b)) return `${path}: ${a} != ${b}`;
    return null;
  }
  if (a === null || b === null || typeof a !== 'object' || typeof b !== 'object') return a === b ? null : `${path}: ${a} != ${b}`;
  const ka = Object.keys(a), kb = Object.keys(b);
  if (ka.length !== kb.length) return `${path}: keys ${ka} != ${kb}`;
  for (const k of ka) {
    const d = same(a[k], b[k], `${path}.${k}`);
    if (d) return d;
  }
  return null;
}
const clone = (v) => (v === undefined ? v : structuredClone(v));
let cases = 0;
const counts = {};
// fn(args): runs with the switch off and on, from copies of the arguments (some rules advance words in place); the
// arguments after the call are compared too. mustUseCore: the core has to answer this case.
function check(name, fn, args, mustUseCore = true) {
  const a = clone(args), b = clone(args);
  setPv('coreRules', false);
  let outJs, errJs = null;
  try {
    outJs = fn(...a);
  } catch (e) {
    errJs = String(e);
  }
  setPv('coreRules', true);
  const before = coreCalls;
  let outCore, errCore = null;
  try {
    outCore = fn(...b);
  } catch (e) {
    errCore = String(e);
  }
  setPv('coreRules', null);
  const used = coreCalls > before;
  if (errJs || errCore) assert.equal(errCore, errJs, `${name}(${JSON.stringify(args)}): errors differ`);
  else {
    const d = same(outJs, outCore, 'result') ?? same(a, b, 'arguments');
    assert.equal(d, null, `${name}(${JSON.stringify(args)}): ${d}`);
  }
  if (mustUseCore && !errJs) assert(used, `${name}(${JSON.stringify(args)}): the core did not answer`);
  counts[name] = (counts[name] ?? 0) + 1;
  cases++;
}

// ---- inputs ------------------------------------------------------------------------------------------------------------
let seed = 0x2468ace1;
const rand = () => {
  seed ^= seed << 13;
  seed >>>= 0;
  seed ^= seed >>> 17;
  seed ^= seed << 5;
  seed >>>= 0;
  return seed;
};
const pick = (list) => list[rand() % list.length];
const randomWords = () => Array.from({ length: 6 }, () => rand());
const floats = [0, -0, 1, -1, 0.5, 30, 29.999998, 1e-30, 3.4e38, 1e39, -1e39, Infinity, -Infinity, NaN, 113130, 98765.4321, 1000, 999.5, 1e6, 360000];
const randomFloat = () => (rand() % 4 === 0 ? pick(floats) : ((rand() / 0x100000000) * 2 - 0.5) * pick([1, 10, 1000, 1e5, 1e7]));

// generator and seeds
for (let k = 0; k < 400; k++) check('seededWords', lineup.seededWords, [k < 200 ? rand() : pick([0, 1, 0xffffffff, -1, 2 ** 32, 1.5, NaN])]);
for (let k = 0; k < 2000; k++) {
  const w = k % 5 === 0 ? [0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff - (k % 3)] : randomWords();
  check('nextWord', lineup.nextWord, [w]);
}
check('nextWord(Uint32Array)', (w) => [lineup.nextWord(w), Array.from(w)], [Uint32Array.from(randomWords())]);
for (const d of [new Date(2026, 9, 13, 1, 2, 3), new Date(1999, 11, 31, 23, 59, 59), new Date(2000, 0, 1, 0, 0, 0), new Date(2031, 5, 15, 12, 30, 7)])
  check('clockSeed', lineup.clockSeed, [d]);
for (let peak = -1; peak <= 4; peak++) for (let human = -1; human <= 11; human++) check('peakRival', lineup.peakRival, [peak, human]);

// rosters
for (let k = 0; k < 600; k++) check('buildRosterFrom', lineup.buildRosterFrom, [randomWords(), rand() % 12 - 1, rand() % 5 - 1]);
for (let k = 0; k < 200; k++) check('buildRoster', lineup.buildRoster, [rand(), rand() % 10, rand() % 4]);
for (let k = 0; k < 800; k++) {
  const entries = lineup.buildRoster(rand(), rand() % 10, rand() % 3);
  const previous = Array.from({ length: rand() % 5 }, () => (rand() % 4 === 0 ? null : rand() % 17));
  const round = pick([1, 2, 3, 4, 0]);
  check('roundEntries', lineup.roundEntries, [entries, { round, career: !!(rand() & 1), rival: rand() % 10, previous }]);
}
for (let k = 0; k < 400; k++) check('freestyleRoster', lineup.freestyleRoster, [randomWords(), rand() % 11 - 1, rand() % 11 - 1]);
for (let human = 0; human < 10; human++)
  for (let round = 1; round <= 3; round++)
    check('lineupFor', lineup.lineupFor, [{ seed: rand(), human: { base: human, cheat: 0 }, peak: rand() % 3, round, career: true, previous: [rand() % 10, null, rand() % 17] }]);

// relationships: every record byte and event
const hex = (b) => (b & 255).toString(16).padStart(2, '0');
for (let kind = -1; kind <= 4; kind++)
  for (let score = -128; score <= 127; score++) {
    const record = hex(kind) + hex(rand() % 7) + hex(score);
    check('relationshipAge', lineup.relationshipAge, [record]);
    for (const event of [-1, 0, 1, 2, 3, 4, 1.5]) check('relationshipEvent', lineup.relationshipEvent, [record, event], Number.isInteger(event));
  }

// difficulty and roles
for (let slot = 0; slot <= 6; slot++)
  for (let level = -1; level <= 3; level++)
    for (let course = -1; course <= 5; course++) check('npcDifficulty', lineup.npcDifficulty, [slot, level, course], slot >= 1 && slot <= 5 && level >= 0 && level <= 2);
for (let slot = 0; slot <= 6; slot++) for (let round = 0; round <= 4; round++) check('npcRoundRole', lineup.npcRoundRole, [slot, round]);

// anchor RNG words
for (const code of [...Object.keys(EVENT_ANCHOR_DRAWS), 'ARA1', 'XXXX', 'ABA'])
  for (let base = 0; base < 10; base++) for (const cheat of [0, 12]) check('eventAnchorWords', eventAnchorWords, [code, { base, cheat }], code.length === 4);

// lineups.json documents: assembled lineups (relationship ageing, scores, difficulty, roles) and the anchor words
const root = new URL('public/assets/', import.meta.url);
const courses = fs.readdirSync(root).filter((c) => fs.existsSync(new URL(`${c}/lineups.json`, root)));
for (const course of courses) {
  const data = JSON.parse(fs.readFileSync(new URL(`${course}/lineups.json`, root)));
  for (let base = 0; base < 10; base++) {
    for (const cheat of [0, 14]) check(`anchorRandomWords ${course}`, lineup.anchorRandomWords, [data, { base, cheat }]);
    const values = lineup.buildRoster(rand(), base, data.peak ?? 0).slice(0, data.template?.riders?.length ?? 5);
    for (const round of [null, 1, 2, 3])
      for (const level of [null, 0, 1, 2]) {
        const assemble = (d) => {
          try {
            return lineup.assembleLineup(d, base, values, { round, level });
          } catch (e) {
            return String(e);
          }
        };
        check(`assembleLineup ${course}`, assemble, [data], false);
      }
  }
}

// estimates and rankings
for (let k = 0; k < 3000; k++) {
  const raceTicks = rand() % 8 === 0 ? pick(floats) : rand() % 20000;
  const args = [raceTicks, randomFloat(), randomFloat(), rand() % 7];
  check('ai-race estimateFinishTicks', raceEstimate, args);
  check('rival estimateTicks', rival.estimateTicks, args);
  check('career estimateFinishTicks', career.estimateFinishTicks, [{ raceTicks, origin: args[1], remaining: args[2], place: args[3] }, pick([30, 25, 30.5])]);
  check('rival estimatePoints', rival.estimatePoints, [rand() % 4 === 0 ? pick(floats) : (rand() % 200000) - 1000, randomFloat(), randomFloat()]);
  check('career opponentEstimate', career.Career.opponentEstimate, [(rand() % 200000) - 1000, randomFloat(), randomFloat()]);
}
for (let k = 0; k < 1500; k++) {
  const n = 1 + (rand() % 6);
  const values = Array.from({ length: n }, () => (rand() % 5 === 0 ? pick([0, -0, Infinity, -Infinity, 2 ** 31, -(2 ** 31), 2 ** 32 + 5]) : (rand() % 9) - 2));
  check('rankEntries', career.rankEntries, [values, !!(rand() & 1)]);
  check('orderByTime', orderByTime, [values]);
  const rows = values.map((v, i) => ({ human: i === 0, dnf: rand() % 7 === 0, finishTicks: rand() % 3 ? null : rand() % 9000, score: v * 1000, remaining: randomFloat(), origin: randomFloat() }));
  check('rivalResult', rival.rivalResult, [pick([4, 5]), rows, rand() % 9000]);
}
check('rankEntries NaN (JS)', career.rankEntries, [[3, NaN, 1], true], false);

// medals, records, difficulty, scores (career.json)
const rules = JSON.parse(fs.readFileSync(new URL('CAREER/career.json', root)));
for (let mode = -1; mode <= 12; mode++) {
  for (let rank = -1; rank <= 6; rank++) check('placementMedal', career.placementMedal, [mode, rank]);
  for (let course = -2; course <= 19; course++) {
    check('recordSlot', career.recordSlot, [rules, mode, course], course >= 0 || (mode >= 6 && mode <= 11));
    check('platinumThreshold', career.platinumThreshold, [rules, mode, course], false);
  }
  check('recordSlot (no course)', career.recordSlot, [rules, mode, null]);
}
for (let k = 0; k < 500; k++) {
  const row = [rand() % 100, rand() % 80, rand() % 60, rand() % 40];
  check('exploreMedal', career.exploreMedal, [rand() % 120, row]);
}
for (let level = -1; level <= 3; level++) for (let counter = -3; counter <= 3; counter++) for (const up of [true, false]) check('adaptLevel', career.adaptLevel, [{ level, counter }, up]);
for (const row of rules.freestyle_ai ?? [])
  for (let ai = 0; ai < 5; ai++)
    for (let level = 0; level <= 2; level++) {
      const draw = rand();
      check('aiFreestyleScore', (r) => career.aiFreestyleScore(r, row.course, row.round, ai, level, () => draw), [rules], row.scores?.[ai] !== undefined);
    }
for (let k = 0; k < 2000; k++) {
  const p = { golds: rand() % 15, rivals: rand() % 13, challenges: rand() % 40, challengeTotal: pick([0, 33, 40, 88]), collected: rand() % 300, collectTotal: pick([0, 299, 300]), highlights: rand() % 25 };
  check('mountainPercent', career.mountainPercent, [p]);
}
check('mountainPercent full', career.mountainPercent, [{ golds: 14, rivals: 12, challenges: 88, challengeTotal: 88, collected: 300, collectTotal: 300, highlights: 24 }]);
for (let peak = 0; peak <= 4; peak++) for (let human = -1; human <= 10; human++) check('rivalCharacter', rival.rivalCharacter, [peak, human], peak >= 1 && peak <= 3);

console.log(`core-rules: ${cases} cases identical (core calls ${coreCalls}): ${Object.entries(counts).map(([k, v]) => `${k} ${v}`).join(', ')}`);
