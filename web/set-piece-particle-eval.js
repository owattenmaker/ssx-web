// Sprite evaluation of the set-piece particle effects the core exports (web/stage_world.inc
// stage_world_particles): the structure of the original VU1 program 0x439A40 (entries 0x000 burst / 0x510 trail;
// web/set-piece-particle-sprites.js is the bit-exact model) with native floats, for drawing. No three.js here, so
// node tests and tools can use it.
export const SPRITE_FLOATS = 8;             // x, y, z (source cm), half size (cm), r, g, b, a (GS integers, 128 = 1)
export const MAX_HALF_PIXELS = 64;          // MINI 64 at VU1 0x3A8 / 0x890
export const SOURCE_VIEWPORT = [512, 448];
import {mul as eeMul, vuAdd, vuSub, fromBits, bitsOf} from './ee-scalar-float.js';
// VU DIV (round toward zero), as set-piece-particle-sprites.js vuDiv.
const vuDivF = (a, b) => { const q = a / b; let r = Math.fround(q); if (!Number.isFinite(q) || r === 0) return r; if (Math.abs(r * b) > Math.abs(a)) r = fromBits(bitsOf(r) - 1); return r; };
// The per-particle accumulators (age, time, fraction, lifetime) use the VU/EE single-precision operations so the
// particle count (age < life, age >= 0 tests) equals the VU1 program's; the per-sprite arithmetic stays native.
const T_MAX = 2.700000047683716, POLY_A = -0.7300000190734863, POLY_B = 0.11299999803304672;

// Shape of this code (Safari): the kernels run every frame, so JavaScriptCore's top tier (FTL) compiles them mid-race.
// Written the obvious way (LFSR steps, EE helpers and constant-index parameter loads inlined in one loop nest), its
// integer range optimization took 1-2 s per kernel on a compiler thread, and any garbage collection in that window
// waited for it with the main thread stopped (the 300-800 ms mid-race freezes). Hence: the LFSR streams and the VU
// accumulators run in small separate loops into scratch arrays, the parameter words are read once, and the trail's
// ring walk and row evaluation are separate functions; every kernel now compiles in < 30 ms. Same values, same order.

// RINIT/RNEXT (23-bit LFSR): the next state word; as a float32 the state is a draw in [1, 2).
// The burst's nine streams (words 14..18, 20..23), each stepped once per slot: slot s at [9s, 9s + 9) in the order
// 38, 3C, 40, 44, 48, 50, 54, 58, 5C (the float view of the state words).
let burstBits = new Uint32Array(64 * 9), burstDraws = new Float32Array(burstBits.buffer);
function burstRandoms(K, slots) {
  if (slots * 9 > burstBits.length) { burstBits = new Uint32Array(slots * 9); burstDraws = new Float32Array(burstBits.buffer); }
  const B = burstBits;
  for (let k = 0; k < 9; k++) {
    let s = K[k < 5 ? 14 + k : 15 + k];
    for (let slot = 0, q = k; slot < slots; slot++, q += 9) { const t = ((s & 0x7fffff) | 0x3f800000) >>> 0; s = ((((t << 1) ^ ((t >>> 4) & 1) ^ ((t >>> 22) & 1)) & 0x7fffff) | 0x3f800000) >>> 0; B[q] = s; }
  }
  return burstDraws;
}

