// Fog-particle puffs (pv fogPuffs; docs/visual-parity.md 41): SSB kind-5 instances, each pointing to a kind-4 set of puffs
// (tools/export_fog_puffs.py -> <package>/fog-puffs.json). The PS2 draws them through cPS2FogParticleMan (vtable 0x4882E0):
//  - cull 0x22A270 (VU0 0xDB8): the 8 corners of the instance's +0x68/+0x74 box through the frustum clip matrix, VU CLIP against
//    |w|: all 8 flagged on one side -> dropped (the node's occluder volumes, result 2, are not modelled: the occluding geometry hides
//    those puffs here);
//  - build 0x2DC190, per instance: centre depth dc = trunc(view depth of the box centre, cm); dc >= 25000 dropped; fade
//    f10 = 1 below 18000, else (25000 - dc) * 1/7000 (gp-0x3C58 / -0x3C54 / -0x3C50);
//    per puff (instance matrix x local position): d = trunc(view depth); d <= 500 dropped; f0 = (d - 500) * 0.00050008 below
//    2499.667 (gp-0x3C4C / -0x3C48), else 1; alpha f7 = f10 * f0 (0: dropped); a screen sprite centred on the puff, half extent
//    size * P / w (P = the GS scale 272.65 / -318.09 px: a view-aligned square of half side `size` cm), ST (0,0) at the
//    bottom-left corner, (1,1) top-right, one Z (the centre's); vertex RGBA = trunc(r, g, b, f7 x 128);
//    list 2 (box crossing the guard band) also drops sprites wholly off screen (the GPU clips them here);
//  - 0x2DBF98: qsort (0x418EF8, comparator 0x2DC168) far to near by the integer depth; material: texture fog0 (FX entry 4,
//    renderer +0xF60), CLAMP, TFX MODULATE, GS ALPHA 0x44 (Cs - Cd) x As >> 7 + Cd, ZTST GREATER (depth tested) without Z write
//    (ZMSK), alpha test GREATER 0, priority 7: after the fog composite (not fogged), before the glare and ScreenTint ->
//    drawn in the encoded composite (web/snow-composite.js) like the set-piece particles;
//  - draw 0x2DC7B0: GIF REGLIST sprites (PRIM 0x56: SPRITE, TME, ABE), per sprite ST RGBAQ XYZ2 ST XYZ2.
// Checked against PS2 frames: tools/export_fog_puffs.py --check-state (parse) and web/test-fog-puffs.mjs (sprites of the last
// PS2 frame in the savestate: 41/41 ARA1 tick 4818).
import * as T from 'three/webgpu';
import {attribute, texture, vec4, uniform, positionGeometry, modelViewMatrix, cameraProjectionMatrix, uv, select} from 'three/tsl';
import {toFrame} from './frame-space.js';
import {registerEncodedEffect} from './snow-composite.js';
import {pv} from './pv-flags.js';import {drawOrder, EFFECT, SUBMIT} from './ps2-draw-order.js';
import {setUpdateRange} from './heap-views.js';

const F = Math.fround;
// Streamed (CTM) packages with puffs (tools/export_fog_puffs.py; web/test-fog-puffs.mjs checks this against the exported files).
export const FOG_PUFF_PEAK = {ABC1: 1, ARA1: 1, ASS1: 1, CBA2: 2, CHP2: 2, DBC2: 2, DRA4: 2, DSS2: 2, E: 3, EBA3: 3, EBC3: 3, ERA5: 3, ESS3: 3};
export const streamedFogRoot = (code) => (FOG_PUFF_PEAK[code] ? `/assets/PEAK${FOG_PUFF_PEAK[code]}/${code}/` : null);
export const INSTANCE_FAR = 25000, INSTANCE_FADE = 18000, INSTANCE_RATE = F(0.0001428571413271129);
export const PUFF_NEAR = 500, PUFF_FADE_END = F(2499.66748046875), PUFF_RATE = F(0.0005000831442885101);

