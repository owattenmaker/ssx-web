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
// pv mpTickLock (main.js frame): the online frame lock. A ~60 Hz drawn frame of an online race runs one tick and draws it, as offline
// (web/tick-lock.js), with the server clock as the reference instead of tickLock's own debt.
//   - A frame is locked once it and the 2 frames before it were ~60 Hz frames (0.75..1.25 ticks, tick-lock.js's bounds). A lone
//     2-vsync frame of a 120 Hz display, a missed vsync and the first frames after a stall keep the race pace above (its catch-up).
//   - behind = the server clock's ticks not yet run at the frame's start (mpGame.behind(): expected - tick; null without a GO).
//   - Behind: a locked frame whose one tick would leave the race tick more than LOCK_SLACK behind the server clock runs one tick more.
//     The unlocked fixed clock keeps about the same band (0..1 tick behind, plus the jitter), so a locked tick does not run later
//     against the server clock than today's.
//   - Ahead: one tick fewer (a frame without a tick) only past SLACK ahead, where paceSeconds slows down too, so a locked frame never
//     holds a pad sample for a frame where today's path would not (a clock-offset jump of a tick or so stays inside the band).
//   - A correction moves the lag a whole tick away from the edge it crossed: no 0 / 2 flip-flop under rAF jitter below a tick.
// frame(dt, behind) -> the frame's ticks, or null (the caller paces the frame).
export const LOCK_STREAK = 3;
export const LOCK_SLACK = 1;
export function createRaceLock() {
  const step = 1 / 60;
  let streak = 0;
  return {
    frame(dt, behind) {
      const sixty = dt >= 0.75 * step && dt <= 1.25 * step;
      streak = sixty ? streak + 1 : 0;
      if (streak < LOCK_STREAK || behind == null || !Number.isFinite(behind)) return null;
      const after = behind - 1;
      if (after > LOCK_SLACK) return 2;
      if (after < -SLACK) return 0;
      return 1;
    },
    reset() {
      streak = 0;
    }
  };
}
