// Six-rider race vs an original PS2 event capture (tools/ps2_capture.py, --event baseline = the
// countdown anchor). The human replays the capture's pad through the production pad path; the five
// computer riders run the original NPC provider in their own core instances (web/ai-racers.js).
// Nothing is copied from the capture after the start: the shared RNG is seeded once at the anchor
// record and every draw afterwards comes from the six riders' code.
// Usage: node compare-ai-capture.mjs RUN.bin [--zoe] [--isolate] [--report out.json] [--trace slot:a:b] [--no-relations]
// Record N (tick T) holds the riders' state after tick T-1 and the RNG at the start of tick T.
import fs from 'node:fs';
// CORE_JS=path/core.js: a private core (CORE_OUT=dir sh web/build-core.sh) instead of web/runtime, as compare-ps2-capture.mjs.
const createCore = (await import(process.env.CORE_JS ? (await import('node:url')).pathToFileURL(process.env.CORE_JS).href : './runtime/core.js')).default;
import { createAiRacers, rngNext, syncWorldNodes } from './ai-racers.js';
import * as eventSnapshot from './event-snapshot.js';
import * as eventReturn from './event-return.js';
import { readAiCapture, rosterOrder } from './ps2-capture-ai.mjs';
import { loadStageWorld, compareStageWorld, loadSnapshots } from './stage-world-compare.mjs';

const args = process.argv.slice(2);
if (args.includes('--coast-device')) process.env.COAST_DEVICE = '1'; // the Give Up's coast from the device pad (menu_pad.py's samples), as the PS2 read it
if (args.includes('--peak-splines')) process.env.PEAK_SPLINES = '1'; // pv peakSplines for this run (web/peak-capture.mjs: core set_piece_streamed)
const capturePath = args.find((a) => !a.startsWith('--') && a.endsWith('.bin'));
const reportPath = args.includes('--report') ? args[args.indexOf('--report') + 1] : null;
const trace = args.includes('--trace') ? args[args.indexOf('--trace') + 1].split(':').map(Number) : null;
const limit = args.includes('--ticks') ? Number(args[args.indexOf('--ticks') + 1]) : Infinity;
if (!capturePath) throw new Error('capture path required');
const root = new URL('public/assets/', import.meta.url);
const read = (p) => fs.readFileSync(new URL(p, root));
const text = (p) => read(p).toString('utf8');
const human = await createCore();
process.on('uncaughtException', (e) => { console.error(e instanceof Error ? e.stack : (human.getExceptionMessage ? human.getExceptionMessage(e) : e)); if (!(e instanceof Error) && e?.stack) console.error(e.stack); console.error('at record', globalThis.__compareTick, 'stage', globalThis.__compareStage); process.exit(1); });
const put = (core, bytes) => { const p = core._malloc(bytes.length); core.HEAPU8.set(bytes, p); return p; };
const str = (core, s) => put(core, Buffer.from(s + '\0'));
// --human ID [--base ID]: another human (riders.json, its settings as main.js selectRider merges them); --document PATH: a
// lineup document (web/lineup.js assembleLineup) instead of the course's npc-riders.json.
const argValue = (flag) => (args.includes(flag) ? args[args.indexOf(flag) + 1] : null);
const humanRider = argValue('--human') ? JSON.parse(fs.readFileSync(new URL('public/assets/riders.json', import.meta.url))).find((r) => r.id === argValue('--human')) : null;
const humanPackage = humanRider ? humanRider.package : args.includes('--zoe') ? 'RIDER_ZOE' : 'RIDER_SAM';
const courseCode = JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8')).location || 'ARA1';
const resources = {
  packetsJson: text('ANIMATIONS/animation-packets.json'), packetsBin: read('ANIMATIONS/animation-packets.bin'), initialText: text(courseCode === 'ARA1' ? 'ANIMATIONS/initial.json' : `${courseCode}/initial.json`),
  collision: read(`${courseCode}/collision.bin`), terrainText: text(`${courseCode}/terrain.json`), worldCollisionText: text(`${courseCode}/world_collision.json`), railsText: text(`${courseCode}/rails.json`), riderText: {},
};
resources.terrainHash = JSON.parse(resources.terrainText).source_sha256;
const doc = JSON.parse(argValue('--document') ? fs.readFileSync(argValue('--document'), 'utf8') : text(`${courseCode}/npc-riders.json`));
for (const r of doc.riders) resources.riderText[r.package] = text(`${r.package}/rider.json`);
// Human core: the same initialisation as compare-ps2-capture.mjs.
// --ctm-full CODE (with --in-world-ai; pv eventInWorld + eventInWorldAi, docs/ctm-events-in-world.md stage 4): one capture from a Peak 1
// location arrival through free ride, the event's gate, WS1's hold, the card and the race, the human on the streamed Peak 1 world as
// web/compare-ps2-capture.mjs --course PEAK1 --peak-arrival --ctm-in-world runs it (web/ctm-in-world-setup.mjs, web/peak-capture.mjs), the
// computer riders made by the page's in-world path and started at the countdown's tick 0 (record C) after the human's event_grid_start.
// The riders are compared from C on (local/ctm-events/caps/c0a-full-ai: tools/ps2_capture.py build --ai-state + capture_card.py --ai-dynamic).
const ctmFull = argValue('--ctm-full');
if (ctmFull && !args.includes('--in-world-ai')) throw new Error('--ctm-full needs --in-world-ai');
let humanText = ctmFull ? text('PEAK1/initial.json') : resources.initialText;
if (humanRider && humanRider.id !== 'zoe') {   // the human's own settings (web/character-roster.js)
  const { humanSettings, composeCheat } = await import('./character-roster.js');
  const settingsOf = (pkg) => { const f = new URL(`public/assets/${pkg}/settings.json`, import.meta.url); return fs.existsSync(f) ? JSON.parse(fs.readFileSync(f)) : null; };
  const base = argValue('--base') && argValue('--base') !== 'zoe' ? settingsOf(`RIDER_${argValue('--base').toUpperCase()}`) : null;
  humanText = JSON.stringify(humanSettings(JSON.parse(resources.initialText), humanRider.kind === 'cheat' ? composeCheat(base, settingsOf(humanRider.package)) : settingsOf(humanRider.package)));
}
human._init_animation(str(human, resources.packetsJson), str(human, text(`${humanPackage}/rider.json`)), str(human, humanText), put(human, resources.packetsBin), resources.packetsBin.length);
human._init_race(str(human, humanText));
if (humanRider && courseCode !== 'ARA1') {   // the human's own grid spot on this course (web/lineup.js, as ai-race.js prepare)
  const { humanGridState } = await import('./lineup.js');
  const data = JSON.parse(text(`${courseCode}/lineups.json`)), roster = JSON.parse(text('riders.json'));
  const base = humanRider.kind === 'cheat' ? roster.find((r) => r.id === (argValue('--base') || 'zoe')).character : humanRider.character;
  const grid = humanGridState(data, humanRider, base);
  if (grid) human._human_grid_seed(str(human, JSON.stringify(grid)));
}
human._animation_use_physics(1);
if (!ctmFull) { // (--ctm-full: the streamed Peak 1 world, web/peak-capture.mjs loadPeakWorld below)
human._init_world(put(human, resources.collision), resources.collision.length / 4);
const hash = str(human, resources.terrainHash);
human._init_terrain(str(human, resources.terrainText));
human._init_world_collision(str(human, resources.worldCollisionText), hash);
human._init_body_terrain(str(human, resources.terrainText));
human._init_rails(str(human, resources.railsText), hash);
}
// Section activation 0x101B60 runs natively (web/section_gameplay.inc): its slot-1 draws (0x341bbc, builtin3 key 8)
// are no longer injected from the capture. --no-sections keeps the older injection for comparison.
const sectionsFile = new URL(`${courseCode}/SECTIONS/sections.json`, root);
const nativeSections = !ctmFull && !args.includes('--no-sections') && !!human._init_sections && fs.existsSync(sectionsFile); // (--ctm-full: the streamed world's own)
if (nativeSections) human._init_sections(str(human, fs.readFileSync(sectionsFile, 'utf8')));
// Stage world (web/stage_world.inc): the particle / LiveComp-timer data the browser loads (set-pieces-renderer.js); its
// timer programs draw the shared RNG (builtin3 key8/key6, builtin19). --no-stage-world leaves it out.
const stageWorld = !ctmFull && !args.includes('--no-stage-world') && loadStageWorld(human, root, courseCode);
const drawsBetween = (a, b) => { const w = a.slice(); for (let n = 0; n <= 200; n++) { if (w.every((x, k) => x === b[k])) return n; rngNext(w); } return -1; };
let tickDraws = [];
let pendingRng = null; const rngBlips = [];
const rngTrace = process.env.RNG_TRACE ? process.env.RNG_TRACE.split(':').map(Number) : null;
// --shared-visual: one visual stream (0x4FF018 + LCG gp+0xA0C) across the rider cores in the original pass order (FX passes
// after every rider's 121818, phase-major; world visual pass; camera last), seeded from --visual-state (a
// tools/export_world_visual_state.py export of the location's ready savestate) and checked against a capture that watches
// 0x4ff018:0x18 (and 0x4a3afc:4).
const sharedVisual = args.includes('--shared-visual');
// --replay-return (pv eventReturnInWorld (b), the ctm-events/c0a-ret3 gate): the in-world results' auto replay as the page runs it
// (web/event-snapshot.js): the countdown snapshot at the start, the results-time snapshot at the live stop, then the replay (the
// countdown snapshot back and REPLAY_TICKS of the recorded run, default 600) before the Transport restores the results time.
// --ws13 (with --ctm-full; pv eventReturnInWorld, docs/ctm-events-in-world.md stage 5 "WS13"): a capture through the results' Next heat
// (local/ctm-events/caps/c0a-ws13): the live race and its countdown snapshot as --replay-return, then at WS13's first record the
// results-time snapshot back (0x20CCF8's Next heat: 0x2706F0, then 231250(S, 13, 0, 1)), WS13's enter and the gondola (the human at
// its NIS actor, the director's camera point from the record), the semi's lineup in the same contexts (web/ai-race.js prepare, round 2:
// web/lineup.js lineupFor with the qualifier's finish order), 1289F0's grid start at the grid record, the card's Continue, the semi.
const ws13 = args.includes('--ws13');
const replayReturn = args.includes('--replay-return') || ws13;
// --in-world-ai [--node-seed NODES.json] (pv eventInWorldAi, docs/ctm-events-in-world.md stage 4): the computer riders set up as the page's
// in-world event makes them (web/main.js cb.eventAiPrepare): the first context takes the event package in parts with its body collision only
// (web/load-slices.js feedContextWorld / feedEventRails: the camera terrain stays the human's), the others copy it by key; after the start
// the human's node states go into their contexts and follow as kind 9 (web/ai-racers.js syncWorldNodes). --node-seed: the node states of
// the capture's countdown savestate (local/ctm-events/caps/<run>.nodes.json: tools/export_peak_seed.py seed_state's nodes) put into the
// human first (peak_world_seed), the CTM world the riders race in (the first heat's free-ride DeadNodes, the hidden gate volume).
const inWorldAi = args.includes('--in-world-ai') ? await (async () => {
  const { feedContextWorld, feedEventRails } = await import('./load-slices.js');
  const { eventTerrainParts, eventWorldParts, eventRailParts, parseKey } = await import('./peak-world-batches.js');
  const tb = read(`${courseCode}/terrain.json`), wb = read(`${courseCode}/world_collision.json`), rb = read(`${courseCode}/rails.json`);
  const t = eventTerrainParts(tb.toString('utf8')), w = eventWorldParts(wb.toString('utf8')), r = eventRailParts(rb.toString('utf8'));
  const cut = { hash: t.hash, keys: { world: parseKey(wb, t.hash), body: parseKey(tb, t.hash), rails: parseKey(rb, t.hash) }, terrain: t, world: w, rails: r };
  const now = async () => {};
  return { cut, prepareWorld: async (c) => { const h = str(c, cut.hash); try { await feedContextWorld(c, cut, h, { yieldFn: now, budgetMs: 1e9 }); await feedEventRails(c, cut, h, { yieldFn: now, budgetMs: 1e9 }); } finally { c._free(h); } } };
})() : null;
if (inWorldAi) resources.worldKeys = { ...inWorldAi.cut.keys, hash: inWorldAi.cut.hash };
const racers = await createAiRacers({ human, resources, document: doc, isolate: args.includes('--isolate'), sharedVisual, ...(inWorldAi ? { prepareWorld: inWorldAi.prepareWorld, hostAtStart: !!ctmFull, ...(ctmFull ? { anchorTick: 0 } : {}) } : {}),
  onDraws: (slot, before, controller, motion) => { if (controller || motion) tickDraws.push(`${slot}:c${controller}m${motion}`); },
  afterRider: (slot) => injectWorldDraws(slot) });
