// The original front-end rider preview (Select Character, Setup Character, Rider Details): its own model, not the
// race rider. docs/characters.md "Front-end preview"; packages from tools/export_fe_preview.py (RIDER_<ID>/fe/).
//
// Model: the FE assembly of the preview slot (*(*(gp-0x848)+0x7C) + 0xB0): the outfit with the cinematic (NIS)
// head, eyes and hands. The NIS head and both hands are morph-target parts: the weight of morph i is channel
// morph_ids[i] of the FE clip's stream for the part's file (5 head, 8 left hand, 9 right hand); the eyes are
// bones of file 6. The preview is unscaled (geometry +0x140 = 1) and uses the live FE bind matrices.
//
// Lighting (0x19EE88): a cleared bank + the rider's IRR.DAT record (0x389590, weight 1, modulation 1 = the record)
// and 0x389CB8(bank, hips world position, 1.0) with the FE camera's view matrix; no environment bank, no local
// lights. The core's own port of that rim path (shade_rider_lighting with an empty local-light selection,
// web/rider_lighting_bridge.cpp) gives the coefficients; the per-vertex evaluation and GS HIGHLIGHT2 are the race
// rider's (web/rider-lighting-nodes.js).
import { Vector4, NoColorSpace } from 'three';
import { uniformArray, uniform, varying, texture, vec3, vec4, select, normalLocal, modelNormalMatrix } from 'three/tsl';
import { toFrame } from './frame-space.js';
import { riderIrradianceNode, riderHighlight2Node } from './rider-lighting-nodes.js';
import { riderDrawState } from './rider-material.js';   // the PS2 draw state per material
import { originalRiderBoneInverses } from './rider-bind.js';
import { wardrobeFile } from './wardrobe.js';   // Equip Gear outfits: generated FE packages (web/wardrobe.js)
import { packageTextureBlob, texelTexture, bcTexture, textureDecodeConfig } from './texture-archive.js';   // rider texture archives (docs/asset-formats.md)
import { createGuardedWorker, workerUrl } from './worker-guard.js';   // build handshake + main-thread fallback (docs/workers.md)
import { prepareFrontEndPreview, decodeTextureBlob } from './fe-preview-prepare.js';
import { quality, onQualityChange } from './quality.js';   // rider texture set (retexture)
// the next animation frame (a timer when frames stop: a hidden tab)
const nextFrame = () => new Promise((r) => { let done = false; const go = () => { if (!done) { done = true; r(); } }; globalThis.requestAnimationFrame?.(go); setTimeout(go, 50); });
const FE_PREVIEW_WORKER = workerUrl((Worker) => new Worker(new URL('./fe-preview-worker.js', import.meta.url), { type: 'module' }));

const DEG = Math.PI / 180;
const SKIN_BONES = 32;   // >= every FE assembly (24..31 bones)
// Morphing parts (NIS head, hands, board flex) are padded to a multiple of MORPH_VERTS vertices: three writes the morph
// texture width (= the vertex count) into the vertex shader, so every rider's head and hands had its own shader and
// pipeline (8 per intro cast; docs/firefox-load.md). The added vertices are zero and no index refers to them.
const MORPH_VERTS = 256;
// Further to one of two shapes (MORPH_TIERS: vertices x morphs; the vertex count and the morph count are both
// constants of the shader): the added morphs are zero and their weight stays 0 (pose(): a weight past the part's own morphs is 0),
// and three's morph loop skips a zero weight (MorphNode: If(influence != 0)), so the drawn vertices are the same.
const MORPH_TIERS = [[256, 27], [768, 36]];
export function padMorphPart(d, tiers = true) {
  if (!d.morphs?.length) return d;
  const count = d.interleaved.length / 10;
  let padded = Math.ceil(count / MORPH_VERTS) * MORPH_VERTS, morphs = d.morphs.length;
  if (tiers) { const t = MORPH_TIERS.find(([v, m]) => padded <= v && morphs <= m); if (t) [padded, morphs] = t; }
  if (padded === count && morphs === d.morphs.length) return d;
  const grow = (a, n) => { if (a.length === n) return a; const b = new a.constructor(n); b.set(a); return b; };
  const padMorphs = d.morphs.map((m) => grow(m, padded * 3)); while (padMorphs.length < morphs) padMorphs.push(new Float32Array(padded * 3));
  return { ...d, interleaved: grow(d.interleaved, padded * 10), skinIndex: grow(d.skinIndex, padded * 4), skinWeight: grow(d.skinWeight, padded * 4), morphs: padMorphs };
}