// Entry 0x000 (380518): the burst kernel (84 words K, float view F of the same words). Writes sprites to out
// from index n; returns the new count. Copies whose centre leaves the guard band are drawn (the GPU clips them).
export function burstSpritesFast(K, F, out, n, limit) {
  const slots = K[0] | 0, blur = K[1] | 0;
  const sizeRange = F[4], lifeRange = F[5], sizeBase = F[6], lifeBase = F[7], sizeDelta = F[8], alphaStep = F[12], blurStep = F[13];
  let age = F[2]; const period = F[3];
  const fx = F[24], fy = F[25], fz = F[26], vbx = F[28], vby = F[29], vbz = F[30];
  const F32 = F[32], F33 = F[33], F34 = F[34], F36 = F[36], F37 = F[37], F38 = F[38], F40 = F[40], F41 = F[41], F42 = F[42], F44 = F[44], F45 = F[45], F46 = F[46], F48 = F[48], F49 = F[49], F50 = F[50], F52 = F[52], F53 = F[53], F54 = F[54], F56 = F[56], F57 = F[57], F58 = F[58], F60 = F[60], F61 = F[61], F62 = F[62], F63 = F[63], F64 = F[64], F65 = F[65], F66 = F[66], F67 = F[67], F68 = F[68], F69 = F[69], F70 = F[70], F71 = F[71], F72 = F[72], F73 = F[73], F74 = F[74], F75 = F[75];
  const R = burstRandoms(K, slots);
  let slotIndex = 0;
  for (let slot = 0, q = 0; slot < slots; slot++, q += 9) {
    const r38 = R[q], r3C = R[q + 1], r40 = R[q + 2], r44 = R[q + 3], r48 = R[q + 4], r50 = R[q + 5], r54 = R[q + 6], r58 = R[q + 7], r5C = R[q + 8];
    const life = vuAdd(lifeBase, eeMul(lifeRange, r5C));
    if (age < life) {
      const ca0 = F60 + F64 * r50 + F72 * age + F68 * r54, ca1 = F61 + F65 * r50 + F73 * age + F69 * r54;
      const ca2 = F62 + F66 * r50 + F74 * age + F70 * r54, ca3 = F63 + F67 * r50 + F75 * age + F71 * r54;
      const size = Math.abs(sizeDelta * age * vuDivF(1, life) + sizeBase + sizeRange * r58);
      const px = F44 + F56 * slotIndex + F48 * r38 + F52 * r3C, py = F45 + F57 * slotIndex + F49 * r38 + F53 * r3C, pz = F46 + F58 * slotIndex + F50 * r38 + F54 * r3C;
      const vx = vbx + F32 * r40 + F36 * r44 + F40 * r48, vy = vby + F33 * r40 + F37 * r44 + F41 * r48, vz = vbz + F34 * r40 + F38 * r44 + F42 * r48;
      const r = Math.trunc(Math.min(Math.max(ca0, 0), 255)), g = Math.trunc(Math.min(Math.max(ca1, 0), 255)), b = Math.trunc(Math.min(Math.max(ca2, 0), 255));
      let fade = 1, t = age;
      for (let count = blur; ;) {
        const t2 = Math.min(t, T_MAX), poly = t2 * POLY_A + t2 * t2 * POLY_B;
        if (n < limit) {
          const o = n * SPRITE_FLOATS;
          out[o] = fx * t + px + (fx - vx) * poly; out[o + 1] = fy * t + py + (fy - vy) * poly; out[o + 2] = fz * t + pz + (fz - vz) * poly;
          out[o + 3] = size; out[o + 4] = r; out[o + 5] = g; out[o + 6] = b; out[o + 7] = Math.trunc(ca3 * fade) & 255; n++;
        }
        fade -= alphaStep; t = vuSub(t, blurStep);
        if (t < 0) break;
        if (--count <= 0) break;
      }
    }
    age = vuSub(age, period); slotIndex += 1;
    if (age < 0) break;
  }
  return n;
}

