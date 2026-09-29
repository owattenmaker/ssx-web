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
import { createAiRacers, rngNext } from './ai-racers.js';
import { readAiCapture } from './ps2-capture-ai.mjs';
import { loadStageWorld, compareStageWorld, loadSnapshots } from './stage-world-compare.mjs';

const args = process.argv.slice(2);
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
let humanText = resources.initialText;
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
human._init_world(put(human, resources.collision), resources.collision.length / 4);
const hash = str(human, resources.terrainHash);
human._init_terrain(str(human, resources.terrainText));
human._init_world_collision(str(human, resources.worldCollisionText), hash);
human._init_body_terrain(str(human, resources.terrainText));
human._init_rails(str(human, resources.railsText), hash);
// Section activation 0x101B60 runs natively (web/section_gameplay.inc): its slot-1 draws (0x341bbc, builtin3 key 8)
// are no longer injected from the capture. --no-sections keeps the older injection for comparison.
const sectionsFile = new URL(`${courseCode}/SECTIONS/sections.json`, root);
const nativeSections = !args.includes('--no-sections') && !!human._init_sections && fs.existsSync(sectionsFile);
if (nativeSections) human._init_sections(str(human, fs.readFileSync(sectionsFile, 'utf8')));
// Stage world (web/stage_world.inc): the particle / LiveComp-timer data the browser loads (set-pieces-renderer.js); its
// timer programs draw the shared RNG (builtin3 key8/key6, builtin19). --no-stage-world leaves it out.
const stageWorld = !args.includes('--no-stage-world') && loadStageWorld(human, root, courseCode);
const drawsBetween = (a, b) => { const w = a.slice(); for (let n = 0; n <= 200; n++) { if (w.every((x, k) => x === b[k])) return n; rngNext(w); } return -1; };
let tickDraws = [];
let pendingRng = null; const rngBlips = [];
const rngTrace = process.env.RNG_TRACE ? process.env.RNG_TRACE.split(':').map(Number) : null;
// --shared-visual: one visual stream (0x4FF018 + LCG gp+0xA0C) across the rider cores in the original pass order (FX passes
// after every rider's 121818, phase-major; world visual pass; camera last), seeded from --visual-state (a
// tools/export_world_visual_state.py export of the location's ready savestate) and checked against a capture that watches
// 0x4ff018:0x18 (and 0x4a3afc:4).
const sharedVisual = args.includes('--shared-visual');
const racers = await createAiRacers({ human, resources, document: doc, isolate: args.includes('--isolate'), sharedVisual,
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
const WORLD_DRAW_CALLERS = new Set(nativeSections ? [] : [0x341bbc]); // 0x359460 (spline modifier from a rider's trigger) now runs natively in that rider's core
let pendingWorld = null; let injected = 0;
function injectWorldDraws(slot) {
  if (!pendingWorld) return 0; let n = 0;
  for (const e of pendingWorld) if (!e.done && (e.slot === slot || (e.slot === null && slot === -1))) { e.done = true; injected++; n++; }
  return n;
}

// ---- capture ----
const manifest = JSON.parse(fs.readFileSync(capturePath.replace(/\.bin$/, '.capture.json'), 'utf8'));
const RECORD = manifest.record || 8192;
const raw = fs.readFileSync(capturePath);
const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
const records = [];
for (let at = 0; at + RECORD <= raw.length; at += RECORD) {
  const u = (o) => dv.getUint32(at + o, true), f = (o) => dv.getFloat32(at + o, true);
  records.push({ tick: u(4), index: u(28), control: u(20), position: [f(32 + 0x10), f(32 + 0x14), f(32 + 0x18)], velocity: [f(32 + 0xE0), f(32 + 0xE4), f(32 + 0xE8)],
    others: [0, 1, 2, 3, 4].map((k) => ({ position: [f(3008 + 32 * k), f(3012 + 32 * k), f(3016 + 32 * k)], velocity: [f(3024 + 32 * k), f(3028 + 32 * k), f(3032 + 32 * k)] })),
    rng: Array.from({ length: 6 }, (_, k) => u(8896 + 4 * k)) });
}
const BUTTONS = ['Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown', 'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2'];
const R255 = new Float32Array(new Uint32Array([0x3b808081]).buffer)[0];
function towardZero(x) { let r = Math.fround(x); if (Math.abs(r) > Math.abs(x)) { const b = new Float32Array([r]); const u = new Uint32Array(b.buffer); u[0] -= 1; r = b[0]; } return r; }
const axisByte = (v) => Math.floor((Math.max(-1, Math.min(1, v)) + 1) * 127.5 + 0.5);
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
  globalThis.__compareStage = 'race_begin'; human._race_begin();
  globalThis.__compareStage = 'step_rider'; const state = f32(human, human._step_rider(o[0], o[6], o[2] ? 1 : 0, o[7]), 16);
  globalThis.__compareStage = 'animation_tick'; human._animation_tick(state[7], o[10], o[11], state[9], state[8], o[6], o[8], o[12], o[7], 0, state[15], o[13]);
  const pose = f32(human, human._pose_physical(), 12);
  globalThis.__compareStage = 'race_end'; human._race_end();
  if (sharedVisual) globalThis.__pendingCameraHead = [pose[9], pose[10], pose[11]]; else human._step_camera_head(pose[9], pose[10], pose[11]);
}
function tick(pad, record = null) { tickDraws = []; pendingWorld = record && worldDraws ? record.rng.log.filter((e) => WORLD_DRAW_CALLERS.has(e.ra)).map((e) => ({ slot: e.rider ? (e.rider.slot < 0 ? 0 : e.rider.slot + 1) : null })) : null;
  globalThis.__compareStage = 'begin'; racers.beginTick(); humanTick(pad); globalThis.__compareStage = 'end'; racers.endTick();
  if (sharedVisual && globalThis.__pendingCameraHead) { human._step_camera_head(...globalThis.__pendingCameraHead); globalThis.__pendingCameraHead = null; } } // cameras after every rider pass (0x230D7C)

// ---- event start and pre-anchor grid ticks ----
human._reset_pad_history(); human._start_event(); racers.start();
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
for (let t = 0; t < records[0].tick; t++) tick(neutral);
racers.setSharedRng(records[0].rng);
// Human score object + HUD bank (docs/tricks-scoring.md), seeded from the anchor record as compare-ps2-capture.mjs --event does
// (stale front-end words in the bank), then compared per tick like it; and the boost words (+0x2F8 meter, +0x2F4 tier, +0x2F0 time).
const scoreLayout = manifest.layout?.score_000_1d0, hudLayout = manifest.layout?.hud_slots_44x6;
const SCORE_WORDS = 0x1d0 / 4, HUD_WORDS = 44 * 6, SCORE_SKIP = new Set([0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1cc]);
const scoreState = { ticks: 0, exact: 0, first: null, boostTicks: 0, boostExact: 0, firstBoost: null };
if (scoreLayout && hudLayout && human._score_object_seed) { const w = new Uint32Array(SCORE_WORDS + HUD_WORDS);
  for (let k = 0; k < SCORE_WORDS; k++) w[k] = dv.getUint32(scoreLayout + 4 * k, true); for (let k = 0; k < HUD_WORDS; k++) w[SCORE_WORDS + k] = dv.getUint32(hudLayout + 4 * k, true);
  const p = human._malloc(w.byteLength); human.HEAPU8.set(new Uint8Array(w.buffer), p); human._score_object_seed(p, records[0].tick); human._free(p); }
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
for (let i = 0; i + 1 < records.length && i < limit; i++) {
  globalThis.__compareTick = records[i].tick;
  if (weatherWatch) compareWeather(i, records[i].tick);
  if (sharedVisual && visWatch >= 0) { // the stream at the record point (loop start = after the previous tick's cameras)
    const at = i * RECORD + manifest.layout.watch_offset, ps2w = Array.from({ length: 6 }, (_, k) => dv.getUint32(at + visWatch + 4 * k, true)), webw = Array.from(new Uint32Array(human.HEAPU8.buffer, human._visual_rng_words(), 6));
    if (!visualFirst.words && ps2w.some((x, k) => x !== webw[k])) visualFirst.words = { tick: records[i].tick, webBehind: drawsBetween(webw, ps2w), webAhead: drawsBetween(ps2w, webw) };
    if (lcgWatch >= 0 && !visualFirst.lcg) { const ps2l = dv.getUint32(at + lcgWatch, true), webl = new Uint32Array(human.HEAPU8.buffer, human._visual_lcg_word(), 1)[0]; if (ps2l !== webl) visualFirst.lcg = { tick: records[i].tick, web: webl.toString(16), ps2: ps2l.toString(16) }; }
    if (process.env.VISUAL_SPAN_AI && globalThis.__visPrev) { const [a, b2] = process.env.VISUAL_SPAN_AI.split(':').map(Number); if (records[i].tick - 1 >= a && records[i].tick - 1 <= b2) console.error('vspan', records[i].tick - 1, 'web', drawsBetween(globalThis.__visPrev[0], webw), 'ps2', drawsBetween(globalThis.__visPrev[1], ps2w)); }
    globalThis.__visPrev = [webw, ps2w];
  }
  tick(padFor(records[i].index), aiCapture ? aiCapture.records[i + 1] : null);
  const ps2 = records[i + 1];
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
    for (let k = 0; k < rec.ai.length; k++) for (let b = 0; b <= rec.ai.length; b++) { const pr = rec.ai[k].pairRecords[b], wr = racers.worldState.slice(((k + 1) * 6 + b) * 10, ((k + 1) * 6 + b) * 10 + 10);
      if ((pr.enabled !== 0) !== (wr[0] !== 0) || Math.fround(wr[2]) !== pr.distance || Math.fround(wr[3]) !== pr.bearing) { if (!first.records) first.records = { tick: rec.tick, slot: k + 1, other: b, web: wr.slice(0, 4), ps2: [pr.enabled, pr.human, pr.distance, pr.bearing] }; } }
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
const summary = { capture: capturePath, ticks: rows.length, humanScore: scoreState, firstHumanInexact: first.human, firstRngMismatch: first.rng, rngBlips: rngBlips.slice(0, 40), firstRankMismatch: first.rank || null, firstPairRecordMismatch: first.records || null, finalRanks: rowsRank[rowsRank.length - 1],
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
console.log(JSON.stringify(summary, null, 1));
if (reportPath) fs.writeFileSync(reportPath, JSON.stringify({ summary, rows }, null, 1));
