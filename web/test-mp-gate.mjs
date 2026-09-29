// Hosting gate + static hardening (web/server/gate.mjs, web/server/mp-server.mjs; docs/hosting.md): without the session
// cookie the page is the login form, assets / status / the WebSocket answer 401; wrong passwords fail and are rate
// limited; the right one sets an HttpOnly Secure cookie and redirects only to same-site paths; dotfiles and symlinks
// out of the static roots are never served.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import net from 'node:net';
import { fileURLToPath } from 'node:url';
import crypto from 'node:crypto';

const web = path.dirname(fileURLToPath(import.meta.url));
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-gate-'));
const root = path.join(dir, 'root'); fs.mkdirSync(path.join(root, 'assets'), { recursive: true });
fs.writeFileSync(path.join(root, 'index.html'), '<!doctype html>game');
fs.writeFileSync(path.join(root, 'assets', 'a.json'), '{"ok":1}');
fs.writeFileSync(path.join(root, 'assets', 'empty.bin'), '');
fs.writeFileSync(path.join(root, 'build.json'), '{"id":"b1"}'); fs.writeFileSync(path.join(root, 'assets', 'main-AbCd_12-.js'), '//'); fs.writeFileSync(path.join(root, 'assets', 'terrain-worker-core.js'), '//');
fs.writeFileSync(path.join(root, '.env'), 'SECRET=1');
fs.writeFileSync(path.join(dir, 'outside.txt'), 'outside');
fs.symlinkSync(path.join(dir, 'outside.txt'), path.join(root, 'assets', 'link.txt'));
const PASSWORD = crypto.randomBytes(9).toString('base64url');
fs.writeFileSync(path.join(dir, 'password'), PASSWORD + '\n', { mode: 0o600 });
const EDGE = crypto.randomBytes(24).toString('hex'); fs.writeFileSync(path.join(dir, 'edge'), EDGE + '\n', { mode: 0o600 });
const port = 18000 + Math.floor(Math.random() * 1000);
const server = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1', '--static', root], { cwd: web,
  env: { ...process.env, MP_GATE_PASSWORD_FILE: path.join(dir, 'password'), MP_GATE_SECRET_FILE: path.join(dir, 'secret'), MP_TRUST_PROXY: '1', MP_EDGE_SECRET_FILE: path.join(dir, 'edge'), MP_DIAG_LOG: path.join(dir, 'diag.log') }, stdio: ['ignore', 'pipe', 'inherit'] });