if (doc.game_mode) for (const c of [human, ...racers.npcs.map((n) => n.core)]) c._event_kind?.(doc.game_mode.kind);
// In-race relationships (0x155BF0, as web/ai-race.js): the fresh tables aged once at the event load (0x155E58), used only when
// they give the document's levels (the capture state's table); the rider-pair reactions then move them, and 10F560's record
// flag +0x1C (relationship >= 2) follows (PS2 bc-race-tuck2 599: Mac's soft attacks raise his record of Zoe to level 2, and
// the human's 115D48 then picks the peer reaction 319). --no-relations keeps the document's levels fixed.
if (!args.includes('--no-relations') && doc.relationships?.characters) {
  const { ageRelationships, applyRelationshipEvent, relationshipScores } = await import('./lineup.js');
  const sessions = new URL('ARA1/lineup-sessions.json', root);
  if (fs.existsSync(sessions)) {
    const R = doc.relationships, riders = racers.npcs.length + 1, characters = R.characters, banks = R.target_banks;
    const tables = ageRelationships(JSON.parse(fs.readFileSync(sessions, 'utf8')).fresh, characters.slice(0, riders), banks);
    const levels = () => relationshipScores(tables, characters, banks, R.rival_character);
    if (JSON.stringify(levels()) === JSON.stringify(R.scores)) racers.onReact = (target, other, kind, attack) => {
      applyRelationshipEvent(tables, characters, banks, target, other, (kind === 2 ? 1 : 0) + (attack ? 2 : 0)); racers.setRelationships(levels()); };
    else console.error('relationships: the aged fresh table does not give the document levels; in-race changes off');
  }
}   // backcountry rival events: 0x535C10 5 / 6 (docs/backcountry.md)
// --world-draws: shared-RNG consumers outside the rider code that the browser does not run yet
// (course-script object 0x341AA0 in the world pass 0x101B60). Their draws are taken from the capture's RNG log (--ai-state) at the same position:
// after the rider whose pass drew them, or after every rider for world passes. Rider draws are
// never injected.
const WORLD_DRAW_CALLERS = new Set(nativeSections || ctmFull ? [] : [0x341bbc]); // (--ctm-full: the streamed world's section activation runs natively) // 0x359460 (spline modifier from a rider's trigger) now runs natively in that rider's core
let pendingWorld = null; let injected = 0;
function injectWorldDraws(slot) {
  if (!pendingWorld) return 0; let n = 0;
  for (const e of pendingWorld) if (!e.done && (e.slot === slot || (e.slot === null && slot === -1))) { e.done = true; injected++; n++; }
  return n;
}

// ---- capture ----
const manifest = JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8'));
const RECORD = manifest.record || 8192;
let raw = fs.readFileSync(capturePath);
const inWorldSetup = ctmFull ? await import('./ctm-in-world-setup.mjs') : null, arrival = ctmFull ? inWorldSetup.arrivalSeeds(raw, RECORD) : null;
if (arrival) raw = raw.subarray(arrival.P * RECORD); // the comparison starts at the arrival's placement record P
const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
const records = [], blockOf = rosterOrder(manifest);
for (let at = 0; at + RECORD <= raw.length; at += RECORD) {
  const u = (o) => dv.getUint32(at + o, true), f = (o) => dv.getFloat32(at + o, true);
  records.push({ tick: u(4), index: u(28), control: u(20), position: [f(32 + 0x10), f(32 + 0x14), f(32 + 0x18)], velocity: [f(32 + 0xE0), f(32 + 0xE4), f(32 + 0xE8)],
    // roster slot k's block (the record keeps actor-address order; a lineup document keeps roster order: ps2-capture-ai.mjs rosterOrder)
    others: [0, 1, 2, 3, 4].map((k) => blockOf[k] ?? k).map((k) => ({ position: [f(3008 + 32 * k), f(3012 + 32 * k), f(3016 + 32 * k)], velocity: [f(3024 + 32 * k), f(3028 + 32 * k), f(3032 + 32 * k)] })),
    rng: Array.from({ length: 6 }, (_, k) => u(8896 + 4 * k)) });
}
const BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown', 'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
function towardZero(x) { let r = Math.fround(x); if (Math.abs(r) > Math.abs(x)) { const b = new Float32Array([r]); const u = new Uint32Array(b.buffer); u[0] -= 1; r = b[0]; } return r; }
const axisByte = (v) => Math.floor((Math.max(-1, Math.min(1, v)) + 1) * 127.5 + 0.5);
// The menu script's last pressed segment (the map confirm), the device pad WS15's frame reads.
function returnPad(menus) { const pressed = menus.segments.filter((g) => g.buttons?.length); return pressed[pressed.length - 1]; }
// The menu script's device pad (local/ctm-events/menu_pad.py) at a record: the record's watch of its sample counter (0x9C804, +1 per
// device read, sampled right after this tick's read) picks the segment of the last read.
const menuSampleWatch = (() => { let o = 0; for (const w of manifest.layout?.watches || []) { if (Number(w.address) === 0x9C800) return o + 4; o += w.length; } return -1; })();
function menuSampleAt(i) { return menuSampleWatch < 0 ? -1 : dv.getUint32(i * RECORD + manifest.layout.watch_offset + menuSampleWatch, true); }
function menuSegmentAt(sample) { let end = 0; for (const g of manifest.menus?.segments || []) { end += g.frames; if (sample - 1 < end) return g; } return {}; }
function decodePad(seg) {
  const values = new Float32Array(24); const held = new Set(seg.buttons || []);
  BUTTONS.forEach((name, i) => { const on = held.has(name); values[i] = i >= 4 ? towardZero((on ? 255 : 0) * R255) : (on ? 1 : 0); });
  [axisByte(seg.rx || 0), axisByte(-(seg.ry || 0)), axisByte(seg.lx || 0), axisByte(-(seg.ly || 0))].forEach((b, axis) => {
    const neg = Math.max(Math.trunc((79 - b) * 255 / 79), 0), pos = Math.max(Math.trunc((b - 176) * 255 / 79), 0);
    values[16 + 2 * axis] = towardZero(neg * R255); values[17 + 2 * axis] = towardZero(pos * R255);
  });
  return values;
}
const padEntries = []; { let end = 0; for (const seg of manifest.segments) { end += seg.frames; padEntries.push({ end, values: decodePad(seg) }); } }
const padFor = (index) => (padEntries.find((e) => index < e.end) || padEntries[padEntries.length - 1]).values;
const padPtr = human._malloc(96);
const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
function humanTick(pad) {
  globalThis.__compareStage = 'human';
  human.HEAPF32.set(pad, padPtr >> 2);
  const o = Array.from(f32(human, human._pad_tick(padPtr), 24));
  if (process.env.PAD_TICKS) { const [a, b] = process.env.PAD_TICKS.split(':').map(Number), t = globalThis.__compareTick; if (t >= a && t <= b) console.error('padtick', t, BUTTONS.filter((_, k) => pad[k]).join('+') || '-', 'out', o.map((x) => +x.toFixed(3)).join(',')); } // (diagnostic: a tick range's pads and commands)
  if (process.env.PAD_TRACE && globalThis.__padTrace > 0) { globalThis.__padTrace--; console.error('pad', BUTTONS.filter((_, k) => pad[k]).join('+') || '-', 'out', o.map((x) => +x.toFixed(3)).join(',')); } // PAD_TRACE (diagnostic): the return's first ticks
  globalThis.__compareStage = 'race_begin'; human._race_begin();
  globalThis.__compareStage = 'step_rider'; const state = f32(human, human._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16);
  globalThis.__compareStage = 'animation_tick'; human._animation_tick(state[7], o[10], o[11], state[9], state[8], o[6], o[8], o[12], o[7], 0, state[15], o[13]);
  const pose = f32(human, human._pose_physical(), 12);
  globalThis.__compareStage = 'race_end'; human._race_end();
  if (sharedVisual) globalThis.__pendingCameraHead = [pose[9], pose[10], pose[11]]; else human._step_camera_head(pose[9], pose[10], pose[11]);
}
// REPLAY_PROBE: a core's per-subsystem snapshot hashes (core snapshot_hashes: [n, then file index, lo, hi]) by file name.
function snapshotHashes(c) { const p = c._snapshot_hashes(), n = new Uint32Array(c.HEAPU8.buffer, p, 1)[0], v = new Uint32Array(c.HEAPU8.buffer, p + 4, n * 3), out = {};
  for (let k = 0; k < n; k++) { const q = c._snapshot_file(v[3 * k]); let e = q; while (c.HEAPU8[e]) e++; out[new TextDecoder().decode(c.HEAPU8.subarray(q, e))] = v[3 * k + 1] + ':' + v[3 * k + 2]; } return out; }
// REPLAY_PROBE: 4 KB page hashes of the whole memory (FNV-1a over 32-bit words).
function pageHashes(mem) { const w = new Uint32Array(mem.buffer, 0, mem.length >> 2), n = w.length >> 10, h = new Uint32Array(n); for (let p = 0; p < n; p++) { let x = 0x811c9dc5; for (let k = p << 10, e = k + 1024; k < e; k++) x = Math.imul(x ^ w[k], 16777619); h[p] = x; } return h; }
// REPLAY_PROBE: a rough world-state fingerprint (the first 512 words behind each info export; layouts differ, so only which ones moved).
const REPLAY_WORLD_EXPORTS = ['_stage_world_info', '_stage_builtin_counts', '_stage_world_meshanim_state', '_stage_world_flag_words', '_set_piece_info', '_set_piece_sections', '_mission_info', '_weather_info', '_race_world_state', '_stage_script_info', '_pickup_info', '_section_listed', '_world_events', '_peak_world_events', '_stage_teleport_info', '_camera_state_words', '_weather_keep_state', '_fog_keep_state', '_race_progress_info', '_score_object_dump', '_rider_state'];
function replayWorldDump() { const out = {}; for (const f of REPLAY_WORLD_EXPORTS) { if (typeof human[f] !== 'function') continue; try { const p = human[f](); if (!p) continue; out[f] = Array.from(new Uint32Array(human.HEAPU8.buffer, p, 512)); } catch (e) { out[f] = String(e.message).slice(0, 40); } } return out; }
function tick(pad, record = null) { if (globalThis.__rep?.recording) globalThis.__rep.list.push({ pad: Float32Array.from(pad), record }); tickDraws = []; pendingWorld = record && worldDraws ? record.rng.log.filter((e) => WORLD_DRAW_CALLERS.has(e.ra)).map((e) => ({ slot: e.rider ? (e.rider.slot < 0 ? 0 : e.rider.slot + 1) : null })) : null;
  globalThis.__compareStage = 'begin'; racers.beginTick(); humanTick(pad); globalThis.__compareStage = 'end'; racers.endTick();
  if (globalThis.__rep) { const rep = globalThis.__rep, st = [human, ...racers.npcs.map((n) => n.core)].flatMap((c) => Array.from(f32(c, c._reference_motion(), 6))).concat(Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)).map((w) => w | 0));
    if (rep.recording) rep.live.push(st); else if (rep.replaying) { const k = rep.at++, l = rep.live[k]; if (!rep.first && l && st.some((v, j) => Math.fround(v) !== Math.fround(l[j]))) { const j = st.findIndex((v, jj) => Math.fround(v) !== Math.fround(l[jj])); const prev = k > 0 ? rep.live[k - 1].slice(36).map((w) => w >>> 0) : rep.rng, u = (a) => a.slice(36).map((w) => w >>> 0); rep.first = { tick: k, field: j < 36 ? `rider ${Math.floor(j / 6)} ${['x', 'y', 'z', 'vx', 'vy', 'vz'][j % 6]}` : `rng ${j - 36}`, live: l[j], rerun: st[j], liveDraws: drawsBetween(prev, u(l)), rerunDraws: drawsBetween(prev, u(st)) }; } } }
  if (sharedVisual && globalThis.__pendingCameraHead) { human._step_camera_head(...globalThis.__pendingCameraHead); globalThis.__pendingCameraHead = null; } } // cameras after every rider pass (0x230D7C)

