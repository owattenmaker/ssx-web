// Rival locator beam (rider FX component at RFX+0xB00: init 0x2E3930, update 0x2E39D8, draw 0x2E3AF8; docs/backcountry.md).
// Drawn for a computer rider in the rival challenges only: game mode 4/5 and the streamer course (+0x1BC) 14..16
// (Happiness, Ruthless, The Throne), single player (race +0x7C < 2), and not while the rider's progress is inhibited
// (+0xAC4). An 8-vertex strip (GS draw 0x238 of the renderer) along the camera's up axis from the rider's world bone
// rider+0x8A8 (bone 5): base alpha 0, +130 cm and +20130 cm at the component colour, +20260 cm alpha 0; half width 85 cm
// along the camera's right axis. Colour = (r, g, b) x 255 and alpha x 128 from +0x2C/+0x30/+0x34 and +0x28. The pulse of
// 0x2E39D8 (phase += 2*pi/60, lerp of +0x08 and +0x18 by 0.5 + 0.5 sin) runs only for the human (vtable +0x40), so the
// rival keeps its initial colour +0x28 = +0x08 = (a 0.5, r 1, g 0, b 0): a red beam at half alpha (PS2 savestates
// happiness-ready / glide / glide620: the rival's phase +4 stays 0). Texture: the renderer's 'beam' (renderer+0xFFC,
// 64x64 PSMCT32, tools/export_rival_fx.py: white, alpha a triangle across S peaking at the centre); S runs across the
// strip (0 on the -right edge, 1 on the +right edge). Modulated by the vertex colour (x2 in the GS 128 = 1.0 scale) and
// blended additively: red over the sky reads pink, as in the PS2 frames (bc-race-idle.tick401.png).
import { texture as tslTexture, attribute, vec4, select, uniform } from 'three/tsl';
import { toFrame, frameTextureSpace, linearOutput, linearTextureSpace } from './frame-space.js';
import { registerEncodedEffect } from './snow-composite.js';
import { pv } from './pv-flags.js';
import { drawOrder, EFFECT, SUBMIT } from './ps2-draw-order.js';
const LEVELS = [0, 130, 20130, 20260], ALPHA = [0, 1, 1, 0], HALF_WIDTH = 85;
export const BEAM_BONE = 5;
export const BEAM_COLOUR = { a: 0.5, r: 1, g: 0, b: 0 };

export function beamVertices(base, right, up, colour = BEAM_COLOUR) {   // source cm; right/up unit vectors
  const out = [];
  for (let k = 0; k < 4; k++) for (const s of [-1, 1]) {
    const p = [0, 1, 2].map((i) => base[i] + up[i] * LEVELS[k] + s * right[i] * HALF_WIDTH);
    out.push({ p, rgba: [colour.r, colour.g, colour.b, ALPHA[k] * colour.a] });
  }
  return out;
}

