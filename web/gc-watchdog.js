// pv gcWatchdog: WebKit's JavaScriptCore can stop running full garbage collections for the rest of the page's life
// (docs/mobile.md "Hangs": the WebKit memory runaway). From then on only eden collections run, and everything only a full
// collection frees piles up: the frames' old-generation garbage, dead cores and buffers, jettisoned JIT code, GPU and audio
// wrappers. WebKit Malloc grows +3-8 MB/s, JIT code 20 -> 360 MB, the tab reaches 5-8 GB in 25 min (a phone kills it in minutes).
//
// Why (JSC heap/Heap.cpp, GCActivityCallback.cpp): the full-GC timer is armed with lastFullGCLength / timeSlice(bytes x deathRate),
// deathRate = (sizeBefore - sizeAfter) / sizeBefore of the last full collection, 0 when sizeAfter >= sizeBefore. Extra memory
// (ArrayBuffers, wasm memories) allocated while a concurrent full collection runs is reported as visited
// (reportExtraMemoryAllocatedPossiblyFromAlreadyMarkedCell) but its allocation count is reset at the end of that collection, so a
// load that allocates big buffers during a full collection can end it with sizeAfter > sizeBefore: deathRate 0, the timer's delay is
// infinite and it is never armed again. The eden timer's requests are Eden-scoped (they ignore m_shouldDoFullCollection), so a full
// collection then needs an allocation-limit collection, which normal play never reaches.
//
// Detection, from two kinds of marker (FinalizationRegistry):
// - old: an object held HOLD_MS (so it is old generation), then dropped; only a full collection frees it. Healthy WebKit frees
//   each 3-7 s after its drop.
// - young: an object dropped at once; any collection frees it. Young markers freed while old ones wait = eden collections run and
//   full ones do not. A page that allocates nothing (an idle pause) runs neither, and is not a stall.
// Stalled: an old marker has waited STALL_MS with nothing old freed, and at least MIN_EDENS young markers were freed meanwhile.
// STALL_MS is over twice the longest healthy gap between full collections measured (17 s over 10-min Peak 2 Races on a 64 GB Mac,
// JSC's Aggressive growth; 28 s seen by another agent); a stalled heap never has one again.
//
// Kick: one-page WebAssembly.Memory objects, one more each second while the stall lasts (held until it ends, at most KICK_MAX).
// Each takes a "fast memory" slot (a virtual reservation, no physical pages; the count is process-wide, the game's cores hold some),
// and JSC's BufferMemoryManager asks for a full collection once half the slots are in use (runtime/BufferMemoryHandle.cpp
// tryAllocateFastMemory: SuccessAndNotifyMemoryPressure -> collectAsync(Full); maxNumWasmFastMemories is 8 with a large
// gigacage, else 3). One at a time stops at the first that crosses the threshold, short of the no-slot-left case (collectSync(Full)
// on the main thread). That collection has a real death rate, so the timer runs again; it also frees the kick's memories.
// Measured (Mac WebKit, docs/mobile.md): in a fresh page 3 memories do nothing, the 4th gives a full collection ~100 ms later; on a
// live runaway Memories brought full collections back at once (footprint 2106 -> 1548 MB, JIT 60 -> 23 MB) and they kept running
// on their own, where 2 x 1 GB and 2 x 4 GB ArrayBuffers did nothing.
// The kick waits up to SAFE_WAIT_MS for isSafe() (a load screen, a pause, a cutscene), then goes anyway.
// Only in JavaScriptCore (V8 / SpiderMonkey have no such timer; there the watchdog does not start). QA: window.__gcWatchdog.
const MARK_MS = 1000, HOLD_MS = 3000, STALL_MS = 40000, MIN_EDENS = 5, COOLDOWN_MS = 30000, SAFE_WAIT_MS = 5000, KICK_MAX = 6;

