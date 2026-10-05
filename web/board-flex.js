// Rider morph parts (pv boardFlex, docs/characters.md "Board flex"): the PS2 board (board_BoardFlex<X>, part file 2, 8 morphs) and the
// race hands (HandsX, file 7, 18 morphs: fists, grab poses) are morph-target parts. 30F2B0 blends their weights from the clip's
// file-2 / file-7 streams with the bones' layer weights into *(geometry+0x3C) (the core's board_morph_weights,
// web/animation_bridge.cpp); the renderer adds sum(w_i x delta_i) to the part's vertices before skinning (web/rider-skinning.js).
// The deltas come from the package's board-flex.json / .bin and hand-morphs.json / .bin (tools/export_board_flex.py).
import { pv } from './pv-flags.js';
import { wardrobeFile } from './wardrobe.js';

// The renderer's weight columns (one shader for every rider): the board's 8, then the hands' 18.
export const MORPH_COLUMNS = [
  { stem: 'board-flex', file: 2, column: 0, count: 8, part: /BoardFlex/i },
  { stem: 'hand-morphs', file: 7, column: 8, count: 18, part: /^([a-z]+_)?Hands[A-Z]?(\.mnf)?$/i }
];
export const MORPH_COLUMN_COUNT = 26;

async function loadPart(root, stem, fetchJson, fetchBuffer) {
  const virtualMeta = wardrobeFile(root + stem + '.json', 'json');
  // an outfit package (web/wardrobe.js raceRoot) is virtual: no files there means a part without morphs
  if (virtualMeta === undefined && root.includes('/WARDROBE/')) return null;
  let meta;
  let bin;
  try {
    meta = virtualMeta !== undefined ? virtualMeta : await fetchJson(root + stem + '.json');
    const virtualBin = wardrobeFile(root + stem + '.bin', 'buffer');
    bin = virtualBin !== undefined ? virtualBin : await fetchBuffer(root + stem + '.bin');
  } catch {
    return null;
  }
  if (!meta || meta.version !== 1 || !(meta.morph_count > 0) || !(meta.vertex_count > 0)) return null;
  if (bin.byteLength !== meta.morph_count * meta.vertex_count * 12) throw Error(`${root}${stem}.bin: unexpected size`);
  if (meta.mirror?.length !== meta.morph_count) throw Error(`${root}${stem}.json: mirror table`);
  return { meta, deltas: new Float32Array(bin) };
}

// The package's morph parts {parts: [{meta, deltas, column}]}, or null (switch off, a package without them, Sam's own boards).
// rig: the package's rider.json (no morphing part: nothing to fetch). fetchJson / fetchBuffer: the caller's loaders (they
// throw on a missing file).
export async function loadBoardFlex(root, rig, fetchJson, fetchBuffer) {
  if (!pv('boardFlex')) return null;
  const parts = [];
  for (const c of MORPH_COLUMNS) {
    const part = (rig?.parts || []).find((p) => c.part.test(p.resource || p.part || ''));
    if (!(part?.morph_count > 0)) continue;
    const loaded = await loadPart(root, c.stem, fetchJson, fetchBuffer);
    if (!loaded || loaded.meta.file !== c.file || loaded.meta.morph_count > c.count) continue;
    parts.push({ ...loaded, column: c.column });
  }
  return parts.length ? { parts } : null;
}

// After the core's init_animation (which clears the parts): the board's slot bit is the geometry's slot count (source_bone_slot_count)
// + morph index 0 (files 0 and 1 have no morphs); the hands' is the upper-body mask's bit above the slot count (morph_upper_bit:
// rider+0x8C0 holds it, 0x11C298); the mirror table part+0x40. flex.layout: the configured parts, in the core's weight order.
export function configureBoardFlex(core, flex, rig) {
  if (!flex || !core?._board_morph_configure) return false;
  const slots = rig?.source_bone_slot_count;
  if (!Number.isInteger(slots)) return false;
  const put = (mirror) => {
    const pointer = core._malloc(mirror.length);
    core.HEAPU8.set(mirror, pointer);
    return pointer;
  };
  const layout = [];
  // clears the parts; a package without a morphing board adds none here (count 0)
  const board = flex.parts.find((p) => p.meta.file === 2);
  const boardMirror = board ? board.meta.mirror : [0];
  const boardPointer = put(boardMirror);
  try {
    core._board_morph_configure(slots, board ? boardMirror.length : 0, boardPointer);
  } finally {
    core._free(boardPointer);
  }
  if (board) layout.push({ column: board.column, count: board.meta.morph_count });
  const hands = flex.parts.find((p) => p.meta.file === 7);
  const handsBit = hands && core._morph_part_add && core._morph_upper_bit ? core._morph_upper_bit(slots) : -1;
  if (hands && handsBit >= 0) {
    const pointer = put(hands.meta.mirror);
    try {
      if (core._morph_part_add(7, handsBit, hands.meta.morph_count, pointer)) layout.push({ column: hands.column, count: hands.meta.morph_count });
    } finally {
      core._free(pointer);
    }
  }
  flex.layout = layout;
  return layout.length > 0;
}

// Whether the core's morph parts need configuring (a fresh init_animation cleared them).
export function boardFlexUnconfigured(core) {
  return !!core?._board_morph_slot && core._board_morph_slot() < 0;
}

// The drawn pose's weights of every configured part (a view into the core's heap, valid until its next call), or null.
export function boardFlexWeights(core, count) {
  if (!core?._board_morph_weights || core._board_morph_count() !== count) return null;
  const pointer = core._board_morph_weights();
  return pointer ? core.HEAPF32.subarray(pointer >> 2, (pointer >> 2) + count) : null;
}
