let lastResetPlacement = 0;
// the title's load meter (web/boot-screen.js, docs/first-load.md): the game code has started; game files report their bytes as they stream
globalThis.ssxBoot?.step('js', 1);
onDownloadBytes((url, got, total) => globalThis.ssxBoot?.bytes(url, got, total));
// first: field diagnostics for real devices (web/diagnostics.js)
import { diagnoseRenderer, diagnose, diagRide } from './diagnostics.js';
import { installDeviceRecovery } from './gpu-recovery.js';
// first: game-data fetch sharing + download bar (web/downloads.js)
import { onDownloadBytes, prefetchDownload, peekDownload, downloadProgress } from './downloads.js';
import * as T from 'three/webgpu';
import { setDrawOrderWorld, WORLD_SORTED_ORDER } from './ps2-draw-order.js';
// the frame's colour space, set before any Color or material is built (pv encodedBlend)
import { configureFrameSpace, frameTextureSpace, linearOutput, linearTextureSpace, frameBufferType } from './frame-space.js';
// ?originalWorld=0 fallback: sRGB-decoded copies of the package textures
const linearMaps = new WeakMap();
// between the world's own draws (0) and the effects (>= 1)
const SORTED_CLASS_ORDER = WORLD_SORTED_ORDER;
// identical node graphs -> identical WGSL, one module + pipeline (docs/firefox-load.md)
import './shader-keys.js';
import { OriginalUI, drawStreamingNote } from './ui.js';
// sliceLoad (docs/ctm-flow.md)
import {
  initEnvironmentSliced,
  cutEventWorld,
  feedEventWorld,
  feedContextWorld,
  feedEventRails,
  initWorldStepped,
  framePause
} from './load-slices.js';
import { createCutscenes, playCutscene, COURSE_CODES } from './cutscenes.js';
import { releasePreviewCores } from './fe-preview.js';
const cutscene = (o) => (ui.cb.cutscene ? ui.cb.cutscene(o) : Promise.resolve({ played: false }));
// the original in-engine cutscenes (NIS): event intros, podium, transport, lodge (docs/cutscenes.md)
let cutscenes = null;
import { widescreenView } from './widescreen.js';
import { createBoardTrail } from './board-trail.js';
import { createSnowRenderer } from './snow-renderer.js';
import { createWakeRenderer } from './wake-renderer.js';
import { createBoostRenderer } from './boost-renderer.js';
import { createFogRenderer } from './fog-renderer.js';
let fogRenderer;
let terrainRefinement;
let riderLightingUpdate;
let animationResources,
  selectedRider = bootRider();
// the boot model (and first pick) is Zoe, else Sam
function bootRider() {
  return { id: 'zoe', name: 'Zoe', package: 'RIDER_ZOE' };
}
let wakeRenderer, boostRenderer;
// weather-renderer.js: snowfall, blowing snow, lens drops (docs/weather.md)
let snowRenderer,
  weatherRenderer = null,
  terrainSparkle = null,
  lightningStrikes = 0;
let boardTrail;
import { captureRiderFrame, applyRiderFrame } from './rider-frame.js';
import { originalRiderBoneInverses } from './rider-bind.js';
import { createOriginalRiderSkinning } from './rider-skinning.js';
import { createTerrainRefinement } from './terrain-refinement.js';
import { createRiderLightingMaterial, createRiderLightingUpdate, riderDrawState } from './rider-material.js';
import { createOriginalWorldMaterials, createOriginalSkyMaterials } from './world-material.js';
import { createScreenTint } from './screen-tint.js';
import { createSunFlare } from './sun-flare.js';
import { createLightGlow } from './light-glow.js';
import { createGlarePass } from './glare-pass.js';
let screenTint, sunFlare, lightGlow, glarePass, riderShadows;
let currentRiderFrame = null,
  previousRiderFrame = null,
  posePhysicalFrame = null;
import { FixedStepClock } from './fixed-step-clock.js';
import {
  gameTrace,
  traceOut,
  memoryHash,
  TRACE_OUT_FLOATS,
  createGameTick,
  readTrickHudSlots,
  RESULTS_TICKS_FINISH,
  RESULTS_TICKS_TIME_UP
} from './game-tick.js';
import { buildPad, createKeyboardContext, loadKeyboardMode, saveKeyboardMode } from './pad-input.js';
import { pollPads } from './gamepad.js';
import { installPadMenus } from './gamepad-menus.js';
import { createTouchControls } from './touch-controls.js';
// phones: touch deck + presentation quality (docs/mobile.md)
import { quality, sizeRenderer, frameGate, onQualityChange } from './quality.js';
import { updateMovingInstances } from './moving-instances.js';
import { createSetPieceRenderer } from './set-pieces-renderer.js';
let padPtr = 0;
const simulation = new FixedStepClock();
import createCore from './runtime/core.js';
import { createAiRace } from './ai-race.js';
import { syncWorldNodes } from './ai-racers.js';
import { settleRaced, humanCharacter, seededWords, nextWord } from './lineup.js';
import { EVENT_ANCHOR_DRAWS, eventAnchorWords } from './event-anchor-rng.js';
import { createMpGame } from './net/mp-game.js';
import { createOpponentFx } from './opponent-fx.js';
import { pv } from './pv-flags.js';
import { installYieldShim, yieldBudget } from './yield-shim.js';
import { startGcWatchdog, isJavaScriptCore } from './gc-watchdog.js';
import { collectBefore, collectNow, coreTracker } from './switch-gc.js';
import { ps2FrameTime } from './ps2-frame-pacing.js';
import { organizeStaticWorld } from './static-world.js';
import { registerGpuRestore, restoreGpuCopies, fetchBuffer, releaseWorldCopies, forgetGpuRestore } from './gpu-copies.js';
import { abortCompiles } from './compile-abort.js';
import { PLANE_DROP } from './plane-drop.js';
import { startOpensPause } from './start-rules.js';
// when Start pauses (0x230A34)
import { CTX, BIT as CTX_BIT } from './pause-contexts.js';
import {
  createFreeRide,
  peakCourseEntry,
  stationStart,
  peakWorldOf,
  freeRideWorldOf,
  freeRideHolds,
  peakRunWorld,
  PEAK_DEFAULT_STATION,
  isStreamedWorld
} from './free-ride.js';
import { drawFreeRideHud, drawPeakRunHud } from './free-ride-hud.js';
import { createBigChallenges } from './big-challenges.js';
let freeRide = null;
let bigChallenges = null;
let faqPending = false;
// pv faqDefer: gp-0x1024 != -1 (web/main.js 'faq')
// Big Challenges in free ride (web/big-challenges.js)
// connected Peak 1 world: free ride / peak runs (?course=PEAK1, web/free-ride.js, docs/peak-mountain.md)
function faqOpen() {
  if (!faqPending) return;
  const cu = ui.careerUI;
  if (!cu?.freeRide) {
    faqPending = false;
    return;
  }
  // world state 4 only: not under a station / Transport cut (WS14 / 11 / 10)
  if (!running || isPaused() || ui.screen !== 'game' || cutscenes?.active || nisTransport || !cu.messages?.ready) return;
  faqPending = false;
  overlay.open();
  cu.messages.openFaq(() => {
    ui.set('game');
    ui.cb.resume();
  });
}
// computer riders' board trails, wakes and snow spray (web/opponent-fx.js)
let opponentFx = null;
import { createRivalBeam, createRiderIcons, BEAM_BONE } from './rival-beam.js';
// backcountry rival locator beam (RFX+0xB00, docs/backcountry.md)
let rivalBeam = null,
  riderIcons = null;
import { createGameAudio, COURSE_INDEX } from './game-audio.js';

// original music, DJ and mixer (web/game-audio.js, docs/audio-logic.md)
// songs stream bar by bar, behind the game's own downloads
const gameAudio = createGameAudio({ musicStream: { busy: () => downloadProgress().active } });
gameAudio.whenReady().catch((e) => console.warn('Audio unavailable', e));
let audioScreen = null,
  audioPhase = null,
  audioFinished = false;
import { MultiplayerScreens } from './mp-ui.js';
// online races (web/net/mp-game.js)
let mpGame = null;
import { loadCharacter, humanSettings, applyHumanPairInputs } from './character-roster.js';
import { collectStart, collectApply, collectPoll } from './stage-collect.js';
import { createReplay } from './replay.js';
// the race replay behind the post-race screens and the results' Replay item (docs/replay.md)
import { ReplayUi } from './replay-ui.js';
import { createRiderShadows, initRiderShadowAtlas } from './rider-shadow.js';
// PS2 rider shadows on the terrain (world agent) // Conquer the Mountain collectibles <-> career save
import { pruneEncodedEffects } from './snow-composite.js';
import { transportMapEnter, sessionReturn, sessionRidersLeave, requireReturnCore } from './event-return.js';
// pv eventReturnInWorld (b): an in-world event's replay snapshots // pv eventReturnInWorld: the in-world return (the c0a-ret3 gate runs the
// same)
import {
  snapshotAttach,
  snapshotSave,
  snapshotRestore,
  snapshotCountdown,
  snapshotReleaseRiders,
  requireSnapshotCore,
  SNAPSHOT_COUNTDOWN,
  SNAPSHOT_RESULTS
} from './event-snapshot.js';
// R&B/Crow's Nest event configuration (docs/slopestyle-bigair.md)
import { loadFreestyleEvent, applyFreestyleEvent, liveTimeLimit } from './freestyle-event.js';
import { loadProgressMeter, progressMeterRiders } from './progress-meter-hud.js';
// in-race progress meter 0x20EDA0 (HUD flag 0x40)
let progressMeter = null;
// world texture library / texture archives (docs/asset-formats.md)
import {
  packageTexture,
  packageTextureBlob,
  pngTexture,
  prefetchTextureArchive,
  textureSource,
  configureTextureDecode
} from './texture-archive.js';
// Equip Gear outfits: generated rider packages (docs/characters.md)
import { outfitRider, wardrobeFile, riderSamplesUrl } from './wardrobe.js';
// pv onlineRecords: the online boards' run replays (docs/online-records.md)
import { installOnlineReplay } from './online-replay.js';
import { withProfile, riderStamp } from './career-rider.js';
// the profile's attribute bytes on every career run (free ride too) // careerRider: the profile's gear and ubers on the
// human at every world / event load and run start
import { runAttributes } from './buy-attribs.js';
import { createWeatherRenderer } from './weather-renderer.js';
import { createTerrainSparkle } from './terrain-sparkle.js';
// pad vibration (FE Controller Settings > Vibration 1P): the original 0x125B18 motor model
import { Rumble } from './rumble.js';
const rumble = new Rumble();
// Ubertrick Setup preview: web/fe-screens.js
// Select Character FE preview: camera, placement, FE clips (web/character-select.js)
const frontEndRider = () =>
  ui.equipGear?.owns(ui.screen) ? ui.equipGear : ui.feScreens?.ownsRider?.(ui.screen) ? ui.feScreens : ui.characterSelect;
// Original computer riders (web/ai-race.js): one core per opponent; `?ai=0` races alone.
let aiRace = null,
  aiActive = false;
// ui.cb.standings / lineup: module-level, not closures made inside loadCourse. A course without computer riders (the streamed world) does
// not set them again, and in JavaScriptCore those closures kept loadCourse's whole scope of the course that made them, and through it that
// course's core: the menu boot course's 128 MB wasm memory and ~130 MB of its load stayed through the first career ride (WebKit; Chrome
// freed them; docs/mobile.md "Load spikes")
const aiStandings = () => (aiActive ? aiRace.standings() : null),
  aiLineup = () => (aiRace ? aiRace.names() : []);
// web/freestyle-event.js
let freestyleEvent = null;
const $ = (s) => document.querySelector(s),
  keys = new Set(),
  v = new T.Vector3(),
  up = new T.Vector3(0, 1, 0);
const NEUTRAL_PAD = new Float32Array(24);
let postFinishTicks = 0,
  raceInfo = null,
  finished = false,
  cameraPose = null,
  previousCameraPose = null,
  allSamples = null,
  animPose = null,
  animInfo = null,
  animationReady = false,
  route = [],
  lastRescues = 0,
  renderer,
  core,
  scene,
  skyScene,
  camera,
  sam,
  rig,
  clips,
  bones,
  sky,
  ready = false,
  running = false,
  clock = 0,
  score = 0,
  pending = 0,
  trick = '',
  trickUntil = 0,
  last = 0,
  acc = 0,
  poseTime = 0,
  activeClip = '',
  state,
  spawn,
  fps = 0;
// Score object HUD message bank (0x1488DE0 layout: 44 slots x {type,maximum,value,arg,field10,points}) + total (+0x198) for
// web/trick-hud.js.
// web/game-tick.js
const trickHudSlots = () => readTrickHudSlots(core);
// Course/event (tools/locations.py -> /assets/courses.json): ?course=CODE loads that location; Snow Jam keeps its historical asset roots.
let course = {
    code: 'ARA1',
    name: 'Snow Jam',
    event: 'race',
    label: 'Snow Jam - Race',
    root: '/assets/ARA1/',
    sky: '/assets/SKY/',
    sunFlare: '/assets/SUN_FLARE/',
    lightGlow: '/assets/LIGHT_GLOW/',
    startfire: '/assets/STARTFIRE/',
    initial: '/assets/ANIMATIONS/initial.json',
    eventStart: true
  },
  courses = [course];
const DEFAULT_COURSE = course;
const load = async (path, type = 'json') => {
  const v = wardrobeFile(path, type);
  if (v !== undefined) return v;
  const r = await fetch(path);
  if (!r.ok) throw Error(`${path}: ${r.status}`);
  return type === 'json' ? r.json() : r.arrayBuffer();
};
const origin = new T.Vector3(),
  lastCamera = new T.Vector3();
const loader = new T.TextureLoader();
const ui = new OriginalUI({
  warmup: () => {
    // the chosen rider's race model (ensureRider), the event's computer-rider lineup for this human (web/lineup.js), then the warm-up (a
    // lazy first course first: pv lazyCourse). riderPrefetch: the rider's files from the pick, the lineup planned (and downloading)
    // before the rider loads, the intro's data under the warm-up
    prefetchEventRider();
    return (lazyPending() ? courseLive() : Promise.resolve())
      .then(async () => {
        if (riderStale()) await selectRider(selectedRider.entry);
        // ensureRider's first step, before the plan
        prefetchLineup(eventOf());
        return ensureRider();
      })
      .then(() =>
        aiRace && (mpGame?.aiAllowed() ?? true)
          ? aiRace.prepare({ rider: selectedRider, event: eventOf() }).catch((e) => console.warn('Computer-rider lineup failed', e))
          : Promise.resolve()
      )
      .then(() => {
        prefetchIntro();
        return warmupRender(ui.warmFramesDone);
      });
  },
  // FE rider speech (web/rider-speech.js)
  speech: (bank, options) => gameAudio.speak(bank, options),
  keyboard: (mode) => setKeyboardMode(mode),
  rider: selectRider,
  start: (...a) => {
    overlay.drop();
    // the pause's Restart: the menu goes with the run
    startRun(...a);
  },
  course: selectCourse,
  // whole-peak runs (career-ui.js transport): the streamed world at the top of the peak
  freeRide: (peakCourse, opt) => {
    // pv eventReturnInWorld (docs/ctm-events-in-world.md stage 5): back from an in-world event through the map (results Transport, WS14 ->
    // WS15 / another location): no world load; the event's settings go (cb.eventInWorldEnd: kind 4 / mode 12, free ride's race document,
    // the riders' contexts to the pool) and the run goes on in the same world through the in-world Transport (the same location: Session
    // point 1 and the white fade, 236058)
    // in the streamed world: transport inside it (web/free-ride.js transport, world state 14 arg 1); else load the world (opt.reload: a CTM
    // start from the front end is always a world load, 1A0B00 -> cPreGameLoadScreen)
    if (
      pv('eventReturnInWorld') &&
      worldEvent &&
      freeRide &&
      !opt?.reload &&
      course?.freeRide?.kind === 4 &&
      freeRideHolds(course, peakCourse)
    ) {
      (async () => {
        // the results' Transport: 0x2706F0 puts back the results-time state (the live ticks stopped there: gameHost.liveStopAt), then its
        // stop frame and the WS14 frame each run one tick before the map holds the world (PS2 c0a-ret2: map total ticks = the last live
        // tick + 3)
        requireReturnCore(core);
        inWorldResultsRestore();
        const here = freeRide.course(),
          bank = freeRide.bankFor(here);
        // the stop frame's tick, world state 14's enter 0x236250 arg 2 (web/event-return.js transportMapEnter: the boost meter, 11D390's
        // event branch on the event's start row and the grid hold), the WS14 frame's tick
        gameTick.simulate(inputs());
        if (!bank) throw new Error('In-world return: no path bank for course ' + here);
        transportMapEnter({ human: core, cstr: coreString, bank });
        gameTick.simulate(inputs());
        // the same location: world state 15 (236058) as web/event-return.js sessionReturn runs it (no rider reset: 230180 and 11D390's
        // free-ride branch), then the page's run state (returnRun)
        const entry = peakCourse === here && !(here >= 14 && here <= 16) ? freeRide.spawnFor(here, 1, 2) : null;
        if (entry) {
          await ui.cb.eventInWorldEnd({ keepRiders: pv('eventInWorldAi'), session: { bank, entry } });
          returnRun();
          return;
        }
        await ui.cb.eventInWorldEnd({ keepRiders: pv('eventInWorldAi') });
        // WS15 -> WS1 arg 0 -> WS3 -> WS4: the free-ride run again (its start state; the placement below)
        startRun();
        transportInWorld(peakCourse);
      })().catch((e) => console.warn('In-world return', e));
      return 'transport';
    }
    // another peak's world (docs/peak3.md): load it
    if (!opt?.reload && freeRide && course.freeRide?.kind === 4 && running && freeRideHolds(course, peakCourse)) {
      // the whole mountain is one world (every transport inside it); a peak world keeps its own stations
      transportInWorld(peakCourse);
      return 'transport';
    }
    overlay.drop();
    // a Transport's Yes into another page world: the MCOMM's context pops at the Yes (its ride across the switch already played)
    return selectFreeRide(peakCourse, !!opt?.reload);
  },
  peakRun: (mode) => selectPeakRun(mode),
  freeRideRespawn: (station) => {
    const s = course.freeRide && freeRide?.spawnFor(station ?? 17);
    if (s) {
      spawn = s;
      resetPhysics(true);
      freeRide.afterReset();
    }
  },
  // the lodge's Return to Game is a world load at the station (0x1A11C0 -> 118loadoutlodge -> WS10, PS2 lr-tri): the
  // world start's ride, a new rider at the 11D390 entry (web/free-ride.js worldEntry) with the ride start and the world load's audio
  // (startRun: gameAudio.runStart out of the lodge)
  freeRideWorldLoad: (station) => {
    const s = course.freeRide && freeRide?.worldEntry?.(station ?? 17);
    if (!s) return false;
    spawn = s;
    // the lodge's world load: the station's overlay goes
    overlay.drop();
    startRun();
    return true;
  },
  // MCOMM Session (overlay 0x20 -> world state 15, 236058): 11DE60(rider, point, 2) at the current course, 11DF18
  freeRideSession: (point) => {
    const c = freeRide?.course();
    const s = course.freeRide && c >= 0 && freeRide.spawnFor(c, point, 2);
    if (s) {
      spawn = s;
      resetPhysics(true);
      freeRide.afterReset();
      // world state 15's white fade (2E4370, 1.0 s, the HUD over it)
      cutscenes?.fadeFrom?.({ ticks: 60, colour: 'white', hud: true });
    }
  },
  freeRideCourse: () => (freeRide ? freeRide.course() : -1),
  // sessionMap (web/career-ui.js drawSessionMap): the location's session points 1..8 (region kind 2; 26B5E0 falls back to the first row
  // past the last) and the rider, world cm
  freeRideSessionMap: () => {
    const c = freeRide ? freeRide.course() : -1;
    if (!(c >= 0) || !core?._rider_world_state) return null;
    const points = Array.from({ length: 8 }, (_, k) => freeRide.spawnFor(c, k + 1, 2)?.region?.position?.slice() ?? null);
    return { course: c, points, rider: Array.from(new Float32Array(core.HEAPF32.buffer, core._rider_world_state(), 3)) };
  },
  // world screen 11 (location crossing): MCOMM overlay 4 has no Transport / Session
  freeRideCrossing: () => !!freeRide && !core?._peak_world_course_ready?.(),
  // career Buy Attributes -> original stat getters (web/attribute_bridge.cpp)
  // freestyle run time limit for the next start (career-ui.js; core 125228)
  timeLimit: (ticks) => {
    nextTimeLimit = ticks | 0;
  },
  // slope style computer opponent: OPPONENT line and the heat result (web/ai-race.js opponent)
  opponent: (atFinish) => (aiRace ? (atFinish ? (aiRace.opponentAtFinish ?? aiRace.opponent()) : aiRace.opponent()) : null),
  attributes: (bytes) => {
    if (!core?._set_rider_attributes) return;
    const p = core._malloc(28);
    new Int32Array(core.HEAPU8.buffer, p, 7).set(bytes);
    core._set_rider_attributes(p, 0);
    core._free(p);
  },
  resume: () => overlay.close(),
  // pause Give Up (0x20DA58 -> 1253D0): DNF the unfinished run and close the menu; the next tick finishes it with TIME'S UP
  giveUp: () => {
    replay?.note('giveUp');
    core?._race_give_up?.();
    overlay.close();
  },
  quit: () => {
    running = false;
    clearInput();
    rumble.stop();
    gameAudio.leaveWorld();
    // the menu goes with the run (Quit, a Restart's quit)
    overlay.drop();
  },
  camera: (type) => {
    replay?.note('camera', type);
    return core ? core._set_camera_view(type) : type;
  },
  widescreen: () => layoutStage()
});
ui.gameAudio = gameAudio;
// pause contexts (web/pause-contexts.js): the audio pause follows the stack (a holder pushed with audio: 289B70 / 289BB8); a stopped
// simulation stops the pad's motors
const contexts = ui.contexts;
contexts.onChange(({ audio, sim, audioChanged, simChanged }) => {
  if (simChanged && sim) rumble.stop();
  if (audioChanged) gameAudio.pause(audio);
});
/* the lodge exit is a world load on the PS2 (cGameLoadStateOutLodge -> WS10: 0x14D068 assembles the rider from the
 profile): the rider picked or changed in the lodge (Equip Gear, Ubertrick Setup) loads under 118loadoutlodge (web/career-ui.js) */
ui.cb.readyView = readyView;
// pv ws13Rebuild: the round's computer riders at a Next heat / results Restart (web/career-ui.js ws13Riders)
ui.cb.heatLineup = () =>
  aiRace && (mpGame?.aiAllowed() ?? true)
    ? aiRace.prepare({ rider: selectedRider, event: eventOf() }).catch((e) => console.warn('Computer-rider lineup failed', e))
    : null;
ui.cb.refreshRider = () => (pendingRider || riderStale() ? ensureRider().catch((e) => console.warn('Rider refresh failed', e)) : null);
// career round for the PA lines / podium (web/game-audio.js)
// audio menus + UI sounds (web/audio-menu.js) use the audio API
gameAudio.context = () => {
  const ev = ui.careerMode ? ui.careerUI?.career?.active?.ev : null;
  // pv djVisited: the rider's saved visited mask P+0xACC (145D38 -> audio+0x579C) and the Peak 2 lock P+0x278 bit 12 (146008)
  const cu = pv('djVisited') && ui.careerMode ? ui.careerUI : null,
    me = cu?.career && cu.riderId != null ? cu.career.save?.riders?.[cu.riderId] : null;
  return {
    career: !!ev?.career,
    round: ev?.career ? (ev.round ?? 1) : 3,
    mode: ev?.mode ?? null,
    ...(me ? { visited: cu.visitedMask(me), peak2Locked: !me.peaks?.[1] } : {})
  };
};
// Stage/projection for the original Widescreen mode (widescreen.js): 4:3 stage (16:9 = letterboxed 3D band) or 16:9 Anamorphic. Course
// select (ui 'event' screen / career flow): another location reloads the page into it, keeping the rider and the start request (all course
// assets are loaded once by init). Returns false while navigating.

// pv nisTick (docs/ctm-parity.md "The NIS rider hold"): world state 14's rider actor (123640) holds the rider at the cut's anchor (19
// NIS_Lodge / 28 NIS_Transport, ground-snapped, facing its X axis) in control 13 / motion 3 while the world runs; every placement releases
// it first (123B48 before 11D390: resetPhysics)
let nisHeld = false,
  nisStation = -1,
  nisTransport = false;
// pv worldUnderCuts (docs/ctm-events-in-world.md stage 1): the rider held by a CTM cut's rider actor while the world ticks (WS1 fly-over,
// WS10 arrivals); nisCutAt: the actor root waiting for the rider's first posed tick
let nisCut = false,
  nisCutAt = null;
function nisCutHold(r) {
  if (!core?._nis_hold) return;
  if (!nisPosed()) {
    nisCutAt = r ?? true;
    return;
  }
  nisCutAt = null;
  if (r) core._nis_hold(1, r.pos[0], r.pos[1], r.pos[2], Math.cos(r.yaw), Math.sin(r.yaw));
  else {
    const P = new Float32Array(core.HEAPF32.buffer, core._rider_world_state(), 3),
      [qx, qy, qz, qw] = new Float32Array(core.HEAPF32.buffer, core._rider_orientation(), 4),
      fx = 2 * (qx * qy - qw * qz),
      fy = 1 - 2 * (qx * qx + qz * qz);
    core._nis_hold(1, P[0], P[1], P[2], fx || 1, fy);
  }
}
function nisCutEnd() {
  if (!nisCut) return;
  nisCut = false;
  nisCutAt = null;
  core?._nis_hold?.(0, 0, 0, 0, 0, 0);
}
// the hold skips the pose (121700 / 121728): only once a tick has posed the rider (game-tick.js reads that frame for the camera head)
function nisPosed() {
  return !!core?._pose_physical && new Float32Array(core.HEAPF32.buffer, core._pose_physical(), 12)[7] > 0;
}
function nisHoldAt(station) {
  if (!core?._nis_hold || !nisPosed()) return false;
  const L = COURSE_CODES[station.course];
  // the actor's first tick: the anchor frame x key 0 (0, 0, -15) of lodge_arr3 #148 / hub_trans_arr #166 (cutscenes.js actorStart); the
  // bare anchor until that script copy is in
  const a = cutscenes?.actorStart?.(station.action === 4 ? 148 : 166, L) || cutscenes?.anchorOf?.(station.action === 4 ? 19 : 28, L);
  if (!a) return false;
  // pv nisAfterScan: the rider actor's start is the next tick's NIS tick (0x230BE4, before the rider manager and its section pass;
  // gameHost.nisStart), not this tick's 121818
  if (pv('nisAfterScan')) nisStartPending = a;
  else core._nis_hold(1, a.pos[0], a.pos[1], a.pos[2], Math.cos(a.yaw), Math.sin(a.yaw));
  nisHeld = true;
  nisStation = station.course;
  return true;
}
let nisStartPending = null;
function nisStart() {
  const a = nisStartPending;
  if (a) {
    nisStartPending = null;
    if (nisHeld && core?._nis_hold) core._nis_hold(1, a.pos[0], a.pos[1], a.pos[2], Math.cos(a.yaw), Math.sin(a.yaw));
  }
  // pv nisBoneProbe: 120F20's 1242B0 from the human actor's posed board root (+0xAFC holds: cutscenes.js humanBoard)
  if (pv('nisBoneProbe') && (nisHeld || nisCut || nisTransport) && core?._nis_hold_probe) {
    const hb = cutscenes?.humanBoard?.();
    if (hb?.afc) core._nis_hold_probe(2, hb.pos[0], hb.pos[1], hb.pos[2]);
  }
}
function nisRelease() {
  nisStartPending = null;
  if (!nisHeld) return;
  nisHeld = false;
  core?._nis_hold?.(0, 0, 0, 0, 0, 0);
}
// The door / booth hold exists only under its cut and the prompt / map (123640 .. 123B48). The rider still held in the plain ride (no cut,
// not paused) for 30 frames is placed at the station as the release does: a path that missed it froze the ride until the next unpause (diag
// t93ez0j6 86.9 s: No at the door, then the frozen ride until the MCOMM's resume)
function nisResume() {
  if (!nisHeld || cutscenes?.active) return;
  const s = course?.freeRide && freeRide?.spawnFor(nisStation);
  if (s) {
    spawn = s;
    resetPhysics(true);
    freeRide.afterReset();
  } else nisRelease();
}
let nisStrayFrames = 0;
function nisWatch() {
  if (nisCut && nisCutAt !== null && nisPosed()) nisCutHold(nisCutAt === true ? null : nisCutAt);
  if (!nisHeld || isPaused() || cutscenes?.active || ui.screen !== 'game' || transporting) {
    nisStrayFrames = 0;
    return;
  }
  if (++nisStrayFrames < 30) return;
  nisStrayFrames = 0;
  console.warn('NIS hold outside its cut: released at the station');
  try {
    nisResume();
  } catch (e) {
    console.error('NIS release failed', e);
    nisRelease();
  }
}

// Conquer the Mountain free ride (career-ui.js goWorld): reload into the streamed Peak 1 world at a course's arrival
let transporting = false;
// the in-world Transport runs (WS14 -> WS11 -> WS10): Start opens no pause (startOpensPause gets it as a cut). PS2
// local/transport-stall/ps2/run1 (frprompt Yes, Start at samples 80 / 200 / 350 / 555 in WS14 / WS11 / WS11 / WS10): the tick runs on, no
// MCOMM; Start in WS4 (649) pauses. So a second Transport can never be asked for while one runs
async function transportInWorld(dest) {
  if (transporting) return;
  transporting = true;
  const cu = ui.careerUI;
  // pv nisTick (docs/ctm-parity.md "The Transport's ride"): the world runs through the ride (PS2 ctm-parity/transport to-c: ticks 921 ->
  // 1471 without a stop), the rider held by each step's actor (cutscenes.js onHumanActor) from the start until the arrival's placement
  const held = pv('nisTick') && !!core?._nis_hold && !!course?.freeRide && nisPosed();
  let ws11Hold = null;
  // no NIS hold: the world stops through the ride (port)
  try {
    if (held) {
      overlay.close();
      // a Transport picked from the MCOMM: its pause context pops at the Yes and the world ticks through the ride (PS2 to-c-x 921 -> 1471);
      // paused, the streamer never ran and the ride waited out its 2-minute cap
      nisTransport = true;
      const P = new Float32Array(core.HEAPF32.buffer, core._rider_world_state(), 3),
        [qx, qy, qz, qw] = new Float32Array(core.HEAPF32.buffer, core._rider_orientation(), 4),
        fx = 2 * (qx * qy - qw * qz),
        fy = 1 - 2 * (qx * qx + qz * qz);
      core._nis_hold(1, P[0], P[1], P[2], fx || 1, fy);
    } else {
      ws11Hold = contexts.push(CTX.HOLD, { owner: 'WS11 transport (no NIS hold)' });
      overlay.close();
    }
    ui.set('game');
    gameAudio.travel?.(dest);
    // 27A860 -> 28E8C0(20)
    // the ride's cuts (departure, in-air loop until the rows are in, station arrival) are played by free-ride.js through web/cutscenes.js
    let arrivedSent = false;
    // pv transportFade: world state 10 enter at the release (R+1): 2B3A98 music resume, pktrans stop
    const s = await freeRide.transport(dest, {
      ticking: held,
      rider: selectedRider?.id,
      onRelease: pv('transportFade')
        ? () => {
            arrivedSent = true;
            gameAudio.arrived?.();
          }
        : null,
      // another peak's unvisited backcountry, reached inside the one world: its first arrival
      firstVisit: !!cu?.freeRide && dest >= 14 && dest <= 16 && !(cu.visitedMask?.() & (1 << dest))
    });
    ui.set('game');
    if (!arrivedSent) gameAudio.arrived?.();
    if (cu?.freeRide) cu.freeRide.course = dest;
    nisTransport = false;
    if (s) {
      spawn = s;
      resetPhysics(true);
      freeRide.afterReset();
    }
    // pv eventReturnInWorld: WS15 (236058) places the race's riders on the human's row too (PS2 c0d-ws15: all six at Session point 1,
    // riding in rider pairs until the WS4 restart at record 8)
    if (worldAiReturn && !worldAiReturn.placed) {
      const r = worldAiReturn;
      if (s?.region && s.sameLocation) {
        r.placed = true;
        aiRace = r.race;
        aiRace.resume({ place: (c) => c._place_rider_region(...s.region.position, ...s.region.direction, 0) });
        aiActive = true;
      } else worldAiEnd();
    }
    if (s) {
    } else if (held) core._nis_hold(0, 0, 0, 0, 0, 0);
    if (!held) {
      contexts.pop(ws11Hold);
      clearInput();
    }
    // a course arrival fades in from black over 30 ticks under the HUD (PS2 aba1 arrival: the fade's first frames reset the
    // painters, clear at +30)
    // a Transport to the current location is world state 15 (236058): Session point 1, then the fade 2E4370(mgr, 1, 0,
    // white, 0, 0, 1.0): white fading out over 1.0 s of the 60 Hz timer (2E47E8: 1 - t / 1.0), the HUD over it (PS2 stations-ws15)
    if (s?.sameLocation) cutscenes?.fadeFrom?.({ ticks: 60, colour: 'white', hud: true });
    else if (s && (dest < 14 || dest >= 17))
      // pv transportFade: the list stop's slide-out 2EA780, the HUD inside the shrinking picture (ui.js hudSqueeze)
      cutscenes?.fadeFrom?.({ ticks: 30, colour: 'black', hud: true, bars: pv('transportFade') });
  } catch (e) {
    console.error('Transport failed', e);
    if (nisTransport) {
      nisTransport = false;
      core?._nis_hold?.(0, 0, 0, 0, 0, 0);
    }
    overlay.close();
  } finally {
    transporting = false;
    contexts.pop(ws11Hold);
  }
}
function riderParams(url) {
  url.searchParams.set('rider', selectedRider.id);
  if (selectedRider.base) url.searchParams.set('base', selectedRider.base);
  else url.searchParams.delete('base');
  return url;
}
/* Course / world changes load in the running page (loadCourse, unloadCourse below) and push a history entry with the query a
   reload would use (deep links, reload-from-URL). The callbacks keep their contract: false = the page is changing course (the
   switch ends like a load with ?autostart=1: careerUI.resume() or the event load); ui.courseReady resolves when it is done. */
