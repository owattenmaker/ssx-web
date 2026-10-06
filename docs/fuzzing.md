# Coverage-guided pad fuzzing (fuzzing agent, 2026-10-05)

The goal is to find the physics and animation divergences between the port's core and the PS2 that hand-made pad scripts miss.
The method:
1. Mutate the gated captures' pads.
2. Keep the variants that reach new core code in the port (fast, headless).
3. Capture the kept ones on ARMSX2 from the same baseline and compare them tick by tick.

Tools are in local/fuzz (scratch, not shipped). Nothing here touches web/runtime or the core sources.

## Coverage core

- `local/fuzz/build-cov-core.sh OUT` builds a coverage core:
  - It takes web/build-core.sh's own em++ line (sources, flags, exports) with clang source-based coverage added:
    `-fprofile-instr-generate -fcoverage-mapping`, `--profiling-funcs` (llvm-cov needs the names), local/fuzz/cov_export.cpp
    (the profile buffer API) and local/fuzz/cov-post.js.
  - `-flto` is dropped to save build time.
  - It compiles a snapshot of the sources copied to OUT/src, so llvm-cov's line numbers stay right while other agents edit the
    tree.
- Arithmetic is unchanged: the full `test-ps2-captures` on the coverage core is 465 scenarios clean (CORE_JS=).
- Counters:
  - With `SSX_PROFRAW_DIR` set, every core instance writes its raw profile at process exit (one file per process; the rider
    contexts share one instance's counters).
  - The comparers' RNG-alignment child passes write nothing.
- emsdk's llvm-profdata (LLVM 24) reads raw profile version 11 only, but emscripten's wasm32 compiler-rt writes version 10, with
  56-byte data records counted as 48-byte structs. `local/fuzz/profraw2text.py` converts the raw files to LLVM's text format;
  llvm-profdata merges that, and llvm-cov reads it with the wasm.
- `covmap.py report DIR` gives coverage per gameplay area. `covmap.py gaps DIR` lists uncovered functions, blocks and one-sided
  branches, with the PS2 addresses their comments carry. Render, camera, fx, stage, mission and seed-data code is excluded.

### Baseline (all 465 ps2-captures scenarios, coverage core v2: the snapshot build)

| area | functions | regions | branches |
|---|---|---|---|
| air | 99.0% | 95.3% | 90.1% |
| landing | 100% | 98.6% | 93.8% |
| press | 96.5% | 94.7% | 87.7% |
| crash/reset | 96.5% | 89.8% | 81.1% |
| plant | 96.2% | 89.4% | 74.6% |
| scoring | 94.3% | 89.7% | 82.7% |
| npc/race | 93.3% | 88.3% | 80.0% |
| uber/tricks | 85.2% | 86.5% | 80.9% |
| pair/attack/collision | 86.0% | 81.7% | 73.1% |
| rail | 83.5% | 82.2% | 73.2% |
| animation | 75.8% | 81.0% | 74.8% |
| ground/core | 77.3% | 79.8% | 72.2% |
| **gameplay** | **84.7%** (1793/2118) | **83.9%** (17008/20268) | **76.3%** (11720/15362) |

Many one-sided branches are compound conditions on callbacks that are always set, which no pad can flip.

**The v1 build's gap labels were wrong:** it compiled the live tree, which other agents had edited before the gaps were listed, so
its labels were 2-5 lines off. Every label here comes from the v2 snapshot (cov-core/src). Rails below 555 cm/s (13B0A4), the
13BB14 push-away and the teeter force (342538, riders/fareastmyth-uber-b) are all covered.

**Reachable code never run:**
- originalRailUberBalanceStep (the kind-10 rail Uber balance state);
- originalNpcDesignatedBehavior (100B90).

**Branches never taken** (`targets.py list`):
- originalCollisionReaction: the control 9 / 11 returns, the surface reset (surfaceProperty44), classes 20 / 21 picking 361.
- originalRailAttach:
  - style picks: flag330 off a press, control 12;
  - previous style 3 / 4.
- pair_react: a kind-2 pair crash, and a pair contact in control 1.
- surface_landing_control: a control other than 5 / 13.

**Not reachable offline:**
- OriginalRiderPairSystem::respondToAttack: online races only (race_world_pair_respond);
- refreshProximity: native engine/rider_world.hpp only;
- native and test-only helpers.

## Fuzzer

`local/fuzz/fuzz.py`:
- `seeds`: the eligible seeds, 349 of them:
  - gated cases in web/test-ps2-captures.mjs with a baseline savestate and a pad script;
  - six-rider cases through compare-ai-capture.mjs.
  - Peak, CTM, document and weather cases are excluded.
- `round NAME BUDGET PAR`: one round of mutated variants.
- `calibrate PAR`: every seed unmutated, to calibrate the invariant caps.

**A variant** is a seed's whole pad from its own baseline, so ARMSX2 can capture it from frame 0.
- It replays through compare-ai-capture.mjs (six-rider seeds), or compare-ps2-capture.mjs --pad with --sync-rng.
- With --sync-rng it uses the seed's cached per-tick RNG draw counts (`__RNG_DRAWS_IN`, local/fuzz/rng):
  - every tick starts from the PS2's recorded RNG words, so the computer riders' draws are in the stream;
  - up to the mutation the replay is the gate's exact one;
  - after it, the words are an approximation.
- Speed: about 2-4 s a variant.

**Mutations:**
- applied at a branch point of the source's records (a control or motion change), or at a random frame;
- timings shifted by 1..6 ticks;
- chords (grabs, Ubers, single buttons; frame-perfect or sloppy);
- mashes, reversed sticks, released buttons;
- Select resets, L1 / R1 attacks;
- stance-side carves, right-stick presses;
- segments spliced from other runs on the same course.

**Guided families** (local/fuzz/guide.mjs, closed loop):
- How it works:
  - the hook wraps `core._pad_tick` and writes the pad from the live state over a window of script frames;
  - the pad actually played is saved as the variant's open-loop pad, which is what re-runs and ARMSX2 replay.
- The families:
  - `steer`: waypoints onto the teeter rails, with random offsets, gain and jump timing;
  - `railslow`: brake on rails;
  - `grabhit`: a grab held over a whole air;
  - `presshit`: a held board press;
  - `chord`: an Uber chord on a rail.

**Kept:** a variant is kept when it adds a coverage key. Keys come straight from the raw counters of gameplay functions:
(function, counter, count bucket 1 / 2 / 3-4 / 5-8 / 9+). Kept variants join the mutation pool.

**Invariants** (local/fuzz/fuzz-hook.mjs, calibrated on the PS2-exact seeds, local/fuzz/verified.json):
- no NaN / inf in motion, bones or boost;
- speed at most 3400 cm/s (the exact seeds never pass 3333.334);
- dv at most 3500 cm/s a tick, outside resets (exact seeds: 3271);
- boost meter within -0.01..1.01 (the PS2 dips to -0.0014);
- control 0..13;
- no core exception;
- no terrain 0.75..2 m above a grounded rider's root on a floor (normal y > 0.9, not in a crash). The one deep-powder spot the
  exact replays show (CRA3, 1949) is whitelisted;
- control transitions no exact replay shows are flagged as unverified states.

**Targets and the exact-mode queue** (local/fuzz/targets.py, local/fuzz/exact-queue.json):
- A kept variant is checked against the targets above: functions, and the missing side of one-sided branches by line and column.
- Variants that reach a target or an unverified state are queued for exact-mode ARMSX2 capture.

**Differential step** (local/fuzz/diff.py, paused until exact mode lands):
- `build ID` captures a variant on ARMSX2. It uses one instance, silent and niced, and waits while `pgrep -x ARMSX2` shows 4.
- `compare ID` compares as the gate does.
- `chunks` / `revert` minimise a divergence by reverting the variant's pad changes chunk by chunk.

## Rounds

Keys are (function, counter, bucket) for gameplay functions. Regions and branches are the merged gameplay coverage of the baseline
plus every kept variant so far.

| round | variants | kept | keys | regions | branches | violations | new targets | new unverified states |
|---|---|---|---|---|---|---|---|---|
| r1 (core v1, 5 wide) | 300 | 137 | 23134 -> 24232 | - | - | 0 | 2 rail-attach sides | 7>0 7>3 4>3 10>0 2>10 8>3 1>9 5>11 |
| baseline v2 | - | - | 22974 | 83.9% | 76.3% | - | - | - |
| r2 (2 wide) | 300 | 140 | 24402 | 84.2% | 76.6% | 0 | rail-attach 566 (press in reverse stance), 569 (board upside down) | 2>9 4>11 7>8 5>9 8>5 |
| r3teeter (guide only) | 80 | 28 | 24507 | - | - | 0 | - (grinds the 0x1d08 teeter rail) | - |
| r4 | 300 | 105 | 25338 | 84.4% | 76.9% | 0 | collision 129 (fast low 342 / 332), 135, 142 (classes 18 / 19 -> 351), 155 (328 / 329); rail-attach 583 (106D9C press onto a rail, 26); rail-motion 507 | - |
| r5 | 300 | 91 | 25841 | 84.4% | 77.0% | 0 | - | 11>9 |

Coverage has levelled off: r5 added 8 regions and 17 branches. Most of what is left is unreachable from a pad (always-set
callback checks, online and native paths) or needs situations the seeds never reach (a pair crash, control 11 / 9 collisions).

## Findings

- **Port invariants:**
  - 1280 variants in five rounds: 0 core exceptions, 0 NaN / inf, 0 speed or dv cap violations.
  - The ground-sink hits were all false alarms: the CRA3 deep-powder spot that the exact replays show too (whitelisted), one
    single-tick 0.76 m landing dip, and one reading of a computer rider's context. fuzz-hook.mjs now reads the human through its
    own context view in six-rider runs.
- **Unverified port states:** control transitions no PS2-exact replay shows: 7>0 7>3 4>3 10>0 2>10 8>3 1>9 5>11 2>9 4>11 7>8 5>9
  8>5 11>9. They are queued for exact-mode capture.
- **Exact-mode queue** (local/fuzz/exact-queue.json, 22 entries):
  - every target reach and unverified state, each with its seed and variant directory;
  - the r1 entries were re-checked on core v2 (`rechecked`).
- **PS2 divergences handed over:** none yet. The differential step is paused until the exact-mode core swap.
