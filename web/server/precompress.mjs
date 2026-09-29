#!/usr/bin/env node
// Gzip copies of the game data for the hosting server (docs/hosting.md): `node server/precompress.mjs <root> <out>`
// writes <out>/<relative path>.gz for every compressible file of <root> (game data and JSON; PNG / video / music are
// already compressed) when it saves at least 10%, skips files whose copy is up to date, and removes copies whose
// source is gone. mp-server.mjs (MP_PRECOMPRESSED=<out>) serves them with Content-Encoding: gzip, so neither the home
// upload nor the Cloudflare edge carries the raw bytes (the course binaries shrink to a third; colors.bin to 4%).
// Brotli copies (<path>.br, quality 9, 16 MB window) as well for the Xbox HD rider archives (WARDROBE/<ID>/*-xbox.tex,
// docs/xbox-textures.md section 8): their DXT blocks repeat across the outfit variants of an archive, which gzip's 32 KB
// window cannot see (Zoe's gear-xbox.tex: 21.5 MB raw, gzip 11.5 MB, brotli 3.1 MB). mp-server prefers .br for br clients.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { pipeline } from 'node:stream/promises';

const SKIP = new Set(['.png', '.jpg', '.jpeg', '.mp4', '.mus', '.gz', '.br', '.woff2']), MIN_BYTES = 16 * 1024, MAX_RATIO = 0.9;
const BROTLI = /-xbox\.tex$/;
const [root, out] = process.argv.slice(2).map((p) => p && path.resolve(p));
if (!root || !out) { console.error('usage: precompress.mjs <root> <out>'); process.exit(2); }

let written = 0, kept = 0, skipped = 0, rawBytes = 0, gzBytes = 0, brotliWritten = 0;
const seen = new Set();
async function walk(dir) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (entry.name.startsWith('.')) continue;
    const file = path.join(dir, entry.name);
    if (entry.isDirectory()) { await walk(file); continue; }
    if (!entry.isFile() || SKIP.has(path.extname(entry.name).toLowerCase())) continue;
    const stat = fs.statSync(file); if (stat.size < MIN_BYTES) continue;
    if (BROTLI.test(entry.name)) await brotli(file, stat, path.join(out, path.relative(root, file)) + '.br');
    const target = path.join(out, path.relative(root, file)) + '.gz'; seen.add(target);
    try { const t = fs.statSync(target); if (t.mtimeMs >= stat.mtimeMs) { kept++; rawBytes += stat.size; gzBytes += t.size; continue; } } catch {}
    fs.mkdirSync(path.dirname(target), { recursive: true });
    const temp = target + '.tmp';
    await pipeline(fs.createReadStream(file), zlib.createGzip({ level: 9 }), fs.createWriteStream(temp));
    const size = fs.statSync(temp).size;
    if (size > stat.size * MAX_RATIO) { fs.rmSync(temp); fs.rmSync(target, { force: true }); seen.delete(target); skipped++; continue; }
    fs.renameSync(temp, target); written++; rawBytes += stat.size; gzBytes += size;
  }
}
async function brotli(file, stat, target) {
  seen.add(target);
  try { const t = fs.statSync(target); if (t.mtimeMs >= stat.mtimeMs) return; } catch {}
  const temp = target + '.tmp'; fs.mkdirSync(path.dirname(target), { recursive: true });
  await pipeline(fs.createReadStream(file), zlib.createBrotliCompress({ params: { [zlib.constants.BROTLI_PARAM_QUALITY]: 9, [zlib.constants.BROTLI_PARAM_LGWIN]: 24, [zlib.constants.BROTLI_PARAM_SIZE_HINT]: stat.size } }), fs.createWriteStream(temp));
  if (fs.statSync(temp).size > stat.size * MAX_RATIO) { fs.rmSync(temp); fs.rmSync(target, { force: true }); seen.delete(target); return; }
  fs.renameSync(temp, target); brotliWritten++;
}
function prune(dir) {
  if (!fs.existsSync(dir)) return;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const file = path.join(dir, entry.name);
    if (entry.isDirectory()) { prune(file); if (!fs.readdirSync(file).length) fs.rmdirSync(file); }
    else if (!seen.has(file)) fs.rmSync(file);
  }
}
await walk(root); prune(out);
console.log(`precompress: ${written} written, ${kept} up to date, ${skipped} not worth it; ${(rawBytes / 1e6).toFixed(0)} MB -> ${(gzBytes / 1e6).toFixed(0)} MB${brotliWritten ? `; ${brotliWritten} brotli` : ''}`);
