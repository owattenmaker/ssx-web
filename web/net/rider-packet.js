// Online rider packets (binary WebSocket frames; the server prefixes the sender's slot byte when relaying).
//
// State (kind 2), sent at 20 Hz by every racer for its own rider, little endian:
//   0  u8  kind = 2
//   1  u8  flags: 1 finished, 2 gave up / DNF, 4 pair view present, 8 lighting present, 16 FX records present
//   2  u8  bones B              (world_pose_bones count)
//   3  u8  body spheres S       (pair view AA0 sphere count, <= 20)
//   4  u32 race tick            (ticks since the shared GO; every racer's tick 0 is the same wall-clock instant)
//   8  f32 remaining            (+0x4D0 route distance, the ranking key 0x10F998)
//  12  f32 finish ticks         (-1 while on course)
//  16  f32 x3 authored body scale (graph.scale: rider_skin_scale)
//  28  u16 reset placements     (111890 placements; with the rescues a teleport, never interpolated across)
//  30  u16 rescues
//  32  f32 x3 physical position (source cm, pair view 99..101)
//  44  f32 x3 velocity          (source cm/s, pair view 87..89)
//  56  B x 7 f32 world pose     (world_pose_bones: position xyz, quaternion xyzw per bone -- the palette is rebuilt
//                                bit for bit from these, web/net/pose-codec.js)
//      pair view (flag 4): words 0..6, S x 4 sphere words (7..), words 87..136 of pair_view (web/npc_gameplay.inc)
//      lighting (flag 8): environment irradiance bank 40 f32, rim scalar, query bounds min3 max3, rank point 3
//      FX records (flag 16): u8 count, then per record u8 ticks before this packet's tick, u8 flags (1 delta,
//                            2 a reset placement before this pass, 4 a rescue / new run before it) and the rider FX
//                            inputs of that tick's FX pass (web/fx_puppet.inc): the first in full, later ones as a
//                            81-bit change mask + the changed fields (typed: small integers u8/i16/i32, floats f32)
// About 1.4 KB + ~0.45 KB of FX records for a 27-bone rider (the old palette packet was ~10 KB).
//
// World (kind 4, reliable): shared world entity changes of this racer's core (web/shared_world.inc): u16 trigger
//   count, u16 event word count, trigger keys u32..., event words u32... (replayed in every other racer's core as
//   web/ai-racers.js does between the six rider cores).
//
// Attack (kind 3): 0 u8 kind, 1 u8 victim slot, 2 u8 attacker slot, 3 u8 0, 4 u32 tick, 8 f32 x3 direction, 20 f32 impulse.
//   Sent by the attacker's client when its 107888 attack branch hit a remote rider; the victim's client applies the
//   107E70 response to its own rider (web/net/pair-net.js).
export const STATE = 2, ATTACK = 3, WORLD = 4;
export const FLAG_FINISHED = 1, FLAG_DNF = 2, FLAG_PAIR = 4, FLAG_LIGHTING = 8, FLAG_FX = 16;
const HEADER = 56, PAIR_TAIL = 50, LIGHTING = 50;

export function stateSize(bones, spheres, pair = true, lighting = true) {
  return HEADER + bones * 28 + (pair ? (7 + spheres * 4 + PAIR_TAIL) * 4 : 0) + (lighting ? LIGHTING * 4 : 0);
}

