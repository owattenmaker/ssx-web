import fs from 'node:fs';
// Edge worker (deploy/edge-worker.js, docs/hosting.md): accepts exactly the cookies web/server/gate.mjs issues, sends
// nothing cached to a visitor without one, passes pages / login / WebSocket through, and fetches /assets/* through the
// Cloudflare cache with the edge secret.
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import worker from '../deploy/edge-worker.js';
// deploy/deploy-staged.sh bumps CACHE_GEN on every game-data change: read it instead of pinning a number.
const CACHE_GEN = +fs.readFileSync(new URL('../deploy/edge-worker.js', import.meta.url), 'utf8').match(/^const CACHE_GEN = (\d+);/m)[1];

const secret = crypto.randomBytes(32), env = { GATE_SECRET: secret.toString('hex'), EDGE_SECRET: 'edge-' + crypto.randomBytes(8).toString('hex') };
const cookieFor = (expiry, key = secret) => `ssx_gate=v1.${expiry}.${crypto.createHmac('sha256', key).update(`v1.${expiry}`).digest('base64url')}`; // gate.mjs token()
const future = Math.floor(Date.now() / 1000) + 3600;
// A tampered signature: change a middle character to a different one. (Replacing the last character with 'A' was flaky:
// 1 in 64 signatures already end in 'A', and the last base64url character also carries padding bits.)
const tamper = (c) => { const i = c.length - 5; return c.slice(0, i) + (c[i] === 'A' ? 'B' : 'A') + c.slice(i + 1); };
const calls = [];
// The origin (web/server/mp-server.mjs serveStatic) marks exactly Vite's hashed bundles immutable; missing files 404.
const originHashed = (p) => /^\/assets\/[^/]+-[A-Za-z0-9_-]{8}\.(js|css|wasm)$/.test(p);
globalThis.fetch = async (req, init) => { calls.push({ url: req.url, edge: req.headers.get('x-ssx-edge'), cf: init?.cf }); const p = new URL(req.url).pathname;
  if (p.includes('/gone-')) return new Response('not found', { status: 404, headers: { 'content-type': 'text/plain' } });
  return new Response('body', { status: 200, headers: { 'cache-control': originHashed(p) ? 'public, max-age=31536000, immutable' : 'public, max-age=3600', 'cdn-cache-control': 'max-age=2592000', etag: '"x"' } }); };
const run = (path, { cookie, method = 'GET', headers = {} } = {}) => worker.fetch(new Request('https://ssx.example.com' + path, { method, headers: { ...(cookie ? { cookie } : {}), ...headers } }), env);

// No / bad session: 401 without touching origin or cache.
for (const cookie of [undefined, cookieFor(future, crypto.randomBytes(32)), cookieFor(Math.floor(Date.now() / 1000) - 5), 'ssx_gate=v1.1.x', tamper(cookieFor(future))]) {
  calls.length = 0; const r = await run('/assets/ARA1/vertices.bin', { cookie });
  assert.equal(r.status, 401); assert.equal(calls.length, 0, 'an unauthenticated request never reaches the cache');
}
// Valid session: through the cache with the edge secret, browser-private headers.
calls.length = 0; let r = await run('/assets/ARA1/vertices.bin', { cookie: 'other=1; ' + cookieFor(future) });
assert.equal(r.status, 200); assert.equal(calls[0].edge, env.EDGE_SECRET); assert.equal(calls[0].cf.cacheEverything, true); assert.equal(calls[0].url, `https://ssx.example.com/assets/ARA1/vertices.bin?g=${CACHE_GEN}`, 'generation in the fetched URL (the edge cache key)');
assert.equal(r.headers.get('cache-control'), 'private, no-cache'); assert.equal(r.headers.get('cdn-cache-control'), null);
r = await run('/assets/main-DU0lxg8m.js', { cookie: cookieFor(future) }); assert.equal(r.headers.get('cache-control'), 'private, max-age=31536000, immutable');
// Only what the origin calls hashed is pinned in browsers (docs/workers.md): look-alike names and 404s are revalidated.
for (const p of ['/assets/terrain-worker-core.js', '/assets/ARA1/some-textures0.js', '/assets/gone-DU0lxg8m.js']) {
  r = await run(p, { cookie: cookieFor(future) }); assert.equal(r.headers.get('cache-control'), 'private, no-cache', p);
}
// Pass-through (origin decides): pages, login, WebSocket, POST.
for (const [path, opts] of [['/', {}], ['/index.html', {}], ['/gate/login', { method: 'POST' }], ['/mp', { headers: { upgrade: 'websocket' } }], ['/mp/status', {}]]) {
  calls.length = 0; await run(path, opts); assert.equal(calls.length, 1); assert.equal(calls[0].edge, null, `${path}: no edge secret`); assert.equal(calls[0].cf, undefined, `${path}: not cached`);
}
console.log('edge worker: gate cookies verified (forged, expired, malformed, tampered refused before the cache), assets cached with the edge secret, pages/login/WebSocket passed through');
