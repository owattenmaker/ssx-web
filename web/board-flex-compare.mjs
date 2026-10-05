// TICK_HOOK observer (compare-ps2-capture.mjs) for the rider morph parts (docs/characters.md "Board flex"): the core's morph weights
// (board_morph_weights, web/animation_bridge.cpp) against the PS2's *(geometry+0x3C), which 30F2B0 blended in the same tick: the
// board (file 2), the race hands (file 7) and Stretch's SpecialA (file 46), each the parts the capture's live geometry has. The
// capture watches the human's weight array (local/board-flex/recapture.py, tools/ps2_capture.py --watch WEIGHTS:256). Record i+1
// holds the state tick i left, as for every other field.
// Summary: boardFlex / handMorphs / specialMorphs {ticks, exact, nonzero, first: {tick, web, ps2}} or {skipped}.
import fs from 'node:fs';
import { execFileSync } from 'node:child_process';

// The capture's human geometry, read from its baseline savestate (*(rider+0x780)): the weights pointer +0x3C and every active morph
// part (part+0x18 active, +0x4C count): its file, slot bit (+0x10 slot count + part+0xC morph index), weight offset part+0x8 and
// mirror table part+0x40. Snow Jam Zoe: weights 0x5DC000; board (file 2) bit 29 +0, hands (file 7) bit 31 +44.
const GEOMETRY_SCRIPT = [
  'import sys,zipfile,struct,json',
  'ee=zipfile.ZipFile(sys.argv[1]).read("eeMemory.bin")',
  'w=lambda a: struct.unpack_from("<I",ee,a&0x1ffffff)[0]',
  'g=w(int(sys.argv[2],16)+0x780)',
  'parts=[]',
  'for i in range(w(g+8)):',
  ' p=w(g+0xC)+i*0x58',
  ' n=w(p+0x4C)',
  ' if n and w(p+0x18) and w(p+0xC)<0x80000000:',
  '  parts.append(dict(file=w(p),bit=w(g+0x10)+w(p+0xC),offset=w(p+8),mirror=[w(w(p+0x40)+4*k) for k in range(n)]))',
  'print(json.dumps(dict(weights=w(g+0x3C),parts=parts)))'
].join('\n');
function geometryOf(manifest) {
  return JSON.parse(execFileSync('python3', ['-c', GEOMETRY_SCRIPT, manifest.baseline, manifest.rider], { encoding: 'utf8' }));
}
const KEYS = { 2: 'boardFlex', 7: 'handMorphs', 46: 'specialMorphs' };

export function create({ core, dv, RECORD, captureManifest }) {
  const watches = captureManifest.layout?.watches || [];
  const geometry = geometryOf(captureManifest);
  const WEIGHTS = geometry.weights;
  let watchAt = -1;
  let watchLength = 0;
  let offset = 0;
  for (const w of watches) {
    if (Number(w.address) === WEIGHTS) {
      watchAt = offset;
      watchLength = w.length;
    }
    offset += w.length;
  }
  const allKeys = Object.values(KEYS);
  if (watchAt < 0 || !core._board_morph_configure) {
    const why = watchAt < 0 ? `the capture does not watch 0x${WEIGHTS.toString(16)}` : 'the core has no board_morph_configure';
    return { tick() {}, summary: () => Object.fromEntries(allKeys.map((k) => [k, { skipped: why }])) };
  }
  // the board first (board_morph_configure clears the parts), then the others in file order, as web/board-flex.js configures them
  const ordered = [...geometry.parts].sort((x, y) => (x.file === 2 ? -1 : y.file === 2 ? 1 : x.file - y.file));
  const parts = ordered.filter((p) => KEYS[p.file] && (p.file === 2 || core._morph_part_add));
  // a part past the watched window cannot be compared (Stretch's SpecialA is at +81: a 0x100-byte watch ends at +64)
  for (const p of parts) if ((p.offset + p.mirror.length) * 4 > watchLength) throw new Error(`file-${p.file} weights +${p.offset} lie past the ${watchLength}-byte watch`);
  const put = (mirror) => {
    const pointer = core._malloc(mirror.length);
    core.HEAPU8.set(mirror, pointer);
    return pointer;
  };
  for (const part of parts) {
    const pointer = put(part.mirror);
    if (part.file === 2) core._board_morph_configure(part.bit, part.mirror.length, pointer);
    else {
      // the page takes the hands' bit from the upper-body mask: it must be the live geometry's
      if (part.file === 7 && core._morph_upper_bit) {
        const slots = geometry.parts.find((p) => p.file === 2)?.bit;
        if (slots !== undefined && core._morph_upper_bit(slots) !== part.bit) throw new Error(`hands morph bit ${core._morph_upper_bit(slots)}, the PS2's is ${part.bit}`);
      }
      core._morph_part_add(part.file, part.bit, part.mirror.length, pointer);
    }
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
        row[KEYS[part.file]] = { web, ps2 };
      });
      if (log) log.push(row);
    },
    summary() {
      if (log) fs.writeFileSync(process.env.BOARD_FLEX_LOG, JSON.stringify(log));
      const out = {};
      parts.forEach((part, p) => {
        out[KEYS[part.file]] = stats[p];
      });
      for (const k of allKeys) if (!out[k]) out[k] = { skipped: 'no such morph part in this geometry (or the core cannot add it)' };
      return out;
    }
  };
}
