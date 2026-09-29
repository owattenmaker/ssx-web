// Waving flags / streamers (flg_* instances): browser port of the original flag
// manager, bit-exact with engine/flag_cloth.hpp (0x34C668 wind + 0x34B818 tick,
// 0x34BCA0 vertices, 0x34B228 build). Data: assets/FLAGS/flags.json
// (tools/export_flags.py). Source units are centimetres, Z up; world output is
// native metres (x, z, -y) / 100, the convention of the world vertices.
//
// Per 60 Hz game tick call tick(frame) once (frame = race tick counter; each
// cloth recomputes its grid only when frame % 2 == its slot parity, like the
// original). Per rendered frame call writeWorld() (or clothVertices()) and
// upload. Every registered instance draws its cloth's shared model-space grid
// with its own instance matrix: (height-1) triangle strips of 2*width vertices.

import { mul, add, sub, div, fromBits } from './ee-scalar-float.js';
export { mul, add, sub, div };

const unitRandom = (word) => sub(fromBits(((word & 0x7fffff) | 0x3f800000) >>> 0), 1);

const TWO_PI = fromBits(0x40c90fdb), NEG_TWO_PI = fromBits(0xc0c90fdb), TO_INDEX = fromBits(0x42a2f983);
const WAVE_MIN = fromBits(0x3c23d70a), UV_MIN = fromBits(0x3a83126f);
const WIND_AMPLITUDE = { 2: fromBits(0x3e99999a), 3: fromBits(0x3ee66666) }, WIND_DEFAULT = fromBits(0x3e19999a);

// 0x34BCA0. cloth: {parameters, width, height, widthSpan, heightSpan, base (Float32Array w*h*3), phase[4]}.
export function flagVertices(cloth, wind, sine, out = new Float32Array(cloth.width * cloth.height * 3)) {
  const p = cloth.parameters, w = cloth.width, h = cloth.height;
  const both = !!(p.attachStart && p.attachEnd), free = !p.attachStart && !p.attachEnd;
  const strength = add(p.minimumStrength, mul(wind, sub(1, p.minimumStrength)));
  const phase = [0, 1, 2, 3].map((i) => mul(cloth.phase[i], NEG_TWO_PI));
  const amp = [0, 1, 2, 3].map((i) => mul(p.amplitude[i], strength));
  let s = 0, c = 0;
  const lookup = (angle) => { const i = (Math.trunc(mul(angle, TO_INDEX)) & 0x1ff); s = sine[i]; c = sine[i + 0x80]; };
  const column = new Float32Array(w * 3);
  let sumA = 0, sumB = 0, colValue = 0;
  const calmWeight = sub(1, strength);
  for (let a2 = 0; a2 < w; a2++) {
    let a0 = a2, f3 = div(colValue, cloth.widthSpan); const u = f3;
    if (free) f3 = 1;
    else if (both) { if (0.5 < f3) f3 = sub(1, f3); }
    else if (p.attachEnd === 1) a0 = w - a2 - 1;
    const f5 = mul(u, TWO_PI);
    lookup(add(phase[0], mul(f5, p.frequency[0])));
    if (!both) sumA = add(sumA, mul(mul(amp[0], f3), c));
    let x = mul(mul(amp[0], f3), s), y = sumA, z = 0;
    lookup(add(phase[1], mul(f5, p.frequency[1])));
    if (!both) sumB = add(sumB, mul(mul(amp[1], f3), c));
    x = add(x, mul(mul(amp[1], f3), s)); y = add(y, sumB); z = add(z, 0);
    const o = [x, y, z];
    for (let k = 0; k < 3; k++) o[k] = add(o[k], add(mul(mul(p.gust[k], f3), strength), mul(mul(p.calm[k], f3), calmWeight)));
    column[a0 * 3] = o[0]; column[a0 * 3 + 1] = o[1]; column[a0 * 3 + 2] = o[2];
    colValue = add(colValue, 1);
  }
  let vertex = 0, rowValue = 0;
  const base = cloth.base;
  for (let row = 0; row < h; row++) {
    let cv = 0;
    for (let col = 0; col < w; col++) {
      let f8 = div(cv, cloth.widthSpan); const f9 = div(rowValue, cloth.heightSpan); let f10 = f8;
      if (both) { if (0.5 < f8) f8 = sub(1, f8); }
      else if (p.attachEnd === 1) { f8 = sub(1, f8); f10 = f8; }
      else if (free) f8 = 1;
      const v = vertex * 3;
      if (p.pinFirstRow && f9 === 0) {           // original quirk: no vertex increment
        out[v] = base[v]; out[v + 1] = base[v + 1]; out[v + 2] = base[v + 2]; cv = add(cv, 1); continue;
      }
      let rx = add(base[v], column[col * 3]), ry = add(base[v + 1], column[col * 3 + 1]), rz = add(base[v + 2], column[col * 3 + 2]);
      if (0 < f10 && WAVE_MIN < amp[2]) {
        const half = mul(add(f10, f9), 0.5), mid = mul(add(f8, f9), 0.5);
        lookup(add(phase[2], mul(half, mul(p.frequency[2], TWO_PI))));
        rx = add(rx, mul(mul(amp[2], f8), s)); ry = add(ry, mul(mul(amp[2], mid), c)); rz = add(rz, mul(mul(amp[2], f9), s));
      }
      if (0 < f10 && WAVE_MIN < amp[3]) {
        const half = mul(add(f10, f9), 0.5), mid = mul(add(f8, f9), 0.5);
        lookup(add(phase[3], mul(half, mul(p.frequency[3], TWO_PI))));
        rx = add(rx, mul(mul(amp[3], f8), s)); ry = add(ry, mul(mul(amp[3], mid), c)); rz = add(rz, mul(mul(amp[3], sub(1, f9)), s));
      }
      out[v] = rx; out[v + 1] = ry; out[v + 2] = rz; vertex++; cv = add(cv, 1);
    }
    rowValue = add(rowValue, 1);
  }
  return out;
}