// AFB rotation channels -> quaternion (web/main.js afb(): the original compressed Euler form, Z-up to Y-up).
export function afb(T, a, b, c) {
  const sin = [], cos = [];
  for (const [i, n] of [a, b, c].entries()) {
    const q = ((n % 4) + 4) % 4,
      r = q - 2 * Math.floor(q * 0.5);
    sin[i] = r * (2 - r) * (Math.floor(q) & 2 ? -1 : 1);
    cos[i] = Math.sqrt(Math.max(0, 1 - sin[i] * sin[i])) * ((Math.floor(q) + 1) & 2 ? -1 : 1);
  }
  const raw = [cos[0], sin[0] * sin[1] * cos[2], sin[0] * cos[1], sin[0] * sin[1] * sin[2]];
  return new T.Quaternion(raw[0], raw[2], -raw[1], raw[3]).normalize();
}

// One clip sample: frames i/j and the blend between them (30 Hz authored; loop wraps over frame_count-1).
export function clipSample(clip, t, loop) {
  const n = clip.frame_count, f = loop ? (t * clip.fps) % (n - 1) : Math.min(t * clip.fps, n - 1);
  return { clip, i: Math.floor(f), j: Math.min(n - 1, Math.floor(f) + 1), blend: f - Math.floor(f) };
}
export function channelValue(samples, s, stream, x) {
  const at = stream.offset >> 2;
  return samples[at + s.i * stream.channels + x] * (1 - s.blend) + samples[at + s.j * stream.channels + x] * s.blend;
}
// Morph weights of one part for a sample (null without a stream for the part's file: rest shape).
export function morphWeights(part, samples, s) {
  const stream = part.file == null ? null : s.clip.streams?.[String(part.file)];
  if (!stream || !part.morphs?.length) return null;
  return part.morphs.map((m) => (m.channel < stream.channels ? channelValue(samples, s, stream, m.channel) : 0));
}

// FE preview package location for a roster entry (Sam has his own race package copied into RIDER_SAM/fe).
export function previewRoot(entry) { return entry?.fe_root || (entry?.package ? `/assets/${entry.package}/fe/` : null); }   // fe_root: a worn outfit's package

async function fetchOk(url, type) {
  const v = wardrobeFile(url, type === 'json' ? 'json' : 'buffer');
  if (v !== undefined) return v;
  const r = await fetch(url);
  if (!r.ok) throw Error(`${url}: ${r.status}`);
  return type === 'json' ? r.json() : r.arrayBuffer();
}

