// Streamed Peak 1 world, browser side (docs/peak-mountain.md).
//
// The original loads whole locations while riding (ELF location table 0x43E250, streaming table 0x442168,
// residency rows 0x442488 switched by stage builtin 68). Which locations are IN the world on a tick is the
// core's business (web/peak_world.inc: the ported streaming rows gate every world query); this module only
// makes sure the data is there before the core needs it, without long tasks:
//   - collision: a worker (peak-world-worker.js) fetches and cuts a location's terrain/instances/rails into small
//     documents; `pump()` feeds them to the core's init functions in append mode within a per-frame budget;
//   - render: the location's draw package (same format as a course package) is built by the caller's
//     `buildRender(entry)` (web/main.js asset()) in the background and shown/hidden with the residency.
// Collision data stays in the core once loaded (the whole peak is ~100 MB of core memory); render groups are
// released when their location has been out of the world for a while (memory on Safari).
import { createGuardedWorker, workerUrl } from './worker-guard.js';
import { preparePeakLocation } from './peak-world-prepare.js';
import { pv } from './pv-flags.js';
import { painterRegions } from './painter-regions.js';
// The worker script of this build (content-hashed by Vite); web/worker-guard.js checks its build and falls back to
// preparePeakLocation on the main thread (docs/workers.md).
const PEAK_WORLD_WORKER = workerUrl((Worker) => new Worker(new URL('./peak-world-worker.js', import.meta.url), { type: 'module' }));
export async function createPeakWorld({ core, load, manifestUrl = '/assets/PEAK1/peak.json' }) {
  const manifest = await load(manifestUrl);
  const byCode = new Map(manifest.locations.map((l) => [l.code, { ...l, core: 'absent', batches: null, render: null, renderState: 'absent', lastResident: 0 }]));
  const worker = createGuardedWorker({ name: 'peak-world', url: PEAK_WORLD_WORKER, local: () => preparePeakLocation, stallTimeoutMs: 60000 });
  let first = true, feeding = null;
  const queue = []; // locations waiting for core feeding, in request order
  // The world's name travels with the location (the whole mountain's locations live in the per-peak folders, docs/peak3.md).
  // pv regionTick: every location's painter sections come with its collision data (web/painter-regions.js)
  const located = pv('regionTick'); painterRegions.reset();
  const fetchCore = (entry) => worker.request({ root: entry.root, world: manifest.name, painters: located, railParts: pv('sliceLoad') && !!core._rail_load_hash }); // (a core with the rail parts: web/rail_bridge.cpp segment_base / partial)
  const fetchEnv = (url) => worker.request({ env: url });
  const put = (text) => { const bytes = new TextEncoder().encode(text + '\0'), p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); return p; };
  let hashPtr = 0;
  const call = (fn, text, ...rest) => { const p = put(text); try { return fn(p, ...rest); } finally { core._free(p); } };
  // The whole mountain's environment slices (MOUNTAIN/ENV/<LOC>, docs/peak3.md section 6): added when the location is wanted,
  // dropped when free-ride.js releases it (its lattices are presentation only: rider lighting).
  const envRoot = manifest.environment_slices ? new URL(manifest.environment_slices, new URL(manifestUrl, globalThis.location?.href ?? 'http://localhost/')).pathname : null;
  const env = new Map(); // code -> {state: 'fetching' | 'queued' | 'loaded', batches, track}
  function feedEnv(batch) {
    const t = new TextEncoder().encode(batch.text + '\0'), pt = core._malloc(t.length), pb = core._malloc(Math.max(1, batch.bytes.length));
    core.HEAPU8.set(t, pt); core.HEAPU8.set(batch.bytes, pb);
    try { core._environment_add(pt, pb, batch.bytes.length); } finally { core._free(pt); core._free(pb); }
  }
  // The location record's Weather section (web/weather.inc weather_location: the core picks the record of the rider's region track
  // on the PS2's tick, 2C0778 / gp+0x770). ?weather=0 leaves the weather off (web/main.js).
  const weatherOff = new URL(globalThis.location?.href ?? 'http://localhost/').searchParams.get('weather') === '0';
  function feedWeather(entry) {
    const text = entry.weather; if (weatherOff || !core._weather_location || typeof text !== 'string' || !text.trimStart().startsWith('{')) return;
    const p = put(text); try { core._weather_location(entry.track, p); } finally { core._free(p); }
  }
  // pv regionTick: the record's Fog / Lighting sections to the core (selected by gp+0x770 in its camera / rider block steps),
  // ScreenTint / Sun / glare to the page's painters (web/painter-regions.js).
  function feedPainters(entry) {
    const d = entry.painters; if (!d) return;
    const give = (fn, text) => { if (!fn || typeof text !== 'string' || !text.trimStart().startsWith('{')) return; const p = put(text); try { fn(entry.track, p); } catch (e) { console.warn(`Painter record ${entry.code}`, e); } finally { core._free(p); } };
    give(core._fog_location, d.fog); give(core._lighting_location, d.lighting);
    painterRegions.set(entry.track, { tint: d.tint, sun: d.sun, glare: d.glare });
  }
  // Append one slice of a location to the loaded world (the core's init functions in append mode).
  function feed(batch) {
    core._peak_world_append(1);
    try {
      if (batch.kind === 'world') call(core._init_world_collision, batch.text, hashPtr);
      else if (batch.kind === 'terrain') { call(core._init_terrain, batch.text); call(core._init_body_terrain, batch.text); }
      else if (batch.kind === 'rails') call(core._init_rails, batch.text, hashPtr);
    } finally { core._peak_world_append(0); }
  }
  // The first location replaces whatever world the core had (one plain init per system, merged slices), then the
  // streamed world starts: nothing resident, instance/rail storage reserved so held pointers survive later appends.
  // pv sliceLoad: the first slice of each kind replaces the course world (plain init), the rest of the location is appended slice
  // by slice like every later location and committed, so no core call on the load screen takes more than a slice's few ms (the merged
  // init was 0.9-1.5 s per call at 4x CPU). Returns false then: pump() feeds the rest.
  const sliced = pv('sliceLoad');
  function beginSliced(entry) {
    const pick = (k) => entry.batches.find((b) => b.kind === k), w = pick('world'), t = pick('terrain'), r = pick('rails');
    core._peak_world_append(0);
    call(core._init_terrain, t.text);
    call(core._init_world_collision, w.text, hashPtr);
    call(core._init_body_terrain, t.text);
    call(core._init_rails, r.whole ?? r.text, hashPtr); // the whole catalog (a plain init; its parts are for appended locations)
    core._peak_world_begin();
    core._peak_world_reserve(65536, 4096);
    entry.batches = entry.batches.filter((b) => b !== w && b !== t && b.kind !== 'rails'); entry.cursor = 0;
    return false;
  }
  function begin(entry) {
    if (sliced) return beginSliced(entry);
    const of = (k) => entry.batches.filter((b) => b.kind === k).map((b) => JSON.parse(b.text));
    const world = of('world').reduce((a, b) => { if (!a) return b;
      for (const [t, v] of Object.entries(b.bindings)) { a.bindings[t] ??= { descriptors: [] }; const base = a.bindings[t].descriptors.length; a.bindings[t].descriptors.push(...v.descriptors);
        for (const i of b.instances) if (String(i.track) === t) i.collision_descriptor += base; }
      a.instances.push(...b.instances); Object.assign(a.collision_meshes, b.collision_meshes); Object.assign(a.render_model_nodes, b.render_model_nodes); return a; }, null);
    const terrain = JSON.stringify(of('terrain').reduce((a, b) => { if (!a) return b; a.patches.push(...b.patches); return a; }, null));
    core._peak_world_append(0);
    call(core._init_terrain, terrain);
    call(core._init_world_collision, JSON.stringify(world), hashPtr);
    call(core._init_body_terrain, terrain);
    call(core._init_rails, entry.batches.find((b) => b.kind === 'rails').text, hashPtr);
    core._peak_world_begin();
    core._peak_world_reserve(65536, 4096);
    return true;
  }
  async function requestCore(code) {
    const entry = byCode.get(code); if (!entry) throw Error(`Unknown Peak 1 location ${code}`);
    if (entry.core !== 'absent') return entry.ready;
    entry.core = 'fetching';
    entry.ready = (async () => {
      const data = await fetchCore(entry);
      if (!hashPtr) { const b = new TextEncoder().encode(data.hash + '\0'); hashPtr = core._malloc(b.length); core.HEAPU8.set(b, hashPtr); }
      entry.counts = data.counts; entry.bounds = data.bounds ?? null; entry.batches = data.batches; entry.weather = data.weather ?? null; entry.painters = data.painters ?? null; entry.core = 'queued'; queue.push(entry);
      await new Promise((resolve) => { entry.done = resolve; });
    })();
    return entry.ready;
  }
  // Feed queued batches for at most `budgetMs` (called once per frame; the loading screen calls it with a large budget).
  function pump(budgetMs = 3) {
    const until = performance.now() + budgetMs;
    for (const [code, e] of env) { // environment slices first (small calls), one location at a time
      if (e.state !== 'queued') continue;
      while (e.batches.length && performance.now() < until) feedEnv(e.batches.shift());
      if (!e.batches.length) { e.state = 'loaded'; e.batches = null; }
      break;
    }
    while (performance.now() < until) {
      if (!feeding) { feeding = queue.shift() || null; if (!feeding) return false; feeding.cursor = 0; }
      const entry = feeding;
      if (entry.weather !== undefined) { feedWeather(entry); entry.weather = undefined; } // the record's Weather section first (before any of its patches)
      if (entry.painters) { feedPainters(entry); entry.painters = null; }
      if (first) { first = false; if (begin(entry)) finish(entry); continue; }
      if (entry.cursor < entry.batches.length) { feed(entry.batches[entry.cursor++]); continue; }
      core._peak_world_commit(); finish(entry);
    }
    return true;
  }
  function finish(entry) { entry.core = 'loaded'; entry.batches = null; feeding = null; entry.done?.(); }
  return {
    manifest, locations: byCode, requestCore, pump,
    loaded: (code) => byCode.get(code)?.core === 'loaded',
    // A location the streaming rows want goes to the front of the feed queue.
    prioritize(code) { const i = queue.findIndex((e) => e.code === code); if (i > 0) queue.unshift(...queue.splice(i, 1)); },
    busy: () => !!feeding || queue.length > 0 || [...env.values()].some((e) => e.state === 'queued'),
    // Environment slices of the whole mountain (none for a single-peak world: its environment.json holds every patch).
    requestEnv(code) {
      if (!envRoot || !core._environment_add || env.has(code)) return;
      const e = { state: 'fetching', batches: null, track: byCode.get(code)?.track }; env.set(code, e);
      fetchEnv(envRoot + code).then((data) => { if (env.get(code) !== e) return; e.batches = data.batches; e.state = 'queued'; })
        .catch((error) => { console.warn(`Environment slice ${code} failed`, error); if (env.get(code) === e) env.delete(code); });
    },
    dropEnv(code) { const e = env.get(code); if (!e) return; env.delete(code); if (e.state === 'loaded' || e.state === 'queued') core._environment_drop?.(byCode.get(code).track); },
    envLoaded: (code) => env.get(code)?.state === 'loaded',
    // pv peakRelease: free a loaded location's collision in the core (web/peak_world.inc peak_world_free_track: refused while it is
    // collidable or in the section octree); its next requestCore fetches and feeds it again, into the same slots. True when freed.
    releaseCore(code) {
      const e = byCode.get(code); if (!e || e.core !== 'loaded' || !core._peak_world_free_track) return false;
      if (core._peak_world_free_track(e.track) < 0) return false;
      e.core = 'absent'; e.ready = null; e.batches = null; return true;
    },
    // pv peakRelease: a location fetched and cut but not fed (queued behind a wanted row's feeds) that nothing wants any more: its
    // slices are dropped (the next requestCore fetches it again). Never the one being fed.
    dropQueued(code) {
      const e = byCode.get(code); if (!e || e.core !== 'queued' || feeding === e) return false;
      const i = queue.indexOf(e); if (i >= 0) queue.splice(i, 1);
      e.core = 'absent'; e.ready = null; e.batches = null; e.done = null; return true;
    },
    dispose() { worker.terminate(); if (hashPtr) core._free(hashPtr); painterRegions.reset(); },
  };
}
