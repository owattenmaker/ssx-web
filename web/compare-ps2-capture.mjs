// Continuous web-core vs original PS2 comparison from tools/ps2_capture.py captures.
// Usage: node compare-ps2-capture.mjs CAPTURE.bin [--report out.json] [--zoe]
// Each PS2 record N holds the state that resulted from tick N-1 plus the command that tick N
// consumed. The web core replays the recorded command (decoded for the recorded controller
// state exactly like engine/original_command.cpp) and its post-step state is compared with
// record N+1.
import fs from 'node:fs';
// CORE_JS=path/core.js: compare with a private core (CORE_OUT=dir sh web/build-core.sh) instead of web/runtime.
const createCore = (await import(process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js')).default;
import { loadStageWorld, compareStageWorld, loadSnapshots } from './stage-world-compare.mjs';
import * as inWorldSetup from './ctm-in-world-setup.mjs';

const args = process.argv.slice(2);
const capturePath = args.find((a) => !a.startsWith('--'));
const reportPath = args.includes('--report') ? args[args.indexOf('--report') + 1] : null;
// --finish-place N: the human's place at a freestyle finish, as the page's host ranks it with the posted scores (web/career.js
// finishPlace); race_end asks for it on the finish tick (web/race_bridge.cpp js_finish_place: 0x239230 boost meter, 0x23A05C +0x100).
const finishPlace = args.includes('--finish-place') ? Number(args[args.indexOf('--finish-place') + 1]) : null;
const traceRange = args.includes('--trace') ? args[args.indexOf('--trace') + 1].split(':').map(Number) : null;
if (!capturePath) throw new Error('capture path required');
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const json = (p) => JSON.parse(read(p));
const core = await createCore();
// PS2_ARITH=exact: an SSX_PS2_EXACT_FPU core runs the setup below in mode 1 (the baselines' history) and the capture on the
// console model, from the switch before the first tick (docs/ps2-float.md "Mode-1 history").
if (process.env.PS2_ARITH === 'exact' && core._ps2_arith_exact) {
  core._ps2_arith_exact(0);
}
if (finishPlace != null) core.finishHost = { place: () => finishPlace };
// Stage builtin 34, the Metro-City phone booths / water towers (web/stage_teleport.inc): on;
// STAGE_TELEPORT=0 turns it off. A capture with booth_injections (the PS2 hook at 0x121818 wrote rider+0xA30 = a booth instance on
// those ticks) injects the same contact before this tick's race_end.
const boothInjections = new Map(); let boothTick = -1;
const boothInject = () => { const r = boothInjections.get(boothTick); if (r != null) core._stage_contact_inject(r); };
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e.stack : core.getExceptionMessage(e)); process.exit(1); });
const put = (bytes) => { const p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); return p; };
const str = (path) => put(Buffer.concat([read(path), Buffer.from([0])]));
// Course: --course CODE, else the capture manifest's location (tools/ps2_capture.py), else Snow Jam.
const courseManifest = JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8'));
const course = args.includes('--course') ? args[args.indexOf('--course') + 1] : (courseManifest.location || 'ARA1');
const initialPath = course === 'ARA1' ? 'ANIMATIONS/initial.json' : course + '/initial.json';
// --human RIDER_X: another selectable rider as the human, with its package's settings.json over the course initial.json as the page
// applies them (web/character-roster.js humanSettings; a cheat skin rides on Zoe: composeCheat). Use it with --event on a capture from
// the character's own countdown (local/reference/pcsx2/characters/<id>/countdown.p2s; local/ps2-capture/riders-capture.sh): a glide
// capture has no seed of its own here (the glide seed is Zoe's ANIMATIONS/initial.json, ~39 cm from Stretch's glide.p2s).
// --base RIDER_Y: the cheat skin's base rider (a skin on a goofy rider: characters/brodi-on-psymon).
const humanPackage = args.includes('--human') ? args[args.indexOf('--human') + 1] : null;
const basePackage = args.includes('--base') ? args[args.indexOf('--base') + 1] : null;
const rider = humanPackage || (args.includes('--zoe') ? 'RIDER_ZOE' : 'RIDER_SAM');
// BONE_SCAN: the PS2 geometry slot of compiled bone b. A --human package maps its rig with rider.json source_bone_slots (the live
// actor's slots: Zoe skips her inactive eye bones 24 / 25, Stretch has none, Moby's tshirt sits at 29 / 30); otherwise Zoe's rule.
const humanBoneSlots = humanPackage ? json(humanPackage + '/rider.json').source_bone_slots || null : null;
const boneSlot = (b, n) => {
  if (humanBoneSlots && b < humanBoneSlots.length) return humanBoneSlots[b];
  return b >= 24 && n <= 27 ? b + 2 : b;
};
// Poked Uber rows (0x530EC0 + char*0x1FE + slot*6 + (tier >= 5), the byte 0x14FEA8 reads for grab slot `slot`; Zoe = char 4):
// the grab profile gets the table 0x45AEB8 row as the lodge Ubertrick Setup would (web/fe-screens.js uberRow).
const uberRowPokes = (courseManifest.pokes || []).flatMap((p) => [0, 1, 2, 3].map((k) => ({ a: Number(p.address) + k - 0x530EC0 - 4 * 0x1FE, v: (Number(p.value) >>> (8 * k)) & 0xff }))).filter((p) => p.a >= 0 && p.a < 0x1FE && p.a % 6 < 2);
const initialBytes = (() => { if (!uberRowPokes.length) return read(initialPath);
  const init = JSON.parse(read(initialPath)); const shop = json('CAREER/shop.json').uber_tricks; const points = json('UI/character-select.json').uber_points.table;
  for (const { a, v } of uberRowPokes) { const slot = Math.floor(a / 6), tier = a % 6; const e = shop.categories.find((c) => c.category === slot)?.entries[v]; if (!e) continue;
    const [begin, hold] = points[e.name_index]; init.original_grab_control.profile.uber[tier][slot] = { semantic: e.trick_ids[0], upper_semantic: e.trick_ids[1], score_id: e.name_index, begin_points: begin, hold_points: hold }; }
  return Buffer.from(JSON.stringify(init)); })();
const humanBytes = await (async () => {
  if (!humanPackage) return initialBytes;
  const { humanSettings, composeCheat } = await import('./character-roster.js');
  const doc = json(humanPackage + '/settings.json');
  const base = basePackage ? json(basePackage + '/settings.json') : null;
  const character = doc.kind === 'cheat' ? composeCheat(base, doc) : doc;
  return Buffer.from(JSON.stringify(humanSettings(JSON.parse(initialBytes), character)));
})();
core._init_animation(str('ANIMATIONS/animation-packets.json'), str(rider + '/rider.json'), put(Buffer.concat([humanBytes, Buffer.from([0])])), put(read('ANIMATIONS/animation-packets.bin')), read('ANIMATIONS/animation-packets.bin').length);
core._init_race(humanPackage ? put(Buffer.concat([humanBytes, Buffer.from([0])])) : str(initialPath));
// --human on a course with lineups.json: the human's own grid spot there (web/lineup.js humanGridState, as web/ai-race.js humanGrid
// installs it); a course without one carries the Snow Jam state over (web/animation_bridge.cpp human_event_seed_for_course).
if (humanPackage && course !== 'ARA1' && fs.existsSync(new URL(course + '/lineups.json', root))) {
  const { humanGridState } = await import('./lineup.js');
  const roster = json('riders.json');
  const entry = roster.find((r) => r.package === humanPackage);
  const baseEntry = entry.kind === 'cheat' ? roster.find((r) => r.package === (basePackage || 'RIDER_ZOE')) : entry;
  const grid = humanGridState(json(course + '/lineups.json'), entry, baseEntry.character);
  if (grid) core._human_grid_seed(put(Buffer.from(JSON.stringify(grid) + '\0')));
}
core._animation_use_physics(1);
// --course PEAK1 (a free-ride / peak-run capture on the streamed Peak 1 world, web/peak-capture.mjs): every Peak 1
// location is loaded after the records are read (below) and the capture's streaming rows drive the residency.
const peakMode = /^PEAK\d$/.test(course) || /^MOUNTAIN/.test(course);   // PEAK1, PEAK3 (docs/peak3.md), MOUNTAIN[x] (the whole mountain, docs/peak3.md section 6)
if (!peakMode) {
const mesh = read(course + '/collision.bin');
core._init_world(put(mesh), mesh.length / 4);
const terrainJson = json(course + '/terrain.json');
const hash = put(Buffer.from(terrainJson.source_sha256 + '\0'));
core._init_terrain(str(course + '/terrain.json'));
core._init_world_collision(str(course + '/world_collision.json'), hash);
core._init_body_terrain(str(course + '/terrain.json'));
core._init_rails(str(course + '/rails.json'), hash);
}
// STAGE_WORLD_PS2=snapshots.json (tools/export_particle_snapshots.py): single-rider run with the section activation
// 0x101B60 (after race_end, as main.js without computer riders) and the stage world (web/stage_world.inc) loaded like
// the browser; the core's particle effects are compared with the PS2 savestates at the record ticks.
// STAGE_WORLD=1: the same browser stage world (sections + stage programs) without snapshot comparisons.
// --seed-visual-rng / --sync-visual-rng imply the stage world: its post-rider passes (flag wind, camera splash, lightning,
// crowd flash timers; web/stage_world.inc stage_world_visual_pass) draw from the visual stream every tick.
const visualWorld = process.env.STAGE_WORLD !== '0' && (args.includes('--seed-visual-rng') || args.includes('--sync-visual-rng'));
const stageSnaps = process.env.STAGE_WORLD_PS2 ? loadSnapshots(process.env.STAGE_WORLD_PS2) : process.env.STAGE_WORLD === '1' || visualWorld ? new Map() : null, stageRows = [];
if (stageSnaps && !peakMode) { // a streamed world's sections and flag manager come from web/peak-capture.mjs
  const sections = new URL(`${course}/SECTIONS/sections.json`, root);
  if (core._init_sections && fs.existsSync(sections)) core._init_sections(put(Buffer.from(fs.readFileSync(sections, 'utf8') + '\0')));
  if (!loadStageWorld(core, root, course)) throw new Error('STAGE_WORLD_PS2: stage world data missing');
}
// --lighting: the course environment package (rider Lighting painter, web/environment_bridge.cpp) is loaded and its state is
// compared with a capture watching the human's Lighting property object (watch 0: 0x60 bytes, watch 1: its 2C0778 wrapper).
const lightingMode = args.includes('--lighting');
if (lightingMode) { const bytes = read(course + '/environment.bin'), at = put(bytes); core._init_environment(str(course + '/environment.json'), at, bytes.length); core._free(at); }
const lighting = { ticks: 0, referenceTicks: 0, scalarTicks: 0, firstReference: null, firstScalar: null, references: {} };
const irradianceNames = lightingMode ? Object.fromEntries(Object.entries(JSON.parse(fs.readFileSync(new URL('../local/assets/native/IRRADIANCE/irradiance.json', import.meta.url), 'utf8')).records).map(([k, r]) => [r.index, k])) : null;
const lightingName = (w0, w1) => w1 === 0x123400 ? irradianceNames[w0] : w0 === 0 && w1 === 0 ? '(null)' : Buffer.from(new Uint32Array([w0, w1]).buffer).toString('latin1').replace(/\0+$/, '');
const start = json(course + '/start.json');
const f32 = (ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);

// ---- original command decoding (engine/original_command.cpp) ----
const AXIS = new Float32Array(new Uint32Array([0x3d042108]).buffer)[0];
function towardZero(x) {
  let r = Math.fround(x);
  if (Math.abs(r) > Math.abs(x)) {
    const b = new Float32Array([r]); const u = new Uint32Array(b.buffer); u[0] -= 1; r = b[0];
  }
  return r;
}
const axis = (word, shift) => { let v = (word >>> shift) & 63; if (v >= 32) v -= 64; return towardZero(v * AXIS); };
const GRABS = [1, 2, 4, 8, 3, 5, 9, 6, 10, 12, 7, 11, 13, 14, 15];
function decode(state, a, b) {
  const c = { state, unsupported: null };
  switch (state) {
    case 0:
      if (a & 0xc2000) c.unsupported = 'cruise attack/handplant bits';
      Object.assign(c, { turn: axis(a, 20), crouch: axis(a, 26), brake: axis(b, 0), board: axis(b, 6), jumpPressed: !!(a & 0x4000), jump: +!!(a & 0x8000), boostPressed: !!(a & 0x10000), boost: +!!(a & 0x20000) });
      break;
    case 2: {
      const jump = +!!(a & 0x2000);
      const prewind = axis(b, 0);
      Object.assign(c, { jump, boost: +!!(a & 0x4000), prewindTurn: prewind, turn: Math.max(-0.5, Math.min(0.5, prewind)), spin: axis(a, 15), flip: axis(a, 21), crouch: jump ? 1 : 0 });
      break;
    }
    case 5: {
      if (a & 0xc000a000) c.unsupported = 'air handplant/late spin bits';
      const g = (a >>> 16) & 255; let press = (b >>> 18) & 3; if (press === 3) press = -1;
      const fb = axis(b, 6);
      Object.assign(c, { boost: +!!(a & 0x4000), spin: axis(a, 24), flip: axis(b, 0), fb, turn: axis(b, 12), crouch: Math.max(fb, 0), brake: Math.max(-fb, 0), board: press, grab: g === 255 ? 0 : GRABS[g] });
      break;
    }
    case 3:
      Object.assign(c, { boostPressed: !!(a & (1 << 13)), boost: +!!(a & (1 << 14)), turn: axis(a, 15), board: axis(a, 21) });
      break;
    case 4:
      Object.assign(c, { turn: axis(a, 24), crouch: axis(b, 0), passive: ((a >>> 16) & 255) << 24 >> 24, handplant: !!(a & 0x2000) });
      break;
    default:
      c.unsupported = `controller ${state}`;
  }
  return c;
}

// ---- capture records ----
const captureManifest = JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8'));
for (const b of captureManifest.booth_injections || []) if (!Number(b.rider)) boothInjections.set(b.tick, Number(b.resource)); // the human's (a computer rider's: compare-ai-capture.mjs)
if (process.env.STAGE_TELEPORT !== '0') core._stage_teleport_enable?.(1);
const RECORD = captureManifest.record || 8192;
let raw = fs.readFileSync(capturePath);
// --peak-arrival (with --course PEAK1): a location entry capture (free-ride Transport arrival, tools/ps2_capture.py from a
// screen-10 state): the comparison starts at the placement record (the first after the transport control 13), which the
// browser reproduces with core place_rider_region (web/peak-capture.mjs arrive). The words the placement keeps are seeded from
// the records: the retained speed limit +0x2E4 and the boost words (record P-1), the route heading +0x4CC and the location id
// +0x434 (record P), and the game tick 1298C8 (the rider manager's +8, record P's tick field).
// --peak-ws15 SEED (with --course PEAK1; pv eventReturnInWorld, docs/ctm-events-in-world.md stage 5): the return from an event (WS15), record 0
// the placement, the kept words from SEED (web/ctm-in-world-setup.mjs ws15Seeds).
const ws15Seed = args.includes('--peak-ws15') ? JSON.parse(fs.readFileSync(args[args.indexOf('--peak-ws15') + 1], 'utf8')) : null;
const arrivalMode = args.includes('--peak-arrival') || !!ws15Seed;
// --peak-fresh (with --course PEAK1): a world start's capture (tools/ps2_capture.py from the state before its first tick, e.g. the
// CTM last-lodge start): record 0 is the placement of the world load's new rider, which the browser reproduces with core
// fresh_rider_start + place_rider_region (web/free-ride.js placeRegion; docs/peak-mountain.md "Fresh rider at a world start").
const freshMode = args.includes('--peak-fresh');
// --station-hold (pv nisTick + nisAfterScan; docs/ctm-parity.md "The NIS teleport after the section scan"): builtin 68's lodge door /
// booth (core peak_world_events kind 1, action 3 / 4, fired in a tick's 121818) starts the cut's rider actor: 123640 holds the rider at
// the next update's NIS tick (0x230BE4), after the firing tick's section pass 0x101B60 and before the next record (the provider exit),
// so the firing tick's section pass still sees the rider where it was. The actor root is the next record's (+0x110, facing +0x1B0 /
// +0x1B4). STATION_HOLD_EARLY=1 (diagnostic): the hold inside the firing tick, before its section pass (the page
// with pv nisAfterScan off).
const stationHold = args.includes('--station-hold');
let stationPending = null; globalThis.__beforeSectionPass = null;
const stationFired = () => { if (!stationHold || !core._peak_world_events) return false; const p = core._peak_world_events() >> 2, H = new Int32Array(core.HEAPU8.buffer), n = H[p]; let fired = false;
  for (let k = 0; k < n; k++) { const kind = H[p + 1 + 4 * k], b = H[p + 3 + 4 * k]; if (kind === 1 && (b === 3 || b === 4)) fired = true; } return fired; };
