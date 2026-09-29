import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import createCore from './runtime/core.js';
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
const core = await createCore();
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e.stack : core.getExceptionMessage(e)); process.exit(1); });
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
const f32 = (ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
const full = core._original_axis(1);
if (new Uint32Array(new Float32Array([full]).buffer)[0] !== 0x3f7fffff) throw new Error('full axis ' + full);
function boot() {
  core._reset_animation();
  core._reset_race();
  core._reset_rider(...start.position, start.heading);
}
function frame(command) {
  core._ride_command(command.turn || 0, command.crouch || 0, command.brake || 0, command.board || 0, command.spin || 0, command.flip || 0, command.jump || 0, command.boost || 0, command.grab || 0, command.apply ?? 1);
  core._rail_shoulder_input(command.grab || 0);
  core._rail_preinput(command.flip || 0);
  core._rail_rotation_input(command.spin || 0);
  core._race_begin();
  const state = f32(core._step_rider(command.turn || 0, command.jump || 0, command.brake ? 1 : 0, command.boost || 0), 16);
  core._animation_tick(state[7], command.turn || 0, command.brake || 0, state[9], state[8], command.jump || 0, command.grab || 0, command.boost || 0, command.boost || 0, 0, state[15], command.flip || 0);
  const motion = Array.from(f32(core._reference_motion(), 20));
  const orient = Array.from(f32(core._rider_orientation(), 4));
  const boost = Array.from(f32(core._boost_info(), 8));
  const rail = Array.from(f32(core._rail_gameplay_info(), 8));
  const info = Array.from(f32(core._animation_info(), 19));
  const air = Array.from(f32(core._air_control_info(), 22));
  const pred = Array.from(f32(core._prediction_info(), 19));
  const landing = Array.from(f32(core._landing_info(), 6));
  core._race_end();
  return { state: Array.from(state), motion, orient, boost, rail, info, air, pred, landing };
}
const probe = execFileSync('python3', ['-c', `
import json,sys
from pathlib import Path
sys.path.insert(0,'tools')
import zipfile
from reference_probes import riders
from reference_air_control import extract_air_control
names=['snow-jam-glide.p2s','snow-jam-glide-1.p2s','snow-jam-glide-30-a.p2s','snow-jam-glide-120.p2s','snow-jam-brake-30.p2s','snow-jam-turn-left-29.p2s','snow-jam-turn-left-30.p2s','snow-jam-jump-30.p2s','snow-jam-jump-31.p2s','snow-jam-jump-60.p2s','snow-jam-jump-90.p2s','snow-jam-jump-short-2.p2s','snow-jam-jump-medium-16.p2s','snow-jam-charge-1.p2s','snow-jam-charge-2.p2s','snow-jam-charge-3.p2s','snow-jam-air-spin-30.p2s']
out={}
for name in names:
    mem=zipfile.ZipFile(Path('local/reference/pcsx2')/name).read('eeMemory.bin')
    human=[r for r in riders(mem) if r['kind']=='human']
    assert len(human)==1, name
    h=human[0]
    out[name]={
      'cm':h['position_cm'],
      'vel':h['integration_vector_1e0'],
      'quat':list(h['source_quaternion']),
      'mode':h['motion_mode'],
      'control':h['control_state'],
      'turn':h['controls']['turn_1f0'],
      'brake':h['controls']['brake_214'],
      'crouch':h['controls']['auxiliary_220'],
      'boost':None if not h['original_boost'] else {k:h['original_boost']['state'][k] for k in ('meter','amount','tier','super_time')},
      'air':extract_air_control(mem)['state'] if h['control_state']==5 else None,
    }
print(json.dumps(out))
`], {cwd: new URL('..', import.meta.url).pathname, encoding: 'utf8'});
const ps2 = JSON.parse(probe);
const failures = [];
function same(actual, expected, label) {
  if (actual.length !== expected.length) { failures.push(label + ' length'); return; }
  for (let i = 0; i < actual.length; i++) if (actual[i] !== expected[i]) failures.push(`${label}[${i}] ${actual[i]} != ${expected[i]}`);
}
const airKeys = ['mode','phase','extended','target_spin','target_flip','progress_spin','progress_flip','total_spin','total_flip','adjust_spin','adjust_flip','scored_spin','scored_flip','max_spin','max_flip','axis_blend','hold_spin','hold_flip','input_angle','idle_time','spin_rate','flip_rate'];
function checkAir(row, name, label) {
  const state = ps2[name].air;
  if (!state) { failures.push(label + ' has no PS2 air state'); return; }
  airKeys.forEach((key, i) => { if (row.air[i] !== state[key]) failures.push(`${label} air ${key} ${row.air[i]} != ${state[key]}`); });
}
function check(row, name, label) {
  const ref = ps2[name];
  same(row.motion.slice(0, 3), ref.cm, label + ' position cm');
  same(row.motion.slice(3, 6), ref.vel, label + ' velocity');
  same(row.orient, ref.quat, label + ' facing');
  if (row.motion[12] !== ref.turn) failures.push(`${label} turn ${row.motion[12]} != ${ref.turn}`);
  if (row.motion[13] !== ref.brake) failures.push(`${label} brake ${row.motion[13]} != ${ref.brake}`);
  if (row.motion[14] !== ref.crouch) failures.push(`${label} crouch ${row.motion[14]} != ${ref.crouch}`);
  if (!!row.state[8] !== (ref.mode === 0)) failures.push(`${label} ground ${row.state[8]} ps2 mode ${ref.mode}`);
  if (ref.boost) {
    if (row.boost[0] !== ref.boost.meter) failures.push(`${label} meter ${row.boost[0]} != ${ref.boost.meter}`);
    if (row.boost[1] !== ref.boost.amount) failures.push(`${label} amount ${row.boost[1]} != ${ref.boost.amount}`);
    if (row.boost[3] !== ref.boost.tier) failures.push(`${label} tier ${row.boost[3]} != ${ref.boost.tier}`);
  }
}
boot();
const seeded = { motion: Array.from(f32(core._reference_motion(), 20)), orient: Array.from(f32(core._rider_orientation(), 4)), state: Array.from(f32(core._rider_state(), 16)), boost: Array.from(f32(core._boost_info(), 8)) };
check(seeded, 'snow-jam-glide.p2s', 'seed');
check(frame({}), 'snow-jam-glide-1.p2s', 'glide1');
const jump = json(new URL('../local/reference/pcsx2/jump-canonical-90.json', import.meta.url));
boot();
const jumpRows = [];
for (let tick = 0; tick < 90; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment.controls || {};
  jumpRows.push(frame({
    turn: controls.turn || 0,
    crouch: controls.crouch || 0,
    brake: controls.brake || 0,
    board: controls.boardPress || 0,
    spin: controls.spin || 0,
    flip: controls.flip || 0,
    jump: controls.jumpHeld ? 1 : 0,
    boost: controls.boostHeld ? 1 : 0,
    apply: 1,
  }));
}
check(jumpRows[29], 'snow-jam-jump-30.p2s', 'jump charge');
check(jumpRows[30], 'snow-jam-jump-31.p2s', 'jump release');
check(jumpRows[59], 'snow-jam-jump-60.p2s', 'air');
check(jumpRows[89], 'snow-jam-jump-90.p2s', 'air later');
checkAir(jumpRows[30], 'snow-jam-jump-31.p2s', 'jump release');
checkAir(jumpRows[59], 'snow-jam-jump-60.p2s', 'air');
checkAir(jumpRows[89], 'snow-jam-jump-90.p2s', 'air later');
boot();
let neutral;
for (let i = 0; i < 30; i++) neutral = frame({});
check(neutral, 'snow-jam-glide-30-a.p2s', 'glide');
boot();
for (let i = 0; i < 120; i++) neutral = frame({});
check(neutral, 'snow-jam-glide-120.p2s', 'glide120');
boot();
let brakeRow;
for (let i = 0; i < 30; i++) brakeRow = frame(i === 0 ? {} : { brake: full });
check(brakeRow, 'snow-jam-brake-30.p2s', 'brake');
boot();
let turnRow;
for (let i = 0; i < 29; i++) turnRow = frame(i === 0 ? {} : { turn: -full });
check(turnRow, 'snow-jam-turn-left-29.p2s', 'turn29');
boot();
for (let i = 0; i < 30; i++) turnRow = frame(i === 0 ? {} : { turn: -full });
check(turnRow, 'snow-jam-turn-left-30.p2s', 'turn');
boot();
let row = frame({ jump: 1 });
check(row, 'snow-jam-charge-1.p2s', 'charge1');
boot();
row = frame({});
row = frame({ jump: 1 });
check(row, 'snow-jam-charge-2.p2s', 'charge2');
boot();
row = frame({ jump: 1 });
row = frame({ jump: 1, crouch: 1 });
row = frame({ jump: 1, crouch: 1 });
check(row, 'snow-jam-charge-3.p2s', 'charge3');
boot();
row = frame({ jump: 1 });
row = frame({});
check(row, 'snow-jam-jump-short-2.p2s', 'jump short');
boot();
row = frame({ jump: 1 });
for (let i = 0; i < 14; i++) row = frame({ jump: 1, crouch: 1 });
row = frame({});
check(row, 'snow-jam-jump-medium-16.p2s', 'jump medium');
boot();
const spun = [];
for (let tick = 0; tick < 60; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment.controls || {};
  spun.push(frame({
    turn: controls.turn || 0,
    crouch: controls.crouch || 0,
    brake: controls.brake || 0,
    spin: tick >= 31 ? full : (controls.spin || 0),
    flip: controls.flip || 0,
    jump: controls.jumpHeld ? 1 : 0,
    apply: 1,
  }));
}
check(spun[59], 'snow-jam-jump-60.p2s', 'spin flight');
checkAir(spun[59], 'snow-jam-air-spin-30.p2s', 'spin');
if (spun[30].air[7] !== 0) failures.push('release frame spun before the air command');
boot();
let landed;
let touchdown = null;
let afterTouch = null;
for (let tick = 0; tick < 160; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment ? segment.controls || {} : {};
  landed = frame({
    turn: controls.turn || 0,
    crouch: controls.crouch || 0,
    jump: controls.jumpHeld ? 1 : 0,
    apply: 1,
  });
  if (!touchdown && landed.state[11]) {
    touchdown = { motion: landed.motion.slice(0, 6), landing: Array.from(f32(core._landing_info(), 6)), info: landed.info[15] };
  } else if (touchdown && !afterTouch) afterTouch = landed;
}
if (!touchdown) failures.push('neutral jump produced no landing sample');
else {
  const duration = touchdown.landing[2];
  const ratio = touchdown.landing[1] / touchdown.landing[0];
  if (touchdown.landing[3] !== 4) failures.push(`neutral landing skipped the posed query ${touchdown.landing[3]}`);
  if (!(duration > 10)) failures.push(`neutral landing duration ${duration}`);
  if (duration <= 40) {
    if (Math.abs(ratio - 0.7) > 0.000002) failures.push(`short landing ratio ${ratio}`);
  } else if (!(ratio > 0.7 && ratio <= 1) && Math.abs(ratio - 1) > 0.000002) failures.push(`long landing ratio ${ratio} after ${duration}`);
  if (Math.abs(touchdown.motion[3]) + Math.abs(touchdown.motion[4]) + Math.abs(touchdown.motion[5]) === 0) failures.push('landing sample has no velocity');
  const air = ps2['snow-jam-jump-90.p2s'].cm;
  if (touchdown.motion[0] === air[0] && touchdown.motion[1] === air[1] && touchdown.motion[2] === air[2]) failures.push('landing sample is still the airborne position');
  if (touchdown.info === 8) failures.push('neutral landing entered a crash');
  if (!afterTouch || afterTouch.state[8] !== 1) failures.push('neutral landing did not stay on snow');
  else if (!(afterTouch.motion[2] < air[2])) failures.push('neutral landing did not continue downhill');
}
if (landed.state[8] !== 1) failures.push('neutral jump did not return to snow');
if (landed.info[15] === 8) failures.push('neutral landing entered a crash');
boot();
let grabbed;
let grabEngaged = false;
for (let tick = 0; tick < 60; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment.controls || {};
  grabbed = frame({
    turn: controls.turn || 0,
    crouch: controls.crouch || 0,
    jump: controls.jumpHeld ? 1 : 0,
    grab: tick >= 31 && tick < 50 ? 1 : 0,
    apply: 1,
  });
  if (grabbed.info[2] !== 0) grabEngaged = true;
}
check(grabbed, 'snow-jam-jump-60.p2s', 'grab flight');
if (!grabEngaged) failures.push('shoulder grab did not engage');
if (grabbed.air[0] !== 3) failures.push(`shoulder grab left air mode ${grabbed.air[0]}`);
const grabDefs = json('ANIMATIONS/initial.json').original_grab_control.profile.grabs;
const grabMasks = [1, 2, 4, 8, 3, 5, 9, 6, 10, 12, 7, 11, 13, 14, 15];
for (let index = 0; index < grabMasks.length; index++) {
  boot();
  let row;
  for (let tick = 0; tick < 60; tick++) {
    const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
    const controls = segment.controls || {};
    row = frame({
      turn: controls.turn || 0,
      crouch: controls.crouch || 0,
      jump: controls.jumpHeld ? 1 : 0,
      grab: tick >= 31 ? grabMasks[index] : 0,
      apply: 1,
    });
    if (tick === 32 && row.info[0] !== grabDefs[index].semantic) failures.push(`grab ${grabMasks[index]} semantic ${row.info[0]} != ${grabDefs[index].semantic}`);
  }
  check(row, 'snow-jam-jump-60.p2s', `grab ${grabMasks[index]} flight`);
  if (row.air[0] !== 3) failures.push(`grab ${grabMasks[index]} air mode ${row.air[0]}`);
}
boot();
let adjusted;
for (let tick = 0; tick < 60; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment.controls || {};
  adjusted = frame({
    turn: tick >= 31 ? -full : (controls.turn || 0),
    crouch: controls.crouch || 0,
    jump: controls.jumpHeld ? 1 : 0,
    apply: 1,
  });
}
check(adjusted, 'snow-jam-jump-60.p2s', 'air adjust flight');
if (adjusted.air[9] === 0) failures.push('left-stick air adjust did not change adjust spin');
if (adjusted.info.slice(6, 10).every((v, i) => v === [0, 0, 0, 1][i])) failures.push('in-air adjust left the presentation pose untouched');
boot();
let flipped;
for (let tick = 0; tick < 60; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment.controls || {};
  flipped = frame({
    turn: controls.turn || 0,
    crouch: controls.crouch || 0,
    flip: tick >= 31 ? full : 0,
    jump: controls.jumpHeld ? 1 : 0,
    apply: 1,
  });
}
check(flipped, 'snow-jam-jump-60.p2s', 'flip flight');
if (flipped.air[8] === 0) failures.push('air flip total stayed zero');
if (flipped.air[7] !== 0) failures.push(`air flip also changed spin total ${flipped.air[7]}`);
boot();
let shortFlip = null;
for (let tick = 0; tick < 160 && !shortFlip; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment ? segment.controls || {} : {};
  const row = frame({
    turn: controls.turn || 0,
    crouch: controls.crouch || 0,
    flip: tick >= 31 && tick < 41 ? full : 0,
    jump: controls.jumpHeld ? 1 : 0,
    apply: 1,
  });
  if (row.state[11]) shortFlip = row;
  if (row.info[15] === 8) { failures.push('short flip crashed instead of landing'); break; }
}
if (!shortFlip || shortFlip.state[8] !== 1) failures.push('short flip did not return to snow');
boot();
let crashed = false;
let crashRow;
for (let tick = 0; tick < 130; tick++) {
  const segment = jump.native.accepted_input.segments.find((s) => s.start <= tick && tick < s.end);
  const controls = segment ? segment.controls || {} : {};
  crashRow = frame({
    turn: controls.turn || 0,
    crouch: controls.crouch || 0,
    flip: tick >= 31 ? full : 0,
    jump: controls.jumpHeld ? 1 : 0,
    apply: 1,
  });
  if (crashRow.info[15] === 8) crashed = true;
}
if (!crashed) failures.push('held flip returned to snow without the original crash');
const charge = JSON.parse(execFileSync('python3', ['-c', `
import json, struct, zipfile, sys
sys.path.insert(0,'tools')
from reference_input import extract_input, raw_accepted_delta, float32_zero
base=extract_input(zipfile.ZipFile('local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
mem=zipfile.ZipFile('local/reference/pcsx2/snow-jam-charge-long-240.p2s').read('eeMemory.bin')
outcome=extract_input(mem)
raw=raw_accepted_delta(base, outcome)
def axis(word, shift):
    value=(word>>shift)&63
    if value>=32: value-=64
    return float32_zero(value*struct.unpack('<f', struct.pack('<I', 0x3d042108))[0])
control=raw['initial_control_state']
commands=[]
for seg in raw['segments']:
    word0, word1=int(seg['word0'],16), int(seg['word1'],16)
    for _ in range(seg['duration']):
        # Air idle is 0x00ff0000. A zero word is the cruise sample recorded after touchdown.
        if control==5 and word0==0 and word1==0: control=0
        if control==0:
            if word0&0xc2000 or word1&~0xfff: raise SystemExit('unsupported cruise command')
            commands.append(dict(turn=axis(word0,20), crouch=axis(word0,26), brake=axis(word1,0), board=axis(word1,6), jump=1 if word0&0xc000 else 0))
            if word0&0xc000: control=2
        elif control==2:
            if word0&0xf8000000 or word1&~0xfff: raise SystemExit('unsupported crouch command')
            held=bool(word0&0x2000)
            commands.append(dict(turn=0, crouch=1. if held else 0., brake=0, board=0, jump=1 if held else 0))
            if not held: control=5
        elif control==5:
            if word0&0xc000a000 or word1&~0xfffff: raise SystemExit('unsupported air command')
            commands.append(dict(turn=axis(word1,12), crouch=0, brake=0, board=0, spin=axis(word0,24), flip=axis(word1,0), jump=0))
        else:
            raise SystemExit('unsupported control')
rider=0x14701a0
owner=struct.unpack_from('<I', mem, rider+0x77c)[0]
traj=struct.unpack_from('<I', mem, rider+0x788)[0]
def vec(at): return list(struct.unpack_from('<3f', mem, at))
print(json.dumps(dict(commands=commands, status=struct.unpack_from('<i', mem, traj+0xac)[0],
    predicted=struct.unpack_from('<f', mem, traj+0x98)[0], elapsed=struct.unpack_from('<f', mem, traj+0xa0)[0],
    surface=struct.unpack_from('<i', mem, traj+0x90)[0], heading=vec(traj+0x10), normal=vec(traj+0x20),
    position=vec(traj+0x70), velocity=vec(traj+0x80), impact=struct.unpack_from('<f', mem, rider+0x770)[0],
    focus=struct.unpack_from('<I', mem, owner+0x10)[0], leave=struct.unpack_from('<I', mem, owner+0x14)[0])))
`], {cwd: new URL('..', import.meta.url).pathname, encoding: 'utf8'}));
boot();
let chargeLanding = null;
let chargeAfter = null;
if (charge.commands.length !== 240) failures.push(`charge-long command count ${charge.commands.length}`);
for (let tick = 0; tick < charge.commands.length; tick++) {
  const row = frame(charge.commands[tick]);
  if (!chargeLanding && row.state[11]) chargeLanding = row;
  else if (chargeLanding && !chargeAfter) chargeAfter = row;
}
if (!chargeLanding) failures.push('charge-long produced no landing sample');
else {
  const pred = chargeLanding.pred;
  same(pred.slice(13, 16), charge.position, 'charge landing integrated position');
  same(pred.slice(16, 19), charge.velocity, 'charge landing integrated velocity');
  same(pred.slice(7, 10), charge.heading, 'charge landing heading');
  same(pred.slice(10, 13), charge.normal, 'charge landing normal');
  if (pred[1] !== charge.status) failures.push(`charge landing status ${pred[1]} != ${charge.status}`);
  if (pred[2] !== charge.predicted) failures.push(`charge landing predicted time ${pred[2]} != ${charge.predicted}`);
  if (pred[3] !== charge.elapsed) failures.push(`charge landing elapsed ${pred[3]} != ${charge.elapsed}`);
  if (pred[5] !== charge.surface) failures.push(`charge landing surface ${pred[5]} != ${charge.surface}`);
  if (chargeLanding.landing[2] !== charge.focus - charge.leave) failures.push(`charge landing duration ${chargeLanding.landing[2]}`);
  if (chargeLanding.landing[3] !== 4) failures.push(`charge landing query ${chargeLanding.landing[3]}`);
  if (chargeLanding.landing[4] !== -charge.impact) failures.push(`charge landing impact ${chargeLanding.landing[4]} != ${-charge.impact}`);
  if (chargeLanding.state[8] !== 1) failures.push('charge landing left the rider airborne');
  if (chargeLanding.info[15] === 8) failures.push('charge landing crashed');
  if (chargeLanding.motion[0] === charge.position[0] && chargeLanding.motion[1] === charge.position[1] && chargeLanding.motion[2] === charge.position[2]) failures.push('charge landing did not resolve onto the snow');
  if (!chargeAfter || chargeAfter.state[8] !== 1) failures.push('charge landing did not stay on snow');
  else if (!(chargeAfter.motion[2] < chargeLanding.motion[2])) failures.push('charge landing did not continue downhill');
}
if (failures.length) {
  console.log(failures.join('\n'));
  process.exit(1);
}
console.log('PS2 ride step matches glide, glide-120, brake, turn, charge, jump, spin, grabs, flip flight, and the charge-long landing.');
