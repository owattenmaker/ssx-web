// pipeline warms that outlive their course.
//
// three r186 Renderer.compileAsync lists the scene's render items synchronously, then builds them one at a time with a
// yieldToMain() between items, each against the render context captured at the call. The world warms (free-ride.js warmSliced,
// cutscene host.compile, both through fog-renderer.js compileObject) queue thousands of items; on WebKit they are still being
// built seconds later. main.js unloadCourse waits for the free-ride warms at most 1.5 s and then disposes the course,
// so the remaining items kept building after the dispose:
// - geometry, bindings and textures of disposed objects were uploaded again, and nothing disposes them a second time;
// - their pipelines hold usedTimes forever;
// - their render context's depth-stencil texture was already destroyed, so the pipelines failed to build: "Async render
//   pipeline creation failed: GPUDepthStencilState.format is required".
// The web memory runs (24 world loads, WebKit) showed textures, pipelines and WebContent malloc growing with every switch.
//
// trackCompiles(renderer) wraps compileAsync to capture the item list of each call. The list is the array three assigns to
// renderer._compilationPromises during the call's synchronous part. abortCompiles() empties every list still being built, so
// three's for...of loop ends after the item in flight, and waits for that item; a call made during that wait builds nothing (main.js
// calls it before freeRide.stop(), so a warm slice can still start meanwhile). The next course's warms are not touched.
const lists = new Map(); // item list -> the call's settled promise
export const compileAbortStats = { calls: 0, aborted: 0, items: 0, late: 0 }; // aborted calls, the items they still listed, calls made while closed
let closed = false; // while abortCompiles waits: a warm started meanwhile (a free-ride slice before its stop()) belongs to the course going away

export function trackCompiles(renderer) {
  if (!renderer || renderer.__compileTrack || typeof renderer.compileAsync !== 'function') return;
  renderer.__compileTrack = true;
  const compileAsync = renderer.compileAsync;
  renderer.compileAsync = function (...args) {
    let value = this._compilationPromises, captured = null;
    Object.defineProperty(this, '_compilationPromises', { configurable: true, enumerable: true,
      get: () => value, set: (v) => { if (captured === null && Array.isArray(v) && v !== value) captured = v; value = v; } });
    let p;
    try { p = compileAsync.apply(this, args); }
    finally { Object.defineProperty(this, '_compilationPromises', { configurable: true, enumerable: true, writable: true, value }); }
    if (captured && closed) { compileAbortStats.late++; compileAbortStats.items += captured.length; captured.length = 0; } // listed for a course being unloaded: nothing built
    else if (captured) { compileAbortStats.calls++; const list = captured; lists.set(list, Promise.resolve(p).catch(() => {}).finally(() => lists.delete(list))); }
    return p;
  };
}

// The compiles still building: [calls, items left].

// Empties every list in flight and waits (at most `ms`) for the items being built to finish. Resolves the items dropped.
export async function abortCompiles(ms = 1000) {
  if (!lists.size) return 0;
  let dropped = 0; for (const list of lists.keys()) { dropped += list.length; list.length = 0; }
  compileAbortStats.aborted += lists.size; compileAbortStats.items += dropped;
  closed = true;
  try { await Promise.race([Promise.all([...lists.values()]), new Promise((r) => setTimeout(r, ms))]); }
  finally { closed = false; }
  return dropped;
}
