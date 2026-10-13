# Core migration: the gameplay rules into the C++ core

Goal (the user's): the C++ core is the whole game simulation, so native Mac / Windows front ends only render, play audio,
read input and network. The rules that still live in the page's JS move into the core, slice by slice, without changing a
result. Each slice runs behind a default-off pv switch; the JS copy goes after the switch has been on through a few deploys.

Proof per slice: the full `node test-ps2-captures.mjs` (534), the AI gates, the slice's page tests and a differential or
page trace with the switch on and off. Scratch cores: `CORE_OUT=local/core-migration/coreN sh web/build-core.sh`.

Page tests on a scratch core with a switch: `CORE_DIR=<dir> PAGE_PV=<switch> node test-....mjs`. web/headless-chrome.mjs
serves CORE_DIR over web/runtime and adds PAGE_PV to the ?pv= of every game page it opens (test-replay keeps its own CORE_DIR
server). local/core-migration/page-run.mjs is tools/tas/page-run.mjs with CORE_DIR, `--pv` and a fixed presentation seed
(?presentationSeed=0x182200), for whole-race ?simtrace traces with a switch off and on.

## Survey (2026-10-13)

The JS that decides simulation results, beyond presentation:

- **The tick order.** web/game-tick.js simulate (84-185) is the only sequence the page, its replay (web/replay.js steps
  gameTick.simulate) and the records verifier (headless Chrome on the page's ?verify path) run. The comparers keep their own
  copies, none identical to it (see "The page's sequence and the gates'" below).
- **The rider manager 0x128AF0** (web/ai-racers.js): the three stages, the shared-RNG cursor and split / defer (the later riders'
  controller draws predicted from their previous tick), the NPC world buffer, the pair dispatch through EM_JS callbacks
  (race_world.cpp 116-122, animation_bridge.cpp 181-182, race_bridge.cpp 96), world-event replication, the visual-stream
  borrowing, the phase-major FX passes and the world / section passes.
- **Mid-tick rules:** the finish celebration (game-tick 154-160; the freestyle place from web/career.js through finishHost, which
  race_end calls, race_bridge.cpp 99), a rescue's reset_animation, the results timing (408 / 288 / 409) and finishSkip, the solo
  anchor RNG (main.js soloTickStart), relationship events pushed back into the computer riders inside the tick (web/ai-race.js).
- **Event set-up:** web/lineup.js (roster 0x23A4F0, rounds 0x23A108, freestyle roster 0x239938, peak rival 0x145750, route roles
  0x10C450, difficulty 0x10C4F8 / 0x10C758, relationships 0x155E58 / 0x155BF0, anchor words), web/ai-race.js (0x122D78, 0x238BF8,
  prepare / install), main.js startRun's call order, the plane-drop seeds.
- **Career:** web/career.js and friends, mostly bookkeeping; they reach the core through attributes, the time limit, the race
  bonus, the AI level / roster, collect masks and cash, Uber rows, gear and the FAQ flag. Award picks and message variants draw
  from the presentation generator that also seeds the next roster.
- **World data handoff:** sections, stage world, flags, avalanches, peak-world part cutting, web/stage-collect.js tables,
  `_stage_teleport_enable` / `_set_fx_reset_kicker` (hard-coded 1 in main.js applyPresentationFast).
- **Multiplayer** (port-only netcode rules): ghost pair views, the contact stream RNG, ranking from packets, pickup arbitration,
  world events, grid seeds.
- **Frame-time-driven input** (not a move, the user's call): cutscenes.js sectionFeed -> `_section_point`, cutscene-plane-fx
  `_stage_cutscene_tick`, the NIS board root from three.js into nis_hold_probe(2).

## Plan

| Slice | What | Switch | State |
| --- | --- | --- | --- |
| M1 | Pure event / career rules (lineup, anchor RNG, estimates, rankings, medals, records, difficulty) | coreRules | core1, verified |
| M2 | The game tick's core sequence (tick_begin = tick_input + race_begin + tick_rider + tick_progress, tick_mid, tick_end, tick_finish, tick_results); M2b the gates on the page's sequence | coreTick | core3, verified |
| M3 | The rider manager into C++ (stages, RNG cursor, pair host, world sync, FX phases) | coreRiderManager | core5, verified |
| M4a | The race's relationship records in the core's rider manager (the last mid-tick page callback) | coreRelationships | core6, verified (page tests pending a tree fix) |
| M4b | The computer riders' event start and the race world's reset; the plane-drop seeds | coreEventStart | core8, verified |
| M5 | Career progression state (results, awards, unlocks, gear, attributes; the presentation-RNG draw order kept) | | planned |
| M6 | The NIS clock / section point / cutscene ticks on game ticks: a behaviour change, the user's call | | not started |
| M7 | Multiplayer rules (pickup arbitration, contact stream, ranking inputs); transport stays in JS | | planned |

The core API a native front end needs afterwards: load (the existing init_* documents); session (event_setup -> the lineup and
packages to load, event_start, restart, give_up, camera view, input map); tick(pad[24]) -> one record (state, pose, camera, race
phase, place, standings, finish / results / rescue / placement flags, trick name, an event queue for audio, rumble, collect /
career, relationship notices, mission UI and world requests); the existing read-outs (skin palettes, FX vertices, HUD slots,
stage-world entities, set-piece logs) and snapshot_*.

## Open design item: rider contexts without the TLS swap (native)

RIDER_LOCAL (web/rider_local.hpp) is thread-local storage, and a rider context is switched by setting wasm's `__tls_base`
(web/rider_tls.S, web/rider_context.cpp). A native build has ordinary TLS, and macOS arm64 does not let user code swap the TLS
register, so the six riders of one race cannot share a thread that way. A native core needs the per-rider state reached
through a context pointer (one struct per rider, or the RIDER_LOCAL variables turned into members reached from a current-context
pointer the rider manager sets), keeping the wasm path's layout and the snapshot registry. Not part of the rule migration; to be
designed before the first native multi-rider race.

## M1: the pure rules (pv coreRules, core1)

- engine/game_rules.hpp: the rules, header-only C++ for the wasm and a native build. They keep the JS versions' arithmetic (IEEE
  single, round to nearest, Math.fround forms; towardZero where the JS rounds toward zero), not the EE's.
- web/rules_bridge.cpp: one export, `rules_eval(fn, io, n)` (doubles in, doubles out over the same buffer; -1 = inputs the core
  version does not take). web/core-rules.js: the JS side, `setRulesCore(core)` (main.js: after the course's core is made, null at
  the unload so the old core can be collected).
- Moved (each JS function asks the core first and keeps its JS body as the fallback): lineup.js seededWords, nextWord, clockSeed,
  peakRival, buildRosterFrom, roundEntries, freestyleRoster, relationshipAge, relationshipEvent, npcRoundRole, npcDifficulty,
  anchorRandomWords, rivalAnchorWords; event-anchor-rng.js eventAnchorWords (the table is in game_rules.hpp too); ai-race.js
  estimateFinishTicks, orderByTime; career.js recordSlot, exploreMedal, mountainPercent, platinumThreshold, placementMedal,
  estimateFinishTicks, rankEntries, adaptLevel, aiFreestyleScore (its draw taken first; nothing before it depends on it),
  Career.opponentEstimate; rival-mode.js rivalCharacter, estimateTicks, estimatePoints, rankBy.
- The JS keeps the inputs the core does not take: non-integers where the JS indexes tables, NaN in a ranking (the JS comparator
  is then no order and the result is V8's sort), missing table rows (the JS throws).
- Not moved: the data shaping around them (assembleLineup's record merging, relationshipScores / Kinds lookups, loadDraws,
  rivalAnchorDraws, freestyleSlots, slotRiders), ai-racers.js rngNext (per tick; moves with M3), main.js soloTickStart's own use
  of EVENT_ANCHOR_DRAWS (moves with M2 / M4).
- Proof: web/test-core-rules.mjs, 41,255 cases identical (Object.is on every number; every relationship record byte x event, every
  slot / level / course, every anchor course x human, assembled lineups of every lineups.json, 3000 random estimate inputs with
  NaN / infinities, 1500 rankings with ties and big values). Full ps2-captures 534 / 534 on core1. Page: a whole Metro City race
  (local/tas/best-8507.tas, Mac, 5 computer riders, 8687 ticks) with the switch off and on: the ?simtrace hashes, per-tick dumps
  and lineups identical. test-rival-page, test-replay and test-records-verifier pass with CORE_DIR=core1 PAGE_PV=coreRules.

## The duplicates (F6) and what the decomp says

- **0x122D78 estimate** (ai-race.js / rival-mode.js `!(average >= floor)` vs career.js `speed < floor`): 0x122DEC is
  `c.lt.s avg, floor` (take the floor when below), career.js's form. The two differ only when the average is NaN (0 / 0 race ticks),
  which the EE FPU never produces (no NaN; its div by 0 gives the largest float), and the end is cvt.w.s, which saturates where
  Math.trunc does not.
- **Rankings 0x238BF8 / 0x238B70**: both sort with 0x418EF8 (newlib-style qsort; under 7 entries an insertion sort with a strict
  test, so ties keep slot order as the JS assumes) and both comparators are unsigned (0x245E58 / 0x245EA0 `sltu`). career.js
  rankEntries compares JS numbers (signed) and rival-mode.js rankBy compares scores signed (`| 0`): they differ from the PS2 only for
  negative scores or values from 2^31 up.
- **The peak rival 0x145750**: lineup.js peakRival (0-based peak) is the code exactly, out-of-range peaks giving 3 included.
  career.js `rival` and rival-mode.js rivalCharacter (1-based) are the same table for peaks 1..3 and throw outside them.
- **The record test**: career.js addRecord (0x154D58: the run goes in at the first entry it is not strictly worse than, so a tie
  ranks the new run above) is the decomp's. career-ui.js finishLui's "new record" uses a strict `score > r.value` (a tie shows no
  new record), and its platinum check reads only a score threshold (a timed mode never shows platinum there).
- **The next heat's lineup**: web/event-heat.js heatEnter (compare-ai-capture --ws13) and main.js ui.cb.heatLineup (the page) are
  two builds of 0x536708 / 144D98 / 147338; not compared yet.

## M2: the game tick in the core (pv coreTick, core3)

- web/game_tick.cpp: the core calls of web/game-tick.js simulate, in its order, as exports the page calls with its own host state
  (passed in, handed back; the state itself stays in JS for now):
  - `tick_begin(pad, lastRescues, lastResetPlacement, animationReady, boneCount, teleportKnown, teleportCount, tweakWord)`: pad_tick,
    race_begin, step_rider, a rescue's reset_animation, animation_tick (the pose copied at once), its reads, race_end, the stage
    teleport re-read. Also as its parts `tick_input(pad)`, race_begin, `tick_rider(command, ...)`, `tick_progress(known, count)`,
    which the comparers call with their own reads between them.
  - `tick_mid()`: a solo run's fx_pass(-1) and section_pass (with computer riders web/ai-racers.js endTick runs them).
  - `tick_end(...)`: the camera, a replay's camera, the pending / trick reads, the finish reads (score, ticks, timed out, dump).
  - `tick_finish(freestyle, aiActive, aiPlace, backcountry)`: the celebration rule (rider +0x100).
  - `tick_results(io)`: the results timing (408 / 288 / 409, finishSkip's Cross edge, the live stop).
- web/game-tick.js `simulateInCore`: simulate from pad_tick on through those exports, every JS hook (free ride, Big Challenges, the
  FAQ, the in-world riders, the computer riders, online races, the progress meter, the replay) at its old place.
- Proof: web/test-core-tick.mjs (two identical cores, the switch off and on: every tick's ?simtrace hash and tick record equal on
  Snow Jam alone and with five computer riders, Select resets and a pause Give Up through the results and the live stop; padTweak off
  and on); full ps2-captures 534 / 534 on core3 with every report line the same as core1's; page traces below.
- Not exercised: the rescue branch (rider_state [13] only rises on the non-finite-position failsafe, core.cpp 1027) and the booth
  teleport branch in a node run (covered by construction: the same calls).
- F9: the TMPDEBUG state (tmpDebug, tmpDebug2, tmpBegin, tmpPreContact) and its exports tmp_debug / tmp_begin / tmp_pre_contact are
  gone from web/animation_bridge.cpp: written every tick, read by nothing (no JS, tool or local script used the exports).

### M2b: the page's sequence and the gates'

The comparers (compare-ps2-capture.mjs, compare-ai-capture.mjs) each kept their own copy of the tick. Run in the page's order
(PAGE_SEQUENCE=1) all 534 gates gave the same reports as their own order, so no capture tells them apart. The PS2's order is the
page's: 0x2306B8, the NIS tick 0x230BE4, then the rider manager 0x128AF0 rider by rider (11B3F8 frame begin = race_begin, 121068
provider and controllers = pad_tick, 1210B0 / 1211F8 / 1216E0 motion = step_rider, 121700 / 121728 pose = animation_tick's pose,
121750 second motion = animation_post with the pairs 0x107888, 1217F8 / 121818 progress = race_end), the FX passes 0x128F20..0x1290F0
after every rider's 121818, section activation 0x101B60, the cameras 0x230D7C. The comparers ran the FX inside animation_tick,
before 121818.

Since 2026-10-13 the comparers run the page's order by default (`PAGE_SEQUENCE=0`: the old order, kept for one release):
- FX deferred to after race_end, then section_pass every tick (compare-ps2); the shared visual stream always (compare-ai);
- a rescue's reset_animation; animation_tick's tweak as the page passes it (pv padTweak);
- on a core with tick_rider: the core's own tick parts (tick_input / tick_rider / tick_progress), and the finish celebration from the
  page's rule (tick_finish) instead of the recorded rider +0x100, except at a freestyle course without --finish-place, where the
  page's place comes from its career standings (posted scores) that a capture does not have, so the record's flag stays.
  web/uber-audio-compare.mjs follows tick_progress for race_end's info.
- Result on core3: 534 / 534, every report line the same as core1's own-order run. The page's celebration rule gives the PS2's
  flag in every AI gate that finishes.

### tweak (pv padTweak, off)

The page passed the boost word (pad_tick [7]) as animation_tick's tweak; the computer riders and the comparers' old human tick pass
the tweak word ([12]). apply_command_words (web/input_bridge.inc) sets boost = tweak = the word's tweak in controller 5 and tweak 0
elsewhere, and only controller 5's grab lifecycle (originalGrabLifecycle) reads it, so they agree wherever it matters. [12] is the
command's own field. pv padTweak passes it (game-tick.js both paths, the core tick's tweakWord, the comparers). Proof: 534 / 534 with
SSX_PV=padTweak on core3, every report line the same; a page trace below.

## F2: the replay's pad history (pv replayPadState, core4)

The divergence: with pv padCarry (on) a live start keeps the pad history 0x321298 the menus and the card fed (main.js startRun skips
reset_pad_history, and padCarryFeed samples it on frames that do not tick: pauses, cards, the results), but a replay restarts with
a settled history and never sees the samples taken between ticks. A button held into the start, or pressed and released in the pause
menu, decodes differently in the replay and the verifier (an edge the live run did not have, or the reverse).

The fix (one mechanism for the start and the pauses; done 2026-10-13):
1. Core: `pad_state_save(out)` / `pad_state_restore(in)` for everything the pad path carries between samples: padHistory (24 x the
   0x321298 channel record), padCommand and cruiseLastControl (web/input_bridge.inc; reset_pad_history resets exactly these).
   Plain words, no pointers.
2. Start: the replay snapshot (main.js createRaceReplay snapshot) keeps `padState` taken at the end of the live startRun; startRun(R)
   restores it where it now calls reset_pad_history (a replay file without it resets as today).
3. Pauses: padCarryFeed marks the history touched; the next live tick, before its pad is recorded (game-tick.js simulate, next to
   replay.record), notes a replay event `padState` with the saved words. Replay: web/replay.js step applies events before the tick
   (eventsAt), so the event handler restores the words there. One event per pause (about 100 bytes), not one per sample.
4. The replay file: `events` already travel in the meta (web/server/replay-file.mjs, version unchanged: an unknown event kind is
   ignored by an old page, which then diverges as today). web/server/records.mjs checkRunFile checks pad-state words only when they
   are an array (at most 512 words; at most 4096 events); a null or absent value (an older client, a core without pad_state_save)
   is ignored. With the switch off the live run records nothing new (no snapshot field, no event).
5. Proof: test-replay's PADSTATE case (Cross held on the card into the start, Cross pressed in a pause and held into the resume,
   then Give Up; ssxQA.padCarry feeds the between-tick samples): with the switch the replay is exact on all 901 ticks (events
   padState@600, giveUp@900); without it the replay leaves the run at tick 600. The upload round trip (test-records-verifier,
   test-online-records, test-replay) passes on the live core and on core4, with the switch off and on. The verifier plays the same
   startRun(R) / event path as the page's replay.
6. A lesson from the first deploy: the page noted a null padState on a core without the export and the server rejected it, so
   genuine uploads failed until the rollback. Anything that changes the replay format or the server's acceptance runs the upload
   round trip on the live core before it sits in the shared tree.

## M3: the rider manager in the core (pv coreRiderManager, core5)

- web/rider_manager.cpp: web/ai-racers.js's per-tick choreography (beginTick, stage 1 from the human's pose, stage 2 from the
  start of the human's race_end, the rider pairs from each rider's 121750, endTick's stage 3, the FX passes, section activation),
  the shared-RNG cursor with its predicted counts, the computer riders' world buffers, the pair host (view, translate, velocity,
  react, random), the shared-world replication and the visual-stream borrowing, with the same calls in the same order.
