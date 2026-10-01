import * as T from 'three/webgpu';
import { attribute, texture, vec4, uniform, select } from 'three/tsl';
import { toFrame } from './frame-space.js';
import { registerEncodedEffect } from './snow-composite.js';
import { drawOrder, EFFECT, SUBMIT } from './ps2-draw-order.js';
export async function createBoostRenderer(origin) {
  const encodedOutput = uniform(false);
  const asset = await (await fetch('/assets/SNOW_FX/snow-fx.json')).json(),
    materials = new Map();
  await Promise.all(
    [57, 58, 59, 60, 61].map(async (id) => {
      const t = asset.textures.find((t) => t.id === id);
      if (!t) throw Error('Original boost texture missing');
      const bytes = new Uint8Array(await (await fetch('/assets/SNOW_FX/' + t.gs_alpha_file)).arrayBuffer());
      if (bytes.length !== t.width * t.height * 4) throw Error('Boost texture extent');
      const map = new T.DataTexture(bytes, t.width, t.height, T.RGBAFormat);
      map.wrapS = map.wrapT = T.RepeatWrapping;
      map.minFilter = map.magFilter = T.LinearFilter;
      map.colorSpace = T.NoColorSpace;
      map.needsUpdate = true;
      const texel = texture(map),
        colour = attribute('boostColour', 'vec4');
      const material = new T.MeshBasicNodeMaterial({
        transparent: true,
        depthWrite: false,
        depthTest: true,
        side: T.DoubleSide,
        forceSinglePass: true,
        fog: false,
        toneMapped: false,
        blending: T.AdditiveBlending
      });
      // Original MODULATE vertex channels are normalized by128, not255.
      // GS MODULATE saturates at 255; ALPHA 0x48 (Cs*As+Cd) adds in encoded space.
      const encodedColour = texel.rgb.mul(colour.rgb).clamp(0, 1);
      material.fragmentNode = vec4(
        select(encodedOutput, encodedColour, toFrame(encodedColour)),
        texel.a.mul(t.gs_alpha_scale).mul(colour.a).clamp(0, 1)
      );
      materials.set(id, material);
    })
  );
  const fxMaterials = await createRiderFxMaterials(encodedOutput);
  const group = new T.Group();
  group.userData.gameplayOnly = true;
  const meshes = [];
  for (let strip = 0; strip < 3; strip++) {
    const geometry = new T.BufferGeometry();
    for (const [name, size] of [
      ['position', 3],
      ['uv', 2],
      ['boostColour', 4]
    ])
      geometry.setAttribute(name, new T.BufferAttribute(new Float32Array(180 * size), size).setUsage(T.DynamicDrawUsage));
    geometry.setDrawRange(0, 0);
    const mesh = new T.Mesh(geometry, materials.get(57));
    mesh.frustumCulled = false;
    mesh.renderOrder = drawOrder(EFFECT.boost(57), SUBMIT.boost);
    group.add(mesh);
    meshes.push(mesh);
  }
  const riderFx = createRiderFxMeshes(group, fxMaterials, origin),
    updateFx = (core) => riderFx.update(core);
  let previous = -1;
  const api = {
    group,
    update(core) {
      updateFx(core);
      const info = new Float32Array(core.HEAPF32.buffer, core._boost_fx_info(), 12);
      if (info[9] === previous) return;
      previous = info[9];
      for (let strip = 0; strip < 3; strip++) {
        const mesh = meshes[strip],
          geometry = mesh.geometry,
          count = info[6 + strip];
        if (count > 180) throw Error('Original boost ribbon capacity exceeded');
        geometry.setDrawRange(0, count);
        mesh.material = materials.get(info[0]);
        if (!mesh.material) throw Error('Unmapped original boost palette');
        // 0x364240 by the palette texture (web/ps2-draw-order.js)
        mesh.renderOrder = drawOrder(EFFECT.boost(info[0]), SUBMIT.boost);
        const data = new Float32Array(core.HEAPF32.buffer, core._boost_fx_vertices(strip), count * 9),
          p = geometry.attributes.position.array,
          uv = geometry.attributes.uv.array,
          colour = geometry.attributes.boostColour.array;
        for (let i = 0; i < count; i++) {
          p[i * 3] = data[i * 9] / 100 - origin.x;
          p[i * 3 + 1] = data[i * 9 + 2] / 100 - origin.y;
          p[i * 3 + 2] = -data[i * 9 + 1] / 100 - origin.z;
          uv[i * 2] = data[i * 9 + 3];
          uv[i * 2 + 1] = data[i * 9 + 4];
          for (let k = 0; k < 4; k++) colour[i * 4 + k] = data[i * 9 + 5 + k];
        }
        for (const a of Object.values(geometry.attributes)) a.needsUpdate = true;
      }
    }
  };
  registerEncodedEffect({
    object: group,
    setEncodedOutput: (v) => {
      encodedOutput.value = !!v;
    },
    populated: () => meshes.some((m) => m.visible && m.geometry.drawRange.count > 0) || riderFx.populated()
  });
  (globalThis.ssxEffects ??= {}).boost = api;
  return api;
}

