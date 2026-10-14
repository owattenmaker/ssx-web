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

**State (2026-10-13):** the course world, sky, fog composite, ScreenTint and the skinned, lit human rider render from the exported
assets. The core ticks at 60 Hz with the page's pacing. It runs in an SDL3 window (macOS) or headless. At the same tick and camera
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
  - Audio is never opened. The window is not focusable and the app does not activate (no Dock icon) unless `--focus`, so a run
    never takes the user's keyboard.
  - Keys: web/pad-input.js Classic; gamepads through SDL's standard mapping; Escape quits.
- `ssx3_render_shot ASSETS RUN.ssxr OUT.png --tick N`: headless, no window.
  - It draws the frame the page draws right after `ssxQA.advance` stepped N + 2 ticks (render alpha 0): the camera and rider
    palette of tick N over the world state of tick N + 1.
  - It prints the ?simtrace hash of tick N, so the page and native states can be checked equal before the pictures are compared.
- `ssx3_image_compare WEB.png NATIVE.png OUT.png`: web | native | difference x 4 side by side, plus the mean / 95th percentile.
- native/tools/browser-shot.mjs: the page's frame at the same tick.
  - It plays the run's TAS pad in a Single Event (as tools/tas/page-run.mjs does) in headless Chrome, muted, widescreen Off, with
    the HUD hidden.
  - It prints the page's ?simtrace hash of tick N.

**What is ported** (each file names its web source):

| Native | Web | What |
| --- | --- | --- |
| frontend/world_renderer.cpp | main.js asset(), world-material.js, fog-renderer.js | the packages (40-byte vertices, colours, indices, vertex alpha, the PS2 light UVs per patch), the terrain combine (Cd - Cl) x Al >> 7, the static-model modulate with blend classes 0 / 1 (pass + fringe) / 2 / 3 (additive), three.js's render lists (opaque by renderOrder, the projected bounding-sphere depth near first, id; transparent far first; every DoubleSide transparent back face, riders' included, before the front faces), one object record per moved / copied batch, the sky pass as the world pass's background, the CLUT fog on the world depth (three.js perspectiveDepthToViewZ), texture chunks hidden while not streamed (section_chunks) |
| frontend/screen_tint.hpp | screen-tint.js | the quadtree payload, the distance blend in fround steps, the lightning flash |
| frontend/rider_renderer.cpp, rider_capture.hpp | rider-skinning.js, rider-lighting-nodes.js, rider-material.js | the core's skin palette pair (previous / current), VU chop add / mul in WGSL, the per-vertex irradiance of the core's ten rows, GS HIGHLIGHT2 on PS2-domain texels, riderDrawState's two blended passes for 'alph' / 'ea*' |
| frontend/pacing.hpp | tick-lock.js, fixed-step-clock.js, ps2-frame-pacing.js | one tick per 60 Hz frame (debt paid with 0 / 2), else the fixed clock with the render alpha |
| frontend/pad_input.hpp | pad-input.js | the 24 channels, bit for bit (ctest frontend_pad_input) |
| frontend/texture_archive.cpp | texture-archive.js | SSXTEX01, exact RGBA8 (stb_image), mips as a 2 x 2 box |
| frontend/uv_scroll.hpp | uv-scroll.js, set-pieces-renderer.js | the scroller 0x35F7D0 in the EE scalar arithmetic (chop MUL / ADD / SUB with the guard bit, DIV.S nearest), the scroll groups by first appearance, the set-piece clock (set_piece_info, the slot-1 section enters, the ready state's phases); drawn as the first instance indexing a storage buffer |
| frontend/set_pieces.hpp, livecomp.hpp, flags.hpp, ee_float.hpp | main.js frame, moving-instances.js, set-pieces-renderer.js, livecomp-animation.js, attached-setpieces.js, flag-animation.js, set-piece-cables.js, ee-scalar-float.js | per world batch this frame: hidden (pickups, event-dead from the event start, stage-program draw states), shown (the hidden batches of flag static draws, MeshAnim pieces, LiveComp 'object' players) and moved (moving instances and their clones, LiveComp node deltas, teeters, attached children, magnets, break pieces); the LiveComp players and the flag cloth stepped one game tick at a time to the core's set-piece clock (the go tick, the section log, the core's trigger / timer starts, section clocks, flag words), from the ready state; the cables as screen-space lines (guard band, near / far), alive by CableLife and the owner's chunk |
| frontend/rider_shadow.hpp, world_renderer.cpp terrain, rider_renderer.cpp drawShadow | rider-shadow.js, rider-skinning.js shadowPose, world-material.js riderShadowReceiver / glintBytes | the rider shadows: the six-bone box fit (374D00, float32), the nearest six riders in view, each silhouette (the drawn palettes, the parts' painter's rank, the board 0 while grounded) into its slot of the 768 x 128 atlas, the terrain's max(Cd - T, 0) in the 30 m slab; and the terrain glint pass 38D168 (terrain-glint.json) before it |
| frontend/set_piece_fx.hpp, particles.hpp, terrain_sparkle.hpp, avalanche.hpp, startfire.hpp | set-piece-particles.js, set-piece-particle-eval.js, fog-puffs.js, set-piece-halos.js, crowd-2d.js, terrain-sparkle.js, avalanche-state.js, avalanche-trails.js, startfire-renderer.js, impact-fx-renderer.js | the stage programs' particle sprites (the VU1 burst / trail kernels: accumulators in the VU / EE operations, the sprite arithmetic in doubles like the page), the fog puffs (0x22A270 box cull, 0x2DC190 fades, far-to-near), the HaloModifier sprites, the 2D crowd's texture frames and camera flashes, the terrain sparkle (program 4's RNG chain, the twinkle rotation, the Surface painter's density) at renderOrder 640, the avalanche pieces' visibility and trails, the impact sprites (sparks with the per-slot fade, glints, grind chunks, the fist sparkle) and the start-gate fountains, each in three's order in its pass |
| frontend/rider_fx.cpp, draw_order.hpp | board-trail.js, wake-renderer.js, snow-renderer.js, snow-billboard.js, boost-renderer.js, opponent-fx.js, snow-composite.js, ps2-draw-order.js | the trail ribbon / roof in five stencil passes after the world's opaque draws (renderOrder 500 .. 504); the encoded pass: the fogged world bytes in their own target, the wake, the ten snow emitters (three's lookAt quaternion, Matrix4.compose, the 128-pixel extent cap, the flipbook frame), the boost ribbons, aura faces and streamers (strm rows flipped), drawn with the world depth in three's transparent order (the 0x364240 key, then each geometry's bounding-sphere depth as three first computed it, then creation order); every computer rider's from its own core, culled by its 100 m sphere (pv fxCull) |
| frontend/sun_flare.hpp, light_glow.hpp, post_effects.cpp, painter_tree.hpp | sun-flare.js, light-glow.js, fog-renderer.js | the Sun painter per camera tick; the 16 x 16 sun depth query and the glow plus nine lens sprites (2F4DB8 / 2F4A08 / 2F4690); each light's sprite quad and query record (2E2B00 / 2E2868 / 2EC478), drawn additively (copies) into the glow target; the composite adds glow, then sun, after the fog and before ScreenTint |

