// Stale-safe workers (web/worker-guard.js, worker-guard-child.js, build-id.js, vite-build-id.js; docs/workers.md), with
// a scripted Worker: the build handshake (same build, stale once then fresh after the cache-busting refetch, stale
// twice -> main thread), start failures (404 / script error, constructor throw, no hello), runtime failures (error,
// messageerror, a hung worker) with the requests in flight replayed, raw mode (queued messages run on the main thread;
// a crash after the start goes to the caller), back/forward-cache revalidation, terminate, and the build-id plugin
// (dev graph hashes follow a worker's own files only; the build value is written into build-id.js).
import assert from 'node:assert/strict';
import fs from 'node:fs'; import os from 'node:os'; import path from 'node:path';
import { createGuardedWorker, setWorkerReporter, workerGuards, workerUrl } from './worker-guard.js';
import { withTransfer } from './worker-guard-child.js';
import { BUILD_ID } from './build-id.js';
import { graphHash, fillBuildId, PROCESS_BUILD_ID } from './vite-build-id.js';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const events = []; setWorkerReporter((kind, data) => events.push({ kind, ...data }));
const FAST = { helloTimeoutMs: 80, stallTimeoutMs: 120, pingMs: 20, revalidateMs: 60 };

// A scripted worker: plan(w) decides how the instance behaves; handler answers requests like worker-guard-child.
class MockWorker {
  static created = []; static plan = null;
  constructor(url, options) {
    this.url = url; this.options = options; this.terminated = false; this.got = [];
    MockWorker.created.push(this);
    const plan = MockWorker.plan; setTimeout(() => { if (!this.terminated) plan(this); }, 1);
  }
  postMessage(data, transfer = []) { if (this.terminated) return; const copy = structuredClone(data, { transfer }); this.got.push(copy); setTimeout(() => !this.terminated && this.onPage?.(copy), 1); }
  terminate() { this.terminated = true; }
  emit(data) { if (!this.terminated) this.onmessage?.({ data }); }
  fail(message) { if (!this.terminated) this.onerror?.({ message, preventDefault() {} }); }
}
globalThis.Worker = MockWorker;
const handler = async (d) => { if (d.throws) throw Error('bad input'); d.touched = true; return withTransfer({ sum: d.a + d.b, where: d.where }, []); };
// A healthy worker of `build`, running `handler` (optionally hanging or dying on some request).
function serve(w, { build = BUILD_ID, hangOn = null, dieOn = null, messageErrorOn = null } = {}) {
  w.onPage = async (d) => {
    if (d.__ssxw === 'ping') { if (!w.hung) w.emit({ __ssxw: 'pong', seq: d.seq }); return; }
    if (d.__ssxw !== 'req') { w.emit({ echo: d }); return; }
    if (d.data.a === hangOn) { w.hung = true; return; }
    if (d.data.a === dieOn) { w.fail('boom'); return; }
    if (d.data.a === messageErrorOn) { w.onmessageerror?.({}); return; }
    try { const out = await handler({ ...d.data, where: 'worker' }); w.emit({ __ssxw: 'res', id: d.id, data: out.reply }); }
    catch (e) { w.emit({ __ssxw: 'res', id: d.id, error: String(e.message) }); }
  };
  w.emit({ __ssxw: 'hello', name: 'test', build });
}
const reset = () => { MockWorker.created.length = 0; events.length = 0; };
const local = () => (d) => handler({ ...d, where: 'main' });

// workerUrl: captures the URL without starting a worker (Vite rewrites the literal; node gets the file URL).
assert.match(workerUrl((Worker) => new Worker(new URL('./peak-world-worker.js', import.meta.url), { type: 'module' })), /^file:.*peak-world-worker\.js$/);
assert.equal(MockWorker.created.length, 0, 'workerUrl never starts a worker');

