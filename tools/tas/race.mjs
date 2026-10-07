// The TAS race in node (docs/tas.md "Node harness"): the page's Single Event race set up again from a run's start state
// (tools/tas/page-run.mjs start.json, or a replay file's meta) the way Watch Replay does it (web/online-replay.js begin / load /
// play -> main.js startRun(R)), and the page's 60 Hz tick (web/game-tick.js simulate, the aiActive path) without the presentation.
// One core holds the human and the five computer riders (rider contexts, web/ai-racers.js), so a whole-race branch point is a
// copy of that core's memory plus the orchestrators' JS state (save() / restore()).
//
//   const race = await createTasRace({ start });   // start: {snapshot, rider, attributes, lineup}
//   race.tick(channels24) -> {finish, raceInfo}; race.save() / race.restore(s); race.progress(); race.state()
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
export const WEB = path.resolve(here, '../../web');
const ASSETS = path.join(WEB, 'public', 'assets');

// The page's fetch of /assets/... from web/public/assets (a missing file is a 404, as the dev server's would be for JSON).
const TYPES = { '.json': 'application/json', '.bin': 'application/octet-stream', '.png': 'image/png' };
function installFetch() {
  if (globalThis.__tasFetch) return;
  const real = globalThis.fetch;
  globalThis.fetch = async (url, init) => {
    const u = String(url);
    if (!u.startsWith('/assets/')) return real(url, init);
    const file = path.join(ASSETS, decodeURIComponent(u.slice('/assets/'.length).split('?')[0]));
    if (!fs.existsSync(file)) return new Response('not found', { status: 404, headers: { 'content-type': 'text/plain' } });
    const type = TYPES[path.extname(file)] ?? 'application/octet-stream';
    return new Response(fs.readFileSync(file), { status: 200, headers: { 'content-type': type } });
  };
  globalThis.__tasFetch = true;
}
// Fresh page storage (the page runs start from cleared localStorage / sessionStorage).
class Memory {
  constructor() {
    this.map = new Map();
  }
  getItem(k) {
    return this.map.has(k) ? this.map.get(k) : null;
  }
  setItem(k, v) {
    this.map.set(k, String(v));
  }
  removeItem(k) {
    this.map.delete(k);
  }
  clear() {
    this.map.clear();
  }
}
function installStorage() {
  for (const k of ['localStorage', 'sessionStorage']) {
    const d = Object.getOwnPropertyDescriptor(globalThis, k);
    if (!d || d.get) Object.defineProperty(globalThis, k, { value: new Memory(), configurable: true, writable: true });
  }
}

const read = (p) => fs.readFileSync(path.join(ASSETS, p));
const text = (p) => read(p).toString('utf8');

