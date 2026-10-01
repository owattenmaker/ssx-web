// LiveComp animation players (builtin 3; engine/livecomp_animation.hpp + the shared
// channel/compose code of engine/rail_modifier.hpp, bit-exact): searchlights (4 s loop),
// blue pinlights (2 s loop, random start), start-gate doors (once at GO), ravens taking off,
// the rock slide, snow crumbs, pickups (speed/trick boost, collecta) and the invisible timers
// that pace fire gushes/fireworks. Data: assets/LIVECOMP/livecomp.json (tools/export_livecomp.py).
//
// Driving it (one call per 60 Hz game tick, same order as the original):
//   anim.fire('go')                      in browser tick 181 BEFORE anim.tick() (stage global
//                                        handler 2; the doors advance in that same tick)
//   anim.fire('section', resource)       when an instance's section streams in (slot 1)
//   anim.fire('contact', ownerResource)  when a rider contacts a trigger (slot 2; rider phase,
//                                        after the entity pass: first advance next tick)
//   anim.tick()                          entity pass: advances every LiveComp; slot-5 ('tick')
//                                        programs start chained LiveComps when the owner's time
//                                        crosses key1/30 s (0x34EBE0); those advance next tick
// Per frame: anim.nodeDeltas(resource) -> per node the native-space (three.js, column-major
// elements) delta rest -> animated, to apply to the static baked geometry of that node's meshes
// (instances[i].meshNodes maps collision-source mesh index -> node).
import { mul, add, sub, div, vuAdd, sqrtNearest, fromBits, bitsOf } from './ee-scalar-float.js';
import { heapU32 } from './heap-views.js';

// Per-frame garbage (docs/web-render-performance.md): a player's matrices are recomputed every drawn frame after a tick.
// The recompute (liveCompMatrices) writes into arrays kept on the channel and the state (sincosTo, rowInto, composeInto):
// the same operations on the same operands as sincos / transformRow / compose, which stay for construction and the tests.
// ---- 0x31BE50 sincos (collision_scalar::sincos) ----
const C = (b) => fromBits(b);
const K_INV_HALF_PI = C(0x3f22f983), K_HALF_PI = C(0x3fc90fdb), K_S7 = C(0x3638ef1f), K_S5 = C(0xb9500d03), K_S3 = C(0x3c088889), K_S1 = C(0xbe2aaaab);
let SIN = 0, COS = 0; // sincosTo's results
function sincosTo(x) {
  let scaled = mul(x, K_INV_HALF_PI); scaled = x < 0 ? sub(scaled, 0.5) : add(scaled, 0.5);
  const quadrant = Math.trunc(scaled); x = sub(x, mul(quadrant, K_HALF_PI));
  const square = mul(x, x); let v = mul(square, K_S7); v = add(v, K_S5);
  v = mul(v, square); v = add(v, K_S3); v = mul(v, square); v = add(v, K_S1);
  v = mul(v, square); v = add(v, 1);
  const s = mul(v, x), c = sqrtNearest(sub(1, mul(s, s)));
  switch (quadrant & 3) { case 0: SIN = s; COS = c; return; case 1: SIN = c; COS = -s; return; case 2: SIN = -s; COS = -c; return; default: SIN = -c; COS = s; }
}
export function sincos(x) { sincosTo(x); return [SIN, COS]; }
// ---- VU 4x4 (row vectors): row j of a*b = a_j * b ----
const transformRow = (m, c) => [0, 1, 2, 3].map((k) => vuAdd(vuAdd(vuAdd(mul(m[0][k], c[0]), mul(m[1][k], c[1])), mul(m[2][k], c[2])), mul(m[3][k], c[3])));
// transformRow(m, [c0, c1, c2, c3]) into o (o is not a row of m)
function rowInto(m, c0, c1, c2, c3, o) {
  const m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3];
  for (let k = 0; k < 4; k++) o[k] = vuAdd(vuAdd(vuAdd(mul(m0[k], c0), mul(m1[k], c1)), mul(m2[k], c2)), mul(m3[k], c3));
}
const matrix4 = () => [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]];
const product = (a, b) => a.map((row) => transformRow(b, row));
const scaledLocal = (m, s) => [m[0], m[1], m[2], [mul(m[3][0], s), mul(m[3][1], s), mul(m[3][2], s), mul(m[3][3], 1)]];
const rows = (flat) => [0, 1, 2, 3].map((r) => flat.slice(4 * r, 4 * r + 4));