// pv beamEncoded (docs/visual-parity.md 41.9): the beam is a priority-7 draw (0x2E3AF8: word2 priority 7, strips rank 3, 'beam'),
// so the PS2 draws it after the fog composite, unfogged, in the encoded pass: GS MODULATE (texel x vertex, vertex rgb x 2 in the
// 128 = 1.0 scale, clamped) and ALPHA 0x48 (Cd + Cs x As) on the bytes.
function byteBeamMaterial(T, texture) {
  const encodedOutput = uniform(false), map = new T.Texture();
  const material = new T.MeshBasicNodeMaterial({ transparent: true, depthWrite: false, side: T.DoubleSide, fog: false, toneMapped: false, blending: T.AdditiveBlending });
  const texel = tslTexture(map), colour = attribute('color', 'vec4'), rgb = texel.rgb.mul(colour.rgb).clamp(0, 1);
  material.fragmentNode = vec4(select(encodedOutput, rgb, toFrame(rgb)), texel.a.mul(colour.a).clamp(0, 1));
  if (texture) new T.TextureLoader().load(texture, (t) => { t.colorSpace = T.NoColorSpace; texel.value = t; material.needsUpdate = true; });
  return { material, encodedOutput };
}
export function createRivalBeam({ T, scene, origin, renderOrder = 650, texture = null }) {
  const geometry = new T.BufferGeometry();
  const position = new T.BufferAttribute(new Float32Array(8 * 3), 3), color = new T.BufferAttribute(new Float32Array(8 * 4), 4);
  const uv = new T.BufferAttribute(new Float32Array([0, 0, 1, 0, 0, 0.33, 1, 0.33, 0, 0.66, 1, 0.66, 0, 1, 1, 1]), 2);
  geometry.setAttribute('position', position); geometry.setAttribute('color', color); geometry.setAttribute('uv', uv);
  geometry.setIndex([0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5, 4, 5, 6, 6, 5, 7]);
  const encoded = pv('beamEncoded');
  let material, encodedOutput = null;
  if (encoded) ({ material, encodedOutput } = byteBeamMaterial(T, texture));
  else {
    material = new T.MeshBasicMaterial({ vertexColors: true, transparent: true, depthWrite: false, side: T.DoubleSide, fog: false, blending: T.AdditiveBlending });
    if (texture) new T.TextureLoader().load(texture, (t) => { t.colorSpace = frameTextureSpace; material.map = t; material.needsUpdate = true; });
  }
  const mesh = new T.Mesh(geometry, material); mesh.frustumCulled = false; mesh.visible = false;
  mesh.renderOrder = encoded && pv('effectOrder') ? drawOrder(EFFECT.beam, SUBMIT.beam) : renderOrder;
  scene.add(mesh);
  if (encoded) registerEncodedEffect({ object: mesh, setEncodedOutput: (v) => { encodedOutput.value = !!v; }, populated: () => mesh.visible });
  const right = new T.Vector3(), up = new T.Vector3();
  return {
    mesh,
    // bone: the rival's world bone BEAM_BONE in source cm (core _world_pose_bones), camera: the three.js camera.
    update(camera, bone, visible = true) {
      mesh.visible = !!(visible && bone && bone.every(Number.isFinite));
      if (!mesh.visible) return;
      camera.updateMatrixWorld();
      right.setFromMatrixColumn(camera.matrixWorld, 0); up.setFromMatrixColumn(camera.matrixWorld, 1);
      // three (x, y, z) = source (x, z, -y) / 100
      const r = [right.x, -right.z, right.y], u = [up.x, -up.z, up.y];
      beamVertices(bone, r, u).forEach((v, i) => {
        position.setXYZ(i, v.p[0] * 0.01 - origin.x, v.p[2] * 0.01 - origin.y, -v.p[1] * 0.01 - origin.z);
        color.setXYZW(i, Math.min(1, v.rgba[0] * 2), Math.min(1, v.rgba[1] * 2), Math.min(1, v.rgba[2] * 2), v.rgba[3]);
      });
      position.needsUpdate = true; color.needsUpdate = true;
    },
    dispose() { scene.remove(mesh); geometry.dispose(); material.dispose(); },
  };
}

// Rider relationship icon '!' (rider FX component at RFX+0xAF0: update 0x2D4C08 per game tick, draw 0x2D5048; texture 'exlm'
// renderer+0xFAC, tools/export_rival_fx.py). For every computer rider and every human viewer: the level is 155B50 (the rider's
// relationship record about the viewer; 3 for the peak rival, so Mac in the Happiness rival challenge), hidden while the
// rider's progress is inhibited (+0xAC4) or during a replay. Colour (a, r, g, b) by level: >= 4 (1, 1, 0, 0) red, 3
// (1, 1, 0.5, 0) orange, 2 (1, 1, 1, 0) yellow, else white; alpha = visibility x 0.8 (+4). Visibility ramps by 0.1 toward
// 1 at level >= 2, else by 1/15 toward 0. Pulses (x, y scale): a 41-tick pop when the level changes, a 31-tick loop at
// level >= 4 while no pop runs, a 24-tick pop when the level rises (reset while either other one runs).
// Draw: bottom edge at bone rider+0x8A8 (5) + camera up x 35 cm, top edge 18 x (sy + 0.003 z) cm above it, half width
// 9 x (sx + 0.003 z) along the camera right, z = view depth in cm, drawn for 0 < z < 2000, alpha x (2000 - z) x 0.002 beyond 1500. Alpha blended (unlike
// the additive beam: the GS packet keeps the default blend).
const POP41_Y = [1.0, 0.785, 0.513, 0.269, 0.142, 0.217, 0.534, 1.025, 1.617, 2.236, 2.811, 3.268, 3.444, 3.212, 2.585, 1.766, 1.021, 0.621, 0.666, 0.976, 1.398, 1.777, 1.958, 1.877, 1.638, 1.335, 1.067, 0.93, 1.017, 1.258, 1.502, 1.597, 1.492, 1.281, 1.028, 0.802, 0.667, 0.682, 0.8, 0.936, 1.0];
const POP41_X = [1.0, 0.746, 0.432, 0.237, 0.342, 1.463, 3.487, 5.347, 5.977, 5.506, 4.838, 4.082, 3.343, 2.727, 2.247, 1.848, 1.533, 1.308, 1.176, 1.121, 1.112, 1.119, 1.113, 1.096, 1.088, 1.085, 1.083, 1.081, 1.077, 1.075, 1.073, 1.071, 1.068, 1.063, 1.055, 1.044, 1.032, 1.02, 1.01, 1.003, 1.0];
const LOOP31_Y = [1.0, 0.862, 0.702, 0.542, 0.404, 0.312, 0.289, 0.394, 0.619, 0.883, 1.104, 1.204, 0.953, 0.511, 0.378, 0.761, 1.39, 2.048, 2.519, 2.679, 2.636, 2.519, 2.384, 2.211, 2.018, 1.821, 1.64, 1.475, 1.315, 1.158, 1.0];
const LOOP31_X = [1.0, 1.022, 1.021, 1.02, 1.041, 1.108, 1.243, 1.663, 2.322, 2.845, 3.053, 2.895, 1.917, 1.236, 1.657, 2.391, 3.213, 3.901, 4.231, 3.734, 2.656, 1.846, 1.531, 1.341, 1.233, 1.163, 1.087, 1.024, 1.007, 1.008, 1.0];
const RISE24_X = [1.0, 0.52, 0.206, 0.162, 0.147, 0.145, 0.149, 0.156, 0.162, 0.162, 0.155, 0.145, 0.134, 0.127, 0.128, 0.14, 0.167, 0.212, 0.291, 0.408, 0.55, 0.704, 0.858, 1.0];
const RISE24_Y = [1.0, 0.6, 0.292, 0.176, 0.319, 0.913, 1.657, 2.037, 1.956, 1.708, 1.37, 1.016, 0.723, 0.565, 0.521, 0.513, 0.536, 0.583, 0.647, 0.722, 0.801, 0.878, 0.947, 1.0];
const LEVEL_COLOUR = (level) => (level >= 4 ? [1, 1, 0, 0] : level === 3 ? [1, 1, 0.5, 0] : level === 2 ? [1, 1, 1, 0] : [1, 1, 1, 1]);
const f = Math.fround;

