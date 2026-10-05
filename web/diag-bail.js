// Field reports of bails (docs/crash-motion.md "Field bail reports"): one small 'bail' diag event per hard crash of the human
// rider, and one per forced reset, so a playtest report like "I get flung in weird directions" comes with data.
//
// Reporting only: the watch reads the core after each game tick (web/game-tick.js host.bailTick) and never writes to it.
// Nothing personal: the course and screen come from diagnostics.js; the pad is the game's own decoded channels, rounded.
//
// A crash event (crash_info [5], the crash serial, going up) carries:
//   tick       the core's game tick (core._game_tick) of the entry;
//   sem        the crash semantic (crash_info [3], 10EB30's a1: 328..410, 358 a long board press, 360 surface 18);
//   impact     crash_info [11], the impact the entry saw; sub: the motion-2 submode at entry (0 sliding, 1 airborne);
//   attacked   the score object's +0x12C (attacked bails, 119B08) went up on this tick;
//   vIn        the rider's velocity over the tick before the entry (m/s, from the positions: x, y up, z);
//   vOut       the mean velocity over the FOLLOW ticks after the entry (m/s), the launch the player saw;
//   turnDeg    the angle between vIn and vOut (3D), and turnHDeg the same in the horizontal plane;
//   placed     a placement or rescue happened inside the FOLLOW ticks (vOut is then left out);
//   pads       the last PADS ticks' pads, oldest first: [buttons mask (channel i -> bit i, 0 Select .. 15 R2),
//              left x, left y, right x, right y (-100..100), ticks left in that tick's drawn frame (1 = last; 0 = not paced)];
//   multi      how many of those ticks had another tick after them in the same drawn frame (catch-up: both took one pad sample).
// A forced reset (reset_info [2], placements, going up) sends { kind: 'reset', reason: reset_info [4], tick }.
export const BAIL_FOLLOW = 15;
export const BAIL_PADS = 8;
export const BAIL_MAX = 60;

const round1 = (x) => Math.round(x * 10) / 10;
const round100 = (x) => Math.round(x * 100);

function angleDeg(a, b) {
  const la = Math.hypot(a[0], a[1], a[2]);
  const lb = Math.hypot(b[0], b[1], b[2]);
  if (!(la > 0.01 && lb > 0.01)) return null;
  const c = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (la * lb);
  return Math.round((Math.acos(Math.max(-1, Math.min(1, c))) * 180) / Math.PI);
}

function horizontal(v) {
  return [v[0], 0, v[2]];
}

// One pad sample from the 24 decoded channels (web/pad-input.js order): the buttons as a mask, the sticks as -100..100.
export function packPad(input, ticksLeft) {
  let mask = 0;
  for (let i = 0; i < 16; i++) {
    if (input[i] > 0) mask |= 1 << i;
  }
  const axis = (negative, positive) => round100((input[positive] || 0) - (input[negative] || 0));
  return [mask, axis(20, 21), axis(22, 23), axis(16, 17), axis(18, 19), ticksLeft ?? 0];
}

// send(kind, data): diagnostics.js diagnose. Returns { tick(core, state, input, ticksLeft), reset() }.
export function createBailWatch(send, { follow = BAIL_FOLLOW, pads = BAIL_PADS, max = BAIL_MAX } = {}) {
  let ring = [];
  let positions = [];
  let lastSerial = null;
  let lastPlacements = null;
  let lastRescues = null;
  let lastAttacked = null;
  let pending = null;
  let sent = 0;

  function emit(kind, data) {
    if (sent >= max) return;
    sent++;
    send(kind, data);
  }

  function finish(p, position) {
    const out = p.data;
    if (!p.placed && position) {
      const vOut = [0, 1, 2].map((i) => ((position[i] - p.start[i]) * 60) / p.ticks);
      out.vOut = vOut.map(round1);
      out.turnDeg = angleDeg(p.vIn, vOut);
      out.turnHDeg = angleDeg(horizontal(p.vIn), horizontal(vOut));
    } else out.placed = true;
    emit('bail', out);
  }

  function tick(core, state, input, ticksLeft) {
    const crash = new Float32Array(core.HEAPF32.buffer, core._crash_info(), 12);
    const reset = new Float32Array(core.HEAPF32.buffer, core._reset_info(), 9);
    const serial = crash[5];
    const placements = reset[2];
    const rescues = state[13];
    let attacked = null;
    if (core._score_object_dump) attacked = new Uint32Array(core.HEAPU8.buffer, core._score_object_dump(), 0x1d0 / 4)[0x12c / 4];
    const position = [state[0], state[1], state[2]];
    const moved = (lastPlacements !== null && placements !== lastPlacements) || (lastRescues !== null && rescues !== lastRescues);

    ring.push(packPad(input, ticksLeft));
    if (ring.length > pads) ring.shift();
    if (moved) positions = [];
    positions.push(position);
    if (positions.length > 3) positions.shift();

    if (pending) {
      pending.ticks++;
      if (moved) pending.placed = true;
      if (pending.ticks >= follow) {
        finish(pending, position);
        pending = null;
      }
    }

    if (lastSerial !== null && serial > lastSerial) {
      if (pending) finish(pending, null);
      // the velocity into the crash: the two positions before this tick's
      const before = positions.length >= 3 ? positions[positions.length - 2] : null;
      const earlier = positions.length >= 3 ? positions[positions.length - 3] : null;
      const vIn = before && earlier ? [0, 1, 2].map((i) => (before[i] - earlier[i]) * 60) : [0, 0, 0];
      const data = {
        tick: core._game_tick?.() ?? null,
        sem: crash[3],
        impact: round1(crash[11]),
        sub: crash[2],
        attacked: attacked !== null && lastAttacked !== null && attacked > lastAttacked,
        vIn: vIn.map(round1),
        speedIn: round1(Math.hypot(vIn[0], vIn[1], vIn[2])),
        pads: ring.slice(),
        multi: ring.filter((p) => p[5] > 1).length
      };
      // the launch is measured from the position before the entry tick, so the entry tick's own step is the first of the FOLLOW
      pending = { data, vIn, start: before ?? position, ticks: 1, placed: !before };
      if (pending.ticks >= follow) {
        finish(pending, position);
        pending = null;
      }
    }
    if (lastPlacements !== null && placements > lastPlacements) emit('reset', { tick: core._game_tick?.() ?? null, reason: reset[4] });

    lastSerial = serial;
    lastPlacements = placements;
    lastRescues = rescues;
    lastAttacked = attacked;
  }

  // a new run: nothing carries over (a run's start resets the core's serials)
  function resetWatch() {
    ring = [];
    positions = [];
    lastSerial = null;
    lastPlacements = null;
    lastRescues = null;
    lastAttacked = null;
    pending = null;
  }

  return {
    tick,
    reset: resetWatch,
    get sent() {
      return sent;
    }
  };
}
