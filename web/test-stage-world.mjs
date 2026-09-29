// Stage world (web/stage_world.inc: trigger / timer programs, LiveComp slot 4/5, section-streamed emitters and every
// particle effect of the course stage scripts) against PS2 savestates (tools/export_particle_snapshots.py), plus the
// renderer's fast sprite evaluators (web/set-piece-particle-eval.js) against the bit-exact VU1 model
// (web/set-piece-particle-sprites.js). Effects are compared word for word except the visual-RNG seeds.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { burstSprites, trailSprites } from './set-piece-particle-sprites.js';
import { burstSpritesFast, trailSpritesFast, SPRITE_FLOATS } from './set-piece-particle-eval.js';

const ref = '../local/reference/set-piece-particles/', runs = '../local/ps2-capture/runs/';
// 1. Fast evaluators vs the exact VU1 model on every live emitter of the draw oracle.
const oraclePath = '../local/browser-validation/set-piece-particle-draw-oracle.json';
if (fs.existsSync(oraclePath)) {
  const oracle = JSON.parse(fs.readFileSync(oraclePath, 'utf8')); let sprites = 0, worst = 0, edge = 0;
  for (const c of oracle.cases) {
    const K = Uint32Array.from(c.kernel.map((x) => x >>> 0)), F = new Float32Array(K.buffer), out = new Float32Array(1 << 18);
    let exact, n;
    if (c.kind === 'particle') { exact = burstSprites(c.kernel); n = burstSpritesFast(K, F, out, 0, 1 << 15); }
    else {
      exact = trailSprites({ kernel: c.kernel, capacity: c.capacity, cursor: c.cursor, particleCount: c.particleCount, positions: c.positions, velocities: c.velocities });
      const A = Uint32Array.from(c.positions.map((x) => x >>> 0)), B = Uint32Array.from(c.velocities.map((x) => x >>> 0));
      n = trailSpritesFast(K, F, c.capacity, c.cursor, new Float32Array(A.buffer), new Float32Array(B.buffer), B, out, 0, 1 << 15);
    }
    // Native floats vs the EE/VU chop rounding: a particle exactly at its lifetime edge may flip (age < life).
    assert.ok(Math.abs(n - exact.length) <= 1, `${c.kind} tex${c.texture}: sprite count ${n} vs ${exact.length}`);
    if (n !== exact.length) { edge++; sprites += n; continue; }
    exact.forEach((s, i) => {
      const o = i * SPRITE_FLOATS, scale = Math.max(1, ...s.positionCm.map(Math.abs));
      for (let a = 0; a < 3; a++) { const e = Math.abs(out[o + a] - s.positionCm[a]) / scale; worst = Math.max(worst, e); assert.ok(e < 2e-5, `${c.kind} sprite ${i} position`); }
      assert.ok(Math.abs(out[o + 3] - s.halfSizeCm) <= 1e-3 * Math.max(1, s.halfSizeCm), `${c.kind} sprite ${i} size`);
      for (let k = 0; k < 4; k++) assert.ok(Math.abs(out[o + 4 + k] - s.colour[k]) <= 1, `${c.kind} sprite ${i} colour ${k}`);
    });
    sprites += n;
  }
  assert.ok(edge <= oracle.cases.length / 20, `${edge} emitters with a lifetime-edge count difference`);
  console.log(`set-piece particle fast evaluators: ${oracle.cases.length} emitters (${edge} with one lifetime-edge particle), ${sprites} sprites within ${worst.toExponential(1)} (relative position) of the VU1 model`);
} else console.log('set-piece particle fast evaluators: draw oracle missing, skipped');

