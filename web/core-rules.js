// The event and career rules in the core (pv coreRules, docs/core-migration.md slice M1): engine/game_rules.hpp through
// web/rules_bridge.cpp rules_eval(fn, io, n). Each function here returns the core's answer, or undefined when the core is
// not there, the switch is off, or the inputs are ones the core version does not take (non-integers, NaN, missing rows):
// the JS caller then runs its own version. Both versions give the same results (web/test-core-rules.mjs).
// main.js hands the page's core in (setRulesCore) after the course's core is made and takes it out at the course unload.
import { pv } from './pv-flags.js';
import { rulesModule } from './rules-module.js';

// Function ids of web/rules_bridge.cpp.
export const RULE = Object.freeze({
  seededWords: 1,
  nextWord: 2,
  wordsAfterDraws: 3,
  clockSeed: 4,
  peakRival: 5,
  buildRoster: 6,
  roundEntries: 7,
  freestyleRoster: 8,
  relationshipAge: 9,
  relationshipEvent: 10,
  npcRoundRole: 11,
  npcDifficulty: 12,
  eventAnchor: 13,
  estimateRace: 14,
  estimateCareer: 15,
  estimatePoints: 16,
  rankEntries: 17,
  rankBy: 18,
  orderByTime: 19,
  placementMedal: 20,
  explorePlace: 21,
  adaptLevel: 22,
  aiFreestyleScore: 23,
  mountainPercent: 24,
  platinumThreshold: 25,
  recordSlot: 26,
  rivalCharacter: 27,
  raceRound: 28,
  freestyleRound: 29,
  eventMedal: 30,
  recordInsert: 31,
  peakResult: 32
});
const IO_DOUBLES = 64;
// The course core (main.js setRulesCore) and its io buffer.
let courseCore = null;
let courseIo = 0;
// The rules module (pv rulesModule, web/rules-module.js) and its io buffer: the rules run there while no course core is there.
let moduleCore = null;
let moduleIo = 0;
// The core the rules run in (null: none, the JS versions run).
export function setRulesCore(core) {
  if (core === courseCore) return;
  courseCore = core && typeof core._rules_eval === 'function' ? core : null;
  courseIo = courseCore ? courseCore._malloc(IO_DOUBLES * 8) : 0;
}
// The rules module when it has loaded and the switch is on; its io buffer is made at its first use.
function moduleTarget() {
  if (!pv('rulesModule')) return null;
  const m = rulesModule();
  if (!m || typeof m._rules_eval !== 'function') return null;
  if (m !== moduleCore) {
    moduleCore = m;
    moduleIo = m._malloc(IO_DOUBLES * 8);
  }
  return moduleCore;
}
// The core for this call: the course core, else the rules module, or null (the switch off, neither there).
function active() {
  if (!pv('coreRules')) return null;
  if (courseCore) return courseCore;
  return moduleTarget();
}
// One call: inputs in, outputs out (an array of numbers), or undefined.
function call(fn, inputs) {
  const core = active();
  if (!core || inputs.length > IO_DOUBLES) return undefined;
  const io = core === courseCore ? courseIo : moduleIo;
  const view = new Float64Array(core.HEAPU8.buffer, io, IO_DOUBLES);
  for (let k = 0; k < inputs.length; k++) view[k] = inputs[k];
  const n = core._rules_eval(fn, io, inputs.length);
  if (n < 0) return undefined;
  const out = new Float64Array(core.HEAPU8.buffer, io, IO_DOUBLES);
  return Array.from(out.subarray(0, n));
}
const isInt = (v) => Number.isInteger(v);
const isWord = (v) => Number.isInteger(v) && v >= 0 && v <= 0xffffffff;
const isNumber = (v) => typeof v === 'number' && !Number.isNaN(v);
const words6 = (w) => w && w.length === 6 && Array.prototype.every.call(w, isWord);
// Writes six words back into the caller's array (an Array or a Uint32Array), as the JS versions advance them in place.
function putWords(w, out, at) {
  for (let k = 0; k < 6; k++) w[k] = out[at + k];
}

