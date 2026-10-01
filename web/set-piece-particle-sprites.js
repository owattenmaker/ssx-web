// Set-piece particle sprites: the original draw of the course particle systems.
// Pure evaluator (no three.js): emitter state in, GS sprite list out.
//
// Two original paths (SLUS_207.72, particle VU1 program = 5th .vutext MPG at 0x439A40):
//
//  burst  - builtin16 (0x2FD420) type-13 emitter entity 0x48EE60 + Particle modifier
//           0x4912B0. System at modifier+0x50 (class init 0x3705E0/0x370018, update
//           0x370788), 0x150-byte kernel at system+0x10 (36CBF8/36CE00/36CE28/370058/
//           36D318/36CCB8). Draw: modifier slot +0x30 = 345BC8 (skips when +0x1E4
//           finished) -> 3708C0 (skips when system+0xC == 0 or the 37DE88 AABB test of
//           system+0x160/+0x170 is outside) -> renderer slot 0x290 = 380518: UNPACK the
//           21-qw kernel to TOPS, MSCAL 0x000. Startfire / midfire / fire gushes / EZ pops /
//           rocket bursts / ambient course emitters (snowwind, steam, fog).
//  trail  - builtin26 (0x2FEE98 -> 0x355D10) DynamicParticle modifier 0x491268 on a moving
//           carrier. Emitter = modifier+0x60, the 0x210-byte birth-ring class shared with
//           the rider snow (births 3710D0). Draw: modifier slot +0x30 = 346070 -> 371380(e, 7)
//           -> renderer slot 0x298 = 3807A0: per batch of 64 birth groups UNPACK 152 qw
//           (kernel e+0x20, header, 65 position + 65 velocity records from cursor+1),
//           MSCAL 0x510. Rocket smoke, spintwin / dragon fire, snow crumbs, EZ rocket core.
//           (The rider snow uses 371688 -> slot 0x2A0 = 380CE0, entry A00/F10, 43-group
//           batches, with per-birth colour tint: that is engine/snow_particles.cpp, not this.)
//
// Coordinates: original source centimetres, Z up (render (x, z, -y) / 100).
// Colours: GS integers, RGB clamped 0..255 by the program (128 = texture x1),
// alpha = trunc(colourA * fade) as the program's ftoi0 (the GS takes its low 8 bits).
// Sprites are screen-aligned rectangles, half extent `halfSizeCm` in view space, capped by
// the program at 64 source-framebuffer pixels per axis (MINI 64 at 0x3A8 / 0x890).
import {mul, vuAdd as add, vuSub as sub, bitsOf, fromBits} from './ee-scalar-float.js';

export const SET_PIECE_PARTICLE_DRAW = Object.freeze({
  maxProjectedHalfPixels: 64,
  sourceViewport: [512, 448],
  // BlendMode -> table 0x44B420 -> setter table 0x491FB0 -> GS ALPHA_1.
  blend: Object.freeze({
    0: Object.freeze({enum: 7, setter: '0x3624F4', gsAlpha: 0x48, equation: 'Cd + (Cs*As >> 7)', additive: true}),
    1: Object.freeze({enum: 5, setter: '0x3624DC', gsAlpha: 0x44, equation: '((Cs - Cd)*As >> 7) + Cd', additive: false}),
    2: Object.freeze({enum: 9, setter: '0x36250C', gsAlpha: 0x42, equation: 'Cd - (Cs*As >> 7)', additive: false}),
  }),
  // Material words (renderer+0xE84 stack): TFX MODULATE/TCC1 bilinear (descriptor TEX1 0x61),
  // ATE with ATST ALWAYS, ZMSK=1 (no depth write), depth tested, word2 priority 7 = layer 1:
  // after the 36AC00 fog composite (unfogged), before 36C790 glare and ScreenTint 3904A0.
  burst: Object.freeze({entry: 0x000, rendererSlot: 0x290, draw: '0x380518', ztst: 'GREATER', stq: 'ST (0,0) at centre-half, (1,1) at centre+half'}),
  trail: Object.freeze({entry: 0x510, rendererSlot: 0x298, draw: '0x3807A0', ztst: 'GEQUAL', stq: 'ST (0,0) at centre+half, (1,1) at centre-half (texture turned 180 degrees)'}),
});

