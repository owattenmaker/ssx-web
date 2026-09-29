// Avalanche draw and rumble loop (pv avalanche; docs/avalanche.md "Draw" and "Audio"). The core plays the recorded avalanches
// (web/avalanche_gameplay.inc) and exports, per playing tumbler, avalanche_pieces() [count, then 22 floats: resource, state (1 following
// its tumbler, 2 a type-2 piece at rest), AvaSpline follower, emitter, the 0x2D9C00 matrix (16, source rows), alpha, scale; then
// [n released since the last call, resources...]; then the loop [refcount (audio+0x6040), n changes since the last call, the refcount
// after each]] and avalanche_info() [definitions, triggers, ticks, active slots, ...].
//  - The AvaSpline pieces' matrices reach the draw through moving_instances() (web/moving-instances.js; their batches are split by
//    prepare.py): here only their visibility. A piece drawn at the start (countdown audit 'static') stays drawn until released; a
//    piece hidden at the start (runtime flag bit 0 clear) shows from the trigger's builtin 0 (it gets an entity: it is then in
//    avalanche_pieces) until released. Released (0x2D7DD8 -> entity vt+0x08(3)): gone until a new race.
//  - The rumble (0x29DEF0 / 0x29E4A0 / 0x29E438 / 0x2DA1C0): one voice while the core's loop refcount is > 0, bank slot 8 sound 2, bus 5,
//    positional at the centroid of the tumblers, volume from the listener's (the human rider, rider +0x110) nearest tumbler and the
//    tumblers' average scale; the per-tumbler sound events call the empty stub 0x29E560 and play nothing.
// One reader per core (avalanche_pieces drains its released list): the draw and the audio share the snapshot of the current tick.
const F = Math.fround;
const cache = new WeakMap();

// Snapshot of the core's avalanche state for its current tick (or null without the exports).
export function avalancheState(core) {
  if (!core?._avalanche_pieces || !core._avalanche_info) return null;
  let c = cache.get(core);
  if (!c) { c = { key: -1, pieces: new Map(), tumblers: [], released: new Set(), loop: false, loopEvents: [] }; cache.set(core, c); }
  const I = new Uint32Array(core.HEAPU8.buffer, core._avalanche_info(), 4), key = I[2] * 4096 + I[1]; // ticks, triggers
  if (key === c.key) return c;
  c.key = key; c.pieces.clear(); c.tumblers.length = 0;
  const H = core.HEAPF32; let at = core._avalanche_pieces() >> 2;
  const n = H[at++];
  for (let k = 0; k < n; k++, at += 22) {
    const m = H.subarray(at + 4, at + 20);
    c.pieces.set(H[at], { state: H[at + 1], follower: H[at + 2] !== 0, emitter: H[at + 3] !== 0, alpha: H[at + 20], scale: H[at + 21] });
    c.tumblers.push([m[12], m[13], m[14], H[at + 21]]); // +96 (source cm), +176
  }
  const r = H[at++]; for (let k = 0; k < r; k++) c.released.add(H[at + k]); at += r;
  c.loop = H[at++] > 0; const t = H[at++]; for (let k = 0; k < t; k++) c.loopEvents.push(H[at + k]); // 0x29DEF0 refcounts, in order
  return c;
}
// The loop's refcount changes since the last call (the audio's: a 1 -> 0 -> 1 within one tick stops and restarts the voice).
export function takeLoopEvents(core) { const c = cache.get(core); if (!c?.loopEvents.length) return []; const e = c.loopEvents; c.loopEvents = []; return e; }
// A new race (web/set-pieces-renderer.js reset): the released pieces come back.
export function resetAvalancheState(core) { const c = cache.get(core); if (c) { c.released.clear(); c.key = -1; } }

// 0x2DA1C0 + 0x29E438 (float32 like the EE): {volume 0..127, centroid (source cm) or null} for the listener L (source cm).
export function avalancheRumble(tumblers, L) {
  if (!tumblers.length) return { volume: 0, centroid: null };
  let sx = 0, sy = 0, sz = 0, ss = 0, dmin = 999999;
  for (const [x, y, z, s] of tumblers) {
    const dx = F(L[0] - x), dy = F(L[1] - y), dz = F(L[2] - z), d = F(Math.sqrt(F(F(F(dx * dx) + F(dy * dy)) + F(dz * dz))));
    sx = F(sx + x); sy = F(sy + y); sz = F(sz + z); ss = F(ss + s); if (d < dmin) dmin = d; // the < 225000000 test never rejects
  }
  const n = tumblers.length, scale = F(ss / n);
  let v = Math.trunc(F(F(F(F(100 - F(dmin * F(0.01))) * F(1.27)) * scale) * F(8.466667175292969)));
  if (v >= 128) v = 127; if (!(v > -1)) v = 0;
  return { volume: v, centroid: [F(sx / n), F(sy / n), F(sz / n)] };
}

// The draw side: visibility of the AvaSpline pieces' batches. meshes: the location's world meshes with userData.movingResource
// (in the scene) and its hidden meshes (group.userData.hiddenMeshes: batch.moving_resource, not in the scene until shown).
export function createAvalancheDraw({ core, group, followers }) {
  const byResource = new Map();
  const add = (resource, mesh, hiddenAtStart) => { let e = byResource.get(resource); if (!e) byResource.set(resource, e = { meshes: [], hiddenAtStart }); e.meshes.push(mesh); };
  group.traverse((o) => { if (o.isMesh && followers.has(o.userData.movingResource)) add(o.userData.movingResource, o, false); });
  for (const m of group.userData.hiddenMeshes || []) { const r = m.userData.batch?.moving_resource; if (followers.has(r)) add(r, m, true); }
  const shown = new Map(); // resource -> drawn
  function apply(resource, e, drawn) {
    if (shown.get(resource) === drawn) return; shown.set(resource, drawn);
    for (const m of e.meshes) {
      if (e.hiddenAtStart) { if (drawn && !m.parent) group.add(m); else if (!drawn && m.parent === group) m.removeFromParent(); }
      else m.visible = drawn;
    }
  }
  return {
    pieces: byResource.size,
    update() {
      const s = avalancheState(core); if (!s) return;
      for (const [r, e] of byResource) apply(r, e, !s.released.has(r) && (s.pieces.has(r) || !e.hiddenAtStart));
    },
    reset() { resetAvalancheState(core); for (const [r, e] of byResource) apply(r, e, !e.hiddenAtStart); },
    get state() { const s = cache.get(core); return { pieces: byResource.size, playing: s?.pieces.size ?? 0, released: s?.released.size ?? 0, loop: !!s?.loop }; },
  };
}
