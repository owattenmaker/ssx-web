// QA page for the Xbox HD rider textures (web/test-texture-archive-decode.mjs part 3, docs/xbox-textures.md section 8): Zoe's and Mac's
// textures-xbox.tex entries through the game's own path (web/texture-archive.js packageTexture with the Xbox HD set chosen) ->
// three.js upload. On WebGPU with 'texture-compression-bc' that is a CompressedTexture (the Xbox's BC blocks + the worker's mip
// chain); each level is drawn texel for texel (nearest, textureLod) into an RGBA8 target and read back, against web/bc-texels.js
// (level 0: decodeBC of the stored blocks; levels 1..: decodeBC of the chain the worker built). GPUs may round the BC1 thirds
// differently from the reference integer rule (D3D allows ~3%): max |difference| and the share of texels off by more than 2 are
// reported. Without the feature the DataTexture path is checked the same way (exact). Result: window.__bcTexture.
import { WebGPURenderer, NoColorSpace, NearestFilter, RenderTarget, Mesh, PlaneGeometry, OrthographicCamera, Scene, MeshBasicNodeMaterial, UnsignedByteType } from 'three/webgpu';
import { texture as tslTexture, uv, vec4, float } from 'three/tsl';
import { packageTexture, configureTextureDecode, textureArchive, archiveBlob } from './texture-archive.js';
import { quality } from './quality.js';
import { parseBCEntry, decodeBC, bcMipChain } from './bc-texels.js';

const out = document.getElementById('result');
async function run() {
  const renderer = new WebGPURenderer({ antialias: false });
  await renderer.init();
  if (!renderer.backend.isWebGPUBackend) return { webgpu: false, ua: navigator.userAgent };
  const config = configureTextureDecode({ renderer });
  quality.riderTextures = 'xbox';
  const rows = [], camera = new OrthographicCamera(-1, 1, 1, -1, 0, 1), scene = new Scene(), quad = new Mesh(new PlaneGeometry(2, 2));
  scene.add(quad);
  for (const url of ['/assets/WARDROBE/ZOE/textures.tex', '/assets/WARDROBE/MAC/textures.tex']) {
    const a = await textureArchive(url);
    for (const e of a.index.entries.filter((x) => x.codec)) {
      const row = { archive: url.replace('/assets/', '').replace('.tex', '-xbox.tex'), id: e.id, codec: e.codec, width: e.width, height: e.height };
      try {
        const tex = await packageTexture(null, '/assets/', { pack: url, id: e.id });
        tex.flipY = false; tex.colorSpace = NoColorSpace; tex.magFilter = tex.minFilter = NearestFilter; tex.needsUpdate = true;
        row.compressed = !!tex.isCompressedTexture; row.domain = tex.userData.entryDomain;
        const entry = parseBCEntry(await (await archiveBlob(url.replace('.tex', '-xbox.tex'), e.id)).arrayBuffer());
        const chain = bcMipChain(entry.codec, entry.blocks, entry.width, entry.height);
        const levels = row.compressed ? chain.length : 1;
        row.levels = [];
        for (let level = 0; level < levels; level++) {
          const w = Math.max(1, e.width >> level), h = Math.max(1, e.height >> level), target = new RenderTarget(w, h, { type: UnsignedByteType, depthBuffer: false });
          const m = new MeshBasicNodeMaterial();
          m.colorNode = tslTexture(tex, uv()).level(float(level));
          m.opacityNode = null;
          m.outputNode = tslTexture(tex, uv()).level(float(level));
          quad.material = m;
          renderer.setRenderTarget(target); renderer.render(scene, camera); renderer.setRenderTarget(null);
          // WebGPU readback rows are padded to 256 bytes
          const raw = new Uint8Array(await renderer.readRenderTargetPixelsAsync(target, 0, 0, w, h)), stride = raw.length === w * h * 4 ? w * 4 : Math.ceil((w * 4) / 256) * 256;
          const ref = decodeBC(entry.codec, chain[level].data, w, h);
          // rows: the readback is either orientation depending on the backend's target origin; take the one that matches
          const compare = (flip) => {
            let max = 0,
              off = 0;
            for (let y = 0; y < h; y++)
              for (let x = 0; x < w; x++)
                for (let c = 0; c < 4; c++) {
                  const d = Math.abs(raw[(flip ? h - 1 - y : y) * stride + x * 4 + c] - ref[(y * w + x) * 4 + c]);
                  if (d > max) max = d;
                  if (d > 2) off++;
                }
            return { max, off };
          };
          const a = compare(false), f = compare(true), best = f.off < a.off || (f.off === a.off && f.max < a.max) ? f : a;
          row.levels.push({ level, w, h, max: best.max, offShare: +(best.off / (w * h * 4)).toFixed(4) });
          target.dispose(); m.dispose();
        }
        tex.dispose();
      } catch (err) { row.error = String(err?.stack || err); }
      rows.push(row); out.textContent += '\n' + JSON.stringify(row);
    }
  }
  renderer.dispose();
  return { webgpu: true, bc: config.bc, ua: navigator.userAgent, rows };
}
window.__bcTexture = run().catch((e) => ({ error: String(e?.stack || e) }));
window.__bcTexture.then((r) => { out.textContent += '\ndone'; window.__bcTextureResult = r; });
