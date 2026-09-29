// Original computer riders (AI racers) for race events. One core runs all six riders, as the original's rider manager
// does: each computer rider is a rider context of the human's core (web/rider_context.cpp, web/rider_local.hpp: its own
// rider state, the course geometry shared) and runs the complete browser rider pipeline (same controllers/motion/
// animation/collision as the human; in the original it is the same actor class) with the original NPC input provider
// 0x10A768 (web/npc_gameplay.inc) instead of the pad. This module owns the shared race world:
//   * roster/grid/identity: npc-riders.json (tools/export_npc_riders.py, countdown anchor savestate);
//   * 0x10F560 every sixth tick: ranking 0x10F998 (+0xEC place), pair proximity records, pacing
//     bounds, designated peer (web/race_world.cpp, run in the human's core);
//   * the one shared game RNG (0x4FF030): every stage of every rider draws from the same words;
//   * per-tick order: the original 0x128AF0 runs every pass over all riders in slot order; the cores
//     run their riders in three stages (pose, 121750 with rider pairs inside it, 121818), see below.
//       beginTick(): 10F560 on the tick-start state -- call before the human's pad_tick/step;
//       endTick():   the computer riders' 121818 -- call after the human's race_end.
// Documented approximations are in docs/ai-racers.md.
export const SLOTS = 6;
// The shared generator 0x317810 (engine/original_random.hpp) on six words.
export function rngNext(w) {
  let value = (w[5] + w[4]) >>> 0; let carry = (value < w[5] || value < w[4]) ? 1 : 0; w[4] = value;
  for (let k = 3; k >= 1; --k) { const nv = (value + w[k] + carry) >>> 0; carry = nv < w[k] ? 1 : 0; value = nv; w[k] = value; }
  value = (value + w[0] + carry) >>> 0; w[0] = value; w[5] = (w[5] + 1) >>> 0;
  if (w[5] === 0) { let k = 4; while (k >= 1) { w[k] = (w[k] + 1) >>> 0; if (w[k] !== 0) break; --k; } if (k === 0) { value = (value + 1) >>> 0; w[0] = value; } }
  return value;
}

function mergeSettings(initial, overrides) {
  const out = { ...initial };
  for (const [key, value] of Object.entries(overrides)) out[key] = (initial[key] && typeof initial[key] === 'object' && !Array.isArray(initial[key])) ? { ...initial[key], ...value } : value;
  return out;
}