try {
  await new Promise((resolve, reject) => { server.stdout.on('data', (d) => { if (/password gate on/.test(d)) resolve(); }); server.on('exit', reject); setTimeout(() => reject(new Error('server start')), 5000); });
  const base = `http://127.0.0.1:${port}`;
  const get = (p, headers = {}) => fetch(base + p, { headers, redirect: 'manual' });
  const login = (password, next = '/', ip = '203.0.113.1') => fetch(base + '/gate/login', { method: 'POST', redirect: 'manual', headers: { 'content-type': 'application/x-www-form-urlencoded', 'cf-connecting-ip': ip }, body: new URLSearchParams({ password, next }) });
  const upgrade = (cookie = '') => new Promise((resolve) => { const s = net.connect(port, '127.0.0.1', () => s.write(`GET /mp HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n${cookie ? `Cookie: ${cookie}\r\n` : ''}\r\n`)); let out = ''; s.on('data', (d) => { out += d; s.destroy(); resolve(out.split('\r\n')[0]); }); s.on('close', () => resolve(out.split('\r\n')[0])); });

  // Locked out without a session.
  let r = await get('/?lobby=abc'); assert.equal(r.status, 200); const form = await r.text(); assert.match(form, /name="password"/); assert.match(form, /value="\/\?lobby=abc"/);
  assert.equal((await get('/assets/a.json')).status, 401);
  assert.equal((await get('/mp/status')).status, 401);
  assert.match(await upgrade(), /401/);
  // Wrong password, rate limit per client address.
  assert.equal((await login('wrong', '/', '203.0.113.9')).status, 401);
  for (let i = 0; i < 5; i++) await login('wrong', '/', '203.0.113.9');
  assert.equal((await login(PASSWORD, '/', '203.0.113.9')).status, 429, 'attempts per minute are limited');
  // Right password: cookie + same-site redirect only.
  r = await login(PASSWORD, '/?lobby=abc'); assert.equal(r.status, 303); assert.equal(r.headers.get('location'), '/?lobby=abc');
  const setCookie = r.headers.get('set-cookie'); assert.match(setCookie, /HttpOnly/); assert.match(setCookie, /Secure/); assert.match(setCookie, /SameSite=Lax/);
  const cookie = setCookie.split(';')[0];
  assert.equal((await login(PASSWORD, '//evil.example/')).headers.get('location'), '/');
  assert.equal((await login(PASSWORD, 'https://evil.example/')).headers.get('location'), '/');
  // With the session: game, assets, status, WebSocket.
  r = await get('/', { cookie }); assert.equal(r.status, 200); assert.equal(await r.text(), '<!doctype html>game');
  r = await get('/assets/a.json', { cookie }); assert.equal(r.status, 200);
  assert.match(r.headers.get('cache-control'), /^private/, 'gated files must not be cached by shared caches (Cloudflare edge)'); assert.equal(r.headers.get('cdn-cache-control'), 'no-store');
  assert.equal((await get('/mp/status', { cookie })).status, 200);
  // The edge worker (shared secret header) gets shared-cacheable game files; a wrong secret stays private.
  r = await get('/assets/a.json', { cookie, 'x-ssx-edge': EDGE }); assert.match(r.headers.get('cache-control'), /^public/); assert.equal(r.headers.get('cdn-cache-control'), 'max-age=300, stale-while-revalidate=604800');
  r = await get('/assets/a.json', { cookie, 'x-ssx-edge': EDGE.replace(/.$/, 'x') }); assert.match(r.headers.get('cache-control'), /^private/); assert.equal(r.headers.get('cdn-cache-control'), 'no-store');
  assert.match((await get('/', { cookie, 'x-ssx-edge': EDGE })).headers.get('cache-control'), /no-cache/);
  // Workers / bundles (docs/workers.md): Vite's hashed files are immutable, a look-alike name is not; the deploy check's
  // build.json is never stored.
  assert.match((await get('/assets/main-AbCd_12-.js', { cookie })).headers.get('cache-control'), /max-age=31536000, immutable/);
  assert.doesNotMatch((await get('/assets/terrain-worker-core.js', { cookie })).headers.get('cache-control'), /immutable/);
  r = await get('/build.json', { cookie }); assert.equal(r.status, 200); assert.equal(r.headers.get('cache-control'), 'no-store'); assert.equal(r.headers.get('cdn-cache-control'), 'no-store');
  assert.equal((await get('/build.json')).status, 401);
  assert.equal((await get('/assets/a.json', { 'x-ssx-edge': EDGE })).status, 401, 'the edge secret alone is no session');
  assert.match(await upgrade(cookie), /101/);
  // A forged / tampered cookie is refused.
  assert.equal((await get('/assets/a.json', { cookie: cookie.slice(0, -2) + (cookie.endsWith('AA') ? 'BB' : 'AA') })).status, 401);
  // An empty file is served (it once crashed the server: read stream end -1), and the server is still up afterwards.
  r = await get('/assets/empty.bin', { cookie }); assert.equal(r.status, 200); assert.equal((await r.arrayBuffer()).byteLength, 0);
  r = await get('/assets/empty.bin', { cookie, range: 'bytes=0-' }); assert.ok(r.status === 200 || r.status === 416);
  assert.equal((await get('/assets/a.json', { cookie })).status, 200, 'server survives');
  // Field diagnostics (web/diagnostics.js): gated, stored one JSON line per batch.
  const diag = (headers) => fetch(base + '/mp/diag', { method: 'POST', headers: { 'content-type': 'application/json', ...headers }, body: JSON.stringify({ session: 's1', events: [{ kind: 'gpu-error', message: 'x' }] }) });
  assert.equal((await diag({})).status, 401, 'no diagnostics without a session');
  assert.equal((await diag({ cookie })).status, 204);
  assert.match(fs.readFileSync(path.join(dir, 'diag.log'), 'utf8'), /"kind":"gpu-error"/);
  // Hardening: no dotfiles, no symlink escapes, no encoded traversal.
  assert.equal((await get('/.env', { cookie })).status, 404);
  assert.equal((await get('/assets/link.txt', { cookie })).status, 404);
  assert.equal((await get('/assets/%2e%2e/%2e%2e/outside.txt', { cookie })).status, 404);
  console.log('hosting gate: edge-cache headers only for the worker secret, login form, 401 without session (assets, status, WebSocket), rate limit, Secure HttpOnly cookie, same-site redirect, tamper, dotfiles, symlinks, traversal OK');
} finally { server.kill(); fs.rmSync(dir, { recursive: true, force: true }); }
