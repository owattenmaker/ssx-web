// Weather drawing (docs/weather.md): the snowfall object's layers and the camera splash, from the core (web/weather.inc).
//
// Snowfall 0x2E6008 (render of the snowfall object, group 2): per layer of the current camera, 0x2E5430 (the layer offset
// follows the camera: the core applies it after each camera update) and 0x2E55D8 -> renderer slot 0x2A8 (flakes, kind 0,
// 0x381310) / 0x2B0 (fluff, kind 1, 0x3816F0): VU1 program 5 at 0x000 / 0x408, one GS sprite per flake:
//   n = LFSR^(k+2)(seed) per axis (RINIT/RNEXT, 3 seeds per layer, [1, 2)) + offset/extent - 1.5, wrapped (+1 below the
//   box, -1 at or above it) into the box [f - 0.5, f + 0.5) with f = camera forward (+0x80) * 0.6; camera-relative
//   position n * extent (layer matrix = diag(extent)), screen-aligned sprite of half size = the layer size (world cm),
//   ST 0..1 over FX 8 'sfal' (renderer+0xF70), vertex colour trunc(RGB*128), A trunc(alpha*128), TFX MODULATE, alpha
//   blend, depth tested, priority 7 (after the fog composite, unfogged: the encoded composite, web/snow-composite.js).
//   Fluff (kind 1): the same volume, half size 600 cm, and the alpha faded by z' = depth / extent: * clamp(3.33 z')
//   below 0.3, * clamp(1 - 3.33 (z' - 0.8)) above 0.8.
//   count = trunc(snowfall * 500) flakes (trunc(fluff * 6) puffs): 3000 per flake layer in the Ruthless blizzard.
// Camera splash 0x2F3E28 (drops 0x2F2C30 'ices' renderer+0x1058, crystals 0x2F3418 'icel' +0x105C): 640x480 screen
//   sprites, rotated quads 0x3781A0 (P -/+ dx -/+ dy, half size w/h), angle + spin * age/life degrees, alpha A * (1 -
//   (age/life)^2), priority 8, over the screen shake offset (0 here).
import * as T from 'three/webgpu';
import {attribute, texture, vec3, vec4, float, uniform, select, positionGeometry, modelViewMatrix, cameraProjectionMatrix, uv, clamp} from 'three/tsl';import {toFrame} from './frame-space.js';
import {registerEncodedEffect} from './snow-composite.js';
import {lfsrNext} from './set-piece-particle-sprites.js';

export const LAYER_WORDS = 25;
const F32 = new Float32Array(1), U32 = new Uint32Array(F32.buffer);
const fromBits = (w) => { U32[0] = w; return F32[0]; };
// Core weather_layers(): [count, per layer 25 words] (web/weather.inc).
export function readLayers(core) {
  if (!core._weather_layers) return [];
  const U = new Uint32Array(core.HEAPU8.buffer); let at = core._weather_layers() >> 2; const n = U[at++], out = [];
  for (let k = 0; k < n; k++, at += LAYER_WORDS) {
    const f = (i) => fromBits(U[at + i]);
    out.push({camera: U[at] | 0, kind: U[at + 1] | 0, count: U[at + 2] | 0, extent: f(3), speed: f(4), size: f(5), alpha: f(6), gravity: f(7),
      offset: [f(8), f(9), f(10)], gustNew: [f(11), f(12), f(13)], gustOld: [f(14), f(15), f(16)], timer: f(17), period: f(18),
      seeds: [U[at + 19], U[at + 20], U[at + 21]], colour: [f(22), f(23), f(24)]});
  }
  return out;
}
// VU1 program 5: flake k of a layer draws the (k + 2)-th RNEXT of each seed (RINIT keeps the 23 mantissa bits).
export function flakeSeeds(seeds, count) {
  const out = new Float32Array(count * 3), s = seeds.map((x) => lfsrNext(x));
  for (let k = 0; k < count; k++) for (let a = 0; a < 3; a++) { s[a] = lfsrNext(s[a]); out[k * 3 + a] = fromBits(s[a]); }
  return out;
}
// 0x2E55D8: the normalized layer offset as the program receives it (row 6 = offset - 1.5 after the box pre-wrap) and the box.
export function layerBox(offset, extent, forward) {
  const q = Math.fround(1 / extent), lower = forward.map((d) => Math.fround(Math.fround(d * 0.6) - 0.5)), upper = lower.map((l) => Math.fround(l + 1));
  const base = offset.map((o, a) => { let v = Math.fround(o * q); if (v < Math.fround(lower[a] - 0.5)) v = Math.fround(v + 1); if (Math.fround(upper[a] + 0.5) < v) v = Math.fround(v - 1); return Math.fround(v - 1.5); });
  return {base, lower, upper};
}
// The CPU model of one flake (tests): the program's wrap, camera-relative source cm.
export function flakePosition(r, box, extent) {
  return r.map((x, a) => { let p = Math.fround(x + box.base[a]); if (p < box.lower[a]) p = Math.fround(p + 1); if (!(p < box.upper[a])) p = Math.fround(p - 1); return Math.fround(p * extent); });
}

