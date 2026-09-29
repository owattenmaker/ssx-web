// SSX 3 Monster Tricks (docs/tricks-scoring.md "Monster tricks"), recovered from the executable.
// Detection and award are in the core score object (engine/trick_bonus.cpp = 0x11B1A8, engine/score_object.cpp 11A168/11A228):
// the committed trick identity is compared field by field with table 0x43D608 (24 x 16 bytes {id, bonus, 7 fields});
// the first match replaces the identity with {0, id << 27} (the name is then just the monster name, table 0x43D320),
// adds bonus x 0.0001 to the pending points (+0x14), posts HUD slot 0x32 for 1.5 s and calls 0x29B7E0 (Arcade_Uber
// speech variant 8, "Monster Trick"). The in-game detection checks no unlock or ownership state: every monster trick
// scores whenever its trick is performed. Unlocks only gate the front-end list (0x1F5DA0).
//
// fields = [direction (w0 bits 10-11: 0 none, 1 FS, 2 BS), spin (w0 12-15), flip kind (w0 16-18), flip count (w0 19-21),
//           flip spin (w0 28-31), grab score id (w1 3-9), late grab score id (w1 11-17, 0 none)].
export const MONSTER_TRICKS = [
  { id: 1, name: 'Da Housecat', bonus: 10000, trick: 'FS 540 G-Money', fields: [1, 3, 0, 0, 0, 37, 0] },
  { id: 2, name: 'Aphrodite', bonus: 10000, trick: 'Triple Back Flip Superman', fields: [0, 0, 3, 2, 0, 38, 0] },
  { id: 3, name: 'Thrice', bonus: 10000, trick: 'FS Rodeo 720 Smithereen', fields: [1, 0, 1, 3, 4, 39, 0] },
  { id: 4, name: 'Swollen Member', bonus: 10000, trick: 'BS 720 Torpedo', fields: [2, 4, 0, 0, 0, 40, 0] },
  { id: 5, name: 'Yellowcard', bonus: 10000, trick: 'BS 900 Mattrickulater', fields: [2, 5, 0, 0, 0, 43, 0] },
  { id: 6, name: 'Alpine Star', bonus: 20000, trick: 'FS Double Back Flip 540 Karolicker', fields: [1, 0, 2, 2, 3, 44, 0] },
  { id: 7, name: 'MxPx', bonus: 10000, trick: 'Back Flip Nosegrab To Late Judo', fields: [0, 0, 1, 2, 0, 7, 45] },
  { id: 8, name: 'Ultimate dnL BOOST', bonus: 20000, trick: 'FS Rodeo 720 dnL BOOST', fields: [1, 0, 1, 3, 4, 46, 0] },
  { id: 9, name: 'Black Eyed Pea', bonus: 10000, trick: 'BS 900 jib O', fields: [2, 5, 0, 0, 0, 49, 0] },
  { id: 10, name: 'Deepsky', bonus: 10000, trick: 'FS 900 Indian To Late Method', fields: [1, 5, 0, 0, 0, 50, 4] },
  { id: 11, name: 'Basement Jaxx', bonus: 20000, trick: 'BS Misty 900 Hand in Hand', fields: [2, 0, 1, 4, 5, 51, 0] },
  { id: 12, name: 'Fischerspooner', bonus: 20000, trick: 'BS Back Flip 360 Kort Martial To Late Stalefish', fields: [2, 0, 1, 2, 2, 52, 6] },
  { id: 13, name: 'Chemical Brother', bonus: 20000, trick: 'Double Back Flip Bar Hop To Late Mute', fields: [0, 0, 2, 2, 0, 55, 5] },
  { id: 14, name: 'X-Executioner', bonus: 30000, trick: 'FS Triple Back Flip 180 SSXorcist', fields: [1, 0, 3, 2, 1, 56, 0] },
  { id: 15, name: 'Ultimate dnL FlipIt', bonus: 20000, trick: 'FS Double Front Flip 360 dnL FlipIt To Late Indy', fields: [1, 0, 2, 1, 2, 57, 3] },
  { id: 16, name: "Juana's Addicion", bonus: 20000, trick: 'BS 720 Indy To Late Katana', fields: [2, 4, 0, 0, 0, 3, 58] },
  { id: 17, name: 'Audio Bully', bonus: 10000, trick: 'FS 1080 Morgan Grinder', fields: [1, 6, 0, 0, 0, 61, 0] },
  { id: 18, name: 'Finger 11', bonus: 20000, trick: 'BS Back Flip 360 Slinger', fields: [2, 0, 1, 2, 2, 62, 0] },
  { id: 19, name: 'N.E.R.D. Fly or Die', bonus: 30000, trick: 'FS Misty 720 Svelton To Late Nosegrab', fields: [1, 0, 1, 4, 4, 63, 7] },
  { id: 20, name: 'Overseer', bonus: 30000, trick: 'Triple Back Flip Nosegrab To Late Lukeloo', fields: [0, 0, 3, 2, 0, 7, 64] },
  { id: 21, name: 'Stoneage', bonus: 20000, trick: 'BS Double Back Flip 180 Madonna', fields: [2, 0, 2, 2, 1, 67, 0] },
  { id: 22, name: 'Autopilot Off', bonus: 20000, trick: 'Double Back Flip Vacation To Late NIFTY Shifty', fields: [0, 0, 2, 2, 0, 68, 30] },
  { id: 23, name: 'The Automator', bonus: 20000, trick: 'BS 360 NIFTY Shifty To Late Footloose', fields: [2, 2, 0, 0, 0, 30, 69] },
  { id: 24, name: 'Placebo', bonus: 20000, trick: 'BS 540 Indy To Late Trickitello', fields: [2, 3, 0, 0, 0, 3, 70] },
];
// Front-end list order (0x441B40): list position p shows monster MONSTER_LIST[p].
export const MONSTER_LIST = [1, 3, 4, 5, 6, 7, 2, 8, 10, 12, 11, 19, 18, 22, 23, 9, 24, 20, 13, 15, 14, 17, 21, 16];
// Unlocks (0x1F5DA0): position p is unlocked when p % 3 < medal[p / 3]; medal[s] (profile byte R+0xBB8+s, written only by
// 0x155390 from the post-event personal bests 0x155420) is how many of the three thresholds (0x440ED0 + 6s) the stat reached.
export const MONSTER_MEDAL_STATS = [
  { stat: 'StayOnRail', thresholds: [2500, 12000, 30000] },
  { stat: 'HoldHandplant', thresholds: [3, 5, 8] },
  { stat: 'StayInAir', thresholds: [5, 8, 9] },
  { stat: 'KOPeopleRace', thresholds: [3, 6, 10] },
  { stat: 'DoUberGrind', thresholds: [5, 8, 10] },
  { stat: 'DoSupUber', thresholds: [5, 8, 10] },
  { stat: 'GetPoints', thresholds: [1500, 5000, 10000] }, // score / 100
  { stat: 'DoXCombo', thresholds: [10, 20, 100] },
];
// 0x155390: the medal tier a stat value earns (never lowered by the caller).
export const monsterMedal = (stat, value) => MONSTER_MEDAL_STATS[stat].thresholds.filter((t) => value >= t).length;
// Unlocked monster ids for the eight medal bytes.
export function unlockedMonsters(medals) {
  return MONSTER_LIST.filter((_, p) => p % 3 < (medals?.[Math.floor(p / 3)] ?? 0));
}
export const monsterById = (id) => MONSTER_TRICKS.find((m) => m.id === id) || null;