// ---- event start and pre-anchor grid ticks ----
// --in-world-ai: a Conquer the Mountain race (0x535C11 = 0, web/stage-collect.js collectStart), so 30C4A8
// leaves the uncollected collectibles to their section's slot-1 program (a LiveComp, magnet and halo) instead of making them DeadNodes.
// The career's collected bits of the course come from the countdown savestate (--node-seed): a collectible is a DeadNode at a CTM
// countdown only when 30C4A8's 1538E8 found its career bit, and that bit is its index in the collection list (core stage_collection_list:
// 30C4A8 stores the resource at +0xC + 4 * count and asks 1538E8 for index = count). The list is the stage setup's: a first start with
// nothing collected builds it, the mask then goes in before the run's own start. --collect-mask LO:HI overrides (QA).
if (inWorldAi && !ctmFull && !args.includes('--single-collect')) { // (--ctm-full: free ride's own collect state, the CTM path)
  let [lo, hi] = [0, 0];
  if (argValue('--collect-mask')) [lo, hi] = argValue('--collect-mask').split(':').map((v) => Number(v) >>> 0);
  else if (argValue('--node-seed') && human._stage_collection_list) {
    human._set_stage_collect_state(0, 0, 0); human._reset_pad_history(); human._start_event();
    const p = human._stage_collection_list(), head = new Uint32Array(human.HEAPU8.buffer, p, 2), list = Array.from(new Uint32Array(human.HEAPU8.buffer, p + 8, head[1]));
    const dead = new Set(JSON.parse(fs.readFileSync(argValue('--node-seed'), 'utf8')).nodes.filter((n) => n[1] === 6).map((n) => n[0]));
    list.forEach((r, k) => { if (dead.has(r)) { if (k < 32) lo = (lo | (1 << k)) >>> 0; else hi = (hi | (1 << (k - 32))) >>> 0; } });
    console.error('collected', list.filter((r) => dead.has(r)).length, 'of', list.length, `mask ${lo.toString(16)}:${hi.toString(16)}`);
  }
  human._set_stage_collect_state(0, lo, hi);
}
let ctmPlan = null, peakWorld = null, ctmReturn = null, heat = null;
let d0Watch = -1; { let o = 0; for (const w of manifest.layout?.watches || []) { if (Number(w.address) === 0x1454BA0) d0Watch = o; o += w.length; } } // A+0xD0 of the Peak 1 world's activation (c0a-full's watch set)
if (ctmFull) { for (const r of records) r.gameTick = r.tick; // (peak-capture.mjs: a game tick restart where the record's tick is not the previous + 1)
  peakWorld = await (await import('./peak-capture.mjs')).loadPeakWorld({ core: human, root, captureManifest: manifest, dv, RECORD, records, arrival: true, world: 'PEAK1' });
  const start = JSON.parse(text('PEAK1/start.json')); human._reset_animation(); human._reset_race(); human._reset_rider(...start.position, start.heading);
  inWorldSetup.applyArrival(human, peakWorld, arrival);
  ctmPlan = inWorldSetup.planCtmInWorld({ code: ctmFull, core: human, dv, RECORD, records, layout: manifest.layout, json: (p) => JSON.parse(text(p)) });
  console.error('ctm full', ctmFull, 'gate record', ctmPlan.G, 'hold', ctmPlan.H, 'countdown', ctmPlan.C);
  { const kindW = (() => { let o = 0; for (const w of manifest.layout.watches || []) { if (Number(w.address) === 0x535C08) return o; o += w.length; } return -1; })(), kindAt = (k) => dv.getInt8(k * RECORD + manifest.layout.watch_offset + kindW + 8);
    let R = records.findIndex((r, k) => k > ctmPlan.C + 1 && kindAt(k) === 4 && kindAt(k - 1) !== 4);
    // --coast-only (c0a-ret2 as captured: its WS15 request is not a player's, so only the race, the Give Up and the coast are scored):
    // R is the first record after the menus' gap (the tick counter restarts), and the loop ends before it
    if (args.includes('--coast-only')) { R = records.findIndex((r, k) => k > ctmPlan.C + 1 && r.tick < records[k - 1].tick); globalThis.__coastOnly = R; }
    if (R > 0) { const riders = (k) => dv.getUint32(k * RECORD + 29248 + 0x78, true), out = records.findIndex((r, k) => k > R && riders(k) < riders(R));
      const loc = peakWorld.manifest.locations.find((l) => peakWorld.manifest.residency.find((row) => row.course === 0)?.locations.includes(l.code) && l.id < 22);
      const bank = JSON.parse(text(`${loc.root.replace(/^\/assets\//, '')}paths.json`)).variants['0'], entry = bank.regions.find((g) => g.kind + 1 === 2 && g.index === 1) ?? bank.regions[0];
      // The Give Up (c0a-ret2): the tick script's Start pauses in the tick of the record that read it (index == the Start's), the pause
      // menu's Give Up (1253D0) comes before the next tick; the EndRace coast (+0x480 TIME'S UP) runs until the auto replay starts. The
      // results' Transport (0x2706F0) restores that results-time state, then the stop frame and the WS14 frame each run one tick (map
      // savestate: total ticks = the last coast record's + 3, riders 3 ticks on) before the map holds the world until WS15.
      let startIndex = -1; { let end = 0; for (const g of manifest.segments) { if (g.buttons?.includes('Start')) { startIndex = end; break; } end += g.frames; } }
      const pauseAt = startIndex >= 0 ? records.findIndex((r) => r.index === startIndex) : -1, coast = menuSampleWatch >= 0 && pauseAt > 0 && pauseAt < R - 1;
      const stopTicks = coast ? (manifest.menus_run?.map_total_ticks != null ? manifest.menus_run.map_total_ticks - (records[R - 1].tick + 1) : Number(argValue('--return-stop-ticks') ?? 2)) : 0;
      ctmReturn = { R, out, entry, bank, pauseAt: coast ? pauseAt : -1, stopTicks }; console.error('ctm return: WS15 record', R, 'tick', records[R].tick, 'riders out at record', out, coast ? `pause at record ${pauseAt}, coast to ${R - 1}, ${stopTicks} ticks after the stop` : ''); } }
  // the event location's start rows (its paths.json variant 0), for WS13's 1297C8(C, 1) (web/event-heat.js)
  var heatBank = () => { const loc = peakWorld.manifest.locations.find((l) => l.code === ctmFull); return JSON.parse(text(`${loc.root.replace(/^\/assets\//, '')}paths.json`)).variants['0']; };
  if (ws13) { heat = (await import('./ctm-heat-setup.mjs')).planHeat({ records, C: ctmPlan.C }); console.error('ws13: replay from record', heat.S, 'WS13', heat.W, 'grid', heat.G, 'semi countdown', heat.C2); }
} else { human._reset_pad_history(); human._start_event(); racers.start(); }
if (inWorldAi && !ctmFull) { const seed = argValue('--node-seed'); if (seed) console.error('node seed', human._peak_world_seed(str(human, fs.readFileSync(seed, 'utf8'))), 'nodes');
  syncWorldNodes(human, racers.npcs.map((n) => n.core)); }
// --ctm-countdown: a CTM countdown savestate's human keeps the words core event_grid_start keeps (compare-ps2-capture.mjs
// --ctm-countdown): the motion-0 stamps (owner +0x10 / +0x14; 13C7A8's push-off speed scale), the boost words, the normals.
if (args.includes('--ctm-countdown')) { const o = manifest.layout.owner_00_40, r0 = (off) => dv.getFloat32(32 + off - 0x100, true), i0 = (off) => dv.getInt32(32 + off - 0x100, true);
  human._ground_tick_seed(dv.getUint32(o + 0x10, true), dv.getUint32(o + 0x14, true));
  human._boost_state_seed(r0(0x2e8), r0(0x2ec), r0(0x2f0), i0(0x2f4), r0(0x2f8), r0(0x2fc), i0(0x304));
  const offs = [0x380, 0x384, 0x388, 0x390, 0x394, 0x398], w = human._malloc(8 * offs.length);
  offs.forEach((off, k) => { human.HEAPF32[(w >> 2) + 2 * k] = off; human.HEAPF32[(w >> 2) + 2 * k + 1] = r0(off); }); human._ground_state_seed(w, offs.length); human._free(w); }
if (sharedVisual) { // the ready savestate's visual state (start of race tick 0)
  const vsPath = args.includes('--visual-state') ? args[args.indexOf('--visual-state') + 1] : null;
  if (vsPath) { const vs = JSON.parse(fs.readFileSync(vsPath, 'utf8')); new Uint32Array(human.HEAPU8.buffer, human._visual_rng_words(), 6).set(vs.visual_rng_4ff018);
    const b = Buffer.from(JSON.stringify(vs) + '\0'), p = human._malloc(b.length); human.HEAPU8.set(b, p); human._set_world_visual_state(p); human._free(p); }
}
const visWatch = (() => { let off = 0; for (const w of manifest.layout?.watches || []) { if (Number(w.address) === 0x4FF018) return off; off += w.length; } return -1; })();
const lcgWatch = (() => { let off = 0; for (const w of manifest.layout?.watches || []) { if (Number(w.address) === 0x4A3AFC) return off; off += w.length; } return -1; })();
const visualFirst = { words: null, lcg: null };
const weatherWatch = args.includes('--weather') ? (() => { const map = JSON.parse(fs.readFileSync(args[args.indexOf('--weather') + 1], 'utf8')), offs = new Map(); let off = 0;
  for (const w of manifest.layout?.watches || []) { offs.set(Number(w.address), off); off += w.length; } const o = (a) => (a === undefined || a === null ? -1 : offs.get(Number(a)) ?? -1);
  return { rider: o(map.rider), camera: o(map.camera), layers: (map.layers || []).map(o), splash: o(map.splash), splash2: o(map.splash2), visual: o(0x4FF018), lcg: o(0x4A3AFC) }; })() : null;
const weatherStats = { ticks: {}, exact: {}, first: {} };
const neutral = new Float32Array(24);
if (!ctmFull) for (let t = 0; t < records[0].tick; t++) tick(neutral);
racers.setSharedRng(records[0].rng);
// Human score object + HUD bank (docs/tricks-scoring.md), seeded from the anchor record as compare-ps2-capture.mjs --event does
// (stale front-end words in the bank), then compared per tick like it; and the boost words (+0x2F8 meter, +0x2F4 tier, +0x2F0 time).
const scoreLayout = manifest.layout?.score_000_1d0, hudLayout = manifest.layout?.hud_slots_44x6;
const SCORE_WORDS = 0x1d0 / 4, HUD_WORDS = 44 * 6, SCORE_SKIP = new Set([0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1cc]);
const scoreState = { ticks: 0, exact: 0, first: null, boostTicks: 0, boostExact: 0, firstBoost: null };
if (scoreLayout && hudLayout && human._score_object_seed) { const w = new Uint32Array(SCORE_WORDS + HUD_WORDS);
  for (let k = 0; k < SCORE_WORDS; k++) w[k] = dv.getUint32(scoreLayout + 4 * k, true); for (let k = 0; k < HUD_WORDS; k++) w[SCORE_WORDS + k] = dv.getUint32(hudLayout + 4 * k, true);
  const p = human._malloc(w.byteLength); human.HEAPU8.set(new Uint8Array(w.buffer), p); human._score_object_seed(p, records[0].tick); human._free(p);
  human._set_score_career_cash?.(dv.getInt32(hudLayout + 24 * 0x19 + 20, true)); } // HUD slot 0x19: the character block's cash (150960), as compare-ps2-capture.mjs
function compareHumanScore(r, tick) {
  if (!scoreLayout || !hudLayout) return; const base = r * RECORD, web = new Uint32Array(human.HEAPU8.buffer, human._score_object_dump(), SCORE_WORDS + HUD_WORDS); let bad = null;
  for (let k = 0; k < SCORE_WORDS && !bad; k++) { const off = 4 * k; if (SCORE_SKIP.has(off)) continue; const pv = dv.getUint32(base + scoreLayout + off, true); if (pv !== web[k]) bad = { tick, key: 'score+0x' + off.toString(16), web: web[k] | 0, ps2: pv | 0 }; }
  for (let k = 0; k < HUD_WORDS && !bad; k++) { const slot = Math.floor(k / 6), field = k % 6, pv = dv.getUint32(base + hudLayout + 4 * k, true), wv = web[SCORE_WORDS + k];
    if (pv === wv) continue; if (field > 0 && dv.getInt32(base + hudLayout + 24 * slot, true) === 0x34 && (web[SCORE_WORDS + slot * 6] | 0) === 0x34) continue; bad = { tick, key: `hud${slot}.${field}`, web: wv | 0, ps2: pv | 0 }; }
  scoreState.ticks++; if (!bad) scoreState.exact++; else if (!scoreState.first) scoreState.first = bad;
  const rr = (o) => dv.getFloat32(base + 32 + o - 0x100, true), ri = (o) => dv.getInt32(base + 32 + o - 0x100, true), m = f32(human, human._reference_motion(), 20);
  const ok = Object.is(Math.fround(m[15]), rr(0x2f8)) && Object.is(Math.fround(m[17]), ri(0x2f4)) && Object.is(Math.fround(m[18]), rr(0x2f0));
  scoreState.boostTicks++; if (ok) scoreState.boostExact++; else if (!scoreState.firstBoost) scoreState.firstBoost = { tick, web: [m[15], m[17], m[18]], ps2: [rr(0x2f8), ri(0x2f4), rr(0x2f0)] };
}
const aiCapture = manifest.layout?.ai_state ? readAiCapture(capturePath) : null;
if (aiCapture && arrival) aiCapture.records.splice(0, arrival.P); // (--ctm-full: from the placement record, as `records`)
// (--ctm-full: a free-ride baseline's build copies no computer rider into the default record blocks; their state is the AI blocks')
if (aiCapture && ctmFull) records.forEach((r, i) => { const a = aiCapture.records[i]?.ai; if (a) r.others = a.map((x) => ({ position: x.position, velocity: x.velocity })); });
const fieldFirst = racersFieldsInit();
function racersFieldsInit() { return [0, 1, 2, 3, 4].map(() => ({})); }
const note = (k, key, tick, web, ps2) => { if (!(key in fieldFirst[k])) fieldFirst[k][key] = { tick, web, ps2 }; };
const same = (a, b) => a[0] === b[0] && a[1] === b[1] && a[2] === b[2];
const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
const first = { human: null, rng: null, ai: [null, null, null, null, null], ai1cm: [null, null, null, null, null], aiControl: [null, null, null, null, null] };
const worldDraws = args.includes('--world-draws');
const rows = [];
const resync = args.includes('--rng-resync'); const rngEvents = []; const rowsRank = [];
const wordsHex = (n) => n.info ? [((n.info[0] | (n.info[1] << 16)) >>> 0).toString(16), ((n.info[2] | (n.info[3] << 16)) >>> 0).toString(16)] : null;
// STAGE_WORLD_PS2=snapshots.json (tools/export_particle_snapshots.py): compare the core's particle effects with the PS2
// savestates at the same ticks (web/stage-world-compare.mjs).
const stageSnaps = process.env.STAGE_WORLD_PS2 ? loadSnapshots(process.env.STAGE_WORLD_PS2) : null;
const stageCompare = [];
const stageDump = process.env.STAGE_WORLD_DUMP ? new Set(process.env.STAGE_WORLD_DUMP.split(',').map(Number)) : null;
const particleEval = stageDump ? await import('./set-piece-particle-eval.js') : null;
// TICK_HOOK=module.mjs: an observer (create({core, racers, dv, RECORD, captureManifest}) -> {tick({i, tick, core, racers}), summary()}),
// run after every compared tick (the human core has run record i's command; record i + 1 holds the PS2 state after it), as in
// compare-ps2-capture.mjs. Its summary is merged into the report.
// HOLD_BONE=record (diagnostic): the human's holds run 120F20's re-probe (core nis_hold_probe) as the PS2 does, from the record's
// +0xAFC and, when set, its posed board-root bone (bone 22) of the coming tick. Off (default), as the page: the human's holds do not
// probe (the page does not pose the rider under the NIS, so it has no bone).
const holdProbeBone = (k) => { if (!human._nis_hold_probe || process.env.HOLD_BONE !== 'record' || k * RECORD + RECORD > dv.byteLength) return;
  const at = k * RECORD, afc = dv.getUint32(at + 32 + 0xAFC - 0x100, true), bone = dv.getUint32(at + 32 + 0x8A0 - 0x100, true), b = at + (manifest.layout.world_bones_32x32 ?? 3264) + 32 * bone;
  if (!afc) human._nis_hold_probe(1, 0, 0, 0); else if (bone < 32) human._nis_hold_probe(2, dv.getFloat32(b, true), dv.getFloat32(b + 4, true), dv.getFloat32(b + 8, true)); };