// ---- 0x351A80 / 0x351538 / 0x351800 channels ----
function findSegment(curve, cache, n, t) {
  const next = cache[n] + 1; cache[n] = next;
  if (next < curve.length) { const s = curve[next]; if (s[4] <= t && t < s[5]) return next; }
  cache[n] = 0; let k = 0;
  if (curve.length - 1 <= 0) return 0;
  for (;;) { if (t < curve[k][5]) return k; cache[n] += 1; ++k; if (!(cache[n] < curve.length - 1)) return k; }
}
function evaluateChannel(ch, t) {
  const tr = ch.track; let active = 0;
  for (let n = 0; n < 16; n++) {
    if (!((tr.mask >>> n) & 1)) continue;
    const curve = tr.curves[active]; const slot = active++;
    let k = ch.segment[slot];
    const inside = k < curve.length && curve[k][4] <= t && t < curve[k][5];
    if (!inside) k = findSegment(curve, ch.segment, slot, t);
    const s = curve[k];
    let v = add(mul(s[0], t), s[1]); v = mul(v, t); v = add(v, s[2]); v = mul(v, t);
    ch.value[n] = add(v, s[3]);
  }
  const deg = DEG;
  sincosTo(mul(-ch.value[3], deg)); const sx = SIN, cx = COS;
  sincosTo(mul(-ch.value[4], deg)); const sy = SIN, cy = COS;
  sincosTo(mul(-ch.value[5], deg)); const sz = SIN, cz = COS;
  const sxsy = mul(sx, sy), cxsy = mul(cx, sy), cxsz = mul(cx, sz), sxcz = mul(sx, cz), sxsysz = mul(sxsy, sz), cxcz = mul(cx, cz);
  const cxszsy = mul(cxsz, sy), sxsz = mul(sx, sz), cxsycz = mul(cxsy, cz), sxsycz = mul(sxsy, cz), cxcy = mul(cx, cy), msxcy = mul(-sx, cy);
  // the channel's own matrix, rewritten in place (read only by compose / composeInto, right after)
  const m = ch.matrix ??= matrix4(), r0 = m[0], r1 = m[1], r2 = m[2], r3 = m[3];
  r0[0] = mul(cy, cz); r0[1] = mul(-cy, sz); r0[2] = sy; r0[3] = 0;
  r1[0] = add(sxsycz, cxsz); r1[1] = sub(cxcz, sxsysz); r1[2] = msxcy; r1[3] = 0;
  r2[0] = sub(sxsz, cxsycz); r2[1] = add(cxszsy, sxcz); r2[2] = cxcy; r2[3] = 0;
  r3[0] = ch.value[0]; r3[1] = ch.value[1]; r3[2] = ch.value[2]; r3[3] = 1;
}
const DEG = C(0x3c8efa36);
// ---- 0x34DD18 compose: node world = scaledLocal(channel or bind) * parent world (instance for roots) ----
function compose(inst, channels, useBind = false) {
  const out = []; let c = 0;
  for (let i = 0; i < inst.nodes.length; i++) {
    const n = inst.nodes[i];
    const source = n.track && !useBind ? channels[c++].matrix : (n.track && c++, n.bindRows);
    const local = scaledLocal(source, inst.scale);
    out.push(product(local, n.parent >= 0 ? out[n.parent] : inst.matrixRows));
  }
  return out;
}
// compose(inst, channels) into out (the state's previous matrices, same node count), without per-node arrays:
// local = scaledLocal(source) (rows 0..2 the source's, row 3 scaled), node world = product(local, parent world).
function composeInto(inst, channels, out) {
  const nodes = inst.nodes, scale = inst.scale;
  if (!out || out.length !== nodes.length) out = nodes.map(matrix4);
  let c = 0;
  for (let i = 0; i < nodes.length; i++) {
    const n = nodes[i], source = n.track ? channels[c++].matrix : n.bindRows, parent = n.parent >= 0 ? out[n.parent] : inst.matrixRows, o = out[i];
    const s0 = source[0], s1 = source[1], s2 = source[2], s3 = source[3];
    rowInto(parent, s0[0], s0[1], s0[2], s0[3], o[0]); rowInto(parent, s1[0], s1[1], s1[2], s1[3], o[1]); rowInto(parent, s2[0], s2[1], s2[2], s2[3], o[2]);
    rowInto(parent, mul(s3[0], scale), mul(s3[1], scale), mul(s3[2], scale), mul(s3[3], 1), o[3]);
  }
  return out;
}

