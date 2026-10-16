// pv mpEstimateClock (docs/multiplayer.md "Results of racers still on course"): the order of an online race's Results rows when a
// racer still on course has no time.
// PS2: each finish runs 125108 -> 238358 -> the mode handler's results (+0x44: 0x23A760 race, 0x23AE00 the multiplayer race). Only
// once every human has finished (12A250 at 0x23A8B4 / 0x23AF0C) does it give the riders still on course (+0x470 < 0) the 0x122D78
// estimate (0x23A954 / 0x23AF74), once. So a human is never estimated, only computer riders. The Results row loop (0x1E5AA0) writes a
// row's time text 'timePoints%d' as "DNF" (0x4A2160) for a rider who gave up (0x5366A8), else its time 0x536640; it has no form for
// an unset time, because its Results wait for every human. Online every racer is a human: a racer still on course gets no time
// (onCourse) until its recorded time comes; its row is drawn with the time left empty.
// rows: {ticks, dnf, onCourse, ...}. Order: the timed rows by time (0x238BF8 order: unsigned ascending, equal times by row), then the
// racers still on course in row (slot) order, then DNF.
import { orderByTime } from '../ai-race.js';

export function orderOnlineRows(rows) {
  const timed = rows.filter((r) => !r.dnf && !r.onCourse);
  const riding = rows.filter((r) => !r.dnf && r.onCourse);
  const dnfs = rows.filter((r) => r.dnf);
  const sorted = orderByTime(timed.map((r) => r.ticks)).map((i) => timed[i]);
  return [...sorted, ...riding, ...dnfs].map((r, place) => ({ ...r, place }));
}
