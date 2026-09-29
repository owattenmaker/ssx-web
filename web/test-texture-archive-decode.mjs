// Texture-archive texels reach the GPU exactly, in Chrome AND WebKit (docs/asset-formats.md "Texture archives").
// 2026-09-26: Safari drew Metro City with black snow and gates, silver/pink rider outfits and no crowd, because WebKit's
// image decoder hands the archives' indexed PNGs (PLTE + tRNS) to WebGPU wrong (palette indices as grey levels from a
// revoked blob-URL <img>; premultiplied RGB for alpha < 255 from a decoded <img> or an ImageBitmap). Chrome passed the
// bytes through, so every Chrome check stayed green. The game now decodes the PNGs itself (web/png-texels.js).
//  1. node: web/png-texels.js decodes every entry of every archive to the texels whose SHA-256 the index records;
//  2. node: packageTexture / archiveTexture return a DataTexture of those texels (never the browser decoder), set up
//     like TextureLoader's textures; the FE preview decode (fe-preview-prepare.js) gives the same texels;
//  3. browsers (web/texture-decode-test.html over a private Vite server): the game's path -> three.js WebGPU upload ->
//     GPU readback equals the digest, in headless Chrome and in the macOS system WebKit (web/webkit-driver.mjs, Safari's
//     engine). Each part is skipped when its browser is unavailable (no Chrome / not macOS / SSX_NO_WEBKIT=1).
//   node test-texture-archive-decode.mjs [--no-browser]
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { decodePngTexels } from './png-texels.js';
import { parseBCEntry, decodeBC, bcMipChain } from './bc-texels.js';

const PUBLIC = new URL('./public/', import.meta.url).pathname, ASSETS = PUBLIC + 'assets/';
const digest = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex').slice(0, 16);
function readArchive(file) {
  const data = fs.readFileSync(file), n = data.readUInt32LE(8), index = JSON.parse(data.toString('utf8', 12, 12 + n)), start = 12 + n + ((16 - ((12 + n) % 16)) % 16);
  return { index, png: (e) => data.subarray(start + e.offset, start + e.offset + e.size) };
}
const archives = [];
(function walk(dir) { for (const d of fs.readdirSync(dir, { withFileTypes: true })) { const p = path.join(dir, d.name); if (d.isDirectory()) walk(p); else if (d.name.endsWith('.tex')) archives.push(p); } })(ASSETS);
if (!archives.length) { console.log('texture archive decode: no archives exported (web/public/assets), skipped'); process.exit(0); }

// ---- 1. every entry, browser decoder module vs the index digest ----
let entries = 0, indexed = 0, translucent = 0, bcEntries = 0; const t0 = Date.now();
for (const file of archives) {
  const a = readArchive(file);
  for (const e of a.index.entries) {
    if (e.codec) {   // an Xbox HD BC entry (web/bc-texels.js): the JS decoder gives the texels of the exporter's reference decode
      const b = parseBCEntry(a.png(e)); assert.equal(`bc${b.codec}`, e.codec); assert.deepEqual([b.width, b.height, b.domain], [e.width, e.height, e.domain], `${file} ${e.id}: header`);
      assert.equal(digest(decodeBC(b.codec, b.blocks, b.width, b.height)), e.rgba, `${path.relative(ASSETS, file)} ${e.id}: BC texels differ from the index digest`);
      entries++; bcEntries++; continue;
    }
    const png = a.png(e), t = await decodePngTexels(png);
    assert.equal(t.width, e.width, `${file} ${e.id}: width`); assert.equal(t.height, e.height, `${file} ${e.id}: height`);
    assert.equal(digest(t.data), e.rgba, `${path.relative(ASSETS, file)} ${e.id}: texels differ from the index digest`);
    entries++; if (png[25] === 3) indexed++;
    for (let i = 3; i < t.data.length; i += 4) if (t.data[i] !== 255) { translucent++; break; }
  }
}
console.log(`png-texels / bc-texels: ${entries} entries of ${archives.length} archives exact (${indexed} indexed, ${translucent} with alpha < 255, ${bcEntries} BC) in ${((Date.now() - t0) / 1000).toFixed(1)} s`);