let arrivalSeedWords = null; // web/ctm-in-world-setup.mjs arrivalSeeds: the words the placement 11D390 keeps, from the records
if (arrivalMode) { const R0 = JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8')).record || 8192;
  arrivalSeedWords = ws15Seed ? inWorldSetup.ws15Seeds(raw, R0, ws15Seed) : inWorldSetup.arrivalSeeds(raw, R0); raw = raw.subarray(arrivalSeedWords.P * R0); }
const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
const records = [];
for (let at = 0; at + RECORD <= raw.length; at += RECORD) {
  const u = (o) => dv.getUint32(at + o, true), f = (o) => dv.getFloat32(at + o, true);
  const rider = (off) => f(32 + off - 0x100);
  records.push({
    tick: u(4), gameTick: u(4), word0: u(8), word1: u(12), mode: u(16), control: u(20), index: u(28), // gameTick: 1298C8 as recorded (tick is renumbered for live captures)
    position: [rider(0x110), rider(0x114), rider(0x118)],
    quat: [rider(0x120), rider(0x124), rider(0x128), rider(0x12c)],
    velocity: [rider(0x1e0), rider(0x1e4), rider(0x1e8)],
    turn: rider(0x1f0), brake: rider(0x214), crouch: rider(0x220), speedLimit: rider(0x2e4), normal: [rider(0x370), rider(0x374), rider(0x378)],
    camEye: [f(2656 + 0x60), f(2656 + 0x64), f(2656 + 0x68)], camLook: [f(2656 + 0x20), f(2656 + 0x24), f(2656 + 0x28)],
    others: [0, 1, 2, 3, 4].map((k) => [f(3008 + 32 * k), f(3012 + 32 * k), f(3016 + 32 * k)]),
    offset9d0: [rider(0x9d0), rider(0x9d4), rider(0x9d8)], finalEye: [f(4288 + 0x20), f(4288 + 0x24), f(4288 + 0x28)], finalLook: [f(4288 + 0xe0), f(4288 + 0xe4), f(4288 + 0xe8)], lift: f(4288 + 0x460), fovNearFar: [f(4288), f(4292), f(4296)], bone22: [f(3264 + 22 * 32), f(3268 + 22 * 32), f(3272 + 22 * 32)],
  });
}
// Closed-loop captures of the whole-mountain runs (tools/ps2_autopilot.py): the game-info tick restarts at a peak run's location
// crossings (world state 11 after a connector's Unload); the pad hook's distinct-tick counter (record +28) runs on, so it numbers
// the ticks since the objectives card's Continue.
if (courseManifest.live) for (let i = 1; i < records.length; i++) {
  if (records[i].index !== records[i - 1].index + 1) throw new Error(`index gap at record ${i}`);
  records[i].tick = records[0].tick + (records[i].index - records[0].index);
}
for (let i = 1; i < records.length; i++) if (records[i].tick !== records[i - 1].tick + 1) {
  // STAGE_WORLD_PS2 runs stop at an event restart (e.g. setpieces-bhp1/full restarts at 4697); gated captures have none.
  if (process.env.STAGE_WORLD_PS2) { records.length = i; break; }
  if ((args.includes('--ctm-in-world') || ws15Seed) && records[i].tick === 0) continue; // the Continue's 1297C8(C, 1) / WS15's WS1 arg 0 -> WS4: the game tick restarts at 0
  throw new Error(`tick gap at record ${i}`);
}

// ---- scenario pad channels (tools/ps2_capture.py decode_pad) ----
const padMode = args.includes('--pad');
const manifest = padMode ? JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8')) : null;
const BUTTON_NAMES = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown', 'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
const axisByte = (v) => Math.floor((Math.max(-1, Math.min(1, v)) + 1) * 127.5 + 0.5);
function decodePad(seg) {
  const values = new Float32Array(24);
  const held = new Set(seg.buttons || []);
  BUTTON_NAMES.forEach((name, i) => { const on = held.has(name); values[i] = i >= 4 ? towardZero((on ? 255 : 0) * R255) : (on ? 1 : 0); });
  const raw = [axisByte(seg.rx || 0), axisByte(-(seg.ry || 0)), axisByte(seg.lx || 0), axisByte(-(seg.ly || 0))];
  raw.forEach((b, axis) => {
    const neg = Math.max(Math.trunc((79 - b) * 255 / 79), 0), pos = Math.max(Math.trunc((b - 176) * 255 / 79), 0);
    values[16 + 2 * axis] = towardZero(neg * R255); values[17 + 2 * axis] = towardZero(pos * R255);
  });
  return values;
}
const padEntries = [];
if (padMode) { let end = 0; for (const seg of manifest.segments) { end += seg.frames; padEntries.push({ end, values: decodePad(seg) }); } }
const padFor = (index) => (padEntries.find((e) => index < e.end) || padEntries[padEntries.length - 1]).values;
const padPtr = core._malloc(96);
let firstWordMismatch = null;

// ---- web replay ----
const peakWorld = peakMode ? await (await import('./peak-capture.mjs')).loadPeakWorld({ core, root, captureManifest, dv, RECORD, records, arrival: arrivalMode || freshMode, world: course }) : null;
core._reset_animation();
core._reset_race();
core._reset_rider(...start.position, start.heading);
if (arrivalMode) { const a = inWorldSetup.applyArrival(core, peakWorld, arrivalSeedWords);
  if (process.env.SEED_FIELDS) console.error('arrival', a.location, 'course', a.course, 'entry', JSON.stringify(a.entry.position), 'limit', arrivalSeedWords.limit); }
// --ctm-in-world CODE (with --course PEAK1 --peak-arrival; pv eventInWorld): arrival -> free ride -> the event's gate -> WS1's hold
// -> the card -> the race in one capture, set up as the page runs it (web/ctm-in-world-setup.mjs; local/ctm-events/caps/c0a-full).
let inWorld = null;
if (args.includes('--ctm-in-world')) { if (!arrivalMode) throw new Error('--ctm-in-world needs --peak-arrival');
  inWorld = inWorldSetup.planCtmInWorld({ code: args[args.indexOf('--ctm-in-world') + 1], core, dv, RECORD, records, layout: captureManifest.layout, json });
  console.error('ctm in world', inWorld.code, 'gate', records[inWorld.G].tick, 'hold', records[inWorld.H].tick, 'countdown record', inWorld.C, 'kind', inWorld.kind, 'mode', inWorld.mode, 'document', inWorld.docPath); }
if (freshMode) { if (!peakWorld || !core._fresh_rider_start) throw new Error('--peak-fresh needs --course PEAK1 and a core with fresh_rider_start'); peakWorld.arrive(() => core._fresh_rider_start(), 0); }
const eventMode = args.includes('--event');
// --pro: Controller Settings "Pro" captures (DATA/CONFIG/INPUT2.MAP, the race copy's controller byte 0x535B30 = 1): the core evaluates the Pro expressions.
if (args.includes('--pro')) { if (!core._set_input_map) throw new Error('core without set_input_map'); core._set_input_map(1); }
function frame(c) {
  // Original control0 returns right after requesting control2 on a jump press, before its cruise targets.
  core._ride_command(c.turn || 0, c.crouch || 0, c.brake || 0, c.board || 0, c.spin || 0, c.flip || 0, c.jump || 0, c.boost || 0, c.grab || 0, c.state === 0 && c.jumpPressed ? 0 : 1);
  core._rail_shoulder_input(c.grab || 0);
  core._rail_preinput(c.flip || 0);
  core._rail_rotation_input(c.spin || 0);
  core._race_begin(); snapshotRollers();
  const state = f32(core._step_rider(c.turn || 0, c.jump || 0, c.brake ? 1 : 0, c.boost || 0), 16);
  core._animation_tick(state[7], c.turn || 0, c.brake || 0, state[9], state[8], c.jump || 0, c.grab || 0, c.boost || 0, c.boost || 0, 0, state[15], c.flip || 0);
  boothInject();
  core._race_end();
  const motion = Array.from(f32(core._reference_motion(), 20));
  return { motion, quat: Array.from(f32(core._rider_orientation(), 4)), info: Array.from(f32(core._animation_info(), 19)), grounded: state[8] };
}
// Production browser path: pad_tick already issued ride_command and rail/start/crash inputs.
// Roller (crashbag RollerModifier) state right after this tick's entity update (race_begin),
// compared with the capture's roller-pool watch window (tools/ps2_capture.py --watch).
let rollerSnapshot = null;
let rollerBytes = [];
let chairliftSnapshot = null; // set_piece_bits(): per lift distance bits + 3 x 16 car matrix words
const snapshotRollers = () => { if (core._set_piece_bits) { const p = core._set_piece_bits(), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0]; chairliftSnapshot = Array.from(new Uint32Array(core.HEAPU8.buffer, p, 1 + n * 49)); }
  if (!core._roller_info) return; const n = f32(core._roller_info(), 1)[0]; rollerSnapshot = Array.from(f32(core._roller_info(), 4 + 13 * n));
  if (process.env.ROLLER_BYTES && core._roller_bytes) rollerBytes = Array.from({ length: n }, (_, k) => Uint8Array.from(core.HEAPU8.subarray(core._roller_bytes(k), core._roller_bytes(k) + 0x2D0))); };
// Camera word parity (core camera_state_words): the live DEFAULT_3 algorithm words (0x390 bytes; the capture record
// holds +0x000..+0x15C at 2656, a --watch of the camera address adds the rest) and the compositor lookAt/eye
// (outer +0xE0/+0x100), lift (+0x460) and last probe normal (+0x470) at the outer-camera block 4288.
// Compositor words: outer-camera offsets of engine/original_camera_words.hpp compositorOffsets (read from the header).
const COMPOSITOR_OFFSETS = fs.readFileSync(new URL('../engine/original_camera_words.hpp', import.meta.url), 'utf8').match(/compositorOffsets\{([^}]*)\}/)[1].split(',').map((x) => Number(x.trim()));
const COMPOSITOR_NAMES = ['look0', 'look1', 'look2', 'look3', 'eye0', 'eye1', 'eye2', 'eye3', 'lift', 'probe0', 'probe1', 'probe2', 'probe3', 'shakeIndex', 'shakeAmp', 'shakePending', 'shakeWasPending',
  ...['period1', 'target1', 'previous1', 'timer1', 'period2', 'target2', 'previous2', 'timer2'].flatMap((n) => [0, 1, 2].map((k) => `${n}[${k}]`)), 'shakeFade', 'shakeActive'];
const CAMERA_WORDS = 228 + COMPOSITOR_OFFSETS.length;
const CAMERA_MAPPED = new Set([0, 4, 8, 0x50, 0x54, ...[0x20, 0x40, 0x60, 0x80, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x100, 0x110, 0x120, 0x130, 0x140, 0x150, 0x160, 0x170, 0x180, 0x190, 0x1A0, 0x1B0, 0x1C0].flatMap((q) => [q, q + 4, q + 8, q + 12]),
  ...Array.from({ length: 0x254 - 0x1D0 >> 2 }, (_, k) => 0x1D0 + 4 * k), ...Array.from({ length: 0x304 - 0x2B8 >> 2 }, (_, k) => 0x2B8 + 4 * k), ...Array.from({ length: 5 }, (_, k) => [0x304 + 12 * k, 0x308 + 12 * k]).flat(),
  ...Array.from({ length: 0x38C - 0x340 >> 2 }, (_, k) => 0x340 + 4 * k)]);
