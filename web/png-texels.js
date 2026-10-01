// Exact RGBA texels of a texture-archive PNG (tools/texture_archive.py: indexed PLTE + tRNS at 1/2/4/8 bits, or 8-bit
// RGBA / RGB; filters 0..4; no interlace), decoded here instead of by the browser's image decoder.
//
// Why (docs/asset-formats.md "Texture archives", 2026-09-26): WebKit (measured: macOS 27 system WebKit, Safari's engine) hands these indexed PNGs
// to WebGPU wrong through its image decoder: an <img> whose blob URL was revoked after load uploads the palette INDICES
// as grey levels, a decoded <img> or an ImageBitmap ('none'/'none') uploads premultiplied RGB wherever alpha < 255.
// Metro City in Safari had black snow and gates, silver/pink rider outfits and no crowd while Chrome, which passes the
// bytes through, was exact. Decoding the few chunk types here gives the same bytes in every browser: the texels whose
// SHA-256 the archive index records (entry.rgba, web/test-texture-archive-decode.mjs).
const SIGNATURE = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];

async function inflate(bytes) {
  // zlib stream (RFC 1950): DecompressionStream('deflate') is the zlib format in every engine that has it.
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate'));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

/** PNG bytes (ArrayBuffer / Uint8Array) -> {width, height, data: Uint8Array RGBA}; throws on a PNG it does not cover. */
export async function decodePngTexels(input) {
  const png = input instanceof Uint8Array ? input : new Uint8Array(input);
  for (let i = 0; i < 8; i++) if (png[i] !== SIGNATURE[i]) throw Error('not a PNG');
  const view = new DataView(png.buffer, png.byteOffset, png.byteLength);
  let at = 8, width = 0, height = 0, depth = 0, colour = -1, plte = null, trns = null; const idat = [];
  while (at + 8 <= png.length) {
    const n = view.getUint32(at), kind = String.fromCharCode(png[at + 4], png[at + 5], png[at + 6], png[at + 7]), body = png.subarray(at + 8, at + 8 + n);
    at += 12 + n;
    if (kind === 'IHDR') {
      width = view.getUint32(16); height = view.getUint32(20); depth = png[24]; colour = png[25];
      if (png[26] || png[27] || png[28]) throw Error('PNG compression/filter/interlace method not covered');
    } else if (kind === 'PLTE') plte = body;
    else if (kind === 'tRNS') trns = body;
    else if (kind === 'IDAT') idat.push(body);
    else if (kind === 'IEND') break;
  }
  const channels = { 2: 3, 3: 1, 6: 4 }[colour];
  if (!channels || (colour !== 3 && depth !== 8) || (colour === 3 && ![1, 2, 4, 8].includes(depth)) || !width || !height) throw Error(`PNG colour type ${colour} depth ${depth} not covered`);
  let size = 0; for (const b of idat) size += b.length;
  const joined = new Uint8Array(size); size = 0; for (const b of idat) { joined.set(b, size); size += b.length; }
  const raw = await inflate(joined);
  const stride = (width * channels * depth + 7) >> 3, bpp = Math.max(1, (channels * depth) >> 3);
  if (raw.length < height * (stride + 1)) throw Error('PNG image data too short');
  // unfilter in place (row y at y*(stride+1)+1, previous row just before it)
  for (let y = 0; y < height; y++) {
    const f = raw[y * (stride + 1)], o = y * (stride + 1) + 1, p = o - stride - 1;
    if (f === 0) continue;
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? raw[o + i - bpp] : 0, b = y ? raw[p + i] : 0, c = y && i >= bpp ? raw[p + i - bpp] : 0;
      let v;
      if (f === 1) v = a; else if (f === 2) v = b; else if (f === 3) v = (a + b) >> 1;
      else if (f === 4) { const q = a + b - c, pa = Math.abs(q - a), pb = Math.abs(q - b), pc = Math.abs(q - c); v = pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
      else throw Error('PNG filter ' + f);
      raw[o + i] = (raw[o + i] + v) & 255;
    }
  }
  const data = new Uint8Array(width * height * 4);
  if (colour === 6) { for (let y = 0; y < height; y++) data.set(raw.subarray(y * (stride + 1) + 1, y * (stride + 1) + 1 + stride), y * width * 4); return { width, height, data }; }
  if (colour === 2) {
    for (let y = 0, d = 0; y < height; y++)
      for (let x = 0, s = y * (stride + 1) + 1; x < width; x++, s += 3, d += 4) {
        data[d] = raw[s];
        data[d + 1] = raw[s + 1];
        data[d + 2] = raw[s + 2];
        data[d + 3] = 255;
      }
    return { width, height, data };
  }
  if (!plte) throw Error('indexed PNG without PLTE');
  const count = (plte.length / 3) | 0, words = new Uint32Array(256), out = new Uint32Array(data.buffer);
  const little = new Uint8Array(new Uint32Array([1]).buffer)[0] === 1;
  for (let j = 0; j < count; j++) {
    const r = plte[j * 3], g = plte[j * 3 + 1], b = plte[j * 3 + 2], a = trns && j < trns.length ? trns[j] : 255;
    words[j] = little ? ((a << 24) | (b << 16) | (g << 8) | r) >>> 0 : ((r << 24) | (g << 16) | (b << 8) | a) >>> 0;
  }
  const per = 8 / depth, mask = (1 << depth) - 1;
  for (let y = 0; y < height; y++) {
    const o = y * (stride + 1) + 1, row = y * width;
    if (depth === 8) { for (let x = 0; x < width; x++) out[row + x] = words[raw[o + x]]; continue; }
    for (let x = 0; x < width; x++) out[row + x] = words[(raw[o + ((x / per) | 0)] >> ((per - 1 - (x % per)) * depth)) & mask];
  }
  return { width, height, data };
}
