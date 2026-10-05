// TICK_HOOK observer (compare-ps2-capture.mjs) for the rider morph parts (docs/characters.md "Board flex"): the core's morph weights
// (board_morph_weights, web/animation_bridge.cpp) against the PS2's *(geometry+0x3C), which 30F2B0 blended in the same tick: the
// board (part file 2, weights 0..7) and, with a core that has morph_part_add, the race hands (file 7, weights 44..61). The capture
// watches the Snow Jam human's weight array (tools/ps2_capture.py build --watch 0x5dc000:256; geometry 0x5DC600). Record i+1
// holds the state tick i left, as for every other field.
// Summary: boardFlex {ticks, exact, nonzero, first: {tick, web, ps2}} for the board; handMorphs the same for the hands; or {skipped}.
import fs from 'node:fs';
import { execFileSync } from 'node:child_process';

// Snow Jam glide's human: geometry 0x5DC600, weights 0x5DC000. Another baseline's human (The Junction's rider 0x14542A0): the
// weights pointer of its own savestate, *(*(rider+0x780)+0x3C) (local/board-flex/recapture.py watches the same address).
const SNOW_JAM_WEIGHTS = 0x5dc000;
function weightsOf(manifest) {
  if (!manifest.baseline || !manifest.rider || !fs.existsSync(manifest.baseline)) return SNOW_JAM_WEIGHTS;
  const script = 'import sys,zipfile,struct\nee=zipfile.ZipFile(sys.argv[1]).read("eeMemory.bin")\n' +
    'w=lambda a: struct.unpack_from("<I",ee,a&0x1ffffff)[0]\nprint(w(w(int(sys.argv[2],16)+0x780)+0x3C))';
  return Number(execFileSync('python3', ['-c', script, manifest.baseline, manifest.rider], { encoding: 'utf8' }).trim());
}
// Snow Jam Zoe's geometry: slot count 29; part 2 (board): morph index 0, weights +0, part+0x40 = [4, 5, 6, 7, 0, 1, 2, 3];
// part 7 (HandsB): morph index 2, weights +44, part+0x40 = [9..17, 0..8]
const SLOT_COUNT = 29;
const PARTS = [
  { key: 'boardFlex', file: 2, offset: 0, mirror: [4, 5, 6, 7, 0, 1, 2, 3] },
  { key: 'handMorphs', file: 7, offset: 44, mirror: [9, 10, 11, 12, 13, 14, 15, 16, 17, 0, 1, 2, 3, 4, 5, 6, 7, 8] }
];

export function create({ core, dv, RECORD, captureManifest }) {
  const watches = captureManifest.layout?.watches || [];
  const WEIGHTS = weightsOf(captureManifest);
  let watchAt = -1;
  let offset = 0;
  for (const w of watches) {
    if (Number(w.address) === WEIGHTS) watchAt = offset;
    offset += w.length;
  }
  if (watchAt < 0 || !core._board_morph_configure) {
    const why = watchAt < 0 ? `the capture does not watch 0x${WEIGHTS.toString(16)}` : 'the core has no board_morph_configure';
    return { tick() {}, summary: () => ({ boardFlex: { skipped: why }, handMorphs: { skipped: why } }) };
  }
  const parts = core._morph_part_add ? PARTS : PARTS.slice(0, 1);
  const put = (mirror) => {
    const pointer = core._malloc(mirror.length);
    core.HEAPU8.set(mirror, pointer);
    return pointer;
  };
  {
    const pointer = put(parts[0].mirror);
    core._board_morph_configure(SLOT_COUNT, parts[0].mirror.length, pointer);
    core._free(pointer);
  }
  for (const part of parts.slice(1)) {
    // the hands' bit from the upper-body mask (rider+0x8C0), as the page takes it (web/board-flex.js)
    const bit = core._morph_upper_bit(SLOT_COUNT);
    if (bit !== 31) throw new Error(`hands morph bit ${bit}, the PS2's is 31`);
    const pointer = put(part.mirror);
    core._morph_part_add(part.file, bit, part.mirror.length, pointer);
    core._free(pointer);
  }
  const base = captureManifest.layout.watch_offset + watchAt;
  const records = dv.byteLength / RECORD;
  const stats = parts.map(() => ({ ticks: 0, exact: 0, nonzero: 0, first: null }));
  const log = process.env.BOARD_FLEX_LOG ? [] : null;
  return {
    tick({ i, tick }) {
      if (i + 1 >= records) return;
      const at = (i + 1) * RECORD + base;
      const pointer = core._board_morph_weights();
      const all = pointer ? core.HEAPF32.subarray(pointer >> 2, (pointer >> 2) + core._board_morph_count()) : null;
      let webAt = 0;
      const row = { tick };
      parts.forEach((part, p) => {
        const n = part.mirror.length;
        const ps2 = Array.from({ length: n }, (_, k) => dv.getFloat32(at + 4 * (part.offset + k), true));
        const web = all ? Array.from(all.subarray(webAt, webAt + n)) : new Array(n).fill(0);
        webAt += n;
        const s = stats[p];
        s.ticks++;
        if (ps2.some((v) => v !== 0)) s.nonzero++;
        // bit-equal (a +0 / -0 difference counts as a mismatch)
        if (web.every((v, k) => Object.is(Math.fround(v), ps2[k]))) s.exact++;
        else if (!s.first) s.first = { tick, web, ps2 };
        row[part.key] = { web, ps2 };
      });
      if (log) log.push(row);
    },
    summary() {
      if (log) fs.writeFileSync(process.env.BOARD_FLEX_LOG, JSON.stringify(log));
      const out = {};
      parts.forEach((part, p) => {
        out[part.key] = stats[p];
      });
      if (!out.handMorphs) out.handMorphs = { skipped: 'the core has no morph_part_add' };
      return out;
    }
  };
}
