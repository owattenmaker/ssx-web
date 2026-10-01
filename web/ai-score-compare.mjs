// TICK_HOOK for compare-ai-capture.mjs: computer riders' score objects against the capture's --watch windows.
// A capture built with `--ai-state --watch SCORE:0x1d0`, where SCORE is a computer rider's score object *(rider+0x790)
// (the ai_state actor window holds +0x790), records that object every tick. Each such watch is matched to its rider's slot
// and compared word for word with that rider's core (score_object_dump), as compare-ai-capture.mjs does the human's.
// Summary: aiScore = [{ slot, character, address, ticks, exact, first }].
const SCORE_WORDS = 0x1d0 / 4;
// The words compare-ai-capture.mjs skips for the human too (pointers and per-frame scratch).
const SCORE_SKIP = new Set([0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1cc]);
export function create({ racers, dv, RECORD, captureManifest }) {
  const layout = captureManifest.layout || {}, ai = layout.ai_state, watches = layout.watches || [];
  if (!ai || !watches.length) return { tick() {}, summary: () => ({ aiScore: [] }) };
  // Each watch window's offset inside the record (windows are packed from watch_offset in order).
  let offset = layout.watch_offset;
  const slots = [];
  for (const w of watches) {
    const address = Number(w.address), length = Number(w.length), at = offset;
    offset += length;
    if (length < 0x1d0) continue;
    for (let k = 0; k < ai.slots; k++) {
      const pointer = dv.getUint32(ai.base + k * ai.stride + ai.slot_fields.actor_000_b40 + 0x790, true);
      if (pointer === address) slots.push({ slot: k, at, address, ticks: 0, exact: 0, first: null });
    }
  }
  return {
    tick({ i }) {
      const base = (i + 1) * RECORD;
      if (base + RECORD > dv.byteLength) return;
      for (const s of slots) {
        const core = racers.npcs[s.slot]?.core;
        if (!core?._score_object_dump) continue;
        const web = new Uint32Array(core.HEAPU8.buffer, core._score_object_dump(), SCORE_WORDS);
        let bad = null;
        for (let k = 0; k < SCORE_WORDS && !bad; k++) {
          const off = 4 * k;
          if (SCORE_SKIP.has(off)) continue;
          const ps2 = dv.getUint32(base + s.at + off, true);
          if (ps2 !== web[k]) bad = { tick: dv.getUint32(base + 4, true), key: 'score+0x' + off.toString(16), web: web[k] | 0, ps2: ps2 | 0 };
        }
        s.ticks++;
        if (!bad) s.exact++;
        else if (!s.first) s.first = bad;
      }
    },
    summary: () => ({
      aiScore: slots.map((s) => ({ ...s, character: racers.npcs[s.slot]?.character ?? null, address: '0x' + s.address.toString(16) })),
    }),
  };
}
