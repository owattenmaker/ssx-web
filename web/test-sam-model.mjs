// Sam's browser package keeps the original riders' conventions (tools/sam_mesh.py, sam_character/model/README.md):
// roster-size textures, stock lighting path (no authored vertex tint), the unchanged gameplay rig, hair on the
// secondary-motion bones, and every part assigned to a PS2 equipment family.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const root = new URL('public/assets/', import.meta.url);
const json = (p) => JSON.parse(fs.readFileSync(new URL(p, root)));
const sam = json('RIDER_SAM/rider.json'), world = json('RIDER_SAM/world.json'), zoe = json('RIDER_ZOE/world.json');
for (const [key, t] of Object.entries(world.textures)) {
  assert(t.width <= 256 && t.height <= 256, `${key} exceeds the roster texture size`);
  assert(Number.isInteger(Math.log2(t.width)) && Number.isInteger(Math.log2(t.height)), `${key} is not a PS2 power-of-two size`);
}
assert(Object.keys(world.textures).length <= Object.keys(zoe.textures).length, 'more maps than an original rider');
const colors = new Float32Array(fs.readFileSync(new URL('RIDER_SAM/colors.bin', root)).buffer.slice(0));
assert(colors.every((v) => v === 1), 'authored vertex tint would route Sam through a non-stock lighting path');
assert.equal(sam.bones.length, 26); assert(sam.bones.every((b) => !Object.keys(b).some((k) => k.startsWith('source_'))), 'gameplay rig changed');
const hair = sam.parts.find((p) => p.name === 'hair_clumps');
const swaying = sam.skin.slice(hair.first_vertex, hair.first_vertex + hair.vertex_count).filter((g) => g.some(([b]) => b === 24 || b === 25)).length;
assert(swaying > hair.vertex_count / 2, 'hair clumps are not on the sec_dangle secondary-motion bones');
const families = new Set(['top', 'bottom', 'boots', 'hands', 'head', 'hair', 'bindings', 'board', 'back']);
assert(sam.parts.every((p) => families.has(p.ps2_family)), 'part without a PS2 equipment family');
assert(sam.parts.some((p) => p.name === 'Sam_head' && p.material === 2), 'sculpted head uses its own head map');
const verts = fs.statSync(new URL('RIDER_SAM/vertices.bin', root)).size / 40;
assert.equal(sam.source_skin.length, verts);
console.log('Sam package:', { vertices: verts, triangles: fs.statSync(new URL('RIDER_SAM/indices.bin', root)).size / 12, maps: Object.values(world.textures).map((t) => `${t.width}x${t.height}`).join(' '), hairSwayVertices: swaying });
