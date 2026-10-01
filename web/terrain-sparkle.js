// Terrain snow sparkle (sparkle; docs/presentation.md 14): the glints the PS2 scatters over the snow near the camera.
//
// Original (SLUS_207.72): the terrain pass 0x38B370 ends with 0x38D968, which runs 0x38D690 for every patch of two render
// lists: +0x4A60 (all four edges at tessellation level 8) and +0x4D84 (mixed edges, those with a level-8 edge). A patch
// sparkles when its record flag word +0xC has bit 23. The edge levels come from 0x22C410:
// table{4, 4, 6, 8}[trunc(clamp(b + k (dA^2 + dB^2), 1, 3))], with dA and dB the camera distances of the edge's two corners
// (record +0x170..+0x19C: P(0,0), P(0,1), P(1,0), P(1,1)). k and b come from 22C1B0 with r0 = 3000 and r1 = 15000 cm
// (renderer +0x33C / +0x340), so an edge is at level 8 when (dA^2 + dB^2) / 2 <= r0^2.
// Per patch: count = trunc(|bbox min - bbox max|^2 x 8 x 3.3333333e-7 x renderer+0xC4 (4.0)). One call of VU1 program 4 at
// 0x1460 then draws count sprites. The UNPACK carries the record's 16 power-basis rows, the terrain object's +0x3E0 3x3
// "twinkle" rotation and +0x420 (colour 128, (10, 0, 3, count)).
// For each sprite, a VU RNG chain seeded from the patch rows gives (u, v) and a random vector n in [-0.5, 0.5)^3.
// The sprite sits at P(u, v) + 12 cm z. alpha = trunc(min(n . s, 1) x 128) with s = T16 a.x + T17 a.y + T18 (a.z + a.w),
// where a = the clip matrix's z column; the sprite is dropped when n . s < 0. It is a screen-aligned sprite of half size
// min(10 cm projected, 3 px) in the 512 x 448 frame, FX 68 'gltr', GS ALPHA 0x49 (Cd + Cd x As), depth tested.
// The twinkle rotation (38B794..38BB..): T = Rz(-0.0045 eye.z) Ry(-0.0045 eye.x) Rx(-0.0045 eye.y) (Rodrigues about the unit
// axes, row convention new = R . old), so the glints change as the camera moves.
// Checked against ARMSX2 (local/ps2-capture/presentation/nis): sprite positions / alpha / size equal the sprites left in VU1
// memory, the lists equal the PS2's, T equals the object's rows in 7 states.
import {mul, vuAdd as add, vuSub as sub, div as eeDiv, add as eeAdd, sub as eeSub, bitsOf, fromBits} from './ee-scalar-float.js';
import {lfsrNext} from './set-piece-particle-sprites.js';
import {attribute, texture, vec4, float, uniform, positionGeometry, modelViewMatrix, cameraProjectionMatrix, uv} from 'three/tsl';
import {painterRegions} from './painter-regions.js';
import {setUpdateRange} from './heap-views.js';

export const SPARKLE_FLAG = 0x800000;
export const PATCH_WORDS = 56;                       // terrain-sparkle.bin record: 48 row floats, lo xyz, hi xyz, resource, 0
const R0 = 3000, R1 = 15000;                         // renderer +0x33C / +0x340 (every PS2 state checked)
const COUNT_SCALE = fromBits(0x34b2f4fc), COUNT_GAIN = 8;   // gp-0x26B4, gp+0x1258 (x renderer+0xC4: the Surface painter's density)
const I1 = fromBits(0x3f85a5a5);                     // program 4 LOI at 0x1520
export const TWINKLE = fromBits(0xbb9374bc);         // gp+0x1254 (-0.0045 rad / cm)