// A trail row: its LFSR stream (seeded by the row's ring word), 9 draws per birth, and the VU accumulators. Per birth
// i, 12 values at [12i, 12i + 12): draws 0..7, age, interpolation fraction, lifetime (draw 8) and its VU reciprocal.
// The row's age after its births goes to trailRowSchedule.end.
let trailBits = new Uint32Array(64 * 9), trailDraws = new Float32Array(trailBits.buffer);
const trailRowSchedule = {values: new Float64Array(64 * 12), end: 0};
function trailSchedule(bits, per, age, ageStep, fractionStep, lifeBase, lifeRange) {
  if (per * 9 > trailBits.length) { trailBits = new Uint32Array(per * 9); trailDraws = new Float32Array(trailBits.buffer); }
  if (per * 12 > trailRowSchedule.values.length) trailRowSchedule.values = new Float64Array(per * 12);
  const B = trailBits, R = trailDraws, V = trailRowSchedule.values;
  for (let q = 0, n = per * 9; q < n; q++) { const t = ((bits & 0x7fffff) | 0x3f800000) >>> 0; bits = ((((t << 1) ^ ((t >>> 4) & 1) ^ ((t >>> 22) & 1)) & 0x7fffff) | 0x3f800000) >>> 0; B[q] = bits; }
  let fraction = 0;
  for (let i = 0; i < per; i++) {
    const o = i * 12, q = i * 9, life = vuAdd(lifeBase, eeMul(lifeRange, R[q + 8]));
    for (let k = 0; k < 8; k++) V[o + k] = R[q + k];
    V[o + 8] = age; V[o + 9] = fraction; V[o + 10] = life; V[o + 11] = vuDivF(1, life);
    age = vuAdd(age, ageStep); fraction = vuAdd(fraction, fractionStep);
  }
  trailRowSchedule.end = age;
  return V;
}
// A trail particle's motion-blur copies (alpha taken before the fade step).
function trailCopies(out, n, limit, blur, age, alphaStep, blurStep, fx, fy, fz, p0x, p0y, p0z, fvx, fvy, fvz, size, cr, cg, cb, ca3) {
  let fade = 1, t = age;
  for (let count = blur; ;) {
    const t2 = Math.min(t, T_MAX), poly = t2 * POLY_A + t2 * t2 * POLY_B;
    const alpha = Math.trunc(ca3 * fade) & 255; fade -= alphaStep;
    if (n < limit) {
      const o = n * SPRITE_FLOATS;
      out[o] = fx * t + p0x + fvx * poly; out[o + 1] = fy * t + p0y + fvy * poly; out[o + 2] = fz * t + p0z + fvz * poly;
      out[o + 3] = size; out[o + 4] = cr; out[o + 5] = cg; out[o + 6] = cb; out[o + 7] = alpha; n++;
    }
    t = vuSub(t, blurStep);
    if (t < 0) break;
    if (--count <= 0) break;
  }
  return n;
}
// One ring row of the trail kernel: its per births from age on (the row's age afterwards: trailRowSchedule.end).
function trailRow(F, blur, out, n, limit, bits, per, age0, px, py, pz, vx, vy, vz, nx, ny, nz, nvx, nvy, nvz, fractionStep) {
  const ageStep = F[3], alphaStep = F[12], blurStep = F[13], sizeRange = F[4], lifeRange = F[5], sizeBase = F[6], lifeBase = F[7], sizeDelta = F[8];
  const F24 = F[24], F25 = F[25], F26 = F[26], F28 = F[28], F29 = F[29], F30 = F[30], F32 = F[32], F33 = F[33], F34 = F[34], F36 = F[36], F37 = F[37], F38 = F[38], F40 = F[40], F41 = F[41], F42 = F[42], F44 = F[44], F45 = F[45], F46 = F[46], F48 = F[48], F49 = F[49], F50 = F[50], F52 = F[52], F53 = F[53], F54 = F[54], F56 = F[56], F57 = F[57], F58 = F[58], F60 = F[60], F61 = F[61], F62 = F[62], F63 = F[63], F64 = F[64], F65 = F[65], F66 = F[66], F67 = F[67], F68 = F[68], F69 = F[69], F70 = F[70], F71 = F[71], F72 = F[72], F73 = F[73], F74 = F[74], F75 = F[75];
  const V = trailSchedule(bits, per, age0, ageStep, fractionStep, lifeBase, lifeRange);
  for (let o = 0, end = per * 12; o < end; o += 12) {
    const r0 = V[o], r1 = V[o + 1], r2 = V[o + 2], r3 = V[o + 3], r4 = V[o + 4], r5 = V[o + 5], r6 = V[o + 6], r7 = V[o + 7], age = V[o + 8], fraction = V[o + 9], lifetime = V[o + 10], inverse = V[o + 11];
    const c = 1 - fraction;
    const bvx = vx * c + nvx * fraction, bvy = vy * c + nvy * fraction, bvz = vz * c + nvz * fraction;
    const bpx = px * c + nx * fraction, bpy = py * c + ny * fraction, bpz = pz * c + nz * fraction;
    const p0x = bpx + F44 + F56 * age + F48 * r0 + F52 * r1, p0y = bpy + F45 + F57 * age + F49 * r0 + F53 * r1, p0z = bpz + F46 + F58 * age + F50 * r0 + F54 * r1;
    if (age < lifetime) {
      const wx = bvx + F28 + F32 * r2 + F36 * r3 + F40 * r4, wy = bvy + F29 + F33 * r2 + F37 * r3 + F41 * r4, wz = bvz + F30 + F34 * r2 + F38 * r3 + F42 * r4;
      const ca3 = F63 + F67 * r5 + F75 * age + F71 * r6;
      const cr = Math.trunc(Math.min(Math.max(F60 + F64 * r5 + F72 * age + F68 * r6, 0), 255)), cg = Math.trunc(Math.min(Math.max(F61 + F65 * r5 + F73 * age + F69 * r6, 0), 255)), cb = Math.trunc(Math.min(Math.max(F62 + F66 * r5 + F74 * age + F70 * r6, 0), 255));
      const fvx = F24 - wx, fvy = F25 - wy, fvz = F26 - wz;
      const size = Math.abs(sizeDelta * age * inverse + sizeBase + sizeRange * r7);
      n = trailCopies(out, n, limit, blur, age, alphaStep, blurStep, F24, F25, F26, p0x, p0y, p0z, fvx, fvy, fvz, size, cr, cg, cb, ca3);
    }
  }
  return n;
}