const T_MAX = fromBits(0x402ccccd), POLY_A = fromBits(0xbf3ae148), POLY_B = fromBits(0x3de76c8b);
// VU DIV rounds toward zero (FE_TOWARDZERO in the VU interpreter; terrain_original::div).
export function vuDiv(a, b) {
  const q = a / b; let r = Math.fround(q);
  if (!Number.isFinite(q) || r === 0) return r;
  const p = r * b;                                  // exact (24 x 24 bits)
  if (Math.abs(p) > Math.abs(a)) r = fromBits(bitsOf(r) - 1);
  return r;
}
// RINIT/RNEXT: 23-bit LFSR, result in [1, 2).
export function lfsrNext(bits) {
  const s = ((bits & 0x7fffff) | 0x3f800000) >>> 0;
  return ((((s << 1) ^ ((s >>> 4) & 1) ^ ((s >>> 22) & 1)) & 0x7fffff) | 0x3f800000) >>> 0;
}
const ftoi0 = (x) => Math.max(-2147483648, Math.min(2147483647, Math.trunc(x)));
const ftoi4 = (x) => ftoi0(x * 16);
const v4 = (words, row) => [0, 1, 2, 3].map((i) => fromBits(words[row * 4 + i]));
const addV = (a, b) => a.map((x, i) => add(x, b[i]));
const subV = (a, b) => a.map((x, i) => sub(x, b[i]));
const scaleV = (a, s) => a.map((x) => mul(x, s));
const maddV = (acc, a, s) => acc.map((x, i) => add(x, mul(a[i], s)));
const f32 = (x) => fromBits(bitsOf(x));

// Kernel words (84 u32 = the 0x150 bytes the renderer uploads). Field map for readers:
export const KERNEL_ROWS = Object.freeze({
  header: 0,      // burst: {slots int, blur int, age, period}   trail: {count int, blur int, 1.5, ageStep}
  sizeLife: 1,    // {sizeRange, lifeRange, sizeBase, lifeBase} (scaled time)
  sizeDelta: 2,   // x = SizeFinal - Size
  blur: 3,        // {alphaStep = 1/NumBlur, blurStep = BlurStep*Damp, burst seed38, seed3C}
  seedsA: 4,      // burst: seeds 40, 44, 48, w = Damp
  seedsB: 5,      // burst: seeds 50, 54, 58, 5C
  force: 6, velocityBase: 7, velocityRange0: 8, velocityRange1: 9, velocityRange2: 10,
  base: 11,       // burst: spawn box corner (point, w 1)   trail: position base
  axisA: 12, axisB: 13,
  drift: 14,      // burst: base drift per slot   trail: position term * scaled age
  colourBase: 15, colourRange0: 16, colourRange1: 17, colourSlope: 18, // (R, G, B, A) x128
  st0: 19, st1: 20,
});

// Optional original view state `vu`: VU1 rows 0..6 as uploaded by 364CD0 (program 4 case,
// context-0 set of the render list view block list+0x69CD0+view*0x180): rows 0..3 = +0x80..+0xB0
// world->guard-band clip matrix (clip = x*r0 + y*r1 + z*r2 + r3), row 4 = +0xC0 viewport scale
// (1024, -1024, z*0.0625, 0), row 5 = +0xD0 offset (2047.5, 2047.5, z*0.0625, 0), row 6 =
// (+0x40, +0x54, 0, 0) = (P00, P11) size scale. Give {bits: 7x4 u32} or {rows: 7x4 floats}.
function viewRows(vu) {
  if (!vu) return null;
  if (vu.bits) return vu.bits.map((r) => r.map((x) => fromBits(x >>> 0)));
  return vu.rows.map((r) => r.map(f32));
}
const transform = (m, v) => {            // mulax/madday/maddaz/maddw chain
  let acc = scaleV(m[0], v[0]); acc = maddV(acc, m[1], v[1]); acc = maddV(acc, m[2], v[2]); return maddV(acc, m[3], v[3]);
};
// VU CLIP against |w|: rejected when any |x|,|y|,|z| exceeds |w|.
const clipped = (p) => { const w = Math.abs(p[3]); return p[0] > w || p[0] < -w || p[1] > w || p[1] < -w || p[2] > w || p[2] < -w; };
function project(rows, clip, sizeNumerators) {
  const q = vuDiv(1, clip[3]);
  let half = sizeNumerators.map((x) => Math.abs(mul(x, q)));
  half = [Math.min(half[0], 64), Math.min(half[1], 64), half[2], half[3]];
  const ndc = [mul(clip[0], q), mul(clip[1], q), mul(clip[2], q), mul(clip[3], q)];
  const screen = rows[5].map((o, i) => add(o, mul(ndc[i], rows[4][i])));
  return {lo: subV(screen, half).map(ftoi4), hi: addV(screen, half).map(ftoi4), halfPixels: [half[0], half[1]]};
}

