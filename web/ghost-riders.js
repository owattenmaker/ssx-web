// Ghost riders (pv ghostRiders; docs/big-multiplayer.md section 4): the other players of a time-attack session, drawn translucent with
// their own package's model and textures. A port-only look (the PS2 has no ghosts). Cheap by design:
//   * one geometry and one set of textures per package, shared by every ghost riding it;
//   * one instanced draw per package material: each instance's bone skin matrices (pose x body scale x the rig's source bind, the
//     matrices web/net/pose-codec.js builds for exact riders, in plain float arithmetic here) sit in one RGBA32F row of the package's
//     bone texture, and the vertex node blends a vertex's (up to four) bones from rider.json source_skin;
//   * no rider lighting, shadow, effects or morphs; screen-door transparency (a per-pixel hash against the opacity) keeps depth writes
//     and needs no sorting: an ordered 4 x 4 Bayer pattern, so the ghost reads as a steady see-through rider, not as noise. The texels
//     are the riders' unlit ones (web/rider-material.js: a ps2-domain texel x 2), tinted, in the frame's colour space.
// update(ghosts) each frame: ghosts = [{pkg, positions Float64Array(B x 3), quats Float64Array(B x 4), scale [3]}] (web/server/
// ghost-packet.mjs bonePositions / decodePose), at most `cap` drawn (the caller orders them nearest first).
import { Fn, attribute, textureLoad, ivec2, int, vec4, vec3, float, floor, mod, cameraProjectionMatrix, cameraViewMatrix, instanceIndex, screenCoordinate,
  texture, uv, If, Discard, uniform } from 'three/tsl';
import { createRiderPackageLoader } from './ghost-package.js';
import { toFrame } from './frame-space.js';

export const GHOST_OPACITY = 0.5;
export const GHOST_CAP = 32;

// pose (q, p, scale) x bind, column major (engine/pose_matrix.hpp originalPoseMatrices / originalSkinPoseMatrix, plain floats)
function skinMatrix(px, py, pz, qx, qy, qz, qw, s, bind, out, at) {
  const r00 = 1 - 2 * (qy * qy + qz * qz), r10 = 2 * (qx * qy + qz * qw), r20 = 2 * (qz * qx - qy * qw);
  const r01 = 2 * (qx * qy - qz * qw), r11 = 1 - 2 * (qz * qz + qx * qx), r21 = 2 * (qy * qz + qx * qw);
  const r02 = 2 * (qz * qx + qy * qw), r12 = 2 * (qy * qz - qx * qw), r22 = 1 - 2 * (qx * qx + qy * qy);
  // the scaled pose matrix's columns: rotation columns x scale, translation p
  const a = [r00 * s[0], r10 * s[0], r20 * s[0], 0, r01 * s[1], r11 * s[1], r21 * s[1], 0, r02 * s[2], r12 * s[2], r22 * s[2], 0, px, py, pz, 1];
  for (let col = 0; col < 4; col++) {
    for (let row = 0; row < 4; row++) {
      let v = 0;
      for (let k = 0; k < 4; k++) v += a[k * 4 + row] * bind[col * 4 + k];
      out[at + col * 4 + row] = v;
    }
  }
}

