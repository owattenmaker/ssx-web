// Set-piece particle systems of the course stage scripts (rocket smoke, spin-twin sparks, dragon fire, fire
// gushes, start/mid fire pops, fireworks, snow crumbs, snow wind, waterfall/river spray, Metro-City and
// Junction emitters, pickup sparkles). The core simulates them (web/stage_world.inc on
// engine/set_piece_particles.hpp) and exports each drawable effect's kernel / birth ring every tick
// (stage_world_particles); the sprites are evaluated here with the structure of the original VU1 program
// (web/set-piece-particle-sprites.js is the bit-exact model; the fast evaluators below use native floats and
// are checked against it in test-set-piece-particles.mjs) and drawn as camera-facing quads:
//   burst  (Particle, 3708C0 -> 380518): ST (0,0) at centre-half; trail (DynamicParticle, 371380 -> 3807A0):
//   ST (0,0) at centre+half (texture turned 180 degrees); half extent capped at 64 source pixels (MINI 64);
//   TFX MODULATE (colour/128), BlendMode 0 = GS 0x48 Cd + Cs*As>>7, 1 = GS 0x44 (Cs-Cd)*As>>7 + Cd, 2 = 0x42
//   Cd - Cs*As>>7; depth tested, no depth write; layer priority 7 (after the fog composite, before the glare
//   and ScreenTint): drawn in the encoded composite with the snow (web/snow-composite.js).
import * as T from 'three/webgpu';
import {attribute, texture, vec2, vec3, vec4, float, select, uniform, positionGeometry, modelViewMatrix, cameraProjectionMatrix, uv} from 'three/tsl';import {toFrame} from './frame-space.js';
import {registerEncodedEffect} from './snow-composite.js';
import {pv} from './pv-flags.js';import {drawOrder, EFFECT, SUBMIT} from './ps2-draw-order.js';

import {SPRITE_FLOATS, MAX_HALF_PIXELS, SOURCE_VIEWPORT, burstSpritesFast, trailSpritesFast, readParticleEffects, particleCombinations} from './set-piece-particle-eval.js';
export {burstSpritesFast, trailSpritesFast, readParticleEffects, particleCombinations};

async function loadTextures(fetchJson, fetchBytes) {
  const meta = await fetchJson('/assets/PARTICLES/textures/textures.json'), maps = new Map();
  await Promise.all(meta.textures.map(async (t) => {
    const bytes = new Uint8Array(await fetchBytes('/assets/PARTICLES/textures/' + t.file));
    if (bytes.length !== t.width * t.height * 4) throw Error('Set-piece particle texture extent ' + t.tag);
    const map = new T.DataTexture(bytes, t.width, t.height, T.RGBAFormat);
    map.minFilter = map.magFilter = T.LinearFilter; map.wrapS = map.wrapT = T.ClampToEdgeWrapping; map.colorSpace = T.NoColorSpace; map.needsUpdate = true;
    maps.set(t.id, {map, scale: t.gs_alpha_scale, tag: t.tag});
  }));
  return maps;
}

