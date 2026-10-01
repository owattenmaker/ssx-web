// sliceLoad (docs/ctm-flow.md "Start and streaming"): the load screen's long core calls cut into small ones that end in the same
// core state, so the load screen keeps animating (the goal: no task much over ~50 ms on a phone-class CPU).
//   - the environment lattice (web/environment_bridge.cpp): init_environment with the globals only and the "streamed" store, then
//     the textures and patches in environment_add parts of 2 textures / 400 patches cut from the document's own text (numbers as
//     written; web/peak-world-batches.js environmentParts), environment.bin put in the core once. Texture ids are the package's indices
//     (every environment.json has id == index) and no package repeats a patch resource, so the streamed store answers every sample
//     exactly like the indexed one (core environment_load_hash, web/test-ctm-stream.mjs).
import { environmentParts } from './peak-world-batches.js';
import { createGuardedWorker, workerUrl } from './worker-guard.js';
import { preparePeakLocation } from './peak-world-prepare.js';
//   - the stage world (web/stage_world.inc): stage_world_part with the three documents' parts (web/peak-world-batches.js
//     stageWorldParts, cut from their own text; core stage_world_load_hash equal to init_stage_world's).
// The cutting (a scan of a few MB of text) runs in the peak-world worker (web/peak-world-prepare.js `cut`), not on the load screen.
const CUT_WORKER = workerUrl((Worker) => new Worker(new URL('./peak-world-worker.js', import.meta.url), { type: 'module' }));
async function cutOffThread(request, transfer = []) {
  const worker = createGuardedWorker({ name: 'peak-world', url: CUT_WORKER, local: () => preparePeakLocation, stallTimeoutMs: 60000 });
  try { return await worker.request(request, transfer); } finally { worker.terminate(); }
}
const absolute = (u) => (u ? new URL(u, globalThis.location?.href ?? 'http://localhost/').href : null);

// the next animation frame (the load screen draws in between; a message-channel yield let Chrome run the slices back to back
// without a frame: 325 ms frames at 4x CPU); a timer when frames stop (a hidden tab)
export const nextFrame = () => new Promise((r) => { let done = false; const go = () => { if (!done) { done = true; r(); } }; globalThis.requestAnimationFrame?.(go); setTimeout(go, 50); });

// A pause for a build that checks in every few ms (main.js asset opts.pause): a frame once `ms` of work ran since the last one.
export const framePause = (ms) => { const tick = stepper(nextFrame, ms); return tick; }; // ('frame': the event load's frame-filling budget)
// url: environment.json (cut from its own text off the main thread), bytes: environment.bin.
export async function initEnvironmentSliced(core, url, bytes, { yieldFn = nextFrame, budgetMs = 8 } = {}) {
  const { head, parts } = await cutOffThread({ cut: 'env', urls: { environment: absolute(url) } });
  const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const data = core._malloc(Math.max(1, bytes.length)); core.HEAPU8.set(bytes, data);
  try {
    { const p = put(head); try { core._init_environment(p, data, 0); } finally { core._free(p); } } // (no textures: no bytes to key its cache)
    let t0 = performance.now();
    for (const part of parts) {
      const p = put(part); try { core._environment_add(p, data, bytes.length); } finally { core._free(p); }
      if (performance.now() - t0 > budgetMs) { await yieldFn(); t0 = performance.now(); }
    }
  } finally { core._free(data); }
}
// urls: {particles, livecomp?, stage?}. Returns what init_stage_world returns (the loaded instance count).
export async function initStageWorldSliced(core, urls, { yieldFn = nextFrame, budgetMs = 8 } = {}) {
  const { parts } = await cutOffThread({ cut: 'stage', urls: { particles: absolute(urls.particles), livecomp: absolute(urls.livecomp), stage: absolute(urls.stage) } });
  const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  let n = 0, t0 = performance.now();
  for (const [kind, text] of parts) {
    const p = put(text); try { n = core._stage_world_part(kind, p); } finally { core._free(p); }
    if (performance.now() - t0 > budgetMs) { await yieldFn(); t0 = performance.now(); }
  }
  return n;
}