// Kernel as the 84-word array; accepts Uint32Array/array of u32, or {words}.
const wordsOf = (k) => (k.words ? k.words : k);

// Entry 0x000 (380518): burst kernel. options: {vu, clip(positionCm) -> truthy when the copy is
// rejected (VU CLIP of the centre against the guard band)}. Returns sprites in draw order.
export function burstSprites(kernel, options = {}) {
  const K = wordsOf(kernel), rows = viewRows(options.vu), out = [];
  const slots = K[0] | 0, blur = K[1] | 0;
  const sizeLife = v4(K, 1), sizeDelta = fromBits(K[8]), alphaStep = fromBits(K[12]), blurStep = fromBits(K[13]);
  let age = fromBits(K[2]); const period = fromBits(K[3]);
  const seeds = {38: K[14], '3C': K[15], 40: K[16], 44: K[17], 48: K[18], 50: K[20], 54: K[21], 58: K[22], '5C': K[23]};
  const next = (k) => (seeds[k] = lfsrNext(seeds[k]), fromBits(seeds[k]));
  const world = {force: v4(K, 6), vBase: v4(K, 7), v0: v4(K, 8), v1: v4(K, 9), v2: v4(K, 10), base: v4(K, 11), a: v4(K, 12), b: v4(K, 13), drift: v4(K, 14)};
  const clipSpace = rows && Object.fromEntries(Object.entries(world).map(([k, v]) => [k, transform(rows, v)]));
  const colourBase = v4(K, 15), colourR0 = v4(K, 16), colourR1 = v4(K, 17), colourSlope = v4(K, 18);
  const [sizeRange, lifeRange, sizeBase, lifeBase] = sizeLife;
  let slotIndex = 0;
  for (let slot = 0; slot < slots; slot++) {
    const s5C = next('5C'), s40 = next(40), s50 = next(50), s44 = next(44), s48 = next(48), s38 = next(38), s3C = next('3C');
    const life = add(lifeBase, mul(lifeRange, s5C));
    const origin = (w) => maddV(maddV(maddV(w.base, w.drift, slotIndex), w.a, s38), w.b, s3C);
    const velocity = (w) => maddV(maddV(maddV(w.vBase, w.v0, s40), w.v1, s44), w.v2, s48);
    const s54 = next(54), s58 = next(58);
    if (age < life) {
      let colour = maddV(maddV(maddV(colourBase, colourR0, s50), colourSlope, age), colourR1, s54);
      const rgb = colour.slice(0, 3).map((c) => Math.min(Math.max(c, 0), 255));
      let size = mul(mul(sizeDelta, age), vuDiv(1, life));
      size = add(add(size, sizeBase), mul(sizeRange, s58));
      const p0w = origin(world), fvw = subV(world.force, velocity(world));
      let p0c, fvc, numerators;
      if (rows) {
        p0c = origin(clipSpace); fvc = subV(clipSpace.force, velocity(clipSpace));
        const s6 = [mul(rows[6][0], size), mul(rows[6][1], size)];
        numerators = [mul(s6[0], rows[4][0]), mul(s6[1], rows[4][1]), 0, 0];
      }
      let fade = 1, t = age;
      for (let copy = 0, count = blur; ; copy++) {
        const t2 = Math.min(t, T_MAX), poly = add(mul(t2, POLY_A), mul(mul(t2, t2), POLY_B));
        const at = (p0, force, fv) => maddV(addV(scaleV(force, t), p0), fv, poly);
        const positionCm = at(p0w, world.force, fvw).slice(0, 3);
        let state = 0, gs = null;
        if (rows) {
          const c = at(p0c, clipSpace.force, fvc);
          state = clipped(c) ? 1 : 0;
          if (!state) gs = project(rows, c, numerators);
        } else if (options.clip) state = options.clip(positionCm) | 0;
        if (!state) {
          const alpha = ftoi0(mul(colour[3], fade));
          const sprite = {slot, copy, positionCm, halfSizeCm: Math.abs(size), age: t, colour: [...rgb.map(ftoi0), alpha & 255], alpha};
          if (gs) sprite.gs = {rgba: [...rgb.map(ftoi0), alpha], v0: gs.lo, v1: gs.hi, halfPixels: gs.halfPixels}; // v0 = ST(0,0) vertex
          out.push(sprite);
          fade = sub(fade, alphaStep);                 // only drawn copies step the fade (0x3E8)
        }
        t = sub(t, blurStep);
        // 0x438..0x448: ABS.z vf0, vf15 stalls on the new t, so FMAND sees its sign flag.
        if (t < 0) break;
        if (--count <= 0) break;
      }
    }
    age = sub(age, period); slotIndex = add(slotIndex, 1);
    if (age < 0) break;
  }
  return out;
}