// 0x2DC190 fades (float32 like the EE): instance fade from the box centre's view depth (cm), 0 = dropped.
export function instanceFade(depthCm) {
  const d = Math.trunc(depthCm);
  if (INSTANCE_FAR <= d) return 0;
  return d < INSTANCE_FADE ? 1 : F(F(INSTANCE_FAR - d) * INSTANCE_RATE);
}
// a puff's alpha factor f7 (0 = dropped) from its view depth (cm) and the instance fade.
export function puffFade(instance, depthCm) {
  const d = Math.trunc(depthCm);
  if (d <= 0 || d <= PUFF_NEAR) return 0;
  const f0 = d < PUFF_FADE_END ? F(F(d - PUFF_NEAR) * PUFF_RATE) : 1;
  return F(instance * f0);
}
export const byte128 = (v) => Math.trunc(F(v * 128)); // swc1 v * 128.0 -> trunc.w.s (RGBAQ bytes, 128 = 1.0)

// A package's puffs in scene space. doc = fog-puffs.json; origin = the scene origin (metres).
export function fogPuffLayout(doc, origin = [0, 0, 0]) {
  const inst = doc?.instances ?? [], m = new T.Matrix4(), p = new T.Vector3();
  const count = inst.reduce((n, i) => n + i.puffs.length * Math.max(1, i.repeat), 0);
  const pos = new Float32Array(count * 3), size = new Float32Array(count), rgb = new Uint8Array(count * 3), owner = new Uint16Array(count);
  const corners = new Float32Array(inst.length * 24), centres = new Float32Array(inst.length * 3), chunks = [];
  let k = 0;
  inst.forEach((x, j) => {
    m.fromArray(x.matrix); // qword 3 = translation (0x2DC190: C = M x instance, then C x (x, y, z, 1))
    for (let r = 0; r < Math.max(1, x.repeat); r++) for (const q of x.puffs) { // 0x2DC190 reads entry 0 for every entry of the kind-4 record
      p.set(q[0], q[1], q[2]).applyMatrix4(m);
      pos[3 * k] = p.x / 100 - origin[0]; pos[3 * k + 1] = p.z / 100 - origin[1]; pos[3 * k + 2] = -p.y / 100 - origin[2];
      size[k] = q[6] / 100; rgb[3 * k] = byte128(q[3]); rgb[3 * k + 1] = byte128(q[4]); rgb[3 * k + 2] = byte128(q[5]); owner[k] = j; k++;
    }
    const lo = x.box_min, hi = x.box_max; // source (x, y, z) -> scene (x, z, -y): an axis-aligned box stays one
    let q = 24 * j; // VU0 0x0AC8 corner order: x fastest over (min, max), then y, then z
    for (const z of [lo[2], hi[2]]) for (const y of [lo[1], hi[1]]) for (const x of [lo[0], hi[0]]) { corners[q++] = x / 100 - origin[0]; corners[q++] = z / 100 - origin[1]; corners[q++] = -y / 100 - origin[2]; }
    const c = [F(F(lo[0] + hi[0]) * 0.5), F(F(lo[1] + hi[1]) * 0.5), F(F(lo[2] + hi[2]) * 0.5)];
    centres[3 * j] = c[0] / 100 - origin[0]; centres[3 * j + 1] = c[2] / 100 - origin[1]; centres[3 * j + 2] = -c[1] / 100 - origin[2];
    chunks.push(x.chunk);
  });
  return {count, pos, size, rgb, owner, corners, centres, chunks, instances: inst.length};
}

// VU0 0xDB8 / 0x0AC8: M (column-major clip matrix) x each corner, CLIP against |w|; true when all corners share a flag (result 1).
export function clipCulled(M, corners, at) {
  let all = 63;
  for (let k = 0; k < 8; k++) {
    const x = corners[at + 3 * k], y = corners[at + 3 * k + 1], z = corners[at + 3 * k + 2];
    const cx = M[0] * x + M[4] * y + M[8] * z + M[12], cy = M[1] * x + M[5] * y + M[9] * z + M[13], cz = M[2] * x + M[6] * y + M[10] * z + M[14], w = Math.abs(M[3] * x + M[7] * y + M[11] * z + M[15]);
    all &= (cx > w ? 1 : cx < -w ? 2 : 0) | (cy > w ? 4 : cy < -w ? 8 : 0) | (cz > w ? 16 : cz < -w ? 32 : 0);
    if (!all) return false;
  }
  return all !== 0;
}

