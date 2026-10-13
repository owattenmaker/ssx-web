// Time-attack ghost packets (docs/big-multiplayer.md 3.1; pv ghostRiders). Shared by the page (web/net/ta-game.js, web/ghost-riders.js)
// and the server (web/server/sessions.mjs); it lives in web/server/ because the server is deployed on its own. Little endian.
//
// Pose (kind 5, client -> server, 20 Hz during an attempt), 34 + 7 x F + 4 x B bytes (156 B for 27 bones and the two board bones):
//    0 u8  kind = 5
//    1 u8  flags: 1 riding an attempt, 2 finished
//    2 u16 serial: the attempt number x 256 + (reset placements + rescues) & 255 (a change is a teleport: never interpolated)
//    4 u32 the sender's server clock, ms (low 32 bits)
//    8 f32 x3 the root bone's world position (world_pose_bones bone 0, source cm)
//   20 i16 x3 velocity, cm/s / 4
//   26 u16 x3 the authored body scale x 10000 (rider_skin_scale)
//   32 u8  bones B, 33 u8 free bones F
//   34 F x (u8 bone, i16 x3 its position from the root, mm): the bones FK cannot place (freeBones: the rig's other roots, board_rootg,
//      and the board, board_childg, which moves in its parent's frame)
//   34 + 7F  B x u32 world rotations, smallest three at 10 bits (2-bit index of the largest component, 3 x 10 bits)
// Every other bone's position is its parent's plus the parent's rotation of the rig's source_translation_cm x the body scale (within
// 0.03 cm, measured: docs/big-multiplayer.md 1.3).
// Lite (kind 6, server-made, 25 B): u16 serial, f32 x3 position, u16 yaw, i8 pitch, i8 roll (the root rotation's), i16 x3 velocity / 4,
//   u8 flags. Dot (kind 7, server-made, 12 B): f32 x3 position.
// Bundle (kind 11, server -> client): u8 kind, u8 count, u32 server ms, then count x (u16 player index, u8 entry kind, u8 length,
//   the entry: a pose without its kind byte, a lite or a dot).
export const POSE = 5, LITE = 6, DOT = 7, BUNDLE = 11;
export const POSE_HEADER = 34, LITE_SIZE = 25, DOT_SIZE = 12, MAX_BONES = 40, MAX_FREE = 8;
// a bone further than this from its FK position (cm) is sent as a free bone (the feet stretch in some crashes: up to 11 cm, measured)
export const FREE_SLACK = 0.5;
// The bones a pose sends as positions: the rig's roots other than bone 0 and the board bone.
export function freeBones(rigBones) {
  const out = [];
  rigBones.forEach((b, i) => {
    if (i > 0 && (b.parent_index < 0 || b.name === 'board_childg')) out.push(i);
  });
  return out;
}
export const FLAG_RIDING = 1, FLAG_FINISHED = 2;
const Q = 511;

// q: [x, y, z, w] -> u32
export function packQuat(x, y, z, w) {
  const c = [x, y, z, w];
  let big = 0;
  for (let k = 1; k < 4; k++) if (Math.abs(c[k]) > Math.abs(c[big])) big = k;
  const sign = c[big] < 0 ? -1 : 1;
  let word = big;
  let shift = 2;
  for (let k = 0; k < 4; k++) {
    if (k === big) continue;
    const v = Math.max(-1, Math.min(1, (c[k] * sign) / Math.SQRT1_2));
    word |= (Math.round(v * Q) + Q) << shift;
    shift += 10;
  }
  return word >>> 0;
}
export function unpackQuat(word, out = new Float64Array(4), at = 0) {
  const big = word & 3;
  let shift = 2;
  let sum = 0;
  for (let k = 0; k < 4; k++) {
    if (k === big) continue;
    const v = (((word >>> shift) & 1023) - Q) / Q * Math.SQRT1_2;
    out[at + k] = v;
    sum += v * v;
    shift += 10;
  }
  out[at + big] = Math.sqrt(Math.max(0, 1 - sum));
  return out;
}

const clampI16 = (v) => Math.max(-32768, Math.min(32767, Math.round(v)));

