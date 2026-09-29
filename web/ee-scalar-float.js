// EE scalar FPU arithmetic for browser ports (PCSX2 semantics used by the
// recompiled originals): MUL/ADD/SUB round toward zero (chop), ADD/SUB keep one
// guard bit (iFPU FPU_ADD_SUB), DIV.S rounds to nearest. Inputs are float32 values.
export const F32 = new Float32Array(1), U = new Uint32Array(F32.buffer);
export const bitsOf = (x) => { F32[0] = x; return U[0]; };
export const fromBits = (b) => { U[0] = b >>> 0; return F32[0]; };
// Round an exactly representable double toward zero to float32.
export function chop(exact) {
  let r = Math.fround(exact);
  if (r !== 0 && Math.abs(r) > Math.abs(exact)) r = fromBits(bitsOf(r) - 1);
  return r;
}
export const mul = (a, b) => chop(a * b);   // float32*float32 is exact in a double
function addSub(a, b, subtract) {
  let x = bitsOf(a), y = bitsOf(b);
  const d = ((x >>> 23) & 255) - ((y >>> 23) & 255);
  if (d >= 25) y &= 0x80000000;
  else if (d <= -25) x &= 0x80000000;
  else if (d > 0) y &= (0xffffffff << (d - 1));
  else if (d < 0) x &= (0xffffffff << (-d - 1));
  const l = fromBits(x), r = fromBits(y);
  return chop(subtract ? l - r : l + r);      // exact in a double after the guard-bit mask
}
export const add = (a, b) => addSub(a, b, false);
export const sub = (a, b) => addSub(a, b, true);
// Correctly rounded float32 quotient (DIV.S nearest). The double quotient only
// double-rounds when it lands exactly on a float32 midpoint; then the exact sign
// of a - q*b (q*b is exact: <= 25 + 24 bits) decides the side.
export function div(a, b) {
  const q = a / b, r = Math.fround(q);
  if (!Number.isFinite(q) || r === q) return r;
  const other = fromBits(bitsOf(r) + (Math.abs(r) > Math.abs(q) ? -1 : 1));
  if ((r + other) / 2 !== q) return r;           // not a midpoint: fround is correct
  const rest = (a - q * b) / b;                  // > 0: true quotient above q
  const up = rest > 0 ? (r > other ? r : other) : rest < 0 ? (r < other ? r : other) : ((bitsOf(r) & 1) ? other : r);
  return up;
}
// VU (and terrain_original) ADD/SUB: one chop rounding of the exact sum, no guard bit.
// TwoSum gives the exact error of the double sum; the chop decision uses its sign.
function vuAddSub(a, b, subtract) {
  if (subtract) b = -b;
  const s = a + b, bb = s - a, err = (a - (s - bb)) + (b - bb);
  let r = Math.fround(s);
  if (r === 0) return r;
  const over = (r - s) - err;                 // r - exact (sign reliable)
  if (over !== 0 && Math.sign(over) === Math.sign(r)) r = fromBits(bitsOf(r) - 1);
  return r;
}
export const vuAdd = (a, b) => vuAddSub(a, b, false);
export const vuSub = (a, b) => vuAddSub(a, b, true);
// SQRT.S rounds to nearest (sqrt of a float32 is never a float32 midpoint in double).
export const sqrtNearest = (x) => Math.fround(Math.sqrt(x));
