// pv tickLock (docs/workers.md "One tick per drawn frame at 60 Hz"): on a 60 Hz display a drawn frame runs exactly one game tick and
// draws that tick's state, as the PS2 does (one update per vblank, 0x316F00, and the newest state drawn: no interpolation).
//
// Without it the fixed clock keeps the frame's leftover time and the page draws between the last two ticks at alpha = leftover * 60
// (main.js renderAlpha): the view shows a state 0..1 tick older than the newest one (mean ~8 ms, up to 16.7 ms), and when the leftover
// sits near a tick boundary, rAF jitter alternates 0- and 2-tick frames (a judder, and a frame whose input waits a tick longer).
//
// frame(dt) -> the ticks to run this frame (0, 1 or 2), or null when the frame is not a ~60 Hz frame (a stall, a 120 / 144 Hz display
// drawing every frame): the caller then uses its own pacing. The time between frames is kept as a debt; past MAX_DEBT ticks one frame
// runs 2 ticks (or 0) to pay it, so the game stays on real time within a few ticks over a session (a 59.94 Hz display: one extra tick
// about every 70 s). The simulation is the same tick for tick: only which drawn frame runs a tick changes.
const STEP = 1 / 60;
const LOW = 0.75 * STEP;
const HIGH = 1.25 * STEP;
const MAX_DEBT = 4 * STEP;
export function createTickLock() {
  let debt = 0;
  return {
    frame(dt) {
      if (!(dt >= LOW && dt <= HIGH)) {
        debt = 0;
        return null;
      }
      debt += dt - STEP;
      if (debt > MAX_DEBT) {
        debt -= STEP;
        return 2;
      }
      if (debt < -MAX_DEBT) {
        debt += STEP;
        return 0;
      }
      return 1;
    },
    reset() {
      debt = 0;
    },
    get debt() {
      return debt;
    }
  };
}
