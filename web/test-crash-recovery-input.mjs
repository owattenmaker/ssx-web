// Crash recovery input through the original provider (pad_tick): control8 packs only
// WipeoutRecover = Square.pressed at bit13 (1285B8..1285D8); 12CB68 fills the meter per press.
// Holding Cross must not speed recovery; tapping Square must.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const c = await createCore(), root = 'public/assets/';
const read = (p) => JSON.parse(fs.readFileSync(root + p));
const put = (b) => { const p = c._malloc(b.length); c.HEAPU8.set(b, p); return p; };
const str = (x) => put(new TextEncoder().encode(JSON.stringify(x) + '\0'));
const raw = fs.readFileSync(root + 'ANIMATIONS/animation-packets.bin');
c._init_animation(str(read('ANIMATIONS/animation-packets.json')), str(read('RIDER_ZOE/rider.json')), str(read('ANIMATIONS/initial.json')), put(raw), raw.length);
const mesh = fs.readFileSync(root + 'ARA1/collision.bin'); c._init_world(put(mesh), mesh.length / 4);
c._init_terrain(str(read('ARA1/terrain.json')));
c._init_world_collision(str(read('ARA1/world_collision.json')), put(new TextEncoder().encode(read('ARA1/terrain.json').source_sha256 + '\0')));
c._animation_use_physics(1); c._init_body_terrain(str(read('ARA1/terrain.json')));
const start = read('ARA1/start.json');
const f = (ptr, n) => new Float32Array(c.HEAPF32.buffer, ptr, n).slice();
const padPtr = c._malloc(96);
const CROSS = 10, SQUARE = 11, DUP = 6;
function run(recover) {
  c._reset_pad_history(); c._reset_animation(); c._reset_rider(...start.position, start.heading);
  let entered = -1, exited = -1, presses = 0, maxMeter = 0;
  for (let tick = 0; tick < 1500; tick++) {
    const crashing = !!f(c._crash_info(), 12)[0];
    const pad = new Float32Array(24);
    if (entered < 0) { if (tick > 90 && tick < 150) pad[CROSS] = 1; if (tick >= 150) pad[DUP] = 1; }
    else if (crashing) {
      if (recover === 'cross') pad[CROSS] = 1;
      if (recover === 'square' && (tick - entered) % 8 < 4) pad[SQUARE] = 1;
    }
    c.HEAPF32.set(pad, padPtr >> 2);
    const cmd = f(c._pad_tick(padPtr), 24);
    if (cmd[14] === 8 && cmd[20]) presses++;
    c._race_begin();
    const s = f(c._step_rider(cmd[0], cmd[6], cmd[2] ? 1 : 0, cmd[7]), 16);
    c._animation_tick(s[7], cmd[10], cmd[11], s[9], s[8], cmd[6], cmd[8], cmd[7], cmd[7], 0, s[15], cmd[13]);
    c._race_end();
    const info = f(c._crash_info(), 12);
    if (info[0] && entered < 0) entered = tick;
    if (info[0]) maxMeter = Math.max(maxMeter, info[7]);
    if (entered >= 0 && !info[0] && exited < 0) { exited = tick; break; }
  }
  return { recover, entered, exited, duration: exited - entered, presses, maxMeter };
}
const none = run('none'), cross = run('cross'), square = run('square');
console.log({ none, cross, square });
assert(none.entered > 0 && cross.entered === none.entered && square.entered === none.entered, 'flip landing must crash identically');
assert.equal(cross.presses, 0, 'Cross is not part of the control8 command word');
assert.equal(cross.duration, none.duration, 'holding Cross changed crash recovery');
assert(square.presses > 0, 'Square taps must reach the control8 WipeoutRecover bit');
assert(square.duration < none.duration, 'tapping Square must shorten the crash');
console.log('Crash recovery follows WipeoutRecover (Square.pressed), not held jump.');
