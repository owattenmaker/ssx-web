// pv eventReturnInWorld (docs/ctm-events-in-world.md stage 5 "WS13"): Next heat in the world, one code path for the page (main.js) and
// compare-ai-capture.mjs --ws13. Everything here follows the PS2 order; the addresses are in the doc.
import { assembleLineup, buildRoster, lineupFor, peakRival, roundEntries } from './lineup.js';

// WS13's enter (0x235AA0): cBE_setState(0) (14DE68), the Big Challenge end (30B7F8: none runs in an event), restartHeat (2382D8), the
// world reset 230180 (core ctm_world_reset: the port's reset_race world part), then the round's roster in the same rider contexts
// (144D98 setNumberAI, 147338 / 1473D0 the characters from GMM+0x18.. / +0x40..; web/ai-race.js prepare for round 2: the qualifier's
// top three, the human skipped, then entries 5..7 of the career's roster, web/lineup.js roundEntries). finishOrder: race slots by place.
export function heatEnter({ human, racers, doc, finishOrder, lineupData, riderText, round = 2 }) {
  human._ctm_world_reset();
  const base = doc.relationships.persistent_human_character, rival = peakRival(lineupData.peak, base);
  const entries = buildRoster(doc.lineup_state.roster_seed, base, lineupData.peak);
  const current = roundEntries(entries, { round: round - 1, career: true, rival, previous: [] });
  const previous = finishOrder.map((slot) => (slot === 0 ? null : current[slot - 1] ?? null)); // web/ai-race.js results(): 0x536708
  const lineup = lineupFor({ seed: null, entries, human: { base, cheat: 0 }, peak: lineupData.peak, round, career: true, previous });
  const next = assembleLineup(lineupData, base, lineup.values);
  for (const r of next.riders) riderText(r.package);
  next.world.pair_inputs[0] = { ...racers.document.world.pair_inputs[0] }; // the human's own (web/ai-race.js install)
  const changed = racers.setDocument(next);
  racers.setAnchorRng(null); // the game RNG runs on (no event load: an anchor's words are a load's)
  return { values: lineup.values, changed, doc: next };
}
