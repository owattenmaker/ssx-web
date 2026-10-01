// CrowdMan2d (stage builtin88; object *(gp-0x6F0), update 0x2294C8, draw 0x229BA8; tools/export_crowd.py):
// * crowd billboards: mode 0 swaps the crowd2d / crowdpod material slots for the shared record 0x536690 whose texture
//   the animator 0x2DBAC0 advances every 3 game ticks through CRWD.SSH an00..an15; on the three courses those slots
//   are exactly world texture 9-161, so its image is replaced by the current frame (no material or pipeline change);
// * camera flashes: the core runs 0x229530 on the shared visual stream in the world visual pass (web/stage_world.inc
//   stage_crowd_tick: 128 slot countdowns from the ready savestate, -= 10*|cheer| + 4 per tick, on expiry c1, c2 and the
//   re-arm 300 + r % 300) and exports the 40-entry flash ring (life 3 ticks); a flash is at centre + axis1*c1 + axis2*c2
//   of its area (tools/export_crowd.py). Sprite: FX 65 'flsh', 100 cm half extent, screen aligned, RGBA 128, GS 0x48
//   additive, depth tested, no Z write, priority 4 (world pass, before the fog composite). The cheer level (audio crowd
//   record +0x18C) comes from set_stage_crowd_cheer (0 = idle without it).
import * as T from 'three/webgpu';
import {attribute, texture, vec4, positionGeometry, modelViewMatrix, cameraProjectionMatrix, uv} from 'three/tsl';import {toFrame} from './frame-space.js';
import {decodePngTexels} from './png-texels.js';
import {setUpdateRange} from './heap-views.js';

export const CROWD_FRAME_TICKS = 3, CROWD_FRAMES = 16, FLASH_RING = 40;
// 0x2DBAC0: frame index for a game tick (the animator steps once every 3 ticks, 16-frame loop).
export const crowdFrame = (tick) => Math.floor(tick / CROWD_FRAME_TICKS) % CROWD_FRAMES;

