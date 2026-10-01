// Rollout switches (AGENTS.md "Shared tree"). The tree deploys as it stands, so a change that is not verified yet sits behind
// a switch here, default off, until it is checked against the PS2 (frames, captures) and in WebKit and Chrome; then its default flips,
// and once it has been on through a few deploys the switch and its off path are deleted (2026-09-30 cleanup: 216 -> 40).
//   ?pv=1              every switch on          ?pv=0   every switch off
//   ?pv=name,-other    the defaults, with name on and other off (a name turns it on, -name turns it off)
//   SSX_PV=name,-other the same for node harnesses; setPv(name, on) in tests
// pv() throws on an unknown name: add a switch to PV_DEFAULTS before any code reads it.
//
// speechRange (memory, docs/audio-logic.md 9.15): a speech line loads as its own byte range of the bank's .dat (a Range request, like
// musicStream) into a small LRU, fetched when the line is resolved or predicted, instead of the whole .dat kept for the session
// (DJ_Hub_Char_Stories_eng.dat alone is 35 MB).
// flyover (web/ctm-event.js prefetch, web/downloads.js; docs/presentation.md "Hold after the fly-over"): the event's package downloads
// under the fly-over, so no black hold follows it. Off: measured +70-80 MB on the phone-class peak (docs/presentation.md section 3).
// lazyCourse (not a presentation item: the first load, docs/first-load.md): the title and the menus start with the front end
// only; the page's course loads behind them from Press START (web/main.js lazyStart / backgroundCourse, web/boot-screen.js). On:
// measured and verified in Chrome and WebKit (docs/first-load.md "Title before the course").
// refreshCap (presentation rate, web/quality.js frameGate): on a display faster than 60 Hz the 60 fps gate draws every n-th animation frame
// (n = floor(Hz / 60 + 0.1)): the frames in between redrew the same 60 Hz ticks, interpolated.
// rideWarm (loading, docs/ctm-flow.md "Ride start"): a Conquer the Mountain world load loads, initialises and compiles the
// career rider under the load screen, so the ride's first frames do not (main.js warmRideRider).
// worldWarm (CTM stalls agent, docs/course-switch.md "World arrivals warm under the load screen"): a Conquer the Mountain world load
// (menu -> career, a Transport across a world switch) waits under the load screen for the start row's pipeline compile (free-ride.js
// rewarm, before: fired and not awaited, so the ride's first frame built 3000 buffers / 7000 bind groups / 20 pipelines, 3.3 s in
// Owen's Safari), then warms what the world pass draws besides the locations (sky, set pieces, rider shadows, post passes) with the
// race warm-up's frames behind the opaque load screen (compile-only under the Transport's visible held loop). Also: web/yield-shim.js
// (three's compile yields without a whole frame per item where scheduler.yield is missing: WebKit), the start row compiled first, a
// DoubleSide pass for two-pass materials (their 'backSide' render objects), cutscene sets and actors compiled per side. Event loads unchanged.
// bindPoseProbe (not a presentation item: field diagnostics, web/diagnostics.js): for 60 s after a load screen opens or closes, a cutscene
// or a ride starts, a few skinned draws a frame are checked for the bind pose (CPU skeleton and the uploaded bone matrices); the first
// hit of a session goes to the /mp/diag telemetry ('bind-pose'): the T-poses reported at a first CTM start, not reproduced in the lab.
// On (2026-09-27): at most 3 checks a frame, 7 us each (64 bones, desktop) while armed; no false hit over the new-career flow (Chrome,
// WebKit), a bind-pose control mesh reported in both.
// nisTick (CTM agent, docs/ctm-parity.md "The NIS rider hold"): under the lodge door / booth cut (world state 14) the world runs on and the
// rider is held at the cut's anchor (123640: control 13 / motion 3; PS2 door-no ticks 394..634, fr-booth2 240 ticks), paused only at the
// prompt / map. Needs the core's nis_hold (web/core.cpp); a core without it, or off: the ride pauses under the cut (stationFlow).
// nisAfterScan (CTM agent, docs/ctm-parity.md "The NIS teleport after the section scan"): the station cut's rider actor (123640)
// places the rider at the start of the next game tick (the NIS tick 0x230BE4, before the rider manager and its section pass 0x101B60),
// not inside the tick whose 121818 fired builtin 68 (before that tick's section pass). Off: the placement as before (one scan early).
// nisBoneProbe (CTM agent, docs/ctm-parity.md "120F20's re-probe under the hold"): under a hold whose rider actor has root motion
// (+0xAFC = the NIS record's byte 7, root_motion_velocity: the lodge door, the booth, the gondola), each game tick's 1242B0 probes the
// ground from the actor's posed board root (rider+0x8A0, bone 22): cutscenes.js humanBoard (the cast rider posed by the NIS clip, at
// its model size; PS2 fr-booth2: within 0.09 cm of the record's bone 22 over 235 ticks) into core nis_hold_probe(2) at the tick's
// nisPreload (CTM agent, docs/ctm-parity.md "Bone 22 from the NIS keys"): on entering a station course, its door / booth cut's step
// (lodge_arr3 #148 / hub_trans_arr #166 with the container's clips), the container's bank on the NIS slot and the human's cast model
// are loaded ahead (cutscenes.js preloadStation), so the cut's first step starts on the hold tick as the PS2's (fr-booth2 record 2644)
// and its posed board root is there for the hold's first probe. Off: the step loads at the hold (about 18 ticks on a first visit).
// gameTickKeep (CTM agent, docs/ctm-parity.md "The game tick at placements"): in a streamed world the core's game tick (1298C8: the scope
// refresh phase, the ground stamps, the boost ring parity) is 0 at a run's start (the world load / WS2; PS2 peak1-green-start record 0 =
// tick 0) and runs on through the in-world placements (11D390 leaves it: door No 636, fr-dra4a-full's crossing). Off: every placement
// reset it to the world seed's landing tick (reset_rider).
// hangWatch (CTM agent, web/diagnostics.js): a tiny worker gets the screen / course / last marks every 250 ms and, when the pings stop for
// over 8 s while the page is visible, posts a 'hang' diag event itself (again every 30 s, 'hang-end' when they resume). A dead WebContent
// kills the worker too, so a hang reports and a crash does not. Silent; field telemetry only (diag on).
// encodedBlend (web/frame-space.js, chosen once per page before any material or Color is built; docs/visual-parity.md 38): the frame
// holds encoded 0..255 values, as the GS frame buffer does, so every blend in the world pass, the sky pass and the front end is the GS's
// (Cs - Cd) x As + Cd on encoded bytes instead of on linear light. Byte-domain materials write their bytes / 255 (frame-space toFrame),
// the fog / encoded composites read the frame without an OETF, three's Color values stay encoded (ColorManagement off), the canvas gets
// no output conversion. Opaque pixels are unchanged; world additives stay in the world pass (byteBlend is moot). Off: linear light.
// threeLean (web/three-patches.js, set as globalThis.__ssxThreeLean by main.js before the renderer exists): three r186's per-draw
// dynamic cache key, bind group key, updateTexture options and sampler key without per-call garbage (the same results).
// ws13Rebuild (web/career-ui.js ws13Riders; main.js ui.cb.heatLineup; docs/ctm-decomp-world-states.md): a race's Next heat / Final
// Round and the results' Restart rebuild the round's computer riders before the gondola, as world state 13 enter (0x235AA0) does; a
// rival challenge's Restart shows its card over the ready state (it was over black).
// bcDecline (web/big-challenges.js -> core mission_lifecycle; docs/ctm-parity.md "Big Challenge lifecycle"): No / Triangle at the offer,
// No at the fail prompt or at "Restart Challenge?", and Quit Challenge reset the rider (30B658 / 30B758 -> 1235F8: control 9, the
// white fade, the route placement, 41 ticks) so the offer does not come straight back; an event gate (builtin 67, 0x3021B8 -> 30B7F8)
// ends a running challenge; world state 10's enter (0x2356A8 -> 309030 / 308988) clears the active challenge, its HUD and the offers.
// finishSkip (web/game-tick.js): a new Cross from the finish + 228 closes the finish panel at once (the PS2's 1E8160 event 5); the
// port always waited 408 / 288 ticks.
// postEventDj (web/game-audio.js finish / hubChatter): the post-event record 2A45C0 / 2A4660 at every CTM finish (place, course, KOs +0x128
// in races, Ubers +0x114; reset at round 1, armed at the final or a one-round event) and the commentary 2A4770 at the next hub chatter
// (2A2E50): Char_Progress 0x2102 (1st), Aggression 0x212E (>= 5 KOs), High_Trick_Score 0x212F (>= 27 Ubers, 24 in Big Air / backcountry),
// a random one avoiding the last; nothing earned during a travel: hub chatter / Terrain_Info / Event_Intro by the destination.
// djVisited (main.js gameAudio.context, web/game-audio.js visited / djTimer): the first-visit flags 579C come from the rider's saved visited
// mask P+0xACC (145D38, careerUI.visitedMask) instead of a per-page mask, and Free_Ride_Intro (and its 579C clear) needs Peak 2 locked
// (146008 = P+0x278 bit 12, 28E730).
// djQueueRules (web/audio-speech.js radioBigIntro, web/game-audio.js pause): Radio BIG intro 2A26F0 stops the current line first (2B11B0 at
// 0x2A272C); a pause resume (289BB8) stops speech and clears the pending DJ only after an in-game song change (+0x5828, 28FAE0), so an
// MCOMM visit no longer drops queued DJ lines.
// mailFreeze (web/career-messages.js): the event's messages post on the finish tick on the PS2 (125108 -> 238358 -> 154EE8) and the icon
// runs 182 frames under the finish HUD before WS5's overlays freeze it (race-f fin 0.050 -> res 3.033 -> f95-after 3.050), so the port's
// results-time post starts the icon at 182 frames: the next ride shows its last ~2 s, not 5 s; opening the Message Center freezes the
// faqDefer (web/main.js 'faq' / faqOpen, web/game-tick.js): the Green Base Station "?" (stage builtin 100 -> 1E3510) touched while a
// pause, prompt or menu is up opens the Message Center's FAQ once the ride runs again (world state 4 0x2309A4 polls gp-0x1024 each
// update after the offer read); the port dropped it. The FAQ is marked shown at the contact (1475C0), as on the PS2.
// transportFade (web/free-ride.js transport, web/cutscenes.js 'transport-ride' / releaseFade / skipLock, main.js transportInWorld, ui.js
// hudSqueeze, core peak_world_transport phases 2 / 3; docs/ctm-parity.md "The Transport's presentation"): the in-world Transport as the
// PS2 plays it: one NIS list (27A860: departure, in-air, held loop, the appended heli drop), the destination requested at the loop's start
// (0x2366C4), the release fading the still-playing loop to black over 30 ticks (27A9F0 -> 2766D0 -> 277980) with the placement at R+29,
// the world fading in under the HUD while the bars slide out over 30 ticks, the dome switch after the list (0x235808), no skip for the
// first 30 ticks (0x236550). Before: three lists with a hard cut at the release.
// departCalls (web/cutscenes.js hubCall, main.js stageTrack): a Transport step's kind-7 channel-0 stage calls without a staged set run on
// the script's hub location (0x2808E8: the scdat container's track globals), e.g. gond_dep #126 / heli_dep #147 at a station (the depart
// LiveComp, the sound loops, the heli snow spray, SetNodeState), with the recorded cleanup at the step end. Before, they were dropped.
// gcWatchdog (memory, WebKit only; docs/mobile.md "Hangs"): web/gc-watchdog.js. When JavaScriptCore has stopped running full collections
// (a load-time full GC left its timer unarmed: the WebKit memory runaway), a few one-page WebAssembly.Memory objects make it run one.
// worldUnderCuts (docs/ctm-events-in-world.md stage 1, section 6.2 / 6.8): the world ticks under the CTM cuts as on the PS2, the human held by
// the cut's rider actor (core nis_hold) instead of a HOLD context that stopped the simulation:
// - WS1 (the event gate's fly-over in the streamed world): no 'WS1 ride-in' HOLD; the rider rides the 30-tick fade in its own control, then
//   is held from the fly-over's first tick (PS2 caps/c0a-gate: gate 3440, control 13 / +0xAC4 1 from 3471).
// - WS10 arrivals (the new-career plane, the backcountry first-visit lists, the heli drops): the ride starts before the list with the rider
//   held and is placed when the list ends; only the movie steps (FMV, lists 29..31) push a HOLD (PS2 new-career: 917 ticks under #153 /
//   #163 with +0xAC4 = 1, none during the ABC1 movie).
// Off: the world stops for the whole cut (the HOLD contexts of docs/pause-contexts.md).
// eventWorldData (docs/ctm-events-in-world.md stage 2; web/ctm-event-plan.js): at the event gate the event package's own small data (the
// computer riders, the race event document, the grid spawn, the progress meter, the slope-style list, the camera triggers, the GO LiveComp
// starts) is read while the fly-over plays, into careerUI.eventPlan. Inert: stage 3 (eventInWorld) uses it.
// eventInWorld (docs/ctm-events-in-world.md stage 3; needs eventWorldData, and worldUnderCuts for the PS2's hold): the CTM gate's event runs
// inside the streamed world with no course switch: main.js cb.eventInWorld (core event_course_seed, init_race with the event's document,
// the event type / game mode, the grid spawn), WS1's approach and idle in the world (cb.introInWorld), the card, then the countdown and the
// run through startRun's event start. The human only (no computer riders until eventInWorldAi); QA, not for players yet.
// eventInWorldAi (docs/ctm-events-in-world.md stage 4; needs eventInWorld): the in-world event's computer riders run in rider contexts fed
// from the streamed world's resident locations (per-context collision, path banks, section node states), made at the gate while the
// fly-over plays, reused (reset, not destroyed) for the next event. QA, not for players yet.
// nisSectionPoint (docs/ctm-events-in-world.md stage 4): every NIS the streamed world plays (arrivals, station cuts, Transport rides, the
// events' fly-over / approach) adds its director's camera point to the section activation, as the PS2 does (0x281370 -> 0x1033B0,
// 0x281100 the outer camera's +0x20; cutscenes.js sectionFeed -> core section_point). pv eventInWorldAi turns it on for the events alone.
// eventReturnInWorld (docs/ctm-events-in-world.md stage 5; needs eventInWorld): back from an in-world event (results Transport -> the
// map) with no world load: main.js cb.freeRide ends the event in place (cb.eventInWorldEnd) and transports inside the world (the same
// location: WS15's Session point 1 and white fade). QA.
// loadCopies (memory, docs/mobile.md "Load spikes"): web/downloads.js reads a body of known size straight into one buffer (no chunk list
// and join) and hands each caller its bytes without the Response body copy (arrayBuffer: one copy of the shared bytes; json / text:
// decoded from them). Before: 3-4 copies of every asset body while a load read it.
// switchGC (memory, WebKit only; docs/mobile.md "Load spikes"): web/switch-gc.js. Before a course's new core is made, while an earlier
// course's core is still alive (JavaScriptCore frees a wasm memory only in a full collection), the gc-watchdog kick asks for one.
// padCarry (CTM agent, docs/ctm-events-in-world.md "Carried presses"): one pad history for the menus and the ride, as the PS2's
// cSSXApp_preUpdate (0x227F20) runs 0x321298 on every app update whatever runs: while the game does not tick (pause menu, cards,
// prompts, results) each 60 Hz frame's pad goes into the core's history (core pad_history_sample), and the menus' open / close and
// a run's start no longer clear the held keys or settle the history. So a Cross pressed on a menu and still held is held on the ride
// with no new press edge: the Give Up's Yes crouches without a prewind (PS2 c0a-ret2, gate ctm-events/c0a-ret2-coast). Needs a core with
// pad_history_sample. Off: the history restarts settled at each run's start and the held keys are cleared at every menu.
// heatRoles (CTM agent, docs/ai-racers.md "Route roles by round"): a career race heat's computer riders take the round's route roles
// (owner +0xE00 / +0xE04 from cComputer_updateRiderDifficulty 0x10C758 -> 0x10C450: qualifier 1 1 0 0 0, semi 1 1 1 0 0, final 2 2 1 1 0 +
// slot 1's E04; web/lineup.js npcRoundRole). Off: every heat rides lineups.json's slot tables, the final's (a Single Event's).
// onlineRecords (online records agent, docs/online-records.md): the course records (61toptimes, the objectives card's record, the
// event INFO's top time) come from the server's boards (web/server/records.mjs, /mp/records); a run that makes an event's online top 5
// is submitted with its replay through Save Records and the game's keyboard; the records screen's Online Records and the main menu's
// Leaderboards show the full boards with Watch Replay. On (2026-09-30; npm test 208/208 with it on). Off: the local PS2 table only.
// careerRival (career-rival agent, docs/career-events.md "The peak rival in career events"): a career race FINAL's slot 1 rides the peak
// semiFresh (career-rival agent, docs/ai-racers.md "The semi's fresh riders"): a career race SEMI's computer riders start with +0x434 = 0x31
// (ground.state.rider_type / identity.rider_type434): WS13 makes new riders (constructor 11B718: 0x31) and 1289F0 places them without a
// hold, so 1218D0 never writes the track id before the push-off, and 13C948 halves their auto boost (PS2 ARA1-semi-zoe; the qualifier's
// and the final's riders are NIS-held and hold the track id). Off: the Single Event tables' track id in every round.
// careerLevel (career-rival agent, docs/ai-racers.md "Difficulty by race level"): a career race's computer riders take +0xDF8 / +0xDFC from
// the human's race level (0x10C4F8 by slot and level 0 / 1 / 2, then 0x10C758's course factor; web/lineup.js npcDifficulty). Off: every
// career race rides lineups.json's slot tables, which hold level 1 (a Single Event's, and a fresh career's).
export const PV_DEFAULTS = Object.freeze({
  speechRange: false,
  flyover: false,
  lazyCourse: true,
  refreshCap: false,
  rideWarm: true,
  worldWarm: true,
  bindPoseProbe: true,
  nisTick: true,
  nisAfterScan: true,
  nisBoneProbe: false,
  nisPreload: false,
  gameTickKeep: true,
  hangWatch: true,
  encodedBlend: true,
  threeLean: true,
  ws13Rebuild: true,
  bcDecline: false,
  finishSkip: false,
  postEventDj: false,
  djVisited: false,
  djQueueRules: false,
  mailFreeze: false,
  faqDefer: false,
  transportFade: false,
  departCalls: false,
  gcWatchdog: true,
  worldUnderCuts: false,
  eventWorldData: false,
  eventInWorld: false,
  eventInWorldAi: false,
  nisSectionPoint: false,
  eventReturnInWorld: false,
  loadCopies: false,
  switchGC: false,
  padCarry: true,
  heatRoles: true,
  onlineRecords: true,
  careerRival: true,
  careerLevel: true,
  semiFresh: true
});
const overrides = new Map();
function fromQuery() {
  // node harnesses: SSX_PV=name,-name (the page's ?pv= syntax) when there is no page URL
  const q = globalThis.location ? new URLSearchParams(globalThis.location.search ?? '').get('pv') : (globalThis.process?.env?.SSX_PV ?? null);
  if (q == null) return null;
  const all = (on) => new Map(Object.keys(PV_DEFAULTS).map((k) => [k, on]));
  if (q === '1' || q === 'all') return all(true);
  if (q === '0' || q === '') return all(false);
  const out = new Map(Object.entries(PV_DEFAULTS));
  for (const w of q.split(',')) { const off = w.startsWith('-'), k = off ? w.slice(1) : w; if (k in PV_DEFAULTS) out.set(k, !off); }
  return out;
}
const query = fromQuery();
export function pv(name) {
  if (!(name in PV_DEFAULTS)) throw new Error(`Unknown presentation switch ${name}`);
  if (overrides.has(name)) return overrides.get(name);
  return query ? query.get(name) : PV_DEFAULTS[name];
}
// Tests: force a switch (null restores the default / query).
export function setPv(name, on) { if (on == null) overrides.delete(name); else overrides.set(name, !!on); }
