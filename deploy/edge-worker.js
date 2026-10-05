// Cloudflare Worker in front of the hosted site (route: deploy/wrangler.toml, local; docs/hosting.md). It runs before Cloudflare's cache on every
// request, so the edge can cache the game files and still never hand them to a visitor without a session:
// - /gate/*, pages and the /mp WebSocket go straight to the origin (its gate shows the login form / answers 401);
// - every other request needs a valid gate cookie (same HMAC as web/server/gate.mjs, secret GATE_SECRET), else 401;
//   with no GATE_SECRET set (the public site since 2026-10-01) no session is checked;
// - /assets/* is then fetched through Cloudflare's cache (the origin marks it shareable only for requests carrying
//   X-SSX-Edge = EDGE_SECRET), so each file leaves the home server about once per Cloudflare location.
const COOKIE = 'ssx_gate';
const CACHE_GEN = 40; // edge cache generation (bump to retire all cached copies)
let keyPromise = null;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const passThrough = request.headers.get('upgrade') || !['GET', 'HEAD'].includes(request.method) ||
      url.pathname.startsWith('/gate/') || url.pathname === '/mp' || url.pathname.startsWith('/mp/') || url.pathname === '/' || url.pathname.endsWith('.html');
    if (passThrough) return fetch(request);
    // Without a GATE_SECRET the site is public (the origin runs without MP_GATE_PASSWORD), so no session is needed.
    if (env.GATE_SECRET && !(await validSession(request, env.GATE_SECRET))) return new Response('login required', { status: 401, headers: { 'content-type': 'text/plain', 'cache-control': 'no-store' } });
    if (!url.pathname.startsWith('/assets/')) return fetch(request);
    const headers = new Headers(request.headers); headers.set('x-ssx-edge', env.EDGE_SECRET);
    // CACHE_GEN in the URL: the edge cache keys on the full URL (custom cf.cacheKey is Enterprise-only and ignored),
    // so bumping it retires every edge copy at once; the origin ignores the query. Any client query is dropped.
    const target = new URL(url); target.search = `?g=${CACHE_GEN}`;
    const upstream = await fetch(new Request(target, { method: request.method, headers }), { cf: { cacheEverything: true } });
    // To the browser: hashed bundles are immutable; game files are revalidated on each load (ETag -> 304 from the
    // edge, no body), so a deploy that changes courses.json or a course package is picked up at once. Which files are
    // hashed is the origin's decision (web/server/mp-server.mjs marks exactly Vite's hashed bundles `immutable`), so a
    // file that only looks hashed is never pinned in browsers for a year (docs/workers.md).
    const response = new Response(upstream.body, upstream);
    const immutable = /\bimmutable\b/.test(upstream.headers.get('cache-control') ?? ''); // (a 404 never is)
    response.headers.set('cache-control', immutable ? 'private, max-age=31536000, immutable' : 'private, no-cache');
    response.headers.delete('cdn-cache-control');
    return response;
  },
};

async function validSession(request, secretHex) {
  if (!secretHex) return false;
  const cookie = (request.headers.get('cookie') ?? '').split(';').map((c) => c.trim()).find((c) => c.startsWith(`${COOKIE}=`));
  const m = /^v1\.(\d{1,12})\.([A-Za-z0-9_-]{43})$/.exec(cookie ? cookie.slice(COOKIE.length + 1) : '');
  if (!m || +m[1] < Date.now() / 1000) return false;
  keyPromise ??= crypto.subtle.importKey('raw', hexBytes(secretHex), { name: 'HMAC', hash: 'SHA-256' }, false, ['verify']);
  const signature = base64urlBytes(m[2]);
  return signature !== null && crypto.subtle.verify('HMAC', await keyPromise, signature, new TextEncoder().encode(`v1.${m[1]}`)); // constant-time
}
const hexBytes = (hex) => Uint8Array.from(hex.trim().match(/../g), (b) => parseInt(b, 16));
function base64urlBytes(s) { try { return Uint8Array.from(atob(s.replace(/-/g, '+').replace(/_/g, '/') + '='.repeat((4 - (s.length % 4)) % 4)), (c) => c.charCodeAt(0)); } catch { return null; } }
