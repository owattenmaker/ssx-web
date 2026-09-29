// Freestyle event configuration of a course (docs/slopestyle-bigair.md): R&B slope style (ASS1) and Crow's Nest big air
// (ABA1). `/assets/<code>/freestyle-event.json` comes from tools/export_freestyle_event.py (the event's countdown-anchor
// savestate): game mode byte 0x535C12, handler index GMM+4 (0 = freestyle handler 0x47CFA0), freestyle kind GMM+8, the
// checkpoint-bonus list 0x4D33B8 and the time limit GMM+0x78. Courses without the file (races, the pipe) keep the core's
// defaults (no bonus list).

export async function loadFreestyleEvent(course, fetchImpl = globalThis.fetch) {
  if (!course?.root) return null;
  try { const r = await fetchImpl(course.root + 'freestyle-event.json'); return r.ok ? await r.json() : null; } catch { return null; }
}

// set_race_bonus(words, mode, handler, freestyleKind, flags): 112FB0 -> 10E558 -> 1194C0 in the human's OriginalRaceSession.
// Slope style (mode 1, kind 1) pays each crossed checkpoint as the 0x29 popup (+value points shown as "+60") and 0x2398E8 adds
// value x 60 ticks to the time limit GMM+0x78 (R&B: two +60 s checkpoints). Other modes never pay (10E558 checks mode 1).
export function applyFreestyleEvent(core, cfg) {
  if (!core?._set_race_bonus) return false;
  const words = new Int32Array(12);
  if (cfg) cfg.bonus_words.forEach((w, k) => { words[k] = w | 0; });
  const ptr = core._malloc(48); new Int32Array(core.HEAPU8.buffer, ptr, 12).set(words);
  if (cfg) core._set_race_bonus(ptr, cfg.game_mode, cfg.handler, cfg.freestyle_kind, cfg.global_flags >>> 0);
  else core._set_race_bonus(ptr, 0, 1, 0, 0); // race defaults (web/race_bridge.cpp)
  core._free(ptr);
  return !!cfg;
}

// The live limit (GMM+0x78 after checkpoint extensions) for the HUD clock 1EC3F8; falls back to the configured limit.
export function liveTimeLimit(core, fallback) {
  const v = core?._race_time_limit_now?.();
  return v > 0 ? v : fallback;
}