// 2. The core against PS2 savestates. [label, harness, capture, args, snapshot files, tick ranges compared [from, through]:
// while the riders that fire the triggers are exact (single-rider runs lack the computer riders' contacts: the Metro-City
// seqgush timer at 418 was started by a computer rider)]. MeshAnim break pieces (bra2-break: the human breaks the
// chinaroof at 6466) compare their deterministic words exactly (flags, life, fade, spin max, end mode, gravity, nodes).
const cases = [
  ['ARA1 six riders (--isolate)', 'compare-ai-capture.mjs', 'setpieces/full.bin', ['--zoe', '--isolate', '--ticks', '2900'], ['ara1-full', 'ara1-crumb', 'ara1-crumb2'], [0, 2819]],
  ['BHP1 (single rider)', 'compare-ps2-capture.mjs', 'setpieces-bhp1/full.bin', ['--pad', '--zoe', '--event', '--sync-rng'], ['bhp1-full', 'bhp1-pickup-burst'], [0, 4420], { score: true }], // point pickups: MagnetModifier + award exact (1085, 2327); bhp1-pickup-burst: the award's builtin25 burst is born at the frozen magnet matrix (1089..2341)
  ['ARA1 one-way volume (single rider)', 'compare-ps2-capture.mjs', 'setpieces/full.bin', ['--pad', '--zoe', '--event', '--sync-rng'], ['ara1-boost'], [11700, 11918], { effects: false }], // particle effects there come from the computer riders' triggers; builtin7 Boost entity words (ctor 0x341388, countdown) at 11758 / 11918
  ['BRA2 six riders (--isolate)', 'compare-ai-capture.mjs', 'setpieces-bra2/full.bin', ['--zoe', '--isolate', '--ticks', '500'], ['bra2-full'], [0, 418]],
  ['BRA2 (single rider)', 'compare-ps2-capture.mjs', 'setpieces-bra2/full.bin', ['--pad', '--zoe', '--event', '--sync-rng'], ['bra2-full', 'bra2-break'], [[820, 2418], [6465, 6582]]],
  // Crow's Nest (solo): osprey (random-gated program 42), dragons, gush, one-way volume from the load; the box vehicles and the
  // tramlores MultiSplines (section-built) vs every PS2 modifier state (set-piece-capture-hooks.mjs).
  ['ABA1 (single rider)', 'compare-ps2-capture.mjs', 'setpieces-aba1/full.bin', ['--pad', '--zoe', '--event', '--sync-rng'], ['aba1-full'], [0, 2418],
    { env: { TICK_HOOK: './set-piece-capture-hooks.mjs', SET_PIECE_FIXTURE: '../local/reference/set-pieces-peak1/aba1-multispline.json' }, check: (s) => assert.ok(s.multiChecked >= 13 && s.multiExact === s.multiChecked && !s.setPieceBad.length, `ABA1 MultiSplines ${JSON.stringify(s.setPieceBad)}`) }],
  // R&B: the computer rider (Moby) fired the seqgush 1000/1100 and MZseq triggers first (419, 2819..3224, 4820 differ single-rider)
  // and the train trigger at 4850 (injected); the rider physics leaves the capture at 6672. MeshAnims: treerailhevbb 1023/1026.
  ['ASS1 (single rider)', 'compare-ps2-capture.mjs', 'setpieces-ass1/full.bin', ['--pad', '--zoe', '--event', '--sync-rng'], ['ass1-full'], [[820, 2419], [3620, 4420], [5220, 6419]],
    { env: { TICK_HOOK: './set-piece-capture-hooks.mjs', SET_PIECE_FIXTURE: '../local/reference/set-pieces-peak1/ass1-train.json', SET_PIECE_INJECT: `4850:${177419 * 8 + 2}` },
      check: (s) => assert.ok(s.splineChecked >= 25 && s.splineExact === s.splineChecked, `ASS1 train ${JSON.stringify(s.setPieceBad)}`) }],
  // Happiness Rival Time (docs/backcountry.md): human + Mac from the rolling start, ravens/osprey/tumbler particles and the
  // stage world while both riders are exact (the human leaves the capture at 5229).
  ['ABC1 two riders', 'compare-ai-capture.mjs', 'setpieces-abc1/full.bin', ['--zoe', '--ticks', '5220'], ['abc1-full'], [0, 5202]],
];
for (const [label, harness, capture, args, snaps, rangeList, options = {}] of cases) {
  const ranges = typeof rangeList[0] === 'number' ? [rangeList] : rangeList, through = ranges.at(-1)[1];
  const files = snaps.map((s) => `${ref}${s}.snapshots.json`);
  if (!fs.existsSync(runs + capture) || files.some((f) => !fs.existsSync(f))) { console.log(`stage world ${label}: capture or snapshots missing, skipped`); continue; }
  const out = execFileSync(process.execPath, [harness, runs + capture, ...args], { cwd: new URL('.', import.meta.url).pathname, env: { ...process.env, ...(options.env || {}), STAGE_WORLD_PS2: files.join(',') }, maxBuffer: 1 << 28, stdio: ['ignore', 'pipe', 'ignore'] }).toString();
  const summary = JSON.parse(out), rows = (harness === 'compare-ai-capture.mjs' ? summary.stageWorld?.ps2 : summary.stageWorld) || [];
  options.check?.(summary);
  if (options.score) assert.ok(summary.firstScoreMismatch == null, `${label}: score differs from tick ${summary.firstScoreMismatch} ${JSON.stringify(summary.scoreFirst).slice(0, 300)}`);
  const checked = rows.filter((r) => ranges.some(([from, to]) => r.tick >= from && r.tick <= to));
  for (const [from, to] of ranges) { const n = checked.filter((r) => r.tick >= from && r.tick <= to).length; assert.ok(n >= (from ? 2 : 1), `${label}: only ${n} snapshots compared in ${from}..${to}`); }
  let effects = 0, meshanims = 0, multi = 0, boosts = 0;
  for (const r of checked) {
    if (options.effects !== false) assert.ok(r.exact === r.ps2 && !r.extra && !r.missing.length, `${label} tick ${r.tick}: ${r.exact}/${r.ps2} effects exact, ${r.extra} extra, missing ${r.missing}; ${JSON.stringify(r.diffs || r.extras)}`);
    if (options.effects !== false) effects += r.ps2;
    for (const m of r.meshanims || []) { assert.ok(m.exact, `${label} tick ${r.tick}: MeshAnim ${m.resource} ${JSON.stringify(m)}`); meshanims++; }
    assert.ok(!(r.meshanimExtra || []).length, `${label} tick ${r.tick}: extra MeshAnims ${r.meshanimExtra}`);
    for (const b of r.boosts || []) { assert.ok(b.exact, `${label} tick ${r.tick}: Boost ${JSON.stringify(b)}`); boosts++; }
    for (const m of r.multi || []) { assert.ok(m.exact, `${label} tick ${r.tick}: MultiParticle group ${JSON.stringify(m)}`); multi++; }
  }
  console.log(`stage world ${label}: ${checked.length} PS2 savestates, ${effects} particle effects word-exact (seeds excluded)${meshanims ? `, ${meshanims} MeshAnim states exact` : ''}${multi ? `, ${multi} MultiParticle group states exact` : ''}${options.score ? ', score exact (stage pickups)' : ''}${boosts ? `, ${boosts} one-way volume states exact` : ''}${summary.splineChecked ? `, ${summary.splineExact} spline piece states exact` : ''}${summary.multiChecked ? `, ${summary.multiExact} MultiSpline states exact` : ''} through tick ${through}`);
}

