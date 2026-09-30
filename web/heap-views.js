// Whole-heap views of a core, reused across frames (docs/web-render-performance.md "Per-frame garbage"): a frame that
// read a core export through `new Uint32Array(core.HEAPU8.buffer, ...)` allocated a typed-array object per read, and the
// set-piece / halo / crowd / weather updates did that dozens of times a frame. The emscripten module keeps HEAPU8 and
// HEAPF32 current after a memory growth; the Uint32 / Int32 views here are rebuilt when the buffer changes, so a read
// after any core call sees the same bytes a fresh view would.
const u32 = new WeakMap(), i32 = new WeakMap();
export function heapU32(core) {
  const buffer = core.HEAPU8.buffer; let view = u32.get(core);
  if (view === undefined || view.buffer !== buffer) { view = new Uint32Array(buffer); u32.set(core, view); }
  return view;
}
export function heapI32(core) {
  const buffer = core.HEAPU8.buffer; let view = i32.get(core);
  if (view === undefined || view.buffer !== buffer) { view = new Int32Array(buffer); i32.set(core, view); }
  return view;
}
// attribute.clearUpdateRanges(); attribute.addUpdateRange(start, count) without a new range object per call: three's backends
// (WebGPU and WebGL updateAttribute) read each range's start / count once and then clear the list, so one record per attribute
// can be reused. Marks the attribute for upload.
const ranges = new WeakMap();
export function setUpdateRange(attribute, start, count) {
  let r = ranges.get(attribute); if (r === undefined) { r = {start: 0, count: 0}; ranges.set(attribute, r); }
  r.start = start; r.count = count; attribute.updateRanges.length = 0; attribute.updateRanges.push(r); attribute.needsUpdate = true;
}
