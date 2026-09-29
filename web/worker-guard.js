// Stale-safe workers (docs/workers.md). Every worker of the game starts through createGuardedWorker:
// - the script URL is the one Vite gives `new Worker(new URL('./x.js', import.meta.url))`: a content-hashed file of the
//   same build as the page (workerUrl() captures it without starting a worker; node gets the file URL);
// - build handshake: the worker's first message names the build it came from (web/build-id.js, web/worker-guard-child.js).
//   Another build (a stale cache, an old page after a deploy, a dev page older than the worker's code): diagnostics
//   'worker-stale', the worker is terminated and fetched once more with a cache-busting query; still wrong -> main thread;
// - a worker that does not start (404 after a deploy, no module workers, a script error), fails later ('error',
//   'messageerror') or stops answering (a ping unanswered for stallTimeoutMs while it has work) is terminated: request
//   workers restart once, then run their handler on the main thread; the requests in flight are replayed there;
// - a back/forward-cache restore pings the worker (5 s) and replaces it when it does not answer; the watchdog pauses
//   while the page is hidden. ?workers=0 runs every worker's handler on the main thread.
// Request mode: guard.request(data, transfer) -> Promise(reply). Raw mode ({raw: true}): a Worker-like object
// (postMessage / onmessage / onerror / terminate) for workers with their own protocol (web/terrain-worker.js).
import { BUILD_ID, expectedWorkerBuild } from './build-id.js';
import { unwrapReply } from './worker-guard-child.js';
import { diagnose } from './diagnostics.js';
import { checkBuild } from './build-check.js';

const search = globalThis.location?.search ?? '';
const FORCE_LOCAL = /[?&]workers=0\b/.test(search);
// ?localWorkers=terrain,peak-world (QA): those workers' handlers on the main thread, the others as usual.
const LOCAL_NAMES = new Set(decodeURIComponent(/[?&]localWorkers=([^&]*)/.exec(search)?.[1] ?? '').split(',').filter(Boolean));
const now = () => (globalThis.performance?.now?.() ?? Date.now());
const clone = (data) => { try { return typeof structuredClone === 'function' ? structuredClone(data) : data; } catch { return data; } };
const message = (e) => String(e?.message ?? e ?? '').slice(0, 300);
let reporter = (kind, data) => { try { diagnose(kind, data); } catch {} if (typeof window !== 'undefined') try { console.info(`[workers] ${kind}`, data); } catch {} };
export function setWorkerReporter(fn) { const previous = reporter; reporter = fn; return previous; } // tests
const report = (kind, data) => reporter(kind, data);

// The URL Vite built for a worker, without starting it: create = (Worker) => new Worker(new URL('./x.js', import.meta.url), {type: 'module'})
// (Vite rewrites that literal pattern to the hashed file; the parameter shadows the real Worker).
export function workerUrl(create) {
  let url = null;
  try { create(class { constructor(u) { url = String(u); } }); } catch {}
  return url;
}
const withQuery = (url, key, value) => `${url}${url.includes('?') ? '&' : '?'}${key}=${encodeURIComponent(value)}`;

const guards = new Set();
let pageVisible = typeof document === 'undefined' || document.visibilityState !== 'hidden';
if (typeof document !== 'undefined' && typeof addEventListener === 'function') {
  addEventListener('pageshow', (e) => { if (e.persisted) for (const g of [...guards]) g._pageshow(); });
  document.addEventListener('visibilitychange', () => { pageVisible = document.visibilityState !== 'hidden'; for (const g of [...guards]) g._visibility(pageVisible); });
}
export const workerGuards = () => [...guards].map((g) => g.status());
globalThis.ssxWorkers = workerGuards; // QA: every live guard and its mode

