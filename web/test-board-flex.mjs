// Board flex (pv boardFlex, docs/characters.md "Board flex"; the weights' gates are ps2-captures boardflex/*).
// 1. Every race package's board-flex.json / .bin and hand-morphs.json / .bin (tools/export_board_flex.py): sizes, the mirror table, the board's place in
//    vertices.bin (its rider.json part), finite deltas.
// 2. The default outfits' wardrobe packages (web/wardrobe.js buildPackage) carry the same board: first vertex, vertex count,
//    mirror table; their GameCube deltas within 1.05 cm of the package's PS2 deltas (the export's measured difference).
// 3. The page: rider-skinning.js applies the deltas before skinning only with the switch, and the human's and the computer
//    riders' cores are configured after init_animation.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import { sourceOf } from './test-source.mjs';
import { Wardrobe, loadWardrobe, assembly, buildPackage } from './wardrobe.js';

const here = fileURLToPath(new URL('.', import.meta.url));
const pub = here + 'public';
const json = (path) => JSON.parse(fs.readFileSync(pub + path));
// BOARD_FLEX_DIR: an export not yet in web/public/assets (tools/export_board_flex.py OUT_DIR), read in its place
const flexDir = process.env.BOARD_FLEX_DIR || pub + '/assets';
const flexJson = (name, stem = 'board-flex') => JSON.parse(fs.readFileSync(`${flexDir}/${name}/${stem}.json`));
const flexBin = (name, stem = 'board-flex') => fs.readFileSync(`${flexDir}/${name}/${stem}.bin`);
// the morph parts: the board (file 2) and the race hands (file 7)
const STEMS = [['board-flex', 2, /BoardFlex/i], ['hand-morphs', 7, /^([a-z]+_)?Hands[A-Z]?(\.mnf)?$/i], ['special-morphs', 46, /^([a-z]+_)?SpecialA(\.mnf)?$/i]];

// ---- 1. packages ----------------------------------------------------------------------------------------------
const packages = fs.existsSync(pub + '/assets') ? fs.readdirSync(pub + '/assets').filter((d) => d.startsWith('RIDER_')) : [];
const withFlex = [];
for (const name of packages) {
  const root = `/assets/${name}/`;
  for (const [stem, file, pattern] of STEMS) {
    if (!fs.existsSync(`${flexDir}/${name}/${stem}.json`)) continue;
    const meta = flexJson(name, stem);
    const bin = flexBin(name, stem);
    const vertices = fs.statSync(pub + root + 'vertices.bin').size / 40;
    const rig = json(root + 'rider.json');
    assert.equal(meta.version, 1, name);
    assert.equal(meta.file, file, `${name} ${stem}: file`);
    assert.equal(bin.length, meta.morph_count * meta.vertex_count * 12, `${name} ${stem}: .bin size`);
    assert.deepEqual([...meta.mirror].sort((x, y) => x - y), [...Array(meta.morph_count).keys()], `${name} ${stem}: mirror is a permutation`);
    assert(meta.first_vertex + meta.vertex_count <= vertices, `${name} ${stem}: inside vertices.bin`);
    const part = rig.parts.find((p) => pattern.test(p.resource || p.part));
    assert.equal(part?.morph_count, meta.morph_count, `${name} ${stem}: rider.json part`);
    const deltas = new Float32Array(bin.buffer, bin.byteOffset, bin.length / 4);
    assert(deltas.every(Number.isFinite), `${name} ${stem}: finite deltas`);
    withFlex.push(`${name}/${stem}`);
  }
}

// ---- 2. wardrobe packages ---------------------------------------------------------------------------------------
const fetcher = async (path) => {
  const file = pub + path;
  if (!fs.existsSync(file)) return { ok: false, status: 404 };
  const data = fs.readFileSync(file);
  return { ok: true, json: async () => JSON.parse(data), arrayBuffer: async () => data.buffer.slice(data.byteOffset, data.byteOffset + data.length), text: async () => String(data) };
};
let wardrobeChecked = 0;
if (fs.existsSync(pub + '/assets/WARDROBE/ZOE/wardrobe.json')) {
  // the outfit riders (cheat skins such as Stretch keep their RIDER_<X> package: wardrobe.js outfitState)
  for (const id of ['zoe', 'mac', 'kaori', 'psymon', 'allegra']) {
    const name = `RIDER_${id.toUpperCase()}`;
    if (!withFlex.some((x) => x.startsWith(name + '/'))) continue;
    const w = await loadWardrobe(id, fetcher);
    const pkg = buildPackage(w, assembly(w, new Wardrobe(w).raceEquipped()), { riderId: id });
    for (const [stem] of STEMS) {
      if (!withFlex.includes(`${name}/${stem}`)) continue;
      const meta = flexJson(name, stem);
      const got = pkg.morphParts[stem]?.meta;
      assert(got, `${id}: the wardrobe package has ${stem}`);
      assert.equal(got.file, meta.file, `${id} ${stem}: file`);
      assert.equal(got.first_vertex, meta.first_vertex, `${id} ${stem}: first vertex`);
      assert.equal(got.vertex_count, meta.vertex_count, `${id} ${stem}: vertex count`);
      assert.deepEqual(got.mirror, meta.mirror, `${id} ${stem}: mirror`);
      // the wardrobe's slot bit (geometryMasks) is the live geometry's (the export reads it from the rider's savestate)
      if (Number.isInteger(meta.slot_bit)) assert.equal(got.slot_bit, meta.slot_bit, `${id} ${stem}: slot bit`);
      const bin = flexBin(name, stem);
      const ps2 = new Float32Array(bin.buffer, bin.byteOffset, bin.length / 4);
      let worst = 0;
      for (let i = 0; i < ps2.length; i++) worst = Math.max(worst, Math.abs(ps2[i] - pkg.morphParts[stem].bin[i]) * 100);
      assert(worst < 1.05, `${id} ${stem}: the wardrobe's GameCube deltas within ${worst.toFixed(2)} cm of the PS2's`);
    }
    wardrobeChecked++;
  }
}

// ---- 3. page wiring ---------------------------------------------------------------------------------------------
const skinning = sourceOf('rider-skinning.js');
assert(/const boardFlex=pv\('boardFlex'\)\?createBoardFlex\(flex,count\):null;/.test(skinning), 'flex nodes only with the switch');
assert(/boardFlex\.morphed\(point,0\)/.test(skinning) && /boardFlex\.morphed\(point,1\)/.test(skinning), 'each palette with its own weights');
const main = sourceOf('main.js');
const inits = main.match(/core\._init_animation\([^;]*\);configureBoardFlex\(core,riderBoardFlex\.get\(rig\),rig\);/g) || [];
assert.equal(inits.length, 2, 'the human core is configured after each init_animation');
assert(/configureBoardFlex\(core,entry\.boardFlex\.flex/.test(sourceOf('opponent-riders.js')), 'computer riders configured at capture');
console.log(`board flex: ${withFlex.length} morph parts in the packages, ${wardrobeChecked} wardrobe packages checked`);