// Entry 0x510 (3807A0): trail emitter birth ring.
// emitter: {kernel (84 u32 of e+0x20), capacity (e+0x178), cursor (e+0x17C), particleCount
//   (e+0x20 = capacity * per-birth), positions (capacity*4 u32 or floats: e+0x1A0 records,
//   xyz), velocities (capacity*4: e+0x1A4 records, xyz velocity coefficient + w seed)}.
// options: {vu, clip} as for burstSprites.
export function trailSprites(emitter, options = {}) {
  const K = wordsOf(emitter.kernel), rows = viewRows(options.vu), out = [];
  const cap = emitter.capacity | 0, per = Math.trunc((emitter.particleCount | 0) / cap), perF = per;
  // Float32Array records are floats; any other array holds the raw u32 words.
  const rec = (arr, slot) => { const at = slot * 4, f = (v) => (arr instanceof Float32Array ? v : fromBits(v >>> 0)); return [f(arr[at]), f(arr[at + 1]), f(arr[at + 2]), f(arr[at + 3])]; };
  const ageStep = fromBits(K[3]), blur = K[1] | 0, alphaStep = fromBits(K[12]), blurStep = fromBits(K[13]);
  const [sizeRange, lifeRange, sizeBase, lifeBase] = v4(K, 1), sizeDelta = fromBits(K[8]);
  const force = v4(K, 6), vBase = v4(K, 7), vr0 = v4(K, 8), vr1 = v4(K, 9), vr2 = v4(K, 10);
  const pBase = v4(K, 11), pr0 = v4(K, 12), pr1 = v4(K, 13), pAge = v4(K, 14);
  const colourBase = v4(K, 15), colourR0 = v4(K, 16), colourR1 = v4(K, 17), colourSlope = v4(K, 18);
  const fractionStep = vuDiv(1, perF);
  let numerators = null;
  const batches = (cap >> 6) + 1; let done = 0;
  for (let batch = 0; batch < batches; batch++) {
    let rows64 = cap - done; if (rows64 >= 65) rows64 = 64;
    let age = mul(mul(perF, batch << 6), ageStep);    // EE mul.s chop (3808F4/380904)
    const first = (done + (emitter.cursor | 0) + 1) % cap; done += rows64;
    const neighbour = done < cap ? (done + (emitter.cursor | 0) + 1) % cap : (done - 1 + (emitter.cursor | 0) + 1) % cap;
    const slots = []; for (let i = 0; i < rows64; i++) slots.push((first + i) % cap); slots.push(neighbour);
    // Group j reads records j and j+1 (positions 22+j / 23+j, velocities 87+j / 88+j). The
    // extra group j = rows64 (the program loops rows64+1 times) reads beyond the valid rows:
    // its next position is VU row 87 (velocity record 0) when rows64 == 64, stale otherwise,
    // and its next velocity is always outside the upload: those particles are approximate.
    for (let j = 0; j <= rows64; j++) {
      const pos = rec(emitter.positions, slots[j]), vel = rec(emitter.velocities, slots[j]);
      const extra = j === rows64;
      const nextPos = !extra ? rec(emitter.positions, slots[j + 1]) : (rows64 === 64 ? rec(emitter.velocities, slots[0]) : pos);
      const nextVel = !extra ? rec(emitter.velocities, slots[j + 1]) : vel;
      const seed = vel[3];
      if (sub(seed, 1) < 0) { age = add(age, mul(ageStep, perF)); continue; }
      let seedBits = bitsOf(seed), fraction = 0;
      const next = () => (seedBits = lfsrNext(seedBits), fromBits(seedBits));
      for (let i = 0; i < per; i++) {
        const r = [next(), next(), next(), next(), next(), next(), next(), next(), next()];
        const complement = sub(1, fraction);
        const birthVelocity = maddV(scaleV(vel.slice(0, 3), complement), nextVel.slice(0, 3), fraction);
        const birthPosition = maddV(scaleV(pos.slice(0, 3), complement), nextPos.slice(0, 3), fraction);
        const lifetime = add(lifeBase, mul(lifeRange, r[8]));
        let p0 = addV(birthPosition, pBase.slice(0, 3));
        p0 = maddV(maddV(maddV(p0, pAge, age), pr0, r[0]), pr1, r[1]);
        if (age < lifetime) {
          let v = maddV(maddV(maddV(addV(birthVelocity, vBase.slice(0, 3)), vr0, r[2]), vr1, r[3]), vr2, r[4]);
          const colour = maddV(maddV(maddV(colourBase, colourR0, r[5]), colourSlope, age), colourR1, r[6]);
          const rgb = colour.slice(0, 3).map((c) => Math.min(Math.max(c, 0), 255));
          const fv = subV(force.slice(0, 3), v);
          let size = mul(mul(sizeDelta, age), vuDiv(1, lifetime));
          size = add(add(size, sizeBase), mul(sizeRange, r[7]));
          if (rows) { const s6 = [mul(rows[6][0], size), mul(rows[6][1], size)]; numerators = [mul(s6[0], rows[4][0]), mul(s6[1], rows[4][1]), 0, 0]; }
          let fade = 1, t = age;
          for (let copy = 0, count = blur; ; copy++) {
            const t2 = Math.min(t, T_MAX), poly = add(mul(t2, POLY_A), mul(mul(t2, t2), POLY_B));
            const positionCm = maddV(addV(scaleV(force.slice(0, 3), t), p0), fv, poly);
            let state = 0, gs = null;
            if (rows) {
              const c = transform(rows, [...positionCm, 1]);
              state = clipped(c) ? 1 : 0;
              if (!state) gs = project(rows, c, numerators);
            } else if (options.clip) state = options.clip(positionCm) | 0;
            const alpha = ftoi0(mul(colour[3], fade));
            fade = sub(fade, alphaStep);                 // delay slot 0x838: every copy steps the fade
            if (!state) {
              const sprite = {batch, group: j, particle: i, copy, slot: slots[j], positionCm, halfSizeCm: Math.abs(size), age: t,
                colour: [...rgb.map(ftoi0), alpha & 255], alpha, approximate: extra && i > 0};
              if (gs) sprite.gs = {rgba: [...rgb.map(ftoi0), alpha], v0: gs.hi, v1: gs.lo, halfPixels: gs.halfPixels}; // v0 = ST(0,0) vertex
              out.push(sprite);
            }
            t = sub(t, blurStep);
            if (t < 0) break;                            // 0x920..0x930 (ABS.w vf0, vf3 stall)
            if (--count <= 0) break;
          }
        }
        age = add(age, ageStep); fraction = add(fraction, fractionStep);
      }
    }
  }
  return out;
}

// Flipbook frame for the emitter: base TextureId + trunc(phase) (3708C0 at 370A3C / 371380
// at 3713A8). The phase belongs to the simulation (system+0x184 / emitter+0x10).
