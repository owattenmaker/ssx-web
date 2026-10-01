// pv eventReturnInWorld (docs/ctm-events-in-world.md stage 5 "WS13"): Next heat in the world, one code path for the page (main.js) and
// compare-ai-capture.mjs --ws13. Everything here follows the PS2 order; the addresses are in the doc.
import { assembleLineup, buildRoster, lineupFor, peakRival, roundEntries } from './lineup.js';
import { syncWorldNodes } from './ai-racers.js';

// The core exports WS13's path runs (pv eventReturnInWorld): a core without one throws here rather than riding on with the wrong state.
export const HEAT_CORE_EXPORTS = [
  '_world_node_states',
  '_shared_world_nodes',
  '_ctm_world_reset',
  '_game_tick_restart',
  '_event_row_enter',
  '_free',
  '_nis_hold',
  '_event_route_seed',
  '_event_grid_start'
];
export const HEAT_RIDER_EXPORTS = ['_world_instance_states', '_world_node_states_apply', '_ctm_world_reset', '_game_tick_restart', '_npc_fresh_rider', '_npc_grid_start', '_event_clock_hold'];
export function requireHeatCore(human, racers) {
  const missing = HEAT_CORE_EXPORTS.filter((f) => typeof human?.[f] !== 'function');
  for (const n of racers?.npcs ?? []) for (const f of HEAT_RIDER_EXPORTS) if (typeof n.core?.[f] !== 'function' && !missing.includes(f)) missing.push(f);
  if (missing.length) throw new Error(`This core has no ${missing.map((f) => f.slice(1)).join(', ')} (WS13, pv eventReturnInWorld)`);
}

// WS13's enter (0x235AA0): cBE_setState(0) (14DE68), the Big Challenge end (30B7F8: none runs in an event), restartHeat (2382D8), the
// world reset 230180 (core ctm_world_reset: the port's reset_race world part), then the round's roster in the same rider contexts
// (144D98 setNumberAI, 147338 / 1473D0 the characters from GMM+0x18.. / +0x40..; web/ai-race.js prepare for round 2: the qualifier's
// top three, the human skipped, then entries 5..7 of the career's roster, web/lineup.js roundEntries). finishOrder: race slots by place.
// bank: the event location's paths.json variant (its start rows), cstr(text) -> a core string. 12AB20 -> 129768 -> 1297C8(C, 1) ends
// the enter: the rider manager's tick C+8 = 0 and 11D390's event branch on the human (core event_row_enter: 112180 on its start row,
// 11D660 there, the grid hold) before the gondola's NIS takes it (PS2 c0a-ws13p460 entry probe; c0a-ws13 record 14017's +0x460).
// 230180, the world reset part of WS13's enter (also the page's, main.js heatInWorld): every context's reset, then the human context's
// node states into the riders' contexts.
export function heatResetWorld({ human, racers }) {
  requireHeatCore(human, racers);
  for (const c of [human, ...racers.npcs.map((n) => n.core)]) c._ctm_world_reset(); // 230180 resets the one world every rider context mirrors (web/shared_world.inc)
  // ... and every context then holds the human context's node states (web/ai-racers.js syncWorldNodes): a rider context's copy of an
  // instance the qualifier changed by a shared-world replay (no stage script of its own to reset) is the load state again (PS2 c0a-ws13:
  // the semi's rider 1 re-fires trigger 266760's Spline pieces at 868; without it the riders' copy kept the fired 0x200004). The human's
  // reset includes 308C60's 308DB8 (the Big Challenge markers hidden again, core ctm_world_reset), so the riders take those Hides too.
  syncWorldNodes(human, racers.npcs.map((n) => n.core));
}
// 12AB20 -> 129768 -> 1297C8(C, 1), the end of WS13's enter after the round's roster (also the page's): the game RNG runs on, every
// context's tick C+8 = 0, and 11D390's event branch on the human (core event_row_enter: the start row, 11D660, the grid hold).
export function heatRows({ human, racers, bank, cstr }) {
  if (!bank || !cstr) throw new Error('heatRows: the location\'s start rows (bank) and cstr are required');
  racers.setAnchorRng(null); // the game RNG runs on (no event load: an anchor's words are a load's)
  for (const c of [human, ...racers.npcs.map((n) => n.core)]) c._game_tick_restart(0); // 1297C8: C+8 = 0
  const p = cstr(JSON.stringify(bank));
  try { if (!human._event_row_enter(p)) throw new Error('event_row_enter: no start row'); } finally { human._free(p); }
}
export function heatEnter({ human, racers, doc, finishOrder, lineupData, riderText, bank, cstr, round = 2 }) {
  if (!bank || !cstr) throw new Error('heatEnter: the location\'s start rows (bank) and cstr are required');
  heatResetWorld({ human, racers });
  const base = doc.relationships.persistent_human_character, rival = peakRival(lineupData.peak, base);
  const entries = buildRoster(doc.lineup_state.roster_seed, base, lineupData.peak);
  const current = roundEntries(entries, { round: round - 1, career: true, rival, previous: [] });
  const previous = finishOrder.map((slot) => (slot === 0 ? null : current[slot - 1] ?? null)); // web/ai-race.js results(): 0x536708
  const lineup = lineupFor({ seed: null, entries, human: { base, cheat: 0 }, peak: lineupData.peak, round, career: true, previous });
  const next = assembleLineup(lineupData, base, lineup.values, { round }); // the round's route roles (lineup.js npcRoundRole)
  for (const r of next.riders) riderText(r.package);
  next.world.pair_inputs[0] = { ...racers.document.world.pair_inputs[0] }; // the human's own (web/ai-race.js install)
  const changed = racers.setDocument(next);
  heatRows({ human, racers, bank, cstr });
  return { values: lineup.values, changed, doc: next };
}
