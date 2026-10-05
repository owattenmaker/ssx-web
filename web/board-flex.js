// Board flex (pv boardFlex, docs/characters.md "Board flex"): the PS2 board, board_BoardFlex<X> (part file 2), is a morph-target
// part. 30F2B0 blends its 8 weights from the clip's file-2 stream with the bones' layer weights into *(geometry+0x3C) (the core's
// board_morph_weights, web/animation_bridge.cpp); the renderer adds sum(w_i x delta_i) to the board vertices before skinning
// (web/rider-skinning.js). The deltas come from the package's board-flex.json / board-flex.bin (tools/export_board_flex.py).
import { pv } from './pv-flags.js';
import { wardrobeFile } from './wardrobe.js';

// The package's board morph targets, or null (switch off, a package without them, Sam's boards).
// rig: the package's rider.json (no morphing board part: nothing to fetch). fetchJson / fetchBuffer: the caller's loaders (they
// throw on a missing file).
export async function loadBoardFlex(root, rig, fetchJson, fetchBuffer) {
  if (!pv('boardFlex')) return null;
  const board = (rig?.parts || []).find((p) => /BoardFlex/i.test(p.resource || p.part || ''));
  if (!(board?.morph_count > 0)) return null;
  const virtualMeta = wardrobeFile(root + 'board-flex.json', 'json');
  // an outfit package (web/wardrobe.js raceRoot) is virtual: no board-flex files there means a board without morphs
  if (virtualMeta === undefined && root.includes('/WARDROBE/')) return null;
  let meta;
  let bin;
  try {
    meta = virtualMeta !== undefined ? virtualMeta : await fetchJson(root + 'board-flex.json');
    const virtualBin = wardrobeFile(root + 'board-flex.bin', 'buffer');
    bin = virtualBin !== undefined ? virtualBin : await fetchBuffer(root + 'board-flex.bin');
  } catch {
    return null;
  }
  if (!meta || meta.version !== 1 || !(meta.morph_count > 0) || !(meta.vertex_count > 0)) return null;
  if (bin.byteLength !== meta.morph_count * meta.vertex_count * 12) throw Error(`${root}board-flex.bin: unexpected size`);
  if (meta.mirror?.length !== meta.morph_count) throw Error(`${root}board-flex.json: mirror table`);
  return { meta, deltas: new Float32Array(bin) };
}

// After the core's init_animation (which clears it): the board's morph slot bit is the geometry's slot count (source_bone_slot_count)
// + morph index 0 (files 0 and 1 have no morphs), the mirror table part+0x40.
export function configureBoardFlex(core, flex, rig) {
  if (!flex || !core?._board_morph_configure) return false;
  const slots = rig?.source_bone_slot_count;
  if (!Number.isInteger(slots)) return false;
  const count = flex.meta.morph_count;
  const pointer = core._malloc(count);
  try {
    core.HEAPU8.set(flex.meta.mirror, pointer);
    core._board_morph_configure(slots, count, pointer);
  } finally {
    core._free(pointer);
  }
  return true;
}

// The drawn pose's weights (a view into the core's heap, valid until its next call), or null.
export function boardFlexWeights(core, count) {
  if (!core?._board_morph_weights || core._board_morph_count() !== count) return null;
  const pointer = core._board_morph_weights();
  return pointer ? core.HEAPF32.subarray(pointer >> 2, (pointer >> 2) + count) : null;
}
