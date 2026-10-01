// Avalanche trails (avalancheTrails; docs/avalanche.md "Trails"). The avalanche component's draw 0x2D9130 (vt +0x1C) runs
// 0x2D8EA8(slot) for each active slot, and that runs 0x371688(emitter, 7) for each playing tumbler whose group has an emitter:
// the colour emitter's birth ring drawn by particle program 0x439A40 entry 0xA00 (renderer +0x2A4 -> 0x380CE0, word0 0x100),
// the rider snow's path. 0x371688 draws only with +0x174 (enabled) set and +0x1E0 (live births) > 0.
// The core exports the emitter images and rings (web/avalanche_gameplay.inc avalanche_trails). The sprites here are
// engine/snow_particles.cpp OriginalSnowParticles::particles() / originalSnowParticle() with the EE float operations
// (web/ee-scalar-float.js). Every retail trail uses texture 4 'fog0', one flip frame, blend mode 1 (GS 0x44): MODULATE
// (colour / 128), alpha blended, depth tested, no Z write; priority 7 after the fog composite: the encoded pass, key rank 1.
import * as T from 'three/webgpu';
import {attribute, texture, vec4, select, uniform, positionGeometry, modelViewMatrix, cameraProjectionMatrix, uv} from 'three/tsl';
import {toFrame} from './frame-space.js';
import {registerEncodedEffect} from './snow-composite.js';
import {mul, add, sub, div, fromBits} from './ee-scalar-float.js';
import {drawOrder, SUBMIT} from './ps2-draw-order.js';

// Dynamic emitter (engine/set_piece_particles.hpp dynamic_emitter) and particle kernel (particle_kernel) offsets, bytes.
const E = {Texture: 0x4, Blend: 0x8, FlipCount: 0xC, FlipPhase: 0x10, Kernel: 0x20, Enabled: 0x174, Cursor: 0x17C, Active: 0x1E0};
const K = {N: 0x00, AgeStep: 0x0C, SizeRange: 0x10, LifeRange: 0x14, SizeBase: 0x18, LifeBase: 0x1C, SizeDelta: 0x20, Force: 0x60,
  VelBase: 0x70, VelR0: 0x80, VelR1: 0x90, VelR2: 0xA0, PosBase: 0xB0, PosR0: 0xC0, PosR1: 0xD0, ColBase: 0xF0, ColR0: 0x100,
  ColR1: 0x110, ColSlope: 0x120};
export const TRAIL_FLOATS = 8; // x, y, z (source cm), half extent (cm), r, g, b, a (GS integers, 128 = 1)
const IMAGE_WORDS = 130;       // +0x000 .. +0x207
const T_MAX = Math.fround(2.700000047683716), P1 = Math.fround(-0.7300000190734863), P2 = Math.fround(0.11299999803304672), INV255 = Math.fround(0.003921568859368563);
const MAX_HALF_PIXELS = 128, SOURCE_VIEWPORT = [512, 448]; // entry 0xA00's size cap (SNOW_FX view: max_projected_half_extent 128)

// The core export: [count, then per trail: resource, pool index, 130 image words, n, ring A (n x 4), ring B (n x 4), colours (n)].
export function readAvalancheTrails(core) {
  if (!core._avalanche_trails) return [];
  const U = new Uint32Array(core.HEAPU8.buffer), Fv = new Float32Array(core.HEAPU8.buffer);
  let at = core._avalanche_trails() >> 2; const count = U[at++], out = [];
  for (let k = 0; k < count; k++) {
    const resource = U[at], index = U[at + 1]; at += 2;
    const image = U.subarray(at, at + IMAGE_WORDS), imageF = Fv.subarray(at, at + IMAGE_WORDS); at += IMAGE_WORDS;
    const n = U[at++];
    const ringA = Fv.subarray(at, at + 4 * n); at += 4 * n;
    const ringB = Fv.subarray(at, at + 4 * n), ringBits = U.subarray(at, at + 4 * n); at += 4 * n;
    const colours = U.subarray(at, at + n); at += n;
    out.push({resource, index, image, imageF, n, ringA, ringB, ringBits, colours,
      texture: image[E.Texture >> 2] | 0, blend: image[E.Blend >> 2] | 0, flipCount: image[E.FlipCount >> 2] | 0, flipPhase: imageF[E.FlipPhase >> 2]});
  }
  return out;
}

