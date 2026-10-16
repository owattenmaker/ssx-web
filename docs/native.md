# Native build

Owner: the native-port agent (2026-10-13). The goal (the user's): native macOS and Windows builds of the game that run the
same simulation core as the browser build, bit for bit. The core is the C++ under web/ and engine/ that web/build-core.sh
compiles to wasm; the native build compiles the same sources with clang for the host and checks every result against the wasm
build's.

State:
- N1 (the build and its parity tests) and N2 (the native replay verifier, section 6) work on macOS arm64.
- N3, the native front end (section 7): the course, sky, fog, tint and rider render in an SDL3 window or headless.
- Windows x64 and Linux: the build is written for them (clang-cl, no POSIX-only calls), but they are not built or tested yet.

## 1. Build

Nothing extracted from the game is in this repository. Make the game data and the generated core sources from your own discs
first, exactly as for the browser build (README "Build the game from your own discs", docs/iso-pipeline.md):

```sh
python3 tools/setup_from_iso.py --iso "SSX 3 (USA).iso" --gamecube "SSX 3 (USA).rvz" --state-pack ssx3-statepack.zip
```

That writes web/public/assets (the game data), web/generated and engine/generated (core sources generated from your disc) and
the wasm core web/runtime, which the parity tests compare against. Then:

```sh
cmake -S . -B build/native-core -G Ninja
cmake --build build/native-core
ctest --test-dir build/native-core --output-on-failure
```

You need CMake 3.21+, Ninja (or another generator), node 22.12+ (the tests run the wasm reference), Python 3 (the replayer's
export table) and clang:

| Host | Compiler | State |
| --- | --- | --- |
| macOS arm64 | Apple clang (Xcode or the command line tools) or LLVM clang | built and tested |
| Windows x64 | llvm-mingw (clang for x86_64-w64-mingw32, UCRT): cross-compiled from macOS, or native on Windows; clang-cl should work too | links (section 8); not run yet |
| Linux x64 / arm64 | clang | not built yet |

GCC and MSVC's cl are refused at configure time: the rider contexts need clang's emulated TLS (section 3).

Configure options:

| Option | Default | What |
| --- | --- | --- |
| `SSX_BUILD_CORE` | ON | the native core (native/) |
| `SSX_BUILD_METAL_PROTOTYPE` | OFF | the older engine/ Metal prototype (docs/native-engine.md) |
| `SSX_EXACT_FPU` | OFF | the console-arithmetic core, `-DSSX_PS2_EXACT_FPU=1` (docs/ps2-float.md) |
| `SSX_CORE_CFLAGS` | empty | extra flags for the core's units, as web/build-core.sh's `SSX_CORE_CFLAGS` |
| `SSX_NATIVE_UBSAN` | OFF | report every float-to-int conversion out of range; the tests fail on one (section 4) |
| `SSX_WASM_CORE` | web/runtime/core.js | the wasm core the parity tests compare against |
| `SSX_ASSETS` | web/public/assets | the game data |
| `SSX_PS2_RUNS` | local/ps2-capture/runs | PS2 captures for the gate-replay test (optional) |

The build type does not change the core's code. The configuration flags are cleared, so there is never an `NDEBUG`, because
the wasm build has none. The core's own flags set the optimisation.

## 2. Targets

| Target | What |
| --- | --- |
| `ssx3_core` | static library: the core's units, native/core/rider_context_native.cpp, native/core/host_hooks.cpp |
| `ssx3_race_driver` | headless Snow Jam race: the human alone or with the five computer riders, a scripted pad, an optional pause Give Up; prints each tick's ?simtrace hash |
| `ssx3_replay_calls` | replays a logged wasm export-call stream and checks every probe (the human's state, pose, shared and visual RNG) |
| `ssx3_verify_replay` | the native replay verifier (section 6) |

**The source list** is read from web/build-core.sh's em++ line when CMake configures (native/cmake/core_sources.cmake). A unit
added there is part of the next native build, with no second list to keep in step. Left out:
- web/rider_context.cpp and web/rider_tls.S, the wasm `__tls_base` swap;
- the `SSX_SNAPSHOT_REGISTRY` define, because the rider-context snapshot's functions live in web/rider_context.cpp and are not
  native yet.

**The replayer's export table** is generated at build time by native/gen_dispatch.py from build-core.sh's
`EXPORTED_FUNCTIONS`, using the signatures in the sources. One replayer serves any logged stream.

## 3. How the native core stays bit-identical

- **The same arithmetic flags as the wasm build:**
  - `-ffp-contract=off` (no FMA: arm64 and x86-64 have it, wasm does not);
  - `-fno-fast-math`;
  - `-fsigned-char`, as in wasm and on Apple arm64 (Linux aarch64 defaults to unsigned).
  The optimisation level does not change results under these flags; the tests check it.
- **The emscripten names.** native/core/shim/emscripten/emscripten.h maps EMSCRIPTEN_KEEPALIVE to `used`, and an EM_JS hook
  becomes an extern C function. native/core/host_hooks.cpp defines those functions with the page's answers for a host that
  uses the core's rider manager.
- **Rider contexts** (RIDER_LOCAL is thread_local).
  - The wasm build switches a rider by storing `__tls_base`. Natively the core's units are compiled with `-femulated-tls`, so
    every thread-local access calls `__emutls_get_address`.
  - native/core/rider_context_native.cpp defines that call. Each variable gets one offset in a 4 MB context block (the same
    offset in every block), and switching the context is one pointer store.
  - A new context is a copy of the pristine image with the units' static initialisers run in it, as web/rider_context.cpp
    does.
  - One thread: contexts are switched, never run at the same time.
  - MSVC's cl has no emulated TLS, so on Windows the compiler is clang-cl.
- **The WebAssembly floating-point environment** (2026-10-13).
  - native_shim.h makes fesetround inert and fegetround always nearest, as musl's wasm fenv is, and defines SSX_NATIVE_CORE.
  - With it, the 9 engine/ files that branch on `__EMSCRIPTEN__` take their wasm branches natively too (the software toward-zero
    arithmetic of engine/original_rounding.hpp), and the hardware rounding mode never changes.
  - The two builds therefore run the same arithmetic by construction, not only where the tests reach. A plain float operation
    inside a rounding scope, or a direct fesetround (engine/reset_fade.hpp), would otherwise round toward zero natively and to
    nearest in wasm.
  - The wasm output is byte-identical with the change: CORE_OUT builds of the tree before and after.
- **The parity tests (section 5)** check every build against the wasm build.

## 4. Platform hazards (what can still differ)

- **Float-to-int conversions of NaN or of values out of range.** wasm saturates (NaN gives 0); arm64 saturates the same way;
  x86-64 gives INT_MIN. A Mac build cannot show this, so run the parity tests with `-DSSX_NATIVE_UBSAN=ON`: every such
  conversion is reported, and the test fails and lists the sites. 2026-10-13: none in the four tests.
- **libm.** Bit-identical across platforms only for sqrt and fmod. The core's other transcendental functions are its own EE
  polynomials (engine/collision_scalar.hpp atan, engine/original_camera.hpp asin). The exception is web/replay_camera.inc
  (std::sin / cos / atan2), which is the replay view: drawn, never simulated. If it ever has to match the web build bit for
  bit, the native build should take musl's sinf / cosf / atan2f (MIT, the functions emscripten uses).
- **Uninitialised reads** differ between emscripten's zeroed heap and native malloc. No sanitizer finds them on macOS; only the
  parity tests do.
- **Pointer and long sizes.** wasm32 has 32-bit pointers, size_t and long. Natively pointers and size_t are 64-bit; long is
  64-bit on macOS / Linux and 32-bit on Windows. The core keeps its state in fixed-width types, and the tests cover what they
  run.

## 5. Tests

`ctest --test-dir build/native-core`. native/tests/parity.mjs runs each test. A test exits 77 (skipped) when the game data, the
wasm core or the PS2 capture is missing, so a build without the private captures stays green.

| Test | What |
| --- | --- |
| core_race_solo | Snow Jam, Zoe alone, 3000 ticks of the scripted pad: every tick's hash against the wasm build's (native/tools/ref-trace.mjs, the page's game-tick sequence) |
| core_race_ai | the same with the course's five computer riders (the core's rider manager, six rider contexts) |
| core_race_give_up | the six-rider race with a pause Give Up at 900, through the results (1309 ticks) |
| core_gate_event_race | the event-race gate (web/compare-ps2-capture.mjs on the wasm core, logged by native/tools/spy-core.js): 55,500 export calls replayed natively, all 2418 probes identical |
| core_verify_replays | the native replay verifier against the browser verifier on every fixture: verdict, value and every replayed tick's ?simtrace hash (section 6) |
| frontend_pad_input | native/frontend/pad_input.hpp against web/pad-input.js: 4800 random frames (Classic, Simple, standard pads) bit for bit; no game data needed |

**The wasm results are cached** in the build folder (parity-cache/), keyed by the wasm core's hash and the script. They are
game-derived data: they stay in the build folder, never in the repository.

**The reference must match the sources.** The comparison means something only when the wasm core was built from the same
sources as the native build. web/runtime is the deployed core, which can lag the tree while a migration slice is in progress.
To compare against the tree:

```sh
CORE_OUT=local/native-ref sh web/build-core.sh
cmake build/native-core -DSSX_WASM_CORE=$PWD/local/native-ref/core.js
ctest --test-dir build/native-core
```

Note: a CORE_OUT build still regenerates web/generated/snapshot in the shared tree (AGENTS.md).

Measured (2026-10-13, M-series Mac under load; the wasm core under node):

| Run | Native | wasm |
| --- | --- | --- |
| solo race, 3000 ticks | 1.0 s | |
| six riders, 3000 ticks | 3.7 s | 6.0 s |
| event-race gate replay | 1.5 s (with JSON parsing of the 39 MB stream) | 6.5 s (the comparer run) |

## 6. N2: the native replay verifier

`ssx3_verify_replay ASSETS_DIR REPLAY.ssxr [--trace OUT.json]` takes a portable run replay (web/server/replay-file.mjs, raw SSXR) and
re-simulates it headless the way the page's ?verify path does. It prints the browser verifier's result as one JSON line:
`{ok, reason, value, finishTick, ms}`. Exit codes: 0 ran, 2 not runnable here (`ok: null`), 1 a simulation error.

**What it mirrors** (native/verify/verify_replay.cpp; each step names its page code):
- **web/online-replay.js begin / load / play / verifyRun.** The rider and course from riders.json / courses.json.
- **main.js loadCourse:**
  - init_world;
  - the set pieces (sections, the stage world, avalanches, flags);
  - the boot rider, terrain, world collision, body terrain, fog, weather, rider lighting, rails.
- **web/ai-race.js createAiRace / web/ai-racers.js createAiRacers.** Rider contexts, attachWorld, configureRider, the rider host
  flags, manager_configure, the anchor words.
- **The rider.**
  - web/character-roster.js loadCharacter / composeCheat / humanSettings, with the uber rows and the pair inputs.
  - main.js initializeRiderAnimation.
- **web/ai-race.js prepareFixed.** The replay's lineup: web/lineup.js assembleLineup with the replay's relationship tables,
  humanGridState, install / setDocument (a slot is reconfigured only when the page's JSON string comparison says it changed).
- **The attribute bytes.**
- **main.js applyPresentationFast's four flags** in every context (stage_teleport_enable and set_fx_reset_kicker change the
  simulation).
- **main.js startRun(R):**
  - the time limit and web/freestyle-event.js set_race_bonus;
  - the resets, the input map, the placement, the camera view, web/stage-collect.js collectApply, start_event;
  - the computer riders' start: the core's manager_event_start, which M4b proved identical to the page's JS start;
  - syncWorldNodes, the relationships;
  - the RNG words.
- **The replay camera.**
- **web/game-tick.js simulateInCore every tick:** manager_begin_tick (or soloTickStart's anchor RNG), tick_begin, manager_end_tick
  or tick_mid, tick_end, tick_finish, tick_results. Replay events (giveUp, camera) apply before their tick.

The first finish decides, as verifyRun does.

**JS semantics kept:**
- the documents are ordered JSON (key order as JS objects keep it, integer-like keys first), so the page's string comparisons
  come out the same;
- numbers are written as JSON.stringify writes them (no -0, no NaN, integral values as integers);
- web/ai-racers.js mergeSettings is the one-level spread.

**Not handled yet** (reported as `ok: null`, never a verdict):
- wardrobe outfits (`w1:`, `sam:`);
- in-world replays;
- a race lineup without its relationship tables (older files).

Handled since the first version: backcountry rival events (prepareRival, rivalAnchorWords, event_kind), a Single Event slope-style
opponent (prepareFreestyle on a fresh page's tables), and cheat riders (composeCheat).

**Proof (2026-10-13, core8):** 15 fixtures, each identical to the browser verifier (native/tools/browser-verify.mjs: the game's
?verify page in headless Chrome with ?simtrace=1) in verdict, value and every replayed tick's hash:

| Fixture | What | Verdict | Ticks |
| --- | --- | --- | --- |
| sj-ai | Snow Jam, Zoe, 5 computer riders | reproduced, 14054 | 14234 |
| sj-mac | Snow Jam, Mac (settings.json), attributes 55, the page's roster | reproduced, 14891 | 15071 |
| sj-solo | Snow Jam, the solo time-attack lineup | reproduced, 15019 | 15199 |
| sj-camera | sj-ai with two camera-view events | reproduced | 14234 |
| aba1-score | Crow's Nest big air, the score claim (soloTickStart's anchor RNG) | reproduced, 2540 | 2815 |
| aba1-higher | the same, a higher claim | score 2540, claimed 3040 | 2815 |
| fake-faster | sj-ai, a faster claim | time 14054, claimed 13904 | 14234 |
| fake-nopad | sj-ai's meta, no pad | finished at tick 13198 | 13199 |
| bra2-ai-nofinish | Metro City, Mac, 5 computer riders, the TAS pad on this core | no finish in 8687 ticks | 8687 |
| tas-8507 / tas-8964 / tas-8964b | local/tas/page replays recorded on older cores | no finish | 8687 / 9144 / 9144 |
| sj-brodi | Snow Jam, Brodi (a cheat skin on Zoe), 5 computer riders | reproduced, 13742 | 13922 |
| abc1-rival-time | Happiness Rival Time, Mac the rival (rivals.json, no countdown) | reproduced, 14637 | 14637 |
| ass1-slope-opponent | R&B slope style with its computer opponent (score claim) | reproduced, 9290 | 10871 |

The Rival Points run (Happiness Jam) matches the live page trace on all 15,312 ticks too. It is not a fixture: its score was 0, and the
records server refuses a 0 claim.

**A page finding: a Single Event slope style's replay is not its live run.** On the ass1-slope-opponent run, from tick 8922 (the
computer opponent's finish) the live page's human boost meter (boost_info[0]) is 0 and its score object differs. In the browser
verifier's re-run (the page's replay path) the meter carries on (0.3747...); the native verifier is identical to that re-run.
The verdict is the same either way. The cause is in the page's live path (the career UI's Single Event is active live, not in the
replay), not found yet. Found 2026-10-13, reported to the coordinator.

**Fixtures** are game-derived data, so they live in local/native-port/fixtures (`-DSSX_VERIFY_FIXTURES=`). Each has three files:
`<name>.ssxr`, `<name>.verdict.json` and `<name>.trace.json`. The verdict and trace files are browser-verify.mjs output.
`--dump OUT.json` writes tools/tas/page-run.mjs's dump.json rows (per-tick states), for finding a divergence. To make a fixture:
1. Run `node tools/tas/page-run.mjs PAD.tas DIR [--course C --rider R --lineup J]`: a live page run, its replay.ssxr and trace.
   For a run with no finish, use `node native/tools/tas-replay.mjs PAD.tas DIR OUT.ssxr` instead.
2. Run `node native/tools/browser-verify.mjs --traces DIR FILE.ssxr...`.

**Cost.** Six riders, 14,234 ticks:
- native: 10.6 s wall, 210 MB peak RSS;
- the browser verifier in headless Chrome: 9.7 s simulation, 22 s wall under `nice -n 19`, plus Chrome's page load and GPU process.

A solo or big-air run is 0.5-3.5 s natively, with nothing to start. Before the WebAssembly floating-point environment (section 3),
fesetround took about 30 % of the native time; without it, and with -O3, Snow Jam with five riders took 8.4 s in a scratch build.

**LTO breaks exactness natively.** `-O3 -flto` diverges, even on a solo run, and fails on six riders with "Race tick already open".
-O3 alone stays exact. A full `-fsanitize=undefined` run reports nothing. Likely cause: LLVM treats a thread-local address
(`llvm.threadlocal.address`, here emulated TLS) as fixed within a function, and cross-unit inlining across a context switch
breaks that (the hazard docs/core-migration.md M3 notes). Not investigated further: native builds do not use LTO.

## 7. N3: the native front end

**State (2026-10-14):** the course world, sky, fog composite, ScreenTint, the skinned, lit riders and the race HUD render from the
exported assets, and the game audio plays through SDL3. The core ticks at 60 Hz with the page's pacing. It runs in an SDL3 window (macOS) or headless. At the same tick and camera
a Snow Jam frame is within a mean 4.8 / 255 of the page's (below).

**Build:** `-DSSX_BUILD_FRONTEND=ON`.
- **wgpu-native** v29.0.1.1: native/cmake/wgpu_native.cmake fetches the official release zip at configure time, pinned by
  SHA-256 per platform. Nothing is vendored.
- **SDL3** from the system (Homebrew `sdl3`; zlib licence) for the window. Without SDL3 only the headless tools build.

**Targets:**
- `ssx3`, the windowed front end (native/frontend/app.cpp):
  - `ssx3 ASSETS [--course ARA1 --rider zoe]` is a live time trial: the solo lineup, the page's live startRun(null).
  - `--start RUN.ssxr` plays a replay's start live; `--watch RUN.ssxr` plays its recorded pad.
  - `--shot OUT.png --shot-tick N` writes a frame and quits; `--hidden` opens no window on screen; `--at X,Y` places the window.
  - `--quality low|medium|high` is the page's tier (default `high`, a desktop's; frontend/quality.hpp ports web/quality.js
    TIER_DEFAULTS): it sets the rider texture set and the core's presentationFast. `--riders ps2|xbox` and `--bc 0|1` are the
    page's `?riders=` and `?bc=`.
  - The game audio (frontend/game_audio.hpp, below) plays through an SDL3 audio stream. `--mute` opens no device, and `--hidden`
    always mutes, so automated runs never make a sound: a silent device thread then renders the graph at the wall-clock rate, as a
    device would, so the voices' ends and the music's pumps keep their timing.
  - `--audio-log OUT.json`, written with the shot, holds the director timeline, the sfx requests with their ticks, the speech posts
    and each tick's listener. A shot run with it steps one tick per 60 Hz frame, as browser-shot `--paced` steps the page.
  - The event load runs under the load screen (frontend/loading_screen.hpp): the course, the rider, the audio's world banks, the
    lineup, the world's GPU resources, the effects and the HUD load on a loader thread while the main thread draws the screen at
    60 Hz. Its stages feed the meter (`course`, `rider`, `lineup`, `warm`). When the load is done the screen shows 100%, holds 12 frames
    and fades out, then the race starts (the audio's world load and run start). `--load-shot OUT.png` writes the screen's 120th
    frame.
  - The window is not focusable and the app does not activate (no Dock icon) unless `--focus`, so a run never takes the user's
    keyboard.
  - A Single Event race ends in its results (frontend/single_event.hpp, below): on the core's results tick (tick_results' show,
    finish + 408) a new top-5 time opens "Top 5 Record Times" (61toptimes), else "Single Event Results" (43final_standings). The
    menus read the keys and the pad as the page's do (frontend/menu_input.hpp). Freestyle events still end at the live stop.
  - The auto replay plays behind them, as on the page (web/replay.js 'auto', main.js replayFrame). The live run keeps its start
    snapshot (liveStart: the run's snapshot fields and the start words of the game RNG, the visual stream and gp+0xA0C) and every
    tick's pad up to the finish. When the results are up, the live ticks stop and the run starts again from the snapshot on the same
    cores (Verifier::replayRestart = main.js startRun(R); GameAudio::replayRewind). It steps the recorded pad on its own clock
    (min(dt, 0.25)) and draws through the replay camera: the course's camera triggers, replay_camera_begin(0x5D) and tick_end's
    [10..19] pose, no blend across a cut. Past the finish tick it rewinds and loops.
  - Results menu: Next event and Quit leave the app for now (no event select or title yet). Replay is greyed, as the page greys it
    without the full replay. Restart opens 'Restart / Are you sure?' (87yndialog over the PDA frame, frontend/ctm_pda.hpp, with the
    20-frame exit before the choice acts). Yes starts the round again as the page does without cutscenes (play(): startRun as a live
    start, the replay stopped). The page's gondola ride (world state 13) and the round card come with the cutscenes.
    **Paused 2026-10-14 before this was built:** the Restart prompt and the restart are written (app.cpp restartRun,
    single_event.hpp Screen::Restart, ctm_pda.hpp) and app.cpp compiled, but the link stopped on another agent's in-progress
    web/net_world.inc (std::set without <set>). The Restart path has not been run or checked against the page yet; see the resume
    point below.
  - The records table is saved in the user's app data folder (SDL_GetPrefPath "ssx3/native", save.json: `{version, records}` as
    the page's save keeps them). `--save PATH` names another file. Hidden and shot runs keep no save unless `--save` is given.
  - QA:
    - `--results-log OUT.json` writes the standings and the outcome when the results open (`.1`, `.2`... for the runs after a
      Restart).
    - With the shot, `--canvas-log OUT.json` writes that frame's UI canvas calls and `--ui-clock F` fixes the panels' timeline frame
      (the PDA opened and its popup set at 0).
    - `--shot-replay N` shoots the frame after the auto replay's tick N; `--shot-run K` shoots in the run after K Restarts.
    - `--menu-keys K1,K2,...` sends menu keys, each once the screen takes input.
  - Keys: web/pad-input.js Classic; gamepads through SDL's standard mapping; Escape quits during the race and is Triangle (back)
    on the results.
- `ssx3_render_shot ASSETS RUN.ssxr OUT.png --tick N`: headless, no window.
  - The set pieces step to the core's clock every 400 ticks, as the page's frames between browser-shot's steps do (one step is at
    most 3600 ticks, so a single late frame would skip the sections). The refined terrain is committed in the shot's frame. The print
    also gives the sun painter's values and the sun query's visibility.
  - `--quality` defaults to `low` here, as browser-shot loads the page (`?quality=low`: the PS2 rider textures, presentationFast on);
    `--riders` and `--bc` as in `ssx3`. The printed line names the tier, the rider set and whether BC blocks were uploaded.
  - `--hud` draws the race HUD over the frame; `--canvas-log OUT.json` also writes that frame's UI canvas calls.
  - It draws the frame the page draws right after `ssxQA.advance` stepped N + 2 ticks (render alpha 0): the camera and rider
    palette of tick N over the world state of tick N + 1.
  - It prints the ?simtrace hash of tick N, so the page and native states can be checked equal before the pictures are compared.