- The core switches the rider contexts itself (rider_tls_set). LLVM takes a function's thread-local addresses as fixed for the
  whole function, so everything run in another context goes through a non-inlined function (runOpaque); inlined, it reached the
  previous context's variables (the first run's "Invalid computer-rider roster").
- rider_host bit 8 routes the riders' hooks (web/animation_bridge.cpp after-pose and pairs, web/race_bridge.cpp race_end's
  before-progress, web/race_world.cpp's pair callbacks) to the manager; without it the EM_JS paths run (also the online pair-net).
  The page is called back in the human's context only: the pair reactions' speech and relationship (ai-race.js onPairAudio /
  onReact), and the QA hooks (draw counts, world-draw injection).
- The event set-up stays in ai-racers.js (configure, start, lineups, relationships, holdTick, the in-world return); it tells the
  manager (manager_configure / start / resume / set_anchor) and reads its state after each tick (the tick, the counts, every
  computer rider's provider words and finish, the ranking world).
- The manager's state is RIDER_LOCAL in the human's context (registered in the rider-context snapshot), so the in-world
  snapshot saves and restores it with the context.
- Proof: web/test-core-rider-manager.mjs (two identical cores, the switch off and on: every tick's ?simtrace hash, tick record,
  manager state and hook calls equal; Snow Jam with five computer riders through a Give Up and its results, 4909 ticks; with the
  QA hooks, 1500 ticks and 19,509 hook calls).
- Full ps2-captures 534 / 534 on core5 with SSX_PV=coreRiderManager (every AI gate through compare-ai-capture's createAiRacers)
  and with the defaults, every report line the same as core1's. Page (CORE_DIR=core5): the Metro City race with five computer
  riders, 8687 ticks, ?simtrace and dumps identical with the switch off and on; test-rival-page, test-replay,
  test-records-verifier pass with PAGE_PV=coreRiderManager; test-ai-racers, test-lineups, test-slopestyle-bigair,
  test-rider-contexts (CORE_JS now honoured by ai-race-node.mjs and test-rider-contexts.mjs), test-stage-world, test-mp-pickups
  pass with SSX_PV=coreRiderManager.
- Left in JS for later slices: the event set-up (M4), the relationships the reactions change mid-tick (the manager calls the page
  back for them; moving the tables in is M4 / M5 work), holdTick (WS1's approach), the online pair-net host (M7).

## M4a: the relationships in the core (pv coreRelationships, core6)

- web/rider_manager.cpp `Relationships`: the race's records (3 banks x 10 characters x 10 [kind, level, score]), the participants,
  the 6x6 levels / kinds, and a per-tick queue of the pair reactions. On a reaction (107E70's handlers) the core keeps the speech's
  relation value of that moment (web/sfx-game.js relationOf), applies 0x155BF0 (engine/game_rules.hpp relationshipEvent, the
  record stored as the page's hex bytes are), recomputes the levels / kinds (the peak rival 3 / 2) and pushes changed levels to
  race_world and each computer rider's row, in the order web/ai-race.js onReact did, then the knockout and the reaction.
- web/ai-race.js loads them at each start and a replay's restore (loadCoreRelationships) and drains the queue after each tick
  (drainCoreRelationships): the speech hook with the core's value (web/game-audio.js takes it as onPairAudio's 5th argument), the
  career notice when the human's record rose a level (not in replays), then the changed records back into its tables (their
  text), the document's levels / kinds, and its storage. compare-ai-capture.mjs loads its aged tables the same way.
- A no-op unless the switch is on, the rider manager runs and the core has the exports (ai-racers.js relationshipsAvailable). A
  first version called an export core5 lacks with the switch off and broke every AI race on the tree for a while; guarded since.
- Proof: test-core-rider-manager.mjs's relationships pair (the page rule in the tick against the core's: the same traces, the 4
  reactions with their notices and scores, the records after 3000 ticks); full ps2-captures 534 / 534 with SSX_PV=coreRelationships
  and with the defaults on core6 (bc/bc-race-tuck2's levels change in the core: Mac's record of Zoe to 2); the Metro City page race
  (8687 ticks): ?simtrace, dumps, the stored records and the document's levels / kinds identical off / on; test-rival-page and
  test-records-verifier pass with the switch.

## M4b: the event start in the core (pv coreEventStart, core8)

- web/rider_manager.cpp `manager_event_start`: web/ai-racers.js start(): each computer rider's provider set up again from its
  record text (npc_configure with the same document text), its stat bytes when they are not all 5, its pad history settled, its
  start (npc_start_event, or npc_grid_start, under `hold` with its event clock held in PreRace); the manager's counts; the race
  world's reset (world.reset / proximity: the records' distances and bearings from one race_world_frame of the grid positions,
  the ranks, the levels, then the pair inputs) read from the document (JS number coercions kept: an absent field NaN, null 0, a
  bool 1 / 0); every context's world-event log dropped. The page passes the same texts it passed before.
- web/game_tick.cpp `plane_drop_seed`: main.js resetPhysics's CTM plane drop seeds in their order (the rider head first, the
  start velocity, drop_air_seed, the camera's 271 words, the air streamers); the data stays web/plane-drop.js's.
- Not moved, on purpose: the rest of main.js startRun. Its core calls are interleaved with page modules that also call the core
  (freeRide.afterReset, mpGame.beforeStart, the collectibles, the computer riders' start, the in-world sync, the presentation
  resets), so a single core call would change their order; they move with the modules (M5, M7). The order a native front end
  must make, from startRun: race_time_limit, set_race_bonus, reset_fog, reset_rider_lighting, reset_animation, reset_race (not for
  an in-world grid start), the pad history (reset, or the replay's pad state, or kept under pv padCarry), set_input_map, the
  placement (reset_rider and the seeds, or the in-world NIS release), set_camera_view (a replay), the free-ride reset, the
  multiplayer grid seed, the collectibles, start_event or (in-world) event_route_seed / section_restart / event_grid_start, the
  computer riders' start, the in-world node sync, and for a replay the RNG words.
- The two toggles applyPresentationFast sets every frame (stage_teleport_enable 1, set_fx_reset_kicker 1) stay page calls: they
  default off in the core and compare-ai-capture does not set them, so making them core defaults would change its runs.
- Proof: test-core-rider-manager.mjs's event-start pair (both on the core's rider manager, started by makeGame, restarted at 700 and
  given a held grid start with the human's rank at 1400: 2000 ticks identical); test-plane-drop-seed.mjs (the page's calls and the
  core's, 600 ticks identical); full ps2-captures 534 / 534 on core7 with SSX_PV=coreRelationships,coreEventStart and with the
  defaults; the CTM plane drop on the page (a new career's Happiness start, clock frozen, 600 ticks): identical off / on.
