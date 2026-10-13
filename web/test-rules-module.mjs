// The rules module (docs/core-migration.md M5; web/rules-module.js, web/build-rules.sh): its presentation generator gives the page's
// (web/lineup.js: boot, draws, the raced events' per-frame draws, the roster seeds) on long scripted sessions, every word and seed the
// same, the pending-draw sums with NaN models included. (Its rules_eval is the course core's: web/test-core-rules.mjs with
// CORE_JS=<rules.js> runs every M1 case through it.) RULES_JS=<scratch rules.js>; skipped when there is no module.
import assert from 'node:assert/strict';
import { setPv } from './pv-flags.js';
import { seededWords, nextWord, SEED_DRAW } from './lineup.js';
import { loadRulesModule } from './rules-module.js';

setPv('coreRules', false);
const t0 = performance.now();
const m = await loadRulesModule();
const loadMs = performance.now() - t0;
if (!m || !m._presentation_boot) {
  console.log('rules-module: no rules module (skipped)');
  process.exit(0);
}

// web/lineup.js's generator, statement for statement (its state is private to that module)
function pageGenerator(seed) {
  let presentation = null,
    pendingDraws = 0;
  const boot = () => {
    if (!presentation) {
      presentation = seededWords(seed);
      nextWord(presentation);
    }
  };
  return {
    note(model, ticks) {
      if (model) pendingDraws += Math.max(0, Math.round(model.after_seed + model.per_tick * Math.max(0, ticks - model.anchor_tick)));
    },
    draw() {
      boot();
      return nextWord(presentation);
    },
    rosterSeed() {
      boot();
      for (; pendingDraws > 0; pendingDraws--) nextWord(presentation);
      let s = 0;
      for (let n = 1; n < SEED_DRAW; n++) s = nextWord(presentation);
      return s;
    }
  };
}
// the module's, with the page's call sites (boot on the first use, the model's fields as JS numbers)
function moduleGenerator(seed) {
  const boot = () => {
    if (!m._presentation_booted()) m._presentation_boot(seed);
  };
  const num = (v) => (v === undefined ? NaN : v === null ? 0 : Number(v));
  return {
    note(model, ticks) {
      if (model) m._presentation_note(num(model.after_seed), num(model.per_tick), num(model.anchor_tick), num(ticks));
    },
    draw() {
      boot();
      return m._presentation_draw() >>> 0;
    },
    rosterSeed() {
      boot();
      return m._presentation_roster_seed() >>> 0;
    }
  };
}

let state = 0x1d872b41;
const rand = () => {
  state ^= state << 13;
  state >>>= 0;
  state ^= state >>> 17;
  state ^= state << 5;
  state >>>= 0;
  return state;
};
const models = [
  { after_seed: 412.5, per_tick: 1.875, anchor_tick: 18 },
  { after_seed: 0, per_tick: 0.4999999999999999, anchor_tick: 0 },
  { after_seed: 2.5, per_tick: 0, anchor_tick: 100 },
  { after_seed: -10, per_tick: 0.25, anchor_tick: 180 },
  { per_tick: 1 }
];
const seed = 0x182200;
const page = pageGenerator(seed),
  mod = moduleGenerator(seed);
let calls = 0;
for (let k = 0; k < 20000; k++) {
  const op = rand() % 10;
  if (op < 7) assert.equal(mod.draw(), page.draw(), `draw ${k}`);
  else if (op < 9) {
    const model = models[rand() % models.length], ticks = rand() % 12000;
    page.note(model, ticks);
    mod.note(model, ticks);
  } else assert.equal(mod.rosterSeed(), page.rosterSeed(), `roster seed ${k}`);
  calls++;
}
// the NaN model above left the pending draws NaN in both: no more per-frame draws are taken
assert(Number.isNaN(new Float64Array(m.HEAPU8.buffer, m._presentation_state(), 7)[6]), 'a NaN model keeps the pending draws NaN');
console.log(`rules-module: ${calls} presentation calls identical (load ${loadMs.toFixed(1)} ms in node)`);
