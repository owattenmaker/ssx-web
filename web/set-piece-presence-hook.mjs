// TICK_HOOK module for compare-ps2-capture.mjs (web/test-set-piece-seeds.mjs): which Spline set pieces exist at each kept PS2
// savestate, against web/set_piece_gameplay.inc. SET_PIECE_PRESENCE: {splines: {tick: {resource: {kind, distance, t[3]}}},
// multi: {tick: {resource: {distance, matrices}}}} (raw bits; every entity whose first modifier is a SplineModifier 0x48F250,
// a PositionModifier 0x48F5F0 (kind 'Position') or a MultiSplineModifier 0x48F168).
//   PS2 SplineModifier: a launched piece (set_piece_splines, phase 1) with +0x3C distance and +0x90 translation bit-exact, or a
//     resident looping Spline (moving_instances key, presence only: its words are the load seed's).
//   PS2 PositionModifier: a piece frozen at its path end (phase 2).
//   Anything the port has at that tick and the PS2 has not is an extra (a raven kept flying after its section-leave destroy).
import fs from 'node:fs';
export function create({ core }) {
  const fix = JSON.parse(fs.readFileSync(process.env.SET_PIECE_PRESENCE, 'utf8'));
  const r = { presenceChecked: 0, presenceExact: 0, presenceLoops: 0, presenceBad: [], presenceMultiChecked: 0, presenceMultiExact: 0, presenceTimeline: [] };
  let last = '';
  const state = () => {
    const U = new Uint32Array(core.HEAPU8.buffer), F = new Float32Array(core.HEAPU8.buffer);
    const pieces = new Map(), lifts = new Map(), loops = new Set();
    { const p = core._set_piece_splines() >> 2, n = F[p]; for (let k = 0; k < n; k++) { const o = p + 2 + 7 * k; pieces.set(F[o], { phase: F[o + 1], distance: U[o + 3], t: [U[o + 4], U[o + 5], U[o + 6]] }); } }
    { let i = core._set_piece_multi_bits() >> 2; const n = U[i++];
      for (let k = 0; k < n; k++) { const res = U[i], active = U[i + 1], cars = U[i + 2], distance = U[i + 3]; i += 4; const ms = []; for (let j = 0; j < cars; j++) { ms.push(Array.from(U.subarray(i, i + 16))); i += 16; } lifts.set(res, { active, distance, ms }); } }
    { const p = core._moving_instances() >> 2, n = F[p]; // resident loops: the drawn moving keys that are neither pieces nor lift cars (a crashbag roller or an
      // AvaSpline follower would count as one too; the kept states of test-set-piece-seeds.mjs have none)
      for (let k = 0; k < n; k++) { const key = F[p + 1 + 17 * k]; if (!pieces.has(key) && !lifts.has(key % 1048576)) loops.add(key); } }
    return { pieces, lifts, loops };
  };
  return {
    tick({ tick }) {
      const s = state();
      const line = `pieces [${[...s.pieces].map(([k, v]) => `${k}:${v.phase}`).join(' ')}] loops [${[...s.loops].join(' ')}] multi [${[...s.lifts].filter(([, v]) => v.active).map(([k]) => k).join(' ')}]`;
      if (line !== last && r.presenceTimeline.length < 400) { r.presenceTimeline.push([tick, line]); last = line; }
      const want = fix.splines?.[tick];
      if (want) {
        for (const [res, w] of Object.entries(want)) {
          r.presenceChecked++; const x = s.pieces.get(+res);
          if (w.kind === 'Position') { if (x && x.phase === 2) r.presenceExact++; else r.presenceBad.push(['position', tick, +res, x ? `phase ${x.phase}` : 'none']); }
          else if (x) { if (x.phase === 1 && x.distance === w.distance && w.t.every((v, c) => v === x.t[c])) r.presenceExact++; else r.presenceBad.push(['piece', tick, +res, `phase ${x.phase} distance ${x.distance.toString(16)} ps2 ${w.distance.toString(16)}`]); }
          else if (s.loops.has(+res)) { r.presenceExact++; r.presenceLoops++; }
          else r.presenceBad.push(['missing', tick, +res, w.kind]);
        }
        for (const k of s.pieces.keys()) if (!want[k]) r.presenceBad.push(['extra-piece', tick, k]);
        for (const k of s.loops) if (!want[k]) r.presenceBad.push(['extra-loop', tick, k]);
      }
      const multi = fix.multi?.[tick];
      if (multi) {
        for (const [res, w] of Object.entries(multi)) { r.presenceMultiChecked++; const x = s.lifts.get(+res);
          if (x && x.active && x.distance === w.distance && w.matrices.every((m, k) => m.every((v, j) => v === x.ms[k]?.[j]))) r.presenceMultiExact++; else r.presenceBad.push(['multi', tick, +res]); }
        for (const [res, x] of s.lifts) if (x.active && !multi[res]) r.presenceBad.push(['multi-extra', tick, res]);
      }
    },
    summary() { return r; },
  };
}