// ---- eventSlices (docs/ctm-flow.md 8.4, core world_bridge.cpp event_world_seal): an event course's core world in parts ----
// The files come through the page's downloads (web/downloads.js: counted on the load screen, shared with later readers); their bytes
// go to the worker (transferred), which cuts them from their own text and computes the whole documents' parse keys. Returns
// {hash, keys: {world, body, rails}, terrain: {head, parts}, world: {head, parts}, rails: {head, parts, count}}, or null (the caller
// then loads the whole documents).
export async function cutEventWorld(buffers) {
  try { return await cutOffThread({ cut: 'event', buffers }, Object.values(buffers)); }
  catch (e) { console.warn('Event world cut failed (whole documents)', e); return null; }
}
// The event load's work per frame fills a frame under 50 ms: the budget is 36 ms less what the last yield took (the frame's render and
// the page's other work: ~8 ms in Chrome on a desktop, ~22 in WebKit, 25-40 on a phone), between 16 and 30 ms (a call is at most ~5 ms at 1x
// CPU, ~20 at 4x). A fixed small budget cost a frame's render per few ms of work (WebKit: +2 s on Snow Jam's load).
const EVENT_BUDGET_MS = 'frame';
// alive(): throws when the load was abandoned (main.js courseLoadGuard: the first course behind the menus, dropped for another course), checked
// after every yield so an abandoned load stops within a frame instead of feeding on (the next course waits for it on the switch chain).
const stepper = (yieldFn, budgetMs, alive = null) => {
  let t0 = performance.now(), budget = budgetMs === 'frame' ? 20 : budgetMs;
  return async () => {
    if (performance.now() - t0 <= budget) return;
    const y0 = performance.now(); await yieldFn(); alive?.(); t0 = performance.now();
    // (work + the rest of the frame under ~36 ms: the next vsync at 50; at least 16 ms, so slow frames do not stretch the load)
    if (budgetMs === 'frame') budget = Math.max(16, Math.min(30, 36 - (t0 - y0)));
  };
};
// init_terrain / init_world_collision / init_body_terrain of the whole documents, in parts (hashPtr: the terrain's source hash in the core).
export async function feedEventWorld(core, cut, hashPtr, { yieldFn = nextFrame, budgetMs = EVENT_BUDGET_MS, alive = null } = {}) {
  const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const call = (fn, text, ...rest) => { const p = put(text); try { return fn(p, ...rest); } finally { core._free(p); } };
  const tick = stepper(yieldFn, budgetMs, alive);
  core._event_world_begin();
  call(core._init_terrain, cut.terrain.head); call(core._init_world_collision, cut.world.head, hashPtr); call(core._init_body_terrain, cut.terrain.head);
  core._peak_world_append(1);
  try {
    for (const part of cut.world.parts) { call(core._init_world_collision, part, hashPtr); await tick(); }   // every world part before any terrain part
    for (const part of cut.terrain.parts) { call(core._terrain_part, part); await tick(); }
  } finally { core._peak_world_append(0); }
  const w = put(cut.keys.world), t = put(cut.keys.body); try { core._event_world_seal(w, t); } finally { core._free(w); core._free(t); }
}
// pv eventInWorldAi (docs/ctm-events-in-world.md stage 4): the same load into a computer rider's context of the streamed world, whose camera
// terrain (shared by every context) is the streamed world's: no init_terrain, and the parts build the context's body collision only
// (core event_world_bodies_only). The seal fills the parse caches the other contexts copy by cut.keys.
export async function feedContextWorld(core, cut, hashPtr, { yieldFn = nextFrame, budgetMs = EVENT_BUDGET_MS, alive = null } = {}) {
  const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const call = (fn, text, ...rest) => { const p = put(text); try { return fn(p, ...rest); } finally { core._free(p); } };
  const tick = stepper(yieldFn, budgetMs, alive);
  core._event_world_begin(); core._event_world_bodies_only();
  call(core._init_world_collision, cut.world.head, hashPtr); call(core._init_body_terrain, cut.terrain.head);
  core._peak_world_append(1);
  try {
    for (const part of cut.world.parts) { call(core._init_world_collision, part, hashPtr); await tick(); }
    for (const part of cut.terrain.parts) { call(core._terrain_part, part); await tick(); }
  } finally { core._peak_world_append(0); }
  const w = put(cut.keys.world), t = put(cut.keys.body); try { core._event_world_seal(w, t); } finally { core._free(w); core._free(t); }
}
// init_rails of the whole catalog, in parts.
export async function feedEventRails(core, cut, hashPtr, { yieldFn = nextFrame, budgetMs = EVENT_BUDGET_MS, alive = null } = {}) {
  const put = (t) => { const b = new TextEncoder().encode(t + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); return p; };
  const call = (fn, text, ...rest) => { const p = put(text); try { return fn(p, ...rest); } finally { core._free(p); } };
  const tick = stepper(yieldFn, budgetMs, alive);
  call(core._init_rails, cut.rails.head, hashPtr); core._peak_world_reserve(0, cut.rails.count);
  core._peak_world_append(1);
  try { for (const part of cut.rails.parts) { call(core._init_rails, part, hashPtr); await tick(); } } finally { core._peak_world_append(0); }
  const k = put(cut.keys.rails); try { core._rails_seal(k); } finally { core._free(k); }
}
// init_world(collision.bin) with its triangle tree built across frames (core init_world_begin / init_world_step).
export async function initWorldStepped(core, floats, { yieldFn = nextFrame, budgetMs = EVENT_BUDGET_MS, step = 40000, alive = null } = {}) {
  const p = core._malloc(Math.max(4, floats.byteLength)); core.HEAPF32.set(floats, p >> 2);
  try { core._init_world_begin(p, floats.length); } finally { core._free(p); }
  const tick = stepper(yieldFn, budgetMs, alive);
  while (!core._init_world_step(step)) await tick();
}
