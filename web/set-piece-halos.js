// HaloModifier glow sprites of the stage-script pickups (builtin97 0x3057C0; web/stage_world.inc stage_world_halos):
// collecta / trickboost (FX 42 whha), BHP1 pointa (FX 39 gcha, spinning -9 deg/tick) and pointc (FX 41 rdha, two
// counter-rotating copies). Draw 0x3462A0 -> 0x2D1D10: centre P = row 3 of 0x34FED8(instance, node) (the magnet's
// matrix when the pickup flies, a LiveComp node world matrix for node >= 0); the whole sprite is skipped when P is
// outside the view volume (0x37DBE8 outcode of the centre only); angle 0 -> screen-aligned sprite 0x377CF0, else the
// rotated quad 0x3781A0 (angle * 0x3C8EFA36, corners P -/+ dx -/+ dy as light-glow.js glowQuad), a second one at -angle
// for textures 1 and 4. Material 0x501420 + edits: GS ALPHA 0x48 (Cd + Cs*As>>7), TFX MODULATE, ZTST GEQUAL at the
// sprite depth (the centre's), no Z write, priority 8: after the fog composite, not fogged, ScreenTinted -> drawn in
// the encoded composite (web/snow-composite.js) like the set-piece particles.
import * as T from 'three/webgpu';
import {attribute, texture, vec4, float, select, uniform, positionGeometry, modelViewMatrix, cameraProjectionMatrix, uv} from 'three/tsl';import {toFrame} from './frame-space.js';
import {registerEncodedEffect} from './snow-composite.js';
import {setUpdateRange} from './heap-views.js';
import {drawOrder, EFFECT, SUBMIT} from './ps2-draw-order.js';

export const HALO_FX_BASE = 37; // FX texture table 0x4891B0 index = 37 + key1
export const HALO_TEXTURES = {37: 'blha', 38: 'bsha', 39: 'gcha', 40: 'orha', 41: 'rdha', 42: 'whha'};
const DEG = new Float32Array(new Uint32Array([0x3c8efa36]).buffer)[0]; // gp-0x3F98: pi/180, one ulp high
const HALO_FLOATS = 12;

// Core export -> sprite list: [{resource, key, node, rgba bytes, sizeCm, angleRad, P (source cm) | null, copies}].
export function readHalos(core) {
  if (!core._stage_world_halos) return [];
  const F = new Float32Array(core.HEAPU8.buffer); let at = core._stage_world_halos() >> 2; const n = F[at++], out = [];
  for (let k = 0; k < n; k++, at += HALO_FLOATS) {
    const key = F[at + 1], angle = F[at + 8], P = Number.isNaN(F[at + 9]) ? null : [F[at + 9], F[at + 10], F[at + 11]];
    const bytes = [F[at + 3], F[at + 4], F[at + 5], F[at + 6]].map((v) => Math.trunc(Math.fround(v * 128))); // vertex R,G,B,A
    out.push({
      resource: F[at],
      key,
      node: F[at + 2],
      rgba: bytes,
      sizeCm: F[at + 7],
      angleDeg: angle,
      angle: angle === 0 ? 0 : Math.fround(angle * DEG),
      P,
      copies: angle !== 0 && (key === 1 || key === 4) ? 2 : 1
    });
  }
  return out;
}

