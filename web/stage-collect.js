// Conquer the Mountain collectibles between the core's stage setup and the browser career save (web/career.js).
// 0x535C11 (0 career, 1 single event, 2 multiplayer) and the course's collect mask go to the core before every race
// start (set_stage_collect_state: 30C4A8 makes single-event and already-collected ones DeadNodes); each collect of this
// race (stage_collect_events: list index + award) marks the career bit and adds the cash (30C3E0 / 119EF8).
import { pv } from './pv-flags.js';
import {COURSE_INDEX} from './game-audio.js';

// Streamed Peak 1 world (docs/peak-mountain.md "Collectibles"): each location's stage opens the ONE collectible slot (ctx+0x2C0,
// 30B928: only when free) when its read completes and frees it at its unload; the collect record is that location's course
// (profile record (setKey + *0x5305F0) * 12). SDB track -> course index of the Peak 1 locations with a collectible set.
export const PEAK1_COLLECT_TRACKS = Object.freeze({1: 17, 12: 18, 8: 0, 16: 1, 11: 5, 5: 8, 15: 11, 6: 14});
// Peak 3 (docs/peak3.md): E 36 -> 21, EBA3 41 -> 10, EBC3 42 -> 16, EHP3 44 -> 13, ERA5 45 -> 4, ESS3 48 -> 7 (the connectors have none).
export const PEAK3_COLLECT_TRACKS = Object.freeze({36: 21, 41: 10, 42: 16, 44: 13, 45: 4, 48: 7});
// Peak 2 (docs/peak2.md): C 18 -> 19, D 27 -> 20, CRA3 24 -> 2, DRA4 32 -> 3, DSS2 35 -> 6, CBA2 22 -> 9, CHP2 23 -> 12, DBC2 30 -> 15.
export const PEAK2_COLLECT_TRACKS = Object.freeze({18: 19, 27: 20, 24: 2, 32: 3, 35: 6, 22: 9, 23: 12, 30: 15});
// The whole mountain (docs/peak3.md section 6): every peak's sets (SDB tracks are unique across the mountain).
export const MOUNTAIN_COLLECT_TRACKS = Object.freeze({ ...PEAK1_COLLECT_TRACKS, ...PEAK2_COLLECT_TRACKS, ...PEAK3_COLLECT_TRACKS });
export const PEAK_COLLECT_TRACKS = Object.freeze({ PEAK1: PEAK1_COLLECT_TRACKS, PEAK2: PEAK2_COLLECT_TRACKS, PEAK3: PEAK3_COLLECT_TRACKS, MOUNTAIN: MOUNTAIN_COLLECT_TRACKS });
// Returns what it gave the core ({cash, state, rows}), which collectApply() gives it again: a replay (web/replay.js)
// re-runs the event from the career state of its start, not from the one the run left.
export function collectStart(core, {careerMode, career, riderId, courseCode}) {
  if (!core?._set_stage_collect_state) return null;
  const applied = { cash: (career?.save?.riders?.[riderId]?.cash ?? 0) | 0, state: null, rows: [] };
  if (PEAK_COLLECT_TRACKS[courseCode]) {
    applied.state = [0, 0, 0]; // free ride and the peak runs only exist in Conquer the Mountain (0x535C11 = 0)
    if (core._set_stage_collect_row) for (const [track, course] of Object.entries(PEAK_COLLECT_TRACKS[courseCode])) {
      const [lo, hi] = career && riderId ? career.collectMask(riderId, course) : [0, 0];
      applied.rows.push([Number(track), lo >>> 0, hi >>> 0]);
    }
  } else {
    const course = COURSE_INDEX[courseCode];
    const [lo, hi] = careerMode && career && course !== undefined ? career.collectMask(riderId, course) : [0, 0];
    applied.state = [careerMode ? 0 : 1, lo >>> 0, hi >>> 0];
  }
  collectApply(core, applied);
  return applied;
}
export function collectApply(core, applied) {
  if (!core?._set_stage_collect_state || !applied) return;
  // 150960: HUD slot 0x19 is the character block's cash C+0xAC4, which the core's 119EF8 awards add to (web/score_gameplay.inc).
  core._set_score_career_cash?.(applied.cash);
  core._set_stage_collect_state(...applied.state);
  if (core._set_stage_collect_row) for (const r of applied.rows) core._set_stage_collect_row(...r);
}

export function collectPoll(core, {careerMode, career, riderId, courseCode}) {
  if (!core?._stage_collect_events) return 0;
  scorePoll(core, { career, riderId });
  const U = new Uint32Array(core.HEAPU8.buffer), at = core._stage_collect_events() >> 2, n = U[at];
  const peakTracks = PEAK_COLLECT_TRACKS[courseCode], track = peakTracks ? core._stage_collect_track?.() ?? -1 : -1;
  const course = peakTracks ? peakTracks[track] : COURSE_INDEX[courseCode];
  if ((careerMode || peakTracks) && career && course !== undefined && n > 0) {
    const events = []; for (let k = 0; k < n; k++) events.push([U[at + 1 + 2 * k], U[at + 2 + 2 * k]]); // copy first: markCollected may grow the heap
    // 153B00: the career bit (saved at once, web/career.js) - what collectStart gives the core after a page reload, a new run or
    // a peak change; within the session the core's own row takes the bit at the award (stage_collectible_award).
    // pv unlistedPickup: 30B9A0 pays a pickup whose resource is not in the stage's list too (30C3E0 finds no index, so no bit; 151178 ->
    // 10F338 -> 119EF8 kind 3 -> 150A90 still adds the cash): the core queues it with index 0xFFFFFFFF (the next core build).
    for (const [index, amount] of events) { if (index === 0xFFFFFFFF && pv('unlistedPickup')) { if (amount > 0) { career.earnCash ? career.earnCash(riderId, amount) : (career.rider(riderId).cash += amount); career.persist?.(); } continue; } career.markCollected(riderId, course, index, amount); }
  }
  return n;
}
// The career's collected list indexes of a course (profile row C+4+12*course: bit = the stage list index of builtin 38), for
// anything that shows or hides a location's collectibles outside the core.
export function collectedIndexes(career, riderId, course) {
  const [lo, hi] = career && riderId ? career.collectMask(riderId, course) : [0, 0], out = [];
  for (let i = 0; i < 64; i++) if (((i < 32 ? lo : hi) >>> (i & 31)) & 1) out.push(i);
  return out;
}
// Conquer the Mountain free ride (0x535C10 == 4, 0x535C11 == 0): tricks, point pickups and combos pay min(points / 500, 20)
// through 119EF8 kinds 0 / 1 / 2 (11A228 / 119608 / 117718; PS2 runs/peak2/fr-d-glide 2371: a 690-point trick pays $1).
// The core queues each award (score_career_events: [count, (kind, amount)...]); the career adds it like any award's cash
// (1597B0 -> 159818: the earnings goal can complete at once; web/career.js earnCash).
export function scorePoll(core, {career, riderId}) {
  if (!core?._score_career_events) return 0;
  const H = new Int32Array(core.HEAPU8.buffer), at = core._score_career_events() >> 2, n = H[at];
  if (!n || !career || !riderId) return n;
  const amounts = []; for (let k = 0; k < n; k++) amounts.push(H[at + 2 + 2 * k]); // copy first: earnCash may grow the heap
  for (const amount of amounts) if (amount > 0) career.earnCash ? career.earnCash(riderId, amount) : (career.rider(riderId).cash += amount);
  career.persist?.();
  return n;
}
