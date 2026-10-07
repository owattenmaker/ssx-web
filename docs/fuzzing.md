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
- inside the ground: a terrain surface 0.8..2 m above a grounded rider's root on a floor (normal y > 0.9, not in a crash) and no
  floor at its feet (a cast from +0.6 m finds nothing from 0.3 m below to 0.55 m above the root). Walls, lips, overhangs and
  tunnel roofs above a grounded rider are not flagged;
- control transitions no PS2 capture shows are flagged as unverified states. The control is rider_control_info [0], the
  controller 121068 runs, which equals the records' control word on every tick of the exact seeds. reference_motion [11]
  (physicsState.controlState) keeps 0 / 4 through the air controls and disagreed on 114k exact-seed ticks; r1-r8 used it.
  The PS2 set is local/fuzz/ps2-transitions.json (ps2states.py, every capture under runs/ including the fuzz ones).

**Targets and the exact-mode queue** (local/fuzz/targets.py, local/fuzz/exact-queue.json):
- A kept variant is checked against the targets above: functions, and the missing side of one-sided branches by line and column.
- Variants that reach a target or an unverified state are queued for exact-mode ARMSX2 capture.

**Differential step** (local/fuzz/diff.py):
- Captures are built from the seed's baseline, pokes and isolation, with the variant's whole pad. One ARMSX2 instance runs at a
  time, silent and niced (turbo, with a normal-speed rerun on a ring overrun). The build waits while `pgrep -x ARMSX2` shows 4.
- `FUZZ_FPU=mode1` puts them in runs/fuzz-mode1; the default `exact` puts them in runs/fuzz-exact.
  - **Mode 1 is the decisive reference.** The port is bit-exact with ARMSX2 mode 1, so a mode-1 capture's first divergence is
    logic. This is the one exception to exact-mode captures (coordinator, 2026-10-05).
  - Exact-mode captures drift from the port within 1-340 ticks. They only answer whether the PS2 can reach a state at all.
- **Event seeds** (races, rival time): captured with --ai-state and compared with compare-ai-capture.mjs --zoe --world-draws
  (--isolate when isolated). An "isolated" event capture still runs the computer riders on the PS2: the Rival Time rival fires
  stage triggers (r1-0066, traced by the physics agent), and the human-only comparer never runs them. Watches are dropped when
  --ai-state is added to a seed captured without it.
- Commands:
  - `build ID`, `compare ID`;
  - `trace ID`: the PS2's and the port's control transitions;
  - `batch N`: N kept variants, target reaches first;
  - `minimise ID`: greedy. It reverts each pad-change chunk before the divergence, latest first, and keeps a revert when the
    capture still diverges with the same signature. The signature is (PS2 control, port control) for a human-only compare; for
    six riders, the earliest of human, RNG and each computer rider.
  - `chunks` / `revert`.

## Rounds

Keys are (function, counter, bucket) for gameplay functions. Regions and branches are the merged gameplay coverage of the baseline
plus every kept variant so far.

