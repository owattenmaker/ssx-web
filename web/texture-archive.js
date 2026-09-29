// Texture archives (docs/asset-formats.md "Texture archives", tools/texture_archive.py): many textures in one file.
//  - the shared world texture library /assets/TEXTURES/world.tex: every SSB kind-9 texture, id = its resource id, the
//    same id in every location (the original's texture handle, tools/export_world_textures.py);
//  - a package's GameCube lightmaps <package>/lightmaps.tex (only drawn with ?originalWorld=0);
//  - a rider's textures (the counterpart of the character's own texture archive on the disc, DATA/CHAR/<X>TXP.BIG):
//    WARDROBE/<ID>/textures.tex the default outfits, gear.tex the other Equip Gear textures, icons.tex the item icons
//    (id = texture stem, tools/export_rider_textures.py).
// An archive is fetched once per page (through web/downloads.js: shared, counted by the loading screens) and kept as
// one Blob; a texture is decoded when a package asks for it, from a slice of that Blob. Course switches, streamed peak
// locations, riders and cutscenes reuse it.
//
// Texels: decoded here (web/png-texels.js) into a THREE.DataTexture, never by the browser's image decoder. The entries
// are indexed PNGs (PLTE + tRNS); WebKit (measured in the macOS 27 system WebKit) hands those to WebGPU wrong: an <img> whose blob
// URL was revoked after load uploads the palette INDICES as grey levels (riders silver/pink, 16-colour snow and gate
// textures near-black, 2026-09-26), a decoded <img> or an ImageBitmap uploads premultiplied RGB where alpha < 255
// (crowd, particles, alpha-tested edges). Chrome passed the bytes through, so only Safari broke. The JS decode gives
// the exact texels of the index digest (entry.rgba) in every browser: web/test-texture-archive-decode.mjs.
//
// world.json texture entries: {path} = a PNG file next to the package (older exports), or {pack, id}: pack is the
// archive URL, package-relative unless it starts with '/' (`archive` in rider entries names the disc BIG, not this).
//
// Xbox HD rider set (pv xboxRiders, quality.riderTextures 'xbox'; docs/xbox-textures.md section 8): a rider's textures.tex /
// gear.tex is read from its twin textures-xbox.tex / gear-xbox.tex, which holds every id of the PS2 archive: the Xbox's own BC1 /
// BC2 blocks where the Xbox texture is better (web/bc-texels.js entry: 'SXBC' header, domain 'xbox' = full intensity, halved by
// the rider material), the PS2 PNG entry otherwise. A BC entry is decoded off the main thread (web/texture-decode-worker.js):
// on WebGPU with 'texture-compression-bc' into a CompressedTexture (its blocks and a mip chain), else into exact RGBA texels
// (a DataTexture, mips generated as for every texture). A twin that cannot be read (not deployed) falls back to the PS2 archive.
import { DataTexture, CompressedTexture, RGBAFormat, UnsignedByteType, LinearFilter, LinearMipmapLinearFilter, RGBA_S3TC_DXT1_Format, RGBA_S3TC_DXT3_Format } from 'three/webgpu';
import { decodePngTexels } from './png-texels.js';
import { isBCEntry, parseBCEntry, decodeBC } from './bc-texels.js';
import { decodeTextureJob } from './texture-decode-job.js';
import { createGuardedWorker, workerUrl } from './worker-guard.js';
import { quality, device } from './quality.js';

const MAGIC = 'SSXTEX01';
const archives = new Map(); // absolute URL -> Promise<archive>, most recently used last
// Rider archives (gear.tex 3-11 MB) are dropped beyond the most recent RIDER_ARCHIVES (Safari keeps Blob data in memory);
// one needed again is read again (HTTP cache: a revalidation, no body). The world library and lightmaps stay.
// The Xbox HD gear archives hold raw BC blocks (8-40 MB each, brotli on the wire): the rider archives kept are also held to a byte
// budget (96 MB; 32 MB and at most 3 archives with the Xbox HD set on a phone), the archive in use always kept.
const RIDER_ARCHIVES = 8, RIDER_ARCHIVES_HD_PHONE = 3, evictable = (key) => key.includes('/assets/WARDROBE/');
const hdPhone = () => quality.riderTextures === 'xbox' && (device.phone || device.ios || device.android);
const riderArchiveLimit = () => (hdPhone() ? RIDER_ARCHIVES_HD_PHONE : RIDER_ARCHIVES), riderArchiveBytes = () => (hdPhone() ? 32 : 96) * 1048576;
const archiveBytes = new Map();   // absolute URL -> Blob size of a loaded rider archive
function trimRiderArchives(keep) {
  let total = archiveBytes.get(keep) ?? 0;
  for (const k of [...archives.keys()].filter(evictable).reverse()) {   // newest first; one still loading counts 0
    if (k === keep) continue;
    const size = archiveBytes.get(k) ?? 0;
    if (total + size > riderArchiveBytes()) { archives.delete(k); archiveBytes.delete(k); } else total += size;
  }
}

