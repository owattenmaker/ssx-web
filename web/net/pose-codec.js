// Remote rider skin palette rebuilt from the streamed pose, exactly as the core builds it.
//
// The core's palette (web/animation_bridge.cpp rider_skin_palette) is a pure function of
//   * the rider's cached world pose (world_pose_bones: per bone source-cm position xyz + quaternion xyzw),
//   * the authored body scale graph.scale (rider_skin_scale),
//   * the package's source bind matrices and integer-percent source weights (rider.json),
// evaluated with the original toward-zero float arithmetic:
//   310120   originalPoseMatrices   quaternion/position -> matrix, per-column scale   (engine/pose_matrix.hpp)
//   3106CC   originalSkinPoseMatrix scaled pose x bind                                 (engine/pose_matrix.hpp)
//   386BD0   originalSkinPalette    weight x 0.01 VU accumulation per weight group     (engine/skin_palette.hpp)
// so a receiver that gets the pose floats bit for bit rebuilds the sender's palette bit for bit. The
// arithmetic below is engine/software_float.hpp (binary32 toward zero from binary64 residuals; JS numbers are
// binary64 and Math.fround rounds to nearest like float(double)).
const f32 = new Float32Array(1), u32 = new Uint32Array(f32.buffer);
function reduceMagnitude(value) { f32[0] = value; u32[0] -= 1; return f32[0]; }
function correct(rounded, residual) {
  if (rounded > 0 && residual < 0) return reduceMagnitude(rounded);
  if (rounded < 0 && residual > 0) return reduceMagnitude(rounded);
  return rounded;
}
export function add(a, b) {
  const sum = a + b, rounded = Math.fround(sum);
  if (!Number.isFinite(a) || !Number.isFinite(b)) return rounded;
  if (!Number.isFinite(rounded)) return reduceMagnitude(rounded);
  const error = Math.abs(a) >= Math.abs(b) ? b - (sum - a) : a - (sum - b);
  return correct(rounded, (sum - rounded) + error);
}
export function sub(a, b) { return add(a, -b); }
export function mul(a, b) {
  const exact = a * b, rounded = Math.fround(exact);
  if (!Number.isFinite(a) || !Number.isFinite(b)) return rounded;
  if (!Number.isFinite(rounded)) return reduceMagnitude(rounded);
  return correct(rounded, exact - rounded);
}

// engine/pose_matrix.hpp originalPoseMatrices(...).scaled, column major (out[col*4+row]).
export function poseMatrixScaled(px, py, pz, qx, qy, qz, qw, scale, out) {
  const q = [qx, qy, qz, qw], twice = [0, 0, 0], square = [0, 0, 0], weighted = [0, 0, 0];
  for (let i = 0; i < 3; i++) { twice[i] = add(q[i], q[i]); square[i] = mul(twice[i], q[i]); weighted[i] = mul(twice[i], q[3]); }
  const yz = add(mul(twice[1], q[2]), mul(0, 0)), zx = add(mul(twice[2], q[0]), mul(0, 0)), xy = add(mul(twice[0], q[1]), mul(0, 0));
  const r = [
    sub(sub(1, square[1]), mul(1, square[2])), add(add(0, xy), mul(1, weighted[2])), sub(add(0, zx), mul(1, weighted[1])), 0,
    sub(add(0, xy), mul(1, weighted[2])), sub(sub(1, square[2]), mul(1, square[0])), add(add(0, yz), mul(1, weighted[0])), 0,
    add(add(0, zx), mul(1, weighted[1])), sub(add(0, yz), mul(1, weighted[0])), sub(sub(1, square[0]), mul(1, square[1])), 0,
    px, py, pz, 1];
  const s = [scale[0], scale[1], scale[2], 1];
  for (let col = 0; col < 4; col++) for (let row = 0; row < 4; row++) out[col * 4 + row] = mul(r[col * 4 + row], s[col]);
  return out;
}
// engine/pose_matrix.hpp originalSkinPoseMatrix(pose, bind).
export function skinPoseMatrix(pose, bind, out, at = 0) {
  for (let col = 0; col < 4; col++) for (let row = 0; row < 4; row++) {
    let value = mul(pose[row], bind[col * 4]);
    for (let k = 1; k < 4; k++) value = add(value, mul(pose[k * 4 + row], bind[col * 4 + k]));
    out[at + col * 4 + row] = value;
  }
  return out;
}

// Per-package skin description: bind matrices (bit-cast words) and the distinct weight groups in first-occurrence
// order (web/animation_bridge.cpp setup_skin_bind; the same order web/rider-skinning.js uses for palette slots).
export function skinLayout(rig) {
  if (!rig.source_bind_matrix_words || !rig.source_skin) throw new Error('rider.json lacks source skin/bind rows');
  const bones = rig.source_bind_matrix_words.length;
  const bind = rig.source_bind_matrix_words.map((row) => new Float32Array(new Uint32Array(row).buffer));
  const map = new Map(), groups = [], slots = new Uint32Array(rig.source_skin.length);
  rig.source_skin.forEach((row, i) => {
    const key = JSON.stringify(row);
    if (!map.has(key)) { map.set(key, groups.length); groups.push(row.map(([bone, weight]) => ({ bone, weight: mul(weight, PERCENT) }))); }
    slots[i] = map.get(key);
  });
  return { bones, bind, groups, slots };
}
const PERCENT = new Float32Array(new Uint32Array([0x3c23d70a]).buffer)[0];

// pose: Float32Array(bones*7) = world_pose_bones layout (position xyz, quaternion xyzw); scale: [sx, sy, sz].
// out: Float32Array(groups*16), the core's rider_skin_palette.
export function buildPalette(layout, pose, scale, out = new Float32Array(layout.groups.length * 16)) {
  const { bones, bind, groups } = layout, matrices = new Float64Array(bones * 16), scaled = new Array(16);
  for (let b = 0; b < bones; b++) {
    const o = b * 7;
    poseMatrixScaled(pose[o], pose[o + 1], pose[o + 2], pose[o + 3], pose[o + 4], pose[o + 5], pose[o + 6], scale, scaled);
    skinPoseMatrix(scaled, bind[b], matrices, b * 16);
  }
  for (let g = 0; g < groups.length; g++) {
    const group = groups[g], at = g * 16;
    for (let i = 0; i < group.length; i++) {
      const { bone, weight } = group[i], m = bone * 16;
      for (let lane = 0; lane < 16; lane++) { const value = mul(matrices[m + lane], weight); out[at + lane] = i ? add(mul(out[at + lane], 1), value) : value; }
    }
  }
  return out;
}
