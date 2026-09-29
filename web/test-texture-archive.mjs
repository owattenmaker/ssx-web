// Texture archives (docs/asset-formats.md "Texture archives"; tools/texture_archive.py, tools/export_world_textures.py,
// tools/export_rider_textures.py, web/texture-archive.js):
//  - the world texture library: every SSB kind-9 id 0..787 once, index sane, every PNG decodes to the texels its digest
//    names; the SDB counts (788 textures, 623 light pages) recorded;
//  - every world package (course, connector, peak location, sky dome, cutscene set) references the library or its own
//    lightmaps.tex, with matching dimensions, and keeps no 9-/10- PNG copies;
//  - rider archives (when exported): every wardrobe texture stem and every RIDER_* package entry resolves, Equip Gear
//    packages built by web/wardrobe.js reference the archive with the same texture choices as before (id = stem);
//  - the browser loader (web/texture-archive.js) reads entries byte for byte, fetches an archive once, and resolves
//    {pack, id} / {path} entries.
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const PUBLIC = new URL('./public/', import.meta.url).pathname;
const ASSETS = PUBLIC + 'assets/';
const MAGIC = 'SSXTEX01';

function readArchive(file) {
  const data = fs.readFileSync(file);
  assert.equal(data.toString('latin1', 0, 8), MAGIC, `${file}: magic`);
  const n = data.readUInt32LE(8), index = JSON.parse(data.toString('utf8', 12, 12 + n)), start = 12 + n + ((16 - ((12 + n) % 16)) % 16);
  let end = 0;
  for (const e of index.entries) { assert(e.offset >= end - 0 && e.offset + e.size <= data.length - start, `${file}: entry ${e.id} inside the payload`); end = Math.max(end, e.offset + e.size); }
  assert.equal(start + end, data.length, `${file}: payload ends with the last entry`);
  return { index, png: (e) => data.subarray(start + e.offset, start + e.offset + e.size), byId: new Map(index.entries.map((e) => [e.id, e])) };
}
// PNG decoder (8-bit RGB/RGBA, 1..8-bit indexed with tRNS, filters 0..4) -> RGBA
export function decodePng(b) {
  assert.equal(b.toString('latin1', 1, 4), 'PNG');
  let at = 8, w, h, depth, ct, plte = Buffer.alloc(0), trns = Buffer.alloc(0); const idat = [];
  while (at < b.length) { const n = b.readUInt32BE(at), t = b.toString('latin1', at + 4, at + 8), d = b.subarray(at + 8, at + 8 + n); at += 12 + n;
    if (t === 'IHDR') { w = d.readUInt32BE(0); h = d.readUInt32BE(4); depth = d[8]; ct = d[9]; } else if (t === 'PLTE') plte = d; else if (t === 'tRNS') trns = d; else if (t === 'IDAT') idat.push(d); }
  const ch = { 2: 3, 6: 4, 3: 1 }[ct], bpp = Math.max(1, (ch * depth) >> 3), stride = Math.ceil((w * ch * depth) / 8), raw = zlib.inflateSync(Buffer.concat(idat));
  const out = Buffer.alloc(w * h * 4); let prev = Buffer.alloc(stride);
  for (let y = 0; y < h; y++) {
    const f = raw[y * (stride + 1)], line = Buffer.from(raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1)));
    for (let i = 0; i < stride && f; i++) { const a = i >= bpp ? line[i - bpp] : 0, u = prev[i], c = i >= bpp ? prev[i - bpp] : 0; let p = 0;
      if (f === 1) p = a; else if (f === 2) p = u; else if (f === 3) p = (a + u) >> 1; else { const q = a + u - c, pa = Math.abs(q - a), pb = Math.abs(q - u), pc = Math.abs(q - c); p = pa <= pb && pa <= pc ? a : pb <= pc ? u : c; }
      line[i] = (line[i] + p) & 255; }
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 4;
      if (ct === 6) line.copy(out, o, x * 4, x * 4 + 4);
      else if (ct === 2) { line.copy(out, o, x * 3, x * 3 + 3); out[o + 3] = 255; }
      else { const bit = x * depth, j = (line[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1); plte.copy(out, o, j * 3, j * 3 + 3); out[o + 3] = j < trns.length ? trns[j] : 255; }
    }
    prev = line;
  }
  return { w, h, rgba: out };
}
const digest = (rgba) => crypto.createHash('sha256').update(rgba).digest('hex').slice(0, 16);
const archives = new Map(); const archiveAt = (url) => { const file = PUBLIC + url.replace(/^\//, ''); if (!archives.has(file)) archives.set(file, readArchive(file)); return archives.get(file); };
function checkEntries(file, archive, sample = Infinity) {
  let n = 0;
  for (const e of archive.index.entries) { if (n++ >= sample) break; const { w, h, rgba } = decodePng(archive.png(e)); assert.deepEqual([w, h], [e.width, e.height], `${file} ${e.id}: size`); assert.equal(digest(rgba), e.rgba, `${file} ${e.id}: texels`); }
  return n;
}

// ---- the world texture library --------------------------------------------------------------------------------
const WORLD = ASSETS + 'TEXTURES/world.tex';
const world = readArchive(WORLD);
assert.equal(world.index.kind, 'world'); assert.equal(world.index.texture_count, 788); assert.equal(world.index.light_page_count, 623);
assert.deepEqual(world.index.entries.map((e) => e.id), Array.from({ length: 788 }, (_, i) => i), 'one entry per kind-9 id, in id order');
assert(world.index.entries.every((e) => e.colours <= 256), 'PS2/GameCube textures are paletted (indexed PNG)');
assert.deepEqual(world.index.entries.filter((e) => e.source === 'gamecube').map((e) => e.id), [121, 625], 'GameCube CMPR fallbacks (tools/import_world.py)');
checkEntries(WORLD, world);

// ---- world packages ------------------------------------------------------------------------------------------------
const worldPackages = [];
(function walk(dir) { for (const d of fs.readdirSync(dir, { withFileTypes: true })) { if (!d.isDirectory()) continue; const p = path.join(dir, d.name), rel = path.relative(ASSETS, p);
  if (/^(RIDER_|WARDROBE)/.test(rel) || rel.startsWith('CUTSCENES/PROPS')) continue; if (fs.existsSync(p + '/world.json')) worldPackages.push(p); walk(p); } })(ASSETS);
let refs = 0, lightmapRefs = 0, lightmapArchives = 0;
for (const dir of worldPackages) {
  const rel = '/assets/' + path.relative(ASSETS, dir) + '/', d = JSON.parse(fs.readFileSync(dir + '/world.json'));
  for (const [key, t] of Object.entries(d.textures || {})) {
    if (!/^(9|10)-\d+$/.test(key)) continue;
    assert(t.pack !== undefined && t.path === undefined, `${rel}${key}: an archive reference`);
    const url = t.pack.startsWith('/') ? t.pack : rel + t.pack, a = archiveAt(url), e = a.byId.get(t.id);
    assert(e, `${rel}${key}: ${url} has id ${t.id}`); assert.equal(t.id, +key.split('-')[1], `${rel}${key}: id = resource id`);
    assert.deepEqual([e.width, e.height], [t.width, t.height], `${rel}${key}: dimensions`);
    if (key.startsWith('9-')) { assert.equal(t.pack, '/assets/TEXTURES/world.tex', `${rel}${key}: the shared library`); refs++; } else { assert.equal(t.pack, 'lightmaps.tex'); lightmapRefs++; }
  }
  assert(!fs.readdirSync(dir).some((f) => /^(9|10)-\d+\.png$/.test(f)), `${rel}: no per-location texture PNG copies`);
  if (fs.existsSync(dir + '/lightmaps.tex')) { lightmapArchives++; const a = archiveAt(rel + 'lightmaps.tex'); assert.equal(a.index.kind, 'lightmaps'); checkEntries(dir, a, 4); }
}
assert(worldPackages.length >= 3 && refs > 0, 'world packages found');

// ---- rider archives (tools/export_rider_textures.py) -------------------------------------------------------------
let riders = 0, riderRefs = 0, gearChecks = 0;
const wardrobeDir = ASSETS + 'WARDROBE/';
const riderIds = fs.existsSync(wardrobeDir) ? fs.readdirSync(wardrobeDir).filter((d) => fs.existsSync(wardrobeDir + d + '/wardrobe.json')) : [];
const { Wardrobe, assembly, buildPackage } = await import('./wardrobe.js');
for (const id of riderIds) {
  const w = JSON.parse(fs.readFileSync(wardrobeDir + id + '/wardrobe.json'));
  if (!w.texture_pack) continue;   // not converted yet (older export)
  riders++;
  assert.equal(w.texture_pack, `/assets/WARDROBE/${id}/textures.tex`);
  // textures.tex = the default outfits, gear.tex = the other items (or everything in textures.tex: --single)
  const packs = [w.texture_pack, w.gear_pack].filter(Boolean), a = { byId: new Map(packs.flatMap((u) => [...archiveAt(u).byId])) };
  for (const [stem, t] of Object.entries(w.textures)) { const e = archiveAt(t.pack ?? w.texture_pack).byId.get(stem); assert(e, `${id}: ${stem} in ${t.pack}`); assert.deepEqual([e.width, e.height], [t.width, t.height], `${id}: ${stem} size`); }
  for (const u of packs) checkEntries(u, archiveAt(u), 6);
  if (w.icon_pack) { const icons = archiveAt(w.icon_pack); for (const name of Object.keys(w.icons || {})) assert(icons.byId.get(name) || !fs.existsSync(wardrobeDir + id + '/icons'), `${id}: icon ${name}`); }
  assert(!fs.existsSync(wardrobeDir + id + '/textures') || !fs.readdirSync(wardrobeDir + id + '/textures').some((f) => f.endsWith('.png')), `${id}: no texture PNG copies`);
  // Equip Gear packages: the same texture choices (the archive id is the stem the texture rule picked)
  if (w.parts && fs.existsSync(wardrobeDir + id + '/parts.bin') && !w.cheat) {
    const bin = fs.readFileSync(wardrobeDir + id + '/parts.bin'); w.bin = bin.buffer.slice(bin.byteOffset, bin.byteOffset + bin.byteLength);
    // the default outfit and a few gear combinations (a combination the rules leave without a texture fails both builds alike)
    for (const picks of [[], [0, 3, 8], [1, 5, 13, 21], [2, 9, 17]]) {
      const wd = new Wardrobe(w, null, { allOwned: true }), candidates = wd.list.filter((e) => e.texture && !wd.equipped(e.item));
      for (const k of picks) { const e = candidates[(k * 7) % Math.max(1, candidates.length)]; if (e) wd.toggle(e.item); }
      const asm = assembly(w, wd.raceEquipped()), build = (doc) => { try { return buildPackage(doc, asm, { riderId: id.toLowerCase() }); } catch (e) { return String(e.message); } };
      const pkg = build(w), old = build({ ...w, texture_pack: undefined, gear_pack: undefined, textures: Object.fromEntries(Object.entries(w.textures).map(([k, t]) => [k, { ...t, pack: undefined }])) });   // the PNG-path build
      if (typeof old === 'string') { assert.equal(pkg, old, `${id}: the same outcome`); continue; }
      const stems = Object.values(pkg.world.textures).map((t) => t.id), before = Object.values(old.world.textures).map((t) => path.basename(t.path, '.png'));
      assert.deepEqual(stems, before, `${id}: same textures, same order as the PNG build`);
      for (const t of Object.values(pkg.world.textures)) { assert.equal(t.pack, w.textures[t.id].pack ?? w.texture_pack); assert(archiveAt(t.pack).byId.get(t.id), `${id}: ${t.id}`); }
      assert.deepEqual(pkg.world.batches, old.world.batches); gearChecks++;
    }
  }
}
for (const pkg of fs.readdirSync(ASSETS).filter((d) => d.startsWith('RIDER_'))) for (const sub of ['', 'fe/']) {
  const file = `${ASSETS}${pkg}/${sub}world.json`; if (!fs.existsSync(file)) continue;
  for (const [key, t] of Object.entries(JSON.parse(fs.readFileSync(file)).textures || {})) {
    if (t.pack === undefined) { assert(fs.existsSync(`${ASSETS}${pkg}/${sub}${t.path}`), `${pkg}/${sub}${key}: PNG file`); continue; }
    const e = archiveAt(t.pack).byId.get(t.id); assert(e, `${pkg}/${sub}${key}: ${t.pack} ${t.id}`); assert.deepEqual([e.width, e.height], [t.width, t.height]); riderRefs++;
  }
}

// ---- the browser loader --------------------------------------------------------------------------------------------
let fetches = 0;
globalThis.fetch = async (url) => { fetches++; const u = new URL(url, 'http://localhost/'); const file = PUBLIC + decodeURIComponent(u.pathname).replace(/^\//, ''); return fs.existsSync(file) ? new Response(fs.readFileSync(file)) : new Response('missing', { status: 404 }); };
const { textureArchive, archiveBlob, textureSource, packageTextureBlob } = await import('./texture-archive.js');
const a1 = await textureArchive('/assets/TEXTURES/world.tex'), a2 = await textureArchive('/assets/TEXTURES/world.tex');
assert.equal(a1, a2); assert.equal(fetches, 1, 'one download per archive');
for (const id of [0, 8, 121, 625, 787]) { const b = Buffer.from(await (await archiveBlob('/assets/TEXTURES/world.tex', id)).arrayBuffer()); assert(b.equals(world.png(world.byId.get(id))), `loader entry ${id}`); }
assert.deepEqual(textureSource('/assets/ARA1/', { pack: 'lightmaps.tex', id: 5 }), { archive: '/assets/ARA1/lightmaps.tex', id: 5 });
assert.deepEqual(textureSource('/assets/ARA1/', { pack: '/assets/TEXTURES/world.tex', id: 5 }), { archive: '/assets/TEXTURES/world.tex', id: 5 });
assert.deepEqual(textureSource('/assets/RIDER_MAC/', { path: '9-0.png', archive: 'mactxn.big' }), { file: '/assets/RIDER_MAC/9-0.png' }, 'a rider entry names its disc BIG in `archive`, not a texture archive');
const lm = worldPackages.find((d) => fs.existsSync(d + '/lightmaps.tex'));
if (lm) { const rel = '/assets/' + path.relative(ASSETS, lm) + '/', t = Object.values(JSON.parse(fs.readFileSync(lm + '/world.json')).textures).find((x) => x.pack === 'lightmaps.tex'); const b = await packageTextureBlob(rel, t); assert.equal(Buffer.from(await b.arrayBuffer()).toString('latin1', 1, 4), 'PNG'); }
await assert.rejects(archiveBlob('/assets/TEXTURES/world.tex', 99999), /no texture/);

console.log('texture archives:', { worldTextures: world.index.entries.length, worldBytes: fs.statSync(WORLD).size, worldPackages: worldPackages.length, libraryRefs: refs,
  lightmapArchives, lightmapRefs, riderArchives: riders, riderPackageRefs: riderRefs, gearCombinationsChecked: gearChecks });
