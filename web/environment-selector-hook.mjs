// TICK_HOOK module for compare-ai-capture.mjs / compare-ps2-capture.mjs (web/test-tunnel-lighting.mjs): the human's environment
// selector (web/environment_bridge.cpp environment_selector, stage builtin 74's tunnels) against the capture's watch of the
// human's environment block 0x4FA370 (+0x24), bit for bit, on every record (the port after record i's command vs record i + 1).
export function create({ core, dv, RECORD, captureManifest }) {
  let off = captureManifest.layout.watch_offset, at = -1;
  for (const w of captureManifest.layout.watches || []) { if (Number(w.address) === 0x4FA370) at = off; off += Number(w.length); }
  if (at < 0) throw new Error('environment-selector-hook: the capture does not watch 0x4FA370');
  const r = { selectorChecked: 0, selectorExact: 0, selectorNonzero: 0, selectorFirst: null };
  return {
    tick({ i, tick }) {
      if (!core._environment_selector) return;
      const ps2 = dv.getUint32((i + 1) * RECORD + at + 0x24, true), web = new Uint32Array(new Float32Array([core._environment_selector()]).buffer)[0];
      r.selectorChecked++; if (ps2) r.selectorNonzero++;
      if (ps2 === web) r.selectorExact++; else if (!r.selectorFirst) r.selectorFirst = { tick, ps2: new Float32Array(new Uint32Array([ps2]).buffer)[0], web: core._environment_selector() };
    },
    summary() { return r; },
  };
}