// Package preparation runs in web/fe-preview-worker.js (fetch, parse, skin/index/morph arrays, texture decode): a rider
// switch on Select Character must not stall the main thread. One shared worker; generated Equip Gear packages
// (web/wardrobe.js, main-thread memory) are posted to it.
let worker = null;
function prepareInWorker(root) {
  if (typeof Worker === 'undefined') return null;
  worker ??= createGuardedWorker({ name: 'fe-preview', url: FE_PREVIEW_WORKER, local: () => prepareFrontEndPreview });
  const files = {};
  for (const name of ['world.json', 'rider.json', 'vertices.bin', 'indices.bin', 'morphs.bin']) {
    const v = wardrobeFile(root + name, name.endsWith('.json') ? 'json' : 'buffer');
    if (v !== undefined) files[name] = v;
  }
  // world.json and the texture PNGs are read here (web/downloads.js: shared with the race package, counted by the
  // loading screens; the rider's texture archive is fetched once): the worker gets the PNG Blobs and decodes them.
  return (async () => {
    files['world.json'] ??= await fetchOk(root + 'world.json', 'json');
    const blobs = await textureBlobs(root, files['world.json']);
    return worker.request({ root, files, blobs, bc: textureDecodeConfig().bc });   // bc: Xbox HD entries as BC blocks (web/texture-archive.js)
  })();
}
// The PNG Blob of every texture of a package: a generated package's own file (web/wardrobe.js), an archive entry
// (the rider's texture archive, web/texture-archive.js) or an older export's PNG file.
async function textureBlobs(root, world) {
  const out = {};
  await Promise.all(Object.entries(world.textures || {}).map(async ([k, t]) => {
    const local = t.path !== undefined ? wardrobeFile(root + t.path, 'buffer') : undefined;
    out[k] = local !== undefined ? new Blob([local]) : await packageTextureBlob(root, t);
  }));
  return out;
}
// Main-thread fallback (no Worker, e.g. node tests): the same prepared shape.
async function prepareHere(root) {
  const [world, rig, vb, ib, mb] = await Promise.all([fetchOk(root + 'world.json', 'json'), fetchOk(root + 'rider.json', 'json'),
    fetchOk(root + 'vertices.bin'), fetchOk(root + 'indices.bin'), fetchOk(root + 'morphs.bin').catch(() => new ArrayBuffer(0))]);
  const textures = {}, blobs = await textureBlobs(root, world);
  await Promise.all(Object.entries(blobs).map(async ([k, blob]) => {
    textures[k] = await decodeTextureBlob(blob, textureDecodeConfig().bc);
  }));
  const vertices = new Float32Array(vb), indices = new Uint32Array(ib);
  const parts = rig.parts.map((part) => {
    const v0 = part.first_vertex, count = part.vertex_count, skinIndex = new Uint16Array(count * 4), skinWeight = new Float32Array(count * 4);
    for (let i = 0; i < count; i++) rig.skin[v0 + i].slice(0, 4).forEach(([j, w], k) => { skinIndex[i * 4 + k] = j; skinWeight[i * 4 + k] = w; });
    const batches = world.batches.filter((b) => b.first_index >= part.first_index && b.first_index < part.first_index + part.index_count);
    const keys = [], groups = [], list = [];
    for (const b of batches) {
      let at = keys.indexOf(b.texture);
      if (at < 0) {
        at = keys.length;
        keys.push(b.texture);
      }
      groups.push([list.length, b.index_count, at]);
      for (let i = 0; i < b.index_count; i++) list.push(indices[b.first_index + i] - v0);
    }
    return { interleaved: vertices.slice(v0 * 10, (v0 + count) * 10), skinIndex, skinWeight, index: new Uint32Array(list), groups, keys,
             morphs: (part.morphs || []).map((m) => new Float32Array(mb.slice(m.offset, m.offset + count * 12))) };
  });
  return { world, rig, textures, parts };
}

