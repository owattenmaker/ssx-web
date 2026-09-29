// Located world-painter records of a streamed world for the page's painters (pv regionTick; docs/weather.md 10, 11).
//
// PS2: 2C0778 steps every environment wrapper with the painter record of location gp+0x770, the track of the human's last
// contacted patch, which 2ED490 writes after block 0's wrappers stepped. The camera block (0x15EBCC, after the rider
// manager: Fog, ScreenTint, Sun, glare) therefore switches on the tick the human first touches the new location, the human's
// own painters (block 0: Weather, rider Lighting) one tick later. A record that is not loaded (or has no section of the
// wrapper's type) gives the class defaults.
//
// The core keeps gp+0x770 (web/weather.inc weather_region_update) and selects its own located records (Weather in
// web/weather.inc, Fog and rider Lighting in web/environment_bridge.cpp). ScreenTint / Sun / glare are stepped by the page
// once per tick right after the core's tick (web/game-tick.js present): they read gp+0x770 there and take the record of that
// track, fed here by web/peak-world.js with each location's collision data (so it is in before the location can be touched).
// The page's old asynchronous region hook (web/main.js freeRide.on('region')) is ignored by the painters while this is active.
const docs = new Map(); // track -> {tint, sun, glare}: the parsed document, null (the location has none)
let active = false;

export const painterRegions = {
  get active() { return active; },
  // A new streamed world (web/peak-world.js createPeakWorld / dispose): no records.
  reset() { docs.clear(); active = false; },
  // A location's documents: {tint: screen-tint.json | null, sun: sun-painter.json | null, glare: glare-painter.json | null}.
  set(track, { tint = null, sun = null, glare = null } = {}) { docs.set(track, { tint, sun, glare }); active = true; },
  // The core's painter region gp+0x770 (-1: none yet; -2: not available).
  region(core) {
    if (!active || typeof core?._weather_region_info !== 'function') return -2;
    return new Int32Array(core.HEAPU8.buffer, core._weather_region_info(), 5)[0];
  },
  // undefined: no record loaded for the track; null: the record has no section of that kind; else the document.
  doc(track, kind) { const d = docs.get(track); return d ? (d[kind] ?? null) : undefined; },
};

// pv painterWorldLoad (web/environment_bridge.cpp environment_world_load): a world load's location entry leaves every painter
// wrapper at its class defaults with +0 = 0 (the load's steps ran in a region whose record was not loaded), so the start
// location's record blends in at its rate. True once per world load; call after the painter's own tick resets.
export function followWorldLoad(state, core) {
  const n = core?._environment_world_loads?.();
  if (n === undefined) return false;
  const seen = state.worldLoads ?? 0; state.worldLoads = n;
  return n !== seen;
}

// A painter's per-tick region follow: call before its step with the core; returns {changed, record, doc} when gp+0x770 moved
// to another track (record false: not loaded), else null. Until the world's first contact (-1) the painter keeps its package.
export function followRegion(state, core, kind) {
  const t = painterRegions.region(core);
  if (t < 0 || t === state.track) return null;
  state.track = t; state.located = true;
  const d = painterRegions.doc(t, kind);
  return { record: d !== undefined, doc: d ?? null };
}