// PS2 camera words of record r: [word index, value] for every mapped word the capture holds.
function ps2CameraWords(r) {
  const base = r * RECORD, out = []; const camera = Number(captureManifest.camera || 0);
  const watches = captureManifest.layout?.watches || []; let watchAt = -1, offset = 0;
  for (const w of watches) { if (camera && Number(w.address) === camera && w.length >= 0x390) watchAt = offset; offset += w.length; }
  for (let o = 0; o < 0x390; o += 4) { if (!CAMERA_MAPPED.has(o)) continue;
    // Spline flag words (+0x304 + 12i) pack clamped/allocated/built/count/segments/current in bits 0..26 (the rest is not modelled).
    const splineFlags = o >= 0x304 && o < 0x340 && (o - 0x304) % 12 === 0;
    if (watchAt >= 0) out.push([o >> 2, (dv.getUint32(base + captureManifest.layout.watch_offset + watchAt + o, true) & (splineFlags ? 0x7FFFFFF : 0xFFFFFFFF)) >>> 0]);
    else if (o < 0x160) out.push([o >> 2, dv.getUint32(base + 2656 + o, true)]); }
  COMPOSITOR_OFFSETS.forEach((o, k) => out.push([228 + k, dv.getUint32(base + 4288 + o, true)]));
  return out;
}
const cameraWordName = (k) => k < 228 ? 'alg+0x' + (4 * k).toString(16) : 'comp.' + COMPOSITOR_NAMES[k - 228];
let cameraWordTicksExact = 0, cameraWordTicks = 0, firstCameraWordMismatch = null; const cameraWordFirst = {}, cameraShakeRngTicks = [];
const shakeSync = process.argv.includes('--camera-shake-sync');
function padFrame(c) {
  const vw = () => core._visual_rng_words ? Array.from(new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6)) : null, phases = process.env.VISUAL_TRACE ? [vw()] : null;
  core._race_begin(); snapshotRollers(); phases?.push(vw()); if (process.env.VISUAL_CHECK && core._visual_rng_words) globalThis.__visAfterBegin = Array.from(new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6));
  // VISUAL_SPAN=a:b: draws from the previous sync point (record T words) to this one (before overwriting with record T+1),
  // i.e. the browser's count for the PS2 record interval T -> T+1.
  if (process.env.VISUAL_SPAN && globalThis.__visualSyncWords && globalThis.__visSpanFrom) { const [a, b] = process.env.VISUAL_SPAN.split(':').map(Number), t = globalThis.__visSpanTick;
    if (t >= a && t <= (b ?? a)) console.error('span', t, 'web', drawsBetween(globalThis.__visSpanFrom, vw()), 'ps2', drawsBetween(globalThis.__visSpanFrom, globalThis.__visualSyncWords)); }
  if (globalThis.__visualSyncWords) { globalThis.__visSpanFrom = globalThis.__visualSyncWords.slice(); globalThis.__visSpanTick = globalThis.__visTickNow; }
  if (globalThis.__visualSyncWords) { new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6).set(globalThis.__visualSyncWords); if (phases) phases.push(vw()); }
  if (globalThis.__visualProbe) { const w = Array.from(new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6)), rec = globalThis.__visualProbe; globalThis.__visualProbe = null; console.error('visual probe (first record, after race_begin): browser', drawsBetween(rec, w), 'draws ahead /', drawsBetween(w, rec), 'behind'); }
  if (globalThis.__visualSeedOnce) { const w = new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6), rec = globalThis.__visualSeedOnce; globalThis.__visualSeedOnce = null;
    if (process.env.VISUAL_CHECK) console.error('visual seed: browser words before the copy are', drawsBetween(rec, Array.from(w)), 'draws ahead of the record (-1: not on the path)', drawsBetween(Array.from(w), rec), 'behind');
    w.set(rec); if (process.env.VISUAL_CHECK) globalThis.__visAfterBegin = Array.from(w); }
  const state = f32(core._step_rider(c.turn, c.jump, c.brake ? 1 : 0, c.boost), 16); phases?.push(vw());
  core._animation_tick(state[7], c.animTurn, c.animBrake, state[9], state[8], c.jump, c.grab, c.tweak, c.boost, 0, state[15], c.animFlip); phases?.push(vw());
  const pose = f32(core._pose_physical(), 12);
  boothInject(); // PS2 booth-contact injection captures (docs/stage-teleport.md): this tick's 121818 runs the booth program
  core._race_end(); // course events (finish -> camera 0x162258) precede the camera update, as in main.js
  if (globalThis.__beforeSectionPass) { const f = globalThis.__beforeSectionPass; globalThis.__beforeSectionPass = null; f(); } // --station-hold with STATION_HOLD_EARLY
  if (stageSnaps || peakWorld) { const ev = core._section_pass?.(); if (process.env.SECTION_TRACE) { const [a, b] = process.env.SECTION_TRACE.split(':').map(Number); if (globalThis.__visTickNow >= a && globalThis.__visTickNow <= b) { const p = core._set_piece_sections(), H = new Uint32Array(core.HEAPU8.buffer, p, 2); console.error('sections', globalThis.__visTickNow, 'events', ev, 'scans', H[1]); } } } phases?.push(vw()); // Peak 1: its section activation (challenge planes, fences); SECTION_TRACE=a:b (diagnostic)
  // --station-hold: the next update's NIS tick (0x230BE4) runs after this section pass and before the next record (the provider exit),
  // so this tick's outputs below are compared with a record that already holds the rider at the actor
  if (stationPending) { const [x, y, z, fx, fy] = stationPending; stationPending = null; core._nis_hold(1, x, y, z, fx, fy); console.error('station hold after the section pass of tick', globalThis.__visTickNow); }
  if (phases) globalThis.__visPhases = phases.slice(1).map((w, k) => (globalThis.__visualSyncWords && k === 1) ? 'sync' : drawsBetween(phases[k], w)); // race_begin, step_rider, animation_tick, race_end+sections
  const vis = () => core._visual_rng_words ? Array.from(new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6)) : null;
  const visBeforeCamera = process.env.VISUAL_TRACE ? vis() : null;
  const cam = Array.from(f32(core._step_camera_head(pose[9], pose[10], pose[11]), 9));
  if (visBeforeCamera) globalThis.__visTrace = [visBeforeCamera, vis()];
  const algo = Array.from(f32(core._camera_algorithm_info(), 6));
  const camWords = core._camera_state_words ? Uint32Array.from(new Uint32Array(core.HEAPU8.buffer, core._camera_state_words(), CAMERA_WORDS)) : null;
  const motion = Array.from(f32(core._reference_motion(), 20));
  const head = [pose[9], pose[10], pose[11]], camIn = Array.from(f32(core._camera_source_input(), 38));
  return { motion, cam, algo, camWords, head, camIn, quat: Array.from(f32(core._rider_orientation(), 4)), info: Array.from(f32(core._animation_info(), 19)), grounded: state[8] };
}
const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
const rows = [];
const fieldFirst = {}; // FIELD_DIFF
let firstRollerMismatch = null, rollerTicksExact = 0, firstChairliftMismatch = null, chairliftTicksExact = 0;
let firstLimit = null, limitAgreed = null, limitAgreedRow = null, firstLimitDivergence = null, firstExact = null, firstPos1cm = null, firstControl = null, firstMode = null, unsupported = null;
// --event: the browser's real race start (grid, countdown control6) advanced to the PS2 countdown anchor tick.
if (eventMode) {
  core._reset_pad_history(); if (process.env.STAGE_COLLECT_PATH && core._set_stage_collect_state) core._set_stage_collect_state(+process.env.STAGE_COLLECT_PATH, 0, 0); /* 0x535C11 (QA: 0 = career race collectibles) */ core._start_event();
  if (process.env.CAMERA_SEED_JSON) { const [file, key] = process.env.CAMERA_SEED_JSON.split(':'); const w = Uint32Array.from(JSON.parse(fs.readFileSync(file, 'utf8'))[key].words);
    const wp = core._malloc(4 * CAMERA_WORDS), mp = core._malloc(CAMERA_WORDS); core.HEAPU8.set(new Uint8Array(w.buffer), wp); core.HEAPU8.fill(1, mp, mp + CAMERA_WORDS); core._camera_seed_words(wp, mp); core._free(wp); core._free(mp); }
  // --carry-seed OFF,OFF,...: a CTM countdown savestate's rider carries free-ride / hold words the event seed does not have
  // (docs/ctm-events-in-world.md §6.3): copy them from record 0 (core ground_state_seed; the countdown holds them still).
  if (args.includes('--carry-seed')) { const offs = args[args.indexOf('--carry-seed') + 1].split(',').map(Number), w = core._malloc(8 * offs.length);
    offs.forEach((o, k) => { core.HEAPF32[(w >> 2) + 2 * k] = o; core.HEAPF32[(w >> 2) + 2 * k + 1] = dv.getFloat32(32 + o - 0x100, true); }); core._ground_state_seed(w, offs.length); core._free(w); }
  // --ctm-countdown: a CTM countdown savestate (the first heat ridden in from free ride, or a WS13 heat): the words core
  // event_grid_start keeps from the rider, from record 0 (the countdown holds them): the motion-0 stamps (owner +0x10 / +0x14),
  // the boost words +0x2E8..+0x304 and the normals +0x380 / +0x390.
  if (args.includes('--ctm-countdown')) { const o = captureManifest.layout.owner_00_40, r0 = (off) => dv.getFloat32(32 + off - 0x100, true), i0 = (off) => dv.getInt32(32 + off - 0x100, true);
    core._ground_tick_seed(dv.getUint32(o + 0x10, true), dv.getUint32(o + 0x14, true));
    core._boost_state_seed(r0(0x2e8), r0(0x2ec), r0(0x2f0), i0(0x2f4), r0(0x2f8), r0(0x2fc), i0(0x304));
    const offs = [0x380, 0x384, 0x388, 0x390, 0x394, 0x398], w = core._malloc(8 * offs.length);
    offs.forEach((off, k) => { core.HEAPF32[(w >> 2) + 2 * k] = off; core.HEAPF32[(w >> 2) + 2 * k + 1] = r0(off); }); core._ground_state_seed(w, offs.length); core._free(w); }
  const neutral = new Float32Array(24);
  for (let t = 0; t < records[0].tick; t++) { core.HEAPF32.set(neutral, padPtr >> 2); const o = Array.from(f32(core._pad_tick(padPtr), 24)); padFrame({ turn: o[0], jump: o[6], brake: o[2], boost: o[7], grab: o[8], tweak: o[12], animTurn: o[10], animBrake: o[11], animFlip: o[13] }); }
}
// --weather MAP with a "flags" watch (the flag manager +0x10..+0x1F): a mid-run baseline's manager wind comes from record 0
// (a streamed world's manager was built at its world load, long before the baseline; its slots are left as the port has them).
if (args.includes('--weather') && core._set_world_visual_state) { const map = JSON.parse(fs.readFileSync(args[args.indexOf('--weather') + 1], 'utf8'));
  let off = -1, o = 0; for (const w of captureManifest.layout?.watches || []) { if (map.flags !== undefined && Number(w.address) === Number(map.flags)) off = o; o += w.length; }
  if (off >= 0) { const at = captureManifest.layout.watch_offset + off, W = (k) => dv.getUint32(at + 4 * k, true);
    const b = Buffer.from(JSON.stringify({ flags: { wind_bits: { wind: W(0), base: W(1), delta: W(2), timer: W(3) }, slots: [] } }) + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); try { core._set_world_visual_state(p); } finally { core._free(p); } } }
// A mid-run baseline's world visual state (tools/export_world_visual_state.py -> CAPTURE.visual-state.json: crowd flash
// timers, flag wind and slots, the section list) replaces the ready-state seed of those systems.
{ const vsPath = capturePath.replace(/\.bin$/, '.visual-state.json');
  if (visualWorld && core._set_world_visual_state && fs.existsSync(vsPath)) { const b = Buffer.from(fs.readFileSync(vsPath, 'utf8') + '\0'), p = core._malloc(b.length); core.HEAPU8.set(b, p); try { core._set_world_visual_state(p); } finally { core._free(p); } } }
// --peak-run (with --event): a Peak 1 Race/Jam start from the objectives card's Continue (docs/peak-mountain.md). The grid
// slot is the course's rival rolling start (11DE60(rider, 0, 1) -> 11D660 semantic 5, 11DF18 velocity), but the run's
// setup had placed the rider there before and run its reset-motion frames: +0x2E4 = 3333.33 (11B3F8 motion 1) and
// +0x380 = +0x370 (core peak_run_start_seed), and the route words of those frames' 112A50 updates (+0x490..+0x4CC; the
// heading +0x4CC is 13C948's fall line), which the rival's ready state computed at a slightly different spot.
// With --course MOUNTAIN[x] (the All Peak runs / the Peak 2 Race on the whole mountain) the seed is the objectives card's own state
// (tools/export_peak_seed.py --world), so only the words those frames leave are seeded, as with --event.
if (args.includes('--peak-run')) { if (!(eventMode || peakMode) || !core._peak_run_start_seed || !core._reset_route_seed) throw new Error('--peak-run needs --event (or a streamed world) and a core with peak_run_start_seed/reset_route_seed'); core._peak_run_start_seed();
  const w = core._malloc(52); for (let k = 0; k < 13; k++) core.HEAPF32[(w >> 2) + k] = dv.getFloat32(32 + 0x490 + 4 * k + (k >= 3 ? 4 : 0) + (k >= 6 ? 4 : 0) + (k >= 9 ? 4 : 0) - 0x100, true); core._reset_route_seed(w); core._free(w); }
const seed = { motion: Array.from(f32(core._reference_motion(), 20)) };
// SEED_FIELDS=1: the web start state (ground_state_dump) against record 0, every differing rider field.
if (process.env.SEED_FIELDS) { const ptr = core._ground_state_dump(), n = f32(ptr, 1)[0], pairs = f32(ptr + 4, n * 2), out = [];
  for (let k = 0; k < n; k++) { const off = pairs[2 * k], wv = pairs[2 * k + 1], pv = dv.getFloat32(32 + off - 0x100, true); if (Math.fround(wv) !== pv) out.push(`0x${off.toString(16)} ${wv} ${pv}`); }
  console.error('seed fields', out.join(' | ')); if (core._boost_info) console.error('seed boost (meter, amount, window, tier, modifier, super, drain, flags)', Array.from(f32(core._boost_info(), 8)).join(','), 'ps2', [0x2f8, 0x2fc, 0x2e8].map((o) => dv.getFloat32(32 + o - 0x100, true)).concat([dv.getInt32(32 + 0x2f4 - 0x100, true), dv.getFloat32(32 + 0x2ec - 0x100, true), dv.getFloat32(32 + 0x2f0 - 0x100, true), dv.getInt32(32 + 0x304 - 0x100, true)]).join(',')); }
// Trick score object *(rider+0x790) and HUD slot headers (tools/ps2_capture.py SCORE_RECORD/HUD_SLOTS_RECORD,
// docs/tricks-scoring.md). Mid-run baselines seed the browser object from the first record; --event
// starts from the browser's own event-start reset. --score reports the first differing word per field.
const scoreLayout = captureManifest.layout?.score_000_1d0, hudLayout = captureManifest.layout?.hud_slots_44x6;
const scoreMode = !!(scoreLayout && hudLayout && core._score_object_dump);
const SCORE_WORDS = 0x1d0 / 4, HUD_WORDS = 44 * 6;
const SCORE_SKIP = new Set([0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1cc]); // pointers/count, renderer-owned type bitmask (117400), past the object
const SCORE_FLOATS = new Set([0x14, 0x18, 0x1c, 0x24, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x6c, 0x78, 0xa4, 0x144, 0x14c, 0x150, 0x154, 0x158, 0x15c, 0x160, 0x164, 0x168, 0x1a8, 0x1c4]);
const scoreFirst = {}; let scoreTicksExact = 0, firstScoreMismatch = null;
if (captureManifest.pokes && captureManifest.pokes.length && core._boost_state_seed) { // poked savestates (e.g. near-full boost for Uber captures)
  const r0 = (off) => dv.getFloat32(32 + off - 0x100, true), i0 = (off) => dv.getInt32(32 + off - 0x100, true);
  core._boost_state_seed(r0(0x2e8), r0(0x2ec), r0(0x2f0), i0(0x2f4), r0(0x2f8), r0(0x2fc), i0(0x304));
}
// Checkpoint bonus (112FB0 -> 10E558 -> 1194C0, web/race_bridge.cpp set_race_bonus): a capture that pokes the list
// 0x4D33B8 (6 x {int32 value, float distance}), the game mode word 0x535C10 (byte 0x535C12) or *0x5308D0 gets the same
// configuration. The event handler index is *(G+0xC0)+4: 0 (freestyle) on the BHP1 pipe with GMM+8 = 2, else 1 (race).
{ const pokes = new Map((captureManifest.pokes || []).map((p) => [Number(p.address), Number(p.value) >>> 0]));
  const bonusWords = Array.from({ length: 12 }, (_, k) => pokes.get(0x4D33B8 + 4 * k) ?? 0);
  if (core._set_race_bonus && (bonusWords.some((w) => w) || pokes.has(0x535C10) || pokes.has(0x5308D0))) {
    const mode = pokes.has(0x535C10) ? (pokes.get(0x535C10) >>> 16) & 255 : 0; const pipe = (captureManifest.location || 'ARA1') === 'BHP1';
    const ptr = core._malloc(48); new Int32Array(core.HEAPU8.buffer, ptr, 12).set(bonusWords.map((w) => w | 0)); core._set_race_bonus(ptr, mode, pipe ? 0 : 1, pipe ? 2 : 0, pokes.get(0x5308D0) ?? 0); core._free(ptr); } }
// Freestyle events with their own configuration (tools/export_freestyle_event.py, web/freestyle-event.js; docs/slopestyle-bigair.md):
// in --event mode the course's game mode/handler/kind, checkpoint-bonus list 0x4D33B8 and time limit GMM+0x78 (R&B, Crow's Nest).
if (eventMode && core._set_race_bonus && !(captureManifest.pokes || []).some((p) => [0x535C10, 0x5308D0].includes(Number(p.address)) || (Number(p.address) >= 0x4D33B8 && Number(p.address) < 0x4D33E8))) {
  const file = new URL(`public/assets/${course}/freestyle-event.json`, import.meta.url);
  if (fs.existsSync(file)) { const cfg = JSON.parse(fs.readFileSync(file, 'utf8'));
    const ptr = core._malloc(48); new Int32Array(core.HEAPU8.buffer, ptr, 12).set(cfg.bonus_words.map((w) => w | 0)); core._set_race_bonus(ptr, cfg.game_mode, cfg.handler, cfg.freestyle_kind, cfg.global_flags >>> 0); core._free(ptr);
    if (cfg.timed && core._race_time_limit) core._race_time_limit(args.includes('--time-limit') ? Number(args[args.indexOf('--time-limit') + 1]) : cfg.time_limit_ticks); } } // --time-limit: a capture that pokes GMM+0x78 (e.g. runs/weather/ess3-weather: 0x59fe78 = 14400)
