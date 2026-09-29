// Flag / streamer animation (web/flag-animation.js):
// 1. bit-exact against engine/flag_cloth.hpp goldens (tools/test_flag_cloth_native.py,
//    whose port is itself proven equal to the recompiled originals in the same run);
// 2. against live PS2 flag-manager memory (tools/export_flags.py flag-snapshots.json):
//    the mesh's quantized strip buffers (short = trunc(v * scale)) must be reproduced
//    exactly from the snapshot phases/wind (current tick, or the previous tick when the
//    slot parity skipped this one);
// 3. manager bookkeeping + world placement against the packaged static flag quads.
// usage: node test-flag-animation.mjs [LOCATION] (default ARA1: public/assets/FLAGS; others
// public/assets/<LOC>/FLAGS from tools/export_flags.py --location LOC).
import { readFileSync, existsSync } from 'node:fs';
import { FlagAnimation, flagVertices, flagWindTick, flagClothTick, flagBuild, flagTriangleIndices, mul, add, sub, div } from './flag-animation.js';

const LOC = process.argv[2] || 'ARA1';
const dir = new URL(LOC === 'ARA1' ? './public/assets/FLAGS/' : `./public/assets/${LOC}/FLAGS/`, import.meta.url);
// PS2 snapshots and the native golden are test data: public/test-data, laid out like public/assets
const testDir = new URL(dir.href.replace('/public/assets/', '/public/test-data/'));
const load = (name) => JSON.parse(readFileSync(new URL(name, dir), 'utf8'));
const data = load('flags.json');
const F = new Float32Array(1), U = new Uint32Array(F.buffer);
const f = (b) => { U[0] = b >>> 0; return F[0]; };
const bitsOf = (x) => { F[0] = x; return U[0]; };
const fs = (a) => a.map(f);
let failures = 0;
const fail = (msg) => { if (failures++ < 10) console.error(msg); };
const same = (a, b) => a.length === b.length && a.every((x, i) => bitsOf(x) === bitsOf(b[i]));

const CLOTHS = LOC === 'ARA1' ? 8 : data.cloths.length, INSTANCES = LOC === 'ARA1' ? 41 : data.instances.length;
if (data.cloths.length !== CLOTHS || data.instances.length !== INSTANCES || data.sine_table.length !== 640) throw new Error('unexpected flags.json shape');
const sine = new Float32Array(data.sine_table);

const params = (p) => ({ ...p, speed: fs(p.speed), amplitude: fs(p.amplitude), frequency: fs(p.frequency), calm: fs(p.calm), gust: fs(p.gust), minimumStrength: f(p.minimumStrength), uvSpeed: fs(p.uvSpeed) });
const clothFrom = (c) => ({ parameters: params(c.parameters), width: c.width, height: c.height, widthSpan: f(c.widthSpan), heightSpan: f(c.heightSpan),
  base: new Float32Array(fs(c.base)), phase: fs(c.phase), uvOffset: fs(c.uvOffset), parity: c.parity });

