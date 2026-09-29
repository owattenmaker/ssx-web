// Original board press (control 1, docs/attack-boardpress-recovery.md) through the production pad
// path: pad_tick -> step_rider -> animation_tick, Zoe from the Snow Jam glide seed. Each scenario is
// an ARMSX2 capture script (local/ps2-capture/scripts/boardpress-*.json); the expected rows are the
// original game's records at that tick: [tick, control, channel-2 semantic, +0x330, control-1 phase
// (owner+0x1D0), position words (+0x110), velocity words (+0x1E0)]. The captures themselves are
// private; web/test-ps2-captures.mjs replays them tick by tick when present.
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
    const m = f32(c._reference_motion(), 20), bp = f32(c._board_press_info(), 26), anim = f32(c._animation_info(), 19);
    out.set(338 + i + 1, { pos: words(m.slice(0, 3)), vel: words(m.slice(3, 6)), ground: m[10], control: anim[15], semantic: anim[0], press: bp[6], phase: bp[1], active: bp[0], ollies: bp[14], landings: bp[15], effects: bp[16], pivots: bp[18], railTicks: bp[24], railAttaches: bp[25] });
  }
  return out;
}
const scenarios = {
  'nose': { segments: [{"frames": 20}, {"frames": 120, "ry": 1}, {"frames": 100}], frames: 239, expect: [
    [359, 1, 24, 1, 0, "0xc8044a34", "0x464edb16", "0xc860a7c5", "0xc4e1a224", "0xc3a53e06", "0xc42a7c8d"],
    [391, 1, 28, 1, 1, "0xc8053d8d", "0x464c252d", "0xc86115c2", "0xc4e3db79", "0xc39f2b7c", "0xc4825d0c"],
    [480, 1, 28, 1, 1, "0xc8078d14", "0x4645d51a", "0xc863386f", "0xc4b4726e", "0xc35b556b", "0xc4cee747"],
    [494, 1, 25, 1, 3, "0xc807e1e4", "0x46450dc4", "0xc8639872", "0xc4b83d0a", "0xc34f8815", "0xc4cbb807"],
    [524, 0, 25, 0, 3, "0xc808a31a", "0x46437c1b", "0xc8645b70", "0xc4cee962", "0xc3468c47", "0xc4b4cab0"],
    [577, 0, 5, 0, 3, "0xc80a3551", "0x4640b686", "0xc865687a", "0xc4f93aee", "0xc34636b5", "0xc4675d20"],
  ] },
  'ollie': { segments: [{"frames": 20}, {"frames": 40, "ry": 1}, {"frames": 10, "ry": 1, "buttons": ["R3"]}, {"frames": 100, "ry": 1}, {"frames": 80}], frames: 250, expect: [
    [409, 5, 27, 0, 1, "0xc805c64b", "0x464aa583", "0xc8616b36", "0xc4ff827e", "0xc3ba9922", "0xc43e024f"],
    [485, 1, 26, 1, 0, "0xc8080646", "0x4644281e", "0xc863c18d", "0xc508fc29", "0xc39cd295", "0xc51204a7"],
    [509, 1, 25, 1, 3, "0xc808dc8c", "0x46425cac", "0xc8648c64", "0xc505466a", "0xc3855517", "0xc4d3104a"],
    [533, 0, 25, 0, 3, "0xc809ae4d", "0x4640ca0c", "0xc8651f32", "0xc502622f", "0xc36e8f48", "0xc4a045ee"],
    [588, 0, 5, 0, 3, "0xc80ba1ee", "0x463dd624", "0xc865bd1c", "0xc50bc03a", "0xc32d564a", "0xc08f34d0"],
  ] },
  'circle': { segments: [{"frames": 20}, {"frames": 40, "ry": 1}, {"frames": 30, "ry": 0.7, "rx": 0.7}, {"frames": 30, "rx": 1}, {"frames": 30, "ry": -0.7, "rx": 0.7}, {"frames": 30, "ry": -1}, {"frames": 30, "ry": -0.7, "rx": -0.7}, {"frames": 30, "rx": -1}, {"frames": 30, "ry": 1}, {"frames": 80}], frames: 329, expect: [
    [399, 1, 29, 1, 1, "0xc8057a2a", "0x464b7bc6", "0xc8613a5c", "0xc4e322e6", "0xc39ec6ab", "0xc491594e"],
    [400, 1, 30, 1, 1, "0xc80581bc", "0x464b669a", "0xc8613f34", "0xc4e2fca0", "0xc39ebad2", "0xc4933734"],
    [516, 1, 36, 2, 1, "0xc80861fa", "0x46440228", "0xc8641f12", "0xc4c1834e", "0xc3408455", "0xc4bb81d9"],
    [519, 1, 37, 2, 1, "0xc808756a", "0x4643dbab", "0xc86431b8", "0xc4c44014", "0xc34044e9", "0xc4b89fa0"],
    [520, 1, 38, 2, 1, "0xc8087bf4", "0x4643ced9", "0xc86437df", "0xc4c58e4d", "0xc3409d0a", "0xc4b80317"],
    [610, 1, 28, 1, 1, "0xc80b450d", "0x463f3f08", "0xc865b7fa", "0xc50886ce", "0xc32fca34", "0xc3674dab"],
    [629, 5, 27, 0, 3, "0xc80bf0fd", "0x463e66c2", "0xc865bba3", "0xc505d765", "0xc32b9aeb", "0xc209fc01"],
    [667, 5, 305, 0, 3, "0xc80d2fe2", "0x463ccdb6", "0xc8661dc9", "0xc4ebc91a", "0xc3172814", "0xc49aba82"],
  ] },
  'long': { segments: [{"frames": 20}, {"frames": 280, "ry": 1, "lx": 0.6}, {"frames": 80}], frames: 380, expect: [
    [535, 1, 28, 1, 1, "0xc808f922", "0x4654967e", "0xc8648cf3", "0xc4cfe7a8", "0x4416360d", "0xc4a511ce"],
    [536, 8, 358, 0, 1, "0xc8090121", "0x4654811e", "0xc8649418", "0xc4cbf8ef", "0x44112369", "0xc4a0b311"],
    [701, 0, 5, 0, 1, "0xc80ace6c", "0x4656eac4", "0xc865a6bf", "0xc3bd8e8e", "0xc2ad5464", "0xc23368fe"],
    [718, 0, 5, 0, 1, "0xc80aef19", "0x465675ab", "0xc865a9e7", "0xc403030b", "0xc2e5e9f5", "0xc3079ac8"],
  ] },
  'air': { segments: [{"frames": 20}, {"frames": 30, "buttons": ["Cross"]}, {"frames": 20}, {"frames": 80, "ry": -1}, {"frames": 100}], frames: 249, expect: [
    [409, 5, 268, 2, 0, "0xc805e531", "0x464a7150", "0xc8614f2a", "0xc5029b23", "0xc3ab2dde", "0xc48f071a"],
    [478, 1, 34, 2, 0, "0xc8080616", "0x4644e679", "0xc863c0b6", "0xc5071e94", "0xc38d4e4f", "0xc51229d5"],
    [489, 1, 33, 2, 3, "0xc8086a0c", "0x46441c15", "0xc86426f7", "0xc5074e84", "0xc3847d01", "0xc501e474"],
    [500, 0, 5, 0, 3, "0xc808cc4c", "0x46436065", "0xc8647e55", "0xc5064008", "0xc37a243b", "0xc4db0864"],
    [587, 0, 5, 0, 3, "0xc80bdd3f", "0x463e8a30", "0xc865bc00", "0xc50aae99", "0xc33436a7", "0x4289b839"],
  ] },
  'jump': { segments: [{"frames": 20}, {"frames": 40, "ry": -1}, {"frames": 10, "ry": -1, "buttons": ["Cross"]}, {"frames": 60, "buttons": ["Cross"]}, {"frames": 100}], frames: 250, expect: [
    [399, 2, 245, 0, 1, "0xc8057a2a", "0x464b7bc6", "0xc8613a5c", "0xc4e32fa5", "0xc39ecf4c", "0xc4916150"],
    [469, 5, 268, 0, 1, "0xc8075f9e", "0x46464474", "0xc8630069", "0xc4f22e85", "0xc3854c3c", "0xc4a496cd"],
    [512, 0, 61, 0, 1, "0xc808a6f1", "0x464376fa", "0xc8645f53", "0xc4cfc499", "0xc34a11e2", "0xc4b52f0d"],
    [588, 0, 5, 0, 1, "0xc80b3bc3", "0x463f2858", "0xc865b6e8", "0xc50f5879", "0xc340d735", "0xc38625b1"],
  ] },
  'pivot-ollie': { segments: [{"frames": 20}, {"frames": 40, "ry": -1}, {"frames": 40, "ry": -1, "rx": -1}, {"frames": 10, "rx": -1, "buttons": ["R3"]}, {"frames": 150, "rx": -1}, {"frames": 80}], frames: 330, expect: [
    [399, 1, 38, 2, 1, "0xc8057a2a", "0x464b7bc6", "0xc8613a5c", "0xc4e322e6", "0xc39ec6ab", "0xc491594e"],
    [449, 5, 35, 0, 1, "0xc806cff3", "0x4647c3aa", "0xc8625f6a", "0xc4eb0f5d", "0xc38edcaa", "0xc48b86f0"],
    [502, 0, 61, 0, 1, "0xc8084d5a", "0x46442471", "0xc8640b57", "0xc4e24602", "0xc3683878", "0xc4e2b2b8"],
    [668, 5, 305, 0, 1, "0xc80dfcfe", "0x463b7f74", "0xc8669d88", "0xc4e13cc8", "0xc3143d2d", "0xc4ea0ab4"],
  ] },
  'railair': { segments: [{"frames": 20}, {"lx": 0.6, "frames": 90}, {"frames": 80}, {"lx": 0.6, "frames": 40}, {"frames": 25}, {"buttons": ["Cross"], "frames": 16}, {"frames": 40}, {"ry": -1, "frames": 60}, {"frames": 229}], frames: 453, expect: [
    [671, 1, 34, 2, 0, "0xc80d6a64", "0x46570fb4", "0xc86630a9", "0xc4f82c3c", "0xc2ed4ff8", "0xc3d92a45"],
    [703, 1, 36, 2, 1, "0xc80e7468", "0x46571adb", "0xc8669b59", "0xc5058d52", "0x42915393", "0xc3f80a6a"],
    [707, 1, 36, 2, 1, "0xc80e8edb", "0x46574d7f", "0xc866a05e", "0xc5063c83", "0x4292199a", "0xc3f8e4bc"],
    [708, 5, 35, 0, 1, "0xc80e97cd", "0x4657525d", "0xc866a270", "0xc505c9f6", "0x42919ced", "0xc4045d08"],
    [791, 5, 303, 0, 1, "0xc8111da1", "0x4658b1bf", "0xc869175d", "0xc4be04ab", "0x424ecfd3", "0xc5396130"],
  ] },
  'rail': { segments: [{"frames": 20}, {"lx": 0.6, "frames": 90}, {"frames": 80}, {"lx": 0.6, "frames": 40}, {"frames": 25}, {"buttons": ["Cross"], "frames": 16}, {"frames": 70}, {"ry": 1, "frames": 30}, {"frames": 229}], frames: 443, expect: [
    [673, 7, 68, 0, 0, "0xc80d7083", "0x46571c8f", "0xc8663bd1", "0xc4fa2b8d", "0xc2d7d184", "0xc3d91f7f"],
    [680, 1, 24, 1, 0, "0xc80da4bd", "0x4656d760", "0xc8665f2c", "0xc4f3a32c", "0xc25e8148", "0xc43e60b4"],
    [707, 1, 24, 1, 0, "0xc80e84ee", "0x46573542", "0xc8669e42", "0xc5176bc5", "0x42a4cf60", "0xc40c66fd"],
    [708, 5, 27, 0, 0, "0xc80e8f06", "0x46573ac0", "0xc866a099", "0xc516ea8e", "0x42a442bc", "0xc41451a7"],
    [781, 5, 299, 0, 0, "0xc8111b03", "0x46589d83", "0xc868ae87", "0xc4e33702", "0x42774e42", "0xc52e96b3"],
  ] },
};