// GS modulate (texel x vertex / 128, in the 8-bit display space; vertex = c x 255) for the mid-grey 'exlm' body (0x80): the
// displayed colour is min(1, c). three.js decodes the sRGB texture to linear (0.5 -> 0.214) and encodes the product again,
// so the vertex colour carries linear(min(1, c)) and the material colour 1 / linear(0.5).
const srgbToLinear = (v) => (v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4);
const TINT_SCALE = 1 / srgbToLinear(0.5), gsTint = (c) => srgbToLinear(Math.min(1, c));   // x TINT_SCALE in material.color (vertex colours stay <= 1)
export function iconEntry() { return { vis: 0, level: 0, i20: 0, i24: 0, i28: 0, colour: [1, 1, 1, 1], sx: 0, sy: 0 }; }
// 0x2D4C08 for one (rider, viewer) entry; hidden: +0xAC4 or replay (entry +0 = 0 and nothing else changes).
export function iconStep(e, level, hidden = false) {
  if (hidden) { e.vis = 0; return e; }
  const prev = e.level; e.level = level; e.colour = LEVEL_COLOUR(level).slice();
  const v = e.vis;
  if (level >= 2) e.vis = f(1.1) < v ? f(v - f(0.1)) : v < f(0.9) ? f(v + f(0.1)) : 1;
  else e.vis = f(0.06666667) < v ? f(v - f(0.06666667)) : v < f(-0.06666667) ? f(v + f(0.06666667)) : 0;
  e.colour[0] = f(e.vis * f(0.8));
  if (e.i20 > 0 || prev !== level) e.i20 = (e.i20 + 1) % 41;
  if (e.i24 > 0 || (level >= 4 && e.i20 === 0)) e.i24 = (e.i24 + 1) % 31;
  if (e.i28 > 0 || prev < level) { if (!(e.i28 > 0)) e.i28 = 0; e.i28 = (e.i28 + 1) % 24; }
  if (e.i20 > 0 || e.i24 > 0) e.i28 = 0;
  e.sx = f(f(f(e.vis * POP41_X[e.i20]) * LOOP31_X[e.i24]) * RISE24_X[e.i28]);
  e.sy = f(f(f(e.vis * POP41_Y[e.i20]) * LOOP31_Y[e.i24]) * RISE24_Y[e.i28]);
  return e;
}