// Entry 0x510 (3807A0): the trail birth ring (ring A positions / ring B velocities with the seed in w).
export function trailSpritesFast(K, F, capacity, cursor, ringA, ringB, ringBits, out, n, limit) {
  const cap = capacity | 0; if (cap <= 0) return n;
  const per = Math.trunc((K[0] | 0) / cap); if (per <= 0) return n;
  const ageStep = F[3], blur = K[1] | 0;
  const fractionStep = vuDivF(1, per);
  const batches = (cap >> 6) + 1; let done = 0;
  for (let batch = 0; batch < batches; batch++) {
    let rows64 = cap - done; if (rows64 >= 65) rows64 = 64;
    let age = eeMul(eeMul(per, batch << 6), ageStep);
    const first = (done + cursor + 1) % cap; done += rows64;
    const neighbour = done < cap ? (done + cursor + 1) % cap : (done - 1 + cursor + 1) % cap;
    for (let j = 0; j <= rows64; j++) {
      const slot = j < rows64 ? (first + j) % cap : neighbour;
      const extra = j === rows64;
      const nextSlot = !extra ? (j + 1 < rows64 ? (first + j + 1) % cap : neighbour) : slot;
      const seedBitsStart = ringBits[slot * 4 + 3], seed = ringB[slot * 4 + 3];
      if (vuSub(seed, 1) < 0) { age = vuAdd(age, eeMul(ageStep, per)); continue; }
      const px = ringA[slot * 4], py = ringA[slot * 4 + 1], pz = ringA[slot * 4 + 2];
      const vx = ringB[slot * 4], vy = ringB[slot * 4 + 1], vz = ringB[slot * 4 + 2];
      let nx, ny, nz; if (!extra) { nx = ringA[nextSlot * 4]; ny = ringA[nextSlot * 4 + 1]; nz = ringA[nextSlot * 4 + 2]; } else if (rows64 === 64) { const s0 = (first) % cap; nx = ringB[s0 * 4]; ny = ringB[s0 * 4 + 1]; nz = ringB[s0 * 4 + 2]; } else { nx = px; ny = py; nz = pz; }
      const nvx = !extra ? ringB[nextSlot * 4] : vx, nvy = !extra ? ringB[nextSlot * 4 + 1] : vy, nvz = !extra ? ringB[nextSlot * 4 + 2] : vz;
      n = trailRow(F, blur, out, n, limit, seedBitsStart >>> 0, per, age, px, py, pz, vx, vy, vz, nx, ny, nz, nvx, nvy, nvz, fractionStep); age = trailRowSchedule.end;
    }
  }
  return n;
}

