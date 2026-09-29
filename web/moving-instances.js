// Draw batches of instances that follow a runtime entity matrix (crashbag RollerModifier,
// chairlift MultiSplineModifier cars; core moving_instances()): world vertices are baked with the
// authored instance matrix, so the core returns the three.js-space delta (entity matrix *
// authored^-1, bottom row 0 0 0 1) per active instance under key = resource + copy * 2^20. Copy 0
// moves the authored batches; copies > 0 (chairlift cars 1.., clone instances of the original)
// draw clones of the authored batches. The static meshes sit at position -origin, so the origin
// shift is applied to the translation column.
const COPY = 1048576;
const copies = new WeakMap(); // authored mesh -> Map(copy -> clone mesh)
const offsets = new WeakMap(); // core -> reusable resource/copy -> heap offset index
function place(mesh, delta, origin, offset = 0) {
  mesh.matrixAutoUpdate = false;
  const e = mesh.matrix.fromArray(delta, offset).elements;
  e[12] -= origin.x; e[13] -= origin.y; e[14] -= origin.z;
  mesh.matrixWorldNeedsUpdate = true;
}
export function movingDeltas(core) {
  const deltas = new Map();
  if (!core._moving_instances) return deltas;
  const pointer = core._moving_instances();
  const count = core.HEAPF32[pointer >> 2];
  const values = new Float32Array(core.HEAPF32.buffer, pointer + 4, count * 17);
  for (let i = 0; i < count; i++) deltas.set(values[i * 17], values.slice(i * 17 + 1, i * 17 + 17));
  return deltas;
}
export function updateMovingInstances(core, meshes, origin) {
  if (!meshes.length || !core._moving_instances) return;
  // The draw consumes these matrices synchronously, before another core call. Index
  // the live heap instead of allocating a copied 16-float array for every car.
  let deltas = offsets.get(core);
  if (!deltas) { deltas = new Map(); offsets.set(core, deltas); }
  deltas.clear();
  const pointer = core._moving_instances(), values = core.HEAPF32, at = pointer >> 2;
  for (let i = 0; i < values[at]; i++) deltas.set(values[at + 1 + i * 17], at + 2 + i * 17);
  for (const mesh of meshes) {
    const resource = mesh.userData.movingResource;
    const delta = deltas.get(resource);
    if (delta === undefined) {
      if (!mesh.matrixAutoUpdate) { mesh.matrixAutoUpdate = true; mesh.position.copy(origin).negate(); mesh.quaternion.identity(); mesh.scale.set(1, 1, 1); }
    } else place(mesh, values, origin, delta);
    let clones = copies.get(mesh);
    for (let copy = 1; deltas.has(resource + copy * COPY) || clones?.has(copy); copy++) {
      const cloneDelta = deltas.get(resource + copy * COPY);
      if (!clones) { clones = new Map(); copies.set(mesh, clones); }
      let clone = clones.get(copy);
      if (!clone && cloneDelta !== undefined && mesh.parent) {
        clone = mesh.clone(); clone.userData = { movingResource: resource, movingCopy: copy };
        mesh.parent.add(clone); clones.set(copy, clone);
      }
      if (!clone) continue;
      clone.visible = cloneDelta !== undefined && mesh.visible;
      if (cloneDelta !== undefined) place(clone, values, origin, cloneDelta);
    }
  }
}
