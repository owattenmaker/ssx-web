// Online race pacing (web/net/mp-game.js pace): the seconds of game time one drawn frame asks the frame clock
// (web/fixed-step-clock.js) for, so that the race tick follows the server clock (ticks since GO) through frame hitches,
// a slow tab and clock drift: up to CATCH_UP ticks per frame more, SLOW_DOWN tick fewer, beyond SLACK ticks.
//   expected  the server clock in ticks since GO (Date.now() - the local GO instant) * 0.06
//   tick      the race ticks run so far
//   dt        the frame's seconds
//   pending   the frame clock's debt in seconds: it runs at most 12 ticks a frame and keeps the rest, so that time is
//             already scheduled and is not asked for again. Counting only the ticks run (pending 0: clients before
//             2026-09-28), a catch-up after a hitch re-added the same backlog every frame; the surplus then ran the race
//             tick ~50-600 ticks ahead of the server clock for seconds (web/server/plausibility.mjs clock-ahead).
export const SLACK = 2, CATCH_UP = 12, SLOW_DOWN = 1;
export function paceSeconds({ expected, tick, dt, pending = 0 }) {
  const behind = expected - tick - 60 * (dt + pending);
  if (Math.abs(behind) <= SLACK) return dt;
  return Math.max(0, dt + Math.sign(behind) * Math.min(Math.abs(behind) - SLACK, behind > 0 ? CATCH_UP : SLOW_DOWN) / 60);
}
