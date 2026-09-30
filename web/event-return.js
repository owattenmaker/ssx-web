// pv eventReturnInWorld (docs/ctm-events-in-world.md stage 5): an in-world CTM event's return to its own location after the results'
// Transport, in the PS2's order. main.js (the page) and compare-ai-capture.mjs (the c0a-ret3 gate) both run these, so the gate scores the
// players' sequence. Every core call is one of the rider contexts of the one module (web/ai-racers.js); cstr(text) returns a C string in
// its heap, freed here.
//
// PS2 (SLUS_207.72, gp 0x4A30F0), after the live ticks end at the auto replay's start (web/game-tick.js liveStopAt):
// 1. the results' Transport: overlay command 1 with item 2 -> 0x20CF88 stopAutoReplay 0x2706F0 (the results-time state back), the stop
//    frame's tick;
// 2. world state 14's enter 0x236250 (arg 2) -> transportMapEnter, then the WS14 frame's tick; the map holds the world;
// 3. the map's same-location confirm (0x236604: 231250(S, 15)) -> world state 15's enter 0x236058 -> sessionReturn.

import { applyFreestyleEvent } from './freestyle-event.js';

// The core steps the return needs (a core without one throws here rather than returning different riders). web/ai-racers.js resume
// needs race_world_pair_restart / rider_peers_restart / game_tick_restart / race_world_rank_mode on the cores it holds.
export const RETURN_CORE_EXPORTS = ['_transport_map_enter', '_event_course_seed', '_peak_world_event_kind', '_peak_world_game_mode', '_race_time_limit',
  '_set_race_bonus', '_rider_setup_player_reset', '_race_world_player_setup', '_init_race', '_reset_race', '_reset_pad_history', '_mission_world_session',
  '_location_entry_place', '_place_rider_region', '_game_tick_restart', '_race_world_pair_restart', '_rider_peers_restart', '_race_world_rank_mode',
  '_rider_pose_step'];
export function requireReturnCore(core) { const missing = RETURN_CORE_EXPORTS.filter((f) => typeof core?.[f] !== 'function'); if (missing.length) throw new Error(`This core has no ${missing.map((f) => f.slice(1)).join(', ')}`); }

function withString(human, cstr, text, f) { const p = cstr(text); try { return f(p); } finally { human._free(p); } }

// World state 14's enter 0x236250 with arg 2, the human's part (core transport_map_enter: the boost meter, then 11D390's event branch:
// 112180 on the bank's start row and the grid hold). bank: the location's paths.json variant (the object).
export function transportMapEnter({ human, cstr, bank }) {
  requireReturnCore(human);
  withString(human, cstr, JSON.stringify(bank), (p) => { if (!human._transport_map_enter(p)) throw new Error('transport_map_enter: no start row'); });
}

// World state 15's enter 0x236058 (the return to the event's location, Session point 1): 30B7F8 (the host ends a Big Challenge), then
// - 238160(GMM, 12): free ride's kind and mode, no event course, no freestyle bonus list and no time limit (GMM+0x78; the event's
//   own: web/freestyle-event.js);
// - the rider in setup slot 1 turns to a player setup (149A88(., 1): Zoe, stats 0.5; c0a-ret3's watch; the caller is unconfirmed);
// - 230180: the race and world reset (the port's init_race of free ride's document + reset_race), whose 129768 -> 1297C8(C, 1) runs
//   11D390's free-ride branch for every rider in the list's order (the human first; core location_entry_place: 112180 + 11DE60 /
//   11DF18) with the pair records made again and the rider manager's tick at 0 (web/ai-racers.js resume);
// - 11DE60(human, Session point 1, 2) + 11DF18(human, 1) (0x2360CC / 0x2360E8): the human placed again.
// entry: the location's Session point 1 row {position, direction}; freeRideDoc: free ride's race document text (PEAK1/initial.json).
export function sessionReturn({ human, racers, cstr, bank, entry, freeRideDoc }) {
  requireReturnCore(human); for (const n of racers?.npcs ?? []) requireReturnCore(n.core);
  withString(human, cstr, '', (p) => human._event_course_seed(p)); human._peak_world_event_kind(4); human._peak_world_game_mode(12);
  applyFreestyleEvent(human, null); human._race_time_limit(0);
  if (racers?.npcs?.[0]) { racers.npcs[0].core._rider_setup_player_reset(); human._race_world_player_setup(1); }
  withString(human, cstr, freeRideDoc, (p) => human._init_race(p)); human._reset_race(); human._reset_pad_history();
  const [x, y, z] = entry.position, [dx, dy, dz] = entry.direction;
  withString(human, cstr, JSON.stringify(bank), (b) => {
    human._mission_world_session(0);
    if (!human._location_entry_place(b, 0, x, y, z, dx, dy, dz)) throw new Error('location_entry_place: no start row');
    if (racers) racers.resume({ place: (c) => { const slot = racers.npcs.findIndex((n) => n.core === c) + 1; if (!c._location_entry_place(b, slot, x, y, z, dx, dy, dz)) throw new Error('location_entry_place: no start row'); } });
    else human._game_tick_restart(0); // (no riders kept: 129768's C+8 = 0 for the human alone; web/ai-racers.js resume restarts every core's)
    human._place_rider_region(x, y, z, dx, dy, dz, 0);
    human._mission_world_session(1);
  });
}

// World state 1's phase 3 completion after the return's tick 7: 12B030 (0x234714) -> 12AE38 (the computer riders destroyed, C+0x78
// 6 -> 1) + 10F3B8 (the pair records made again: the human's peers off), then its exit 0x234750 in the same frame: 128A10 -> 1297C8(C, 0)
// (C+8 = 0 at 0x1297F0) and 129160 (0x23488C: the listed riders' pose pass, core rider_pose_step: one full-rate animation step, no
// physics). The riders' contexts go back to the host (detach). The ticks before are counted by web/ai-racers.js (its riders sit out
// ticks 3 and 4).
export function sessionRidersLeave({ human, racers }) {
  const blocks = racers ? racers.detach() : [];
  requireReturnCore(human); human._rider_peers_restart(1); human._game_tick_restart(0); // 12B030's 10F3B8 with the human alone; 1297C8(C, 0)'s C+8 = 0
  human._rider_pose_step(); // 129160
  return blocks;
}

// The location's Session point 1 row of its paths.json variant 0 (the exported region kind 1 = runtime kind 2, index 1).
export function sessionPointRow(bank) { return bank.regions.find((g) => g.kind + 1 === 2 && g.index === 1) ?? bank.regions[0]; }
