// BC1 / BC2 (DXT1 / DXT3) rider textures of the Xbox HD set (docs/xbox-textures.md section 8; tools/export_xbox_riders.py).
// An archive entry (web/texture-archive.js) is a 16-byte header + the Xbox's own blocks, level 0 only:
//   0 'SXBC', 4 u8 codec (1 = BC1 / DXT1, 2 = BC2 / DXT3), 5 u8 domain (1 = 'xbox': 255 = 1.0, the rider material halves it
//   into the PS2 texel domain; 0 = 'ps2'), 6 u16 width, 8 u16 height, 10..15 zero; then ceil(w/4) x ceil(h/4) blocks,
//   row-major (D3D layout: BC2 = 8 bytes of 4-bit alpha, then a BC1 colour block read in 4-colour mode).
// Two uses, both off the main thread (web/texture-decode-job.js, the fe-preview worker):
//   decodeBC     -> exact RGBA texels (tools/xbox_textures.py decode_dxt: the same integer rules), for the DataTexture path
//                   (WebGL, or WebGPU without 'texture-compression-bc'); three then generates the mips as for every texture;
//   bcMipChain   -> the blocks for a CompressedTexture: level 0 as stored, levels 1.. from a 2x2 box filter of the decoded
//                   level above (three's mip generation filters the same way) encoded again (encodeBC).
// Plain module (no DOM, no three): node tests, workers and the main thread share it.
export const BC_HEADER = 16;
export const BC_DOMAINS = Object.freeze(['ps2', 'xbox']);

export function isBCEntry(bytes) {
  return bytes.length >= BC_HEADER && bytes[0] === 0x53 && bytes[1] === 0x58 && bytes[2] === 0x42 && bytes[3] === 0x43;   // 'SXBC'
}

export function blockBytes(codec) { return codec === 1 ? 8 : 16; }
export function levelBytes(codec, width, height) { return Math.max(1, (width + 3) >> 2) * Math.max(1, (height + 3) >> 2) * blockBytes(codec); }

/** ArrayBuffer | Uint8Array of an entry -> {codec, domain, width, height, blocks (view)}. */
export function parseBCEntry(buffer) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  if (!isBCEntry(bytes)) throw Error('Not a BC texture entry');
  const codec = bytes[4], domain = BC_DOMAINS[bytes[5]], width = bytes[6] | (bytes[7] << 8), height = bytes[8] | (bytes[9] << 8);
  if ((codec !== 1 && codec !== 2) || !domain || !width || !height) throw Error('Bad BC texture header');
  const size = levelBytes(codec, width, height);
  if (bytes.length < BC_HEADER + size) throw Error('Truncated BC texture entry');
  return { codec, domain, width, height, blocks: bytes.subarray(BC_HEADER, BC_HEADER + size) };
}

/** The entry bytes for a level-0 block set (tools/export_xbox_riders.py writes the same). */
export function bcEntry(codec, domain, width, height, blocks) {
  const out = new Uint8Array(BC_HEADER + blocks.length);
  out.set([0x53, 0x58, 0x42, 0x43, codec, BC_DOMAINS.indexOf(domain), width & 255, width >> 8, height & 255, height >> 8]);
  out.set(blocks, BC_HEADER);
  return out;
}

const expand = (c, pal, o) => {
  const r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  pal[o] = (r << 3) | (r >> 2); pal[o + 1] = (g << 2) | (g >> 4); pal[o + 2] = (b << 3) | (b >> 2); pal[o + 3] = 255;
};
// The four colours of a colour block (four = 4-colour mode: BC2 always, BC1 when c0 > c1).
function palette(c0, c1, four, pal) {
  expand(c0, pal, 0); expand(c1, pal, 4);
  for (let k = 0; k < 3; k++) {
    const a = pal[k], b = pal[4 + k];
    if (four) { pal[8 + k] = ((2 * a + b) / 3) | 0; pal[12 + k] = ((a + 2 * b) / 3) | 0; }
    else { pal[8 + k] = (a + b) >> 1; pal[12 + k] = 0; }
  }
  pal[11] = 255; pal[15] = four ? 255 : 0;
}