// resources: { packetsJson, packetsBin (Uint8Array), initialText, riderText: {PACKAGE: text},
//              terrainText, worldCollisionText, railsText, terrainHash }
// A rider context is used through a view with the core's API: each export call runs in that rider's context (the core's
// __tls_base set to the context's block around the call, web/rider_tls.S). Memory is the human's core's.
export const singleCoreSupported = (core) => !!(core && core._rider_context_create && core.___tls_base);
// The current block is mirrored in JS (module.__riderTls.current, docs/sim-performance.md "Rider glue"): only these views write
// __tls_base (the core's rider_context_create restores it before returning), so the mirror always equals it and a call reads the
// mirror instead of the WebAssembly.Global getter.
export const riderTlsCurrent = (module) => (module.__riderTls ??= { current: module.___tls_base.value }).current;
export function riderContextCore(module, block) {
  const tls = module.___tls_base, view = { riderContext: block, module, getExceptionMessage: module.getExceptionMessage };
  const mirror = module.__riderTls ??= { current: tls.value };
  for (const [name, fn] of Object.entries(module)) {
    if (name[0] !== '_' || name[1] === '_' || typeof fn !== 'function') continue;
    if (fn.length > 12) throw new Error(`riderContextCore: ${name} takes ${fn.length} arguments`);
    // Fixed parameters, no rest/spread: these run hundreds of times per tick and must not allocate (GC pauses on phones).
    view[name] = (a, b, c, d, e, f, g, h, i, j, k, l) => {
      const previous = mirror.current; if (previous === block) return fn(a, b, c, d, e, f, g, h, i, j, k, l);
      tls.value = block; mirror.current = block; try { return fn(a, b, c, d, e, f, g, h, i, j, k, l); } finally { tls.value = previous; mirror.current = previous; }
    };
  }
  for (const heap of ['HEAPU8', 'HEAPF32']) Object.defineProperty(view, heap, { get: () => module[heap] });
  return view;
}
export async function createAiRacers({ human: humanModule, resources, document: initialDoc, isolate = false, count = Math.min(SLOTS, initialDoc.riders.length + 1) /* 2 in the backcountry rival events (docs/backcountry.md) */, onDraws = null, afterRider = null, sharedVisual = false }) {
  if (!singleCoreSupported(humanModule)) throw new Error('This core has no rider contexts (web/rider_context.cpp)');
  // The human's own context seen through a view, so its exports run in it even while a computer rider's context is
  // current (rider pairs call back into the human's race world from inside a computer rider's 121750).
  const humanBlock = humanModule._rider_context_current();
  const human = riderContextCore(humanModule, humanBlock);
  const initial = JSON.parse(resources.initialText);
  let doc = initialDoc; // the lineup document (npc-riders.json, or web/lineup.js assembleLineup); setDocument() replaces it
  const riders = doc.riders.slice(0, count - 1);
  const npcs = [];
  const enc = new TextEncoder();
  // One computer rider's own setup in its core (init_animation with its rider.json and settings, then npc_configure).
  // Run at creation and again by reconfigure() when a new lineup puts another rider (or relationship row) in the slot.
  function configureRider(core, rider) {
    const inputs = [];
    const put = (bytes) => { const p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); inputs.push(p); return p; };
    const str = (text) => put(enc.encode(text + '\0'));
    const release = () => { for (const p of inputs.splice(0)) core._free(p); };
    const riderText = resources.riderText[rider.package];
    if (!riderText) throw new Error(`Missing rider package ${rider.package}`);
    const settings = JSON.stringify(mergeSettings(initial, rider.settings));
    performance.mark(`ai:${rider.character}:animation`);
    core._init_animation(str(resources.packetsJson), str(riderText), str(settings), put(resources.packetsBin), resources.packetsBin.length); release();
    configureNpc(core, rider);
  }
  // npc_configure alone: the provider's driving/route/pacing state from the record. Also run at every start(): the
  // original reloads the event on a restart, so a restart must not keep the previous run's NPC state.
  function configureNpc(core, rider) {
    const inputs = [];
    const str = (text) => { const bytes = enc.encode(text + '\0'), p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); inputs.push(p); return p; };
    try { core._npc_configure(str(JSON.stringify(rider)), str(JSON.stringify({ event_variant: doc.event_variant, relationships: doc.relationships, anchor_tick: doc.anchor_tick }))); }
    catch (e) { throw new Error(`${rider.character} configure: ${core.getExceptionMessage ? core.getExceptionMessage(e) : e}`); }
    finally { for (const p of inputs) core._free(p); }
    // The rider's own stat getters (0x1494C0..: attribute bank 2 for computer riders, tools/export_npc_riders.py): Peak 2
    // riders ride at level 4 (raw 20) where the Peak 1 anchors hold 5 (docs/peak2.md). Documents without it keep the seeds.
    const a = rider.attributes;
    if (a?.raw?.length === 7 && core._set_rider_attributes && a.raw.some((v) => v !== 5)) {
      const raw = core._malloc(28); new Int32Array(core.HEAPU8.buffer, raw, 7).set(a.raw); core._set_rider_attributes(raw, a.override | 0); core._free(raw);
    }
  }
  // keep the loading screen drawing (resources.yieldFn, pv eventSlices: a frame, as task yields alone let the setup run back to back)
  const yieldFrame = resources.yieldFn ?? (() => new Promise((resolve) => setTimeout(resolve, 0)));
  // A rider context's course world: the init calls a fresh core runs for its course, but init_world / init_terrain, whose
  // geometry every context shares (web/core.cpp); the course package parses are copies of the human's (web/world_bridge.cpp
  // parse cache), so a context sets up in ~0.1 s.
  async function attachWorld(core) {
    const inputs = [], put = (bytes) => { const p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); inputs.push(p); return p; };
    const str = (text) => put(enc.encode(text + '\0')), release = () => { for (const p of inputs.splice(0)) core._free(p); };
    if (resources.yieldFn && core._animation_prepare) { core._animation_prepare(str(resources.initialText)); release(); await yieldFrame(); } // (pv eventSlices: the settings parsed in a frame of their own)
    core._init_race(str(resources.initialText)); release();
    core._animation_use_physics(1);
    // pv eventSlices (web/ai-race.js worldKeys): the parse caches by the documents' keys (no multi-MB text copied into the core per rider;
    // the rail catalog parsed once); a cache holding another document answers 0 and the texts are fetched and passed as before.
    const keys = resources.worldKeys, keyed = (fn, key) => { if (!key || !fn) return false; const ok = !!fn(str(key)); release(); return ok; };
    const texts = async () => { if (!resources.worldCollisionText && resources.worldTexts) Object.assign(resources, await resources.worldTexts()); };
    if (!keyed(core._init_world_collision_cached, keys?.world)) { await texts(); core._init_world_collision(str(resources.worldCollisionText), str(resources.terrainHash)); release(); }
    await yieldFrame();
    if (!keyed(core._init_body_terrain_cached, keys?.body)) { await texts(); core._init_body_terrain(str(resources.terrainText)); release(); }
    await yieldFrame();
    if (!keyed(core._init_rails_cached, keys?.rails)) { await texts(); core._init_rails(str(resources.railsText), str(resources.terrainHash)); release(); }
  }
  for (const rider of riders) {
    performance.mark(`ai:${rider.character}:create`);
    if (!resources.riderText[rider.package]) throw new Error(`Missing rider package ${rider.package}`);
    const core = riderContextCore(humanModule, humanModule._rider_context_create());
    await attachWorld(core); await yieldFrame(); // keep the loading screen drawing
    // pv eventSlices: init_animation's three documents parsed ahead, one a frame (core animation_prepare: the same parses)
    if (resources.yieldFn && core._animation_prepare) for (const text of [resources.packetsJson, resources.riderText[rider.package], JSON.stringify(mergeSettings(initial, rider.settings))]) {
      const b = enc.encode(text + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); try { core._animation_prepare(p); } finally { core._free(p); } await yieldFrame(); }
    configureRider(core, rider);
    if (resources.yieldFn) await yieldFrame(); // (pv eventSlices: the next rider's init_race in a frame of its own)
    performance.mark(`ai:${rider.character}:done`);
    npcs.push({ slot: rider.slot, character: rider.character, package: rider.package, core, record: rider, command: null, state: null, progress: null, finished: false });
  }
  const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
  // Views are cached per rider (the pointers are fixed per context; a view is rebuilt only after memory growth): the
  // per-tick choreography below reads them many times per tick and must not allocate much (GC pauses on phones).
  const rngViews = new Map();
  const rngView = (core) => { let v = rngViews.get(core); if (!v || v.buffer !== core.HEAPU8.buffer) { v = new Uint32Array(core.HEAPU8.buffer, core._animation_rng_words(), 6); rngViews.set(core, v); } return v; };
  const humanRider = { slot: 0, core: human, human: true, character: 'human' };
  let allList = null; // [human, ...computer riders], rebuilt only if the rider list changes (called many times per tick)
  const all = () => (allList && allList.length === npcs.length + 1 ? allList : (allList = [humanRider, ...npcs]));
  let tick = 0;
  const riderBuffer = human._malloc(6 * 6 * 4);
  const world = {
    // Pair distance/bearing of every record from the grid positions (0x10F560's arithmetic in the human core), so a
    // record the refresh skips (a disabled pair: --isolate) holds the original's countdown value too.
    proximity() {
      const init = [count, doc.world.tail, doc.world.rank_mode];
      for (let a = 0; a < 6; a++) { for (let b = 0; b < 6; b++) init.push(a !== b && a < count && b < count ? 1 : 0, 0, 1e10, 0, 0, 0, 0, 0, 0); init.push(a); }
      for (let a = 0; a < 36; a++) init.push(0);
      const p = human._malloc(init.length * 4); human.HEAPF32.set(init, p >> 2); human._race_world_reset(p); human._free(p);
      const buf = f32(human, riderBuffer, 36); buf.fill(0);
      for (const r of all()) { const s = live(r).state; buf.set([s[0], s[1], s[2], 1e5, 0, 1], r.slot * 6); }
      human._race_world_frame(0, riderBuffer);
      const w = f32(human, human._race_world_state(), 360);
      return (a, b) => [w[(a * 6 + b) * 10 + 2], w[(a * 6 + b) * 10 + 3]];
    },
    reset() {
      const near = this.proximity();
      const init = [count, doc.world.tail, doc.world.rank_mode];
      for (let a = 0; a < 6; a++) {
        for (let b = 0; b < 6; b++) {
          const r = a < count && b < count ? doc.world.records[a]?.[b] : null;
          let enabled = r ? r.enabled : 0;
          if (isolate && (a === 0 || b === 0) && a !== b) enabled = 0; // tools/ps2_capture.py --isolate
          const [distance, bearing] = r && a !== b ? near(a, b) : [r ? r.distance : 1e10, r ? r.bearing : 0];
          init.push(enabled, r ? r.human : 0, distance, bearing, r ? r.t10 : 0, r ? r.t14 : 0, r ? r.t18 : 0, r ? r.t1c : 0, r ? r.t20 : 0);
        }
        init.push(doc.world.ranks[a] ?? a);
      }
      for (let a = 0; a < 6; a++) for (let b = 0; b < 6; b++) init.push(doc.relationships.scores[a]?.[b] ?? 0);
      const p = human._malloc(init.length * 4); human.HEAPF32.set(init, p >> 2); human._race_world_reset(p); human._free(p);
      const pairs = []; for (let s = 0; s < 6; s++) { const w = doc.world.pair_inputs?.[s]; pairs.push(w ? w.weight_attribute : 0, w ? w.collision_stat : 0, w ? w.attack_stat : 0); } pairs.push(doc.world.knockdown_cheat ? 1 : 0);
      const q = human._malloc(pairs.length * 4); human.HEAPF32.set(pairs, q >> 2); human._race_world_pair_setup(q); human._free(q);
    },
  };
  const toArray = (v) => { const a = new Array(v.length); for (let i = 0; i < v.length; i++) a[i] = v[i]; return a; }; // Array.from without the iterator objects
  const live = (r) => ({ state: f32(r.core, r.core._rider_world_state(), 16).slice(), progress: f32(r.core, r.core._race_progress_info(), 8).slice() });
  // The same two reads without copies, for a reader that uses them at once (worldBuffer): the exports fill fixed per-context
  // arrays, viewed in place (views renewed after memory growth); `liveNow` hands out one reused record.
  const fixedViews = new Map(); // core -> { buffer, state, progress, npc } typed arrays over rider_world_state / race_progress_info / npc_world_buffer
  const fixedOf = (core) => { let v = fixedViews.get(core); if (!v || v.buffer !== core.HEAPU8.buffer) { v = { buffer: core.HEAPU8.buffer, state: null, progress: null, npc: null }; fixedViews.set(core, v); } return v; };
  const worldStateNow = (core) => { const p = core._rider_world_state(), v = fixedOf(core); if (!v.state || v.state.byteOffset !== p) v.state = new Float32Array(v.buffer, p, 16); return v.state; };
  const progressNow = (core) => { const p = core._race_progress_info(), v = fixedOf(core); if (!v.progress || v.progress.byteOffset !== p) v.progress = new Float32Array(v.buffer, p, 8); return v.progress; };
  const liveRecord = { state: null, progress: null };
  const liveNow = (r) => { liveRecord.state = worldStateNow(r.core); liveRecord.progress = progressNow(r.core); return liveRecord; };
  function snapshot() { for (const r of all()) r.start = live(r); } // tick-start: [pos3, vel3, path, upper class, control, motion, human, quat4, timeScale] + progress
  // npc_world_buffer for `target`; view(r) picks which state of peer r the reading pass sees.
  // upperClass(r): the class word (+7) to use for peer r instead of view(r)'s, or undefined.
  function worldBuffer(target, view, worldState, upperClass = null) {
    const fixed = fixedOf(target.core); if (!fixed.npc) fixed.npc = new Float32Array(fixed.buffer, target.core._npc_world_buffer(), 160); // a fixed array (pure getter)
    const w = fixed.npc; w.fill(0);
    w[0] = tick; w[1] = count;
    const self = target.slot, per = 360; // worldState[360..]: per-rider words (read in place, no per-call copies)
    w[2] = worldState[per + self * 6 + 1]; w[3] = worldState[per + self * 6 + 2]; w[4] = worldState[per + self * 6 + 3]; w[5] = worldState[per + self * 6 + 4];
    for (const r of all()) {
      const p = 8 + 12 * r.slot, v = view(r), s = v.state;
      w[p] = s[0]; w[p + 1] = s[1]; w[p + 2] = s[2]; w[p + 3] = s[3]; w[p + 4] = s[4]; w[p + 5] = s[5];
      const cls = upperClass ? upperClass(r) : undefined;
      w[p + 6] = s[6]; w[p + 7] = r.human ? 1 : 0; w[p + 8] = cls === undefined ? s[7] : cls; w[p + 9] = v.progress[0]; w[p + 10] = 1;
      const rec = (self * 6 + r.slot) * 10;
      for (let k = 0; k < 10; k++) w[80 + 10 * r.slot + k] = worldState[rec + k];
    }
  }
  const explain = (core, label, fn) => { try { return fn(); } catch (e) { if (e instanceof Error) throw e; throw new Error(`${label}: ${core.getExceptionMessage ? core.getExceptionMessage(e) : e}`); } };
  let coreList = null; // [human, ...computer riders' cores], rebuilt only if the rider list changes (read many times per tick)
  const cores = () => (coreList && coreList.length === npcs.length + 1 ? coreList : (coreList = [human, ...npcs.map((n) => n.core)]));
  // ---- One shared world, the original pass order (0x128AF0) ----
  // Every pass of 128AF0 runs over all riders in slot order: 120F20, 121068 (providers/controllers),
  // 1210B0, 1211F8, 1216E0 (motion update), 121700/121728 (pose, 11EB98 -> AA0 spheres), then 121750
  // (second motion phase: touchdown, 13F488/13AA48, 105398, rider pairs 107888, 13F358 clamp), then
  // 1217F8/121818 (course progress, route, triggers). The cores run it in three stages:
  //   stage 1: each rider up to and including its pose (human: pad_tick, race_begin, step_rider,
  //            animation_pose; computer riders: npc_tick, race_begin, step_rider, animation_pose);
  //   stage 2: each rider's animation_post (121750) -- rider pairs are dispatched from inside it
  //            (Module.riderHost.pairs), so later riders are bumped before their own 121750;
  //   stage 3: each rider's race_end (121818).
  // The human core runs its own tick unchanged (main.js simTick): its animation_tick calls
  // riderHost.afterPose (the computer riders' stage 1) between its pose and its 121750, and its
  // race_end calls riderHost.beforeProgress (the computer riders' stage 2) first; endTick runs the
  // computer riders' stage 3.
  // The one shared game RNG (0x4FF030): stage-1 draws are controller-pass draws (121068: providers,
  // 115D48, 131620) and continue one controller cursor in slot order; the rare other stage-1 draws
  // (motion kind) start after every rider's controller draws (the later riders' counts are predicted
  // from their previous tick, docs/ai-racers.md). Stages 2 and 3 draw in slot order from one cursor
  // (the running core holds it; pair dispatch draws from it).
  let anchorRng = null;
  let stage = 0, tickStart = new Uint32Array(6), cursor = new Uint32Array(6), motionTotal = 0, activeCore = human;
  const lastControllerDraws = new Array(SLOTS).fill(0);
  const later = (slot) => lastControllerDraws.reduce((sum, c, j) => sum + (j > slot ? c : 0), 0);
  const enter = (core, slot) => { rngView(core).set(cursor); core._animation_rng_split(1); core._animation_rng_defer(later(slot) + motionTotal); };
  const leaveStage1 = (core, slot) => {
    const info = core._animation_rng_split_info(), counts = new Uint32Array(core.HEAPU8.buffer, info, 3);
    const before = onDraws ? Array.from(cursor) : null; cursor.set(rngView(core)); lastControllerDraws[slot] = counts[0]; motionTotal += counts[1];
    if (onDraws) onDraws(slot, before, counts[0], counts[1]);
  };
  const countDraws = (a, b) => { const w = Uint32Array.from(a); for (let n = 0; n <= 256; n++) { if (w.every((x, k) => x === b[k])) return n; rngNext(w); } return -1; };
  // Run `fn` in `core` with the shared cursor; returns the cursor after it.
  function onShared(core, slot, words, fn) {
    const v = rngView(core); v.set(words); const prev = activeCore; activeCore = core;
    try { fn(); } finally { activeCore = prev; }
    const after = rngView(core).slice();
    if (onDraws) { const n = countDraws(words, after); if (n) onDraws(slot, words, 0, n); }
    return after;
  }
  const advance = (words, n) => { for (let k = 0; k < n; k++) rngNext(words); return words; };
  // sharedVisual: the one visual stream of the original (0x4FF018 six words + the gp+0xA0C LCG) lives in the human core;
  // a computer rider's core borrows it for its passes that draw (FX passes, 121818 programs) and hands it back. The FX
  // passes run after every rider's 121818, phase by phase over all riders (0x128F20..0x1290F0: board sparks for every
  // rider, then the board track, then snow, ...), docs/visual-rng-order.md.
  // The six words and the LCG word are rider-context statics (fixed addresses per context): views cached per context
  // (renewed when the memory grows), not two exports and two typed arrays per rider and FX phase.
  const visualViews = new Map();
  const visualOf = (core) => {
    let v = visualViews.get(core);
    if (!v || v.buffer !== core.HEAPU8.buffer || v.context !== core.riderContext) {
      const buffer = core.HEAPU8.buffer;
      v = { buffer, context: core.riderContext, words: core._visual_rng_words ? new Uint32Array(buffer, core._visual_rng_words(), 6) : null, lcg: core._visual_lcg_word ? new Uint32Array(buffer, core._visual_lcg_word(), 1) : null };
      visualViews.set(core, v);
    }
    return v;
  };
  function onVisual(core, fn) {
    if (!sharedVisual || core === human) return fn();
    const v = visualOf(core); if (!v.words || !v.lcg) return fn();
    const h = visualOf(human);
    v.words.set(h.words); v.lcg[0] = h.lcg[0];
    try { return fn(); } finally { h.words.set(v.words); h.lcg[0] = v.lcg[0]; }
  }
  const fxPassLabels = ['fx pass 0', 'fx pass 1', 'fx pass 2', 'fx pass 3', 'fx pass 4'];

  // Rider pairs 0x107888 (web/race_world.cpp dispatcher in the human core), called from inside the
  // initiating rider's 121750 (motion 0/1/2/4 posts). Callbacks reach every rider core; draws come
  // from the running core's cursor, reactions in another core run on a copy of it.
  const pairsEnabled = count > 1 && !!human._race_world_pairs;
  const pairCounts = [0, 0, 0, 0, 0];
  // pair_view is a pure read of a rider: inside one dispatch (race_world_pairs of one slot) a slot's view is reused until a
  // callback moves, pushes or reacts a rider (then every view is read again). Outside a dispatch nothing is reused.
  let pairViewGeneration = 0, pairDispatching = false;
  const pairViewBytes = Array.from({ length: SLOTS }, () => new Uint8Array(560)), pairViewSeen = new Array(SLOTS).fill(-1);
  humanModule.pairHost = {
    view(slot, out) {
      if (pairDispatching && pairViewSeen[slot] === pairViewGeneration) { human.HEAPU8.set(pairViewBytes[slot], out); return; }
      const p = cores()[slot]._pair_view(); human.HEAPU8.copyWithin(out, p, p + 560); // 140 words, byte copy (one memory)
      if (pairDispatching) { pairViewBytes[slot].set(human.HEAPU8.subarray(out, out + 560)); pairViewSeen[slot] = pairViewGeneration; }
    },
    translate(slot, x, y, z) { pairViewGeneration++; cores()[slot]._pair_translate(x, y, z); },
    velocity(slot, x, y, z, reseed) { pairViewGeneration++; cores()[slot]._pair_set_velocity(x, y, z, reseed); },
    react(slot, kind, animation, attack, eventPtr, other) {
      pairViewGeneration++;
      if (other !== undefined) {   // 107E70 -> 10E228/10E2E8/10E3A8/10E468: speech 2A0A30 first, then the relationship 155BF0
        api.onPairAudio?.(slot, other, kind, attack);
        api.onReact?.(slot, other, kind, attack);
        if (kind === 2 && attack) cores()[other]?._pair_knockout?.(); // 10E468: 119400 KO count + popup 0x2C, 10E098(attacker, 1.0, 2)
      }
      const c = cores()[slot], e = new Float32Array(human.HEAPF32.buffer, eventPtr, 10).slice(), p = c._malloc(40); c.HEAPF32.set(e, p >> 2);
      if (c === activeCore) explain(c, `pair reaction ${slot}`, () => c._pair_react(kind, animation, attack, p));
      else { const a = rngView(activeCore), v = rngView(c); v.set(a); explain(c, `pair reaction ${slot}`, () => c._pair_react(kind, animation, attack, p)); rngView(activeCore).set(rngView(c)); }
      c._free(p);
    },
    random() { return rngNext(rngView(activeCore)); },
  };
  const dispatchPairs = (slot) => {
    if (!pairsEnabled || stage !== 2) return;
    pairViewGeneration++; pairDispatching = true;
    let out; try { out = explain(human, 'rider pairs', () => Array.from(f32(human, human._race_world_pairs(tick, slot), 5))); } finally { pairDispatching = false; pairViewGeneration++; }
    out.forEach((v, k) => { pairCounts[k] += v; });
  };
  // One shared world: a course-script trigger fired in one core, and every change a rider makes to a
  // shared entity (crashbag rollers, boost pickups, log teeters, trigger contacts, section MultiSplines;
  // web/shared_world.inc), is replayed at once (no draw, no rider effect) in every other core, right
  // after the pass that made it -- later riders of the same tick see it, as in the one original world.
  const knownTriggers = new Set();
  const syncWorld = (from) => {
    if (from._world_triggers) {
      const p = from._world_triggers(), n = new Uint32Array(from.HEAPU8.buffer, p, 1)[0], list = new Uint32Array(from.HEAPU8.buffer, p + 4, n).slice();
      for (const k of list) if (!knownTriggers.has(k)) { knownTriggers.add(k); for (const c of cores()) if (c !== from) c._world_trigger_apply(k); }
    }
    if (!from._world_events) return;
    const p = from._world_events(), n = new Uint32Array(from.HEAPU8.buffer, p, 1)[0];
    if (!n) return;
    const words = new Uint32Array(from.HEAPU8.buffer, p + 4, n).slice();
    for (let at = 0; at < n;) {
      const len = 2 + words[at + 1], event = words.subarray(at, at + len);
      for (const c of cores()) if (c !== from) { const q = c._malloc(len * 4); new Uint32Array(c.HEAPU8.buffer, q, len).set(event); explain(c, 'shared world event', () => c._world_event_apply(q)); c._free(q); }
      worldEvents++; worldEventKinds[words[at]] = (worldEventKinds[words[at]] || 0) + 1; at += len;
    }
  };
  let worldEvents = 0; const worldEventKinds = {};
  function stage1() { // riderHost.afterPose: the human's pose is done
    if (stage !== 1) return;
    leaveStage1(human, 0); syncWorld(human);
    for (const n of npcs) {
      // 121068 reads peers' positions/routes/progress at the tick start and earlier riders' upper-body classes after their controllers
      // (an earlier rider's class read now, in place: rider_world_state is a pure read).
      worldBuffer(n, (r) => r.start, api.worldState, (r) => (r.slot < n.slot ? worldStateNow(r.core)[7] : undefined));
      enter(n.core, n.slot);
      n.command = explain(n.core, `${n.character} provider`, () => f32(n.core, n.core._npc_tick(), 24)).slice();
      n.info = f32(n.core, n.core._npc_tick_info(), 9).slice();
      explain(n.core, `${n.character} tick`, () => {
        const c = n.command, core = n.core;
        core._race_begin();
        const state = f32(core, core._step_rider(c[0], c[6], c[2] ? 1 : 0, c[7]), 16);
        core._animation_pose(state[7], c[10], c[11], state[9], state[8], c[6], c[8], c[12], c[7], 0, state[15], c[13]);
      });
      leaveStage1(n.core, n.slot); syncWorld(n.core);
    }
    for (const c of cores()) { c._animation_rng_split(0); c._animation_rng_defer(0); }
    rngView(human).set(advance(cursor.slice(), motionTotal));
    stage = 2;
  }
  function stage2() { // riderHost.beforeProgress: the human's 121750 is done
    if (stage === 1) stage1();
    if (stage !== 2) return;
    let words = rngView(human).slice(); syncWorld(human);
    for (const n of npcs) {
      words = onShared(n.core, n.slot, words, () => explain(n.core, `${n.character} post`, () => { n.core._animation_post(); }));
      syncWorld(n.core);
    }
    rngView(human).set(words);
    stage = 3;
  }
  human._rider_host(pairsEnabled ? 3 : 1);
  // One module: its hooks run in whichever rider context is current. The phase hooks belong to the human's context (a
  // computer rider's race_end reaches js_rider_before_progress too; the hooks run the other riders' stages from inside
  // the human's tick); rider pairs go to the slot of the running context.
  humanModule._rider_parse_cache_clear?.();
  const slotOf = new Map([[humanBlock, 0], ...npcs.map((n) => [n.core.riderContext, n.slot])]);
  humanModule.riderHost = { afterPose: () => { if (riderTlsCurrent(humanModule) === humanBlock) stage1(); }, beforeProgress: () => { if (riderTlsCurrent(humanModule) === humanBlock) stage2(); }, pairs: () => dispatchPairs(slotOf.get(riderTlsCurrent(humanModule))) };
  // | 4: no renderer poses (a computer rider is drawn from its skin palette, not animation_post's bone poses; web/animation_bridge.cpp)
  for (const n of npcs) n.core._rider_host((pairsEnabled ? 2 : 0) | 4);
  const api = {
    npcs,
    get tick() { return tick; },
    get pairCounts() { return pairCounts.slice(); }, // [checks, separations, impulses, attacks, reactions]
    get worldEvents() { return worldEvents; }, // shared-entity changes replayed across the rider cores
    get worldEventKinds() { return { ...worldEventKinds }; }, // by kind (web/shared_world.inc)
    // Event start for the computer riders (call right after the human's start_event).
    start() {
      for (const n of npcs) { configureNpc(n.core, n.record); n.core._reset_pad_history(); explain(n.core, `${n.character} start`, () => n.core._npc_start_event()); n.finished = false; }
      tick = 0; stage = 0; world.reset(); lastControllerDraws.fill(0); knownTriggers.clear();
      for (const c of cores()) if (c._world_events) c._world_events(); // drop entries of the previous run
      worldEvents = 0; for (const k of Object.keys(worldEventKinds)) delete worldEventKinds[k];
    },
    setSharedRng(words) { rngView(human).set(words); },
    get document() { return doc; },
    onReact: null,   // (target, other, kind 1 soft / 2 crash, attack): a rider pair reaction (107E70), after onPairAudio
    onPairAudio: null,   // same arguments, before the relationship update (the handlers' 2A0A30 speech precedes 155BF0)
    // Live relationship(a, b) (slots): score = 0x155B50 level, kind = 0x155AB0 kind (document.relationships.scores/kinds).
    relation(a, b) { const r = doc.relationships; return { score: r.scores?.[a]?.[b] ?? 0, kind: r.kinds?.[a]?.[b] ?? null }; },
    // The game RNG words the original has at the countdown anchor (web/lineup.js anchorRandomWords); null keeps the
    // core's own (the capture gates set theirs with setSharedRng).
    setAnchorRng(words) { anchorRng = words ? Uint32Array.from(words) : null; },
    // relationship(own, peer) levels (6x6): the human core's 10F560 world and each computer rider's own row.
    setRelationships(scores) {
      doc.relationships.scores = scores.map((row) => row.slice());
      const m = human._malloc(36 * 4); human.HEAPF32.set(scores.flat(), m >> 2); human._race_world_set_relationships?.(m); human._free(m);
      for (const n of npcs) { if (!n.core._npc_set_relationships) continue; const q = n.core._malloc(24); n.core.HEAPF32.set(scores[n.slot], q >> 2); n.core._npc_set_relationships(q); n.core._free(q); }
    },
    // A new lineup (web/lineup.js): the document for world.reset/relationships, and every slot whose record or
    // relationship row changed is set up again in its own core (init_animation + npc_configure). Returns the slots
    // reconfigured. The rider packages of the new riders must be in resources.riderText.
    setDocument(next) {
      const changed = [];
      const row = (d, slot) => JSON.stringify(d.relationships.scores[slot]);
      for (const n of npcs) {
        const rider = next.riders.find((r) => r.slot === n.slot);
        if (!rider) throw new Error(`Lineup lacks slot ${n.slot}`);
        const same = JSON.stringify(rider) === JSON.stringify(n.record) && row(next, n.slot) === row(doc, n.slot)
          && next.event_variant === doc.event_variant && next.anchor_tick === doc.anchor_tick;
        if (same) continue;
        changed.push(n.slot);
      }
      doc = next;
      for (const n of npcs) if (changed.includes(n.slot)) {
        const rider = next.riders.find((r) => r.slot === n.slot);
        configureRider(n.core, rider);
        Object.assign(n, { character: rider.character, package: rider.package, record: rider, finished: false });
      }
      return changed;
    },
    // Before the human's tick: manager refresh 10F560 on the tick-start state (0x128AF0 runs it
    // before any rider pass), the human's 115D48 peers, and the human's RNG cursors.
    beginTick() {
      if (anchorRng && tick === doc.anchor_tick) rngView(human).set(anchorRng);   // the game RNG the original has at the anchor
      snapshot();
      const buf = f32(human, riderBuffer, 36); buf.fill(0);
      for (const r of all()) { const s = r.start.state; buf.set([s[0], s[1], s[2], r.start.progress[0], 0, 1], r.slot * 6); }
      // rank mode 2 (freestyle, R&B): every slot's run score +0x198 at the tick start (web/race_world.cpp race_world_score)
      if (doc.world.rank_mode === 2 && human._race_world_score) for (const r of all()) human._race_world_score(r.slot, new Int32Array(r.core.HEAPU8.buffer, r.core._score_object_dump(), 0x1d0 / 4)[0x198 / 4]);
      // 12A250 (11A228 early return): every human finished; the human core holds its +0x470 (web/score_gameplay.inc)
      { const done = f32(human, human._race_progress_info(), 8)[3] >= 0 ? 1 : 0; for (const n of npcs) n.core._set_humans_finished?.(done); }
      human._race_world_frame(tick, riderBuffer);
      this.worldState = toArray(f32(human, human._race_world_state(), 396)); // a plain array: consumers serialise it (compare-ai-capture reports)
      worldBuffer(humanRider, (r) => r.start, this.worldState); human._rider_world_peers();
      if (sharedVisual) for (const c of [human, ...npcs.map((n) => n.core)]) c._set_fx_deferred?.(1); // lineups can bring new cores
      tickStart = new Uint32Array(rngView(human)); cursor = new Uint32Array(tickStart); motionTotal = 0;
      enter(human, 0);
      stage = 1;
    },
    // After the human's tick (race_end): stage 3 -- each computer rider's 121818 in slot order (it
    // sees every rider's 121750 and the earlier riders' 121818), then the world pass draws.
    endTick() {
      if (stage === 1) stage1(); // a human tick that ran without its hooks
      if (stage === 2) stage2();
      let words = rngView(human).slice(); syncWorld(human);
      if (afterRider) advance(words, afterRider(0));
      for (const n of npcs) {
        worldBuffer(n, liveNow, this.worldState);
        words = onShared(n.core, n.slot, words, () => onVisual(n.core, () => explain(n.core, `${n.character} progress`, () => { const race = f32(n.core, n.core._race_end(), 8); if (race[2]) n.finished = true; })));
        syncWorld(n.core);
        if (afterRider) advance(words, afterRider(n.slot));
      }
      // World pass 0x101B60 (section activation: slot-1/3 stage programs, their builtin3/19/77 draws) runs once
      // per tick after every rider's passes. A core that ports it exports race_world_pass (run here, in the
      // human core, with the shared cursor; single-rider builds run it at the end of race_end instead).
      if (human._race_world_pass) { rngView(human).set(words); explain(human, 'world pass', () => human._race_world_pass()); words = rngView(human).slice(); syncWorld(human); }
      if (afterRider) advance(words, afterRider(-1));
      rngView(human).set(words);
      // The rider FX passes over every rider, phase-major, on the shared visual stream (deferred in every core).
      if (sharedVisual) { const list = cores(); for (let phase = 0; phase <= 4; phase++) for (const c of list) onVisual(c, () => explain(c, fxPassLabels[phase], () => c._fx_pass?.(phase))); }
      // Section activation 0x101B60 (web/section_gameplay.inc): the end of the rider manager, after every rider; its
      // slot-1 programs' shared-RNG draws are the last draws of the tick.
      if (human._section_pass) human._section_pass();
      stage = 0; tick++;
    },
    // Standings from the shared ranking (+0xEC) and course progress (+0x4D0, finish +0x478).
    standings() {
      const rows = all().map((r) => {
        const progress = Array.from(f32(r.core, r.core._race_progress_info(), 8));
        return { slot: r.slot, human: !!r.human, character: r.character, remaining: progress[0], finished: progress[3] >= 0, finishTicks: progress[4], rank: this.worldState ? this.worldState[360 + r.slot * 6] : r.slot };
      });
      return rows;
    },
  };
  return api;
}
