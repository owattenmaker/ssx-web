// The original front-end rider preview packages (tools/export_fe_preview.py -> RIDER_<ID>/fe/, web/fe-preview.js):
// every roster entry has one; the ten riders carry their live FE assembly (NIS head/eyes/hands), bone slots and
// morph counts; the FE clips' streams map onto their bones and morph channels; the FE lighting through the core is
// the IRR.DAT record plus the 389CB8 rim only.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import * as T from 'three';
import createCore from './runtime/core.js';
import { originalRiderBoneInverses } from './rider-bind.js';
import { clipSample, morphWeights, previewRoot } from './fe-preview.js';

const read = (name) => fs.readFileSync(new URL('./public' + name, import.meta.url));
const json = (name) => JSON.parse(read(name));
const riders = json('/assets/riders.json');
const library = json('/assets/ANIMATIONS/library.json');
const samples = new Float32Array(read('/assets/ANIMATIONS/samples.f32').buffer.slice(0));
const clip = (name) => library.clips.find((c) => c.name === name);
const IRR = { 0: 'moby', 1: 'kaori', 2: 'allegra', 3: 'mac', 4: 'zoe', 5: 'griff', 6: 'elise', 7: 'elise', 8: 'elise', 9: 'viggo' };
const core = await createCore();
const put = (bytes) => { const p = core._malloc(bytes.byteLength); core.HEAPU8.set(new Uint8Array(bytes.buffer, bytes.byteOffset, bytes.byteLength), p); return p; };
{ const c = put(Buffer.concat([read('/assets/ARA1/local-lights.json'), Buffer.from([0])])), t = put(Buffer.concat([read('/assets/ARA1/light-tree.json'), Buffer.from([0])])); core._init_rider_lighting(c, t); core._free(c); core._free(t); }