const tickHook = process.env.TICK_HOOK ? await import(new URL(process.env.TICK_HOOK, `file://${process.cwd()}/`).href).then((m) => m.create({ core: human, racers, dv, RECORD, captureManifest: manifest })) : null;
for (let i = 0; i + 1 < records.length && i < limit && !(globalThis.__coastOnly > 0 && i >= globalThis.__coastOnly - 1); i++) {
  globalThis.__compareTick = records[i].tick;
  if (process.env.REPLAY_PROBE && ctmPlan && i === (+process.env.REPLAY_PROBE_FROM || 0)) globalThis.__repWorldHashes = pageHashes(human.HEAPU8); // (the reference for the dirty set)
  if (weatherWatch) compareWeather(i, records[i].tick);
  if (sharedVisual && visWatch >= 0) { // the stream at the record point (loop start = after the previous tick's cameras)
    const at = i * RECORD + manifest.layout.watch_offset, ps2w = Array.from({ length: 6 }, (_, k) => dv.getUint32(at + visWatch + 4 * k, true)), webw = Array.from(new Uint32Array(human.HEAPU8.buffer, human._visual_rng_words(), 6));
    if (!visualFirst.words && ps2w.some((x, k) => x !== webw[k])) visualFirst.words = { tick: records[i].tick, webBehind: drawsBetween(webw, ps2w), webAhead: drawsBetween(ps2w, webw) };
    if (lcgWatch >= 0 && !visualFirst.lcg) { const ps2l = dv.getUint32(at + lcgWatch, true), webl = new Uint32Array(human.HEAPU8.buffer, human._visual_lcg_word(), 1)[0]; if (ps2l !== webl) visualFirst.lcg = { tick: records[i].tick, web: webl.toString(16), ps2: ps2l.toString(16) }; }
    if (process.env.VISUAL_SPAN_AI && globalThis.__visPrev) { const [a, b2] = process.env.VISUAL_SPAN_AI.split(':').map(Number); if (records[i].tick - 1 >= a && records[i].tick - 1 <= b2) console.error('vspan', records[i].tick - 1, 'web', drawsBetween(globalThis.__visPrev[0], webw), 'ps2', drawsBetween(globalThis.__visPrev[1], ps2w)); }
    globalThis.__visPrev = [webw, ps2w];
  }
  if (ctmPlan) { // --ctm-full: the human alone until the countdown's tick 0, then the six riders (web/ctm-in-world-setup.mjs, as the page)
    if (heat && i >= heat.S - 1 && i < heat.C2) { // --ws13: the results, Next heat, WS13 and WS1 arg 3 (the semi's countdown at C2)
      if (i === heat.S - 1) { heat.results = eventSnapshot.snapshotSave(eventSnapshot.SNAPSHOT_RESULTS, { human, racers }); if (globalThis.__rep) globalThis.__rep.recording = false;
        const rows0 = racers.standings(), order = rows0.map((r, k) => ({ k, t: r.finished ? r.finishTicks : Infinity, rank: r.rank })).sort((a, b) => a.t - b.t || a.rank - b.rank).map((x) => x.k);
        heat.finishOrder = order; console.error('ws13: live stop at record', i, 'finish order (slots)', order.join(' ')); }
      if (i < heat.W - 1) continue; // the auto replay behind the results (its records are the replay's frames); Next heat restores the results time
      if (i === heat.W - 1) {
        eventSnapshot.snapshotRestore(eventSnapshot.SNAPSHOT_RESULTS, heat.results, { human, racers }); // 0x2706F0 (0x20CCF8's Next heat)
        tick(neutral, null); // the stop frame: one live tick of the restored results time before WS13 (as the Transport's, c0a-ret3; PS2 c0a-ws13: riders 3 and 4's 115D48 draws)
        heat.enter = (await import('./event-heat.js')).heatEnter({ human, racers, doc, finishOrder: heat.finishOrder, lineupData: JSON.parse(text(`${courseCode}/lineups.json`)),
          riderText: (pkg) => (resources.riderText[pkg] ??= text(`${pkg}/rider.json`)), cstr: (t) => str(human, t), bank: heatBank() });
        console.error('ws13: enter at record', i + 1, 'semi lineup', heat.enter.values.join(' '), 'changed slots', heat.enter.changed.join(' '));
        if (process.env.RANK_TRACE) console.error('ws13: human progress after the enter', Array.from(f32(human, human._race_progress_info(), 8)).map((x) => +x.toFixed(1)).join(','), 'pos', Array.from(f32(human, human._reference_motion(), 3)).map(Math.round).join(','));
        // (the stop frame is this record's interval: WS13's first frame writes record W, its section scan after the record is the next one's)
        { const q = (off) => dv.getFloat32((i + 1) * RECORD + 32 + off - 0x100, true); human._nis_hold(1, q(0x110), q(0x114), q(0x118), q(0x1B0), q(0x1B4)); }
        { const w = Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)), ps2w = records[i + 1].rng; if (w.some((x, k) => x !== ps2w[k])) { heat.rngFirst = { record: i + 1, tick: records[i + 1].tick, webBehind: drawsBetween(w, ps2w), webAhead: drawsBetween(ps2w, w) }; console.error('ws13: RNG first differs at record', i + 1, JSON.stringify(heat.rngFirst)); } }
        continue;
      }
      // The gondola's / WS1's NIS actor carries the human. The NIS update (and WS1's 1289F0 grid placement) runs after the frame's section
      // scan (the end of the rider manager): a teleport there is the next record's position, and the next frame's scan is the first to
      // see it (PS2 c0a-ws13: the gondola at record W, its 341BBC draws after W; the grid at WS1 tick 220, its draws after it).
      // The hold lasts while the record's +0xAC4 is set; its first record without it is WS1's last tick, whose 1297C8(C, 0) puts the
      // human on its grid row (11D390, after the frame as the NIS update): PS2 c0a-ws13 record 14538 (+0xAC4 0, the grid position).
      heat.pendingHold = () => { const q = (off) => dv.getFloat32((i + 1) * RECORD + 32 + off - 0x100, true);
        if (dv.getUint32((i + 1) * RECORD + 32 + 0xAC4 - 0x100, true)) { human._nis_hold(1, q(0x110), q(0x114), q(0x118), q(0x1B0), q(0x1B4)); holdProbeBone(i + 2); return; }
        if (heat.gridPlaced) return; heat.gridPlaced = i + 1; human._nis_hold(0, 0, 0, 0, 1, 0);
        { const p = str(human, text(ctmPlan.docPath)); try { if (!human._event_route_seed(p)) throw new Error(`${ctmPlan.docPath}: no event route`); } finally { human._free(p); } }
        const [x, y, z] = ctmPlan.start.position; human._event_grid_start(x, y, z, ctmPlan.start.heading); console.error('ws13: the human on its grid row at record', i + 1); };
      if (human._section_point && !args.includes('--no-section-camera')) { const at = i * RECORD + (manifest.layout.outer_camera_000_480 ?? 4288) + 0x20; human._section_point(1, dv.getFloat32(at, true), dv.getFloat32(at + 4, true), dv.getFloat32(at + 8, true)); }
      if (i < heat.G) { const rw = () => Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)), r0 = rw(); peakWorld.beforeTick(i); humanTick(padFor(records[i].index)); const r1 = rw(); human._fx_pass?.(-1); const r2 = rw(); human._section_pass?.(); const r3 = rw();
        if (process.env.WS13_TRACE && i - heat.W < +process.env.WS13_TRACE) console.error('ws13 tick', i, 'draws human', drawsBetween(r0, r1), 'fx', drawsBetween(r1, r2), 'sections', drawsBetween(r2, r3));
        tickHook?.hold?.({ i, tick: records[i + 1]?.tick, core: human, racers }); // (diagnostic, as above)
        heat.pendingHold(); heat.pendingHold = null;
        const w = r3, ps2w = records[i + 1].rng;
        if (!heat.rngFirst && w.some((x, k) => x !== ps2w[k])) { heat.rngFirst = { record: i + 1, tick: records[i + 1].tick, webBehind: drawsBetween(w, ps2w), webAhead: drawsBetween(ps2w, w) }; console.error('ws13: RNG first differs at record', i + 1, JSON.stringify(heat.rngFirst)); }
        continue; }
      if (i === heat.G) { for (const n of racers.npcs) n.core._npc_fresh_rider(); // 128958 -> 129E20: the round's riders made fresh (WS13 phase 0)
        // 1289F0's riders are the round's fresh ones (+0xEC 0, constructor 0x125EB8) and the human keeps its place (PS2 c0a-ws13 14036: 2,0,0,0,0,0);
        // 10F998's first ranking over the equal grid distances then gives 5,1,2,3,0,4 (keys -(remaining + 20 x rank), shell sort 0x3E6328)
        { const humanRank = racers.worldState ? racers.worldState[360] : 0; racers.start({ gridStart: true, hold: true, ranks: [humanRank, 0, 0, 0, 0, 0] }); console.error('ws13: grid ranks from', humanRank, racers.worldState ? Array.from(racers.worldState.slice(360, 396)).filter((_, k) => k % 6 === 0).join(',') : null); }
        console.error('ws13: grid start at record', i); } // 1289F0: the semi's riders on the grid (PreRace), the tick restarts
      // WS1 arg 3's update once its NIS list has ended (279298 == 0): 128958 / 128998, 128A48(C, 0) and, in a race, 128A48(C, 1) (rank mode
      // 1, +0xEC = the list index), before that frame's rider manager tick. PS2 call-site probe local/ctm-events/caps/c0a-ws13prank: 0x234570
      // and 0x234594 at tick 222, two frames after the gondola actor's release 123B48 (tick 220, record 14538); the list's end is the NIS
      // engine's, which the comparer does not run: its frame is the probe's (the hold's last record + 2).
      if (heat.gridPlaced && i === heat.gridPlaced + 2) { human._race_world_rank_mode(1); console.error('ws13: WS1 rank reset (128A48(C, 1)) before record', i); }
      if (i === heat.C2 - 1) { heat.pendingHold = null; continue; } // the card (WS2): the world holds; its Continue is the next record's tick 0
    }
    if (heat && i === heat.C2) { // the card's Continue: WS1's exit (128958, 1289F0, 128A10: 1297C8(C, 0) + 128A48), WS3's countdown (as the first heat's C)
      human._nis_hold(0, 0, 0, 0, 1, 0); human._reset_pad_history(); human._section_point?.(0, 0, 0, 0);
      { const p = str(human, text(ctmPlan.docPath)); try { if (!human._event_route_seed(p)) throw new Error(`${ctmPlan.docPath}: no event route`); } finally { human._free(p); } }
      const [x, y, z] = ctmPlan.start.position; human._event_grid_start(x, y, z, ctmPlan.start.heading);
      if (!args.includes('--no-section-restart')) human._section_restart?.();
      racers.start({ gridStart: true }); console.error('ws13: the semi countdown at record', i);
      if (process.env.WS13_LEAVE) for (const n of racers.npcs) n.core._ground_tick_seed(0, +process.env.WS13_LEAVE); // (diagnostic)
    }
    if (inWorldSetup.ctmInWorldBeforeTick(human, ctmPlan, i, { cstr: (t) => str(human, t), cfile: (f) => str(human, text(f)) }) === 'skip') continue;
    if (i === ctmPlan.C && !args.includes('--no-section-restart')) human._section_restart?.(); // 129768 -> 0x103358 at the Continue (main.js startRun)
    if (i === ctmPlan.C) { racers.start({ gridStart: !!globalThis.__riderHold }); syncWorldNodes(human, racers.npcs.map((n) => n.core));
      if (process.env.RNG_AT_C) { const w = Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)); console.error('rng at the countdown record', records[i].tick, 'web behind', drawsBetween(w, records[i].rng), 'ahead', drawsBetween(records[i].rng, w), 'next record behind', drawsBetween(w, records[i + 1].rng)); }
      if (args.includes('--rng-at-countdown')) racers.setSharedRng(records[i].rng); } // (diagnostic: the free-ride stretch's RNG is not scored here)
    // REPLAY_PROBE (diagnostic, pv eventReturnInWorld (b)): the page's replay start state (web/replay.js liveStart -> main.js snapshot) and
    // every tick's pad from here to the live stop; at the live stop the in-world restart and the re-run are compared with the live state.
    if ((process.env.REPLAY_PROBE || replayReturn) && i === ctmPlan.C) globalThis.__rep = { recording: true, list: [], live: [], at: 0, rng: Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)),
      visual: Array.from(new Uint32Array(human.HEAPU8.buffer, human._visual_rng_words(), 6)), lcg: human._visual_lcg_word ? new Uint32Array(human.HEAPU8.buffer, human._visual_lcg_word(), 1)[0] : null };
    if (replayReturn && !process.env.REPLAY_PROBE && i === ctmPlan.C) { eventSnapshot.snapshotAttach({ human, racers, qa: true }); globalThis.__rep.js = eventSnapshot.snapshotCountdown({ human, racers }); } // (as the page's live start)
    if (process.env.REPLAY_PROBE && i === ctmPlan.C) globalThis.__rep.world = replayWorldDump();
    if (process.env.REPLAY_PROBE && i === ctmPlan.C) console.error('replay probe context bytes', human._rider_context_bytes?.(), 'memory', human.HEAPU8.length);
    if (process.env.REPLAY_PROBE && i === ctmPlan.C) globalThis.__rep.mem = new Uint8Array(human.HEAPU8);
    if (process.env.REPLAY_PROBE && i === ctmPlan.C && human._snapshot_init) { // the rider-context snapshot (web/world_snapshot.hpp): sizes at the countdown's save
      const cstr = (c, p) => { if (!p) return null; let e = p; while (c.HEAPU8[e]) e++; return new TextDecoder().decode(c.HEAPU8.subarray(p, e)); };
      const heap0 = human._snapshot_heap_used?.(); eventSnapshot.snapshotAttach({ human, racers, qa: true }); globalThis.__rep.js = eventSnapshot.snapshotSave(eventSnapshot.SNAPSHOT_COUNTDOWN, { human, racers });
      globalThis.__rep.hash0 = [human, ...racers.npcs.map((n) => n.core)].map((c) => snapshotHashes(c));
      const n = human._snapshot_entries(), rows = []; for (let k = 0; k < n; k++) rows.push({ name: cstr(human, human._snapshot_entry_name(k)), file: cstr(human, human._snapshot_entry_file(k)), bytes: human._snapshot_entry_heap ? [human, ...racers.npcs.map((n) => n.core)].reduce((a, c) => a + c._snapshot_entry_heap(k), 0) : human._snapshot_entry_bytes(k), size: human._snapshot_entry_size(k), flags: human._snapshot_entry_flags(k) });
      { const heap1 = human._snapshot_heap_used?.(); eventSnapshot.snapshotSave(eventSnapshot.SNAPSHOT_RESULTS, { human, racers }); console.error('snapshot heap: init + slot 0', heap1 - heap0, 'bytes; + slot 1', human._snapshot_heap_used() - heap1, '; snapshot_bytes', eventSnapshot.snapshotBytes({ human, racers }), '; first saves by context (slot 0 / slot 1):', [human, ...racers.npcs.map((n) => n.core)].map((c) => c._snapshot_slot_heap(0) + '/' + c._snapshot_slot_heap(1)).join(' ')); }
      globalThis.__rep.entryHash0 = [human, ...racers.npcs.map((n) => n.core)].map((c) => Array.from({ length: n }, (_, k) => c._snapshot_entry_hash(k))); globalThis.__rep.entryRows = Array.from({ length: n }, (_, k) => ({ name: cstr(human, human._snapshot_entry_name(k)), file: cstr(human, human._snapshot_entry_file(k)), flags: human._snapshot_entry_flags(k) }));
      rows.sort((a, b) => b.bytes - a.bytes); console.error('snapshot bytes per context:', [human, ...racers.npcs.map((n) => n.core)].map((c) => c._snapshot_bytes()).join(' '), 'entries', n);
      console.error('snapshot largest copies (all six contexts, one slot):', rows.slice(0, 40).map((r) => `${r.name} (${r.file}) ${r.bytes}`).join(' | ')); }
    if (process.env.REPLAY_PROBE && i === ctmPlan.C && globalThis.__repWorldHashes) { const h = pageHashes(human.HEAPU8), w = globalThis.__repWorldHashes; globalThis.__rep.dirty = h.map((x, p) => (p < w.length && x === w[p] ? 0 : 1)); console.error('replay probe pages dirtied from the first record to the countdown:', globalThis.__rep.dirty.reduce((a, b) => a + b, 0), 'of', h.length); }
    // --ctm-full through the return (pv eventReturnInWorld; local/ctm-events/caps/c0a-ret): the Give Up's pause, results and map leave a gap
    // in the records; record R (free ride's kind again, the game tick back at 0) is WS15 (236058). Before it, as the page's cb.freeRide ->
    // cb.eventInWorldEnd -> startRun -> the in-world Transport: the event's settings go (kind 4 / mode 12, free ride's race document, no event
    // seed), the world resets (230180: startRun's reset_race), 11DE60 / 11DF18 place the human at Session point 1 and the race's riders on the
    // same row (PS2: all six there, riding in rider pairs), and at the WS4 restart (C+0x78 6 -> 1) the riders go.
    // Record R is the placement itself (all six at the row, no tick after it: c0a-ret 3032), so it replaces the tick that makes record R.
    // REPLAY_SNAP_N (diagnostic): the whole memory as the web had it N ticks after the countdown's tick 0, put back before WS15.
    if (process.env.REPLAY_SNAP_N && ctmPlan && i === ctmPlan.C + +process.env.REPLAY_SNAP_N) { globalThis.__replaySnap = { mem: new Uint8Array(human.HEAPU8), tick: records[i].tick }; console.error('replay snapshot at record', i, 'tick', records[i].tick, 'bytes', globalThis.__replaySnap.mem.length); }
    if (ctmReturn && i === ctmReturn.R - 1 && globalThis.__replaySnap) { human.HEAPU8.set(globalThis.__replaySnap.mem, 0); console.error('replay state put back (tick', globalThis.__replaySnap.tick, ')'); }
    // The tick that read the Start finishes after the pause menu: its record (written after the menu) holds the device pad's sample
    // (c0a-ret2 record 3032: sample 356, the Yes's Cross) and the human is finished in it (+0x470 = 1/60 in the next record)
    if (ctmReturn && ctmReturn.pauseAt > 0 && i === ctmReturn.pauseAt) { if (globalThis.__rep?.recording) globalThis.__rep.list.push({ giveUp: true }); human._race_give_up(); console.error('give up before record', i, 'tick', records[i].tick); }
    // --pad-carry (pv padCarry): the pause menu's frames feed the human's pad history (core pad_history_sample: cSSXApp_preUpdate's
    // 0x321298 runs on every app update, menu or game), from its first device sample to the one before the resumed tick's
    if (ctmReturn && ctmReturn.pauseAt > 0 && i === ctmReturn.pauseAt && args.includes('--pad-carry')) { const last = menuSampleAt(i), p = human._malloc(96);
      try { for (let k = 1; k < last; k++) { human.HEAPF32.set(decodePad(menuSegmentAt(k)), p >> 2); human._pad_history_sample(p); } } finally { human._free(p); }
      console.error('pad carry: menu samples 1 ..', last - 1, 'into the history before record', i); }
    if (ctmReturn && i === ctmReturn.R - 1 && ctmReturn.pauseAt > 0) { // the last coast tick, then the Transport's stop frame and the WS14 frame
      tick(process.env.COAST_DEVICE ? decodePad(menuSegmentAt(menuSampleAt(i))) : padFor(records[i].index), aiCapture ? aiCapture.records[i + 1] : null);
      if (replayReturn && globalThis.__rep?.recording) { const rep = globalThis.__rep; rep.recording = false; // the results-time snapshot, the replay, the Transport's restore
        const heap = () => (human._snapshot_heap_used?.() / 1048576).toFixed(2), H = ['before the results save ' + heap()];
        const results = eventSnapshot.snapshotSave(eventSnapshot.SNAPSHOT_RESULTS, { human, racers }); H.push('saved ' + heap()); const rngAt = Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6));
        eventSnapshot.snapshotRestore(eventSnapshot.SNAPSHOT_COUNTDOWN, rep.js, { human, racers }); H.push('countdown restored ' + heap());
        const n = Math.min(+(process.env.REPLAY_TICKS ?? 600), rep.list.length); let peak = 0; for (let k = 0; k < n; k++) { const e = rep.list[k]; if (e.giveUp) human._race_give_up(); else tick(e.pad, e.record); peak = Math.max(peak, +heap()); } H.push('replay peak ' + peak.toFixed(2));
        eventSnapshot.snapshotRestore(eventSnapshot.SNAPSHOT_RESULTS, results, { human, racers }); H.push('results restored ' + heap() + ' (memory ' + (human.HEAPU8.length / 1048576) + ' MB)'); console.error('replay return heap (MB):', H.join(' | '));
        const same = Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)).every((w, k) => w === rngAt[k]);
        console.error('replay return: results-time snapshot saved, the replay ran', n, 'ticks from the countdown snapshot, the results time restored (RNG', same ? 'as saved)' : 'DIFFERS)'); }
      if (globalThis.__rep?.recording) { const rep = globalThis.__rep; rep.recording = false;
        const dump = () => { const all = [human, ...racers.npcs.map((n) => n.core)].map((c) => { const ptr = c._ground_state_dump(), cnt = f32(c, ptr, 1)[0]; return Array.from(f32(c, ptr + 4, cnt * 2)); });
          return { riders: all, rng: Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)), visual: Array.from(new Uint32Array(human.HEAPU8.buffer, human._visual_rng_words(), 6)), tick: human._game_tick?.() }; };
        { const a = rep.mem, b = human.HEAPU8, P = 4096; let pages = 0, bytes = 0; const regions = []; for (let o = 0; o < a.length; o += P) { let d = 0; for (let k = o; k < Math.min(o + P, a.length); k++) if (a[k] !== b[k]) d++; if (d) { pages++; bytes += d; const r = regions[regions.length - 1]; if (r && r[1] === o) r[1] = o + P; else regions.push([o, o + P]); } }
          if (rep.dirty) { let outside = 0; for (let o = 0, p = 0; o < a.length; o += P, p++) { let d = false; for (let k = o; k < Math.min(o + P, a.length) && !d; k++) d = a[k] !== b[k]; if (d && !rep.dirty[p]) outside++; } console.error('replay probe race-changed pages outside the pre-countdown dirty set:', outside); }
          console.error('replay probe memory changed since the countdown:', pages, 'pages of 4 KB,', bytes, 'bytes,', regions.length, 'runs; largest', regions.map((r) => r[1] - r[0]).sort((x, y) => y - x).slice(0, 8).join(' '), 'memory now', b.length, 'tls', human._rider_context_current?.()); rep.mem = null; }
        if (rep.entryHash0) { const cs = [human, ...racers.npcs.map((n) => n.core)], changed = new Map(); cs.forEach((cc, ci) => rep.entryHash0[ci].forEach((h, k) => { if (cc._snapshot_entry_hash(k) !== h) changed.set(k, (changed.get(k) ?? '') + ci); }));
          console.error('snapshot entries changed by the race (context indices):', changed.size, [...changed].map(([k, who]) => `${rep.entryRows[k].name}${rep.entryRows[k].flags & 1 ? '' : '*'}:${who}`).join(' '));
          const unchanged = rep.entryRows.map((r, k) => ({ ...r, k })).filter((r) => !changed.has(r.k) && !(r.flags & 1)); console.error('snapshot container entries unchanged by the race:', unchanged.length, unchanged.map((r) => r.name).join(' ')); }
        const live = dump(), call = (t, f) => { const p = str(human, t); try { return f(p); } finally { human._free(p); } };
        // the countdown snapshot back (web/event-snapshot.js, as the page's replay restart), then the self-check: every subsystem's
        // hash as at the save
        eventSnapshot.snapshotRestore(eventSnapshot.SNAPSHOT_COUNTDOWN, rep.js, { human, racers });
        [human, ...racers.npcs.map((n) => n.core)].forEach((c, ci) => { const now = snapshotHashes(c), was = rep.hash0[ci], bad = Object.keys(was).filter((f) => was[f] !== now[f]); console.error('snapshot self-check context', ci, bad.length ? 'differs: ' + bad.join(' ') : 'equal'); });
        if (rep.entryHash0) [human, ...racers.npcs.map((n) => n.core)].slice(0, 1).forEach((c, ci) => { const bad = rep.entryRows.map((r, k) => ({ ...r, k })).filter((r) => !(r.flags & 2) && c._snapshot_entry_hash(r.k) !== rep.entryHash0[ci][r.k]); console.error('snapshot self-check entries differing (human):', bad.map((r) => `${r.name}${r.flags & 1 ? '' : '*'}`).join(' ')); });
        { const w = replayWorldDump(); for (const f of Object.keys(rep.world)) { const a = rep.world[f], b = w[f]; const n = Array.isArray(a) && Array.isArray(b) ? a.filter((x, k) => x !== b[k]).length : (a === b ? 0 : 'n/a'); if (n) console.error('replay probe world', f, 'words differing at the restart', n, Array.isArray(a) ? a.findIndex((x, k) => x !== b[k]) : ''); } }
        rep.replaying = true; for (const e of rep.list) { if (e.giveUp) human._race_give_up(); else tick(e.pad, e.record); } rep.replaying = false; console.error('replay probe first difference', JSON.stringify(rep.first));
        const again = dump(); console.error('replay probe: ticks', rep.list.length, 'game tick live', live.tick, 're-run', again.tick, 'rng same', live.rng.every((w, k) => w === again.rng[k]), 'visual same', live.visual.every((w, k) => w === again.visual[k]));
        live.riders.forEach((a, r) => { const b = again.riders[r], bad = []; for (let k = 0; k < a.length; k += 2) if (Math.fround(a[k + 1]) !== Math.fround(b[k + 1])) bad.push('0x' + a[k].toString(16) + ' ' + a[k + 1] + ' ' + b[k + 1]);
          console.error('replay probe rider', r, 'fields differing', bad.length, bad.slice(0, 12).join(' | ')); }); }
      const transport = manifest.menus.segments.find((g, k) => g.buttons?.includes('Cross') && manifest.menus.segments.slice(0, k).reduce((a, x) => a + x.frames, 0) >= (manifest.menus_run?.results_cross_from ?? 1000)) ?? {};
      // The stop frame's tick, world state 14's enter (web/event-return.js transportMapEnter, as the page), then the WS14 frame's tick
      for (let k = 0; k < ctmReturn.stopTicks; k++) {
        if (k === 1 && !process.env.RETURN_NO_WS14) eventReturn.transportMapEnter({ human, cstr: (t) => str(human, t), bank: ctmReturn.bank });
        tick(decodePad(transport), null); }
    }
    // Record R is the placement itself (all six at the row, no tick after it: c0a-ret3 3321), so it replaces the tick that makes record R:
    // world state 15's enter as the page runs it (web/event-return.js sessionReturn)
    if (ctmReturn && i === ctmReturn.R - 1) { ctmReturn.placedNow = true; ctmReturn.rowR = rows.length; // (the row of record R)
      if (process.env.PAD_TRACE) globalThis.__padTrace = +process.env.PAD_TRACE;
      eventReturn.sessionReturn({ human, racers, cstr: (t) => str(human, t), bank: ctmReturn.bank, entry: ctmReturn.entry, freeRideDoc: text('PEAK1/initial.json') });
      console.error('return at record', i, 'tick', records[i].tick, 'riders out at', ctmReturn.out);
      if (process.env.PAIR_TRACE) console.error('stance after placement', [human, ...racers.npcs.map((n) => n.core)].map((c) => new Uint32Array(c.HEAPU8.buffer, c._pair_view(), 140)[135]).join(' '));
    }
    if (process.env.PAIR_TRACE && ctmReturn && i === ctmReturn.R + 1 && !globalThis.__pairWrap) { globalThis.__pairWrap = true; const h = human.module ?? human; const host = (human.module ?? human).pairHost ?? globalThis.Module?.pairHost;
      const hm = racers.human?.module; const mod = [human.module, human].find((m) => m && m.pairHost); if (mod) { const t0 = mod.pairHost.translate; mod.pairHost.translate = (slot, x, y, z) => { console.error('translate tick', records[i].tick, 'slot', slot, [x, y, z].map((v) => v.toFixed(2)).join(',')); return t0(slot, x, y, z); }; } else console.error('no pairHost found'); }
    if (process.env.PAIR_TRACE && ctmReturn && i === ctmReturn.R + 2 && globalThis.__pairWrap) { const mod = [human.module, human].find((m) => m && m.pairHost); /* keep */ }
    if (ctmReturn && i === ctmReturn.out) { eventReturn.sessionRidersLeave({ human, racers }); ctmReturn.gone = true; ctmReturn.rowOut = rows.length; } // (record out's bones are the WS2 frame's paused-pass pose, applied here after record out is scored)
    peakWorld.beforeTick(i);
    // The NIS director's camera point (core section_point; 0x281370 / 0x281100 / 0x281400): in from WS1's hold (the fly-over's start),
    // renewed where the capture's A+0xD0 reads -1 (the next script's director: remove + add), out at the Continue; at every tick the
    // outer camera's +0x20 as this record holds it (the previous tick's camera update, which 0x281100 copies before 0x101B60).
    if (human._section_point && !args.includes('--no-section-camera') && i >= ctmPlan.H && i <= ctmPlan.C) {
      const at = i * RECORD + (manifest.layout.outer_camera_000_480 ?? 4288) + 0x20, eye = [dv.getFloat32(at, true), dv.getFloat32(at + 4, true), dv.getFloat32(at + 8, true)];
      const renewed = d0Watch >= 0 && i > ctmPlan.H && dv.getInt32(i * RECORD + manifest.layout.watch_offset + d0Watch, true) === -1;
      if (i === ctmPlan.C) human._section_point(0, 0, 0, 0);
      else { if (renewed) human._section_point(0, 0, 0, 0); human._section_point(1, ...eye); }
    }
    if (process.env.RNG_AT_C && i < ctmPlan.C) { const w = Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)), b = drawsBetween(w, records[i].rng), a = drawsBetween(records[i].rng, w); if (b !== globalThis.__rngOffB || a !== globalThis.__rngOffA) { globalThis.__rngOffB = b; globalThis.__rngOffA = a; console.error('rng record', i, 'tick', records[i].tick, 'web behind', b, 'ahead', a); } }
    if (i < ctmPlan.C) { humanTick(padFor(records[i].index)); human._fx_pass?.(-1); human._section_pass?.();
      if (process.env.SECTION_TRACE) { const [a, b] = process.env.SECTION_TRACE.split(':').map(Number), p = human._set_piece_sections(), n = new Uint32Array(human.HEAPU8.buffer, p, 2), L = new Uint32Array(human.HEAPU8.buffer, p + 8, n[0] * 6);
        const seen = globalThis.__secSeen ?? 0; if (records[i].tick >= a && records[i].tick <= b) { const pos = Array.from(f32(human, human._reference_motion(), 3)).map(Math.round); console.error('section', records[i].tick, 'scans', n[1], 'pos', pos.join(','), 'new', Array.from({ length: n[0] - seen }, (_, k) => Array.from(L.subarray(6 * (seen + k), 6 * (seen + k) + 5)).join('/')).join(' ')); } globalThis.__secSeen = n[0]; }
      inWorldSetup.ctmInWorldAfterTick(human, ctmPlan, i);
      // Under WS1's hold the NIS actor carries the rider (the approach and idle clips): the page follows it every tick (main.js
      // onHumanActor -> nis_hold); here the recorded rider position of the next record (after this tick) stands in for the actor's.
      // The section scan's human point is that position (PS2 c0a-snap: rider+0x110 moves -129756 -> -131138 through the approach).
      // The riders under WS1 (core npc_grid_start's carried words): fresh riders placed at their approach actors from the record where the
      // capture first shows them off their load placement, then ticked held there each tick after the human's pass (the rider manager's order).
      if (aiCapture && !args.includes('--no-rider-hold') && i >= ctmPlan.H && i + 1 < ctmPlan.C - 1) {
        const ai = aiCapture.records[i]?.ai, prevAi = aiCapture.records[i - 1]?.ai;
        if (!globalThis.__riderHold && ai && prevAi && ai.some((a, k) => a.position[0] !== prevAi[k].position[0] && prevAi[k].position[0] !== 0)) globalThis.__riderHold = { from: i, fresh: true };
        if (globalThis.__riderHold) { const h = globalThis.__riderHold;
          racers.holdTick((slot) => { const a = aiCapture.records[i].ai[slot - 1], v = new DataView(a.raw.actor_000_b40.buffer, a.raw.actor_000_b40.byteOffset, 0xB40); return [a.position[0], a.position[1], a.position[2], v.getFloat32(0x1B0, true), v.getFloat32(0x1B4, true)]; }, { fresh: h.fresh });
          if (h.fresh) console.error('riders held from record', i, 'tick', records[i].tick); h.fresh = false; }
      }
      tickHook?.hold?.({ i, tick: records[i + 1]?.tick, core: human, racers }); // (diagnostic: a hook's view of the held ticks, before the actor placement)
      if (!args.includes('--hold-fixed') && i >= ctmPlan.H && i + 1 < ctmPlan.C - 1) { const q = (off) => dv.getFloat32((i + 1) * RECORD + 32 + off - 0x100, true); human._nis_hold(1, q(0x110), q(0x114), q(0x118), q(0x1B0), q(0x1B4)); holdProbeBone(i + 2); }
      continue; }
  }
  if (ctmReturn?.placedNow) ctmReturn.placedNow = false; // (the placement record: nothing ticks)
  else if (ctmReturn?.gone) { humanTick(padFor(records[i].index)); human._fx_pass?.(-1); human._section_pass?.(); } // (the riders left at the WS4 restart)
  // The return's first tick reads the menu pad (menu_pad.py's device script), not the tick script: the map confirm's Cross (menu samples
  // 2054..2062) is still held when WS15 runs and ps2_capture's pad hook comes back on only after that tick (PS2: record R + 1 control 2,
  // the prewind; R + 2 control 5, the jump when the tick script's neutral pad releases it).
  else if (ctmReturn && i === ctmReturn.R && manifest.menus && !process.env.RETURN_TICK_PAD) tick(decodePad(menuSampleWatch >= 0 ? menuSegmentAt(menuSampleAt(i)) : returnPad(manifest.menus)), aiCapture ? aiCapture.records[i + 1] : null);
  // (the coast is the tick script's: menu_pad.py's pad switch is ps2_capture's F_SCORE word, which every coast record's log rewrites, so
  // ps2_capture's pad hook drives the human again from the first record after the pause; COAST_DEVICE feeds the menu pad instead)
  else if (ctmReturn && ctmReturn.pauseAt > 0 && process.env.COAST_DEVICE && i >= ctmReturn.pauseAt && i < ctmReturn.R - 1) tick(decodePad(menuSegmentAt(menuSampleAt(i))), aiCapture ? aiCapture.records[i + 1] : null); // the coast: ps2_capture's pad hook is off, the device pad drives
  else tick(padFor(records[i].index), aiCapture ? aiCapture.records[i + 1] : null);
  if (heat?.pendingHold) { heat.pendingHold(); heat.pendingHold = null; } // --ws13: the NIS actor's placement after the frame (above)
  if (process.env.SECTION_TRACE && ctmPlan && i >= ctmPlan.C && ((i < ctmPlan.C + 3) || (records[i].tick >= +process.env.SECTION_TRACE.split(':')[0] && records[i].tick <= +process.env.SECTION_TRACE.split(':')[1]))) { const p = human._set_piece_sections(), n = new Uint32Array(human.HEAPU8.buffer, p, 2), L = new Uint32Array(human.HEAPU8.buffer, p + 8, n[0] * 6), seen = globalThis.__secSeen ?? 0;
    console.error('section after tick', records[i].tick, 'scans', n[1], 'new', Array.from({ length: n[0] - seen }, (_, k) => Array.from(L.subarray(6 * (seen + k), 6 * (seen + k) + 5)).join('/')).join(' ')); globalThis.__secSeen = n[0]; }
  if (process.env.RNG_AT_C && ctmPlan && i >= ctmPlan.C && (i < ctmPlan.C + 3 || (process.env.RNG_WIN && records[i].tick >= +process.env.RNG_WIN.split(':')[0] && records[i].tick <= +process.env.RNG_WIN.split(':')[1]))) { const w = Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6)); console.error('after tick', records[i].tick, 'web vs record', records[i + 1].tick, 'behind', drawsBetween(w, records[i + 1].rng), 'ahead', drawsBetween(records[i + 1].rng, w)); }
  const ps2 = records[i + 1];
  tickHook?.tick({ i, tick: ps2.tick, core: human, racers });
  if (stageSnaps?.has(ps2.tick)) stageCompare.push(compareStageWorld(human, stageSnaps.get(ps2.tick), { allDiffs: !!process.env.STAGE_WORLD_ALLDIFFS }));
  if (stageDump?.has(ps2.tick)) { // STAGE_WORLD_DUMP=t,...: the drawable particle effects after tick t-1 (sprite bounds, source cm)
    const effects = particleEval.readParticleEffects(human), out = new Float32Array(1 << 20), me = Array.from(f32(human, human._reference_motion(), 3));
    for (const e of effects) { const n = e.kind === 0 ? particleEval.burstSpritesFast(e.K, e.F, out, 0, 1 << 17) : particleEval.trailSpritesFast(e.K, e.F, e.capacity, e.cursor, e.ringA, e.ringB, e.ringBits, out, 0, 1 << 17);
      const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity]; let size = 0, alpha = 0;
      for (let k = 0; k < n; k++) { for (let a = 0; a < 3; a++) { lo[a] = Math.min(lo[a], out[k * 8 + a]); hi[a] = Math.max(hi[a], out[k * 8 + a]); } size = Math.max(size, out[k * 8 + 3]); alpha = Math.max(alpha, out[k * 8 + 7]); }
      if (process.env.STAGE_WORLD_SPRITES && e.kind === 1) console.error(Array.from({ length: n }, (_, k) => Array.from(out.subarray(k * 8, k * 8 + 8)).map((x) => Math.round(x)).join(' ')).join('\n'));
      console.error(`tick ${ps2.tick} rider ${me.map((x) => x.toFixed(0))} effect ${e.kind ? 'trail' : 'burst'} res ${e.resource} tex ${e.textureId} blend ${e.blend} sprites ${n} lo ${lo.map((x) => x.toFixed(0))} hi ${hi.map((x) => x.toFixed(0))} maxHalf ${size.toFixed(1)} maxA ${alpha}`); }
  }
  compareHumanScore(i + 1, ps2.tick);
  const motion = Array.from(f32(human, human._reference_motion(), 20));
  const humanExact = same(motion.slice(0, 3), ps2.position) && same(motion.slice(3, 6), ps2.velocity);
  if (process.env.HUMAN_TRACE) { const [a, b] = process.env.HUMAN_TRACE.split(':').map(Number); if (ps2.tick >= a && ps2.tick <= b) {
    console.error(`human ${ps2.tick} ps2 ctl ${ps2.control} pos ${ps2.position.map((x) => x.toFixed(4))} vel ${ps2.velocity.map((x) => x.toFixed(4))}`);
    console.error(`      web ctl ${motion[11]} gnd ${motion[10]} pos ${motion.slice(0, 3).map((x) => x.toFixed(4))} vel ${motion.slice(3, 6).map((x) => x.toFixed(4))}`); } }
  if (process.env.BONE_TRACE && (ctmReturn ? i >= ctmReturn.R - 3 && i <= ctmReturn.R + (+process.env.BONE_TRACE > 1 ? +process.env.BONE_TRACE : 6) : false)) { const wb = human._world_pose_bones(), nb = f32(human, wb, 1)[0], w = f32(human, wb + 4, nb * 7), at = (i + 1) * RECORD + manifest.layout.world_bones_32x32;
    let worst = 0, wi = -1; const per = []; for (let b = 0; b < Math.min(nb, 32); b++) { let m = 0; for (let k = 0; k < 3; k++) { const d = Math.abs(w[b * 7 + k] - dv.getFloat32(at + b * 32 + 4 * k, true)); m = Math.max(m, d); if (d > worst) { worst = d; wi = b; } } per.push(+m.toFixed(3)); } if (+process.env.BONE_TRACE > 1) console.error('bones per', ps2.tick, per.join(' '));
    console.error('bones', ps2.tick, 'count', nb, 'worst position diff', worst.toFixed(4), 'bone', wi, 'web b0', Array.from(w.slice(0, 7)).map((x) => x.toFixed(2)).join(','), 'ps2 b0', Array.from({ length: 8 }, (_, k) => dv.getFloat32(at + 4 * k, true).toFixed(2)).join(',')); }
  if (process.env.SEQ_TRACE && ctmReturn && i >= ctmReturn.R + 5 && i <= ctmReturn.out + 2) { const p = human._animation_sequences_info(), n = f32(human, p, 1)[0], v = f32(human, p + 4, n); const rows = []; // SEQ_TRACE (diagnostic): the human's animation sequences after each tick
    for (let k = 0; k + 26 <= n; k += 26) rows.push(`ch${v[k]} sem${v[k + 1]} w${v[k + 4].toFixed(3)}/${v[k + 5].toFixed(3)} fade${v[k + 6].toFixed(3)} clip${v[k + 11]} t${v[k + 12].toFixed(4)} r${v[k + 3].toFixed(3)}`); console.error('seq', i + 1, ps2.tick, rows.join(' | '));
    { const at = (i + 1) * RECORD + manifest.layout.sequences_6x216_channel_address_d0, cnt = dv.getUint32((i + 1) * RECORD + manifest.layout.sequence_count, true), out = [];
      for (let q = 0; q < Math.min(cnt, 6); q++) { const b = at + q * 216; out.push(`ch${dv.getUint32(b, true)} ` + Array.from({ length: 52 }, (_, w) => { const u = dv.getUint32(b + 8 + 4 * w, true), f = dv.getFloat32(b + 8 + 4 * w, true); return (Math.abs(f) > 1e-6 && Math.abs(f) < 1e6) ? +f.toFixed(4) : (u < 0x10000 ? u : '0x' + u.toString(16)); }).join(',')); }
      console.error('ps2seq', i + 1, ps2.tick, cnt, out.join(' || ')); } }
  if (process.env.PAIR_TRACE && ctmReturn && i >= ctmReturn.R - 1 && i <= ctmReturn.R + 9) { const hm = f32(human, human._reference_motion(), 3); console.error('pos', ps2.tick, 'human web', Array.from(hm).map((x) => x.toFixed(1)).join(','), 'ps2', ps2.position.map((x) => x.toFixed(1)).join(','));
    racers.npcs.forEach((n, k) => { const m = f32(n.core, n.core._reference_motion(), 3); console.error('pos', ps2.tick, n.character, 'web', Array.from(m).map((x) => x.toFixed(1)).join(','), 'ps2', ps2.others[k].position.map((x) => x.toFixed(1)).join(',')); }); }
  if (process.env.PAIR_TRACE && ctmReturn && i >= ctmReturn.R - 1 && i <= ctmReturn.R + 9) console.error('pairs after record', i + 1, 'tick', ps2.tick, racers.pairCounts.join(','), 'world', racers.worldState.slice(0, 40).map((x) => +(+x).toFixed(1)).join(' '));
  if (process.env.HUMAN_FIELD) { const [fa, fb] = process.env.HUMAN_FIELD.split(':').map(Number); if (ps2.tick >= fa && ps2.tick <= fb) { // HUMAN_FIELD=a:b (diagnostic): the human's ground_state_dump fields that differ from record i+1
    const ptr = human._ground_state_dump(), cnt = f32(human, ptr, 1)[0], pairs = f32(human, ptr + 4, cnt * 2);
    for (let j = 0; j < cnt; j++) { const o = pairs[2 * j], wv = pairs[2 * j + 1]; if (o + 4 > 0xb40) continue; const pv = dv.getFloat32((i + 1) * RECORD + 32 + o - 0x100, true); if (Math.fround(wv) !== pv || o === +process.env.HFIELD_OFF) console.error('hfield', ps2.tick, '0x' + o.toString(16), 'web', wv, 'ps2', pv); } } }
  if (!humanExact && !first.human) first.human = { tick: ps2.tick, errCm: dist(motion.slice(0, 3), ps2.position) };
  const rng = Array.from(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6));
  // The record samples the RNG at the human's provider exit, after the next tick's entity pass (race_begin in the port): a world
  // draw of that pass (a LiveComp timer program: Gravitude's billboard) is in the record one tick before the browser has made
  // it. A difference the next record no longer shows is such a sampling blip (listed), not the first RNG difference.
  const rngOff = rng.some((w, k) => w !== ps2.rng[k]);
  if (!first.rng && pendingRng !== null) { if (rngOff) first.rng = { tick: pendingRng }; else rngBlips.push(pendingRng); pendingRng = null; }
  if (!first.rng && rngOff && pendingRng === null && !rngBlips.includes(ps2.tick)) { if (drawsBetween(rng, ps2.rng) > 0 && drawsBetween(rng, ps2.rng) <= 8) pendingRng = ps2.tick; else first.rng = { tick: ps2.tick }; }
  // --rng-resync (diagnostic): list every tick whose draws differ, then continue from the PS2 state.
  if (resync && rng.some((w, k) => w !== ps2.rng[k])) {
    const log = aiCapture ? aiCapture.records[i + 1].rng.log : []; const ps2Draws = [];
    for (const e of log) { const who = e.rider ? (e.rider.slot < 0 ? 0 : e.rider.slot + 1) : '?'; const last = ps2Draws[ps2Draws.length - 1]; if (last && last.who === who && last.ra === e.ra) last.n++; else ps2Draws.push({ who, ra: e.ra.toString(16), pass: e.pass ? e.pass.toString(16) : '?', n: 1 }); }
    rngEvents.push(`tick ${ps2.tick - 1}: web ${tickDraws.join(' ')} | ps2 ${ps2Draws.map((x) => `${x.who}@${x.ra}/${x.pass}x${x.n}`).join(' ')}`);
    racers.setSharedRng(ps2.rng);
  }
  const row = { tick: ps2.tick, humanExact, ai: [] };
  if (aiCapture) { // 10F560 of tick T runs before record T: this browser tick's world vs record i.
    const rec = aiCapture.records[i], ranks = [rec.human.rank, ...rec.ai.map((a) => a.rank)], web = ranks.map((_, s) => racers.worldState[360 + s * 6]); // 2 riders in the backcountry rival events
    if (ranks.some((r, s) => r !== web[s]) && !first.rank) first.rank = { tick: rec.tick, web, ps2: ranks };
    if (process.env.RANK_TRACE && heat && i >= heat.G && i < heat.G + +process.env.RANK_TRACE) console.error('ranks', i, rec.tick, 'web', web.join(','), 'ps2', ranks.join(','), 'remaining web', racers.standings().map((r) => Math.round(r.remaining)).join(','), 'ps2', [rec.human.remaining, ...rec.ai.map((a) => a.remaining)].map(Math.round).join(',')); // (diagnostic)
    for (let k = 0; k < rec.ai.length; k++) for (let b = 0; b <= rec.ai.length; b++) { const pr = rec.ai[k].pairRecords[b], wr = racers.worldState.slice(((k + 1) * 6 + b) * 10, ((k + 1) * 6 + b) * 10 + 10);
      if ((pr.enabled !== 0) !== (wr[0] !== 0) || Math.fround(wr[2]) !== pr.distance || Math.fround(wr[3]) !== pr.bearing) { const m = { tick: rec.tick, slot: k + 1, other: b, web: wr.slice(0, 4), ps2: [pr.enabled, pr.human, pr.distance, pr.bearing] }; if (!first.records) first.records = m; if (ctmReturn?.rowR != null && rows.length > ctmReturn.rowR && !first.returnRecords) first.returnRecords = { ...m, row: rows.length }; } }
    rowsRank.push(ranks.join('')); }
  if (rngTrace && aiCapture && ps2.tick >= rngTrace[0] && ps2.tick <= rngTrace[1]) { const log = aiCapture.records[i + 1].rng.log;
    const ps2Draws = []; for (const e of log) { const who = e.rider ? (e.rider.slot < 0 ? 0 : e.rider.slot + 1) : '?'; const last = ps2Draws[ps2Draws.length - 1]; if (last && last.who === who && last.ra === e.ra) last.n++; else ps2Draws.push({ who, ra: e.ra.toString(16), n: 1 }); }
    console.error(`rng tick ${ps2.tick - 1}: web ${tickDraws.join(' ')} | ps2 ${ps2Draws.map((x) => `${x.who}@${x.ra}x${x.n}`).join(' ')}`); }
  racers.npcs.forEach((n, k) => {
    const m = Array.from(f32(n.core, n.core._reference_motion(), 20));
    const o = ps2.others[k];
    const exact = same(m.slice(0, 3), o.position) && same(m.slice(3, 6), o.velocity);
    const err = dist(m.slice(0, 3), o.position);
    row.ai.push({ exact, errCm: +err.toFixed(3), control: m[11], ground: m[10] });
    if (!exact && !first.ai[k]) first.ai[k] = { tick: ps2.tick, errCm: err, velErr: dist(m.slice(3, 6), o.velocity), control: m[11], words: wordsHex(n), draws: n.info?.[5] };
    if (err > 1 && !first.ai1cm[k]) first.ai1cm[k] = { tick: ps2.tick, errCm: err, control: m[11] };
    if (aiCapture) { // record i+1 (tick T): provider words of tick T-1 = this browser tick; state after tick T-1.
      const rec = aiCapture.records[i + 1], a = rec.ai[k];
      const w0 = (n.info[0] + n.info[1] * 65536) >>> 0, w1 = (n.info[2] + n.info[3] * 65536) >>> 0;
      if (w0 !== a.words[0] || w1 !== a.words[1]) note(k, 'words', ps2.tick, [w0.toString(16), w1.toString(16), n.info[4]], [a.words[0].toString(16), a.words[1].toString(16)]);
      const ws = Array.from(f32(n.core, n.core._rider_world_state(), 16)), pr = Array.from(f32(n.core, n.core._race_progress_info(), 8));
      { // 11B3F8 runs in the pre-pass 120F20, before record T is written: record T holds tick T's limit.
        const pre = aiCapture.records[i].ai[k], dvp = new DataView(pre.raw.actor_000_b40.buffer, pre.raw.actor_000_b40.byteOffset);
        const lim = f32(n.core, n.core._physics_info(), 1)[0] * 100; if (process.env.LIMIT_SLOT && +process.env.LIMIT_SLOT === k + 1) console.error('limit', ps2.tick - 1, lim.toFixed(5), dvp.getFloat32(0x2e4, true).toFixed(5), 'motion', Array.from(f32(n.core, n.core._rider_world_state(), 16))[9], pre.motionMode); if (Math.fround(lim) !== dvp.getFloat32(0x2e4, true)) note(k, 'speedLimit2E4', ps2.tick - 1, lim, dvp.getFloat32(0x2e4, true)); }
      if (ws[8] !== a.controlState) note(k, 'control', ps2.tick, ws[8], a.controlState);
      if (ws[9] !== a.motionMode) note(k, 'motion', ps2.tick, ws[9], a.motionMode);
      if (Math.fround(ws[15]) !== a.timeScale) note(k, 'timeScale300', ps2.tick, ws[15], a.timeScale);
      if (ws[6] !== a.routePath) note(k, 'routePathAB8', ps2.tick, ws[6], a.routePath);
      if (Math.fround(pr[0]) !== a.remaining) note(k, 'remaining4D0', ps2.tick, pr[0], a.remaining);
      const q = ws.slice(11, 15); if (q.some((x, j) => Math.fround(x) !== a.quaternion[j])) note(k, 'quaternion120', ps2.tick, q, a.quaternion);
      // Field-level: the rider struct fields the core exposes (ground_state_dump: offset/value pairs).
      const ptr = n.core._ground_state_dump(); const cnt = f32(n.core, ptr, 1)[0]; const pairs = f32(n.core, ptr + 4, cnt * 2);
      const actorRaw = new DataView(a.raw.actor_000_b40.buffer, a.raw.actor_000_b40.byteOffset, a.raw.actor_000_b40.byteLength);
      for (let j = 0; j < cnt; j++) { const off = pairs[2 * j], wv = pairs[2 * j + 1]; if (off + 4 > 0xb40 || off === 0x2e4) continue; const pv = actorRaw.getFloat32(off, true); if (Math.fround(wv) !== pv) note(k, '0x' + off.toString(16), ps2.tick, wv, pv); }
      // NPC_FIELD=slot:hexOffset:a:b (diagnostic): that ground_state_dump field of the slot, web and PS2, each tick in [a, b].
      if (process.env.NPC_FIELD) { const [fs_, fo, fa, fb] = process.env.NPC_FIELD.split(':'); if (+fs_ === k + 1 && ps2.tick >= +fa && ps2.tick <= +fb) for (let j = 0; j < cnt; j++) { const o = pairs[2 * j]; if (o + 4 > 0xb40) continue; const wv = pairs[2 * j + 1], pv = actorRaw.getFloat32(o, true); if (fo === 'all' ? Math.fround(wv) !== pv : o === parseInt(fo, 16)) console.error('field', ps2.tick, 'slot', k + 1, '0x' + o.toString(16), 'web', wv, 'ps2', pv); }
        if (+fs_ === k + 1 && ps2.tick >= +fa && ps2.tick <= +fb) console.error('contact', ps2.tick, 'slot', k + 1, Array.from(f32(n.core, n.core._terrain_contact_info(), 12)).map((x) => +x.toFixed(4)).join(','), 'ps2 surface +0x438', actorRaw.getInt32(0x438, true), 'patch?', actorRaw.getInt32(0x434, true)); }
      const np = n.core._npc_state_dump(); const nc = f32(n.core, np, 1)[0]; const npairs = f32(n.core, np + 4, nc * 2);
      const ownerRaw = new DataView(a.raw.owner_de0_f50.buffer, a.raw.owner_de0_f50.byteOffset, a.raw.owner_de0_f50.byteLength);
      for (let j = 0; j < nc; j++) { const off = npairs[2 * j], wv = npairs[2 * j + 1]; const isInt = [0xe0c, 0xe10, 0xe14, 0xe18, 0xe1c, 0xe20, 0xe24, 0xe28, 0xe40, 0xe70, 0xe74, 0xe78, 0xf34].includes(off);
        const pv = isInt ? ownerRaw.getInt32(off - 0xde0, true) : ownerRaw.getFloat32(off - 0xde0, true); if (Math.fround(wv) !== pv) note(k, 'owner+0x' + off.toString(16), ps2.tick, wv, pv); }
      // 10F560 of tick T ran before record T: compare this browser's refresh at tick T (next beginTick) later.
    }
    if (trace && trace[0] === k + 1 && ps2.tick >= trace[1] && ps2.tick <= trace[2]) {
      console.error(`tick ${ps2.tick} slot ${k + 1} ${n.character} limit ${(f32(n.core, n.core._physics_info(), 1)[0] * 100).toFixed(4)} ctl ${m[11]} gnd ${m[10]} words ${wordsHex(n)} draws ${n.info?.[5]} scale ${n.info?.[6]} path ${n.info?.[7]}`);
      console.error(`   web pos ${m.slice(0, 3).map((x) => x.toFixed(3))} vel ${m.slice(3, 6).map((x) => x.toFixed(3))}`);
      console.error(`   ps2 pos ${o.position.map((x) => x.toFixed(3))} vel ${o.velocity.map((x) => x.toFixed(3))}`);
      console.error(`   web air ${Array.from(f32(n.core, n.core._npc_air_info(), 9)).map((x) => +x.toFixed(4))} | ps2 words ${aiCapture ? aiCapture.records[i + 1].ai[k].words.map((w) => w.toString(16)) : ''}`);
      if (aiCapture) { const ow = new DataView(aiCapture.records[i + 1].ai[k].raw.owner_de0_f50.buffer, aiCapture.records[i + 1].ai[k].raw.owner_de0_f50.byteOffset); const np = n.core._npc_state_dump(); const nc = f32(n.core, np, 1)[0]; const pr = f32(n.core, np + 4, nc * 2);
        console.error('   trick web', [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].map((j) => +pr[2 * (3 + j) + 1].toFixed(3)).join(','), '| ps2', [0xe0c, 0xe10, 0xe14, 0xe18, 0xe1c, 0xe20, 0xe24, 0xe28].map((o) => ow.getInt32(o - 0xde0, true)).concat([0xe2c, 0xe30, 0xe34, 0xe38, 0xe3c].map((o) => +ow.getFloat32(o - 0xde0, true).toFixed(3))).join(',')); }
    }
  });
  rows.push(row);
}
// --weather: a capture watching the weather state (tools/export_weather.py objects of the baseline: the human's Weather
// painter object 0xA0 bytes + its wrapper +8 (distance source), the camera block 6 painter, the snowfall layers 0xB4 bytes
// each, the camera-0 splash +0x10 / +0x100C, 0x4FF018 and gp+0xA0C): record N = after tick N-1, as the browser before tick N.
// Ranges: the first differing tick per field group; the capture's addresses come from --weather-map (JSON {rider, camera,
// layers[], splash}).
function compareWeather(i, tick) {
  const at = i * RECORD + manifest.layout.watch_offset, U = (k) => dv.getUint32(at + k, true), m = weatherWatch;
  const note = (key, web, ps2) => { weatherStats.ticks[key] = (weatherStats.ticks[key] || 0) + 1; if (web === ps2) { weatherStats.exact[key] = (weatherStats.exact[key] || 0) + 1; return; } if (!weatherStats.first[key]) weatherStats.first[key] = { tick, web, ps2 }; };
  const Fb = (arr, k) => { const b = new Float32Array(1); b[0] = arr[k]; return new Uint32Array(b.buffer)[0]; };
  if (m.rider >= 0) { const w = new Float32Array(human.HEAPU8.buffer, human._weather_painter_info(), 40);
    for (let k = 0; k < 19; k++) { note(`rider.cur${k}`, Fb(w, 2 + k), U(m.rider + 8 + 8 * k)); } note('rider.distance', Fb(w, 1), U(m.rider)); }
  if (m.camera >= 0) { const w = new Float32Array(human.HEAPU8.buffer, human._weather_info(), 30);
    for (let k = 0; k < 19; k++) note(`camera.cur${k}`, Fb(w, 11 + k), U(m.camera + 8 + 8 * k)); note('camera.distance', Fb(w, 10), U(m.camera)); }
  if (m.layers.length) { const L = new Uint32Array(human.HEAPU8.buffer, human._weather_layers(), 1 + 25 * 6);
    m.layers.forEach((o, n) => { if (o < 0) return; const b = 1 + 25 * n; note(`layer${n}.count`, L[b + 2], U(o + 8)); note(`layer${n}.timer`, L[b + 17], U(o + 0x90));
      for (let k = 0; k < 3; k++) { note(`layer${n}.offset`, L[b + 8 + k], U(o + 0x20 + 4 * k)); note(`layer${n}.gust`, L[b + 11 + k], U(o + 0x30 + 4 * k)); } }); }
  if (m.splash >= 0) { const S = new Float32Array(human.HEAPU8.buffer, human._weather_splash(), 5);
    note('splash.drops', S[0], U(m.splash + 4)); note('splash.crystals', S[1], U(m.splash + 8)); note('splash.speed', Fb(S, 2), U(m.splash2 + 0x14)); note('splash.pending', Fb(S, 3), U(m.splash2 + 0x18)); note('splash.snowfall', Fb(S, 4), U(m.splash2 + 0x1C)); }
  if (m.visual >= 0) { const w = new Uint32Array(human.HEAPU8.buffer, human._visual_rng_words(), 6); note('visual.w5', w[5], U(m.visual + 20)); }
  if (process.env.WEATHER_TRACE && tick <= +process.env.WEATHER_TRACE) { const w = new Float32Array(human.HEAPU8.buffer, human._weather_info(), 11), S = new Float32Array(human.HEAPU8.buffer, human._weather_splash(), 5); console.error('weather', tick, 'eye', Array.from(w.slice(7, 10)).map((x) => x.toFixed(2)).join(','), 'splash', Array.from(S).map((x) => +x.toFixed(4)).join(','), 'steps', w[2]); }
  if (manifest.layout?.outer_camera_000_480 !== undefined) { const w = new Float32Array(human.HEAPU8.buffer, human._weather_info(), 11), base = i * RECORD + manifest.layout.outer_camera_000_480 + 0x20; for (let k = 0; k < 3; k++) note('camera.eye', Fb(w, 7 + k), dv.getUint32(base + 4 * k, true)); }
  if (m.lcg >= 0) note('lcg', new Uint32Array(human.HEAPU8.buffer, human._visual_lcg_word(), 1)[0], U(m.lcg));
}
const summary = { capture: capturePath, ticks: rows.length, ctmReturn: ctmReturn ? { row: ctmReturn.rowR ?? null, outRow: ctmReturn.rowOut ?? null } : null, humanScore: scoreState, firstHumanInexact: first.human, firstRngMismatch: first.rng, rngBlips: rngBlips.slice(0, 40), firstRankMismatch: first.rank || null, firstPairRecordMismatch: first.records || null, firstReturnPairRecordMismatch: first.returnRecords || null, // (from the in-world return's record on)
  finalRanks: rowsRank[rowsRank.length - 1],
  ai: racers.npcs.map((n, k) => ({ slot: n.slot, character: n.character, firstInexact: first.ai[k], firstOver1cm: first.ai1cm[k], exactTicks: rows.filter((r) => r.ai[k].exact).length })),
  standings: racers.standings(), rngEvents, injectedWorldDraws: injected,
  fields: fieldFirst.map((fields) => Object.fromEntries(Object.entries(fields).sort((a, b) => a[1].tick - b[1].tick).slice(0, +(process.env.FIELDS_MAX || 12)))) };