export function isJavaScriptCore(nav = globalThis.navigator) {
  const ua = String(nav?.userAgent || '');
  if (/(CriOS|FxiOS|EdgiOS|OPiOS)\//.test(ua)) return true;   // every iOS browser is WebKit
  return /AppleWebKit\//.test(ua) && !/(Chrome|Chromium|Edg|OPR|Android)\//.test(ua);
}

// one more one-page memory for the kick (null when the engine refuses it)
export function kickMemory() { try { return new WebAssembly.Memory({ initial: 1 }); } catch { return null; } }

let started = null;
export function startGcWatchdog(opts = {}) { return (started ??= createGcWatchdog(opts)); }
// one watchdog; setInterval / FinalizationRegistry / requestAnimationFrame from `env` (tests), else the page's
export function createGcWatchdog({ isSafe = () => false, now = () => performance.now(), makeMemory = kickMemory, log = (...a) => console.info(...a), env = globalThis } = {}) {
  const Registry = env.FinalizationRegistry, every = env.setInterval?.bind(env), raf = env.requestAnimationFrame?.bind(env);
  if (typeof Registry !== 'function' || typeof WeakRef !== 'function' || typeof WebAssembly?.Memory !== 'function') return null;
  const state = { old: 0, oldFreed: 0, youngFreed: 0, lastOldFreedAt: now(), youngAtLastOld: 0, kicks: 0, lastKickAt: -Infinity, stallSince: null, episode: null, stalls: [] };
  const waiting = new Set(), holding = [];
  let held = [];   // the running kick's memories (strong until the stall ends)
  const reg = new Registry((id) => {
    if (id < 0) { state.youngFreed++; return; }
    waiting.delete(id); state.oldFreed++; state.lastOldFreedAt = now(); state.youngAtLastOld = state.youngFreed;
  });
  const safe = () => { try { return !!isSafe(); } catch { return false; } };
  let seq = 0, gapMax = 0, gapUntil = 0, lastFrame = 0;
  const frame = (t) => {
    if (lastFrame) gapMax = Math.max(gapMax, t - lastFrame);
    lastFrame = t;
    if (t < gapUntil) raf(frame);
    else {
      const s = state.stalls.at(-1);
      if (s) s.maxFrameMs = Math.round(gapMax);
      lastFrame = 0;
    }
  };
  // the longest frame from a kick to 3 s after it
  const watchFrames = (t) => { const idle = gapUntil <= t; gapUntil = t + 3000; if (idle) { gapMax = 0; if (typeof raf === 'function') raf(frame); } };
  const tick = () => {
    const t = now();
    reg.register({ young: t }, -(++seq));
    holding.push({ t, id: ++seq });
    while (holding.length && t - holding[0].t >= HOLD_MS) { const o = holding.shift(); waiting.add(o.id); reg.register(o, o.id); state.old++; }
    const ep = state.episode;
    if (ep && state.lastOldFreedAt > ep.at) {   // the kick worked: an old marker was freed after it began
      ep.recoveredMs = Math.round(state.lastOldFreedAt - ep.at); held = []; state.episode = null;
      log(`gc-watchdog: full collections back after ${ep.memories} one-page memories (${ep.recoveredMs} ms)`);
      return;
    }
    if (ep) {   // still stalled: one more memory, up to KICK_MAX; then give up until the cooldown ends
      if (ep.memories < KICK_MAX) { const m = makeMemory(); if (m) held.push(m); ep.memories++; watchFrames(t); }
      else { held = []; state.episode = null; ep.gaveUp = true; log('gc-watchdog: no full collection after ' + ep.memories + ' memories'); }
      return;
    }
    const stalled = waiting.size > 0 && t - state.lastOldFreedAt > STALL_MS && state.youngFreed - state.youngAtLastOld >= MIN_EDENS;
    if (!stalled) { state.stallSince = null; return; }
    state.stallSince ??= t;
    if (t - state.lastKickAt < COOLDOWN_MS || (!safe() && t - state.stallSince < SAFE_WAIT_MS)) return;
    state.lastKickAt = t; state.kicks++; state.stallSince = null;
    const m = makeMemory(); if (m) held.push(m); watchFrames(t);
    state.episode = { at: Math.round(t), sinceFreedMs: Math.round(t - state.lastOldFreedAt), waiting: waiting.size, memories: 1, safe: safe() };
    state.stalls.push(state.episode); if (state.stalls.length > 20) state.stalls.shift();
    log(`gc-watchdog: no full collection for ${Math.round((t - state.lastOldFreedAt) / 1000)} s; asking for one`);
  };
  // Before a new core is instantiated (main.js newCore): drop the running kick's memories so they can never hold a fast-memory slot the
  // core needs (JSC has few: 3 on devices without the large gigacage, i.e. iPhones). Dropped, unreferenced memories are fine: with no
  // slot left, BufferMemoryManager runs a synchronous full collection and reclaims them; only held ones would push the core onto a
  // bounds-checked memory for the whole session. The episode ends; the stall check starts over.
  state.release = () => { if (!held.length && !state.episode) return false; held = []; if (state.episode) { state.episode.released = true; state.episode = null; } return true; };
  state.timer = every(tick, MARK_MS);
  return state;
}