// ---- FX records (web/fx_puppet.inc FX_RECORD layout) ----
export const FX_FIELDS = 81;
// Wire type per field: 1 u8, 2 i16, 3 u16, 4 i32, 5 u32, 0 f32 (exact bits).
const FX_TYPES = (() => {
  const t = new Uint8Array(FX_FIELDS); // f32 by default
  const set = (type, ...ids) => { for (const i of ids) t[i] = type; };
  set(1, 0, 1, 2, 27, 32, 36, 61); set(5, 3, 38); set(2, 4, 8, 9, 46, 47, 48, 49, 55, 57); set(3, 50, 51, ...Array.from({ length: 14 }, (_, i) => 67 + i)); set(4, 58, 62);
  return t;
})();
const FX_SIZE = [4, 1, 2, 2, 4, 4], MASK = Math.ceil(FX_FIELDS / 8);
function writeField(v, at, type, x) {
  switch (type) { case 1: v.setUint8(at, x); break; case 2: v.setInt16(at, x, true); break; case 3: v.setUint16(at, x, true); break; case 4: v.setInt32(at, x, true); break; case 5: v.setUint32(at, x, true); break; default: v.setFloat32(at, x, true); }
  return at + FX_SIZE[type];
}
function readField(v, at, type) {
  switch (type) { case 1: return v.getUint8(at); case 2: return v.getInt16(at, true); case 3: return v.getUint16(at, true); case 4: return v.getInt32(at, true); case 5: return v.getUint32(at, true); default: return v.getFloat32(at, true); }
}
// records: [{back (ticks before the packet tick), values Float32Array(FX_FIELDS), reset (0, 1 placement, 2 rescue)}]
export function encodeFx(records) {
  const out = new DataView(new ArrayBuffer(1 + records.length * (2 + MASK + FX_FIELDS * 4))); let at = 0;
  out.setUint8(at++, records.length);
  let prev = null;
  for (const r of records) {
    out.setUint8(at++, Math.min(255, r.back));
    const resetBits = (r.reset === 1 ? 2 : 0) | (r.reset === 2 ? 4 : 0);
    if (!prev) { out.setUint8(at++, resetBits); for (let i = 0; i < FX_FIELDS; i++) at = writeField(out, at, FX_TYPES[i], r.values[i]); }
    else {
      out.setUint8(at++, 1 | resetBits); const maskAt = at; at += MASK; const mask = new Uint8Array(MASK);
      for (let i = 0; i < FX_FIELDS; i++) if (!Object.is(r.values[i], prev[i])) { mask[i >> 3] |= 1 << (i & 7); at = writeField(out, at, FX_TYPES[i], r.values[i]); }
      for (let k = 0; k < MASK; k++) out.setUint8(maskAt + k, mask[k]);
    }
    prev = r.values;
  }
  return new Uint8Array(out.buffer, 0, at);
}
function decodeFx(v, at, end) {
  const count = v.getUint8(at++), records = []; let prev = null;
  for (let n = 0; n < count; n++) {
    if (at + 2 > end) return null;
    const back = v.getUint8(at++), bits = v.getUint8(at++), delta = bits & 1, reset = bits & 4 ? 2 : bits & 2 ? 1 : 0, values = new Float32Array(FX_FIELDS);
    if (delta && !prev) return null;
    let mask = null; if (delta) { if (at + MASK > end) return null; mask = new Uint8Array(v.buffer, v.byteOffset + at, MASK).slice(); at += MASK; }
    for (let i = 0; i < FX_FIELDS; i++) {
      if (mask && !(mask[i >> 3] & (1 << (i & 7)))) { values[i] = prev[i]; continue; }
      if (at + FX_SIZE[FX_TYPES[i]] > end) return null;
      values[i] = readField(v, at, FX_TYPES[i]); at += FX_SIZE[FX_TYPES[i]];
    }
    records.push({ back, values, reset }); prev = values;
  }
  return at === end ? records : null;
}