async function fxTexture(tag) {
  const meta = (await (await fetch('/assets/FX/fx.json')).json()).textures[tag]; if (!meta) throw Error('Missing FX texture ' + tag);
  const bytes = new Uint8Array(await (await fetch('/assets/FX/' + meta.file)).arrayBuffer());
  if (bytes.length !== meta.width * meta.height * 4) throw Error('FX texture extent ' + tag);
  const map = new T.DataTexture(bytes, meta.width, meta.height, T.RGBAFormat);
  map.minFilter = map.magFilter = T.LinearFilter; map.wrapS = map.wrapT = T.ClampToEdgeWrapping; map.colorSpace = T.NoColorSpace; map.needsUpdate = true;
  return map;
}
function quadGeometry() { // corner x, y in {-1, 1}; ST s = (x + 1) / 2, t = (1 - y) / 2
  const g = new T.PlaneGeometry(2, 2);
  for (let v = 0; v < g.attributes.uv.count; v++) g.attributes.uv.setXY(v, (g.attributes.position.getX(v) + 1) / 2, (1 - g.attributes.position.getY(v)) / 2);
  return g;
}

// capacity: {flakes, fluff} instances per layer (the largest count of the course's payloads; the core clamps nothing).
// density: the share of each layer's flakes drawn (quality tiers; 1 = the PS2's count).
export async function createWeatherRenderer({core, origin, capacity = {flakes: 3000, fluff: 64}, density = 1}) {
  if (!core._weather_info || !core._weather_layers) return null;
  const [sfal, ices, icel] = await Promise.all(['sfal', 'ices', 'icel'].map(fxTexture));
  const encodedOutput = uniform(false), group = new T.Group(); group.name = 'weather'; group.userData.gameplayOnly = true;
  const layers = [];
  const out = (rgb, a) => vec4(select(encodedOutput, rgb, toFrame(rgb)), a);
  const makeLayer = (kind) => {
    const cap = kind === 0 ? capacity.flakes : capacity.fluff;
    const geometry = quadGeometry(), seed = new T.InstancedBufferAttribute(new Float32Array(cap * 3), 3); geometry.setAttribute('flakeSeed', seed);
    const u = {base: uniform(new T.Vector3()), lower: uniform(new T.Vector3()), upper: uniform(new T.Vector3()), extent: uniform(1), eye: uniform(new T.Vector3()),
      size: uniform(0), colour: uniform(new T.Vector4(1, 1, 1, 1))};
    const r = attribute('flakeSeed', 'vec3'), p0 = r.add(u.base);
    const wrap = (v, lo, hi) => { const a = select(v.lessThan(lo), v.add(1), v); return select(a.greaterThanEqual(hi), a.sub(1), a); };
    const p = vec3(wrap(p0.x, u.lower.x, u.upper.x), wrap(p0.y, u.lower.y, u.upper.y), wrap(p0.z, u.lower.z, u.upper.z)).mul(u.extent); // source cm, camera-relative
    const world = u.eye.add(vec3(p.x, p.z, p.y.negate()).div(100)), view = modelViewMatrix.mul(vec4(world, 1));
    const half = u.size.div(100);
    const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: true, side: T.DoubleSide, forceSinglePass: true, fog: false, toneMapped: false});
    material.vertexNode = cameraProjectionMatrix.mul(vec4(view.x.add(positionGeometry.x.mul(half)), view.y.add(positionGeometry.y.mul(half)), view.z, 1));
    const texel = texture(sfal, uv());
    let alpha = texel.a.mul(255 / 128).mul(u.colour.w);
    if (kind === 1) { // program 5 at 0x408: z' = depth / extent
      const z = view.z.negate().mul(100).div(u.extent);
      const fade = select(z.lessThanEqual(0.3), clamp(z.mul(3.33), 0, 1), select(z.greaterThan(0.8), clamp(float(1).sub(z.sub(0.8).mul(3.33)), 0, 1), float(1)));
      alpha = alpha.mul(fade);
    }
    material.fragmentNode = out(texel.rgb.mul(u.colour.xyz).clamp(0, 1), alpha.clamp(0, 1));
    const mesh = new T.InstancedMesh(geometry, material, cap); mesh.count = 1; mesh.visible = false; mesh.frustumCulled = false; mesh.renderOrder = 695 + kind;
    mesh.name = kind === 0 ? 'snowfall flakes' : 'snowfall fluff';
    group.add(mesh);
    return {kind, cap, mesh, seed, u, seeds: null};
  };
  // Camera splash: 640x480 screen sprites straight to clip space.
  const makeSplash = (map, cap, order) => {
    const geometry = quadGeometry();
    const pos = new T.InstancedBufferAttribute(new Float32Array(cap * 4), 4).setUsage(T.DynamicDrawUsage), rot = new T.InstancedBufferAttribute(new Float32Array(cap * 2), 2).setUsage(T.DynamicDrawUsage), col = new T.InstancedBufferAttribute(new Float32Array(cap * 4), 4).setUsage(T.DynamicDrawUsage);
    geometry.setAttribute('splashPos', pos); geometry.setAttribute('splashRot', rot); geometry.setAttribute('splashColour', col);
    const P = attribute('splashPos', 'vec4'), R = attribute('splashRot', 'vec2'), C = attribute('splashColour', 'vec4');
    const x = positionGeometry.x.mul(P.z), y = positionGeometry.y.negate().mul(P.w); // screen y grows downward
    const sx = P.x.add(x.mul(R.x)).sub(y.mul(R.y)), sy = P.y.add(x.mul(R.y)).add(y.mul(R.x));
    const material = new T.MeshBasicNodeMaterial({transparent: true, depthWrite: false, depthTest: false, side: T.DoubleSide, forceSinglePass: true, fog: false, toneMapped: false});
    material.vertexNode = vec4(sx.div(320).sub(1), float(1).sub(sy.div(240)), 0, 1);
    const texel = texture(map, uv());
    material.fragmentNode = out(texel.rgb.mul(C.yzw).clamp(0, 1), texel.a.mul(255 / 128).mul(C.x).clamp(0, 1));
    const mesh = new T.InstancedMesh(geometry, material, cap); mesh.count = 1; mesh.visible = false; mesh.frustumCulled = false; mesh.renderOrder = order;
    mesh.name = 'camera splash'; group.add(mesh);
    return {mesh, pos, rot, col, cap};
  };
  // 0x2E5920 builds 4 flake layers and 2 fluff layers per camera: all six materials exist before the race (loading warm-up).
  for (const kind of [0, 0, 0, 0, 1, 1]) layers.push(makeLayer(kind));
  const drops = makeSplash(ices, 30, 900), crystals = makeSplash(icel, 24, 901);
  registerEncodedEffect({object: group, setEncodedOutput: (v) => { encodedOutput.value = !!v; }, populated: () => group.children.some((m) => m.visible && m.count > 0)});
  const state = {layers: 0, flakes: 0, fluff: 0, drops: 0, crystals: 0, snowfall: 0};
  const forward = new T.Vector3(), eye = new T.Vector3();
  const vtx = (n, v) => Math.trunc(Math.fround(v * 128)) / 128;
  function fillSplash(target, F, at, n) {
    const m = Math.min(n, target.cap), p = target.pos.array, r = target.rot.array, c = target.col.array;
    for (let k = 0; k < m; k++) {
      const o = at + k * 14, x = F[o], y = F[o + 1], w = F[o + 2], h = F[o + 3], age = F[o + 4], life = F[o + 5];
      const t = Math.fround(age / life), fade = Math.fround(1 - Math.fround(t * t));
      const angle = Math.fround(F[o + 10] + Math.fround(t * F[o + 11])) * Math.PI / 180;
      p[k * 4] = x; p[k * 4 + 1] = y; p[k * 4 + 2] = w; p[k * 4 + 3] = h; r[k * 2] = Math.cos(angle); r[k * 2 + 1] = Math.sin(angle);
      c[k * 4] = vtx(0, Math.fround(F[o + 6] * fade)); c[k * 4 + 1] = vtx(0, F[o + 7]); c[k * 4 + 2] = vtx(0, F[o + 8]); c[k * 4 + 3] = vtx(0, F[o + 9]);
    }
    target.mesh.count = Math.max(m, 1); target.mesh.visible = m > 0;
    if (m) for (const a of [target.pos, target.rot, target.col]) { a.clearUpdateRanges(); a.addUpdateRange(0, m * a.itemSize); a.needsUpdate = true; }
    return m;
  }
  const api = {
    group, state,
    // Once per drawn frame, after the frame's ticks.
    update(camera) {
      const info = new Float32Array(core.HEAPF32.buffer, core._weather_info(), 11);
      if (!info[0]) { group.visible = false; return; }
      group.visible = true;
      const all = readLayers(core).filter((l) => l.camera === 0);
      while (layers.length < all.length) layers.push(makeLayer(all[layers.length].kind));
      camera.getWorldDirection(forward); const fwd = [forward.x, -forward.z, forward.y]; // three -> source axes
      eye.set(info[7] / 100 - origin.x, info[9] / 100 - origin.y, -info[8] / 100 - origin.z);
      state.layers = all.length; state.flakes = state.fluff = 0;
      all.forEach((l, i) => {
        const L = layers[i]; if (L.kind !== l.kind) { L.mesh.visible = false; return; }
        if (!L.seeds || L.seeds.some((s, a) => s !== l.seeds[a])) { L.seed.array.set(flakeSeeds(l.seeds, L.cap)); L.seed.needsUpdate = true; L.seeds = l.seeds.slice(); }
        const box = layerBox(l.offset, l.extent, fwd);
        L.u.base.value.set(...box.base); L.u.lower.value.set(...box.lower); L.u.upper.value.set(...box.upper); L.u.extent.value = l.extent; L.u.eye.value.copy(eye);
        L.u.size.value = l.size; L.u.colour.value.set(vtx(0, l.colour[0]), vtx(0, l.colour[1]), vtx(0, l.colour[2]), vtx(0, l.alpha));
        const n = Math.min(Math.max(0, l.count), L.cap), drawn = density >= 1 ? n : Math.floor(n * density);
        L.mesh.count = Math.max(drawn, 1); L.mesh.visible = drawn > 0 && l.alpha > 0;
        if (l.kind === 0) state.flakes += drawn; else state.fluff += drawn;
      });
      for (let i = all.length; i < layers.length; i++) layers[i].mesh.visible = false;
      const F = new Float32Array(core.HEAPF32.buffer, core._weather_splash(), 5); const nd = F[0], nc = F[1]; state.snowfall = F[4];
      const S = new Float32Array(core.HEAPF32.buffer, core._weather_splash() + 20, (nd + nc) * 14);
      state.drops = fillSplash(drops, S, 0, nd); state.crystals = fillSplash(crystals, S, nd * 14, nc);
    },
    setDensity(v) { density = v; },
    dispose() { group.traverse((o) => { if (o.isMesh) { o.geometry.dispose(); o.material.dispose(); } }); for (const t of [sfal, ices, icel]) t.dispose(); },
  };
  (globalThis.ssxEffects ??= {}).weather = api; return api;
}
