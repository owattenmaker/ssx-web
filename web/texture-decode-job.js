// Off-main-thread decode of a BC texture entry (web/bc-texels.js; the Xbox HD rider set, docs/xbox-textures.md section 8).
// Runs in web/texture-decode-worker.js, or on the main thread when the worker is unavailable (web/worker-guard.js).
// Request {buffer: ArrayBuffer of the entry, bc: true when the renderer uploads BC blocks (WebGPU 'texture-compression-bc')}
//   bc  -> {codec, domain, width, height, mipmaps: [{width, height, data}]}   (a CompressedTexture's levels)
//   !bc -> {domain, width, height, data}                                       (exact RGBA texels, a DataTexture)
import { withTransfer } from './worker-guard-child.js';
import { parseBCEntry, decodeBC, bcMipChain } from './bc-texels.js';

export function decodeTextureJob({ buffer, bc }) {
  const e = parseBCEntry(buffer);
  if (bc) {
    const mipmaps = bcMipChain(e.codec, e.blocks, e.width, e.height);
    return withTransfer({ codec: e.codec, domain: e.domain, width: e.width, height: e.height, mipmaps }, mipmaps.map((m) => m.data.buffer));
  }
  const data = decodeBC(e.codec, e.blocks, e.width, e.height);
  return withTransfer({ domain: e.domain, width: e.width, height: e.height, data }, [data.buffer]);
}