// 1. Same build: requests answered by the worker, in order; the handler's input is the worker's copy.
reset(); MockWorker.plan = (w) => serve(w);
let g = createGuardedWorker({ name: 'test', url: '/assets/test-AbCdEf12.js', local, ...FAST });
const input = { a: 1, b: 2 };
assert.deepEqual(await g.request(input), { sum: 3, where: 'worker' });
assert.equal(input.touched, undefined); assert.equal(g.state, 'worker'); assert.equal(await g.ready, 'worker');
assert.equal(MockWorker.created[0].options.type, 'module'); assert.equal(MockWorker.created[0].url, '/assets/test-AbCdEf12.js');
await assert.rejects(g.request({ a: 1, b: 2, throws: true }), /bad input/);
assert.equal(events.length, 0, 'no diagnostics on the happy path');
g.terminate(); assert.equal(g.state, 'closed'); assert.ok(MockWorker.created[0].terminated);

// 2. Stale once (an old cached copy), fresh after the cache-busting refetch: diagnostics either way.
reset(); MockWorker.plan = (w) => serve(w, { build: /[?&]v=/.test(w.url) ? BUILD_ID : 'b-old' });
g = createGuardedWorker({ name: 'test', url: '/assets/test-AbCdEf12.js', local, ...FAST });
const early = g.request({ a: 2, b: 2 }); // posted before the handshake: held back, then sent to the right worker
assert.deepEqual(await early, { sum: 4, where: 'worker' });
assert.equal(MockWorker.created.length, 2); assert.ok(MockWorker.created[0].terminated);
assert.match(MockWorker.created[1].url, /^\/assets\/test-AbCdEf12\.js\?v=/);
assert.equal(MockWorker.created[0].got.filter((m) => m.__ssxw === 'req').length, 0, 'nothing reached the stale worker');
assert.deepEqual(events.map((e) => e.kind), ['worker-stale', 'worker-recovered']);
assert.equal(events[0].got, 'b-old'); assert.equal(events[0].action, 'refetch');
g.terminate();

// 3. Stale twice: main thread; the queued request runs there, on a copy of its input.
reset(); MockWorker.plan = (w) => serve(w, { build: 'b-old' });
g = createGuardedWorker({ name: 'test', url: '/assets/test-AbCdEf12.js', local, ...FAST });
const input3 = { a: 5, b: 1 };
assert.deepEqual(await g.request(input3), { sum: 6, where: 'main' });
assert.equal(input3.touched, undefined, 'the main-thread handler gets a copy (as a worker would)');
assert.equal(g.state, 'local'); assert.deepEqual(events.map((e) => e.kind), ['worker-stale', 'worker-stale', 'worker-fallback']);
assert.equal(events[1].action, 'main-thread'); assert.ok(MockWorker.created.every((w) => w.terminated));
assert.deepEqual(await g.request({ a: 1, b: 1 }), { sum: 2, where: 'main' }); g.terminate();

// 4. The script does not load (a 404 after a deploy, a syntax error): refetch, then main thread.
reset(); MockWorker.plan = (w) => w.fail('404');
g = createGuardedWorker({ name: 'test', url: '/assets/gone-AbCdEf12.js', local, ...FAST });
assert.deepEqual(await g.request({ a: 1, b: 3 }), { sum: 4, where: 'main' });
assert.deepEqual(events.map((e) => e.kind), ['worker-start-failed', 'worker-start-failed', 'worker-fallback']); g.terminate();
// No hello at all (a worker that never starts): the hello timeout.
reset(); MockWorker.plan = () => {};
g = createGuardedWorker({ name: 'test', url: '/x.js', local, ...FAST });
assert.deepEqual(await g.request({ a: 1, b: 1 }), { sum: 2, where: 'main' }); assert.equal(events[0].kind, 'worker-start-failed'); assert.equal(events[0].reason, 'hello-timeout'); g.terminate();
// The constructor throws (no module workers): main thread at once, no refetch.
reset(); globalThis.Worker = class { constructor() { throw new TypeError('Module scripts are not supported'); } };
g = createGuardedWorker({ name: 'test', url: '/x.js', local, ...FAST });
assert.equal(g.state, 'local'); assert.deepEqual(await g.request({ a: 2, b: 3 }), { sum: 5, where: 'main' });
assert.deepEqual(events.map((e) => e.kind), ['worker-fallback']); g.terminate();
globalThis.Worker = MockWorker;
// Without a main-thread handler: requests reject, the caller hears onError.
reset(); MockWorker.plan = (w) => w.fail('404'); let heard = null;
g = createGuardedWorker({ name: 'test', url: '/x.js', onError: (e) => { heard = e.message; }, ...FAST });
await assert.rejects(g.request({ a: 1, b: 1 }), /unavailable/); assert.match(heard, /unavailable/); g.terminate();

