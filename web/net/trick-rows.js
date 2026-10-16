// pv mpBoard: the order of an online trick event's Results rows (slope style, super pipe, big air), by score.
// PS2 (recompiled code, the tables read from SLUS_207.72): in two-player play (0x535C11 == 2) initGameMode 0x238160 takes the mode
// through the jump table 0x47C0D0: race -> handler 8 (vtable 0x47CED0, results 0x23AE00), slope style / super pipe / big air ->
// handler 9 (vtable 0x47CE68, results +0x44 = 0x239EC8), Rival Time / Points -> 5 / 6. 0x239EC8 copies the rider's score object
// (+0x790 -> +0xFC..+0x1A8) and stores its total (+0x198, the stack's +0x9C) in 0x536640[player]; a rider with +0x480 (gave up) gets
// 0 and the DNF flag 0x5366A8. Once every human has finished (12A250 at 0x23A00C) it ranks with 238B70(results, 2): qsort (418EF8)
// of the order 0x536708 with 0x245E30 (unsigned, the higher value first), then the places 0x536730 from it with 0x245EC0. The race
// handler 0x23AE00 ranks with 238BF8 (0x245E78: the lower time first). Equal scores compare 0 in 0x245E30; which of them qsort puts
// first is not traced, so equal scores keep the row (slot) order here.
// rows: {score, dnf, onCourse, ...}. Order: the scored rows by score, highest first, then the racers still on course in row order,
// then DNF.
export function orderTrickRows(rows) {
  const scored = rows.filter((r) => !r.dnf && !r.onCourse && Number.isInteger(r.score));
  const riding = rows.filter((r) => !r.dnf && !scored.includes(r));
  const dnfs = rows.filter((r) => r.dnf);
  const sorted = scored
    .map((r, i) => ({ r, i }))
    .sort((a, b) => (b.r.score >>> 0) - (a.r.score >>> 0) || a.i - b.i)
    .map((x) => x.r);
  return [...sorted, ...riding.map((r) => ({ ...r, onCourse: true })), ...dnfs].map((r, place) => ({ ...r, place }));
}
// the course's event kind is a trick event (courses.json `event`)
export const TRICK_EVENTS = ['slopestyle', 'superpipe', 'bigair'];
