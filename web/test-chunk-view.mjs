// web/chunk-view.js (pv taPodiumWorld): the texture-chunk streamer's steady state at the time-attack podium's NIS eye.
// - On every course with podium steps (CUTSCENES/locators.json), the chunk of its podium interior (mdl_<LOC>_eb_podium_int, the finish
//   stage) is resident in the steady state at the podium locator, whether or not it is resident at the race start.
// - Snow Jam: the steady set at the podium equals what the core's own streamer (section_gameplay.inc ChunkStreaming) held near the
//   finish of a ridden Single Event (headless Chrome, tuck pad, tick 14000: 25 27 28 34 and the locked 0 6 33 35 36), and at the start
//   gate it keeps the start's chunks (5 30 31 32) and drops the finish's.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import { chunkModel, requestedChunks, steadyResidency } from './chunk-view.js';

const assets = new URL('public/assets/', import.meta.url).pathname;
const json = (f) => JSON.parse(fs.readFileSync(assets + f, 'utf8'));
const locators = json('CUTSCENES/locators.json').locations;
const sorted = (set) => [...set].sort((a, b) => a - b);
let courses = 0;
let notAtStart = 0;
for (const [code, loc] of Object.entries(locators)) {
  const podium = loc.locators?.['0'];
  if (!podium?.podium_steps) continue;
  const file = `${code}/SECTIONS/sections.json`;
  if (!fs.existsSync(assets + file)) continue;
  const doc = json(file);
  const model = chunkModel(doc);
  const stage = doc.instances.find((i) => /_eb_podium_int$/.test(i.name ?? ''));
  assert.ok(stage, `${code}: no podium interior instance`);
  const chunk = stage.chunk[1];
  const ids = doc.chunks.map((c) => c.id);
  const steady = steadyResidency(model, podium.pos, ids);
  assert.equal(steady.get(chunk), true, `${code}: the podium interior's chunk ${chunk} is not resident at the podium`);
  for (const id of model.locked) if (steady.has(id)) assert.equal(steady.get(id), true, `${code}: locked chunk ${id} dropped`);
  if (!doc.chunks.find((c) => c.id === chunk)?.resident_at_start) notAtStart++;
  courses++;
}
assert.equal(courses, 14, 'the 14 courses with podium steps');

// Snow Jam against the core's streamer near the finish (measured)
const ara1 = json('ARA1/SECTIONS/sections.json');
const model = chunkModel(ara1);
const at = locators.ARA1.locators['0'].pos;
assert.deepEqual(sorted(requestedChunks(model, at)), [25, 27, 28, 34]);
const steady = steadyResidency(model, at, ara1.chunks.map((c) => c.id));
assert.deepEqual(sorted([...steady].filter(([, on]) => on).map(([id]) => id)), [0, 6, 25, 27, 28, 33, 34, 35, 36]);
const gate = locators.ARA1.locators['1'].pos;
assert.deepEqual(sorted(requestedChunks(model, gate)), [5, 30, 31, 32]);
console.log(`chunk-view: ${courses} podium courses (${notAtStart} with the stage's chunk not resident at the start); Snow Jam = the core's streamer`);