- `ssx3_ui_shot ASSETS OUT.png --screen loading [--frame F] [--percent P] [--black B] [--hint-counter N] [--device keyboard|gamepad]
  [--keyboard Classic|Simple] [--canvas-log OUT.json]`: a front-end screen at a fixed state on the UI canvas over black.
  native/tools/ui-shot.mjs draws the page's at the same state (its update() fixed, the hint counter and keyboard mode in storage) and
  records the same canvas log. `--screen single [--ticks T] [--rider R] [--keys K1,K2,...]` is a Snow Jam race's results for fixed
  QA standings (the human finishing at T ticks and five computer riders) on fresh records, then the menu keys once the screen takes
  input. The page side goes through careerUI.finish with `?pv=-onlineRecords`.
- `ssx3_image_compare WEB.png NATIVE.png OUT.png`: web | native | difference x 4 side by side, plus the mean / 95th percentile.
- native/tools/browser-shot.mjs: the page's frame at the same tick.
  - It plays the run's TAS pad in a Single Event (as tools/tas/page-run.mjs does) in headless Chrome, muted, widescreen Off, with
    the HUD hidden.
  - It prints the page's ?simtrace hash of tick N and the page's texture decode config.
  - `--riders ps2|xbox` and `--bc 0|1` pass `?riders=` and `?bc=` to the page.
  - `--hud` keeps the race HUD; `--canvas-log OUT.json` records the #ui canvas calls from one ui.draw to the next (op, arguments in
    the 9-argument drawImage form, transform, global alpha, and the image: a PNG's name or the FNV-1a of a canvas's getImageData).
  - `--chunk K` steps K ticks per page call (default 400). `--paced` awaits an animation frame after every tick, so the page's
    frame work runs between ticks as in live play: GO, the audio listener and the effects' frame history.
  - `--audio-log OUT.json` loads the page with `?perf=1`, unlocks its audio (the title's Press START does that) and writes the
    same log as `ssx3 --audio-log`. Use it with `--paced` and one chunk (`--chunk` at least the tick), because a gap between page
    calls runs extra frames without ticks.
  - `--pv LIST` passes `?pv=` (`-onlineRecords`: the PS2's local records table, as native keeps). `--ui-clock F` fixes the results
    panels' timeline frame (and opens the PDA and its popup at 0). `--no-replay` greys Replay, as native has no full replay yet.
  - `--replay-ticks R` steps the auto replay behind the results R ticks (window.__replay.step) after tick N: ssx3 `--shot-replay
    R - 1` draws the same frame. `--keys K1,K2,...` sends menu keys after tick N, each once the screen takes input (the frame clock
    runs from then on, so the UI frame counter steps).
  - Past the pad's last frame the run holds that frame, as a replay's pad stream (and the native reader) keeps its last values. So
    a run can go on to its results.
- native/tools/compare-canvas-log.mjs PAGE.json NATIVE.json: the two logs compared entry by entry, numbers exactly.

**What is ported** (each file names its web source):

| Native | Web | What |
| --- | --- | --- |
| frontend/world_renderer.cpp | main.js asset(), world-material.js, fog-renderer.js | the packages (40-byte vertices, colours, indices, vertex alpha, the PS2 light UVs per patch), the terrain combine (Cd - Cl) x Al >> 7, the static-model modulate with blend classes 0 / 1 (pass + fringe) / 2 / 3 (additive), three.js's render lists (opaque by renderOrder, the projected bounding-sphere depth near first, id; transparent far first; every DoubleSide transparent back face, riders' included, before the front faces), one object record per moved / copied batch, the sky pass as the world pass's background, the CLUT fog on the world depth (three.js perspectiveDepthToViewZ), texture chunks hidden while not streamed (section_chunks) |
| frontend/screen_tint.hpp | screen-tint.js | the quadtree payload, the distance blend in fround steps, the lightning flash |
| frontend/rider_renderer.cpp, rider_capture.hpp | rider-skinning.js, rider-lighting-nodes.js, rider-material.js | the core's skin palette pair (previous / current), VU chop add / mul in WGSL, the per-vertex irradiance of the core's ten rows, GS HIGHLIGHT2 on PS2-domain texels, riderDrawState's two blended passes for 'alph' / 'ea*' |
| frontend/pacing.hpp | tick-lock.js, fixed-step-clock.js, ps2-frame-pacing.js | one tick per 60 Hz frame (debt paid with 0 / 2), else the fixed clock with the render alpha |
| frontend/pad_input.hpp | pad-input.js | the 24 channels, bit for bit (ctest frontend_pad_input) |
| frontend/texture_archive.cpp | texture-archive.js | SSXTEX01, exact RGBA8 (stb_image), mips as a 2 x 2 box |
| frontend/race_hud.hpp, trick_hud.hpp, hud_widgets.hpp, hud_canvas.cpp | ui.js draw ('game'), trick-hud.js, boost-gauge.js, boost-orb.js, boost-letters.js, race-place-hud.js, progress-meter-hud.js, input-glyphs.js (key caps), game-tick.js (readTrickHudSlots, the HUD pre-pass), ai-race.js (the place timers), main.js (ui.draw's state) | the race HUD on the 640 x 448 UI canvas: the place with its change and 1st-place glow timers, the race clock, the speed, the progress meter (EE float), the boost orb, coil glows, gauge and letters, the switch icon, the countdown, the trick slots (every slot case, the Uber hint, the recover meter, the big GO! / MISSION SUCCESS message), the finish / time-up banner and the reset fade; the page's tinted canvas copies made the way Chrome makes them (multiply / destination-in, source-in, ImageData with its float32 unpremultiply); Canvas 2D drawImage as Chrome rasterizes it (bilinear, strict source rect, edge coverage, float transform arguments composed in double); the canvas stretched over the 3D frame as the stage stacks #ui over #game. The key caps' rounded rects and arrows are rasterized at 4 x 4 samples a pixel |
| frontend/bc_texels.hpp, rider_renderer.cpp, quality.hpp | bc-texels.js, texture-archive.js, rider-material.js, quality.js | the Xbox HD rider set (the desktop default, tier medium / high): a WARDROBE textures.tex / gear.tex read from its `-xbox` twin (the PS2 archive when the twin is missing), its 'SXBC' entries uploaded as BC1 / BC2 with bcMipChain's levels where the device has texture-compression-bc (else decodeBC's texels), the entry's own texel domain ('xbox': scale 127.5). The decode and every re-encoded mip level are bit-identical to bc-texels.js on 938 entries of five archives (JS Math.round and V8 / JavaScriptCore's Math.hypot ported; the render library builds with -ffp-contract=off) |
| frontend/uv_scroll.hpp | uv-scroll.js, set-pieces-renderer.js | the scroller 0x35F7D0 in the EE scalar arithmetic (chop MUL / ADD / SUB with the guard bit, DIV.S nearest), the scroll groups by first appearance, the set-piece clock (set_piece_info, the slot-1 section enters, the ready state's phases); drawn as the first instance indexing a storage buffer |
| frontend/set_pieces.hpp, livecomp.hpp, flags.hpp, ee_float.hpp | main.js frame, moving-instances.js, set-pieces-renderer.js, livecomp-animation.js, attached-setpieces.js, flag-animation.js, set-piece-cables.js, ee-scalar-float.js | per world batch this frame: hidden (pickups, event-dead from the event start, stage-program draw states), shown (the hidden batches of flag static draws, MeshAnim pieces, LiveComp 'object' players) and moved (moving instances and their clones, LiveComp node deltas, teeters, attached children, magnets, break pieces); the LiveComp players and the flag cloth stepped one game tick at a time to the core's set-piece clock (the go tick, the section log, the core's trigger / timer starts, section clocks, flag words), from the ready state; the cables as screen-space lines (guard band, near / far), alive by CableLife and the owner's chunk |
| verify/board_flex.hpp, rider_renderer.cpp morphed | board-flex.js, rider-skinning.js createBoardFlex | pv boardFlex: the board's, hands' and SpecialA's morph parts loaded from the package, configured in the core after the human's init_animation (replay_session.hpp, as main.js) and at a computer rider's first capture; each palette's tick's 27 weights; sum(w x delta) added to the source position before skinning (the shadow silhouette too) |
| frontend/rider_shadow.hpp, world_renderer.cpp terrain, rider_renderer.cpp drawShadow | rider-shadow.js, rider-skinning.js shadowPose, world-material.js riderShadowReceiver / glintBytes | the rider shadows: the six-bone box fit (374D00, float32), the nearest six riders in view, each silhouette (the drawn palettes, the parts' painter's rank, the board 0 while grounded) into its slot of the 768 x 128 atlas, the terrain's max(Cd - T, 0) in the 30 m slab; and the terrain glint pass 38D168 (terrain-glint.json) before it |
| frontend/set_piece_fx.hpp, particles.hpp, terrain_sparkle.hpp, avalanche.hpp, startfire.hpp | set-piece-particles.js, set-piece-particle-eval.js, fog-puffs.js, set-piece-halos.js, crowd-2d.js, terrain-sparkle.js, avalanche-state.js, avalanche-trails.js, startfire-renderer.js, impact-fx-renderer.js | the stage programs' particle sprites (the VU1 burst / trail kernels: accumulators in the VU / EE operations, the sprite arithmetic in doubles like the page), the fog puffs (0x22A270 box cull, 0x2DC190 fades, far-to-near), the HaloModifier sprites, the 2D crowd's texture frames and camera flashes, the terrain sparkle (program 4's RNG chain, the twinkle rotation, the Surface painter's density) at renderOrder 640, the avalanche pieces' visibility and trails, the impact sprites (sparks with the per-slot fade, glints, grind chunks, the fist sparkle) and the start-gate fountains, each in three's order in its pass |
| frontend/rider_fx.cpp, draw_order.hpp | board-trail.js, wake-renderer.js, snow-renderer.js, snow-billboard.js, boost-renderer.js, opponent-fx.js, snow-composite.js, ps2-draw-order.js | the trail ribbon / roof in five stencil passes after the world's opaque draws (renderOrder 500 .. 504); the encoded pass: the fogged world bytes in their own target, the wake, the ten snow emitters (three's lookAt quaternion, Matrix4.compose, the 128-pixel extent cap, the flipbook frame), the boost ribbons, aura faces and streamers (strm rows flipped), drawn with the world depth in three's transparent order (the 0x364240 key, then each geometry's bounding-sphere depth as three first computed it, then creation order); every computer rider's from its own core, culled by its 100 m sphere (pv fxCull) |
| frontend/loading_screen.hpp, hud_canvas.cpp fillPolygon / rotated drawImage, hud_widgets.hpp multiplyRegion | loading-screen.js, load-meter.js | the event load screen: GL.LUI 110ctrl_load from LOADING/loading.json (frame-0 properties, timeline events, linear animation tracks, layers), the sprites (tinted copies of a page region: bilinear inside the source rect, multiply, destination-in), the gouraud quads as solid or two-stop linear-gradient fills, the rotated snowflakes, the LUI text (FEFONT x 0.79), the keyboard panel (Classic / Simple rows, key caps) or the DualShock picture, the load hint (0x245950's rotation), the percentage of the load meter (work stages by their typical time, creeping between reports, never backwards) and the fades. ssx3_ui_shot against ui-shot.mjs: every canvas call equal (748, 442 and 656 calls at three states); pixels within 0.28 to 0.67 mean |
| frontend/career_data.hpp, single_event.hpp, menu_input.hpp | career.js (raceResult, heatRecord, addRecord, completeEvent, records), career-ui.js (finish, drawResults, drawRecords, resultItems, items / disabled / key / choose / back of ctm-records and ctm-results), locale.js, race-time.mjs, menu-rules.js, screen-phases.js, gamepad-menus.js | a Single Event race's results: the times, ranks and medal through the core's result kernel (engine/career_rules.hpp), the top-5 entry and the saved table (the PS2's local one: the page's pv onlineRecords boards are not in native), the two screens' rows, messages and menus (wrapping, skipping greyed items, the 31-frame intro lock, Single Event focus on Restart) and the menus' pad history and key repeat. sj-ai to its results tick: the page's outcome, screen and focus, 290 canvas calls equal but one; ssx3_ui_shot `--screen single` against ui-shot.mjs in 6 states with key presses: canvas calls equal but one (the cut canvas's ±1) |
| app.cpp RunReplay, verify/replay_session.hpp replayRestart, race_presenter.hpp replayView, game_audio replayRewind | replay.js (liveStart, record, finish, start('auto'), rewind, frame), main.js (createRaceReplay host, replayFrame, startRun(R)), game-tick.js (the replay camera), game-audio.js replayRewind | the auto replay behind the results: the start snapshot, the recorded pad, the restart on the same cores, the replay camera and its cuts, the loop. sj-ai's replay ticks 300 / 3000 / 9000 behind the results against browser-shot `--replay-ticks`: 0.38 / 0.74 / 0.41 mean (local/native-port/shots/fe-sj-ai-replay*) |
| frontend/ctm_pda.hpp | ctm-pda.js (frame, opening, dialog) | the PDA frame (PDATemplate's layers from 11, over the world) and the Yes / No popup 87yndialog: the results' Restart prompt |
| frontend/terrain_refinement.hpp, world_renderer.cpp terrainCommit | terrain-refinement.js, terrain-detail.js, terrain-mesh.js, terrain-worker-core.js, terrain-overlays.js | the near-rider terrain: every 0.1 s of the game clock (or a new contact patch) the plan of up to 8 patches within 30 m (2 m hysteresis, the contact patch first) at 32 x 32 with their neighbours' edges matched, each patch's bicubic mesh from its polynomial (JS's evaluation order, Math.hypot), committed on the frame after (the page's worker reply): the coarse triangles of the patches that change degenerated or restored in the course's index buffer, the refined meshes drawn with their patch's material and light-page map |
| frontend/audio_decode.hpp | audio-decode.js | EA .pt headers, EA-XA, PS-ADPCM, PCM8, MicroTalk (UTK) streams and banks |
| frontend/webaudio.hpp/.cpp | (Chromium's Web Audio) | AudioContext in 128-frame quanta, AudioParam automation (cancelAndHold, event pruning), gain, buffer source (linear interpolation, loops, sub-sample starts), equal-power panner, lowpass biquad, the destination; one recursive lock between the game and audio threads |
| frontend/audio_engine.hpp | audio-engine.js | the buses, MIX.INF mixes, duck / release, sliders, FE / WORLD scale modes, the master gain and declick |
| frontend/audio_sfx.hpp/.cpp, audio_sfx_game.hpp | sfx.js, sfx-game.js | banks, layers, envelopes, LFOs, the listener's spatial gain and pan, voice pools and stealing, delayed requests, fades, pause; the game's sounds from the cores' audio events, board loops, landings, grunts, the HUD sounds, big air, the countdown and GO |
| frontend/audio_pathfinder.hpp/.cpp | pathfinder.js | the .mpf song graphs: the timed core (voices, commits, branch picks, loops, events, beat-synced slices) and the player (look-ahead, reconcile, levels, ramps, pause, fade, the lowpass, the loop overlay) |
| frontend/audio_speech.hpp, audio_speech_events.hpp | audio-speech.js, audio-speech-events.js | the EA speech events, the request table and scheduler, the one voice of lines back to back, the DJ duck |
| frontend/audio_crowd.hpp, audio_world.hpp, audio_painters.hpp | audio-crowd.js, audio-world.js, audio-painters.js, avalanche-state.js | the crowd .eam sequences, the world emitters (named banks, contacts, script sounds, the avalanche rumble, thunder), the Mix / MusicTrigger / Ambience painters |
| frontend/game_audio.hpp/.cpp | game-audio.js, main.js (gameTick, frame audio, listener) | the director for Single Events: song choice and radio modes, the loading loop, the world load, the venue intro, GO, the finish, the DJ timers and OnIdle flags, the relationship records for the pass and hit speech, the per-frame listener (the camera the frame read, before its render moves it) and the per-tick game tick |
| frontend/now_playing.hpp | now-playing.js, audio-menu.js drawHud | the EA RADIO BIG popup: the lines' layout, the slide and fade, drawn over the HUD (ctest frontend_now_playing: every playlist song's popup tick for tick equal) |
| frontend/sun_flare.hpp, light_glow.hpp, post_effects.cpp, painter_tree.hpp | sun-flare.js, light-glow.js, fog-renderer.js | the Sun painter per camera tick; the 16 x 16 sun depth query and the glow plus nine lens sprites (2F4DB8 / 2F4A08 / 2F4690); each light's sprite quad and query record (2E2B00 / 2E2868 / 2EC478), drawn additively (copies) into the glow target; the composite adds glow, then sun, after the fog and before ScreenTint |

**Visual parity** (local/native-port/shots: web | native | difference x 4; the ?simtrace hashes of the tick are equal in every shot):

| Shot | Run, tick | Mean / p95 | Notes |
| --- | --- | --- | --- |
| sj-ai-900-shadow | Snow Jam, Zoe, 5 computer riders, 900 | 1.76 / 8 | the rider shadow on the pipe wall; 1.81 before it, 23.3 / 135 at first light |
| sj-solo-2700-shadow | Snow Jam, Zoe, solo, 2700 | 1.10 / 4 | the shadow on the run-out; 1.25 before it |
| sj-solo-190-flex | Snow Jam, Zoe, solo, 190 | 1.88 / 8 | the board's flex (pv boardFlex) matches; 2.06 with the rest shape, the crowd and gate doors as before |
| sj-solo-300-effects | the same run, 300 | 2.00 / 7 | gondolas, cable, banners, glows, sun |
| sj-solo-1200-fx | the same run, 1200 | 2.47 / 9 | 4.8 / 21 at first light |
| sj-solo-2700-fx | the same run, 2700 | 1.25 / 5 | the spray and both air streamers |
| aba1-600 | Crow's Nest big air, Zoe, 600 | 1.21 / 4 | a second course (native/tools/replay-tas.mjs: the replay's pad as a TAS for browser-shot) |
| sj-solo-190-xbox | Snow Jam, Zoe, solo, 190, `--riders xbox` both | 1.87 / 8 | the Xbox HD set, BC on both; on the rider's pixels 5.26 (5.37 with the PS2 set on both, 6.60 with the sets crossed); `--bc 0` on both: 5.16 |
| hud-sj-solo-300 | Snow Jam, Zoe, solo, 300, `--hud` both | 2.00 / 7 | the race HUD: 67 canvas calls, all equal (arguments, transforms, alphas, every tinted image's bytes); on the HUD's 17757 pixels 1.48 / 5 (the world under them 2.15) |
| hud-sj-ai-900 | the 5-rider run, 900 | 1.77 / 8 | the place (1ST/6 with its glow), the computer riders on the meter: 79 calls equal; HUD pixels 1.33 / 6 |
| hud-sj-ai-60, -185, -200 | the same run, the countdown and GO! | 2.36, 2.29, 4.29 | 71, 81 and 81 calls equal (the 200 frame's mean is the world's, see below) |
| hud-sj-solo-1200, -2700 | the solo run | 2.44, 1.11 | 66 and 68 calls equal; HUD pixels 1.52 and 1.35 |
| hud-sj-solo-15300 | the solo run after its finish | 4.94 / 24 | the FINISH! banner and 00:04:10: 17 calls equal (the world under it is not the page's: see "Next", the 15300 note) |
| sj-solo-190, -300, -1200, -2700-terrain | the solo run (`--lineup` the replay's solo lineup), with the near-rider terrain refinement | 1.59, 1.21, 1.71, 0.91 | before it 1.88, 2.00, 2.47, 1.25 |
| sj-solo-14700, -15050, -15150-finish | the run's end, the trace hashes equal | 1.25, 1.01, 1.73 | before: 2.55, 4.70, 4.19 (the coarse terrain hid the board trail after the finish; ssx3_render_shot stepped the set pieces once, so the finish area's flags never started) |
| sj-ai-200, -900-terrain | the 5-rider run with the refinement | 3.04, 1.07 | before: 4.29, 1.76 (200: the sun query, see above) |
| sj-ai-900-xbox | Snow Jam, 5 computer riders, 900, `--riders xbox` both | 1.76 / 8 | the opponents read their twins too; on the riders' pixels 7.05 (7.11 PS2 on both, 7.68 crossed) |

The earlier milestones' shots stay in the folder for comparison.

What differs is not ported yet, or not drawn the same:
- the freestyle HUD (career-ui.js hud: big air, super pipe, slope style and Rival Points events, which a Single Event also uses): the
  time limit's countdown clock (red in the last 10 s), the 3-row standings over the event's posted scores, the OPPONENT line and the
  finish panel. They read the event the FE set up (career.js postedStandings / timeLimit, the computer riders' heat scores), so they
  come with the FE flow; until then native draws the race HUD for those events (hud-aba1-500: the page's standings and 00:00:55, native's
  race clock 00:00:05; every other element is the same).
- the key caps' vector parts (the rounded rect, its gradient and outline, the arrows) are rasterized at 4 x 4 samples, not Skia's
  analytic coverage; their FEFONT labels go through the exact glyph path.
- the sun's glow and lens flare just after GO! (sj-ai 200, 3.04): GPU raster precision on a near-horizontal edge. The sun's 16 x 16
  depth query (2EC478) sits on a roof edge that is almost horizontal. Over x 493..536 native rasterizes that edge one row lower than the
  page does, so 3 rows of the query are open instead of 4 (visibility 0.1875 against about 0.25), and the glow and its nine lens sprites
  (scaled by the visibility) come out at 0.75 of the page's. The camera matrices agree to about 1e-15, the painter's values are equal,
  and the world vertex stage runs three's float32 steps (view x matrixWorld in the shader, then the projection). What is left is the
  last bits of the GPU's vertex arithmetic (Dawn's and wgpu's Metal shader builds), which a near-horizontal edge turns into a whole
  row. Not pursued.
- texture filtering at edges (documented, not pursued): see "Texture filtering edges" below.
- tier high's MSAA and render scale: the page renders with 4 samples on tiers medium / high (main.js applyAntialias: the canvas and
  the fog-renderer pass targets) and at the display's pixel ratio (renderScale 'full'). Native draws its 640 x 480 frame without MSAA
  and scales it into the window. The parity shots compare at quality=low (no MSAA, the 640-wide frame), where the two agree.

**Texture filtering edges** (documented only; the residual differences along texture and silhouette edges in every shot):
- Mip contents. The page's PNG textures are DataTextures with generateMipmaps on (texture-archive.js texelTexture): three's WebGPU
  backend builds each level on the GPU (WebGPUTexturePassUtils generateMipmaps), drawing a full-screen triangle that samples the level
  above through a sampler with only `minFilter: linear`. Native builds the levels on the CPU as a rounded 2 x 2 box ((sum + 2) / 4).
  - For even sizes the GPU's sample lands on the centre of the 2 x 2 block, so it is the same mean, except that the conversion of the
    filtered float back to unorm8 is the GPU's: a mean that ends in exactly .5 (sum = 2 mod 4) may round the other way. That is at
    most 1 / 255 on a level, but the levels feed each other.
  - For odd sizes (and a 1-wide axis) the GPU samples at the destination texel's centre in the source's UV space, with bilinear
    weights other than 1/4; native clamps the second column / row instead. These differ by more.
  - It reaches the frame wherever a surface is minified (distant terrain, the riders' small textures) and most visibly on
    alpha-tested edges (the rider's 'alph' cut-outs, the crowd, the fences), where the mip's alpha decides which texels pass.
- Not a source of difference:
  - anisotropy and the sampler: both sides use linear / linear / linear with maxAnisotropy 4 (main.js texSetup `anisotropy = 4`;
    world_renderer.cpp and rider_renderer.cpp), and both run on Metal on this Mac (Dawn in Chrome, wgpu in native);
  - the Xbox HD BC path: the page uploads bcMipChain's levels itself (no GPU generation), and native uploads the same bytes.
- To close it: build the native mips with the same GPU pass (a full-screen triangle sampling the level above with a linear minFilter
  sampler, as WebGPUTexturePassUtils does), instead of the CPU box.

Not checked against a page frame yet: the avalanches (no backcountry page shot: a Rival Time run's start needs its rival lineup), the
impact sprites (no grind in these runs) and the start-gate fountains (see below).

**A page finding: the start-gate fountains are never drawn.** main.js passes `startfire: !!course.startfire` to the snow renderer, but the
courses it uses come from courses.json, whose entries have no `startfire` (only main.js's built-in Snow Jam entry had it, and courses.json
replaces that list at boot). Native follows the page (no fountains); the port is there for a courses.json entry that sets it.

Two page behaviours that are not reproduced, both noted in the code: the page draws `Math.random` words for the ready-state flag
grids (their phases are then overwritten from the savestate) and once the core's flag words run out; native uses 0. A flag mesh whose
source batch is blended would take only the pass, not the fringe (none is: every texture 138 / 268 hidden batch of the 14 courses
with flags is blend class 0).

Frame-history state: the page steps the terrain sparkle's Surface painter at most 8 times a frame and builds two patches' sprites a frame,
and the start-gate fountains at most 30 serials a frame; native steps them once a tick and builds every patch in the frame. In live play
(a frame a tick) they agree; a page capture that skips ticks between frames (browser-shot's 400-tick advances) can differ.

**Known difference: tied strip order.** Two strip effects with the same renderOrder (two riders' wakes, the aura faces, the boost
ribbons) are ordered by the depth of each geometry's bounding sphere centre. three computes that centre once, on the mesh's first
projection, from the whole position attribute (its capacity, zeros and stale vertices included), and keeps it. On the page that first
projection is not deterministic, so native cannot take it from "the same frame":
- it is the first encoded-pass render (web/snow-composite.js, only when some effect is populated) after the mesh exists and is visible;
- which frames the page draws depends on wall-clock pacing (display rate, tick-lock debt, dropped frames), not on the tick;
- the loading warm-up draws real frames through the encoded pass, and the warm-up is sliced by pipeline build time;
- native/tools/browser-shot.mjs steps the run in chunks of 400 ticks (ssxQA.advance) with page frames only between the chunks.

Natively the centre comes from the first frame that draws the mesh with the pass populated: the first such tick in the window
(`ssx3`, one frame a tick under tick-lock) and the shot's only frame in ssx3_render_shot. Only overlapping strips with tied
renderOrder can blend in another order. Additive strips (boost ribbons, aura faces, streamers) give the same bytes in either order
within the 8-bit rounding of each blend, so in practice this affects the normally blended wakes of two riders that overlap on screen.

ssx3_render_shot prints the frame's light glow count and whether the sun is in view (`glows`, `sun`), to pick ticks that show them.

**Audio parity** (ssx3 `--audio-log` against browser-shot `--audio-log --paced`, the same run to the same tick):

| Run | To tick | Result |
| --- | --- | --- |
| sj-ai (Snow Jam, 5 computer riders) | 3000 | 154 sfx requests on both sides, equal in tick, bank slot, sound, bus, volume, distance gain and layer count, except 6 crash sounds whose variant is a `Math.random` pick (0x30 + 0..2). The listener's position and matrix are equal on every tick. The speech posts (the venue intro, the Radio BIG intro, the artist intro) are equal in event and speaker. |
| aba1-score (Crow's Nest big air, solo) | 2000 | 93 of 98 requests equal. The others: two grunt variants (random picks), two crowd notes a tick apart (the crowd sequences run on the wall clock), and one named-bank world emitter that native starts 4 ticks earlier at volume 0 (the page waits for the bank's fetch; native reads it at once) |

Earlier checks: the decoders are bit-exact on 789 bank patches, 9,806 speech lines and 11,849 music samples. The Web Audio scenarios are
exact or within -107 dB of Chrome's render. Each of the 111 sfx sounds is within -90 dB, 30 of them exact. The pathfinder core gives
3,478 identical segments across 45 songs.

What differs:
- the song pick and the other `Math.random` draws (crash and grunt variants, DJ lines): random on both sides.
  - **To follow (pv sfxPs2Rng / crowdSeqPs2, [audio-logic.md](audio-logic.md) 5.6):** the page now takes the crash sound, the
    human's grunt and the crowd picks from the core's presentation stream (`visual_rng_words`, BXrand 0x3177F0 = 0x317A08). It
    uses the PS2 formulas (web/sfx-ps2-rng.js) and the dispatchers' gates. The draws come after every rider's physics and before
    121818: web/game-tick.js calls tick_input, race_begin, tick_rider, then gameAudio.riderPass, then tick_progress. With the same
    point and rules, native's picks equal the page's. A reaction posts all its crowd notes in its own tick, at
    10 x 94 / 93.75 ms a sequence tick, on the audio clock. Compare the post tick and the delay, not the note's start tick.
  - **pv sfxPs2Rng: the draws native must make** (once it flips, the combined ?simtrace hash includes the visual words they move).
    The page's draw sites, in order; each draw is one BXrand step (0x317A08) on the human core's `visual_rng_words`:
    - **The rider pass** (web/game-tick.js: tick_input, race_begin, tick_rider, then `gameAudio.riderPass`, then tick_progress).
      sfx-game `riderPass` takes the riders sfx-game already knows, in slot order. It reads each rider's queued `audio_events` (without
      clearing them) and fresh `audio_telemetry`, and draws, per event in order:
      - AE_CRASH (3): a crash draw if `(human || listener distance < 100) && surface b != 2 && b != 3`; then a grunt draw if the grunt gate
        is open and the rider is the human; then a crowd draw if `human && |a| > 0 && crowd.reacts('fall', level)`.
      - AE_CRASH_LOOP (4) with b != 0: a crash draw (the surface is telemetry [5]).
      - AE_CRASH_GRUNT (6) kinds 0 / 1: a grunt draw (gate, human). Kind 2 and AE_SOFT (18) draw nothing, but they close the gate for the
        rest of the pass.
      - AE_LANDING (2): a crowd draw if `human && a > 0 && crowd.reacts('trick', level)`.
      - AE_PAIR (24): a grunt draw for the target B (c) when `(A || B human) && CHARACTER > 0 && neither motion is 2` and the gate is open.
      - The grunt gate: CHARACTER bus > 0, not a replay, the rider not finished, no grunt voice playing (any rider's, `G.gruntVoice`), and no
        grunt already decided in this pass.
      - The values are kept per (rider, event index). The tick's dispatch takes them by kind. A queue that has not been drained yet is
        not drawn again (`r.preSeen`).
    - **The tick's audio** (`gameAudio.gameTick` -> sfx-game `tick`): an event the pass did not see draws when its dispatcher runs (same
      gates). The anticipation pick (crowd.update -> 2A5AE8) and the finish cheer `crowd.trick(2)` (286EA0) draw here.
    - **Formulas:** crash `0x30 + (r & 0x7FFF) * 3 / 0x7FFF`; grunt kind 1 `(r & 0x7FFF) * 3 / 0x7FFF`, kind 0 the same + 3; crowd
      `list[r % n]`.
    - **Replays:** the replay start does not restore 0x4FF018 (main.js startRun), as the PS2's 0x26DBF0 restores 0x4FF030 only. A
      replay draws with its own gates, so the replay checks compare `__simTrace.sim`: the hash without the camera and the visual words.
- the startup timing: the native first frame loads the riders' renderers (about 0.9 s, after the world-load audio), which the page
  does under its load screen. So the lines and DJ timers after the world load start later natively. This is fixed with the load
  screen in the FE flow.
- the free-ride, hub, transport and post-event paths of the director (Conquer the Mountain only) are not ported. Pause (pause()) is ported,
  but nothing calls it until the FE flow has a pause menu.
- the avalanche rumble is ported (per tick, from the snapshot the draw shares), but no fixture yet starts an avalanche within its
  checked ticks.

**Next** (playable end to end, each module ported 1:1 from the page's):
- the in-race HUD: done for races (above); the freestyle HUD comes with the FE flow;
- audio through SDL3: done for Single Events (above);
- the two renderer gaps (re-shot with `--paced`): sj-solo 15300 was not a valid comparison (the run's pad ends at 15199 and the page
  stops ticking at its results, and the page ran the 5-rider default lineup: the solo fixture needs `--lineup` with the replay's
  `enabled: false` lineup). At valid ticks the finish frames now agree (below); sj-ai 200 is written up above;
- **Resume point (paused 2026-10-14 at the user's request):**
  1. Build `ssx3` once web/net_world.inc compiles. Run ctest; its 6 fast tests and core_verify_replays passed after the results
     work and before the Restart edits.
  2. Check Restart on sj-ai. Native: `--menu-keys Space,ArrowUp,Space` from the results (Restart, then Up to Yes, then Yes).
     Compare `--results-log`'s `.1` file for the second run with the first, and check that `--shot-run 1` frames match the first
     run's frames at the same tick. Page: browser-shot `--keys Space,ArrowUp` with `--ui-clock 120 --canvas-log`, for the
     87yndialog's canvas calls.
  3. The slow app load (about 20 s, against about 7 s for ssx3_render_shot's load) is still not explained. The sequential load
     was as slow, so the loader thread is not the cause. Time the loader stages next.
  4. Then the order the coordinator set: the results' menu sounds, the title, the Single Event select, the objectives card and
     the intro (the cutscenes, which the Restart's gondola ride needs too), then tier high's MSAA and render scale.
  5. When pv sfxPs2Rng flips on the page, native's crash and grunt sfx picks need the same presentation-RNG draws. The sfx agent
     documents the draw sites, and the coordinator says when.
- the FE screens and the game flow: the load screen, a race's results and the auto replay behind them are done and checked;
  Restart without the cutscenes is written but not yet run (above). Next:
  - the results' menu sounds (web/audio-menu.js), and the freestyle results;
  - then the title, the Single Event select, the objectives card and the intro;
- tier high's MSAA (4 samples on the world and the fog-renderer pass targets, main.js applyAntialias) and render scale 'full' (the
  display's pixel ratio): the native app defaults to tier high, so players see the difference.

## 8. Windows

**Cross build from macOS** (2026-10-13). This uses llvm-mingw 20261006 (mstorsjo/llvm-mingw, the
`llvm-mingw-20261006-ucrt-macos-universal.tar.xz` release, SHA-256 `a7a19b11…ebf6`), unpacked into local/vendor/llvm-mingw:

```sh
cmake -S . -B build/native-win64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=native/cmake/llvm-mingw-x86_64.cmake   # LLVM_MINGW=<dir> elsewhere
cmake --build build/native-win64
ctest --test-dir build/native-win64        # windows_link
```

- **The executables are standalone.** They link statically: libc++, libunwind and the compiler runtime are inside. They import
  only Windows system DLLs (KERNEL32 and the UCRT api-ms-win-crt-*).
- **windows_link** (native/tests/check-pe.mjs) checks each .exe: an x86-64 PE image that imports only system DLLs.
- **Nothing runs on macOS** (wine is not used), so the parity tests below need a Windows machine.
- **Fixed for Windows:**
  - M_PI (`_USE_MATH_DEFINES`) and `<string>` come in through native/core/shim/native_shim.h;
  - the replay meta is parsed as chars (newer libc++ has no `char_traits<unsigned char>`);
  - the context blocks are allocated with `_aligned_malloc`.

**Running the parity tests on a Windows machine** (none available yet):
1. **Install:** git, CMake 3.21+, Ninja, node 22.12+, Python 3, and llvm-mingw (the `ucrt-x86_64` release; put its `bin` on PATH).
   Chrome is also needed, but only to make new fixtures.
2. **Bring the prepared data**, built from your own disc: either run tools/setup_from_iso.py on the Windows machine, or copy
   `web/public/assets`, `web/generated`, `engine/generated` and `web/runtime` from a prepared checkout of the same commit.
   For the gate test, also copy local/ps2-capture/runs/event-race.{bin,capture.json}. For the replay test, copy
   local/native-port/fixtures.
3. **Configure natively** (no toolchain file):
   `cmake -S . -B build\native-core -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`
4. **Build and test:** `cmake --build build\native-core`, then `ctest --test-dir build\native-core --output-on-failure`.
   - This runs the same tests: the race traces, the gate replay, the replay verifier fixtures and the pad input.
   - The wasm reference runs under node on Windows too.
   - Each test must say "identical"; the first differing tick is the lead.
5. **Run the float-conversion check:** a second build with `-DSSX_NATIVE_UBSAN=ON`. This is the first build where x86-64's
   INT_MIN conversion could differ from wasm.

A cross-built .exe from build/native-win64 also runs there: for example
`ssx3_verify_replay.exe web\public\assets fixtures\sj-ai.ssxr`, which must print the fixture's verdict.

**The front end: untested, parked** (2026-10-14, the user's call). Configuring a Windows build with `-DSSX_BUILD_FRONTEND=ON` stops
with "The Windows front end is not supported yet". The core tools above (build/native-win64) are unchanged and need no new downloads.
- A cross build of the front end linked cleanly once (ssx3.exe, ssx3_render_shot.exe, ssx3_image_compare.exe with wgpu-native's
  windows-gnu release linked statically) and passed the PE import check. It needed SDL3's Windows release, a download that was not
  approved, so that build and the fetch were removed. Bringing it back needs an approved SDL3 for Windows (the official mingw
  development release, shipped as SDL3.dll next to ssx3.exe), and then the import check for the front-end images (check-pe.mjs
  `--allow SDL3.dll`; the system DLLs wgpu-native imports are already in its list).
- The toolchain file finds CMake packages only under the target's roots (`CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY`), so a host
  SDL3 (Homebrew's) is never picked up by a cross build.
- D3D12 / DXGI / Vulkan are not imported: wgpu-native loads them at run time (d3d12.dll, dxgi.dll, vulkan-1.dll), so the static
  check cannot see them.

**A first run on real Windows has to check** (once it is unparked; no Windows machine yet):
1. **Start-up:** `ssx3.exe <assets> --course ARA1` opens the window. wgpu-native picks an adapter (D3D12 first) and the surface
   (`WGPUSurfaceSourceWindowsHWND` from SDL's HWND / HINSTANCE) configures with a BGRA8Unorm or RGBA8Unorm format (the non-sRGB
   choice in app.cpp). An adapter or surface failure prints the webgpu error and exits.
2. **Shaders:** every pipeline compiles on the D3D12 backend (WGSL -> HLSL through naga, FXC or DXC). The windowed run builds them all
   at load, and `--shot OUT.png --shot-tick N --hidden` covers the same code with no presenting.
3. **Parity:** `ssx3_render_shot.exe <assets> sj-solo.ssxr out.png --tick 300` then `ssx3_image_compare.exe` against
   local/native-port/shots' web frames. The trace hash it prints must equal the macOS one (3993471844 at 1200, 2318221815 at 300).
   The image mean should be the macOS one within a little (D3D12's texture filtering and rasterization rules differ slightly from
   Metal's).
4. **Input:** keyboard focus only on click (SDL_WINDOW_NOT_FOCUSABLE is ignored on Windows: check the window does not take focus
   when it opens), the keyboard mapping (scancodes), and an XInput / DirectInput pad through SDL's gamepad mapping.
5. **Pacing:** tick-lock at 60 Hz on a 60 Hz display (debt paid with 0 / 2 ticks), the fixed clock on a 120 / 144 Hz one; vsync through
   `WGPUPresentMode_Fifo`.
6. **Console:** ssx3.exe is a console-subsystem program (main(), no SDL_main), so a console window opens with it. That is fine for a
   first run. A release build would link `-mwindows`.
7. **The float-conversion check** above (`SSX_NATIVE_UBSAN=ON`) on x86-64, with the front end on.

## 8b. Linux (x86-64, arm64): prepared, not built or run

The build is ready for Linux in CMake and the code (2026-10-14), but nothing has been built or run on Linux. There is no Linux machine,
container or VM here, and the user has said no downloads for Linux for now; the Linux code is compile-checked on macOS only.
- **Configure:** `cmake -S . -B build/native-core -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DSSX_BUILD_FRONTEND=ON`.
  Clang is required (as on macOS). The core and the tools have no platform code: `std::aligned_alloc` for the context blocks,
  `-femulated-tls`.
- **wgpu-native:** native/cmake/wgpu_native.cmake fetches `wgpu-linux-x86_64-release.zip` (pinned). For arm64
  `wgpu-linux-aarch64-release.zip` (15,755,874 bytes) stops configure with "SHA-256 not pinned yet" until its download is approved and
  pinned. Linked statically, with dl / pthread / m.
- **SDL3:** the system package through `find_package(SDL3 CONFIG)` (Debian 13 / Ubuntu 25.04+: `libsdl3-dev`, 3.2.x). Without it only the
  headless tools build (ssx3_render_shot, ssx3_image_compare and the verifier / parity tools).
- **The window's surface** (app.cpp): X11 (`WGPUSurfaceSourceXlibWindow` from SDL's X11 display and window number) or Wayland
  (`WGPUSurfaceSourceWaylandSurface`), as SDL's video driver chose (`SDL_VIDEO_DRIVER=x11|wayland`). That branch was compile-checked
  on macOS against the same SDL3 and webgpu.h headers.
- **A first Linux run has to check:**
  - the ctest suite (it runs the wasm reference under node);
  - ssx3_render_shot's trace hashes against the macOS ones, and its frames with ssx3_image_compare (Vulkan, or GL as a fallback);
  - the window on X11 and on Wayland (focus, the pad through SDL);
  - the SDL3 3.2 API: app.cpp is built against 3.4 on macOS, and its calls and hints must also exist in 3.2 (not checked yet).
- **The download set proposed for later** (not approved; nothing fetched). Sizes are from HEAD requests:
  - wgpu-native v29.0.1.1 for Linux arm64, to pin its SHA-256 (Apache-2.0 / MIT):
    `https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-linux-aarch64-release.zip`, 15,755,874 bytes. The x86-64 zip
    (15,962,605 bytes) is pinned already and would be fetched by configure on Linux.
  - A VM image for the installed qemu (qemu-system-aarch64 / x86_64 11.1 from Homebrew): Debian 13 genericcloud arm64
    (`https://cloud.debian.org/images/cloud/trixie/latest/debian-13-genericcloud-arm64.qcow2`, 337,510,400 bytes; HVF-accelerated) and
    amd64 (`debian-13-genericcloud-amd64.qcow2`, 341,508,096 bytes; emulated). Alternative: Ubuntu 25.10 arm64
    (`https://cloud-images.ubuntu.com/releases/25.10/release/ubuntu-25.10-server-cloudimg-arm64.img`, 883,270,144 bytes).
  - Inside the VM, Debian's packages: clang, cmake, ninja-build, libsdl3-dev (3.2.10), nodejs, python3 and mesa-vulkan-drivers
    (lavapipe, for headless Vulkan).

## 9. Layout

| Path | What |
| --- | --- |
| CMakeLists.txt | the root project: `SSX_BUILD_CORE`, `SSX_BUILD_METAL_PROTOTYPE`, the parked `SSX_BUILD_PS2_REFERENCE` |
| native/CMakeLists.txt | the core library, the tools, the tests |
| native/cmake/core_sources.cmake | reads the units and defines from web/build-core.sh |
| native/cmake/llvm-mingw-x86_64.cmake | the Windows x64 cross toolchain (llvm-mingw) |
| native/cmake/wgpu_native.cmake | the pinned wgpu-native release (fetched at configure time) |
| native/tests/check-pe.mjs | the Windows link check |
| native/third_party/stb | stb_image / stb_image_write (public domain or MIT), the front end's PNG reading and writing |
| native/core/ | the native rider contexts, host hooks, race driver, call replayer, emscripten shim |
| native/gen_dispatch.py | the replayer's export table |
| native/tests/parity.mjs | the parity tests |
| native/tools/ref-trace.mjs, spy-core.js | the wasm side: the race driver's script on the wasm core, the export-call logger |
| native/verify/ | the replay verifier: replay_file.hpp (SSXR and the pad stream), js_json.hpp (JS JSON semantics), core_api.hpp, verify_replay.cpp |
| native/tools/browser-verify.mjs | the browser verifier's verdicts and replay traces (the N2 reference) |
| native/tools/tas-replay.mjs | a replay file of a page run with no finish (page-run's start.json + the TAS pad) |
| native/frontend/ | the front end: app.cpp (SDL3 window), render_shot.cpp, world_renderer (+ post_effects, rider_fx, rider_shadow), rider_renderer / rider_capture, race_presenter, set_pieces / set_piece_fx (livecomp, flags, particles, avalanche, startfire, terrain_sparkle), screen_tint, sun_flare, light_glow, uv_scroll, pacing, pad_input, gpu (webgpu.h helpers), texture_archive, image_io, image_compare |
| native/verify/replay_session.hpp | the race set up as the page does (shared by the verifier and the front end; live or replay) |
| native/tools/browser-shot.mjs | the page's frame at a tick (visual parity; `--eval` for page diagnostics) |
| native/tools/replay-tas.mjs | a replay file's pad as a TAS file (browser-shot plays a replay's run) |
| native/tools/compare-canvas-log.mjs | the page's and native's UI canvas call logs compared (the HUD's call-for-call check) |
| native/build-core-native.sh | the first prototype build script (superseded by CMake; kept while the migration agent uses it) |

Third-party code: nlohmann/json (MIT, vendored in web/third_party/nlohmann, shared with the wasm build); stb (public domain or MIT,
vendored in native/third_party/stb); fetched at configure time for the front end: wgpu-native (Apache-2.0 / MIT); SDL3 (zlib) is a
system install. Each is listed here and in README with its licence.
