// Original handplant (control 11 + motion 5, docs/handplant-recovery.md) through the production
// pad path: pad_tick -> step_rider -> animation_tick, Zoe from the Snow Jam glide seed. Expected
// words are bit-exact rider states recorded from the original game (ARMSX2 captures
// local/ps2-capture/runs/handplant-*.bin, record tick = seed tick 338 + pad index + 1).
import fs from 'node:fs';
import assert from 'node:assert/strict';
import createCore from './runtime/core.js';
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
const c = await createCore();
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e.stack : c.getExceptionMessage(e)); process.exit(1); });
const put = (bytes) => { const p = c._malloc(bytes.length); c.HEAPU8.set(bytes, p); return p; };
const str = (path) => put(Buffer.concat([read(path), Buffer.from([0])]));
c._init_animation(str('ANIMATIONS/animation-packets.json'), str('RIDER_ZOE/rider.json'), str('ANIMATIONS/initial.json'), put(read('ANIMATIONS/animation-packets.bin')), read('ANIMATIONS/animation-packets.bin').length);
c._init_race(str('ANIMATIONS/initial.json'));
c._animation_use_physics(1);
const mesh = read('ARA1/collision.bin'); c._init_world(put(mesh), mesh.length / 4);
const hash = put(Buffer.from(json('ARA1/terrain.json').source_sha256 + '\0'));
c._init_terrain(str('ARA1/terrain.json')); c._init_world_collision(str('ARA1/world_collision.json'), hash); c._init_body_terrain(str('ARA1/terrain.json'));
c._init_rails(str('ARA1/rails.json'), hash);
const start = json('ARA1/start.json');
const f32 = (ptr, n) => new Float32Array(c.HEAPF32.buffer, ptr, n).slice();
const words = (values) => Array.from(new Uint32Array(Float32Array.from(values).buffer)).map((w) => '0x' + w.toString(16));
const decoder = new TextDecoder(), trickName = () => { const p = c._trick_name(); return decoder.decode(c.HEAPU8.subarray(p, c.HEAPU8.indexOf(0, p))); };

// tools/ps2_capture.py decode_pad: 16 buttons then four stick axes as negative/positive pairs.
const BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown', 'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
const towardZero = (x) => { let r = Math.fround(x); if (Math.abs(r) > Math.abs(x)) { const b = new Float32Array([r]); new Uint32Array(b.buffer)[0] -= 1; r = b[0]; } return r; };
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
function pad(seg) {
  const v = new Float32Array(24), held = new Set(seg.buttons || []);
  BUTTONS.forEach((n, i) => { const on = held.has(n); v[i] = i >= 4 ? towardZero((on ? 255 : 0) * R255) : (on ? 1 : 0); });
  const byte = (x) => Math.floor((Math.max(-1, Math.min(1, x)) + 1) * 127.5 + 0.5);
  [byte(seg.rx || 0), byte(-(seg.ry || 0)), byte(seg.lx || 0), byte(-(seg.ly || 0))].forEach((b, a) => {
    v[16 + 2 * a] = towardZero(Math.max(Math.trunc((79 - b) * 255 / 79), 0) * R255); v[17 + 2 * a] = towardZero(Math.max(Math.trunc((b - 176) * 255 / 79), 0) * R255);
  });
  return v;
}
const padPtr = c._malloc(96);
// Runs a capture script; returns per-record-tick state after the tick that produced it.
function run(segments, frames) {
  c._reset_animation(); c._reset_race(); c._reset_rider(...start.position, start.heading); c._reset_pad_history();
  const entries = []; let end = 0; for (const s of segments) { end += s.frames; entries.push({ end, v: pad(s) }); }
  const out = new Map();
  for (let i = 0; i < frames; i++) {
    c.HEAPF32.set((entries.find((e) => i < e.end) || entries[entries.length - 1]).v, padPtr >> 2);
    const o = f32(c._pad_tick(padPtr), 24);
    c._race_begin(); const s = f32(c._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16);
    c._animation_tick(s[7], o[10], o[11], s[9], s[8], o[6], o[8], o[12], o[7], 0, s[15], o[13]);
    const pose = f32(c._pose_physical(), 12); assert(f32(c._step_camera_head(pose[9], pose[10], pose[11]), 9).every(Number.isFinite)); c._race_end();
    const m = f32(c._reference_motion(), 20), hp = f32(c._handplant_info(), 24), anim = f32(c._animation_info(), 19);
    out.set(338 + i + 1, { pos: words(m.slice(0, 3)), vel: words(m.slice(3, 6)), ground: m[10], control: m[11], active: hp[0], phase: hp[1], motion: hp[2], side: hp[3], fast: hp[4], rail: hp[5], roll: hp[6], lean: hp[7], attempts: hp[10], entries: hp[11], launches: hp[12], attaches: hp[13], failedCruise: hp[14], scoreKind: hp[15], exitPhase: hp[18], entryControl: hp[23], semantic: anim[0], name: trickName() });
  }
  return out;
}
const approach = [{ frames: 20 }, { frames: 90, lx: 0.6 }, { frames: 80 }, { frames: 30, lx: 1 }];
const expectState = (rows, tick, pos, vel, label) => {
  const r = rows.get(tick);
  assert.deepEqual(r.pos, pos, `${label}: position at tick ${tick}`);
  assert.deepEqual(r.vel, vel, `${label}: velocity at tick ${tick}`);
};