/** Blocks -> RGBA texels (Uint8Array width x height x 4). */
export function decodeBC(codec, blocks, width, height, out = new Uint8Array(width * height * 4)) {
  const step = blockBytes(codec), bw = Math.max(1, (width + 3) >> 2), bh = Math.max(1, (height + 3) >> 2), pal = new Uint8Array(16);
  for (let by = 0; by < bh; by++) {
    for (let bx = 0; bx < bw; bx++) {
      const p = (by * bw + bx) * step, c = codec === 1 ? p : p + 8;
      const c0 = blocks[c] | (blocks[c + 1] << 8), c1 = blocks[c + 2] | (blocks[c + 3] << 8);
      const bits = (blocks[c + 4] | (blocks[c + 5] << 8) | (blocks[c + 6] << 16) | (blocks[c + 7] << 24)) >>> 0;
      palette(c0, c1, codec !== 1 || c0 > c1, pal);
      for (let i = 0; i < 16; i++) {
        const x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
        if (x >= width || y >= height) continue;
        const k = ((bits >>> (2 * i)) & 3) * 4, o = (y * width + x) * 4;
        out[o] = pal[k]; out[o + 1] = pal[k + 1]; out[o + 2] = pal[k + 2];
        out[o + 3] = codec === 1 ? pal[k + 3] : (((blocks[p + (i >> 1)] >> ((i & 1) * 4)) & 15) * 17);
      }
    }
  }
  return out;
}

/** 2x2 box filter (rounded mean of the texels that exist) -> the next mip level {width, height, data}. */
export function boxDown(data, width, height) {
  const w = Math.max(1, width >> 1), h = Math.max(1, height >> 1), out = new Uint8Array(w * h * 4);
  const sx = width > 1 ? 2 : 1, sy = height > 1 ? 2 : 1, n = sx * sy;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      for (let c = 0; c < 4; c++) {
        let s = 0;
        for (let dy = 0; dy < sy; dy++) for (let dx = 0; dx < sx; dx++) s += data[((y * sy + dy) * width + x * sx + dx) * 4 + c];
        out[(y * w + x) * 4 + c] = ((s + (n >> 1)) / n) | 0;
      }
    }
  }
  return { width: w, height: h, data: out };
}