function selectFreeRide(peakCourse, reload = false) {
  const url = new URL(location.href),
    q = url.searchParams,
    // the course's peak world (docs/peak3.md), or the whole mountain
    world = freeRideWorldOf(peakCourse);
  if (
    !reload &&
    live &&
    q.get('course') === world &&
    !q.has('peakMode') &&
    Number(q.get('peakCourse') ?? PEAK_DEFAULT_STATION[world]) === peakCourse &&
    freeRide
  )
    return true;
  q.set('course', world);
  q.delete('peakMode');
  q.set('peakCourse', String(peakCourse));
  riderParams(url);
  q.set('autostart', '1');
  navigateCourse(url);
  return false;
}
function selectPeakRun(mode) {
  const url = new URL(location.href),
    q = url.searchParams,
    world = peakRunWorld(mode);
  if (live && q.get('course') === world && Number(q.get('peakMode')) === mode && freeRide) return true;
  q.set('course', world);
  q.delete('peakCourse');
  q.set('peakMode', String(mode));
  riderParams(url);
  q.set('autostart', '1');
  navigateCourse(url);
  return false;
}
function selectCourse(c) {
  if (!c) return true;
  if (c.code === course.code && !course.freeRide) {
    if (live) return true;
    // the lazy first course: the event load waits for it
    if (lazyPending() && (!lazyBoot.started || backgroundJob?.key === c.code)) return courseLive().then(() => live);
    if (switchBusy) return false;
  }
  const url = new URL(location.href);
  url.searchParams.set('course', c.code);
  url.searchParams.delete('peakCourse');
  url.searchParams.delete('peakMode');
  riderParams(url);
  url.searchParams.set('autostart', '1');
  navigateCourse(url);
  return false;
}
function layoutStage() {
  const view = widescreenView(ui.widescreen, 0),
    stage = $('#stage');
  stage.dataset.widescreen = view.mode;
  if (!camera || !renderer) return;
  camera.aspect = view.cameraAspect;
  camera.updateProjectionMatrix();
  // render scale: web/quality.js
  sizeRenderer(renderer, stage.clientWidth, Math.round(stage.clientHeight * view.band[1]), view.band[1]);
  sunFlare?.setBand(view.band);
  lightGlow?.setBand(view.band);
}
let worldScene,
  setPieceRenderer = null;
const pickupMeshes = [],
  eventDeadMeshes = [],
  movingMeshes = [];
async function asset(name, isRider = false, root = '/assets/' + name + '/', opts = null) {
  // opts.pause(): awaited between slices (streamed Peak 1 locations build on idle frames, web/free-ride.js)
  let sliceAt = performance.now();
  const slice = async () => {
    if (opts?.pause && performance.now() - sliceAt > 4) {
      await opts.pause();
      sliceAt = performance.now();
    }
  };
  const d = await load(root + 'world.json');
  // the texture archive (world library) downloads alongside the vertex data
  for (const [k, t] of Object.entries(d.textures))
    if (t.pack !== undefined && !k.startsWith('10-')) prefetchTextureArchive(textureSource(root, t).archive);
  const [vb, ib, cb] = await Promise.all(['vertices.bin', 'indices.bin', 'colors.bin'].map((f) => load(root + f, 'buffer')));
  const inter = new T.InterleavedBuffer(new Float32Array(vb), 10),
    indices = new Uint32Array(ib),
    colors = new T.BufferAttribute(new Float32Array(cb), 4);
  // the package's batches draw from one index buffer (their draw ranges), except the event terrain's (terrain refinement
  // edits its copies)
  const sharedIndex = !isRider ? Object.assign(new T.BufferAttribute(indices, 1), { ssxShared: true }) : null;
  const originalWorld = new URL(location.href).searchParams.get('originalWorld') !== '0',
    originalKind = !originalWorld
      ? null
      : name === course.code || /^\/assets\/PEAK\d\//.test(root)
        ? 'world'
        : name === 'SKY'
          ? 'sky'
          : null;
  // textures: PNG files or texture archives (the shared world texture library, web/texture-archive.js); the GameCube lightmaps (10-) are
  // only sampled by the fallback materials below, not by the original world/sky materials
  const textures = {};
  const texEntries = Object.entries(d.textures).filter(([k]) => !(originalKind && k.startsWith('10-')));
  const texSetup = (k, t, tex) => {
    tex.flipY = false;
    tex.wrapS = tex.wrapT = T.RepeatWrapping;
    tex.colorSpace = k.startsWith('9-') ? frameTextureSpace : T.NoColorSpace;
    tex.anisotropy = 4;
    tex.userData.texelDomain = t.texel_domain;
    // rider PS2 texels (web/rider-material.js)
    if (k.startsWith('10-')) {
      tex.channel = 1;
      tex.wrapS = tex.wrapT = T.ClampToEdgeWrapping;
    }
    textures[k] = tex;
  };
  // eventSlices, a paused build (opts.pause): the PNGs download together and decode one per slice (a decode is ~10-40 ms at 4x CPU; all
  // together were one ~400 ms task)
  if (opts?.pause) {
    const blobs = texEntries.map(([, t]) => packageTextureBlob(root, t));
    for (let n = 0; n < texEntries.length; n++) {
      const [k, t] = texEntries[n];
      texSetup(k, t, await pngTexture(loader, await blobs[n]));
      await slice();
    }
  } else await Promise.all(texEntries.map(async ([k, t]) => texSetup(k, t, await packageTexture(loader, root, t))));
  const world =
    originalKind === 'world'
      ? await createOriginalWorldMaterials({ root, textures, vertices: inter.array, vertexCount: inter.count, loader, load })
      : originalKind === 'sky'
        ? createOriginalSkyMaterials({ textures })
        : null;
  const group = new T.Group();
  group.userData.courseHash = d.source_sha256;
  // event course world: its shared vertex data (interleaved position/normal/uv/uv1, colours, PS2 light UVs, vertex alpha; ~35 MB on ARA1)
  // is only read by the GPU upload: main.js warmupRender drops the JS copies once every batch has drawn. Index arrays stay
  // (terrain-overlays.js edits them); bounding volumes are precomputed below.
  if (name === course.code && !course.freeRide)
    group.userData.releaseGeometryArrays = () => {
      const shared = new Set([inter]);
      group.traverse((o) => {
        const g = o.geometry;
        if (!g || g.attributes.position?.data !== inter) return;
        for (const k of ['color', 'ps2LightUv', 'ps2VertexAlpha']) {
          const a = g.attributes[k];
          if (a && !a.isInterleavedBufferAttribute && a.usage === T.StaticDrawUsage) shared.add(a);
        }
      });
      // gpuRestore (web/gpu-copies.js): a new GPU device (web/gpu-recovery.js) uploads again from these arrays, so they are read again
      // first
      {
        const light = [],
          alpha = [];
        let colour = false;
        group.traverse((o) => {
          const g = o.geometry;
          if (!g || g.attributes.position?.data !== inter) return;
          const c = g.attributes.color,
            l = g.attributes.ps2LightUv,
            a = g.attributes.ps2VertexAlpha;
          if (c === colors && shared.has(c)) colour = true;
          if (l && shared.has(l) && !light.includes(l)) light.push(l);
          if (a && shared.has(a) && !alpha.includes(a)) alpha.push(a);
        });
        registerGpuRestore(group, async () => {
          const [vb, cb, ab] = await Promise.all(['vertices.bin', 'colors.bin', 'vertex-alpha.bin'].map((f) => fetchBuffer(root + f)));
          if (vb.byteLength !== inter.count * 40) throw Error(root + 'vertices.bin changed');
          inter.array = new Float32Array(vb);
          if (colour) colors.array = new Float32Array(cb);
          for (const a of alpha) a.array = Float32Array.from(new Uint8Array(ab), (v) => v / 128);
          if (light.length) {
            const uv = world.lightUvFrom(inter.array);
            for (const a of light) a.array = uv;
          }
        });
      }
      for (const a of shared) a.array = new a.array.constructor(0);
      return shared.size;
    };
  group.userData.worldTextures = textures;
  group.userData.worldAtlas = world?.atlas;
  // released with a streamed Peak 1 location (web/free-ride.js)
  // web/crowd-2d.js animates the crowd texture
  let skinI, skinW;
  if (isRider) {
    const sourceVertices = inter.array.slice();
    const authoredScale = animationResources.settings.original_animation.scale;
    if (authoredScale.length !== 3 || !authoredScale.every((x) => Number.isFinite(x) && x > 0 && x === authoredScale[0]))
      throw Error('Unsupported rider geometry scale');
    group.userData.riderScale = authoredScale[0];
    for (let i = 0; i < inter.array.length; i += 10) for (let k = 0; k < 3; k++) inter.array[i + k] *= authoredScale[0];
    rig = await load(root + 'rider.json');
    group.userData.sourceSkin = createOriginalRiderSkinning(rig, sourceVertices, origin);
    if (group.userData.sourceSkin)
      group.userData.sourceLighting = createRiderLightingMaterial(
        group.userData.sourceSkin,
        colors.array.some((v, i) => i % 4 < 3 && v !== 1)
      );
    clips = (
      await load(wardrobeFile(root + 'animation-samples.json') !== undefined ? root + 'animation-samples.json' : riderSamplesUrl(root))
    ).clips;
    // one clip table for the original riders
    bones = rig.bones.map((b) => {
      const o = new T.Bone();
      o.name = b.name;
      o.position.fromArray(b.translation).multiplyScalar(group.userData.riderScale);
      o.quaternion.fromArray(b.rotation).normalize();
      return o;
    });
    rig.bones.forEach((b, i) => {
      if (b.parent >= 0) bones[b.parent].add(bones[i]);
      else group.add(bones[i]);
    });
    skinI = new Uint16Array(rig.skin.length * 4);
    skinW = new Float32Array(rig.skin.length * 4);
    rig.skin.forEach((weights, i) =>
      weights.slice(0, 4).forEach(([j, w], k) => {
        skinI[i * 4 + k] = j;
        skinW[i * 4 + k] = w;
      })
    );
  }
  // web/rider-shadow.js: original part order
  if (isRider && group.userData.sourceSkin)
    group.userData.shadowRider = {
      skin: group.userData.sourceSkin,
      batches: d.batches.slice(),
      indices: indices.slice(),
      core: () => core
    };
  if (isRider) {
    const by = new Map();
    for (const b of d.batches) {
      const key = b.texture + ':' + b.lightmap;
      if (!by.has(key)) by.set(key, { ...b, ranges: [] });
      by.get(key).ranges.push(indices.slice(b.first_index, b.first_index + b.index_count));
    }
    let cursor = 0;
    const merged = [];
    const copy = indices.slice();
    for (const b of by.values()) {
      let n = 0;
      for (const r of b.ranges) {
        indices.set(r, cursor + n);
        n += r.length;
      }
      merged.push({ ...b, first_index: cursor, index_count: n });
      cursor += n;
    }
    d.batches = merged;
  }
  const materials = new Map();
  const bootWorld = name === course.code && !isRider ? globalThis.ssxBoot : null;
  // the title's load meter: the course world's build
  let batchN = 0;
  for (const b of d.batches) {
    await slice();
    bootWorld?.step('world', (0.8 * batchN++) / d.batches.length);
    const key = b.texture + ':' + b.lightmap;
    let mat = materials.get(key);
    if (!mat && world) mat = world.material(b);
    if (!mat) {
      const skin = !!group.userData.sourceSkin,
        base = textures['9-' + b.texture];
      // ?originalWorld=0 fallback (non-rider): three's own linear-light combine, written through web/frame-space.js (linearToFrame,
      // sRGB-decoded texels)
      let map = base;
      if (!skin && base) {
        map = linearMaps.get(base);
        if (!map) {
          map = base.clone();
          map.colorSpace = linearTextureSpace;
          map.needsUpdate = true;
          linearMaps.set(base, map);
        }
      }
      mat = new T.MeshBasicNodeMaterial({
        map,
        lightMap: textures['10-' + b.lightmap] || null,
        lightMapIntensity: 1.75,
        vertexColors: true,
        side: T.DoubleSide,
        transparent: name === 'SKY' && !!b.has_alpha,
        alphaTest: b.instance ? 0.35 : 0
      });
      if (!skin) mat.outputNode = linearOutput();
      if (group.userData.sourceSkin) mat.vertexNode = group.userData.sourceSkin.vertexNode;
      group.userData.sourceLighting?.attach(mat);
      // the PS2 rider draw state per material (web/rider-material.js): 'alph' / 'ea*' blended in two passes, the rest opaque
      if (isRider) mat = riderDrawState(mat, b.material);
      materials.set(key, mat);
    }
    const g = new T.BufferGeometry();
    g.setAttribute('position', new T.InterleavedBufferAttribute(inter, 3, 0));
    g.setAttribute('normal', new T.InterleavedBufferAttribute(inter, 3, 3));
    g.setAttribute('uv', new T.InterleavedBufferAttribute(inter, 2, 6));
    g.setAttribute('uv1', new T.InterleavedBufferAttribute(inter, 2, 8));
    g.setAttribute('color', colors);
    world?.prepareGeometry(g);
    if (sharedIndex && !(name === course.code && !b.instance)) {
      g.setIndex(sharedIndex);
      g.setDrawRange(b.first_index, b.index_count);
      if (Array.isArray(mat)) mat.forEach((_, i) => g.addGroup(b.first_index, b.index_count, i));
    } else {
      g.setIndex(new T.BufferAttribute(indices.slice(b.first_index, b.first_index + b.index_count), 1));
      if (Array.isArray(mat)) mat.forEach((_, i) => g.addGroup(0, b.index_count, i));
    }
    let mesh;
    if (isRider) {
      group.userData.sourceSkin?.attach(g);
      g.setAttribute('skinIndex', new T.BufferAttribute(skinI, 4));
      g.setAttribute('skinWeight', new T.BufferAttribute(skinW, 4));
      mesh = new T.SkinnedMesh(g, mat);
      mesh.frustumCulled = false;
    } else {
      mesh = new T.Mesh(g, mat);
      mesh.userData.terrainBase = name === course.code && !b.instance;
      const box = new T.Box3();
      for (let i = b.first_index; i < b.first_index + b.index_count; i++) {
        const n = indices[i] * 10;
        box.expandByPoint(v.fromArray(inter.array, n));
      }
      g.boundingBox = box;
      g.boundingSphere = box.getBoundingSphere(new T.Sphere());
      mesh.position.copy(origin).negate();
      mesh.userData.indexFirst = b.first_index;
      // the batch's place in indices.bin (web/gpu-copies.js restore)
      // the depth-sorted static-model classes after class 1 (render queue key 364240)
      if (b.instance && (b.blend === 2 || b.blend === 3)) mesh.renderOrder = SORTED_CLASS_ORDER;
    }
    if (b.pickup_resource !== undefined) {
      mesh.userData.pickupResource = b.pickup_resource;
      pickupMeshes.push(mesh);
    }
    if (b.event_dead_resource !== undefined) {
      mesh.userData.eventDeadResource = b.event_dead_resource;
      eventDeadMeshes.push(mesh);
    }
    if (b.moving_resource !== undefined) {
      mesh.userData.movingResource = b.moving_resource;
      movingMeshes.push(mesh);
      // a streamed location's (web/free-ride.js release dispatches dispose): out of the list with its package
      mesh.addEventListener('dispose', () => {
        const k = movingMeshes.indexOf(mesh);
        if (k >= 0) movingMeshes.splice(k, 1);
      });
    }
    // LiveComp players (set-pieces-renderer.js)
    if (b.livecomp_resource !== undefined) {
      mesh.userData.liveComp = [b.livecomp_resource, b.livecomp_node];
      (group.userData.liveCompMeshes ??= []).push(mesh);
    }
    // stage-program instance state (set-pieces-renderer.js)
    if (b.script_resource !== undefined) mesh.userData.scriptResource = b.script_resource;
    // texture chunk streaming 0x22A5A0 (set-pieces-renderer.js)
    if (b.chunk !== undefined) {
      mesh.userData.chunk = b.chunk;
      (group.userData.chunkMeshes ??= []).push(mesh);
    }
    /* runtime flag bit0 clear: trigger/reset volumes and placeholders the original never draws (prepare.py) */
    // flags redraw theirs (set-pieces-renderer.js)
    if (b.hidden_resource === undefined) group.add(mesh);
    else {
      mesh.userData.hiddenResource = b.hidden_resource;
      mesh.userData.batch = b;
      (group.userData.hiddenMeshes ??= []).push(mesh);
    }
  }

  // gpuRelease (web/gpu-copies.js): the CPU copies of a world package the GPU holds are dropped by its owner once drawn or compiled
  // (free-ride.js a streamed location after its warm-up; warmupRender the event world's textures), each restorable for a new GPU device.
  // World texture 9-161 stays: the crowd animator swaps its image (web/crowd-2d.js).
  if (originalKind === 'world') {
    const light = [],
      alpha = [],
      entries = new Map();
    group.traverse((o) => {
      const g = o.geometry;
      if (!g || g.attributes.position?.data !== inter) return;
      const l = g.attributes.ps2LightUv,
        a = g.attributes.ps2VertexAlpha;
      if (l && !light.includes(l)) light.push(l);
      if (a && !alpha.includes(a)) alpha.push(a);
    });
    for (const [k, t] of Object.entries(d.textures)) {
      const tex = textures[k];
      if (tex?.source && k !== '9-161') entries.set(tex.source, t);
    }
    group.userData.releaseGpuCopies = ({ geometry = true } = {}) =>
      releaseWorldCopies({
        group,
        backend: renderer.backend,
        root,
        inter: geometry ? inter : null,
        colors: geometry ? colors : null,
        light: geometry ? light : [],
        alpha: geometry ? alpha : [],
        lightUvFrom: world.lightUvFrom,
        textureEntries: entries,
        indices: geometry
      });
  }
  if (isRider) {
    group.updateMatrixWorld(true);
    const inverseBind = originalRiderBoneInverses(rig, group.userData.riderScale);
    const skeleton = inverseBind ? new T.Skeleton(bones, inverseBind) : new T.Skeleton(bones);
    group.children.filter((o) => o.isSkinnedMesh).forEach((m) => m.bind(skeleton, m.matrixWorld));
  }
  // World batches keep their authored placement. Cache local matrices while
  // retaining normal world-matrix propagation for parent changes and animation.
  if (!isRider) {
    for (const mesh of [...group.children, ...(group.userData.hiddenMeshes ?? [])]) {
      mesh.updateMatrix();
      mesh.matrixAutoUpdate = false;
    }
    if (name !== 'SKY') {
      group.updateMatrix();
      group.matrixAutoUpdate = false;
    }
  }
  return group;
}
/* the human's init_animation / init_race documents parsed ahead, one a load-screen frame (core animation_prepare: the call
   then takes the same parse; a changed or unprepared document is parsed in the call as before) */
async function prepareRiderAnimation() {
  if (!core?._animation_prepare) return;
  // a frame once a frame's work is done
  const sliceFrame = framePause('frame');
  for (const value of [animationResources.metadata, rig, animationResources.settings, animationResources.settings]) {
    await sliceFrame();
    const b = new TextEncoder().encode(JSON.stringify(value) + '\0'),
      p = core._malloc(b.length);
    core.HEAPU8.set(b, p);
    try {
      core._animation_prepare(p);
    } finally {
      core._free(p);
    }
  }
  await sliceFrame();
}
function initializeRiderAnimation() {
  const pointers = [];
  const put = (bytes) => {
    const p = core._malloc(bytes.length);
    pointers.push(p);
    core.HEAPU8.set(bytes, p);
    return p;
  };
  const json = (value) => put(new TextEncoder().encode(JSON.stringify(value) + '\0'));
  try {
    const settings = json(animationResources.settings),
      packets = put(new Uint8Array(animationResources.packets));
    core._init_animation(json(animationResources.metadata), json(rig), settings, packets, animationResources.packets.byteLength);
    core._init_race(settings);
    core._animation_use_physics(1);
    resetPhysics();
    const p = core._animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
    animPose = new Float32Array(core.HEAPF32.buffer, p, bones.length * 7).slice();
    animInfo = new Float32Array(core.HEAPF32.buffer, core._animation_info(), 19).slice();
    posePhysicalFrame = new Float32Array(core.HEAPF32.buffer, core._pose_physical(), 12).slice();
    currentRiderFrame = previousRiderFrame = null;
    cameraPose = previousCameraPose = null;
  } finally {
    for (const p of pointers) core._free(p);
  }
}
function disposeRider(group) {
  group.userData.sourceSkin?.dispose();
  const geometries = new Set(),
    materials = new Set(),
    textures = new Set();
  group.traverse((o) => {
    if (o.isMesh) {
      geometries.add(o.geometry);
      for (const m of Array.isArray(o.material) ? o.material : [o.material]) {
        materials.add(m);
        if (m.map) textures.add(m.map);
        if (m.lightMap) textures.add(m.lightMap);
        if (m.userData.sourceTexture) textures.add(m.userData.sourceTexture);
      }
    }
  });
  for (const g of geometries) g.dispose();
  for (const m of materials) m.dispose();
  for (const t of textures) t.dispose();
}
/* Picking a rider (Select Character cycling, cheat list, Equip Gear, ?rider=) only records the choice; the race model, its settings and the
 animation core load when a race needs them (ensureRider: the event load's warm-up, ssxQA, startRun), so a switch never stalls the front end
 (the menus draw web/fe-preview.js, prepared off the main thread). */
let loadedRider = selectedRider,
  pendingRider = null;
const sameRider = (a, b) => !!a && !!b && a.id === b.id && a.base === b.base && a.outfit === b.outfit && (a.uber ?? '') === (b.uber ?? '');
async function selectRider(rider) {
  if (!rider?.package) throw Error('Unknown rider');
  const entry = rider;
  rider = await outfitRider(ui, rider);
  rider = withProfile(ui, entry, rider);
  selectedRider = rider;
  pendingRider = sameRider(rider, loadedRider) ? null : rider;
}
// careerRider (web/career-rider.js): the entry is resolved again when what it came from changed (play mode, gear record, uber selection)
/* the rival challenges' objectives card lies over the ready state, as on the PS2 (WS2 at race tick 0: the riders at
 their start spots, the placement camera; local/reference/pcsx2/{happiness-mac,ruthless,the-throne}-ready). readyView places the
 run as startRun does and poses it without a tick (an animation step of 0, one camera step); the game RNG words are put back and
 the card's Continue starts the run from scratch (startRun resets the rider, the camera 0x11D390 set-targets and the race). */
let readyShown = false,
  readyAi = false;
const readyScreen = () => readyShown && ui.screen === 'ctm-objectives';
function readyView() {
  {
    // pv ws13Rebuild: a peak run's streamed world (no event start) gets its card's ready view too, without _start_event (as startRun)
    const skip = !live
      ? 'live'
      : running
        ? 'running'
        : !core
          ? 'core'
          : !animationReady
            ? 'animationReady'
            : course.eventStart === false && !pv('ws13Rebuild')
              ? 'eventStart'
              : null;
    // QA: which guard skipped the card's ready view
    globalThis.__readyViewSkip = skip;
    if (skip) return;
  }
  readyAi = false;
  try {
    const W = (p) => new Uint32Array(core.HEAPU8.buffer, p, 6),
      rng = W(core._animation_rng_words()).slice(),
      vis = W(core._visual_rng_words()).slice(),
      lcgPtr = core._visual_lcg_word?.(),
      lcg = lcgPtr ? new Uint32Array(core.HEAPU8.buffer, lcgPtr, 1)[0] : null;
    core._reset_animation();
    core._reset_race();
    core._reset_pad_history();
    resetPhysics();
    freeRide?.afterReset();
    if (course.eventStart !== false) core._start_event();
    const f = (p, n) => new Float32Array(core.HEAPF32.buffer, p, n).slice(),
      pose = f(core._animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0), bones.length * 7),
      info = f(core._animation_info(), 19),
      phys = f(core._pose_physical(), 12);
    state = f(core._rider_state(), 16);
    const shown = state.slice();
    shown[8] = phys[8];

    // the card's camera: the chase camera settled behind the frozen riders (WS2 updates the camera while the race waits)
    let cam = f(core._step_camera_head(phys[9], phys[10], phys[11]), 9);
    /* the ready state's own camera, the event camera seed (core camera_event_seed_view: eye, look-at). WS2 does not
  step it (PS2 happiness-ready: the same eye over 900 card frames; the rival's board nose at the right edge). Else the chase settled. */
    const seedView = core._camera_event_seed_view?.();
    if (seedView) cam.set(new Float32Array(core.HEAPF32.buffer, seedView, 6));
    else
      for (let k = 0; k < 600; k++) {
        const next = f(core._step_camera_head(phys[9], phys[10], phys[11]), 9);
        const moved = Math.hypot(next[0] - cam[0], next[1] - cam[1], next[2] - cam[2]);
        cam = next;
        if (k > 30 && moved < 1e-5) break;
      }
    cameraPose = previousCameraPose = cam;
    currentRiderFrame = previousRiderFrame = captureRiderFrame(
      shown,
      pose,
      info,
      rig.bones.map((b) => b.parent),
      origin,
      phys.subarray(3, 7),
      phys.subarray(0, 3)
    );

    // the card's rider as WS2 draws it: its source skin in the ready pose, shaded with the environment irradiance settled at
    // the start spot (core environment_settle; PS2 the-throne-ready row 0 = 0.95 dark + 0.05 bright bank) and the card's camera, not the
    // unlit menu fallback (texels doubled) that stood before the first tick
    if (riderLightingUpdate && sam?.userData.sourceLighting) {
      humanSkinPending = true;
      humanSkinReset = true;
      flushHumanSkin();
      core._environment_settle?.(60);
      riderLightingUpdate.update();
      sam.userData.sourceLighting.capture(core);
    }

    // the rival at its start spot (ai-race.js readyPose; the lighting reads this camera)
    readyAi = course.eventStart !== false && !!aiRace && aiRace.enabled !== false && (mpGame?.aiAllowed() ?? true) && aiRace.readyPose();
    W(core._animation_rng_words()).set(rng);
    W(core._visual_rng_words()).set(vis);
    if (lcgPtr) new Uint32Array(core.HEAPU8.buffer, lcgPtr, 1)[0] = lcg;
    readyShown = true;
  } catch (e) {
    console.warn('Ready view unavailable', e);
    readyShown = false;
    cameraPose = previousCameraPose = null;
    currentRiderFrame = previousRiderFrame = null;
  }
}
const riderStale = () =>
  // pv onlineRecords: a downloaded run's rider is the uploader's
  !!selectedRider?.entry && !selectedRider.replayFixed && selectedRider.stamp !== riderStamp(ui, selectedRider.entry);
/* the career block of the human: a cheat skin plays on its base rider's (setup slot +0x11, 145C38; lodgeCheats) */
const careerId = () => (selectedRider?.kind === 'cheat' ? selectedRider.base || 'zoe' : selectedRider?.id);
async function ensureRider() {
  if (riderStale()) await selectRider(selectedRider.entry);
  while (pendingRider) {
    const r = pendingRider;
    pendingRider = null;
    await loadRiderNow(r);
  }
}
async function loadRiderNow(rider) {
  if (sameRider(loadedRider, rider)) return;
  if (loadedRider && sameRider(loadedRider, { ...rider, uber: loadedRider.uber })) {
    // only the uber rows changed: new settings on the same model
    const previous = animationResources.settings;
    ready = false;
    ui.ready = false;
    ui.sync();
    clearInput();
    try {
      const character = await loadCharacter(rider);
      animationResources.settings = humanSettings(animationResources.baseSettings ?? animationResources.settings, character);
      applyHumanPairInputs(aiRace, character);
      await prepareRiderAnimation();
      initializeRiderAnimation();
      loadedRider = rider;
    } catch (error) {
      animationResources.settings = previous;
      initializeRiderAnimation();
      throw error;
    } finally {
      clearInput();
      last = performance.now();
      ready = true;
      ui.ready = true;
      ui.sync();
    }
    return;
  }
  ready = false;
  ui.ready = false;
  ui.sync();
  clearInput();
  const previous = { rig, bones, clips, model: sam, settings: animationResources.settings };
  let model;
  try {
    // per-character human settings (web/character-roster.js, tools/export_characters.py); Zoe/Sam = course initial.json
    const character = await loadCharacter(rider);
    animationResources.baseSettings ??= animationResources.settings;
    animationResources.settings = humanSettings(animationResources.baseSettings, character);
    applyHumanPairInputs(aiRace, character);
    model = await asset(rider.package, true, rider.root);
    clips = previous.clips;
    await prepareRiderAnimation();
    initializeRiderAnimation();
    model.traverse((o) => {
      if (o.isMesh) {
        o.renderOrder = 600;
        for (const m of Array.isArray(o.material) ? o.material : [o.material]) if (fogRenderer) m.fog = false;
      }
    });
    scene.remove(previous.model);
    sam = model;
    // shown by the frame's visibility pass
    sam.visible = false;
    scene.add(sam);
    loadedRider = rider;
    disposeRider(previous.model);
  } catch (error) {
    rig = previous.rig;
    bones = previous.bones;
    clips = previous.clips;
    animationResources.settings = previous.settings;
    applyHumanPairInputs(aiRace, null);
    if (model) disposeRider(model);
    initializeRiderAnimation();
    throw error;
  } finally {
    clearInput();
    last = performance.now();
    ready = true;
    ui.ready = true;
    ui.sync();
  }
}
function afb(a, b, c) {
  const sin = [],
    cos = [];
  for (const [i, n] of [a, b, c].entries()) {
    const q = ((n % 4) + 4) % 4,
      r = q - 2 * Math.floor(q * 0.5);
    sin[i] = r * (2 - r) * (Math.floor(q) & 2 ? -1 : 1);
    cos[i] = Math.sqrt(Math.max(0, 1 - sin[i] * sin[i])) * ((Math.floor(q) + 1) & 2 ? -1 : 1);
  }
  const raw = [cos[0], sin[0] * sin[1] * cos[2], sin[0] * cos[1], sin[0] * sin[1] * sin[2]];
  return new T.Quaternion(raw[0], raw[2], -raw[1], raw[3]).normalize();
}
function animateRider(name, dt) {
  const clip = clips.find((c) => c.name === name) || clips[0];
  if (activeClip !== name) {
    poseTime = 0;
    activeClip = name;
  }
  poseTime += dt;
  const n = clip.frame_count || clip.frames.length,
    t = (poseTime * clip.fps) % n,
    i = Math.floor(t),
    j = (i + 1) % n,
    blend = t - i;
  rig.bones.forEach((b, k) => {
    const stream = clip.parts?.[String(b.file)],
      packed = clip.streams?.[String(b.file)];
    if (!stream && !packed) return;
    const a = stream?.[i],
      c = stream?.[j],
      val = (x) =>
        packed
          ? allSamples[(packed.offset >> 2) + i * packed.channels + x] * (1 - blend) +
            allSamples[(packed.offset >> 2) + j * packed.channels + x] * blend
          : a[x] * (1 - blend) + c[x] * blend;
    const tc = b.animation_translation_channel,
      rc = b.animation_rotation_channel;
    if (tc >= 0)
      bones[k].position.lerp(
        v.set(val(tc) / 100, val(tc + 2) / 100, -val(tc + 1) / 100).multiplyScalar(sam.userData.riderScale),
        1 - Math.exp(-dt * 22)
      );
    if (rc >= 0) bones[k].quaternion.slerp(afb(val(rc), val(rc + 1), val(rc + 2)), 1 - Math.exp(-dt * 22));
  });
}
// the keyboard per tick of a multi-tick frame. The log holds this frame's key events (event timestamps); keyBase the keys
// held at the previous frame's end. A frame that runs several ticks (a hitch) gives its k-th tick the keys as of the previous frame +
// (k+1)/60 s, the PS2 pad ring's sample of that vblank (0x326B88), so a release during the stall lands on its tick; ticks at or past the
// frame time read the current keys. Simple-mode roles are rebuilt in time order from the frame's start. Gamepad and touch keep the frame's
// one sample (they cannot be read during a stall).
function clearInput() {
  keys.clear();
  touchControls.clear();
  keyLog.length = 0;
  keyBase = new Set();
}
const keyLog = [];
let keyBase = new Set();
function stallKeyInput(input, ms, dt, snap, skip) {
  const base = keyBase,
    log = keyLog.slice();
  keyBase = new Set(keys);
  keyLog.length = 0;
  if (!snap || skip || typeof input === 'function' || dt * 1000 <= 1000 / 60 + 0.5) return input;
  const start = ms - dt * 1000;
  let restored = false;
  return (k) => {
    if (!restored) {
      restored = true;
      keyboardPad.steering = new Map(snap[0]);
      keyboardPad.serial = snap[1];
    }
    const t = start + ((k + 1) * 1000) / 60;
    const state = t < ms - 0.5 ? new Set(base) : keys;
    if (state !== keys)
      for (const [et, code, down] of log)
        if (et <= t) {
          if (down) state.add(code);
          else state.delete(code);
        }
    return buildPad((c) => state.has(c) || touchControls.held(c), touchControls.pad(pollPads()), keyboardPad);
  };
}