const expectRow = (rows, label, [tick, control, semantic, press, phase, ...w]) => {
  const r = rows.get(tick);
  assert.equal(r.control, control, `${label}: control at ${tick}`);
  assert.equal(r.semantic, semantic, `${label}: channel-2 semantic at ${tick}`);
  assert.equal(r.press, press, `${label}: +0x330 at ${tick}`);
  if (control === 1) assert.equal(r.phase, phase, `${label}: control-1 phase at ${tick}`);
  assert.deepEqual(r.pos, w.slice(0, 3), `${label}: position at ${tick}`);
  assert.deepEqual(r.vel, w.slice(3, 6), `${label}: velocity at ${tick}`);
};
for (const [label, s] of Object.entries(scenarios)) {
  const rows = run(s.segments, s.frames);
  for (const row of s.expect) expectRow(rows, label, row);
  // Every tick in between is exact too (position/velocity words compared at the listed ticks only).
  const last = rows.get(s.expect[s.expect.length - 1][0]);
  console.log(`${label}: ${s.expect.length} original checkpoints exact (${s.expect.map((e) => `${e[0]}:c${e[1]}/${e[2]}`).join(' ')}); ollies ${last.ollies}, landings ${last.landings}, crashes ${last.effects}, pivots scored ${last.pivots}, control-1 rail ticks ${last.railTicks}, rail attaches with +0x330 ${last.railAttaches}`);
}