// 5. Runtime failures after a good start: restart once (requests in flight replayed), then the main thread.
reset(); MockWorker.plan = (w) => serve(w, { dieOn: 7 });
g = createGuardedWorker({ name: 'test', url: '/x.js', local, ...FAST });
await g.ready;
assert.deepEqual(await g.request({ a: 7, b: 1 }).catch((e) => e.message), { sum: 8, where: 'main' }, 'the request that killed two workers finished on the main thread');
assert.equal(MockWorker.created.length, 2); assert.deepEqual(events.map((e) => e.kind), ['worker-failed', 'worker-failed', 'worker-fallback']);
assert.equal(events[0].action, 'restart'); g.terminate();
// messageerror counts as a failure too; a transferred buffer cannot be replayed (the request rejects).
reset(); MockWorker.plan = (w) => serve(w, { messageErrorOn: 9 });
g = createGuardedWorker({ name: 'test', url: '/x.js', local, maxRestarts: 0, ...FAST }); await g.ready;
const buf = new ArrayBuffer(8);
await assert.rejects(g.request({ a: 9, b: 1, buf }, [buf]), /messageerror|in flight/);
assert.equal(buf.byteLength, 0, 'transferred'); assert.equal(g.state, 'local'); g.terminate();
// A hung worker: no pong while it has work -> terminated, the request replayed on a new worker.
reset(); let n = 0; MockWorker.plan = (w) => serve(w, { hangOn: n++ === 0 ? 4 : null });
g = createGuardedWorker({ name: 'test', url: '/x.js', local, ...FAST }); await g.ready;
assert.deepEqual(await g.request({ a: 4, b: 4 }), { sum: 8, where: 'worker' });
assert.equal(events[0].kind, 'worker-failed'); assert.equal(events[0].reason, 'unresponsive'); assert.match(events[0].message, /no answer/);
assert.ok(MockWorker.created[0].terminated); g.terminate();
// An idle worker is not pinged (no work, no timer traffic).
reset(); MockWorker.plan = (w) => serve(w);
g = createGuardedWorker({ name: 'test', url: '/x.js', local, ...FAST }); await g.ready; await sleep(100);
assert.equal(MockWorker.created[0].got.filter((m) => m.__ssxw === 'ping').length, 0); g.terminate();

// 6. Back/forward cache restore: a worker that does not answer within revalidateMs is replaced.
reset(); n = 0; MockWorker.plan = (w) => { serve(w); if (n++ === 0) w.hung = true; };
g = createGuardedWorker({ name: 'test', url: '/x.js', local, ...FAST }); await g.ready;
g._pageshow(); await sleep(200);
assert.equal(MockWorker.created.length, 2); assert.ok(MockWorker.created[0].terminated); assert.equal(g.state, 'worker');
assert.equal(events[0].kind, 'worker-failed'); g.terminate();
// ...and one that answers is kept.
reset(); MockWorker.plan = (w) => serve(w);
g = createGuardedWorker({ name: 'test', url: '/x.js', local, ...FAST }); await g.ready; g._pageshow(); await sleep(150);
assert.equal(MockWorker.created.length, 1); assert.equal(events.length, 0); g.terminate();

