// PS2 rider shadows (SLUS_207.72; docs/terrain-render-fidelity.md "Rider shadows"): per visible rider (human and
// computer riders) 1225F0 -> 122898 fits a box around six bones in a straight-down light space (374D00), renders the
// rider's silhouette into a 128x128 PSMCT16 texture (flat 38 -> reads 32; the board and bindings paint 0 while
// grounded, painter's order by part slot, no depth test) and the terrain pass 38D448 redraws the near patches with
// that texture through the box's UV matrix, blend 0x81 with As 0x80: Cd = max(Cd - T, 0) per channel, before the fog
// composite and ScreenTint. The light is always (0, 0, -1): no sun, height or distance dependence; receivers are
// terrain only, within the 30 m slab below the feet.
// Browser: one 768x128 atlas (six 128-pixel tiles), one render per frame (all riders), the terrain material samples it
// (riderShadowReceiver, web/world-material.js). Riders are scene groups tagged userData.shadowRider = {skin (the source
// skin, web/rider-skinning.js worldNode), batches, indices (original part order), core() (world_pose_bones /
// pose_physical)}.
import * as T from 'three/webgpu';
import {Fn, uniform, uniformArray, texture, vec2, vec4, float, int, Loop, positionWorld, attribute, select, dot, max, clamp, round, Discard, varying, renderGroup} from 'three/tsl';
import {pv} from './pv-flags.js';

export const SHADOW_MAX = 6, SHADOW_TEXELS = 128;
const BONES = [5, 10, 15, 18, 21, 0], MARGIN = 70, SLAB = 3000, VALUE = 32 / 255; // 0x428C0000 cm; 30 m slab; 38 -> PSMCT16 -> 32
const f = Math.fround;

// Receiver state (module level: the world materials are built before the riders load).
const atlas = new T.RenderTarget(SHADOW_TEXELS * SHADOW_MAX, SHADOW_TEXELS, {type: T.UnsignedByteType, depthBuffer: true});
atlas.texture.minFilter = atlas.texture.magFilter = T.LinearFilter; atlas.texture.generateMipmaps = false; atlas.texture.colorSpace = T.NoColorSpace;
const rows = uniformArray(Array.from({length: 3 * SHADOW_MAX}, () => new T.Vector4()), 'vec4'); // per slot: u, v, depth-below-feet rows over browser world metres
const count = uniform(0, 'int');
// pv staticRefresh (web/static-world.js): the receiver's per-frame values in the shared render group, so a terrain batch that skips three's
// per-object refresh still reads this frame's rows (same values, updated once per render)
if (pv('staticRefresh')) { rows.setGroup(renderGroup); count.setGroup(renderGroup); }
// bytes: the terrain's GS colour (0..255 per channel) after the light pass. Returns max(bytes - sum of shadow texels, 0).
export function riderShadowReceiver(bytes) {
  return Fn(() => {
    const p = vec4(positionWorld, 1), sum = float(0).toVar();
    Loop({start: int(0), end: count, type: 'int', condition: '<'}, ({i}) => {
      const u = dot(rows.element(i.mul(3)), p), v = dot(rows.element(i.mul(3).add(1)), p), lz = dot(rows.element(i.mul(3).add(2)), p);
      const t = clamp(vec2(u, v), .5 / SHADOW_TEXELS, 1 - .5 / SHADOW_TEXELS); // GS CLAMP to the rider's own texture
      const s = round(texture(atlas.texture, vec2(t.x.add(float(i)).div(SHADOW_MAX), t.y)).level(0).r.mul(255));
      // Outside the box the GS CLAMP repeats the edge texels, which the +/-70 cm margin keeps at 0 for the original
      // riders; a silhouette reaching the edge (Sam's larger body, gear) would otherwise streak across the whole slab.
      const inside = u.greaterThanEqual(0).and(u.lessThanEqual(1)).and(v.greaterThanEqual(0)).and(v.lessThanEqual(1));
      sum.addAssign(select(lz.lessThanEqual(SLAB).and(inside), s, float(0)));
    });
    return max(bytes.sub(sum), 0);
  })();
}

