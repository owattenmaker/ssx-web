// QA page (web/test-frame-space.mjs; docs/visual-parity.md section 38): the GS blend ALPHA 0x44, ((Cs - Cd) x As >> 7) + Cd on bytes, drawn the
// way the game draws a world-pass blend: byte-domain materials writing web/frame-space.js toFrame(bytes / 255) into a half-float target (the
// world pass), read back as the fog composite reads it (fromFrame, then bytes). Every source byte x 8 destinations x 8 GS alphas.
// ?pv=encodedBlend: the encoded frame. ?backend=webgl: three's WebGL2 backend.
import * as T from 'three/webgpu';
import {texture,uv,vec4} from 'three/tsl';
import {encodedFrame,toFrame} from './frame-space.js';
const W = 64, H = 256, DS = [0, 36, 73, 109, 146, 182, 219, 255], AS = [8, 16, 32, 48, 64, 80, 96, 120];
const oetf = (l) => (l <= 0.0031308 ? l * 12.92 : 1.055 * l ** (1 / 2.4) - 0.055);
const gs = (s, d, A) => Math.max(0, Math.min(255, (((s - d) * A) >> 7) + d));
const out = document.querySelector('#result'), result = { encoded: encodedFrame };
let renderer = null;
try {
  if (!navigator.gpu && new URL(location.href).searchParams.get('backend') !== 'webgl') { result.webgpu = false; throw null; }
  renderer = new T.WebGPURenderer({ antialias: false, forceWebGL: new URL(location.href).searchParams.get('backend') === 'webgl' });
  renderer.setSize(W, H); await renderer.init(); result.backend = renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2';
  const tex = (data) => { const t = new T.DataTexture(data, W, H, T.RGBAFormat, T.UnsignedByteType); t.colorSpace = T.NoColorSpace; t.minFilter = t.magFilter = T.NearestFilter; t.generateMipmaps = false; t.needsUpdate = true; return t; };
  const dst = new Uint8Array(W * H * 4), src = new Uint8Array(W * H * 4);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const o = (y * W + x) * 4, d = DS[x & 7]; dst.set([d, (d + y) & 255, y, 255], o); src.set([y, y, y, Math.round(AS[x >> 3] / 128 * 255)], o); }
  const target = new T.RenderTarget(W, H, { type: T.HalfFloatType, depthBuffer: false });   // the world pass's colour target
  const camera = new T.OrthographicCamera(-1, 1, 1, -1, 0, 1), geometry = new T.PlaneGeometry(2, 2), dstTex = tex(dst), srcTex = tex(src);
  const md = new T.MeshBasicNodeMaterial(); md.outputNode = vec4(toFrame(texture(dstTex, uv()).rgb), 1);
  const ms = new T.MeshBasicNodeMaterial({ transparent: true, depthTest: false, depthWrite: false }); const st = texture(srcTex, uv()); ms.outputNode = vec4(toFrame(st.rgb), st.a);
  const scene = new T.Scene(), a = new T.Mesh(geometry, md), b = new T.Mesh(geometry, ms); b.renderOrder = 1; scene.add(a, b);
  // three's WebGL2 readback polls its fence with requestAnimationFrame, which an offscreen WebKit window never runs: poll with a timer here
  if (!renderer.backend.isWebGPUBackend) window.requestAnimationFrame = (f) => setTimeout(() => f(performance.now()), 4);
  const read = async () => { let px = await renderer.readRenderTargetPixelsAsync(target, 0, 0, W, H); if (px instanceof Uint16Array) px = Float32Array.from(px, (h) => T.DataUtils.fromHalfFloat(h)); return px; };
  const toByte = (v) => Math.round((encodedFrame ? Math.min(1, Math.max(0, v)) : oetf(Math.min(1, Math.max(0, v)))) * 255);
  renderer.setRenderTarget(target);
  // opaque: the destination alone must come back as its bytes, in both frame spaces; the row order is found from it
  b.visible = false; renderer.render(scene, camera); const opaque = await read();
  let flip = null;
  for (const f of [false, true]) { let bad = 0; for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const o = ((f ? H - 1 - y : y) * W + x) * 4, i = (y * W + x) * 4; for (let c = 0; c < 3; c++) if (toByte(opaque[o + c]) !== dst[i + c]) bad++; } if (!bad) { flip = f; break; } }
  result.opaqueExact = flip !== null;
  b.visible = true; renderer.render(scene, camera); const blended = await read();
  let sum = 0, max = 0, within1 = 0, n = 0; const row = (y) => (flip ? H - 1 - y : y);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const got = toByte(blended[(row(y) * W + x) * 4]), e = Math.abs(got - gs(y, DS[x & 7], AS[x >> 3])); sum += e; max = Math.max(max, e); if (e <= 1) within1++; n++; }
  Object.assign(result, { blends: n, mean: +(sum / n).toFixed(2), max, within1: +(within1 / n).toFixed(4) });
  target.dispose(); md.dispose(); ms.dispose(); dstTex.dispose(); srcTex.dispose(); geometry.dispose();
} catch (e) { if (e) result.error = String(e?.stack ?? e); }
finally { renderer?.dispose(); }
result.ua = navigator.userAgent; window.__frameSpace = result; out.textContent = JSON.stringify(result);