// touch deck (web/touch-controls.js): PS2 pad controls -> the same keyboard codes / gamepad sticks as below
const touchControls = createTouchControls({
  ui,
  isRunning: () => running,
  pause: () => {
    if (
      running &&
      !isPaused() &&
      !finished &&
      !mpGame?.racing &&
      startOpensPause({ screen: ui.screen, running, paused: isPaused(), finished, cutscene: !!cutscenes?.active || transporting })
    )
      overlay.open();
  }
});
let keyScreen = 'title',
  keyPaused = false,
  frameScreen = 'title';
// startRules (web/start-rules.js): the keyboard's Start opens the pause from the ride only, as it stood before any menu handled the key;
// the menus take Escape (back) / Enter (accept) themselves
window.addEventListener('keydown', (e) => {
  if (e.defaultPrevented) return;
  if (['Space', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.code)) e.preventDefault();
  if (running && keyLog.length < 512) keyLog.push([e.timeStamp, e.code, 1]);
  keys.add(e.code);
  if (e.repeat) return;
  if (e.code === 'KeyR' && ready && ui.screen !== 'loading' && !mpGame?.racing && !ui.onlineMode) startRun();
  if (e.code === 'Backspace') e.preventDefault();
  if ((e.code === 'Escape' || e.code === 'Enter') && running && !e.ssxPadMenu) {
    if (startOpensPause({ screen: keyScreen, running, paused: keyPaused, finished, cutscene: !!cutscenes?.active || transporting }))
      overlay.open();
  }
});
window.addEventListener(
  'keydown',
  () => {
    keyScreen = ui.screen;
    keyPaused = isPaused();
  },
  true
);
window.addEventListener('keyup', (e) => {
  if (running && keyLog.length < 512) keyLog.push([e.timeStamp, e.code, 0]);
  keys.delete(e.code);
});
// ?perf=1 (profiling scripts): automation windows lose focus mid-run; keep racing (and the held keys) on blur/hide
const perfNoFocusPause = new URL(location.href).searchParams.get('perf') === '1';
window.addEventListener('blur', () => {
  if (perfNoFocusPause) return;
  clearInput();
  if (running) overlay.open();
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden && running && !perfNoFocusPause) overlay.open();
});
const keyboardPad = createKeyboardContext(loadKeyboardMode());
function setKeyboardMode(mode) {
  keyboardPad.mode = mode;
  keyboardPad.steering.clear();
  saveKeyboardMode(mode);
  return mode;
}

// pv padCarry: a frame the game does not tick still consumes the pad: cSSXApp_preUpdate's 0x321298 updates the one history the ride reads
// (core pad_history_sample), 60 a second, at most 4 a frame as the menus' own pad model
let padCarryAcc = 0;
function padCarryFeed(input, dt) {
  if (!pv('padCarry') || !core?._pad_history_sample || !padPtr) return;
  padCarryAcc = Math.min(padCarryAcc + dt, 4 / 60);
  while (padCarryAcc >= 1 / 60 - 1e-9) {
    padCarryAcc -= 1 / 60;
    core.HEAPF32.set(input, padPtr >> 2);
    core._pad_history_sample(padPtr);
  }
}
function inputs() {
  const down = (k) => keys.has(k) || touchControls.held(k);
  const real = pollPads();
  // the active pad of every slot (not only slot 0: Windows often puts the controller in slot 1+), in the standard layout (web/gamepad.js,
  // gamepad-map.js); it also tells the control hints (web/input-glyphs.js)
  return buildPad(down, touchControls.pad(real), keyboardPad);
}

// gamepad in the menus: the same keys the touch deck sends (web/gamepad-menus.js)
const padMenus = installPadMenus({
  screen: () => ui.screen,
  isRunning: () => running,
  // the UI frame counter (web/screen-phases.js)
  phases: ui.phases
});
function resetPhysics(keepTick = false) {
  nisRelease();
  // pv gameTickKeep: the game tick 1298C8 of a streamed world: 0 at a run's start (the world load), kept by an in-world placement (11D390
  // does not touch it); reset_rider reloads the seed's
  const tick = pv('gameTickKeep') && course?.freeRide && core._game_tick_restart ? (keepTick ? core._game_tick() : 0) : null;
  core._reset_rider(...spawn.position, spawn.heading);
  if (tick != null) core._game_tick_restart(tick);
  // a free-ride start that is not a region placement (the CTM plane drop, 11DF18 without a row) runs the rider FX reset
  // 111890 too (2DF3B0: kicker buildup FX+0x10 = 0; core rider_fx_reset); place_rider_region runs it itself
  if (freeRide && !(spawn.region && freeRide?.placeRegion && core._place_rider_region)) {
    core._set_fx_reset_kicker?.(1);
    core._rider_fx_reset?.();
  }
  const drop = !!spawn.drop;
  // the CTM plane drop as the PS2 has it (web/plane-drop.js, menus/fr/ctmstart f02700): the rider lies head first (+0x120;
  // its forward +0x1B0 68 degrees down; set_rider_velocity commits it), and the camera takes that tick's words: 10 ticks into the lock the
  // drop's set-target 0x176FE0 started from that forward, looking down the slope
  if (drop) new Uint32Array(core.HEAPU8.buffer, core._rider_orientation(), 4).set(PLANE_DROP.quaternion);
  if (spawn.velocity) core._set_rider_velocity(...spawn.velocity);
  if (drop && core._drop_air_seed && PLANE_DROP.animation) {
    const b = new TextEncoder().encode(PLANE_DROP.animation + '\0'),
      p = core._malloc(b.length);
    core.HEAPU8.set(b, p);
    core._drop_air_seed(p);
    core._free(p);
    // the air controller (control 5) with that tick's air clips and the placement's zeroed ground-control triplets (core
    // drop_air_seed), not a passive departure from the grounded seed with its 8.5 cm lift
  } else if (spawn.region && freeRide?.placeRegion && core._place_rider_region) freeRide.placeRegion(spawn);
  else if (spawn.forwardSpeed && freeRide) core._set_rider_velocity(...freeRide.forwardVelocity(spawn.forwardSpeed));
  // 11DF18 (web/free-ride.js spawnFor)
  if (drop && core._camera_seed_words) {
    const w = core._malloc(271 * 4),
      k = core._malloc(271);
    new Uint32Array(core.HEAPU8.buffer, w, 271).set(PLANE_DROP.camera);
    core.HEAPU8.fill(1, k, k + 271);
    core._camera_seed_words(w, k);
    core._free(w);
    core._free(k);
  }
  // the air streamers as that tick has them (scroll 0.81, 4 rows: web/boost_gameplay.inc rider_fx_streamer_seed), so the
  // ribbon samples strm's bright rows from the start
  if (drop && core._rider_fx_streamer_seed && PLANE_DROP.streamer) {
    const p = core._malloc(PLANE_DROP.streamer.length * 4);
    core.HEAPF32.set(PLANE_DROP.streamer, p >> 2);
    core._rider_fx_streamer_seed(p);
    core._free(p);
  }
  state = new Float32Array(core.HEAPF32.buffer, core._rider_state(), 16).slice();
}
let resultsPending = false;
// 0x317A08 on a copy of the visual RNG 0x4FF018 (core visual_rng_words): the value 0x3177F0 would return next
let nextTimeLimit = 0;
let hudPrepass = null;
function visualRandPeek(core) {
  if (!core?._visual_rng_words) return 1;
  const w = Array.from(new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6));
  let v = (w[5] + w[4]) >>> 0,
    c = v < w[5] || v < w[4] ? 1 : 0;
  for (let k = 3; k >= 1; --k) {
    const n = (v + w[k] + c) >>> 0;
    c = n < w[k] ? 1 : 0;
    v = n;
  }
  return (v + w[0] + c) >>> 0;
}
/* career run statistics (0x155420 -> monster-trick medals, web/monster-tricks.js): recorded when a run ends; the next run begins */
/* In-engine cutscenes (web/cutscenes.js, docs/cutscenes.md): the host gives the scene, camera, the rider cast and the
   ground query; ui.cb.intro plays the event intro after the load screen (ui.loadEvent), ui.cutscene draws its overlay. */
function cutsceneRider(r, slot) {
  if (!r) return null;
  const id = typeof r === 'string' ? r : r.id,
    entry = (typeof r === 'object' && r.package ? r : null) || ui.riders.find((x) => x.id === id);
  if (!entry) return null;
  const base = entry.kind === 'cheat' ? ui.riders.find((x) => x.id === (entry.base || 'zoe')) : entry;
  // the boot rider (bootRider(): id / name / package only, kept when the page starts on its rider) has no character: its scfilter bit came
  // out 0, so every rider-masked step was dropped (the Transport's held loop: the list ended after the in-air while the rows still loaded,
  // the game screen between it and the arrival)
  return {
    id,
    slot,
    entry,
    character: base?.character ?? ui.riders.find((x) => x.id === (base?.id ?? id))?.character ?? -1,
    scale: (base?.model_size ?? entry.model_size ?? 100) / 100
  };
}
function cutsceneCast() {
  const human = cutsceneRider(selectedRider, 0),
    race = [human];
  const npcs =
    // pv eventInWorldAi: the in-world event's riders
    aiRace && aiRace.enabled !== false && (!course?.freeRide || worldEvent?.ai === aiRace) ? aiRace.racers.npcs : [];
  for (const n of npcs) race[n.slot] = cutsceneRider(n.character, n.slot);
  return { humans: [human], race, ai: race.slice(1).filter(Boolean), roles: [] };
}
function setupCutscenes() {
  if (new URL(location.href).searchParams.get('cutscenes') === '0') return;
  cutscenes = createCutscenes({
    T,
    scene,
    camera,
    origin,
    nisStopped: () => contexts.stops(CTX_BIT.NIS),
    // pv eventInWorldAi: the NIS director's camera point in the streamed world's section activation (core section_point; cutscenes.js
    // sectionFeed)
    onRaceActors: (list) => {
      if (!pv('eventInWorldAi') || !worldEvent?.ai || aiActive) return;
      worldAiHold ??= { pos: {}, started: false };
      for (const a of list) worldAiHold.pos[a.slot] = [a.pos[0], a.pos[1], a.pos[2], Math.cos(a.yaw), Math.sin(a.yaw)];
    },
    sectionPoint: (eye) => {
      if (
        // the gate's fly-over plays before cb.eventInWorld
        (!(pv('nisSectionPoint') || (pv('eventInWorldAi') && (worldEvent || worldAi))) && !(eye === null && sectionPointOn)) ||
        !core?._section_point ||
        !course?.freeRide
      )
        return;
      sectionPointOn = !!eye;
      if (eye) core._section_point(1, eye[0], eye[1], eye[2]);
      else core._section_point(0, 0, 0, 0);
    },
    // the NIS tick's mask bit 0x08 (0x230BDC)
    // never a core whose load is still running
    get core() {
      return coreLoading ? null : core;
    },
    // pv departCalls: a streamed world's location code -> its stage track (cutscenes.js hubCall)
    stageTrack: (c) => freeRide?.manifest?.streaming?.find((r) => r.code === c)?.track ?? -1,
    // pv nisTick: a Transport's ride holds the rider on each step's actor (123640 at the step's start; the TRANSP heli / gondola locators)
    onHumanActor: (r) => {
      if (nisTransport && core?._nis_hold) core._nis_hold(1, r.pos[0], r.pos[1], r.pos[2], Math.cos(r.yaw), Math.sin(r.yaw));
      else if (nisCut) nisCutHold(r);
    },
    // pv worldUnderCuts: a movie step (lists 29..31) is the only part of a cut that stops the world (PS2 new-career: the tick stands still
    // through the ABC1 movie)
    onMovie: (on) => {
      if (!pv('worldUnderCuts')) return;
      if (on) movieHold ??= contexts.push(CTX.HOLD, { owner: 'WS10 movie' });
      else {
        contexts.pop(movieHold);
        movieHold = null;
      }
    },
    audio: gameAudio,
    ui,
    // without what the switch has added (renderAcross)
    // a list held across a course switch draws itself (cutscenes.js acrossSwitch)
    render: () => (!live ? renderAcross() : renderer.render(scene, camera)),
    compile: (g) =>
      pv('worldWarm') && fogRenderer
        ? // pv worldWarm: a two-pass material once per side (the actors are built detached: nothing draws them meanwhile)
          compileFor([g], true)
        : fogRenderer
          ? fogRenderer.compileObject(g)
          : renderer.compileAsync(g, camera, scene),
    // pv worldWarm: a cutscene set compiled before it joins the scene (web/cutscenes.js ensureSet)
    // actors/sets compile for the world pass's target, as they draw (docs/firefox-load.md)
    compileHidden: pv('worldWarm')
      ? (g) => {
          if (fogRenderer) return compileFor([g], true);
          g.visible = true;
          try {
            return renderer.compileAsync(g, camera, scene);
          } finally {
            g.visible = false;
          }
        }
      : null,
    // 0x3369D8 nearest ground under the locator
    snap: (p, raw) => {
      const h = core?._height_at?.(raw[0] / 100, raw[2] / 100 + 1, -raw[1] / 100);
      return h > -1e8 ? h * 100 : null;
    },
    location: () => course?.code,
    // not the world still being loaded
    freeRideCourse: () => (coreLoading ? -1 : (freeRide?.course() ?? -1))
  });
  cutscenes.defaultCast = cutsceneCast;
  cutscenes.riderRecord = cutsceneRider;
  ui.cutscene = cutscenes;
  window.__cutscenes = cutscenes;
  // the actors' pipelines are built before the intro shows
  ui.cb.introPrepare = () =>
    course?.freeRide || mpGame?.racing || ui.mpUI?.racing
      ? null
      : cutscenes
          .prepare({
            steps: cutscenes.introSteps(ui.careerUI?.active?.career ? 'career' : 'single'),
            cast: cutsceneCast(),
            location: course.code
          })
          .then((r) => gpuIdle().then(() => r));

  // after the finish (career-ui.js): podium / rival challenge (0x27AC60), with the winner's chartune (28CDF8)
  ui.cb.cutscene = async (o) => {
    if (!cutscenes || (course?.freeRide && !['lodge', 'transport', 'transport-arrive', 'arrival', 'script'].includes(o.kind)))
      return { played: false };
    const back = ui.screen;
    ui.set('cutscene');
    // pv worldUnderCuts: the arrival list plays over the ticking world, the rider held by its actor (PS2 new-career: 917 ticks, +0xAC4 1);
    // cb.arrivalPlace places it after the list
    const underCut = !!o.hold && pv('worldUnderCuts') && running && !!core?._nis_hold && !!course?.freeRide;
    if (underCut) {
      nisCut = true;
      nisCutHold(null);
      // a first visit's list opens on its movie (WS10 pre-update phase 0: the tick stands still from WS10 until the movie ends, PS2
      // new-career s631..2334): the movie's hold covers the list's load too; cutscenes.js onMovie(false) pops it
      if (o.firstVisit) movieHold ??= contexts.push(CTX.HOLD, { owner: 'WS10 movie' });
    }
    // o.hold: the rider waits (world state 10 arrivals)
    const ws10Hold = o.hold && !underCut ? contexts.push(CTX.HOLD, { owner: `WS10 cutscene hold (${o.kind})` }) : null;
    if (o.kind === 'podium') gameAudio.podium?.();
    // an arrival intro (the Happiness plane, the heli drops) holds the audio ride start (28E888) until it ends (web/game-audio.js
    // arrivalCinematic)
    const arrival = o.kind === 'arrival';
    if (arrival) gameAudio.arrivalCinematic?.(true);
    try {
      return await playCutscene({ location: course.code, ...o });
    } catch (e) {
      console.warn('Cutscene failed', e);
      return { played: false };
    } finally {
      if (arrival) gameAudio.arrivalCinematic?.(false);
      contexts.pop(ws10Hold);
      if (underCut) {
        contexts.pop(movieHold);
        movieHold = null;
      }
      if (ui.screen === 'cutscene' && o.restore !== false) ui.set(back);
    }
  };
  // pv worldUnderCuts: the end of an arrival list played over the running ride (career-ui.js enterWorld): the rider leaves its actor and is
  // placed at the arrival (the new-career plane drop, the heli landing) as the ride start did; the game tick restarts (PS2 new-career: tick
  // 0 at WS2 after the list)
  // pv eventInWorld (docs/ctm-events-in-world.md stage 3): a CTM event run inside the streamed world, as the PS2 runs it (22D6C8 sets the
  // event type and game mode in place; WS1..WS3 set the round up with no load). The event's data is careerUI.eventPlan
  // (web/ctm-event-plan.js): its start seeds (core event_course_seed: the streamed world keeps its node / instance lists), its race event
  // document (init_race: the paths are the streamed bank's, test-ctm-event-world), the grid spawn, the slope-style list and the progress
  // meter. Stage 3 is the human's run only (no computer riders, QA). eventInWorldEnd returns to free ride's settings.

  // game mode -> 0x535C10 (22D6C8's kind table 0x47B450)
  const EVENT_TYPE_OF_MODE = [0, 1, 3, 2, 5, 5];
  // pv eventInWorldAi (docs/ctm-events-in-world.md stage 4): the in-world event's computer riders, made at the gate while the fly-over
  // plays (the PS2 makes them at gate + 2, 128958 / 129E20, and loads them under the fly-over). They are rider contexts of the streamed
  // core (the last in-world event's again: a context has no destroy), each given the event package's world, which is exactly the event's
  // resident locations (web/test-ctm-event-world.mjs), in parts: the first context loads it (web/load-slices.js feedContextWorld: the
  // shared camera terrain stays the streamed world's), the others copy it by key. At the start the human's node states are copied into them
  // and follow as kind 9.
  ui.cb.eventAiPrepare = ({ entry }) => {
    if (
      !pv('eventInWorldAi') ||
      !pv('eventInWorld') ||
      !core?._event_world_bodies_only ||
      !course?.freeRide ||
      !entry ||
      new URL(location.href).searchParams.get('ai') === '0'
    )
      return null;
    const human = core,
      get = (f, type) =>
        fetch(f).then((r) => {
          if (!r.ok) throw Error(`${f}: ${r.status}`);
          return type === 'json' ? r.json() : r.arrayBuffer();
        }),
      envRoot = entry.environmentRoot || entry.root;
    if (worldAiPool.core !== human) worldAiPool = { core: human, blocks: [] };
    const job = (async () => {
      const [terrain, world, rails, environmentMeta, environmentBin, lights0, lights1] = await Promise.all([
        get(entry.root + 'terrain.json'),
        get(entry.root + 'world_collision.json'),
        get(entry.root + 'rails.json'),
        get(envRoot + 'environment.json', 'json'),
        get(envRoot + 'environment.bin'),
        get(entry.root + 'local-lights.json', 'json'),
        get(entry.root + 'light-tree.json', 'json')
      ]);
      const cut = await cutEventWorld({ terrain, world, rails });
      if (!cut) throw Error('Event world cut failed');
      if (core !== human) return null;
      const hb = new TextEncoder().encode(cut.hash + '\0');
      return createAiRace({
        T,
        scene,
        human,
        course: entry,
        loader,
        origin,
        humanName: () => selectedRider.name,
        environmentMeta,
        environmentBytes: new Uint8Array(environmentBin),
        lightAssets: [lights0, lights1],
        worldKeys: { ...cut.keys, hash: cut.hash },
        contexts: worldAiPool.blocks,
        hostAtStart: true,
        anchorTick: 0,
        // the human's free-ride world rules (free-ride.js), one world
        contextSetup: (c) => {
          c._stage_object_route?.(1);
          c._stage_load_flags?.(1);
        },
        prepareWorld: async (c) => {
          const p = c._malloc(hb.length);
          c.HEAPU8.set(hb, p);
          try {
            await feedContextWorld(c, cut, p);
            await feedEventRails(c, cut, p);
          } finally {
            c._free(p);
          }
        }
      });
    })();
    job.catch((e) => console.warn('In-world computer riders unavailable', e));
    worldAi = { code: entry.code, job };
    return job;
  };
  ui.cb.eventInWorld = async ({ plan, mode, entry }) => {
    if (!pv('eventInWorld') || !core?._event_course_seed || !course?.freeRide || !plan?.ready || !entry) return false;
    const put = (t) => {
        const b = new TextEncoder().encode(t + '\0'),
          p = core._malloc(b.length);
        core.HEAPU8.set(b, p);
        return p;
      },
      call = (f, t) => {
        const p = put(t);
        try {
          return f(p);
        } finally {
          core._free(p);
        }
      };
    if (!call(core._event_course_seed, plan.code)) return false;
    const [meter, fs] = await Promise.all([loadProgressMeter(entry).catch(() => null), loadFreestyleEvent(entry).catch(() => null)]);
    worldEvent = { code: plan.code, mode, entry, plan, spawn, freestyle: freestyleEvent, meter: progressMeter, hud: ui.progressMeter };
    call(core._init_race, plan.initialText);
    core._peak_world_event_kind(EVENT_TYPE_OF_MODE[mode] ?? 0);
    core._peak_world_game_mode(mode);
    spawn = { position: plan.start.position, heading: plan.start.heading };
    freestyleEvent = fs;
    progressMeter = meter;
    ui.progressMeter = meter;
    {
      const ai = worldAi;
      worldAi = null;
      if (ai && ai.code !== plan.code)
        ai.job.then(
          (r) => {
            if (r && worldAiPool.core === core) worldAiPool.blocks.push(...r.detach());
          },
          () => {}
        );
      else if (ai) {
        const race = await ai.job.catch(() => null);
        if (race && worldEvent && core) {
          worldEvent.ai = race;
          worldEvent.prevAi = aiRace;
          aiRace = race;
          race.racers.setAnchorRng(null);
          ui.cb.standings = aiStandings;
          ui.cb.lineup = aiLineup;
          // the riders' trails / wake / spray / streamers and relationship icons (and a backcountry rival's beam), made once per world as
          // loadCourse makes them for an event course: they hide while no race runs (aiActive) and go with the world at the next course
          // switch
          race.onRelationshipNotice = (character, score) => ui.careerUI?.messages?.notify(character, score);
          if (opponentFx && opponentFx.entries.length < race.racers.npcs.length) {
            opponentFx.update([], camera, false);
            opponentFx = null;
          }
          // (a smaller set's meshes stay hidden)
          if (!opponentFx)
            opponentFx = await createOpponentFx({ scene, origin, count: race.racers.npcs.length, sampleCore: core }).catch((e) => {
              console.warn('Opponent trails unavailable', e);
              return null;
            });
          if (riderIcons && (riderIcons.__count ?? 0) < race.racers.npcs.length) {
            riderIcons.reset();
            riderIcons = null;
          }
          if (!riderIcons) {
            riderIcons = createRiderIcons({ T, scene, origin, count: race.racers.npcs.length, texture: '/assets/UI/rival-exclaim.png' });
            riderIcons.__count = race.racers.npcs.length;
          }
          // QA: the in-world race's trails and icons
          if (new URL(location.href).searchParams.has('qa')) Object.assign(window, { __opponentFx: opponentFx, __riderIcons: riderIcons });
          if (entry.event === 'backcountry' && !rivalBeam)
            rivalBeam = createRivalBeam({ T, scene, origin, texture: '/assets/UI/rival-beam.png' });
        }
      }
      // pv eventInWorldAi
    }
    return true;
  };
  ui.cb.eventInWorldEnd = async ({ keepRiders = false, session = null } = {}) => {
    const w = worldEvent;
    if (!w || !core) return false;
    worldEvent = null;
    worldAiHold = null;

    // pv eventReturnInWorld: WS15 keeps the race's riders for the return's first ticks (transportInWorld places them with the human,
    // worldAiBefore removes them)
    // pv eventInWorldAi: the contexts wait for the next in-world event
    if (w.ai && keepRiders) {
      aiActive = false;
      if (aiRace === w.ai) aiRace = w.prevAi ?? null;
      worldAiReturn = { race: w.ai, ticks: 0, placed: false };
    } else if (w.ai) {
      aiActive = false;
      if (aiRace === w.ai) aiRace = w.prevAi ?? null;
      core._shared_world_nodes?.(0);
      try {
        const blocks = w.ai.detach();
        if (worldAiPool.core === core) worldAiPool.blocks.push(...blocks);
      } catch (e) {
        console.warn('In-world computer riders', e);
      }
    }
    const put = (t) => {
        const b = new TextEncoder().encode(t + '\0'),
          p = core._malloc(b.length);
        core.HEAPU8.set(b, p);
        return p;
      },
      call = (f, t) => {
        const p = put(t);
        try {
          return f(p);
        } finally {
          core._free(p);
        }
      };
    // pv eventReturnInWorld, the same location (session): world state 15 through web/event-return.js sessionReturn (free ride's settings,
    // the race's riders placed with the human; their removal after 8 ticks: gameHost.worldAiBefore)
    if (session) {
      const text = await fetch(course.initial)
        .then((r) => (r.ok ? r.text() : null))
        .catch(() => null);
      if (!text || !core) throw new Error('In-world return: no free-ride document');
      const r = worldAiReturn;
      sessionReturn({
        human: core,
        racers: r ? { npcs: r.race.racers.npcs, resume: (o) => r.race.resume(o) } : null,
        cstr: put,
        bank: session.bank,
        entry: session.entry.region,
        freeRideDoc: text
      });
      if (r) {
        r.placed = true;
        aiRace = r.race;
        aiActive = true;
      }
      worldAiReturn = { race: r?.race ?? null, ticks: 0, placed: true, session: true };
      spawn = session.entry;
      freeRide?.sessionReturned?.();
      freestyleEvent = w.freestyle;
      progressMeter = w.meter;
      ui.progressMeter = w.hud;
      return true;
    }
    call(core._event_course_seed, '');
    core._peak_world_event_kind(course.freeRide.kind);
    core._peak_world_game_mode(course.freeRide.mode ?? 12);
    const text = await fetch(course.initial)
      .then((r) => (r.ok ? r.text() : null))
      .catch(() => null);
    if (text && core) call(core._init_race, text);
    spawn = w.spawn;
    freestyleEvent = w.freestyle;
    progressMeter = w.meter;
    ui.progressMeter = w.hud;
    return true;
  };

  // pv eventReturnInWorld: WS13 (Next heat, results Restart; 0x235AA0) resets the in-world event's world before the gondola: 230180 (core
  // ctm_world_reset; the fences' DeadNodes purged, missions / stage world / pickups back, the activation list kept)
  ui.cb.heatReset = () => {
    if (pv('eventReturnInWorld') && worldEvent && core?._ctm_world_reset) core._ctm_world_reset();
  };
  ui.cb.inWorldEvent = () => (worldEvent ? { code: worldEvent.code, mode: worldEvent.mode } : null);
  ui.cb.introInWorld = (next) => {
    if (!worldEvent) {
      next();
      return;
    }
    ui.set('cutscene');
    let done = false;
    // WS1's [approach, idle] after the fly-over, in the streamed world at the event's location (cutscenes.js 'career-ridein')
    const go = () => {
      if (done) return;
      done = true;
      next();
    };

    // pv eventInWorldAi: this human's lineup first (the warm-up's aiRace.prepare of a loaded event), so the cast has its riders
    const lineup =
      worldEvent.ai && aiRace === worldEvent.ai
        ? aiRace
            .prepare({ rider: selectedRider, event: eventOf() })
            .then(
              // the game RNG runs on from free ride (no event load: the anchor's words are a load's)
              () => worldEvent?.ai?.racers.setAnchorRng(null)
            )
            .catch((e) => console.warn('Computer-rider lineup failed', e))
        : Promise.resolve();
    lineup
      .then(() => cutscenes.playEventIntro({ mode: 'career', cast: cutsceneCast(), location: worldEvent?.code, onIdle: go }))
      .then(go, (e) => {
        console.warn('Event intro failed', e);
        go();
      });
  };
  ui.cb.arrivalPlace = () => {
    if (!core || !running) return false;
    nisCutEnd();
    resetPhysics();
    freeRide?.afterReset();
    return true;
  };
  ui.cb.intro = (next) => {
    if (course?.freeRide || mpGame?.racing || ui.mpUI?.racing) {
      next();
      return;
    }
    // online: the server's start does not wait for a local intro
    ui.set('cutscene');
    let done = false;
    const go = () => {
      if (done) return;
      done = true;
      next();
    };
    cutscenes
      .playEventIntro({ mode: ui.careerUI?.active?.career ? 'career' : 'single', cast: cutsceneCast(), location: course.code, onIdle: go })
      .then(go, (e) => {
        console.warn('Event intro failed', e);
        go();
      });
  };
}
function careerRunEnd(finish, dump = null) {
  if (!ui.careerUI) return;
  if (!dump) {
    if (!core) return;
    dump = Uint32Array.from(new Uint32Array(core.HEAPU8.buffer, core._score_object_dump(), 0x1d0 / 4));
  }
  ui.careerUI.runEnd?.(dump, finish);
}
// the start state of the run, kept for its replay (web/replay.js)
let runTimeLimit = 0,
  runInputMap = 0,
  runCollect = null;
/* R: a replay's start state (web/replay.js): the race again from its start on the same core, without the run's career, audio
   or UI side effects, then the start words the reset does not restore */