// 0x34C668 wind part. state {wind, base, delta, timer}; random() -> uint32 word (0x3177F0).
export function flagWindTick(state, mode, fps, random) {
  const amplitude = WIND_AMPLITUDE[mode] ?? WIND_DEFAULT;
  const timer = add(state.timer, div(1, fps)); state.timer = timer;
  if (1 <= timer) {
    state.timer = sub(timer, 1); state.base = add(state.base, state.delta);
    const low = -amplitude, r = unitRandom(random());
    state.delta = add(low, mul(sub(amplitude, low), r));
    if (add(state.base, state.delta) < 0) state.delta = -state.base;
    if (1 < add(state.base, state.delta)) state.delta = sub(1, state.base);
  }
  state.wind = add(state.base, mul(state.delta, state.timer));
}

// 0x34B818 (phase advance, parity-gated grid, UV scroll). Returns true when the grid changed.
export function flagClothTick(cloth, wind, frame, sine) {
  const p = cloth.parameters;
  const strength = add(p.minimumStrength, mul(wind, sub(1, p.minimumStrength)));
  for (let i = 0; i < 4; i++) { let x = add(cloth.phase[i], mul(p.speed[i], strength)); if (1 <= x) x = sub(x, 1); cloth.phase[i] = x; }
  let updated = false;
  if (frame % 2 === cloth.parity) { flagVertices(cloth, wind, sine, cloth.vertices); updated = true; }
  if (UV_MIN < p.uvSpeed[0] || UV_MIN < p.uvSpeed[1]) {
    for (let k = 0; k < 2; k++) { let x = add(cloth.uvOffset[k], p.uvSpeed[k]); if (1 < x) x = sub(x, 1); cloth.uvOffset[k] = x; }
  }
  return updated;
}

// 0x34B228 grid build (phases from four random words; rest grid, UV, colour bilinear).
export function flagBuild(cloth, corners, random, wind, sine) {
  for (let i = 0; i < 4; i++) cloth.phase[i] = unitRandom(random());
  cloth.uvOffset = [0, 0];
  const p = cloth.parameters;
  cloth.width = 8; cloth.height = p.amplitude[2] < WAVE_MIN && p.amplitude[3] < WAVE_MIN ? 2 : 5;
  cloth.widthSpan = cloth.width - 1; cloth.heightSpan = cloth.height - 1;
  const n = cloth.width * cloth.height;
  cloth.base = new Float32Array(n * 3); cloth.uv = new Float32Array(n * 2); cloth.colour = new Float32Array(n * 4); cloth.vertices = new Float32Array(n * 3);
  const bilinear = (field, a, iu, u, iv, v) => {
    const k = corners[field];
    const top = add(mul(k[0][a], iu), mul(k[1][a], u)), bottom = add(mul(k[2][a], iu), mul(k[3][a], u));
    return add(mul(top, iv), mul(bottom, v));
  };
  let vertex = 0;
  for (let row = 0; row < cloth.height; row++) for (let col = 0; col < cloth.width; col++, vertex++) {
    const u = div(col, cloth.widthSpan), v = div(row, cloth.heightSpan), iu = sub(1, u), iv = sub(1, v);
    for (let a = 0; a < 3; a++) cloth.base[vertex * 3 + a] = bilinear('position', a, iu, u, iv, v);
    for (let a = 0; a < 2; a++) cloth.uv[vertex * 2 + a] = bilinear('uv', a, iu, u, iv, v);
    cloth.colour[vertex * 4] = 1;
    for (let a = 1; a < 4; a++) cloth.colour[vertex * 4 + a] = bilinear('colour', a, iu, u, iv, v);
  }
  cloth.mesh = true;
  flagVertices(cloth, wind, sine, cloth.vertices);
}

