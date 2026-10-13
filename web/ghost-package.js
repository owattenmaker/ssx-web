// A rider package for the ghost riders (web/ghost-riders.js): loaded once and shared by every ghost riding it. The same files and
// texture batches web/opponent-riders.js draws (world.json, rider.json, vertices.bin, indices.bin; textures through
// web/texture-archive.js), with the skin as per-vertex bones and weights (rider.json source_skin: up to four [bone, integer percent]).
import { packageTexture } from './texture-archive.js';
import { frameTextureSpace } from './frame-space.js';

export function createRiderPackageLoader({ T, load, loader, root = '/assets/' }) {
  const cache = new Map();
  const textures = [];
  async function loadPackage(pkg) {
    const base = root + pkg + '/';
    const [manifest, rig, vb, ib] = await Promise.all([load(base + 'world.json'), load(base + 'rider.json'), load(base + 'vertices.bin', 'buffer'), load(base + 'indices.bin', 'buffer')]);
    if (!rig.source_skin || !rig.source_bind_matrix_words) throw new Error(`${pkg}: rider.json lacks the source skin`);
    const vertices = new Float32Array(vb);
    const indices = new Uint32Array(ib);
    const count = vertices.length / 10;
    if (rig.source_skin.length !== count) throw new Error(`${pkg}: vertex and skin counts differ`);
    const position = new Float32Array(count * 3);
    const bones = new Float32Array(count * 4);
    const weights = new Float32Array(count * 4);
    const uvs = new Float32Array(count * 2);
    for (let i = 0; i < count; i++) {
      // source cm, Z up (web/rider-skinning.js sourcePosition)
      position[i * 3] = vertices[i * 10] * 100;
      position[i * 3 + 1] = -vertices[i * 10 + 2] * 100;
      position[i * 3 + 2] = vertices[i * 10 + 1] * 100;
      uvs[i * 2] = vertices[i * 10 + 6];
      uvs[i * 2 + 1] = vertices[i * 10 + 7];
      rig.source_skin[i].slice(0, 4).forEach(([bone, percent], k) => {
        bones[i * 4 + k] = bone;
        weights[i * 4 + k] = percent * 0.01;
      });
    }
    const shared = {
      ghostPosition: new T.BufferAttribute(position, 3),
      position: new T.BufferAttribute(position, 3),
      ghostBones: new T.BufferAttribute(bones, 4),
      ghostWeights: new T.BufferAttribute(weights, 4),
      uv: new T.BufferAttribute(uvs, 2)
    };
    const maps = {};
    await Promise.all(Object.entries(manifest.textures).map(async ([key, t]) => {
      const texture = await packageTexture(loader, base, t);
      texture.flipY = false;
      texture.wrapS = texture.wrapT = T.RepeatWrapping;
      texture.colorSpace = frameTextureSpace;
      // the texel domain (ps2: 128 = 1.0), as web/opponent-riders.js keeps it for web/rider-material.js
      texture.userData.texelDomain = t.texel_domain;
      maps[key] = texture;
      textures.push(texture);
    }));
    // one part per texture: its batches' index ranges merged (web/opponent-riders.js does the same)
    const byTexture = new Map();
    for (const b of manifest.batches) {
      if (!byTexture.has(b.texture)) byTexture.set(b.texture, []);
      byTexture.get(b.texture).push(indices.subarray(b.first_index, b.first_index + b.index_count));
    }
    const parts = [];
    for (const [key, ranges] of byTexture) {
      const map = maps['9-' + key];
      if (!map) continue;
      const merged = new Uint32Array(ranges.reduce((n, r) => n + r.length, 0));
      let at = 0;
      for (const r of ranges) {
        merged.set(r, at);
        at += r.length;
      }
      const geometry = new T.BufferGeometry();
      // `position` is required by three for the draw range (its own attribute object: one object under two names breaks the WebGPU
      // vertex layout); the vertex node reads ghostPosition
      geometry.setAttribute('position', shared.position);
      for (const [name, attribute] of Object.entries(shared)) if (name !== 'position') geometry.setAttribute(name, attribute);
      geometry.setIndex(new T.BufferAttribute(merged, 1));
      parts.push({ geometry, map });
    }
    // the rig as the ghosts need it (bone parents and offsets, bind matrices); the parsed skin rows are not kept
    const keep = { bones: rig.bones.map((b) => ({ name: b.name, parent_index: b.parent_index, source_translation_cm: b.source_translation_cm })),
      source_bind_matrix_words: rig.source_bind_matrix_words };
    return { rig: keep, parts };
  }
  const api = (pkg) => {
    if (!cache.has(pkg)) cache.set(pkg, loadPackage(pkg));
    return cache.get(pkg);
  };
  api.dispose = () => {
    for (const t of textures) t.dispose();
    cache.clear();
  };
  return api;
}