| round | variants | kept | keys | regions | branches | violations | new targets | new unverified states |
|---|---|---|---|---|---|---|---|---|
| r1 (core v1, 5 wide) | 300 | 137 | 23134 -> 24232 | - | - | 0 | 2 rail-attach sides | 7>0 7>3 4>3 10>0 2>10 8>3 1>9 5>11 |
| baseline v2 | - | - | 22974 | 83.9% | 76.3% | - | - | - |
| r2 (2 wide) | 300 | 140 | 24402 | 84.2% | 76.6% | 0 | rail-attach 566 (press in reverse stance), 569 (board upside down) | 2>9 4>11 7>8 5>9 8>5 |
| r3teeter (guide only) | 80 | 28 | 24507 | - | - | 0 | - (grinds the 0x1d08 teeter rail) | - |
| r4 | 300 | 105 | 25338 | 84.4% | 76.9% | 0 | collision 129 (342 / 332), 135, 142 (351), 155 (328 / 329); rail-attach 583 (106D9C); rail-motion 507 | - |
| r5 | 300 | 91 | 25841 | 84.4% | 77.0% | 0 | - | 11>9 |
| r6-r8 (3 wide) | 900 | 189 | 26725 | - | - | 0 (sink false alarms fixed) | collision 150 (classes 20 / 21 -> 361), rail-attach 567 / 568 / 591 | 5>3 12>7 (PS2 has both) |
| r9 | 300 | 62 | 26998 | - | - | 0 | pair-react 317 / 319 (a pair contact in a board press, hl/hl-ai-9) | 2>0 |
| r10 | 300 | 54 | 27193 | - | - | 0 | - | 2>8 9>8 12>8 |
| r11-r15 | 1500 | 193 | 27921 | - | - | 0 real (Allegra 3450 cm/s and a 3559 cm/s crash stop are the PS2's own, mode 1) | - | 10>5 5>10 10>4 4>10 (finishes) |

Coverage has levelled off: r5 added 8 regions and 17 branches. Most of what is left is unreachable from a pad (always-set
callback checks, online and native paths) or needs situations the seeds never reach (a pair crash, control 11 / 9 collisions).

## Findings

- **Port invariants:** about 4600 variants in sixteen rounds.
  - 0 core exceptions, 0 NaN / inf, 0 speed or dv cap violations.
  - Every ground-sink hit was a false alarm. They led to the floor check above, and to reading the human through its own
    context view in six-rider runs.
- **Unverified states:** every port transition flagged so far also appears on the PS2 in mode 1 at the same tick (7>3, 1>9,
  2>9, 4>11, 7>8, 5>9, 8>5, 11>9, 5>3, 12>7, 2>0, 2>8, 12>8), or in the exact captures (2>10, 8>3). No control-state bug came from
  this check; the divergences came from the mode-1 differential.
- **Mode-1 divergences handed to the physics agent** (minimised; status on the latest scratch core rechecked):

  | variant | what | status |
  |---|---|---|
  | r1-0066 (throne) | the human hits EBC3 fallingRocks_1003 at 3586 | harness gap (the rival fires the trigger); withdrawn |
  | r5-0165-min (Junction) | a Select reset from a handplant keeps +0x1E0 | fixed (core32) |
  | r4-0005-m8 (Snow Jam) | landing with an air board press pending: a crash instead of control 1 | fixed (core34) |
  | r7-0053 (press-ice-rnb) | the rail-release velocity 1-2 ULP; a soft exit to control 0 in the air | fixed (core35); the RNG at 3690 remains |
  | r3teeter-0029 (Snow Jam) | the control 3 -> 0 -> 4 command words | fixed (core35) |
  | r10-0198 (dra4) | an air-to-reset ran that tick's post contacts | fixed (core37) |
  | r2-0204 / r6-0103 (gravitude) | Mac's pose 2 cm off, then his 2585 landing normal | pose fixed (core38); the human's 2664 rail crash open |
  | gravitude seed | Luther's pose 1 cm off from 2403, physics exact | fixed (core38) |
  | r2-0249, r12-0064, r14-0154, r3teeter-0011 | RNG, human, crash stop, passive-to-air bones | exact on core40 |
  | r4-0048, r7-0028, r7-0082, r10-0134 | the shared RNG differs hundreds of ticks after a human pad change, riders exact | open (core40) |
  | r7-0038 (cra3) | Allegra 2096 fixed; Mac at 4998 | open (core40) |
  | r10-0153 (Snow Jam) | a forced ground reset placed 15.5 cm off | open (core40) |
  | r1-0203 (hl-glide-6) | bones after a Select reset from a board press | open (core40) |
  | r13-0191 (hl-glide-5) | a handplant -> passive air -> landing, 0.016 cm | open (core40) |
  | r7-0036 (bf-score-rail-uber) | bones 3 / 4 after a rail Uber (12) to rail (7) | open (core40) |
  | r6-0273, r8-0022 (dra4) | Luther, Mac | past dra4's known limit (RNG 3772); deprioritised |
- **Computer-rider poses:**
  - **The gap:** --ai-state recorded no computer-rider bones, which feed physics (13A7B0's probe, the body queries).
  - **Now:** `build --ai-bones` and compare-ai-capture.mjs firstBoneInexact cover it (docs/ai-racers.md "Verification");
    diff.py records them with FUZZ_AI_BONES=1.
  - **Gate captures:** gravitude, dra4 and cra3 are re-captured with --ai-bones (mode 1) in local/ps2-capture/runs/aibones
    (aibones-capture.py).
  - **On core40:** gravitude and cra3 are bone-exact for every rider to the end. In dra4, Psymon's bones split at 3772, one tick
    before the gate's known RNG split.
- **2026-10-07, on core-batch11:**
  - Spline constructor (builtin 19, 0x35955C) draws from a trigger are missing in the port: r13-0063 (ASS1, a computer rider's
    contact) and r4-0236 (CRA3, the human's reset).
  - Others sent: r7-0000 (Allegra's rail attach after the human's line), r1-0204 (control 3 kept into the air), r8-0165 (a
    false hit on mdl_ASS1_railADD_panel_23), r5-0194 / r5-0160 (handplant landing; crash slide), r4-0279 (an Allegra-Mac pair
    contact).
  - The DSS2 style-mile countdown seeds' own six-rider run has Moby off around 2530-2632 (press-ice-seed-six). These seeds
    have only human gates.
- **Stopping point (2026-10-07):** mode-1 batch 9 (28 variants) found no new divergence; its three hits are the known seed limits
  (kick-doubt's finish, DSS2 Moby 2632; now in diff.py KNOWN_SIX_LIMITS). The fuzz rounds' coverage keys have flattened (+102 in
  r16). Both stop conditions of the brief are met; further batches only after core fixes land (recheck.py first).
- **Rechecks:** local/fuzz/recheck.py CORE_JS runs every mode-1 capture again (event seeds six-rider, against each seed's own
  six-rider gate limit, diff.py six_limit). Results are in recheck-<core>.json.
- **Exact-mode queue:** local/fuzz/exact-queue.json. Every entry is captured in exact mode (runs/fuzz-exact); the state entries
  are in mode 1 too.