// pv eventReturnInWorld: a C string in the core's heap (web/event-return.js frees it)
function coreString(t) {
  const b = new TextEncoder().encode(t + '\0'),
    p = core._malloc(b.length);
  core.HEAPU8.set(b, p);
  return p;
}
// pv eventReturnInWorld: free ride's run again after world state 15's return (web/event-return.js sessionReturn placed the riders):
// startRun's page state without its core resets. The PS2's return resets no rider (230180 and 11D390's free-ride branch only), so no
// reset_animation / reset_rider / start_event here; then WS15's white fade (2E4370, 1.0 s, the HUD over it)
function returnRun() {
  readyShown = readyAi = false;
  cutscenes?.stop();
  nisCutEnd();
  overlay.drop();
  careerRunEnd(null);
  audioFinished = false;
  audioPhase = null;
  rumble.reset();
  gameAudio
    .runStart({
      courseIndex: freeRide?.course() >= 0 ? freeRide.course() : course.freeRide.course,
      singleEvent: !ui.careerMode,
      freeRide: { kind: course.freeRide.kind, mode: course.freeRide.mode },
      courseCode: course.code,
      character: selectedRider?.id
    })
    .catch((e) => console.warn('Race music failed', e));
  runTimeLimit = 0;
  nextTimeLimit = 0;
  ui.set('game');
  finished = false;
  resultsPending = false;
  raceInfo = null;
  clearInput();
  riderIcons?.reset();
  progressMeter?.load();
  state = new Float32Array(core.HEAPF32.buffer, core._rider_state(), 16).slice();
  lastRescues = state[13];
  cameraPose = previousCameraPose = null;
  currentRiderFrame = previousRiderFrame = null;
  clock = score = pending = 0;
  trickUntil = 0;
  trick = '';
  acc = 0;
  last = performance.now();
  lastCamera.set(0, 0, 0);
  simulation.reset();
  running = true;
  runHoldsEnd('returnRun');
  ui.careerUI?.runBegin?.();
  ui.trickHud?.resetUberHint?.(visualRandPeek(core) & 1);
  replay?.liveStart();
  cutscenes?.fadeFrom?.({ ticks: 60, colour: 'white', hud: true });
}
function startRun(R = null) {
  if (!live) return;
  readyShown = readyAi = false;
  cutscenes?.stop();
  nisCutEnd();
  // pv worldUnderCuts / eventInWorld: the cut's rider hold ends with the run's start
  // the start-gate idle loop ends with the objectives card (world state 2 exit 0x236CD8)
  if (!R && (pendingRider || riderStale())) {
    // a rider picked without an event load (e.g. ssxQA, restart after a menu switch), or its profile changed (the lodge, a career start; pv
    // careerRider): load it first
    ensureRider()
      .then(() => startRun())
      .catch((e) => console.error(e));
    return;
  }
  // a replay: the game sounds start over, the music plays on (288AE0)
  if (!R) {
    // buyAttribs (web/buy-attribs.js): the runtime bank 0x535538 is the profile's bytes at every run, the stat getters read it live
    {
      const a = runAttributes(ui, careerId());
      if (a) ui.cb.attributes(a);
    }
    careerRunEnd(null);
    audioFinished = false;
    audioPhase = null;
    rumble.reset();
    gameAudio
      .runStart({
        courseIndex: worldEvent
          ? (COURSE_INDEX[worldEvent.code] ?? 0)
          : course.freeRide
            ? freeRide?.course() >= 0
              ? freeRide.course()
              : course.freeRide.course
            : (COURSE_INDEX[course.code] ?? 0),
        singleEvent: !ui.careerMode,
        freeRide: course.freeRide && !worldEvent ? { kind: course.freeRide.kind, mode: course.freeRide.mode } : null,
        courseCode: worldEvent?.code ?? course.code,
        character: selectedRider?.id
      })
      .catch((e) => console.warn('Race music failed', e));
  } else audioSafe(() => gameAudio.replayRewind?.());
  runTimeLimit = R ? R.timeLimit : nextTimeLimit;
  core?._race_time_limit?.(runTimeLimit);
  nextTimeLimit = 0;
  // slope style checkpoint bonus list (web/freestyle-event.js)
  applyFreestyleEvent(core, freestyleEvent);
  terrainRefinement?.reset();
  sam.userData.sourceSkin?.reset();
  sam.userData.sourceLighting?.reset();
  core._reset_fog();
  core._reset_rider_lighting();
  for (const mesh of eventDeadMeshes) mesh.visible = true;
  if (!R) ui.set('game');
  finished = false;
  resultsPending = false;
  raceInfo = null;
  aiActive = false;
  if (animationReady) {
    core._reset_animation();
    // pv eventInWorld: an offline PS2 event start resets no stage world (the Big Challenge markers' Hide nodes, the missions, the pickups
    // carry on); event_grid_start resets the race session itself, keeping the world
    if (!(worldEvent && core._event_grid_start)) core._reset_race();
  }
  if (!R && !pv('padCarry')) clearInput();
  // pv padCarry: the start from a card keeps the held pad and its history (the PS2's one history; a replay starts settled)
  if (R || !pv('padCarry') || !core._pad_history_sample) core._reset_pad_history();
  runInputMap = R ? R.inputMap : (ui.feScreens?.inputMap?.() ?? 0);
  core._set_input_map?.(runInputMap);
  // FE Controller Settings Default/Pro: INPUT.MAP / INPUT2.MAP
  if (worldEvent && core._event_grid_start)
    // pv eventInWorld: the carried rider is placed by event_grid_start below
    nisRelease();
  else resetPhysics();
  // the run's camera option (a change in the run is replayed at its tick)
  if (R) core._set_camera_view?.(R.cameraView);
  freeRide?.afterReset();
  if (
    !new URL(location.href).searchParams.has('qaGlide') &&
    !new URL(location.href).searchParams.has('railTest') &&
    !new URL(location.href).searchParams.has('pickupTest') &&
    !new URL(location.href).searchParams.has('finishTest')
  ) {
    if (course.eventStart !== false || worldEvent) {
      if (!R) mpGame?.beforeStart(core);
      if (R) collectApply(core, R.collect);
      else if (!worldEvent)
        runCollect = collectStart(core, {
          careerMode: ui.careerMode,
          career: ui.careerUI?.career,
          riderId: careerId(),
          courseCode: course.code
        });
      // pv eventInWorld: the streamed world's own collectible slot (free-ride afterReset), the CTM path
      if (worldEvent && core._event_grid_start) {
        // pv eventInWorld: 11D390's grid placement of the carried rider (core event_grid_start: the free-ride words it keeps), on the
        // event's grid route (its document's original_reset.event_route; the streamed world's document holds free ride's)
        const b = new TextEncoder().encode(worldEvent.plan.initialText + '\0'),
          p = core._malloc(b.length);
        core.HEAPU8.set(b, p);
        try {
          core._event_route_seed?.(p);
        } finally {
          core._free(p);
        }
        core._section_restart?.();
        // 129768 -> 0x103358 at the Continue: the activation list emptied, a rescan at tick 0's end (PS2 c0a-full-ai)
        core._event_grid_start(...spawn.position, spawn.heading);
      } else core._start_event();
      aiActive =
        // career slope style: nobody rides
        !!aiRace && aiRace.enabled !== false && (mpGame?.aiAllowed() ?? true);
      if (aiActive)
        aiRace.start(
          R
            ? { replay: R.ai }
            : worldEvent?.ai === aiRace && worldAiHold?.started
              ? // pv eventInWorldAi: npc_grid_start keeps what WS1 carried
                { gridStart: true }
              : undefined
        );
      if (worldEvent) worldAiHold = null;
      if (aiActive && worldEvent?.ai === aiRace)
        // pv eventInWorldAi: the human's world state into the riders' contexts
        worldAiSync(aiRace);
      riderIcons?.reset();
      // every rider placed (0x11D660 -> 0x2105B0)
      progressMeter?.load();
    }
    for (const mesh of eventDeadMeshes) mesh.visible = false;
    state = new Float32Array(core.HEAPF32.buffer, core._rider_state(), 16).slice();
  }
  lastRescues = state[13];
  cameraPose = previousCameraPose = null;
  currentRiderFrame = previousRiderFrame = null;
  clock = score = pending = 0;
  trickUntil = 0;
  trick = '';
  acc = 0;
  last = performance.now();
  lastCamera.set(0, 0, 0);
  if (R) {
    // the words the reset leaves as the run left them: the shared game RNG 0x4FF030, the visual stream 0x4FF018 and gp+0xA0C
    const W = (p) => new Uint32Array(core.HEAPU8.buffer, p, 6);
    W(core._animation_rng_words()).set(R.rng);
    W(core._visual_rng_words()).set(R.visual);
    if (R.lcg != null && core._visual_lcg_word) new Uint32Array(core.HEAPU8.buffer, core._visual_lcg_word(), 1)[0] = R.lcg;
    return;
  }
  simulation.reset();
  running = true;
  runHoldsEnd('startRun');
  ui.careerUI?.runBegin?.();
  ui.trickHud?.resetUberHint?.(visualRandPeek(core) & 1);
  // 0x1E9AD0 HUD init: owner+0x55C = 0x3177F0() & 1 (next visual-RNG word, not consumed here)
  replay?.liveStart();
}
// Pause contexts (web/pause-contexts.js, docs/pause-contexts.md): every freeze of the world is a pause context its owner pushes and pops;
// paused is the derived read: the top context stops the simulation (bit 0x01).
function isPaused() {
  return contexts.stops(CTX_BIT.SIM);
}
// The overlay over the ride: the pause menu / MCOMM (0x230A94 -> 0x20CA10), the lodge prompt / map (WS14 0x236504), the FAQ (overlay 0x22:
// its context is not read yet), context 2 with the audio pause (289B70 / 289BB8). main.js holds its handle: open() pushes it, close() pops
// it when the menu returns to the ride (Return, Triangle, Give Up, a Transport's Yes), drop() when the menu goes with its world (Quit,
// Restart, a world load, the results). The sub-screens (Map, Yes / No, Options, Audio, Messages) push nothing. station: the no-NIS-hold
// station cut's world stop, which ends with the station's prompt / map.
const overlay = {
  h: null,
  station: null,
  open(screen = null, { owner = 'pause menu / MCOMM / station prompt', ends = (s) => s === 'game', ctx = CTX.MENU } = {}) {
    if (!this.h) {
      last = performance.now();
      this.h = contexts.push(ctx, { owner, audio: true, ends });
      if (!pv('padCarry')) clearInput();
    }
    ui.set(screen || (bigChallenges?.pauseScreen() ?? 'pause'));
  },
  close() {
    if (this.drop()) {
      try {
        nisResume();
      } catch (e) {
        console.error('NIS release failed', e);
        nisRelease();
      }
      // the close always completes (a throwing placement left the world paused under 'game')
      last = performance.now();
    }
    ui.set('game');
    // (also the ride's return from a Big Challenge prompt, whose own context 3 its owner popped; pv padCarry: a held key stays held, as the
    // PS2 pad)
    if (!pv('padCarry')) clearInput();
  },
  drop() {
    contexts.pop(this.station);
    this.station = null;
    const had = contexts.pop(this.h);
    this.h = null;
    return had;
  }
};
// WS1 (the event's intro lists: the ride-in fly-over, the event transport) stops the ride until the event's run starts (WS3) or the world
// goes.
/* the human's node states into the riders' contexts (after their start's own event seeds), then kind 9 (web/ai-racers.js syncWorldNodes) */
function worldAiSync(race) {
  if (race && core)
    syncWorldNodes(
      core,
      race.racers.npcs.map((n) => n.core)
    );
}

// pv eventReturnInWorld: the race's riders out of the world after the return (their contexts to the pool)
function worldAiEnd() {
  const r = worldAiReturn;
  worldAiReturn = null;
  if (!r) return;
  if (r.race && aiRace === r.race) {
    aiActive = false;
    aiRace = null;
  }
  try {
    // pv eventReturnInWorld, WS15's return: WS1's phase 3 end (the riders out) and its exit's tick restart (web/event-return.js
    // sessionRidersLeave)
    const blocks = r.session
      ? sessionRidersLeave({ human: core, racers: r.race })
      : (r.race.racers && snapshotReleaseRiders({ racers: r.race.racers }), r.race.detach());
    if (worldAiPool.core === core) worldAiPool.blocks.push(...blocks);
  } catch (e) {
    console.warn('In-world riders', e);
  }
}
// worldAiHold: pv eventInWorldAi, the riders' NIS actor spots under WS1 (cutscenes.js onRaceActors) pv eventInWorldAi: the riders being
// made at the gate (ui.cb.eventAiPrepare), and the rider contexts of the last in-world event (no destroy: reused) worldEvent: pv
// eventInWorld's event run in the streamed world (ui.cb.eventInWorld)
let ws1Hold = null,
  movieHold = null,
  qaStepping = false,
  worldEvent = null,
  worldAi = null,
  worldAiPool = { core: null, blocks: [] },
  worldAiHold = null,
  sectionPointOn = false,
  worldAiReturn = null;
function runHoldsEnd(at) {
  contexts.pop(ws1Hold);
  ws1Hold = null;
  contexts.pop(movieHold);
  movieHold = null;
  if (at === 'stopRun') {
    nisCut = false;
    nisCutAt = null;
  }
  const left = contexts.clear();
  overlay.h = overlay.station = null;
  // the safety net: a leaked holder must not freeze the next run
  if (left.length) console.warn(`pause contexts left open at ${at} (a missing pop): ${left.join(', ')}`);
}
let startButtonHeld = false;

// ?simtrace=1 (QA, docs/workers.md): the per-tick exactness trace of the running game (web/game-tick.js gameTrace)
const simTrace =
  new URL(location.href).searchParams.get('simtrace') === '1' ? { ticks: [], out: new Float32Array(TRACE_OUT_FLOATS) } : null;
if (simTrace) {
  window.__simTrace = simTrace;
  window.__memoryHash = () => memoryHash(core);
}

// the 60 Hz game tick: web/game-tick.js (simulate / present, docs/workers.md "Game tick")
const gameHost = {
  get core() {
    return core;
  },
  get replay() {
    return replay;
  },
  get padPtr() {
    return padPtr;
  },
  get boneCount() {
    return bones.length;
  },
  get animationReady() {
    return animationReady;
  },
  get wantPrepassSlots() {
    return !!ui.trickHud;
  },
  get backcountry() {
    return course.event === 'backcountry';
  },
  // the finish celebration of a freestyle run (web/game-tick.js)
  freestylePlace(score) {
    return ui.careerUI?.freestyleFinishPlace?.(score) ?? null;
  },
  get freeRide() {
    return freeRide;
  },
  get bigChallenges() {
    return worldEvent ? null : bigChallenges;
  },
  faqTick() {
    faqOpen();
  },
  // pv eventInWorldAi: WS1's approach carries the event's riders (fresh at their actors, then held there each tick after the human;
  // ai-racers.js holdTick)
  // pv eventReturnInWorld (web/game-tick.js): an in-world event's live ticks end where the PS2's auto replay starts (finish + 288 TIME'S UP
  // / + 408 FINISH; its results-time state is what the Transport keeps)
  liveStopAt(timedOut) {
    return pv('eventReturnInWorld') && worldEvent ? (timedOut ? RESULTS_TICKS_TIME_UP : RESULTS_TICKS_FINISH) : null;
  },
  // pv nisAfterScan: the NIS tick at the game tick's start (0x230BE4), before every pass of the tick
  nisStart() {
    nisStart();
  },
  // the 8 ticks WS15 -> WS1 arg 0 -> WS2 -> WS3 -> WS4 (tick restart)
  worldAiBefore() {
    const r = worldAiReturn;
    if (!r?.placed) return;
    if (r.ticks >= 8) {
      worldAiEnd();
      return;
    }
    r.ticks++;
  },
  worldAiTick() {
    if (!worldAiHold || aiActive || !worldEvent?.ai || worldEvent.ai !== aiRace) return;
    try {
      aiRace.racers.holdTick((slot) => worldAiHold.pos[slot] ?? null, { fresh: !worldAiHold.started });
      worldAiHold.started = true;
    } catch (e) {
      console.warn('In-world riders under WS1', e);
      worldAiHold = null;
    }
  },
  // pv faqDefer: after the offer read (0x2309A4 follows 0x230890)
  get aiActive() {
    return aiActive;
  },
  get aiRace() {
    return aiRace;
  },
  get mpGame() {
    return mpGame;
  },
  get progressMeter() {
    return progressMeter;
  },
  progressMeterRiders,
  get state() {
    return state;
  },
  set state(v) {
    state = v;
  },
  get lastRescues() {
    return lastRescues;
  },
  set lastRescues(v) {
    lastRescues = v;
  },
  get lastResetPlacement() {
    return lastResetPlacement;
  },
  set lastResetPlacement(v) {
    lastResetPlacement = v;
  },
  get animPose() {
    return animPose;
  },
  set animPose(v) {
    animPose = v;
  },
  get animInfo() {
    return animInfo;
  },
  set animInfo(v) {
    animInfo = v;
  },
  get posePhysicalFrame() {
    return posePhysicalFrame;
  },
  set posePhysicalFrame(v) {
    posePhysicalFrame = v;
  },
  get pending() {
    return pending;
  },
  set pending(v) {
    pending = v;
  },
  get score() {
    return score;
  },
  set score(v) {
    score = v;
  },
  get trickUntil() {
    return trickUntil;
  },
  set trickUntil(v) {
    trickUntil = v;
  },
  get clock() {
    return clock;
  },
  set clock(v) {
    clock = v;
  },
  get raceInfo() {
    return raceInfo;
  },
  set raceInfo(v) {
    raceInfo = v;
  },
  get finished() {
    return finished;
  },
  set finished(v) {
    finished = v;
  },
  get postFinishTicks() {
    return postFinishTicks;
  },
  set postFinishTicks(v) {
    postFinishTicks = v;
  },
  get resultsPending() {
    return resultsPending;
  },
  set resultsPending(v) {
    resultsPending = v;
  },
  get running() {
    return running;
  },
  set running(v) {
    running = v;
  },
  get paused() {
    return !qaStepping && isPaused();
  },
  // the results replace the pause menu (web/game-tick.js)
  leavePause() {
    overlay.drop();
  },
  get ui() {
    return ui;
  },
  keyboardPad,
  get currentRiderFrame() {
    return currentRiderFrame;
  },
  set currentRiderFrame(v) {
    currentRiderFrame = v;
  },
  get previousRiderFrame() {
    return previousRiderFrame;
  },
  set previousRiderFrame(v) {
    previousRiderFrame = v;
  },
  get cameraPose() {
    return cameraPose;
  },
  set cameraPose(v) {
    cameraPose = v;
  },
  get previousCameraPose() {
    return previousCameraPose;
  },
  set previousCameraPose(v) {
    previousCameraPose = v;
  },
  get riderIcons() {
    return riderIcons;
  },
  get screenTint() {
    return screenTint;
  },
  get sunFlare() {
    return sunFlare;
  },
  get glarePass() {
    return glarePass;
  },
  get lightningStrikes() {
    return lightningStrikes;
  },
  set lightningStrikes(v) {
    lightningStrikes = v;
  },
  audioSafe: (f) => audioSafe(f),
  gameAudio,
  captureRiderFrame,
  get rig() {
    return rig;
  },
  origin,
  get sam() {
    return sam;
  },
  get humanSkinReset() {
    return humanSkinReset;
  },
  set humanSkinReset(v) {
    humanSkinReset = v;
  },
  get humanSkinPending() {
    return humanSkinPending;
  },
  set humanSkinPending(v) {
    humanSkinPending = v;
  },
  get riderLightingUpdate() {
    return riderLightingUpdate;
  },
  get terrainRefinement() {
    return terrainRefinement;
  },
  get trick() {
    return trick;
  },
  set trick(v) {
    trick = v;
  },
  careerRunEnd: (f, d) => careerRunEnd(f, d),
  clearInput: () => clearInput(),
  get selectedRider() {
    return selectedRider;
  },
  get careerId() {
    return careerId();
  },
  rumble,
  collectPoll,
  get course() {
    return course;
  },
  get hudPrepass() {
    return hudPrepass;
  },
  set hudPrepass(v) {
    hudPrepass = v;
  },
  // a solo event (no computer riders racing) takes the PS2's game RNG at its countdown anchor (web/event-anchor-rng.js);
  // a course whose anchor counts computer-rider start draws drops them when nobody rides
  rideTick(st, jump) {
    if (!mpGame?.simulating?.()) diagRide(st, jump);
  },
  soloTickStart(c) {
    if (course.freeRide || mpGame?.simulating?.()) return;
    const e = EVENT_ANCHOR_DRAWS[course.code];
    if (!e || !c._game_tick || c._game_tick() !== e.tick) return;
    const a = eventAnchorWords(course.code, humanCharacter(selectedRider ?? { id: 'zoe', character: 4 }, ui.riders ?? []));
    if (!a) return;
    let w = a.words;
    if (e.start) {
      w = seededWords(0);
      for (let k = 0; k < a.draws - e.start; k++) nextWord(w);
    }
    new Uint32Array(c.HEAPU8.buffer, c._animation_rng_words(), 6).set(w);
  }
};
const gameTick = createGameTick(gameHost, {
  trace: (rec) => {
    if (simTrace)
      simTrace.ticks.push(
        gameTrace(
          core,
          aiActive ? aiRace.racers.npcs.map((n) => n.core) : [],
          traceOut(simTrace.out, { state, posePhysicalFrame, raceInfo, cameraPose: rec.camera ?? null })
        )
      );
  }
});
function simTick(input) {
  gameTick.tick(input, FixedStepClock.ticksLeft);
}
/* The race replay (web/replay.js, docs/replay.md; pv 'replay'): the run re-simulated from its start on this core with its recorded
   pad, behind the post-race screens once they show (auto, looping; 0x20A8F8 -> 0x2706B8) and for the results' Replay item (full). */
const REPLAY_AUTO_SCREENS = ['results', 'ctm-results', 'ctm-award', 'ctm-records'],
  REPLAY_SCREENS = [
    ...REPLAY_AUTO_SCREENS,
    'replay',
    'ctm-restart',
    'ctm-quitsave',
    'ctm-quit',
    // pv onlineRecords: the records' Online Records keeps the replay behind it, as Records does
    'ctm-board'
  ];
const QA_NO_EVENT = ['qaGlide', 'railTest', 'pickupTest', 'finishTest'].some((k) => new URL(location.href).searchParams.has(k));
let replay = null;
function createRaceReplay() {
  return createReplay({
    /* 0x20A8F8: every event kind but the peak runs (modes 6-11); no results in free ride; online races keep the finish camera */
    allowed: () =>
      !!core &&
      // pv onlineRecords: a downloaded run
      (!!ui.onlineWatch?.active ||
        (!QA_NO_EVENT &&
          (inWorldReplay() || (course?.eventStart !== false && !course?.freeRide)) &&
          !mpGame?.racing &&
          !ui.mpUI?.racing &&
          !ui.onlineMode &&
          !((ui.careerUI?.career?.active?.ev?.mode ?? 0) >= 6))),
    snapshot: () => ({
      inWorld: inWorldReplay() ? inWorldCountdownSave() : null,
      timeLimit: runTimeLimit,
      inputMap: runInputMap,
      cameraView: ui.cameraView ?? 0x3d,
      collect: runCollect,
      ai: aiActive ? aiRace.replaySnapshot() : null,
      rng: Array.from(new Uint32Array(core.HEAPU8.buffer, core._animation_rng_words(), 6)),
      visual: Array.from(new Uint32Array(core.HEAPU8.buffer, core._visual_rng_words(), 6)),
      lcg: core._visual_lcg_word ? new Uint32Array(core.HEAPU8.buffer, core._visual_lcg_word(), 1)[0] : null
    }),
    restart: (R) => {
      try {
        if (R?.inWorld) return inWorldReplayRestart(R);
        startRun(R);
        return true;
      } catch (e) {
        console.warn('Replay start failed', e);
        return false;
      }
    },
    simulate: (input) => gameTick.simulate(input, FixedStepClock.ticksLeft),
    present: (rec) => gameTick.present(rec),
    event: (e) => {
      if (e.kind === 'giveUp') core._race_give_up?.();
      else if (e.kind === 'camera') core._set_camera_view?.(e.value);
    },
    ended: () => {
      aiRace?.replayEnd?.();
      // the session's camera option again
      if (core?._set_camera_view) core._set_camera_view(ui.cameraView ?? 0x3d);
    },
    // the replay view (web/replay_camera.inc): the course's replay camera triggers (tools/export_camera_triggers.py), loaded with the run
    prepare: () => {
      // pv eventReturnInWorld: an in-world event's course triggers, not the streamed world's
      const c = core,
        code = inWorldReplay() ? worldEvent.code : course?.code;
      if (!c?._replay_camera_triggers || !code || (replayTriggersCore === c && replayTriggersCode === code)) return;
      replayTriggersCore = c;
      replayTriggersCode = code;
      fetch('/assets/' + code + '/camera-triggers.json')
        .then((r) => (r.ok ? r.text() : null))
        .then((t) => {
          if (!t || core !== c) return;
          const b = new TextEncoder().encode(t + '\0'),
            p = c._malloc(b.length);
          c.HEAPU8.set(b, p);
          try {
            c._replay_camera_triggers(p);
          } finally {
            c._free(p);
          }
        })
        .catch((e) => console.warn('Replay cameras unavailable', e));
    },
    /* a seek's ticks that are not drawn: their queued sounds and career events are dropped */
    skipped: () => {
      for (const c of [core, ...(aiActive ? aiRace.racers.npcs.map((n) => n.core) : [])]) c?._audio_events_clear?.();
      core?._stage_collect_events?.();
      core?._score_career_events?.();
    },
    cameraBegin: (fromLive) => core?._replay_camera_begin?.(0x5d, fromLive ? 1 : 0),
    cameraRewind: () => core?._replay_camera_rewind?.(),
    cameraMode: (t) => core?._replay_camera_mode?.(t),
    cameraPaused: () => {
      if (!core?._replay_camera_step) return;
      const m = replay.manual,
        v = new Float32Array(core.HEAPF32.buffer, core._replay_camera_step(1, m[0], m[1], m[2]), 10).slice();
      previousCameraPose = cameraPose = v;
    }
  });
}
/* pv eventReturnInWorld (b): an in-world event replays behind its results (docs/replay.md §2a) with the PS2's two snapshots: the
 countdown's at the live start (0x26D818: the replay's restart, its seeks) and the results time's at the replay's start (R+0x3D0, state 9),
 which the results' Transport puts back (0x2706F0). The contexts' buffers are made at the first save (a one-time growth, reused). */
function inWorldReplay() {
  return pv('eventReturnInWorld') && !!worldEvent && !!core;
}
const inWorldRacers = () => (worldEvent?.ai === aiRace && aiRace ? aiRace.racers : null);
// the results-time snapshot's JS state (slot 1), from the replay's first start to the Transport
let inWorldResults = null;
function inWorldCountdownSave() {
  requireSnapshotCore(core);
  const racers = inWorldRacers();
  {
    const q = new URL(location.href).searchParams;
    snapshotAttach({ human: core, racers, qa: q.has('qa') && q.get('snapshotQa') !== '0' });
  }
  // ?qa: the kept tables' checks at every save / restore (snapshotQa=0: without them, e.g. a memory run)
  inWorldResults = null;
  return { js: snapshotCountdown({ human: core, racers }) };
}
// the replay's restart (web/replay.js rewind): at the replay's start the results time is kept (once per run), then the countdown comes
// back; the page's run state as startRun(R) leaves it, without its core resets
function inWorldReplayRestart(R) {
  const racers = inWorldRacers();
  if (!inWorldResults) inWorldResults = snapshotSave(SNAPSHOT_RESULTS, { human: core, racers });
  snapshotRestore(SNAPSHOT_COUNTDOWN, R.inWorld.js, { human: core, racers });
  aiRace?.replayRestore?.(R.ai);
  aiActive = !!racers;
  readyShown = readyAi = false;
  finished = false;
  resultsPending = false;
  raceInfo = null;
  core._set_camera_view?.(R.cameraView);
  state = new Float32Array(core.HEAPF32.buffer, core._rider_state(), 16).slice();
  lastRescues = state[13];
  cameraPose = previousCameraPose = null;
  currentRiderFrame = previousRiderFrame = null;
  clock = score = pending = 0;
  trickUntil = 0;
  trick = '';
  return true;
}
/* the results' Transport (0x20CF88 -> 0x2706F0): the auto replay stops and the results time comes back, before the stop frame's tick */
function inWorldResultsRestore() {
  if (!inWorldResults) return;
  replay?.stop();
  snapshotRestore(SNAPSHOT_RESULTS, inWorldResults, { human: core, racers: inWorldRacers() });
  inWorldResults = null;
  state = new Float32Array(core.HEAPF32.buffer, core._rider_state(), 16).slice();
  lastRescues = state[13];
  cameraPose = previousCameraPose = null;
  currentRiderFrame = previousRiderFrame = null;
}
let replayTriggersCore = null,
  replayTriggersCode = null;
replay = createRaceReplay();
// QA: the run's replay (web/replay.js)
if (new URL(location.href).searchParams.has('qa')) window.__replay = replay;
// the results' Replay item: the full replay under its overlay (web/replay-ui.js); Exit replay goes back to the results, whose replay starts
// again
let replayBack = null;
if (replay) {
  ui.replayUi = new ReplayUi(ui, replay, {
    exit: () => {
      if (ui.onlineWatch?.exit()) return;
      // pv onlineRecords: Watch Replay goes back to its board
      const back = replayBack || 'ctm-results';
      replayBack = null;
      replay.stop();
      ui.set(back);
      ui.index = 2;
      ui.sync();
      replay.start('auto');
    }
  });
  ui.replayUi.load();
  ui.cb.replayAvailable = () => !!replay?.available() && !!ui.replayUi?.ready;
  ui.cb.replay = () => {
    if (!ui.cb.replayAvailable()) return false;
    replayBack = ui.screen;
    replay.stop();
    running = false;
    if (!replay.start('full')) return false;
    ui.replayUi.open();
    ui.set('replay');
    ui.sync();
    return true;
  };
}
// pv onlineRecords (web/online-replay.js): the run's replay file for the online boards, Watch Replay
if (pv('onlineRecords'))
  installOnlineReplay({
    ui,
    replay,
    core: () => core,
    course: () => course,
    courses: () => courses,
    rider: () => selectedRider,
    setRider: (r) => {
      selectedRider = r;
      pendingRider = sameRider(r, loadedRider) ? null : r;
    },
    aiRace: () => aiRace,
    inWorld: () => inWorldReplay(),
    careerId: () => careerId(),
    navigate: (url, o) => navigateCourse(url, o),
    stopLive: () => {
      running = false;
    },
    audioStart: () => {
      audioFinished = false;
      audioPhase = null;
      gameAudio
        .runStart({
          courseIndex: COURSE_INDEX[course.code] ?? 0,
          singleEvent: true,
          freeRide: null,
          courseCode: course.code,
          character: selectedRider?.id
        })
        .catch((e) => console.warn('Replay music failed', e));
    },
    get coreUrl() {
      return coreWasmUrl;
    }
  });