// ---- 1. goldens ----
if (existsSync(new URL('flag-golden.json', testDir))) {
  const g = JSON.parse(readFileSync(new URL('flag-golden.json', testDir), 'utf8'));
  let n = 0;
  for (const [a, b, m, s, d, q] of g.arithmetic) {
    const x = f(a), y = f(b);
    if (bitsOf(mul(x, y)) !== m || bitsOf(add(x, y)) !== s || bitsOf(sub(x, y)) !== d || bitsOf(div(x, y)) !== q) fail(`arithmetic ${x} ${y}`);
    n++;
  }
  for (const c of g.vertices) {
    const cloth = clothFrom(c), out = flagVertices(cloth, f(c.wind), sine);
    if (cloth.parameters.pinFirstRow) out.fill(0, cloth.width * (cloth.height - 1) * 3);
    if (!same(Array.from(out), fs(c.vertices))) fail(`vertices case ${n}`); n++;
  }
  for (const c of g.wind) {
    const [wind, base, delta, timer] = fs(c.in); const s = { wind, base, delta, timer }; let used = 0;
    flagWindTick(s, c.mode, c.fps, () => { used++; return c.word; });
    if (used !== c.used || !same([s.wind, s.base, s.delta, s.timer], fs(c.out))) fail(`wind case ${n}`); n++;
  }
  for (const c of g.tick) {
    const cloth = clothFrom(c); cloth.vertices = new Float32Array(cloth.width * cloth.height * 3);
    const updated = flagClothTick(cloth, f(c.wind), c.frame, sine);
    if (updated && cloth.parameters.pinFirstRow) cloth.vertices.fill(0, cloth.width * (cloth.height - 1) * 3);
    if (updated !== !!c.updated || !same(cloth.phase, fs(c.phaseOut)) || !same(cloth.uvOffset, fs(c.uvOffsetOut)) || (updated && !same(Array.from(cloth.vertices), fs(c.vertices)))) fail(`tick case ${n}`); n++;
  }
  for (const c of g.build) {
    const p = fs(c.corners.position), uv = fs(c.corners.uv), col = fs(c.corners.colour);
    const corners = { position: [0, 1, 2, 3].map((i) => p.slice(3 * i, 3 * i + 3)), uv: [0, 1, 2, 3].map((i) => uv.slice(2 * i, 2 * i + 2)), colour: [0, 1, 2, 3].map((i) => col.slice(4 * i, 4 * i + 4)) };
    const cloth = { parameters: params(c.parameters), phase: [0, 0, 0, 0] }; let at = 0;
    flagBuild(cloth, corners, () => c.words[at++], f(c.wind), sine);
    if (cloth.parameters.pinFirstRow) cloth.vertices.fill(0, cloth.width * (cloth.height - 1) * 3);
    if (cloth.height !== c.height || !same(cloth.phase, fs(c.phase)) || !same(Array.from(cloth.base), fs(c.base)) || !same(Array.from(cloth.uv), fs(c.uv)) ||
        !same(Array.from(cloth.colour), fs(c.colour)) || !same(Array.from(cloth.vertices), fs(c.vertices))) fail(`build case ${n}`);
    n++;
  }
  console.log(`${n} engine/flag_cloth.hpp golden cases match bit for bit`);
} else console.log('flag-golden.json absent (run tools/test_flag_cloth_native.py): skipping golden comparison');

// ---- 2. PS2 memory ----
if (existsSync(new URL('flag-snapshots.json', testDir))) {
  const snaps = JSON.parse(readFileSync(new URL('flag-snapshots.json', testDir), 'utf8')); let cur = 0, prev = 0;
  for (const c of snaps.cases) {
    const def = data.cloths[c.cloth], w = def.width, h = def.height;
    const cloth = { parameters: def.parameters, width: w, height: h, widthSpan: w - 1, heightSpan: h - 1, base: new Float32Array(c.grid.flat()), phase: c.phase.slice() };
    const quantize = (v) => Array.from(v, (x) => Math.trunc(mul(x, c.mesh_scale)));   // EE mul (round toward zero)
    const buffers = c.buffers.map((b) => b.flat());
    const match = (v) => buffers.some((b) => b.every((x, i) => x === v[i]));
    const m = c.manager, p = def.parameters;
    const now = quantize(flagVertices(cloth, m.wind, sine));
    if (match(now)) { cur++; continue; }
    // Previous tick: undo this tick's phase advance; wind one tick back (no keyframe crossing).
    const strength = add(p.minimumStrength, mul(m.wind, sub(1, p.minimumStrength)));
    const before = { ...cloth, phase: c.phase.map((x, i) => { let y = sub(x, mul(p.speed[i], strength)); if (y < 0) y = add(y, 1); return y; }) };
    const windBefore = add(m.base, mul(m.delta, sub(m.timer, div(1, data.fps))));
    if (match(quantize(flagVertices(before, windBefore, sine)))) { prev++; continue; }
    fail(`snapshot ${c.snapshot} slot ${c.slot}: no mesh buffer reproduced`);
  }
  console.log(`${snaps.cases.length} PS2 flag-manager slots reproduced exactly (${cur} current tick, ${prev} previous tick by parity)`);
} else console.log('flag-snapshots.json absent: skipping PS2 memory comparison');