// Triangle list for the h-1 strips (strip s: (s,c),(s+1,c) for c = 0..w-1).
export function flagTriangleIndices(width, height) {
  const out = [];
  for (let s = 0; s + 1 < height; s++) for (let c = 0; c + 1 < width; c++) {
    const a = s * width + c, b = (s + 1) * width + c;
    out.push(a, b, a + 1, a + 1, b, b + 1);
  }
  return new Uint32Array(out);
}

// Visual-stream stand-in when the lead has no shared 0x4FF018 RNG to pass in.
function defaultRandom() { return (Math.random() * 0x100000000) >>> 0; }

export class FlagAnimation {
  // data = flags.json. options.random: () => uint32 (0x3177F0 words; shared visual RNG for parity),
  // options.parityBase: gp+0xF20 toggle at manager construction (0 in the audited race).
  constructor(data, { random = defaultRandom, parityBase = 0, mode = data.wind.mode, fps = data.fps } = {}) {
    this.data = data; this.random = random; this.mode = mode; this.fps = fps;
    this.sine = new Float32Array(data.sine_table);
    if (this.sine.length !== 640) throw new Error('flag sine table must hold 640 entries');
    const i = data.wind.initial; this.wind = { wind: i.wind, base: i.base, delta: i.delta, timer: i.timer };
    this.slots = Array.from({ length: 15 }, (_, k) => ({ parity: (parityBase + k) & 1, count: 0, instances: [] }));
    this.instances = data.instances.map((x) => ({ ...x, slot: -1 }));
    this.byResource = new Map(this.instances.map((x) => [x.resource, x]));
  }
  // Entity construction (stage slot-1 program when the instance's section activates).
  activate(resource) {
    const inst = this.byResource.get(resource); if (!inst || inst.slot >= 0) return inst?.slot ?? -1;
    const cloth = this.data.cloths[inst.cloth];
    for (let k = 0; k < 15; k++) {
      const s = this.slots[k];
      if (s.count && s.cloth === inst.cloth) { s.instances.push(inst); s.count++; inst.slot = k; return k; }
    }
    for (let k = 0; k < 15; k++) {
      const s = this.slots[k]; if (s.count) continue;
      const group = cloth.groups.find((g) => g.resources.includes(resource)) ?? cloth.groups[0];
      Object.assign(s, { cloth: inst.cloth, parameters: cloth.parameters, phase: [0, 0, 0, 0] });
      flagBuild(s, group.corners, this.random, this.wind.wind, this.sine);
      s.instances = [inst]; s.count = 1; inst.slot = k; return k;
    }
    return -1;   // all 15 slots busy: the original leaves the flag undrawn
  }
  // Activate in ascending rid order (flags.json order), as the original registers a section.
  activateAll() { for (const inst of this.instances) this.activate(inst.resource); }
  deactivate(resource) {
    const inst = this.byResource.get(resource); if (!inst || inst.slot < 0) return false;
    const s = this.slots[inst.slot]; const k = s.instances.indexOf(inst);
    s.instances[k] = s.instances[s.instances.length - 1]; s.instances.pop(); s.count--; inst.slot = -1;
    if (s.count === 0) { s.mesh = false; s.base = s.vertices = s.uv = s.colour = null; }
    return true;
  }
  // One game tick (60 Hz). Returns the slots whose grid changed this tick.
  tick(frame) {
    flagWindTick(this.wind, this.mode, this.fps, this.random);
    const changed = [];
    for (let k = 0; k < 15; k++) { const s = this.slots[k]; if (s.count && s.mesh && flagClothTick(s, this.wind.wind, frame, this.sine)) changed.push(k); }
    return changed;
  }
  // Model-space grid (cm) of an active instance, or null.
  clothOf(resource) { const inst = this.byResource.get(resource); return inst && inst.slot >= 0 ? this.slots[inst.slot] : null; }
  // Native-space positions (m) of one instance: world = local * M (row vectors), native = (x, z, -y)/100.
  writeWorld(resource, out, origin = null) {
    const inst = this.byResource.get(resource), s = this.clothOf(resource); if (!s) return null;
    const m = inst.matrix, v = s.vertices, n = s.width * s.height;
    out ??= new Float32Array(n * 3);
    for (let i = 0; i < n; i++) {
      const x = v[i * 3], y = v[i * 3 + 1], z = v[i * 3 + 2];
      const wx = x * m[0] + y * m[4] + z * m[8] + m[12], wy = x * m[1] + y * m[5] + z * m[9] + m[13], wz = x * m[2] + y * m[6] + z * m[10] + m[14];
      out[i * 3] = wx / 100 - (origin?.x ?? 0); out[i * 3 + 1] = wz / 100 - (origin?.y ?? 0); out[i * 3 + 2] = -wy / 100 - (origin?.z ?? 0);
    }
    return out;
  }
}