// Parse the core export (web/stage_world.inc stage_world_particles): effects with their word views.
// pool (optional, the renderer's; docs/web-render-performance.md "Per-frame garbage"): {effects: [], U, F} kept across frames. The
// returned array and its records are the pool's, rewritten by the next call; a record's views are made again only when the heap
// buffer, its word offset or its ring capacity changed (the views read the heap live, so the same views see this frame's words).
export function readParticleEffects(core, pool = null) {
  if (!core._stage_world_particles) return [];
  const p = core._stage_world_particles() >> 2, buffer = core.HEAPU8.buffer;
  if (pool && pool.buffer !== buffer) { pool.buffer = buffer; pool.U = new Uint32Array(buffer); pool.F = new Float32Array(buffer); pool.effects.length = 0; }
  const U = pool ? pool.U : new Uint32Array(buffer), Fv = pool ? pool.F : new Float32Array(buffer), n = U[p]; let at = p + 1;
  const effects = pool ? pool.effects : [];
  for (let k = 0; k < n; k++) {
    const kind = U[at], textureId = U[at + 1], blend = U[at + 2], resource = U[at + 3]; at += 4;
    let e = pool ? effects[k] : undefined;
    if (e === undefined || e.at !== at) { e = {kind, textureId, blend, resource, K: U.subarray(at, at + 84), F: Fv.subarray(at, at + 84), at, ringAt: -1}; if (pool) effects[k] = e; }
    else { e.kind = kind; e.textureId = textureId; e.blend = blend; e.resource = resource; }
    at += 84;
    if (kind === 1) {
      const capacity = U[at]; e.cursor = U[at + 1]; at += 2;
      if (e.ringAt !== at || e.capacity !== capacity) {
        e.capacity = capacity; e.ringAt = at;
        e.ringA = Fv.subarray(at, at + 4 * capacity); e.ringB = Fv.subarray(at + 4 * capacity, at + 8 * capacity); e.ringBits = U.subarray(at + 4 * capacity, at + 8 * capacity);
      }
      at += 8 * capacity;
    } else if (e.capacity !== undefined) { delete e.capacity; delete e.cursor; delete e.ringA; delete e.ringB; delete e.ringBits; e.ringAt = -1; }
    if (!pool) effects.push(e);
  }
  if (pool) effects.length = n;
  return effects;
}


// Every (texture, blend, kind) combination a course can draw: its stage programs' parameter blocks (flip
// sequences included) and the effects alive at race tick 0.
export function particleCombinations(doc) {
  const out = new Map();
  const add = (texture, blend, kind) => { const key = `${texture}|${blend}|${kind}`; if (!out.has(key)) out.set(key, {texture, blend, kind}); };
  for (const b of doc.blocks) { const w = b.words, frames = Math.max(1, w[52]); for (let f = 0; f < frames; f++) add(w[49] + f, w[50], b.builtin === 26 ? 1 : 0); }
  for (const e of doc.initial.effects) if (e.kind === 'Particle') { const frames = Math.max(1, e.emitter[0x180 / 4]); for (let f = 0; f < frames; f++) add(e.emitter[1] + f, e.emitter[2], 0); }
  for (const g of doc.multi || []) { const frames = Math.max(1, g.emitter[0x180 / 4]); for (let f = 0; f < frames; f++) add(g.emitter[1] + f, g.emitter[2], 0); } // MultiParticle groups (roadflares)
  return [...out.values()];
}