const packHas = (pack, id) => { const d = fs.readFileSync(new URL('./public' + pack, import.meta.url)), n = d.readUInt32LE(8); return JSON.parse(d.toString('utf8', 12, 12 + n)).entries.some((e) => e.id === id); };
const summary = [];
for (const entry of riders) {
  const root = previewRoot(entry);
  const rig = json(root + 'rider.json'), world = json(root + 'world.json');
  const vertices = read(root + 'vertices.bin').byteLength / 40, indices = new Uint32Array(read(root + 'indices.bin').buffer.slice(0)), morphBytes = read(root + 'morphs.bin').byteLength;
  const where = entry.id;
  assert.equal(world.vertex_count, vertices, where);
  assert.equal(rig.skin.length, vertices, where); assert.equal(rig.source_skin.length, vertices, where);
  assert.equal(rig.parts.reduce((n, p) => n + p.vertex_count, 0), vertices, `${where}: parts cover the mesh`);
  for (const p of rig.parts) for (let i = p.first_index; i < p.first_index + p.index_count; i++) assert(indices[i] >= p.first_vertex && indices[i] < p.first_vertex + p.vertex_count, `${where} ${p.part}: index outside its part`);
  for (const t of Object.values(world.textures)) assert(t.pack !== undefined ? packHas(t.pack.startsWith('/') ? t.pack : root + t.pack, t.id) : fs.existsSync(new URL('./public' + root + t.path, import.meta.url)), `${where}: ${t.path ?? t.pack + ' ' + t.id}`);   // rider texture archive (web/texture-archive.js) or PNG file
  let expectedMorphBytes = 0;
  for (const p of rig.parts) { assert.equal(p.morphs.length, p.morph_count, `${where} ${p.part}`); for (const m of p.morphs) { assert.equal(m.offset, expectedMorphBytes); expectedMorphBytes += p.vertex_count * 12; } }
  assert.equal(morphBytes, expectedMorphBytes, `${where}: morphs.bin size`);
  const inverses = originalRiderBoneInverses(rig, 1);
  assert.equal(inverses.length, rig.bones.length, where);
  const fe = rig.fe;
  assert(fe && fe.irradiance?.coefficient_bits?.length === 40 && fe.rim_constants?.length === 5, `${where}: fe block`);
  const parts = rig.parts.map((p) => p.part);
  if (entry.kind === 'rider') {
    // The live FE assembly of characters/<id>/select.p2s: NIS head, eyes and both hands, 36 + 27 + 27 morphs.
    assert.equal(fe.evidence.kind, 'live', where);
    assert.equal(fe.character, entry.character, where);
    assert.equal(fe.irradiance.name, IRR[entry.character], `${where}: IRR record (0x19EE88)`);
    assert.equal(rig.source_bone_slot_count, rig.bones.length, `${where}: every FE bone slot active`);
    assert.deepEqual(fe.root_position_cm, [136, -250, -82], where);
    for (const name of ['HeadA_NIS', 'Eyes_NIS']) assert(parts.includes(name), `${where}: ${name}`);
    const hands = rig.parts.filter((p) => /^Hands._NIS$/.test(p.part)), dummy = rig.parts.filter((p) => /^Dummy._NIS$/.test(p.part)), head = rig.parts.find((p) => p.part === 'HeadA_NIS');
    assert.equal(hands.length, 1, where); assert.equal(dummy.length, 1, where);
    assert.deepEqual([head.file, hands[0].file, dummy[0].file, rig.parts.find((p) => p.part === 'Eyes_NIS').file], [5, 8, 9, 6], where);
    assert.deepEqual([head.morph_count, hands[0].morph_count, dummy[0].morph_count], [36, 27, 27], where);
    // FE clips: every stream of a package file has that file's channel count (bone channels or one per morph).
    for (const name of [fe.clips.idle, fe.clips.cheer, fe.clips.ubertrick]) {
      const c = clip(name); assert(c, `${where}: clip ${name}`);
      for (const p of rig.parts) {
        const stream = c.streams[String(p.file)];
        const boneChannels = Math.max(0, ...rig.bones.filter((b) => b.file === p.file).map((b) => Math.max(b.animation_translation_channel, b.animation_rotation_channel) + 3));
        const expected = p.morph_count || boneChannels;
        if (!expected || p.board) continue;
        // The body, eyes and NIS morph parts are always animated by the idle/cheer (FE_A_CYC, Rider Details, animates the
        // body only); hair and other secondary parts may have no stream (secondary motion).
        if (!stream && (name === fe.clips.ubertrick || ![0, 8, 9].includes(p.file))) continue;   // FE_GEAR_MAC_CYC has no face (5) stream
        assert(stream, `${where} ${name}: no stream for ${p.part} (file ${p.file})`);
        assert.equal(stream.channels, expected, `${where} ${name}: ${p.part} channels`);
        if (p.morph_count) { const w = morphWeights(p, samples, clipSample(c, 0.5, true)); assert.equal(w.length, p.morph_count); assert(w.every(Number.isFinite)); }
      }
    }
    // Lighting: bank = the record (0x389590 x 1.0, modulation 1), + 389CB8 rim at the hips: RGB lanes stay the record.
    const env = put(new Uint32Array(fe.irradiance.coefficient_bits)), view = put(new Uint32Array(fe.view_matrix_bits)), point = put(new Float32Array([147.385, -245.699, 23.174, 1])), constants = put(new Float32Array(fe.rim_constants));
    const out = new Uint32Array(core.HEAPU8.buffer, core._shade_rider_lighting(env, view, point, fe.rim_scale, constants, 0, 0), 40).slice();
    for (let i = 0; i < 40; i++) if (i % 4 < 3) assert.equal(out[i], fe.irradiance.coefficient_bits[i], `${where}: FE lighting RGB lane ${i}`);
    assert(new Float32Array(out.buffer).some((v, i) => i % 4 === 3 && v !== 0), `${where}: rim lane`);
    for (const p of [env, view, point, constants]) core._free(p);
  } else if (entry.kind === 'cheat') {
    assert.equal(fe.evidence.kind, 'rule', where); assert.equal(fe.irradiance.name, 'fe_map', where);
    assert(parts.includes('HeadA_NIS'), `${where}: NIS head`);
  } else {
    assert.equal(entry.id, 'sam'); assert.equal(fe.irradiance.name, 'fe_map');
  }
  summary.push(`${entry.id}:${rig.bones.length}b/${rig.parts.filter((p) => p.morph_count).map((p) => p.morph_count).join('+') || 0}m`);
}
// Kaori, the task's reference: the exact FE assembly of her select.p2s, and a raised peace-sign hand in her idle.
const kaori = json('/assets/RIDER_KAORI/fe/rider.json');
assert.deepEqual(kaori.parts.map((p) => p.resource), ['kaori_TopA.mnf', 'board_BindingsA.mnf', 'board_BoardFlexA.mnf', 'kaori_BottomA.mnf', 'kaori_HeadA_NIS.mnf',
  'kaori_Eyes_NIS.mnf', 'kaori_HandsA_NIS.mnf', 'kaori_DummyA_NIS.mnf', 'kaori_BootsA.mnf', 'kaori_PigtailsA.mnf']);
const hand = kaori.parts.find((p) => p.part === 'HandsA_NIS'), weights = morphWeights(hand, samples, clipSample(clip('FE_GEAR_KAORI_CYC'), 0, true));
assert(weights.filter((w) => Math.abs(w) > 0.5).length >= 3, 'Kaori idle curls fingers (peace sign)');
// Morph units (docs/characters.md "Morph units"): int8 targets are millimetres, so Kaori's face shapes move
// centimetres (jaw ~4.5 cm, blink ~1 cm), not the sub-millimetre shapes of the position-unit reading.
{ const blob = new Float32Array(read('/assets/RIDER_KAORI/fe/morphs.bin').buffer.slice(0)), head = kaori.parts.find((p) => p.part === 'HeadA_NIS');
  const peak = head.morphs.map((m) => { let best = 0; for (let i = 0; i < head.vertex_count; i++) { const o = (m.offset >> 2) + i * 3; best = Math.max(best, Math.hypot(blob[o], blob[o + 1], blob[o + 2])); } return best; });
  assert(Math.max(...peak) > 0.03 && Math.max(...peak) < 0.08, `Kaori face morph peak ${Math.max(...peak)} m`); }
assert.ok(new T.Matrix4());
console.log('FE preview packages:', riders.length, summary.join(' '));
