import assert from 'node:assert/strict';
import fs from 'node:fs';
import createCore from './runtime/core.js';
const core = await createCore();
const bytes = fs.readFileSync('public/assets/ARA1/collision.bin');
const triangles = new Float32Array(bytes.buffer, bytes.byteOffset, bytes.length / 4);
const memory = core._malloc(bytes.length);
core.HEAPF32.set(triangles, memory / 4);
core._init_world(memory, triangles.length);
core._free(memory);
const start = JSON.parse(fs.readFileSync('public/assets/ARA1/start.json'));
const traces = [];
for (const held of [0, 1]) {
  core._reset_rider(...start.position, start.heading);
  const trace = [];
  for (let tick = 0; tick < 600; tick++) {
    const state = new Float32Array(core.HEAPF32.buffer,
      core._step_rider(0, +(tick > 90 && tick < 150), 0, held), 16).slice();
    const boost = new Float32Array(core.HEAPF32.buffer, core._boost_info(), 8).slice();
    assert(boost.every(Number.isFinite));
    assert.equal(boost[0], 0, 'empty meter must not refill itself');
    assert.equal(boost[1], 0, 'empty meter must not grant thrust');
    assert.equal(new Float32Array(core.HEAPF32.buffer, core._physics_info(), 5)[3], 0);
    if (held && tick === 0) assert.equal(boost[7] & 16, 16, 'original press feedback lost');
    trace.push(Array.from(state));
  }
  traces.push(trace);
}
assert.deepEqual(traces[1], traces[0], 'holding boost with an empty meter changed motion');
core._reset_rider(...start.position, start.heading);
assert.equal(new Float32Array(core.HEAPF32.buffer, core._boost_info(), 8)[7], 0);
console.log('Original empty-meter boost: no free acceleration on ground or in air; press feedback and restart verified.');
