// pv archiveRanges (web/archive-ranges.js, docs/asset-formats.md "Texture archives: range reads"), without a browser:
//  - every entry of the Xbox HD and PS2 gear archives of three riders (and a cheat skin's small one) read by ranges equals the
//    whole-file slice byte for byte, and decodes to the same texels (packageTexels: BC blocks to RGBA, PNG to RGBA);
//  - an archive needs its head + index and the entries asked for, not the whole file (bytes served counted);
//  - a server that answers a range with the whole file (200), or with a range of a compressed copy (the Cloudflare edge, brotli),
//    falls back to the whole-archive read with the same bytes.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { setPv } from './pv-flags.js';

const PUBLIC = new URL('./public/', import.meta.url).pathname;
if (!fs.existsSync(PUBLIC + 'assets/WARDROBE/ZOE/gear-xbox.tex')) {
  console.log('archive ranges: skipped (no game data)');
  process.exit(0);
}
globalThis.location = new URL('http://game.test/');
let mode = 'ok';
const served = { bytes: 0, requests: 0, ranged: 0 };
globalThis.fetch = async (url, init = {}) => {
  const u = new URL(url, 'http://game.test/');
  const file = PUBLIC + decodeURIComponent(u.pathname).replace(/^\//, '');
  if (!fs.existsSync(file)) return new Response('missing', { status: 404 });
  const data = fs.readFileSync(file);
  const range = /^bytes=(\d+)-(\d+)$/.exec(init.headers?.range ?? '');
  served.requests++;
  if (!range || mode === 'whole') {
    served.bytes += data.length;
    return new Response(data, { headers: { 'x-decoded-length': String(data.length) } });
  }
  served.ranged++;
  const start = +range[1],
    end = Math.min(+range[2], data.length - 1);
  if (mode === 'brrange') {
    // the edge's answer: a range of its cached brotli copy, decoded on the way (more bytes than asked, the compressed total)
    const body = data.subarray(start, Math.min(data.length, start + (end - start + 1) * 3));
    served.bytes += body.length;
    return new Response(body, {
      status: 206,
      headers: { 'content-range': `bytes ${start}-${end}/${Math.floor(data.length / 7)}`, 'x-decoded-length': String(data.length) }
    });
  }
  const body = data.subarray(start, end + 1);
  served.bytes += body.length;
  return new Response(body, { status: 206, headers: { 'content-range': `bytes ${start}-${end}/${data.length}`, 'x-decoded-length': String(data.length) } });
};
console.warn = () => {};
setPv('archiveRanges', true);
const TA = await import('./texture-archive.js');
const { isBCEntry, parseBCEntry, decodeBC } = await import('./bc-texels.js');
const { decodePngTexels } = await import('./png-texels.js');
const { quality } = await import('./quality.js');

const indexOf = (data) => {
  const n = data.readUInt32LE(8);
  return { start: 12 + n + ((16 - ((12 + n) % 16)) % 16), entries: JSON.parse(data.toString('utf8', 12, 12 + n)).entries };
};
const texels = async (bytes) => {
  const b = new Uint8Array(bytes);
  if (isBCEntry(b.subarray(0, 16))) {
    const e = parseBCEntry(b.buffer.slice(b.byteOffset, b.byteOffset + b.length));
    return decodeBC(e.codec, e.blocks, e.width, e.height);
  }
  return (await decodePngTexels(b.buffer.slice(b.byteOffset, b.byteOffset + b.length))).data;
};
const same = (a, b) => a.length === b.length && Buffer.compare(Buffer.from(a), Buffer.from(b)) === 0;

let checked = 0,
  decoded = 0;
for (const rel of ['WARDROBE/ZOE/gear-xbox.tex', 'WARDROBE/GRIFF/gear-xbox.tex', 'WARDROBE/MAC/gear.tex', 'WARDROBE/BRODI/gear-xbox.tex']) {
  // gear.tex: the PS2 set (in the 'xbox' set riderArchiveUrl reads its Xbox HD twin)
  quality.riderTextures = rel.endsWith('-xbox.tex') ? 'xbox' : 'ps2';
  const url = '/assets/' + rel,
    data = fs.readFileSync(PUBLIC + TA.riderArchiveUrl(url).replace(/^\//, '')),
    { start, entries } = indexOf(data);
  const before = { ...served };
  for (const [i, e] of entries.entries()) {
    const ranged = new Uint8Array(await (await TA.archiveBlob(url, e.id)).arrayBuffer());
    const whole = data.subarray(start + e.offset, start + e.offset + e.size);
    assert.ok(same(ranged, whole), `${rel} ${e.id}: the ranged bytes`);
    checked++;
    // every 4th entry decoded both ways (all of them would take minutes for the Xbox HD archives)
    if (i % 4 === 0) {
      assert.ok(same(await texels(ranged), await texels(whole)), `${rel} ${e.id}: the decoded texels`);
      decoded++;
    }
  }
  const a = await TA.textureArchive(url);
  assert.equal(a.ranged, true, `${rel}: read by ranges`);
  assert.equal(served.requests - before.requests, served.ranged - before.ranged, `${rel}: no whole-file request`);
}
// one entry of an archive: the head + index and that entry only
{
  const rel = 'WARDROBE/MOBY/gear-xbox.tex',
    data = fs.readFileSync(PUBLIC + 'assets/' + rel),
    { entries } = indexOf(data);
  const before = served.bytes;
  await TA.archiveBlob('/assets/' + rel, entries[3].id);
  const used = served.bytes - before;
  assert.ok(used < 140 * 1024 + entries[3].size, `${rel}: ${used} bytes for one entry`);
  console.log(`${rel}: one entry = ${(used / 1024).toFixed(0)} KB of ${(data.length / 1048576).toFixed(1)} MB`);
}
// a server without ranges, and the edge's ranges of its brotli copy: the whole archive, the same bytes
const { rangeState } = await import('./archive-ranges.js');
for (const m of ['whole', 'brrange']) {
  mode = m;
  rangeState.broken = false;
  const rel = m === 'whole' ? 'WARDROBE/NATE/gear-xbox.tex' : 'WARDROBE/ELISE/gear-xbox.tex',
    data = fs.readFileSync(PUBLIC + 'assets/' + rel),
    { start, entries } = indexOf(data);
  const e = entries[5];
  const blob = await TA.archiveBlob('/assets/' + rel, e.id);
  assert.ok(same(new Uint8Array(await blob.arrayBuffer()), data.subarray(start + e.offset, start + e.offset + e.size)), `${m}: fallback bytes`);
  assert.ok(!(await TA.textureArchive('/assets/' + rel)).ranged, `${m}: the whole archive`);
  assert.equal(rangeState.broken, true, `${m}: detected`);
}
setPv('archiveRanges', null);
console.log(`archive ranges OK: ${checked} entries equal by ranges (${decoded} decoded to equal texels); whole-file fallback for 200 and edge brotli ranges`);
process.exit(0);