// 22C1B0: the edge-level line through (2 r0^2, 3) and (2 r1^2, 1) (EE float: sub / add / mul chop, div nearest).
const TWO_R0 = eeAdd(mul(R0, R0), mul(R0, R0)), EDGE_K = eeDiv(eeSub(1, 3), eeSub(eeAdd(mul(R1, R1), mul(R1, R1)), TWO_R0)), EDGE_B = eeSub(3, mul(EDGE_K, TWO_R0));
const dist2 = (e, c, o) => { const x = eeSub(e[0], c[o]), y = eeSub(e[1], c[o + 1]), z = eeSub(e[2], c[o + 2]); return eeAdd(eeAdd(mul(x, x), mul(y, y)), mul(z, z)); };
const level8 = (dA, dB) => eeAdd(EDGE_B, mul(EDGE_K, eeAdd(dA, dB))) >= 3;
// 0x22C410 for one patch: corners (Float32Array, 12 floats from `at`: P(0,0), P(0,1), P(1,0), P(1,1)) and the eye (source cm).
// Edges P0P1, P2P3, P0P2, P1P3; true when any is at level 8.
export function hasLevel8Edge(corners, at, eye) {
  const d0 = dist2(eye, corners, at), d1 = dist2(eye, corners, at + 3), d2 = dist2(eye, corners, at + 6), d3 = dist2(eye, corners, at + 9);
  return level8(d0, d1) || level8(d2, d3) || level8(d0, d2) || level8(d1, d3);
}
// 38D6CC..38D734: the sprite count of a patch from its record bbox (lo, hi: source cm) and the density renderer+0xC4 (the Surface
// painter's current value: 1.0 on most courses, e.g. ABC1 1.8 / 4.0 by region, CRA3 1.5 / 3.0). The count is halved for a
// mixed-list entry with +8 < 2, but the drawn mixed entries have +8 = 2 (0 is skipped; 1 was not seen in 20 PS2 states).
const countBase = (lo, hi) => {
  const x = eeSub(lo[0], hi[0]),
    y = eeSub(lo[1], hi[1]),
    z = eeSub(lo[2], hi[2]);
  return mul(eeAdd(eeAdd(mul(x, x), mul(y, y)), mul(z, z)), mul(COUNT_GAIN, COUNT_SCALE));
};
const countAt = (base, density) => Math.max(0, Math.trunc(mul(base, density)));
export function sparkleCount(lo, hi, density = 1) { return countAt(countBase(lo, hi), density); }
// The four record corners from the rows (row q = coefficient 15 - q; P(u, v) = sum c[i + 4j] u^i v^j).
export function patchCorners(rows, at = 0) {
  const c = (k, a) => rows[at + (15 - k) * 3 + a], out = new Float32Array(12);
  for (let a = 0; a < 3; a++) {
    let u1 = 0, v1 = 0, all = 0;
    for (let i = 0; i < 4; i++) { u1 += c(i, a); v1 += c(4 * i, a); for (let j = 0; j < 4; j++) all += c(i + 4 * j, a); }
    out[a] = c(0, a); out[3 + a] = v1; out[6 + a] = u1; out[9 + a] = all;
  }
  return out;
}
// Program 4 at 0x1460 without the camera: each sprite's position P(u, v) + 12 cm (source cm) and its random vector n.
// rows: Float32Array (48 floats from `at`, the VU rows 0..15 xyz). Returns Float32Array(count x 6). The RNG chain is the VU's
// chop arithmetic (the seeds are its float bits); `exact` also evaluates P(u, v) in VU arithmetic (tests), otherwise in doubles
// (within 0.01 cm, ~5x faster: 0.1 ms per patch on a desktop).
export function sparkleSprites(rows, count, at = 0, exact = false) {
  const R = (q, a) => rows[at + q * 3 + a];
  const r0 = [R(0, 0), R(0, 1), R(0, 2)], r1 = [R(1, 0), R(1, 1), R(1, 2)], r2 = [R(2, 0), R(2, 1), R(2, 2)], r3z = R(3, 2);
  const w = 1;                                                    // the rows' w (1.0 in every record)
  const vf14 = [add(r1[0], r2[0]), add(r1[1], r2[1]), add(r1[2], r2[2])];
  const vf17a = [add(r0[0], r1[0]), add(r0[1], r1[1]), add(r0[2], r1[2])];
  let n0 = add(r1[0], w), n1 = add(r2[1], r1[1]), n2 = add(r3z, w);          // vf23 = (vf09.x + vf10.w, vf10.y + vf09.y, vf11.z + vf08.w)
  const vf16 = [add(vf17a[0], I1), add(vf17a[1], I1), add(vf17a[2], I1)];
  const vf17 = [add(vf14[0], I1), add(vf14[1], I1), add(vf14[2], I1)];
  const u0z = add(vf16[2], vf16[1]); vf17[2] = add(vf17[2], vf17[1]);
  n0 = add(n0, I1); n1 = add(n1, I1); n2 = add(n2, I1);
  let uz = add(u0z, vf16[0]), vz = add(vf17[2], vf17[0]);             // vf21.z, vf22.z
  let u3 = 0, u2 = 0, v3 = 0;                                          // vf19.x (u^3), vf19.y (u^2), vf20.x (v^3)
  const out = new Float32Array(count * 6);
  const seed = (x) => (bitsOf(x) & 0x7fffff) | 0x3f800000;
  for (let k = 0; k < count; k++) {
    n2 = add(n2, u3); n0 = add(n0, u2); n1 = add(n1, vz);
    let s = add(n2, v3);                                               // vf05.z
    n0 = add(n0, n1); n1 = add(n1, n2);
    n2 = fromBits(lfsrNext(seed(s)));                                   // RINIT vf05z, RNEXT.z vf23
    const rx = lfsrNext(seed(n0)); s = add(uz, vz); n0 = fromBits(rx); // RINIT vf23x, vf05.z = vf21.z + vf22.z, RNEXT.x
    n1 = fromBits(lfsrNext(seed(n1)));                                  // RINIT vf23y, RNEXT.y
    const sv = add(vz, s);                                              // vf16.z
    const ru = fromBits(lfsrNext(seed(s)));                             // RINIT vf05z, RNEXT vf17
    const nx = sub(n0, 1.5), ny = sub(n1, 1.5), nz = sub(n2, 1.5);
    const rv = fromBits(lfsrNext(seed(sv)));                            // RINIT vf16z, RNEXT vf16
    const u = sub(ru, 1), v = sub(rv, 1);
    u2 = mul(u, u); const v2 = mul(v, v); v3 = mul(v2, v); u3 = mul(u2, u);
    n0 = nx; n1 = ny; n2 = nz; uz = u; vz = v;
    // P(u, v) = p0 v^3 + p1 v^2 + p2 v + p3, p_q = rows(4q) u^3 + rows(4q+1) u^2 + rows(4q+2) u + rows(4q+3)
    const o = k * 6;
    if (exact) for (let a = 0; a < 3; a++) {
      const p = (q) => add(add(add(mul(R(q, a), u3), mul(R(q + 1, a), u2)), mul(R(q + 2, a), u)), R(q + 3, a));
      const P = add(add(add(mul(p(0), v3), mul(p(4), v2)), mul(p(8), v)), p(12));
      out[o + a] = a === 2 ? add(P, 12) : P;
    } else for (let a = 0; a < 3; a++) {
      const b = at + a, p = (q) => ((rows[b + q * 3] * u + rows[b + q * 3 + 3]) * u + rows[b + q * 3 + 6]) * u + rows[b + q * 3 + 9];
      out[o + a] = ((p(0) * v + p(4)) * v + p(8)) * v + p(12) + (a === 2 ? 12 : 0);
    }
    out[o + 3] = nx; out[o + 4] = ny; out[o + 5] = nz;
  }
  return out;
}
// 38B794..38BAF0: the twinkle rotation of the terrain object (+0x3E0 rows) from the camera position (source cm).
export function twinkleRotation(eye) {
  const rod = (axis, t) => {
    const c = Math.cos(t), s = Math.sin(t), a = [0, 0, 0]; a[axis] = 1;
    const K = [[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]];
    return [0, 1, 2].map((i) => [0, 1, 2].map((j) => c * (i === j ? 1 : 0) + (1 - c) * a[i] * a[j] + s * K[i][j]));
  };
  const mm = (A, B) => [0, 1, 2].map((i) => [0, 1, 2].map((j) => A[i][0] * B[0][j] + A[i][1] * B[1][j] + A[i][2] * B[2][j]));
  let M = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
  for (const [axis, coord] of [[0, 1], [1, 0], [2, 2]]) M = mm(rod(axis, eye[coord] * TWINKLE), M);   // x <- eye.y, y <- eye.x, z <- eye.z
  return M;
}
// The glint vector s = T16 a.x + T17 a.y + T18 (a.z + a.w); a = the PS2 guard-band clip matrix's z column (the image of the
// source z axis: (sx right.z, sy up.z, forward.z, forward.z), sx = 0.25 x the NDC x scale, sy = 0.21875 x the NDC y scale).
export function glintVector(T, a) {
  return [0, 1, 2].map((k) => T[0][k] * a[0] + T[1][k] * a[1] + T[2][k] * (a[2] + a[3]));
}