// 3. The world visual pass (web/stage_world.inc stage_world_visual_pass + flag grid builds) against the PS2 draw trace of
// the shared visual generator 0x4FF018 (local/ps2-capture/runs/visual-rng/full2.trace.json, Snow Jam from the countdown;
// trace labels = update + 1 for the post-rider passes and the section pass): flag wind 0x34C71C, camera splash 0x2F3BE8,
// lightning 0x390CA0, flag grid builds 0x34B288 on the same updates; crowd flash expiries 0x22961C on the same updates
// until the first re-armed timer (its value comes from the stream).
{
  const tracePath = '../local/ps2-capture/runs/visual-rng/full2.trace.json';
  if (!fs.existsSync(tracePath) || !fs.existsSync(runs + 'setpieces/full.bin')) console.log('world visual pass: trace or capture missing, skipped');
  else {
    const out = execFileSync(process.execPath, ['compare-ps2-capture.mjs', runs + 'setpieces/full.bin', '--pad', '--sync-rng', '--zoe', '--event'], { cwd: new URL('.', import.meta.url).pathname, env: { ...process.env, STAGE_WORLD: '1', VISUAL_LOG: '1' }, maxBuffer: 1 << 28, stdio: ['ignore', 'pipe', 'ignore'] }).toString();
    const v = JSON.parse(out).visualLog, web = new Map([1, 2, 3, 4, 5, 6].map((k) => [k, []])); // 6 = snowfall wind timers (0x2E5030) on the gp+0xA0C LCG, not this trace's stream
    for (let i = 0; i < v.length; i += 2) web.get(v[i + 1]).push(v[i]);
    const trace = JSON.parse(fs.readFileSync(tracePath, 'utf8'));
    const entries = Array.isArray(trace.entries) ? trace.entries : JSON.parse(trace.entries.replace(/'/g, '"'));
    const first = entries[0].tick, last = entries.at(-1).tick, ps2 = (ra) => entries.filter((e) => e.ra === ra).map((e) => e.tick);
    const inTrace = (list, shift) => list.map((t) => t + shift).filter((t) => t >= first + shift && t <= last); // the trace starts at update `first` (its post-rider draws are labelled first + 1)
    assert.deepEqual(inTrace(web.get(1), 1), ps2(0x34c71c), 'flag wind draw updates');
    assert.deepEqual(inTrace(web.get(2), 1), ps2(0x2f3be8), 'camera splash draw updates');
    assert.deepEqual(inTrace(web.get(3), 1), ps2(0x390ca0), 'lightning draw updates');
    assert.deepEqual(inTrace(web.get(5), 0).flatMap((t) => [t, t, t, t]), ps2(0x34b288), 'flag grid build updates (4 draws each)');
    const crowdWeb = inTrace(web.get(4), 1), crowdPs2 = ps2(0x22961c); let same = 0; while (same < crowdWeb.length && crowdWeb[same] === crowdPs2[same]) same++;
    assert.ok(same >= 8, `crowd flash expiries match only ${same}`);
    console.log(`world visual pass vs the PS2 0x4FF018 trace: flag wind ${web.get(1).length}, splash ${web.get(2).length}, lightning ${web.get(3).length} and ${web.get(5).length} flag grid builds on the PS2 updates; crowd expiries equal through the first ${same} (then the re-arm values follow the stream)`);
  }
}