// 1. Cruise entry (0x131620), held balance, reflect exit (phase 5), airborne exit clip, landing.
{
  const rows = run([...approach, { frames: 80, buttons: ['Circle'] }, { frames: 400 }], 360);
  assert(rows.get(569).failedCruise > 0, 'Circle near the fence must first fail the 0x107578 test in cruise');
  assert.equal(rows.get(569).control, 0);
  expectState(rows, 569, ['0xc809d544', '0x4656c212', '0xc865293f'], ['0xc4a684f0', '0x44b6a7e8', '0xc442b120'], 'failed cruise attempts');
  const entry = rows.get(570);
  assert.equal(entry.control, 11); assert.equal(entry.phase, 1); assert.equal(entry.motion, 5); assert.equal(entry.entryControl, 0);
  assert.equal(entry.side, 1, 'toe-side (HPTS) handplant'); assert.equal(entry.rail, 1, 'fence spline is rail-flagged (0x30003)'); assert.equal(entry.fast, 0);
  assert.equal(entry.semantic, 39, 'HPTS_INTO');
  expectState(rows, 570, ['0xc809db40', '0x465763de', '0xc86526c5'], ['0xc43845b6', '0x449ba7dd', '0x43987046'], 'motion 5 entry');
  assert.equal(rows.get(589).phase, 1); assert.equal(rows.get(590).phase, 2); assert.equal(rows.get(590).semantic, 40, 'kind-9 HPTS_BAL seek');
  assert.equal(rows.get(590).scoreKind, 1, 'held after a slow entry scores kind 1 (Handplant)');
  expectState(rows, 590, ['0xc80a117b', '0x465d1db6', '0xc8651057'], ['0x0', '0x0', '0x0'], 'pinned at the lip');
  let roll = 0; for (let t = 591; t < 639; t++) { assert.equal(rows.get(t).phase, 2); assert(rows.get(t).roll >= roll, 'neutral balance drifts outward'); roll = rows.get(t).roll; }
  assert(roll > 0.25 && roll < 1, 'released with |b| >= 0.25 on a rail spline');
  assert.equal(rows.get(639).phase, 5); assert.equal(rows.get(639).semantic, 43, 'HPTS_EXITBAL_REFLECT'); assert.equal(rows.get(639).exitPhase, 5);
  expectState(rows, 640, ['0xc80a1198', '0x465d01fe', '0xc8650fc2'], ['0xc1dc258b', '0xc3cf2eb8', '0x42f971c6'], 'phase-5 launch (0x139548)');
  assert.equal(rows.get(640).motion, 1); assert.equal(rows.get(640).launches, 1); assert.equal(rows.get(640).control, 11);
  assert.match(rows.get(640).name, /Handplant/, 'launch commits the handplant trick name');
  assert.equal(rows.get(681).control, 4); assert.equal(rows.get(681).active, 0);
  expectState(rows, 681, ['0xc80a15eb', '0x4658dda7', '0xc8654ef0'], ['0xc1bffb33', '0xc3b4acf7', '0xc47e0710'], 'exit clip complete (287, control 4)');
  assert.equal(rows.get(682).ground, 1); assert.equal(rows.get(682).control, 0);
  expectState(rows, 682, ['0xc80a1604', '0x4658c58f', '0xc865532b'], ['0xc38f75fe', '0xc3d41d1f', '0xc40d46be'], 'landing after the handplant');
  console.log(`cruise entry: ${rows.get(569).failedCruise} failed attempts, INTO 570..589, balance to b=${roll.toFixed(3)}, reflect launch 640, landed 682; "${rows.get(640).name.trim()}"`);
}
// 2. Released before INTO completes: quick reflect (phase 3, Handspring).
{
  const rows = run([...approach, { frames: 15, buttons: ['Circle'] }, { frames: 400 }], 330);
  assert.equal(rows.get(590).phase, 3); assert.equal(rows.get(590).scoreKind, 2); assert.equal(rows.get(590).semantic, 41, 'HPTS_TRANS_REFLECT');
  expectState(rows, 591, ['0xc80a1604', '0x465d06b1', '0xc8650f2e'], ['0xc487aec3', '0xc3ac0d94', '0x4383ce38'], 'phase-3 launch');
  assert.match(rows.get(591).name, /Handspring/);
  expectState(rows, 663, ['0xc80b37cf', '0x465747f0', '0xc865bb50'], ['0xc455609c', '0xc3874979', '0xc4d27c65'], 'handspring flight');
  console.log(`handspring: phase 3 at 590, launch 591, control 4 at ${[...rows].find(([, r]) => r.control === 4)[0]}; "${rows.get(591).name.trim()}"`);
}
// 3. Balance input: HandplantBalance pushes b negative (phase 4 through) or past 1 (phase 5).
for (const [lx, phase, pos, vel] of [[1, 4, ['0xc80a115d', '0x465d396d', '0xc8650fc2'], ['0x41dc258b', '0x43cf2eb8', '0x42f971c6']], [-1, 5, ['0xc80a1198', '0x465d01fe', '0xc8650fc2'], ['0xc1dc258b', '0xc3cf2eb8', '0x42f971c6']]]) {
  const rows = run([...approach, { frames: 34, buttons: ['Circle'] }, { frames: 46, buttons: ['Circle'], lx }, { frames: 400 }], 300);
  assert(Math.abs(rows.get(620).lean) > 0.05, 'HandplantBalance drives the kind-9 seek amount rider+0x244');
  assert.equal(rows.get(634).phase, phase); expectState(rows, 635, pos, vel, `lean ${lx} launch`);
  console.log(`lean ${lx}: exit phase ${phase} at 634, lean244 ${rows.get(620).lean.toFixed(3)}`);
}
// 4. Released with |b| < 0.25 on a rail spline: phase 6 exit to rail through 0x106848.
{
  const rows = run([...approach, { frames: 42, buttons: ['Circle'] }, { frames: 400 }], 320);
  assert.equal(rows.get(601).phase, 6); assert.equal(rows.get(601).semantic, 42, 'HPTS_EXITBAL_THROUGH');
  expectState(rows, 602, ['0xc80a14c7', '0x465d2137', '0xc8650fa9'], ['0xc4455b26', '0x4251b491', '0x4314e72a'], 'phase-6 launch along the tangent');
  expectState(rows, 637, ['0xc80a818b', '0x465d94c9', '0xc8653120'], ['0xc42f96ca', '0x423a938c', '0xc43fbb83'], 'before the exit-event rail attach');
  assert.equal(rows.get(638).control, 7); assert.equal(rows.get(638).attaches, 1); assert.equal(rows.get(638).active, 0);
  assert.deepEqual(rows.get(638).vel, ['0xc4513068', '0xc28eb762', '0xc2fd2759'], 'rail attach velocity');
  console.log('rail exit: phase 6 at 601, attached to control 7 at 638');
}
// 5. Natural-air entry (0x12F730) after failed airborne attempts.
{
  const rows = run([{ frames: 20 }, { frames: 90, lx: 0.6 }, { frames: 80 }, { frames: 36, lx: 0.6 }, { frames: 54 }, { frames: 90, buttons: ['Circle'] }, { frames: 300 }], 300);
  expectState(rows, 632, ['0xc80c23c0', '0x46538a55', '0xc865c42f'], ['0xc500e271', '0x43717c48', '0xc3ae3110'], 'natural air with Circle');
  assert.equal(rows.get(633).entryControl, 4);
  expectState(rows, 633, ['0xc80c2c9f', '0x46540869', '0xc865c475'], ['0xc4889043', '0x44728657', '0xc206b756'], 'natural-air entry');
  console.log(`natural air: ${rows.get(633).attempts} attempts, entered at 633`);
}
// 6. Spin (control 5) entry (0x133308) after a charged jump; the 0x134CB0 exit bake runs first.
{
  const rows = run(JSON.parse('[{"frames":20},{"frames":90,"lx":0.6},{"frames":80},{"frames":40,"lx":0.6},{"frames":40},{"buttons":["Cross"],"frames":16},{"buttons":["Circle"],"frames":90},{"frames":214}]'), 300);
  expectState(rows, 627, ['0xc80bf672', '0x4653f86f', '0xc865bcef'], ['0xc5038b74', '0x43978053', '0x43836a70'], 'charged jump');
  assert.equal(rows.get(628).entryControl, 5);
  expectState(rows, 628, ['0xc80bff99', '0x465477a3', '0xc865bade'], ['0xc48d0716', '0x4474cfb4', '0x437de9e8'], 'spin entry');
  console.log('spin: entered from control 5 at 628');
}
console.log('Handplant: cruise/natural-air/spin entries, balance, phases 3/4/5/6 and launches match the original captures.');