function replayWorld() {
  return !!replay?.active && REPLAY_SCREENS.includes(ui.screen);
}
/* per frame: start the auto replay when the run's results are up (after the podium), stop it when the post-race screens are left */
function replayFrame(dt) {
  if (!replay) return;
  if (replay.active && (!REPLAY_SCREENS.includes(ui.screen) || cutscenes?.active)) {
    replay.stop();
    return;
  }
  if (!replay.active && replay.autoPending && REPLAY_AUTO_SCREENS.includes(ui.screen) && !cutscenes?.active && replay.available()) {
    running = false;
    // the live post-finish ticks end: the replay replaces them
    replay.start('auto');
  }
  if (replay.active && ui.screen === 'replay' && ui.replayUi) {
    ui.replayUi.frame();
    if (ui.replayUi.circle) ui.replayUi.held(++replayCircle);
    else replayCircle = 0;
  }
  if (replay.active) {
    replay.frame(Math.min(dt, quality.maxFrameDt));
    // the replay's own tick clock places the frame between its ticks
    acc = replay.pending;
    flushHumanSkin();
  }
}
let replayCircle = 0;
/* An audio failure (e.g. a song the decoder cannot read) must not stop the frame or the simulation: logged once per kind. */
const audioFailures = new Set();
function audioSafe(f) {
  try {
    f();
  } catch (e) {
    const k = String(e?.message || e);
    if (!audioFailures.has(k)) {
      audioFailures.add(k);
      console.warn('Game audio failed (the game goes on)', e);
    }
  }
}
function audioScreens() {
  if (ui.screen === audioScreen) return;
  const from = audioScreen;
  audioScreen = ui.screen;
  if (audioScreen === 'loading') gameAudio.loadingStart({ courseCode: course?.code, character: selectedRider?.id });
  else if (from === 'loading' && !audioMenuAfterLoad && course?.freeRide && freeRide && gameAudio.freeWorldLoaded)
    // a streamed world is up: its world load 2867E8 + 234F40 now, before world state 10's arrival list (the new career's
    // plane intro plays pktrans under the ABC1 movie, web/game-audio.js freeWorldLoaded)
    gameAudio
      .freeWorldLoaded({
        courseIndex: freeRide.course() >= 0 ? freeRide.course() : course.freeRide.course,
        courseCode: course.code,
        character: selectedRider?.id,
        freeRide: { kind: course.freeRide.kind, mode: course.freeRide.mode }
      })
      .catch((e) => console.warn('World music failed', e));
  else if (from === 'loading' && !audioMenuAfterLoad)
    // 28FA98 loop off + world load 2867E8: song in its idle section
    gameAudio
      .worldLoaded({
        courseIndex: COURSE_INDEX[course.code] ?? 0,
        singleEvent: !ui.careerMode,
        courseCode: course.code,
        character: selectedRider?.id
      })
      .catch((e) => console.warn('Race music failed', e));
  else gameAudio.screen(audioScreen);
  audioMenuAfterLoad = false;
}
// No course loaded (in-app course switch): the loading screen (or a menu) is drawn, audio follows the screens, nothing touches the old
// world.
/* ---- riderPrefetch: the event's riders download before their loaders ask (docs/first-load.md) --------------------------------
   The human rider's package when the event is picked (alongside a course still loading behind the load screen), the lineup's
   packages as soon as it is planned (before the human rider loads, alongside it), the intro's cutscene data under the warm-up.
   downloads.js keeps each prefetched body once, until its loader takes it (RIDER_KEEP_MS at most); a texture archive is kept by
   texture-archive.js. Nothing is decoded or built early. */
const RIDER_KEEP_MS = 180000,
  riderPrefetched = new Map();
function prefetchRiderPackage(root, human = false) {
  // a generated outfit package (memory)
  if (!root || wardrobeFile(root + 'world.json') !== undefined) return;
  const last = riderPrefetched.get(root);
  if (last && performance.now() - last < RIDER_KEEP_MS) return;
  riderPrefetched.set(root, performance.now());
  const keep = { keepMs: RIDER_KEEP_MS };
  for (const f of ['vertices.bin', 'indices.bin', 'colors.bin', 'rider.json']) prefetchDownload(root + f, keep);
  if (human) prefetchDownload(riderSamplesUrl(root), keep);
  prefetchDownload(root + 'world.json', keep)
    .then((ok) => (ok ? peekDownload(root + 'world.json') : null))
    .then((bytes) => {
      if (!bytes) return;
      const d = JSON.parse(new TextDecoder().decode(bytes));
      for (const [k, t] of Object.entries(d.textures || {}))
        if (t.pack !== undefined && !k.startsWith('10-')) prefetchTextureArchive(textureSource(root, t).archive);
    })
    .catch(() => {});
}
/* the chosen rider (ensureRider's package, loadCharacter's settings) */
function prefetchEventRider() {
  const r = pendingRider ?? selectedRider;
  if (!r?.package) return;
  prefetchRiderPackage(r.root || '/assets/' + r.package + '/', true);
  for (const pkg of new Set([r.package, r.kind === 'cheat' && r.base ? ui.riders.find((x) => x.id === r.base)?.package : null]))
    if (pkg && ui.riders.find((x) => x.package === pkg)?.settings)
      prefetchDownload('/assets/' + pkg + '/settings.json', { keepMs: RIDER_KEEP_MS });
}
/* the event's computer riders (web/ai-race.js plan: the lineup prepare() installs) */
function prefetchLineup(ev) {
  if (!aiRace || !(mpGame?.aiAllowed() ?? true)) return;
  let pk = [];
  try {
    pk = aiRace.plan?.({ rider: selectedRider, event: ev }) ?? [];
  } catch (e) {
    console.warn('Lineup plan failed', e);
  }
  for (const p of new Set(pk)) prefetchRiderPackage('/assets/' + p + '/');
}
/* the event intro's scripts and animation banks (web/cutscenes.js prepareData), under the warm-up */
function prefetchIntro() {
  if (!cutscenes?.prepareData || course?.freeRide || mpGame?.racing || ui.mpUI?.racing) return;
  cutscenes
    .prepareData({
      steps: cutscenes.introSteps(ui.careerUI?.active?.career ? 'career' : 'single'),
      cast: cutsceneCast(),
      location: course.code
    })
    .catch(() => {});
}
const eventOf = () =>
  // Single Event freestyle: its roster opponent (career.js)
  ui.careerMode || course?.event === 'backcountry' || ui.careerUI?.active ? ui.careerUI?.career?.active?.ev : null;
function idleFrame(ms) {
  last = ms;
  audioSafe(() => {
    audioScreens();
    gameAudio.listener(camera, origin);
  });
  if (!ui.loading.active) {
    ui.draw(ui.lastState || {});
    drawStreamingNote(ui);
  }
}
/* ---- Lazy first course (pv lazyCourse, docs/first-load.md "Title before the course") ------------------------------------------
   The title and the menus start with the front end only (its screens, fonts, the core compile, the GPU): "Press START" does not
   wait for a course. The page's course (Snow Jam, or ?course=) loads behind the menus from Press START (startLazyCourse: a job on
   the course-switch chain, backgroundCourse), or from the first thing that needs it (an event load's warm-up, an online start,
   ?qa=1); a request for another course abandons it at its next step, so the chosen course loads first. The event load screen
   covers what is left, its percentage held to the work done (web/boot-screen.js courseProgress, loading-screen.js workFraction).
   ?course=...&autostart=1 loads as before. feStage: the front-end rider previews until a course is live, with what they need
   loaded first after Press START (the FE clips and samples) and a core of their own for the preview lighting (0x19EE88:
   shade_rider_lighting needs a light world), so the course's core is untouched until its own load. */
let lazyBoot = null,
  feStage = null,
  backgroundJob = null,
  courseLoadGuard = null;
const lazyPending = () => !live && !!lazyBoot;
const lazyKey = () => (backgroundJob && !backgroundJob.abandoned ? backgroundJob.key : lazyBoot && !lazyBoot.started ? lazyBoot.key : null);
// the FE previews are not part of a course (its unload)
const feOwned = (o) => o.name === 'fe-preview' || o === feStage?.anchor;
const FE_RIG = { bones: [] };
const yieldToEvents = () =>
  new Promise((r) => {
    const c = new MessageChannel();
    c.port1.onmessage = () => {
      c.port1.close();
      r();
    };
    c.port2.postMessage(0);
  });
// the world build behind the menus: 4 ms slices, input in between
const courseLoadYield = async () => {
  courseLoadGuard?.();
  await yieldToEvents();
};
function guardedLoad(p, t) {
  courseLoadGuard?.();
  const g = courseLoadGuard;
  return load(p, t).then((v) => {
    g?.();
    return v;
  });
}
function lazyStart(first, params) {
  course = first;
  ui.course = first;
  lazyBoot = { started: false, key: courseKeyOf(params) };
  feStage = { ready: false, core: null, promise: null, anchor: new T.Object3D() };
  feStage.anchor.name = 'fe-anchor';
  scene.add(feStage.anchor);
  ui.loading.workFraction = () => {
    const p = globalThis.ssxBoot?.courseProgress;
    return backgroundJob && !backgroundJob.abandoned && p ? p.target().fraction : null;
  };
  // QA: the course loads at once; ssxQA once it is live (publishQA)
  if (params.has('qa')) startLazyCourse();
}
function startLazyCourse() {
  if (!lazyBoot || lazyBoot.started) return;
  lazyBoot.started = true;
  if (live || switchTarget) return;
  // another course was asked for first
  navigateCourse(new URL(location.href), { history: 'none', after: 'background' });
}
/* resolves once the course switches asked for so far are done (the lazy first course starts now if it has not) */
function courseLive() {
  startLazyCourse();
  const wait = () => {
    const c = switchChain;
    return c.then(() => (c === switchChain ? undefined : wait()));
  };
  return wait();
}
function publishQA(qa) {
  if (!lazyBoot) {
    window.ssxQA = qa;
    return;
  }
  courseLive().then(() => {
    if (live) window.ssxQA = qa;
  });
}
/* the previews' needs: the FE clips (ANIMATIONS/library.json) and samples (the course reuses both) and the preview core */
function loadFrontEndStage() {
  const st = feStage;
  if (!st) return Promise.resolve();
  return (st.promise ??= (async () => {
    const [lib, samples, c] = await Promise.all([
      libraryClips ? null : load('/assets/ANIMATIONS/library.json'),
      allSamples ? null : load('/assets/ANIMATIONS/samples.f32', 'buffer'),
      newCore()
    ]);
    libraryClips ??= lib.clips;
    allSamples ??= new Float32Array(samples);
    const [lights, tree] = await Promise.all([load(course.root + 'local-lights.json'), load(course.root + 'light-tree.json')]),
      put = (x) => {
        const b = new TextEncoder().encode(JSON.stringify(x) + '\0'),
          p = c._malloc(b.length);
        c.HEAPU8.set(b, p);
        return p;
      },
      a = put(lights),
      b = put(tree);
    try {
      c._init_rider_lighting(a, b);
    } finally {
      c._free(a);
      c._free(b);
    }
    if (feStage === st) {
      st.core = c;
      st.ready = true;
    }
  })().catch((e) => console.warn('Front-end previews unavailable before the course', e)));
}
/* the first live frame: the previews light through the course's core from here (the preview core is dropped) */
function endFrontEndStage() {
  const st = feStage;
  feStage = null;
  lazyBoot = null;
  if (!st) return;
  st.anchor.removeFromParent();
  if (st.core) {
    releasePreviewCores();
    for (const o of [ui.characterSelect, ui.equipGear]) if (o && o.core === st.core) o.core = null;
  }
}
/* a menu frame without a course: idleFrame plus the FE rider previews (as frame() draws them with a course) */
function feFrame(ms) {
  const dt = last ? Math.min(0.25, Math.max(0, (ms - last) / 1000)) : 0;
  last = ms;
  const st = feStage;
  // Press START (or an invite link's lobby)
  if (lazyBoot && !lazyBoot.started && (ui.screen !== 'title' || ui.titleOut)) startLazyCourse();
  audioSafe(() => {
    audioScreens();
    gameAudio.listener(camera, origin);
  });
  /* under the event load screen (it draws itself): an empty frame of the hidden scene all the same. WebKit (macOS 27) kept ~12 MB/s of
    the page's memory while a course loaded under the load screen with no WebGPU frame presented (+0.8-1.2 GB by a 6 Mbit/s race
    start, not given back); a frame each animation frame keeps it as when the menus draw (measured, docs/first-load.md) */
  if (ui.loading.active) {
    feRender(false);
    return;
  }
  const riderScreen =
    ['character', 'setup', 'details'].includes(ui.screen) || !!ui.equipGear?.owns(ui.screen) || !!ui.feScreens?.ownsRider?.(ui.screen);
  // its package loads meanwhile; nothing shows until it can be posed and lit (the original shows nothing while it loads)
  if (st.ready) {
    try {
      const fer = frontEndRider();
      if (riderScreen) {
        fer?.pose(T, [], FE_RIG, dt, { clips: libraryClips, samples: allSamples, scale: 1, core: st.core });
        fer?.place(T, st.anchor, camera);
      }
      fer?.showPreview?.(ui.screen, false);
    } catch (e) {
      st.ready = false;
      console.warn('Front-end preview failed', e);
    }
  } else if (riderScreen) {
    const cs = ui.characterSelect;
    try {
      cs?.preview3d.want(T, cs.previewEntry());
      cs?.preview3d.show(false);
    } catch {}
  }
  ui.draw(ui.lastState || {});
  // the course streaming behind the menus is not something they wait for
  if (!backgroundJob) drawStreamingNote(ui);
  // the preview over the menus' background
  feRender(true);
}
/* the scene with what the course load has put in it so far hidden (preview: the FE rider preview drawn, else nothing) */
function feRender(preview) {
  scene.background = null;
  const hidden = [];
  for (const o of scene.children)
    if (o.visible && !(preview && o.name === 'fe-preview')) {
      o.visible = false;
      hidden.push(o);
    }
  try {
    renderer.render(scene, camera);
  } finally {
    for (const o of hidden) o.visible = true;
  }
}
/* the lazy first course: switchCourse without the load screen, the menus live meanwhile */
async function backgroundCourse(job) {
  const next = courseFor(job.url.searchParams),
    t0 = performance.now();
  backgroundJob = job;
  switchBusy = true;
  const alive = () => {
    if (job.abandoned) throw Object.assign(Error('Course load abandoned for another course'), { abandoned: true });
  };
  try {
    if (!next) throw Error('Unknown or unprepared course ' + (job.url.searchParams.get('course') || 'ARA1'));
    await Promise.race([loadFrontEndStage(), job.abandonedAt]);
    alive();
    courseGen++;
    live = false;
    course = next;
    ui.course = next;
    courseLoadGuard = alive;
    job.started = true;
    await loadCourse(next);
    loadedKey = job.key;
    loadedRider = { ...bootRider(), id: '' };
    // as switchCourse: the chosen rider loads with the event
    pendingRider = selectedRider;
    performance.mark('course:live');
    const rec = {
      code: next.code,
      key: job.key,
      after: job.after,
      background: true,
      loadMs: Math.round(performance.now() - t0),
      atMs: Math.round(performance.now()),
      memory: { ...renderer.info.memory },
      sceneChildren: scene.children.length
    };
    window.__courseLoads.push(rec);
    diagnose('course', rec);
    console.info(`Course ${job.key} behind the menus: ${rec.loadMs} ms`, rec);
  } catch (e) {
    courseLoadGuard = null;
    live = false;
    // what it built goes with the next course's unload
    if (job.started && !courseRoots.length && loadBefore) courseRoots = scene.children.filter((o) => !loadBefore.has(o) && !feOwned(o));
    if (job.abandoned) return;
    if (switchTarget === job) switchTarget = null;
    const detail = e instanceof Error ? e.message : String(e);
    console.error('Course load failed', e);
    document.body.dataset.loadError = detail;
    diagnose('course-failed', { key: job.key, background: true, message: detail });

    // as a failed first load: the title card says so
    ui.error = e?.network ? 'Connection lost. Reload the page' : 'Load failed';
    ui.ready = false;
    ui.loading.cancel?.();
    ui.set('title');
    ui.draw({});
    return;
  } finally {
    courseLoadGuard = null;
    switchBusy = false;
    if (backgroundJob === job) backgroundJob = null;
    globalThis.ssxBoot?.courseDone?.();
  }
  if (switchTarget !== job) return;
  switchTarget = null;
  // an event, a lobby or a menu asked for meanwhile
  if (job.after !== 'background') afterSwitch(job);
}
let humanSkinReset = false,
  humanSkinPending = false;
function flushHumanSkin() {
  if (!humanSkinPending) return;
  humanSkinPending = false;
  if (sam?.userData.sourceSkin && core) {
    sam.userData.sourceSkin.capture(core, humanSkinReset);
    humanSkinReset = false;
  }
}
// web/presentation_fast.hpp: the low tier builds skin palettes and snow sprites with plain float arithmetic (presentation only, every rider
// context; web/quality.js)
function applyPresentationFast() {
  // the air streamers at the render matrices' scale (web/boost_gameplay.inc set_rider_fx_render_scale)
  const on = quality.presentationFast ? 1 : 0,
    fxScale = 1;
  core?._set_presentation_fast?.(on);
  core?._set_rider_fx_render_scale?.(fxScale);
  const booth = 1;
  const spray = 1;
  // the FX reset of a placement zeroes the kicker buildup (web/animation_bridge.cpp set_fx_reset_kicker)
  core?._set_fx_reset_kicker?.(spray);
  // stage builtin 34, the Metro-City phone booths / water towers (web/stage_teleport.inc)
  core?._stage_teleport_enable?.(booth);
  for (const n of aiRace?.racers?.npcs ?? []) {
    n.core._set_presentation_fast?.(on);
    n.core._set_rider_fx_render_scale?.(fxScale);
    n.core._stage_teleport_enable?.(booth);
    n.core._set_fx_reset_kicker?.(spray);
  }
}
function frame(ms) {
  if (!live) {
    if (feStage) feFrame(ms);
    else idleFrame(ms);
    return;
  }
  if (feStage) endFrontEndStage();
  const dt = last ? Math.max(0, (ms - last) / 1000) : 0;
  last = ms;
  fps += (1 / Math.max(dt, 0.001) - fps) * 0.04;
  if (ready) {
    applyPresentationFast();
    const steerSnap = [new Map(keyboardPad.steering ?? []), keyboardPad.serial];
    let input = inputs();
    const startDown = input[1] > 0;
    // the pad's Start opens the pause from the ride only (0x230A34); in a menu it is the menu's accept
    // (web/gamepad-menus.js)
    if (
      // a Start a menu accepted is spent
      startDown &&
      !startButtonHeld &&
      !padMenus?.taken?.('start') &&
      frameScreen === 'game' &&
      startOpensPause({ screen: ui.screen, running, paused: isPaused(), finished, cutscene: !!cutscenes?.active || transporting })
    )
      overlay.open();
    frameScreen = ui.screen;
    nisWatch();
    // no Start pause during a cutscene (0x231AB8: no shipped NIS sets flag 4)
    startButtonHeld = startDown;
    // online races keep running behind the pause menu (neutral pad) and follow the server race clock (web/net/mp-game.js)
    const online = !!mpGame?.simulating(),
      simHeld = contexts.simFrame();
    // a pop runs the tick from the next frame (E11)
    if (running && (!simHeld || online) && !mpGame?.holding()) {
      // pv padCarry: the finished ride reads the pad as the PS2's (its control 10 ignores it; the finish tick's control 0 still crouches on
      // a held Cross: c0a-ret2 1700)
      const tickInput = (finished && !pv('padCarry')) || simHeld ? NEUTRAL_PAD : input;
      gameTick.advance(
        simulation,
        // offline, the PS2 frame loop's pacing (web/ps2-frame-pacing.js): at most 12 updates a drawn frame and the backlog
        // dropped, not replayed at 12 a frame (a Safari stall mid-rotation fast-forwarded the air auto-complete) low tier: a frame slower
        // than 4 ticks drops the excess (slow motion, never a catch-up spiral); web/quality.js
        online ? mpGame.pace(dt, simulation.pending) : Math.min(ps2FrameTime(dt), quality.maxFrameDt),
        stallKeyInput(tickInput, ms, dt, steerSnap, online || finished || simHeld)
      );
      acc = simulation.pending;
      flushHumanSkin();
      padCarryAcc = 0;
    } else {
      keyBase = new Set(keys);
      keyLog.length = 0;
      padCarryFeed(input, dt);
    }
    mpGame?.afterFrame();
    replayFrame(dt);

    // audio: front-end screen themes, loading loop, GO, finish (docs/audio-logic.md 3.7)
    audioSafe(() => {
      audioScreens();
      if (running) {
        const phase = raceInfo ? raceInfo[5] : null;
        if (audioPhase === 4 && phase !== 4 && phase != null) gameAudio.go();
        // rival challenge rolling start (2872A8, docs/backcountry.md)
        if (audioPhase === 3 && phase === 5 && course.event === 'backcountry')
          gameAudio.rivalStart?.({ rival: aiRace?.lineup?.rival ?? aiRace?.racers?.npcs?.[0]?.character ?? 'mac', mp: !!mpGame });
        audioPhase = phase;
        if (finished && !audioFinished) {
          audioFinished = true;
          // (place: a freestyle run's 0x536730 from 238B70's ranking, career-ui.js freestyleFinishPlace; races take the AI standings'
          // place)
          gameAudio.finish({
            place: aiActive ? (aiRace.hud()?.place ?? 0) : 0,
            timedOut: !!core._race_timed_out?.(),
            ai: aiActive ? aiRace : null,
            challengeKind: course.event === 'backcountry' ? (ui.careerUI?.career?.active?.ev?.mode === 5 ? 6 : 5) : 0,
            // pv postEventDj: 2A45C0's KOs (+0x128) and Ubers (+0x114) of the human's score object
            stats:
              pv('postEventDj') && core._score_object_dump
                ? ((w) => ({
                    ko: w[0x128 / 4] | 0,
                    ubers: w[0x114 / 4] | 0,
                    place: ui.careerUI?.freestyleFinishPlace?.(core._race_timed_out?.() ? 0 : w[0x198 / 4] | 0) ?? null
                  }))(new Uint32Array(core.HEAPU8.buffer, core._score_object_dump(), 0x1d0 / 4))
                : null
          });
        }
        if (!isPaused()) gameAudio.update(1);
      }
      gameAudio.listener(camera, origin);
    });
    if (['character', 'setup', 'details'].includes(ui.screen)) camera.setViewOffset(640, 448, 190, 0, 640, 448);
    else camera.clearViewOffset();
    const pos = new T.Vector3(state[0], state[1], state[2]).sub(origin),
      grounded = !!state[8];
    sam.position.copy(pos);
    const normal = grounded ? new T.Vector3(state[4], state[5], state[6]) : up;
    const forward = new T.Vector3(Math.sin(state[3]), 0, Math.cos(state[3]));
    forward.addScaledVector(normal, -forward.dot(normal)).normalize();
    const right = new T.Vector3().crossVectors(normal, forward).normalize();
    const renderAlpha = T.MathUtils.clamp(acc * 60, 0, 1);
    if (
      currentRiderFrame &&
      (readyScreen() ||
        ['game', 'pause', 'results', 'ctm-pause', 'ctm-giveup', 'ctm-results', 'ctm-award', 'ctm-records'].includes(ui.screen) ||
        replayWorld() ||
        !!bigChallenges?.owns(ui.screen))
    ) {
      applyRiderFrame(sam, bones, previousRiderFrame, currentRiderFrame, renderAlpha);
      activeClip = 'semantic:' + animInfo[0];
    } else if (
      // the lodge's Ubertrick Setup and Equip Gear animate their preview over the paused free ride
      !isPaused() ||
      !!ui.feScreens?.ownsRider?.(ui.screen) ||
      !!ui.equipGear?.owns(ui.screen)
    ) {
      sam.quaternion.setFromRotationMatrix(new T.Matrix4().makeBasis(right.clone().negate(), normal, forward.clone().negate()));
      if (!frontEndRider()?.pose(T, bones, rig, dt, { clips, samples: allSamples, scale: sam.userData.riderScale, core }))
        animateRider('FL_STAND_CYC', dt);
    }

    // the card left without Continue: the ready riders hide
    if (aiActive || (readyAi && readyScreen())) aiRace.update(renderAlpha);
    else if (readyAi) {
      readyAi = false;
      aiRace?.renderer?.reset(aiRace.opponents);
    }
    mpGame?.render(renderAlpha);
    const sourceSkinActive =
      sam.userData.sourceSkin?.display(
        !!currentRiderFrame &&
          (['game', 'pause', 'results', 'ctm-pause', 'ctm-giveup', 'ctm-results', 'ctm-award', 'ctm-records'].includes(ui.screen) ||
            replayWorld() ||
            !!bigChallenges?.owns(ui.screen) ||
            readyScreen()),
        renderAlpha
      ) || false;
    const skinningMode = sourceSkinActive ? selectedRider.id : 'legacy';
    if (document.body.dataset.sourceSkinning !== skinningMode) document.body.dataset.sourceSkinning = skinningMode;
    // a backcountry heli hovering on after its arrival (web/cutscenes.js)
    // web/cutscenes.js: its camera and actors replace the chase camera and the race rider
    const cutscenePose = cutscenes?.active
      ? cutscenes.update(Math.min(dt, 0.25))
      : (cutscenes?.linger?.(isPaused() ? 0 : Math.min(dt, 0.25)), null);
    const originalCameraActive =
      cameraPose &&
      (readyScreen() ||
        ['game', 'pause', 'results', 'ctm-pause', 'ctm-giveup', 'ctm-results', 'ctm-award', 'ctm-records'].includes(ui.screen) ||
        replayWorld() ||
        !!bigChallenges?.owns(ui.screen));
    if (!originalCameraActive) {
      camera.fov = ['character', 'setup', 'details'].includes(ui.screen) ? 45 : 66;
      camera.updateProjectionMatrix();
      const desired = pos
        .clone()
        .addScaledVector(forward, -4.8)
        .add(new T.Vector3(0, 2.5, 0));
      if (!running && clock === 0)
        desired
          .copy(pos)
          .addScaledVector(forward, 3.5)
          .addScaledVector(right, 1.4)
          .add(new T.Vector3(0, 1.1, 0));
      const groundCam = core._height_at(desired.x + origin.x, desired.y + origin.y + 4, desired.z + origin.z) - origin.y;
      if (groundCam > -1e8) desired.y = Math.max(desired.y, groundCam + 1.2);
      if (lastCamera.lengthSq() === 0) lastCamera.copy(desired);
      lastCamera.lerp(desired, 1 - Math.exp(-dt * 12));
      const floor = core._height_at(lastCamera.x + origin.x, lastCamera.y + origin.y + 25, lastCamera.z + origin.z) - origin.y;
      if (floor > -1e8) lastCamera.y = Math.max(lastCamera.y, floor + 1.6);
      camera.position.copy(lastCamera);
      if (sky) sky.position.copy(camera.position).add(origin);
      camera.lookAt(
        pos
          .clone()
          .addScaledVector(forward, running ? 1 : 0)
          .add(new T.Vector3(0, 1.1, 0))
      );
    }
    if (originalCameraActive) {
      const fogValues = new Float32Array(core.HEAPF32.buffer, core._fog_info(), 11);
      const gf = scene.fog || scene.userData.genericFog;
      if (gf && fogValues[10] && fogValues[7] > 0) {
        gf.near = fogValues[1] / 100;
        gf.far = fogValues[2] / 100;
        gf.color.setRGB(fogValues[3], fogValues[4], fogValues[5]);
      }
      const prior = previousCameraPose || cameraPose,
        a = renderAlpha,
        at = (i) => T.MathUtils.lerp(prior[i], cameraPose[i], a);
      camera.position.set(at(0) - origin.x, at(1) - origin.y, at(2) - origin.z);
      // 376C58: the source FOV is the half horizontal angle of a 4:3 view (512x448 GS scale 272.65/318.09 px), so the
      camera.lookAt(at(3) - origin.x, at(4) - origin.y, at(5) - origin.z);
      // vertical half-angle tangent is tan(fov)*3/4 (86.4 x 70.3 degrees for DEFAULT_3), not a full vertical angle.
      camera.fov = (2 * Math.atan(Math.tan(at(6)) * 0.75) * 180) / Math.PI;
      camera.near = Math.max(0.05, at(7));
      camera.far = at(8);
      camera.updateProjectionMatrix();
      if (sky) sky.position.copy(camera.position).add(origin);
    }
    if (cutscenePose) {
      camera.clearViewOffset();
      cutscenes.applyCamera(camera, cutscenePose);
      if (sky) sky.position.copy(camera.position).add(origin);
    }
    sam.scale.setScalar(1);
    if (
      (['character', 'setup', 'details'].includes(ui.screen) || ui.equipGear?.owns(ui.screen) || ui.feScreens?.ownsRider?.(ui.screen)) &&
      !frontEndRider()?.place(T, sam, camera)
    ) {
      sam.position.set(0, 0, 0);
      sam.quaternion.setFromAxisAngle(up, -Math.PI / 2);
      camera.position.set(0, 1.05, 4.2);
      camera.lookAt(0, 1, 0);
    }
    for (const mesh of pickupMeshes) mesh.visible = !!core._pickup_visible(mesh.userData.pickupResource);
    updateMovingInstances(core, movingMeshes, origin);
    setPieceRenderer?.update();
    freeRide?.update();
    if (boardTrail) boardTrail.update(core);
    if (wakeRenderer) wakeRenderer.update(core);
    if (boostRenderer) boostRenderer.update(core);
    if (snowRenderer) snowRenderer.update(core, camera);
    if (weatherRenderer) weatherRenderer.update(camera);
    // a cutscene camera: the Surface painter steps at the drawn camera (web/terrain-sparkle.js)
    terrainSparkle?.update(camera, cutscenePose ? null : core);
    // Public diagnostics for local QA, no guest runtime or private browser state.
    const boostDiagnostics = new Float32Array(core.HEAPF32.buffer, core._boost_info(), 8);
    const predictionDiagnostics = new Float32Array(core.HEAPF32.buffer, core._prediction_info(), 7);
    const bodyQuery = new Float32Array(core.HEAPF32.buffer, core._body_query_info(), 14);
    const bodyResponse = new Float32Array(core.HEAPF32.buffer, core._body_response_info(), 11);
    const collisionReaction = new Float32Array(core.HEAPF32.buffer, core._collision_reaction_info(), 10);
    const crashDiagnostics = new Float32Array(core.HEAPF32.buffer, core._crash_info(), 12);
    const resetDiagnostics = new Float32Array(core.HEAPF32.buffer, core._reset_info(), 9);
    const trackDiagnostics = new Float32Array(core.HEAPF32.buffer, core._trail_info(), 6),
      lightingDiagnostics = new Float32Array(core.HEAPF32.buffer, core._environment_info(), 7);
    const snowDiagnostics = new Float32Array(core.HEAPF32.buffer, core._snow_info(), 23);
    const wakeDiagnostics = new Float32Array(core.HEAPF32.buffer, core._wake_info(), 12);
    const railDiagnostics = new Float32Array(core.HEAPF32.buffer, core._rail_gameplay_info(), 8);
    const railRotationDiagnostics = new Float32Array(core.HEAPF32.buffer, core._rail_rotation_info(), 4);
    const boostHud = new Float32Array(core.HEAPF32.buffer, core._boost_hud_info(), 13),
      boostPrepass = hudPrepass?.boost ?? boostHud;
    window.demoState = {
      boostFlashPaletteTier: boostPrepass[12],
      boostPendingCount: boostHud[5],
      boostPendingPhase: boostHud[6],
      boostFlashPhase: boostPrepass[8],
      boostLetterCount: boostHud[2],
      boostLetterFraction: boostHud[3],
      boostLetterRemoved: !!boostHud[4],
      boostHudPreview: boostHud[0],
      boostHudStored: boostHud[1],
      railRotations: railRotationDiagnostics[0],
      railStyle: railRotationDiagnostics[1],
      railActive: !!railDiagnostics[0],
      railAttachments: railDiagnostics[2],
      railExits: railDiagnostics[3],
      railFrames: railDiagnostics[4],
      railBalance: railDiagnostics[5],
      railId: railDiagnostics[6],
      wakeVertices: wakeDiagnostics[11],
      wakeRows: wakeDiagnostics[1],
      wakeActive: !!wakeDiagnostics[3],
      wakeVelocity: Array.from(wakeDiagnostics.slice(7, 10)),
      snowParticles: Array.from(snowDiagnostics.slice(0, 10)),
      snowWakeUnavailable: !!snowDiagnostics[21],
      trailVertices: trackDiagnostics[0],
      trailSamples: trackDiagnostics[4],
      trailLighting: Array.from(lightingDiagnostics.slice(3)),
      trailLightingGap: !!lightingDiagnostics[1],
      resetFadeAlpha: resetDiagnostics[8],
      resetting: !!resetDiagnostics[0],
      resetProgress: resetDiagnostics[1],
      resetPlacements: resetDiagnostics[2],
      resetCompletions: resetDiagnostics[3],
      resetPath: resetDiagnostics[5],
      crashing: !!crashDiagnostics[0],
      crashPhase: crashDiagnostics[1],
      crashMotion: crashDiagnostics[2],
      crashAnimation: crashDiagnostics[3],
      crashTicks: crashDiagnostics[4],
      crashDetached: !!crashDiagnostics[6],
      crashRecovery: crashDiagnostics[7],
      crashResetReason: crashDiagnostics[8],
      collisionSerial: collisionReaction[0],
      collisionReactionKind: collisionReaction[1],
      collisionReactionAnimation: collisionReaction[2],
      collisionRecoveryRequested: !!collisionReaction[9],
      bodyResponseAccepted: !!bodyResponse[0],
      bodyBounced: !!bodyResponse[2],
      bodyQueryAvailable: !!bodyQuery[0],
      bodyQueryComplete: !!bodyQuery[1],
      bodyContact: !!bodyQuery[2],
      predictionAvailable: !!predictionDiagnostics[0],
      predictionStatus: predictionDiagnostics[1],
      predictedLandingSeconds: predictionDiagnostics[4],
      predictionFailed: !!predictionDiagnostics[6],
      boostMeter: boostDiagnostics[0],
      boostAmount: boostDiagnostics[1],
      boostTier: boostDiagnostics[3],
      ready,
      running,
      paused: isPaused(),
      seconds: clock,
      simulationDebt: simulation.pending,
      score,
      pending,
      grounded,
      position: pos.toArray(),
      riderVisualPosition: sam.position.toArray(),
      cameraPosition: camera.position.toArray(),
      speed: state[7],
      charge: state[9],
      respawns: state[13],
      clip: activeClip,
      controlState: animInfo?.[15] ?? 0,
      fps: Math.round(fps),
      draws: renderer.info.render.drawCalls,
      triangles: renderer.info.render.triangles,
      backend: renderer.backend.isWebGPUBackend ? 'WebGPU' : 'WebGL2'
    };
  }
  if (worldScene) {
    // Big Challenge prompt / fail overlays draw over the paused world (web/big-challenges.js)
    const playing =
      readyScreen() ||
      warming ||
      ['game', 'results', 'ctm-results', 'ctm-award', 'ctm-records'].includes(ui.screen) ||
      replayWorld() ||
      !!cutscenes?.active ||
      !!bigChallenges?.overWorld(ui.screen);
    for (const o of scene.children) if (o.userData.gameplayOnly) o.visible = playing;
    // the computer / online riders under a menu (pause, options, popups): hidden like the player's (aiRace.update shows them again) after
    // the gameplay visibility pass: computer riders' trails/wake/spray only while they race
    if (!playing) for (const o of scene.children) if (o.userData.opponentRider) o.visible = false;
    opponentFx?.update(aiRace ? aiRace.racers.npcs.map((n) => n.core) : [], camera, playing && aiActive);
    if (riderIcons) {
      const bones = (aiRace?.racers?.npcs || []).map((n) => {
        const p = n.core._world_pose_bones(),
          m = p ? new Float32Array(n.core.HEAPF32.buffer, p, 1)[0] : 0;
        return m > BEAM_BONE ? Array.from(new Float32Array(n.core.HEAPF32.buffer, p + 4 * (1 + BEAM_BONE * 7), 3)) : null;
      });
      riderIcons.update(camera, bones, playing && aiActive);
    }
    if (rivalBeam) {
      const c = aiRace?.racers?.npcs?.[0]?.core,
        p = c && playing && aiActive ? c._world_pose_bones() : 0,
        n = p ? new Float32Array(c.HEAPF32.buffer, p, 1)[0] : 0;
      rivalBeam.update(
        camera,
        n > BEAM_BONE ? Array.from(new Float32Array(c.HEAPF32.buffer, p + 4 * (1 + BEAM_BONE * 7), 3)) : null,
        n > BEAM_BONE
      );
    }
    worldScene.visible = playing;
    sky.visible = playing;
    // the original FE preview model replaces the race rider there (web/fe-preview.js)
    sam.visible =
      !cutscenes?.active &&
      !frontEndRider()?.showPreview?.(ui.screen, playing) &&
      // in the ride screens the rider draws once a tick has posed it (the PS2 draws its rider after the update that poses
      // it);
      // before, the model drew at its bind pose (a new rider) or FL_STAND_CYC
      !(
        !currentRiderFrame &&
        (readyScreen() ||
          ['game', 'pause', 'results', 'ctm-pause', 'ctm-giveup', 'ctm-results', 'ctm-award', 'ctm-records'].includes(ui.screen) ||
          replayWorld() ||
          !!bigChallenges?.owns(ui.screen))
      ) &&
      (playing ||
        ['character', 'setup', 'details'].includes(ui.screen) ||
        !!ui.equipGear?.owns(ui.screen) ||
        !!ui.feScreens?.ownsRider?.(ui.screen));
    scene.background = null;
  }
  ui.draw({
    racePlace: raceInfo && raceInfo[5] === 5 && !finished ? (aiActive ? aiRace.hud() : (mpGame?.hud() ?? null)) : null,
    boostFlashPaletteTier: window.demoState?.boostFlashPaletteTier ?? 0,
    boostPendingCount: window.demoState?.boostPendingCount ?? 0,
    boostPendingPhase: window.demoState?.boostPendingPhase ?? -1,
    boostFlashPhase: window.demoState?.boostFlashPhase ?? -1,
    boostLetterCount: window.demoState?.boostLetterCount ?? 0,
    boostLetterFraction: window.demoState?.boostLetterFraction ?? 0,
    boostLetterRemoved: window.demoState?.boostLetterRemoved ?? true,
    boostPreview: window.demoState?.boostHudPreview ?? 0,
    boostStored: window.demoState?.boostHudStored ?? 0,
    // rider +0x320 == +0x324
    stanceRegular: core?._rider_stance_info ? new Float32Array(core.HEAPF32.buffer, core._rider_stance_info(), 9)[2] !== 0 : true,
    seconds: raceInfo?.[0] ?? clock,
    raceTicks: core ? new Float32Array(core.HEAPF32.buffer, core._race_result_info(), 6)[4] : 0,
    // the finish time with penalties, as the results (the finish banner's value, 0x536640)
    finishTicks: core ? new Float32Array(core.HEAPF32.buffer, core._race_result_info(), 6)[1] : 0,
    timeLimitTicks: liveTimeLimit(core, 0),
    // GMM+0x78 incl. slope style checkpoint extensions
    countdown: raceInfo?.[5] === 4 ? Math.ceil(raceInfo[6] / 60) : 0,
    ...(() => {
      const hud = running ? trickHudSlots() : null;
      return hud ? { score: hud.total, trickSlots: hud.slots } : { score: score + pending };
    })(),
    speed: state?.[7] || 0,
    trick: pending > 0 || clock < trickUntil ? trick : '',
    progress: raceInfo?.[1] || 0,
    message: finished ? 'FINISH!' : '',
    timedOut: !!core?._race_timed_out?.(),
    finishElapsed: core ? new Float32Array(core.HEAPF32.buffer, core._race_result_info(), 6)[3] : -1
  });
  drawStreamingNote(ui);
  if (core) ui.drawResetFade(new Float32Array(core.HEAPF32.buffer, core._reset_info(), 9)[8]);
  terrainRefinement?.commit();
  // the loading screen is opaque: no hidden-scene draws (and no builds) behind it except the warm-up frames
  if (
    (readyScreen() ||
      warming ||
      ['game', 'results', 'ctm-results', 'ctm-award', 'ctm-records'].includes(ui.screen) ||
      cutscenes?.active ||
      !!bigChallenges?.overWorld(ui.screen)) &&
    fogRenderer
  ) {
    riderShadows ??= createRiderShadows({ renderer, scene, origin });
    riderShadows.update(camera, warming, warmShadowLimit);
    fogRenderer.update(core);
    if (warming) warmView.apply();
    try {
      fogRenderer.render();
    } finally {
      if (warming) warmView.restore();
    }
  } else if (['game', 'results', 'ctm-results', 'ctm-award', 'ctm-records'].includes(ui.screen) && skyScene) {
    renderer.autoClear = true;
    renderer.render(skyScene, camera);
    renderer.autoClear = false;
    renderer.render(scene, camera);
    renderer.autoClear = true;
  } else if (ui.screen !== 'loading') renderer.render(scene, camera);
  if (window.demoState) {
    window.demoState.draws = renderer.info.render.drawCalls;
    window.demoState.triangles = renderer.info.render.triangles;
  }
}
// Race-start warm-up (under the opaque loading screen): three.js builds every node material and the GPU compiles every pipeline the first
// time it is drawn, which froze the first race frames for up to seconds. Compile the whole race scene with view culling off, then draw a
// few real frames through the post passes (fog, sun flare, halos, snow composite, glare) while the loading screen still covers the canvas.
let warming = false,
  warmed = false;
