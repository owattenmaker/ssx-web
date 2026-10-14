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

## M3 online host correction (2026-10-14)

See [multiplayer.md](multiplayer.md#gate-opening-freeze-hotfix-2026-10-14). A configured manager does not own an
online race: `rider_manager_native()` must also require `rider_host_flags() & 8`. Otherwise gate-opening pair
queries access dormant NPC bodies and throw. Offline start/resume re-arms the manager bit after online disposal.
Core `1e117b0c` contains this correction; no exports or collision rules changed. The online pair regression now
retains a configured offline manager, and the offline smoke test exercises an online host takeover/disposal.

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
| M5a/b | The rules module (rules without a course core) and the session's presentation generator | rulesModule | in the tree, verified |
| M5c | The career's result kernel (race / freestyle rounds, medal, records entry, peak runs) | coreCareerResults | core11, verified |
| M5d | The award cascade and the career messages in the rules module (the save as ordered JSON) | coreCareerCascade | in the tree, verified |
| M5e | The lodge's purchases (gear, uber tricks, songs, rewards, attributes) | coreCareerLodge | in the tree, verified |
| M5f | The world's career events (collectibles, Big Challenge status words) | coreCareerWorld | in the tree, verified |
| B | The peak world's station decisions (split HUD, booth / lodge door ending a Big Challenge) at the PS2's point | coreWorldEvents / stationSplitPs2 | in progress |
| M6 | The NIS / cutscene clock on game ticks (approved by the user) | nisOnTicks | decomp |
| M8 | The CTM world-state machine | | after C |
| M6 | The NIS clock / section point / cutscene ticks on game ticks: a behaviour change, the user's call | | not started |
| M7 | Multiplayer rules (pickup arbitration, contact stream, ranking inputs); transport stays in JS | | planned |

The core API a native front end needs afterwards: load (the existing init_* documents); session (event_setup -> the lineup and
packages to load, event_start, restart, give_up, camera view, input map); tick(pad[24]) -> one record (state, pose, camera, race
phase, place, standings, finish / results / rescue / placement flags, trick name, an event queue for audio, rumble, collect /
career, relationship notices, mission UI and world requests); the existing read-outs (skin palettes, FX vertices, HUD slots,
stage-world entities, set-piece logs) and snapshot_*.

## Rider contexts without the TLS swap (native)

Resolved: see "Native core" below (emulated TLS: every RIDER_LOCAL access goes through the current context block, a switch is one
pointer store; no source changes).

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

## M5a / M5b: the rules module and the presentation generator (pv rulesModule, off)

- The rules module: web/runtime/rules.{js,wasm} (8 KB glue + 36 KB wasm, 1 MiB linear memory), built by `sh web/build-rules.sh`
  (RULES_OUT=<dir> for a scratch build) from web/rules_bridge.cpp (the same engine/game_rules.hpp and rules_eval the course core
  compiles: one source of truth) and web/rules_module.cpp (engine/presentation_rules.hpp, the session's presentation generator:
  presentation_boot / booted / draw / note / roster_seed / state). web/rules-module.js loads it (a literal import, so the bundler
  hashes it like the course core; node: RULES_JS=<rules.js>). Rebuild it whenever game_rules.hpp or presentation_rules.hpp changes.
- M5a, the rules without a course core: web/core-rules.js runs the rules in the course core while one is there (main.js
  setRulesCore), else in the rules module (coreRules and rulesModule on, the module loaded), else the JS versions. Each core has
  its own io buffer.
- M5b, the presentation generator: web/lineup.js keeps the session's generator (0x4FF018: the boot draw, the career's reward picks
  and messages 0x3177F0, the per-event draws noted from lineups.json presentation_model, each roster seed = draw 129 after them)
  in the rules module when it has loaded by the generator's first use (bootPresentation; per-frame draws noted before go with it).
  Otherwise the page keeps it for the whole session; the two never mix within a session.