// ---- 0x341AA0 constructor / 0x341D48 tick / 0x34EBE0 crossing ----
const THIRD = C(0x3d088889), BIG = C(0x501502f9);
export function liveCompConstruct(inst, words, random, fps = 60) {
  const r = (k) => fromBits(words[k]);
  const channels = inst.nodes.filter((n) => n.track).map((n) => ({ track: n.track, segment: new Int32Array(16), value: [...n.track.base, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], matrix: null }));
  const start = channels.length && channels[0].track.curves.length && channels[0].track.curves[0].length ? channels[0].track.curves[0][0][4] : 0;
  for (const ch of channels) evaluateChannel(ch, start);   // 0x34D9B0 base (time = start)
  const unit = (lo, hi) => add(lo, mul(sub(hi, lo), sub(fromBits(((random() & 0x7fffff) | 0x3f800000) >>> 0), 1)));
  const s = { inst, channels, mode: (words[1] << 16) >> 16, enabled: 1, done: 0, delay: 0, previous: BIG, unclamped: BIG, dirty: true, matrices: null };
  s.low = 0 <= r(3) ? mul(r(3), THIRD) : start;
  s.high = r(4) < 0 ? inst.length : mul(r(4), THIRD);
  let t = r(7) < 0 ? s.low : mul(r(7), THIRD); if (t < s.low) t = s.low; if (s.high < t) t = s.high;
  if (words[8] !== 0) t = unit(s.low, s.high);
  s.time = t;
  if (r(6) === 0) s.rate = div(mul(r(5), THIRD), fps);
  else s.rate = div(add(mul(r(5), THIRD), mul(mul(r(6), THIRD), unit(-1, 1))), fps);
  if (words[2] === 1) s.rate = -s.rate;
  s.sampleTime = s.time;
  return s;
}
// Returns 'finished' | 'updated' | 'done' (done: slot 4 runs; the time stays at the range end).
export function liveCompTick(s) {
  s.unclamped = BIG; s.previous = BIG;
  if (s.done) return 'finished';
  if (s.enabled && s.delay >= 0) {
    if (s.delay > 0) s.delay -= 1;
    else {
      s.previous = s.time; let t = s.time;
      if (s.mode === 1) {
        if (0 <= s.rate) { t = add(t, s.rate); s.unclamped = t; if (s.high < t) t = add(s.low, sub(t, s.high)); }
        else { t = add(t, s.rate); s.unclamped = t; if (t < s.low) t = sub(s.high, sub(s.low, t)); }
      } else if (s.mode === 2) {
        if (0 <= s.rate) { t = add(t, s.rate); s.unclamped = t; if (s.high < t) { const o = sub(t, s.high); s.rate = -s.rate; t = sub(s.high, o); } }
        else { t = add(t, s.rate); s.unclamped = t; if (t < s.low) { const u = sub(s.low, t); s.rate = -s.rate; t = add(s.low, u); } }
      } else if (0 <= s.rate) { if (t === s.high) s.done = 1; t = add(t, s.rate); s.unclamped = t; if (s.high < t) t = s.high; }
      else { if (t === s.low) s.done = 1; t = add(t, s.rate); s.unclamped = t; if (t < s.low) t = s.low; }
      s.time = t; s.advanced = true;
    }
  }
  s.dirty = true; s.sampleTime = s.time;
  return s.done ? 'done' : 'updated';
}
// 0x34EBE0: did this tick's advance cross x (seconds)?
export function liveCompCrossed(s, x) {
  const p = s.previous, n = s.unclamped;
  if (p === BIG) return false;
  return p < n ? p <= x && x < n : n <= x && x < p;
}
// 0x361098: node world matrices (source space, rows), instance transform included.
// The returned arrays are the state's own and are rewritten by its next recompute (s.matrixVersion counts recomputes).
export function liveCompMatrices(s) {
  if (s.dirty) {
    const chs = s.channels;
    for (let i = 0; i < chs.length; i++) evaluateChannel(chs[i], s.sampleTime);
    s.matrices = composeInto(s.inst, chs, s.matrices);
    s.matrixVersion = (s.matrixVersion ?? 0) + 1;
    s.dirty = false;
  }
  return s.matrices;
}