const drawnWaiters = [];
// warm-up steps waiting for a frame the frame gate drew (resolved by the animation loop after frame()) warmSpread: riders
// whose shadow silhouettes draw in a warm frame (web/rider-shadow.js)
let warmShadowLimit = Infinity;
/* The warm frame's draw set, re-applied right before each warm render: the per-frame updates (set-piece particles/halos,
   crowd flashes, snow/wake/boost/trail batches, opponent effects, rival icons) hide or empty whatever has nothing to draw
   yet, and three skips an empty batch, so their materials/pipelines (incl. the encoded-pass render-target variants) would
   otherwise first build mid-race (Safari: 100-400 ms Metal compiles). Empty batches draw one (degenerate) instance or
   triangle for the warm frame; counts are restored right after it. */
const warmView = {
  groups: [],
  drawables: [],
  active: new Set(),
  saved: [],
  apply() {
    for (const o of this.groups) o.visible = true;
    for (const o of this.drawables) o.visible = this.active.has(o);
    for (const o of this.active) {
      if (o.isInstancedMesh && !(o.count > 0)) {
        this.saved.push([o, 'count', o.count]);
        o.count = 1;
      }
      const g = o.geometry;
      if (g) {
        if (g.drawRange && !(g.drawRange.count > 0)) {
          this.saved.push([g.drawRange, 'count', g.drawRange.count]);
          g.drawRange.count = Math.min(3, g.index?.count ?? g.attributes.position?.count ?? 3);
        }
        if (g.isInstancedBufferGeometry && !(g.instanceCount > 0)) {
          this.saved.push([g, 'instanceCount', g.instanceCount]);
          g.instanceCount = 1;
        }
      }
    }
  },
  restore() {
    for (let i = this.saved.length - 1; i >= 0; i--) {
      const [t, k, v] = this.saved[i];
      t[k] = v;
    }
    this.saved.length = 0;
  }
};
// pv worldWarm (docs/course-switch.md "World arrivals warm under the load screen"): the streamed world's start-row compile (free-ride.js
// rewarm), awaited by warmWorld
let worldRewarm = null;
// a core is being built (loadCourse: from its instance to live); the scene's children when the switch began (the held loop's
// draw)
let coreLoading = false,
  acrossBefore = null;
// QA: a core is being built
if (new URL(location.href).searchParams.has('qa'))
  Object.defineProperty(window, '__coreLoading', { get: () => coreLoading, configurable: true });
function renderAcross() {
  const hidden = [];
  if (acrossBefore)
    for (const o of scene.children)
      if (o.visible && !acrossBefore.has(o) && !/^(cutscene|fe-preview)/.test(o.name || '')) {
        o.visible = false;
        hidden.push(o);
      }
  try {
    renderer.render(scene, camera);
  } finally {
    for (const o of hidden) o.visible = true;
  }
}
// skip(o): a subtree the warm-up leaves as it is (pv worldWarm: the streamed locations, compiled by their own rewarm); cap: new material
// variants a slice (the adaptive rate's ceiling)
async function warmupRender(framesDone = null, { skip = null, cap = warmNewMaterials } = {}) {
  if (warmed || !renderer || !worldScene) {
    framesDone?.();
    return;
  }
  warmed = true;
  const gen = courseGen,
    alive = () => {
      if (gen !== courseGen) throw Error('Warm-up abandoned: course changed');
    };
  performance.mark('warm:start');

  // refined terrain patches (terrain-refinement.js) use their own vertex layout: warm one proxy per terrain material
  const terrainProxies = [
    ...(terrainRefinement?.warmProxies?.() ?? []),
    // section-activated flag cloth meshes (set-pieces-renderer.js)
    ...(setPieceRenderer?.warmProxies?.() ?? [])
  ];
  for (const p of terrainProxies) worldScene.add(p);

  // a step ends with a frame the gate drew (30 fps / auto tiers skip animation frames; a slice revealed on a skipped one was
  // never drawn), at most 4 animation frames
  const nextFrame = () =>
    new Promise((resolve) => {
      let n = 0,
        open = true;
      const done = (ms) => {
        if (open) {
          open = false;
          resolve(ms);
        }
      };
      drawnWaiters.push(done);
      const tick = () => {
        if (!open) return;
        if (++n >= 4) done();
        else requestAnimationFrame(tick);
      };
      requestAnimationFrame(tick);
    });
  // pre-order, as traverse
  const each = skip
    ? (root, f) => {
        const st = [root];
        while (st.length) {
          const o = st.pop();
          if (o !== root && skip(o)) continue;
          f(o);
          for (let k = o.children.length - 1; k >= 0; k--) st.push(o.children[k]);
        }
      }
    : (root, f) => root.traverse(f);
  const culled = [];
  each(scene, (o) => {
    if (o.frustumCulled) {
      o.frustumCulled = false;
      culled.push(o);
    }
  });
  /* every descendant, not only top-level groups: hidden set pieces, rider parts and effects would otherwise build their
    materials and pipelines on the first race frame that shows them */
  const shown = [];
  const showAll = (root) =>
    each(root, (o) => {
      shown.push([o, o.visible]);
      o.visible = true;
    });
  showAll(scene);
  if (skyScene) showAll(skyScene);
  const drawables = [];
  each(scene, (o) => {
    if (o.isMesh || o.isPoints || o.isLine || o.isSprite) drawables.push(o);
  });
  for (const o of drawables) o.visible = false;
  warmView.drawables = drawables;
  warmView.groups = [];
  each(scene, (o) => {
    if (!(o.isMesh || o.isPoints || o.isLine || o.isSprite)) warmView.groups.push(o);
  });
  warmView.active.clear();
  /* the sky compiles asynchronously alongside the slices (awaited before the final frames) */
  const skyCompile =
    // the fog renderer draws the sky in its own pass, built by the first warm frame
    skyScene && !fogRenderer
      ? renderer.compileAsync(skyScene, camera).catch((e) => console.warn('Race warm-up sky compile failed', e))
      : null;
  /* Real frames through the post passes (fog, sun flare, halos, snow composite, glare: frame() renders while `warming`),
    revealing a slice of the drawables each frame: every node material and pipeline is built by the exact render path the
    race uses, a few at a time, so the loading screen keeps animating. Then draw until two frames in a row add no
    pipeline (WebGL links programs synchronously; Safari/Metal compiles are slow), at most 30 more frames. */
  // textures swapped in at run time (snow flipbook frames) upload now; post passes that only draw with something in view (sun Z query,
  // light glows) draw once
  scene.traverse((o) => {
    for (const t of o.userData.warmTextures ?? []) renderer.initTexture(t);
  });
  if (sunFlare) sunFlare.warm = true;
  if (lightGlow) lightGlow.warm = true;
  // the glare pass and the glow query (web/glare-pass.js, light-glow.js)
  if (glarePass) glarePass.warm = true;
  if (lightGlow) lightGlow.warmQuery = true;
  warming = true;
  const pipelineCount = () => renderer._pipelines?.caches?.size ?? 0;
  try {
    /* a slice ends after warmSlice drawables or warmNewMaterials not-yet-built material variants (a new node material
     is the expensive build; repeats of a built one are cheap) */
    /* the post passes and what always draws build in a frame of their own, then the course's shared vertex buffers (>= 1 MB:
     the interleaved vertices, colours, light UVs, vertex alpha; tens of MB) upload one per frame, before the first slice draws them */
    {
      const skyMeshes = [];
      skyScene?.traverse((o) => {
        if (o.isMesh && o.visible) {
          skyMeshes.push(o);
          o.visible = false;
        }
      });
      const attributes = renderer._attributes,
        seen = new Set(),
        big = [];
      if (typeof attributes?.update === 'function')
        worldScene.traverse((o) => {
          if (!o.isMesh || !o.geometry) return;
          for (const a of Object.values(o.geometry.attributes)) {
            const b = a.isInterleavedBufferAttribute ? a.data : a;
            if (!seen.has(b) && (b.array?.byteLength ?? 0) >= 1 << 20) {
              seen.add(b);
              big.push(a);
            }
          }
        });
      warmView.active = new Set();
      warmShadowLimit = 0;
      await nextFrame();
      alive();
      /* then, a step a frame: three sky meshes, a rider's shadow silhouettes (web/rider-shadow.js) and one shared vertex buffer */
      for (
        let step = 1, steps = Math.max(riderShadows?.entryCount ?? 0, Math.ceil(skyMeshes.length / 3), big.length);
        step <= steps;
        step++
      ) {
        for (const o of skyMeshes.slice(3 * (step - 1), 3 * step)) o.visible = true;
        warmShadowLimit = step;
        if (big[step - 1])
          attributes.update(
            big[step - 1],
            // AttributeType.VERTEX
            1
          );
        await nextFrame();
        alive();
      }
      warmShadowLimit = Infinity;
      for (const o of skyMeshes) o.visible = true;
    }
    const built = new Set(),
      variant = (o) =>
        `${Array.isArray(o.material) ? o.material.map((m) => m.id).join(',') : o.material?.id}|${o.isSkinnedMesh ? 1 : 0}|${o.isInstancedMesh ? 1 : 0}`;
    /* only the current slice is drawn (built materials and pipelines stay cached), so warm frames stay light on slow GPUs */

    // new materials per frame follow their measured cost (a build is 40-55 ms at 4x CPU: one a frame there, three on a
    // desktop)
    // the first slice builds one, then its measured cost sets the rate
    let newLimit = 1;

    // on a gated tier (30 fps: a drawn frame every 2nd animation frame) a drawn frame takes that many animation frames'
    // slices, and the new-material rate follows the drawn frame's own work (frame(), not the wait for it)
    const perNow = () => Math.max(1, Math.round(60 / (window.__frameRate?.() || 60)));
    for (let i = 0; i < drawables.length; ) {
      const per = perNow(),
        slice = [];
      let fresh = 0;
      for (const end = i + warmSlice * per; i < drawables.length && i < end && fresh < newLimit; i++) {
        const k = variant(drawables[i]);
        if (!built.has(k)) {
          built.add(k);
          fresh++;
        }
        drawables[i].visible = true;
        slice.push(drawables[i]);
      }
      warmView.active = new Set(slice);
      const f0 = performance.now();
      const work = await nextFrame();
      alive();
      if (fresh)
        newLimit = Math.max(
          1,
          Math.min(
            cap * per,
            // cap: pv worldWarm passes more (their pipelines were compiled first: a new variant costs its render object)
            Math.floor((40 * per) / ((work > 0 ? work : performance.now() - f0) / fresh))
          )
        );
      for (const o of slice) o.visible = false;
    }
    for (const o of drawables) o.visible = true;
    // then the whole scene together (draw-order and pass combinations)
    warmView.active = new Set(drawables);
    performance.mark('warm:slices');
    await skyCompile;
    alive();
    performance.mark('warm:sky');

    // three's canvas output pass (an empty scene straight to the hidden canvas: the first cutscene frame draws the scene
    // directly)
    try {
      renderer.render(new T.Scene(), camera);
    } catch (e) {
      console.warn('Warm output pass', e);
    }
    for (let n = 0, quiet = 0, last = pipelineCount(); n < 30 && !(n >= 2 && quiet >= 2); n++) {
      await nextFrame();
      alive();
      const now = pipelineCount();
      quiet = now === last ? quiet + 1 : 0;
      last = now;
    }
    worldScene.userData.releaseGeometryArrays?.();
    worldScene.userData.releaseGeometryArrays = null;
    worldScene.userData.releaseGpuCopies?.({ geometry: false });
    // the event world's texels
    // every world batch has drawn: the GPU holds its vertex data
    worldScene.userData.releaseGpuCopies = null;
  } finally {
    warming = false;
    warmShadowLimit = Infinity;
    if (glarePass) glarePass.warm = false;
    if (lightGlow) lightGlow.warmQuery = false;
    if (sunFlare) sunFlare.warm = false;
    if (lightGlow) lightGlow.warm = false;
    warmView.restore();
    warmView.groups = [];
    warmView.drawables = [];
    warmView.active = new Set();
    for (const [o, v] of shown) o.visible = v;
    for (const o of culled) o.frustumCulled = true;
    for (const p of terrainProxies) {
      p.removeFromParent();
      p.geometry.dispose();
    }
    performance.mark('warm:end');
  }
  /* Firefox builds the pipelines serially in its GPU process, behind the (hidden) warm frames: the load screen stays until the
    GPU has caught up (docs/firefox-load.md). The intro prep may start now (framesDone): it only needs the CPU meanwhile. */
  framesDone?.();
  await gpuIdle();
  performance.mark('warm:gpu');
}
/* resolves once the GPU has run everything submitted so far (WebGPU; pipelines included), at most 30 s */
const gpuIdle = () => {
  const q = renderer?.backend?.device?.queue;
  return q ? Promise.race([q.onSubmittedWorkDone().catch(() => {}), new Promise((r) => setTimeout(r, 30000))]) : Promise.resolve();
};
// per warm-up frame: drawables revealed, new material variants built
const warmSlice = 60,
  warmNewMaterials = 3;
// ---- Course lifecycle and in-app navigation (docs/course-switch.md) ----------------------------------------------------- A course /
// world change never reloads the page: navigateCourse() pushes a history entry with the query a reload would use (?course, &rider, &base,
// &autostart, &peakCourse / &peakMode, online: &online &lobby), then switchCourse() shows the original load screen, releases the old course
// (unloadCourse) and builds the new one (loadCourse) in the running page. Back / Forward (popstate) load the entry's course and return to
// the menu it was left from. Each course gets a fresh wasm core instance (the compiled module is shared); the old instance is dropped with
// every per-course object holding it.
let loadBefore = null,
  live = false,
  switchBusy = false,
  courseGen = 0,
  courseRoots = [],
  courseDisposers = [],
  switchChain = Promise.resolve(),
  switchTarget = null,
  audioMenuAfterLoad = false,
  libraryClips = null,
  coreModule = null,
  loadedKey = '';
const mpLighting = {},
  coreRefs = [];
window.__courseLoads = [];
window.__coreRefs = coreRefs;
ui.courseReady = switchChain;
// course switches asked for before the page's first course is loaded and set up (init) wait on this gate, so they run after
// it, never alongside it (two loads wrote the same globals: a page stuck on 'game' with no core). init opens it (openBootGate).
let bootGate = null,
  bootKey = null;
{
  switchChain = new Promise((r) => (bootGate = r));
  ui.courseReady = switchChain;
  /* Back / Forward from the start (init's own listener came only after the boot load): during the boot, back to the page's own course drops
    the switch asked for meanwhile (the boot course's event then runs as if none was asked); another entry is a switch queued behind it */
  addEventListener('popstate', (e) => {
    if (bootGate && bootKey != null && courseKeyOf(new URL(location.href).searchParams) === bootKey) {
      switchTarget = null;
      return;
    }
    onPopState(e);
  });
}
const openBootGate = () => {
  const g = bootGate;
  bootGate = null;
  g?.();
};
const POP_SCREENS = new Set(['title', 'main', 'character', 'setup', 'event', 'details']);
const coreWasmUrl = new URL('./runtime/core.wasm', import.meta.url);
function compileCore() {
  return (async () => {
    try {
      const r = await fetch(coreWasmUrl);
      if (!r.ok) throw Error(`core.wasm: ${r.status}`);
      try {
        return await WebAssembly.compileStreaming(r.clone());
      } catch {
        return await WebAssembly.compile(await r.arrayBuffer());
      }
    } catch (e) {
      console.warn('Core precompile failed (per-load compile instead)', e);
      return null;
    }
  })();
}
/* A fresh core instance per course from the one compiled module (instantiation only; no second download or compile). */
async function newCore() {
  const module = await (coreModule ??= compileCore());
  // pv gcWatchdog: its kick memories never held across a core instantiation (iOS has 3 fast-memory slots; web/gc-watchdog.js release)
  window.__gcWatchdog?.release?.();
  // pv switchGC (web/switch-gc.js): the earlier courses' cores collected first (JavaScriptCore frees a wasm memory only in a full
  // collection)
  if (pv('switchGC') && isJavaScriptCore()) {
    const r = await collectBefore();
    if (r.alive) window.__switchGC = [...(window.__switchGC || []).slice(-19), r];
  }
  if (!module) {
    coreModule = null;
    return createCore();
  }
  return createCore({
    instantiateWasm(imports, receive) {
      WebAssembly.instantiate(module, imports).then(
        (inst) => {
          // its memory: what a full collection frees
          if (pv('switchGC')) for (const v of Object.values(inst.exports)) if (v instanceof WebAssembly.Memory) coreTracker.track(v);
          receive(inst);
        },
        (e) => console.error('Core instantiate failed', e)
      );
      return {};
    }
  });
}
function courseFor(params) {
  const code = params.get('course') || 'ARA1';
  // streamed Peak 1 world: the ARA1 package supplies the shared settings, the locations stream (free-ride.js)
  if (isStreamedWorld(code)) return peakCourseEntry(courses, params);
  return courses.find((c) => c.code === code) || (code === 'ARA1' ? DEFAULT_COURSE : null);
}
const courseKeyOf = (params) => {
  const code = params.get('course') || 'ARA1';
  return !isStreamedWorld(code)
    ? code
    : params.has('peakMode')
      ? code + '/run' + params.get('peakMode')
      : code + '/' + (params.get('peakCourse') ?? PEAK_DEFAULT_STATION[code]);
};
/* history: 'push' (default; an online lobby entry is replaced by the lobby's next course), 'replace' or 'none' (popstate).
   after: 'autostart' (as a load with ?autostart=1), 'lobby' (online lobby screen), 'menu' (Back / Forward: `screen`). */
function navigateCourse(url, { history: mode, after = 'autostart', screen = null } = {}) {
  url = new URL(url, location.href);
  const online = after === 'lobby' ? 1 : 0;
  mode ??= online && history.state?.online ? 'replace' : 'push';
  try {
    if (mode === 'push') {
      history.replaceState({ ...(history.state || {}), ssx3: 1, screen: ui.screen }, '');
      history.pushState({ ssx3: 1, online }, '', url);
    } else if (mode === 'replace') history.replaceState({ ...(history.state || {}), ssx3: 1, online }, '', url);
  } catch (e) {
    console.warn('History update failed', e);
  }
  const job = { url, after, screen, key: courseKeyOf(url.searchParams), at: performance.now() };
  // the event's rider alongside the course
  if (after === 'autostart') prefetchEventRider();
  if (after === 'background') job.abandonedAt = new Promise((r) => (job.abandon = r));
  else {
    // the lazy first course (backgroundCourse): asked for again, its load carries on and ends as this request; another course abandons it,
    // so the chosen course loads first (under the load screen from now)
    const bg = switchTarget?.after === 'background' ? switchTarget : backgroundJob && !backgroundJob.abandoned ? backgroundJob : null;
    if (bg && !bg.abandoned && bg.key === job.key) {
      Object.assign(bg, { url, after, screen });
      if (after === 'autostart') ui.loading.open();
      return switchChain;
    }
    if (bg) {
      bg.abandoned = true;
      bg.abandon?.();
      if (after === 'autostart') {
        ui.loading.cancel?.();
        ui.loading.open();
        job.loadingOpen = true;
      }
    }
  }
  switchTarget = job;
  // a newer request replaces one still queued
  const run = switchChain.then(() => (switchTarget === job ? (after === 'background' ? backgroundCourse(job) : switchCourse(job)) : null));
  switchChain = run.catch((e) => console.error('Course switch failed', e));
  ui.courseReady = switchChain;
  return switchChain;
}
function stopRun() {
  if (running || isPaused()) rumble.stop();
  running = false;
  runHoldsEnd('stopRun');
  finished = false;
  resultsPending = false;
  aiActive = false;
  clearInput();
  cutscenes?.stop();
  try {
    gameAudio.leaveWorld();
  } catch (e) {
    console.warn('Audio leave failed', e);
  }
  audioScreen = null;
}
async function switchCourse(job) {
  const { url, after } = job,
    params = url.searchParams,
    next = courseFor(params),
    t0 = performance.now();
  let rec = null;
  switchBusy = true;
  try {
    if (!next) throw Error('Unknown or unprepared course ' + (params.get('course') || 'ARA1'));
    courseGen++;
    live = false;
    ready = false;
    ui.ready = false;
    stopRun();
    worldEvent = null;
    // pv eventInWorld: an in-world event goes with its world
    worldAiReturn = null;
    // switchGate
    acrossBefore = new Set(scene.children);
    course = next;
    ui.course = next;
    // the original load screen: it animates on its own frames while the course unloads and loads
    if (!(job.loadingOpen && ui.loading.active)) {
      ui.loading.cancel?.();
      ui.loading.open();
    }
    const released = await unloadCourse(),
      t1 = performance.now();
    // the held draw hides what the new course adds (renderAcross): only what is still in the scene matters; the released course's objects
    // were kept, and through their closures its core, until the new course went live (WebKit: its 128 MB wasm memory resident beside the
    // new core's through the whole load, docs/mobile.md "Load spikes")
    if (acrossBefore) acrossBefore = new Set(scene.children.filter((o) => acrossBefore.has(o)));
    await loadCourse(next);
    loadedKey = job.key;
    const t2 = performance.now();
    // the boot model is RIDER_SAM on the course's initial settings (as on a first load); the chosen rider loads with the event
    // (ensureRider)
    loadedRider = { ...bootRider(), id: '' };
    pendingRider = selectedRider;
    // the ride's rider under the load screen
    const rideJob =
      pv('rideWarm') && next.freeRide && after === 'autostart' && switchTarget === job
        ? warmRideRider().catch((e) => console.warn('Ride rider warm-up failed', e))
        : null;
    // pv worldWarm: the world's pipelines before the ride starts (its compiles alongside the rider's)
    if (pv('worldWarm') && next.freeRide && after === 'autostart' && !params.has('peakMode') && switchTarget === job)
      await warmWorld(job, rideJob).catch((e) => console.warn('World warm-up failed', e));
    await rideJob;
    // pv switchGC (web/switch-gc.js): one full collection before the ride / event starts, under the load screen
    if (pv('switchGC') && isJavaScriptCore() && switchTarget === job) {
      const r = await collectNow();
      window.__switchGC = [...(window.__switchGC || []).slice(-19), { after: job.key, ...r }];
    }
    ready = true;
    ui.ready = true;
    ui.sync();
    rec = {
      code: next.code,
      key: job.key,
      after,
      unloadMs: Math.round(t1 - t0),
      loadMs: Math.round(t2 - t1),
      queuedMs: Math.round(t0 - job.at),
      released,
      memory: { ...renderer.info.memory },
      sceneChildren: scene.children.length
    };
    window.__courseLoads.push(rec);
    diagnose('course', rec);
    console.info(`Course ${job.key}: unload ${rec.unloadMs} ms, load ${rec.loadMs} ms`, rec);
  } catch (e) {
    const detail = e instanceof Error ? e.message : String(e);
    console.error('Course load failed', e);
    document.body.dataset.loadError = detail;
    diagnose('course-failed', { key: job.key, message: detail });
    /* back to Snow Jam so the menus have a world again */
    try {
      if (job.key === 'ARA1') throw e;
      courseGen++;
      live = false;
      if (!courseRoots.length && loadBefore) courseRoots = scene.children.filter((o) => !loadBefore.has(o) && !feOwned(o));
      // a load that failed half way
      await unloadCourse();
      course = DEFAULT_COURSE;
      await loadCourse(courseFor(new URLSearchParams()));
      loadedKey = 'ARA1';
      loadedRider = { id: '', name: 'Sam', package: 'RIDER_SAM' };
      pendingRider = selectedRider;
      try {
        const u = new URL(location.href);
        for (const k of ['course', 'peakCourse', 'peakMode', 'autostart', 'online', 'lobby']) u.searchParams.delete(k);
        history.replaceState({ ssx3: 1 }, '', u);
      } catch {}
      ready = true;
      ui.ready = true;
      ui.loading.cancel?.();
      audioMenuAfterLoad = true;
      ui.set('main');
    } catch (e2) {
      console.error('Fallback load failed', e2);
      ui.error = 'Load failed';
      ui.ready = false;
      ui.loading.cancel?.();
      ui.set('title');
      ui.draw({});
    }
    return;
  } finally {
    switchBusy = false;
  }
  // another course was asked for meanwhile: it loads next
  if (switchTarget !== job) return;
  switchTarget = null;
  afterSwitch(job);
}
/* pv rideWarm (docs/ctm-flow.md "Ride start"): a world load into a free ride (Conquer the Mountain, a peak run) has no event load
   to prepare the rider, so the ride's startRun loaded and set it up (_init_animation + _init_race: 250-400 ms on a phone, more with
   its downloads) and its first frames built its node materials (340-475 ms). Here, under the load screen: ensureRider (careerRider's
   re-resolve decides as before whether the rider is stale; only the time it runs moves), then its materials compile for the world
   pass as the ride draws them (fog-renderer compileObject at the world pass's render-context depth 1; a two-pass transparent
   material once per side, as free-ride.js streamWarm). startRun then finds the rider ready. The ride is the same tick for tick
   (a new career with and without the arrival cutscene, 900 ticks: scratch ctm-trace). */