// Every preview (Select Character, Equip Gear, cutscene actors) lights through the core it was last given; a course change in the
// page (main.js unloadCourse) drops the old core instance, so the previews forget it (their pointers were in its memory).
const previews = new Set();
let textureSet = quality.riderTextures;
onQualityChange((q) => { if (q.riderTextures === textureSet) return; textureSet = q.riderTextures; for (const r of previews) r.deref()?.retexture(); });
export function releasePreviewCores() { for (const r of previews) { const p = r.deref(); if (!p) { previews.delete(r); continue; } p.core = null; p.pointers = null; } }
export class FrontEndPreview {
  // cache: root -> built model (kept while the front end is open: switching back is instant); pending: root -> promise.
  // compile(group): optional (set by the owner) — compiles the new model's pipelines before it is shown
  // (renderer.compileAsync), so its first draw builds nothing.
  constructor() {
    previews.add(new WeakRef(this));
    this.root = null;
    this.model = null;
    this.loading = null;
    this.failed = new Set();
    this.pointers = null;
    this.core = null;
    this.cache = new Map();
    this.pending = new Map();
    this.compile = null;
  }
  get ready() {
    return !!this.model && this.model.root === this.root;
  }
  // Ask for the package of `entry`; returns true while it is (or is becoming) the preview.
  want(T, entry) {
    const root = previewRoot(entry);
    if (!root || this.failed.has(root)) return false;
    if (root !== this.root) {
      this.root = root;
      const cached = this.cache.get(root);
      if (cached) {
        this.swap(cached);
        this.loading = null;
        return true;
      }
      const request = (this.loading = this.fetch(T, root)
        .then((model) => {
          if (this.loading !== request) return;
          this.swap(model);
          this.loading = null;
        })
        .catch((error) => {
          console.warn('FE preview package unavailable (tools/export_fe_preview.py)', root, error);
          this.failed.add(root);
          if (this.loading === request) this.loading = null;
        }));
    }
    return true;
  }
  swap(model) {
    const old = this.model;
    this.model = model;
    if (old && old !== model) {
      old.group.parent?.remove(old.group);
      old.group.visible = false;
      if (old === this.stale) {
        this.stale = null;
        old.dispose();
      }
    }
  }
  // The rider texture set changed (Options > Display & Touch 'Texture set', xboxRiders): every cached model is dropped; the one shown
  // stays until its rebuild (the owner's next want()) swaps in.
  retexture() {
    this.release();
    if (this.model) {
      this.cache.delete(this.model.root);
      this.stale = this.model;
    }
    this.root = null;
    this.failed.clear();
  }
  // Prepare (worker) + build + compile a package without showing it; cached. Used for the current rider and to
  // warm the neighbours / the roster in idle time (prefetch).
  fetch(T, root) {
    if (this.cache.has(root)) return Promise.resolve(this.cache.get(root));
    if (this.pending.has(root)) return this.pending.get(root);
    const job = (async () => {
      const mark = (n) => {
        try {
          performance.mark(`fe:${n}:${root}`);
        } catch {}
      };
      mark('fetch');
      const prepared = await (prepareInWorker(root) || prepareHere(root));
      mark('build');
      const model = this.build(T, root, prepared);
      mark('compile');
      const pending = [];
      if (this.compile) {
        await this.compileParts(model, pending);
        try {
          await Promise.all(pending);
        } catch (error) {
          console.warn('FE preview compile', error);
        }
        if (this.model !== model) model.group.visible = false;
      }
      mark('ready');
      this.cache.set(root, model);
      return model;
    })().finally(() => this.pending.delete(root));
    this.pending.set(root, job);
    return job;
  }
  // The model's meshes compiled part by part with frames between (a part's node material build is 40-110 ms at 4x CPU; the whole
  // model in one compile was one 300-500 ms task: on the character select after its Cross, under a load screen for the cutscene
  // cast). Each part rides in a detached group for its call (listed as its only child: it keeps its parent and place). Its
  // pipelines go to pending (awaited by fetch).
  async compileParts(model, pending) {
    // compileAsync skips invisible objects: compile it visible, then hide it until it is swapped in (fetch)
    model.group.visible = true;
    try {
      // each part's builds run in its call; the pipelines compile on and are awaited together; a frame once ~16 ms of builds ran
      const one = new model.group.constructor();
      let t0 = performance.now();
      for (const mesh of model.meshes) {
        one.children.length = 0;
        one.children.push(mesh);
        try {
          pending.push(Promise.resolve(this.compile(one)));
        } finally {
          one.children.length = 0;
        }
        if (performance.now() - t0 > 16) {
          await nextFrame();
          t0 = performance.now();
        }
      }
    } catch (error) {
      console.warn('FE preview compile', error);
    }
  }
  // active(): feCompileSpread, the owner's screen still wants the roster (web/character-select.js); the prefetch stops once it does
  // not (it went on under the CTM world load: nine riders' builds on the load screen) and resolves false.
  prefetch(T, entries, { active = null } = {}) {
    // one package at a time (the worker and the compile are serial anyway), never the one being shown
    const roots = entries.map(previewRoot).filter((r) => r && !this.failed.has(r) && !this.cache.has(r));
    const next = async () => {
      for (const r of roots) {
        if (active && !active()) return false;
        if (this.cache.has(r)) continue;
        try {
          await this.fetch(T, r);
        } catch {
          this.failed.add(r);
        }
      }
      return true;
    };
    return (this.prefetching = (this.prefetching || Promise.resolve()).then(next));
  }
  release() {
    for (const [root, m] of this.cache)
      if (m !== this.model) {
        m.group.parent?.remove(m.group);
        m.dispose();
        this.cache.delete(root);
      }
  }