// Per frame: the sprites 0x2DC190 builds for this camera, sorted far to near (stable; the PS2's qsort orders equal integer depths
// its own way). keep(j) = instance j is resident and not culled. Returns the sprite count; out.order / out.alpha / out.depth filled.
export function buildFogSprites(layout, viewMatrix, keep, out) {
  const e = viewMatrix.elements, depth = (x, y, z) => -(e[2] * x + e[6] * y + e[10] * z + e[14]) * 100; // view depth, cm
  const fade = out.fade ??= new Float32Array(layout.instances);
  for (let j = 0; j < layout.instances; j++) {
    const c = layout.centres;
    fade[j] = keep(j) ? instanceFade(depth(c[3 * j], c[3 * j + 1], c[3 * j + 2])) : 0;
  }
  const n = layout.count, P = layout.pos, order = out.order ??= [], alpha = out.alpha ??= new Uint8Array(n), dep = out.depth ??= new Int32Array(n);
  order.length = 0;
  for (let k = 0; k < n; k++) {
    const f10 = fade[layout.owner[k]]; if (!f10) continue;
    const d = depth(P[3 * k], P[3 * k + 1], P[3 * k + 2]), f7 = puffFade(f10, d);
    if (!f7) continue;
    alpha[k] = byte128(f7); dep[k] = Math.trunc(d); order.push(k);
  }
  order.sort((a, b) => dep[b] - dep[a]); // far to near (comparator 0x2DC168)
  return order.length;
}

async function loadFog0(fetchJson, fetchBytes) {
  const meta = await fetchJson('/assets/PARTICLES/textures/textures.json'), t = meta.textures.find((x) => x.tag === 'fog0');
  if (!t) throw Error('fog0 texture missing (PARTICLES/textures)');
  const bytes = new Uint8Array(await fetchBytes('/assets/PARTICLES/textures/' + t.file));
  if (bytes.length !== t.width * t.height * 4) throw Error('fog0 texture extent');
  const map = new T.DataTexture(bytes, t.width, t.height, T.RGBAFormat);
  map.minFilter = map.magFilter = T.LinearFilter; map.wrapS = map.wrapT = T.ClampToEdgeWrapping; map.colorSpace = T.NoColorSpace; map.needsUpdate = true;
  return {map, scale: t.gs_alpha_scale};
}