// ---- the renderer (web/main.js, web/free-ride.js) ----
// Sets: one per terrain package (a course, or each streamed location): its terrain-sparkle.bin (tools/export_terrain_sparkle.py)
// as typed views over the one ArrayBuffer, the corners and counts derived once. Only the attached sets are kept.
export function sparkleSet(buffer, density = 1) {
  const head = new DataView(buffer, 0, 16), version = head.getUint32(4, true);
  if (String.fromCharCode(...new Uint8Array(buffer, 0, 4)) !== 'SPKL' || (version !== 1 && version !== 2)) throw Error('Not a terrain sparkle file');
  const n = head.getUint32(8, true), words = new Float32Array(buffer, 16, n * PATCH_WORDS), ids = new Uint32Array(buffer, 16, n * PATCH_WORDS);
  const corners = new Float32Array(n * 12), base = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const at = i * PATCH_WORDS; corners.set(patchCorners(words, at), i * 12);
    base[i] = countBase(words.subarray(at + 48, at + 51), words.subarray(at + 51, at + 54));
  }
  const set = {n, words, ids, corners, base, counts: new Int32Array(n), density: NaN, surface: version === 2 ? surfaceTree(buffer, head.getUint32(12, true)) : null, visible: true};
  setDensity(set, density);
  return set;
}
// The sprite counts at a density (renderer+0xC4).
export function setDensity(set, density) {
  if (set.density === density) return false;
  for (let i = 0; i < set.n; i++) set.counts[i] = countAt(set.base[i], density);
  set.density = density; return true;
}

