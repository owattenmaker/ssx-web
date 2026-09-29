// QA page for the texture-archive texels (web/test-texture-archive-decode.mjs, docs/asset-formats.md "Texture archives"): loads
// archive entries through the game's own path (web/texture-archive.js packageTexture -> THREE texture -> WebGPU upload
// by three.js), reads the GPU texels back and compares their SHA-256 with the digest the archive index records (the
// exact source texels). Also records what the browser's own image decoder would have uploaded (<img> from a revoked
// blob URL, the path shipped until 2026-09-26; a decoded <img>; ImageBitmap 'none'/'none'), which shows whether this
// engine mangles indexed PNGs (WebKit does). Result: window.__textureDecode (a Promise) and #result.
import { WebGPURenderer, TextureLoader, SRGBColorSpace } from 'three/webgpu';
import { packageTexture, textureArchive, archiveBlob } from './texture-archive.js';
import { decodePngTexels } from './png-texels.js';

const out = document.getElementById('result');
const hex = async (bytes) => Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))).map((b) => b.toString(16).padStart(2, '0')).join('').slice(0, 16);
const unpad = (buf, w, h) => { const src = new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength), row = Math.ceil((w * 4) / 256) * 256, o = new Uint8Array(w * h * 4); for (let y = 0; y < h; y++) o.set(src.subarray(y * row, y * row + w * 4), y * w * 4); return o; };
const loadImg = (src) => new Promise((ok, fail) => { const im = new Image(); im.onload = () => ok(im); im.onerror = fail; im.src = src; });

async function run() {
  const renderer = new WebGPURenderer({ antialias: false });
  await renderer.init();
  if (!renderer.backend.isWebGPUBackend) return { webgpu: false, ua: navigator.userAgent };
  const device = renderer.backend.device, loader = new TextureLoader();
  // the browser's own decoder: straight copyExternalImageToTexture (what three.js did with an <img> / ImageBitmap)
  const external = async (source, w, h) => {
    const tex = device.createTexture({ size: [w, h], format: 'rgba8unorm', usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.RENDER_ATTACHMENT });
    device.queue.copyExternalImageToTexture({ source, flipY: false }, { texture: tex, premultipliedAlpha: false }, [w, h]);
    const row = Math.ceil((w * 4) / 256) * 256, buf = device.createBuffer({ size: row * h, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
    const enc = device.createCommandEncoder(); enc.copyTextureToBuffer({ texture: tex }, { buffer: buf, bytesPerRow: row }, [w, h]); device.queue.submit([enc.finish()]);
    await buf.mapAsync(GPUMapMode.READ); const data = unpad(new Uint8Array(buf.getMappedRange().slice(0)), w, h); buf.unmap(); buf.destroy(); tex.destroy(); return data;
  };
  const pick = async (url, n) => {
    const a = await textureArchive(url), all = a.index.entries;
    if (!n || all.length <= n) return all.map((e) => [url, e]);
    // spread over the archive, plus the first few 16-colour and first few translucent entries (decoded to tell)
    const chosen = new Set(all.filter((_, i) => i % Math.ceil(all.length / n) === 0));
    for (const e of all.filter((e) => e.colours <= 16).slice(0, 3)) chosen.add(e);
    let translucent = 0;
    for (const e of all) { if (translucent >= 4) break; if (e.colours > 256) continue; const t = await decodePngTexels(await (await archiveBlob(url, e.id)).arrayBuffer()); let min = 255; for (let i = 3; i < t.data.length; i += 4) min = Math.min(min, t.data[i]); if (min < 255) { chosen.add(e); translucent++; } }
    return [...chosen].map((e) => [url, e]);
  };
  const entries = [...await pick('/assets/WARDROBE/ZOE/textures.tex'), ...await pick('/assets/WARDROBE/MAC/textures.tex'), ...await pick('/assets/TEXTURES/world.tex', 12)];
  const rows = [];
  for (const [url, e] of entries) {
    const row = { archive: url.replace('/assets/', ''), id: e.id, width: e.width, height: e.height, colours: e.colours };
    try {
      // the game path (main.js / opponent-riders.js / cutscenes.js): packageTexture -> three.js upload
      const tex = await packageTexture(loader, '/assets/', { pack: url, id: e.id });
      tex.flipY = false; tex.colorSpace = SRGBColorSpace; renderer.initTexture(tex);
      const gpu = unpad(await renderer.backend.copyTextureToBuffer(tex, 0, 0, e.width, e.height, 0), e.width, e.height);
      row.game = (await hex(gpu)) === e.rgba; row.dataTexture = !!tex.isDataTexture; tex.dispose();
      // the browser decoders, for the record
      const blob = await archiveBlob(url, e.id);
      { const u = URL.createObjectURL(blob), im = await loadImg(u); URL.revokeObjectURL(u); row.imgRevoked = (await hex(await external(im, e.width, e.height))) === e.rgba; }
      { const u = URL.createObjectURL(blob), im = await loadImg(u); await im.decode(); row.imgDecoded = (await hex(await external(im, e.width, e.height))) === e.rgba; URL.revokeObjectURL(u); }
      { const bm = await createImageBitmap(blob, { premultiplyAlpha: 'none', colorSpaceConversion: 'none' }); row.imageBitmap = (await hex(await external(bm, e.width, e.height))) === e.rgba; bm.close(); }
    } catch (err) { row.error = String(err?.stack || err); }
    rows.push(row); out.textContent += '\n' + JSON.stringify(row);
  }
  renderer.dispose();
  return { webgpu: true, ua: navigator.userAgent, rows };
}
window.__textureDecode = run().catch((e) => ({ error: String(e?.stack || e) }));
window.__textureDecode.then((r) => { out.textContent += '\ndone'; window.__textureDecodeResult = r; });