// pv rivalIcon (docs/presentation.md "Rival marker"): the GS draw in its own byte space. 2D5048 sends the colour as
// r, g, b x 255 and a x 128 (0x437F0000 / 0x43000000), MODULATE with the exlm texel (body 0x80, outline 0x00, alpha 0x80):
// Cs = T x V / 128 (orange 255,127,0 at level 3), As = Ta x Va / 128 = 0.8, blended (Cs - Cd) x As + Cd on the encoded
// frame (the encoded post-rider pass, web/snow-composite.js). three's linear-space blend of the old material showed a
// pale, pinkish, see-through triangle (Happiness rival frames: PS2 body ~(215,130,45) vs (227,163,155)).
function byteIconMaterial(T, texture) {
  const encodedOutput = uniform(false), map = new T.Texture();
  const material = new T.MeshBasicNodeMaterial({ transparent: true, depthWrite: false, side: T.DoubleSide, fog: false, toneMapped: false });
  const texel = tslTexture(map), colour = attribute('color', 'vec4');
  const bytes = texel.rgb.mul(colour.rgb).mul(255 / 128).clamp(0, 1);
  material.fragmentNode = vec4(select(encodedOutput, bytes, toFrame(bytes)), texel.a.mul(colour.a).clamp(0, 1));
  if (texture) new T.TextureLoader().load(texture, (t) => { t.colorSpace = T.NoColorSpace; texel.value = t; material.needsUpdate = true; });
  return { material, encodedOutput };
}
export function createRiderIcons({ T, scene, origin, count, texture = null, renderOrder = 651 }) {
  const bytesMode = pv('rivalIcon');
  let material, encodedOutput = null;
  if (bytesMode) ({ material, encodedOutput } = byteIconMaterial(T, texture));
  else {
    // the old linear-light icon (pv rivalIcon off): texel x vertex colour x TINT_SCALE on linear light, written through frame-space.js
    material = new T.MeshBasicNodeMaterial({ vertexColors: true, transparent: true, depthWrite: false, side: T.DoubleSide, fog: false });
    material.color.setRGB(TINT_SCALE, TINT_SCALE, TINT_SCALE, T.LinearSRGBColorSpace); material.outputNode = linearOutput();
    if (texture) new T.TextureLoader().load(texture, (t) => { t.colorSpace = linearTextureSpace; material.map = t; material.needsUpdate = true; });
  }
  const group = new T.Group(); scene.add(group);
  const tint = bytesMode ? (c) => Math.min(1, c) : gsTint;
  const items = Array.from({ length: count }, () => {
    const g = new T.BufferGeometry(), p = new T.BufferAttribute(new Float32Array(12), 3), c = new T.BufferAttribute(new Float32Array(16), 4);
    g.setAttribute('position', p); g.setAttribute('color', c); g.setAttribute('uv', new T.BufferAttribute(new Float32Array([0, 1, 1, 1, 0, 0, 1, 0]), 2)); g.setIndex([0, 1, 2, 2, 1, 3]);
    const mesh = new T.Mesh(g, material); mesh.frustumCulled = false; mesh.visible = false; group.add(mesh);
    mesh.renderOrder = bytesMode && pv('effectOrder') ? drawOrder(EFFECT.icon, SUBMIT.icon) : renderOrder; // pv effectOrder: 0x364240 (exlm, rank 3)
    return { entry: iconEntry(), mesh, p, c };
  });
  if (bytesMode) registerEncodedEffect({ object: group, setEncodedOutput: (v) => { encodedOutput.value = !!v; }, populated: () => items.some((it) => it.mesh.visible) });
  const right = new T.Vector3(), up = new T.Vector3(), eye = new T.Vector3(), fwd = new T.Vector3();
  return {
    reset() { for (const it of items) { it.entry = iconEntry(); it.mesh.visible = false; } },
    tick(levels, hidden = []) { items.forEach((it, k) => iconStep(it.entry, levels[k] ?? 0, !!hidden[k])); },
    update(camera, bones, visible = true) {
      camera.updateMatrixWorld(); right.setFromMatrixColumn(camera.matrixWorld, 0); up.setFromMatrixColumn(camera.matrixWorld, 1);
      fwd.setFromMatrixColumn(camera.matrixWorld, 2).negate(); eye.setFromMatrixPosition(camera.matrixWorld);
      items.forEach((it, k) => {
        const b = bones[k], e = it.entry;
        if (!visible || !b || !b.every(Number.isFinite) || e.vis === 0) { it.mesh.visible = false; return; }
        const P = new T.Vector3(b[0] * 0.01 - origin.x, b[2] * 0.01 - origin.y, -b[1] * 0.01 - origin.z);
        const z = P.clone().sub(eye).dot(fwd) * 100;   // view depth, cm
        if (!(z > 0 && z < 2000)) { it.mesh.visible = false; return; }
        const grow = f(z * 0.003), hw = 9 * (e.sx + grow) * 0.01, hh = 18 * (e.sy + grow) * 0.01, fade = z > 1500 ? (2000 - z) * 0.002 : 1;
        const C = P.clone().addScaledVector(up, 0.35);
        [[-1, 1], [1, 1], [-1, 0], [1, 0]].forEach(([sx, sy], i) => {
          const V = C.clone().addScaledVector(right, sx * hw).addScaledVector(up, sy * hh); it.p.setXYZ(i, V.x, V.y, V.z);
          it.c.setXYZW(i, tint(e.colour[1]), tint(e.colour[2]), tint(e.colour[3]), Math.min(1, e.colour[0] * fade));
        });
        it.p.needsUpdate = true; it.c.needsUpdate = true; it.mesh.visible = true;
      });
    },
  };
}