// s: {flags, serial, time, velocity [3], scale [3], pose Float32Array(B x 7) (world_pose_bones: position, quaternion xyzw),
//     free (freeBones of the rig), rigBones (rider.json bones: any other bone FK misses by more than FREE_SLACK is sent free too)}
// -> Uint8Array
export function encodePose(s) {
  const bones = s.pose.length / 7;
  if (!Number.isInteger(bones) || bones < 1 || bones > MAX_BONES) throw new Error('ghost pose: bones');
  const free = (s.free ?? []).filter((b) => b > 0 && b < bones);
  if (s.rigBones) free.push(...stretched(s.pose, s.scale ?? [1, 1, 1], s.rigBones, new Set(free)));
  free.splice(MAX_FREE);
  const quatsAt = POSE_HEADER + 7 * free.length;
  const out = new ArrayBuffer(quatsAt + 4 * bones);
  const v = new DataView(out);
  v.setUint8(0, POSE);
  v.setUint8(1, s.flags ?? 0);
  v.setUint16(2, (s.serial ?? 0) & 0xffff, true);
  v.setUint32(4, (s.time ?? 0) >>> 0, true);
  for (let k = 0; k < 3; k++) {
    v.setFloat32(8 + 4 * k, s.pose[k], true);
    v.setInt16(20 + 2 * k, clampI16((s.velocity?.[k] ?? 0) / 4), true);
    v.setUint16(26 + 2 * k, Math.max(0, Math.min(65535, Math.round((s.scale?.[k] ?? 1) * 10000))), true);
  }
  v.setUint8(32, bones);
  v.setUint8(33, free.length);
  free.forEach((b, i) => {
    const at = POSE_HEADER + 7 * i;
    v.setUint8(at, b);
    for (let k = 0; k < 3; k++) v.setInt16(at + 1 + 2 * k, clampI16((s.pose[b * 7 + k] - s.pose[k]) * 10), true);
  });
  for (let b = 0; b < bones; b++) {
    const o = b * 7;
    v.setUint32(quatsAt + 4 * b, packQuat(s.pose[o + 3], s.pose[o + 4], s.pose[o + 5], s.pose[o + 6]), true);
  }
  return new Uint8Array(out);
}
// bytes: a pose packet, with or without its kind byte (withKind). -> {flags, serial, time, root, velocity, scale, bones, free Map(bone ->
// [x, y, z] from the root), quats Float64Array(B x 4)} or null
export function decodePose(bytes, withKind = true) {
  const off = withKind ? 0 : -1;
  if (!(bytes instanceof Uint8Array) || bytes.length < POSE_HEADER + off) return null;
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (withKind && v.getUint8(0) !== POSE) return null;
  const bones = v.getUint8(32 + off);
  const count = v.getUint8(33 + off);
  const quatsAt = POSE_HEADER + off + 7 * count;
  if (!bones || bones > MAX_BONES || count > MAX_FREE || bytes.length !== quatsAt + 4 * bones) return null;
  const root = [0, 1, 2].map((k) => v.getFloat32(8 + off + 4 * k, true));
  if (!root.every(Number.isFinite)) return null;
  const free = new Map();
  for (let i = 0; i < count; i++) {
    const at = POSE_HEADER + off + 7 * i;
    const b = v.getUint8(at);
    if (!b || b >= bones) return null;
    free.set(b, [0, 1, 2].map((k) => v.getInt16(at + 1 + 2 * k, true) / 10));
  }
  const quats = new Float64Array(bones * 4);
  for (let b = 0; b < bones; b++) unpackQuat(v.getUint32(quatsAt + 4 * b, true), quats, b * 4);
  return {
    flags: v.getUint8(1 + off),
    serial: v.getUint16(2 + off, true),
    time: v.getUint32(4 + off, true),
    root,
    velocity: [0, 1, 2].map((k) => v.getInt16(20 + off + 2 * k, true) * 4),
    scale: [0, 1, 2].map((k) => v.getUint16(26 + off + 2 * k, true) / 10000),
    bones,
    free,
    quats
  };
}
// The bones whose exact position is further than FREE_SLACK from where their parent's rotation of the rig offset puts them.
function stretched(pose, scale, rigBones, skip) {
  const out = [];
  const bones = pose.length / 7;
  for (let b = 1; b < bones && b < rigBones.length; b++) {
    const p = rigBones[b].parent_index;
    if (p < 0 || skip.has(b)) continue;
    const t = rigBones[b].source_translation_cm;
    const tx = t[0] * scale[0], ty = t[1] * scale[1], tz = t[2] * scale[2];
    const qx = pose[p * 7 + 3], qy = pose[p * 7 + 4], qz = pose[p * 7 + 5], qw = pose[p * 7 + 6];
    const cx = qy * tz - qz * ty, cy = qz * tx - qx * tz, cz = qx * ty - qy * tx;
    const x = pose[p * 7] + tx + 2 * (qw * cx + qy * cz - qz * cy);
    const y = pose[p * 7 + 1] + ty + 2 * (qw * cy + qz * cx - qx * cz);
    const z = pose[p * 7 + 2] + tz + 2 * (qw * cz + qx * cy - qy * cx);
    if (Math.hypot(x - pose[b * 7], y - pose[b * 7 + 1], z - pose[b * 7 + 2]) > FREE_SLACK) out.push(b);
  }
  return out;
}
// The server's lite entry from a pose packet (with its kind byte).
export function liteFromPose(bytes) {
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const out = new Uint8Array(LITE_SIZE);
  const o = new DataView(out.buffer);
  o.setUint16(0, v.getUint16(2, true), true);
  for (let k = 0; k < 3; k++) o.setFloat32(2 + 4 * k, v.getFloat32(8 + 4 * k, true), true);
  // the root rotation as yaw / pitch / roll (Z up)
  const q = unpackQuat(v.getUint32(POSE_HEADER + 7 * v.getUint8(33), true));
  const [x, y, z, w] = q;
  const yaw = Math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z));
  const pitch = Math.asin(Math.max(-1, Math.min(1, 2 * (w * y - z * x))));
  const roll = Math.atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y));
  o.setUint16(14, Math.round(((yaw / (2 * Math.PI)) + 1) % 1 * 65535), true);
  o.setInt8(16, Math.round((pitch / Math.PI) * 127));
  o.setInt8(17, Math.round((roll / Math.PI) * 127));
  for (let k = 0; k < 3; k++) o.setInt16(18 + 2 * k, v.getInt16(20 + 2 * k, true), true);
  o.setUint8(24, v.getUint8(1));
  return out;
}
export function decodeLite(bytes) {
  if (bytes.length !== LITE_SIZE) return null;
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return {
    serial: v.getUint16(0, true),
    root: [0, 1, 2].map((k) => v.getFloat32(2 + 4 * k, true)),
    yaw: (v.getUint16(14, true) / 65535) * 2 * Math.PI,
    pitch: (v.getInt8(16) / 127) * Math.PI,
    roll: (v.getInt8(17) / 127) * Math.PI,
    velocity: [0, 1, 2].map((k) => v.getInt16(18 + 2 * k, true) * 4),
    flags: v.getUint8(24)
  };
}
export function dotFromPose(bytes) {
  return new Uint8Array(bytes.buffer, bytes.byteOffset + 8, DOT_SIZE).slice();
}
export function decodeDot(bytes) {
  if (bytes.length !== DOT_SIZE) return null;
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return { root: [0, 1, 2].map((k) => v.getFloat32(4 * k, true)) };
}
// entries: [{player, kind, bytes}] (a pose's bytes with its kind byte; it is dropped on the wire) -> Uint8Array
export function encodeBundle(time, entries) {
  let size = 6;
  const parts = entries.map((e) => (e.kind === POSE ? e.bytes.subarray(1) : e.bytes));
  for (const p of parts) size += 4 + p.length;
  const out = new Uint8Array(size);
  const v = new DataView(out.buffer);
  v.setUint8(0, BUNDLE);
  v.setUint8(1, entries.length);
  v.setUint32(2, time >>> 0, true);
  let at = 6;
  entries.forEach((e, i) => {
    v.setUint16(at, e.player, true);
    v.setUint8(at + 2, e.kind);
    v.setUint8(at + 3, parts[i].length);
    out.set(parts[i], at + 4);
    at += 4 + parts[i].length;
  });
  return out;
}
// -> {time, entries: [{player, kind, value}]} (value decoded: a pose, a lite or a dot) or null
export function decodeBundle(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length < 6 || bytes[0] !== BUNDLE) return null;
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const count = v.getUint8(1);
  const entries = [];
  let at = 6;
  for (let i = 0; i < count; i++) {
    if (at + 4 > bytes.length) return null;
    const player = v.getUint16(at, true);
    const kind = v.getUint8(at + 2);
    const len = v.getUint8(at + 3);
    const body = bytes.subarray(at + 4, at + 4 + len);
    if (body.length !== len) return null;
    const value = kind === POSE ? decodePose(body, false) : kind === LITE ? decodeLite(body) : kind === DOT ? decodeDot(body) : null;
    if (!value) return null;
    entries.push({ player, kind, value });
    at += 4 + len;
  }
  return at === bytes.length ? { time: v.getUint32(2, true), entries } : null;
}
// Bone world positions (source cm) from a decoded pose and the rig (rider.json bones: parent_index, source_translation_cm):
// out Float64Array(B x 3). The parents come before their children in the rig's order (every package's rig, checked).
export function bonePositions(pose, rigBones, out = new Float64Array(pose.bones * 3)) {
  const q = pose.quats;
  const s = pose.scale;
  for (let b = 0; b < pose.bones; b++) {
    if (b === 0) {
      out[0] = pose.root[0];
      out[1] = pose.root[1];
      out[2] = pose.root[2];
      continue;
    }
    const f = pose.free.get(b);
    if (f) {
      for (let k = 0; k < 3; k++) out[b * 3 + k] = pose.root[k] + f[k];
      continue;
    }
    const p = rigBones[b].parent_index;
    const t = rigBones[b].source_translation_cm;
    const tx = t[0] * s[0];
    const ty = t[1] * s[1];
    const tz = t[2] * s[2];
    const qx = q[p * 4], qy = q[p * 4 + 1], qz = q[p * 4 + 2], qw = q[p * 4 + 3];
    // v + 2 w (q x v) + 2 q x (q x v)
    const cx = qy * tz - qz * ty;
    const cy = qz * tx - qx * tz;
    const cz = qx * ty - qy * tx;
    out[b * 3] = out[p * 3] + tx + 2 * (qw * cx + qy * cz - qz * cy);
    out[b * 3 + 1] = out[p * 3 + 1] + ty + 2 * (qw * cy + qz * cx - qx * cz);
    out[b * 3 + 2] = out[p * 3 + 2] + tz + 2 * (qw * cz + qx * cy - qy * cx);
  }
  return out;
}
