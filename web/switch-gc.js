// pv switchGC (memory, docs/mobile.md "Load spikes"): the previous course's core is collected before the next one is made.
//
// Each course runs on a fresh core instance with its own wasm memory (128 MB and up, docs/course-switch.md). unloadCourse drops
// every reference to the old one, but JavaScriptCore frees a wasm memory only in a full collection, and that often came seconds
// into the next load: WebKit (phone heap policy) had the old course's core, a lazily loaded boot course's core and the new one
// resident together at an event load (WebAssembly Memory 371 MB, WebContent 1602-1633 MB; the new core alone: 128 MB).
// Chrome frees them before the next load's first frame.
//
// Before the new core is instantiated, while an earlier core's memory is still alive, one-page WebAssembly.Memory objects are made
// one at a time (the gc-watchdog kick, web/gc-watchdog.js: JSC's BufferMemoryManager asks for a full collection once half of its
// fast-memory slots are in use) until the earlier memories are gone, a full collection has run without freeing them (something
// still holds them: waiting longer does not help), or timeoutMs has passed. The kick's memories are dropped right after (the next
// collection frees them). Only in JavaScriptCore; the caller decides (pv switchGC). Nothing in the simulation changes: the new core
// is made exactly as before, only later by the wait (measured in docs/mobile.md).
//
// Liveness comes from FinalizationRegistry callbacks, never from WeakRef.deref() (a deref during a concurrent collection keeps its
// target for that cycle). "A full collection ran" = an aged marker was freed: markers are held AGE_MS (so they are old generation,
// which only a full collection frees), then kept until a wait releases them.
import { kickMemory } from './gc-watchdog.js';

const AGE_MS = 2000, MARK_EVERY_MS = 1000, AGED_MAX = 3;

// the cores' wasm memories made (tracked) and not yet collected, and the full collections seen
export function createCoreTracker({ Registry = globalThis.FinalizationRegistry, every = globalThis.setInterval?.bind(globalThis), now = () => performance.now() } = {}) {
  const live = new Set(); let seq = 0, fulls = 0, started = false;
  const ok = typeof Registry === 'function';
  const reg = ok ? new Registry((id) => live.delete(id)) : null, markers = ok ? new Registry(() => { fulls++; }) : null;
  const young = [], aged = [];
  const mark = () => { const t = now(); young.push({ t }); while (young.length && t - young[0].t >= AGE_MS) { aged.push(young.shift()); if (aged.length > AGED_MAX) aged.shift(); } };
  return {
    track(memory) {
      if (!ok || !memory) return;
      if (!started && typeof every === 'function') {
        started = true;
        every(mark, MARK_EVERY_MS);
      }
      const id = ++seq;
      live.add(id);
      reg.register(memory, id);
    },
    get live() {
      return live.size;
    },
    get fulls() {
      return fulls;
    },
    // drop the aged markers (only a full collection frees them): the count to wait past, or null when none has aged yet
    release() {
      if (!aged.length) return null;
      const at = fulls;
      for (const m of aged.splice(0)) markers.register(m, 0);
      return at;
    },
    mark // (tests)
  };
}
export const coreTracker = createCoreTracker();

// Resolves {alive (at the start), freed, left, full (a full collection seen), memories, ms}.
export async function collectBefore(tracker = coreTracker, { timeoutMs = 800, stepMs = 50, maxMemories = 6, makeMemory = kickMemory,
  sleep = (ms) => new Promise((r) => setTimeout(r, ms)), now = () => performance.now() } = {}) {
  const first = tracker.live, t0 = now();
  if (!first) return { alive: 0, freed: true, memories: 0, ms: 0 };
  const since = tracker.release();
  let held = [], made = 0, grace = false;
  try {
    for (;;) {
      if (made < maxMemories && !grace) { const m = makeMemory(); made++; if (m) held.push(m); }
      await sleep(stepMs);
      const full = since != null && tracker.fulls > since, out = now() - t0 >= timeoutMs;
      // after a full collection one more step: its other finalization callbacks (the memories') may come in a later task
      if (!tracker.live || out || (full && grace)) return { alive: first, freed: !tracker.live, left: tracker.live, full, memories: made, ms: Math.round(now() - t0) };
      if (full) grace = true;
    }
  } finally { held = null; }
}

// After a course goes live: one full collection now (the load's garbage and whatever the old course left), not when JSC's timer gets
// to it. In the first ride after a load none ran for 20-40 s (WebKit, both heap policies): the boot course's core and 300-400 MB of
// load garbage stayed resident until then (docs/mobile.md "Load spikes"). Resolves {full, memories, ms}; full = an aged marker freed.
export async function collectNow(tracker = coreTracker, { timeoutMs = 600, stepMs = 50, maxMemories = 6, makeMemory = kickMemory,
  sleep = (ms) => new Promise((r) => setTimeout(r, ms)), now = () => performance.now() } = {}) {
  const since = tracker.release(), t0 = now();
  if (since == null) return { full: false, memories: 0, ms: 0, noMarker: true };
  let held = [], made = 0;
  try {
    for (;;) {
      if (made < maxMemories) { const m = makeMemory(); made++; if (m) held.push(m); }
      await sleep(stepMs);
      const full = tracker.fulls > since;
      if (full || now() - t0 >= timeoutMs) return { full, memories: made, ms: Math.round(now() - t0) };
    }
  } finally { held = null; }
}
