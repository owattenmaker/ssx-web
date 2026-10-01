// New-career plane FX (planeFx; docs/presentation.md "New-career plane"): what the kind-7 channel-0 stage calls of
// #153 abc1_heli_arr_midway / #163 heli_arrb_<char>_midwayabc1 do besides the plane's LiveComp (web/cutscene-stage-sets.js).
// PS2 (tools/export_cutscene_sets.py, the ABC1 track-6 globals run through the stage VM):
//  - the engine: 0x0DE99225 (#153 t0) / 0x0EE53794 (#163 t0) = builtin 73 (297EB8: stop) then builtin 31 (297950: loop
//    script sound 201 on the plane instance: id >= 200 -> TRANSPORT bank sound 1, bus 5, volume 127, distance parameter
//    300 m (2A9988; 400 m only for a location record word 8), the voice at the instance's entity matrix, refreshed every
//    frame by 297FA0); their cleanup 0x07D196B4 (0x2807B0 at the step end) = builtin 73 (stop) + SetNodeState 1;
//  - the snow spray: 0x0D9F751E (#153 t250, #163 t0) belongs to ABC1 global program 2, not to the plane: builtin 25, a
//    Particle on ospreySpray_1001 (60 particles, continuous, texture 25 blend 1, rising radial puffs). The core runs it on
//    the location's stage globals (web/presentation_core.inc stage_global_call), and its effects advance with the script's
//    ticks (the PS2 entity pass runs under the NIS; the page's world is paused behind the cutscene). Its cleanup 0x0D905E74
//    (builtin 69 mode 0) runs there at the step end: a Particle's stop is a no-op, so the spray goes on (PS2 new-career
//    s3200..3330: the clouds under the jump).
import { SLOT } from './sfx.js';

export const NOSCRIPT = 0x049A9AD4;
const MAX_TICKS_PER_FRAME = 8;

// host: the cutscene host (core getter, audio = web/game-audio.js).
export function createPlaneFx(host) {
  let loop = null, position = null, acc = 0, calls = 0;
  const stats = { calls: 0, coreCalls: 0, loopStarts: 0, loopStops: 0, ticks: 0 };   // QA (window.__cutscenes.planeFx.stats)
  const coreCleanups = new Set(), loopCleanups = new Set();
  const core = () => host.core;
  const sfx = () => host.audio?.sfx || null;
  function stopLoop() { if (loop) stats.loopStops++; try { loop?.stop?.(0); } catch {} loop = null; }
  // The plane instance's entity matrix translation (source cm): the LiveComp's root node while it plays, else the instance.
  function planePosition(set, out) {
    const meta = set.meta, inst = meta.livecomp?.instances?.[0], m = inst && set.stage?.anim?.matrices?.(inst.resource);
    const p = m?.[0]?.[3] ?? meta.instances?.[0]?.position_cm; if (!p) return out;
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; return out;
  }
  function startLoop(set) {
    stopLoop();
    const id = set.meta?.sound_loop, s = sfx(); if (!(id >= 200) || !s?.bankOf?.(SLOT.TRANSPORT)?.bnk) return;
    position = planePosition(set, [0, 0, 0]);
    stats.loopStarts++;
    try { loop = s.play({ slot: SLOT.TRANSPORT, sound: id - 200, bus: 'UI', volume: 127, position, vanish: 300, tag: 'script' }) || null; } catch { loop = null; }
  }
  const api = {
    get active() { return !!loop || calls > 0; },
    get loop() { return loop; },
    stats,
    // A channel-0 event [symbol, cleanup] of a step with a staged set shown; owned: the set started a player with it.
    call(set, symbol, cleanup, owned) {
      if (!set) return;
      stats.calls++;
      const recorded = cleanup != null && (cleanup >>> 0) !== NOSCRIPT;
      if (owned) { startLoop(set); if (recorded) loopCleanups.add(cleanup >>> 0); return; }
      const c = core(), track = set.meta?.location_index;
      if (!c?._stage_global_call || !(track >= 0)) return;
      // the human rides in the plane under the drop NIS (0x2FEB24's 300 m rider test): the plane's position stands in for it
      const at = planePosition(set, [NaN, NaN, NaN]);
      if (c._stage_global_call(track, symbol >>> 0, at[0], at[1], at[2])) { calls++; stats.coreCalls++; if (recorded) coreCleanups.add(cleanup >>> 0); }
    },
    // 0x2807B0 at the step end: the recorded cleanups (the plane's: builtin 73, the engine stops; the spray's: in the core).
    stepEnd(set) {
      const c = core(), track = set?.meta?.location_index;
      if (c?._stage_global_call && track >= 0) for (const k of coreCleanups) { const at = planePosition(set, [NaN, NaN, NaN]); c._stage_global_call(track, k, at[0], at[1], at[2]); }
      coreCleanups.clear();
      if (loopCleanups.size) stopLoop();
      loopCleanups.clear();
    },
    // Script ticks elapsed this frame: the effects the calls built advance one entity pass per tick.
    advance(ticks) {
      if (!calls || !(ticks > 0)) return;
      const c = core(); if (!c?._stage_cutscene_tick) return;
      acc += ticks; let n = Math.floor(acc); acc -= n; n = Math.min(n, MAX_TICKS_PER_FRAME);
      for (let k = 0; k < n; k++) c._stage_cutscene_tick();
      stats.ticks += n;
    },
    // Per frame: the engine voice follows the plane (297FA0).
    frame(set) { if (loop && position && set) planePosition(set, position); },
    // The list ended or stopped: the voice goes, the run's entity pass owns the effects.
    end() { stopLoop(); loopCleanups.clear(); coreCleanups.clear(); acc = 0; if (calls) { calls = 0; core()?._stage_cutscene_end?.(); } },
  };
  return api;
}