  build(T, root, { world, rig, textures: bitmaps, parts: prepared }) {
    const textures = {};
    for (const [k, bitmap] of Object.entries(bitmaps)) {
      // exact texels (web/png-texels.js via fe-preview-prepare.js) or an ImageBitmap for a PNG the decoder does not cover
      // an Xbox HD entry (docs/xbox-textures.md section 8): BC blocks + mips (bcTexture) or its texels, userData.entryDomain 'xbox'
      const tex = bitmap.mipmaps ? bcTexture(bitmap) : bitmap.data ? texelTexture(bitmap) : new T.Texture(bitmap);
      if (bitmap.domain) tex.userData.entryDomain = bitmap.domain;
      tex.flipY = false;
      tex.wrapS = tex.wrapT = T.RepeatWrapping;
      tex.colorSpace = NoColorSpace;
      tex.anisotropy = 4;
      tex.premultiplyAlpha = false;
      tex.needsUpdate = true;
      textures[k] = tex;
    }
    const group = new T.Group();
    group.name = 'fe-preview';
    group.visible = false;
    group.userData.previewRoot = root; // web/diagnostics.js bind-pose probe
    const bones = rig.bones.map((b) => {
      const o = new T.Bone();
      o.name = b.name;
      o.position.fromArray(b.translation);
      o.quaternion.fromArray(b.rotation).normalize();
      return o;
    });
    rig.bones.forEach((b, i) => {
      if (b.parent >= 0) bones[b.parent].add(bones[i]);
      else group.add(bones[i]);
    });
    // Lighting: ten coefficient rows shared by every material of this preview.
    const rows = Array.from({ length: 10 }, () => new Vector4()),
      coefficients = uniformArray(rows, 'vec4'),
      lit = uniform(false);
    const n = modelNormalMatrix.mul(normalLocal); // skinned normal, world (three Y-up)
    const lighting = varying(
      riderIrradianceNode(
        Array.from({ length: 10 }, (_, i) => coefficients.element(i)),
        vec3(n.x, n.z.negate(), n.y)
      ),
      'ssxFeLighting'
    );
    const materials = new Map();
    const material = (key) => {
      if (materials.has(key)) return materials.get(key);
      const map = textures['9-' + key],
        ps2 = (map.userData.entryDomain ?? world.textures['9-' + key].texel_domain) === 'ps2'; // 'xbox': full intensity, halved below
      const m = new T.MeshBasicNodeMaterial({ map, side: T.DoubleSide, alphaTest: 0.35, fog: false });
      // GS texels: the PS2 rider textures hold colour at half intensity (128 = 1.0; tools/export_fe_preview.py
      // ps2_texels); a GameCube-domain texture (Sam) is halved into that domain.
      const sample = texture(map),
        texel = vec4(sample.rgb.mul(ps2 ? 255 : 127.5), sample.a.mul(255));
      const combined = riderHighlight2Node(texel, lighting).div(255);
      const unlit = texel.rgb.mul(2 / 255).min(1); // no lighting yet: texel x 1.0
      m.outputNode = select(lit, vec4(toFrame(combined.rgb), combined.a), vec4(toFrame(unlit), sample.a));
      // 'alph' / 'ea*' blended in two passes ([high, low]), the others opaque (web/rider-material.js)
      const name = world.batches.find((b) => b.texture === key)?.material;
      m.userData.materialName = name; // (QA: the PS2 material name)
      const drawn = riderDrawState(m, name);
      materials.set(key, drawn);
      return drawn;
    };
    const meshes = [];
    rig.parts.forEach((part, p) => {
      // 0x19F548: the board is moved out of view (Select/Setup/Details); Equip Gear shows it (web/wardrobe.js, mesh.userData.board)
      const d = padMorphPart(prepared[p]),
        g = new T.BufferGeometry(),
        inter = new T.InterleavedBuffer(d.interleaved, 10);
      g.setAttribute('position', new T.InterleavedBufferAttribute(inter, 3, 0));
      g.setAttribute('normal', new T.InterleavedBufferAttribute(inter, 3, 3));
      g.setAttribute('uv', new T.InterleavedBufferAttribute(inter, 2, 6));
      g.setAttribute('skinIndex', new T.BufferAttribute(d.skinIndex, 4));
      g.setAttribute('skinWeight', new T.BufferAttribute(d.skinWeight, 4));
      if (d.morphs.length) {
        g.morphAttributes.position = d.morphs.map((m) => new T.BufferAttribute(m, 3));
        g.morphTargetsRelative = true;
      }
      const drawn = d.keys.map((k) => material(k)),
        mats = drawn.map((m) => [].concat(m)[0]);
      for (const [start, count, at] of d.groups) g.addGroup(start, count, at);
      // the second (low-alpha, no Z) pass of a blended material: the same ranges, drawn after every first pass of the part
      drawn.forEach((m, at) => {
        if (!Array.isArray(m)) return;
        const low = mats.push(m[1]) - 1;
        for (const [start, count, k] of d.groups) if (k === at) g.addGroup(start, count, low);
      });
      g.setIndex(new T.BufferAttribute(d.index, 1));
      const mesh = new T.SkinnedMesh(g, mats);
      mesh.frustumCulled = false;
      mesh.renderOrder = 600;
      mesh.userData.part = part;
      if (part.board) {
        mesh.visible = false;
        mesh.userData.board = true;
      }
      group.add(mesh);
      meshes.push(mesh);
    });
    group.updateMatrixWorld(true);
    // Pad every preview skeleton to one bone count: the skinning shader sizes its bone array by the count, so all
    // riders share the same pipelines (first compile only; Safari stalls on every new pipeline). Dummy bones are
    // identity and unreferenced by the skin.
    const inverses = originalRiderBoneInverses(rig, 1),
      skinBones = bones.slice();
    while (skinBones.length < SKIN_BONES) {
      const b = new T.Bone();
      group.add(b);
      skinBones.push(b);
      inverses?.push(new T.Matrix4());
    }
    const skeleton = inverses ? new T.Skeleton(skinBones, inverses) : new T.Skeleton(skinBones);
    for (const m of meshes) m.bind(skeleton, m.matrixWorld);
    const fe = rig.fe || {};
    return {
      root,
      group,
      bones,
      rig,
      meshes,
      rows,
      lit,
      fe,
      hips: bones[rig.bones.findIndex((b) => b.name === (fe.hips_bone || 'hips'))],
      dispose() {
        for (const m of meshes) m.geometry.dispose();
        for (const m of materials.values()) for (const x of [].concat(m)) x.dispose();
        for (const t of Object.values(textures)) {
          t.dispose();
          t.image?.close?.();
        }
      }
    };
  }