// Poked stat level override rider+0xB34 (every stat getter 0x148D80..0x149690 uses it as the numerator when > 0, e.g.
// POKE=0x1470CD4:11 maxes all seven stats): the browser applies the same override through set_rider_attributes.
{ const levelPoke = (captureManifest.pokes || []).find((p) => Number(p.address) === Number(captureManifest.rider) + 0xB34);
  if (levelPoke && core._set_rider_attributes) { const raw = core._malloc(28); new Int32Array(core.HEAPU8.buffer, raw, 7).fill(5); core._set_rider_attributes(raw, Number(levelPoke.value) | 0); core._free(raw); } }
// --attributes A,B,C,D,E,F,G: the human's raw attribute bytes in the runtime bank 0x535538 order (speed, accel, tricks, edging, spin,
// toughness, stability), e.g. a lodge Buy Attributes purchase before the world load (docs/career-events.md "Buy Attributes").
if (args.includes('--attributes')) { const bytes = args[args.indexOf('--attributes') + 1].split(',').map(Number); if (bytes.length !== 7 || !core._set_rider_attributes) throw new Error('--attributes needs 7 bytes and a core with set_rider_attributes');
  const raw = core._malloc(28); new Int32Array(core.HEAPU8.buffer, raw, 7).set(bytes); core._set_rider_attributes(raw, 0); core._free(raw); }
// Mid-run baselines: the glide seeds do not carry the idle clock (+0x35C, 115D48) or the retained speed limit
// (+0x2E4, 11B3F8); --seed-idle / --seed-limit copy them from the first record.
if (!eventMode) { const r0 = (off) => dv.getFloat32(32 + off - 0x100, true);
  if (args.includes('--seed-idle') && core._idle_clock_seed) core._idle_clock_seed(r0(0x35c));
  // Record 0's +0x2E4 is the limit tick 0 uses (11B3F8 runs before the provider hook): held, so tick 0 does not step it again.
  if (args.includes('--seed-limit') && core._speed_limit_seed) (core._speed_limit_seed_held ?? core._speed_limit_seed)(r0(0x2e4)); }
// --event also seeds it after the browser's own countdown pre-roll: the HUD bank keeps stale words from the front end
// (field10 of slots 5/6/7/0x19 and of reused slots), which the original never clears.
if (scoreMode && (!eventMode || !args.includes('--no-score-seed'))) {
  const words = new Uint32Array(SCORE_WORDS + HUD_WORDS);
  for (let k = 0; k < SCORE_WORDS; k++) words[k] = dv.getUint32(scoreLayout + 4 * k, true);
  for (let k = 0; k < HUD_WORDS; k++) words[SCORE_WORDS + k] = dv.getUint32(hudLayout + 4 * k, true);
  const ptr = core._malloc(words.byteLength); core.HEAPU8.set(new Uint8Array(words.buffer), ptr); core._score_object_seed(ptr, records[0].tick + Number(process.env.SCORE_TICK_OFFSET || 0)); core._free(ptr);
  // HUD slot 0x19 holds the character block's cash (150960): the core's copy that 119EF8 awards add to
  core._set_score_career_cash?.(dv.getInt32(hudLayout + 24 * 0x19 + 20, true));
}
const HUD_FIELDS = ['type', 'maximum', 'value', 'arg', 'field10', 'points'];
function compareScore(recordIndex, tick) {
  const web = new Uint32Array(core.HEAPU8.buffer, core._score_object_dump(), SCORE_WORDS + HUD_WORDS);
  const base = recordIndex * RECORD; let exact = true;
  const show = (off, bits) => SCORE_FLOATS.has(off) ? new Float32Array(new Uint32Array([bits]).buffer)[0] : bits | 0;
  for (let k = 0; k < SCORE_WORDS; k++) { const off = 4 * k; if (SCORE_SKIP.has(off)) continue;
    const pv = dv.getUint32(base + scoreLayout + off, true), wv = web[k]; if (pv === wv) continue; exact = false;
    const key = 'score+0x' + off.toString(16); if (!(key in scoreFirst)) scoreFirst[key] = { tick, web: show(off, wv), ps2: show(off, pv) }; }
  for (let k = 0; k < HUD_WORDS; k++) { const slot = Math.floor(k / 6), field = k % 6;
    const pv = dv.getUint32(base + hudLayout + 4 * k, true), wv = web[SCORE_WORDS + k]; if (pv === wv) continue;
    const ptype = dv.getInt32(base + hudLayout + 24 * slot, true), wtype = web[SCORE_WORDS + slot * 6] | 0;
    if (field > 0 && ptype === 0x34 && wtype === 0x34) continue; // stale fields of a free slot
    exact = false; const key = `hud${slot}.${HUD_FIELDS[field]}`;
    const f = (b) => (field === 1 || field === 2) ? new Float32Array(new Uint32Array([b]).buffer)[0] : b | 0;
    if (!(key in scoreFirst)) scoreFirst[key] = { tick, web: f(wv), ps2: f(pv), ps2Type: ptype, webType: wtype }; }
  if (process.env.SCORE_TRACE) { const [a, b] = process.env.SCORE_TRACE.split(':').map(Number); if (tick >= a && tick <= (b ?? a)) {
    const fl = (x) => new Float32Array(new Uint32Array([x]).buffer)[0]; const offs = (process.env.SCORE_FIELDS || '0x0,0x8,0x14,0x34,0x38,0x9c,0xa0,0xa4').split(',').map(Number);
    console.error('score', tick, offs.map((o) => `${o.toString(16)}:${SCORE_FLOATS.has(o) ? fl(web[o / 4]).toPrecision(6) : web[o / 4] | 0}/${SCORE_FLOATS.has(o) ? fl(dv.getUint32(base + scoreLayout + o, true)).toPrecision(6) : dv.getInt32(base + scoreLayout + o, true)}`).join(' ')); } }
  if (process.env.HUD_TRACE) { const [a, b] = (process.env.HUD_TRACE_TICKS || '0:99999').split(':').map(Number); if (tick >= a && tick <= b) { const slots = process.env.HUD_TRACE.split(',').map(Number);
    const fl = (x) => new Float32Array(new Uint32Array([x]).buffer)[0];
    console.error('hud', tick, slots.map((k) => { const w = [0, 1, 2, 3, 4, 5].map((j) => web[SCORE_WORDS + 6 * k + j]), p = [0, 1, 2, 3, 4, 5].map((j) => dv.getUint32(base + hudLayout + 24 * k + 4 * j, true));
      return `${k}:[${w[0] | 0},${fl(w[1]).toPrecision(4)},${fl(w[2]).toPrecision(4)},${w[3] | 0},${w[5] | 0}]/[${p[0] | 0},${fl(p[1]).toPrecision(4)},${fl(p[2]).toPrecision(4)},${p[3] | 0},${p[5] | 0}]`; }).join(' ')); } }
  if (exact) scoreTicksExact++; else if (!firstScoreMismatch) firstScoreMismatch = tick;
  return exact;
}
const syncVisual = (() => { if (!args.includes('--sync-visual-rng')) return -1; let offset = 0; for (const w of captureManifest.layout?.watches || []) { if (Number(w.address) === 0x4FF018) return offset; offset += w.length; } throw new Error('--sync-visual-rng needs a capture watching 0x4ff018:0x18'); })();
const seedVisual = (() => { if (!args.includes('--seed-visual-rng')) return -1; let offset = 0; for (const w of captureManifest.layout?.watches || []) { if (Number(w.address) === 0x4FF018) return offset; offset += w.length; } throw new Error('--seed-visual-rng needs a capture watching 0x4ff018:0x18'); })();
let visualRngTicksExact = 0, firstVisualRngMismatch = null;
const visualWatch = (() => { let offset = 0; for (const w of captureManifest.layout?.watches || []) { if (Number(w.address) === 0x4FF018) return offset; offset += w.length; } return -1; })();
const cameraVariant = args.includes('--camera-variant') ? Number(args[args.indexOf('--camera-variant') + 1]) : 0;
const seedError = dist(seed.motion.slice(0, 3), records[0].position);
// --sync-rng: the PS2's computer riders also draw from the shared generator (4FF030) that the
// browser's single rider owns, so copy the recorded state in before each tick (captures >= 16 KiB).
const syncRng = args.includes('--sync-rng') && RECORD >= 16384;
// Shared generator 0x317810 (engine/original_random.hpp) on a live word view.
function rngNext(w) { let value = (w[5] + w[4]) >>> 0; let carry = (value < w[5] || value < w[4]) ? 1 : 0; w[4] = value;
  for (let k = 3; k >= 1; --k) { const nv = (value + w[k] + carry) >>> 0; carry = nv < w[k] ? 1 : 0; value = nv; w[k] = value; }
  value = (value + w[0] + carry) >>> 0; w[0] = value; w[5] = (w[5] + 1) >>> 0;
  if (w[5] === 0) { let k = 4; while (k >= 1) { w[k] = (w[k] + 1) >>> 0; if (w[k] !== 0) break; --k; } if (k === 0) { value = (value + 1) >>> 0; w[0] = value; } }
  return value; }
// Within a PS2 frame the computer riders draw between the human's controller-phase draws and its
// motion-phase draws (event-race 1472: the get-up reaction variant is the tick's first draw; 899: the
// landing-crash variant is the third, after two opponent draws). Each tick starts from the recorded
// state and the core defers (PS2 draws - browser draws) until its first motion-phase draw
// (_animation_rng_defer, ControllerDraws in animation_bridge.cpp). The browser's per-tick draw counts
// come from earlier child passes (iterated until they agree). RNG_NO_ALIGN=1 disables this;
// RNG_DRAWS_DUMP=file writes the final pass's browser draw counts.
const rngWords = (i) => Array.from({ length: 6 }, (_, k) => dv.getUint32(i * RECORD + 8896 + 4 * k, true));
function drawsBetween(a, b) { const w = a.slice(); for (let n = 0; n <= 64; n++) { if (w.every((x, k) => x === b[k])) return n; rngNext(w); } return -1; }
const ps2Draws = syncRng ? records.slice(0, -1).map((_, i) => drawsBetween(rngWords(i), rngWords(i + 1))) : [];
let webDrawsIn = null;
// CAPTURE.rng-order.json (web/ps2-capture-ai.mjs --rng-order, from an --ai-state re-run of the same capture): the recorded
// pass of every draw. A computer rider's 121818 draws (route score 10D410) come after the human's 121750 draws (peak1/
// rnb-event-tuck 6601: landing variant 62 is the tick's first draw), so the deferral is the computer riders' draws before
// the first non-controller draw ('m') the browser replays; ticks without one keep the count rule below.
const rngOrderPath = capturePath.replace(/\.bin$/, '.rng-order.json');
const rngOrder = syncRng && fs.existsSync(rngOrderPath) ? JSON.parse(fs.readFileSync(rngOrderPath, 'utf8')).order : null;
const orderedSkip = (i) => { const o = rngOrder?.[i]; if (!o || o.length !== ps2Draws[i]) return null; const m = o.indexOf('m'); return m < 0 ? null : o.slice(0, m).split('O').length - 1; };
if (syncRng && process.env.__RNG_DRAWS_IN) webDrawsIn = JSON.parse(fs.readFileSync(process.env.__RNG_DRAWS_IN, 'utf8'));
else if (syncRng && !process.env.RNG_NO_ALIGN && !process.env.__RNG_DRAWS_OUT) {
  const { execFileSync } = await import('node:child_process');
  const tmp = `${(await import('node:os')).tmpdir()}/rng-draws-${process.pid}.json`; const childArgs = [new URL(import.meta.url).pathname, ...args.filter((a, k) => a !== '--report' && args[k - 1] !== '--report' && a !== '--trace' && args[k - 1] !== '--trace')];
  let previous = null;
  for (let pass = 0; pass < 6; pass++) {
    const env = { ...process.env, __RNG_DRAWS_OUT: tmp, }; for (const k of Object.keys(env)) if (/TRACE|BONE_SCAN|FIELD_DUMP|INSTANCE_EVENTS/.test(k)) delete env[k]; if (previous) { fs.writeFileSync(tmp + '.in', JSON.stringify(previous)); env.__RNG_DRAWS_IN = tmp + '.in'; }
    execFileSync(process.execPath, childArgs, { env, stdio: ['ignore', 'ignore', 'inherit'] });
    const counts = JSON.parse(fs.readFileSync(tmp, 'utf8')); const stable = (previous && counts.every((c, k) => c === previous[k])) || (!previous && counts.every((c, k) => !(ps2Draws[k] > c))); previous = counts; if (stable) break;
  }
  webDrawsIn = previous; for (const f of [tmp, tmp + '.in']) if (fs.existsSync(f)) fs.unlinkSync(f);
}
// --camera-seed: a mid-run baseline starts the web camera from the record-0 words (the algorithm is constructed on the
// first camera step, then these words replace its decoded fields; words the capture lacks keep their constructed values).
if (args.includes('--camera-seed') && core._camera_seed_words) {
  const words = new Uint32Array(CAMERA_WORDS), mask = new Uint8Array(CAMERA_WORDS); for (const [k, v] of ps2CameraWords(0)) { words[k] = v; mask[k] = 1; }
  const wp = core._malloc(4 * CAMERA_WORDS), mp = core._malloc(CAMERA_WORDS); core.HEAPU8.set(new Uint8Array(words.buffer), wp); core.HEAPU8.set(mask, mp); core._camera_seed_words(wp, mp); core._free(wp); core._free(mp);
}
const webDrawsOut = []; let webSkip = 0;
// PS2_ARITH=exact: a core with the arithmetic switch (tools/ps2-float/make_swap_tree.py) computes on the console model
// (engine/ps2_fpu.hpp) from here, the capture's first tick. The setup above ran in mode 1, as the baseline's own history did
// (docs/ps2-float.md "Mode-1 history").
if (process.env.PS2_ARITH === 'exact') {
  if (!core._ps2_arith_exact) {
    throw new Error('PS2_ARITH=exact needs a core with ps2_arith_exact');
  }
  core._ps2_arith_exact(1);
}
// TICK_HOOK=module.mjs: an observer module (create(ctx) -> {tick(t), summary()}) run after every compared tick, e.g.
// web/uber-audio-compare.mjs (audio dispatch vs the PS2 call log). Its summary is merged into the report.
const tickHook = process.env.TICK_HOOK ? await import(new URL(process.env.TICK_HOOK, `file://${process.cwd()}/`).href).then((m) => m.create({ core, dv, RECORD, records, captureManifest, capturePath, args })) : null;
// Boost/Tricky state (rider +0x2F8 meter, +0x2FC amount, +0x2F4 Uber tier, +0x2F0 Tricky/super time) bit for bit per tick.
let boostTicks = 0, boostTicksExact = 0, firstBoostMismatch = null, firstBoostAmountMismatch = null;
// --weather MAP.json (docs/weather.md; the capture watches tools/export_weather.py objects, as compare-ai-capture.mjs --weather):
// {rider, camera, flags?} = the watched addresses of the human's Weather painter object (block 0, 0xA0 bytes), the camera block 6
// painter and the flag manager +0x10 (wind, base, delta, timer). Record N = after tick N-1, compared with the browser before
// tick N; per field the ticks, exact ticks and the first difference (summary.weather).
const weatherMap = args.includes('--weather') ? JSON.parse(fs.readFileSync(args[args.indexOf('--weather') + 1], 'utf8')) : null;
const weatherStats = { ticks: {}, exact: {}, first: {} };
const weatherWatch = weatherMap ? (() => { const offs = new Map(); let off = 0; for (const w of captureManifest.layout?.watches || []) { offs.set(Number(w.address), off); off += w.length; }
  const o = (a) => (a === undefined || a === null ? -1 : offs.get(Number(a)) ?? -1); return { rider: o(weatherMap.rider), camera: o(weatherMap.camera), flags: o(weatherMap.flags), lightning: o(weatherMap.lightning ?? 0x4A4698) }; })() : null;
