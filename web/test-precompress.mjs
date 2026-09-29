// Precompressed game data (web/server/precompress.mjs + mp-server.mjs MP_PRECOMPRESSED, docs/hosting.md): gzip copies
// only where they save space, served only to gzip clients, never when stale, bytes identical after decoding, the real
// size in X-Decoded-Length (web/downloads.js), and pruned when the source goes away.
import assert from 'node:assert/strict';
import { spawn, execFileSync } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import http from 'node:http';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const web = path.dirname(fileURLToPath(import.meta.url));
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ssx-pre-')), root = path.join(dir, 'public'), out = path.join(dir, 'pre');
fs.mkdirSync(path.join(root, 'assets', 'ARA1'), { recursive: true });
const plain = Buffer.alloc(200000); for (let i = 0; i < plain.length; i++) plain[i] = (i >> 7) & 255; // compressible
const noise = crypto.randomBytes(100000);                                                               // not worth it
fs.writeFileSync(path.join(root, 'assets', 'ARA1', 'vertices.bin'), plain);
fs.writeFileSync(path.join(root, 'assets', 'ARA1', 'noise.bin'), noise);
fs.writeFileSync(path.join(root, 'assets', 'ARA1', 'gone.json'), JSON.stringify({ a: 'x'.repeat(50000) }));
// an Xbox HD rider archive (docs/xbox-textures.md section 8): a block repeated beyond gzip's 32 KB window, which brotli finds
fs.mkdirSync(path.join(root, 'assets', 'WARDROBE', 'ZOE'), { recursive: true });
const tile = crypto.randomBytes(40000), hd = Buffer.concat([tile, tile, tile, tile, tile]);
fs.writeFileSync(path.join(root, 'assets', 'WARDROBE', 'ZOE', 'gear-xbox.tex'), hd);
const pre = (...args) => execFileSync(process.execPath, ['server/precompress.mjs', root, out, ...args], { cwd: web, encoding: 'utf8' });
assert.match(pre(), /2 written.*; 1 brotli/);
assert.ok(fs.existsSync(path.join(out, 'assets/ARA1/vertices.bin.gz')) && !fs.existsSync(path.join(out, 'assets/ARA1/noise.bin.gz')));
assert.ok(fs.existsSync(path.join(out, 'assets/WARDROBE/ZOE/gear-xbox.tex.br')) && !fs.existsSync(path.join(out, 'assets/ARA1/vertices.bin.br')), 'brotli only for *-xbox.tex');
assert.match(pre(), /0 written, 2 up to date/, 'unchanged files are not recompressed');
fs.rmSync(path.join(root, 'assets', 'ARA1', 'gone.json')); pre();
assert.ok(!fs.existsSync(path.join(out, 'assets/ARA1/gone.json.gz')), 'copies of removed files are pruned');

const port = 19000 + Math.floor(Math.random() * 1000);
const server = spawn(process.execPath, ['server/mp-server.mjs', '--port', String(port), '--host', '127.0.0.1', '--static', root, '--precompressed', out], { cwd: web, stdio: ['ignore', 'pipe', 'inherit'] });
const get = (p, headers = {}) => new Promise((resolve, reject) => http.get({ host: '127.0.0.1', port, path: p, headers }, (res) => { const c = []; res.on('data', (d) => c.push(d)); res.on('end', () => resolve({ res, body: Buffer.concat(c) })); }).on('error', reject));
try {
  await new Promise((resolve, reject) => { server.stdout.on('data', resolve); server.on('exit', reject); });
  let r = await get('/assets/ARA1/vertices.bin', { 'accept-encoding': 'gzip, br' });
  assert.equal(r.res.headers['content-encoding'], 'gzip'); assert.ok(r.body.length < plain.length / 3); assert.deepEqual(zlib.gunzipSync(r.body), plain);
  assert.equal(+r.res.headers['x-decoded-length'], plain.length); assert.equal(r.res.headers.vary, 'Accept-Encoding'); assert.equal(r.res.headers['accept-ranges'], undefined);
  r = await get('/assets/ARA1/vertices.bin'); assert.equal(r.res.headers['content-encoding'], undefined); assert.deepEqual(r.body, plain);
  r = await get('/assets/WARDROBE/ZOE/gear-xbox.tex', { 'accept-encoding': 'gzip, deflate, br' });
  assert.equal(r.res.headers['content-encoding'], 'br'); assert.ok(r.body.length < hd.length / 3); assert.deepEqual(zlib.brotliDecompressSync(r.body), hd); assert.equal(+r.res.headers['x-decoded-length'], hd.length);
  const brTag = r.res.headers.etag;
  // gzip cannot see the repeats (no .gz copy: not worth it): a gzip-only client gets the raw bytes, under another ETag
  r = await get('/assets/WARDROBE/ZOE/gear-xbox.tex', { 'accept-encoding': 'gzip' }); assert.equal(r.res.headers['content-encoding'], undefined); assert.deepEqual(r.body, hd); assert.notEqual(r.res.headers.etag, brTag);
  r = await get('/assets/ARA1/noise.bin', { 'accept-encoding': 'gzip' }); assert.equal(r.res.headers['content-encoding'], undefined); assert.deepEqual(r.body, noise);
  // A newer source than its copy (deploy before precompress ran): the raw file, never stale bytes.
  const newer = Buffer.from(plain); newer[0] ^= 1; fs.writeFileSync(path.join(root, 'assets', 'ARA1', 'vertices.bin'), newer);
  const future = new Date(Date.now() + 5000); fs.utimesSync(path.join(root, 'assets', 'ARA1', 'vertices.bin'), future, future);
  r = await get('/assets/ARA1/vertices.bin', { 'accept-encoding': 'gzip' }); assert.equal(r.res.headers['content-encoding'], undefined); assert.deepEqual(r.body, newer);
  console.log('precompress: gzip copies only when worth it, served only to gzip clients, identical after decoding, never stale, X-Decoded-Length, pruned; brotli for *-xbox.tex to br clients');
} finally { server.kill(); fs.rmSync(dir, { recursive: true, force: true }); }