- Boot: main.js starts loadRulesModule() at boot with the switch on and waits for nothing. Measured (local/core-migration/qa/
  boot-cost.mjs, dev server, alternating loads): Chrome headless title at 811-813 ms off and on (6 + 6 loads; rules.wasm fetched at
  62-82 ms); WebKit title median 416 ms off / 420 ms on (8 + 8 loads, spread 278-434 ms both ways; fetched at 200-300 ms). A failed
  load leaves rulesModule() null: the JS versions run.
- Bundle: a scratch online build (the hosting config with its output in local/) gives assets/rules-*.wasm (35.7 KB) and the glue in
  the main chunk; deploy-staged.sh needs nothing new.
- Proof: web/test-rules-module.mjs (20,000 presentation calls against the JS version); the career-session differential
  local/core-migration/qa/career-session.mjs (three scripts x 1500 events: race / freestyle / rival rounds, awards, rewards, inbox
  messages, cash, collectibles, roster seeds, noteRaced / settleRaced and a malformed model): the presentation words and roster
  seeds taken, the results, the generator's next words and the save blob (savedAt aside) identical off / module at boot / module
  after the first quarter; every rider's inbox full (25) and 8-11 awards each. Page (live core, Metro City single event and CTM,
  8687 ticks): ?simtrace, dumps and start identical off / on, the module loaded and holding the generator (its first word the
  roster seed 3044084137).

## M5c: the career's result kernel (pv coreCareerResults, core11)

- engine/career_rules.hpp, through rules_eval (web/rules_bridge.cpp ids 28..32, in the course core and the rules module):
  raceRound (web/career.js raceResult: DNF / finish / 0x122D78 estimate times, the ranks, the round, the level 0x147D20 with the
  0x23AA24 margin, the message), freestyleRound (freestyleResult: the opponent's update, totals / shown / ranks, the round, the level,
  the message), eventMedal (completeEvent's medal and platinum), recordInsert (addRecord's entry: f32 seconds, the first entry the run
  is not worse than) and peakResult (web/peak-run.js).
- web/core-rules.js coreRaceRound / coreFreestyleRound / coreEventMedal / coreRecordInsert / corePeakResult return undefined (the JS
  runs) with the switch off, without a core that has the kernel, or for inputs the JS coerces oddly (non-finite times or scores, a
  missing human, more than 8 riders). The page applies the outputs to its records; a level the kernel left as it was stays the same
  object; round 3 sets no advance / message (as the JS).
- Proof: web/test-core-career-results.mjs (two careers in lockstep, off / on: every result and every save; 43,192 rounds over 20,000
  events in the rules module, 10,758 in core11's course core with CORE_JS); the career-session differential (3 x 1500 events,
  rulesModule + coreCareerResults: draws, results, the generator's next words, the save blob identical); full ps2-captures 534 / 534
  on core11; CORE_DIR=core11 PAGE_PV=coreCareerResults: test-rival-page, test-replay, test-records-verifier, test-online-records,
  test-ctm-flow pass.

## M5d / M5e: the award cascade, the career messages and the lodge (pv coreCareerCascade, pv coreCareerLodge)

- engine/career_cascade.hpp in the rules module (web/rules_module.cpp career_data / career_kernel / career_out_size; web/build-rules.sh
  adds the nlohmann include): web/career.js complete1591E8, earn159818, exploreGoal1599A0 and grant159CD0 with the goal predicates,
  the gear inventory (web/lodge.js GearInventory), the reward record and the collection bonuses; web/career-messages.js's hooks
  (eventComplete, postAward, the earnings notices) with MessageInbox / MessageRules; M5e the lodge's purchases (buyGear, equipGear,
  buyUber, selectUber, buySong, buyReward with lastBonus, buyAttributePoint, buyAttribute).