// Core flash ring (stage_world_crowd): [128 slots, n, n resources, 40 x (resource, c1 bits, c2 bits, life)].
export function readCrowd(core) {
  const U = new Uint32Array(core.HEAPU8.buffer), F = new Float32Array(core.HEAPU8.buffer); let at = core._stage_world_crowd() >> 2;
  const slots = Array.from(U.subarray(at, at + 128)); at += 128; const n = U[at++]; const animated = Array.from(U.subarray(at, at + n)); at += n;
  const ring = []; for (let k = 0; k < FLASH_RING; k++, at += 4) ring.push({resource: U[at], c1: F[at + 1], c2: F[at + 2], life: U[at + 3] | 0});
  return {slots, animated, ring};
}
export async function createCrowd2d({core, group, root, origin = [0, 0, 0], load = (p) => fetch(p).then((r) => (r.ok ? r.json() : null))}) {
  const data = await load(root + 'CROWD/crowd.json').catch(() => null); if (!data) return null;
  const textures = group.userData.worldTextures || {}, targets = data.textures.map((k) => textures[k]).filter(Boolean);
  // Frames swap in as the targets' images: exact decoded texels for the DataTextures of web/texture-archive.js (WebKit's
  // ImageBitmap premultiplies alpha < 255 even with premultiplyAlpha 'none'), an ImageBitmap for an image texture.
  const decoded = targets.length > 0 && targets.every((t) => t.isDataTexture);
  const bitmap = async (url) => {
    const blob = await (await fetch(url)).blob();
    return decoded
      ? decodePngTexels(await blob.arrayBuffer())
      : createImageBitmap(blob, { imageOrientation: 'none', premultiplyAlpha: 'none', colorSpaceConversion: 'none' });
  };
  const frames = await Promise.all(data.frames.map((f) => bitmap('/assets/FX/crowd/' + f.file)));
  const statics = targets.map((t) => t.image);
  // Flash sprites (instanced, world pass).
  const fx = await load('/assets/FX/fx.json').catch(() => null), meta = fx?.textures?.flsh;
  let mesh = null, centres = null;
  if (meta) {
    const bytes = new Uint8Array(await (await fetch('/assets/FX/' + meta.file)).arrayBuffer());
    const map = new T.DataTexture(bytes, meta.width, meta.height, T.RGBAFormat);
    map.minFilter = map.magFilter = T.LinearFilter;
    map.wrapS = map.wrapT = T.ClampToEdgeWrapping;
    map.colorSpace = T.NoColorSpace;
    map.needsUpdate = true;
    const geometry = new T.PlaneGeometry(2, 2);
    centres = new T.InstancedBufferAttribute(new Float32Array(FLASH_RING * 4), 4).setUsage(T.DynamicDrawUsage);
    geometry.setAttribute('flashCentre', centres);
    const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: true, side: T.DoubleSide, fog: false, toneMapped: false});
    const c = attribute('flashCentre', 'vec4'), view = modelViewMatrix.mul(vec4(c.xyz, 1));
    material.vertexNode = cameraProjectionMatrix.mul(vec4(view.x.add(positionGeometry.x.mul(c.w)), view.y.add(positionGeometry.y.mul(c.w)), view.z, 1));
    const t = texture(map, uv()), As = t.a.mul(255 / 128); // colour 128,128,128,128: Cs = texel, As = texel alpha
    material.colorNode = vec4(toFrame(t.rgb.mul(As).clamp(0, 1)), 1);
    material.blending = T.CustomBlending; material.blendSrc = T.OneFactor; material.blendDst = T.OneFactor; material.blendEquation = T.AddEquation;
    material.blendSrcAlpha = T.ZeroFactor; material.blendDstAlpha = T.OneFactor;
    mesh = new T.InstancedMesh(geometry, material, FLASH_RING); mesh.count = 1; mesh.visible = false; mesh.frustumCulled = false; mesh.userData.gameplayOnly = true; mesh.name = 'crowd camera flashes';
    const holder = new T.Group(); holder.position.set(-origin[0], -origin[1], -origin[2]); holder.add(mesh); group.add(holder);
  }
  const areas = new Map(data.flashes.map((a) => [a.resource, a])); let frame = -1;
  const state = {frame: -1, flashes: 0, slots: 0, textures: targets.length};
  const setFrame = (i) => { if (i === frame) return; frame = i; for (const t of targets) { t.image = i < 0 ? statics[targets.indexOf(t)] : frames[i]; t.needsUpdate = true; } };
  return {
    state,
    // After the frame's ticks: tick = the race's game tick (set_piece_info()[1]).
    update(tick) {
      if (!core._stage_world_crowd) return;
      // Same layout and values as readCrowd, read in place (no per-frame slot array / ring objects).
      const U = new Uint32Array(core.HEAPU8.buffer), F = new Float32Array(core.HEAPU8.buffer); let at = core._stage_world_crowd() >> 2;
      let used = 0; for (let k = 0; k < 128; k++) if (U[at + k] !== 0xffffffff) used++;
      at += 128; at += 1 + U[at]; // slots, then n + the n animated resources: the flash ring follows
      setFrame(crowdFrame(tick)); state.frame = frame; state.slots = used;
      if (mesh) {
        let n = 0; const C = centres.array;
        for (let k = 0; k < FLASH_RING; k++, at += 4) {
          const a = areas.get(U[at]); if ((U[at + 3] | 0) <= 0 || !a) continue; const c1 = F[at + 1], c2 = F[at + 2];
          const p0 = a.centre[0] + a.axis1[0] * c1 + a.axis2[0] * c2, p1 = a.centre[1] + a.axis1[1] * c1 + a.axis2[1] * c2, p2 = a.centre[2] + a.axis1[2] * c1 + a.axis2[2] * c2;
          C[n * 4] = p0 / 100; C[n * 4 + 1] = p2 / 100; C[n * 4 + 2] = -p1 / 100; C[n * 4 + 3] = 1; n++;
        }
        state.flashes = n; mesh.count = Math.max(n, 1); mesh.visible = n > 0; if (n) setUpdateRange(centres, 0, n * 4);
      }
    },
    reset() { setFrame(-1); },
  };
}