// 7. Raw mode (a worker with its own protocol, web/terrain-worker.js): messages queued before the handshake reach the
// worker; if it cannot start they run on the main thread (on a copy: the caller may drop fields afterwards).
reset(); MockWorker.plan = (w) => serve(w);
let got = []; g = createGuardedWorker({ name: 'test', url: '/x.js', raw: true, local: (send) => ({ data }) => send({ echo: data, main: true }), ...FAST });
g.onmessage = ({ data }) => got.push(data); g.postMessage({ type: 'init', n: 1 }); await sleep(30);
assert.deepEqual(got, [{ echo: { type: 'init', n: 1 } }]); g.terminate();
reset(); MockWorker.plan = (w) => w.fail('404'); got = [];
g = createGuardedWorker({ name: 'test', url: '/x.js', raw: true, local: (send) => ({ data }) => { data.patches.length = 0; send({ echo: 'init', main: true }); }, ...FAST });
g.onmessage = ({ data }) => got.push(data); const patches = [1, 2, 3]; g.postMessage({ type: 'init', patches }); await sleep(40);
assert.deepEqual(got, [{ echo: 'init', main: true }]); assert.equal(patches.length, 3, 'main-thread handler worked on a copy'); g.terminate();
// A raw worker that dies after the start: its state is gone, the caller's onerror decides (no silent restart).
reset(); MockWorker.plan = (w) => serve(w); let rawError = null;
g = createGuardedWorker({ name: 'test', url: '/x.js', raw: true, local: (send) => () => send({}), ...FAST }); await g.ready;
g.onerror = (e) => { rawError = e.message; }; MockWorker.created[0].fail('crash'); await sleep(5);
assert.match(rawError, /test worker error: crash/); assert.equal(g.state, 'failed'); assert.equal(MockWorker.created.length, 1); g.terminate();

// 8. Terminate: no callbacks afterwards; the registry forgets the guard (course switches do not leak workers).
reset(); MockWorker.plan = (w) => serve(w);
const before = workerGuards().length;
g = createGuardedWorker({ name: 'test', url: '/x.js', raw: true, local: null, ...FAST }); got = [];
g.onmessage = ({ data }) => got.push(data); await g.ready; g.postMessage({ late: 1 }); g.terminate(); await sleep(20);
assert.deepEqual(got, []); assert.equal(workerGuards().length, before); assert.ok(MockWorker.created[0].terminated);

// 9. The build-id plugin: dev hashes follow a worker's own module graph; the build value lands in build-id.js.
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-build-id-'));
const put = (f, text) => fs.writeFileSync(path.join(dir, f), text);
put('w.js', "import { x } from './dep.js';\nimport './side.js';\n"); put('dep.js', 'export const x = 1;\n'); put('side.js', '\n'); put('other.js', 'export {}\n');
const h0 = graphHash('w.js', dir);
put('other.js', 'export const unrelated = 2;\n'); assert.equal(graphHash('w.js', dir), h0, 'a file outside the worker graph does not change its id');
put('dep.js', 'export const x = 22;\n'); const h1 = graphHash('w.js', dir); assert.notEqual(h1, h0, 'an import of the worker changed');
put('side.js', 'export const y = 3;\n'); assert.notEqual(graphHash('w.js', dir), h1, 'a side-effect import of the worker changed');
fs.rmSync(dir, { recursive: true, force: true });
const src = fs.readFileSync(new URL('./build-id.js', import.meta.url), 'utf8');
const filled = fillBuildId(src, PROCESS_BUILD_ID, null);
assert.ok(filled.includes(`export const BUILD_ID = "${PROCESS_BUILD_ID}";`) && filled.includes('export const WORKER_BUILDS = null;'), 'build-id.js markers filled');
assert.ok(fillBuildId(src, 'dev-1', { terrain: 'dev-abc' }).includes('WORKER_BUILDS = {"terrain":"dev-abc"}'));
assert.equal(BUILD_ID, 'unbuilt', 'without Vite the module keeps its defaults');

console.log('worker guard: same build, stale -> refetch -> ok, stale twice -> main thread, start failures (404, no hello, no module workers), runtime error / messageerror / hang with replay, bfcache revalidation, raw mode, terminate, build-id plugin');