// ---- personal bests -> medals (0x155420 / 0x155390) ----
// The record is the rider's score object from +0xFC (the run statistics), read at the end of every Conquer-the-Mountain run
// (finish 0x1251B8 -> 0x238358 -> 0x154AB8, and reset / restart / quit 0x1297C8 / 0x12B090 with score = time = -1).
// Records R+0xDE4 + 8i {best, course}: 0 KO +0x128, 1 Ubers +0x114, 2 super Ubers +0x118, 3 Uber grinds +0x11C, 4 best
// combo +0x184, 5 handplant +0x158 (s), 6 rail +0x154 (cm), 7 air +0x14C (s) (floats stored as (int)(f + 0.5)), 8 time
// (race finishes, whole seconds, lower is better), 9 score (slope style / big air / pipe / rival points finishes).
export const RUN_RECORDS = [
  { offset: 0x128, medal: 3 }, { offset: 0x114, medal: -1 }, { offset: 0x118, medal: 5 }, { offset: 0x11C, medal: 4 },
  { offset: 0x184, medal: 7 }, { offset: 0x158, float: true, medal: 1 }, { offset: 0x154, float: true, medal: 0 }, { offset: 0x14C, float: true, medal: 2 },
];
// Run statistics from web/runtime core score_object_dump() words (the score object 0..0x1D0).
export function runStats(words) {
  const f = new Float32Array(1), u = new Uint32Array(f.buffer);
  return RUN_RECORDS.map((r) => { const w = words[r.offset / 4] >>> 0; if (!r.float) return w | 0; u[0] = w; return Math.trunc(Math.fround(f[0] + 0.5)); });
}
// 0x155420 on a monster state {bests: [10], medals: [8], bestCombo}. finish: {score, seconds, scoreEvent, timeEvent} or null.
// Returns the medal indices whose tier rose.
export function recordRun(state, stats, finish = null) {
  state.bests ??= Array(10).fill(null); state.medals ??= Array(8).fill(0); const raised = [];
  const bump = (medal, value) => { if (medal < 0) return; const tier = monsterMedal(medal, value); if (tier > state.medals[medal]) { state.medals[medal] = tier; raised.push(medal); } };
  stats.forEach((v, i) => { if (state.bests[i] == null || v >= state.bests[i]) { state.bests[i] = v; bump(RUN_RECORDS[i].medal, v); } });
  if (finish?.timeEvent && finish.seconds >= 0 && (state.bests[8] == null || finish.seconds <= state.bests[8])) state.bests[8] = finish.seconds;
  if (finish?.scoreEvent && finish.score > 0 && (state.bests[9] == null || finish.score >= state.bests[9])) { state.bests[9] = finish.score; bump(6, Math.trunc(finish.score / 100)); }
  state.bestCombo = Math.max(state.bestCombo ?? 0, stats[4]);
  return raised;
}
