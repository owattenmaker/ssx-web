// pv worldWarm: three r186's compile yields in browsers without scheduler.yield (Safari / WebKit).
//
// three's yieldToMain() (three.core.js) returns scheduler.yield() where it exists (Chrome) and otherwise waits for the next
// requestAnimationFrame. Renderer.compileAsync yields after every item it builds, and the async node build (NodeBuilder buildAsync)
// after every shader stage of every build stage. So in WebKit a compile advanced one item per frame, and a new node material took
// several frames: free-ride.js rewarm of a streamed world's start row (thousands of meshes, fog-renderer compileObject) ran for
// 30-50 s, far past the load screen, and the ride's first frames drew the locations it had not reached yet (Owen's Safari:
// 3000 buffers, 7000 bind groups, 20 pipelines in the first frame, a 3.3 s stall). Chrome's scheduler.yield continues within a
// few milliseconds, so there the same compile ends under the load screen.
//
// installYieldShim() gives WebKit a scheduler.yield of the same kind: the continuation runs as the next task (MessageChannel, so
// input and the page's other tasks run in between, as with Chrome's), and once yieldBudget.ms of wall time has passed since the last
// frame wait, the next yield waits for an animation frame, so a long compile never holds the frame loop for more than a few
// milliseconds a frame. A browser with its own scheduler.yield keeps it (Chrome is unchanged). Only when the compiles finish
// changes: they draw nothing, and the simulation does not use them.
// budget.ms: the compile work allowed per frame (main.js raises it while a load screen covers the frame: nothing else needs the time)
export const yieldBudget = { ms: 4 };

export function installYieldShim(g = globalThis) {
  if (typeof g.scheduler?.yield === 'function' || typeof g.MessageChannel !== 'function' || typeof g.requestAnimationFrame !== 'function') return false;
  const ch = new g.MessageChannel(), queue = [];
  ch.port1.onmessage = () => { const r = queue.shift(); if (r) r(); };
  let windowStart = -1;
  const next = () => new Promise((resolve) => { queue.push(resolve); ch.port2.postMessage(0); });
  const frame = () => new Promise((resolve) => g.requestAnimationFrame(() => { windowStart = -1; resolve(); }));
  const yieldNow = () => {
    const now = g.performance.now();
    if (windowStart < 0) windowStart = now;
    return now - windowStart < yieldBudget.ms ? next() : frame();
  };
  try {
    if (g.scheduler && typeof g.scheduler === 'object') g.scheduler.yield = yieldNow;
    else Object.defineProperty(g, 'scheduler', { value: { yield: yieldNow }, configurable: true, writable: true });
  } catch { return false; }
  return typeof g.scheduler?.yield === 'function';
}