export async function createSetPieceHalos({core, origin = [0, 0, 0], fetchJson = (p) => fetch(p).then((r) => r.json()), fetchBytes = (p) => fetch(p).then((r) => r.arrayBuffer()), capacity = 64}) {
  const meta = await fetchJson('/assets/FX/fx.json'), maps = new Map();
  await Promise.all(Object.entries(HALO_TEXTURES).map(async ([id, tag]) => {
    const t = meta.textures[tag]; if (!t) return;
    const bytes = new Uint8Array(await fetchBytes('/assets/FX/' + t.file));
    if (bytes.length !== t.width * t.height * 4) throw Error('Halo texture extent ' + tag);
    const map = new T.DataTexture(bytes, t.width, t.height, T.RGBAFormat);
    map.minFilter = map.magFilter = T.LinearFilter; map.wrapS = map.wrapT = T.ClampToEdgeWrapping; map.colorSpace = T.NoColorSpace; map.needsUpdate = true;
    maps.set(Number(id), map);
  }));
  const encodedOutput = uniform(false), group = new T.Group(); group.name = 'set-piece halos'; group.userData.gameplayOnly = true;
  group.position.set(-origin[0], -origin[1], -origin[2]);
  const meshes = new Map();
  for (const [id, map] of maps) {
    // PlaneGeometry(2, 2): corner x, y in {-1, 1}; ST s = (x + 1) / 2, t = (1 - y) / 2 (t = 0 at the top row).
    const geometry = new T.PlaneGeometry(2, 2);
    for (let v = 0; v < geometry.attributes.uv.count; v++) geometry.attributes.uv.setXY(v, (geometry.attributes.position.getX(v) + 1) / 2, (1 - geometry.attributes.position.getY(v)) / 2);
    const centre = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage); // xyz native, w half size (m)
    const spin = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage);   // cos, sin, t sign (-1 rotated 3781A0, +1 screen-aligned 377CF0)
    const colour = new T.InstancedBufferAttribute(new Float32Array(capacity * 4), 4).setUsage(T.DynamicDrawUsage); // vertex bytes / 128
    geometry.setAttribute('haloCentre', centre); geometry.setAttribute('haloSpin', spin); geometry.setAttribute('haloColour', colour);
    const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: true, side: T.DoubleSide, forceSinglePass: true, fog: false, toneMapped: false});
    const c = attribute('haloCentre', 'vec4'), sp = attribute('haloSpin', 'vec4'), view = modelViewMatrix.mul(vec4(c.xyz, 1));
    // View-space corner: x * size * (cos R + sin U) + k * y * size * (-sin R + cos U) (R, U = view axes, k = t sign).
    const x = positionGeometry.x.mul(c.w), y = positionGeometry.y.mul(c.w).mul(sp.z);
    const corner = vec4(view.x.add(x.mul(sp.x)).sub(y.mul(sp.y)), view.y.add(x.mul(sp.y)).add(y.mul(sp.x)), view.z, 1);
    // 0x37DBE8: the centre alone decides (outside the view volume: the whole sprite is dropped).
    const cc = cameraProjectionMatrix.mul(view), inside = cc.x.abs().lessThanEqual(cc.w).and(cc.y.abs().lessThanEqual(cc.w)).and(cc.w.greaterThan(0)).and(cc.z.lessThanEqual(cc.w));
    material.vertexNode = select(inside, cameraProjectionMatrix.mul(corner), vec4(0, 0, 2, 1));
    const texel = texture(map, uv()), col = attribute('haloColour', 'vec4');
    const rgb = texel.rgb.mul(col.rgb).clamp(0, 1), As = texel.a.mul(255 / 128).mul(col.a);  // MODULATE (raw GS alpha 128 = 1.0)
    const pre = rgb.mul(As).clamp(0, 1);                                                       // 0x48: Cd + Cs*As>>7
    material.fragmentNode = vec4(select(encodedOutput, pre, toFrame(pre)), 1);
    material.blending = T.CustomBlending; material.blendSrc = T.OneFactor; material.blendDst = T.OneFactor; material.blendEquation = T.AddEquation;
    material.blendSrcAlpha = T.ZeroFactor; material.blendDstAlpha = T.OneFactor; material.blendEquationAlpha = T.AddEquation;
    const mesh = new T.InstancedMesh(geometry, material, capacity); mesh.count = 1; mesh.frustumCulled = false; mesh.visible = false; // count 1: warmable
    mesh.renderOrder = drawOrder(EFFECT.halo(id), SUBMIT.halo); // 0x364240: priority 8, after every priority-7 effect
    mesh.userData.halo = {key: id - HALO_FX_BASE};
    group.add(mesh); meshes.set(id - HALO_FX_BASE, {mesh, centre, spin, colour, count: 0});
  }
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
  const state = {halos: 0, sprites: 0, unresolved: 0}, scratchP = [0, 0, 0];
  return {
    group, meshes, state,
    // Once per rendered frame after the tick(s). nodePosition(resource, node): source-cm P of a JS-animated LiveComp node.
    // Reads the core's halo records directly (same values as readHalos) without per-halo objects/arrays.
    update(nodePosition = () => null) {
      for (const m of meshes.values()) m.count = 0;
      state.halos = 0; state.sprites = 0; state.unresolved = 0;
      if (core._stage_world_halos) {
        const F = new Float32Array(core.HEAPU8.buffer); let at = core._stage_world_halos() >> 2; const n = F[at++]; state.halos = n;
        for (let k = 0; k < n; k++, at += HALO_FLOATS) {
          const key = F[at + 1], target = meshes.get(key); if (!target) continue;
          let px, py, pz;
          if (Number.isNaN(F[at + 9])) { const P = nodePosition(F[at], F[at + 2], scratchP); if (!P) { state.unresolved++; continue; } px = P[0]; py = P[1]; pz = P[2]; }
          else { px = F[at + 9]; py = F[at + 10]; pz = F[at + 11]; }
          const angleDeg = F[at + 8], angle = angleDeg === 0 ? 0 : Math.fround(angleDeg * DEG), copies = angleDeg !== 0 && (key === 1 || key === 4) ? 2 : 1, size = F[at + 7];
          // vertex R,G,B,A bytes
          const r = Math.trunc(Math.fround(F[at + 3] * 128)),
            g = Math.trunc(Math.fround(F[at + 4] * 128)),
            b = Math.trunc(Math.fround(F[at + 5] * 128)),
            al = Math.trunc(Math.fround(F[at + 6] * 128));
          for (let copy = 0; copy < copies; copy++) {
            if (target.count >= target.centre.count) break;
            const i = target.count++, a = copy ? -angle : angle, C = target.centre.array, Sp = target.spin.array, Co = target.colour.array, o = i * 4;
            C[o] = px / 100; C[o + 1] = pz / 100; C[o + 2] = -py / 100; C[o + 3] = size / 100;
            Sp[o] = Math.cos(a); Sp[o + 1] = Math.sin(a); Sp[o + 2] = angle === 0 ? 1 : -1; Sp[o + 3] = 0;
            Co[o] = r / 128; Co[o + 1] = g / 128; Co[o + 2] = b / 128; Co[o + 3] = al / 128;
            state.sprites++;
          }
        }
      }
      for (const m of meshes.values()) {
        if (m.count) { const as = m.attributes ??= [m.centre, m.spin, m.colour]; for (let i = 0; i < as.length; i++) setUpdateRange(as[i], 0, m.count * 4); }
        m.mesh.count = Math.max(m.count, 1); m.mesh.visible = m.count > 0;
      }
    },
    dispose() { for (const m of meshes.values()) { m.mesh.geometry.dispose(); m.mesh.material.dispose(); } for (const t of maps.values()) t.dispose(); group.removeFromParent(); },
  };
}