// 374D00 in browser terms. bones: 6 x [x, y, z] PS2 cm (head, hands, feet, hips), up: the rider+0x1C0 column.
// Returns null when degenerate; else {A, B, C, minx, maxx, miny, maxy}.
export function fitShadow(bones, up) { return fitShadowInto(bones, up, newFit()); }
// fitShadow into out (newFit()), without per-call arrays (per-frame garbage, docs/web-render-performance.md): the same
// float operations in the same order.
const newFit = () => ({A: [0, 0, 0], B: [0, 0, 0], C: [0, 0, 0], ac: 0, bc: 0, minx: 0, maxx: 0, miny: 0, maxy: 0});
const L = [0, 0, -1], N = [0, 0, 0], X = [0, 0, 0];
function fitShadowInto(bones, up, out) {
  N[0] = f(up[0]); N[1] = f(up[1]); N[2] = f(up[2]); normalizeInto(N);
  const A = out.A, B = out.B, C = out.C, b3 = bones[3], b4 = bones[4];
  for (let k = 0; k < 3; k++) C[k] = f(f(b3[k] + b4[k]) * .5);
  if ((N[0] === 0 && N[1] === 0 && Math.abs(N[2]) === 1)) { N[0] = f(N[0] + .01); normalizeInto(N); }
  crossInto(N, L, A); normalizeInto(A); crossInto(L, A, X); B[0] = X[0]; B[1] = X[1]; B[2] = X[2]; normalizeInto(B);
  const ac = dot3(A, C), bc = dot3(B, C);
  let minx = Infinity, maxx = -Infinity, miny = Infinity, maxy = -Infinity;
  for (let i = 0; i < bones.length; i++) { const p = bones[i], x = f(dot3(A, p) - ac), y = f(dot3(B, p) - bc); minx = Math.min(minx, x); maxx = Math.max(maxx, x); miny = Math.min(miny, y); maxy = Math.max(maxy, y); }
  out.ac = ac; out.bc = bc; out.minx = f(minx - MARGIN); out.maxx = f(maxx + MARGIN); out.miny = f(miny - MARGIN); out.maxy = f(maxy + MARGIN);
  return out;
}
const dot3 = (a, b) => f(f(f(a[0] * b[0]) + f(a[1] * b[1])) + f(a[2] * b[2]));
// o = a x b (o is neither a nor b)
const crossInto = (a, b, o) => { o[0] = f(a[1] * b[2] - a[2] * b[1]); o[1] = f(a[2] * b[0] - a[0] * b[2]); o[2] = f(a[0] * b[1] - a[1] * b[0]); };
const normalizeInto = (a) => { const n = Math.hypot(a[0], a[1], a[2]); if (n > 0) { a[0] = f(a[0] / n); a[1] = f(a[1] / n); a[2] = f(a[2] / n); } };
// Receiver rows over browser world metres b (PS2 p = (100(bx+ox), -100(bz+oz), 100(by+oy))): u = (A.p - A.C - minx)/w,
// v = (maxy - (B.p - B.C))/h, depth below the feet lz = C.z - p.z.
export function receiverRows(fit, origin) {
  const out = [new T.Vector4(), new T.Vector4(), new T.Vector4()]; receiverRowsInto(fit, origin[0], origin[1], origin[2], out, 0);
  return out.map((r) => [r.x, r.y, r.z, r.w]);
}
// receiverRows into the Vector4s out[at..at+2]
function receiverRowsInto(fit, ox, oy, oz, out, at) {
  const A = fit.A, B = fit.B, w = fit.maxx - fit.minx, h = fit.maxy - fit.miny;
  // v.p as a row over (bx, by, bz, 1): [100 v0, 100 v2, -100 v1, 100 (v0 ox - v1 oz + v2 oy)]
  const a0 = 100 * A[0], a1 = 100 * A[2], a2 = -100 * A[1], a3 = 100 * (A[0] * ox - A[1] * oz + A[2] * oy);
  const b0 = 100 * B[0], b1 = 100 * B[2], b2 = -100 * B[1], b3 = 100 * (B[0] * ox - B[1] * oz + B[2] * oy);
  out[at].set(a0 / w, a1 / w, a2 / w, (a3 - fit.ac - fit.minx) / w);
  out[at + 1].set(-b0 / h, -b1 / h, -b2 / h, (fit.maxy - b3 + fit.bc) / h);
  out[at + 2].set(0, -100, 0, fit.C[2] - 100 * oy);
}