  // Pose the preview skeleton and morphs from sample A, crossfaded toward B by `weight` (web/character-select.js).
  // opts.board: keep the clip's board (Ubertrick Setup shows the rider on the board, 0x184C60 skips 0x19F548; web/fe-screens.js).
  apply(T, A, B, weight, samples, { board = false } = {}) {
    const model = this.model;
    if (!this.ready) return false;
    const v = new T.Vector3(),
      q = new T.Quaternion();
    const local = (s, b) => {
      const stream = s.clip.streams?.[String(b.file)];
      if (!stream) return null;
      const tc = b.animation_translation_channel,
        rc = b.animation_rotation_channel,
        x = (k) => channelValue(samples, s, stream, k);
      return {
        p: tc >= 0 ? new T.Vector3(x(tc) / 100, x(tc + 2) / 100, -x(tc + 1) / 100) : null,
        r: rc >= 0 ? afb(T, x(rc), x(rc + 1), x(rc + 2)) : null
      };
    };
    model.rig.bones.forEach((b, k) => {
      const bone = model.bones[k];
      bone.position.fromArray(b.translation);
      bone.quaternion.fromArray(b.rotation).normalize();
      const pa = local(A, b),
        pb = B ? local(B, b) : null;
      if (pa?.p) bone.position.copy(pa.p);
      if (pa?.r) bone.quaternion.copy(pa.r);
      if (pb) {
        if (pb.p) bone.position.lerp(v.copy(pb.p), weight);
        if (pb.r) bone.quaternion.slerp(q.copy(pb.r), weight);
      }
      if (b.name === 'board_rootg' && !board) bone.position.set(0, 100, 0); // 0x19F548: the board is moved past the far plane
    });
    if (board || model.boardShown) {
      for (const mesh of model.meshes) if (mesh.userData.board) mesh.visible = board;
      model.boardShown = board;
    }
    for (const mesh of model.meshes) {
      const part = mesh.userData.part;
      if (!mesh.morphTargetInfluences) continue;
      const wa = morphWeights(part, samples, A),
        wb = B ? morphWeights(part, samples, B) : null;
      for (let i = 0; i < mesh.morphTargetInfluences.length; i++) {
        const a = wa ? (wa[i] ?? 0) : 0;
        mesh.morphTargetInfluences[i] = wb ? a + ((wb[i] ?? 0) - a) * weight : a; // past the part's own morphs: 0
      }
    }
    return true;
  }

