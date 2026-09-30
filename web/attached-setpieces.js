// Attached set pieces (engine/parent_modifier.hpp; data: LIVECOMP/attached.json from
// tools/export_attached_setpieces.py):
// * ParentModifier (vtable 0x48F508, builtin18): a child instance drawn with its parent entity's node
//   world matrix, row 3 += offset * M (0x357108). Parents that are LiveComp players of
//   web/livecomp-animation.js (Metro-City / Junction searchlightglowa_* on searchlightbasea_* node 3)
//   are evaluated here, bit-exactly (EE/VU float emulation of ee-scalar-float.js);
// * spline LiveComps (the raven flyby's wing flap, the Junction blimp's bob/propellers) and the
//   children of spline LiveComps (blimpad x2, blimplights) run in the core (web/attached_setpieces.inc,
//   export set_piece_attached(): [count, then per entry resource, node (-1 = the whole instance),
//   16 floats: three.js column-major native delta]).
// Draw: every attached resource's batches are split per node (prepare.py livecomp_nodes); apply()
// sets each mesh's matrix to delta (+ the mesh's rest position) like the log teeters.
import { mul, vuAdd, bitsOf, fromBits } from './ee-scalar-float.js';

const rows = (flat) => [0, 1, 2, 3].map((r) => flat.slice(4 * r, 4 * r + 4));
// VU vmulax/vmadday/vmaddaz/vmaddw: v * M (row vector).
const transformRow = (m, c) => [0, 1, 2, 3].map((k) => vuAdd(vuAdd(vuAdd(mul(m[0][k], c[0]), mul(m[1][k], c[1])), mul(m[2][k], c[2])), mul(m[3][k], c[3])));
// 0x357108: child matrix = parent node world with row 3 += offset * M (offset w 0), VU vadd.xyzw.
export function parentMatrix(parentNode, offset) {
  const moved = transformRow(parentNode, offset);
  return [parentNode[0].slice(), parentNode[1].slice(), parentNode[2].slice(), parentNode[3].map((x, k) => vuAdd(x, moved[k]))];
}
function invertAffine(m) {   // rows; double precision (render use)
  const a = m[0], b = m[1], c = m[2], t = m[3];
  const det = a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
  const i = [[(b[1] * c[2] - b[2] * c[1]) / det, (a[2] * c[1] - a[1] * c[2]) / det, (a[1] * b[2] - a[2] * b[1]) / det, 0],
    [(b[2] * c[0] - b[0] * c[2]) / det, (a[0] * c[2] - a[2] * c[0]) / det, (a[2] * b[0] - a[0] * b[2]) / det, 0],
    [(b[0] * c[1] - b[1] * c[0]) / det, (a[1] * c[0] - a[0] * c[1]) / det, (a[0] * b[1] - a[1] * b[0]) / det, 0]];
  i.push([0, 1, 2].map((k) => -(t[0] * i[0][k] + t[1] * i[1][k] + t[2] * i[2][k])).concat(1));
  return i;
}
const mulRows = (a, b) => a.map((r) => [0, 1, 2, 3].map((k) => r[0] * b[0][k] + r[1] * b[1][k] + r[2] * b[2][k] + r[3] * b[3][k]));
const S = [[0.01, 0, 0, 0], [0, 0, -0.01, 0], [0, 0.01, 0, 0], [0, 0, 0, 1]], SI = [[100, 0, 0, 0], [0, 0, 100, 0], [0, -100, 0, 0], [0, 0, 0, 1]];
// Native (three.js, column-major elements) delta taking the baked rest placement to `world` (source rows).
export const nativeDelta = (rest, world) => Float32Array.from(mulRows(mulRows(mulRows(SI, invertAffine(rest)), world), S).flat());
// nativeDelta(rest, parentMatrix(node, offset)) into out (Float32Array 16) without per-call arrays (docs/web-render-performance.md
// "Per-frame garbage"): pre = mulRows(SI, invertAffine(rest)), fixed per parent; the same operations in the same order.
const W3 = new Float64Array(4), B = new Float64Array(16);
function childDeltaInto(pre, node, offset, out) {
  const n0 = node[0], n1 = node[1], n2 = node[2], n3 = node[3], o0 = offset[0], o1 = offset[1], o2 = offset[2], o3 = offset[3];
  for (let k = 0; k < 4; k++) W3[k] = vuAdd(n3[k], vuAdd(vuAdd(vuAdd(mul(n0[k], o0), mul(n1[k], o1)), mul(n2[k], o2)), mul(n3[k], o3)));
  for (let r = 0; r < 4; r++) { const a = pre[r]; for (let k = 0; k < 4; k++) B[4 * r + k] = a[0] * n0[k] + a[1] * n1[k] + a[2] * n2[k] + a[3] * W3[k]; }
  for (let r = 0; r < 4; r++) for (let k = 0; k < 4; k++) out[4 * r + k] = B[4 * r] * S[0][k] + B[4 * r + 1] * S[1][k] + B[4 * r + 2] * S[2][k] + B[4 * r + 3] * S[3][k];
  return out;
}