function compareWeather(i, tick) {
  const at = i * RECORD + captureManifest.layout.watch_offset, U = (k) => dv.getUint32(at + k, true), m = weatherWatch;
  const note = (key, web, ps2) => { weatherStats.ticks[key] = (weatherStats.ticks[key] || 0) + 1; if (web === ps2) { weatherStats.exact[key] = (weatherStats.exact[key] || 0) + 1; return; } if (!weatherStats.first[key]) weatherStats.first[key] = { tick, web, ps2 }; };
  const Fb = (arr, k) => { const b = new Float32Array(1); b[0] = arr[k]; return new Uint32Array(b.buffer)[0]; };
  if (m.rider >= 0 && core._weather_painter_info) { const w = f32(core._weather_painter_info(), 40);
    for (let k = 0; k < 19; k++) note(`rider.cur${k}`, Fb(w, 2 + k), U(m.rider + 8 + 8 * k)); note('rider.distance', Fb(w, 1), U(m.rider)); }
  if (m.camera >= 0 && core._weather_info) { const w = f32(core._weather_info(), 30);
    for (let k = 0; k < 19; k++) note(`camera.cur${k}`, Fb(w, 11 + k), U(m.camera + 8 + 8 * k)); note('camera.distance', Fb(w, 10), U(m.camera)); }
  if (m.lightning >= 0 && core._weather_lightning) { const L = f32(core._weather_lightning(), 10); // ScreenTint gp+0x15A8 counter, +0x15C0 distance, +0x15C4 thunder delay
    note('lightning.counter', L[0] | 0, dv.getInt32(at + m.lightning, true)); note('lightning.distance', Fb(L, 5), U(m.lightning + 0x18)); note('lightning.delay', L[6] | 0, dv.getInt32(at + m.lightning + 0x1C, true)); }
  if (m.flags >= 0 && core._stage_flag_wind) { const w = f32(core._stage_flag_wind(), 5); ['wind', 'base', 'delta', 'timer'].forEach((n, k) => note(`flags.${n}`, Fb(w, k), U(m.flags + 4 * k))); }
  if (core._weather_region_info) { const r = new Int32Array(core.HEAPU8.buffer, core._weather_region_info(), 5); const patch = dv.getInt32(i * RECORD + 32 + 0x430 - 0x100, true); weatherStats.region ??= []; const last = weatherStats.region[weatherStats.region.length - 1];
    if (!last || last[1] !== r[0] || last[2] !== (patch === -1 ? -1 : patch & 0xFF)) weatherStats.region.push([tick, r[0], patch === -1 ? -1 : patch & 0xFF]); } // [tick, web gp+0x770, PS2 rider+0x430 track] at changes
}
// SNAPSHOT_KEEP_CHECK (QA, docs/replay.md §2a): the rider-context snapshot saved before the first tick and restored after the last
// (web/event-snapshot.js with its QA checks): a kept table (web/snapshot-policy.mjs) that this run changed, or a hook whose world no
// longer matches, fails the restore. Needs a core built with SSX_SNAPSHOT=1.
const snapshotKeepCheck = process.env.SNAPSHOT_KEEP_CHECK ? await import('./event-snapshot.js') : null;
if (snapshotKeepCheck) { snapshotKeepCheck.snapshotAttach({ human: core, qa: true }); globalThis.__keepCheckState = snapshotKeepCheck.snapshotSave(0, { human: core }); }
for (let i = 0; i + 1 < records.length; i++) {
  if (inWorld && inWorldSetup.ctmInWorldBeforeTick(core, inWorld, i, { cstr: (t) => put(Buffer.from(t + '\0')), cfile: str }) === 'skip') { if (syncRng) webDrawsOut.push(0); continue; } // WS1's last tick
  if (weatherWatch) compareWeather(i, records[i].tick);
  let rngStart = null;
  if (syncRng) { const w = new Uint32Array(core.HEAPU8.buffer, core._animation_rng_words(), 6); for (let k = 0; k < 6; k++) w[k] = dv.getUint32(i * RECORD + 8896 + 4 * k, true);
    const skip = webSkip = webDrawsIn && ps2Draws[i] > 0 ? orderedSkip(i) ?? Math.max(0, ps2Draws[i] - (webDrawsIn[i] || 0)) : 0; if (process.env.DRAW_TRACE) { const [a, b] = process.env.DRAW_TRACE.split(':').map(Number); if (records[i].tick >= a && records[i].tick <= (b ?? a)) console.error('draws', records[i].tick, 'ps2', ps2Draws[i], 'web', webDrawsIn?.[i], 'skip', skip); } rngStart = Array.from(w); if (core._animation_rng_defer) core._animation_rng_defer(skip); else { for (let k = 0; k < skip; k++) rngNext(w); rngStart = Array.from(w); } }
  if (process.env.VISUAL_TRACE && core._visual_rng_words) { const now = Array.from(new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6)); const [a, b] = process.env.VISUAL_TRACE.split(':').map(Number);
    const vw = syncVisual >= 0 || (captureManifest.layout?.watches || []).some((w) => Number(w.address) === 0x4FF018) ? (() => { let off = 0; for (const w of captureManifest.layout.watches) { if (Number(w.address) === 0x4FF018) return off; off += w.length; } })() : -1;
    const ps2At = (r) => vw >= 0 ? Array.from({ length: 6 }, (_, k) => dv.getUint32(r * RECORD + captureManifest.layout.watch_offset + vw + 4 * k, true)) : null;
    if (globalThis.__visStart && records[i - 1] && records[i - 1].tick >= a && records[i - 1].tick <= (b ?? a)) console.error('visualtotal', records[i - 1].tick, 'web', drawsBetween(globalThis.__visStart, now), 'phases', JSON.stringify(globalThis.__visPhases), 'camera', globalThis.__visTrace ? drawsBetween(globalThis.__visTrace[0], globalThis.__visTrace[1]) : '?', 'ps2', vw >= 0 ? drawsBetween(ps2At(i - 1), ps2At(i)) : '?', 'web start = ps2 start', vw >= 0 ? globalThis.__visStart.every((x, k) => x === ps2At(i - 1)[k]) : '?');
    globalThis.__visStart = now; }
  // --sync-visual-rng: copy the recorded visual RNG (0x4FF018; snow, stage effects and camera shake draw from it) at the
  // tick start, like --sync-rng does for the shared generator, when the capture watches it (--watch 0x4ff018:0x18).
  // --seed-visual-rng: copy the recorded visual RNG once, at the first record (a mid-run baseline's stream state); from then
  // on every consumer (snow, stage effects, camera shake) must draw in the PS2 order for the words to stay equal.
  // The first compared tick is skipped: a mid-run baseline starts the browser's effects fresh (their start-up draws are not
  // the PS2's running emitters), so the stream is seeded at the start of the second tick.
  if (i === 1 && seedVisual >= 0 && core._visual_rng_words) { const w = new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6), rec = Array.from({ length: 6 }, (_, k) => dv.getUint32(RECORD + captureManifest.layout.watch_offset + seedVisual + 4 * k, true));
    globalThis.__visualSeedOnce = rec; } // applied after the next race_begin (padFrame), the record point, as --sync-visual-rng
  if (i === 0 && seedVisual >= 0 && process.env.VISUAL_CHECK) globalThis.__visualProbe = Array.from({ length: 6 }, (_, k) => dv.getUint32(captureManifest.layout.watch_offset + seedVisual + 4 * k, true));
  // The record is taken at the human provider exit, after this tick's world object pass (stage effects, 0x354F98): the
  // words are copied after race_begin (the browser's world pass), where the two streams meet (padFrame).
  globalThis.__visTickNow = records[i].tick;
  if (process.env.SURFACE_TRACE) { const [a, b] = process.env.SURFACE_TRACE.split(':').map(Number); if (records[i].tick >= a && records[i].tick <= b) console.error('surface', records[i].tick, 'ps2 rider+0x438', dv.getInt32(i * RECORD + 32 + 0x438, true), 'web spark438', core._impact_fx_info ? f32(core._impact_fx_info(), 17)[10] : '?', 'web ground', f32(core._rider_state(), 16)[8]); }
  // LCG_SPAN=a:b: PS2 LCG steps between consecutive records vs the browser's over the same iteration.
  if (process.env.LCG_SPAN && core._visual_lcg_word) { let off = -1, o = 0; for (const w of captureManifest.layout?.watches || []) { if (Number(w.address) === 0x4A3AFC) off = o; o += w.length; }
    const webl = new Uint32Array(core.HEAPU8.buffer, core._visual_lcg_word(), 1)[0], ps2l = off >= 0 ? dv.getUint32(i * RECORD + captureManifest.layout.watch_offset + off, true) : 0;
    const steps = (a, b) => { let w = a, n = 0; while (w !== b && n < 200) { w = ((Math.imul(w, 0x18FCD) + 0xE9507C) & 0x7FFFFF) | 0x3F800000; n++; } return n < 200 ? n : -1; };
    const [a, b] = process.env.LCG_SPAN.split(':').map(Number);
    if (globalThis.__lcgPrev && records[i - 1].tick >= a && records[i - 1].tick <= b) { const mk = core._fx_lcg_marks ? Array.from(new Uint32Array(core.HEAPU8.buffer, core._fx_lcg_marks(), 5)) : null; console.error('lcgspan', records[i - 1].tick, 'web', steps(globalThis.__lcgPrev[0], webl), 'ps2', steps(globalThis.__lcgPrev[1], ps2l), 'fx', mk ? JSON.stringify([steps(globalThis.__lcgPrev[0], mk[0]), steps(mk[0], mk[1]), steps(mk[1], mk[2]), steps(mk[2], mk[3]), steps(mk[3], mk[4]), steps(mk[4], webl)]) : '', 'snow', core._snow_lcg_marks ? JSON.stringify((() => { const sm = Array.from(new Uint32Array(core.HEAPU8.buffer, core._snow_lcg_marks(), 9)); return sm.slice(1).map((x, k) => steps(sm[k], x)); })()) : ''); }
    globalThis.__lcgPrev = [webl, ps2l]; }
  // LCG_AT=a:b: print the browser's visual LCG gp+0xA0C at those records (captures without the 0x4a3afc watch).
  if (process.env.LCG_AT && core._visual_lcg_word) { const [a, b] = process.env.LCG_AT.split(':').map(Number); if (records[i].tick >= a && records[i].tick <= (b ?? a)) console.error('lcgat', records[i].tick, new Uint32Array(core.HEAPU8.buffer, core._visual_lcg_word(), 1)[0].toString(16)); }
  // LCG_CHECK=1: the shared visual LCG gp+0xA0C (watched 0x4a3afc:4) against the browser's at every record (tick start).
  if (process.env.LCG_CHECK && core._visual_lcg_word && !globalThis.__lcgBad) { let off = -1, o = 0; for (const w of captureManifest.layout?.watches || []) { if (Number(w.address) === 0x4A3AFC) off = o; o += w.length; }
    if (off >= 0) { const ps2l = dv.getUint32(i * RECORD + captureManifest.layout.watch_offset + off, true), webl = new Uint32Array(core.HEAPU8.buffer, core._visual_lcg_word(), 1)[0];
      if (i >= 1 && ps2l !== webl) { globalThis.__lcgBad = true; let w = webl, n = 0; while (w !== ps2l && n < 64) { w = ((Math.imul(w, 0x18FCD) + 0xE9507C) & 0x7FFFFF) | 0x3F800000; n++; } console.error('lcg differs at record', records[i].tick, 'web', webl.toString(16), 'ps2', ps2l.toString(16), 'web behind by', n < 64 ? n : '?'); } } }
  // VISUAL_CHECK=1 (with --seed-visual-rng): first record whose 0x4FF018 words differ from the browser's at the same point
  // (after the next tick's race_begin, where --sync-visual-rng would copy them).
  if (process.env.VISUAL_CHECK && seedVisual >= 0 && i >= 2 && globalThis.__visAfterBegin && !globalThis.__visFirstBad) {
    const ps2w = Array.from({ length: 6 }, (_, k) => dv.getUint32(i * RECORD + captureManifest.layout.watch_offset + seedVisual + 4 * k, true));
    if (ps2w.some((x, k) => x !== globalThis.__visAfterBegin[k])) { globalThis.__visFirstBad = records[i].tick; console.error('visual words differ from record', records[i].tick, 'web', drawsBetween(ps2w, globalThis.__visAfterBegin), 'draws ahead of the PS2 (negative: behind)', 'or', drawsBetween(globalThis.__visAfterBegin, ps2w)); }
  }
  globalThis.__visualSyncWords = syncVisual >= 0 && core._visual_rng_words ? Array.from({ length: 6 }, (_, k) => dv.getUint32(i * RECORD + captureManifest.layout.watch_offset + syncVisual + 4 * k, true)) : null;
  let cmd, web;
  peakWorld?.beforeTick(i); // the capture's streaming rows and path-bank events for this tick
  if (stationHold) { // this tick's check of builtin 68, before its section pass
    const q = (off) => dv.getFloat32((i + 1) * RECORD + 32 + off - 0x100, true), next = i + 1 < records.length ? [q(0x110), q(0x114), q(0x118), q(0x1B0), q(0x1B4)] : null;
    globalThis.__beforeSectionPass = () => { if (!stationFired() || !next) return; if (process.env.STATION_HOLD_EARLY) { core._nis_hold(1, ...next); console.error('station hold (early) inside tick', records[i].tick); } else stationPending = next; }; }
  if (cameraVariant && i === 0) core._camera_variant_qa(cameraVariant); // before the first camera step (the seed or the construction uses it)
  if (padMode) {
    core.HEAPF32.set(padFor(records[i].index), padPtr >> 2);
    const out = Array.from(f32(core._pad_tick(padPtr), 24));
    const w0 = (out[15] + out[16] * 65536) >>> 0, w1 = (out[17] + out[18] * 65536) >>> 0;
    if ((w0 !== records[i].word0 || w1 !== records[i].word1 || out[14] !== records[i].control) && !firstWordMismatch)
      firstWordMismatch = { tick: records[i].tick, webControl: out[14], ps2Control: records[i].control, web: [w0.toString(16), w1.toString(16)], ps2: [records[i].word0.toString(16), records[i].word1.toString(16)] };
    cmd = { state: out[14], turn: out[0], crouch: out[1], brake: out[2], board: out[3], spin: out[4], flip: out[5], jump: out[6], boost: out[7], grab: out[8], tweak: out[12], animTurn: out[10], animBrake: out[11], animFlip: out[13] };
    boothTick = records[i].tick; web = padFrame(cmd);
  } else {
    cmd = decode(records[i].control, records[i].word0, records[i].word1);
    if (cmd.unsupported && !unsupported) unsupported = { index: i, tick: records[i].tick, why: cmd.unsupported, words: [records[i].word0.toString(16), records[i].word1.toString(16)] };
    if (cmd.unsupported) break;
    boothTick = records[i].tick; web = frame(cmd);
  }
  if (inWorld && inWorldSetup.ctmInWorldAfterTick(core, inWorld, i)) { if (rngStart) webDrawsOut.push(Math.max(0, drawsBetween(rngStart, Array.from(new Uint32Array(core.HEAPU8.buffer, core._animation_rng_words(), 6))))); continue; } // WS1's hold: not compared
  const ps2 = records[i + 1];
  // RUMBLE_TRACE=1: this tick's rumble-source audio events (web/audio_events.hpp) beside the recorded owner +0xDFC / +0xE00 (pad motor intensities, web/rumble.js)
  if (process.env.RUMBLE_TRACE && core._audio_events) { const q = core._audio_events() >> 2, F = new Float32Array(core.HEAPU8.buffer), n = F[q], ev = []; for (let k = 0; k < n; k++) ev.push(Array.from(F.subarray(q + 1 + 5 * k, q + 6 + 5 * k)).map((x) => +x.toFixed(3))); core._audio_events_clear(); const o = (i + 1) * RECORD + captureManifest.layout.owner_de0_e00;
    console.error('rumble', ps2.tick, ps2.control, +dv.getFloat32(o + 0x1c, true).toFixed(3), +dv.getFloat32(o + 0x20, true).toFixed(3), JSON.stringify(ev), +Math.hypot(...records[i].velocity).toFixed(2), +Math.hypot(...ps2.velocity).toFixed(2), +f32(core._collision_reaction_info(), 10)[8].toFixed(3), +f32(core._crash_info(), 12)[11].toFixed(3), f32(core._crash_info(), 12)[1], ...(captureManifest.layout?.watches?.length >= 1 && captureManifest.layout.watches[0].length >= 8 ? [0, 4].map((k) => +dv.getFloat32((i + 1) * RECORD + captureManifest.layout.watch_offset + k, true).toFixed(3)) : [])); } // + the first --watch window's two floats (rumble captures: owner +0xDFC, +0xE00)
  // Rider +0x100 (finish celebration) is cleared by the event's results handler at the finish (125108 -> 238358 ->
  // 0x239230/0x23A760 place rules): the host decides it, so mirror the recorded value into finish_celebrate.
  if (core._finish_celebrate) { const flag = dv.getInt32((i + 1) * RECORD + 32 + 0x100 - 0x100, true); if (flag !== globalThis.__celebrate) { globalThis.__celebrate = flag; core._finish_celebrate(flag ? 1 : 0); } }
  if (process.env.MAGNET_TRACE && core._stage_world_magnets) { const q = core._stage_world_magnets() >> 2, F = new Float32Array(core.HEAPU8.buffer), n = F[q]; const want = +process.env.MAGNET_TRACE;
    for (let k = 0; k < n; k++) { const m = Array.from(F.subarray(q + 1 + 10 * k, q + 8 + 10 * k)); if (m[0] === want && ps2.tick >= +(process.env.MAGNET_FROM || 0) && ps2.tick <= +(process.env.MAGNET_TO || 1e9)) console.error('magnet', ps2.tick, JSON.stringify(m.map((x) => +x.toFixed(2))), 'rider', JSON.stringify(Array.from(f32(core._rider_state(), 3)).map((x) => +(x * 100).toFixed(1)))); } }
  if (process.env.POSE_DUMP && process.env.POSE_DUMP.split(',').map(Number).includes(ps2.tick)) console.error('pose', ps2.tick, JSON.stringify(Array.from(f32(core._pose_physical(), 12))), JSON.stringify(Array.from(f32(core._world_pose_bones(), 1 + 7 * 22)).filter((_, k) => [0, 5, 10, 15, 18, 21].some((b) => k >= 1 + 7 * b && k < 4 + 7 * b))));
  if (stageSnaps?.has(ps2.tick)) stageRows.push(compareStageWorld(core, stageSnaps.get(ps2.tick), { allDiffs: !!process.env.STAGE_WORLD_ALLDIFFS }));
  if (rngStart) { let d = drawsBetween(rngStart, Array.from(new Uint32Array(core.HEAPU8.buffer, core._animation_rng_words(), 6))); const pending = core._animation_rng_pending ? core._animation_rng_pending() : 0; if (core._animation_rng_defer) d -= Math.max(0, webSkip - pending); webDrawsOut.push(Math.max(0, d)); }
  // Frame-begin 11B3F8 runs before the provider: record i holds the limit used by tick i. physics_info [6] is the exact cm/s
  // word ([0] is m/s, which rounds when scaled back); an older core has something else there.
  const pinfo = f32(core._physics_info(), 7), webLimit = Math.abs(pinfo[6] - pinfo[0] * 100) < 1 ? pinfo[6] : pinfo[0] * 100, ps2Limit = records[i].speedLimit;
  if (process.env.BONE_SCAN && (!globalThis.__boneDone || process.env.BONE_SCAN === 'all')) { const wb = f32(core._world_pose_bones(), 1 + 32 * 7); const n = wb[0]; const base = (i + 1) * RECORD + 3264; const bad = [];
    for (let b = 0; b < Math.min(n, +(process.env.BONE_SCAN_MAX || 29)); b++) for (let k = 0; k < 7; k++) { const pb = boneSlot(b, n); const pv = dv.getFloat32(base + 32 * pb + (k < 3 ? k * 4 : 16 + (k - 3) * 4), true), wv = wb[1 + b * 7 + k]; if (Math.fround(wv) !== pv) bad.push([b, k, wv, pv]); }
    if (bad.length) { console.error('bones', ps2.tick, JSON.stringify(bad.slice(0, 8))); globalThis.__boneDone = true; } }
  if (process.env.PROBE_TRACE && process.env.PROBE_TRACE.split(',').map(Number).includes(ps2.tick)) { const wb = f32(core._world_pose_bones(), 1 + 32 * 7); console.error('probe', ps2.tick, JSON.stringify(Array.from(f32(core._landing_probe_info(), 17))), 'webBone22', JSON.stringify(Array.from(wb.slice(1 + 22 * 7, 1 + 22 * 7 + 3)))); }
  // BONE_DUMP=ticks BONE_DUMP_BONES=22,23: full web vs PS2 world bone transforms (position, quaternion).
  if (process.env.BONE_DUMP && process.env.BONE_DUMP.split(',').map(Number).includes(ps2.tick)) { const wb = f32(core._world_pose_bones(), 1 + 32 * 7); for (const b of (process.env.BONE_DUMP_BONES || '22').split(',').map(Number)) console.error('bone', ps2.tick, b, 'web', Array.from(wb.slice(1 + b * 7, 8 + b * 7)).join(','), 'ps2', [0, 1, 2, 4, 5, 6, 7].map((k) => dv.getFloat32((i + 1) * RECORD + 3264 + 32 * b + 4 * k, true)).join(',')); }
  // BOUNDS_TRACE=1: web rider query bounds (11E150, rider+0x400/+0x410) after the step vs the record; prints the differing ticks.
  if (process.env.BOUNDS_TRACE) { const b = f32(core._rider_query_bounds(), 7), w = [b[0], b[1], b[2], b[4], b[5], b[6]], p = [0x400, 0x404, 0x408, 0x410, 0x414, 0x418].map((o) => dv.getFloat32((i + 1) * RECORD + 32 + o - 0x100, true)); if (w.some((x, k) => x !== p[k])) console.error('bounds', ps2.tick, 'web', w.join(','), 'ps2', p.join(',')); }
  if (process.env.ROUTE_TRACE && process.env.ROUTE_TRACE.split(',').map(Number).includes(ps2.tick)) console.error('route', ps2.tick, JSON.stringify(Array.from(f32(core._route_info(), 8))), 'ps2 4CC', dv.getFloat32((i + 1) * RECORD + 32 + 0x4cc - 0x100, true));
  if (process.env.LIMIT_TRACE && ps2.tick >= +process.env.LIMIT_TRACE.split(':')[0] && ps2.tick <= +(process.env.LIMIT_TRACE.split(':')[1] ?? process.env.LIMIT_TRACE)) console.error('limit', records[i].tick, webLimit, ps2Limit, JSON.stringify(Array.from(f32(core._physics_info(), 6))));
  if (Math.fround(webLimit) !== ps2Limit && firstLimit === null) firstLimit = { tick: records[i].tick, control: records[i].control, web: Math.fround(webLimit), ps2: ps2Limit };
  // A course seed's retained limit can differ from the savestate's (the countdown anchors' pre-race history); 11B3F8's filter
  // (x 0.97 / 0.9 a tick) then reaches the PS2's bits within ~130 ticks. limitAgreed: the first equal tick; firstLimitDivergence:
  // the first difference after it (test-ps2-captures gates that one).
  if (Math.fround(webLimit) === ps2Limit) { if (limitAgreed === null) { limitAgreed = records[i].tick; limitAgreedRow = rows.length; } }
  else if (limitAgreed !== null && firstLimitDivergence === null) firstLimitDivergence = { tick: records[i].tick, control: records[i].control, web: Math.fround(webLimit), ps2: ps2Limit };
  const posErr = dist(web.motion.slice(0, 3), ps2.position);
  const velErr = dist(web.motion.slice(3, 6), ps2.velocity);
  const exact = web.motion[0] === ps2.position[0] && web.motion[1] === ps2.position[1] && web.motion[2] === ps2.position[2] && web.motion[3] === ps2.velocity[0] && web.motion[4] === ps2.velocity[1] && web.motion[5] === ps2.velocity[2];
  const webControl = web.motion[11], webGround = web.motion[10];
  const row = { tick: ps2.tick, control: ps2.control, webControl, mode: ps2.mode, webGround, posErrCm: posErr, velErrCmps: velErr, exact,
    turn: [web.motion[12], ps2.turn], brake: [web.motion[13], ps2.brake], crouch: [web.motion[14], ps2.crouch], semantic: web.info[0] };
  if (web.cam) { const eye = [web.cam[0] * 100, -web.cam[2] * 100, web.cam[1] * 100], look = [web.cam[3] * 100, -web.cam[5] * 100, web.cam[4] * 100];
    row.camEyeErrCm = dist(eye, ps2.camEye); row.camLookErrCm = dist(look, ps2.camLook); row.fov = web.cam[6];
    row.finalEyeErrCm = dist(eye, ps2.finalEye); row.finalLookErrCm = dist(look, ps2.finalLook); row.ps2Lift = ps2.lift; row.webFovNearFar = web.cam.slice(6, 9); row.ps2FovNearFar = ps2.fovNearFar;
    row.algoEyeErrCm = dist(web.algo.slice(0, 3), ps2.camEye); row.algoLookErrCm = dist(web.algo.slice(3, 6), ps2.camLook); }
  // CAM_INPUT_TRACE=1: camera head input (11E0xx head bone, rider+0x89C) and velocity vs the record, first differing ticks.
  if (process.env.CAM_INPUT_TRACE && web.head) { const rr = (o) => dv.getFloat32((i + 1) * RECORD + 32 + o - 0x100, true); const bone = +(process.env.CAM_HEAD_BONE || 22), bb = (i + 1) * RECORD + 3264 + 32 * bone, ph = [dv.getFloat32(bb, true), dv.getFloat32(bb + 4, true), dv.getFloat32(bb + 8, true)];
    if (ph.some((x, k) => Math.fround(web.head[k]) !== x) || web.camIn.slice(8, 11).some((x, k) => x !== ps2.velocity[k])) { globalThis.__camIn = (globalThis.__camIn || 0) + 1; if (globalThis.__camIn <= +process.env.CAM_INPUT_TRACE ) console.error('camin', ps2.tick, 'head', web.head.join(','), '|', ph.join(','), 'vel', web.camIn.slice(8, 11).join(','), '|', ps2.velocity.join(','), 'mode', web.camIn[28]); } }
  if (process.env.VISUAL_TRACE && globalThis.__visTrace && globalThis.__visStart) { const [a, b] = process.env.VISUAL_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('visual', ps2.tick, 'draws before camera', drawsBetween(globalThis.__visStart, globalThis.__visTrace[0]), 'camera', drawsBetween(globalThis.__visTrace[0], globalThis.__visTrace[1])); }
  if (process.env.TRAIL_TRACE && core._trail_info) { const [a, b] = process.env.TRAIL_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('trail', ps2.tick, JSON.stringify(Array.from(f32(core._trail_info(), 16)).map((x) => +x.toFixed(3)))); }
  if (process.env.SNOW_TRACE) { const [a, b] = process.env.SNOW_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('snow', ps2.tick, JSON.stringify(Array.from(f32(core._snow_info(), 23)).map((x) => +x.toFixed(2))), 'crashFrame', f32(core._crash_info(), 12)[1], 'phases', JSON.stringify(globalThis.__visPhases)); }
  if (process.env.CAM_IN_DUMP) { const [a, b] = process.env.CAM_IN_DUMP.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('camin', ps2.tick, 'director', JSON.stringify(Array.from(f32(core._camera_director_info(), 8))), JSON.stringify(web.camIn), 'quat web', web.quat.join(','), 'ps2', ps2.quat.join(','), 'ps2 normal', ps2.normal.join(',')); }
  if (visualWatch >= 0 && core._visual_rng_words && ((seedVisual >= 0 && i >= 1) || eventMode)) { const w = new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6); let same = true; for (let k = 0; k < 6; k++) if (w[k] !== dv.getUint32((i + 1) * RECORD + captureManifest.layout.watch_offset + visualWatch + 4 * k, true)) same = false;
    if (same) visualRngTicksExact++; else if (!firstVisualRngMismatch) firstVisualRngMismatch = { tick: ps2.tick }; }
  if (web.camWords) { let diffs = []; for (const [k, v] of ps2CameraWords(i + 1)) if (web.camWords[k] !== v) diffs.push(k);
    // --camera-shake-sync: a crash shake start (15E668 -> 1656B0, 12 visual-RNG draws) whose walk differs only because the
    // visual RNG stream differs (world effects draw from it too): adopt the PS2 compositor words and count the tick apart.
    if (shakeSync && diffs.length && diffs.every((k) => k >= 228) && diffs.some((k) => k >= 245 && k < 251)) {
      const words = new Uint32Array(CAMERA_WORDS), mask = new Uint8Array(CAMERA_WORDS); for (const [k, v] of ps2CameraWords(i + 1)) if (k >= 228) { words[k] = v; mask[k] = 1; }
      const wp = core._malloc(4 * CAMERA_WORDS), mp = core._malloc(CAMERA_WORDS); core.HEAPU8.set(new Uint8Array(words.buffer), wp); core.HEAPU8.set(mask, mp); core._camera_seed_words(wp, mp); core._free(wp); core._free(mp);
      cameraShakeRngTicks.push(ps2.tick); diffs = []; }
    cameraWordTicks++; if (!diffs.length) cameraWordTicksExact++;
    const fl = (b) => new Float32Array(new Uint32Array([b]).buffer)[0], show = (k, v) => (k >= 0x2C0 / 4 && k < 0x304 / 4) || k === 0x37C / 4 || (k >= 0x304 / 4 && k < 0x340 / 4 && (k - 0x304 / 4) % 3 === 0) || [241, 243, 244, 270].includes(k) ? v | 0 : fl(v);
    const ps2Words = new Map(ps2CameraWords(i + 1));
    for (const k of diffs) { const name = cameraWordName(k); if (!(name in cameraWordFirst)) cameraWordFirst[name] = { tick: ps2.tick, web: show(k, web.camWords[k]), ps2: show(k, ps2Words.get(k)) }; }
    if (diffs.length && !firstCameraWordMismatch) firstCameraWordMismatch = { tick: ps2.tick, words: diffs.slice(0, 12).map((k) => [cameraWordName(k), show(k, web.camWords[k]), show(k, ps2Words.get(k))]) };
    if (process.env.CAM_TRACE) { const [a, b] = process.env.CAM_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('cam', ps2.tick, diffs.length, diffs.slice(0, 16).map((k) => `${cameraWordName(k)}:${show(k, web.camWords[k])}/${show(k, ps2Words.get(k))}`).join(' ')); } }
  rows.push(row);
  // FX_DUMP=file: per-tick web rider-contact FX state (impact_fx_info + the 0x150 spark kernel words, test-impact-fx.mjs).
  if (process.env.FX_DUMP && core._impact_fx_kernel) (globalThis.__fxRows ??= []).push({ tick: ps2.tick, info: Array.from(f32(core._impact_fx_info(), 17)), kernel: Array.from(new Uint32Array(core.HEAPU8.buffer, core._impact_fx_kernel(), 84)), fist: Array.from(f32(core._impact_fx_sprites(3), 32)), snow: Array.from(f32(core._snow_info(), 23)) });
  if (process.env.SPHERE_TRACE && process.env.SPHERE_TRACE.split(',').map(Number).includes(ps2.tick) && RECORD >= 16384) {
    const v = f32(core._body_volume_info(), 108); const base = (i + 1) * RECORD + 8192; const u = (o) => dv.getUint32(base + o, true), g = (o) => dv.getFloat32(base + o, true);
    const n = u(0x2c); const out = [];
    for (let k = 0; k < n; k++) { const pc = [g(48 + 32 * k), g(52 + 32 * k), g(56 + 32 * k)]; const wc = [v[10 + k * 5], v[11 + k * 5], v[12 + k * 5]]; out.push(`${u(68 + 32 * k)}:${dist(pc, wc).toFixed(3)}`); }
    console.error('spheres', ps2.tick, 'broad', dist([g(16), g(20), g(24)], [v[2], v[3], v[4]]).toFixed(3), out.join(' '));
  }
  if (process.env.CRASH_TRACE && ps2.tick >= Number(process.env.CRASH_TRACE) && ps2.tick <= Number(process.env.CRASH_TRACE) + 8) {
    const ownerSub = dv.getInt32((i + 1) * RECORD + 3168 + 0x30, true);
    console.error('crash', ps2.tick, 'ps2 submode', ownerSub, 'web crash_info', JSON.stringify(Array.from(f32(core._crash_info(), 12)).map((x) => +x.toFixed(3)))); }
  if (process.env.UP_TRACE && Math.abs(ps2.tick - Number(process.env.UP_TRACE)) <= 3) { const ptr = core._ground_state_dump(); const n = f32(ptr, 1)[0]; const pairs = f32(ptr + 4, n * 2); const get = (off) => { for (let k = 0; k < n; k++) if (pairs[2 * k] === off) return pairs[2 * k + 1]; return NaN; };
    const rr = (off) => dv.getFloat32((i + 1) * RECORD + 32 + off - 0x100, true);
    console.error('up', ps2.tick, 'web', [get(0x180), get(0x184), get(0x188)].map((x) => x.toFixed(4)).join(','), 'ps2', [rr(0x180), rr(0x184), rr(0x188)].map((x) => x.toFixed(4)).join(',')); }
  if (process.env.LOCAL_TRACE && process.env.LOCAL_TRACE.split(',').map(Number).includes(ps2.tick)) {
    const lp = f32(core._sampled_local_pose(), 29 * 7); const base = (i + 1) * RECORD; const bad = [];
    const nl = f32(core._sampled_local_pose(), 0).length ? 27 : 27; for (let b = 0; b < nl; b++) { const pb = b >= 24 ? b + 2 : b; let m = 0; for (let k = 0; k < 3; k++) m = Math.max(m, Math.abs(lp[b * 7 + k] - dv.getFloat32(base + 5440 + 16 * pb + 4 * k, true))); for (let k = 0; k < 4; k++) m = Math.max(m, Math.abs(lp[b * 7 + 3 + k] - dv.getFloat32(base + 5952 + 16 * pb + 4 * k, true))); if (m) bad.push(`${b}:${m.toExponential(2)}`); }
    if (process.env.LOCAL_ROOTS) for (const b of [0, 22]) console.error('  localbone', b, 'web', Array.from(lp.slice(b * 7, b * 7 + 7)).map((x) => x.toFixed(4)).join(','), 'ps2', [0, 1, 2].map((k) => dv.getFloat32(base + 5440 + 16 * b + 4 * k, true).toFixed(4)).concat([0, 1, 2, 3].map((k) => dv.getFloat32(base + 5952 + 16 * b + 4 * k, true).toFixed(4))).join(','));
    console.error('local', ps2.tick, bad.join(' ') || 'exact', 'controls', JSON.stringify(Array.from(f32(core._pose_controls_info(), 17)).map((x) => +x.toFixed(6)))); }
  // PRED_TRACE=a:b with a capture that watches the trajectory predictor *(rider+0x788) (--watch ADDR:0x100): heading/normal/times/status.
  if (process.env.PRED_TRACE) { const [a, b] = process.env.PRED_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) {
    const w = (captureManifest.layout?.watch_offset ?? 10752) + (i + 1) * RECORD, pf = (o) => dv.getFloat32(w + o, true), pi = (o) => dv.getInt32(w + o, true);
    const p = Array.from(f32(core._prediction_info(), 19)); const hex = (x) => new Uint32Array(new Float32Array([x]).buffer)[0].toString(16);
    const cmp = (name, web, ps) => `${name} ${web.map((x, k) => Math.fround(x) === ps[k] ? '=' : `${hex(x)}/${hex(ps[k])}`).join(',')}`;
    console.error('pred', ps2.tick, `status ${p[1]}/${pi(0xac)} surf ${p[5]}/${pi(0x90)} flags ${dv.getInt16(w + 0x94, true)}`, cmp('t', [p[2], p[3]], [pf(0x98), pf(0xa0)]), cmp('head', p.slice(7, 10), [pf(0x10), pf(0x14), pf(0x18)]), cmp('n', p.slice(10, 13), [pf(0x20), pf(0x24), pf(0x28)])); } }
  if (process.env.ROLLER_WEB) { const [a, b] = process.env.ROLLER_WEB.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('rollerweb', ps2.tick, JSON.stringify(rollerSnapshot && rollerSnapshot.map((x) => +x.toFixed(2)))); }
  if (process.env.FIELD_DUMP && process.env.FIELD_DUMP.split(',').map(Number).includes(ps2.tick)) {
    const ptr = core._ground_state_dump(); const n = f32(ptr, 1)[0]; const pairs = f32(ptr + 4, n * 2); const out = [];
    for (let k = 0; k < n; k++) { const off = pairs[2 * k], wv = pairs[2 * k + 1]; const pv = dv.getFloat32((off === 0x2e4 ? i : i + 1) * RECORD + 32 + off - 0x100, true); if (Math.fround(wv) !== pv) out.push(`0x${off.toString(16)} ${wv} ${pv}`); }
    console.error('fields', ps2.tick, out.join(' | ')); }
  // STAGE_TRACE=a:b: stage-script dispatch counters and the newest fired (resource, program, tick?) triples.
  if (process.env.STAGE_TRACE && core._stage_script_info) { const [a, b] = process.env.STAGE_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) { const p = core._stage_script_info(); const h = new Uint32Array(core.HEAPU8.buffer, p, 7); const n = h[6]; const log = new Uint32Array(core.HEAPU8.buffer, p + 28, 3 * n); console.error('stage', ps2.tick, Array.from(h).join(','), 'last', JSON.stringify(Array.from(log.slice(Math.max(0, 3 * n - 9))))); } }
  // PROGRESS_TRACE=1: first tick where the web race progress (+0x4D0 remaining / +0x4D4 best, race_progress_info) leaves the record.
  if (process.env.PROGRESS_TRACE && core._race_progress_info && !globalThis.__progressDone) { const w = f32(core._race_progress_info(), 2), p = [0x4d0, 0x4d4].map((o) => dv.getFloat32((i + 1) * RECORD + 32 + o - 0x100, true));
    if (Math.fround(w[0]) !== p[0] || Math.fround(w[1]) !== p[1]) { console.error('progress', ps2.tick, 'web', w[0], w[1], 'ps2', p[0], p[1]); globalThis.__progressDone = true; } }
  // RESET_TRACE=a:b: web reset controller state (reset_info: active, progress, placements, completions, reason, path, observers, distance) and collision history.
  if (process.env.RESET_TRACE) { const [a, b] = process.env.RESET_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('reset', ps2.tick, JSON.stringify(Array.from(f32(core._reset_info(), 9)).map((x) => +x.toFixed(3))), 'collision', JSON.stringify(Array.from(f32(core._collision_reaction_info(), 10)).map((x) => +x.toFixed(3))), 'ps2 3F0', dv.getFloat32((i + 1) * RECORD + 32 + 0x3f0 - 0x100, true).toFixed(3), '3F4', dv.getFloat32((i + 1) * RECORD + 32 + 0x3f4 - 0x100, true).toFixed(3)); }
  if (process.env.BP_TRACE && core._board_press_info) { const [a, b] = process.env.BP_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) console.error('bp', ps2.tick, 'ps2 ctl', ps2.control, 'web', JSON.stringify(Array.from(f32(core._board_press_info(), 26)).map((x) => +x.toFixed(4)))); }
  if (args.includes('--board-press') && core._board_press_info && RECORD >= 16384) { // +0x330, control-1 object (owner+0x1D0, record 9072+0x10) and channel-2 semantic
    const b = f32(core._board_press_info(), 26); const base = (i + 1) * RECORD; const co = base + 9072 + 0x10;
    const ps2bp = { press: dv.getInt32(base + 32 + 0x330 - 0x100, true), phase: dv.getInt32(co, true), time: dv.getFloat32(co + 4, true), full: dv.getFloat32(co + 8, true), idle: dv.getFloat32(co + 12, true), ollie: dv.getInt32(co + 16, true), semantic: dv.getInt32(base + 6464 + 8, true) };
    const webbp = { press: b[6], phase: b[1], time: b[2], full: b[3], idle: b[4], ollie: b[5], semantic: web.info[0] };
    const control1 = ps2.control === 1;
    const pressSemantic = (x) => x >= 23 && x <= 38;
    for (const k of Object.keys(ps2bp)) { if (!control1 && ['phase', 'time', 'full', 'idle', 'ollie'].includes(k)) continue; if (k === 'semantic' && !control1 && !pressSemantic(ps2bp.semantic) && !pressSemantic(webbp.semantic) && !process.env.BP_ALL_SEMANTICS) continue; const key = 'bp.' + k;
      if (!(key in fieldFirst) && Math.fround(webbp[k]) !== Math.fround(ps2bp[k])) fieldFirst[key] = { tick: ps2.tick, web: webbp[k], ps2: ps2bp[k] }; }
  }
  if (args.includes('--fields')) {
    const ptr = core._ground_state_dump(); const n = f32(ptr, 1)[0]; const pairs = f32(ptr + 4, n * 2);
    // +0x2E4 against record i (the limit tick i used, as firstSpeedLimitMismatch): the record after holds tick i+1's 11B3F8.
    for (let k = 0; k < n; k++) { const off = pairs[2 * k], wv = pairs[2 * k + 1]; const pv = dv.getFloat32((off === 0x2e4 ? i : i + 1) * RECORD + 32 + off - 0x100, true);
      const key = '0x' + off.toString(16); if (!(key in fieldFirst) && Math.fround(wv) !== pv && (process.env.FIELD_EXACT || Math.abs(wv - pv) > 1e-6 * Math.max(1, Math.abs(pv)))) fieldFirst[key] = { tick: ps2.tick, web: wv, ps2: pv }; }
  }
  if (process.env.POSE_TRACE && process.env.POSE_TRACE.split(',').map(Number).includes(ps2.tick)) {
    const ptr = core._world_pose_bones(); const n = f32(ptr, 1)[0]; const wb = f32(ptr + 4, n * 7);
    const off = Number(process.env.POSE_OFFSET || 1); const pb = (b) => [0, 1, 2].map((k) => dv.getFloat32((i + off) * RECORD + 3264 + b * 32 + 4 * k, true));
    const errs = []; for (let b = 0; b < Math.min(n, 32); b++) errs.push(+dist([wb[b * 7], wb[b * 7 + 1], wb[b * 7 + 2]], pb(b)).toFixed(2));
    console.error('pose', ps2.tick, 'bones', n, JSON.stringify(errs));
  }
  if (process.env.SEQ_ROOTS && core._animation_sequence_roots && process.env.SEQ_ROOTS.split(',').map(Number).includes(ps2.tick)) { const p = core._animation_sequence_roots(); const n = f32(p, 1)[0]; const v = f32(p + 4, n);
    for (let k = 0; k < n; k += 10) console.error('seqroot', ps2.tick, 'ch', v[k], 'sem', v[k + 1], 'root', Array.from(v.slice(k + 2, k + 9)).map((x) => +x.toFixed(5)).join(','), 'mirror', v[k + 9]); }
  if (process.env.SEQ_TRACE && process.env.SEQ_TRACE.split(',').map(Number).includes(ps2.tick)) { const p = core._animation_sequences_info(); const n = f32(p, 1)[0]; const v = f32(p + 4, n);
    for (let k = 0; k < n; k += 26) console.error('seq', ps2.tick, 'ch', v[k], 'sem', v[k + 1], 'rate', process.env.SEQ_EXACT ? v[k + 3] : v[k + 3].toFixed(4), 'w', v[k + 4].toFixed(4), 'tgt', v[k + 5].toFixed(4), 'fade', v[k + 6].toFixed(4), 'stop', v[k + 7], 'clip', v[k + 11], 't', process.env.SEQ_EXACT ? v[k + 12] : v[k + 12].toFixed(4), 'sw', v[k + 14].toFixed(4), 'clip2', v[k + 16], 't2', v[k + 17].toFixed(4), 'sw2', v[k + 19].toFixed(4)); }
  if (lightingMode) { const base = (i + 1) * RECORD + captureManifest.layout.watch_offset, v = f32(core._environment_lighting_info(), 8), u = new Uint32Array(new Float32Array([v[6], v[7]]).buffer);
    const ps2Name = lightingName(dv.getUint32(base + 8, true), dv.getUint32(base + 12, true)), webName = lightingName(u[0], u[1]), ps2Values = [dv.getFloat32(base + 0x48, true), dv.getFloat32(base + 0x50, true)];
    lighting.ticks++; lighting.references[webName] = (lighting.references[webName] || 0) + 1;
    if (ps2Name === webName) lighting.referenceTicks++; else if (!lighting.firstReference) lighting.firstReference = { tick: ps2.tick, web: webName, ps2: ps2Name };
    if (Math.fround(v[2]) === ps2Values[0] && Math.fround(v[3]) === ps2Values[1]) lighting.scalarTicks++; else if (!lighting.firstScalar) lighting.firstScalar = { tick: ps2.tick, web: [v[2], v[3]], ps2: ps2Values };
    if (process.env.LIGHT_TRACE && (ps2Name !== webName || ps2.tick % 100 === 0)) console.error('light', ps2.tick, 'web', webName, v[2], v[3], v[5], 'ps2', ps2Name, ...ps2Values, dv.getFloat32(base, true)); }
  if (process.env.BODY_TRACE && Math.abs(ps2.tick - Number(process.env.BODY_TRACE)) <= 1) console.error('body', ps2.tick, 'query', JSON.stringify(Array.from(f32(core._body_query_info(), 14)).map((x) => +x.toFixed(3))), 'resp', JSON.stringify(Array.from(f32(core._body_response_info(), 11)).map((x) => +x.toFixed(3))));
  if (process.env.INSTANCE_EVENTS && RECORD >= 16384 && core._instance_contact_info) { const base = i * RECORD + RECORD; const cnt = dv.getUint32(base + 8928, true), ra = dv.getUint32(base + 8920, true); const w = f32(core._instance_contact_info(), 23);
    if (cnt !== globalThis.__ps2HitCount || w[2] !== globalThis.__webNotify || w[3] !== globalThis.__webSoft) { console.error('event', ps2.tick, 'ps2 count', cnt, 'ra', '0x' + ra.toString(16), 'closing', dv.getFloat32(base + 8944 + 48, true).toFixed(3), '| web responses', w[1], 'notify', w[2], 'soft', w[3], 'incomplete', w[4], 'path', w[6], 'closing', w[7].toFixed(3)); globalThis.__ps2HitCount = cnt; globalThis.__webNotify = w[2]; globalThis.__webSoft = w[3]; } }
  if (process.env.INSTANCE_TRACE && Math.abs(ps2.tick - Number(process.env.INSTANCE_TRACE)) <= 2 && core._instance_contact_info) { const base = i * RECORD + RECORD; const hk = (o) => dv.getFloat32(base + 8920 + o, true);
    console.error('instance', ps2.tick, 'web', JSON.stringify(Array.from(f32(core._instance_contact_info(), 26)).map((x) => +x.toFixed(4))), 'ps2 notify count', dv.getUint32(base + 8928, true), 'normal', [hk(56), hk(60), hk(64)].map((x) => x.toFixed(4)).join(','), 'closing', hk(72).toFixed(4)); }
  if (process.env.SURF_TRACE && Math.abs(ps2.tick - Number(process.env.SURF_TRACE)) <= 3) { const rr = (off, t = 'f') => t === 'i' ? dv.getInt32(i * RECORD + RECORD + 32 + off - 0x100, true) : dv.getFloat32(i * RECORD + RECORD + 32 + off - 0x100, true);
    console.error('surf', ps2.tick, 'ps2 surf', rr(0x438, 'i'), 'dist454', rr(0x454).toFixed(3), 'comp758', rr(0x758).toFixed(3), 'd1', rr(0x2bc).toFixed(3), '| web dist', f32(core._ground_contact_info(), 6)[5].toFixed(3), 'contact', JSON.stringify(Array.from(f32(core._terrain_contact_info(), 12)).map((x) => +x.toFixed(3))), 'body', JSON.stringify(Array.from(f32(core._body_response_info(), 11)).map((x) => +x.toFixed(3)))); }
  if (process.env.AIR_EXIT_TRACE && Math.abs(ps2.tick - Number(process.env.AIR_EXIT_TRACE)) <= 1) console.error('airexit', ps2.tick, JSON.stringify(Array.from(f32(core._air_exit_info(), 19)).map((x) => +x.toFixed(4))), 'railexit', JSON.stringify(Array.from(f32(core._rail_exit_info(), 4))));
  if (process.env.RAIL_TRACE && Math.abs(ps2.tick - Number(process.env.RAIL_TRACE)) <= 2) console.error('rail', ps2.tick, JSON.stringify(Array.from(f32(core._rail_gameplay_info(), 8))), 'probe', JSON.stringify(Array.from(f32(core._rail_attach_probe(), 12)).map((x) => +x.toFixed(2))), 'ps2bone22', ps2.bone22.map((x) => x.toFixed(2)).join(','), 'ps2off', ps2.offset9d0.map((x) => x.toFixed(2)).join(','), 'webpos', web.motion.slice(0, 3).map((x) => x.toFixed(1)).join(','));
  if (args.includes('--dump-takeoff') && ps2.tick === Number(args[args.indexOf('--dump-takeoff') + 1])) console.error('takeoff', JSON.stringify(Array.from(f32(core._jump_takeoff_info(), 21))), 'physics', JSON.stringify(Array.from(f32(core._physics_info(), 12))));
  if (traceRange && ps2.tick >= traceRange[0] && ps2.tick <= traceRange[1]) {
    const fmt = (v) => v.map((x) => x.toFixed(4)).join(',');
    console.error(`tick ${ps2.tick} cmd=${JSON.stringify(Object.fromEntries(Object.entries(cmd).filter(([k, v]) => v && k !== 'unsupported')))}`);
    console.error(`  ps2 ctl ${ps2.control} mode ${ps2.mode} pos ${fmt(ps2.position)} vel ${fmt(ps2.velocity)} n ${fmt(ps2.normal)} q ${fmt(ps2.quat)}`);
    console.error(`  web ctl ${webControl} gnd ${webGround} pos ${fmt(web.motion.slice(0, 3))} vel ${fmt(web.motion.slice(3, 6))} q ${fmt(web.quat)} sem ${web.info[0]}`);
  }
  // Roller pool watch: modifier k (creation order) at window+0x10+k*0x2E0; +0x20 timer, +0x30 position, +0x40 quaternion, +0x50 momentum.
  const rollerWatch = (captureManifest.layout?.watches || [])[0];
  if (rollerSnapshot && rollerWatch && rollerWatch.length >= 0x2E0 && !firstRollerMismatch) {
    const w = captureManifest.layout.watch_offset + i * RECORD, n = rollerSnapshot[0], word = (o) => dv.getUint32(w + o, true), fl = (o) => dv.getFloat32(w + o, true);
    for (let k = 0; k < Math.max(n, 2) && (k + 1) * 0x2E0 <= rollerWatch.length; k++) {
      const m = 0x10 + k * 0x2E0, live = word(m) === 0x48F080;
      const web = k < n ? rollerSnapshot.slice(4 + 13 * k, 4 + 13 * k + 13) : null;
      const ps2 = live ? [fl(m + 0x20), fl(m + 0x30), fl(m + 0x34), fl(m + 0x38), fl(m + 0x50), fl(m + 0x54), fl(m + 0x58), fl(m + 0x40), fl(m + 0x44), fl(m + 0x48), fl(m + 0x4C)] : null;
      const same = (!web && !ps2) || (web && ps2 && ps2.every((x, j) => Object.is(Math.fround(web[j + 1]), x)));
      if (process.env.ROLLER_TRACE && (web || ps2)) console.error('roller', records[i].tick, k, 'web', web && web.map((x) => +x.toFixed(3)).join(','), '| ps2', ps2 && ps2.map((x) => +x.toFixed(3)).join(','));
      if (process.env.ROLLER_BYTES && live && k < rollerBytes.length && Math.abs(records[i].tick - Number(process.env.ROLLER_BYTES)) <= 1) {
        const diffs = []; for (let o = 4; o < 0x2D0; o += 4) { if ([0xD0, 0x178, 0x1A0, 0x1C0, 0x1C4, 0x1C8].includes(o)) continue; const a = new DataView(rollerBytes[k].buffer).getUint32(o, true), b = word(m + o); if (a !== b) diffs.push(`+${o.toString(16)}:${a.toString(16)}/${b.toString(16)}`); }
        console.error('rollerbytes', records[i].tick, k, diffs.length, diffs.slice(0, 40).join(' ')); }
      if (!same) { firstRollerMismatch = { tick: records[i].tick, roller: k, web, ps2 }; break; }
      if (ps2) rollerTicksExact++;
    }
  }
  // Chairlift watch (setpieces captures): modifier windows 0x592E80/0x593580 (+0x10 distance) and the clone
  // window 0xBAB330 (clone matrices at +0x10; lift 0 clones 0xBAB6A0/0xBAB5F0/0xBAB540, lift 1 0xBAB490/0xBAB3E0/0xBAB330).
  const watches = captureManifest.layout?.watches || [];
  const windowOf = (address) => { let offset = 0; for (const w of watches) { const a = Number(w.address); if (address >= a && address + 4 <= a + w.length) return offset + address - a; offset += w.length; } return -1; };
  if (chairliftSnapshot && chairliftSnapshot[0] === 2 && windowOf(0x592E90) >= 0 && windowOf(0xBAB6A0 + 0x4C) >= 0 && !firstChairliftMismatch) {
    const base = captureManifest.layout.watch_offset + i * RECORD, word = (address) => dv.getUint32(base + windowOf(address), true);
    const lifts = [[0x592E80, [0xBAB6A0, 0xBAB5F0, 0xBAB540]], [0x593580, [0xBAB490, 0xBAB3E0, 0xBAB330]]];
    let same = true;
    lifts.forEach(([modifier, clones], l) => { const at = 1 + l * 49; if (word(modifier + 0x10) !== chairliftSnapshot[at]) same = false;
      clones.forEach((clone, k) => { for (let j = 0; j < 16; j++) if (word(clone + 0x10 + 4 * j) !== chairliftSnapshot[at + 1 + k * 16 + j]) same = false; }); });
    if (process.env.CHAIRLIFT_TRACE) console.error('chairlift', records[i].tick, same ? 'exact' : 'differs', 'web d', new Float32Array(Uint32Array.of(chairliftSnapshot[1]).buffer)[0], 'ps2 d', dv.getFloat32(base + windowOf(0x592E90), true));
    if (same) chairliftTicksExact++; else firstChairliftMismatch = { tick: records[i].tick };
  }
  if (scoreMode) row.scoreExact = compareScore(i + 1, ps2.tick);
  { const rr = (o) => dv.getFloat32((i + 1) * RECORD + 32 + o - 0x100, true), ri = (o) => dv.getInt32((i + 1) * RECORD + 32 + o - 0x100, true);
    const ps2b = [rr(0x2f8), rr(0x2fc), ri(0x2f4), rr(0x2f0)], webb = web.motion.slice(15, 19); boostTicks++;
    // Gated: meter, amount, Uber tier, Tricky/Super time (the ride-off amount clear is 12F730's, one tick after 131620's request).
    if ([0, 1, 2, 3].every((k) => Object.is(Math.fround(webb[k]), ps2b[k]))) boostTicksExact++; else if (!firstBoostMismatch) firstBoostMismatch = { tick: ps2.tick, web: webb, ps2: ps2b };
    if (!Object.is(Math.fround(webb[1]), ps2b[1]) && !firstBoostAmountMismatch) firstBoostAmountMismatch = { tick: ps2.tick, web: webb[1], ps2: ps2b[1] }; }
  tickHook?.tick({ i, tick: ps2.tick, web, row, cmd });
  if (process.env.BOOST_TRACE) { const [a, b] = process.env.BOOST_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= (b ?? a)) { const rr = (o) => dv.getFloat32((i + 1) * RECORD + 32 + o - 0x100, true), ri = (o) => dv.getInt32((i + 1) * RECORD + 32 + o - 0x100, true);
    console.error('boost', ps2.tick, 'web meter/amount/tier/super', web.motion.slice(15, 19).map((x) => +x.toFixed(6)).join(','), 'ps2', [rr(0x2f8), rr(0x2fc), ri(0x2f4), rr(0x2f0)].map((x) => +x.toFixed(6)).join(','), 'ctl', ps2.control, webControl); } }
  if (!exact && firstExact === null) firstExact = row;
  if (posErr > 1 && firstPos1cm === null) firstPos1cm = row;
  if (webControl !== ps2.control && firstControl === null) firstControl = row;
  if ((webGround ? 0 : 1) !== (ps2.mode === 0 ? 0 : 1) && firstMode === null) firstMode = row;
}
const maxPos = rows.reduce((m, r) => Math.max(m, r.posErrCm), 0), maxVel = rows.reduce((m, r) => Math.max(m, r.velErrCmps), 0);
const summary = { capture: capturePath, ticks: rows.length, seedErrorCm: seedError, exactTicks: rows.filter((r) => r.exact).length,
  maxPosErrCm: maxPos, maxVelErrCmps: maxVel, firstInexact: firstExact, firstOver1cm: firstPos1cm, firstControlMismatch: firstControl, firstSpeedLimitMismatch: firstLimit, speedLimitAgreed: limitAgreed, speedLimitAgreedRow: limitAgreedRow, firstSpeedLimitDivergence: firstLimitDivergence, fieldFirst, firstWordMismatch, firstModeMismatch: firstMode, unsupported, rollerTicksExact, firstRollerMismatch, chairliftTicksExact, firstChairliftMismatch,
  cameraWordTicks, cameraWordTicksExact, firstCameraWordMismatch, cameraShakeRngTicks, visualRngTicksExact, firstVisualRngMismatch, cameraWordFirst: Object.fromEntries(Object.entries(cameraWordFirst).sort((a, b) => a[1].tick - b[1].tick).slice(0, 40)),
  controlsSeen: [...new Set(records.map((r) => r.control))],
  ...(lightingMode ? { lighting } : {}),
  ...(weatherWatch ? { weather: weatherStats } : {}),
  ...(scoreMode ? { scoreTicksExact, firstScoreMismatch, scoreFirst: Object.fromEntries(Object.entries(scoreFirst).sort((a, b) => a[1].tick - b[1].tick)) } : {}) };