async function compileRider() {
  const fr = fogRenderer,
    m = sam;
  if (!fr?.compileObject || !m) return;
  const two = [];
  m.traverse((o) => {
    if (o.isMesh)
      for (const x of [].concat(o.material || [])) if (x.transparent && x.side === T.DoubleSide && !x.forceSinglePass) two.push(x);
  });
  for (const side of two.length ? [T.BackSide, T.FrontSide] : [null]) {
    if (side != null) for (const x of two) x.side = side;
    try {
      await fr.compileObject(m, { depth: 1 });
    } finally {
      if (side != null) for (const x of two) x.side = T.DoubleSide;
    }
  }
}
async function warmRideRider() {
  const t = performance.now();
  await ensureRider();
  const t1 = performance.now();
  await compileRider();
  await gpuIdle();
  try {
    performance.measure('ride:rider', { start: t, end: t1 });
    performance.measure('ride:compile', { start: t1 });
  } catch {}
}
// pv worldWarm (docs/course-switch.md "World arrivals warm under the load screen"): objects compiled for the world pass as one call
// (fog-renderer compileObject: its target, MRT and call depth), unculled for the call's listing. sides: a transparent DoubleSide material
// draws in two passes (BackSide, then FrontSide: three renderObject), so it is compiled once per side; only while nothing shows the frame
// (the opaque load screen).
async function compileFor(list, sides = false) {
  const fr = fogRenderer;
  if (!fr?.compileObject || !list.length) return;
  const two = new Set(),
    twoMeshes = [];
  if (sides)
    for (const o of list)
      o.traverse((x) => {
        let any = false;
        for (const m of [].concat(x.material || []))
          if (m && m.transparent && m.side === T.DoubleSide && !m.forceSinglePass) {
            two.add(m);
            any = true;
          }
        if (any) twoMeshes.push(x);
      });
  const tmp = new T.Group();
  for (const [side, items] of two.size
    ? [
        [T.BackSide, list],
        [T.FrontSide, twoMeshes],
        // as the draw lists it: the 'backSide' pass's render object too
        [T.DoubleSide, twoMeshes]
      ]
    : [[null, list]]) {
    if (side != null) for (const m of two) m.side = side;
    const culled = [];
    let p = null;
    try {
      for (const o of items) {
        tmp.children.push(o);
        o.traverse((x) => {
          if (x.frustumCulled) {
            x.frustumCulled = false;
            culled.push(x);
          }
        });
      }
      p = fr.compileObject(tmp);
    } catch (e) {
      console.warn('World warm compile', e);
    } finally {
      tmp.children.length = 0;
      for (const x of culled) x.frustumCulled = true;
    }
    try {
      await p;
    } catch (e) {
      console.warn('World warm compile', e);
    } finally {
      if (side != null) for (const m of two) m.side = T.DoubleSide;
    }
  }
}
// pv worldWarm: a Conquer the Mountain world arrival (menu -> career, a Transport across a world switch) prepared before the load screen
// lets the ride start. 1: the start row's own compile (free-ride.js rewarm: every mesh of the resident locations, unculled, both sides),
// which loadCourse starts and which was never awaited, so the ride's first frame built what it had not reached yet (Owen's Safari: 3000
// buffers, 7000 bind groups, 20 pipelines, 3.3 s). 2: the rest of what the world pass draws (sky, set pieces and their particles / halos,
// terrain sparkle, rider shadows, the post passes): behind the opaque load screen the race warm-up's frames with the locations left as they
// are (skip); under the Transport's visible held loop (load screen world mode) a compile of those objects only, no frame. The simulation is
// not touched (nothing ticks).
async function warmWorld(job, riderReady = null) {
  const gen = courseGen,
    mine = () => gen === courseGen && switchTarget === job,
    t0 = performance.now(),
    budget = yieldBudget.ms,
    // the Transport's held loop shows the frame
    shown = !ui.loading.active || !!ui.loading.session?.world;
  yieldBudget.ms = shown ? 8 : 12;
  // a load screen covers the frame (world mode: the held loop still draws): more compile work a frame (web/yield-shim.js)
  try {
    const isLoc = (o) => o.userData?.peakLocation != null,
      lead = freeRide?.rewarmLeadCodes ?? null,
      isLead = (o) => isLoc(o) && (!lead || lead.has(o.userData.peakLocation));
    // what the world pass draws besides the locations (set pieces, particles, halos, weather, sparkle), compiled alongside the start row's
    // rewarm, in 8 calls that compile side by side (a call builds its items one after another); a two-pass material per side only while
    // nothing shows the frame
    const rest = [];
    const walk = (o) => {
      if (isLoc(o) || o === sam) return;
      // the rider: rideWarm's (it may swap the model meanwhile)
      if ((o.isMesh || o.isPoints || o.isLine || o.isSprite) && o.layers.mask & 1) rest.push(o);
      for (const c of o.children) walk(c);
    };
    for (const r of courseRoots) walk(r);
    const per = Math.max(1, Math.ceil(rest.length / 8)),
      pre = [];
    for (let i = 0; i < rest.length; i += per) pre.push(compileFor(rest.slice(i, i + per), !shown));
    // the start row's part (web/free-ride.js rewarm)
    const rw = freeRide?.rewarmLead ?? worldRewarm;
    await Promise.race([Promise.all([rw, ...pre]), new Promise((r) => setTimeout(r, 30000))]);
    await riderReady;
    if (!mine()) return;
    const t1 = performance.now();
    // behind the opaque load screen: the race warm-up's frames for what only a draw builds (the sky pass, post passes, rider shadows, the
    // encoded effects, a two-pass material's BackSide render object); the locations are left as they are (skip: their rewarm compiled every
    // pass, the DoubleSide one included)
    if (!shown) await warmupRender(null, { skip: isLoc, cap: warmSlice });
    else await gpuIdle();
    try {
      performance.measure('world:rewarm', { start: t0, end: t1 });
      performance.measure('world:warm', { start: t1 });
    } catch {}
  } finally {
    yieldBudget.ms = budget;
  }
}
function afterSwitch({ after, screen, url }) {
  // pv onlineRecords: Watch Replay's course (web/online-replay.js)
  if (after === 'replay') {
    ui.onlineWatch?.afterSwitch();
    return;
  }
  if (after === 'autostart') {
    /* as a page loaded with ?autostart=1: a career round / world resumes at its objectives (web/career-ui.js), else the event loads */
    const cu = ui.careerUI;
    // career screens as on a fresh page: the pending round or world sets them up
    if (cu?.career?.save?.pending) cu.freeRide = null;
    if (!cu?.resume())
      ui.loadEvent(() => {
        ui.set('game');
        startRun();
      });
    return;
  }
  // the host's start came during the load: its event load takes the load screen over
  if (after === 'lobby' && mpGame?.session?.starting) return;
  ui.loading.cancel?.();
  audioMenuAfterLoad = true;
  // a menu, not the event: the FE audio (the next event load prepares the world again)
  gameAudio.leaveWorld();
  const online = url.searchParams.get('online') === '1' || url.searchParams.has('lobby');
  if (after === 'lobby') {
    const lobby = mpGame?.session?.client?.state?.lobby;
    ui.set(ui.onlineMode ? (lobby ? 'mp-lobby' : 'mp-lobbies') : 'main');
    return;
  }
  if (online && ui.mpUI) {
    ui.onlineMode = true;
    ui.careerMode = false;
    ui.mpUI.enter();
    return;
  }
  ui.set(screen || 'main');
}
/* Back / Forward: leave what runs (online race or lobby, career session, the run), load the entry's course if it differs, and go
   to the menu the entry was left from (the Single Event selector at Select Peak for its screens and for an event entry never
   left from a menu; else the main menu). */
function onPopState(e) {
  const url = new URL(location.href),
    params = url.searchParams,
    st = e.state || {},
    online = params.get('online') === '1' || params.has('lobby');
  try {
    if (mpGame?.racing) mpGame.session.quitRace();
    if (ui.onlineMode && !online) {
      mpGame?.session?.client?.leave();
      ui.onlineMode = false;
    }
  } catch (err) {
    console.warn('Online leave failed', err);
  }
  const cu = ui.careerUI;
  if (cu && (cu.active || cu.freeRide || cu.career?.save?.pending)) {
    if (cu.career?.save?.pending) delete cu.career.save.pending;
    try {
      cu.career?.persist();
    } catch {}
    cu.active = null;
    cu.freeRide = null;
  }
  if (!online) ui.careerMode = false;
  const screen = POP_SCREENS.has(st.screen)
    ? st.screen
    : /^fe-(peak|mode|event)$/.test(st.screen ?? '') ||
        (!!st.screen && !!ui.eventSelect?.owns?.(st.screen)) ||
        (!st.screen && params.get('autostart') === '1' && !online && !isStreamedWorld(params.get('course') || 'ARA1'))
      ? // the Single Event selector (web/fe-event-select.js; ui.set('event') opens it at Select Peak)
        'event'
      : 'main';
  applyUrlRider(params).catch((err) => console.warn('Rider from URL failed', err));
  if ((courseKeyOf(params) === loadedKey && live && !switchBusy) || (lazyPending() && courseKeyOf(params) === lazyKey())) {
    if (backgroundJob) backgroundJob.after = 'background';
    stopRun();
    if (online && ui.mpUI) {
      ui.onlineMode = true;
      ui.mpUI.enter();
    } else ui.set(screen);
    return;
  }
  navigateCourse(url, { history: 'none', after: 'menu', screen });
}
async function applyUrlRider(params) {
  const id = params.get('rider');
  if (!id || !ui.riders?.length) return;
  const base = params.get('base') || undefined;
  if (id === selectedRider.id && (base ?? selectedRider.base) === selectedRider.base) return;
  const found = ui.riders.find((r) => r.id === id),
    entry = found?.kind === 'cheat' && base ? { ...found, base } : found;
  if (!entry) return;
  await selectRider(entry);
  ui.characterSelect?.adopt(entry);
}
/* Quality tier / MSAA at run time (web/quality.js setQuality): the canvas and fog-renderer pass targets follow renderer.samples;
   the pipelines for the new sample count are built now (warm-up) instead of on the next race frames. */
function applyAntialias() {
  if (!renderer) return;
  const want = quality.antialias && !renderer.backend?.compatibilityMode ? 4 : 0;
  if (renderer.samples === want) return;
  renderer._samples = want;
  const was = warmed;
  warmed = false;
  if (was && live && !warming) warmupRender().catch((e) => console.warn('Re-warm failed', e));
}
const yieldTask = () => new Promise((r) => setTimeout(r, 0));
/* Textures a material draws (map slots, node graphs); render-target textures belong to their targets (disposed by their owner). */
function materialTextures(m, out, seen) {
  const stack = [];
  const push = (v) => {
    if (!v || typeof v !== 'object') return;
    if (v.isTexture) {
      if (!v.isRenderTargetTexture) out.add(v);
    } else if (v.isNode && !seen.has(v)) {
      seen.add(v);
      stack.push(v);
    }
  };
  for (const k of Object.keys(m)) push(m[k]);
  push(m.userData?.sourceTexture);
  while (stack.length) {
    const n = stack.pop();
    if (n.isTextureNode) push(n.value);
    try {
      for (const c of n.getChildren()) push(c);
    } catch {}
  }
}
/* Remove and release object trees: every object's dispose event (three frees its render objects: bindings, uniform buffers,
   pipelines refs), then materials, geometries (GPU attributes), skeletons, textures. Sliced so the load screen keeps animating. */
function collectTree(roots, objects = new Set()) {
  for (const r of roots) r?.traverse((o) => objects.add(o));
  return objects;
}
async function disposeRoots(roots, textures = new Set(), objects = collectTree(roots)) {
  // web/gpu-copies.js: a disposed world's restores (gpuRelease, keyed by its package groups) no longer keep its tree
  for (const o of objects) forgetGpuRestore(o);
  const geometries = new Set(),
    materials = new Set(),
    skeletons = new Set(),
    seen = new Set();
  for (const r of roots) r?.removeFromParent();
  for (const o of objects) {
    if (o.parent && !objects.has(o.parent)) o.removeFromParent();
    for (const t of o.userData.warmTextures ?? []) if (t?.isTexture) textures.add(t);
    // run-time swapped maps (snow flipbooks)
    if (o.geometry) geometries.add(o.geometry);
    for (const m of [].concat(o.material || [])) if (m) materials.add(m);
    if (o.skeleton) skeletons.add(o.skeleton);
  }
  for (const m of materials) materialTextures(m, textures, seen);
  let t = performance.now();
  const step = async () => {
    if (performance.now() - t > 8) {
      await yieldTask();
      t = performance.now();
    }
  };
  for (const o of objects) {
    o.dispatchEvent({ type: 'dispose' });
    await step();
  }
  for (const m of materials) {
    m.dispose();
    await step();
  }
  for (const g of geometries) {
    g.dispose();
    await step();
  }
  for (const s of skeletons) s.dispose();
  for (const x of textures) {
    x.dispose();
    await step();
  }
  return { objects: objects.size, geometries: geometries.size, materials: materials.size, textures: textures.size };
}
/* Everything loadCourse built, in dependency order (modules that free core memory run before the core is dropped). */
async function unloadCourse() {
  const roots = [],
    textures = new Set();
  const safe = (what, f) => {
    try {
      return f();
    } catch (e) {
      console.warn(`Course teardown: ${what}`, e);
    }
  };
  /* every object of the course, taken before the modules below detach parts of it (free-ride stop: locations, peak set pieces) */
  const objects = collectTree([...courseRoots.filter((o) => o.parent === scene), sam, worldScene, ...(skyScene?.children ?? [])]);
  bigChallenges = null;
  if (ui.bigChallenges) ui.bigChallenges = null;
  ui.freeRideHud = null;
  // web/compile-abort.js: the warms still building (free-ride locations, cutscene sets, the ride's rider) finish their item in flight and
  // stop before anything below is released
  await abortCompiles(1000).catch(() => {});
  if (freeRide) {
    safe('free ride', () => freeRide.stop());
    await freeRide.settle?.(1500).catch(() => {});
    // its warms in flight compile against this course's passes: done before they are disposed below
    freeRide = null;
  }
  window.__freeRide = null;
  // lineup.js keeps the raced event (and its riders' core) until the next roster seed
  safe('raced event draws', () => settleRaced());
  if (aiRace) {
    safe('computer riders', () => aiRace.renderer?.dispose());
    aiRace = null;
  }
  if (window.ssxAiRace) window.ssxAiRace = null;
  aiActive = false;
  safe('rival beam', () => rivalBeam?.dispose());
  rivalBeam = riderIcons = opponentFx = null;
  safe('terrain refinement', () => terrainRefinement?.dispose());
  terrainRefinement = null;
  safe('terrain sparkle', () => terrainSparkle?.dispose());
  terrainSparkle = null;
  setPieceRenderer = null;
  for (const [what, p] of [
    ['fog', fogRenderer],
    ['sun', sunFlare],
    ['glow', lightGlow],
    ['glare', glarePass],
    ['shadows', riderShadows]
  ])
    safe(what, () => p?.dispose?.());
  fogRenderer = sunFlare = lightGlow = glarePass = riderShadows = screenTint = null;
  for (const f of courseDisposers.splice(0)) roots.push(...(safe('disposer', f) || []));
  for (const o of courseRoots) if (o.parent === scene) roots.push(o);
  if (sam) {
    safe('rider skin', () => sam.userData.sourceSkin?.dispose());
    if (!roots.includes(sam)) roots.push(sam);
  }
  if (worldScene) {
    const u = worldScene.userData;
    roots.push(...(u.hiddenMeshes || []));
    for (const t of Object.values(u.worldTextures || {})) if (t?.isTexture) textures.add(t);
    const a = u.worldAtlas;
    if (a?.isTexture) textures.add(a);
    else safe('world atlas', () => a?.dispose?.());
    if (!roots.includes(worldScene)) roots.push(worldScene);
  }
  if (skyScene) roots.push(...skyScene.children);
  collectTree(roots, objects);
  const released = await disposeRoots(roots, textures, objects);
  courseRoots = [];
  pickupMeshes.length = 0;
  eventDeadMeshes.length = 0;
  movingMeshes.length = 0;
  // encoded-pass effects of the old course (snow-composite.js)
  pruneEncodedEffects((e) => {
    let o = e.object;
    while (o.parent) o = o.parent;
    return o === scene;
  });
  // QA handles; board-trail.js keeps the last core there
  delete globalThis.ssxEffects;
  worldScene = sky = skyScene = sam = null;
  boardTrail = snowRenderer = wakeRenderer = boostRenderer = weatherRenderer = null;
  progressMeter = null;
  ui.progressMeter = null;
  freestyleEvent = null;
  window.__freestyleEvent = null;
  riderLightingUpdate = null;
  animationReady = false;
  animPose = animInfo = posePhysicalFrame = null;
  cameraPose = previousCameraPose = null;
  currentRiderFrame = previousRiderFrame = null;
  raceInfo = null;
  hudPrepass = null;
  lastResetPlacement = 0;
  lastCamera.set(0, 0, 0);
  releasePreviewCores();
  // FE previews / cutscene actors light through the last core they were given
  if (ui.characterSelect) ui.characterSelect.core = null;
  core = null;
  replayTriggersCore = null;
  worldRewarm = null;
  // its promise's reactions reach the old free ride
  // the replay host's cached core kept the old course's wasm memory alive through the next load
  padPtr = 0;
  while (coreRefs.length > 8) coreRefs.shift();
  return released;
}
/* One course / world in the page (Single Event course, the streamed Peak 1 world, an online lobby's course): everything below is
   per course; unloadCourse releases it (GPU objects via three's dispose events, the wasm core instance, workers, audio world).
   The renderer, scene/camera objects, UI, audio engine, input, cutscene player and online session stay for the page. */