// s: {tick, remaining, finishTicks, scale[3], placements, rescues, finished, dnf, pose Float32Array(B*7),
//     pair Uint32Array(140)|null, lighting: {irradiance(40), rim, bounds(6), point(3)}|null, fx: records|null}
export function encodeState(s) {
  const bones = s.pose.length / 7, pair = s.pair && s.pair.length >= 137 ? s.pair : null;
  const spheres = pair && pair[0] ? Math.min(pair[1], 20) : 0;
  const fx = s.fx?.length ? encodeFx(s.fx) : null, fixed = stateSize(bones, spheres, !!pair, !!s.lighting);
  const out = new ArrayBuffer(fixed + (fx ? fx.length : 0)), v = new DataView(out), f = new Float32Array(out, 32, 6), u = new Uint32Array(out, 0, fixed / 4);
  const flags = (s.finished ? FLAG_FINISHED : 0) | (s.dnf ? FLAG_DNF : 0) | (pair ? FLAG_PAIR : 0) | (s.lighting ? FLAG_LIGHTING : 0) | (fx ? FLAG_FX : 0);
  v.setUint8(0, STATE); v.setUint8(1, flags); v.setUint8(2, bones); v.setUint8(3, spheres); v.setUint32(4, s.tick >>> 0, true);
  v.setFloat32(8, s.remaining ?? 0, true); v.setFloat32(12, s.finishTicks ?? -1, true);
  for (let k = 0; k < 3; k++) v.setFloat32(16 + 4 * k, s.scale[k], true);
  v.setUint16(28, (s.placements ?? s.serial ?? 0) & 0xffff, true); v.setUint16(30, (s.rescues ?? 0) & 0xffff, true);
  const pf = pair ? new Float32Array(pair.buffer, pair.byteOffset, 140) : null;
  for (let k = 0; k < 3; k++) { f[k] = pf ? pf[99 + k] : s.position?.[k] ?? 0; f[3 + k] = pf ? pf[87 + k] : s.velocity?.[k] ?? 0; }
  // The platform is little endian (every browser/node target); typed-array copies keep the exact bits.
  let w = HEADER / 4;
  u.set(new Uint32Array(s.pose.buffer, s.pose.byteOffset, s.pose.length), w); w += s.pose.length;
  if (pair) { u.set(pair.subarray(0, 7), w); w += 7; u.set(pair.subarray(7, 7 + spheres * 4), w); w += spheres * 4; u.set(pair.subarray(87, 137), w); w += PAIR_TAIL; }
  if (s.lighting) {
    const l = new Float32Array(out, w * 4, LIGHTING);
    l.set(s.lighting.irradiance, 0); l[40] = s.lighting.rim; l.set(s.lighting.bounds, 41); l.set(s.lighting.point, 47);
  }
  if (fx) new Uint8Array(out, fixed).set(fx);
  return new Uint8Array(out);
}

// frame = [sender slot, ...packet]. Returns null for anything malformed.
export function decodeFrame(frame) {
  if (!(frame instanceof Uint8Array) || frame.length < 2) return null;
  const slot = frame[0], bytes = frame.slice(1), v = new DataView(bytes.buffer), kind = bytes[0];
  if (kind === ATTACK) {
    if (bytes.length !== 24) return null;
    return { kind, slot, victim: bytes[1], attacker: bytes[2], tick: v.getUint32(4, true), direction: [v.getFloat32(8, true), v.getFloat32(12, true), v.getFloat32(16, true)], amount: v.getFloat32(20, true) };
  }
  if (kind === WORLD) {
    if (bytes.length < 8 || bytes.length % 4) return null;
    const triggers = v.getUint16(4, true), words = v.getUint16(6, true);
    if (bytes.length !== 8 + (triggers + words) * 4) return null;
    const all = new Uint32Array(bytes.buffer, 8, triggers + words);
    return { kind, slot, tick: v.getUint32(0, true) >>> 8, triggers: Array.from(all.subarray(0, triggers)), events: all.slice(triggers) };
  }
  if (kind !== STATE || bytes.length < HEADER) return null;
  const flags = bytes[1], bones = bytes[2], spheres = bytes[3], fixed = stateSize(bones, spheres, !!(flags & FLAG_PAIR), !!(flags & FLAG_LIGHTING));
  if (!bones || spheres > 20 || bytes.length < fixed || (!(flags & FLAG_FX) && bytes.length !== fixed)) return null;
  const f = new Float32Array(bytes.buffer, 32, 6), u = new Uint32Array(bytes.buffer, 0, fixed / 4);
  const placements = v.getUint16(28, true), rescues = v.getUint16(30, true);
  const s = { kind, slot, flags, finished: !!(flags & FLAG_FINISHED), dnf: !!(flags & FLAG_DNF), tick: v.getUint32(4, true),
    remaining: v.getFloat32(8, true), finishTicks: v.getFloat32(12, true), scale: [v.getFloat32(16, true), v.getFloat32(20, true), v.getFloat32(24, true)],
    placements, rescues, serial: (placements + rescues) & 0xffff, position: [f[0], f[1], f[2]], velocity: [f[3], f[4], f[5]], pose: null, pair: null, lighting: null, fx: null };
  let w = HEADER / 4;
  s.pose = new Float32Array(bytes.buffer, w * 4, bones * 7); w += bones * 7;
  if (flags & FLAG_PAIR) { // back to the full 140-word pair_view layout (unused sphere slots zero)
    const pair = new Uint32Array(140);
    pair.set(u.subarray(w, w + 7), 0); w += 7; pair.set(u.subarray(w, w + spheres * 4), 7); w += spheres * 4; pair.set(u.subarray(w, w + PAIR_TAIL), 87); w += PAIR_TAIL;
    s.pair = pair;
  }
  if (flags & FLAG_LIGHTING) {
    const l = new Float32Array(bytes.buffer, w * 4, LIGHTING);
    s.lighting = { irradiance: l.slice(0, 40), rim: l[40], bounds: l.slice(41, 47), point: l.slice(47, 50) };
  }
  if (flags & FLAG_FX) { s.fx = decodeFx(v, fixed, bytes.length); if (!s.fx) return null; }
  for (const x of [...s.pose, ...s.position, ...s.velocity, ...s.scale, s.remaining]) if (!Number.isFinite(x)) return null;
  return s;
}

