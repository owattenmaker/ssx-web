import { FixedStepClock } from './fixed-step-clock.js';
// The game's 60 Hz tick (docs/workers.md "Game tick"). Per-tick exactness trace for the running game (?simtrace=1).
// FNV-1a over the human's tick outputs (rider state, posed physical frame, race info, camera), every rider's state /
// pose / world state, the shared visual and game RNG words and the human's score object: the words a change to the
// game loop must reproduce bit for bit (QA: traces before / after the change).
export const TRACE_OUT_FLOATS = 45; // rider_state 16, pose_physical 12, race_end 8, camera 9
import { pv } from './pv-flags.js';

export function gameTrace(human, npcCores, out) {
  let h = 0x811c9dc5 >>> 0;
  const mix = (u32) => { for (let i = 0; i < u32.length; i++) h = Math.imul(h ^ u32[i], 16777619) >>> 0; };
  mix(new Uint32Array(out.buffer, out.byteOffset, out.length));
  for (const c of [human, ...npcCores]) {
    mix(new Uint32Array(c.HEAPU8.buffer, c._rider_state(), 16)); mix(new Uint32Array(c.HEAPU8.buffer, c._pose_physical(), 12));
    if (c._rider_world_state) mix(new Uint32Array(c.HEAPU8.buffer, c._rider_world_state(), 8));
  }
  mix(new Uint32Array(human.HEAPU8.buffer, human._visual_rng_words(), 6));
  if (human._animation_rng_words) mix(new Uint32Array(human.HEAPU8.buffer, human._animation_rng_words(), 6));
  if (human._score_object_dump) mix(new Uint32Array(human.HEAPU8.buffer, human._score_object_dump(), 0x1d0 / 4));
  return h;
}
// The tick outputs of the trace: state (after the animation tick), posed physical frame, race info, camera pose.
export function traceOut(out, { state, posePhysicalFrame, raceInfo, cameraPose }) {
  out.fill(0);
  if (state) out.set(state.subarray(0, 16), 0);
  if (posePhysicalFrame) out.set(posePhysicalFrame.subarray(0, 12), 16);
  if (raceInfo) out.set(raceInfo.subarray(0, 8), 28);
  if (cameraPose) out.set(cameraPose.subarray(0, 9), 36);
  return out;
}
// FNV-1a of the whole wasm memory (every byte of every rider, presentation caches included).
export function memoryHash(core) {
  const u = new Uint32Array(core.HEAPU8.buffer, 0, core.HEAPU8.byteLength >> 2);
  let h = 0x811c9dc5 >>> 0; for (let i = 0; i < u.length; i++) h = Math.imul(h ^ u[i], 16777619) >>> 0;
  return { hash: h, bytes: core.HEAPU8.byteLength };
}

// Score object HUD message bank (0x1488DE0 layout: 44 slots x {type,maximum,value,arg,field10,points}) + total (+0x198)
// for web/trick-hud.js.
export function readTrickHudSlots(core) {
  if (!core?._score_hud_slots) return null;
  const raw = new Float32Array(core.HEAPF32.buffer, core._score_hud_slots(), 44 * 6 + 4), slots = new Array(44).fill(null);
  for (let k = 0; k < 44; k++) {
    const o = 6 * k, type = raw[o]; if (type === 0x34) continue;
    const points = raw[o + 5];
    slots[k] = { type, maximum: raw[o + 1], value: raw[o + 2], arg: raw[o + 3], field10: raw[o + 4], points,
      text: (type === 0 || type === 0xE) ? ((p) => { let t = ''; for (let i = 0; i < 0x80 && core.HEAPU8[p + i]; i++) t += String.fromCharCode(core.HEAPU8[p + i]); return t; })(core._score_hud_text(k)) : String(points) };
  }
  return { slots, total: raw[44 * 6] };
}

export const POST_FINISH_TICKS = 409, RESULTS_TICKS_FINISH = 408, RESULTS_TICKS_TIME_UP = 288;
// pv finishSkip (docs/ctm-decomp-world-states.md): the finish-standings overlay 0xC (race, 1E8098) / 0xD (freestyle) opens 180 ticks after
// WS5 enter (= the port's finish + 3); its update and events start once its transition-in ends (WS5 + 225), and a NEW Cross edge from then
// (UI event 5, 1E8160 with +0xA0 set) closes it: 39F840, WS_advance 0x231320 -> WS12 (a podium list queued) or WS7, the results on that tick.
// A Cross held from before does not count (the 0x321298 edge rule). ARMSX2 race-q fin-mash-cross: accepted at WS5 + 230, WS7 at + 231.
export const FINISH_SKIP_FROM = 228;
const NO_STICKS = [0, 0, 0];
const f32 = (core, ptr, n) => new Float32Array(core.HEAPF32.buffer, ptr, n);
const trickNames = new TextDecoder();