- The save: the page sends JSON.stringify(save) and the kernel works on it as an ordered JSON document (nlohmann::ordered_json: new keys
  in the JS's creation order; integer-like keys are ordered by JSON.parse as JS orders them). web/career-kernel.js writes the result
  into the page's save in place (syncInto: the objects the page holds stay the same objects; a value JSON cannot carry stays as it
  was). Non-integer numbers are doubles both ways and round-trip exactly (JSON.parse / nlohmann's shortest form); the differential
  carries one.
- Draws: the cascade's reward picks and message variants come from the session's presentation generator, so M5d runs only when it
  lives in the rules module (web/lineup.js presentationInModule) and the career draws from it; otherwise the JS runs. M5e draws
  nothing (lodgeModule). A failed kernel call leaves the generator and the save as they were; the kernel is then off for the session
  and the JS runs with the hooks' own steps (CareerMessages.awardMessage / earnMessages), so nothing is lost.
- Proof: web/test-core-career-cascade.mjs (child processes, 2 sessions x 500 events with every path: rounds, collectibles, cash,
  Big Challenges, explore goals, direct awards, monster medals, notices, all eight lodge operations, a non-integer value: results,
  the reward record, the mail icon, the generator's next words and the save blob identical off / on / after a kernel failure); the
  M5d session differential (3 x 1500 events); the page (a won R&B career final: the kernel ran, the save, inbox, reward record, HUD
  and dumps identical off / on); boot with the 266 KB module: Chrome title 815 vs 817 ms, WebKit 462 vs 464 ms (no cost).

## M5f: the world's career events (pv coreCareerWorld)

- engine/career_cascade.hpp ops `collect` (web/career.js markCollected: the collect bit 153B00, the award's cash 119EF8 kind 3 with
  the earnings notices, 1599A0 the explore goal when the peak's collectible medal changed) and `challenge` (web/big-challenges.js
  challengeStatus: the status word, the table's initial words 151600, 159B08 the explore goal when the challenge medal changed);
  web/career-kernel.js worldKernel (integer indexes only; with the cascade, since the awards draw).
- Proof: web/test-core-career-cascade.mjs with collects (in and out of range, with and without cash) and status words (bit 3 on /
  off, indexes -1 .. past the table): identical with every kernel switch off / on / after a kernel failure; the collect / challenge /
  career node tests and the round trips with the switch.

## B: the peak world's station decisions (pv coreWorldEvents, pv stationSplitPs2)

- web/peak_world.inc `peak_run_setup(on, active, time, s0..s4)` (web/free-ride.js at the world start and every run start, with
  web/peak-run.js peakSetup's tier row) and `peak_world_apply_events()` (the end of `peak_world_tick`; alone in a capture comparison
  that drives the rows): the station decisions free-ride.js drainEvents made, made in the core. A booth / lodge door (builtin 68
  actions 3 / 4) ends a running Big Challenge (236250 -> 30B7F8: mission_world_session 0); a peak run's Load into a station (17..21)
  posts the next split of the tier row for 5 s (0x2A time, 0x2B points; a zero split shows nothing, the counter advances anyway). The
  events stay queued for the page's cutscenes, prompts and maps.
- pv coreWorldEvents (on bit 0) keeps the page's point and inputs exactly (the end of the Load's tick; Math.round(raceInfo[0] x 60)).
- pv stationSplitPs2 (on bit 1; a behaviour change): the PS2's point and clock (local/core-migration/decomp/station-split.md). The Load
  is in update A's rider pass (121818 -> 30A060 -> builtin 68 -> 22D088: streamer +0x1B4 = 4); update A+1's streamer pass 22D8D8
  case 4 requests world state 10 (231250) and 230F40 enters it; update A+2's 230F40 runs WS10's update and its exit 235868 ->
  238510 -> 23B5F8 / 23C560 (0x235934), before that update's rider manager. In tick terms the split posts at the end of the tick
  after the Load's tick, after that tick's 117C28 aging (the slot's clock reads 0 at the next sample), with
  int(f32(C+0xC) x f32 0.016666668) (0x23B65C; C+0xC the race clock before the next increment). The page posted it one tick early.
- The comparer (web/compare-ps2-capture.mjs --peak-run on the whole mountain) restarts the race clock as the page's run start does
  (race_clock_restart; PEAK_CLOCK=0: the old start) and, with either switch, drives the core's decisions after race_end.
- Proof: allpeak/apr-start (the E station split) with SSX_PV=stationSplitPs2 on core12: the score exact to the end (13999 / 13999;
  the gate's limit is END with the switch, 12471 without); p2r-start, apj-start, peak1-race-start exact both ways.
- **Unconfirmed: the points split (0x2B, 23C560).** No capture has a points-mode peak run reaching a station, so its value
  (the page's score - split x 100) and its clock are not checked against the PS2; its timing follows the same 235868 path.

## Native core

A native build of the course core (native/build-core-native.sh -> local/native-core: clang, the host's architecture, the sources of
web/build-core.sh except the wasm TLS swap) with a headless race driver (native/core/race_driver.cpp: Snow Jam, Zoe alone or with
the five computer riders, scripted pad, a Give Up; one ?simtrace hash per tick) and a replayer of logged wasm call streams
(native/core/replay_calls.cpp, its export table generated by native/gen_dispatch.py; the stream logged by
local/core-migration/qa/native-gate/spy-core.js around web/compare-ps2-capture.mjs on a ps2-captures gate).

- The wasm-only parts and their native stand-ins: emscripten.h (native/core/shim: EMSCRIPTEN_KEEPALIVE, EM_JS as extern "C"
  declarations), the host hooks (native/core/host_hooks.cpp: the page defaults with the core's rider manager), `__tls_base`
  (native/core/shim/native_shim.h maps `__builtin_wasm_tls_base()` to the current context block).
- Rider contexts, the context pointer: the core's units are compiled with `-femulated-tls`, so every RIDER_LOCAL access is a call
  of `__emutls_get_address(control)`, which native/core/rider_context_native.cpp defines: each variable gets an offset in a
  context block at its first access (the same offset in every context; its initial bytes go into the pristine image and every
  existing block), and the access is the current block + offset. Switching contexts is one pointer store (rider_tls_set, as wasm's
  global.set of `__tls_base`); a new context is the pristine image with the units' static-initialisation work run in it, as
  web/rider_context.cpp does. No source changes: web/rider_manager.cpp's wasm inContext (rider_tls_set + runOpaque) runs as is.
  Needs clang on every platform (Apple clang, clang / clang-cl on Windows, clang on Linux); MSVC has no emulated TLS.
- The first prototype ran each rider context on a thread of its own and handed work between them (condition variables): bit-exact,
  but 354 hand-offs per six-rider tick at about 9 us each. Copying the 185 KB TLS image in and out at each switch would cost about
  130 MB of copies per tick; spinning hand-offs halved the time but kept four cores busy. Both dropped.
- Results (macOS arm64): the six-rider race (4909 ticks) 3.9 s native vs 6.0 s for the wasm core in node (16.6 s with the thread
  contexts); solo 3409 ticks 0.8 s. Every tick's hash equal to the wasm core's (local/core-migration/qa/ref-trace.mjs on core9 /
  core10 and the live core), six riders and solo. The gate replay (event-race: 53,101 calls, 2418 probes of state, world state,
  pose and both RNGs) identical; a perturbed stream is caught at the first probe after the change.
- Open for a shipping native host: the in-world snapshot (web/rider_context.cpp SSX_SNAPSHOT_REGISTRY) copies the TLS block by
  size; with emulated TLS a function-local static first reached after snapshot_init lies past the saved size, so the native
  snapshot needs every variable assigned before its first save (the registry's static constructors assign the file-scope ones).
  Threads besides the simulation's (audio, render) must not touch RIDER_LOCAL state (they see the current context).

### Build and tests

The native-port agent owns native/ and the root CMakeLists.txt; the build steps and tests are in [native.md](native.md), the
maintained page. The first prototype was native/build-core-native.sh (local/native-core/race_driver and replay_calls, the gate
stream logged with native/tools/spy-core.js around web/compare-ps2-capture.mjs, the wasm reference from native/tools/ref-trace.mjs).

## Core API

The course core's exports are the API a front end builds on (the page through web/runtime/core.js, a native host by linking the
same sources). docs/core-api.md lists every export with its signature and comment (generated: `node tools/core-api.mjs >
docs/core-api.md`; `--check` says whether it is current). Exports are add-only: a changed signature or meaning gets a new
name, and the old one stays until no caller is left (the page, web/compare-*.mjs, native/). (The rule, confirmed 2026-10-13.)

A race as the page makes it (web/main.js, web/game-tick.js with pv coreTick, web/ai-racers.js with pv coreRiderManager; the order
native/core/race_driver.cpp follows):

1. Load, in the human's context: `set_fx_deferred(1)`, `init_animation(packets json, rig json, settings json, packets, size)`,
   `init_race(settings)`, `animation_use_physics(1)`, `init_world(triangles, count)`, `init_terrain(terrain)`,
   `init_world_collision(world, hash)`, `init_body_terrain(terrain)`, `init_rails(rails, hash)`.
2. Computer riders: per rider `rider_context_create()`, then in its context the same init_* calls with its rig and merged settings,
   `npc_configure(record, context)` and its stat bytes (`set_rider_attributes`) when they are not all 5; `rider_host(3 | 8)`,
   `rider_parse_cache_clear()`, in each rider context `rider_host(2 | 4 | 8)`; `manager_configure(blocks, slots, riders, count,
   flags)`.
3. Start: `reset_pad_history()`, `start_event()`, `manager_event_start(rider texts, context, world, grid, hold, isolate, ranks)`
   (startRun's full order, with the free-ride, multiplayer and in-world variants: "M4b" above).
4. Each tick: `manager_begin_tick(anchor, scores)` (with computer riders), `tick_begin(pad[24], rescues, placement, posed, bones,
   teleportKnown, teleports, tweak)` -> the record (state, pose, race info, flags), `manager_end_tick()` or `tick_mid()`,
   `tick_end(...)` -> camera and end flags, `tick_finish(...)` when the end flags say finished, `tick_results(io)` (results,
   rescue and quit flags). Read-outs: `rider_state()`, `pose_physical()`, `rider_world_state()`, `race_result_info()`,
   `score_object_dump()`, `trick_name()`, the FX / HUD / skin read-outs.
5. Pause actions: `race_give_up()`, restart (startRun again), `race_timed_out()`.

Host rules a front end has to keep (they are I/O or UI, not simulation):

- **The streaming hold (+0x1D0 wait, 2306B8).** In a streamed world (free ride, peak runs: web/free-ride.js) a game tick's gameplay
  is skipped while a location the rider can reach is not in: `peak_world_blocking()` (a row still reading after its Load trigger)
  or, on the host's side, an active (or about to be active) row whose collision or draw data is not loaded yet. The tick then runs
  only `peak_world_tick()` and the host's feed (web/game-tick.js: `freeRide.stalled()` -> `freeRide.tick()`, no other pass). The
  host reports a location's data with `peak_world_data_ready(track, 1)`; a native host whose data is always resident never holds.
- **Big Challenge prompts.** The offer queue (`mission_offer_peek` / `mission_offer_pop`: type 8 an offer, 9 a failed run) opens the
  host's prompt; the answer is an input event, `mission_prompt(kind, id)` (1 accept, 2 decline / quit, 4 retry). The status words
  and the cash the core queues (`mission_ui_events` kinds 5 / 6) go to the career (web/big-challenges.js drainEvents; M5f).
- **Stations and arrivals.** `peak_world_events` (kind 1 builtin 68: course, action, mode; 3 gates; 5 course changes; 6 the FAQ) open
  the host's cutscenes, maps and world loads (the CTM world-state machine, M8).

The rules (`rules_eval(fn, io, n)`, web/rules_bridge.cpp, RULE ids in web/core-rules.js) and the presentation generator
(`presentation_*`) are in both the course core and the rules module; a native host links them once.

Still on the page, to move (docs/core-migration.md plan): the career's result kernel, award cascade and lodge (M5c-e), free ride
and the peak world, Big Challenges, stage collectibles, the cutscene section feed; multiplayer rules (M7). Until they move, a native
host has to port them from the page's JS (same order, same draws) or run without them.