// pv shadowAtlasInit: the atlas is a render target that world and rider materials sample (riderShadowReceiver). three creates a render
// target's GPU texture when it is first drawn into, destroying the one it made if a material bound the texture earlier. A streamed
// world compiled under the load screen (pv streamWarm, rideWarm) or drawn before any rider cast a shadow bound it first, and every
// frame after the first shadow then failed validation ("Destroyed texture [768x128] used in a submit"): the rider stopped drawing in
// free ride (PEAK1 station after a second ride start), and a teleported rider froze the frame (PEAK2 DBC2). Made a render target
// once, cleared, as soon as the renderer exists (main.js), and kept for the page (768 x 128: never disposed).
let atlasReady = null;
// Keyed by the renderer's device: after a device recovery (web/gpu-recovery.js beforeResume) the atlas is made again on the new one.
export function initRiderShadowAtlas(renderer) {
  const key = renderer?.backend?.device ?? renderer;
  if (!renderer || atlasReady === key) return;
  const prior = renderer.getRenderTarget(), alpha = renderer.getClearAlpha(), clearColor = new T.Color(); renderer.getClearColor(clearColor);
  try { renderer.setRenderTarget(atlas); renderer.setClearColor(0x000000, 0); renderer.clear(); atlasReady = key; }
  finally { renderer.setRenderTarget(prior); renderer.setClearColor(clearColor, alpha); }
}
export function createRiderShadows({renderer, scene, origin}) {
  const shadowScene = new T.Scene(), camera = new T.OrthographicCamera(-1, 1, 1, -1, 0, 1), entries = new Map();
  const frustum = new T.Frustum(), matrix = new T.Matrix4(), sphere = new T.Sphere(), clear = new T.Color();
  const state = {riders: 0, slots: 0};
  function entryFor(group) {
    let e = entries.get(group); if (e) return e;
    const info = group.userData.shadowRider, skin = info.skin; if (!skin?.worldNode) return null;
    // Per vertex painter's rank from the original part order: parts before the board = 0 (TopA), the board and
    // bindings ('bord' batches) = 1, later parts = 2.
    let vertexCount = 0; for (const x of info.indices) vertexCount = Math.max(vertexCount, x + 1);
    const rank = new Float32Array(vertexCount).fill(2); let seenBoard = false;
    for (const b of info.batches) {
      const r = b.material === 'bord' ? 1 : seenBoard ? 2 : 0; if (b.material === 'bord') seenBoard = true;
      for (let k = b.first_index; k < b.first_index + b.index_count; k++) rank[info.indices[k]] = r;
    }
    const rowX = uniform(new T.Vector4()), rowY = uniform(new T.Vector4()), slot = uniform(0), board = uniform(VALUE);
    const material = new T.MeshBasicNodeMaterial({side: T.DoubleSide, depthTest: true, depthWrite: true, fog: false, toneMapped: false});
    material.depthFunc = T.LessEqualDepth;
    const ndc = varying(vec2(0));
    material.vertexNode = Fn(() => {
      const w = skin.worldNode, p = vec4(w.xyz.div(w.w), 1), r = attribute('shadowRank', 'float');
      const n = vec2(dot(rowX, p), dot(rowY, p)); ndc.assign(n);
      const x = slot.add(n.x.mul(.5).add(.5)).div(SHADOW_MAX).mul(2).sub(1), y = n.y.negate(); // three's texture convention on both backends (WGSL as is, GLSL flips render targets): v = 0 samples the row drawn at clip y = +1, and receiver v = (1 - n.y) / 2
      return vec4(x, y, float(.8).sub(r.mul(.3)), 1);
    })();
    material.colorNode = Fn(() => {
      Discard(ndc.x.abs().greaterThan(1).or(ndc.y.abs().greaterThan(1))); // 128x128 SCISSOR
      const r = attribute('shadowRank', 'float'), v = select(r.equal(1), board, float(VALUE));
      return vec4(v, v, v, 1);
    })();
    const meshes = [];
    group.traverse((o) => {
      if (!o.isMesh || !o.geometry.getAttribute('sourcePosition')) return;
      if (!o.geometry.getAttribute('shadowRank')) o.geometry.setAttribute('shadowRank', new T.BufferAttribute(rank, 1));
      const m = new T.Mesh(o.geometry, material); m.frustumCulled = false; m.visible = false; m.userData.source = o; shadowScene.add(m); meshes.push(m);
    });
    e = {group, info, meshes, rowX, rowY, slot, board, material}; entries.set(group, e); return e;
  }
  // scene.traverse order, without descending into the static world (pv staticWorld cells and location groups, web/static-world.js):
  // no rider lives there, and it is thousands of objects per frame
  const riderList = [], walk = (o) => { if (o.userData.shadowRider) riderList.push(o); if (o.userData.staticWorld) return; const c = o.children; for (let i = 0; i < c.length; i++) walk(c[i]); };
  function riders() { riderList.length = 0; walk(scene); return riderList; }
  // per-frame scratch (docs/web-render-performance.md "Per-frame garbage"): each entry keeps its bones, fit and candidate record
  const candidates = [], byDistance = (a, b) => a.d - b.d, up = [0, 0, 0];
  const api = {
    state, atlas,
    get entryCount() { return entries.size; }, // riders with silhouette meshes (main.js warmupRender, pv warmSpread)
    // Once per rendered frame after the riders' palettes were displayed, before the world pass. warming: render every
    // rider's shadow pipeline (loading screen) whatever its visibility.
    // warmLimit (pv warmSpread, main.js warmupRender): while warming, only the first warmLimit riders' silhouettes draw, so their
    // materials build a rider per loading frame instead of all in the first one.
    update(view, warming = false, warmLimit = Infinity) {
      if (api.enabled === false) { count.value = 0; return; }
      view.updateMatrixWorld(); frustum.setFromProjectionMatrix(matrix.multiplyMatrices(view.projectionMatrix, view.matrixWorldInverse));
      candidates.length = 0; const groups = riders();
      for (let g = 0; g < groups.length; g++) {
        const group = groups[g];
        const e = entryFor(group); if (!e) continue; for (const m of e.meshes) m.visible = warming; // warming: every rider's silhouette pipeline builds, posed or not (computer riders have no pose before the race)
        const core = e.info.core?.(); if (!core?._world_pose_bones) continue;
        let visible = group.visible; for (let p = group.parent; p && visible; p = p.parent) visible = p.visible;
        if (!visible && !warming) continue;
        // The pose the skin draws this frame (interpolated between ticks, web/rider-skinning.js shadowPose); the core's
        // current tick for a skin without it. On the PS2 the box and the silhouette come from the same tick's pose.
        const drawn = e.info.skin.shadowPose?.();
        const F = drawn ? drawn.bones : core.HEAPF32, bp = drawn ? 0 : core._world_pose_bones() >> 2, n = F[bp];
        if (n < 22) continue;
        const bones = e.bones ??= BONES.map(() => [0, 0, 0]); let finite = true;
        for (let i = 0; i < BONES.length; i++) { const o = bones[i], at = bp + 1 + 7 * BONES[i]; o[0] = F[at]; o[1] = F[at + 1]; o[2] = F[at + 2]; finite = finite && Number.isFinite(o[0]) && Number.isFinite(o[1]) && Number.isFinite(o[2]); }
        const P = drawn ? drawn.physical : F, pp = drawn ? 0 : core._pose_physical() >> 2, q0 = P[pp + 3], q1 = P[pp + 4], q2 = P[pp + 5], q3 = P[pp + 6], grounded = P[pp + 8] !== 0;
        up[0] = 2 * (q0 * q2 + q3 * q1); up[1] = 2 * (q1 * q2 - q3 * q0); up[2] = 1 - 2 * (q0 * q0 + q1 * q1); // physical frame +Z (rider+0x1C0)
        if (!finite || !Number.isFinite(up[0]) || !Number.isFinite(up[1]) || !Number.isFinite(up[2])) continue;
        const fit = fitShadowInto(bones, up, e.fit ??= newFit());
        sphere.center.set(fit.C[0] / 100 - origin.x, fit.C[2] / 100 - origin.y, -fit.C[1] / 100 - origin.z); sphere.radius = 3;
        if (!warming && !frustum.intersectsSphere(sphere)) continue; // stand-in for the rider visibility flag (rider+0xB18)
        const cand = e.candidate ??= {e, fit, grounded: false, d: 0}; cand.grounded = grounded; cand.d = view.position.distanceToSquared(sphere.center); candidates.push(cand);
      }
      candidates.sort(byDistance); const slots = Math.min(candidates.length, SHADOW_MAX);
      for (let k = 0; k < slots; k++) {
        const {e, fit, grounded} = candidates[k];
        const w = fit.maxx - fit.minx, h = fit.maxy - fit.miny, mx = (fit.maxx + fit.minx) / 2, my = (fit.maxy + fit.miny) / 2;
        e.rowX.value.set(fit.A[0] * 2 / w, fit.A[1] * 2 / w, fit.A[2] * 2 / w, (-fit.ac - mx) * 2 / w);
        e.rowY.value.set(-fit.B[0] * 2 / h, -fit.B[1] * 2 / h, -fit.B[2] * 2 / h, (fit.bc + my) * 2 / h);
        e.slot.value = k; e.board.value = grounded ? 0 : VALUE;
        for (const m of e.meshes) { let v = true; for (let o = m.userData.source; o && o !== e.group && v; o = o.parent) v = o.visible; m.visible = v; } // only the parts the rider draws (hidden gear / variants cast nothing)
        receiverRowsInto(fit, origin.x, origin.y, origin.z, rows.array, 3 * k);
      }
      count.value = warming ? SHADOW_MAX : slots; state.riders = candidates.length; state.slots = slots;
      if (!slots && !warming) return;
      if (warming && warmLimit < Infinity) { let k = 0; for (const e of entries.values()) if (k++ >= warmLimit) for (const m of e.meshes) m.visible = false; }
      const prior = renderer.getRenderTarget(), autoClear = renderer.autoClear, alpha = renderer.getClearAlpha(); renderer.getClearColor(clear);
      try { renderer.setRenderTarget(atlas); renderer.autoClear = true; renderer.setClearColor(0x000000, 0); renderer.render(shadowScene, camera); }
      finally { renderer.setRenderTarget(prior); renderer.autoClear = autoClear; renderer.setClearColor(clear, alpha); }
      if (warming) count.value = slots;
    },
    // Course change (main.js unloadCourse): the silhouette meshes' render objects go with their dispose events.
    dispose() { if (!atlasReady) atlas.dispose(); for (const e of entries.values()) { for (const m of e.meshes) { m.removeFromParent(); m.dispatchEvent({type: 'dispose'}); } e.material.dispose(); } entries.clear(); },
  };
  return api;
}
