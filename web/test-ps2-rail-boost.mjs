import assert from 'node:assert/strict';
import fs from 'node:fs';
import createCore from './runtime/core.js';
const core = await createCore();
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e.stack : core.getExceptionMessage(e)); process.exit(1); });
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
const put = (bytes) => { const p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); return p; };
const str = (path) => put(Buffer.concat([read(path), Buffer.from([0])]));
const meta = str('ANIMATIONS/animation-packets.json'), rig = str('RIDER_SAM/rider.json'), cfg = str('ANIMATIONS/initial.json');
const packets = read('ANIMATIONS/animation-packets.bin');
core._init_animation(meta, rig, cfg, put(packets), packets.length);
core._init_race(cfg);
core._animation_use_physics(1);
const mesh = read('ARA1/collision.bin');
core._init_world(put(mesh), mesh.length / 4);
const terrain = str('ARA1/terrain.json');
const terrainJson = json('ARA1/terrain.json');
const hash = put(Buffer.from(terrainJson.source_sha256 + '\0'));
core._init_terrain(terrain);
core._init_world_collision(str('ARA1/world_collision.json'), hash);
core._init_body_terrain(terrain);
core._init_rails(str('ARA1/rails.json'), hash);
const start = json('ARA1/start.json');
const profile = json('ANIMATIONS/initial.json').original_boost.profile;
const f32 = (ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
const full = core._original_axis(1);
function boot(position = start.position, heading = start.heading) {
  core._reset_animation();
  core._reset_race();
  core._reset_rider(...position, heading);
}
function frame(command = {}) {
  core._ride_command(command.turn || 0, command.jump ? 1 : (command.crouch || 0), command.brake || 0, command.board || 0, command.spin || 0, command.flip || 0, command.jump || 0, command.boost || 0, command.grab || 0, command.apply ?? 1);
  core._rail_shoulder_input(command.grab || 0);
  core._rail_preinput(command.flip || 0);
  core._rail_rotation_input(command.spin || 0);
  core._race_begin();
  const state = f32(core._step_rider(command.turn || 0, command.jump || 0, command.brake ? 1 : 0, command.boost || 0), 16);
  core._animation_tick(state[7], command.turn || 0, command.brake || 0, state[9], state[8], command.jump || 0, command.grab || 0, command.boost || 0, command.boost || 0, 0, state[15], command.flip || 0);
  const boost = Array.from(f32(core._boost_info(), 8));
  const rail = Array.from(f32(core._rail_gameplay_info(), 8));
  const info = Array.from(f32(core._animation_info(), 19));
  const motion = Array.from(f32(core._rider_state(), 16));
  core._race_end();
  return { boost, rail, info, motion };
}
function spend(meter) {
  boot();
  core._award_trick_meter(meter);
  const before = frame({});
  const spent = frame({ boost: 1 });
  let speed = spent.motion[7];
  for (let i = 0; i < 20; i++) speed = frame({ boost: 1 }).motion[7];
  return { amount: spent.boost[1], meterBefore: before.boost[0], speed };
}
const low = spend(0.2);
const mid = spend(0.5);
const high = spend(0.9);
assert(low.meterBefore > 0 && low.meterBefore <= profile.medium_threshold, 'low spend fixture missed the first boost band');
assert(mid.meterBefore > profile.medium_threshold && mid.meterBefore <= profile.full_threshold, 'middle spend fixture missed the second boost band');
assert(high.meterBefore > profile.full_threshold, 'high spend fixture missed the third boost band');
assert.equal(low.amount, 0.25);
assert.equal(mid.amount, 0.625);
assert.equal(high.amount, 1);
assert(low.speed < mid.speed && mid.speed < high.speed, `boost levels did not change speed ${low.speed} ${mid.speed} ${high.speed}`);
boot();
const plain = [];
for (let i = 0; i < 21; i++) plain.push(frame({}));
assert(high.speed > plain.at(-1).motion[7], 'full boost was no faster than coasting');

boot();
core._award_trick_meter(1);
const filled = Array.from(f32(core._boost_info(), 8));
assert.equal(filled[0], 1, 'filling the bar did not reach a full meter');
assert.equal(filled[5], 20, 'a full meter did not enter the original uber timer');
let uberFrames = 0;
let superTime = filled[5];
for (let tick = 0; tick < 1300 && superTime !== 0; tick++) {
  superTime = frame({}).boost[5];
  uberFrames++;
  assert(superTime <= 20, 'uber timer ran upward');
}
assert.equal(superTime, 0, 'uber timer did not end');
assert(uberFrames > 1000 && uberFrames < 1300, `uber duration was ${uberFrames} frames`);

function trickMeter() {
  boot();
  let previous = 0;
  let first = 0;
  for (let pass = 0; pass < 2; pass++) {
    for (let tick = 0; tick < 140; tick++) {
      const jump = tick >= 1 && tick < 30;
      const grab = tick >= 35 && tick < 55 ? 1 : 0;
      const row = frame({ jump: jump ? 1 : 0, crouch: jump && tick > 1 ? 1 : 0, grab });
      if (row.motion[11]) {
        const gained = row.boost[0] - previous;
        if (pass === 0) first = gained;
        else assert(gained < first && gained > 0, `repeat trick filled ${gained}, first filled ${first}`);
        previous = row.boost[0];
        break;
      }
    }
  }
  assert(first > 0, 'first landed trick did not fill the boost meter');
}
trickMeter();

const catalog = json('ARA1/rails.json');
const segment = catalog.rails.find((r) => r.name === 'spline_ARA1_RAIL_3007').segments[0].native;
const delta = segment.end.map((v, i) => v - segment.start[i]);
const length = Math.hypot(delta[0], delta[2]);
const direction = delta.map((v) => v / length);
const spawn = segment.start.map((v, i) => v - direction[i] * 3.5);
spawn[1] = -4132.04 + 0.05;
const heading = Math.atan2(direction[0], direction[2]);
function grind(turn, spin) {
  boot(spawn, heading);
  core._set_rider_velocity(direction[0] * 900, -direction[2] * 900, 0);
  let attached = -1;
  let balance = 0;
  let semantic = 0;
  for (let tick = 0; tick < 80; tick++) {
    const row = frame(tick >= 25 ? { turn, spin } : {});
    if (row.rail[0]) {
      if (attached < 0) attached = tick;
      balance = row.rail[5];
      semantic = row.info[0];
    }
  }
  return { attached, balance, semantic };
}
const center = grind(0, 0);
const left = grind(-full, 0);
const right = grind(full, 0);
assert(center.attached >= 0 && center.attached < 30, 'rail did not attach');
// RailBalance (LStickR-LStickL) reaches 0x113F38 unnegated (0x131E00): stick left slides the rider toward -right, so the
// rail sits to its right and balance = clamp(dot(right, hit-bone)/30) rises (PS2 capture rail-balance-lr).
assert(left.balance > center.balance && right.balance < center.balance, `left-stick balance did not follow the stick ${left.balance} ${center.balance} ${right.balance}`);
const front = grind(0, full);
const back = grind(0, -full);
assert.notEqual(front.semantic, back.semantic, 'd-pad grind rotation did not change side');
assert.notEqual(front.semantic, center.semantic, 'd-pad grind stayed on the neutral cycle');

boot(spawn, heading);
core._set_rider_velocity(direction[0] * 900, -direction[2] * 900, 0);
let leftRail = false;
let sawAir = false;
let sawSnow = false;
// With the original 0x13AF28 entry pull the rider stays on this rail for ~700 ticks.
for (let tick = 0; tick < 1000 && !sawSnow; tick++) {
  const row = frame({});
  if (row.rail[0]) leftRail = true;
  if (leftRail && !row.rail[0]) {
    if (row.motion[8] === 0) sawAir = true;
    if (row.motion[8] === 1 && row.info[15] === 0) sawSnow = true;
  }
}
assert(leftRail && sawAir && sawSnow, `leaving the rail did not return through air to snow air=${sawAir} snow=${sawSnow}`);

boot(spawn, heading);
core._set_rider_velocity(direction[0] * 900, -direction[2] * 900, 0);
let locked = false;
for (let tick = 0; tick < 40; tick++) {
  const row = frame(tick >= 20 ? { grab: 1 } : {});
  locked = locked || row.info[15] === 12;
}
assert.equal(locked, false, 'shoulder uber-rail started without uber');
boot(spawn, heading);
core._set_rider_velocity(direction[0] * 900, -direction[2] * 900, 0);
core._award_trick_meter(1);
let uber = false;
for (let tick = 0; tick < 80; tick++) {
  const row = frame(tick >= 30 ? { grab: 1 } : {});
  uber = uber || row.info[15] === 12;
}
assert.equal(uber, true, 'shoulder uber-rail did not start while uber was active');
console.log('PS2 rail and boost step matches attach, balance, grind side, exit, uber gating, three spend levels, repeat fill, and uber timing.');
