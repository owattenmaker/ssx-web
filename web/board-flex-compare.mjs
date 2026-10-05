// TICK_HOOK observer (compare-ps2-capture.mjs) for board flex (docs/characters.md "Board flex"): the core's board morph weights
// (board_morph_weights, web/animation_bridge.cpp) against the PS2's *(geometry+0x3C)[0..7], the board part's 8 weights that
// 30F2B0 blended in the same tick. The capture watches the Snow Jam human's weight array (tools/ps2_capture.py build --watch
// 0x5dc000:256; geometry 0x5DC600). Record i+1 holds the state tick i left, as for every other field.
// Summary: boardFlex {ticks, exact, nonzero, first: {tick, web, ps2}} or {skipped}.
import fs from 'node:fs';

const WEIGHTS = 0x5dc000;
const COUNT = 8;
// Snow Jam Zoe: geometry +0x10 slot count 29, board morph index 0 (part 2 +0xC), part+0x40 = [4, 5, 6, 7, 0, 1, 2, 3]
const SLOT_BIT = 29;
const MIRROR = [4, 5, 6, 7, 0, 1, 2, 3];

export function create({ core, dv, RECORD, captureManifest }) {
  const watches = captureManifest.layout?.watches || [];
  let watchAt = -1;
  let offset = 0;
  for (const w of watches) {
    if (Number(w.address) === WEIGHTS) watchAt = offset;
    offset += w.length;
  }
  if (watchAt < 0 || !core._board_morph_configure) {
    const why = watchAt < 0 ? 'the capture does not watch 0x5dc000' : 'the core has no board_morph_configure';
    return { tick() {}, summary: () => ({ boardFlex: { skipped: why } }) };
  }
  const mirror = core._malloc(COUNT);
  core.HEAPU8.set(MIRROR, mirror);
  core._board_morph_configure(SLOT_BIT, COUNT, mirror);
  core._free(mirror);
  const base = captureManifest.layout.watch_offset + watchAt;
  const records = dv.byteLength / RECORD;
  let ticks = 0;
  let exact = 0;
  let nonzero = 0;
  let first = null;
  const log = process.env.BOARD_FLEX_LOG ? [] : null;
  return {
    tick({ i, tick }) {
      if (i + 1 >= records) return;
      const at = (i + 1) * RECORD + base;
      const ps2 = Array.from({ length: COUNT }, (_, k) => dv.getFloat32(at + 4 * k, true));
      const pointer = core._board_morph_weights();
      const web = pointer ? Array.from(core.HEAPF32.subarray(pointer >> 2, (pointer >> 2) + COUNT)) : new Array(COUNT).fill(0);
      ticks++;
      if (ps2.some((v) => v !== 0)) nonzero++;
      // bit-equal (a +0 / -0 difference counts as a mismatch)
      const same = web.every((v, k) => Object.is(Math.fround(v), ps2[k]));
      if (same) exact++;
      else if (!first) first = { tick, web, ps2 };
      if (log) log.push({ tick, web, ps2 });
    },
    summary() {
      if (log) fs.writeFileSync(process.env.BOARD_FLEX_LOG, JSON.stringify(log));
      return { boardFlex: { ticks, exact, nonzero, first } };
    }
  };
}