export function coreSeededWords(seed) {
  const s = seed >>> 0;
  const out = call(RULE.seededWords, [s]);
  return out;
}
export function coreNextWord(w) {
  if (!active() || !words6(w)) return undefined;
  const out = call(RULE.nextWord, Array.from(w));
  if (!out) return undefined;
  putWords(w, out, 1);
  return out[0];
}
// The words after `draws` draws from seed `seed` (an Array of six).
export function coreWordsAfterDraws(seed, draws) {
  if (!isWord(seed) || !isWord(draws)) return undefined;
  return call(RULE.wordsAfterDraws, [seed, draws]);
}
export function coreClockSeed(date) {
  if (!active()) return undefined;
  const out = call(RULE.clockSeed, [date.getSeconds(), date.getMinutes(), date.getHours(), date.getDate(), date.getMonth() + 1, date.getFullYear()]);
  return out?.[0];
}
export function corePeakRival(peak, human) {
  if (!isInt(peak) || !isInt(human)) return undefined;
  return call(RULE.peakRival, [peak, human])?.[0];
}
export function coreBuildRosterFrom(w, human, peak) {
  if (!active() || !words6(w) || !isInt(human) || !isInt(peak)) return undefined;
  const out = call(RULE.buildRoster, [...Array.from(w), human, peak]);
  if (!out) return undefined;
  putWords(w, out, 10);
  return out.slice(0, 10);
}
export function coreRoundEntries(entries, round, career, rival, previous) {
  if (!active() || !Array.isArray(entries) || entries.length !== 10 || !entries.every(isInt)) return undefined;
  if (!isInt(round) || !isInt(rival) || !Array.isArray(previous)) return undefined;
  const first = previous.slice(0, 3);
  if (!first.every((v) => v == null || (isInt(v) && v >= 0))) return undefined;
  const coded = first.map((v) => (v == null ? -1 : v));
  return call(RULE.roundEntries, [...entries, round, career ? 1 : 0, rival, coded.length, ...coded]);
}
export function coreFreestyleRoster(w, human, rival) {
  if (!active() || !words6(w) || !isInt(human) || !isInt(rival)) return undefined;
  const out = call(RULE.freestyleRoster, [...Array.from(w), human, rival]);
  if (!out) return undefined;
  const n = out[0];
  putWords(w, out, 1 + n);
  return out.slice(1, 1 + n);
}
// [kind, level, score] of an aged record (0x155E58) / after a pair event (0x155BF0).
export function coreRelationshipAge(kind, score) {
  if (!isInt(kind) || !isInt(score)) return undefined;
  return call(RULE.relationshipAge, [kind, score]);
}
export function coreRelationshipEvent(kind, score, event) {
  if (!isInt(kind) || !isInt(score) || !isNumber(event)) return undefined;
  if (!isInt(event)) return undefined;
  return call(RULE.relationshipEvent, [kind, score, event]);
}
export function coreNpcRoundRole(slot, round) {
  if (!isInt(slot) || !isInt(round)) return undefined;
  return call(RULE.npcRoundRole, [slot, round])?.[0];
}
export function coreNpcDifficulty(slot, level, course) {
  if (!isInt(slot) || !isInt(level) || !isInt(course)) return undefined;
  const out = call(RULE.npcDifficulty, [slot, level, course]);
  return out ? { df8: out[0], dfc: out[1] } : undefined;
}
// { tick, draws, words } for a course and human, null when the course has no anchor entry.
export function coreEventAnchor(code, human) {
  if (!active() || typeof code !== 'string' || !isInt(human?.base)) return undefined;
  if (code.length !== 4) return code.length ? null : undefined;
  const chars = Array.from(code, (c) => c.charCodeAt(0));
  if (!chars.every((c) => c > 0 && c < 128)) return undefined;
  const out = call(RULE.eventAnchor, [...chars, human.base, human.cheat ? 1 : 0]);
  if (!out) return undefined;
  if (!out.length) return null;
  return { tick: out[0], draws: out[1], words: out.slice(2, 8) };
}
export function coreEstimateRace(raceTicks, origin, remaining, slot) {
  if (![raceTicks, origin, remaining, slot].every((v) => typeof v === 'number')) return undefined;
  return call(RULE.estimateRace, [raceTicks, origin, remaining, slot])?.[0];
}
export function coreEstimateCareer(raceTicks, origin, remaining, place, minimumSpeed) {
  if (![raceTicks, origin, remaining, place, minimumSpeed].every((v) => typeof v === 'number')) return undefined;
  return call(RULE.estimateCareer, [raceTicks, origin, remaining, place, minimumSpeed])?.[0];
}
export function coreEstimatePoints(score, remaining, origin) {
  if (![score, remaining, origin].every((v) => typeof v === 'number')) return undefined;
  return call(RULE.estimatePoints, [score, remaining, origin])?.[0];
}
function rankWith(fn, values, ascending) {
  if (!Array.isArray(values) || values.length > IO_DOUBLES - 2 || !values.every(isNumber)) return undefined;
  return call(fn, [ascending ? 1 : 0, values.length, ...values]);
}
export const coreRankEntries = (values, ascending) => rankWith(RULE.rankEntries, values, ascending);
export const coreRankBy = (values, ascending) => rankWith(RULE.rankBy, values, ascending);
export function coreOrderByTime(times) {
  if (!Array.isArray(times) || times.length > IO_DOUBLES - 1 || !times.every((t) => typeof t === 'number')) return undefined;
  return call(RULE.orderByTime, [times.length, ...times]);
}
export function corePlacementMedal(mode, rank) {
  if (!isInt(mode) || !isInt(rank)) return undefined;
  return call(RULE.placementMedal, [mode, rank])?.[0];
}
export function coreExploreMedal(count, row) {
  if (typeof count !== 'number' || !Array.isArray(row) || row.length > IO_DOUBLES - 2 || !row.every((x) => typeof x === 'number')) return undefined;
  return call(RULE.explorePlace, [count, row.length, ...row])?.[0];
}
export function coreAdaptLevel(level, counter, up) {
  if (!isInt(level) || !isInt(counter)) return undefined;
  const out = call(RULE.adaptLevel, [level, counter, up ? 1 : 0]);
  return out ? { level: out[0], counter: out[1] } : undefined;
}
// The posted score from the table score and this rider's draw (web/career.js aiFreestyleScore draws first, then calls this).
export function coreAiFreestyleScore(tableScore, aiIndex, level, level0Scale, level2Scale, jitter, draw) {
  if (![tableScore, level0Scale, level2Scale, jitter].every((v) => typeof v === 'number')) return undefined;
  if (!isInt(aiIndex) || !isInt(level) || !isWord(draw)) return undefined;
  return call(RULE.aiFreestyleScore, [tableScore, aiIndex, level, level0Scale, level2Scale, jitter, draw])?.[0];
}
export function coreMountainPercent({ golds, rivals, challenges, challengeTotal, collected, collectTotal, highlights }) {
  const v = [golds, rivals, challenges, challengeTotal, collected, collectTotal, highlights];
  if (!v.every((x) => typeof x === 'number')) return undefined;
  return call(RULE.mountainPercent, v)?.[0];
}
// {ticks} / {score} / null, or undefined.
export function corePlatinumThreshold(platinum, mode, course) {
  if (!isInt(mode) || !isInt(course) || !Array.isArray(platinum) || platinum.length > IO_DOUBLES - 3) return undefined;
  const column = platinum.map((row) => row?.[1]);
  if (!column.every((x) => typeof x === 'number')) return undefined;
  const out = call(RULE.platinumThreshold, [mode, course, column.length, ...column]);
  if (!out) return undefined;
  if (out[0] === 0) return null;
  return out[0] === 1 ? { ticks: out[1] } : { score: out[1] };
}
export function coreRecordSlot(recordSlots, mode, course) {
  if (!isInt(mode)) return undefined;
  if (mode >= 6 && mode <= 11) return call(RULE.recordSlot, [mode, 0, 0, 0, 0, 0, 0])?.[0];
  if (course == null) return call(RULE.recordSlot, [mode, 17, 0, 0, 0, 0, 0])?.[0];
  if (!isInt(course) || course < 0) return undefined;
  if (course >= 17) return call(RULE.recordSlot, [mode, course, 0, 0, 0, 0, 0])?.[0];
  const row = recordSlots?.[course];
  if (!Array.isArray(row) || row.length < 4 || !row.slice(0, 4).every(isInt)) return undefined;
  return call(RULE.recordSlot, [mode, course, 1, row[0], row[1], row[2], row[3]])?.[0];
}
export function coreRivalCharacter(peak, human) {
  if (!isInt(peak) || !isInt(human)) return undefined;
  return call(RULE.rivalCharacter, [peak, human])?.[0];
}

