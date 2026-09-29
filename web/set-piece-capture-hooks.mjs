// TICK_HOOK module for compare-ps2-capture.mjs (web/test-stage-world.mjs): moving set pieces against PS2 savestate words.
// SET_PIECE_FIXTURE: {splines: {tick: {resource: {distance, t[3]}}}, multi: {tick: {resource: {distance, matrices}}}} (raw
// bits; local/reference/set-pieces-peak1/, written from the setpieces-ass1 / setpieces-aba1 kept snapshots).
// SET_PIECE_INJECT=tick:key[,tick:key] replays another rider's trigger (world_trigger_apply, owner * 8 + slot) after that tick: the
// single-rider replay lacks the R&B computer rider (Moby fired the train trigger 177419 at 4850 in setpieces-ass1/full).
import fs from 'node:fs';
export function create({ core }) {
  const fixture = JSON.parse(fs.readFileSync(process.env.SET_PIECE_FIXTURE, 'utf8'));
  const injects = (process.env.SET_PIECE_INJECT || '').split(',').filter(Boolean).map((x) => x.split(':').map(Number)); // tick:key[,tick:key]
  const r = { splineChecked: 0, splineExact: 0, multiChecked: 0, multiExact: 0, railChecked: 0, railExact: 0, teeterChecked: 0, teeterExact: 0, setPieceBad: [] };
  // RAIL_FIXTURE (tools/export_rail_modifier_snapshots.py): the run-time RailModifiers (bounds, rail, node, rest inverse) and the
  // location teeters (AnimTeeter +0x00..+0x40) of each kept savestate against web/rail_dynamic.inc at the same tick.
  const rails = process.env.RAIL_FIXTURE ? JSON.parse(fs.readFileSync(process.env.RAIL_FIXTURE, 'utf8')).snapshots : null;
  return {
    tick({ tick }) {
      for (const [injectTick, injectKey] of injects) if (tick === injectTick) { // as web/ai-racers.js replays another core: trigger launch, then its events 4 and 6
        core._world_trigger_apply(injectKey);
        for (const e of [[4, 1, injectKey >>> 3], [6, 2, injectKey >>> 3, 0]]) { const q = core._malloc(4 * e.length); new Uint32Array(core.HEAPU8.buffer, q, e.length).set(e); core._world_event_apply(q); core._free(q); }
      }
      const U = new Uint32Array(core.HEAPU8.buffer), F = new Float32Array(core.HEAPU8.buffer);
      const splines = fixture.splines?.[tick];
      if (splines) {
        const p = core._set_piece_splines() >> 2, n = F[p], web = new Map();
        for (let k = 0; k < n; k++) { const o = p + 2 + 7 * k; web.set(F[o], [U[o + 3], U[o + 4], U[o + 5], U[o + 6]]); }
        for (const [res, w] of Object.entries(splines)) { r.splineChecked++; const x = web.get(+res);
          if (x && x[0] === w.distance && w.t.every((v, c) => v === x[1 + c])) r.splineExact++; else r.setPieceBad.push(['spline', tick, +res]); }
      }
      const snap = rails?.[tick];
      if (snap && core._rail_dynamic_bits) {
        let i = core._rail_dynamic_bits() >> 2; const n = U[i++], web = new Map();
        for (let k = 0; k < n; k++) { web.set(`${U[i]}:${U[i + 1]}`, Array.from(U.subarray(i, i + 27))); i += 27; }
        for (const x of snap.rails) { r.railChecked++; const w = web.get(`${x.owner}:${x.packed}`);
          const want = [x.owner, x.packed, x.node, ...x.bounds, ...x.inverse];
          if (w && want.every((v, j) => (v >>> 0) === w[j])) r.railExact++; else r.setPieceBad.push(['rail', tick, x.owner, x.packed, w ? want.map((v, j) => ((v >>> 0) === w[j] ? null : [j, (v >>> 0).toString(16), w[j].toString(16)])).filter(Boolean).slice(0, 6) : 'missing']); web.delete(`${x.owner}:${x.packed}`); }
        for (const k of web.keys()) r.setPieceBad.push(['rail-extra', tick, k]);
        let t = core._rail_location_teeters() >> 2; const tn = U[t++], teeters = new Map();
        for (let k = 0; k < tn; k++) { teeters.set(U[t], { words: Array.from(U.subarray(t + 1, t + 18)), rails: U[t + 18] }); t += 19; }
        for (const x of snap.teeters) { r.teeterChecked++; const w = teeters.get(x.resource);
          if (w && w.rails && x.words.every((v, j) => (v >>> 0) === w.words[j])) r.teeterExact++; else r.setPieceBad.push(['teeter', tick, x.resource, w ? x.words.findIndex((v, j) => (v >>> 0) !== w.words[j]) : 'missing']); }
        for (const [res, w] of teeters) if (w.rails && !snap.teeters.some((x) => x.resource === res)) r.setPieceBad.push(['teeter-extra', tick, res]);
      }
      const multi = fixture.multi?.[tick];
      if (multi) {
        let i = core._set_piece_multi_bits() >> 2; const n = U[i++], web = new Map();
        for (let k = 0; k < n; k++) { const res = U[i], active = U[i + 1], cars = U[i + 2], distance = U[i + 3]; i += 4; const ms = []; for (let j = 0; j < cars; j++) { ms.push(Array.from(U.subarray(i, i + 16))); i += 16; } web.set(res, { active, distance, ms }); }
        for (const [res, w] of Object.entries(multi)) { r.multiChecked++; const x = web.get(+res);
          if (x && x.active && x.distance === w.distance && w.matrices.every((m, k) => m.every((v, j) => v === x.ms[k]?.[j]))) r.multiExact++; else r.setPieceBad.push(['multi', tick, +res]); }
        for (const [res, x] of web) if (x.active && !multi[res]) r.setPieceBad.push(['multi-extra', tick, res]);
      }
    },
    summary() { return r; },
  };
}
