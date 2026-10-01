// QA page for the stale-safe workers (docs/workers.md): starts each of the game's workers through web/worker-guard.js
// exactly as the game does (same scripts, same main-thread handlers) and runs one small request on each. Writes
// {build, workers: {name: {state, ms, ok, build}}, events} into #result and POSTs it to /qa-result when served by a
// QA server (local/browser-validation harnesses). ?workers=0 forces the main thread.
import { BUILD_ID } from './build-id.js';
import { createGuardedWorker, workerUrl, setWorkerReporter, workerGuards } from './worker-guard.js';
import { createTerrainWorkerHandler } from './terrain-worker-core.js';
import { preparePeakLocation } from './peak-world-prepare.js';
import { prepareFrontEndPreview } from './fe-preview-prepare.js';
import { decodeAudioJob } from './audio-decode-job.js';
import { buildState } from './build-check.js';

const events = [], previous = setWorkerReporter((kind, data) => { events.push({ kind, ...data }); previous(kind, data); });
const URLS = {
  'peak-world': workerUrl((Worker) => new Worker(new URL('./peak-world-worker.js', import.meta.url), { type: 'module' })),
  terrain: workerUrl((Worker) => new Worker(new URL('./terrain-worker.js', import.meta.url), { type: 'module' })),
  'fe-preview': workerUrl((Worker) => new Worker(new URL('./fe-preview-worker.js', import.meta.url), { type: 'module' })),
  'audio-decode': workerUrl((Worker) => new Worker(new URL('./audio-decode-worker.js', import.meta.url), { type: 'module' })),
};
const checks = {
  // A request that fails in the handler (a missing file): the error comes back through the same channel.
  'peak-world': (g) =>
    g.request({ env: '/assets/__missing__/none' }).then(
      () => false,
      (e) => !!e.message
    ),
  'audio-decode': (g) => g.request({ bnk: new Uint8Array(0), patches: [] }).then((d) => Array.isArray(d) && d.length === 0),
  'fe-preview': (g) =>
    g.request({ root: '/assets/__missing__/' }).then(
      () => false,
      (e) => !!e.message
    ),
  terrain: (g) =>
    new Promise((resolve) => {
      g.onmessage = ({ data }) => resolve(data.type === 'cleared' && data.id === 7);
      g.onerror = () => resolve(false);
      g.postMessage({ type: 'clear', id: 7 });
    })
};
const locals = { 'peak-world': () => preparePeakLocation, 'fe-preview': () => prepareFrontEndPreview, 'audio-decode': () => decodeAudioJob, terrain: (send) => createTerrainWorkerHandler(send) };
const out = { build: BUILD_ID, ua: navigator.userAgent, workers: {}, events };
try {
  await Promise.all(Object.keys(URLS).map(async (name) => {
    const t0 = performance.now();
    const g = createGuardedWorker({ name, url: URLS[name], local: locals[name], raw: name === 'terrain' });
    const state = await g.ready, ms = Math.round(performance.now() - t0);
    const ok = await Promise.race([checks[name](g), new Promise((r) => setTimeout(() => r('timeout'), 20000))]);
    out.workers[name] = { state, ms, ok, ...g.status(), url: URLS[name] };
    g.terminate();
  }));
  out.live = workerGuards().length; out.buildState = await Promise.resolve(buildState());
  out.passed = Object.values(out.workers).every((w) => w.ok === true);
} catch (e) { out.error = String(e?.stack || e); out.passed = false; }
document.querySelector('#result').textContent = JSON.stringify(out, null, 2);
fetch('/qa-result', { method: 'POST', body: JSON.stringify(out), headers: { 'content-type': 'application/json' } }).catch(() => {});