// small synthetic PNGs: filters 1..4 and the 1/2/4-bit index packing (the exporter writes filter 0 only)
{
  const zlib = await import('node:zlib');
  const chunk = (kind, body) => { const b = Buffer.alloc(12 + body.length); b.writeUInt32BE(body.length, 0); b.write(kind, 4, 'latin1'); body.copy(b, 8); b.writeUInt32BE(zlib.crc32(Buffer.concat([Buffer.from(kind, 'latin1'), body])) >>> 0, 8 + body.length); return b; };
  const png = (w, h, depth, colour, rows, extra = []) => { const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = depth; ihdr[9] = colour;
    return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk('IHDR', ihdr), ...extra, chunk('IDAT', zlib.deflateSync(Buffer.concat(rows))), chunk('IEND', Buffer.alloc(0))]); };
  // RGBA 3x2 with filter 1 (Sub) on row 0 and 4 (Paeth) on row 1
  const px = [[10, 20, 30, 255], [40, 50, 60, 128], [70, 80, 90, 0], [15, 25, 35, 255], [45, 55, 65, 100], [75, 85, 95, 1]];
  const r0 = [1, ...px[0], ...px[1].map((v, i) => (v - px[0][i]) & 255), ...px[2].map((v, i) => (v - px[1][i]) & 255)];
  const paeth = (a, b, c) => { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c); return pa <= pb && pa <= pc ? a : pb <= pc ? b : c; };
  const r1 = [4]; for (let x = 0; x < 3; x++) for (let c = 0; c < 4; c++) { const a = x ? px[3 + x - 1][c] : 0, b = px[x][c], cc = x ? px[x - 1][c] : 0; r1.push((px[3 + x][c] - paeth(a, b, cc)) & 255); }
  const t = await decodePngTexels(png(3, 2, 8, 6, [Buffer.from(r0), Buffer.from(r1)]));
  assert.deepEqual(Array.from(t.data), px.flat(), 'RGBA filters Sub + Paeth');
  // 2-bit indexed, 5 wide (a partial last byte), tRNS shorter than the palette
  const plte = Buffer.from([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]), trns = Buffer.from([0, 77]);
  const idx = [0, 1, 2, 3, 1], byte = (k) => idx.slice(k * 4, k * 4 + 4).reduce((v, j, i) => v | (j << (6 - 2 * i)), 0);
  const q = await decodePngTexels(png(5, 1, 2, 3, [Buffer.from([0, byte(0), byte(1)])], [chunk('PLTE', plte), chunk('tRNS', trns)]));
  assert.deepEqual(Array.from(q.data), [1, 2, 3, 0, 4, 5, 6, 77, 7, 8, 9, 255, 10, 11, 12, 255, 4, 5, 6, 77], '2-bit indexed + tRNS');
  await assert.rejects(decodePngTexels(png(1, 1, 16, 6, [Buffer.alloc(9)])), /not covered/, '16-bit PNGs go to the fallback');
}

