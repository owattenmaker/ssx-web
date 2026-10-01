// pv eventReturnInWorld (b): an in-world event's two snapshots (docs/replay.md §2a), the port's form of the PS2's replay snapshots:
// slot 0 at the countdown (0x26D818 at the gate open: the replay's restart, R1 / L1) and slot 1 at the results time (R+0x3D0, saved
// at the auto replay's start: the results' Transport puts it back, 0x2706F0). Each rider context saves and restores its own state
// (core snapshot_save / snapshot_restore, web/world_snapshot.hpp); the computer riders' orchestrator its JS state (web/ai-racers.js
// saveState / restoreState). The buffers are made once at the in-world attach (snapshotAttach) and reused for every event.
// main.js (the page) and compare-ai-capture.mjs (REPLAY_PROBE, the gates) both run these.
export const SNAPSHOT_COUNTDOWN = 0, SNAPSHOT_RESULTS = 1;
export const SNAPSHOT_CORE_EXPORTS = [
  '_snapshot_init',
  '_snapshot_save',
  '_snapshot_restore',
  '_snapshot_keep_changed_count',
  '_snapshot_keep_changed_name',
  '_snapshot_bytes',
  '_snapshot_qa',
  '_snapshot_missing',
  '_snapshot_failed_hook',
  '_snapshot_check',
  '_snapshot_clear'
];
const coresOf = (human, racers) => [human, ...(racers?.npcs ?? []).map((n) => n.core)];
const text = (core, p) => { if (!p) return ''; let e = p; while (core.HEAPU8[e]) e++; return new TextDecoder().decode(core.HEAPU8.subarray(p, e)); };
export function requireSnapshotCore(core) {
  const missing = SNAPSHOT_CORE_EXPORTS.filter((f) => typeof core?.[f] !== 'function');
  if (missing.length) throw new Error(`This core has no ${missing.map((f) => f.slice(1)).join(', ')}`);
}
// The in-world attach: every context's buffers and holders (a one-time growth). qa: the kept tables' checks at every save / restore.
export function snapshotAttach({ human, racers = null, qa = false }) {
  for (const c of coresOf(human, racers)) {
    requireSnapshotCore(c); c._snapshot_qa(qa ? 1 : 0);
    const missing = c._snapshot_init();
    if (missing)
      throw new Error(
        `snapshot: ${Array.from({ length: missing }, (_, k) => text(c, c._snapshot_missing(k))).join(', ')} can be neither copied nor kept (web/snapshot-policy.mjs)`
      );
  }
}
// Returns the JS state that goes with the slot (the caller keeps it for the restore).
export function snapshotSave(slot, { human, racers = null }) {
  for (const c of coresOf(human, racers)) if (!c._snapshot_save(slot)) throw new Error(`snapshot: save ${slot} before the attach`);
  return { racers: racers ? racers.saveState() : null };
}
// Every context is checked first (core snapshot_check changes nothing), so a restore that cannot run leaves the state as it is.
// The countdown's save (the live start): slot 0, and slot 1 overwritten with the same state. Slot 1 held the last event's results
// time until then, and its copies' references (a rider's rig and clips through its animation graph, entity geometry) kept that
// event's data alive through this race; a copy into the same buffers releases them without an allocation. The results-time save
// overwrites slot 1 again at the replay's start.
export function snapshotCountdown({ human, racers = null }) {
  const state = snapshotSave(SNAPSHOT_COUNTDOWN, { human, racers });
  for (const c of coresOf(human, racers)) c._snapshot_save(SNAPSHOT_RESULTS);
  return state;
}
export function snapshotRestore(slot, state, { human, racers = null }) {
  const cores = coresOf(human, racers);
  for (const c of cores) {
    const r = c._snapshot_check(slot);
    if (r === 1) throw new Error(`snapshot: restore ${slot} without its save`);
    if (r === 2) {
      const n = c._snapshot_keep_changed_count();
      throw new Error(
        `snapshot: kept tables changed since the save: ${Array.from({ length: n }, (_, k) => text(c, c._snapshot_keep_changed_name(k))).join(', ')} (web/snapshot-policy.mjs)`
      );
    }
    if (r === 3) throw new Error(`snapshot: ${text(c, c._snapshot_failed_hook())} no longer matches its save (web/world_snapshot.hpp hooks)`);
  }
  for (const c of cores) if (c._snapshot_restore(slot) !== 1) throw new Error(`snapshot: ${text(c, c._snapshot_failed_hook())} failed after its check`);
  if (racers && state?.racers) racers.restoreState(state.racers);
}
// The event's end (its riders leave): the rider contexts' copies go. They hold references to the riders' data (the rig and clips
// through the animation graph), which the next event's riders would otherwise load beside (a transient peak of the old and new
// riders: WASM memory 184 -> 221 MB once, measured on the page). The next countdown save makes them again (about 3 MB, in the freed memory).
export function snapshotReleaseRiders({ racers = null }) { for (const n of racers?.npcs ?? []) { n.core._snapshot_clear(SNAPSHOT_COUNTDOWN); n.core._snapshot_clear(SNAPSHOT_RESULTS); } }
export function snapshotBytes({ human, racers = null }) { return coresOf(human, racers).reduce((a, c) => a + c._snapshot_bytes(), 0); }
