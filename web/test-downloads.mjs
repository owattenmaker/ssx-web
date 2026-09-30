// web/downloads.js retries (field reports: "Failed to fetch", Firefox "Error in input stream"): network errors, a body
// that breaks or stalls half way, and HTTP 408/429/5xx retry with backoff; 404 does not; the last failure rejects with
// network: true; progress counters roll back for a retried body; same-URL requests still share one download.
import assert from 'node:assert/strict';
globalThis.location = new URL('http://game.test/');
const script = []; let calls = 0;
const body = (chunks, { breakAfter = -1, stallAfter = -1 } = {}) => new ReadableStream({
  async start(c) {
    for (let i = 0; i < chunks.length; i++) {
      if (i === breakAfter) { c.error(new TypeError('Error in input stream')); return; }
      if (i === stallAfter) return; // never closes: the stall watchdog has to abort it
      c.enqueue(chunks[i]);
    }
    c.close();
  },
});
globalThis.fetch = async (url, init) => {
  calls++; const step = script.shift(); if (!step) throw Error('unexpected fetch ' + url);
  if (step.throw) throw new TypeError(step.throw);
  const stream = step.stream?.();
  if (!stream || !init?.signal) return new Response(stream ?? step.text ?? '', { status: step.status ?? 200, headers: step.headers ?? {} });
  { // like a real fetch body, the abort signal errors the stream (a stalled body only ends that way)
    const reader = stream.getReader(); const aborted = new Promise((_, j) => init.signal.addEventListener('abort', () => j(Object.assign(Error('aborted'), { name: 'AbortError' }))));
    aborted.catch(() => {});
    return new Response(new ReadableStream({ async pull(c) { const r = await Promise.race([reader.read(), aborted]); if (r.done) c.close(); else c.enqueue(r.value); } }), { status: step.status ?? 200, headers: step.headers ?? {} });
  }
};
const warnings = []; console.warn = (...a) => warnings.push(a.join(' '));
const { downloadRetry, downloadState } = await import('./downloads.js');
downloadRetry.delaysMs = [5, 5, 5, 5]; downloadRetry.stallMs = 150;
const bytes = (n, v) => new Uint8Array(n).fill(v);
const text = async (r) => new Uint8Array(await r.arrayBuffer());

