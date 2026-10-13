# Native build

Owner: the native-port agent (2026-10-13). The goal (the user's): native macOS and Windows builds of the game that run the
same simulation core as the browser build, bit for bit. The core is the C++ under web/ and engine/ that web/build-core.sh
compiles to wasm; the native build compiles the same sources with clang for the host and checks every result against the wasm
build's.

State:
- N1 (the build and its parity tests) and N2 (the native replay verifier, section 6) work on macOS arm64.
- Next: N3, a minimal native front end.
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

A solo or big-air run is 0.5-3.5 s natively, with nothing to start. The native time is dominated by fesetround (about 30 % of the
samples): the proposed wasm floating-point environment (the coordinator's call: 9 engine/ files take their `__EMSCRIPTEN__` branches with SSX_NATIVE_CORE) takes it out. With that change and -O3, Snow Jam with five riders is 8.4 s.

**LTO breaks exactness natively.** `-O3 -flto` diverges, even on a solo run, and fails on six riders with "Race tick already open".
-O3 alone stays exact. A full `-fsanitize=undefined` run reports nothing. Likely cause: LLVM treats a thread-local address
(`llvm.threadlocal.address`, here emulated TLS) as fixed within a function, and cross-unit inlining across a context switch
breaks that (the hazard docs/core-migration.md M3 notes). Not investigated further: native builds do not use LTO.

## 7. N3: the native front end (plan; nothing rendered yet)

**Platform layer: SDL3** (zlib licence; 3.4 is installed by Homebrew here).
- One library covers the window, keyboard, gamepads, high-DPI and later audio on macOS, Windows and Linux.
- SDL_Gamepad gives the W3C "standard" layout that web/pad-input.js reads, from the same controller database family the browsers use.
- A hidden window (`SDL_WINDOW_HIDDEN`, no focus taken) or no window at all serves the tests.
- GLFW has no audio or gamepad mapping database; a hand-written layer per OS is three times the work.

**GPU: webgpu.h,** with wgpu-native first (prebuilt releases, Apache-2.0 / MIT) and Dawn (BSD-3, Chrome's implementation) as a later
swap behind the same header. The web build's three.js WebGPU materials are TSL, so their WGSL comes from three.js. The port
writes them as about 6-8 WGSL pipelines:
- terrain with the PS2 light page;
- instances per blend class (opaque, the pass + fringe pair, additive);
- the sky;
- the skinned, lit rider;
- the fog CLUT composite;
- ScreenTint.

All of them do integer maths on GS bytes in RGBA8Unorm targets (pv encodedBlend: no sRGB, identity output), so they port exactly.

**Data:** the exported assets as the page reads them.
- ARA1: world.json, vertices.bin (40-byte stride), indices.bin, colors.bin, vertex-alpha.bin, terrain-light-atlas.png,
  terrain-lighting.json / terrain-render.json, screen-tint.json.
- TEXTURES/world.tex (SSXTEX01: indexed PNGs) and the SKY package.
- RIDER_<ID> (rider.json, mesh, WARDROBE textures).
- From the core: rider_skin_palette, rider_lighting_gpu_coefficients, fog_palette_info / rgba, fog_info, and the tick's camera
  (eye, look, half-FOV, near, far in native meters).

**Pacing:** web/tick-lock.js (one tick per display frame within ±25 % of 60 Hz, else web/fixed-step-clock.js with the
interpolation alpha).

**Input:** native/frontend/pad_input.hpp is web/pad-input.js, natively: the Classic / Simple keyboards, the standard pad, and the
same arithmetic. The ctest frontend_pad_input checks 4800 frames bit for bit against the JS.

**Visual parity check:** the native renderer draws offscreen at the same tick and camera as a page screenshot (Chrome headless);
the two are compared side by side.

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
| native/frontend/pad_input.hpp | web/pad-input.js natively (the front end's input); native/tests/pad-cases.mjs + pad_input_test.cpp check it |
| native/build-core-native.sh | the first prototype build script (superseded by CMake; kept while the migration agent uses it) |

Third-party code: nlohmann/json (MIT, vendored in web/third_party/nlohmann, shared with the wasm build). Nothing else yet;
each new library is listed here and in README with its licence.
