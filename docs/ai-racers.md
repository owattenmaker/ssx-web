# Computer riders (AI racers)

Race events now run the five original computer riders. A full Snow Jam race can be finished
against them in the browser, with the in-race place display, the Single Event Results and the
career results. The same data path loads Metro-City (BRA2). Other courses run without computer
riders unless they have an `npc-riders.json` file.

## Architecture

In the original, a computer rider is the same actor class as the human. The actor vtable
0x458660 differs from the human's 0x4583A8 in only two ways:

- The feedback hooks at +0x80..+0x9C are no-ops. The human versions write owner +0xDFC/+0xE00,
  which are NPC state on a computer rider.
- The route score at +0xA4 is 0x10D410 instead of 0x1446E8.

The motion owner then takes its command words from provider 0x10A768 instead of the pad
provider 0x127998. The browser therefore runs each computer rider through the complete rider
pipeline of the human's core, in its own **rider context** (see "One core, six riders" below):
one core runs all six riders, as the original's rider manager does. The six riders behave as one
world: they run in the original pass order, rider pairs are resolved inside the initiating rider's
own update, and every change to a shared world entity is replayed in every rider's world at once
(see "One world, the original pass order" below). Each rider uses the same controllers, motion,
animation, collision, crash, reset, rails and handplants as the human. Only these parts are
replaced:

| Replaced part | Browser | Original |
|---|---|---|
| Command source | `npc_tick` → `originalNpcControl` (engine/npc_input.cpp, npc_air.cpp) → `apply_command_words` (the pad path's per-controller decode) | 0x10A768 |
| Grid seed and identity | `npc_configure`/`npc_start_event` (web/npc_gameplay.inc), `browserEventRiderSeed` hook in `begin_event_rider` | countdown anchor |
| Body-query detail | `browserHumanRider=false` → coarse 3x3 world body queries (world_bridge.cpp, crash airborne query) | actor +0x868 kind 0 |
| On-route obstacle skip | `npc_obstacle_query_enabled` (13F4CC: within 50 cm of the route, skip the 0x3342D0 ground query; 105398 and 107888 still run) | +0x874 == 0 |
| Route progress | `npc_follow_route`: 0x1125C0 → 112A50 computer branch with the 10D410 score and the shared RNG | 121818 |
| Human-only reactions | 10E910 kinds 1–3 (Uber landing) and the 12E6BC get-up kind 4 are gated on `browserHumanRider`; kinds 5 (10E098 full meter) and 6 (1200D0 super timer end) now run for every rider | +0x874 |
| Time scale +0x300 | provider pacing 10DEF0/120090 (rubber band); ground motion keeps it for computer riders; boost timers use it | written only by 120090 |

Per-rider data comes from `tools/export_npc_riders.py [--location ARA1|BRA2]`. It writes
`local/assets/native/<course>/npc-riders.json` and `web/public/assets/<course>/npc-riders.json`
from the course's countdown-anchor savestate.

It uses the same verified extractors as the human settings:

- landing, boost, air entry, rail context, grab and trick profiles;
- animation inputs and secondary motion;
- reset/event route;
- ground profile and state (event-start.json);
- NPC driving, pacing and score state, grab catalog and relationships (tools/reference_npc.py);
- pair weights;
- the anchor's pair records and ranks.

The human's sections extracted the same way equal the shipped `initial.json`, which checks the
extraction semantics. The only per-character differences are body scale (0.92, 0.83, 0.94, 0.70
and 1.0), the Uber grab table, stance and the hair slot. All stats equal the human's.

Rider packages for rendering are `web/public/assets/RIDER_{PSYMON,ALLEGRA,MOBY,GRIFF,LUTHER}`
(tools/export_opponent_packages.py). `web/opponent-riders.js` draws them with the human's source
skinning and original rider lighting.

`web/ai-race.js` wires everything into `main.js`: roster loading, rendering, standings, the place
HUD and results. `?ai=0` races alone.

**Lineup:** the five riders are picked per human by the original roster build 0x23A4F0 (`web/lineup.js`). The records
are assembled from `/assets/<course>/lineups.json` (`tools/export_lineups.py`), and rider contexts are set up again in place
(`ai-racers.js setDocument`). The game RNG is set to the original's anchor words for the lineup (seeded 0 at the load, plus
the load's draws, plus 10; a backcountry rival event has no countdown and starts from its ready state's words, docs/backcountry.md "Rival computer rider"). Relationships age at each load (0x155E58) and change on pair reactions (0x155BF0). See
[characters.md](characters.md#computer-rider-lineups-2026-09-23).

**Anchor RNG without lineups.json** (2026-09-28, pv `eventAnchorRng`, `web/event-anchor-rng.js`). Some courses have no
lineups.json or rivals.json: Crow's Nest, Launch Time and Much-2-Much (big air), The Junction, Schizophrenia and
Perpendiculous (super pipe), Gravitude and Kick Doubt. They take the same anchor words, from per-course draw counts
measured on record 0 of each countdown-anchor capture (Zoe), with the lineups character rule and 2 start draws per
computer rider.
- Solo events are seeded by main.js `soloTickStart` when `core._game_tick()` reaches the anchor tick.
- Gravitude and Kick Doubt are seeded through `racers.setAnchorRng`.
- Before this the core kept its seed state's generator (Crow's Nest 109 draws for the PS2's 9, Gravitude 1381 for 26).
  Every random pick then differed from the PS2. The capture gates never saw it: `--sync-rng` copies the PS2's words
  each tick.
- Seeded once, with no per-tick sync, these captures stay exact to the end: crows-invert, crows-adjust, launch-time,
  much-2-much, perpendiculous, schizophrenia and pipe-event-start. The Gravitude page matches the PS2's shared RNG on
  300/300 ticks.
- Known leftovers:
  - one-tick ordering blips of 1-2 draws (the PS2 draws them before the human's provider; they resync the next tick);
  - a missed draw at t and t+6 in control 10 after the finish (Schizophrenia 3609): fixed 2026-09-28, see set-pieces.md "Timer
    splines after the finish"; the page's shared RNG is now exact to the end of the Schizophrenia capture.

## One core, six riders (2026-09-24; web/rider_local.hpp, web/rider_context.cpp, web/ai-racers.js)

Until 2026-09-24 every computer rider was a separate core instance (`createCore()` per rider, the
course world copied into each from a template memory image). On an iPhone a Snow Jam race with
computer riders was killed or drew half a world: each core's 128 MB wasm memory ended up resident
(~147 MB per rider in Chrome's renderer, 1.46-1.64 GB for the race vs 0.99 GB without computer
riders). Now one core holds all six riders.

- **Per-rider state is thread-local.** Every mutable global and function-local static of the core
  (web/*.cpp, web/*.inc, the generated course seeds, engine/original_input_provider.cpp's input map;
  `originalRoundingMode` already was) is `RIDER_LOCAL` = `__attribute__((no_destroy)) constinit
  thread_local`; node containers and objects holding `std::function` (graph, crash runtime, trail,
  sparks, sections, stage maps) are `RIDER_LOCAL_LAZY` (constructed empty on the context's first
  use). A rider context is one TLS block. The human's is the static block; `rider_context_create()`
  copies the pristine block (captured by a priority-101 constructor, before anything touches it) and
  runs the module's static-initialisation work in it (`rider_statics_core/rails/world/animation/
  environment`: the computed initial values that were dynamic initialisers — course seeds via the
  context's own fresh `browserGlideLocation` "ARA1", as in a fresh core — and the controller
  callback registrations), so a new context starts exactly like a freshly instantiated core.
- **Switching is one `global.set`.** The build (web/build-core.sh) compiles with `-matomics
  -mbulk-memory -ftls-model=local-exec`. With a non-shared memory wasm-ld keeps a static TLS block
  but no mutable `__tls_base`, so web/rider_tls.S defines it (and `rider_tls_size`) and
  web/patch-rider-tls.mjs writes their initial values from the link map (`.tdata` address and size)
  after the link. JS switches a context with `Module.___tls_base.value = block`.
- **Shared, not per rider:** the course geometry (web/core.cpp `world` triangle mesh and
  `cameraTerrain` patches, immutable after the load), constant tables (camera knots/shake tables,
  spline/magnet constants, score tables, the set-piece tables built from compiled seeds), Peak 1
  world residency, the environment textures (web/environment_bridge.cpp, immutable, ~5 MB) and two
  parse caches: web/world_bridge.cpp keeps the parse of the course's world_collision.json /
  terrain.json (the human's, done first) and a computer rider's `init_world_collision` /
  `init_body_terrain` copy it instead of parsing again (the parse is a pure function of the text;
  its one side effect, the set-piece location name, is replayed). Everything else — including each
  rider's copy of the world entities (rollers, set pieces, teeters, pickups), its race session, RNG
  cursors and visual stream words — is per rider, exactly as it was per core, so the shared-world
  replay (web/shared_world.inc) and the pass-order choreography below are unchanged.
- **API.** `riderContextCore(module, block)` (web/ai-racers.js) gives a rider context the core's
  API: every `_export` runs with `__tls_base` set to the block; `HEAPU8`/`HEAPF32` are the core's.
  `racers.npcs[k].core` is such a view, so web/opponent-riders.js, opponent-fx.js, rider-shadow.js,
  game-audio.js and the progress meter read each rider as before. The human's context is used
  through a view too inside ai-racers.js (rider pairs call the human's race world from inside a
  computer rider's 121750). The module-level hooks (`Module.riderHost`, `Module.pairHost`) dispatch
  on the current block: the phase hooks run only in the human's context, rider pairs go to the
  running context's slot.
- **Set-up.** A computer rider: `rider_context_create()`, then `init_race`, `animation_use_physics`,
  `init_world_collision`, `init_body_terrain`, `init_rails` (no `init_world` / `init_terrain`: the
  geometry is shared), then `init_animation` + `npc_configure` as before (about 0.1 s per rider in
  node; the old path built a world template in a worker and copied ~60 MB into each core). The
  computer riders' packages no longer need collision.bin. web/ai-world.js / ai-world-worker.js are
  no longer used.
- **Presentation only, per drawn frame.** The computer riders' skin palettes and rider lighting
  (web/opponent-riders.js `capture`) are captured only on the last two ticks of a frame
  (web/fixed-step-clock.js `FixedStepClock.ticksLeft`; a frame interpolates between those two), with
  a teleport reset kept pending until the next capture. The palette is a pure function of the cached
  pose (docs/multiplayer.md) and the lighting refresh keeps no history, so nothing the simulation or
  the RNGs read changes. The FX passes, poses and physics of every rider still run every tick.
- **Exactness.** Every gate matches the multi-core build word for word: test-ps2-captures (all
  scenarios; the seven six-rider capture reports are byte-identical), test-ai-racers, test-lineups,
  test-long-runs, test-stage-world (six-rider parts), the online tests, and full Snow Jam / Metro-City
  races side by side (16000 ticks, every rider's state, progress, game and visual RNG words each
  tick). `web/test-rider-contexts.mjs` (npm test): a context created after its core's human raced
  700 ticks rides 1500 ticks exactly like a freshly instantiated core.
- **Build check.** web/check-rider-globals.mjs (run by web/build-core.sh on the link map) fails the build when a web/ or
  engine/ object has a plain mutable global (`.data`/`.bss`) that is not on its shared list: a new global must be
  `RIDER_LOCAL` or be added there with its reason, otherwise all six riders would share it.
- **Measured.** Node: one memory of 161 MB (was 6 memories, 804 MB; 386 MB touched, now 128), process RSS 336 MB (was
  1028). Chrome, 390x844 mobile emulation, 4x CPU, interleaved with the old build under the same machine load: renderer
  RSS after a forced GC 1020-1075 MB (was 1315-1454; without computer riders 923-1041), 7.5-10 fps (was 6-7.5); desktop
  1108-1235 MB (was 1372-1385), 60 fps both; event load ~8 s (unchanged at 1x; the rider set-up is ~0.1 s per rider).
- **Audit (static per-rider state, 919 TLS objects, ~102 KB per context):** physics/collision/motion
  (core.cpp, 21 KB), FX (trail, snow, wake, sparks, breath, boost; 28 KB), score/HUD/tricks (12 KB),
  animation graph/pose/skin/crash (10 KB), NPC provider and the pair world (9 KB), stage world and
  scripts (8 KB), environment/fog/lighting, peak streaming, camera, audio queue, input, race clock /
  reset route, rails/teeters, set pieces/sections (1-3 KB each). Heap state per rider (its world
  entity copies, body-collision instance copy, terrain grid copy, animation clips, rider package,
  lights) is about 12-15 MB.

### Next step: the simulation in a worker (not done)

2026-09-25: after the exact speed-ups of docs/sim-performance.md the simulation is under half of a phone frame; that doc's
"Phase 4" has the step-by-step plan (frame record per drawn frame, the lazily built presentation buffers read in the worker).

With one core the whole simulation could run in a Worker and leave the main thread to rendering. It was not done here
because it is not a clean cut yet: main.js and the renderers read the core directly every tick and every frame (about
50 exports: HUD and audio state, camera, the snow/trail/wake/boost buffers, skin palettes, rider lighting, set-piece and
stage-world logs, `racers.npcs[k].core` views), mostly as views into the core's memory. Two ways:
1. **Shared memory.** Build the core with a shared `WebAssembly.Memory` so a worker simulates and the main thread keeps
   reading the same memory. Needs cross-origin isolation (COOP/COEP headers in web/server/mp-server.mjs, the Vite
   config and deploy/edge-worker.js; every cross-origin asset must allow it) and a tick/frame handshake so a frame never
   reads a half-written tick (double-buffered presentation records, or the worker pausing at frame boundaries). With a
   shared memory wasm-ld makes `__tls_base` mutable itself (web/rider_tls.S / patch-rider-tls.mjs would be dropped).
2. **Snapshots.** The worker posts, per drawn frame, exactly the values the main thread reads (a transferable record per
   rider, like the online FX records of docs/multiplayer.md). No isolation headers, but every consumer in main.js must
   move to the record.
Either way main.js's `simTick` and the fixed clock move into the worker; that is main.js restructuring beyond this change.

## One world, the original pass order (web/ai-racers.js, web/race_world.cpp)

0x128AF0 runs every pass over all riders in slot order: 10F560 (manager), 120F20, 121068
(providers, controllers), 1210B0, 1211F8, 1216E0 (motion update), 121700/121728 (pose; 11EB98 →
106828 builds the AA0 spheres), 121750 (second motion phase: touchdown, 13F488/13AA48, 105398,
rider pairs 107888, the 13F358 clamp, rail/crash posts), 1217F8/121818 (course progress, route,
triggers), and after all riders the world pass 0x101B60. The rider contexts run each rider in three stages:

| Stage | Human (main.js `simTick`, unchanged) | Computer riders (ai-racers.js, each in its context) |
|---|---|---|
| 1 up to the pose | `pad_tick`, `race_begin`, `step_rider`, `animation_tick` (first half: `animation_pose`) | `npc_tick`, `race_begin`, `step_rider`, `animation_pose` |
| 2 121750 | second half of `animation_tick` (`animation_post_phase`) | `animation_post` |
| 3 121818 | `race_end` | `race_end` |

The human's context drives the others through two hooks (`Module.riderHost`, enabled by `rider_host(3)`):
its `animation_tick` calls `afterPose` between its pose and its 121750 (the computer riders' stage
1), and its `race_end` calls `beforeProgress` first (their stage 2). `endTick` runs their stage 3 and
the world pass. A tick that ran without the hooks falls back to running the missing stages.
Computer riders get `rider_host(2 | 4)`: bit 4 skips their renderer bone poses (animation_post's return, read only for the
human; a computer rider is drawn from its skin palette), docs/sim-performance.md "Simulation core, round 3".

- **Rider pairs 0x107888** (`OriginalRiderPairSystem`, engine/rider_pair_system.hpp) are dispatched
  from inside each rider's 121750 (`rider_pair_point`: after 105398 and before the 13F358 clamp in
  13F178/139C88, the 13C098 slot of the rail post, after 105398 in the crash posts 137860/138640), via
  `Module.riderHost.pairs` → `race_world_pairs(tick, slot)` in the human's context. A later rider is bumped
  before its own 121750; its view reports the unclamped velocity (`groundUnclampedVelocity`, 13F358
  has not run) and its pose-time AA0 spheres; a velocity set by an earlier rider becomes its unclamped
  velocity and is clamped by its own 121750. Callbacks: `pair_view`, `pair_translate` (0x106538),
  `pair_set_velocity` (0x107E70 + 0x1135B8 reseed), `pair_react` (soft 0x108388, crash 0x10EB30).
- **Shared game RNG 0x4FF030.** Stage 1 draws are controller-pass draws (121068: providers, 115D48,
  131620) and continue one controller cursor in slot order; the rare other stage-1 draws start after
  every rider's controller draws (later riders' counts predicted from their previous tick). Stages 2
  and 3 draw from one cursor in slot order: the running context holds it, pair dispatch draws from it and
  a reaction in another context runs on a copy of it. Then the world pass.
- **Manager refresh 0x10F560** every sixth game tick before any rider pass: ranking 0x10F998 (+0xEC,
  Knuth-gap shell sort 0x3E6328), pacing bounds ±50000, pair proximity records (+0x1C = relationship
  ≥ 2), designated peer 0x10F878.
- **Shared world entities** (web/shared_world.inc). Each rider context keeps a world copy (as each core did);
  entity updates (race_begin: rollers 35E850, teeters 342358, splines, MultiSplines) are deterministic and
  run in every context. Every change a rider makes to a shared entity is logged (`world_events`) and replayed
  (`world_event_apply`: no RNG draw, no effect on the replaying context's rider) in every other context right
  after the stage that made it, so later riders of the same tick see it: 1 scripted instance contact
  (crashbag builtin0/15 creation, RollerModifier re-kick), 2 boost pickup taken, 3 log-teeter attach
  force, 4 trigger contact (the renderer's contact log), 5 section MultiSpline activation, 6 stage
  trigger contact (web/stage_script_gameplay.inc). Course-script spline launches (121818 → 30A060 slot
  2) are replayed by key (`world_triggers`/`world_trigger_apply`) the same way.
- **World pass 0x101B60** (section activation, its builtin3 key8/key6 draws): native. The human's context runs
  `section_pass()` after every rider's 121818 (web/ai-racers.js endTick, main.js without computer riders):
  web/section_gameplay.inc on engine/section_streaming.hpp, slot-1/slot-3 programs through the stage VM
  (web/stage_world.inc), LiveComp random starts drawn from the shared gameplay RNG in resource order.
  `compare-ai-capture.mjs` no longer takes world draws from the log when the sections load (2026-09-23).

## Finish (control 10, web/finish_gameplay.inc, every rider)

Controllers 0–3 start with 0x116378. When +0x470 ≥ 0 they request control 10 (0x12C678):

- **Phase 0:** turn target ±0.1, boost off, semantic 11, velocity ×0.97 with brake target 1.
- **Phase 1:** velocity ×0.93 until |brake| < 0.2 and turn 0. Then the 10E028 reaction
  (+0x100 ? 1 : 4) and semantic 4.
- **Phase 2:** the pending reaction plays at rate 0.75. Afterwards +0x470 = max(+0x470, 10).
- **Phase 3:** stopped.

In phase 0, with +0x2DC == 0, 0x114CC0 first turns a rider moving backwards around by 180 degrees
(`originalReverseTurn` + 0x115168, semantic 21); only then does phase 0 require the ground (11FE98).

## Standings, HUD and results

- **In-race place 0x21E1B0** (`web/race-place-hud.js`, drawn by ui.js in the game screen during
  race phase 5).
  - Atlas `OV_1-3`: `nm1w`..`nm6w`, st/nd/rd/th, slash and the rider count.
  - Layout: (20,20); number 26×42 for 1st, otherwise 40×42.
  - Colours: gold (1, .8, 0) for 1st, blue (0, .494, .7) otherwise.
  - Change animation from 0x1EA930: 1st scales by 1 + sin(4πt)·0.1996·(1−t²); other places fade
    alpha by 0.5t + 0.5.
  - 1st-place glow (0x21E7E0 twice, draw order 10, behind the number and suffix): while the rider
    leads, the place object's +0x4C timer runs 0 → 2 and wraps (+0.026782159 per tick, 0x1EBC10;
    -1 otherwise). The renderer-owned part texture (hud+0x474 = handle 0x5F5, `UI/part-glow.png`) in
    the number colour covers each rect grown about its centre by 1.8101751 (gp-0x535C), alpha
    A × (0.5 + 0.5·tri(g)); a rect wider than 72 is drawn as two 36-wide ends and a constant-U middle
    (`placeGlowStep`/`drawGlow` in web/race-place-hud.js).
- **Single Event Results.**
  - When the last human finishes (race clock requestResults), rows are ordered by time (0x238BF8).
  - Riders still on course get the 0x122D78 estimate: raceTicks + remaining / max(average speed,
    30 − slot).
  - Shown as Rank/Riders/Time mm:ss (ui.js).
- **Career** (docs/career-events.md hooks): `ui.cb.standings()` returns `{human, character|name,
  finishTicks|null, remaining, place, origin, dnf}` per slot, `ui.cb.lineup()` returns the five
  names, and `showResults` passes `standings`.

## Verification

Captures come from `tools/ps2_capture.py build ... --ai-state`. Each 32 KiB record holds every
computer rider's actor/owner windows, provider words and a per-draw RNG log attributed to the
rider-manager pass. `web/ps2-capture-ai.mjs` reads them. `web/compare-ai-capture.mjs RUN.bin
--zoe [--isolate] --world-draws` runs the human pad plus the five native computer riders from the
countdown anchor. Nothing is copied from the capture after the anchor, except one unported
world-pass draw (LiveComp 0x341AA0, `--world-draws`).

**Computer-rider poses (`build --ai-bones`, 2026-10-05, fuzzing agent).** This needs --ai-state and can't be used with --watch:
it takes the watch area.
- **Recorded:** each record also gets every computer rider's posed world bones *(*(actor+0x780)+0x2C), sampled with the AI windows
  (layout.ai_bones):
  - bones 0..21 as positions: the body-volume centres that 13AA48 / 13F488 / 105398 read;
  - bone 22, the board root, as the full row: 13A7B0's landing-probe origin and up.
- **Compared:** compare-ai-capture.mjs checks the port's world_pose_bones for each rider bit for bit, and reports
  summary.ai[k].boneTicks / firstBoneInexact.
- **First results (mode 1, runs/fuzz-mode1):**
  - The unmutated gravitude-race-ai pad has every rider's physics exact to the end, but Luther's whole-body pose is about 1 cm
    off from 2403.
  - Fuzz variant r2-0204 has Mac's pose about 2 cm off from 2163. His physics follows at the 2585 landing normal.

| Capture | Human | Psymon | Allegra | Moby | Griff | Luther | Shared RNG | Ranks, pair records |
|---|---|---|---|---|---|---|---|---|
| event-race-ai (isolated, 2400 ticks) | all | all (was 1845) | all | all | all | all | all (was 2316) | all |
| ai-idle (isolated, 2600 ticks) | all | all (was 1548) | all | all | all | all | all (was 2494) | all |
| event-race-ai-pairs (not isolated, 2400 ticks) | all (was 285) | all (was 1301) | all (was 285) | all (was 1499) | all (was 777) | all (was 771) | all (was 835) | all (was 905 / 773) |
| parity-ai/metro-race (Metro-City, isolated, 1199 ticks) | all (score was 1096) | all | all | all (was 554) | all | all (was 506) | all (was 563) | all |
| parity-ai/ara1-full (Snow Jam, isolated, 3899 ticks) | all (was 3799) | all (was 3282) | all (was 3587) | all (was 3312) | all (was 1644) | all (was 2709) | all (was 2404) | all |
| parity-ai/ess3 (Moby only, 999 ticks) | all | - | - | all (was 815) | - | - | all (was 822) | all |
| parity-ai/dss2 (Moby only, 1799 ticks) | all | - | - | all (was 1509) | - | - | all (was 1626) | all |
| peak2/cra3-race-ai (Ruthless Ridge, isolated, 5999 ticks) | all (was 1384) | all (was 3444) | all (was 1684) | all (was 1869) | all (was 1878) | all (was 1684) | all (was 1152) | all |
| parity-ai/ass1 (Moby only, 5300 ticks) | all (was 5103; score 3302) | - | - | 5034 (was 3673) | - | - | all (was 3721) | all |

"All" means bit-exact position and velocity every tick. Field-level checks cover about 60 ground
fields, the NPC extension DF0..F40 (all trick-plan words), command words, control/motion,
+0x300, AB8 route, +0x4D0 and the speed limit +0x2E4 (1-ulp differences at a few ticks, same as
the human gate). These cases gate `web/test-ps2-captures.mjs` (every rider, the RNG, the ranks and the pair
records must stay exact to the end).

Instruction-level oracles (the existing style):

- `tools/test_npc_input_native.py` now also covers the rail producer 0x10AED8: 5000 cases, 3367
  jump-zone launches, commands, state and RNG exact.
- `tools/test_npc_provider_native.py` (+`--air`): the complete 10A768 provider on live contexts.
- NPC leaf arithmetic uses explicit scalar helpers (engine/npc_float.hpp) so the WASM build gets
  the software toward-zero path. All NPC oracles still match after that rewrite.

Recovered or fixed for the one-world race (2026-09-22, second pass):

- Stage-ordered world (above): pairs inside 121750, 121818 after every 121750, shared entities.
- WASM rounding: `engine/rider_pair_collision.hpp` used `std::sqrt` under the chop guard. Native arm64
  honours the FPCR mode, WASM `f32.sqrt` is always nearest, so the pair direction and impulses were
  1 ulp off in the browser only (the native oracle passed). They use `terrain_original::sqrt` now.
- A terrain departure switches to air motion at 13F2CC (`11FE78(1)`), after 13F488/105398/107888: a
  crash entered by the obstacle response of a departure tick starts sliding (previous motion 0) and
  pair views report a grounded actor (`groundDeparturePending`).
- A touchdown that enters a landing crash before 13AA48 (139C88): the rest of the air post runs with
  the rider as the ragdoll. The 13AA48 response moves the crash actor and 105D98 dispatches as a
  ragdoll impact (restarting the crash predictor with the post-response state; `crashInPost`).
- 137860 (crash airborne body contact, not landed) only pushes, drops the closing velocity and
  restarts the predictor; it has no 105D98 reaction, so the collision history is not updated.
- 115D48's peer-reaction loop checks record +0x1C (the rival flag 10F560 sets from relationship ≥ 2),
  not +4 (peer is human): Allegra no longer taunts the human.
- A passive flight (12F730) leaves +0x2DC alone: the landing (13A968) reads the retained manual spin
  instead of the air spin rate (Psymon ai-idle 1549, Luther event-race-ai-pairs 1293).
- The rail post's 13C140 body query has no 13F488 normal filter (0x13C1A0 builds it without one; the
  response only rejects an opposed normal): Psymon's 10 cm rail attach at event-race-ai 1846 was a
  9.27 cm terrain contact the browser filtered out.
- A computer rider near its route skips only the 13F488 query (13F4CC); 105398 still runs.
- Computer-rider cores free their init inputs right after each init call: ~55 MB used instead of
  ~75 MB (heap still 128 MB).

PS2 evidence for these came from re-running the capture scripts with extra `--watch` windows (Luther's
AA0 body, motion owner and ragdoll predictor rider+0x788; Psymon's world bones *(*(rider+0x780)+0x2C),
query caches rider+0x864/+0x868 and actor window +0x190/+0x420/+0x460), and one savestate at tick
1548 for the original 13A7B0 landing oracle (tools/test_landing_contact_reference.py pattern).
QA exports added: `crash_air_contact_info`, `crash_trajectory_info`, `rider_trajectory_info`,
`rider_query_caches`, `upper_request_info`, `rail_exit_debug`, `body_query_probe`.

Recovered or fixed in the first pass:

- 10AED8 control 7.
- The 10B250 E24 clear on grab release.
- The EE saturating zero-speed path score.
- The reset request switching the tick (0x116120: control 9 updates from the next tick).
- Frame-begin 11B3F8 speed limit using the real motion mode (crash 2, reset 3).
- 10E098 kind-5 and 1200D0 kind-6 upper reactions.
- `web/core.cpp` no longer has a host height failsafe. It teleported every rider to the grid 1.8
  km below the start, before Snow Jam's finish.

A full race, headless (node, about 60 s):

```
node -e "import('./web/ai-race-node.mjs').then(async({createNodeRace})=>{const r=await createNodeRace({});r.start();const pad=new Float32Array(24);pad[22]=1;for(let t=0;t<16000;t++){if(r.tick(pad)[3])break;}console.log(r.racers.standings())})"
```

With the human tucking (left stick up), the five computer riders finish at race ticks
12048–13202 and the human at 14273. The results are then requested.

## Parity fixes, 2026-09-28 (physics-parity agent)

The six-rider captures were ranked by their first computer-rider or shared-RNG difference (`compare-ai-capture.mjs --zoe
--isolate`; the page comparer agrees tick for tick). Three PS2 facts fixed the metro, ESS3, DSS2 and ARA1 captures:

- **Scope list per roster slot.** The rider's ground query 13D818 and body queries 13F488 / 13AA48 walk the rider's scope
  list (rider+0x860, 332DB8), like the landing probe. The pre-pass 120F20 rebuilds it when the game tick % 3 equals
  rider+0x86C % 3, so the refresh phase is per roster slot: the human 0, computer riders 1..5 at 1, 2, 0, 1, 2. The PS2
  metro-scope watches of all six +0xB50 lists confirm it. 120F20 also rebuilds on a world change (scope+4 differs from the
  world's +0xA0, 1448A8 or world+0x70 == 1); that branch is not modelled. Every 11D660 placement rebuilds the list too (reset,
  booth teleport, mission teleport: `apply_reset_placement`), and `reset_rider` clears it, so a restart rides like a fresh
  load. Metro Luther used to stay on the ground at 506, because patch 75280 only joins his list at 508.
- **Landing stance restore.** 139C88 runs 115640 before 11FEC8(0) on every landing except the board-press one
  (0x13A5A8 / 0x13A644). A soft collision off a rail (control 7 → 3) keeps stance 4 (+0x328) through the air, and the touchdown
  then turns the physical frame 90° and clears it (ESS3 Moby 815). `finish_landing` now calls `restore_stance`.
- **Air query filter from the current pose.** The rider manager poses (121728 → 11EB98 → 11FA10, frame +0x160..+0x190)
  before it moves (121750 → 1114A0 → 139C88 → 13AA48), so 13AA48's filter (query+0x10 = rider+0x180) is the frame posed
  in the same tick. The port used the previous tick's up (`prePoseUp`). ESS3 Moby 839 slid along a slope triangle at dot
  0.7979 where the PS2's 0.8003 drops it. This fix also cleared DSS2 and all of ARA1 except Luther at 3799.

- **Departure seed before the clamp.** 13F178 on a departure calls 114298, then its contacts (13F488, 105398, 107888), then
  11FE78(1) → 1399E0 → 1135B8 at 13F2CC, which seeds the flight. Only after that does 13F358 clamp +0x1E0 to the speed limit.
  The predictor therefore integrates from the unclamped velocity. The browser seeded it from the clamped one, so the first air
  tick came out 2 ulp slow in every component. `groundUnclampedVelocity` now also covers departures: the post stage restores it,
  re-seeds, and then clamps. ARA1 Luther 3799 → exact (the whole ARA1 capture is exact to the end), ERA5 Luther 1472 → 1804,
  CRA3 rider 3 1869 → 2137, DRA4 human 835 → the end (physics gate) and RNG 2439 → 2640. DRA4 riders 2 and 3 now leave at
  3142 / 2956 instead of 3157 / 3083. Both come after the shared RNG differs, so their gates were lowered with a note.

- **A crash from an earlier rider's pair keeps +0x390.** The PS2's board-normal update (13F2E0: +0x390 = unit(+0x390 +
  0.5 × +0x370)) is part of 13F178, the ground post. When an earlier rider's 107888 crashes this rider after its ground motion
  (1216E0) but before its own 121750, 1114A0 runs the crash post instead, and +0x390 stays. The port updates it in `step_rider`
  (stage 1). It now restores it when `postMotion` is 2 but the crash began after stage 1 (Gravitude Mac 873, knocked down by Nate).
- **A sliding crash bounce is not a collision event.** The sliding crash post 138640 pushes (106538) and bounces the velocity
  inline. It calls only the hit entity's contact-velocity callback (vt+0x154), never 105D98, so the collision history
  +0x3E0/+0x3F0 and the crash impact flag +0x54 stay. `BrowserCrashRuntime::contacts` dispatched an impact on every sliding
  bounce. Results: Gravitude Mac 977 fixed (Nate / Allegra 1129 → 1232 / 1273); ASS1 Moby 3673 → 5035, with the human and the
  RNG exact to the end (were 5103 / 3721). The human's own crash slides change too (sim-diff crash_info[11] in DBC2); every
  capture gate and the crash tests pass.

- **A stale soft-frame flag on a rail.** `browserSoftFrame` ("12E778 ran this tick") was only set on ticks that reach the soft
  controller. A rider that left a soft collision (control 3 → 0) and took a rail the next tick kept it through every rail tick,
  because the rail path returns earlier. That kept 115D48 off on the rail. The PS2 clock (+0x35C) ran, and its 1 s reaction
  drew at CRA3 1151. `step_rider` now clears the flag at the start of each tick. CRA3 human 1384 → 2279, RNG 1152 → 1696,
  Allegra / Luther 1684 → 1694. Psymon and Moby now leave after the RNG difference; their gates were lowered with a note.

- **A landing rider's pair view.** On the PS2 a touchdown (139D78 → 106538) moves the rider and its AA0 spheres before the same
  121750 runs 107888. The port commits the landing translation to the body volume only after the contacts, so `pair_view`
  reported pre-touchdown spheres for a rider that had just landed. It now adds the pending translation while that rider's own post
  runs (as `crash.beginControl` already did). CRA3 1694, Allegra landing onto Luther: all six riders, the RNG, the ranks and the
  pair records are now exact to the end (5999).

- **In-race relationships.** 10F560 sets each pair record's +0x1C (the 115D48 peer-reaction eligibility) from relationship(a, b) >= 2,
  and the rider-pair reactions change relationships during the race (0x155BF0: +1 soft bump, +2 crash, +4 soft attack, +6 crash
  attack; level = score / 5). The comparer kept the document's levels fixed, and so did the page for every event without
  lineups.json (backcountry rivals, Gravitude, Kick Doubt). PS2 watch of the tables (0x4A6CA8 + bank·0x9B50 + char·0xF88 + other·3 +
  0xBC1) in bc-race-tuck2: Mac's record of Zoe goes 000000 → 000001 (391) → 000105 (397) → 000109 (595) → 00020d (599). At level 2
  the human's 1161 reaction becomes 319 (look at Mac, 1.4 s) instead of 317 (1.33 s), and the idle clock and RNG follow. Now:
  - `compare-ai-capture.mjs` ages the fresh table once (0x155E58) and uses it when it gives the document's levels; it applies
    every reaction (`racers.onReact`). `--no-relations` turns this off.
  - web/ai-race.js (pv `rivalRelations`) keeps session tables for events without lineups.json too.
  bc-race-tuck2 is exact to the end (was human 2009 / Mac 1547 / RNG 1302), both in the comparer and in the real page
  (test-rival-page `abc1-tuck2`). ko-attack is exact to the end too (was 265 / 266 / 309).

- **Air orientation after leaving a rail.** 139A20's orientation tail (0x139A64: approach the predicted landing normal and heading)
  runs on every air-motion tick, whatever the controller. A rider that left a rail is still in control 7 while 0x132770 waits
  for the rotation clip, and the port skipped the tail for any rail-owned frame except control 12. It now runs for control 7
  too, once the rail step no longer owns the tick (`railStepConsumed`). The Throne wind capture: Psymon 2941 (quaternion from
  2925) → the human, Psymon, the RNG, the ranks and the pair records exact to the end (7999).

- **Section LiveComps with timer programs (Gravitude's crash billboards).** The rider's contact with
  mdl_ERA5_CRbillboardTrigger_1000 runs program 83. Its builtin 28 (0x2FF9A8 → the LiveComp's vt+0x120 = 0x341FE8, properties 0x64 delay,
  0x65 time, 0x66 rate, 0x67 range end, 0x68 range start, 0x69 once) plays the billboard 332333 on to 149 / 30 s. The billboard was
  started by its section-enter program 84 (loop 1..45 frames) and has a slot-5 timer program, 85. The LiveComp tick runs
  it every tick: builtin 54 (0x301680, time × 30), builtin 55 crossings, sounds, builtin 77 randoms and builtin 13 ice pieces.
  The port drew section LiveComps only with JS players, so slot 5 never ran and builtins 28 / 54 were unported.
  - A section-started LiveComp whose instance has a slot-4/5 program is now also a core entity (`sectionPlayer`: the JS still draws
    it). This applies only to constructs without draws; there are 14 such instances on the mountain.
  - Builtins 28 and 54 are ported. Builtin 91 (0x3050F0, a camera shake for nearby riders) is a no-op.
  - Results: ERA5 six riders, the human, the score, the RNG, the ranks and the pair records are exact to the end (2799; was RNG
    1283 and riders from 1472). Gravitude race: human 1506 → 3915, riders 1232.. → 4442..5054, RNG 1195 → 3760.
  - The billboard draws land in the entity pass, which the record samples one tick earlier. `compare-ai-capture.mjs` now lists
    such one-tick RNG differences as `rngBlips` rather than as the first RNG difference.
  - The JS player now takes the core's clock (pv `sectionClock`, docs/set-pieces.md), so the fall is drawn as on the PS2.

- **The rail's airborne exit tick.** When 0x132770 ends control 7 for a rider that has left its rail, 1211F8 still approaches turn /
  brake / crouch in that tick. The port's rail step returned before its approach, so the crouch trailed by one step (Gravitude
  Allegra 3749: +0x220 0.4833 → 0.4296 on the PS2) and the control 4 → 5 request came a tick late (3760). Approaching the full triplet
  list there broke the human rail gates; turn / brake / crouch alone is what the PS2 shows. Gravitude race: the human, Nate, Mac and
  Luther exact to the end, Allegra 5054, Moby 5139, RNG 5678.

- **A held jump that meets a rail.** Control 2's controller 12E9B8 calls 116378, 116120 and then 106848. When 106848 attaches, it
  returns at once. The attach tick therefore sets no boost (114130), crouch / brake (113F88), rail steer (113F38) or prewind
  target, and does not choose a new clip (12EE30); only 1211F8's approaches run. 106848 does not request control 13 / 7 for a
  held jump either (only controls 0 / 4 / 5 / 11 do), so 131D08 never runs and +0x200 keeps its rate of 1/30. The port did all of
  this on the attach tick and entered control 7's state. The prewind rates (+0x2A4 / +0x2A8) were off first, then the clip it
  chose moved the posed bone 22, so the rail step's lateral offset (and its lean) differed on the next tick (Gravitude Moby 5136:
  quaternion off by 3e-6 at 5137). Gravitude Moby 5139 → exact to the end. sim-diff differs in 1 of 16 runs (BRA2 trick1 995,
  the human attaching to a rail with Cross held); every capture gate passes.

- **A crash stops the boost.** The crash controller 12CB68 begins with 114130(rider, 0, 0), the boost control with nothing held,
  so +0x2FC drops to 0 on the first crash tick (PS2 Gravitude Allegra 4911). The port's crash controller never called it, so a
  rider who crashed while boosting kept +0x2FC = 1. Its get-up tick (12CB68 recovers, control 0 does not run that tick) then fed the
  boost term to the ground drive 13CE50 (0.17 × 0.08 × 2350 cm/s², about 1 cm/s over the tick): Allegra 5054. `control_crash` now
  calls `originalBoostControl(false, false)` first. The Gravitude race is exact to the end: the human, all five computer riders, the
  RNG, the ranks and the pair records (5999). sim-diff: all 16 runs identical; every capture gate passes.

- **A soft collision off a rail wall keeps its control-7 exit.** 108388's 11FEC8(3) runs 132048, which sets +0x238's target to 0 at
  rate 1/15, so the fading rail cycle's 18..20 blend follows it down (PS2 Kick Doubt Moby, sequence 19 slot weights 0.5364 →
  0.6031 → 0.6697). In the port, the rail post's pair phase (107888) called `rail_apply` with the rail view captured before the
  reaction. With computer riders racing, that put the old +0x238 target back, and the pose drifted by up to 12 cm, so the next
  wall push differed (1.92 against 0.03 cm).
  - Found with watch captures of Moby's AA0 spheres, world bones, local pose and (subject-swapped) sequences:
    `local/ps2-capture/runs/parity-ai/ess3-{aa0,bones,local,moby}`.
  - `rail_exit_contacts` now re-reads the rider after a soft reaction.
  - New gate `parity-ai/ess3-long` (3700 ticks): the human, Moby, the RNG, the ranks, the pair records and the score are exact to
    the end. It was Moby 2945, RNG 2978, human 3278; the HUD sweep's opponent score difference by 3219 came from this.

- **The jump-release tick approaches the rail triplets.** 12E9B8's release (Cross let go on a rail) leaves the rail in the controller,
  and 1211F8 then approaches every triplet, including +0x22C steer, +0x238 balance and the +0x25C attach tolerance, which 13C5A0 has
  just reset to -0.25. The port's release branch returned before the rail step's approaches, and step_rails skipped them because the
  rail was owned when the tick began. So the steer lagged one step: R&B Moby 5033 had -0.5877 against the PS2's -0.6504. His re-attach
  at 5034 then slid 0.21 cm short (the whole 0.2 cm, all along the rider's right). step_rails now also approaches after a release
  (`railHeldRelease`). ASS1 is exact to the end (was Moby 5035); gate raised.

- **A rider's pair pushes reach its cached bones.** 107888's 106538 adds each push to rider+0x9D0, the companion translation. The rider's
  own 121750 commits +0x9D0 to its cached world bones together with the contacts' translation (310530), and the next 121020 clears
  it. So a push from the rider's own dispatch, or from an earlier rider's in the same tick, is in the next tick's board bone, while a
  push after its commit is dropped.
  - The next rail step (13AF28) queries and slides from that bone. PS2 DRA4 Psymon: record 1855's world bones carry the 2.05 cm
    push from the 1854 pair contact, and the web's 1855 rail step started from the unpushed bone (z 0.46 cm off at 1856).
  - `pairCompanion` (web/animation_bridge.cpp) now accumulates pair_translate from the rider's animation_pose until its post commits it.
  - DRA4: riders 1 / 4 1856 → 3927 / 4436, RNG 2640 → 3773, riders 2 / 3 → 4533 / 4030. The isolated human now leaves at 5088, on
    the differing RNG; its gate was lowered with a note.
  - Every other six-rider capture is still exact to the end. sim-diff differs in 7 of 16 runs, all in pose_physical at a pair push
    (for example BRA2's start-gate crowd), which is the intended change.

Still differing: DRA4 from 3773 (RNG) / 3927 (Psymon). Gravitude's RNG 1195 turned out to be the crash billboard's timer program (see above); the draws
land one tick apart only because of where the record samples the entity pass (`rngBlips`).

Known uninitialised read: the rail command 10AED8 stores sp+0x18 into owner+0xE20, but 10B980 writes that slot only on a
jump-zone hit. On a miss +0xE20 takes the stack residue left by the previous roster rider's controller pass (121068 → 111728 at
the same depth). In ground control 0 that residue is that rider's steer field (word0 bits 20..25, signed, / 31); ERA5 slot 4
matches N3's steer on every miss. For slot 1 it comes from the human's controller. The port writes 0. The value is only
tested for nonzero / == 1 (10B250, 10B0E8, 10CAD8). It has not caused a divergence in the captures so far (ESS3 796 is exact
through the end without it).

## Known gaps

- **Departure tick ordering.** core.cpp runs a terrain departure's 114298 in `step_rider` (stage 1),
  before earlier riders' 121750. A pair impulse onto a rider on the very tick it leaves the ground
  therefore reaches it after its 114298 (the motion-mode view is corrected, the takeoff is not).
- **RNG prediction.** A stage-1 draw that is not a controller-pass draw (rare; none in the captures)
  starts after every rider's controller draws, with later riders' counts predicted from their
  previous tick.
- **World pass 0x101B60**: native since 2026-09-23 (see above); `--no-sections` restores the old log-taken draws.
- **Mid-race seeding.** Computer riders start only from the countdown anchor. Captures that start
  mid-race (glide savestates) still use `compare-ps2-capture.mjs --sync-rng`: seeding needs every
  rider's full dynamic state (animation sequences, air/rail/crash controllers, predictor, NPC
  plan) at that tick, which the anchor extractor does not cover.
- **Wind push 0x125970 is human-only** (rider+0x874): ported 2026-09-25 (docs/weather.md); computer riders never push. It is 0 in these captures (no wind on Snow Jam).
- **The finish-line trick bonus 0x1194C0** is human-only and inactive here.
- **Channel-1 reaction masks (done 2026-09-22).** 11C298 builds rider+0x8C0 (310CE8 over bone list
  457A90), +0x8C8 (457B38) and +0x8D0 (4A1090 "morph", ORed into both). They differ per character
  (0x8000fffe/0x8000fff8 Zoe, Sam and Griff; 0x4000... Psymon and Luther; 0x2_0000... Allegra and Moby; +0x8D0 is 0x870
  for all). `tools/export_npc_riders.py` exports them as `identity.upper_mask8c0/8c8/8d0`, and `npc_configure` sets
  `riderMask8C0/8C8/8D0` (web/animation_bridge.cpp). These feed 115D48 reactions, 115B58 requests, the
  finish reaction and attacks. `test-ai-racers.mjs` checks every computer core.
- **Presentation-only human hooks** (pad rumble, camera shake, fades, HUD cues) are not run for
  computer riders, as in the original.
- **Memory and CPU.** One core (see "One core, six riders"): the race's single wasm memory is
  161 MB (was six memories, 804 MB), node RSS 336 MB (was 1028 MB). One 60 Hz tick of six riders
  costs the same as before (about 4.5-7 ms in node): the per-rider FX pass, pose and physics are the
  original's work. The world entities (set pieces, rollers) still update once per rider, as in the
  multi-core layout; sharing them would remove the replay but changes the per-rider timing of
  entity updates, so it is left for a later step.

## Controller-phase order in 131620 and 12C678 (2026-09-30, c0a-ws13)

- **131620 (control 0's update)** calls 115B58 at 0x131868 and 115D48 at 0x131870, then 312AA0 and the 114CC0 reverse turn at
  0x1318EC. So 115D48 sees the channel-1 class and the stance of before the turn. The port ran its non-held reverse-turn check
  (core.cpp) first, so the turn's clip reset the idle clock +0x35C a tick early (c0a-ws13 Allegra 9368, her reaction draws a tick early
  at 9485). Now core.cpp calls `browser_ground_upper_reactions` ahead of that check and `browser_ground_controller_animation` runs them
  only when that call did not (web/animation_bridge.cpp `ground_upper_reactions`). The held path (12E9B8) is unchanged.
- **12C678 (control 10's update)** runs in the controller pass (121068): its phase-2 115B58 play of 315 / 314 (3128E8 -> 311710's
  variant draw at 0x3117B0, word % sum, the first choice whose running remainder is <= 0) is a controller-phase draw. finish_step
  (web/finish_gameplay.inc) now opens `ControllerDraws`; before, its draw took the motion cursor, which starts after the other riders'
  predicted controller draws, and the human's celebration picked leaf 322 (clip 6400) for the PS2's 321 (clip 6144) (c0a-ws13 12046,
  word 0xD3DB07).

## Route roles by round (2026-09-30, CTM events-in-world agent)

cComputer_updateRiderDifficulty (0x10C758) sets each computer rider's owner +0xE00, the route affinity 10D410's path score reads
(originalNpcRouteAffinity), from 0x10C450(C, slot, GMM+0) through the jump table 0x456A70. GMM+0 is the round: 1 qualifier,
2 semi, 3 final, and 3 in a Single Event (0x23A174 sets it). Slots 1 and 2: 1, 1, 2; slot 3: 0, 1, 1; slot 4: 0, 0, 1; slot 5: 0.
+0xE04 is 1 only for slot 1 in round 3; +0xE08 = 1 for event kinds 0x535C10 = 1 / 5 / 6. Overrides not ported: game mode 0x535C12
4 -> +0xE00 0 and 5 -> 2 (the backcountry rival events), the 0x5308D0 bit 2 branch. lineups.json's slot tables hold round 3's
(their anchor is a Single Event), so a career qualifier or semi rode the final's roles; web/lineup.js npcRoundRole /
assembleLineup({ round }) give each round's (the derived ARA1 qualifier / semi / final countdowns: test-lineups.mjs). Live career
heats take them under pv heatRoles; web/event-heat.js (WS13) always. PS2 c0a-ws13: with the semi's roles Elise takes path 117 at 479
as the PS2 does (the final's role 2 for slots 1 / 2 and 1 for slot 4 changed the riders' path picks and so the traffic term).
Open coverage item: no gated capture runs a career heat through the event-load page path (web/ai-race.js prepare / rosterBuild); every
CTM career capture loads a PS2-derived countdown with --document. The evidence for pv heatRoles is test-lineups.mjs's derived-countdown
check and c0a-ws13's semi through web/event-heat.js. To add: one derived PS2 career qualifier capture scored through the event-load path
(compare-page-capture / test-rival-page style), when an ARMSX2 slot is free.

## Difficulty by race level (2026-09-30, career-rival agent)

cComputer_updateRiderDifficulty (0x10C758) also sets each computer rider's +0xDF8 / +0xDFC (NPC pacing words; document leaves
npc.crouch_parameter_df8, npc.driving_state.parameter_df8 / parameter_dfc) through 0x10C4F8(rider, slot, level):
- level = 0x147CB8(profile, event type == 0): the human's profile character block +0x280 (race level 0..2; +0x284 for freestyle kinds);
- three per-slot jump tables give (DF8, DFC): level 0 0x456AB0, level 1 0x456A90, level 2 0x456AD0 (gp-0x7DD4..-0x7D24 floats); then
  DFC x 0.01 (gp-0x7D20);
- back in 0x10C758: course 4 DFC x 1.25, courses 2 / 3 DFC x 1.1 (gp-0x7D1C), DFC capped at 1.0; the PS2 FPU rounds toward zero;
- not modelled: the options word 0x5308D0 bit 2 (DF8 100, DFC 1.0; 0 in every career state) and the mode 4 / 5 role overrides.

lineups.json's slot tables hold level 1 (every Single Event countdown; a fresh career is level 1 too). The rule reproduces all 128 exported
countdown documents (Single Event level 1; ARA1 career qual / semi / final level 1; CRA3 / DRA4 career finals level 2;
local/career-rival/difficulty.py, web/test-career-rival.mjs). Port: web/lineup.js npcDifficulty, assembleLineup({ level }); web/ai-race.js
heatLevel passes web/career.js's live level.race.level (ev.raceLevel, not saved) for a career race under **pv careerLevel** (off). Without it
a career race at level 0 or 2 rides level 1's pacing.

Fixed 2026-10-01 (found on careerrival/dra4-final, not career-specific): the human's hard crash at 229 is an attacked bail on the PS2
(107888's attack branch -> 107E70 a3 = 1 -> 10EB30 a2 -> 119B08: score +0x12C, HUD popup 0x2D). pair_react now passes the flag; the human
score is exact to the end. The rule, the attacker's KO and the second gate: docs/crash-motion.md "Attacked bails".

## The semi's fresh riders (2026-10-01, career-rival agent)

+0x434 (document leaves ground.state.rider_type and identity.rider_type434) is the rider's location track id: the constructor 11B718
sets +0x430 = -1 and +0x434 = 0x31, and 1218D0 writes 22E0E0(+0x430's track) only while +0x430 != -1 (docs/ctm-events-in-world.md "The
semi's push-off"). A career semi's riders are WS13's new ones, placed by 1289F0 without a hold, so they keep 0x31 to the push-off, where
13C948 halves the auto boost (>= 17). The qualifier's riders (the approach NIS's 1242B0 re-probe) and the final's (the start-hut NIS) hold
the track id, the value of every Single Event table (ARA1 0, CRA3 2, DRA4 3).
- tools/export_lineups.py checks the leaf by this rule for career countdowns (apply_round_level). It used to raise "ground.state.rider_type
  is not determined by all (ARA1-semi-zoe)"; the ARA1 / CRA3 / DRA4 builds now pass and give the live files byte for byte.
- **pv semiFresh** (off): web/lineup.js assembleLineup gives a round-2 rider 0x31. Then the page's assembly of all three Snow Jam career
  countdowns (qual / semi / final) equals the PS2's leaf for leaf (web/test-career-rival.mjs).
- Gate careerrival/ara1-semi (the c0c-race capture on the event-load path, the page's live career heat): with 0x31 the human is exact to the
  end, the RNG to 460 (461: the CTM collectibles, ctm-events/c0c-race-riders), the riders to 530..862; with 0 (today's page) the riders
  leave at the push-off (182..204), the RNG at 296, the human at 310.