  // Root transform (136,-250,-82) cm turned (100 + yaw) degrees about Z; three.js convention (x, z, -y) / 100.
  place(T, parent, yaw) {
    const model = this.model;
    if (!this.ready) return false;
    if (model.group.parent !== parent) parent.add(model.group);
    const p = model.fe.root_position_cm || [136, -250, -82];
    model.group.position.set(p[0] / 100, p[2] / 100, -p[1] / 100);
    model.group.scale.setScalar(1);
    model.group.quaternion.setFromAxisAngle(new T.Vector3(0, 1, 0), (100 + yaw) * DEG);
    model.group.updateMatrixWorld(true);
    return true;
  }

  show(visible) {
    if (this.model) this.model.group.visible = visible && this.ready;
  }

  // Coefficients: IRR record + 389CB8 rim at the hips with the FE view matrix, through the core's port.
  // view: optional PS2 view matrix (16 floats, row vectors, scene-relative cm) replacing the FE camera's (web/cutscenes.js).
  light(T, core, view = null) {
    const model = this.model;
    if (!this.ready || !core?._shade_rider_lighting || !model.fe.irradiance) {
      if (model) model.lit.value = false;
      return false;
    }
    try {
      if (this.core !== core) {
        this.core = core;
        this.pointers = { env: core._malloc(160), view: core._malloc(64), point: core._malloc(16), constants: core._malloc(20) };
      }
      const p = this.pointers,
        fe = model.fe;
      new Uint32Array(core.HEAPU8.buffer, p.env, 40).set(fe.irradiance.coefficient_bits);
      if (view) new Float32Array(core.HEAPU8.buffer, p.view, 16).set(view);
      else if (fe.view_matrix_bits) new Uint32Array(core.HEAPU8.buffer, p.view, 16).set(fe.view_matrix_bits);
      else new Float32Array(core.HEAPU8.buffer, p.view, 16).set([-1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 200, 1]);
      new Float32Array(core.HEAPU8.buffer, p.constants, 5).set(fe.rim_constants);
      const hips = model.hips.getWorldPosition(new T.Vector3());
      new Float32Array(core.HEAPU8.buffer, p.point, 4).set([hips.x * 100, -hips.z * 100, hips.y * 100, 1]);
      // No local lights on the FE: a race leaves its selection behind (startRun resets it again).
      if (core._rider_lighting_info && new Float32Array(core.HEAPF32.buffer, core._rider_lighting_info(), 5)[4] > 0)
        core._reset_rider_lighting();
      core._shade_rider_lighting(p.env, p.view, p.point, fe.rim_scale ?? 1, p.constants, 0, 0);
      const gpu = new Float32Array(core.HEAPF32.buffer, core._rider_lighting_gpu_coefficients(), 40);
      for (let i = 0; i < 10; i++) model.rows[i].fromArray(gpu, i * 4);
      model.lit.value = true;
      return true;
    } catch (error) {
      model.lit.value = false;
      return false;
    }
  }
}
