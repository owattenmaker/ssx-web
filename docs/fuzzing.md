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

### Baseline (all 465 ps2-captures scenarios, coverage core v1)

| area | functions | regions | branches |
|---|---|---|---|
| air | 99.0% | 95.3% | 90.1% |
| landing | 100% | 98.6% | 93.8% |
| press | 96.5% | 94.7% | 87.7% |
| crash/reset | 96.5% | 89.8% | 81.1% |
| plant | 96.2% | 89.4% | 74.6% |
| scoring | 94.3% | 89.7% | 82.7% |
| npc/race | 93.3% | 88.2% | 80.0% |
| uber/tricks | 85.2% | 86.5% | 80.9% |
| pair/attack/collision | 86.0% | 81.7% | 73.1% |
| animation | 75.8% | 81.0% | 74.7% |
| rail | 81.4% | 80.2% | 71.6% |
| ground/core | 77.3% | 79.8% | 72.2% |
| **gameplay** | **84.4%** (1787/2117) | **83.7%** (16955/20254) | **76.1%** (11684/15352) |

Many one-sided branches are compound conditions on callbacks that are always set, which no pad can flip.

**Reachable code never run:**
- originalAnimTeeterApplyForce (342538, through 106848's attach force on the ARA1 log teeters);
- originalRailUberBalanceStep (the kind-10 rail Uber balance state);
- originalNpcDesignatedBehavior (100B90).

**Branches never taken:**
- originalCollisionReaction:
  - the surface reset;
  - the control 9 / 11 returns;
  - classes 18..21 picking 351 / 361;
  - the fast low contact 342 / 332.
- originalRailMotionStep:
  - 13B0A4: below 555 cm/s;
  - 13BB14: the push-away;
  - the up.z < 0 steer;
  - 13B6EC: the unclamped pull.
- pair_react: a kind-2 pair crash, and a pair contact during a press.

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

| round | variants | kept | keys | violations | targets reached | unverified states |
|---|---|---|---|---|---|---|
| r1 (core v1) | 300 | 137 | 23134 -> 24232 | 0 real (CRA3 powder sinks, whitelisted) | 2 rail-attach branch sides | 7>0 7>3 4>3 10>0 2>10 8>3 1>9 5>11 |

## Findings

- No port invariant violation so far: 0 core exceptions, 0 NaN, 0 cap violations.
- No PS2 divergence handed over yet: the differential step waits for exact mode.