// 1. network error, then success
script.push({ throw: 'Failed to fetch' }, { stream: () => body([bytes(3, 1), bytes(2, 2)]), headers: { 'content-length': '5' } });
let r = await fetch('/assets/a.bin'); assert.deepEqual([...await text(r)], [1, 1, 1, 2, 2]); assert.equal(calls, 2);
// 2. the body breaks half way (Firefox "Error in input stream"), then success; counters end consistent
calls = 0; script.push({ stream: () => body([bytes(4, 7), bytes(4, 8)], { breakAfter: 1 }), headers: { 'content-length': '8' } }, { stream: () => body([bytes(4, 7), bytes(4, 8)]), headers: { 'content-length': '8' } });
r = await fetch('/assets/b.bin'); assert.equal((await text(r)).length, 8); assert.equal(calls, 2);
assert.equal(downloadState.active, 0); assert.ok(downloadState.received <= downloadState.expected || downloadState.expected === 0, 'no double-counted bytes');
// 3. a stalled body: the watchdog aborts, the retry succeeds
calls = 0; script.push({ stream: () => body([bytes(2, 1), bytes(2, 2)], { stallAfter: 1 }) }, { stream: () => body([bytes(2, 1), bytes(2, 2)]) });
r = await fetch('/assets/c.bin'); assert.equal((await text(r)).length, 4); assert.equal(calls, 2);
// 4. HTTP 503 twice, then 200
calls = 0; script.push({ status: 503 }, { status: 503 }, { stream: () => body([bytes(1, 9)]) });
r = await fetch('/assets/d.bin'); assert.equal(r.status, 200); assert.equal(calls, 3);
// 5. 404 goes straight back
calls = 0; script.push({ status: 404, text: 'missing' });
r = await fetch('/assets/e.bin'); assert.equal(r.status, 404); assert.equal(calls, 1);
// 6. always failing: rejects after 1 + 4 tries with network: true
calls = 0; for (let i = 0; i < 5; i++) script.push({ throw: 'Failed to fetch' });
await assert.rejects(fetch('/assets/f.bin'), (e) => e.network === true && /Download failed: \/assets\/f\.bin/.test(e.message)); assert.equal(calls, 5);
// 7. two requests for one URL share a download
calls = 0; script.push({ stream: () => body([bytes(6, 3)]) });
const [x, y] = await Promise.all([fetch('/assets/g.bin'), fetch('/assets/g.bin')]); assert.equal((await text(x)).length, 6); assert.equal((await text(y)).length, 6); assert.equal(calls, 1);
// 8. not an asset: untouched
calls = 0; script.push({ text: 'ok' }); r = await fetch('/mp/lobbies'); assert.equal(await r.text(), 'ok'); assert.equal(calls, 1);
assert.ok(warnings.some((w) => /Download retry 1\/4: \/assets\/a\.bin \(Failed to fetch\)/.test(w)), 'retries are logged (field diagnostics)');
// 9. a prefetch (web/ctm-event.js, pv flyover): one download, its bytes kept for the first request (no Response copy meanwhile)
calls = 0; script.push({ stream: () => body([bytes(5, 4)]) });
const { prefetchDownload } = await import('./downloads.js');
assert.equal(await prefetchDownload('/assets/h.bin'), true); assert.equal(calls, 1);
await new Promise((res) => setTimeout(res, 20));
r = await fetch('/assets/h.bin'); assert.deepEqual([...await text(r)], [4, 4, 4, 4, 4]); assert.equal(calls, 1, 'the request takes the prefetched bytes');
calls = 0; script.push({ status: 404, text: 'missing' }); assert.equal(await prefetchDownload('/assets/i.bin'), false, 'a prefetch reports a failed file');
assert.equal(await prefetchDownload('/mp/lobbies'), false, 'not an asset: nothing');
// 10. a body with a known size lands in one buffer; a wrong Content-Length (short or long) still gives the real bytes
const { setPv } = await import('./pv-flags.js');
for (const on of [false, true]) { setPv('flyover', on);   // (the one-buffer read is behind pv flyover until verified)
calls = 0; script.push({ stream: () => body([bytes(3, 1), bytes(4, 2)]), headers: { 'content-length': '7' } });
r = await fetch(`/assets/j${on}.bin`); assert.deepEqual([...await text(r)], [1, 1, 1, 2, 2, 2, 2]);
calls = 0; script.push({ stream: () => body([bytes(3, 1), bytes(4, 2)]), headers: { 'content-length': '5' } });
r = await fetch(`/assets/k${on}.bin`); assert.deepEqual([...await text(r)], [1, 1, 1, 2, 2, 2, 2], 'longer than announced');
calls = 0; script.push({ stream: () => body([bytes(3, 1)]), headers: { 'content-length': '9' } });
r = await fetch(`/assets/l${on}.bin`); assert.deepEqual([...await text(r)], [1, 1, 1], 'shorter than announced');
calls = 0; script.push({ stream: () => body([bytes(2, 5), bytes(2, 6)]), headers: { 'x-decoded-length': '4', 'content-encoding': 'br' } });
r = await fetch(`/assets/m${on}.bin`); assert.deepEqual([...await text(r)], [5, 5, 6, 6], 'decoded length');
assert.equal(script.length, 0, 'every body read');
}
setPv('flyover', null);
// 11. pv loadCopies: every reader gets the Response's values from the shared bytes (and the one-buffer read of 10)
setPv('loadCopies', true);
{ const enc = new TextEncoder(), doc = '\uFEFF{"a":[1,2,3],"s":"\u00e9t\u00e9"}', raw = enc.encode(doc);
  const serve = (name, headers = { 'content-length': String(raw.length), 'content-type': 'application/json' }) => { calls = 0; script.push({ stream: () => body([raw.slice(0, 5), raw.slice(5)]), headers }); return fetch(`/assets/${name}`); };
  const want = await new Response(raw).text();
  r = await serve('n1.json'); assert.equal(r.bodyUsed, false); assert.deepEqual(await r.json(), JSON.parse(want)); assert.equal(r.bodyUsed, true);
  await assert.rejects(r.text(), TypeError, 'a body reads once');
  r = await serve('n2.json'); assert.equal(await r.text(), want, 'UTF-8, the BOM dropped as Response.text()');
  r = await serve('n3.json'); const ab = await r.arrayBuffer(); assert.ok(ab instanceof ArrayBuffer); assert.deepEqual([...new Uint8Array(ab)], [...raw]);
  r = await serve('n4.json'); const bl = await r.blob(); assert.equal(bl.type, 'application/json'); assert.deepEqual([...new Uint8Array(await bl.arrayBuffer())], [...raw]);
  r = await serve('n5.json'); const rd = r.body.getReader(); const got = []; for (;;) { const { done, value } = await rd.read(); if (done) break; got.push(...value); } assert.deepEqual(got, [...raw], 'the stream');
  await assert.rejects(r.json(), TypeError, 'read through the stream: used');
  r = await serve('n6.json'); const c2 = r.clone(); assert.deepEqual(await c2.json(), JSON.parse(want)); assert.deepEqual(await r.json(), JSON.parse(want), 'clone()');
  // two readers of one download: each its own buffer (the first transfers and edits its copy)
  calls = 0; script.push({ stream: () => body([bytes(4, 7)]), headers: { 'content-length': '4' } });
  const [p1, p2] = await Promise.all([fetch('/assets/n7.bin'), fetch('/assets/n7.bin')]); assert.equal(calls, 1);
  const b1 = await p1.arrayBuffer(); new Uint8Array(b1).fill(0); structuredClone(b1, { transfer: [b1] });
  assert.deepEqual([...new Uint8Array(await p2.arrayBuffer())], [7, 7, 7, 7], 'the shared bytes are untouched');
  r = await fetch('/assets/n7.bin'); assert.deepEqual([...new Uint8Array(await r.arrayBuffer())], [7, 7, 7, 7], 'a later reader within SHARE_MS');
  calls = 0; script.push({ status: 404, text: 'missing' }); r = await fetch('/assets/n8.bin'); assert.equal(r.status, 404); assert.equal(r.ok, false); assert.equal(await r.text(), 'missing');
  r = await serve('n9.json', { 'x-decoded-length': String(raw.length), 'content-encoding': 'br', 'content-type': 'application/json' }); assert.equal(r.headers.get('content-encoding'), null); assert.equal(await r.text(), want);
  calls = 0; script.push({ stream: () => body([enc.encode('{"broken"')]) }); r = await fetch('/assets/n10.json'); await assert.rejects(r.json(), SyntaxError);
  calls = 0; script.push({ stream: () => body([bytes(3, 1), bytes(4, 2)]), headers: { 'content-length': '5' } });
  r = await fetch('/assets/n11.bin'); assert.deepEqual([...await text(r)], [1, 1, 1, 2, 2, 2, 2], 'longer than announced');
  calls = 0; script.push({ stream: () => body([bytes(3, 1)]), headers: { 'content-length': '9' } });
  r = await fetch('/assets/n12.bin'); assert.deepEqual([...await text(r)], [1, 1, 1], 'shorter than announced');
  assert.equal(script.length, 0, 'every body read'); }
setPv('loadCopies', null);
console.log('Downloads OK: retry on network errors, broken and stalled bodies, 408/429/5xx; 404 passes; final failure rejects');