// One package's puffs. root = the package folder URL (/assets/ARA1/, /assets/PEAK1/ARA1/); resident(chunk) -> false when the
// original has streamed that chunk out. Resolves to null when the package has no puffs.
export async function createFogPuffs({root, origin = [0, 0, 0], resident = () => true, fetchJson = (p) => fetch(p).then((r) => { if (!r.ok) throw Error(`${p}: ${r.status}`); return r.json(); }), fetchBytes = (p) => fetch(p).then((r) => r.arrayBuffer())}) {
  const doc = await fetchJson(root + 'fog-puffs.json').catch(() => null);
  if (!doc?.instances?.length) return null;
  const layout = fogPuffLayout(doc, origin); if (!layout.count) return null;
  const tex = await loadFog0(fetchJson, fetchBytes);
  const capacity = layout.count, geometry = new T.PlaneGeometry(2, 2);
  for (let v = 0; v < geometry.attributes.uv.count; v++) geometry.attributes.uv.setXY(v, (geometry.attributes.position.getX(v) + 1) / 2, (geometry.attributes.position.getY(v) + 1) / 2); // ST (0,0) bottom-left
  const centre = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage); // scene xyz, w = half side (m)
  const colour = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage); // RGBAQ bytes / 128
  geometry.setAttribute('fpCentre', centre); geometry.setAttribute('fpColour', colour);
  const encodedOutput = uniform(false);
  const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: true, side: T.DoubleSide, forceSinglePass: true, fog: false, toneMapped: false});
  const c = attribute('fpCentre', 'vec4'), view = modelViewMatrix.mul(vec4(c.xyz, 1));
  material.vertexNode = cameraProjectionMatrix.mul(vec4(view.x.add(positionGeometry.x.mul(c.w)), view.y.add(positionGeometry.y.mul(c.w)), view.z, 1));
  const texel = texture(tex.map, uv()), col = attribute('fpColour', 'vec4');
  const rgb = texel.rgb.mul(col.rgb).clamp(0, 1), As = texel.a.mul(tex.scale).mul(col.a); // MODULATE; As / 128
  material.fragmentNode = vec4(select(encodedOutput, rgb, toFrame(rgb)), As.clamp(0, 1)); // 0x44: (Cs - Cd) x As + Cd
  material.name = 'FogPuffs';
  const mesh = new T.InstancedMesh(geometry, material, capacity);
  mesh.count = 1; mesh.frustumCulled = false; mesh.renderOrder = pv('effectOrder') ? drawOrder(EFFECT.fogPuffs, SUBMIT.fogPuffs) : 684; mesh.name = 'fog puffs'; // pv effectOrder: 0x364240, t0 1023: first at priority 7
  const group = new T.Group(); group.name = 'fog puffs'; group.add(mesh);
  const out = {}, clip = new T.Matrix4(), mv = new T.Matrix4(), state = {sprites: 0, instances: layout.instances, puffs: layout.count};
  const visible = (j) => layout.chunks[j] === undefined || resident(layout.chunks[j]) !== false;
  // The sprites for the camera that draws: from the encoded composite's populated(camera) and again right before the draw.
  function prepare(camera) {
    group.updateWorldMatrix(true, false);
    mv.multiplyMatrices(camera.matrixWorldInverse, group.matrixWorld);
    // the frustum clip matrix of 0xDB8 (VU0 64..67): the camera's projection with its z row mapping near / far to -w / +w
    clip.copy(camera.projectionMatrix); const e = clip.elements, n0 = camera.near, f0 = camera.far;
    e[2] = 0; e[6] = 0; e[10] = -(f0 + n0) / (f0 - n0); e[14] = -2 * f0 * n0 / (f0 - n0); clip.multiply(mv);
    const M = clip.elements, keep = (j) => visible(j) && !clipCulled(M, layout.corners, 24 * j);
    const n = buildFogSprites(layout, mv, keep, out), C = centre.array, Co = colour.array, P = layout.pos;
    for (let i = 0; i < n; i++) {
      const k = out.order[i], o = 4 * i;
      C[o] = P[3 * k]; C[o + 1] = P[3 * k + 1]; C[o + 2] = P[3 * k + 2]; C[o + 3] = layout.size[k];
      Co[o] = layout.rgb[3 * k] / 128; Co[o + 1] = layout.rgb[3 * k + 1] / 128; Co[o + 2] = layout.rgb[3 * k + 2] / 128; Co[o + 3] = out.alpha[k] / 128;
    }
    if (!n) { C.fill(0, 0, 4); Co.fill(0, 0, 4); } // one empty sprite (zero size, alpha 0): the pipeline stays built and warmable
    setUpdateRange(centre, 0, Math.max(n, 1) * 4); setUpdateRange(colour, 0, Math.max(n, 1) * 4);
    mesh.count = Math.max(n, 1); state.sprites = n;
    return n;
  }
  mesh.onBeforeRender = (renderer, scene, camera) => { prepare(camera); };
  let disposed = false;
  const shown = () => { let o = group; while (o.parent) { if (!o.visible) return false; o = o.parent; } return o.isScene === true && o.visible; };
  const effect = registerEncodedEffect({object: group, setEncodedOutput: (v) => { encodedOutput.value = !!v; }, populated: (camera) => !disposed && shown() && (camera ? prepare(camera) > 0 : state.sprites > 0)});
  return {
    group, mesh, layout, state, prepare, effect,
    dispose() { disposed = true; geometry.dispose(); material.dispose(); tex.map.dispose(); group.removeFromParent(); },
  };
}