function absolute(url) { return new URL(url, globalThis.location?.href ?? 'http://localhost/').href; }

// The archive a rider texture entry is read from: its Xbox HD twin while that set is chosen (and readable).
const HD_TWIN = /(\/assets\/WARDROBE\/[^/]+\/)(textures|gear)\.tex$/, hdMissing = new Set();
export function riderArchiveUrl(url, set = quality.riderTextures) {
  if (set !== 'xbox') return url;
  const m = absolute(url).match(HD_TWIN);
  return m && !hdMissing.has(absolute(url)) ? url.replace(/(textures|gear)\.tex$/, '$1-xbox.tex') : url;
}

export function textureArchive(url) {
  const hd = riderArchiveUrl(url);
  if (hd === url) return loadArchive(url);
  return loadArchive(hd).catch((error) => { hdMissing.add(absolute(url)); console.warn(`Xbox HD rider textures unavailable, PS2 set used: ${error?.message ?? error}`); return loadArchive(url); });
}

function loadArchive(url) {
  const key = absolute(url);
  let job = archives.get(key);
  if (job) { archives.delete(key); archives.set(key, job); }   // most recently used last
  if (!job) {
    job = (async () => {
      const response = await fetch(url);
      if (!response.ok) throw Error(`${url}: ${response.status}`);
      const blob = await response.blob();
      const head = new Uint8Array(await blob.slice(0, 12).arrayBuffer());
      if (String.fromCharCode(...head.subarray(0, 8)) !== MAGIC) throw Error(`${url}: not a texture archive`);
      const n = new DataView(head.buffer).getUint32(8, true);
      const index = JSON.parse(new TextDecoder().decode(await blob.slice(12, 12 + n).arrayBuffer()));
      const start = 12 + n + ((16 - ((12 + n) % 16)) % 16);
      return { url, index, blob, start, entries: new Map(index.entries.map((e) => [e.id, e])) };
    })();
    archives.set(key, job);
    job.catch(() => { if (archives.get(key) === job) archives.delete(key); });
    if (evictable(key)) {
      const riders = [...archives.keys()].filter(evictable); for (const k of riders.slice(0, Math.max(0, riders.length - riderArchiveLimit()))) { archives.delete(k); archiveBytes.delete(k); }
      job.then((a) => { if (archives.get(key) === job) { archiveBytes.set(key, a.blob.size); trimRiderArchives(key); } }, () => {});
    }
  }
  return job;
}

/** Start fetching archives early (e.g. the world library under the loading screen) without waiting. */
export function prefetchTextureArchive(url) { textureArchive(url).catch(() => {}); }

/** The PNG of one entry as a Blob slice (no copy). */
export async function archiveBlob(url, id) {
  const a = await textureArchive(url), e = a.entries.get(id);
  if (!e) throw Error(`${url}: no texture ${id}`);
  return a.blob.slice(a.start + e.offset, a.start + e.offset + e.size, e.codec ? 'application/octet-stream' : 'image/png');
}

// ---- BC entries (the Xbox HD rider set) ----
// bc: upload BC blocks (WebGPU with 'texture-compression-bc'; main.js configureTextureDecode after the renderer is up). ?bc=0 forces
// the RGBA decode (QA of the WebGL / no-BC path).
const decodeConfig = { bc: false };
const bcAllowed = () => { try { return new URLSearchParams(globalThis.location?.search ?? '').get('bc') !== '0'; } catch { return true; } };
export function configureTextureDecode({ renderer = null, bc } = {}) {
  const features = renderer?.backend?.isWebGPUBackend ? renderer.backend.device?.features : null;
  decodeConfig.bc = bcAllowed() && (bc ?? !!features?.has?.('texture-compression-bc'));
  return { ...decodeConfig };
}
export const textureDecodeConfig = () => ({ ...decodeConfig });
const DECODE_WORKER = workerUrl((Worker) => new Worker(new URL('./texture-decode-worker.js', import.meta.url), { type: 'module' }));
let decodeWorker = null;
/** A BC entry's bytes -> the decode job's reply (web/texture-decode-job.js), off the main thread. */
export function decodeBCEntry(buffer, bc = decodeConfig.bc) {
  decodeWorker ??= createGuardedWorker({ name: 'texture-decode', url: DECODE_WORKER, local: () => decodeTextureJob });
  return decodeWorker.request({ buffer, bc }, [buffer]);
}
/** The decode job's reply -> THREE texture: a CompressedTexture (BC blocks + mip chain) or a DataTexture of the texels.
 * userData.entryDomain = the entry's texel domain ('xbox': full intensity, the rider material halves it). */