// ---- 2. the game's loaders use those texels ----
{
  const world = readArchive(ASSETS + 'TEXTURES/world.tex'), realFetch = globalThis.fetch;
  // the PS2 set for the PS2 archive checks, whatever the device default (pv xboxRiders: Xbox HD on a desktop, which node counts as)
  const { quality } = await import('./quality.js'), riderSet = quality.riderTextures; quality.riderTextures = 'ps2';
  globalThis.fetch = async (url) => { const f = path.join(PUBLIC, String(url).replace(/^https?:\/\/[^/]+/, '')); return fs.existsSync(f) ? new Response(fs.readFileSync(f)) : new Response('', { status: 404 }); };
  try {
    const { packageTexture, archiveTexture } = await import('./texture-archive.js');
    const refuse = { loadAsync: () => { throw Error('the browser image decoder must not be used for an archive entry'); } };
    for (const e of world.index.entries.filter((e, i) => e.colours <= 16 || i % 97 === 0).slice(0, 12)) {
      const tex = await packageTexture(refuse, '/assets/COURSE/', { pack: '/assets/TEXTURES/world.tex', id: e.id });
      assert.ok(tex.isDataTexture, `world ${e.id}: DataTexture`);
      assert.equal(digest(tex.image.data), e.rgba, `world ${e.id}: texels`);
      assert.deepEqual([tex.image.width, tex.image.height], [e.width, e.height]);
      // as THREE.TextureLoader's textures: callers set flipY / wrap / colour space themselves
      assert.deepEqual([tex.magFilter, tex.minFilter, tex.generateMipmaps, tex.flipY, tex.premultiplyAlpha], [1006, 1008, true, true, false], `world ${e.id}: sampler setup`);
    }
    const zoe = readArchive(ASSETS + 'WARDROBE/ZOE/textures.tex');
    for (const e of zoe.index.entries) assert.equal(digest((await archiveTexture(refuse, '/assets/WARDROBE/ZOE/textures.tex', e.id)).image.data), e.rgba, `ZOE ${e.id}`);
    const { decodeTextureBlob } = await import('./fe-preview-prepare.js');
    for (const e of zoe.index.entries) assert.equal(digest((await decodeTextureBlob(new Blob([zoe.png(e)]))).data), e.rgba, `FE preview ZOE ${e.id}`);
    // Xbox HD set (docs/xbox-textures.md section 8): the same ids from textures-xbox.tex while the set is chosen, BC entries decoded
    // off the main thread (here the worker guard's local handler) into the index's texels, domain 'xbox'; with BC upload, the blocks
    // as stored at level 0 and a full mip chain
    if (fs.existsSync(ASSETS + 'WARDROBE/ZOE/textures-xbox.tex')) {
      const { riderArchiveUrl, decodeBCEntry } = await import('./texture-archive.js'), was = quality.riderTextures;
      const hd = readArchive(ASSETS + 'WARDROBE/ZOE/textures-xbox.tex');
      assert.deepEqual(hd.index.entries.map((e) => e.id), zoe.index.entries.map((e) => e.id), 'the twin holds the same ids in the same order');
      quality.riderTextures = 'xbox';
      try {
        assert.equal(riderArchiveUrl('/assets/WARDROBE/ZOE/textures.tex'), '/assets/WARDROBE/ZOE/textures-xbox.tex');
        assert.equal(riderArchiveUrl('/assets/TEXTURES/world.tex'), '/assets/TEXTURES/world.tex');
        for (const e of hd.index.entries) {
          const tex = await archiveTexture(refuse, '/assets/WARDROBE/ZOE/textures.tex', e.id);
          assert.equal(digest(tex.image.data), e.rgba, `ZOE HD ${e.id}`); assert.equal(tex.userData.entryDomain, e.codec ? 'xbox' : undefined);
          const fe = await decodeTextureBlob(new Blob([hd.png(e)])); assert.equal(digest(fe.data), e.rgba, `FE preview ZOE HD ${e.id}`);
          if (!e.codec) continue;
          const b = parseBCEntry(hd.png(e)), r = await decodeBCEntry(new Uint8Array(hd.png(e)).slice().buffer, true);
          assert.ok(Buffer.from(r.mipmaps[0].data).equals(Buffer.from(b.blocks)), `ZOE HD ${e.id}: level 0 = the Xbox blocks`);
          assert.equal(r.mipmaps.length, Math.log2(Math.max(b.width, b.height)) + 1, `ZOE HD ${e.id}: mips to 1x1`);
          assert.deepEqual(r.mipmaps.map((m) => m.data.length), bcMipChain(b.codec, b.blocks, b.width, b.height).map((m) => m.data.length));
        }
      } finally { quality.riderTextures = was; }
    }
  } finally { globalThis.fetch = realFetch; quality.riderTextures = riderSet; }
  console.log('texture-archive.js / fe-preview-prepare.js: archive entries become exact DataTexture texels, no browser decoder');
}

if (process.argv.includes('--no-browser')) process.exit(0);

