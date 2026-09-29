// Rival challenges (game modes 4 Rival Time / 5 Rival Points) on the backcountry courses: Happiness ABC1 (Peak 1),
// Ruthless DBC2, The Throne EBC3. Pure rules from SLUS_207.72 (docs/backcountry.md); no DOM, no core.
//
// Handlers (0x238160 mode -> GameModeMan handler, jump table 0x47C0F0): mode 4 -> handler 5 (vtable 0x47CD30, init
// 0x23B6C0, results 0x23B8C8); mode 5 -> handler 6 (vtable 0x47CCC8, init 0x23BB98, results 0x23BDB8). Both: one round
// (+0 = +0x70 = 1), two riders (+0x10 = 1): the player and the peak rival 0x145750 in the next slot.

export const RIVAL_TIME = 4, RIVAL_POINTS = 5;
export const DNF_TICKS = 360000;          // 0x23B964: lui 5 / ori 0x7E40
// Event kind 0x535C10 of the rival modes (0x43E7D0): 5 time challenge, 6 points challenge.
export const eventKind = (mode) => (mode === RIVAL_TIME ? 5 : 6);
// GameModeMan handler index GMM+4 (0x238160) and the checkpoint-bonus accept rule of that handler (vtable +0x30 = 0x244530).
export const handlerIndex = (mode) => (mode === RIVAL_TIME ? 5 : 6);
// HUD flags 0x478078[mode] = 0x1530C047 (both: race place 0x1, score 0x2, clock 0x4, progress 0x40); 1EA930 then adds
// 0x08000000 (the OPPONENT line) when the event kind is neither 0 nor 5, i.e. Rival Points only, and with two or more
// riders sets 0x1 and clears 0x30.
export function hudFlags(mode) {
  let f = 0x1530C047 | 0x1;
  f &= ~0x30;
  if (eventKind(mode) !== 0 && eventKind(mode) !== 5) f |= 0x08000000;
  return f >>> 0;
}

// 0x145750: Peak 1 Mac (3), or Griff (5) when the player is Mac; Peak 2 Nate (7) / Zoe (4); Peak 3 Psymon (8) / Elise (6).
// CHARDB indices; the browser's Sam takes Mac's slot (as in the Sam PS2 build), so Sam meets Griff.
export function rivalCharacter(peak, humanCharacter) {
  return [[3, 5], [7, 4], [8, 6]][peak - 1][humanCharacter === [3, 7, 8][peak - 1] ? 1 : 0];
}
export const CHARACTER_ID = ['moby', 'kaori', 'allegra', 'mac', 'zoe', 'griff', 'elise', 'nate', 'psymon', 'viggo'];

// Rival Points time limit: 1454F8(profile, 1) = table 0x440B38 row (course, round 1) +0xE seconds, x 60 (GMM+0x78, +0x88 = 1).
// Rows 27..29 are the backcountry courses 14..16: posted scores all 0, 300 s. Rival Time is untimed (+0x78 = 0).
export const RIVAL_POINTS_LIMIT_SECONDS = { 14: 300, 15: 300, 16: 300 };
export function timeLimitTicks(mode, course) {
  return mode === RIVAL_POINTS ? (RIVAL_POINTS_LIMIT_SECONDS[course] ?? 300) * 60 : 0;
}

const f = Math.fround;
// 0x122D78 race estimate of a rider still on course when the player finishes (cm per tick, floor 30 - slot).
export function estimateTicks(raceTicks, origin, remaining, slot) {
  let average = f(f(f(origin) - f(remaining)) / f(raceTicks));
  const floor = f(30 - slot);
  if (!(average >= floor)) average = floor;
  return raceTicks + Math.trunc(f(f(remaining) / average));
}
// 0x122E50 points estimate: s = +0x198 + 1; s + cvt.w.s(max(+0x4D0 - 1000, 0) * (float(s) / max(113130 - +0x4D0, 1))).
export function estimatePoints(score, remaining, origin) {
  const s1 = ((score | 0) + 1) | 0, rem = f(remaining);
  let done = f(f(origin) - rem); if (done < 1) done = 1;
  let left = f(rem - 1000); if (left < 0) left = 0;
  const v = f(left * f(f(s1) / done));
  return (s1 + (v >= 2147483648 ? 0x7fffffff : v <= -2147483648 ? -0x80000000 : Math.trunc(v))) | 0;
}

// 0x238BF8 (rank 10, times ascending, unsigned) / 0x238B70 (rank 2, scores descending): 0-based places.
function rankBy(values, ascending) {
  const order = values.map((v, slot) => ({ v: ascending ? v >>> 0 : v | 0, slot }))
    .sort((a, b) => (ascending ? a.v - b.v : b.v - a.v) || a.slot - b.slot);
  const place = []; order.forEach((e, k) => { place[e.slot] = k; }); return place;
}

// Round result. rows: [{human, dnf (+0x480, give up / time up), finishTicks (+0x478) | null, score (+0x198, the value
// latched at the rider's finish for a finished computer rider), remaining (+0x4D0), origin (+0x4D8)}], slot order.
// Rival Time (23B8C8): time = 360000 for +0x480, +0x478 when finished, else the 0x122D78 estimate; ranked by time.
// Rival Points (23BDB8): score = 0 for +0x480, the run score when finished, else the 0x122E50 estimate; ranked by score.
// Win = place 0 and not given up: rider +0x100 = 1 (celebrate), GMM +0x84 = 1 (fresh), +0x74 = +0x70, +0x70 = 0;
// otherwise +0x100 = 0 and the round is played again. The event is complete (+0x9C = 1) either way.
export function rivalResult(mode, rows, raceTicks) {
  const values = rows.map((r, slot) => {
    if (mode === RIVAL_TIME) return r.dnf ? DNF_TICKS : r.finishTicks != null ? r.finishTicks : estimateTicks(raceTicks, r.origin, r.remaining, slot);
    return r.dnf ? 0 : r.finishTicks != null || r.human ? r.score | 0 : estimatePoints(r.score, r.remaining, r.origin);
  });
  const place = rankBy(values, mode === RIVAL_TIME);
  const human = rows.findIndex((r) => r.human), win = place[human] === 0 && !rows[human].dnf;
  return { mode, values, place, human, win, humanPlace: place[human], estimated: rows.map((r, slot) => !r.human && r.finishTicks == null && !r.dnf) };
}

// Objectives card (PS2 frames local/ps2-capture/nav/bc/out-*-load/final.png): title "Rival Challenge" (CMNAMER 0x0626EF15),
// subtitle "<course> - Race" / "<course> - Jam", "Face off against %s in a Rival Challenge!" (OVAMER 0x0005EBD9) and one
// bullet: Rival Time "The first rider to the bottom of Backcountry wins." (OVAMER 0x0177555D), Rival Points "Get to the
// bottom of Backcountry with more points than your rival." (OVAMER 0x083E70D3).
export function objectives(mode) {
  return { title: 0x0626ef15, headline: 0x0005ebd9, bullets: [mode === RIVAL_TIME ? 0x0177555d : 0x083e70d3] };
}