if (process.env.FX_DUMP) fs.writeFileSync(process.env.FX_DUMP, JSON.stringify(globalThis.__fxRows || []));
if (process.env.RNG_DRAWS_DUMP) fs.writeFileSync(process.env.RNG_DRAWS_DUMP, JSON.stringify(webDrawsOut));
if (process.env.__RNG_DRAWS_OUT) { fs.writeFileSync(process.env.__RNG_DRAWS_OUT, JSON.stringify(webDrawsOut)); process.exit(0); }
if (process.env.SET_PIECE_CONTACTS && core._set_piece_contacts) { const p = core._set_piece_contacts(), n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0], v = new Uint32Array(core.HEAPU8.buffer, p + 4, 2 * n); const seen = new Map(); for (let i = 0; i < v.length; i += 2) if (!seen.has(v[i + 1])) seen.set(v[i + 1], v[i]); console.error('set-piece contacts (first tick per resource):', JSON.stringify([...seen])); if (core._set_piece_splines) { const q = core._set_piece_splines(), n = core.HEAPF32[q >> 2]; console.error('spline pieces [resource, phase, launch, distance, x, y, z]:', JSON.stringify(Array.from({ length: n }, (_, k) => Array.from(core.HEAPF32.subarray((q >> 2) + 2 + 7 * k, (q >> 2) + 9 + 7 * k)).map((x) => +x.toFixed(1))))); } }
if (stageSnaps) { summary.stageWorld = stageRows;
  const U = new Uint32Array(core.HEAPU8.buffer), sp = core._stage_script_info() >> 2, fired = U[sp + 6];
  summary.stageFired = Array.from({ length: fired }, (_, k) => Array.from(U.subarray(sp + 7 + 3 * k, sp + 10 + 3 * k)));
  const lc = core._stage_world_livecomps() >> 2; summary.stageLiveComps = Array.from({ length: U[lc] }, (_, k) => [U[lc + 1 + 16 * k], U[lc + 2 + 16 * k], U[lc + 16 + 16 * k]]); // tick, resource, shared-RNG draws
  if (process.env.SECTION_LOG) { const want = new Set(process.env.SECTION_LOG.split(',').map(Number)), q = core._set_piece_sections() >> 2; summary.sectionEvents = [];
    for (let k = 0; k < U[q]; k++) { const e = Array.from(U.subarray(q + 2 + 6 * k, q + 8 + 6 * k)); if (want.has(e[1])) summary.sectionEvents.push(e.slice(0, 4)); } } // tick, resource, action, program
  if (process.env.VISUAL_LOG && core._stage_world_visual_log) { const q = core._stage_world_visual_log() >> 2; summary.visualLog = Array.from(U.subarray(q + 1, q + 1 + 2 * U[q])); }
  const el = core._stage_world_entity_log() >> 2; summary.stageEntityLog = Array.from({ length: U[el] }, (_, k) => Array.from(U.subarray(el + 1 + 3 * k, el + 4 + 3 * k))); }
summary.boostTicks = boostTicks; summary.boostTicksExact = boostTicksExact; summary.firstBoostMismatch = firstBoostMismatch; summary.firstBoostAmountMismatch = firstBoostAmountMismatch;
if (tickHook?.summary) Object.assign(summary, await tickHook.summary());
if (snapshotKeepCheck) { try { snapshotKeepCheck.snapshotRestore(0, globalThis.__keepCheckState, { human: core }); summary.snapshotKeepCheck = 'ok'; } catch (e) { summary.snapshotKeepCheck = e.message; } console.error('snapshot keep check:', summary.snapshotKeepCheck); }
console.log(JSON.stringify(summary, null, 1));
if (reportPath) fs.writeFileSync(reportPath, JSON.stringify({ summary, rows }, null, 1));
