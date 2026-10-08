// The search's macro menu (tools/tas/policy.mjs macros; docs/tas.md "Search"), with its own deterministic generator.
import { DEFAULT_MACRO } from './policy.mjs';

let seed = 1;
export function seedRandom(s) {
  seed = s >>> 0 || 1;
}
export function rand() {
  seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
  return seed / 4294967296;
}
const pick = (a) => a[Math.floor(rand() * a.length)];
const span = (lo, hi) => lo + Math.floor(rand() * (hi - lo + 1));

export const GRABS = [['R1'], ['L1'], ['R2'], ['L2'], ['R1', 'L1'], ['R2', 'L2'], ['R1', 'R2'], ['L1', 'L2']];
export const SPINS = ['DPadLeft', 'DPadRight'];
export const FLIPS = ['DPadUp', 'DPadDown'];

// An air program. Outside Tricky: a spin (or flip) from the take-off and a grab inside it, for the points that fill the meter.
// In Tricky (Super time > 0): Ubers (0x1352A8; several in one air all commit at the landing, docs/tricks-scoring.md).
export function randomProgram(tricky) {
  const steps = [];
  // In Tricky, mostly an Uber chain: 1..5 Ubers (a grab combination with Square, held all through or Square from 2 ticks in), small
  // gaps between them. One long air takes 3 or 4 (local/tas runs: a 195-tick air gave tier 1 -> 4 with a 4-Uber chain).
  if (tricky && rand() < 0.75) {
    const n = 1 + Math.floor(rand() * 5);
    let u = span(1, 8);
    for (let k = 0; k < n; k++) {
      const len = span(6, 45);
      const g = pick(GRABS);
      if (rand() < 0.5) {
        steps.push({ from: u, to: u + len - 1, press: [...g, 'Square'] });
      } else {
        steps.push({ from: u, to: u + len - 1, press: [...g] });
        steps.push({ from: u + 2, to: u + len - 1, press: ['Square'] });
      }
      u += len + span(0, 8);
    }
    return { steps };
  }
  const r = rand();
  if (r < 0.6) steps.push({ from: 1, to: span(15, 150), press: [pick(SPINS)] });
  else if (r < 0.75) steps.push({ from: 1, to: span(15, 120), press: [pick(FLIPS)] });
  const g0 = span(2, 20);
  steps.push({ from: g0, to: g0 + span(10, 120), press: pick(GRABS) });
  return { steps };
}

// count macros for a node whose last macro was prev: its continuation first, the default, then random edits of it.
export function candidates(prev, grounded, tricky, count) {
  const base = { ...DEFAULT_MACRO, ...prev, jump: false, pre: null, program: null, cancel: false };
  const out = [base];
  if (grounded) out.push({ ...DEFAULT_MACRO });
  while (out.length < count) {
    const m = { ...base };
    const r = rand();
    if (grounded) {
      if (r < 0.3) {
        m.offset = Math.max(-8, Math.min(8, (base.offset ?? 0) + pick([-4, -2, -1, 1, 2, 4])));
      } else if (r < 0.55) {
        m.jump = true;
        if (rand() < 0.5) m.pre = pick(SPINS);
        m.program = randomProgram(tricky);
      } else if (r < 0.75) {
        m.program = randomProgram(tricky);
      } else if (r < 0.82) {
        m.boost = pick(['off', 'auto', 'on']);
      } else if (r < 0.88) {
        // a turn too fast for the tuck: brake, or just stand up
        const b = rand() < 0.5;
        m.brake = b;
        m.tuck = !b && rand() < 0.5 ? false : !b;
        if (b) m.boost = 'off';
      } else {
        m.look = pick([8, 12, 16, 22]);
      }
    } else if (r < 0.3) {
      m.cancel = true;
    } else if (r < 0.7) {
      // in the air: cancel the running program, then start this one (tools/tas/policy.mjs)
      m.program = randomProgram(tricky);
      m.cancel = true;
    } else {
      m.offset = Math.max(-8, Math.min(8, (base.offset ?? 0) + pick([-4, -2, 2, 4])));
    }
    out.push(m);
  }
  return out;
}
