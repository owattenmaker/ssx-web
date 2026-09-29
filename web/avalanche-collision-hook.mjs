// TICK_HOOK module for compare-ps2-capture.mjs (web/test-avalanche-collision.mjs): the avalanche pieces' Object entities
// (web/avalanche_gameplay.inc "Pieces' collision", core export avalanche_entities) against PS2 savestate words.
// AVALANCHE_FIXTURE: {states: {tick: {resource: {flags, radius, low[4], high[4], matrix[16]}}}} (raw bits: instance +8 and the
// AvaSpline +0x30, +0x10 / +0x20, +0x40 of each piece, read from the kept savestates by web/test-avalanche-collision.mjs).
// Instance flag bit 0x200 (renderer bookkeeping) is not modelled.
import fs from 'node:fs';
export function create({ core }) {
  const fixture = JSON.parse(fs.readFileSync(process.env.AVALANCHE_FIXTURE, 'utf8')).states;
  const r = { avalancheChecked: 0, avalancheExact: 0, avalancheBad: [] };
  return {
    tick({ tick }) {
      const want = fixture[tick];
      if (!want) return;
      const U = new Uint32Array(core.HEAPU8.buffer); let p = core._avalanche_entities() >> 2; const n = U[p++], web = new Map();
      for (let k = 0; k < n; k++, p += 28) web.set(U[p], { flags: U[p + 1], spline: U[p + 2], radius: U[p + 3], low: Array.from(U.subarray(p + 4, p + 8)), high: Array.from(U.subarray(p + 8, p + 12)), matrix: Array.from(U.subarray(p + 12, p + 28)) });
      for (const [res, w] of Object.entries(want)) {
        r.avalancheChecked++; const x = web.get(+res);
        const bad = !x ? 'no entity' : !x.spline ? 'no AvaSpline' : ((x.flags & ~0x200) >>> 0) !== ((w.flags & ~0x200) >>> 0) ? `flags ${x.flags.toString(16)} ps2 ${w.flags.toString(16)}`
          : x.radius !== w.radius ? 'radius' : !w.low.every((v, c) => v === x.low[c]) ? 'bounds min' : !w.high.every((v, c) => v === x.high[c]) ? 'bounds max'
          : w.matrix.findIndex((v, c) => v !== x.matrix[c]) >= 0 ? `matrix[${w.matrix.findIndex((v, c) => v !== x.matrix[c])}]` : null;
        if (bad) r.avalancheBad.push([+tick, +res, bad]); else r.avalancheExact++;
      }
    },
    summary() { return r; },
  };
}