// Power-up aura (psmr, 2EB198) and air streamers (strm clamp / prbn repeat, 2EF950): web/boost_gameplay.inc rider_fx_*. Raw GS FX textures
// (/assets/FX, alpha 128 = 1.0); MODULATE with vertex channels / 128, ALPHA 0x48 (Cs*As + Cd). One material set per renderer, shared by its
// riders (the human's here; the computer riders' in opponent-fx.js: the PS2 runs 2EADD0 / 2EF6D0 for every rider in the rider manager's FX
// pass, frames local/ps2-capture/presentation/boostfx).
export async function createRiderFxMaterials(encodedOutput) {
  const fxAsset = await (await fetch('/assets/FX/fx.json')).json(),
    fxMaterials = new Map();
  const fxMaterial = async (tag, wrap) => {
    const t = fxAsset.textures[tag];
    if (!t) return null;
    const bytes = new Uint8Array(await (await fetch('/assets/FX/' + t.file)).arrayBuffer());
    if (bytes.length !== t.width * t.height * 4) throw Error('FX texture extent ' + tag);
    // the GS samples strm with its bright rows at T = 1 (PS2 setpieces-bra2 tick 418: the ribbon, T = scroll + k / count up
    // to ~1.8 clamped, is bright over its length; CRA3 6819: brightest at the old end), the export has them at T = 0.
    if (tag === 'strm') {
      const row = t.width * 4,
        flipped = new Uint8Array(bytes.length);
      for (let y = 0; y < t.height; y++) flipped.set(bytes.subarray(y * row, (y + 1) * row), (t.height - 1 - y) * row);
      bytes.set(flipped);
    }
    const map = new T.DataTexture(bytes, t.width, t.height, T.RGBAFormat);
    map.wrapS = map.wrapT = wrap;
    map.minFilter = map.magFilter = T.LinearFilter;
    map.colorSpace = T.NoColorSpace;
    map.needsUpdate = true;
    const texel = texture(map),
      colour = attribute('boostColour', 'vec4'),
      material = new T.MeshBasicNodeMaterial({
        transparent: true,
        depthWrite: false,
        depthTest: true,
        side: T.DoubleSide,
        forceSinglePass: true,
        fog: false,
        toneMapped: false,
        blending: T.AdditiveBlending
      });
    const encodedColour = texel.rgb.mul(colour.rgb).clamp(0, 1);
    material.fragmentNode = vec4(
      select(encodedOutput, encodedColour, toFrame(encodedColour)),
      texel.a
        .mul(255 / 128)
        .mul(colour.a)
        .clamp(0, 1)
    );
    return material;
  };
  fxMaterials.set('psmr', await fxMaterial('psmr', T.ClampToEdgeWrapping));
  fxMaterials.set(63, await fxMaterial('strm', T.ClampToEdgeWrapping));
  fxMaterials.set(61, await fxMaterial('prbn', T.RepeatWrapping));
  return fxMaterials;
}
// One rider's aura faces and streamers in `group`, filled from a rider core (or rider context view) per frame.
export function createRiderFxMeshes(group, fxMaterials, origin) {
  const fxMeshes = [],
    streamerTexture = (m) => {
      for (const [k, v] of fxMaterials) if (v === m) return k;
      return 63;
    };
  for (let strip = 0; strip < 6; strip++) {
    // 0..3 aura faces (priority 7, after the boost strips), 4..5 streamers (priority 8, after the snow)
    const cap = strip < 4 ? 9 : 72,
      geometry = new T.BufferGeometry();
    for (const [name, size] of [
      ['position', 3],
      ['uv', 2],
      ['boostColour', 4]
    ])
      geometry.setAttribute(name, new T.BufferAttribute(new Float32Array(cap * 3 * size), size).setUsage(T.DynamicDrawUsage));
    geometry.setDrawRange(0, 0);
    const material = strip < 4 ? fxMaterials.get('psmr') : fxMaterials.get(strip === 4 ? 63 : 61);
    // one streamer on each texture: the loading warm-up builds both pipelines
    if (!material) continue;
    const mesh = new T.Mesh(geometry, material);
    mesh.frustumCulled = false;
    mesh.renderOrder = strip < 4 ? drawOrder(EFFECT.aura, SUBMIT.aura) : drawOrder(EFFECT.streamer(strip === 4 ? 63 : 61), SUBMIT.streamer);
    mesh.userData.cap = cap * 3;
    group.add(mesh);
    fxMeshes[strip] = mesh;
  }
  let previousFx = -1;
  return {
    meshes: fxMeshes,
    update(core) {
      if (!core._rider_fx_info) return;
      const info = new Float32Array(core.HEAPF32.buffer, core._rider_fx_info(), 8);
      if (info[0] === previousFx) return;
      previousFx = info[0];
      for (let strip = 0; strip < 6; strip++) {
        const mesh = fxMeshes[strip];
        if (!mesh) continue;
        const geometry = mesh.geometry,
          count = Math.min(info[2 + strip], mesh.userData.cap);
        geometry.setDrawRange(0, count);
        if (strip >= 4) {
          mesh.material = fxMaterials.get(info[1]) ?? fxMaterials.get(63);
          mesh.renderOrder = drawOrder(EFFECT.streamer(streamerTexture(mesh.material)), SUBMIT.streamer);
        }
        if (!count) continue;
        const data = new Float32Array(core.HEAPF32.buffer, core._rider_fx_vertices(strip), count * 9),
          p = geometry.attributes.position.array,
          uv = geometry.attributes.uv.array,
          colour = geometry.attributes.boostColour.array;
        for (let i = 0; i < count; i++) {
          p[i * 3] = data[i * 9] / 100 - origin.x;
          p[i * 3 + 1] = data[i * 9 + 2] / 100 - origin.y;
          p[i * 3 + 2] = -data[i * 9 + 1] / 100 - origin.z;
          uv[i * 2] = data[i * 9 + 3];
          uv[i * 2 + 1] = data[i * 9 + 4];
          for (let k = 0; k < 4; k++) colour[i * 4 + k] = data[i * 9 + 5 + k];
        }
        for (const a of Object.values(geometry.attributes)) a.needsUpdate = true;
      }
    },
    clear() {
      previousFx = -1;
      for (const m of fxMeshes) if (m) m.geometry.setDrawRange(0, 0);
    },
    populated() {
      return fxMeshes.some((m) => m && m.visible && m.geometry.drawRange.count > 0);
    }
  };
}