// the core's section-player clocks (web/stage_world.inc stage_world_section_clocks: [count, then per entity 10
// words: resource, mode, enabled, done, delay, rate, low, high, time, sampleTime (floats as bits)]). A section LiveComp with a slot-4 /
// slot-5 program is a core entity too, and the stage builtins 28 / 54 change its clock (Gravitude's crash billboards play on and
// fall), which this player cannot see: its state takes the core's after the frame's ticks.
export function syncSectionClocks(core, live) {
  const p = core._stage_world_section_clocks() >> 2, U = heapU32(core), F = core.HEAPF32, n = U[p];
  for (let k = 0, at = p + 1; k < n; k++, at += 10) {
    const s = live.state(U[at]); if (!s) continue;
    s.mode = (U[at + 1] << 16) >> 16; s.enabled = U[at + 2] | 0; s.done = U[at + 3] | 0; s.delay = U[at + 4] | 0;
    s.rate = F[at + 5]; s.low = F[at + 6]; s.high = F[at + 7]; s.time = F[at + 8];
    if (s.sampleTime !== F[at + 9]) { s.sampleTime = F[at + 9]; s.dirty = true; }
  }
}

// ---- native space helpers: native = (x, z, -y) / 100 (row vectors) ----
function invertAffine(m) {   // rows; 3x3 inverse + translation (double precision, render use)
  const a = m[0], b = m[1], c = m[2], t = m[3];
  const det = a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
  const i = [[(b[1] * c[2] - b[2] * c[1]) / det, (a[2] * c[1] - a[1] * c[2]) / det, (a[1] * b[2] - a[2] * b[1]) / det, 0],
    [(b[2] * c[0] - b[0] * c[2]) / det, (a[0] * c[2] - a[2] * c[0]) / det, (a[2] * b[0] - a[0] * b[2]) / det, 0],
    [(b[0] * c[1] - b[1] * c[0]) / det, (a[1] * c[0] - a[0] * c[1]) / det, (a[0] * b[1] - a[1] * b[0]) / det, 0]];
  i.push([0, 1, 2].map((k) => -(t[0] * i[0][k] + t[1] * i[1][k] + t[2] * i[2][k])).concat(1));
  return i;
}
const mulRows = (a, b) => a.map((r) => [0, 1, 2, 3].map((k) => r[0] * b[0][k] + r[1] * b[1][k] + r[2] * b[2][k] + r[3] * b[3][k]));
const S = [[0.01, 0, 0, 0], [0, 0, -0.01, 0], [0, 0.01, 0, 0], [0, 0, 0, 1]], SI = [[100, 0, 0, 0], [0, 0, 100, 0], [0, -100, 0, 0], [0, 0, 0, 1]];