// ---- 3. real browsers: game path -> three.js WebGPU upload -> GPU readback ----
const { startServer, startBrowser } = await import('./headless-chrome.mjs');
const { startWebKit } = await import('./webkit-driver.mjs');
const server = await startServer();
const check = (name, r) => {
  if (r?.webgpu === false) { console.log(`${name}: no WebGPU adapter, skipped (${r.ua})`); return; }
  assert.ok(r && !r.error && r.rows?.length >= 20, `${name}: QA page result ${JSON.stringify(r)?.slice(0, 400)}`);
  for (const row of r.rows) {
    assert.ok(!row.error, `${name} ${row.archive} ${row.id}: ${row.error}`);
    assert.equal(row.dataTexture, true, `${name} ${row.archive} ${row.id}: not decoded by web/png-texels.js`);
    assert.equal(row.game, true, `${name} ${row.archive} ${row.id}: GPU texels differ from the archive digest`);
  }
  const bad = (k) => r.rows.filter((row) => row[k] === false).length;
  console.log(`${name}: ${r.rows.length} textures exact on the GPU through the game's path; the browser decoder would have got ${bad('imgRevoked')} wrong (revoked blob <img>), ${bad('imgDecoded')} (decoded <img>), ${bad('imageBitmap')} (ImageBitmap)`);
  return r;
};
// Xbox HD set (when exported): Zoe's and Mac's BC entries through the game path, every mip level drawn back within 2 of web/bc-texels.js
// (GPU BC1 thirds may round differently from the reference integer rule)
const HD = fs.existsSync(ASSETS + 'WARDROBE/ZOE/textures-xbox.tex');
const checkBC = (name, r) => {
  if (r?.webgpu === false) { console.log(`${name} BC: no WebGPU adapter, skipped`); return; }
  assert.ok(r && !r.error && r.rows?.length >= 6, `${name} BC: QA page result ${JSON.stringify(r)?.slice(0, 400)}`);
  for (const row of r.rows) {
    assert.ok(!row.error, `${name} ${row.archive} ${row.id}: ${row.error}`);
    assert.equal(row.compressed, r.bc, `${name} ${row.archive} ${row.id}: BC upload ${r.bc ? 'expected' : 'not expected'}`); assert.equal(row.domain, 'xbox');
    for (const l of row.levels) { assert.ok(l.max <= 3 && l.offShare === 0, `${name} ${row.archive} ${row.id} level ${l.level}: max ${l.max}, share off > 2 ${l.offShare}`); }
  }
  console.log(`${name}: ${r.rows.length} Xbox HD textures ${r.bc ? 'uploaded as BC blocks' : 'decoded to texels'}, ${r.rows.reduce((n, x) => n + x.levels.length, 0)} levels within 2 of web/bc-texels.js`);
};
try {
  const chrome = await startBrowser({ width: 800, height: 600 });
  if (!chrome) console.log('headless Chrome: not installed, skipped');
  else {
    try { await chrome.goto(server.origin + '/texture-decode-test.html?riders=ps2'); await chrome.waitFor('!!window.__textureDecode', 60000); check('Chrome', await chrome.evaluate('window.__textureDecode'));
      if (HD) { await chrome.goto(server.origin + '/bc-texture-test.html'); await chrome.waitFor('!!window.__bcTexture', 60000); checkBC('Chrome', await chrome.evaluate('window.__bcTexture')); } }
    finally { await chrome.close(); }
  }
  const webkit = await startWebKit({ width: 800, height: 600, offscreen: true });
  if (!webkit) console.log('WebKit (webkit-driver): unavailable, skipped');
  else {
    try {
      await webkit.goto(server.origin + '/texture-decode-test.html?riders=ps2');
      for (let i = 0; i < 120 && !webkit.events.includes('loaded'); i++) await new Promise((r) => setTimeout(r, 250));
      if (!webkit.events.includes('loaded')) console.log(`WebKit (webkit-driver): the page never loaded (${webkit.events.join(', ') || 'no events'}), skipped`);
      else {
        await webkit.waitFor('!!window.__textureDecode', 60000);
        const r = check('WebKit', await webkit.eval('window.__textureDecode'));
        if (r?.rows) console.log(`WebKit: ${r.ua}`);
        if (HD) {
          await webkit.goto(server.origin + '/bc-texture-test.html');
          await webkit.waitFor('!!window.__bcTextureResult', 120000); checkBC('WebKit', await webkit.eval('window.__bcTextureResult'));
        }
      }
    } finally { await webkit.close(); }
  }
} finally { await server.close(); }