// ---- 3. manager bookkeeping and world placement ----
{
  let seed = 0x3177f0; const random = () => (seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0);
  const anim = new FlagAnimation(data, { random });
  anim.activateAll();
  const used = anim.slots.filter((s) => s.count);
  if (used.length !== CLOTHS || used.reduce((n, s) => n + s.count, 0) !== INSTANCES) fail(`activation did not fill ${CLOTHS} cloths with ${INSTANCES} instances`);
  if (anim.slots.some((s, k) => s.parity !== (k & 1))) fail('slot parity');
  const tris = flagTriangleIndices(8, 5); if (tris.length !== 7 * 4 * 6) fail('triangle count');
  // Rest pose placement: the grid corners through the instance matrix land on the packaged static quad.
  const world = new URL(`./public/assets/${LOC}/`, import.meta.url);
  const native = new URL(`../local/assets/native/${LOC}/`, import.meta.url);
  if (existsSync(new URL('indices.bin', native)) && existsSync(new URL('world.json', native))) {
    const wj = JSON.parse(readFileSync(new URL('world.json', native), 'utf8'));
    const vb = new Float32Array(readFileSync(new URL('vertices.bin', world)).buffer.slice(0));
    const ib = new Uint32Array(readFileSync(new URL('indices.bin', native)).buffer.slice(0));
    let checked = 0, worst = 0;
    for (const src of wj.collision_sources) {
      if (src.kind !== 'instance' || !src.name?.startsWith('flg_')) continue;
      const res = (src.rid << 8) | src.track; const inst = anim.byResource.get(res); if (!inst) continue;
      const s = anim.slots[inst.slot]; const saved = s.vertices; s.vertices = s.base;
      const out = anim.writeWorld(res); s.vertices = saved;
      const quad = [...new Set(ib.slice(src.first_triangle * 3, (src.first_triangle + src.triangle_count) * 3))].map((i) => [vb[i * 10], vb[i * 10 + 1], vb[i * 10 + 2]]);
      for (const k of [0, 7, 32, 39]) {
        const p = [out[k * 3], out[k * 3 + 1], out[k * 3 + 2]];
        const d = Math.min(...quad.map((q) => Math.hypot(q[0] - p[0], q[1] - p[1], q[2] - p[2]))); worst = Math.max(worst, d);
      }
      checked++;
    }
    if (checked !== INSTANCES || worst > 0.02) fail(`world placement: ${checked} instances, worst corner ${worst} m`);
    console.log(`${checked} flag instances: rest-grid corners on the packaged static quads (worst ${worst.toFixed(4)} m)`);
  }
  // Two seconds of ticks: each cloth recomputes only on its parity; motion stays bounded.
  let updates = 0;
  for (let frame = 1; frame <= 120; frame++) {
    const changed = anim.tick(frame); updates += changed.length;
    if (changed.some((k) => anim.slots[k].parity !== frame % 2)) fail('grid update outside slot parity');
  }
  if (updates !== CLOTHS * 60) fail(`expected ${CLOTHS * 60} grid updates, got ${updates}`);
  for (const s of used) if (!s.vertices.every(Number.isFinite) || s.vertices.some((x, i) => Math.abs(x - s.base[i]) > 1200)) fail('unbounded flag motion');
  if (!(anim.wind.wind >= 0 && anim.wind.wind <= 1)) fail('wind outside [0,1]');
  console.log(`FlagAnimation (${LOC}): ${CLOTHS} cloths / ${INSTANCES} instances, ${updates} parity-gated grid updates in 120 ticks, wind ${anim.wind.wind.toFixed(3)}`);
}
if (failures) { console.error(`${failures} flag animation failures`); process.exit(1); }
console.log('flag animation OK');