export class LiveCompAnimation {
  // random: 0x317810 words (gameplay RNG 0x4FF030) for random starts/rates (pinlights).
  constructor(data, { random = () => (Math.random() * 0x100000000) >>> 0, fps = data.fps } = {}) {
    this.data = data; this.random = random; this.fps = fps; this.active = new Map(); this.events = [];
    this.instances = data.instances.map((x) => ({ ...x, matrixRows: rows(x.matrix),
      nodes: x.nodes.map((n) => ({ ...n, bindRows: rows(n.bind) })) }));
    this.byResource = new Map(this.instances.map((x) => [x.resource, x])); this.byName = new Map(this.instances.map((x) => [x.name, x]));
    for (const x of this.instances) x.rest = compose(x, [], true);
  }
  start(inst, words) { const s = liveCompConstruct(inst, words, this.random, this.fps); s.advanced = false; this.active.set(inst.resource, s); return s; }
  // trigger: 'go' | 'section' (key = the instance's resource) | 'contact' (key = the trigger
  // instance's resource, e.g. raventriggera_1000 / rockslidetriggera_1000 / crumbTrig_1000).
  fire(trigger, key = null) {
    const started = [];
    for (const inst of this.instances) for (const st of inst.starts) {
      if (st.trigger !== trigger || st.guard) continue;
      if (trigger === 'section' && inst.resource !== key) continue;
      if (trigger === 'contact' && st.ownerResource !== key) continue;
      started.push(this.start(inst, st.words));
    }
    return started;
  }
  // Entity pass. Chained starts (slot 5 of the owner) are created after the pass.
  // chainStarts false: the core's stage VM runs the slot-5 programs (web/stage_world.inc) and its start log drives start().
  tick() {
    if (this.events.length) this.events = [];
    const created = this.created ??= []; created.length = 0;
    this.active.forEach(this.tickEntry ??= (s, res) => {   // insertion order, as for-of over the Map (no entry arrays)
      const r = liveCompTick(s);
      if (r === 'done') this.events.push({ type: 'done', resource: res, name: s.inst.name });   // slot 4 program runs
      if (s.previous === BIG || this.chainStarts === false) return;
      for (const inst of this.instances) for (const st of inst.starts)
        if (st.trigger === 'tick' && st.guard && st.guard.instance === s.inst.name && liveCompCrossed(s, mul(st.guard.frames30, THIRD))) this.created.push(inst, st.words);
    });
    for (let i = 0; i < created.length; i += 2) this.start(created[i], created[i + 1]);
    created.length = 0;
  }
  state(resource) { return this.active.get(resource) ?? null; }
  matrices(resource) { const s = this.active.get(resource); return s ? liveCompMatrices(s) : null; }
  // Per node: native-space delta (rest -> animated), three.js Matrix4 elements (column-major).
  // Cached per state until its matrices change (set-pieces-renderer asks once per mesh, several meshes per resource,
  // every frame); SI * rest^-1 is fixed per instance node. Same arithmetic, same order as
  // mulRows(mulRows(mulRows(SI, invertAffine(rest)), m), S).flat(), without per-call arrays.
  nodeDeltas(resource) {
    const s = this.active.get(resource); if (!s) return null;
    const w = liveCompMatrices(s), inst = s.inst;
    if (s.deltas && s.deltaVersion === s.matrixVersion) return s.deltas;
    const pre = inst.restInverse ??= inst.rest.map((r) => mulRows(SI, invertAffine(r)));
    const out = s.deltas && s.deltas.length === w.length ? s.deltas : w.map(() => new Array(16).fill(0)), t = s.deltaScratch ??= [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]];
    for (let i = 0; i < w.length; i++) {
      const a = pre[i], b = w[i], d = out[i];
      for (let r = 0; r < 4; r++) { const ar = a[r], tr = t[r]; for (let k = 0; k < 4; k++) tr[k] = ar[0] * b[0][k] + ar[1] * b[1][k] + ar[2] * b[2][k] + ar[3] * b[3][k]; }
      for (let r = 0; r < 4; r++) { const tr = t[r]; for (let k = 0; k < 4; k++) d[r * 4 + k] = tr[0] * S[0][k] + tr[1] * S[1][k] + tr[2] * S[2][k] + tr[3] * S[3][k]; }
    }
    s.deltas = out; s.deltaVersion = s.matrixVersion; return out;
  }
}