export function createGhostRiders({ T, scene, load, loader, origin, cap = GHOST_CAP, opacity = GHOST_OPACITY, tint = [0.75, 0.9, 1.0] }) {
  const group = new T.Group();
  group.name = 'ghost-riders';
  // shown only in gameplay (main.js: visible = playing every frame, hidden under the pause and menus)
  group.userData.gameplayOnly = true;
  scene.add(group);
  const packages = new Map();
  const opacityNode = uniform(opacity);
  const originNode = uniform(new T.Vector3().copy(origin));
  const packageLoader = createRiderPackageLoader({ T, load, loader });
  const stats = { packages: 0, drawn: 0, updateMs: 0 };

  async function build(pkg) {
    const p = await packageLoader(pkg);
    const bones = p.rig.bones.length;
    const binds = p.rig.source_bind_matrix_words.map((row) => new Float32Array(new Uint32Array(row).buffer));
    const data = new Float32Array(bones * 4 * 4 * cap);
    const boneTexture = new T.DataTexture(data, bones * 4, cap, T.RGBAFormat, T.FloatType);
    boneTexture.minFilter = boneTexture.magFilter = T.NearestFilter;
    boneTexture.generateMipmaps = false;
    boneTexture.colorSpace = T.NoColorSpace;
    boneTexture.needsUpdate = true;
    // the vertex: up to four bones of rider.json source_skin, their skin matrices from this instance's row
    const vertexNode = Fn(() => {
      const row = int(instanceIndex);
      const point = vec4(attribute('ghostPosition', 'vec3'), 1);
      const ids = attribute('ghostBones', 'vec4');
      const weights = attribute('ghostWeights', 'vec4');
      const bone = (id) => {
        const base = int(id).mul(4);
        const m = Array.from({ length: 4 }, (_, col) => textureLoad(boneTexture, ivec2(base.add(col), row)));
        return m[0].mul(point.x).add(m[1].mul(point.y)).add(m[2].mul(point.z)).add(m[3]);
      };
      const world = bone(ids.x).mul(weights.x).add(bone(ids.y).mul(weights.y)).add(bone(ids.z).mul(weights.z)).add(bone(ids.w).mul(weights.w));
      // source cm, Z up -> the scene (metres, Y up, minus the scene origin): as web/rider-skinning.js
      const relative = vec4(world.x.mul(0.01).sub(originNode.x), world.z.mul(0.01).sub(originNode.y), world.y.mul(-0.01).sub(originNode.z), 1);
      return cameraProjectionMatrix.mul(cameraViewMatrix).mul(relative);
    })();
    const meshes = [];
    for (const part of p.parts) {
      const material = new T.MeshBasicNodeMaterial({ side: T.DoubleSide });
      material.vertexNode = vertexNode;
      material.fog = false;
      // screen-door transparency: the pixel's 4 x 4 Bayer threshold against the opacity, and the texture's own alpha test
      const domain = part.map.userData?.entryDomain ?? part.map.userData?.texelDomain;
      const gain = domain === 'ps2' || domain === 'xbox' ? 2 : 1;
      material.outputNode = Fn(() => {
        const c = texture(part.map, uv());
        const x = floor(screenCoordinate.x);
        const y = floor(screenCoordinate.y);
        // 2 x 2 Bayer [[0, 2], [3, 1]] = (2x + 3y) mod 4; the 4 x 4 one is 4 x the fine cell + the coarse cell
        const fine = mod(mod(x, 2).mul(2).add(mod(y, 2).mul(3)), 4);
        const coarse = mod(mod(floor(x.div(2)), 2).mul(2).add(mod(floor(y.div(2)), 2).mul(3)), 4);
        const threshold = fine.mul(4).add(coarse).add(0.5).div(16);
        If(threshold.greaterThan(opacityNode), () => {
          Discard();
        });
        If(c.a.lessThan(float(0.35)), () => {
          Discard();
        });
        return vec4(toFrame(c.rgb.mul(gain).min(1).mul(vec3(...tint))), 1);
      })();
      const mesh = new T.InstancedMesh(part.geometry, material, cap);
      mesh.count = 0;
      mesh.frustumCulled = false;
      mesh.renderOrder = 600;
      group.add(mesh);
      meshes.push(mesh);
    }
    stats.packages++;
    return { pkg, rig: p.rig, bones, binds, data, boneTexture, meshes, count: 0 };
  }
  function entry(pkg) {
    let e = packages.get(pkg);
    if (!e) {
      e = { ready: null, value: null };
      e.ready = build(pkg).then((v) => {
        e.value = v;
        return v;
      }, (err) => {
        console.warn('Ghost rider package unavailable', pkg, err);
        return null;
      });
      packages.set(pkg, e);
    }
    return e;
  }

  return {
    group,
    stats,
    // the package's rig (for bonePositions), loading it first
    async rig(pkg) {
      return (await entry(pkg).ready)?.rig ?? null;
    },
    update(ghosts) {
      const t0 = performance.now();
      // the scene origin moves with the course (main.js loadCourse: origin.fromArray(spawn.position))
      originNode.value.copy(origin);
      for (const e of packages.values()) if (e.value) e.value.count = 0;
      let drawn = 0;
      for (const g of ghosts) {
        if (drawn >= cap) break;
        const p = entry(g.pkg).value;
        if (!p || p.count >= cap || g.quats.length / 4 !== p.bones) continue;
        const at = p.count * p.bones * 16;
        for (let b = 0; b < p.bones; b++) {
          skinMatrix(g.positions[b * 3], g.positions[b * 3 + 1], g.positions[b * 3 + 2], g.quats[b * 4], g.quats[b * 4 + 1], g.quats[b * 4 + 2], g.quats[b * 4 + 3],
            g.scale, p.binds[b], p.data, at + b * 16);
        }
        p.count++;
        drawn++;
      }
      for (const e of packages.values()) {
        const p = e.value;
        if (!p) continue;
        for (const m of p.meshes) m.count = p.count;
        if (p.count) p.boneTexture.needsUpdate = true;
      }
      stats.drawn = drawn;
      stats.updateMs = performance.now() - t0;
    },
    set opacity(v) {
      opacityNode.value = v;
    },
    // QA: the scene origin the vertex node subtracts and the first drawn instance's root bone skin matrix of each package
    debug() {
      const out = { origin: [originNode.value.x, originNode.value.y, originNode.value.z], packages: [] };
      for (const e of packages.values()) if (e.value) out.packages.push({ pkg: e.value.pkg, count: e.value.count, m0: Array.from(e.value.data.slice(0, 16)) });
      return out;
    },
    dispose() {
      scene.remove(group);
      for (const e of packages.values()) {
        const p = e.value;
        if (!p) continue;
        p.boneTexture.dispose();
        for (const m of p.meshes) m.material.dispose();
      }
      packageLoader.dispose();
      packages.clear();
    }
  };
}
