// Pad vibration (FE Controller Settings > Vibration 1P, web/fe-options.js) with the original's motor model,
// driving the Gamepad API's dual-rumble actuator. docs/characters.md "Setup Character, Rider Details, Options,
// Load game" (Vibration).
//
// The PS2 keeps two intensities on the human's motion owner: +0xDFC (v0, raised to max(v0, x) through rider
// vtable +0x88 = 0x1278D0) and +0xE00 (v1, set through +0x90 = 0x1278E0); +0x80 (0x1278C0) zeroes both on a reset.
// Once per frame 0x1278E8 -> 0x125B18 (when a pad is present; motors only while the Vibration option is On and the
// game state is below 10, 0x127900 / 0x1474E8) decays them and drives the DualShock (0x327740, stop 0x326CF0):
//   v0 = max(v0 * 0.9133333 - 18.518518, 0), v1 *= 0.9916667        (single-precision, like the EE)
//   large motor  f = max((v1 - 0.5) * 0.01, (v0 - 2) / 972.2222, 0), byte = f * 205 + 50 (off at f = 0)
//   small motor  on while max((v1 - 70) * 0.00625, (v0 - 100) / 5000, 0) > 0
// The core posts every rumble call of the rider step as an audio event (web/audio_events.hpp):
//   AE_RUMBLE_IMPACT (30)  v0 = max(v0, a): landing award 0x10E910 at 10EAD8 (0.5 x the landing speed; board,
//                          rail and instance landings), crash entry 0x10EB30 at 10EB78, obstacle notifications
//                          0x105D98 at 105E7C, instance contacts 0x105398 at 105C30, and the crash controller's
//                          ragdoll impacts 12D28C / 12D6F8 / 12D8A0
//   AE_RUMBLE_SLIDE  (31)  v1 = a: crash slide 12D23C (0.5 x the 0x12E528 playback base, every phase-2 tick)
// tick() must run after the step and before web/sfx-game.js drains the queue (main.js, just ahead of
// gameAudio.gameTick). Replayed against the PS2 captures (web/test-rumble.mjs), v0 matches the recorded +0xDFC on
// every tick of mix-glide, air-tricks and event-race, and v1 the recorded +0xE00 of the derived mix-glide /
// air-tricks captures (local/ps2-capture/runs/rumble, --watch owner+0xDFC:8).
import { activeRawPad } from './gamepad.js';

export const RUMBLE = Object.freeze({ decay0: 0.9133333, sub0: 18.518518, decay1: 0.9916667 });
export const AE_RUMBLE_IMPACT = 30, AE_RUMBLE_SLIDE = 31;
const f32 = Math.fround;

export function rumbleMotors(v0, v1) {
  const f = Math.min(1, Math.max((v1 - 0.5) * 0.01, (v0 - 2) / 972.2222, 0));
  return { strong: f > 0 ? (f * 205 + 50) / 255 : 0, weak: Math.max((v1 - 70) * 0.00625, (v0 - 100) / 5000, 0) > 0 ? 1 : 0 };
}

// The rumble calls of one step: [type, a] pairs from the core audio queue (1 + 64 x 5 floats, count first),
// read without clearing it (web/sfx-game.js clears it).
export function rumbleEvents(core) {
  if (!core?._audio_events) return [];
  const q = core._audio_events() >> 2, F = new Float32Array(core.HEAPF32.buffer), n = Math.min(F[q] | 0, 64), out = [];
  for (let k = 0; k < n; k++) { const t = F[q + 1 + 5 * k]; if (t === AE_RUMBLE_IMPACT || t === AE_RUMBLE_SLIDE) out.push([t, F[q + 2 + 5 * k]]); }
  return out;
}

export class Rumble {
  // pad(): the Gamepad to vibrate, by default the active one of every slot (web/gamepad.js), not slot 0.
  constructor(pad = activeRawPad) { this.pad = pad; this.reset(); }
  reset() { this.v0 = 0; this.v1 = 0; this.stop(); }                    // 0x1278C0
  // One 60 Hz tick: this step's rumble calls, then the 0x125B18 decay; returns the motor levels.
  update(events = []) {
    for (const [type, a] of Array.isArray(events) ? events : []) {
      if (type === AE_RUMBLE_IMPACT) { if (a > this.v0) this.v0 = f32(a); }   // 0x1278D0
      else if (type === AE_RUMBLE_SLIDE) this.v1 = f32(a);                     // 0x1278E0
    }
    this.v0 = Math.max(f32(f32(this.v0 * RUMBLE.decay0) - RUMBLE.sub0), 0); this.v1 = f32(this.v1 * RUMBLE.decay1);
    return rumbleMotors(this.v0, this.v1);
  }
  tick(core, enabled) {
    const m = this.update(rumbleEvents(core));
    if (enabled) this.drive(m); else if (this.last) this.stop();
    return m;
  }
  drive({ strong, weak }) {
    const pad = this.pad(), act = pad?.vibrationActuator;
    // Firefox: GamepadHapticActuator.pulse(value, ms) (behind dom.gamepad.haptic_feedback.enabled), one motor.
    const pulse = !act?.playEffect && pad?.hapticActuators?.[0]?.pulse ? pad.hapticActuators[0] : null;
    if (!act?.playEffect && !pulse) return;
    this.age = (this.age || 0) + 1;
    const same = this.last && Math.abs(this.last.strong - strong) < 0.02 && this.last.weak === weak;
    if (!strong && !weak) { if (this.last && (this.last.strong || this.last.weak)) this.stop(); this.last = { strong, weak }; return; }
    if (same && this.age < 5) return;                                      // refresh the 100 ms effect every ~83 ms
    this.age = 0; this.last = { strong, weak };
    if (pulse) { try { pulse.pulse(Math.max(strong, weak * 0.5), 100)?.catch?.(() => {}); } catch {} return; }
    act.playEffect('dual-rumble', { startDelay: 0, duration: 100, strongMagnitude: strong, weakMagnitude: weak })?.catch?.(() => {});
  }
  stop() {                                                                 // 0x326CF0: all motors off
    this.last = null; this.age = 0;
    try { const pad = this.pad(), act = pad?.vibrationActuator; if (act?.reset) act.reset()?.catch?.(() => {}); else pad?.hapticActuators?.[0]?.pulse?.(0, 0)?.catch?.(() => {}); } catch {}
  }
}
