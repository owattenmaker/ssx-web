// The texture-chunk streamer's steady state for one viewer eye (pv taPodiumWorld, docs/big-multiplayer.md "The podium's world and its
// hand-off to the vote"). The core's streamer (engine/section_streaming.hpp ChunkStreaming, 0x3A8290, run every game update from
// 0x2306A8 with the outer camera: eye = +0x20) requests every leaf chunk of the chunk trees whose box is within the range of the eye
// (0x3AA028 traverse, 0x3A9E50 distance), loads the requested ones one at a time and frees the unrequested ones after the timeout
// (chunk_timeout_frames, then chunk_free_frames). Held at one eye, it ends with exactly the requested chunks and the locked ones
// resident. This module computes that set from the same SECTIONS/sections.json trees and range, without the load wait (a finish has
// already streamed the podium in; the time-attack podium plays without one).
// Not the core's float order: the distance is in doubles here (the core rounds like the EE), so a chunk box at exactly the range's
// edge may differ. Draw eligibility only.

// sections.json (parsed) -> { range, locked: Set, trees }
export function chunkModel(doc) {
  const locked = new Set();
  for (const c of doc?.chunks ?? []) if (c.locked) locked.add(c.id);
  for (const t of doc?.chunk_trees ?? []) if (t.locked_chunk) locked.add(t.locked_chunk);
  return { range: doc?.constants?.viewer_range_cm ?? 0, locked, trees: doc?.chunk_trees ?? [] };
}

// 0x3A9E50: -1 inside the box, else the distance to it
function boxDistance(p, lo, hi) {
  let inside = true;
  let s = 0;
  for (let a = 0; a < 3; a++) {
    if (lo[a] <= p[a] && p[a] <= hi[a]) continue;
    inside = false;
    let d = p[a] - hi[a];
    if (d < 0) d = lo[a] - p[a];
    s += d * d;
  }
  return inside ? -1 : Math.sqrt(s);
}

const planeSide = (q, v) => q[0] * v[0] + q[1] * v[1] + q[2] * v[2] + q[3];

// The chunks the eye requests (0x3AA028 over every tree from its root, the box eye -/+ range), PS2 cm in the rider's frame.
export function requestedChunks(model, eye, range = model.range) {
  const out = new Set();
  const lo = [eye[0] - range, eye[1] - range, eye[2] - range];
  const hi = [eye[0] + range, eye[1] + range, eye[2] + range];
  for (const tree of model.trees) {
    const nodes = tree.nodes ?? [];
    if (!nodes.length) continue;
    const stack = [0];
    while (stack.length) {
      const n = nodes[stack.pop()];
      let overlaps = true;
      for (let a = 0; a < 3 && overlaps; a++) overlaps = lo[a] <= n.max[a] && n.min[a] <= hi[a];
      if (!overlaps) continue;
      if (n.chunk >= 0) {
        const d = boxDistance(eye, n.min, n.max);
        // a chunk node out of range ends the walk below it too (0x3AA028 returns there)
        if (d >= 0 && range < d) continue;
        out.add(n.chunk);
      }
      if (n.child_a >= 0 && planeSide(n.plane_a, lo) < 0) stack.push(n.child_a);
      if (n.child_b >= 0 && planeSide(n.plane_b, hi) < 0) stack.push(n.child_b);
    }
  }
  return out;
}

// The steady state: chunk id -> resident, for every chunk the streamer knows
export function steadyResidency(model, eye, chunks) {
  const want = requestedChunks(model, eye);
  const out = new Map();
  for (const id of chunks) out.set(id, model.locked.has(id) || want.has(id));
  return out;
}