export function createGuardedWorker({ name, url, local = null, raw = false, onMessage = null, onError = null, busy = null, cloneInput = true,
  helloTimeoutMs = 15000, stallTimeoutMs = 30000, pingMs = 5000, maxRestarts = 1, revalidateMs = 5000 } = {}) {
  let state = 'starting', worker = null, attempt = 0, restarts = 0, seq = 0, reqId = 0, info = null, failures = 0;
  let helloTimer = 0, pingTimer = 0, pingSent = 0, pingDeadline = 0, lastHeard = 0, localHandler = null, localReceive = null;
  const queue = [], inflight = new Map(), facade = { onmessage: null, onerror: null };
  let settleReady; const ready = new Promise((r) => { settleReady = r; });

  function spawn(bust) {
    state = 'starting';
    if (!url || typeof Worker === 'undefined' || FORCE_LOCAL || LOCAL_NAMES.has(name)) return fallback(FORCE_LOCAL || LOCAL_NAMES.has(name) ? 'disabled' : !url ? 'no-url' : 'unsupported');
    try { worker = new Worker(bust ? withQuery(url, 'v', `${BUILD_ID}.${Date.now().toString(36)}`) : url, { type: 'module', name: `ssx-${name}` }); }
    catch (e) { return fallback(`construct: ${message(e)}`); }
    const w = worker;
    w.onmessage = (e) => { if (w === worker) received(e.data); };
    w.onerror = (e) => { e?.preventDefault?.(); if (w === worker) failed('error', e?.message || 'the script did not load'); };
    w.onmessageerror = () => { if (w === worker) failed('messageerror', 'a message could not be deserialized'); };
    armHello();
  }
  function armHello() { clearTimeout(helloTimer); helloTimer = setTimeout(() => { if (!pageVisible) armHello(); else failed('hello-timeout', `no hello in ${helloTimeoutMs} ms`); }, helloTimeoutMs); }
  function kill() {
    clearTimeout(helloTimer); helloTimer = 0;
    if (worker) { const w = worker; worker = null; w.onmessage = w.onerror = w.onmessageerror = null; try { w.terminate(); } catch {} }
  }
  function received(d) {
    lastHeard = now();
    if (d && typeof d === 'object' && d.__ssxw) {
      if (d.__ssxw === 'hello') hello(d);
      else if (d.__ssxw === 'pong') pingSent = 0;
      else if (d.__ssxw === 'res') settle(d);
      return;
    }
    if (state === 'worker') deliver(d);
  }
  function deliver(data) { onMessage?.(data); facade.onmessage?.({ data }); }
  function hello(d) {
    if (state !== 'starting') return;
    clearTimeout(helloTimer);
    const expected = expectedWorkerBuild(name);
    info = { build: d.build, page: d.page };
    if (d.build !== expected) {
      const retry = attempt === 0;
      report('worker-stale', { worker: name, expected, got: d.build, attempt, action: retry ? 'refetch' : 'main-thread' });
      checkBuild('worker-mismatch');
      kill();
      if (retry) { attempt = 1; spawn(true); } else fallback('stale');
      return;
    }
    state = 'worker'; settleReady('worker');
    if (attempt > 0) report('worker-recovered', { worker: name, attempt, build: d.build });
    for (const q of queue.splice(0)) worker.postMessage(q.data, q.transfer);
    for (const job of inflight.values()) send(job);
    startPing();
  }
  function send(job) { job.sent = true; worker.postMessage({ __ssxw: 'req', id: job.id, data: job.data }, job.transfer); }
  function settle(d) {
    const job = inflight.get(d.id); if (!job) return;
    inflight.delete(d.id);
    if (d.error != null) job.reject(new Error(d.error)); else job.resolve(d.data);
  }
  // A request whose buffers went to the dead worker (transferred) cannot run again.
  const replayable = (job) => !(job.sent && job.transfer?.length);
  function failed(kind, detail) {
    const starting = state === 'starting';
    kill(); failures++;
    if (starting) {
      report('worker-start-failed', { worker: name, reason: kind, message: detail, attempt, action: attempt === 0 ? 'refetch' : 'main-thread' });
      if (attempt === 0) { attempt = 1; spawn(true); } else fallback(kind);
      return;
    }
    if (state !== 'worker') return;
    stopPing();
    report('worker-failed', { worker: name, reason: kind, message: detail, restarts, action: raw ? 'caller' : restarts < maxRestarts ? 'restart' : 'main-thread' });
    if (raw) { // the worker's state (e.g. the terrain patches) died with it: the caller decides
      state = 'failed'; settleReady('failed');
      const error = { message: `${name} worker ${kind}: ${detail}` }; onError?.(error); facade.onerror?.(error);
      return;
    }
    for (const job of [...inflight.values()]) if (!replayable(job)) { inflight.delete(job.id); job.reject(new Error(`${name} worker ${kind}`)); } else job.sent = false;
    if (restarts < maxRestarts) { restarts++; attempt = 0; spawn(false); } else fallback(kind);
  }
  function fallback(reason) {
    kill(); stopPing();
    if (!local) {
      state = 'failed'; settleReady('failed');
      report('worker-unavailable', { worker: name, reason });
      for (const job of inflight.values()) job.reject(new Error(`${name} worker unavailable: ${reason}`));
      inflight.clear(); queue.length = 0;
      const error = { message: `${name} worker unavailable: ${reason}` }; onError?.(error); facade.onerror?.(error);
      return;
    }
    state = 'local'; settleReady('local');
    report('worker-fallback', { worker: name, reason, build: BUILD_ID });
    if (raw) {
      localReceive = local((data) => queueMicrotask(() => { if (state === 'local') deliver(data); }));
      for (const q of queue.splice(0)) postLocal(q.data);
    } else {
      for (const job of [...inflight.values()]) {
        if (replayable(job)) runLocal(job);
        else { inflight.delete(job.id); job.reject(new Error(`${name} worker failed with the request in flight`)); }
      }
    }
  }
  function postLocal(data) { const input = cloneInput ? clone(data) : data; Promise.resolve().then(() => localReceive?.({ data: input })).catch((e) => console.error(`${name} (main thread)`, e)); }
  function runLocal(job) {
    job.sent = true; localHandler ??= local();
    const input = cloneInput ? clone(job.data) : job.data; // the handler may change its input (a worker gets a copy)
    Promise.resolve().then(() => localHandler(input)).then(
      (out) => { if (inflight.delete(job.id)) job.resolve(unwrapReply(out).reply); },
      (e) => { if (inflight.delete(job.id)) job.reject(e instanceof Error ? e : new Error(String(e))); });
  }
  function startPing() { stopPing(); pingSent = 0; pingTimer = setInterval(tickPing, pingMs); }
  function stopPing() { clearInterval(pingTimer); pingTimer = 0; pingSent = 0; }
  function ping(deadlineMs) { pingSent = now(); pingDeadline = pingSent + deadlineMs; worker.postMessage({ __ssxw: 'ping', seq: ++seq }); }
  function tickPing() {
    if (state !== 'worker' || !pageVisible) return;
    const t = now();
    if (pingSent) { if (t > pingDeadline && t - lastHeard > pingDeadline - pingSent) failed('unresponsive', `no answer for ${Math.round((t - pingSent) / 1000)} s`); return; }
    if (busy ? busy() : raw || inflight.size > 0) ping(stallTimeoutMs);
  }

  function request(data, transfer = []) {
    return new Promise((resolve, reject) => {
      if (state === 'closed' || state === 'failed') { reject(new Error(`${name} worker ${state}`)); return; }
      const job = { id: ++reqId, data, transfer, resolve, reject, sent: false };
      inflight.set(job.id, job);
      if (state === 'worker') send(job); else if (state === 'local') runLocal(job);
    });
  }
  function postMessage(data, transfer = []) {
    if (state === 'worker') worker.postMessage(data, transfer);
    else if (state === 'local') postLocal(data);
    else if (state === 'starting') queue.push({ data, transfer });
  }
  function terminate() {
    if (state === 'closed') return;
    state = 'closed'; settleReady('closed'); kill(); stopPing(); guards.delete(api);
    inflight.clear(); queue.length = 0; localReceive = null; localHandler = null;
  }
  const api = {
    name, ready, request, postMessage, terminate,
    get state() { return state; },
    get onmessage() { return facade.onmessage; }, set onmessage(f) { facade.onmessage = f; },
    get onerror() { return facade.onerror; }, set onerror(f) { facade.onerror = f; },
    status: () => ({ name, state, attempt, restarts, failures, build: info?.build ?? null, inflight: inflight.size }),
    _visibility(visible) { if (visible) { lastHeard = now(); if (pingSent) { const span = pingDeadline - pingSent; pingSent = now(); pingDeadline = pingSent + span; } } },
    _pageshow() {
      checkBuild('pageshow');
      if (state === 'worker') { ping(revalidateMs); setTimeout(tickPing, revalidateMs + 50); }
      else if (state === 'starting') armHello();
    },
  };
  guards.add(api);
  spawn(false);
  return api;
}
