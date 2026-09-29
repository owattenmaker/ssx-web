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
const terrain = new TextEncoder().encode(fs.readFileSync('public/assets/ARA1/terrain.json', 'utf8') + '\0');
const terrainMemory = core._malloc(terrain.length);
core.HEAPU8.set(terrain, terrainMemory);
core._init_terrain(terrainMemory);
core._free(terrainMemory);
const start = JSON.parse(fs.readFileSync('public/assets/ARA1/start.json'));
core._reset_rider(...start.position, start.heading);

let previousGround = true;
let landingTick = -1;
let airborneTicks = 0;
let landingLimit;
let followingLimit;
let landingSpeedRatio;
const observed = [];
for (let tick = 0; tick < 600; tick++) {
  const state = new Float32Array(core.HEAPF32.buffer,
    core._step_rider(0, +(tick > 90 && tick < 150), 0, 0), 16).slice();
  const info = new Float32Array(core.HEAPF32.buffer, core._physics_info(), 6).slice();
  assert(state.every(Number.isFinite));
  if(!state[8]){const prediction=new Float32Array(core.HEAPF32.buffer,core._prediction_info(),7);assert.equal(prediction[0],0);if(state[10]>0)assert.equal(prediction[6],1,'missing object world must be reported as unavailable, not a fabricated valid landing');}
  assert(info.every(Number.isFinite));
  assert(state[7] <= info[5] + .002, `speed exceeds this tick's source cap at ${tick}`);
  if (!previousGround) {
    // Original 11B3F8 sees the motion mode at frame BEGIN, even on a landing tick.
    assert(Math.abs(info[0] - 33.333335876464844) < .00001);
    airborneTicks++;
  }
  if (state[11] && landingTick < 0) {
    landingTick = tick;
    landingLimit = info[0];
    const resolved = new Float32Array(core.HEAPF32.buffer,core._landing_contact_info(),14).slice();
    const normal = resolved.slice(6,9);
    const penetration = Math.max(0,normal.reduce((sum,n,k)=>sum+n*(resolved[3+k]-resolved[k]),0)-resolved[9]);
    // Source positions are float32 centimeters; at this course origin one ULP
    // is 0.15625mm, larger than the old fixed 0.1mm tolerance. This double-
    // precision projection can differ by one source chop step.
    for(let k=0;k<3;k++){
      const expected=resolved[k]+normal[k]*penetration;
      const ulp=2**(Math.floor(Math.log2(Math.max(Math.abs(expected),1)))-23);
      assert(Math.abs(resolved[10+k]-expected)<=Math.max(.001,ulp),'landing discarded tangential position or material depth');
    }
    const distance = normal.reduce((sum,n,k)=>sum+n*(resolved[10+k]-resolved[3+k]),0);
    assert(Math.abs(new Float32Array(core.HEAPF32.buffer,core._ground_contact_info(),6)[5]-distance)<.01,'landing retains stale contact distance');
    const contact = new Float32Array(core.HEAPF32.buffer, core._landing_info(), 4).slice();
    const predictedFloor=core._height_at(resolved[3]/100,resolved[5]/100+.15,-resolved[4]/100);
    assert(Math.abs(predictedFloor-resolved[5]/100)<.005, 'anticipation height disagrees with authored touchdown point');
    assert.equal(contact[3],3,'landing bypassed the original authored-terrain query');
    assert(contact[2] > 10 && contact[2] < 40, 'fixture must exercise a short original landing');
    landingSpeedRatio = contact[1] / contact[0];
    assert(Math.abs(landingSpeedRatio - .7) < .000002, 'short landing skipped original speed reduction');
    assert(Math.abs(state[7] - contact[1]) < .00001, 'published velocity omits ground-entry response');
  } else if (tick === landingTick + 1 && landingTick >= 0) {
    followingLimit = info[0];
    assert(state[8], 'fixture must remain grounded after landing');
    assert(followingLimit < landingLimit, 'ground cap should begin converging downward');
    assert(followingLimit > landingLimit - 1,
      'material refresh discarded source speed-limit smoothing history');
  }
  previousGround = !!state[8];
  observed.push({tick, speed: state[7], limit: info[0], grounded: !!state[8], surface: info[1]});
}
assert(airborneTicks > 10);
assert(landingTick > 0 && followingLimit !== undefined);
console.log({airborneTicks, landingTick, landingLimit, followingLimit, landingSpeedRatio});
fs.mkdirSync('../local/browser-validation', {recursive:true});
fs.writeFileSync('../local/browser-validation/speed-limits.json', JSON.stringify(observed, null, 2));
