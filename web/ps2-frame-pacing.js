// How much game time one drawn frame advances after a slow frame, as the PS2's frame loop does it (pv stallCap, web/main.js
// frame; docs/workers.md "Game tick"). Offline play only: online races keep the server-paced full catch-up.
//
// The PS2 (SLUS_207.72, recompiled local/output; measured with tools/ps2_capture.py stall savestates, docs/HANDOFF.md):
//   - main 0x31AF80 runs the app framework 0x316D60 on the app object *(gp+0x2A74) = 0x4C9428 (+0x10 = 60 Hz,
//     +0x14 = 1/60, +0x20 = 12);
//   - one game update per pad sample. The pad writer 0x326B88 stores one sample per vblank into a 30-slot ring
//     (*(gp-0x850), read +0x2EE0, write +0x2EE4 = (write + 1) % 30, overwriting). After each update the frame loop
//     0x316F00 calls the app's +0x34 (0x227E98): when a sample is pending (0x326B48) it feeds it to the pad history
//     0x321298 and loops to the next update at once, without drawing;
//   - 0x317188: at most app+0x20 = 12 updates per drawn frame. Past that, +0x2C (0x227E68 -> 0x326C60) resyncs the ring
//     (read = write - 1): the backlog is dropped;
//   - a frame of F vblanks therefore runs min(F, 12) updates, and a backlog of 30 or more laps the ring, so only
//     (F - 1) mod 30 + 1 are pending. Stall savestates at Crow's Nest tick 700 (K extra vblanks): K = 2, 4, 11 run K catch-up
//     updates; 12..29 and 45..59 run 11; 31, 35, 40 run 1, 5, 10; 60, 90, 120 run none. (K = 30 ran 11 where the lap predicts
//     none: the ring's write index had lapped onto the read index, a sub-frame race.)
// The port's FixedStepClock keeps its debt instead (12 ticks a frame until paid): a Safari stall turned into a fast-forward of
// every missed tick, e.g. the released air auto-complete (7-9 degrees a tick) finishing in a blink.
export const PS2_MAX_UPDATES = 12;   // app +0x20
export const PS2_PAD_RING = 30;      // 0x326B88 / 0x326B48 ring slots
const STEP = 1 / 60;
// The game time (seconds) a drawn frame of `seconds` may advance: min(F, 12) ticks of F elapsed ticks, F lapped by the 30 ring.
// The clock's leftover is below one tick, so feeding this to FixedStepClock.advance runs at most 12 ticks and keeps no debt.
export function ps2FrameTime(seconds) {
  if (!(seconds > 0)) return 0;
  let ticks = seconds / STEP;
  if (ticks > PS2_PAD_RING) ticks = ((ticks - 1) % PS2_PAD_RING) + 1;
  return Math.min(ticks, PS2_MAX_UPDATES) * STEP;
}
