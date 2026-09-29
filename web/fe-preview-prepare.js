// Preparation of a front-end preview package (web/fe-preview.js): fetch, JSON parse, the per-part vertex/skin/index/morph
// arrays and the texture decode (web/png-texels.js: the exact RGBA texels {width, height, data}; WebKit's
// createImageBitmap premultiplies alpha < 255 even with premultiplyAlpha 'none'; a PNG the decoder does not cover
// falls back to createImageBitmap, bytes kept: no premultiply, no colour conversion). Runs in
// web/fe-preview-worker.js (results transferred), or on the main thread when the worker is unavailable (web/worker-guard.js).
// Request: { root, files?: { name: ArrayBuffer|string } (generated Equip Gear packages, web/wardrobe.js; world.json),
//   blobs?: { textureKey: Blob } (the texture PNGs, read on the main thread) } -> prepared { world, rig, textures, parts }.
import { withTransfer } from './worker-guard-child.js';
import { decodePngTexels } from './png-texels.js';
import { isBCEntry } from './bc-texels.js';
import { decodeTextureJob } from './texture-decode-job.js';
import { unwrapReply } from './worker-guard-child.js';

/** A texture PNG Blob -> exact texels, or an ImageBitmap for a PNG web/png-texels.js does not cover. An Xbox HD BC entry
 * (web/bc-texels.js, docs/xbox-textures.md section 8) -> {codec, domain, width, height, mipmaps} when the renderer uploads BC
 * blocks (bc), else its exact texels {domain, width, height, data} (web/texture-decode-job.js, here: already off the main thread). */
export async function decodeTextureBlob(blob, bc = false) {
  const buffer = await blob.arrayBuffer();
  if (isBCEntry(new Uint8Array(buffer, 0, Math.min(16, buffer.byteLength)))) return unwrapReply(decodeTextureJob({ buffer, bc })).reply;
  try { return await decodePngTexels(buffer); }
  catch { return createImageBitmap(blob, { premultiplyAlpha: 'none', colorSpaceConversion: 'none', imageOrientation: 'none' }); }
}
/** The buffers of a decoded texture to transfer. */
export const textureTransfer = (t) => (t.mipmaps ? t.mipmaps.map((m) => m.data.buffer) : t.data ? [t.data.buffer] : [t]);

const get = async (root, files, name, type) => {
  const local = files?.[name];
  if (local !== undefined) return type === 'json' ? (typeof local === 'string' ? JSON.parse(local) : local) : local;
  const r = await fetch(root + name);
  if (!r.ok) throw new Error(`${root}${name}: ${r.status}`);
  return type === 'json' ? r.json() : r.arrayBuffer();
};

async function prepare(root, files, blobs, bc) {
  const [world, rig, vb, ib, mb] = await Promise.all([get(root, files, 'world.json', 'json'), get(root, files, 'rider.json', 'json'),
    get(root, files, 'vertices.bin'), get(root, files, 'indices.bin'), get(root, files, 'morphs.bin').catch(() => new ArrayBuffer(0))]);
  const textures = {};
  await Promise.all(Object.entries(world.textures).map(async ([key, t]) => {
    // blobs: the PNGs the main thread read (texture archive entries or files, web/fe-preview.js textureBlobs)
    const local = blobs?.[key] ?? (t.path !== undefined ? files?.[t.path] : undefined);
    const blob = local instanceof Blob ? local : local !== undefined ? new Blob([local]) : await (await fetch(root + t.path)).blob();
    textures[key] = await decodeTextureBlob(blob, bc);
  }));
  const vertices = new Float32Array(vb), indices = new Uint32Array(ib), transfer = Object.values(textures).flatMap(textureTransfer);
  const parts = rig.parts.map((part) => {
    const v0 = part.first_vertex, count = part.vertex_count;
    const interleaved = vertices.slice(v0 * 10, (v0 + count) * 10);
    const skinIndex = new Uint16Array(count * 4), skinWeight = new Float32Array(count * 4);
    for (let i = 0; i < count; i++) { const w = rig.skin[v0 + i]; for (let k = 0; k < Math.min(4, w.length); k++) { skinIndex[i * 4 + k] = w[k][0]; skinWeight[i * 4 + k] = w[k][1]; } }
    const batches = world.batches.filter((b) => b.first_index >= part.first_index && b.first_index < part.first_index + part.index_count);
    const keys = [], groups = [], list = [];
    for (const b of batches) {
      let at = keys.indexOf(b.texture); if (at < 0) { at = keys.length; keys.push(b.texture); }
      groups.push([list.length, b.index_count, at]);
      for (let i = 0; i < b.index_count; i++) list.push(indices[b.first_index + i] - v0);
    }
    const index = new Uint32Array(list);
    const morphs = (part.morphs || []).map((m) => new Float32Array(mb.slice(m.offset, m.offset + count * 12)));
    transfer.push(interleaved.buffer, skinIndex.buffer, skinWeight.buffer, index.buffer, ...morphs.map((m) => m.buffer));
    return { interleaved, skinIndex, skinWeight, index, groups, keys, morphs };
  });
  delete rig.skin; delete rig.source_skin;   // not needed by the preview after this point (large)
  return { prepared: { world, rig, textures, parts }, transfer };
}

export async function prepareFrontEndPreview({ root, files, blobs, bc = false }) {
  const { prepared, transfer } = await prepare(root, files, blobs, bc);
  return withTransfer(prepared, transfer);
}