// origin: the scene origin (main.js: the course start.json position, metres) the world geometry is relative to.
export async function createSetPieceParticles({core, particlesDoc, origin = [0, 0, 0], fetchJson = (p) => fetch(p).then((r) => r.json()), fetchBytes = (p) => fetch(p).then((r) => r.arrayBuffer())}) {
  const maps = await loadTextures(fetchJson, fetchBytes);
  const encodedOutput = uniform(false), group = new T.Group(); group.userData.gameplayOnly = true; group.name = 'set-piece particles';
  group.position.set(-origin[0], -origin[1], -origin[2]); // sprite centres are course coordinates (source cm / 100, (x, z, -y))
  const meshes = new Map(); let order = 0;
  // One instanced quad batch per combination; the vertex stage builds the camera-facing sprite (centre, capped half extent).
  function build({texture: id, blend, kind}, capacity = 1024) {
    const tex = maps.get(id); if (!tex) throw Error(`Set-piece particle texture ${id} was not exported (tools/export_set_piece_particle_textures.py)`);
    const geometry = new T.PlaneGeometry(2, 2);
    for (let v = 0; v < geometry.attributes.uv.count; v++) {
      const x = geometry.attributes.uv.getX(v), y = geometry.attributes.uv.getY(v);
      if (kind === 1) geometry.attributes.uv.setXY(v, 1 - x, y); else geometry.attributes.uv.setXY(v, x, 1 - y);
    }
    const centre = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage);
    const colour = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage);
    geometry.setAttribute('spCentre', centre); geometry.setAttribute('spColour', colour);
    const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: true, side: T.DoubleSide, forceSinglePass: true, fog: false, toneMapped: false});
    // Vertex: view-space centre, half extent min(r, 64 px * depth / (|P| * viewport / 2)) per axis (MINI 64).
    // In clip space: a view-space half extent h moves clip x by (P (h,0,0,0)).x; 64 source pixels are 64/256 (x) and
    // 64/224 (y) of clip w.
    const c = attribute('spCentre', 'vec4'), clip = cameraProjectionMatrix.mul(modelViewMatrix.mul(vec4(c.xyz, 1)));
    const dx = cameraProjectionMatrix.mul(vec4(c.w, 0, 0, 0)).x.abs().min(clip.w.mul(MAX_HALF_PIXELS / (SOURCE_VIEWPORT[0] / 2)));
    const dy = cameraProjectionMatrix.mul(vec4(0, c.w, 0, 0)).y.abs().min(clip.w.mul(MAX_HALF_PIXELS / (SOURCE_VIEWPORT[1] / 2)));
    material.vertexNode = clip.add(vec4(positionGeometry.x.mul(dx), positionGeometry.y.mul(dy), 0, 0));
    const texel = texture(tex.map, uv()), col = attribute('spColour', 'vec4');
    const rgb = texel.rgb.mul(col.rgb).clamp(0, 1), As = texel.a.mul(tex.scale).mul(col.a); // As / 128 (may exceed 1: star)
    if (blend === 1) { // 0x44: Cd + (Cs - Cd) * As
      material.fragmentNode = vec4(select(encodedOutput, rgb, toFrame(rgb)), As.clamp(0, 1));
    } else { // 0x48 Cd + Cs*As (0x42 Cd - Cs*As): premultiplied, saturating like the GS COLCLAMP
      const pre = rgb.mul(As).clamp(0, 1);
      material.fragmentNode = vec4(select(encodedOutput, pre, toFrame(pre)), 1);
      material.blending = T.CustomBlending; material.blendSrc = T.OneFactor; material.blendDst = T.OneFactor;
      material.blendEquation = blend === 2 ? T.ReverseSubtractEquation : T.AddEquation;
      material.blendSrcAlpha = T.ZeroFactor; material.blendDstAlpha = T.OneFactor; material.blendEquationAlpha = T.AddEquation;
    }
    const mesh = new T.InstancedMesh(geometry, material, capacity);
    mesh.count = 1; mesh.frustumCulled = false; mesh.renderOrder = 685 + (order++ % 5); mesh.visible = false; // count 1 keeps the pipeline warmable
    if (pv('effectOrder')) mesh.renderOrder = drawOrder(EFFECT.setPieceParticle(id), SUBMIT.setPiece); // 0x364240: rank 1, the texture + the inherited 'spec'
    mesh.userData.setPieceParticles = {texture: id, blend, kind};
    group.add(mesh);
    return {mesh, centre, colour, capacity, count: 0};
  }
  for (const combo of particleCombinations(particlesDoc)) meshes.set(`${combo.texture}|${combo.blend}|${combo.kind}`, build(combo));
  registerEncodedEffect({object: group, setEncodedOutput: (v) => { encodedOutput.value = !!v; }, populated: () => { for (const m of meshes.values()) if (m.mesh.visible && m.mesh.count > 0) return true; return false; }}); // main.js warmupRender shows every mesh (count 1, zero size): the encoded pass pipelines build during loading
  let scratch = new Float32Array(4096 * SPRITE_FLOATS);
  const state = {effects: 0, sprites: 0, dropped: 0, unknown: 0};
  return {
    group, state, meshes,
    // Once per rendered frame after the tick(s): read the core's effects and rebuild the batches.
    update() {
      const effects = readParticleEffects(core);
      for (const m of meshes.values()) m.count = 0;
      const buckets = new Map();
      for (const e of effects) { const key = `${e.textureId}|${e.blend}|${e.kind}`; if (!buckets.has(key)) buckets.set(key, []); buckets.get(key).push(e); }
      state.effects = effects.length; state.sprites = 0; state.unknown = 0;
      for (const [key, list] of buckets) {
        let target = meshes.get(key);
        if (!target) { const [texture, blend, kind] = key.split('|').map(Number); if (!maps.has(texture)) { state.unknown++; continue; } target = build({texture, blend, kind}); meshes.set(key, target); }
        let n = 0;
        for (const e of list) {
          const limit = scratch.length / SPRITE_FLOATS;
          n = e.kind === 0 ? burstSpritesFast(e.K, e.F, scratch, n, limit) : trailSpritesFast(e.K, e.F, e.capacity, e.cursor, e.ringA, e.ringB, e.ringBits, scratch, n, limit);
          if (n >= limit) { state.dropped++; const grown = new Float32Array(scratch.length * 2); grown.set(scratch); scratch = grown; }
        }
        if (n > target.capacity) { group.remove(target.mesh); target.mesh.geometry.dispose(); const [texture, blend, kind] = key.split('|').map(Number); const bigger = build({texture, blend, kind}, Math.max(n, target.capacity * 2)); meshes.set(key, bigger); target = bigger; }
        const cen = target.centre.array, col = target.colour.array;
        for (let i = 0; i < n; i++) {
          const o = i * SPRITE_FLOATS;
          cen[i * 4] = scratch[o] / 100; cen[i * 4 + 1] = scratch[o + 2] / 100; cen[i * 4 + 2] = -scratch[o + 1] / 100; cen[i * 4 + 3] = scratch[o + 3] / 100;
          col[i * 4] = scratch[o + 4] / 128; col[i * 4 + 1] = scratch[o + 5] / 128; col[i * 4 + 2] = scratch[o + 6] / 128; col[i * 4 + 3] = scratch[o + 7] / 128;
        }
        target.count = n; state.sprites += n;
        if (n) { target.centre.clearUpdateRanges(); target.centre.addUpdateRange(0, n * 4); target.centre.needsUpdate = true; target.colour.clearUpdateRanges(); target.colour.addUpdateRange(0, n * 4); target.colour.needsUpdate = true; }
      }
      for (const m of meshes.values()) { m.mesh.count = Math.max(m.count, 1); m.mesh.visible = m.count > 0; }
    },
    dispose() { for (const m of meshes.values()) { m.mesh.geometry.dispose(); m.mesh.material.dispose(); } group.removeFromParent(); },
  };
}
