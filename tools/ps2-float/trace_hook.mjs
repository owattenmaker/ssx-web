// compare-ps2-capture.mjs TICK_HOOK for the matcher (docs/ps2-float.md "Matcher"): with a trace core
// (tools/ps2-float/make_swap_tree.py --trace), records every arithmetic helper call the port makes while stepping
// tick PS2_MATCH_TICK and writes them to PS2_MATCH_OUT (names without TRACE: the comparer strips *TRACE* from its child passes) as JSON: {tick, sites: [...], entries: [[site, op, a, b, r], ...]}.
import fs from 'node:fs';

export function create({ core }) {
  const target = Number(process.env.PS2_MATCH_TICK);
  const out = process.env.PS2_MATCH_OUT;
  if (!core._ps2_trace_begin) {
    throw new Error('trace_hook.mjs needs a trace core (make_swap_tree.py --trace)');
  }
  // Recording runs from here to the end of each tick before the target, and is cleared at each of those ends, so what
  // remains at the target's end is exactly that tick.
  core._ps2_trace_begin();
  let done = false;
  return {
    tick({ tick }) {
      // The comparer's RNG-alignment child passes run the hook too, without PS2_MATCH_OUT: they record nothing.
      if (done || !out) {
        return;
      }
      if (tick < target) {
        core._ps2_trace_begin();
        return;
      }
      core._ps2_trace_end();
      const count = core._ps2_trace_count();
      const words = new Uint32Array(core.HEAPU8.buffer, core._ps2_trace_entries(), count * 5);
      const entries = [];
      for (let k = 0; k < count; k++) {
        entries.push(Array.from(words.subarray(k * 5, k * 5 + 5)));
      }
      const sites = [];
      for (let k = 0; k < core._ps2_trace_site_count(); k++) {
        sites.push(core.UTF8ToString ? core.UTF8ToString(core._ps2_trace_site(k)) : readString(core, core._ps2_trace_site(k)));
      }
      fs.writeFileSync(out, JSON.stringify({ tick, sites, entries }));
      done = true;
    },
    summary() {
      return { traceTick: target, traceWritten: done };
    },
  };
}

function readString(core, pointer) {
  let end = pointer;
  while (core.HEAPU8[end]) {
    end++;
  }
  return Buffer.from(core.HEAPU8.subarray(pointer, end)).toString('utf8');
}
