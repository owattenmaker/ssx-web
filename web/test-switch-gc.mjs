// web/switch-gc.js (pv switchGC): the kick runs only while an earlier core's memory is alive (FinalizationRegistry, no deref), stops
// once they are collected, or once a full collection (an aged marker freed) ran without freeing them, and gives up at the timeout.
import assert from 'node:assert/strict';
import { collectBefore, collectNow, createCoreTracker } from './switch-gc.js';
let t = 0; const now = () => t;
// a fake registry: collect(o) runs the cleanup as a collection would
const regs = []; class Registry { constructor(cb) { this.cb = cb; this.ids = new Map(); regs.push(this); } register(o, id) { this.ids.set(o, id); } }
const collect = (o) => { for (const r of regs) if (r.ids.has(o)) { const id = r.ids.get(o); r.ids.delete(o); r.cb(id); } };
const collectMarkers = (tr) => { for (const r of regs) for (const [o] of [...r.ids]) if (o && typeof o.t === 'number') collect(o); };
const tracker = () => { const tr = createCoreTracker({ Registry, every: null, now }); return tr; };
{ const tr = tracker(); let made = 0;
  const r = await collectBefore(tr, { now, sleep: async (ms) => { t += ms; }, makeMemory: () => ++made }); assert.deepEqual(r, { alive: 0, freed: true, memories: 0, ms: 0 }); assert.equal(made, 0, 'nothing alive: no kick'); }
{ t = 0; const tr = tracker(), a = {}, b = {}; tr.track(a); tr.track(b); assert.equal(tr.live, 2); let made = 0;
  const r = await collectBefore(tr, { now, stepMs: 50, makeMemory: () => ({ n: ++made }), sleep: async (ms) => { t += ms; if (made >= 3) { collect(a); collect(b); } } });
  assert.equal(r.freed, true); assert.equal(r.memories, 3); assert.equal(r.ms, 150, 'freed by the collection after the third memory'); assert.equal(tr.live, 0); }
{ t = 0; const tr = tracker(), kept = {}; tr.track(kept); let made = 0;
  const r = await collectBefore(tr, { now, sleep: async (ms) => { t += ms; }, timeoutMs: 800, stepMs: 50, maxMemories: 6, makeMemory: () => ++made });
  assert.equal(r.freed, false); assert.equal(r.left, 1); assert.equal(made, 6, 'at most maxMemories'); assert.ok(r.ms >= 800 && r.ms < 900, 'gives up at the timeout'); }
{ // a full collection that does not free it (still held): stops one step after it
  t = 0; const tr = tracker(), kept = {}; tr.track(kept); for (let i = 0; i < 4; i++) { tr.mark(); t += 1000; }
  let made = 0; const r = await collectBefore(tr, { now, stepMs: 50, makeMemory: () => ++made, sleep: async (ms) => { t += ms; if (made === 2) collectMarkers(tr); } });
  assert.equal(r.full, true); assert.equal(r.freed, false); assert.equal(made, 2, 'no memory after the full collection'); assert.ok(r.ms < 200); }
{ // the memories' callback a step after the markers': freed
  t = 0; const tr = tracker(), a = {}; tr.track(a); for (let i = 0; i < 4; i++) { tr.mark(); t += 1000; } let n = 0;
  const r = await collectBefore(tr, { now, stepMs: 50, makeMemory: () => ({}), sleep: async (ms) => { t += ms; n++; if (n === 1) collectMarkers(tr); if (n === 2) collect(a); } });
  assert.equal(r.full, true); assert.equal(r.freed, true); }
{ t = 0; const tr = tracker(), a = {}; tr.track(a); let n = 0;
  const r = await collectBefore(tr, { now, sleep: async (ms) => { t += ms; if (++n === 2) collect(a); }, makeMemory: () => null });
  assert.equal(r.freed, true); assert.equal(r.memories, 2, 'a refused memory still counts as a step'); }
{ const tr = createCoreTracker({ Registry: null }); tr.track({}); assert.equal(tr.live, 0, 'no FinalizationRegistry: nothing tracked, no kick'); }
{ // collectNow: kicks until a full collection is seen (an aged marker freed), else the timeout; nothing to watch before a marker aged
  t = 0; const tr = tracker(); tr.track({}); let made = 0;
  assert.deepEqual(await collectNow(tr, { now, sleep: async (ms) => { t += ms; }, makeMemory: () => ++made }), { full: false, memories: 0, ms: 0, noMarker: true }); assert.equal(made, 0);
  for (let i = 0; i < 4; i++) { tr.mark(); t += 1000; }
  const r = await collectNow(tr, { now, stepMs: 50, makeMemory: () => ++made, sleep: async (ms) => { t += ms; if (made === 3) collectMarkers(tr); } });
  assert.deepEqual(r, { full: true, memories: 3, ms: 150 });
  for (let i = 0; i < 4; i++) { tr.mark(); t += 1000; } made = 0;
  const q = await collectNow(tr, { now, stepMs: 50, timeoutMs: 600, maxMemories: 6, makeMemory: () => ++made, sleep: async (ms) => { t += ms; } });
  assert.equal(q.full, false); assert.equal(made, 6); assert.ok(q.ms >= 600 && q.ms < 700); }
console.log('switch-gc OK: kicks only while an earlier core lives, stops when it is collected or a full collection passed, gives up at the timeout');