if (stageWorld) { // stage world activity (web/stage_world.inc): LiveComp starts, slot 4/5 programs, particle effects
  const U = () => new Uint32Array(human.HEAPU8.buffer), at = (fn) => fn.call(human) >> 2;
  const info = U().slice(at(human._stage_world_info), at(human._stage_world_info) + 7);
  const sp = at(human._stage_world_slots), slots = Array.from({ length: U()[sp] }, (_, k) => Array.from(U().subarray(sp + 1 + 4 * k, sp + 5 + 4 * k)));
  const lp = at(human._stage_world_livecomps), starts = Array.from({ length: U()[lp] }, (_, k) => [U()[lp + 1 + 16 * k], U()[lp + 2 + 16 * k], U()[lp + 16 + 16 * k]]);
  summary.stageWorld = { loaded: info[0], alive: info[1], effects: info[2], particleCreates: info[3], refused: info[4], liveCompStarts: info[5], slotRuns: info[6],
    ps2: stageCompare, starts, firstSlots: slots.filter((x, i, a) => i === 0 || a[i - 1][1] !== x[1] || a[i - 1][2] !== x[2]).slice(0, 80) };
}
if (sharedVisual) summary.visual = visualFirst;
if (weatherWatch) summary.weather = weatherStats;
if (tickHook?.summary) Object.assign(summary, await tickHook.summary());
// Avalanches (web/avalanche_gameplay.inc) per core, human first: [definitions, triggers, ticks, active slots, piece entities].
if (human._avalanche_info) summary.avalanches = [human, ...racers.npcs.map((n) => n.core)].map((c) => { const p = c._avalanche_info() >> 2, U = new Uint32Array(c.HEAPU8.buffer);
  return [U[p], U[p + 1], U[p + 2], U[p + 3], c._avalanche_entities ? new Uint32Array(c.HEAPU8.buffer)[c._avalanche_entities() >> 2] : 0]; });
console.log(JSON.stringify(summary, null, 1));
if (reportPath) fs.writeFileSync(reportPath, JSON.stringify({ summary, rows }, null, 1));