async function loadCourse(next) {
  const load = guardedLoad;
  // backgroundCourse: a load abandoned for another course stops at its next file
  globalThis.ssxBoot?.begin('world');
  const gen = courseGen,
    before = (loadBefore = new Set(scene.children));
  course = next;
  ui.course = course;
  setDrawOrderWorld(!!course.freeRide);
  // the CTM boot's FX texture handles (web/ps2-draw-order.js)
  warmed = false;
  spawn = await load(course.root + 'start.json');
  origin.fromArray(spawn.position);
  route = await load(course.root + 'route.json');
  coreLoading = true;
  // switchGate
  core = await newCore();
  // race_end asks it at a freestyle finish (0x239230 / 0x23A05C, fsCelebrate)
  core.finishHost = { place: (score) => gameHost.freestylePlace(score) ?? -1 };
  coreRefs.push(new WeakRef(core));
  core._set_fx_deferred?.(1);
  // FX passes after every rider's 121818 (simTick / ai-racers endTick)
  padPtr = core._malloc(96);
  const terrain = new Float32Array(await load(course.root + 'collision.bin', 'buffer'));
  if (core._init_world_step)
    // the triangle tree across load-screen frames (web/load-slices.js)
    await initWorldStepped(core, terrain, { alive: courseLoadGuard });
  else {
    const ptr = core._malloc(terrain.byteLength);
    core.HEAPF32.set(terrain, ptr / 4);
    core._init_world(ptr, terrain.length);
    core._free(ptr);
  }
  const [metadata, settings, packets] = await Promise.all([
    load('/assets/ANIMATIONS/animation-packets.json'),
    load(course.initial),
    load('/assets/ANIMATIONS/animation-packets.bin', 'buffer')
  ]);
  animationResources = { metadata, settings, packets };
  freestyleEvent = await loadFreestyleEvent(course);
  window.__freestyleEvent = freestyleEvent;
  progressMeter = await loadProgressMeter(course);
  ui.progressMeter = progressMeter;
  // SSB kind-21 path (web/progress-meter-hud.js)
  worldScene = course.freeRide
    ? new T.Group()
    : await asset(
        course.code,
        false,
        course.root,
        courseLoadGuard
          ? { pause: courseLoadYield }
          : // the load screen draws between the build's slices, ~45 ms frames
            { pause: framePause('frame') }
      );
  terrainRefinement = course.freeRide ? null : await createTerrainRefinement(worldScene, origin, course.root);
  worldScene.matrixAutoUpdate = false;
  scene.add(worldScene);
  setPieceRenderer = course.freeRide
    ? null
    : await createSetPieceRenderer({ core, group: worldScene, load, course }).catch((e) => {
        console.warn(
          'Set pieces unavailable',
          typeof WebAssembly !== 'undefined' && e instanceof WebAssembly.Exception && core.getExceptionMessage
            ? core.getExceptionMessage(e).join(': ')
            : e
        );
        return null;
      });
  // the event world's static batches in frustum-culled cells, same draws (web/static-world.js; streamed locations:
  // web/peak-set-pieces.js)
  if (!course.freeRide) organizeStaticWorld(worldScene);
  sky = await asset('SKY', false, course.sky);
  sky.traverse((o) => {
    if (o.isMesh) {
      o.frustumCulled = false;
      o.material.depthWrite = false;
      o.material.depthTest = false;
      o.material.fog = false;
      o.material.color.multiplyScalar(1 / 0.516129);
      o.renderOrder = -10;
    }
  });
  skyScene = new T.Scene();
  // the GS clear behind the sky is black (382AF0, renderer+6AE0 = 0; docs/presentation.md)
  skyScene.background = new T.Color(0x000000);
  skyScene.add(sky);
  const skySettings = await load(course.sky + 'world.json'),
    fog = skySettings.fog.painted[skySettings.fog.start?.entry ?? 0];
  scene.fog = new T.Fog(new T.Color().setRGB(...fog.color), fog.near_cm / 100, fog.far_cm / 100);
  sam = await asset(bootRider().package, true);
  // RIDER_ZOE
  sam.traverse((o) => {
    if (o.isMesh) o.renderOrder = 600;
  });
  // the placeholder rider has no pose until the chosen rider replaces it; the frame's visibility pass does not run during
  // a course switch, when the held transport loop draws the scene (cutscenes.js acrossSwitch)
  sam.visible = false;
  scene.add(sam);
  libraryClips ??= (await load('/assets/ANIMATIONS/library.json')).clips;
  clips = libraryClips;
  allSamples ??= new Float32Array(await load('/assets/ANIMATIONS/samples.f32', 'buffer'));
  // the lattice in small environment_add calls (web/load-slices.js)
  const environmentMeta = await load((course.environmentRoot || course.root) + 'environment.json'),
    environmentBytes = new Uint8Array(await load((course.environmentRoot || course.root) + 'environment.bin', 'buffer'));
  await initEnvironmentSliced(core, (course.environmentRoot || course.root) + 'environment.json', environmentBytes);
  riderLightingUpdate = createRiderLightingUpdate(core, environmentMeta.irradiance);
  Object.assign(mpLighting, { configuration: environmentMeta.irradiance, meta: environmentMeta, bytes: environmentBytes });
  boardTrail = await createBoardTrail(origin);
  scene.add(boardTrail.group);
  resetPhysics();
  const allocations = [];
  const allocString = (x) => {
    const data = new TextEncoder().encode(JSON.stringify(x) + '\0');
    const ptr = core._malloc(data.length);
    allocations.push(ptr);
    core.HEAPU8.set(data, ptr);
    return ptr;
  };
  const packetPtr = core._malloc(packets.byteLength);
  allocations.push(packetPtr);
  core.HEAPU8.set(new Uint8Array(packets), packetPtr);
  const settingsPtr = allocString(settings);
  core._init_animation(allocString(metadata), allocString(rig), settingsPtr, packetPtr, packets.byteLength);
  core._init_race(settingsPtr);
  core._animation_use_physics(1);
  // the packages' own text (the node gates' input, web/ai-race-node.mjs), so the computer riders' contexts, which pass the
  // same text, copy this parse (web/world_bridge.cpp parse cache) instead of parsing again
  const rawText = async (p) => {
    const b = new Uint8Array(await load(p, 'buffer')),
      ptr = core._malloc(b.length + 1);
    allocations.push(ptr);
    core.HEAPU8.set(b, ptr);
    core.HEAPU8[ptr + b.length] = 0;
    return { ptr, json: () => JSON.parse(new TextDecoder().decode(b)) };
  };
  // a streamed world's start row replaces the base course's world (web/peak-world.js), so its terrain, collision and rails
  // are not loaded or built at all
  const baseWorld = !course.freeRide;
  // the course world cut in the peak-world worker and fed in parts (web/load-slices.js; not with the rail / pickup QA
  // fixtures, which read the parsed packages)
  const eventCut =
    baseWorld && core._event_world_seal && !['railTest', 'pickupTest'].some((k) => new URL(location.href).searchParams.has(k))
      ? await Promise.all(['terrain.json', 'world_collision.json', 'rails.json'].map((f) => load(course.root + f, 'buffer'))).then(
          ([terrain, world, rails]) => cutEventWorld({ terrain, world, rails })
        )
      : null;
  const terrainRaw = baseWorld && !eventCut && rawText ? await rawText(course.root + 'terrain.json') : null;
  const terrainPackage = !baseWorld
    ? { source_sha256: '' }
    : eventCut
      ? { source_sha256: eventCut.hash }
      : terrainRaw
        ? terrainRaw.json()
        : await load(course.root + 'terrain.json');
  if (baseWorld && !eventCut) core._init_terrain(terrainRaw ? terrainRaw.ptr : allocString(terrainPackage));
  const worldRaw = baseWorld && !eventCut && rawText ? await rawText(course.root + 'world_collision.json') : null;
  const worldPackage =
    !baseWorld || eventCut
      ? { instances: [] }
      : worldRaw
        ? {
            get instances() {
              return worldRaw.json().instances;
            }
          }
        : await load(course.root + 'world_collision.json');
  const hashBytes = new TextEncoder().encode(terrainPackage.source_sha256 + '\0'),
    hashPtr = core._malloc(hashBytes.length);
  allocations.push(hashPtr);
  core.HEAPU8.set(hashBytes, hashPtr);
  if (eventCut) {
    courseLoadGuard?.();
    await feedEventWorld(core, eventCut, hashPtr, { alive: courseLoadGuard });
    courseLoadGuard?.();
  } else {
    if (baseWorld) core._init_world_collision(worldRaw ? worldRaw.ptr : allocString(worldPackage), hashPtr);
    if (courseLoadGuard) await courseLoadYield();
    // behind the menus: two tasks, not one
    if (baseWorld) core._init_body_terrain(terrainRaw ? terrainRaw.ptr : allocString(terrainPackage));
  }
  core._init_fog(allocString(await load(course.root + 'fog-tree.json')));
  // the Weather painter, snowfall layers, camera splash, lightning (web/weather.inc; ?weather=0: none)
  if (core._init_weather) {
    const w =
      new URL(location.href).searchParams.get('weather') === '0' ? null : await load(course.root + 'weather.json').catch(() => null);
    core._init_weather(w ? allocString(w) : 0);
  }
  const lightAssets = await Promise.all([load(course.root + 'local-lights.json'), load(course.root + 'light-tree.json')]);
  core._init_rider_lighting(allocString(lightAssets[0]), allocString(lightAssets[1]));
  const railCatalog = baseWorld && !eventCut ? await load(course.root + 'rails.json') : { rails: [] };
  if (eventCut) await feedEventRails(core, eventCut, hashPtr, { alive: courseLoadGuard });
  else if (baseWorld) core._init_rails(allocString(railCatalog), hashPtr);
  // the terrain snow sparkle (web/terrain-sparkle.js, docs/presentation.md 14) of the course or of each streamed location
  terrainSparkle = await createTerrainSparkle({ T, origin }).catch((e) => {
    console.warn('Terrain sparkle unavailable', e);
    return null;
  });
  if (terrainSparkle) {
    scene.add(terrainSparkle.group);
    if (!course.freeRide) terrainSparkle.attach('course', course.root);
  }
  if (course.freeRide) {
    // the Peak 1 locations replace the course world (web/free-ride.js)
    freeRide = await createFreeRide({
      core,
      load,
      asset,
      parent: worldScene,
      disposeGroup: disposeRider,
      T,
      sparkle: terrainSparkle,
      origin: origin.toArray(),
      collectStart: () =>
        collectStart(core, { careerMode: ui.careerMode, career: ui.careerUI?.career, riderId: careerId(), courseCode: course.code }),
      manifestUrl: course.environmentRoot + 'peak.json',
      // the world pass's target and MRT (as it draws), else the canvas's
      warm: (g) => (fogRenderer ? fogRenderer.compileObject(g) : renderer.compileAsync(g, camera, scene)),
      idle: () => (!!cutscenes?.active && !cutscenes.seq) || (isPaused() && ui.screen !== 'game') || !!ui.loading?.active,
      covered: () => !!ui.loading?.active || !!ui.stage?.querySelector?.('video'),
      // nothing shows the world (the load screen, a movie over it)
      // nothing animates (a movie, a pause): background feeds and builds may take long calls peak runs (web/peak-run.js):
      // the active career event's tier row
      peakSetup: () => {
        const ev = ui.careerUI?.career?.active?.ev;
        return ev && ev.mode >= 6 && ev.mode <= 11
          ? ui.careerUI.career.peakSetup(ev)
          : // no career run (QA URL): the tier-0 row
            course.freeRide.kind !== 4 && ui.careerUI?.career
            ? ui.careerUI.career.peakSetup({ mode: course.freeRide.mode, course: course.freeRide.course })
            : null;
      },
      splitInputs: () => ({ raceTicks: Math.round((raceInfo?.[0] ?? 0) * 60), score: trickHudSlots()?.total ?? 0 })
    });
    // stage builtin 110: a new career's Green Base Station "?" homes in until the first FAQ was shown (web/peak_world.inc)
    freeRide.faqShown?.(!(ui.careerMode && ui.careerUI?.career && course.freeRide.kind === 4) || !!ui.careerUI?.me?.faqShown);
    await freeRide.start(course.freeRide.course, {
      kind: course.freeRide.kind,
      mode: course.freeRide.mode,
      // the peak run's rows (web/peak-run.js): the whole mountain prefetches along it
      route: course.freeRide.route
    });
    {
      const s = course.freeRide.spawn ?? freeRide.arrivalFor(course.freeRide.course, { grid: course.freeRide.grid });
      if (s) spawn = s;
    }
    window.__freeRide = freeRide;
    if (course.freeRide.kind === 4)
      createBigChallenges({ core, ui, gameAudio, riderId: () => careerId() })
        .then((b) => {
          if (gen !== courseGen) {
            if (ui.bigChallenges === b) ui.bigChallenges = null;
            return;
          }
          bigChallenges = b;
        })
        .catch((e) => console.warn('Big Challenges unavailable', e));
    // builtin 68 actions 4 (lodge door) / 3 (transport booth): world state 14 prompts
    // world painters by region (each location has its own Fog / ScreenTint record)
    const painterCache = new Map(),
      painter = (f) => {
        if (!painterCache.has(f))
          painterCache.set(
            f,
            load(f).catch(() => null)
          );
        return painterCache.get(f);
      };
    freeRide.on('region', async (entry) => {
      const [fog, tint] = await Promise.all([painter(entry.root + 'fog-tree.json'), painter(entry.root + 'screen-tint.json')]);
      if (gen !== courseGen || freeRide.region() !== entry.code) return;
      if (fog) {
        const b = new TextEncoder().encode(JSON.stringify(fog) + '\0'),
          p = core._malloc(b.length);
        core.HEAPU8.set(b, p);
        core._fog_keep_state(1);
        try {
          core._init_fog(p);
        } finally {
          core._fog_keep_state(0);
          core._free(p);
        }
      }
      if (tint) screenTint?.setTree(tint);
      // Weather (type 12): regions follow the location, blend state kept (the rider wind push reads it)
      painter(entry.root + 'weather.json').then((w) => {
        if (
          !w ||
          gen !== courseGen ||
          freeRide.region() !== entry.code ||
          !core._init_weather ||
          new URL(location.href).searchParams.get('weather') === '0'
        )
          return;
        const b = new TextEncoder().encode(JSON.stringify(w) + '\0'),
          p = core._malloc(b.length);
        core.HEAPU8.set(b, p);
        core._weather_keep_state(1);
        try {
          core._init_weather(p);
        } finally {
          core._weather_keep_state(0);
          core._free(p);
        }
      });
      // Sun (type 9) and glare (type 6) sections of the region (null: none -> reset defaults)
      const [sun, glare] = await Promise.all([painter(entry.root + 'sun-painter.json'), painter(entry.root + 'glare-painter.json')]);
      if (gen !== courseGen || freeRide.region() !== entry.code) return;
      if (sun) sunFlare?.setTree?.(sun.painter);
      if (glare) glarePass?.setTree?.(glare.painter);
    });
    // the area sky dome (22DE98: ASKY / BSKY, switched by the skybox triggers)
    const skyRoots = {
        ASKY: '/assets/SKY/',
        BSKY: '/assets/BRA2/sky/',
        CSKY: '/assets/CRA3/sky/',
        DSKY: '/assets/DRA4/sky/',
        ESKY: '/assets/ERA5/sky/'
      },
      skyGroups = new Map([[course.sky || skyRoots.ASKY, sky]]);
    // each peak world starts with its base course's dome (PEAK2: CRA3 = CSKY, docs/peak2.md)
    courseDisposers.push(() => [...skyGroups.values()].filter((g) => g !== sky));
    // the area skies not in skyScene: disposed with the course
    // 0x535C08 changes at a crossing (22DF50): the audio director's 28E8C0(20) course change (web/game-audio.js freeRideCourse,
    // docs/peak-mountain.md)
    freeRide.on('sky', async (code) => {
      const root = skyRoots[code];
      if (!root || gen !== courseGen) return;
      if (!skyGroups.has(root)) {
        const g = await asset('SKY', false, root);
        if (gen !== courseGen) {
          disposeRoots([g]);
          return;
        }
        g.traverse((o) => {
          if (o.isMesh) {
            o.frustumCulled = false;
            o.material.depthWrite = false;
            o.material.depthTest = false;
            o.material.fog = false;
            o.material.color.multiplyScalar(1 / 0.516129);
            o.renderOrder = -10;
          }
        });
        skyGroups.set(root, g);
      }
      const next = skyGroups.get(root);
      if (next === sky) return;
      skyScene.remove(sky);
      sky = next;
      skyScene.add(sky);
    });

    // builtin 67 (RaceRideState volumes): 22D6C8 turns free ride into that course's event (CTM free ride only, not in the backcountry
    // 14..16, not for the FreeRideState entries whose 0x445E40 value is 1): the course's objectives card
    freeRide.on('gate', ({ entry }) => {
      const cu = ui.careerUI,
        c = freeRide.course();
      // pv eventInWorld: the event runs
      if (worldEvent) return;
      if (!cu?.freeRide || course.freeRide.kind !== 4 || !running || isPaused()) return;
      if ([2, 3, 4, 5, 1, 1, 1, 0][entry] === 1 || c < 0 || c >= 14) return;
      const mode = cu.career.standardMode(c);
      if (mode == null) return;
      // in-world event start (web/ctm-event.js): the world runs on under the fade, then pauses for the fly-over
      if (cu.rideIn)
        cu.rideIn(mode, c, {
          pause: () => {
            // world state 1: the world stops under the NIS; no pause menu, the music plays on
            rumble.stop();
            clearInput();
            // pv worldUnderCuts: WS1 pushes no context on the PS2: the world ticks under the fly-over and the rider is held by its rider
            // actor from the list's first tick (caps/c0a-gate: control 13 from the gate + 31)
            if (pv('worldUnderCuts') && core?._nis_hold) {
              nisCut = true;
              nisCutHold(null);
              return;
            }
            ws1Hold ??= contexts.push(CTX.HOLD, { owner: 'WS1 ride-in' });
          }
        });
      else {
        rumble.stop();
        ws1Hold ??= contexts.push(CTX.HOLD, { owner: 'WS1 event transport' });
        clearInput();
        cu.transport(mode, c);
      }
    });
    freeRide.on('course', (c) => {
      gameAudio.freeRideCourse?.(c, { kind: course.freeRide.kind });
      // CTM: WS11/WS10 at the new location (new-career flag, visited mask, last lodge; career-ui.js courseChanged)
      if (course.freeRide.kind === 4) ui.careerUI?.courseChanged?.(c);
    });
    // the "?" contact (stage builtin 100 -> 1E3510; world state 4 then opens overlay 0x22): the Message Center on the Progression/Rewards
    // FAQ 1 over the paused ride (PS2 new career, docs/audio-logic.md 9.11)
    freeRide.on('faq', () => {
      const cu = ui.careerUI;
      if (worldEvent) return;
      // pv faqDefer: 1E3510 sets gp-0x1024 = 0 and marks the FAQ shown (1475C0) at the contact; world state 4's update opens overlay 0x22
      // at 0x2309A4 on a later tick when no offer prompt opened first (gp-0x1024 = -1 again at 0x2309F0), so a contact under the pause / a
      // prompt waits instead of being dropped
      if (pv('faqDefer')) {
        if (!cu?.freeRide) return;
        cu.me.faqShown = true;
        cu.career.persist();
        faqPending = true;
        return;
      }
      if (!cu?.freeRide || !running || isPaused() || !cu.messages?.ready) return;
      cu.me.faqShown = true;
      cu.career.persist();
      overlay.open();
      cu.messages.openFaq(() => {
        ui.set('game');
        ui.cb.resume();
      });
    });
    {
      let drawnKey = '';
      freeRide.on('drawn', (tracks) => {
        const key = tracks.join();
        if (key === drawnKey) return;
        drawnKey = key;
        lightGlow?.setTracks?.(new Set(tracks));
      });
    }
    freeRide.on('station', ({ action, course: station }) => {
      const cu = ui.careerUI;
      if (!cu?.freeRide || !running || worldEvent) return;
      cu.freeRide.station = station;
      overlay.open();
      // the walk-in / booth cut already played (free-ride.js -> web/cutscenes.js)
      if (action === 4) {
        ui.set('ctm-enterlodge');
        ui.index = 0;
        ui.sync();
      } else if (cu.openBooth)
        // the booth map on the ridden peak (web/career-ui.js)
        cu.openBooth(station);
      else {
        ui.set('ctm-peaks');
        ui.index = 2;
        ui.sync();
      }
    });
    // a connector's Load trigger after a riding crossing (world state 10 there; career-ui.js crossingArrived)
    freeRide.on('arrive', (c) => {
      if (course.freeRide?.kind === 4) ui.careerUI?.crossingArrived?.(c);
    });
    // out of this peak world into the next peak's station (web/free-ride.js, career-ui.js crossWorld)
    freeRide.on('crossWorld', (dest) => {
      const cu = ui.careerUI;
      if (cu?.freeRide && running) cu.crossWorld?.(dest);
    });
    // pv nisTick: the cut anchors' locators.json up front (nisHoldAt reads them synchronously at the door / booth)
    if (pv('nisTick')) {
      cutscenes?.ensureIndex?.()?.catch?.(() => {});
      for (const L of ['A', 'B', 'C', 'D', 'E']) for (const n of [148, 166]) cutscenes?.prefetchActor?.(n, L);
    }
    // world state 14 holds the ride under the door / booth cut (web/free-ride.js)
    freeRide.on('stationCut', (station) => {
      const cu = ui.careerUI;
      if (!cu?.freeRide || !running || isPaused()) return;
      // builtin 68's booth branch 0x302410 -> 2A49E8
      if (station?.action === 3) gameAudio.booth?.();
      // pv nisTick: the world runs on under the cut, the rider held at its anchor (123640); the prompt / map pauses ('station')
      if (pv('nisTick') && nisHoldAt(station)) {
        ui.set('game');
        return;
      }
      overlay.station ??= contexts.push(CTX.HOLD, { owner: 'WS14 station cut (no NIS hold)' });
      ui.set('game');
    });
    ui.freeRideHud = (c, level) => {
      if (!freeRide || worldEvent) return false;
      if (course.freeRide.kind !== 4) {
        // peak run HUD 0x1530C006|0x22: the clock counts down the tier limit, the station split for 5 s
        if (level < 2) {
          const ev = ui.careerUI?.career?.active?.ev,
            // as the core's peakSetup hook: the active event's row only when it is a peak run (a single event left active has no tier row:
            // TypeError every frame)
            setup =
              ev && ev.mode >= 6 && ev.mode <= 11
                ? ui.careerUI.career.peakSetup(ev)
                : ui.careerUI?.career
                  ? ui.careerUI.career.peakSetup({ mode: course.freeRide.mode, course: course.freeRide.course })
                  : null,
            ticks = Math.round((raceInfo?.[0] ?? 0) * 60);
          drawPeakRunHud(ui, c, { setup, raceTicks: ticks, split: freeRide.split() });
        }
        return 0x1530c026;
      }
      if (level < 2) {
        const car = ui.careerUI?.career,
          id = careerId(),
          now = freeRide.course();
        drawFreeRideHud(ui, c, {
          course: now,
          loaded: !!core._peak_world_course_ready?.(),
          collected: car && id ? car.collectCount(id, now) : 0,
          cash: car && id ? car.rider(id).cash : 0
        });
      }
      return bigChallenges ? bigChallenges.hudFlags(0x1530c380) : 0x1530c380;
    };
  } else if (new URL(location.href).searchParams.get('ai') !== '0') {
    globalThis.ssxBoot?.step('world', 1);
    try {
      // ?simtrace=1: the course world each rider's core loaded (core hashes; eventSlices keeps them)
      aiRace = await createAiRace({
        T,
        scene,
        human: core,
        course,
        loader,
        origin,
        humanName: () => selectedRider.name,
        environmentMeta,
        environmentBytes,
        lightAssets,
        // the riders' contexts copy the parse caches by key
        worldKeys: eventCut ? { ...eventCut.keys, hash: eventCut.hash } : null
      });
      if (simTrace && core._body_load_hash)
        simTrace.load = [core, ...(aiRace?.racers?.npcs ?? []).map((n) => n.core)].map((c) => [
          c._world_load_hash() >>> 0,
          c._body_load_hash() >>> 0,
          c._rail_load_hash() >>> 0
        ]);
      if (aiRace)
        opponentFx = await createOpponentFx({ scene, origin, count: aiRace.racers.npcs.length, sampleCore: core }).catch((e) => {
          console.warn('Opponent trails unavailable', e);
          return null;
        });
      if (aiRace && course.event === 'backcountry') rivalBeam = createRivalBeam({ T, scene, origin, texture: '/assets/UI/rival-beam.png' });
      if (aiRace)
        riderIcons = createRiderIcons({ T, scene, origin, count: aiRace.racers.npcs.length, texture: '/assets/UI/rival-exclaim.png' });
      if (aiRace && new URL(location.href).searchParams.has('qa'))
        Object.assign(window, { __opponentFx: opponentFx, __riderIcons: riderIcons });
    } catch (e) {
      console.warn('Computer riders unavailable', e);
      aiRace = null;
    }
    ui.cb.standings = aiStandings;
    ui.cb.lineup = aiLineup;
    // Conquer the Mountain relationship messages (web/career-messages.js)
    if (aiRace) aiRace.onRelationshipNotice = (character, score) => ui.careerUI?.messages?.notify(character, score);
  }
  const railTest = new URL(location.href).searchParams.get('railTest');
  if (railTest) {
    const record = railCatalog.rails.find((r) => r.name === railTest);
    if (!record) throw Error('Unknown authored rail fixture');
    const segment = record.segments[0].native,
      d = segment.end.map((v, i) => v - segment.start[i]),
      length = Math.hypot(d[0], d[2]);
    const direction = d.map((v) => v / length),
      position = segment.start.map((v, i) => v - direction[i] * 3.5);
    spawn = { position, heading: Math.atan2(direction[0], direction[2]), velocity: [direction[0] * 900, -direction[2] * 900, 0] };
  }
  const pickupTest = new URL(location.href).searchParams.get('pickupTest');
  if (pickupTest) {
    const id = Number(pickupTest);
    if (!settings.original_pickups.items.some((p) => p.resource === id)) throw Error('Unknown authored pickup fixture');
    const instance = worldPackage.instances.find((p) => ((p.rid << 8) | p.track) === id);
    const p = instance.matrix.slice(12, 15);
    spawn = { position: [p[0] / 100, p[2] / 100, -p[1] / 100], heading: spawn.heading, velocity: [0, 0, 0] };
  }
  const finishTest = new URL(location.href).searchParams.get('finishTest');
  if (finishTest) {
    if (finishTest !== '1') throw Error('Unknown finish fixture');
    const path = settings.original_race_event.original_race_event.paths.find((p) => p.events.some((e) => e.type === 1));
    if (!path) throw Error('Authored finish path missing');
    let distance = Math.max(0, path.events.find((e) => e.type === 1).start - 250);
    const point = path.origin.slice();
    let direction;
    for (const segment of path.segments) {
      direction = segment.slice(0, 3);
      const step = Math.min(distance, segment[3]);
      for (let k = 0; k < 3; k++) point[k] += direction[k] * step;
      if (distance < segment[3]) break;
      distance -= segment[3];
    }
    spawn = {
      position: [point[0] / 100, point[2] / 100, -point[1] / 100],
      heading: Math.atan2(direction[0], -direction[1]),
      velocity: direction.map((x) => x * 900)
    };
  }
  for (const p of allocations) core._free(p);
  snowRenderer = await createSnowRenderer(origin, core, { startfire: !!course.startfire });
  scene.add(snowRenderer.group);
  wakeRenderer = await createWakeRenderer(origin);
  scene.add(wakeRenderer.mesh);
  boostRenderer = await createBoostRenderer(origin);
  scene.add(boostRenderer.group);
  if (core._weather_info && new URL(location.href).searchParams.get('weather') !== '0') {
    weatherRenderer = await createWeatherRenderer({ core, origin }).catch((e) => {
      console.warn('Weather unavailable', e);
      return null;
    });
    if (weatherRenderer) scene.add(weatherRenderer.group);
  }
  animationReady = true;

  let posePtr = core._animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
  animPose = new Float32Array(core.HEAPF32.buffer, posePtr, bones.length * 7).slice();
  animInfo = new Float32Array(core.HEAPF32.buffer, core._animation_info(), 19).slice();
  posePhysicalFrame = new Float32Array(core.HEAPF32.buffer, core._pose_physical(), 12).slice();
  if (new URL(location.href).searchParams.get('originalFog') !== '0') {
    scene.traverse((o) => {
      if (o.isMesh) {
        for (const m of Array.isArray(o.material) ? o.material : [o.material]) m.fog = false;
      }
    });
    if (new URL(location.href).searchParams.get('originalWorld') !== '0')
      screenTint = await createScreenTint(course.root + 'screen-tint.json');
    if (new URL(location.href).searchParams.get('sun') !== '0') sunFlare = await createSunFlare(course.sunFlare);
    sunFlare?.setBand(widescreenView(ui.widescreen, 0).band);
    if (new URL(location.href).searchParams.get('glow') !== '0') {
      lightGlow =
        course.lightGlowCount === 0
          ? null
          : await createLightGlow(origin, course.lightGlow).catch((e) => {
              // ABC1 Happiness has no kind-7 lights (light-glow.json lights: [])
              console.info('No light glows:', e.message);
              return null;
            });
      lightGlow?.setBand(widescreenView(ui.widescreen, 0).band);
    }
    // world painter type 6 glare 36C790 (glare-pass.js; ?glare=0 disables; inert without a type-6 painter, e.g. ARA1)
    if (new URL(location.href).searchParams.get('glare') !== '0' && new URL(location.href).searchParams.get('originalWorld') !== '0')
      glarePass = await createGlarePass(course.root + 'glare.json').catch((e) => {
        console.warn('Glare pass unavailable', e);
        return null;
      });
    fogRenderer = createFogRenderer(
      renderer,
      scene,
      skyScene,
      camera,
      new URL(location.href).searchParams.get('fogStage') || '',
      snowRenderer,
      screenTint,
      sunFlare,
      lightGlow,
      glarePass
    );
    // Options > Display & Touch 'PS2 softness' (web/fog-renderer.js; before the warm-up so its frames build the chosen composite)
    fogRenderer.setSoftness?.(quality.ps2Output);
    // the original fog pass is the only fog. The traverse above clears m.fog on the meshes of this load only; materials
    // made later (streamed free-ride locations, static cells, set pieces) kept three's linear scene fog on top of it, fully fogged at the
    // painter's far (80 m at the ABA1 arrival: pale trees and signs). Detached, it stays the painter's bookkeeping.
    if (fogRenderer) {
      scene.userData.genericFog = scene.fog;
      scene.fog = null;
    }
  }
  worldRewarm = null;
  // pv worldWarm: switchCourse waits for it under the load screen (warmWorld)
  // the start row's pipelines for this course's world pass (it was built before the fog renderer; web/free-ride.js)
  if (course.freeRide && freeRide && fogRenderer) worldRewarm = freeRide.rewarm?.().catch((e) => console.warn('Peak rewarm failed', e));
  courseRoots = scene.children.filter((o) => !before.has(o) && !feOwned(o));
  // the session's Camera option (pause Options) on the new core
  if (core._set_camera_view && ui.cameraView !== 0x3d) core._set_camera_view(ui.cameraView);
  coreLoading = false;
  live = true;
  acrossBefore = null;
  // the scene before the switch / load (switchGate's held draw, the failed-load cleanup) is done with: kept, the Sets held the previous
  // course's whole scene and, through its closures, its core (docs/mobile.md "Hangs")
  loadBefore = null;
}
async function init() {
  const bootAt = performance.now(),
    boot = globalThis.ssxBoot;
  // the core compiles while the front end loads
  coreModule = compileCore();
  boot?.begin('core');
  coreModule.then(() => boot?.step('core', 1));
  await ui.load();
  // course reload into an event (?autostart=1): the original load screen covers the real asset load below
  if (new URL(location.href).searchParams.get('autostart') === '1') ui.loading.open();
  scene = new T.Scene();
  // The scene root stays at identity: do not force every static descendant to rebuild its world matrix each render.
  scene.matrixAutoUpdate = false;
  scene.background = new T.Color(0x7a9cb9);
  scene.fog = new T.Fog(0x8da7bd, 170, 380);
  camera = new T.PerspectiveCamera(66, 4 / 3, 0.1, 800);
  boot?.begin('gpu');
  globalThis.__ssxThreeLean = pv('threeLean');
  if (pv('worldWarm')) installYieldShim();
  // pv gcWatchdog: JSC's full collections stopped (the WebKit memory runaway, docs/mobile.md "Hangs") -> ask for one
  if (pv('gcWatchdog') && isJavaScriptCore())
    window.__gcWatchdog = startGcWatchdog({ isSafe: () => !!(ui.loading?.active || isPaused() || cutscenes?.active) });
  // pv worldWarm: three's compile yields without a frame per item in WebKit (web/yield-shim.js)
  // web/three-patches.js frame8: 8-bit world / sky pass targets with the encoded frame (web/frame-space.js)
  renderer = new T.WebGPURenderer({
    forceWebGL: new URL(location.href).searchParams.get('backend') === 'webgl',
    canvas: $('#game'),
    antialias: quality.antialias,
    alpha: true,
    stencil: true,
    outputBufferType: frameBufferType
  });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5));
  layoutStage();
  await renderer.init();
  boot?.step('gpu', 1);
  // no three.js lights: no per-draw lights cache key (web/pv-flags.js sceneLightingOff)
  if (new URL(location.href).searchParams.get('originalWorld') !== '0') renderer.lighting.enabled = false;
  diagnoseRenderer(renderer);
  configureTextureDecode({ renderer });
  // Xbox HD rider textures: BC blocks uploaded when the device has texture-compression-bc (web/texture-archive.js) the rider shadow atlas a
  // render target before anything samples it (web/rider-shadow.js)
  // a lost GPU device (iOS: reason "destroyed" in the background) gets a new one on the same renderer; the page reloads if that fails, then
  // the title card says so (web/gpu-recovery.js)
  initRiderShadowAtlas(renderer);
  {
    const recovery = installDeviceRecovery(renderer, {
      report: diagnose,
      beforeResume: async () => {
        await restoreGpuCopies();
        // the new device's shadow atlas before anything samples it
        initRiderShadowAtlas(renderer);
      },
      onRecovered: () => diagnoseRenderer(renderer, { recovered: true }),
      onFailed: () => {
        ui.error = 'Graphics lost. Reload the page';
        ui.ready = false;
        ui.set('title');
        ui.draw({});
      }
    });
    if (new URL(location.href).searchParams.has('qa')) window.__gpuRecovery = recovery;
  }
  // QA: profiling scripts hook the node builder and inspect the scene
  if (new URL(location.href).searchParams.has('perf')) {
    window.__perfRenderer = renderer;
    // camera, origin: visual-parity tools pin the drawn view (local/browser-validation/visual-parity/tools/vpworld.mjs)
    window.__perfScene = () => ({ scene, worldScene, camera, origin });
    window.__perfShadows = () => riderShadows;
    window.__perfUI = () => ui;
  }
  renderer.toneMapping = T.NoToneMapping;
  configureFrameSpace(renderer);
  ui.error = 'Loading...';
  ui.draw({});
  courses =
    (
      await load('/assets/courses.json').catch((e) => {
        if (e?.network) throw e;
        // offline after web/downloads.js's retries: the title card says so
        return null;
      })
    )?.courses || courses;
  // backcountry: Single Event 'Happiness' (Rival Time) and 'Happiness Jam' (Rival Points), docs/backcountry.md
  ui.courses = courses.flatMap((c) =>
    c.event === 'backcountry'
      ? [
          { ...c, rivalMode: 4 },
          { ...c, name: c.name + ' Jam', rivalMode: 5 }
        ]
      : [c]
  );
  let params = new URL(location.href).searchParams,
    first = courseFor(params);
  if (!first && params.get('course')) {
    // a course this build's courses.json does not have (an old link, or the page loaded while a deploy swapped the data: field reports
    // CRA3/ERA5): Snow Jam and the menus, not a dead title card
    console.warn('Unknown or unprepared course ' + params.get('course') + ' (web/prepare.py --location): Snow Jam instead');
    diagnose('course-unknown', { code: params.get('course') });
    const u = new URL(location.href);
    for (const k of ['course', 'peakCourse', 'peakMode', 'autostart', 'online', 'lobby']) u.searchParams.delete(k);
    try {
      history.replaceState(history.state, '', u);
    } catch {}
    params = u.searchParams;
    first = courseFor(params);
    ui.loading.cancel?.();
    // no event to load: the title card as on a plain visit
    if (ui.screen === 'loading') ui.set('title');
  }
  if (!first) throw Error('Unknown or unprepared course ' + (params.get('course') || 'ARA1') + ' (web/prepare.py --location)');
  if (pv('lazyCourse') && params.get('autostart') !== '1') {
    await coreModule;
    // the core compile is part of the front end (the preview core, then the course)
    openBootGate();
    // (bootChain: the lazy course loads on the chain)
    lazyStart(first, params);
    // the title and the menus first, the course behind them
  } else {
    bootKey = courseKeyOf(params);
    switchBusy = true;
    try {
      await loadCourse(first);
    } finally {
      switchBusy = false;
    }
    loadedKey = courseKeyOf(params);
  }
  setupCutscenes();
  ready = true;
  ui.ready = true;
  ui.sync();
  boot?.ready();
  try {
    diagnose('boot', lazyBoot ? { ...boot?.timeline?.(), lazy: true } : boot?.timeline?.());
  } catch {}
  window.__courseLoads.push({
    code: course.code,
    boot: true,
    ...(lazyBoot ? { lazy: true } : {}),
    ms: Math.round(performance.now() - bootAt)
  });
  {
    // 30 fps presentation on request (web/quality.js): the 60 Hz game clock runs two ticks per drawn frame
    let drawNow = frameGate(quality.fps, { refreshCap: pv('refreshCap') });
    onQualityChange((q) => {
      drawNow = frameGate(q.fps, { refreshCap: pv('refreshCap') });
      layoutStage();
      applyAntialias();
      fogRenderer?.setSoftness?.(q.ps2Output);
    });
    boot?.handoff();
    // the frame loop draws the title from here (ui.js -> ssxBoot.draw)
    renderer.setAnimationLoop((ms) => {
      if (!drawNow(ms)) return;
      const t0 = performance.now();
      frame(ms);
      drawNow.done(performance.now() - t0);
      // warmSpread (warmupRender): the drawn frame's work
      if (drawnWaiters.length) {
        const work = performance.now() - t0;
        for (const w of drawnWaiters.splice(0)) w(work);
      }
    });
    window.__frameRate = () => drawNow.rate;
  }
  // FE preview models compile before they show (web/fe-preview.js)
  ui.characterSelect.preview3d.compile = (g) => {
    // upload its textures now too (the first draw would)
    g.traverse((o) => {
      for (const m of [].concat(o.material || [])) if (m?.map) renderer.initTexture(m.map);
    });
    return renderer.compileAsync(g, camera, scene);
  };
  {
    const params = new URL(location.href).searchParams,
      // no ?rider and no saved pick (ui.restoreRider): the roster's first rider, index 0 on Select Character
      rider = params.get('rider') || ui.riders[0]?.id || bootRider().id;
    // the boot model is RIDER_SAM on initial.json: load the chosen rider (Sam included) with its own settings
    if (rider === bootRider().id && ui.riders.some((r) => r.id === rider && r.settings)) {
      selectedRider = { ...selectedRider, id: '' };
      loadedRider = { ...loadedRider, id: '' };
    }
    if (rider && rider !== selectedRider.id) {
      // &base=<rider>: a cheat skin on that base rider (docs/characters.md)
      const found = ui.riders.find((r) => r.id === rider),
        entry = found?.kind === 'cheat' && params.get('base') ? { ...found, base: params.get('base') } : found;
      await selectRider(entry || { id: rider, name: rider[0].toUpperCase() + rider.slice(1), package: 'RIDER_' + rider.toUpperCase() });
      if (entry) ui.characterSelect?.adopt(entry);
    }
    if (
      // a switch asked for during the boot load replaces the boot course's event
      params.get('autostart') === '1' &&
      !(bootGate && switchTarget)
    ) {
      // a career/single-event round resumes at its objectives screen (web/career-ui.js)
      if (!ui.careerUI?.resume())
        ui.loadEvent(() => {
          ui.set('game');
          startRun();
        });
    }
  }

  // online multiplayer: lobby screens + race glue
  mpGame = createMpGame({
    T,
    scene,
    load,
    loader,
    origin,
    ui,
    getCore: () => core,
    getState: () => state,
    isRunning: () => running,
    isFinished: () => finished,
    resultTicks: () => {
      const r = new Float32Array(core.HEAPF32.buffer, core._race_result_info(), 6);
      return Math.round(r[1]);
    },
    rider: () => selectedRider,
    course: () => course,
    startRun,
    lighting: mpLighting,
    camera: () => camera,
    // lobby on another course: switch in the page, keeping the socket and the seat
    switchCourse: (url) => navigateCourse(url, { after: 'lobby' }),
    courseReady: () => {
      if (lazyPending()) {
        ui.loading.open();
        return courseLive();
      }
      return switchChain;
    },
    pendingCourse: () => switchTarget?.url.searchParams.get('course') ?? null,
    // hidden tab (no animation frames): the online race keeps simulating from a worker timer
    advance: () => {
      if (!ready || !running || mpGame.holding()) return;
      const now = performance.now(),
        dt = Math.max(0, (now - last) / 1000);
      last = now;
      const input = finished || isPaused() ? NEUTRAL_PAD : inputs();
      gameTick.advance(simulation, mpGame.pace(dt, simulation.pending), input);
      acc = simulation.pending;
      flushHumanSkin();
      mpGame.afterFrame(true);
    }
  });
  ui.mpUI = new MultiplayerScreens(ui, mpGame.session);
  if (new URL(location.href).searchParams.has('perf')) {
    window.__perfMp = mpGame;
    window.__perfAudio = gameAudio;
    window.__perfOpponentFx = () => opponentFx;
    window.__perfCore = () => ({ core, state, keyboardPad, input: inputs() });
  }
  if (new URL(location.href).searchParams.has('qa'))
    publishQA({
      // QA: the front-end/HUD object (draw a given HUD state, career flow)
      ui() {
        return ui;
      },
      rider(id) {
        return selectRider(
          ui.riders.find((r) => r.id === id) || { id, name: id[0].toUpperCase() + id.slice(1), package: 'RIDER_' + id.toUpperCase() }
        ).then(ensureRider);
      },
      ensureRider() {
        return ensureRider();
      },
      loadedRider() {
        return loadedRider;
      },
      humanSettings() {
        return animationResources.settings;
      },
      start() {
        ui.set('game');
        overlay.drop();
        startRun();
        // the simulation only: cutscenes and the NIS play on (the tools' cutscene probes)
        overlay.open('game', { owner: 'QA start (the paused ride, stepped by advance; cb.resume releases it)', ends: null, ctx: CTX.HOLD });
      },
      advance(ticks, pad) {
        const input = pad ? Float32Array.from(pad) : new Float32Array(24);
        qaStepping = true;
        // the ticks run whatever holds the world (host.paused)
        try {
          for (let i = 0; i < ticks; i++) simTick(input);
        } finally {
          qaStepping = false;
        }
        acc = 0;
        return Array.from(state);
      },
      hud(show) {
        $('#ui').style.visibility = show ? 'visible' : 'hidden';
      },
      screenTint() {
        return screenTint?.state;
      },
      sun() {
        return sunFlare?.state;
      },
      glow() {
        return lightGlow?.state;
      },
      glare() {
        return glarePass?.state;
      },
      eventDead(visible) {
        for (const mesh of eventDeadMeshes) mesh.visible = visible;
      },
      setPieces() {
        return setPieceRenderer;
      },
      // the race replay (web/replay.js)
      replay() {
        return replay;
      },
      // the computer riders (their core views: web/ai-racers.js)
      aiRace() {
        return aiRace;
      }
    });
  window.addEventListener('resize', layoutStage);
  // touch deck / visual viewport changes resize the stage without a window resize
  new ResizeObserver(() => layoutStage()).observe($('#stage'));
  try {
    history.replaceState({ ...(history.state || {}), ssx3: 1 }, '');
  } catch {}
  // Back / Forward: in-app course changes (navigateCourse; bootChain: listening from the start)
  // the switches asked for during the boot run now
  openBootGate();
}
init().catch((e) => {
  // a C++ exception from another core instance (a boot / FE core, or a core.js and core.wasm of two builds) has no tag in this core:
  // getExceptionMessage threw here, the handler died and the page sat at 'loading' for good
  let detail;
  try {
    detail = e instanceof Error ? e.message : core?.getExceptionMessage ? core.getExceptionMessage(e).join(': ') : String(e);
  } catch {
    detail = String(e);
  }
  document.body.dataset.loadError = detail;
  console.error(e);
  const failText = e?.network
    ? // web/downloads.js gave up after its retries
      'Connection lost. Reload the page'
    : 'Load failed';
  ui.error = failText;
  globalThis.ssxBoot?.fail(failText);
  ui.ready = false;
  if (ui.screen === 'loading') ui.set('title');
  ui.draw({});
});