export async function createTasRace({ start, course: code = 'BRA2', coreJs = process.env.CORE_JS || path.join(WEB, 'runtime', 'core.js') } = {}) {
  installFetch();
  installStorage();
  const createCore = (await import(pathToFileURL(coreJs).href)).default;
  const { createAiRace } = await import(pathToFileURL(path.join(WEB, 'ai-race.js')).href);
  const { loadCharacter, humanSettings, applyHumanPairInputs } = await import(pathToFileURL(path.join(WEB, 'character-roster.js')).href);
  const { collectApply } = await import(pathToFileURL(path.join(WEB, 'stage-collect.js')).href);
  const courses = JSON.parse(text('courses.json')).courses;
  const course = courses.find((c) => c.code === code);
  const roster = JSON.parse(text('riders.json'));
  const entry = roster.find((r) => r.id === start.rider);
  if (!course || !entry) throw new Error(`unknown course ${code} or rider ${start.rider}`);
  const core = await createCore();
  const keep = [];
  const put = (bytes) => {
    const p = core._malloc(bytes.length);
    core.HEAPU8.set(bytes, p);
    keep.push(p);
    return p;
  };
  const str = (s) => put(Buffer.from((typeof s === 'string' ? s : JSON.stringify(s)) + '\0'));
  const f32 = (ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
  const u32 = (ptr, n) => new Uint32Array(core.HEAPU8.buffer, ptr, n);
  // main.js loadCourse
  const spawn = JSON.parse(text(`${code}/start.json`));
  core._set_fx_deferred?.(1);
  const padPtr = core._malloc(96);
  const collision = read(`${code}/collision.bin`);
  core._init_world(put(collision), collision.length / 4);
  const metadata = text('ANIMATIONS/animation-packets.json');
  const baseSettings = JSON.parse(text(`${code}/initial.json`));
  const packets = read('ANIMATIONS/animation-packets.bin');
  const terrainText = text(`${code}/terrain.json`);
  const hash = str(JSON.parse(terrainText).source_sha256);
  core._init_terrain(str(terrainText));
  core._init_world_collision(str(text(`${code}/world_collision.json`)), hash);
  core._init_body_terrain(str(terrainText));
  core._init_fog(str(text(`${code}/fog-tree.json`)));
  if (core._init_weather) core._init_weather(str(text(`${code}/weather.json`)));
  core._init_rider_lighting(str(text(`${code}/local-lights.json`)), str(text(`${code}/light-tree.json`)));
  core._init_rails(str(text(`${code}/rails.json`)), hash);
  // the computer riders (main.js: createAiRace with no scene)
  const aiRace = await createAiRace({ T: null, scene: null, human: core, course, loader: null, origin: null, humanName: () => entry.name, isolate: false });
  // the human rider (main.js loadRiderNow -> initializeRiderAnimation): its settings over the course initial.json, its rig
  const rider = { ...entry, entry, uber_choice: [], uber: '' };
  const character = await loadCharacter(rider);
  const settings = humanSettings(baseSettings, character);
  applyHumanPairInputs(aiRace, character);
  const rig = text(`${entry.package}/rider.json`);
  const resetPhysics = () => {
    core._reset_rider(...spawn.position, spawn.heading);
  };
  const settingsPtr = str(settings);
  core._init_animation(str(metadata), str(rig), settingsPtr, put(packets), packets.length);
  core._init_race(settingsPtr);
  core._animation_use_physics(1);
  resetPhysics();
  core._animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
  // the lineup (web/online-replay.js load: ai.fixedNext = meta.lineup -> prepare -> prepareFixed)
  if (start.lineup) aiRace.fixedNext = start.lineup;
  await aiRace.prepare({ rider });
  // the attribute bytes (main.js ui.cb.attributes -> set_rider_attributes(raw, 0))
  if (start.attributes && core._set_rider_attributes) {
    const p = core._malloc(28);
    new Int32Array(core.HEAPU8.buffer, p, 7).set(start.attributes);
    core._set_rider_attributes(p, 0);
    core._free(p);
  }
  const R = start.snapshot;
  const host = { lastRescues: 0, finished: false, finish: null, tick: 0 };
  // main.js startRun(R): the restart path a replay runs
  function startRun() {
    core._race_time_limit?.(R.timeLimit);
    core._reset_fog();
    core._reset_rider_lighting();
    core._reset_animation();
    core._reset_race();
    core._reset_pad_history();
    core._set_input_map?.(R.inputMap);
    resetPhysics();
    core._set_camera_view?.(R.cameraView);
    collectApply(core, R.collect);
    core._start_event();
    aiRace.start({ replay: R.ai });
    const state = f32(core._rider_state(), 16);
    host.lastRescues = state[13];
    host.finished = false;
    host.finish = null;
    host.tick = 0;
    u32(core._animation_rng_words(), 6).set(R.rng);
    u32(core._visual_rng_words(), 6).set(R.visual);
    if (R.lcg != null && core._visual_lcg_word) u32(core._visual_lcg_word(), 1)[0] = R.lcg;
  }
  startRun();
  const boneCount = JSON.parse(rig).bones?.length ?? 0;
  // web/game-tick.js simulate, aiActive (the presentation's reads left out; the collect / career queues drained as a replay does)
  function tick(input) {
    core.HEAPF32.set(input, padPtr >> 2);
    aiRace.beginTick();
    const cmd = f32(core._pad_tick(padPtr), 24).slice();
    core._race_begin();
    let state = f32(core._step_rider(cmd[0], cmd[6], cmd[2] ? 1 : 0, cmd[7]), 16).slice();
    if (state[13] !== host.lastRescues) {
      host.lastRescues = state[13];
      core._reset_animation();
    }
    core._animation_tick(state[7], cmd[10], cmd[11], state[9], +state[8], cmd[6], cmd[8], cmd[7], cmd[7], 0, state[15], cmd[13]);
    const pose = f32(core._pose_physical(), 12).slice();
    const raceInfo = f32(core._race_end(), 8).slice();
    aiRace.endTick();
    core._step_camera_head(pose[9], pose[10], pose[11]);
    let finish = null;
    if (raceInfo[2]) {
      host.finished = true;
      const r = f32(core._race_result_info(), 6);
      const dump = new Int32Array(core.HEAPU8.buffer, core._score_object_dump(), 0x1d0 / 4);
      finish = { score: dump[0x198 / 4] | 0, ticks: r[1], dnf: !!core._race_timed_out?.(), tick: host.tick };
      host.finish = finish;
      core._finish_celebrate?.((aiRace.hud()?.place ?? 0) < 3 ? 1 : 0);
    }
    core._stage_collect_events?.();
    core._score_career_events?.();
    host.tick++;
    return { finish, raceInfo };
  }
  // A branch point: the core's whole memory and the JS state of the riders' orchestrators (web/ai-racers.js saveState) and of
  // this harness. The buffer is reused when given.
  function save(into = null) {
    const heap = core.HEAPU8;
    const mem = into?.mem && into.mem.length === heap.length ? into.mem : new Uint8Array(heap.length);
    mem.set(heap);
    return { mem, racers: aiRace.racers.saveState(), host: { ...host }, ai: aiExtra() };
  }
  function restore(s) {
    if (core.HEAPU8.length !== s.mem.length) throw new Error('core memory grew since the save');
    core.HEAPU8.set(s.mem);
    aiRace.racers.restoreState(s.racers);
    Object.assign(host, s.host);
    restoreAiExtra(s.ai);
  }
  // web/ai-race.js keeps the relationship tables (changed by rider-pair reactions) outside the core
  function aiExtra() {
    return JSON.stringify({ tables: aiRace.relationships, scores: aiRace.doc.relationships.scores, kinds: aiRace.doc.relationships.kinds });
  }
  function restoreAiExtra(j) {
    const v = JSON.parse(j);
    aiRace.replayRestore({ tables: v.tables, scores: v.scores, kinds: v.kinds });
  }
  const progress = () => f32(core._race_progress_info(), 8).slice();
  const state = () => f32(core._rider_state(), 16).slice();
  const rng = () => u32(core._animation_rng_words(), 6).slice();
  return { core, aiRace, tick, save, restore, progress, state, rng, host, startRun, boneCount, spawn };
}