export class AttachedSetPieces {
  constructor(data, { core = null } = {}) {
    this.data = data; this.core = core;
    this.jsParents = (data?.parents || []).filter((p) => p.parentKind === 'livecomp').map((p) => ({ ...p, rest: rows(p.childMatrix) }));
    this.resources = new Set([...(data?.parents || []).map((p) => p.child), ...(data?.splineLiveComps || []).map((x) => x.resource)]);
    this.frameResource = []; this.frameNode = []; this.frameDelta = []; this.frameCount = 0;
  }
  // apply()'s deltas of the frame, as deltas() without the Map, its string keys or copies: slot k = (resource, node) and a
  // Float32Array kept across frames; lookups scan from the last slot, so a later entry of a key wins as a later Map.set did.
  frameSlot(resource, node) {
    const k = this.frameCount++; if (k === this.frameDelta.length) this.frameDelta.push(new Float32Array(16));
    this.frameResource[k] = resource; this.frameNode[k] = node; return this.frameDelta[k];
  }
  frameFind(resource, node) { for (let k = this.frameCount - 1; k >= 0; k--) if (this.frameResource[k] === resource && this.frameNode[k] === node) return this.frameDelta[k]; return null; }
  frameDeltas(live) {
    this.frameCount = 0;
    for (let i = 0; i < this.jsParents.length; i++) {
      const p = this.jsParents[i], nodes = live?.matrices(p.parent); if (!nodes || !nodes[p.node]) continue;
      childDeltaInto(p.pre ??= mulRows(SI, invertAffine(p.rest)), nodes[p.node], p.offset, this.frameSlot(p.child, -1));
    }
    const core = this.core;
    if (core?._set_piece_attached) {
      const q = core._set_piece_attached() >> 2, F = core.HEAPF32, n = F[q];
      for (let k = 0; k < n; k++) { const at = q + 1 + 18 * k, d = this.frameSlot(F[at], F[at + 1]); for (let i = 0; i < 16; i++) d[i] = F[at + 2 + i]; }
    }
  }
  // Source-space child matrices of the LiveComp-parented children (live: LiveCompAnimation).
  childMatrices(live) {
    const out = new Map();
    if (!live) return out;
    for (const p of this.jsParents) {
      const nodes = live.matrices(p.parent); if (!nodes || !nodes[p.node]) continue;
      out.set(p.child, parentMatrix(nodes[p.node], p.offset));
    }
    return out;
  }
  // key `${resource}:${node}` (node -1 = every mesh of the instance) -> native delta.
  deltas(live) {
    const out = new Map();
    for (const p of this.jsParents) {
      const nodes = live?.matrices(p.parent); if (!nodes || !nodes[p.node]) continue;
      out.set(`${p.child}:-1`, nativeDelta(p.rest, parentMatrix(nodes[p.node], p.offset)));
    }
    const core = this.core;
    if (core?._set_piece_attached) {
      const q = core._set_piece_attached() >> 2, n = core.HEAPF32[q];
      for (let k = 0; k < n; k++) {
        const at = q + 1 + 18 * k;
        out.set(`${core.HEAPF32[at]}:${core.HEAPF32[at + 1]}`, core.HEAPF32.slice(at + 2, at + 18));
      }
    }
    return out;
  }
  // Meshes split per node (mesh.userData.liveComp = [resource, node]); returns the number moved.
  apply(liveMeshes, live) {
    if (!this.resources.size) return 0;
    this.frameDeltas(live); let moved = 0;
    for (const mesh of liveMeshes) {
      const lc = mesh.userData.liveComp, resource = lc[0], node = lc[1];   // integers (world.json livecomp_resource / _node)
      if (!this.resources.has(resource)) continue;
      const delta = this.frameFind(resource, node) ?? this.frameFind(resource, -1);
      if (!delta) continue;
      mesh.matrixAutoUpdate = false; mesh.userData.lcRest = false;   // a player's matrix (pv liveRest: not the rest draw)
      const e = mesh.matrix.fromArray(delta).elements, o = mesh.userData.restPosition ??= mesh.position.clone();
      e[12] += o.x; e[13] += o.y; e[14] += o.z; mesh.matrixWorldNeedsUpdate = true; moved++;
    }
    return moved;
  }
}
export const matrixBits = (m) => m.flat().map(bitsOf);
export const rowsFromBits = (words) => rows(words.map(fromBits));