const next = (s) => ((((s << 1) ^ ((s >>> 4) & 1) ^ ((s >>> 22) & 1)) & 0x7fffff) | 0x3f800000) >>> 0;
const R = new Float32Array(9);
// The sprites of one trail into out (TRAIL_FLOATS each) from sprite index n; returns the new count (at most limit).
export function trailSprites(t, out, n = 0, limit = Infinity) {
  const w = (o) => t.image[o >> 2], f = (o) => t.imageF[o >> 2], kf = (o) => f(E.Kernel + o);
  if (!w(E.Enabled) || (w(E.Active) | 0) <= 0) return n;
  const births = t.n, per = births ? Math.trunc((w(E.Kernel + K.N) | 0) / births) : 0; if (per <= 0) return n;   // kernel N = particles x ring slots
  const kv = (o) => [kf(o), kf(o + 4), kf(o + 8)], kc = (o) => [kf(o), kf(o + 4), kf(o + 8), kf(o + 12)];
  const ageStep = kf(K.AgeStep), invCount = div(1, per), lifeBase = kf(K.LifeBase), lifeRange = kf(K.LifeRange);
  const sizeBase = kf(K.SizeBase), sizeRange = kf(K.SizeRange), sizeDelta = kf(K.SizeDelta);
  const force = kv(K.Force), velBase = kv(K.VelBase), velR = [kv(K.VelR0), kv(K.VelR1), kv(K.VelR2)];
  const posBase = kv(K.PosBase), posR0 = kv(K.PosR0), posR1 = kv(K.PosR1);
  const colBase = kc(K.ColBase), colR0 = kc(K.ColR0), colR1 = kc(K.ColR1), colSlope = kc(K.ColSlope);
  const cursor = w(E.Cursor) >>> 0, A = t.ringA, B = t.ringB, bytes = (s, i) => (t.colours[s] >>> (8 * i)) & 255;
  let age = 0;
  for (let group = 0; group < births; group++) {
    if (group % 43 === 0) age = mul(mul(group - (group % 43), per), ageStep);
    const a = (cursor + 1 + group) % births, b = (cursor + 1 + Math.min(group + 1, births - 1)) % births;
    if (!(B[a * 4 + 3] >= 1)) { age = add(age, mul(ageStep, per)); continue; }
    let random = ((t.ringBits[a * 4 + 3] & 0x7fffff) | 0x3f800000) >>> 0, fraction = 0;
    for (let i = 0; i < per; i++, age = add(age, ageStep), fraction = add(fraction, invCount)) {
      for (let k = 0; k < 9; k++) { random = next(random); R[k] = fromBits(random); }
      const lifetime = add(lifeBase, mul(lifeRange, R[8])); if (!(age < lifetime)) continue;
      if (n >= limit) return n;
      const complement = sub(1, fraction), tt = Math.min(age, T_MAX), poly = add(mul(tt, P1), mul(mul(tt, tt), P2)), o = n * TRAIL_FLOATS;
      for (let c = 0; c < 3; c++) {
        let v = add(mul(B[a * 4 + c], complement), mul(B[b * 4 + c], fraction));
        v = add(add(add(add(v, velBase[c]), mul(velR[0][c], R[2])), mul(velR[1][c], R[3])), mul(velR[2][c], R[4]));
        let p = add(mul(A[a * 4 + c], complement), mul(A[b * 4 + c], fraction));
        p = add(add(add(p, posBase[c]), mul(posR0[c], R[0])), mul(posR1[c], R[1]));
        p = add(mul(force[c], age), p); p = add(p, mul(sub(force[c], v), poly));
        out[o + c] = p;
      }
      let size = mul(mul(sizeDelta, age), div(1, lifetime)); size = add(add(size, sizeBase), mul(sizeRange, R[7]));
      out[o + 3] = Math.abs(size);
      for (let c = 0; c < 4; c++) {
        let tint = add(mul(bytes(a, c), complement), mul(bytes(b, c), fraction)); tint = mul(tint, INV255);
        let value = add(add(add(mul(colBase[c], tint), mul(colR0[c], R[5])), mul(colSlope[c], age)), mul(colR1[c], R[6]));
        value = Math.max(c < 3 ? Math.min(value, 128) : value, 0);
        out[o + 4 + c] = Math.min(255, Math.max(0, Math.trunc(value)));
      }
      n++;
    }
  }
  return n;
}

