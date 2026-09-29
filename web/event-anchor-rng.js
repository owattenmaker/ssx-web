// The shared game RNG 0x4FF030 at an event's countdown anchor for the courses without lineups.json / rivals.json (pv
// eventAnchorRng; docs/HANDOFF.md "Event anchor RNG"). The event load seeds the generator with 0, then draws a count that
// depends on the course and the human (web/lineup.js load_draws: base, Zoe -1, Moby +1, a cheat skin +4), then each computer
// rider's first countdown start command draws two (start). Without this the core kept its seed state's generator (a glide
// state's: Crow's Nest 109 draws where the PS2 has 9, Gravitude 1381 for 26), so every random pick (landing and upper-body
// variants, soft-collision reactions) differed from the PS2 from the first one: the Crow's Nest release run left the PS2 at
// its 1776 soft collision; seeded, it stays exact to the end.
// Measured: record 0 of each course's countdown-anchor capture (Zoe), draws from seed 0 = base - 1 + start.
import { seededWords, nextWord, loadDraws } from './lineup.js';
export const EVENT_ANCHOR_DRAWS = Object.freeze({
  ABA1: { tick: 18, base: 10, start: 0 },   // Crow's Nest big air: 9 (air-release/crows-invert, monster-xexec)
  BHP1: { tick: 18, base: 17, start: 0 },   // The Junction super pipe: 16 (pipe-brake)
  CBA2: { tick: 18, base: 12, start: 0 },   // Launch Time big air: 11 (peak2/cba2-full)
  CHP2: { tick: 18, base: 15, start: 0 },   // Schizophrenia super pipe: 14 (peak2/chp2-full)
  EBA3: { tick: 19, base: 12, start: 0 },   // Much-2-Much big air: 11 at 19 (peak3/much-2-much-event-tuck)
  EHP3: { tick: 18, base: 13, start: 0 },   // Perpendiculous super pipe: 12 (peak3/perpendiculous-full)
  ERA5: { tick: 18, base: 17, start: 10 },  // Gravitude race, five computer riders: 26 (peak3/gravitude-event-tuck)
  ESS3: { tick: 18, base: 21, start: 2 },   // Kick Doubt slope style, one computer rider: 22 (peak3/kick-doubt-event-tuck)
});
const RULE = { character: { 4: -1, 0: 1 }, cheat: 4 };   // lineups.json load_draws (every course with one has this rule)
// { tick, words } for this course and human ({ base, cheat }: web/lineup.js humanCharacter), or null.
export function eventAnchorWords(code, human = { base: 4, cheat: 0 }) {
  const e = EVENT_ANCHOR_DRAWS[code]; if (!e) return null;
  const n = loadDraws({ load_draws: { base: e.base, ...RULE } }, human) + e.start;
  const w = seededWords(0); for (let k = 0; k < n; k++) nextWord(w);
  return { tick: e.tick, draws: n, words: w };
}