export function encodeAttack({ victim, attacker, tick, direction, amount }) {
  const out = new ArrayBuffer(24), v = new DataView(out);
  v.setUint8(0, ATTACK); v.setUint8(1, victim); v.setUint8(2, attacker); v.setUint32(4, tick >>> 0, true);
  for (let k = 0; k < 3; k++) v.setFloat32(8 + 4 * k, direction[k], true);
  v.setFloat32(20, amount, true);
  return new Uint8Array(out);
}
// World packet: word 0 = kind | tick << 8, then counts and the trigger keys / event words.
export function encodeWorld({ tick, triggers = [], events = new Uint32Array(0) }) {
  const out = new ArrayBuffer(8 + (triggers.length + events.length) * 4), v = new DataView(out);
  v.setUint32(0, (WORLD | ((tick & 0xffffff) << 8)) >>> 0, true); v.setUint16(4, triggers.length, true); v.setUint16(6, events.length, true);
  const all = new Uint32Array(out, 8); all.set(triggers, 0); all.set(events, triggers.length);
  return new Uint8Array(out);
}

// The sender's side: read everything from the local rider's core after its tick.
// lightingConfig: environment.json `irradiance` (rim_scale); rescues: main.js rescue count (rider_state[13]).
// Returns null until the rider has a pose.
export function captureState(core, { tick, finished = false, dnf = false, lightingConfig = null, fx = null }) {
  if (!core?._world_pose_bones || !core._rider_skin_palette_count?.()) return null;
  const f32 = (p, n) => new Float32Array(core.HEAPF32.buffer, p, n);
  const posePtr = core._world_pose_bones(), bones = f32(posePtr, 1)[0];
  if (!bones) return null;
  const pose = f32(posePtr + 4, bones * 7).slice();
  const scale = Array.from(f32(core._rider_skin_scale(), 3));
  const pair = new Uint32Array(core.HEAPU8.buffer, core._pair_view(), 140).slice();
  const progress = f32(core._race_progress_info(), 8), reset = f32(core._reset_info(), 3), state = f32(core._rider_state(), 16);
  let lighting = null;
  if (lightingConfig && core._rider_lighting_info && f32(core._rider_lighting_info(), 1)[0] === 1 && f32(core._environment_irradiance_info(), 1)[0] === 1) {
    const info = core._environment_lighting_info ? f32(core._environment_lighting_info(), 8) : null;
    const bounds = f32(core._rider_query_bounds(), 12), physical = f32(core._pose_physical(), 12);
    lighting = { irradiance: f32(core._environment_irradiance(), 40).slice(), rim: info && info[0] ? info[3] : lightingConfig.rim_scale,
      bounds: [bounds[0], bounds[1], bounds[2], bounds[4], bounds[5], bounds[6]], point: [physical[9], physical[10], physical[11]] };
  }
  return { tick, finished, dnf, pose, scale, pair, lighting, fx, remaining: progress[0], finishTicks: progress[3] >= 0 ? progress[4] : -1,
    placements: reset[2] & 0xffff, rescues: state[13] & 0xffff };
}
// Drain the FX records of the local core (web/fx_puppet.inc fx_records) as [{back, values}], newest last,
// `back` = motion ticks before the newest.
export function drainFx(core) {
  if (!core?._fx_records) return [];
  const p = core._fx_records(), n = new Float32Array(core.HEAPF32.buffer, p, 1)[0], size = FX_FIELDS + 1;
  if (!n) return [];
  const all = new Float32Array(core.HEAPF32.buffer, p + 4, n * size).slice(), last = all[(n - 1) * size];
  return Array.from({ length: n }, (_, i) => ({ back: last - all[i * size], values: all.slice(i * size + 1, i * size + size) }));
}
