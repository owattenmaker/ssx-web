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
| M2 | The game tick's core sequence (tick_begin / tick_mid / tick_end), the results timing; M2b the gates on the page's sequence | coreTick | in progress |
| M3 | The rider manager into C++ (stages, RNG cursor, pair host, world sync, FX phases) | coreRiderManager | planned |
| M4 | Event start (startRun's sequence, lineup install, anchor RNG, relationships push, plane-drop seeds, the toggles) | | planned |
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
