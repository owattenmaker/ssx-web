// The TAS policy (docs/tas.md "Search"): a macro (one search decision, held for a segment of ticks) turned into pad frames from
// the rider's state each tick. Closed loop on purpose: a steering macro follows the guide line with a lateral offset, so a
// change earlier in the run does not invalidate the macros after it. The policy's own memory (guide index, last position, air
// ticks, the air program) is a plain object, saved and restored with the race.
//
// macro = {
//   offset: lateral metres from the guide line (+ = left of the direction of travel),
//   look:   lookahead metres along the guide,
//   tuck:   left stick up on the ground (the crouch that raises the speed limit 11B3F8 reads),
//   boost:  'off' | 'on' (Square on the ground) | 'auto' (Square while the meter is above 2/3, or Super Uber holds it at 1),
//   jump:   true: Cross held (the prewind); the next macro without jump releases it: the ollie (control 5 air, the D-pad tricks),
//   pre:    with jump: the D-pad direction held in the prewind ('DPadLeft' ...),
//   program: an air program, armed for the next take-off (or started now when airborne without one):
//           {steps: [{from, to, press: ['DPadLeft', 'R1', 'Square', ...]}]} by air tick (1 = the first airborne tick),
//   cancel: airborne: drop the running program (land clean),
//   stick:  optional raw left-stick x byte on the ground instead of the guide (0 left .. 255 right)
// }
// In the air the left stick stays centred (the D-pad spins and flips, INPUT.MAP Spin = DPadR - DPadL; a held stick there is
// the passive air's lean).
import { neutralFrame, withButton } from './pad-format.mjs';
import { nearestIndex, pointAhead } from './guide.mjs';

const wrap = (a) => Math.atan2(Math.sin(a), Math.cos(a));

export const DEFAULT_MACRO = Object.freeze({ offset: 0, look: 12, tuck: true, boost: 'auto', jump: false, pre: null, program: null, cancel: false });

export function newPolicyMemory() {
  return { index: 0, last: null, air: 0, heading: null, armed: null, active: null };
}

export function cloneMemory(m) {
  return { index: m.index, last: m.last ? m.last.slice() : null, air: m.air, heading: m.heading, armed: m.armed, active: m.active };
}

// state: rider_state (16 floats); boost: core boost_info (meter, amount, window, tier, modifier, superTime)
export function policyFrame(guide, macro, mem, state, boost, gain = 3) {
  const p = [state[0], state[1], state[2]];
  const grounded = !!state[8];
  if (grounded) {
    mem.air = 0;
    mem.active = null;
  } else {
    mem.air++;
    if (mem.air === 1) {
      mem.active = mem.armed;
      mem.armed = null;
    }
  }
  if (!grounded && macro.cancel) mem.active = null;
  if (macro.program) {
    if (grounded) mem.armed = macro.program;
    else if (!mem.active) mem.active = { steps: macro.program.steps.map((s) => ({ ...s, from: s.from + mem.air - 1, to: s.to + mem.air - 1 })) };
  }
  mem.index = nearestIndex(guide, p, mem.index).index;
  if (mem.last) {
    const dx = p[0] - mem.last[0];
    const dz = p[2] - mem.last[2];
    if (dx * dx + dz * dz > 1e-6) mem.heading = Math.atan2(dx, dz);
  }
  mem.last = p;
  let f = neutralFrame();
  if (grounded) {
    if (macro.stick != null) {
      f.sticks[0] = macro.stick;
    } else {
      const look = macro.look ?? 12;
      const target = pointAhead(guide, mem.index, look);
      const next = pointAhead(guide, mem.index, look + 2);
      // the offset is to the left of the guide's own direction there: h = atan2(dx, dz) rises to the left, so the left
      // normal of (gx, gz) is (gz, -gx)
      const gx = next[0] - target[0];
      const gz = next[2] - target[2];
      const gl = Math.hypot(gx, gz) || 1;
      const off = macro.offset ?? 0;
      const tx = target[0] + (gz / gl) * off;
      const tz = target[2] - (gx / gl) * off;
      const h = mem.heading ?? state[3];
      const want = Math.atan2(tx - p[0], tz - p[2]);
      const err = wrap(want - h);
      const steer = Math.max(-1, Math.min(1, err * gain));
      f.sticks[0] = Math.max(0, Math.min(255, Math.round(128 - steer * 127.5)));
    }
    if (macro.tuck && !macro.jump) f.sticks[1] = 0;
    const meter = boost[0];
    const tier = boost[3];
    const on = macro.boost === 'on' || (macro.boost === 'auto' && (meter > 0.6667 || tier >= 10));
    if (on && !macro.jump) f = withButton(f, 'Square');
    if (macro.jump) {
      f = withButton(f, 'Cross');
      if (macro.pre) f = withButton(f, macro.pre);
    }
  } else if (mem.active) {
    for (const s of mem.active.steps) {
      if (mem.air >= s.from && mem.air <= s.to) {
        for (const b of s.press) f = withButton(f, b);
      }
    }
  }
  return f;
}
