// TICK_HOOK module for compare-ps2-capture.mjs (web/test-avalanche-collision.mjs): the avalanche pieces' Object entities
// (web/avalanche_gameplay.inc "Pieces' collision", core export avalanche_entities) against PS2 words: instance +8 (flags; the
// renderer bookkeeping bits 0x100 / 0x200 are not compared) and the AvaSpline +0x30 (radius), +0x10 / +0x20 (bounds), +0x40
// (matrix), bit for bit.
//   AVALANCHE_FIXTURE: {states: {tick: {resource: {flags, radius, low[4], high[4], matrix[16]}}}} (raw bits of kept savestates,
//     written by web/test-avalanche-collision.mjs), compared after the step of that tick;
//   AVALANCHE_WATCH=resource:modifier:instance,...: the capture's --watch windows of each piece (AvaSpline 0x90 bytes, instance
//     0x80 bytes), compared on every record that shows the AvaSpline: record i (the provider exit, before the riders) against the
//     port after the step of record i's command (its race_begin ran group 1 and the tumbler step).
import fs from 'node:fs';
export function create({ core, dv, RECORD, captureManifest }) {
  const fixture = process.env.AVALANCHE_FIXTURE ? JSON.parse(fs.readFileSync(process.env.AVALANCHE_FIXTURE, 'utf8')).states : {};
  const watch = (process.env.AVALANCHE_WATCH || '').split(',').filter(Boolean).map((x) => x.split(':').map(Number));
  const at = {}; { let off = captureManifest.layout.watch_offset ?? 0; for (const w of captureManifest.layout.watches || []) { at[Number(w.address)] = off; off += Number(w.length); } }
  for (const [, m, inst] of watch) if (at[m] === undefined || at[inst] === undefined) throw new Error(`AVALANCHE_WATCH: ${m.toString(16)} / ${inst.toString(16)} not watched`);
  const r = { avalancheChecked: 0, avalancheExact: 0, avalancheBad: [], trailsChecked: 0, trailsExact: 0, trailsBad: [] };
  // AVALANCHE_FIXTURE trails: {tick: {pool index: {words[130], ringA, ringB, colours}}} against avalanche_trails(): every word but the
  // ring pointers (+0x1A0 / +0x1A4 / +0x204), the GS packet words the draw 0x371688 rewrites (+0x1E8 blend, +0x1EC priority 7 << 5, the +0x1F4 halfword)
  // and the births' seeds (ring B w: draws of the presentation stream, which this solo replay does not follow).
  const trailFixture = process.env.AVALANCHE_FIXTURE ? (JSON.parse(fs.readFileSync(process.env.AVALANCHE_FIXTURE, 'utf8')).trails ?? {}) : {};
  const trails = () => {
    const U = new Uint32Array(core.HEAPU8.buffer); let p = core._avalanche_trails() >> 2; const count = U[p++], out = new Map();
    for (let k = 0; k < count; k++) { const pool = U[p + 1], words = Array.from(U.subarray(p + 2, p + 132)); p += 132; const n = U[p++];
      const ringA = Array.from(U.subarray(p, p + 4 * n)); p += 4 * n; const ringB = Array.from(U.subarray(p, p + 4 * n)); p += 4 * n; const colours = Array.from(U.subarray(p, p + n)); p += n;
      out.set(pool, { words, ringA, ringB, colours }); }
    return out;
  };
  const snapshot = () => {
    const U = new Uint32Array(core.HEAPU8.buffer); let p = core._avalanche_entities() >> 2; const n = U[p++], web = new Map();
    for (let k = 0; k < n; k++, p += 28) web.set(U[p], { flags: U[p + 1], spline: U[p + 2], radius: U[p + 3], low: Array.from(U.subarray(p + 4, p + 8)), high: Array.from(U.subarray(p + 8, p + 12)), matrix: Array.from(U.subarray(p + 12, p + 28)) });
    return web;
  };
  const compare = (tick, res, w, x) => {
    r.avalancheChecked++;
    const bad = !x ? 'no entity' : !x.spline ? 'no AvaSpline' : ((x.flags & ~0x300) >>> 0) !== ((w.flags & ~0x300) >>> 0) ? `flags ${x.flags.toString(16)} ps2 ${w.flags.toString(16)}`
      : x.radius !== w.radius ? 'radius' : !w.low.every((v, c) => v === x.low[c]) ? 'bounds min' : !w.high.every((v, c) => v === x.high[c]) ? 'bounds max'
      : w.matrix.findIndex((v, c) => v !== x.matrix[c]) >= 0 ? `matrix[${w.matrix.findIndex((v, c) => v !== x.matrix[c])}]` : null;
    if (bad) { if (r.avalancheBad.length < 50) r.avalancheBad.push([+tick, +res, bad]); if (process.env.AVALANCHE_DEBUG) console.error('AVDBG', tick, res, JSON.stringify({ web: x, ps2: w })); } else r.avalancheExact++;
  };
  return {
    tick({ i, tick }) {
      const wantTrails = trailFixture[tick];
      if (wantTrails && core._avalanche_trails) {
        const web = trails();
        for (const [pool, w] of Object.entries(wantTrails)) {
          r.trailsChecked++; const x = web.get(+pool);
          const word = !x ? -1 : w.words.findIndex((v, k) => ![0x1A0, 0x1A4, 0x1E8, 0x1EC, 0x1F4, 0x204].includes(4 * k) && v !== x.words[k]);
          const bad = !x ? 'no emitter' : word >= 0 ? `+${(4 * word).toString(16)} ps2 ${w.words[word].toString(16)} port ${x.words[word].toString(16)}`
            : w.ringA.length !== x.ringA.length ? 'ring size' : w.ringA.some((v, k) => v !== x.ringA[k]) ? 'ring A' : w.ringB.some((v, k) => k % 4 !== 3 && v !== x.ringB[k]) ? 'ring B'
            : w.colours.some((v, k) => v !== x.colours[k]) ? 'colours' : null;
          if (bad) { if (r.trailsBad.length < 50) r.trailsBad.push([+tick, +pool, bad]); } else r.trailsExact++;
        }
      }
      const want = fixture[tick];
      if (!want && !watch.length) return;
      const web = snapshot();
      if (want) for (const [res, w] of Object.entries(want)) compare(tick, res, w, web.get(+res));
      for (const [res, m, inst] of watch) {
        const word = (address, o) => dv.getUint32(i * RECORD + at[address] + o, true), words = (o, n) => Array.from({ length: n }, (_, k) => word(m, o + 4 * k));
        if (word(m, 0) !== 0x48F338) continue; // no AvaSpline there yet (the window holds free heap before the trigger)
        compare(dv.getInt32(i * RECORD + 4, true), res, { flags: word(inst, 8), radius: word(m, 0x30), low: words(0x10, 4), high: words(0x20, 4), matrix: words(0x40, 16) }, web.get(res));
      }
    },
    summary() { return r; },
  };
}