export function bcTexture(r) {
  let t;
  if (r.mipmaps) {
    t = new CompressedTexture(r.mipmaps, r.width, r.height, r.codec === 1 ? RGBA_S3TC_DXT1_Format : RGBA_S3TC_DXT3_Format);
    t.magFilter = LinearFilter; t.minFilter = LinearMipmapLinearFilter; t.generateMipmaps = false; t.flipY = false; t.needsUpdate = true;
  } else t = texelTexture(r);
  t.userData.entryDomain = r.domain;
  return t;
}

/** THREE.DataTexture of decoded texels ({width, height, data} RGBA bytes), set up as THREE.TextureLoader's textures are
 * (linear + trilinear mip filtering, generated mipmaps, flipY on: callers set their own flipY / wrap / colour space). */
export function texelTexture({ width, height, data }) {
  const t = new DataTexture(data, width, height, RGBAFormat, UnsignedByteType);
  t.magFilter = LinearFilter; t.minFilter = LinearMipmapLinearFilter; t.generateMipmaps = true; t.flipY = true; t.unpackAlignment = 4;
  t.needsUpdate = true;
  return t;
}

/** PNG Blob -> THREE texture: decoded texels (texelTexture); a PNG the decoder does not cover goes through `loader`
 * (<img>), its object URL kept until the texture is disposed (WebKit may decode it again from the URL). */
export async function pngTexture(loader, blob) {
  let texels = null;
  const buffer = await blob.arrayBuffer();
  if (isBCEntry(new Uint8Array(buffer, 0, Math.min(16, buffer.byteLength)))) return bcTexture(await decodeBCEntry(buffer));   // Xbox HD entry
  try { texels = await decodePngTexels(buffer); } catch (e) { if (!loader) throw e; }
  if (texels) return texelTexture(texels);
  const src = URL.createObjectURL(blob);
  try { const t = await loader.loadAsync(src); t.addEventListener('dispose', () => URL.revokeObjectURL(src)); return t; }
  catch (e) { URL.revokeObjectURL(src); throw e; }
}

/** THREE texture of one archive entry (exact texels, see the header). `loader`: THREE.TextureLoader for the fallback. */
export async function archiveTexture(loader, url, id) {
  return pngTexture(loader, await archiveBlob(url, id));
}

/** Where a texture entry of the package at `root` lives: {archive, id} or {file}. */
export function textureSource(root, t) {
  if (t.pack !== undefined) return { archive: t.pack.startsWith('/') ? t.pack : root + t.pack, id: t.id };
  return { file: root + t.path };
}

/** A package texture as THREE.Texture (archive entry or separate PNG, both decoded by web/png-texels.js). */
export async function packageTexture(loader, root, t) {
  const s = textureSource(root, t);
  if (s.archive) return archiveTexture(loader, s.archive, s.id);
  const r = await fetch(s.file); if (!r.ok) throw Error(`${s.file}: ${r.status}`);
  return pngTexture(loader, await r.blob());
}

/** A package texture's exact RGBA texels {width, height, data} (archive entry or PNG file). */
export async function packageTexels(root, t) {
  const buffer = await (await packageTextureBlob(root, t)).arrayBuffer();
  if (isBCEntry(new Uint8Array(buffer, 0, Math.min(16, buffer.byteLength)))) { const e = parseBCEntry(buffer); return { width: e.width, height: e.height, data: decodeBC(e.codec, e.blocks, e.width, e.height) }; }
  return decodePngTexels(buffer);
}

/** A package texture as a PNG Blob (archive slice or the fetched file). */
export async function packageTextureBlob(root, t) {
  const s = textureSource(root, t);
  if (s.archive) return archiveBlob(s.archive, s.id);
  const r = await fetch(s.file); if (!r.ok) throw Error(`${s.file}: ${r.status}`);
  return r.blob();
}

/** QA / tests: archives fetched so far. */
export function loadedTextureArchives() { return [...archives.keys()]; }