// ---- M5c: the career's result kernel (pv coreCareerResults; engine/career_rules.hpp) ------------------------------------------------
// Each returns the kernel's outputs, or undefined (the switch off, no core, or inputs the kernel does not take): the caller keeps its
// JS version. The inputs are the JS values as the JS versions read them (null / absent fields as their ?? defaults).
const careerResults = () => pv('coreCareerResults');
// (finite numbers only: the JS sorts compare by subtraction, and infinities or NaN there are left to the JS)
const isNum = (v) => Number.isFinite(v);
// web/career.js raceResult's times / ranks / round / level / message for standings [{dnf, finishTicks, remaining, place, origin}].
export function coreRaceRound(round, career, level, standings, human, rules, raceTicks, origin) {
  if (!careerResults() || !active()) return undefined;
  if (!isInt(round) || round < 1 || round > 3 || !level || !isInt(level.level) || !isInt(level.counter)) return undefined;
  if (!Array.isArray(standings) || standings.length > 8 || !isInt(human) || human < 0 || human >= standings.length) return undefined;
  const margin = rules?.level_up_margin;
  if (!isNum(rules?.dnf_ticks) || !isNum(rules?.estimate_min_speed) || !Array.isArray(margin) || !isNum(raceTicks)) return undefined;
  const inputs = [round, career ? 1 : 0, level.level, level.counter, rules.dnf_ticks, rules.estimate_min_speed];
  for (let k = 0; k < 3; k++) inputs.push(isNum(margin[k]) ? margin[k] : NaN);
  inputs.push(raceTicks, human, standings.length);
  for (const s of standings) {
    const finished = s.finishTicks != null;
    const place = s.place ?? 0;
    const from = s.origin ?? origin;
    if (finished && !isNum(s.finishTicks)) return undefined;
    if (!s.dnf && !finished && (!isNum(s.remaining) || !isNum(place) || !isNum(from))) return undefined;
    inputs.push(s.dnf ? 1 : 0, finished ? 1 : 0, finished ? s.finishTicks : 0, isNum(s.remaining) ? s.remaining : 0, isNum(place) ? place : 0, isNum(from) ? from : 0);
  }
  const out = call(RULE.raceRound, inputs);
  if (!out) return undefined;
  const n = out[7];
  return {
    place: out[0],
    advance: out[1] === 1,
    round: out[2],
    level: { level: out[3], counter: out[4] },
    message: [null, 'kT_OVRCongratGoToSemi', 'kT_OVRRaceObjectives7', 'kT_OVRCMNSorryGetThirdSemi', 'kT_OVRCMNSorryGetThird'][out[5]],
    complete: out[6] === 1,
    times: out.slice(8, 8 + n),
    rank: out.slice(8 + n, 8 + 2 * n)
  };
}
const score3 = (s) => Array.isArray(s) && s.length === 3 && s.every(isNum);
// web/career.js freestyleResult's totals / shown / ranks / round / level / message; ev = {round, career, player, opponent, ai}.
export function coreFreestyleRound(ev, level, score, opponent) {
  if (!careerResults() || !active()) return undefined;
  const round = ev?.round;
  if (!isInt(round) || round < 1 || round > 3 || !level || !isInt(level.level) || !isInt(level.counter) || !isNum(score)) return undefined;
  if (!score3(ev.player) || !Array.isArray(ev.ai) || ev.ai.length > 14 || !ev.ai.every((a) => score3(a?.scores))) return undefined;
  const has = !!ev.opponent;
  if (has && !score3(ev.opponent.scores)) return undefined;
  const update = has && !!opponent;
  if (update && opponent.finished && !isNum(opponent.score)) return undefined;
  if (update && !opponent.finished && round === 3 && !isNum(opponent.estimate)) return undefined;
  const inputs = [round, ev.career ? 1 : 0, level.level, level.counter, score, ...ev.player, has ? 1 : 0, ...(has ? ev.opponent.scores : [0, 0, 0])];
  inputs.push(update ? 1 : 0, update && opponent.finished ? 1 : 0, update && isNum(opponent.score) ? opponent.score : 0);
  inputs.push(update && isNum(opponent.estimate) ? opponent.estimate : 0, ev.ai.length);
  for (const a of ev.ai) inputs.push(...a.scores);
  const out = call(RULE.freestyleRound, inputs);
  if (!out) return undefined;
  const n = out[13];
  return {
    place: out[0],
    advance: out[1] === 1,
    round: out[2],
    level: { level: out[3], counter: out[4] },
    message: [null, 0x0812f644, 'kT_OVRCMNSorryGetThird'][out[5]],
    complete: out[6] === 1,
    player: out.slice(7, 10),
    opponent: out.slice(10, 13),
    totals: out.slice(14, 14 + n),
    shown: out.slice(14 + n, 14 + 2 * n),
    rank: out.slice(14 + 2 * n, 14 + 3 * n)
  };
}
// web/career.js completeEvent's medal; threshold: platinumThreshold's {ticks} / {score} / null.
export function coreEventMedal(mode, place, career, threshold, ticks, score) {
  if (!careerResults() || !active() || !isInt(mode) || !isInt(place)) return undefined;
  let kind = 0;
  let amount = 0;
  if (threshold) {
    kind = threshold.ticks != null ? 1 : 2;
    amount = kind === 1 ? threshold.ticks : threshold.score;
    if (!isNum(amount)) return undefined;
  }
  // (the comparison reads the run's value: an absent one is NaN, null 0)
  const value = (v) => (v === undefined ? NaN : v === null ? 0 : v);
  if (!isNum(value(ticks)) || !isNum(value(score))) return undefined;
  const out = call(RULE.eventMedal, [mode, place, career ? 1 : 0, kind, amount, value(ticks), value(score)]);
  return out ? out[0] : undefined;
}
// web/career.js addRecord's entry: {at, secs} for a run against the slot's five values.
export function coreRecordInsert(timed, value, values) {
  if (!careerResults() || !active() || !isNum(value) || !Array.isArray(values) || values.length > 16 || !values.every(isNum)) return undefined;
  const out = call(RULE.recordInsert, [timed ? 1 : 0, value, values.length, ...values]);
  return out ? { at: out[0], secs: out[1] } : undefined;
}
// web/peak-run.js peakResult with the setup's target and the tier-3 / tier-2 rows' limits.
export function corePeakResult(timeChallenge, target, tier3, tier2, ticks, score, dnf) {
  if (!careerResults() || !active() || ![target, tier3, tier2, ticks, score].every(isNum)) return undefined;
  const out = call(RULE.peakResult, [timeChallenge ? 1 : 0, target, tier3, tier2, ticks, score, dnf ? 1 : 0]);
  return out ? { place: out[0], fail: out[1] === 1, value: out[2] } : undefined;
}