// The 60 Hz game tick (docs/workers.md "Game tick"):
//   simulate(input): every core call of one tick, in the original order (main.js simTick until 2026-09-26); returns
//     the tick record for present() (null: the streamed world waits for its data, 2306B8).
//   present(rec): what the page does with a tick: HUD pre-pass, rider frame, skin / lighting uploads, screen tint /
//     sun / glare / lightning, terrain refinement, trick name, career run end, results, rumble, audio, collectibles.
// host: getters and setters over main.js's game state and its collaborators; `rec.core` is the core present() reads.
export function createGameTick(host, { trace = null } = {}) {
  const teleports = new WeakMap(); // core -> its stage teleport count (web/stage_teleport.inc stage_teleport_info[0])
  function simulate(input, ticksLeft = null) {
    const s = host, core = s.core;
    if (s.freeRide?.stalled()) { s.freeRide.tick(); return null; } // streamed world: +0x1D0 wait (2306B8 skips the gameplay part of the frame; the streamer still runs, web/free-ride.js)
    s.nisStart?.(); // pv nisAfterScan: the NIS tick (0x230BE4: a rider actor's start, 123640) before every pass of this game tick (main.js)
    // One consumed PS2 pad sample per 60 Hz tick: original history, INPUT.MAP and provider 0x127998.
    // HUD pre-pass state (0x1EA930: orb/coil palette + flash phase, Uber-hint gating) is one tick behind the slot draws (PS2
    // uber-chain frame 1557: total 5850 with the tier-4 gold coil).
    const rec = { core, ticksLeft, clock: s.clock, prepassBoost: f32(core, core._boost_hud_info(), 13).slice(), prepassSlots: s.wantPrepassSlots ? readTrickHudSlots(core)?.slots ?? null : undefined };
    s.replay?.record(input);   // the run's pad for its replay (web/replay.js; ignored while a replay plays)
    core.HEAPF32.set(input, s.padPtr >> 2);
    s.worldAiBefore?.(); // (pv eventReturnInWorld: WS15's riders leave at the WS4 restart, before this tick's passes; main.js)
    if (s.aiActive) s.aiRace.beginTick(); else { s.soloTickStart?.(core); s.mpGame?.beginTick(); }   // soloTickStart: a solo event's anchor RNG (main.js, pv eventAnchorRng)
    const cmd = f32(core, core._pad_tick(s.padPtr), 24).slice();
    rec.cmdAir = cmd[14] === 4 || cmd[14] === 5; // controllers 4 passive air, 5 air: Simple keyboard mode routes new direction presses to the D-pad
    core._race_begin();
    s.state = f32(core, core._step_rider(cmd[0], cmd[6], cmd[2] ? 1 : 0, cmd[7]), 16).slice();
    if (s.state[13] !== s.lastRescues) { s.lastRescues = s.state[13]; rec.rescue = true; s.pending = 0; if (s.animationReady) core._reset_animation(); }
    if (s.animationReady) {
      const ptr = core._animation_tick(s.state[7], cmd[10], cmd[11], s.state[9], +s.state[8], cmd[6], cmd[8], cmd[7], cmd[7], 0, s.state[15], cmd[13]);
      s.animPose = f32(core, ptr, s.boneCount * 7).slice(); s.animInfo = f32(core, core._animation_info(), 19).slice();
      s.posePhysicalFrame = f32(core, core._pose_physical(), 12).slice(); s.state = f32(core, core._rider_state(), 16).slice();
      const resetState = f32(core, core._reset_info(), 8);
      if (resetState[2] !== s.lastResetPlacement) { s.lastResetPlacement = resetState[2]; rec.placement = true; }
      s.pending = s.animInfo[12];
      if (s.animInfo[13] > 0) { s.score += s.animInfo[13]; s.trickUntil = s.clock + 2; }
    }
    // course events (rider finish 0x125108 -> camera 0x162258) precede the camera update
    s.raceInfo = f32(core, core._race_end(), 8).slice();
    // Stage builtin 34 (a Metro-City phone booth / water tower, pv boothTeleport, web/stage_teleport.inc) placed the rider inside
    // race_end (121818 -> 0x123210): the placed state and pose (the camera follows the new head), and no interpolation across.
    if (core._stage_teleport_info) { const n = f32(core, core._stage_teleport_info(), 1)[0];
      if (n !== teleports.get(core)) { if (teleports.has(core) && n > 0) { rec.placement = true; s.lastResetPlacement = f32(core, core._reset_info(), 8)[2]; s.state = f32(core, core._rider_state(), 16).slice(); if (s.posePhysicalFrame) s.posePhysicalFrame = f32(core, core._pose_physical(), 12).slice(); } teleports.set(core, n); } }
    if (!s.replay?.active) s.rideTick?.(s.state, !!(rec.placement || rec.rescue)); // field stats: distance ridden (main.js gameHost -> diagnostics.js diagRide; reads only)
    s.freeRide?.tick(); s.bigChallenges?.tick(); s.faqTick?.(); s.worldAiTick?.(); // (pv eventInWorldAi: the event's riders held at their NIS actors under WS1, main.js) // world state 4: the offer read 0x230890, then the deferred FAQ 0x2309A4 (pv faqDefer)
    if (s.aiActive) {
      s.aiRace.endTick();
      // 0x2D4C08 per game tick: relationship icons over the computer riders (level = their record about the human, 3 for the peak rival)
      rec.relations = s.aiRace.racers.npcs.map((n) => s.aiRace.racers.relation?.(n.slot, 0)?.score ?? 0);
    } else { core._fx_pass?.(-1); /* the rider FX pass after 121818 (deferred, docs/visual-rng-order.md) */ core._section_pass?.(); s.mpGame?.endTick(); }
    // HUD owner update 0x20ED20 after every rider (web/progress-meter-hud.js); section activation 0x101B60 after every rider (ai-racers.js endTick with computer riders)
    if (s.progressMeter) rec.meter = s.progressMeterRiders(core, s.aiActive ? s.aiRace.racers.npcs : []);
    if (s.animPose) {
      if (!s.posePhysicalFrame?.[7]) throw Error('Missing posed physical frame');
      rec.camera = f32(core, core._step_camera_head(s.posePhysicalFrame[9], s.posePhysicalFrame[10], s.posePhysicalFrame[11]), 9).slice();
      // a replay draws through its replay camera (web/replay_camera.inc), stepped from this tick's camera input
      if (s.replay?.active && core._replay_camera_step) { const m = s.replay.manual ?? NO_STICKS; rec.replayCamera = f32(core, core._replay_camera_step(0, m[0], m[1], m[2]), 10).slice(); }
    }
    if (s.animationReady) { s.pending = f32(core, core._animation_info(), 19)[12]; const name = core._trick_name(); rec.trick = trickNames.decode(core.HEAPU8.subarray(name, core.HEAPU8.indexOf(0, name))); }
    if (s.raceInfo[2]) {
      s.finished = true; s.postFinishTicks = 0;
      const r = f32(core, core._race_result_info(), 6), dump = Uint32Array.from(new Uint32Array(core.HEAPU8.buffer, core._score_object_dump(), 0x1d0 / 4));
      rec.finish = { score: dump[0x198 / 4] | 0, ticks: r[1], dnf: !!core._race_timed_out?.() }; rec.finishDump = dump; s.replay?.finish();
      // rival challenges (23B8C8/23BDB8): only the winner (place 0) celebrates; rider+0x100: the race results handler 0x23A760 clears it for
      // a human placing 4th or worse (place array >= 3), picking the finish reaction 314 over 315
      // freestyle (pv fsCelebrate): 0x239230 ranks the run with the posted scores and sets rider+0x100 for the top three
      const fsPlace = s.freestylePlace?.(rec.finish.dnf ? 0 : rec.finish.score) ?? null;
      if (fsPlace != null) { if (!core._finish_standing) core._finish_celebrate?.(fsPlace < 3 ? 1 : 0); } // a newer core asked main.js's finishHost inside race_end (0x239230 meter too)
      else if (s.aiActive) core._finish_celebrate?.((s.aiRace.hud()?.place ?? 0) < (s.backcountry ? 1 : 3) ? 1 : 0);
    }
    // EndRace requests the results, but the original shows them (and starts the replay) only when the finish HUD is done: 408 ticks after the
    // finish, 288 after a TIME'S UP (+0x480: 125228 timeout or pause Give Up); PS2 pipe-finishov/pipe-brake/pipe giveup captures, docs/career-events.md
    if (s.raceInfo[3]) s.resultsPending = true;
    // the Cross channel of the pad history (0x321298: after an edge the next three updates ignore the input), for pv finishSkip
    const cross = s.finishCross ??= { held: 0, age: 0 }; let crossPressed = false;
    if (cross.age < 3) cross.age++; else { const held = input[10] > 0 ? 1 : 0; if (held !== cross.held) { cross.held = held; crossPressed = held === 1; cross.age = 0; } }
    const skip = crossPressed && s.resultsPending && s.finished && s.postFinishTicks + 1 >= FINISH_SKIP_FROM && pv('finishSkip');
    if (s.resultsPending && s.finished && (skip || s.postFinishTicks + 1 >= (core._race_timed_out?.() ? RESULTS_TICKS_TIME_UP : RESULTS_TICKS_FINISH))) {
      s.resultsPending = false;
      const result = f32(core, core._race_result_info(), 6); if (!result[0]) throw Error('Results requested without finish record');
      rec.results = { ticks: result[1], dnf: !!core._race_timed_out?.(), rows: s.aiActive ? s.aiRace.results(result[4]) : undefined, standings: s.aiActive ? s.aiRace.standings() : undefined };
    }
    rec.place = s.aiActive ? s.aiRace.hud()?.place : 0;
    // the run's replay highlights (web/replay.js observe): take-off / landing / crash / forced reset of the human
    if (s.replay && !s.replay.active) s.replay.observe(!!s.state[8], s.score, !!f32(core, core._crash_info(), 1)[0], !!f32(core, core._reset_info(), 1)[0]);
    s.clock += 1 / 60;
    // the original keeps the finish camera (POST_RACE_1) running behind the results until its replay starts 409 camera updates after the finish
    // pv eventReturnInWorld (main.js gameHost.liveStopAt): an in-world event has no page replay behind its results, and the PS2's live ticks
    // end where its auto replay starts (0x2706B8: finish + 288 TIME'S UP / + 408 FINISH, counting the finish tick; PS2 c0a-ret2 records
    // 3032..3320, the Give Up's coast), whose results-time state 0x2706F0 restores at the Transport
    if (s.finished && ++s.postFinishTicks >= (s.liveStopAt?.(!!core._race_timed_out?.()) ?? POST_FINISH_TICKS)) s.running = false;
    if (trace) trace(rec);
    return rec;
  }
  let goPhase = null; // the race phase (race_end [5]) of the last presented tick
  function present(rec) {
    const p = host, core = rec.core;
    // A replay (web/replay.js) re-simulates the run: its ticks draw the world, the riders, the camera and the sound effects;
    // no HUD, rumble, pad, career, collectibles or results (0x1E9A30 returns at 0x1EC97C, 0x127968, 0x1E1550..0x1E2A08 while
    // the replay manager state is 1..9).
    const replaying = !!p.replay?.active;
    p.hudPrepass = { boost: rec.prepassBoost };
    // The countdown's GO (world state 3 234C68, the tick the race phase goes 4 -> 5, with the GO sounds of web/sfx-game.js): HUD
    // command 6 (0x1EC378) starts big message 8 "GO!" before that frame's HUD update 0x1EA930 advances it. Rolling starts have none.
    const phase = p.raceInfo?.[5] ?? null; if (!replaying && phase === 5 && goPhase === 4) p.ui.trickHud?.command(6); goPhase = phase;
    if (!replaying && p.ui.trickHud) p.ui.trickHud.prepassSlots = rec.prepassSlots ?? null;
    if (!replaying) p.keyboardPad.air = rec.cmdAir;
    if (rec.rescue || rec.placement) { p.currentRiderFrame = p.previousRiderFrame = null; p.cameraPose = p.previousCameraPose = null; }
    if (rec.relations && !replaying) p.riderIcons?.tick(rec.relations);
    if (rec.meter && !replaying) p.progressMeter?.tick(rec.meter);
    if (rec.camera) {
      p.previousCameraPose = p.cameraPose; p.cameraPose = rec.replayCamera ?? rec.camera;   // a replay draws through its replay camera
      if (rec.replayCamera && p.previousCameraPose && p.previousCameraPose[9] !== rec.replayCamera[9]) p.previousCameraPose = rec.replayCamera;   // a cut: no blend
      p.screenTint?.step(core); p.sunFlare?.tick(core); p.glarePass?.tick(core);
      if (core._weather_lightning) { // a lightning strike (web/weather.inc 0x390EC8): the thunder after the distance delay (audio-world.js 291438)
        const L = f32(core, core._weather_lightning(), 10);
        if (L[7] > p.lightningStrikes) p.audioSafe(() => p.gameAudio.thunder?.(L[5])); p.lightningStrikes = L[7];
      }
    }
    if (p.animPose) {
      p.previousRiderFrame = p.currentRiderFrame; const displayState = p.state.slice(); displayState[8] = p.posePhysicalFrame[8];
      p.currentRiderFrame = p.captureRiderFrame(displayState, p.animPose, p.animInfo, p.rig.bones.map((b) => b.parent), p.origin, p.posePhysicalFrame.subarray(3, 7), p.posePhysicalFrame.subarray(0, 3));
      // presentation only: a frame draws the palettes of its last two ticks (as the computer riders', web/ai-race.js); a reset (no previous
      // frame) stays pending until the next capture, a frame whose last ticks did not run captures after the clock
      p.humanSkinReset ||= !p.previousRiderFrame;
      if (rec.ticksLeft == null || rec.ticksLeft <= 2) { p.sam.userData.sourceSkin?.capture(core, p.humanSkinReset); p.humanSkinReset = false; p.humanSkinPending = false; } else p.humanSkinPending = true;
      if (p.sam.userData.sourceLighting) { p.riderLightingUpdate.update(); p.sam.userData.sourceLighting.capture(core); }
    }
    if (p.terrainRefinement) p.terrainRefinement.update(Array.from(p.state.slice(0, 3)), f32(core, core._terrain_contact_info(), 12)[0], rec.clock);
    if (rec.trick !== undefined && !replaying) p.trick = rec.trick;
    if (rec.finish && !replaying) p.careerRunEnd(rec.finish, rec.finishDump);
    if (rec.results && !replaying) { p.leavePause(); p.clearInput(); p.ui.showResults({ rider: p.selectedRider.name, ...rec.results }); p.replay?.resultsShown(); }
    // pad vibration: this step's rumble calls (core audio events 30/31) + the 0x125B18 decay, before sfx-game drains the queue (web/rumble.js)
    p.rumble.tick(core, !replaying && !p.finished && (p.ui.feScreens?.vibration?.() ?? false));
    // game audio tick: core audio events, board loops, crowd, world sounds, painters, speech (web/game-audio.js)
    p.audioSafe(() => p.gameAudio.gameTick({ core, ai: p.aiActive ? p.aiRace : null, raceInfo: p.raceInfo, finished: p.finished, pending: p.pending, character: p.selectedRider?.id, place: rec.place }));
    if (!replaying) p.collectPoll(core, { careerMode: p.ui.careerMode, career: p.ui.careerUI?.career, riderId: p.careerId ?? p.selectedRider?.id, courseCode: p.course.code });   // careerId: a cheat skin's base rider (main.js, pv lodgeCheats)
    else { core._stage_collect_events?.(); core._score_career_events?.(); }   // drained, not paid: the live run already paid them
  }
  // One tick of the running game (the frame clock, ssxQA.advance, the online hidden-tab ticker).
  function tick(input, ticksLeft = null) {
    if (!host.running || host.paused && !host.mpGame?.simulating()) return;
    const rec = simulate(input, ticksLeft);
    if (rec) present(rec);
  }
  // A frame's ticks on the fixed 60 Hz clock (web/fixed-step-clock.js): the same input for each, or input(k) for the frame's k-th tick
  // (pv stallKeys: main.js stallKeyInput); returns the tick count.
  function advance(clock, seconds, input) { let k = 0; return clock.advance(seconds, () => tick(typeof input === 'function' ? input(k++) : input, FixedStepClock.ticksLeft)); }
  return { simulate, present, tick, advance };
}