// ---- World painter type 10, tWPIGD_Surface (factory 2C0408 case 10, ctor 2BC980, vtable 4844C8) ----
// +8 current density (renderer+0xC4 when the sparkle is drawn), +C the sample; class defaults 1.0 / 1.0. Each payload is (rate,
// density), found in the location's point tree (tools/course_painters.py point_tree) at the camera's X/Y. The driver is 2C0778
// (engine/painter_driver.hpp): the first step jumps, a rate >= 0 jumps once the camera travelled that far, else each tick
// blends with w = rate^2: current = w density + (1 - w) current (the single-float blend 2BCECC/2BD30C, EE float).
function surfaceTree(buffer, at) {
  if (!at) return null;
  const v = new DataView(buffer, at), nodeCount = v.getUint32(16, true), payloadCount = v.getUint32(28, true), nodes = [];
  for (let i = 0; i < nodeCount; i++) nodes.push([0, 2, 4, 6].map((k) => v.getUint16(32 + i * 8 + k, true)));
  const p = 32 + nodeCount * 8, payloads = [];
  for (let i = 0; i < payloadCount; i++) payloads.push([v.getFloat32(p + i * 8, true), v.getFloat32(p + i * 8 + 4, true)]);
  return {scale: v.getFloat32(0, true), origin: [v.getFloat32(4, true), v.getFloat32(8, true)], root: v.getUint32(12, true), nodes, outside: v.getUint32(24, true), payloads};
}
// The payload index at (x, y) (source cm), -1: none (the point tree walk of every world painter, as web/screen-tint.js).
export function surfacePayload(tree, x, y) {
  const f = Math.fround; let px = f(f(x - tree.origin[0]) * tree.scale), py = f(f(y - tree.origin[1]) * tree.scale), leaf = null;
  if (px > -1 && px < 32768 && py > -1 && py < 32768) {
    let a = ((Math.trunc(px) << 1) & 0xffff) >>> 0, b = ((Math.trunc(py) << 1) & 0xffff) >>> 0, index = tree.root;
    for (let steps = 0; steps <= tree.nodes.length; steps++) {
      const node = tree.nodes[index]; if (!node) throw Error('Surface painter node outside the tree');
      if (!(node[0] & 1)) { leaf = node; break; }
      index = node[((a >> 15) << 1) | (b >> 15)] >> 1; a = (a << 1) & 0xfffc; b = (b << 1) & 0xfffc;
    }
    if (!leaf) throw Error('Cyclic Surface painter tree');
  }
  const value = leaf ? ((leaf[2] | (leaf[3] << 16)) >>> 0) : tree.outside;
  return value === 0xffffffff || value >= tree.payloads.length ? -1 : value;
}
export function createSurfacePainter() {
  const f = Math.fround;
  const p = {current: 1, distance: -99999, lastX: 0, lastY: 0, selected: -1,
    reset() { p.current = 1; p.distance = 0; },   // 2C09D8: the class defaults, +0 = 0
    restart() { p.current = 1; p.distance = -99999; p.selected = -1; },
    // tree: the record's Surface section (null: none, the class defaults).
    step(tree, x, y) {
      const initial = p.distance === -99999;
      if (!initial) { const dx = eeSub(p.lastX, x), dy = eeSub(p.lastY, y); p.distance = eeAdd(p.distance, f(Math.sqrt(eeAdd(mul(dx, dx), mul(dy, dy))))); }
      p.lastX = x; p.lastY = y;
      if (!tree) { p.reset(); p.selected = -1; return p.current; }   // no record / no Surface section
      const i = p.selected = surfacePayload(tree, x, y); if (i < 0) { p.reset(); return p.current; }
      const [rate, value] = tree.payloads[i], blend = (weight) => { const w = mul(weight, weight); p.current = eeAdd(mul(w, value), mul(eeSub(1, w), p.current)); };
      if (p.current === value) p.distance = 0;
      if (initial) { blend(-1); p.distance = 0; return p.current; }
      if (rate >= 0 && rate <= p.distance) { blend(1); p.distance = 0; } else blend(-rate);
      return p.current;
    }};
  return p;
}
const MAX_HALF = [3 / 256, 3 / 224];   // 3 px of the 512 x 448 frame, in NDC
const BUILDS_PER_FRAME = 2;
// T: three.js (webgpu). origin: the scene origin (metres; scene = (x, z, -y) / 100 - origin). capacity: sprites per frame.
export async function createTerrainSparkle({T, origin, capacity = 4096, fetchBytes = (u) => fetch(u).then((r) => (r.ok ? r.arrayBuffer() : null))}) {
  const meta = (await (await fetch('/assets/FX/fx.json')).json()).textures.gltr; if (!meta) throw Error('Missing FX texture gltr (tools/export_fx_textures.py)');
  const bytes = new Uint8Array(await fetchBytes('/assets/FX/' + meta.file));
  const map = new T.DataTexture(bytes, meta.width, meta.height, T.RGBAFormat);
  map.minFilter = map.magFilter = T.LinearFilter; map.wrapS = map.wrapT = T.ClampToEdgeWrapping; map.colorSpace = T.NoColorSpace; map.needsUpdate = true;
  const sets = new Map(), cache = new Map(), group = new T.Group(); group.name = 'terrain sparkle'; group.userData.gameplayOnly = true;
  const geometry = new T.PlaneGeometry(2, 2);
  // GS ST (0,0) at centre + half
  for (let v = 0; v < geometry.attributes.uv.count; v++) geometry.attributes.uv.setXY(v, (1 - geometry.attributes.position.getX(v)) / 2, (1 + geometry.attributes.position.getY(v)) / 2);
  const centre = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage);   // xyz scene metres, w alpha / 128
  geometry.setAttribute('sparkle', centre);
  const size = uniform(new T.Vector2()), maxHalf = uniform(new T.Vector2(...MAX_HALF));
  const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: true, side: T.DoubleSide, forceSinglePass: true, fog: false, toneMapped: false});
  material.name = 'terrainSparkle';   // pipeline label renderPipeline_terrainSparkle_N (web/test-shader-budget.mjs)
  const c = attribute('sparkle', 'vec4'), clip = cameraProjectionMatrix.mul(modelViewMatrix.mul(vec4(c.xyz, 1)));
  // half extent: 10 cm in view space, at most 3 px per axis (the program's MINIz with row 21 z = 3)
  const hx = size.x.min(clip.w.mul(maxHalf.x)), hy = size.y.min(clip.w.mul(maxHalf.y));
  material.vertexNode = clip.add(vec4(positionGeometry.x.mul(hx), positionGeometry.y.mul(hy), 0, 0));
  // GS ALPHA 0x49: Cd + Cd x As (As = At x Av, 128 = 1.0); the colour of the texture is not used
  const As = texture(map, uv()).a.mul(255 / 128).mul(c.w).clamp(0, 1);
  material.fragmentNode = vec4(As, As, As, float(1));
  material.blending = T.CustomBlending; material.blendEquation = T.AddEquation; material.blendSrc = T.DstColorFactor; material.blendDst = T.OneFactor;
  material.blendSrcAlpha = T.ZeroFactor; material.blendDstAlpha = T.OneFactor; material.blendEquationAlpha = T.AddEquation;
  const mesh = new T.InstancedMesh(geometry, material, capacity); mesh.count = 1; mesh.frustumCulled = false; mesh.renderOrder = 640; mesh.visible = false; mesh.name = 'terrain sparkle';
  group.add(mesh);
  // docs/visual-parity.md 41.9: the PS2 draws the sparkle at priority 4 (0x38DA40: word2 |= 0x80), in the world layer before the fog
  // composite 36AC00, so it is fogged with the terrain. The blend (Cd + Cd x As) scales the destination bytes (the world targets hold
  // encoded bytes, pv encodedBlend).
  const eye = [0, 0, 0];   // the camera in source cm (the 22C410 / twinkle eye)
  const state = {sets: 0, patches: 0, selected: 0, sprites: 0, drawn: 0, cached: 0, density: 1, ms: 0, frames: 0, totalMs: 0, maxMs: 0, eye};   // QA: cost of update()
  const right = new T.Vector3(), up = new T.Vector3(), fwd = new T.Vector3();
  let built = 0;   // patches whose sprites were built this frame (at most BUILDS_PER_FRAME: the rest appear on the next frames)
  // A patch's sprites are a prefix of its RNG chain (the count only ends the loop): a cached chain serves any smaller count.
  function spritesOf(key, set, i) {
    const k = key + ':' + i; let s = cache.get(k);
    if (s && s.length >= set.counts[i] * 6) { cache.delete(k); cache.set(k, s); return s; }
    if (built >= BUILDS_PER_FRAME) return s ?? null;   // a longer chain is due: the shorter one until it is built
    built++; s = sparkleSprites(set.words, set.counts[i], i * PATCH_WORDS); cache.delete(k); cache.set(k, s);
    while (cache.size > 256) cache.delete(cache.keys().next().value);
    return s;
  }
  // The density renderer+0xC4: the Surface painter (createSurfacePainter) stepped once per camera tick (the core's Fog driver tick
  // and X/Y, as web/screen-tint.js; without camera ticks or in a cutscene (core null): 60 steps a second at the drawn camera, the
  // PS2's outer camera +0x20) with the record of the course, or of the core's painter region gp+0x770 in a streamed world (pv
  // regionTick, web/painter-regions.js).
  const painter = createSurfacePainter();
  let density = 1, tree = null, lastTicks = null, resets = -1, wall = 0, lastWall = 0;
  function recordTree(core, x, y) {
    const track = painterRegions.region(core);
    if (track === -2) {   // no painter region (a course, a cutscene camera): the course's record, else the streamed location under the camera
      const c = sets.get('course'); if (c) return c instanceof Promise ? null : c.surface;
      for (const set of sets.values()) if (!(set instanceof Promise) && set.surface && surfacePayload(set.surface, x, y) >= 0) return set.surface;
      return null;
    }
    if (track === -1) return tree;   // no contact yet: the painter keeps its record
    for (const set of sets.values()) if (!(set instanceof Promise) && set.track === track) return set.surface;
    return null;   // that location's record is not loaded: the class defaults
  }
  function stepDensity(core) {
    const info = core?._fog_info ? new Float32Array(core.HEAPF32.buffer, core._fog_info(), 11) : null, now = performance.now();
    if (core?._weather_info) {   // 0x2C03E8 (a rider's reset placement): every painter restarts, the next step jumps
      const w = new Float32Array(core.HEAPF32.buffer, core._weather_info(), 47)[46];
      if (resets >= 0 && w !== resets && painter.distance !== -99999) painter.distance = -99999; resets = w;
    }
    let steps = 0, x = eye[0], y = eye[1];
    if (info && info[10]) {
      const ticks = info[7]; x = info[8]; y = info[9];
      if (lastTicks !== null && ticks < lastTicks) painter.restart();
      steps = lastTicks === null || ticks < lastTicks ? 1 : Math.min(ticks - lastTicks, 8); lastTicks = ticks; wall = 0;
    } else {
      wall = lastTicks !== null || !lastWall ? 1 : Math.min(wall + (now - lastWall) * 0.06, 8); lastTicks = null;
      steps = Math.floor(wall); wall -= steps;
    }
    lastWall = now;
    if (steps) { tree = recordTree(core, x, y); for (let k = 0; k < steps; k++) painter.step(tree, x, y); }
    if (painter.current !== density) { density = painter.current; for (const set of sets.values()) if (!(set instanceof Promise)) setDensity(set, density); }
  }
  const api = {
    group,
    state,
    sets,
    // A terrain package's sparkle patches (key: 'course' or a streamed location code; root: its asset directory; track: its
    // bam.sdb location index, the painter region's key in a streamed world).
    async attach(key, root, track = null) {
      if (sets.has(key)) return sets.get(key);
      const job = fetchBytes(root + 'terrain-sparkle.bin')
        .then((b) => (b ? sparkleSet(b) : null))
        .catch(() => null);
      sets.set(key, job);
      const set = await job;
      if (sets.get(key) !== job) return null;
      if (!set) {
        sets.delete(key);
        return null;
      }
      set.track = track;
      setDensity(set, density);
      sets.set(key, set);
      return set;
    },
    detach(key) {
      sets.delete(key);
      for (const k of [...cache.keys()]) if (k.startsWith(key + ':')) cache.delete(k);
    },
    setVisible(key, on) {
      const s = sets.get(key);
      if (s && !(s instanceof Promise)) s.visible = !!on;
    },
    // Per drawn frame (after the camera is final); core: the game core (the painter's camera ticks), null in a bare scene.
    update(camera, core = null) {
      const t0 = performance.now();
      camera.updateMatrixWorld();
      const p = camera.getWorldPosition(new T.Vector3());
      eye[0] = Math.fround((p.x + origin.x) * 100);
      eye[1] = Math.fround(-(p.z + origin.z) * 100);
      eye[2] = Math.fround((p.y + origin.y) * 100);
      camera.matrixWorld.extractBasis(right, up, fwd);
      fwd.negate();
      stepDensity(core);
      const P = camera.projectionMatrix.elements,
        sx = 0.25 * P[0],
        sy = 0.21875 * P[5];
      const s = glintVector(twinkleRotation(eye), [sx * right.y, sy * up.y, fwd.y, fwd.y]);
      size.value.set(0.1 * P[0], 0.1 * P[5]);
      const arr = centre.array;
      let n = 0,
        selected = 0,
        patches = 0,
        spr = 0;
      built = 0;
      const lim = R0 * R0 * 1.001;
      for (const [key, set] of sets) {
        if (set instanceof Promise || !set.visible) continue;
        patches += set.n;
        const C = set.corners;
        for (let i = 0; i < set.n; i++) {
          const o = i * 12;
          let near = false;
          for (let q = 0; q < 12 && !near; q += 3) {
            const dx = eye[0] - C[o + q],
              dy = eye[1] - C[o + q + 1],
              dz = eye[2] - C[o + q + 2];
            if (dx * dx + dy * dy + dz * dz <= lim) near = true;
          }
          if (!near || set.counts[i] <= 0 || !hasLevel8Edge(C, o, eye)) continue;
          selected++;
          const S = spritesOf(key, set, i);
          if (!S) continue;
          for (let k = 0, end = Math.min(S.length, set.counts[i] * 6); k < end; k += 6) {
            spr++;
            const dot = S[k + 3] * s[0] + S[k + 4] * s[1] + S[k + 5] * s[2];
            if (dot < 0 || n >= capacity) continue;
            const a = Math.trunc(Math.min(dot, 1) * 128);
            if (a <= 0) continue;
            const j = n * 4;
            arr[j] = S[k] / 100 - origin.x;
            arr[j + 1] = S[k + 2] / 100 - origin.y;
            arr[j + 2] = -S[k + 1] / 100 - origin.z;
            arr[j + 3] = a / 128;
            n++;
          }
        }
      }
      if (n) setUpdateRange(centre, 0, n * 4);
      mesh.count = Math.max(n, 1);
      mesh.visible = n > 0;
      const ms = performance.now() - t0;
      Object.assign(state, {
        sets: sets.size,
        patches,
        selected,
        sprites: spr,
        drawn: n,
        cached: cache.size,
        density,
        ms,
        frames: state.frames + 1,
        totalMs: state.totalMs + ms,
        maxMs: Math.max(state.maxMs, ms)
      });
    },
    dispose() {
      sets.clear();
      cache.clear();
      geometry.dispose();
      material.dispose();
      map.dispose();
      group.removeFromParent();
      if (globalThis.ssxEffects?.sparkle === api) delete globalThis.ssxEffects.sparkle;
    }
  };
  (globalThis.ssxEffects ??= {}).sparkle = api;   // QA handle
  return api;
}
