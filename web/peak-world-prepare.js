// Preparation of a streamed location's collision package (web/peak-world.js, docs/peak-mountain.md), run in
// web/peak-world-worker.js, or on the main thread when the worker is unavailable (web/worker-guard.js).
// It fetches and parses the location's terrain.json / world_collision.json / rails.json and cuts them into small
// self-contained documents that the core's ordinary init functions accept in append mode, so the main thread only copies
// a short string into the core per slice (no large JSON.parse or C++ parse on a gameplay frame).
// Request {root, world} -> {hash, batches, weather, counts}; {env: url} -> {batches, track} (a location's environment slice).
import { locationBatches, environmentBatches, stageWorldParts, environmentParts, eventTerrainParts, eventWorldParts, eventRailParts, parseKey } from './peak-world-batches.js';
import { withTransfer } from './worker-guard-child.js';

export async function preparePeakLocation({ root, env, world, painters = false, cut = null, urls = null, railParts = false, buffers = null }) {
  // sliceLoad (web/load-slices.js): the stage-world documents / an environment.json cut into the core's parts off the main thread
  if (cut) {
    const text = (u) => (u ? fetch(u).then((r) => { if (!r.ok) throw Error(`${u}: ${r.status}`); return r.text(); }) : Promise.resolve(''));
    if (cut === 'stage') { const [p, l, s] = await Promise.all([text(urls.particles), text(urls.livecomp), text(urls.stage)]); return { parts: stageWorldParts(p, l, s) }; }
    if (cut === 'env') return environmentParts(await text(urls.environment));
    // eventSlices (web/load-slices.js cutEventWorld): an event course's terrain / world collision / rails bytes, cut into the core's parts
    if (cut === 'event') {
      const bytes = (k) => new Uint8Array(buffers[k]), dec = new TextDecoder(), tb = bytes('terrain'), wb = bytes('world'), rb = bytes('rails');
      const terrain = eventTerrainParts(dec.decode(tb)), worldParts = eventWorldParts(dec.decode(wb)), rails = eventRailParts(dec.decode(rb));
      return { hash: terrain.hash, keys: { world: parseKey(wb, terrain.hash), body: parseKey(tb, terrain.hash), rails: parseKey(rb, terrain.hash) },
        terrain: { head: terrain.head, parts: terrain.parts }, world: worldParts, rails };
    }
    throw Error(`Unknown cut ${cut}`);
  }
  if (env) { // a location's environment slice (the whole mountain: MOUNTAIN/ENV/<LOC>.json + .bin), cut into small core calls
    const [doc, bytes] = await Promise.all([fetch(env + '.json').then((r) => { if (!r.ok) throw Error(`${env}.json: ${r.status}`); return r.json(); }),
      fetch(env + '.bin').then((r) => { if (!r.ok) throw Error(`${env}.bin: ${r.status}`); return r.arrayBuffer(); })]);
    const batches = environmentBatches(doc, new Uint8Array(bytes));
    return withTransfer({ batches, track: doc.track }, batches.map((b) => b.bytes.buffer));
  }
  // the record's painter sections (fetched alongside the collision data)
  const text = (f) => fetch(root + f).then((r) => (r.ok ? r.text() : null)).catch(() => null);
  const painterTexts = painters ? Promise.all(['fog-tree.json', 'lighting.json', 'screen-tint.json', 'sun-painter.json', 'glare-painter.json'].map(text)) : Promise.resolve(null);
  const get = async (f) => { const r = await fetch(root + f); if (!r.ok) throw Error(`${root}${f}: ${r.status}`); return r.json(); };
  // weather.json: the location record's Weather section (web/weather.inc weather_location; none for a location without one)
  const [terrain, worldDoc, rails, weather] = await Promise.all([get('terrain.json'), get('world_collision.json'), get('rails.json'),
    fetch(root + 'weather.json').then((r) => (r.ok ? r.text() : null)).catch(() => null)]);
  // railParts: sliceLoad (the rail catalog in parts) // the manifest's world (MOUNTAIN), else the world of /assets/PEAK<N>/<LOC>/
  const batches = locationBatches(terrain, worldDoc, rails, world ?? /\/(PEAK\d)\//.exec(root)?.[1] ?? 'PEAK1', { railParts });
  // regionTick (web/painter-regions.js): the record's other painter sections, in before any of its patches can be touched.
  // fog / lighting as text (the core parses them: fog_location / lighting_location), ScreenTint / Sun / glare parsed (null: none).
  const docs = await painterTexts;
  const parse = (t) => { try { return t ? JSON.parse(t) : null; } catch { return null; } };
  const painterDocs = docs && { fog: docs[0], lighting: docs[1], tint: parse(docs[2]), sun: parse(docs[3]), glare: parse(docs[4]) };
  // the location's ground extent (scene metres, from its terrain patches' authored bounds): web/free-ride.js streamAhead picks the
  // connector the rider is nearest (instance bounds are no use: A_ASS1 has an instance at the origin)
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const p of terrain.patches) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], p.authored_bounds_min[k]); hi[k] = Math.max(hi[k], p.authored_bounds_max[k]); }
  return {
    hash: terrain.source_sha256,
    batches,
    weather,
    painters: painterDocs,
    bounds: terrain.patches.length ? [lo, hi] : null,
    counts: { patches: terrain.patches.length, instances: worldDoc.instances.length, rails: rails.rails.length }
  };
}