// origin: the scene origin (metres, main.js start.json position). One instanced batch per texture id.
export async function createAvalancheTrails({core, origin = [0, 0, 0], capacity = 8192, fetchJson = (p) => fetch(p).then((r) => r.json()), fetchBytes = (p) => fetch(p).then((r) => r.arrayBuffer())}) {
  const meta = await fetchJson('/assets/PARTICLES/textures/textures.json'), maps = new Map();
  await Promise.all(meta.textures.filter((t) => t.id === 4).map(async (t) => {   // fog0: every retail trail
    const bytes = new Uint8Array(await fetchBytes('/assets/PARTICLES/textures/' + t.file));
    if (bytes.length !== t.width * t.height * 4) throw Error('Avalanche trail texture extent ' + t.tag);
    const map = new T.DataTexture(bytes, t.width, t.height, T.RGBAFormat);
    map.minFilter = map.magFilter = T.LinearFilter; map.wrapS = map.wrapT = T.ClampToEdgeWrapping; map.colorSpace = T.NoColorSpace; map.needsUpdate = true;
    maps.set(t.id, {map, scale: t.gs_alpha_scale});
  }));
  const encodedOutput = uniform(false), group = new T.Group(); group.name = 'avalanche trails'; group.userData.gameplayOnly = true;
  group.position.set(-origin[0], -origin[1], -origin[2]);
  const meshes = new Map();
  function build(id) {
    const tex = maps.get(id); if (!tex) return null;
    const geometry = new T.PlaneGeometry(2, 2);
    for (let v = 0; v < geometry.attributes.uv.count; v++) geometry.attributes.uv.setY(v, 1 - geometry.attributes.uv.getY(v));
    const centre = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage);
    const colour = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage);
    geometry.setAttribute('avCentre', centre); geometry.setAttribute('avColour', colour);
    const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: true, side: T.DoubleSide, forceSinglePass: true, fog: false, toneMapped: false});
    const c = attribute('avCentre', 'vec4'), clip = cameraProjectionMatrix.mul(modelViewMatrix.mul(vec4(c.xyz, 1)));
    const dx = cameraProjectionMatrix.mul(vec4(c.w, 0, 0, 0)).x.abs().min(clip.w.mul(MAX_HALF_PIXELS / (SOURCE_VIEWPORT[0] / 2)));
    const dy = cameraProjectionMatrix.mul(vec4(0, c.w, 0, 0)).y.abs().min(clip.w.mul(MAX_HALF_PIXELS / (SOURCE_VIEWPORT[1] / 2)));
    material.vertexNode = clip.add(vec4(positionGeometry.x.mul(dx), positionGeometry.y.mul(dy), 0, 0));
    const texel = texture(tex.map, uv()), col = attribute('avColour', 'vec4'), rgb = texel.rgb.mul(col.rgb).clamp(0, 1);
    material.fragmentNode = vec4(select(encodedOutput, rgb, toFrame(rgb)), texel.a.mul(tex.scale).mul(col.a).clamp(0, 1));   // GS 0x44
    const mesh = new T.InstancedMesh(geometry, material, capacity);
    mesh.count = 1; mesh.frustumCulled = false; mesh.visible = false;   // count 1: warmable
    mesh.renderOrder = drawOrder({priority: 7, mode: 4, fx: id}, SUBMIT.avalanche);
    group.add(mesh);
    const entry = {mesh, centre, colour}; meshes.set(id, entry); return entry;
  }
  for (const id of maps.keys()) build(id);
  registerEncodedEffect({
    object: group,
    setEncodedOutput: (v) => {
      encodedOutput.value = !!v;
    },
    populated: () => {
      for (const m of meshes.values()) if (m.mesh.visible && m.mesh.count > 0) return true;
      return false;
    }
  });
  const scratch = new Float32Array(capacity * TRAIL_FLOATS), counts = new Map(), state = {trails: 0, sprites: 0, unknownTexture: 0};
  return {
    group, state,
    // Once per rendered frame after the tick(s).
    update() {
      const trails = readAvalancheTrails(core); counts.clear(); state.trails = trails.length; state.sprites = 0;
      const byTexture = new Map();
      for (const t of trails) { const id = t.texture + (t.flipCount >= 2 ? Math.trunc(t.flipPhase) : 0); (byTexture.get(id) ?? byTexture.set(id, []).get(id)).push(t); }
      for (const [id, entry] of meshes) if (!byTexture.has(id)) { entry.mesh.visible = false; entry.mesh.count = 0; }
      for (const [id, list] of byTexture) {
        const entry = meshes.get(id) ?? build(id); if (!entry) { state.unknownTexture++; continue; }
        let all = 0; for (const t of list) all = trailSprites(t, scratch, all, capacity);
        // GS 0x44 with As = 0 leaves the destination as it is (and there is no Z write): those sprites are skipped, same pixels.
        // The retail trails' kernels carry the ambient colour in ColBase (tumbler +480, alpha 0), so most of their sprites are these.
        const C = entry.centre.array, Col = entry.colour.array; let n = 0;
        for (let i = 0; i < all; i++) {
          const o = i * TRAIL_FLOATS; if (!(scratch[o + 7] > 0)) continue;
          C[n * 4] = scratch[o] / 100; C[n * 4 + 1] = scratch[o + 2] / 100; C[n * 4 + 2] = -scratch[o + 1] / 100; C[n * 4 + 3] = scratch[o + 3] / 100;
          Col[n * 4] = scratch[o + 4] / 128; Col[n * 4 + 1] = scratch[o + 5] / 128; Col[n * 4 + 2] = scratch[o + 6] / 128; Col[n * 4 + 3] = scratch[o + 7] / 128; n++;
        }
        entry.mesh.count = n; entry.mesh.visible = n > 0; state.sprites += n;
        if (n) { entry.centre.needsUpdate = true; entry.colour.needsUpdate = true; }
      }
    },
    reset() { for (const m of meshes.values()) { m.mesh.visible = false; m.mesh.count = 0; } },
  };
}