**Visual parity** (local/native-port/shots: web | native | difference x 4; the ?simtrace hashes of the tick are equal in every shot):

| Shot | Run, tick | Mean / p95 | Notes |
| --- | --- | --- | --- |
| sj-ai-900-shadow | Snow Jam, Zoe, 5 computer riders, 900 | 1.76 / 8 | the rider shadow on the pipe wall; 1.81 before it, 23.3 / 135 at first light |
| sj-solo-2700-shadow | Snow Jam, Zoe, solo, 2700 | 1.10 / 4 | the shadow on the run-out; 1.25 before it |
| sj-solo-190-crowd | Snow Jam, Zoe, solo, 190 | 2.06 / 8 | the stadium's animated crowd, the start-gate doors open at GO |
| sj-solo-300-effects | the same run, 300 | 2.00 / 7 | gondolas, cable, banners, glows, sun |
| sj-solo-1200-fx | the same run, 1200 | 2.47 / 9 | 4.8 / 21 at first light |
| sj-solo-2700-fx | the same run, 2700 | 1.25 / 5 | the spray and both air streamers |
| aba1-600 | Crow's Nest big air, Zoe, 600 | 1.21 / 4 | a second course (native/tools/replay-tas.mjs: the replay's pad as a TAS for browser-shot) |

The earlier milestones' shots stay in the folder for comparison.

What differs is not ported yet, or not drawn the same:
- the rider's board flex (pv boardFlex, web/board-flex.js: the board's and the hands' morph targets before skinning): the board's nose and
  tail bend on the page, native draws the rest shape;
- edges: texture filtering details (three.js generates mips on the GPU; anisotropy) and the Xbox HD rider textures (the page's desktop
  default; native uses the PS2 set).

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

**Next:**
- the rider shadow;
- the Windows front-end build (llvm-mingw);
- the HUD from the game's own LUI screens;
- audio.

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
| native/build-core-native.sh | the first prototype build script (superseded by CMake; kept while the migration agent uses it) |

Third-party code: nlohmann/json (MIT, vendored in web/third_party/nlohmann, shared with the wasm build); stb (public domain or MIT,
vendored in native/third_party/stb); fetched at configure time for the front end: wgpu-native (Apache-2.0 / MIT); SDL3 (zlib) is a
system install. Each is listed here and in README with its licence.