const q565 = (r, g, b) => {
  const cl = (v, m) => Math.max(0, Math.min(m, Math.round(v * m / 255)));
  return (cl(r, 31) << 11) | (cl(g, 63) << 5) | cl(b, 31);
};
// One colour block: texels t (16 x RGBA, edge-clamped), mask m (texels to fit; the others are transparent in BC1 3-colour mode).
function encodeColour(t, codec, out, o) {
  const punch = codec === 1 && t.some((v, i) => (i & 3) === 3 && v < 128);
  const use = []; for (let i = 0; i < 16; i++) if (!punch || t[i * 4 + 3] >= 128) use.push(i);
  if (!use.length) { out[o] = out[o + 1] = out[o + 2] = out[o + 3] = 0; out[o + 4] = out[o + 5] = out[o + 6] = out[o + 7] = 255; return; }
  // principal axis of the colours (power iteration on the covariance), endpoints at the extreme projections
  let mr = 0, mg = 0, mb = 0; for (const i of use) { mr += t[i * 4]; mg += t[i * 4 + 1]; mb += t[i * 4 + 2]; }
  mr /= use.length; mg /= use.length; mb /= use.length;
  let xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
  for (const i of use) { const r = t[i * 4] - mr, g = t[i * 4 + 1] - mg, b = t[i * 4 + 2] - mb; xx += r * r; xy += r * g; xz += r * b; yy += g * g; yz += g * b; zz += b * b; }
  let ax = 1, ay = 1, az = 1;
  for (let k = 0; k < 6; k++) {
    const nx = xx * ax + xy * ay + xz * az,
      ny = xy * ax + yy * ay + yz * az,
      nz = xz * ax + yz * ay + zz * az,
      l = Math.hypot(nx, ny, nz);
    if (l < 1e-9) break;
    ax = nx / l;
    ay = ny / l;
    az = nz / l;
  }
  let lo = Infinity, hi = -Infinity;
  for (const i of use) { const d = (t[i * 4] - mr) * ax + (t[i * 4 + 1] - mg) * ay + (t[i * 4 + 2] - mb) * az; if (d < lo) lo = d; if (d > hi) hi = d; }
  let e0 = q565(mr + ax * hi, mg + ay * hi, mb + az * hi), e1 = q565(mr + ax * lo, mg + ay * lo, mb + az * lo);
  const pal = new Uint8Array(16), fit = (c0, c1) => {
    // order: 4-colour mode needs c0 > c1 (BC1; BC2 reads 4-colour anyway), the punch-through mode c0 <= c1
    if (punch ? c0 > c1 : c0 < c1) [c0, c1] = [c1, c0];
    const four = codec !== 1 || c0 > c1; palette(c0, c1, four, pal);
    let bits = 0, err = 0;
    for (let i = 0; i < 16; i++) {
      let best = 0, bd = Infinity;
      if (punch && t[i * 4 + 3] < 128) { bits |= 3 << (2 * i); continue; }
      for (let k = 0; k < (four ? 4 : 3); k++) {
        const dr = t[i * 4] - pal[k * 4],
          dg = t[i * 4 + 1] - pal[k * 4 + 1],
          db = t[i * 4 + 2] - pal[k * 4 + 2],
          d = dr * dr + dg * dg + db * db;
        if (d < bd) {
          bd = d;
          best = k;
        }
      }
      bits |= best << (2 * i); err += bd;
    }
    return { c0, c1, bits: bits >>> 0, err, four };
  };
  let best = fit(e0, e1);
  // one least-squares refinement of the endpoints for the chosen indices (4-colour mode)
  if (best.four && best.c0 !== best.c1) {
    const w0 = [1, 0, 2 / 3, 1 / 3]; let aa = 0, ab = 0, bb = 0; const ra = [0, 0, 0], rb = [0, 0, 0];
    for (let i = 0; i < 16; i++) {
      const a = w0[(best.bits >>> (2 * i)) & 3],
        b = 1 - a;
      aa += a * a;
      ab += a * b;
      bb += b * b;
      for (let c = 0; c < 3; c++) {
        ra[c] += a * t[i * 4 + c];
        rb[c] += b * t[i * 4 + c];
      }
    }
    const det = aa * bb - ab * ab;
    if (Math.abs(det) > 1e-6) {
      const p = [0, 1, 2].map((c) => (ra[c] * bb - rb[c] * ab) / det), q = [0, 1, 2].map((c) => (rb[c] * aa - ra[c] * ab) / det);
      const alt = fit(q565(...p), q565(...q)); if (alt.err < best.err) best = alt;
    }
  }
  out[o] = best.c0 & 255; out[o + 1] = best.c0 >> 8; out[o + 2] = best.c1 & 255; out[o + 3] = best.c1 >> 8;
  out[o + 4] = best.bits & 255; out[o + 5] = (best.bits >>> 8) & 255; out[o + 6] = (best.bits >>> 16) & 255; out[o + 7] = best.bits >>> 24;
}

/** RGBA texels -> blocks (BC1 with 1-bit alpha at 128, or BC2 with 4-bit alpha). Texels outside the image clamp to the edge. */
export function encodeBC(codec, data, width, height) {
  const step = blockBytes(codec), bw = Math.max(1, (width + 3) >> 2), bh = Math.max(1, (height + 3) >> 2), out = new Uint8Array(bw * bh * step), t = new Uint8Array(64);
  for (let by = 0; by < bh; by++) {
    for (let bx = 0; bx < bw; bx++) {
      for (let i = 0; i < 16; i++) {
        const x = Math.min(width - 1, bx * 4 + (i & 3)), y = Math.min(height - 1, by * 4 + (i >> 2)), s = (y * width + x) * 4;
        t[i * 4] = data[s]; t[i * 4 + 1] = data[s + 1]; t[i * 4 + 2] = data[s + 2]; t[i * 4 + 3] = data[s + 3];
      }
      const o = (by * bw + bx) * step;
      if (codec === 2) { for (let i = 0; i < 16; i += 2) out[o + (i >> 1)] = Math.round(t[i * 4 + 3] / 17) | (Math.round(t[(i + 1) * 4 + 3] / 17) << 4); encodeColour(t, 2, out, o + 8); }
      else encodeColour(t, 1, out, o);
    }
  }
  return out;
}

/** The mip chain of a CompressedTexture: [{width, height, data}] down to 1x1; level 0 is a copy of the stored blocks. */
export function bcMipChain(codec, blocks, width, height) {
  const levels = [{ width, height, data: blocks.slice() }];
  let level = { width, height, data: decodeBC(codec, blocks, width, height) };
  while (level.width > 1 || level.height > 1) {
    level = boxDown(level.data, level.width, level.height);
    levels.push({ width: level.width, height: level.height, data: encodeBC(codec, level.data, level.width, level.height) });
  }
  return levels;
}
