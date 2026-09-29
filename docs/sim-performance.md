# Simulation CPU cost (WASM core): profile, exact speed-ups, presentation fast mode

2026-09-25. Goal: less CPU per 60 Hz tick of the six-rider core (docs/ai-racers.md "One core, six riders"), above all
on phones, **without changing a single simulated bit**. Everything below except "Phase 3" is exact: the core computes
the same words as before for every export, test and capture. Phase 3 is an opt-in presentation mode (web/quality.js
low tier) that changes only pixels that nothing in the simulation reads.

Numbers: Mac (M-series) under heavy load from other agents (load average 15-50), so every comparison is interleaved or
paired in one process; absolute values move ±20% between runs, the ratios much less.

## Results

| scenario | before | after | change |
| --- | --- | --- | --- |
| node, 6-rider Snow Jam, phone-like reads (4 ticks per drawn frame, palettes on the last two), CPU ms/tick, paired | 4.73 | 1.85 exact / 1.52 fast | 0.38x / 0.32x |
| node, desktop-like reads (every tick drawn, all palettes every tick), CPU ms/tick, paired | 5.81 | 2.85 exact | 0.49x |
| Chrome, 390x844 mobile emulation, 4x CPU throttle (low tier), simulation ms per tick (CPU profile) | 20.7-23.7 | 7.7-10.0 fast / 9.0-10.7 exact | ~0.40x |
| same, frames per second (3 interleaved rounds each) | 7.0-8.1 | 11.8-14.4 fast / 11.2-13.0 exact | +70% |
| same, game ticks per second (60 = real time; the low tier caps a frame at 4 ticks) | 28-32 | 47-55 | |
| same, simulation share of the frame | 63-65% | 41-45% | |
| Chrome desktop 1280x800, no throttle (high tier, exact), simulation ms per tick | 5.1-6.3 | 2.35-2.42 | 0.42x |
| same, frame callback p50 (120 Hz display) / fps | 11.6-15.1 ms / 64-89 | 8.8-9.7 ms / 102-107 | |

Re-measured on the final tree (web/ as landed, 15:14-15:31, load average 11-20, lower than above). Chrome baseline =
the baseline core + the old ai-racers.js / main.js / quality.js, 2 interleaved rounds each; node: both cores driven by
the new ai-racers.js in one process (bench-sim imports web/'s), so the JS glue gain is not in the node ratio.

| scenario | before | after | change |
| --- | --- | --- | --- |
| node, `web/bench-sim.mjs --ticks 2000` (phone-like reads), CPU ms/tick | 4.45 | 1.67 exact / 1.38 fast | 0.373x / 0.305x |
| node, `--read-fx 1` (desktop-like reads) | 4.68 | 2.45 | 0.525x |
| Chrome 390x844, 4x throttle, low tier: simulation ms/tick | 17.0-18.0 | 6.3-7.1 fast / 7.2-8.2 exact | 0.38x / 0.44x |
| same, fps | 9.0-9.6 | 16.4-19.7 fast / 13.9-16.4 exact | +90% / +60% |
| same, game ticks per second / simulation share of the frame | 36-39 / 63-64% | 58-60 / 36-39% fast; 54-59 / 41-43% exact | |
| Chrome desktop 1280x800, high tier: simulation ms/tick | 4.38-4.40 | 2.01-2.17 | 0.48x |
| same, frame callback p50 / fps (120 Hz display) | 9.9-10.4 ms / 103-104 | 7.1-8.6 ms / 115-122 | |

Per change (node, phone-like reads, one process, 2000 ticks each, median of paired 50-tick chunks, ratio to the
baseline core):

| # | change | cumulative ratio | step |
| --- | --- | --- | --- |
| 0 | baseline (web/build-core.sh as of 12:52) | 1.000 | |
| 1 | fast exact toward-zero primitives (engine/software_float.hpp, original_float.hpp, terrain_contact_math.hpp) | 0.671 | -33% |
| 2 | snow sprites, board-trail and wake vertices built when read, not every tick (web/animation_bridge.cpp) | 0.541 | -19% |
| 3 | one packet decode per (layer, part) in the local pose (engine/animation_motion.cpp); skin palette drops acc x 1 (engine/skin_palette.hpp) | 0.488 | -10% |
| 4 | set-piece instance lookup by index instead of a 3500-instance scan (web/set_piece_gameplay.inc) | 0.451 | -8% |
| 5 | body query scans a compact copy of the terrain bounds (engine/world_body_collision.hpp) | 0.408 | -10% |
| 6 | instance loops skip type-0 instances through an index; the ray segment query scans a compact bounds copy (world_body_collision.hpp, air_trajectory_world.hpp, collision.hpp) | 0.392 | -4% |
| 7 | presentation fast-mode plumbing, off (web/presentation_fast.hpp) | 0.379 | noise |
| 8 | JS glue: cached visual-RNG views, pair views reused within a dispatch (web/ai-racers.js) | ~0.37 | -1..2% in node (rider-context switches 1060 -> 520 per tick) |
| 9 | **presentation fast mode on** (low tier only) | 0.318 | -16% |

Step 2 is worth nothing when every tick is drawn (desktop at 60 fps) and ~15% of the tick at 4 ticks per frame.

## Phase 1: where the time went (before)

Profiling build: web/build-core.sh's em++ line plus `--profiling-funcs` (same -O3; identical traces). Node:
`node --cpu-prof` over 3000 ticks of the six-rider race; Chrome: CDP profiler during a 20 s window of the same race in
mobile emulation (Chrome for Testing, 390x844, DPR 3, 4x CPU throttle, tuck held). **`web/bench-sim.mjs`** is the
repeatable node benchmark (one or more core builds, paired chunks, `:fast`, `--read-fx`); the Chrome and profile
scripts stayed in the session scratchpad.

Chrome, 4x throttle, low tier, share of `simTick` (the simulation was 64-68% of the frame):

| where | share |
| --- | --- |
| exact-float helpers, self time | **30.7%** (terrain_original::mul 13.0, ::add 9.4, originalScalarAddSub 5.5, ::sub 2.5) |
| other wasm | 55.7% |
| JS glue (ai-racers.js views/TLS switches, typed-array views, audio tick, terrain detail) | 13.6% |

By entry point (inclusive): computer riders' FX pass 20.8% (snow 15.8%, of which `OriginalSnowParticles::particles()`
13.9%: the sprite positions recomputed from the birth rings every tick for every rider; board trail 4.2%), their
`animation_pose` 18.0%, `rider_skin_palette` captures 10.4%, their `step_rider` 7.8%, their `race_begin` 6.5%
(`advance_world_entities`: `refresh_chairlift` 4.8% and spline pieces, both mostly a linear scan of the 3496 world
instances per lookup, per piece, per rider context), `animation_post` 5.7%; the human's `animation_tick` 5.7%.

Exact-float calls per tick (instrumented build, Snow Jam, 6 riders): software add 152 k, mul 188 k (terrain_original
add 99 k, sub 19 k, mul 188 k on top of them), EE scalar add/sub (guard bit) 35 k, rounding scopes 22 k, div/sqrt ~1-2 k.
Biggest callers: computer riders' FX pass (51 k adds, 66 k muls: the snow sprites), their pose (45 k / 37 k), palettes.

Other hot spots found on the way: `WorldBodyCollision::query` (per query a linear scan of 2238 terrain patches of
~1.3 KB each and 3496 instances; ~11 queries per tick, in six per-rider copies: 49 k struct visits per tick, almost all
rejected, i.e. cache misses), `AnimationPacket::sample` (a packet decoded once per bone although it serves the whole
part), `rider_skin_matrices`, `sourceTerrainSegment`, `teeterQuery` (rail records scan).

## Phase 2: exact changes

### Fast exact primitives (engine/software_float.hpp)

The EE semantics kept: MUL.S / ADD.S / SUB.S round toward zero, operands and results flushed (DAZ/FTZ), overflow to
FLT_MAX, the EE ADD's one guard bit (originalScalarAddSub's operand masking, unchanged). New formulation:

- **mul**: a binary32 x binary32 product is exact in binary64. If its exponent is in the normal binary32 range
  ([2^-126, 2^128)), clearing the low 29 fraction bits truncates it to 24 bits = the toward-zero binary32 value, exactly
  representable, so the conversion does not round. Exact zero returns directly. Everything else (tiny, huge, inf, NaN)
  goes to `mulReference`, the previous code verbatim.
- **add/sub**: the binary64 sum s = RN(a+b). If s is in the normal range and its low 29 bits are not all zero, the exact
  sum lies within half a binary64 ulp of s, strictly between the same two 24-bit neighbours, so truncating s is the
  result. If the low bits are zero, the FastTwoSum residual (exact for |x| >= |y|) tells an exact sum (return s) from a
  rounded one (reference path). Exact zero returns directly. Everything else: `addReference` (the previous code).
- The toward-zero branch of `originalScalarAddSub` no longer goes through `volatile` operands (a stack frame per call);
  the nearest branch keeps them in a separate function (they fix the operand order, which decides which NaN an add of
  two NaNs propagates). `terrain_original::add/sub/mul/div/sqrt` lose their `volatile` result under Emscripten (wasm
  f32 arithmetic has no excess precision; `-ffp-contract=off` stays). `originalScalarDivide/Sqrt` drop the
  `fegetround/fesetround(FE_TONEAREST)` round trip under Emscripten (musl's wasm fenv only has nearest: a no-op). The
  native build keeps every volatile and fesetround.

Evidence (**`tools/test_exact_float.sh`** re-runs the fast-vs-reference and FPU checks: with its defaults 8.1 billion
native and 4.7 billion WebAssembly results, 0 mismatches; the one-off comparisons against the previous headers were
run from the session scratchpad):
- native arm64, new vs previous implementation (the previous headers renamed into another namespace): 300 M random pairs
  per operation from six generators (uniform bit patterns incl. NaN/inf/denormals, near exponents with structured
  mantissas, cancellation b ~ -a, products at the underflow/overflow thresholds, far exponents with inexact binary64
  sums, few-bit mantissas) + all pairs of 262 special values, and **exhaustive** sweeps: every one of the 2^32 values
  against 12 fixed operands in both orders for add, sub and mul (309 billion results): **0 mismatches** (div, untouched,
  shows NaN-payload-only differences between two compilations of the same source; add/sub/mul none);
- WebAssembly (em++ -O3, node), toward-zero and nearest rider policies: sf add/sub/mul/div/sqrt, originalScalarAdd/
  Subtract/Divide/Sqrt, terrain_original add/sub/mul/div/sqrt: 3.4 billion cases, **0 mismatches, 0 NaN-payload
  differences**;
- against the hardware itself: arm64 FPU with FPCR RMode = toward zero and FZ (flush inputs/outputs) on: 240 M results
  (add/sub/mul): **0 mismatches** (NaN payloads aside);
- `mul(x, 1) == x` for every non-denormal x (exhaustive, native and wasm), which licenses dropping the palette's acc x 1;
- re-run on the final headers (engine/ as landed) against the pre-change headers: native 600 M results (add/sub/mul/
  div/sqrt, specials x specials + the six generators), WebAssembly 843 M results (all 14 helpers, both rider rounding
  policies): 0 mismatches, 0 NaN-payload differences in wasm (native div: NaN payloads only, untouched source).

Gain: -33% of the tick.

### Presentation outputs built when read (web/animation_bridge.cpp)

The snow sprite buffers (`snow_info`/`snow_particles`), board-trail ribbon/roof (`trail_info`/`trail_ribbon`/
`trail_roof`) and wake vertices (`wake_info`/`wake_vertices`) are outputs for the renderers only; nothing in the core
reads them. `update_snow`/`update_trail` now only mark them stale (the snow copy keeps that tick's enabled-emitter
flags); the exports build them on first read from the same births/bands/rows (nothing changes those between the tick and
the read; a reset clears them). A drawn frame reads exactly what the last tick would have written. With 4 ticks per
frame, 3 of 4 builds disappear; riders' buffers are still built every drawn frame (opponent-fx.js reads all).
All the RNG draws (emission, trail LCG, grind chunk) stay in the tick, in order.

### Other exact changes

- `originalAnimationLocalPose`: `AnimationClip::sample(part, time)` decodes every channel of the part's packet and is
  pure; the pose now samples once per (layer, part) and every bone of that part reads its channels from it.
- `originalSkinPalette`: `add(mul(acc, 1), value)` -> `add(acc, value)` (acc is already a flushed toward-zero result).
- `set_piece_instance_at`: a (resource, first index) table sorted by resource, rebuilt when the instance list is another
  one (object, buffer, size, first/last resource) and merged when it only grew; every hit is checked against the list,
  misses fall back to the scan.
- `WorldBodyCollision::query`: the terrain loop scans a compact copy of (bounds, resource, flags) (rebuilt when the
  list's buffer or size or one of eight sampled entries differs; patches are never changed in place), then handles the
  passing patches on the list in list order. Instance loops in `query` and `queryOriginalWorldSegment` walk an index
  of the type != 0 instances (a type-0 instance is skipped whatever its other fields; types never change after the
  load). `CollisionWorld::sourceTerrainSegment` scans a compact copy of the ordered query boxes built in
  `orderPatches` (the only place the order changes).
- web/ai-racers.js: the visual-RNG word views are cached per rider context (were two exports and two typed arrays per
  rider per FX phase); within one rider-pair dispatch a slot's `pair_view` is reused until a callback moves, pushes or
  reacts a rider.
- web/main.js: the human's skin palette is captured on the last two ticks of a drawn frame (as the computer riders'
  already were; a reset stays pending until the next capture, a frame whose last ticks did not run captures after the
  clock).

Tried and dropped (no measurable gain): `always_inline` on originalScalarAddSub, `-msimd128` for the whole core (+3%
within noise; exact), hoisting the rail segment box test out of `originalRailSegmentQuery`.

### Exactness evidence for the whole core

- Per-tick trace of 3000-tick six-rider races (tuck and scripted pads: carving, jumps, spins, grabs), every rider:
  world state, progress, game and visual RNG words, visual LCG, posed bones, score object, animation/pose/crash/boost/
  reset records, all snow sprite buffers, trail ribbon/roof, wake vertices, impact FX, boost ribbons, skin palettes,
  camera view: **identical** to the baseline core built from the same source snapshot, for every build in the table;
  with buffers read only every 4th tick: identical at every read. The final build (web/runtime, 14:31) with the new
  JS glue against the baseline build of the same snapshot with the old JS glue: identical traces (tuck and scripted).
- Full `npm test` command list (141 commands), final build + new JS vs baseline build + old JS in mirrored web/ trees,
  and then again on the live tree (web/ as is, 15:00-15:20, other agents' edits included): same exit codes and the
  same output apart from timings and the TLS block size (110024 -> 110216 B: +192 B of new per-rider state).
  test-ps2-captures: 215 scenarios checked, output identical to the baseline's. test-rider-contexts, test-ai-racers,
  test-lineups (28 Snow Jam + 21 Metro-City + 8 Peak 2 lineups), test-long-runs, test-stage-world (visual-pass
  trace), test-mp-plausibility, test-snow*, test-board-trail, test-wake, test-mp-fx: pass, same output.
  Failing in both trees, unrelated to the core: test-slopestyle-bigair (a Quick Play roster assertion, JS only).
  Flaky in both: test-mp (`timeout waiting for lobby` with its 5 s limit under load; the baseline mirror failed 3 of
  4 re-runs, the live tree passed 2 of 3; it does not load the core). test-edge-worker failed once in the live run
  (deploy/edge-worker.js was being edited) and passes on re-run.
- Native (the engine headers are shared with the Metal build): `ctest` 25/25 pass on the live sources
  (RelWithDebInfo, built outside the repo); tests/software_float_reference.cpp + software_float_wasm.cpp (the
  tools/test_software_float.sh check) give the same output for the baseline and the new headers.
- web/check-rider-globals.mjs passes: all new core state is RIDER_LOCAL or a member of a per-rider heap object.

## Phase 3: presentation fast mode (low tier)

`web/quality.js` tiers get `presentationFast` (low: on; medium/high: off; `?fastfx=0|1` overrides). main.js sets it on
every rider context each frame (`set_presentation_fast`, RIDER_LOCAL flag, default off: every test and the desktop
stay exact). With it on, two pure outputs use plain binary32 arithmetic (web/presentation_fast.hpp), same formulas and
order:
- the skin palette (310120 pose matrix, 3106CC bind product, 386BD0 weighted sum), read only by rider-skinning.js;
- the snow sprites (VU program 439A40), read only by snow-renderer.js.

Visual RNG 0x4FF018 (and the gp+0xA0C LCG) consumers in the core, established from the source: snow emitter births
(`OriginalSnowParticles::emit`, one draw per active enabled emitter birth), the grind-chunk emitter
(`update_board_sparks`), camera shake (`browser_camera_random` -> original_camera 0x1656B0/0x165938), the stage world
(`stage_visual_random`: DynamicParticle emitters, emitter seeds, MeshAnim contact programs, flag grid, the post-rider
pass), the online FX puppet (records/restores the words); the LCG: board trail, sparks, fist sparkle, snow emission.
None of them is touched: every draw still happens in its tick, in order. The fast mode only changes how the two
outputs are evaluated from state the tick has finished writing (the sprite LFSR is local and integer; the palette reads
the cached pose), and it writes nothing but their output buffers.

Evidence:
- simulation identical with the mode on vs off: 6000-tick six-rider races (tuck and scripted), per-tick hash of every
  rider's simulation state (world state, progress, game/visual RNG words, LCG, posed bones, score, animation/pose/
  crash/boost/reset records, camera view), palettes and sprites read every tick: **identical**;
- visual difference (lockstep races, 3000 ticks, 18000 palettes, 3.05 M sprites): palette translations <= 0.125 cm
  (1-2 ulp at course coordinates), rotation/scale terms <= 6e-7; sprites: the same count every tick, positions
  <= 0.125 cm, half extent <= 8e-5 cm, colour <= 1/128 (one GS level on some sprites);
- screenshots of the same tick (Chrome mobile emulation, low tier, race frozen before the countdown with the clock cap
  at 0, `ssxQA.advance(900)` with tuck held, HUD hidden), fast off vs on: the riders stand on the same pixels; 1.2% of
  the pixels differ, 9 by more than 16/255 (max 61), along sprite and rider edges.
  local/browser-validation/sim-performance/t900-fast0.png, t900-fast1.png, t900-side.png, t900-diff-x8.png (x8).

Gain: -16% of the tick at phone pacing (0.379 -> 0.318 of baseline).

## Phase 4: the simulation in a Worker (built, then scrapped)

> 2026-09-26: built, measured and removed. The worker was exact, but it gave 1.37x on a 4x-throttled phone model for +167 MB of memory and one frame of input latency, and desktop gained no frames. The user decided the game already runs well enough on a phone. See docs/workers.md "Simulation worker: scrapped". The plan below is kept as history only.

After phases 2-3 the simulation is 36-45% of a 4x-throttled phone frame (was 63-65%); the rest (three.js encode, the
fog/post chain, UI) is now the larger part. A Worker would overlap the two: a frame costs about max(render, simulation)
instead of their sum, i.e. roughly 1.6-1.8x the frame rate on a phone with two or more performance cores (desktop:
-2.4 ms of main-thread work per tick). Not done here: main.js reads the core directly in simTick and frame() (~50
exports) and it is being edited by other agents; the lazily built presentation buffers (snow_info, trail_info,
wake_info, rider_skin_palette) must also be *called* in the core's thread. Plan (snapshots; no cross-origin isolation
needed):

1. `web/sim-worker.js` owns createCore(), the rider contexts (ai-racers.js as is), the FixedStepClock, simTick's
   per-tick work (pad in; race/HUD/audio/rumble events out) and, after a frame's ticks, the presentation reads.
   main -> worker per frame: {dt, the pad (24 floats), quality (maxFrameDt, presentationFast), pause/start/reset and
   UI commands}; worker -> main: one transferable frame record plus event lists.
2. Frame record: the human's state(16), animation info(19), pose_physical(12), rider frames (previous/current bones),
   camera poses (previous/current), race info(8), HUD pre-pass and trick HUD slots/name; renderer inputs: snow info +
   buffers + flipbook, trail ribbon/roof, wake vertices, boost strips, impact/startfire sprites, the palettes
   [previous, current] of each drawn rider, rider-lighting coefficients, AI world_pose_bones (icons, beam, shadows), fog
   info/palette, pickups visible, moving instances, set-piece/stage-world logs (sections, flags, crowd, halos), audio
   events/telemetry, standings. About 0.3-0.5 MB per frame, transferred without copies from a pool of three buffers.
3. main.js: simTick and simulation.advance move to the worker; frame() sends the input, draws the latest record
   (interpolation alpha from the worker's pending time) and the HUD from it; every renderer's `update(core)` takes the
   record instead (board-trail, wake, snow, boost, impact, startfire, opponent-riders/fx, rider-shadow, set pieces,
   progress meter, game-audio). One frame of input latency unless the worker runs frame N's ticks while main draws
   N-1 (pipelining); acceptable on the low tier.
4. Loading (init_*), QA hooks (ssxQA.advance) and online races (mp-game.js pace/beginTick/endTick) as worker RPCs.
5. Shared memory (-sSHARED_MEMORY, COOP/COEP in web/server/mp-server.mjs, the Vite config and deploy/edge-worker.js)
   would avoid the record copies but still needs the materialising exports called in the worker; the record is simpler.

Estimated 2-4 days, almost all main.js; best done when main.js is not being changed by other agents.

## Files

engine/software_float.hpp, engine/original_float.hpp, engine/terrain_contact_math.hpp, engine/animation_motion.cpp,
engine/skin_palette.hpp, engine/world_body_collision.hpp, engine/air_trajectory_world.hpp, engine/collision.hpp,
web/animation_bridge.cpp, web/set_piece_gameplay.inc, web/presentation_fast.hpp (new), web/build-core.sh (export
`_set_presentation_fast`), web/ai-racers.js, web/main.js, web/quality.js; tools: web/bench-sim.mjs (new),
tools/test_exact_float.sh + tests/exact_float_fast.cpp (new).

Remaining hot spots (node profile after the changes, phone pacing): `WorldBodyCollision::query`'s instance loop (the
instances are mutable at run time, so no compact copy; ~7%), the skin palette on the exact tiers (~7%), the exact
quaternion/pose math of the computer riders' `animation_pose` (LocalPose + Compose ~8%), `originalScalarAddSub`
(~5%), `teeterQuery`'s rail-record scan (~3%).

Not measured: Safari (the target browser) and a real phone. safaridriver could not open a session (Safari > Develop >
Allow Remote Automation is off); JavaScriptCore's wasm tiers may weigh the f64 bit tricks of the fast primitives
differently from V8, so the Safari gain should be checked (`?qa=1`, time `ssxQA.advance(600)` in a race, or the Web
Inspector timeline) before quoting it.


## Follow-up: renderer CPU and uploads (2026-09-27)

See [web-render-performance.md](web-render-performance.md) for the presentation pass: cached static transforms, parity-driven flag uploads, active-prefix trail uploads, and reused moving-scenery/shadow buffers. No core arithmetic changes. The paired Chrome scene-matrix benchmark reduced that operation by about 75%, with exact matrix equality; this is not a whole-frame or FPS claim.

## Free-roam steady state (2026-09-27)

Owen: "free roam between zones is way worse than an event". Goal: find what free roam (Conquer the Mountain, the streamed Peak 1
world) costs per frame that an event on the same ground does not, fix the biggest costs with exact pixels and an untouched
simulation, and hand the streaming hitches to the CTM-stream agent (free-ride.js streaming is theirs).

### Method

- **`web/bench-free-roam.mjs`** (QA tool, not in npm test, not imported by the game): headless Chrome (`--mute-audio`,
  web/headless-chrome.mjs, optional phone emulation 390x844 DPR 3 + `quality=low` + CPU throttling) or the system WebKit
  (web/webkit-driver.mjs), `?mute=1`, a private Vite server over web/. An autopilot rides the AIP race paths (virtual gamepad).
  Scenarios: `hub` = Peak 1 free ride from Green Base Station down A_ARA1 into ARA1 (the hub row A + 5 connectors drawn, then
  A_ARA1 alone between its Unload and Load triggers, then the ARA1 row; `--summary DIR --by vis` splits by the drawn set);
  `zone` = free ride on ARA1 (Snow Jam's row); `ev` / `evnoai` = the Snow Jam race with / without computer riders. Free ride runs
  with streamWarm, streamGate, streamAhead, ctmWorldAudio on so loading stays out of the steady state; `--pv` adds switches,
  `--profile 1` saves a CPU profile.
- **`web/perf-probe.js`** (injected by the bench): per drawn frame the frame interval, rAF callback ms, simulation ms
  (FixedStepClock.advance) and ticks, every core export's exclusive ms (inside / outside the ticks), renderer.render ms, free-ride /
  set-piece update ms, GPU counters (passes, pipeline switches, bind groups, draws, triangles, buffer bytes written), GPU time from
  WebGPU timestamp queries (Chrome and WebKit both expose them), draws per location, a FinalizationRegistry GC probe, long tasks.
- The Mac was shared with other agents (load average 10-22): comparisons are interleaved rounds, and render JS ms per frame is the
  stable measure (frame intervals move with the load).

### Before (baseline, stream switches on)

Frame interval p50 / p95 / p99 ms; cb = rAF callback ms; render = renderer.render JS ms; draws / pipeline switches per frame.

| scenario | Chrome phone 4x | cb / sim / render | draws / switches | WebKit desktop | Chrome desktop |
| --- | --- | --- | --- | --- | --- |
| event, 6 riders | 77 / 101 / 114 | 75 / 36 / 30 | 525 / 303 | 20 / 36 / 49, cb 18 | 60 fps, cb 14 |
| event, ai=0 | 55 / 71 / 83 | 51 / 11 / 31 | 442 / 320 | 17 / 22 / 28, cb 7 | 60 fps, cb 8.5 |
| zone ARA1 (free roam) | 51 / 69 / 102 | 46 / 10 / 27 | 440 / 322 | 16 / 25 / 35, cb 11.5 | 60 fps, cb 9 |
| hub, A row drawn | 50 / 73 / 96 | 44 / 9 / 27 | 830 / 724 | 18 / 24 / 31, cb 11 | 60 fps, cb 10 |
| A_ARA1 only (after Unload) | 45 / 64 / 86 | 36 / 9 / 16 (+ fr.update 7.8) | 337 / 278 | 17 / 27 / 32, cb 7.5 | 60 fps, cb 8 |
| hub, production defaults | 58 / 89 / 99 | 57 / 14 / 31 (+ fr.update 8.1) | 850 / 742 | 18 / 30 / 35, cb 13 | |

GPU time 1.5-7 ms per frame everywhere (WebKit desktop 4-9 ms, p95 up to 17 ms in the ARA1 row): not GPU-bound. GC:
Chrome `(garbage collector)` 0.5-0.6 % of the profiles, no long tasks outside streaming.

### What differs from an event (ranked)

1. **Streaming work on the connector (handed to the CTM-stream agent):** with streamGate on, the A_ARA1 Load trigger held the
   game until ARA1's draw package (3442 batches) was built: 20 s frozen at phone 4x, ~10 s in WebKit desktop; the build starts
   when the rider enters the connector and yields one ~4 ms slice per frame. Meanwhile fr.update 7-18 ms per frame (collision
   feed); production defaults feed the rest of the peak while riding (fr.update 2-8 ms). The remaining riding-build long frames
   are single node-material builds (40-55 ms each at 4x): see sharedWorldMaterials below.
2. **The hub row draws ~2x an event's objects** (A + 5 connectors: ~830-1100 draws vs 440-590). The PS2 draws all six too (row
   state 2); it gates static instances and patches by resident texture chunks (0x22A5A0 / 0x22A698, viewer range 1.5 x far),
   which PEAK1 does not export. With the camera far at most 300 m and the chunk range 450 m, every chunk in the frustum is
   resident in steady state, so chunk gating would change only the load-in (pop-in), not the steady draw count: **not ported
   (fidelity note: PEAK1 chunk trees / 0x22A5A0 gating missing; the event packages have them)**.
3. **three.js per-object overhead** dominates the render in events and free roam alike (~50 % of a phone frame): the frustum
   visit of every visible world batch (`_projectObject`), matrix updates (fixed by the separate "Rendering performance pass",
   web-render-performance.md, plus the free-ride container below), and per-draw bookkeeping (`_renderObjectDirect`, bindings
   `_update`, `writeBuffer`, `needsRenderUpdate`, `updateAttribute`: ~27 % of a phone frame after staticWorld).
4. **Not causes** (measured): collision / terrain queries over every loaded location (13 vs 16 locations fed into the core: same
   ms per tick, bit-identical; the residency gate skips non-resident tracks cheaply); both worlds' set pieces (sp.update 2.2-3.4 ms at
   phone 4x vs the event's 2.3-2.7); per-frame streaming bookkeeping when idle (< 0.3 ms at 4x); GC; the simulation (free roam
   has no computer riders: 9-10 ms per frame at 4x vs the event's 36).

The PS2 at a connector: rows in state 2 are drawn and collidable, state 1 (read, not yet active) is neither, state 7 is collidable
but not drawn (docs/peak-mountain.md "Streaming"); the browser draws exactly the state-2 rows (free-ride.js) and gates the core's
queries by the same states, so no neighbour zone is drawn or queried early.

### Fixes

- **Rendering performance pass (already live, verified here):** frozen world / sky / refined-terrain matrices, flag uploads only on
  changed slots, trail prefix uploads, moving-instance heap reads, shadow pose buffers (web-render-performance.md). Pixel checks:
  (1) the pre-pass web/ tree vs the live tree on the same deterministic run (ssxQA.start, fixed game / visual RNG words and LCG,
  scripted pad, 1030 ticks, 6 paused frames): Snow Jam ai=0 identical in Chrome and WebKit; (2) same page, same paused frame,
  frozen vs every frozen object switched back to per-render recomposition: identical in Chrome (event with AI, hub, zone,
  connector) and WebKit (event, hub, connector). Free-roam runs are not deterministic across page loads (stage-world history),
  hence the same-page checks. One benign difference by review: rider-skinning's first capture after reset() no longer
  interpolates the shadow pose from before the reset.
- **free-ride.js:** the identity `peak1-world` container is frozen (`matrixAutoUpdate = false`); recomposing it every render
  forced a matrixWorld multiply on every location mesh.
- **pv `staticWorld` (on):** web/static-world.js. The plain static batches of each streamed location (peak-set-pieces attach) and of
  an event course's world (main.js after the set-piece renderer) move into 128 m cell groups; a `scene.onBeforeRender` hook
  hides a cell when its box (every member's world bounding sphere + 1 cm) is outside the render camera's frustum, and every cell
  in layer-1 renders (the encoded snow composite); cells skip their frozen members' matrix recursion; rider-shadow's per-frame
  rider search no longer walks the static world. Same draw set and order (three sorts by group order 0, render order, projected
  depth, id). A detached group (course switch, released location) drops its cells. Pixel checks: cells culled vs every cell
  forced visible, identical in Chrome (event with AI, hub, zone, connector) and WebKit (event, hub, zone, connector);
  event ai=0 cross-run pv off vs on identical in both browsers; in-page switches ARA1 -> PEAK1 -> BRA2 left no stale cells.

| render JS ms per frame, off -> on (2 rounds) | Chrome phone 4x | WebKit desktop |
| --- | --- | --- |
| zone ARA1 | 25.3 / 21.6 -> 19.6 / 13.6 | 3.9 / 3.8 -> 2.7 / 2.8 |
| hub A row | 29.3 / 29.4 -> 24.3 / 22.0 | 6.3 / 5.7 -> 5.1 / 4.7 |
| A_ARA1 connector | 16.1 / 14.8 -> 13.3 / 10.6 | 3.0 / 2.8 -> 2.2 / 2.5 |
| ARA1 row after the Load trigger | 26.1 / 25.4 -> 19.2 / 16.8 | 5.3 / 4.5 -> 3.5 / 3.6 |
| event ai=0 | 20.6 / 20.3 -> 17.5 / 18.8 | 5.1 / 4.2 -> 3.9 |
| event, 6 riders | 32.5 / 27.5 -> 28.5 / 24.3 | 6.5 -> 5.4 |

  Frame p50 at phone 4x: zone 46.8 / 39.7 -> 39.7 / 34.2 ms, hub 64.4 / 62.1 -> 51.4 / 41.8 ms. WebKit desktop holds 60 fps in
  every scenario before and after (p50 17 ms); only its JS time drops.
- **worldMerge (tried, dropped):** one draw per cell and material for the opaque static batches (they share the location's vertex
  buffer). Not exact: 20-353 pixels per frame (max 9/255) differ at object edges where opaque batches have coincident or coplanar
  overlapping triangles (depth ties: ~10 % of batches, e.g. A 88, ARA1 346), so the draw order decides; and marginal (phone 4x
  draws 479 -> 406 in the zone, render JS within noise). Removed.
- **pv `sharedWorldMaterials` (on):** web/world-material.js. three keys a built node program by node ids
  (`Node.customCacheKey()` = id), and `createOriginalWorldMaterials` made a fresh `texture(map, uv)` graph per texture of every
  package, so every world material built its own program although the WGSL was the same: a Peak 1 location's draw package cost
  75-170 node builds (16-80 ms each at 4x CPU, the riding build's 40-55 ms long frames) for 0 new WGSL programs; the PEAK1 start
  built 811 states for 41 programs. Now one colour graph per structure (terrain; instance modulate / blend 1, 2, 3; per UV-scroll
  group; the fringe mask) whose texture nodes (`MaterialTexture`, an object-updated TextureNode) take the drawn material's own
  textures (`userData.ps2Textures`, non-enumerable) before that render object's bindings update, as three's materialReference does.
  Code is generated from a fixed placeholder (filterable RGBA8, no colour-space conversion, no flip), so it does not depend on the
  texture bound last; a texture whose code would differ (nearest / nearest, non-RGBA8, flipped, render target...) and the byteBlend
  class keep the per-material graph. `worldMaterialGraph(material, legacy)` (QA) swaps a material back to its per-texture graph.
  Pixel checks: same page and paused frame, shared vs every world material rebuilt on its per-texture graph: identical in Chrome
  (hub 9, connector 8, zone 6, Snow Jam 6 frames) and WebKit (the same); cross-run pv off vs on: Snow Jam, Metro City and Ruthless
  (ai=0) identical in both browsers. WGSL budget with the switch on (PEAK1, ARA1, BRA2 at the low tier): largest module 1280 B.

| node builds (Chrome, 4x CPU) | off | on |
| --- | --- | --- |
| B draw package (prebuild) | 75 builds, 1041 ms | 0 |
| BRA2 draw package | 167, 3070 ms | 17, 427 ms (its own new structures, 5 new WGSL programs) |
| ASS1 draw package | 170, 1584 ms | 5, 311 ms |
| PEAK1 start (Green station), load screen | 832; load-to-ride 17.1 / 21.4 s, warm-up 7.1 / 9.1 s | 132; 14.9 / 18.0 s, 5.5 / 6.2 s |
| Snow Jam event load | 431; warm-up 4.0 / 5.0 s | 129; 3.9 / 3.4 s |
| riding build hub -> A_ARA1 -> ARA1 (streaming switches at their old defaults) | 157 builds while riding, 5 frames > 100 ms, p99 98 / 79 ms | 21, 2, 81 / 70 ms |

  With the CTM-stream switches on (their defaults since 16:34), the next row builds under the arrival / load screen, so the gain
  shows there (shorter builds) rather than on the ride.

- **pv `staticRefresh` (on):** web/static-world.js, web/rider-shadow.js. three refreshes a node-material render object FULL on every
  draw (`NodeMaterialObserver.needsRefresh`: a material with any node property): node, geometry and every binding update. An opaque
  frozen cell member drawn with a shared world material without a UV-scroll uniform has nothing per object to refresh after its
  first draw (world matrix and alpha test fixed, textures bound at the first FULL draw), and the per-frame values it reads are shared:
  the camera, and the rider-shadow receiver's rows / count, which move to the shared render group under the switch. Those render
  objects (`userData.staticRefresh`, set by organizeStaticWorld) take three's own static path: the shared refresh once per render,
  else none. The override wraps `NodeMaterial.prototype.setupObserver` in our code (no node_modules edit; it warns and stays inactive
  if three drops the method); `staticRefresh.enabled` switches it in one page (QA). Not marked: event terrain (refinement edits its
  index), UV-scroll materials, and transparent batches (with them marked, the Snow Jam stadium's additive light shafts and glows drew
  differently until a full refresh; cause not isolated). **Rule for later edits: a per-frame uniform added to the world materials must
  sit in a shared group (renderGroup), or those batches draw it stale.** Pixel checks: same page, the frame after 150-300 ticks drawn
  with the shared refresh vs a full refresh: identical in Chrome and WebKit (event 6, hub 9, connector 8, zone 6 frames); cross-run
  pv off vs on: Snow Jam, Metro City, Ruthless identical in Chrome, Snow Jam and Metro City in WebKit; WebKit Ruthless segment 9
  differs by 766 snowfall pixels in either arm on some runs (run-to-run weather noise: the off arm shows it too). Render JS per frame,
  off -> on, 2 interleaved rounds (a quieter machine than the tables above): Chrome phone 4x zone 12.5 / 12.3 -> 11.4 / 11.1, hub A row
  18.3 / 18.4 -> 16.9 / 17.0, ARA1 row 14.4 / 13.4 -> 12.7 / 12.8, event ai=0 12.7 / 12.7 -> 12.2 / 10.7; WebKit desktop zone 2.9 / 2.8
  -> 2.6 / 2.7, hub 4.6 / 4.7 -> 4.4 / 4.3, ARA1 row 3.5 / 3.7 -> 3.3 / 3.2.

### Next levers (not done)

- The remaining per-draw cost (three's render-object lookup, pipeline / bind group / buffer encode) is per visible object: fewer
  objects needs merging, which changes pixels at the depth ties above, or render bundles (the same caveat on draw order).
- The transparent static batches with the static refresh (see staticRefresh): why they drew differently is not isolated.
- peak-set-pieces ticks every UV-scroll instance of the peak (728) though only each group's representative is read, and walks
  every attached location's LiveComp meshes (490) each frame: ~1 ms at phone 4x.
- `_animation_tick` (1.2 ms per frame at phone 4x) is the largest simulation export in free roam.
- Streaming (CTM-stream agent): the streamGate stall at a Load trigger when the row's draw package is not built yet, and the
  collision feed while riding (fr.update 2-18 ms per frame at 4x).

### Files

web/static-world.js (new), web/peak-set-pieces.js (attach / detach), web/main.js (event world, one statement), web/rider-shadow.js
(riders(), the receiver uniforms' group under staticRefresh), web/free-ride.js (the peak1-world container, one line), web/world-material.js (sharedWorldMaterials), web/pv-flags.js
(`staticWorld`, `sharedWorldMaterials`, `staticRefresh`); tools: web/bench-free-roam.mjs, web/perf-probe.js. Verification scripts (same-frame
toggles, cross-run comparisons, WGSL budget, build counts) stayed in the session scratchpad (`ctmperf/`).

## Browser-port optimizations (2026-09-27, round 2)

Candidates measured (headless Chrome and the system WebKit, the Mac shared and loaded), then fixed in order of value. Everything behind
pv switches; pixel checks as above (same-frame toggles, cross-run on the deterministic event ai=0, device-loss before / after).

| candidate | measured | outcome |
| --- | --- | --- |
| GPU device loss (iOS backgrounding) | every event course drew without its static world after a recovery: Snow Jam 848k of 1.23 M pixels (Chrome), 2.6 M of 3.7 M (WebKit) differ | **fixed, pv `gpuRestore` (on)** |
| CPU copies of GPU data (iPhone memory) | free roam low tier: ~100 MB of vertex / index / texel arrays kept on the JS heap after upload (heap 314-343 MB); event ~21 MB | **fixed, pv `gpuRelease` (on)** |
| high-refresh displays | Chrome with uncapped rAF: 88 % of frames advance no tick and still render; at 120 Hz exactly half | built, pv `refreshCap` (off: a smoothness vs power choice) |
| empty FX batches | 65 of 690 render objects a frame in a six-rider event were empty snow / impact / startfire batches | **fixed, pv `skipEmpty` (on)** |
| streamed-world set pieces | 734 UV-scroll instances ticked for 23 drawn groups; LiveComp matrices of hidden locations | **fixed, pv `setPieceSkip` (on)** |
| shader caching across visits | all 88 WGSL modules byte-identical across page loads: browser caches can hit | nothing to do |
| dynamic resolution | GPU 4.2 ms (Chrome) / 7 ms (WebKit, 1592x896) a frame at desktop, 1.5-2 ms at the phone tier: CPU-bound | not done |
| render bundles | three draws bundles before the other opaque objects: a draw-order change (depth ties change pixels, see worldMerge) | not done |
| main-thread decode | a PEAK1 load spends 62 ms in PNG decode, no long task (8.1 s to ride); the core collision feed dominates | not done |
| allocation churn | 31 -> 19 MB/s in free roam (Chrome sampling) after setPieceSkip; mostly V8 double boxing that JSC does not allocate; ~2 MB/s real object churn | not pursued |
| desktop render targets | 210 MB at the high tier (3 MSAA rgba16float, 3 MSAA depth at 1194x896); the phone tier 7 MB | not done (three internals) |
| WebKit "device mismatch" after a recovery | reported by the destroyed device 5 ms after destroy() (a frame in flight); no old-device object used on the new one (error scopes, device tagging) | explained, harmless |

### gpuRestore (web/gpu-copies.js, web/gpu-recovery.js, web/main.js, web/world-material.js)

main.js drops the event world's vertex, colour, light-UV and alpha arrays after the warm-up (~35 MB on ARA1); gpu-recovery.js re-inits
three on a new device, which uploads again from those arrays: empty after the drop. Now `installDeviceRecovery({beforeResume})` awaits,
after the new device exists and before drawing and the frame loop restart, `restoreGpuCopies()`: every registered owner still in the
scene re-reads its arrays (plain `fetch`: HTTP cache, else the network) and rebuilds them as the load does (world-material.js
`lightUvFrom` recomputes the light UVs); then `initRiderShadowAtlas` (keyed by device, web/rider-shadow.js) makes the shadow atlas on
the new device before anything samples it. A throw is a failed attempt (retry, then the page reload). Checks: the frame after a
simulated loss (`__gpuRecovery.simulateLoss()`, paused) equals the frame before: Snow Jam (ai 0 and 5 riders), Metro City, R&B,
Happiness, Ruthless, PEAK1 hub and zone, Chrome and WebKit; with the shadows off during the recovery (the world binds the atlas first)
too, and no validation error on the new device. Recovery 0.3-0.9 s.

### gpuRelease (web/gpu-copies.js releaseWorldCopies, web/main.js asset(), web/free-ride.js)

A world package's arrays are dropped once three's backend holds a GPU buffer / texture for them: a streamed location after its compile
(free-ride.js buildRender / rewarm, else after its first 2 s of drawing), the event world's texels after the warm-up. Not uploaded yet
(flag and MeshAnim pieces added later, textures of materials that have not drawn): kept. Texture sources are grouped (wrap-mode
clones share one); world texture 9-161 stays (web/crowd-2d.js swaps its image). Restore: indices.bin sliced by each batch's
`indexFirst`, texels re-decoded from the kept archive. Memory, low tier: Chrome free-roam hub JS copies 100 -> 3.6 MB, heap 343 -> 238 MB
(event: texels 6.4 -> 0.7 MB, heap 208 -> 182 MB); WebKit WebContent footprint (paused, 2 min of samples) min 1073 / 1057 -> 931 / 905 MB.
Checks: device loss identical (PEAK1 hub and zone, Snow Jam, Metro City; Chrome, WebKit); Snow Jam cross-run identical; leave and
return paths (reuse within 45 s, rebuild after 45 s, Transport away and back, lodge Return to Game): 0 validation errors, complete
locations, and a device loss after them identical.

### refreshCap (web/quality.js frameGate; off)

On a display faster than 60 Hz the 60 fps gate draws every n-th animation frame, n = floor(Hz / 60 + 0.1) from the median of the last
30 rAF intervals (120 -> 60 fps, 144 -> 72, 165 -> ~82, 240 -> ~60; 60-100 Hz unchanged). The ticks run in the drawn frames (60 a
second); each drawn frame is today's at the same tick and interpolation alpha; the HUD clocks, weather, snow and set pieces advance
per tick; cutscenes, menus and the loading screen are time-based. What changes on a 120 Hz+ display: the in-between interpolated
frames (rider / computer riders / camera motion at 120 fps) and the time-based animation sampling. Chrome with uncapped rAF: busy
main thread 894 -> 547 ms/s. Safari keeps rAF at 60 by default (no change there).

### setPieceSkip (web/peak-set-pieces.js) and skipEmpty (web/snow-renderer.js, impact-fx-renderer.js, startfire-renderer.js)

setPieceSkip: only the UV-scroll groups' representatives tick (734 -> 23 instances in Peak 1; 20000 ticks with random section
activations: 0 mismatches against the full set), and a hidden location's LiveComp meshes are skipped (set the frame the group shows:
free-ride.js sets visibility first). Same paused frame skip on / off identical at the hub, with the hub hidden and after it shows
(Chrome, WebKit); set-piece update 2.9 / 3.7 -> 1.9 / 2.2 ms a frame at phone 4x. skipEmpty: an FX batch with no particles is hidden
(it drew nothing, but three ran its whole per-object path first): empty render objects 65 -> 0 a frame in a six-rider event; Snow Jam
and Metro City cross-run identical in both browsers.

Tools: web/bench-free-roam.mjs (`--stream default` keeps the streaming switches at their defaults), web/perf-probe.js (draws per named
group). Scratch scripts (devloss, returns, shadowloss, devmismatch, explore, passgpu, hz, wkmem) in the session scratchpad `ctmperf/`.

## Safari load and frames (2026-09-28, round 3)

Owen's Safari sessions: ~30 s from an event pick to control (Chrome ~6 s locally), load-screen frames p50 111-115 ms, stalls of 1-7 s
in play. Reproduced with the shared WebKit driver at Owen's 1593x896 drawing buffer (BRA2, the CTM start at Happiness). Tools in the
session scratchpad `ctmperf/`: coldload.mjs (phases, cold cache by renaming every WGSL entry point), wkframes.mjs (per-frame GPU object
creation), wkwarmsample.mjs / wkridesample.mjs (`sample` of the WebContent and GPU processes), wkdraw.mjs (2D drawImage census),
wkglyphcmp.mjs (UI canvases, same page and frozen time), wkcalls.mjs (WebGPU calls per frame), warmstat(-wk).mjs, srwab.mjs.

| finding | measured | outcome |
| --- | --- | --- |
| cold GPU pipeline cache | none: Chrome and WebKit load the same with every WGSL entry point renamed per run | nothing to do |
| **load-screen glyphs (WebKit)** | 70-80 ms load-screen frames while the WebContent main thread (75%) and the WebGPU queue sat idle: WebKit's GPU process (RemoteRenderingBackend) locks and converts (vImage BGRA -> RGBA) the whole accelerated glyph tint atlas (512x221) for each of ~1000 glyph draws a frame into the software UI canvas; the same mechanism as Firefox (docs/firefox-load.md) | **fixed, pv `softGlyphs` (on)** |
| warm-up slices on a gated tier | the 30 fps / auto frame gate skips animation frames; a slice revealed on a skipped frame was never drawn, so hundreds of first draws (render objects, bind groups, index buffers) fell into the whole-scene frame: 500-800 ms at phone 4x; the first warm frame built the post passes, the sky and every rider's shadow silhouette at once | **fixed, pv `warmSpread` (on)** |
| full refreshes of blended / terrain / LiveComp world batches | 380-550 render objects a frame took three's full refresh; the old "transparent batches drew differently" was the crowd texture's frames (web/crowd-2d.js swaps world texture 9-161's image; only a full refresh re-uploads a texture whose version moved) | **fixed, pv `staticRefreshWide` (on)** |
| Safari in play | WebContent main thread at the Peak 1 hub: 57% rAF callbacks, 37% blocked in `GPUCanvasContextCocoa::prepareForDisplay` (a sync wait for the GPU process to drain the frame's WebGPU calls: ~6500 at the hub, ~3900 in Snow Jam, each an IPC message: setBindGroup 1640, setVertexBuffer 1580, drawIndexed 1390, setPipeline 1210, setIndexBuffer 610) | per-draw cost twice over; staticRefreshWide cuts the JS; call counts next |
| stall attribution | field stalls said only how long | **diagnostics.js `stall` / `hitch` events carry a `cause`** |
| per-batch index buffers | 608-692 setIndexBuffer calls a frame at the hub: every batch had its own copy of its index range | **fixed, pv `sharedIndex` (on)** |

### softGlyphs (web/sprite-canvas.js glyphSource, web/ui.js text)

WebKit only: a glyph tint atlas is made exactly as before (an accelerated canvas: `drawImage` of the font page, `source-in` fill) and
drawn from a one-time 1:1 software copy (the same bytes); the accelerated original is released after the copy (no memory growth; QA
pages, ?qa=1, keep both for comparisons). Not all fast variants are exact: software-made tints (pv `softSprites`, off) differ by up to
64 levels on multiply-tinted sprites; per-glyph canvases differ at glyph edges (the atlas's own minification); an ImageBitmap is exact
but as slow. Check: same page, frozen time, the UI canvases with the accelerated atlas vs the copy: identical on title, main, event,
character, options, setup, details, the load screen (4 frames), results / HUD, pause, pause options. In the race HUD the glyphs come from
trick-hud.js's own atlases (~40 small draws a frame) and are unchanged. WebKit: BRA2 warm-up 10.3 -> 3.5 s (frames p50 77 -> 20 ms),
warm:gpu -> intro 7.5-9.8 -> 2.6 s, load to the intro 22-25 -> 11.6 s, to control (with the PS2 intro, ~7 s) 30-33 -> 19.6 s; CTM
Happiness start to control 15.3 -> 10.3 s.

### warmSpread (web/main.js warmupRender, web/rider-shadow.js)

A warm-up step waits for a frame the gate drew (resolved by the animation loop after `frame()`, with that frame's work time; at most 4
animation frames). Before the first slice: a frame for the post passes alone, then a step a frame of three sky meshes, one rider's
shadow silhouettes (`update(view, warming, warmLimit)`) and one shared vertex buffer of the course (>= 1 MB, `renderer._attributes.update`).
On a 30 fps tier a drawn frame takes two animation frames' slices; the new-material rate follows the drawn frame's own work. Longest
warm-up frame: WebKit desktop 172-257 -> 73-88 ms, WebKit phone tier 199-960 -> 105-124 ms (warm-up the same or shorter), Chrome phone
4x 706-999 -> 310-323 ms. Cross-run Snow Jam and BRA2 identical in Chrome and WebKit; no builds on race frames.

### staticRefreshWide (web/static-world.js)

Every render object drawn with a shared world material without a UV-scroll uniform takes three's static refresh (the shared refresh
once per render and observer, else none), marked by organizeStaticWorld or not, and a full refresh whenever what that path skips has
changed since its last full one: the world matrix (16 values), the material's opacity, alpha test and version, its textures' versions,
the geometry's index / position / colour versions (the object group of these graphs holds only opacity, alpha test, the world matrix
and the UV-scroll offset: listed per kind). Full refreshes a frame: Peak 1 start 383 -> 62, Snow Jam with computer riders 554 -> 229.
Render JS, same page and paused frame, switch toggled in blocks: Chrome phone 4x hub 33.9 -> 26.7 ms, Snow Jam 26.8 -> 23.1; WebKit hub
8.8 -> 7.3, Snow Jam 6.4 -> 5.5. Checks: the static vs a full refresh of the same frame identical at hub / connector / zone / event in
Chrome and WebKit (WebKit shows 1-level noise on 3-10 hub pixels in some shots, also with the switch off); cross-run Snow Jam and BRA2
identical in both; device loss identical in both (event and hub). Also fixed: the static path's shared refresh used FULL's constant
(2) where three's SHARED is 1 (same pixels, 1-2 render objects a frame).

### sharedIndex (web/main.js asset, web/gpu-copies.js)

A world package's batches draw from one index buffer (its indices.bin, `ssxShared`), each batch at its draw range (and material groups)
from its first index, instead of a copy of its range each: the same draws; three skips `setIndexBuffer` while consecutive draws share it.
The event course's terrain keeps its copies (terrain refinement edits them in place). gpuRelease drops the shared array once and the
restore reads indices.bin whole. Peak 1 hub, same page and frame, shared vs own copies: setIndexBuffer 648-692 -> 245-274 a frame, WebGPU
calls -6%, WebKit render JS 10.5 / 11.3 / 9.9 -> 9.6 / 11.0 / 8.7 ms. Identical: same page (hub Chrome and WebKit, zone WebKit), cross-run
Snow Jam and BRA2 (both), device loss (both; hub and event), the leave / return paths (both; no validation errors).
The static refresh's check also compares identities now (geometry, index / position / colour attributes, textures): a swapped
attribute of the same version was a hole (found with this change's same-page swap; nothing in the page swaps them).

### Stall attribution (web/diagnostics.js)

`stall` (> 1 s) and `hitch` (250 ms - 1 s, at most 30 a session) events carry `cause`: `busyMs` (script and rendering of the frame
before the gap, WebKit's present wait included: a MessageChannel message posted at the frame's first animation callback),
`longTaskMs` (Chrome), and what happened in the gap: `pipes` / `pipeMs` (render pipelines created synchronously, main-thread ms),
`apipes` and `compiling` (async ones started, still compiling), `shaders`, `tex`, `bufs`, `binds`, `builds` (three node builds),
`marks`, `cutscene`, `streamStalled`.

## Simulation core, round 3 (2026-09-27/28, sim-CPU agent)

Goal (Owen): less frame time in events and free roam. Before this round, an event with 5 computer riders ran at a frame p50 of 77 ms
at Chrome phone 4x with the export-timing probe on (50 ms without it), of which the simulation was about 36 ms (23.5 ms without the
probe). The simulation stays bit-exact: every change below computes the same words as before for every export, test and capture. No
added input latency, no memory growth beyond a few hundred KB of shared caches.

### Results

Sim tick cost after / before, paired (both builds in one process or one page, alternating 50-tick chunks, so machine load hits both
alike). The six-rider Snow Jam tick of web/bench-sim.mjs with phone-like reads. Before = the core of 21:24 (892c43ec) with its
web/ai-racers.js.

| cumulative | WebKit (JavaScriptCore), exact | Chrome desktop, exact | node, exact |
| --- | --- | --- | --- |
| A: rail walk boxes, const reference primitives, pose clip lookup | 0.833 | 0.912 | 0.92 |
| + B: no renderer poses for computer riders, shared chair entities | 0.781 | 0.855 | 0.88 |
| + C: operand flush fast path | 0.718 | 0.788 | 0.85 |
| + D: ai-racers.js glue | 0.708 | 0.760 | 0.84 |
| + E: LTO | 0.677 | 0.755 | 0.80 |
| + F: shared chairlift evaluation | 0.652 | 0.74 | 0.79 |

Other configurations, all after F: presentation fast mode (the phone tier's palettes and snow) WebKit 0.662, Chrome with 4x CPU
throttling 0.754-0.758, node 0.779; reads every tick (desktop pacing) node 0.835. Absolute, Chrome desktop: 1.10 -> 0.83 ms per tick;
WebKit: 1.30 -> 0.85 ms; Chrome 4x fast: 4.4-4.9 -> 3.3-3.7 ms.
The human alone (no computer riders: ai=0 events, free roam's rider; `--solo 1`): WebKit 0.182 -> 0.137 ms per tick (0.750x),
Chrome 4x fast 0.556 -> 0.484 ms (0.863x).

Whole game, Chrome phone emulation 390x844, 4x CPU, low tier (web/perf-probe.js without the export wrapping; 3 interleaved rounds of
30 s; the machine's load moved between 8 and 22, and the same core drifts +-15% between page loads, so these are coarse): event with
5 computer riders, simulation per tick 8.03 -> 7.26 ms (per-round ratios 0.87 / 0.80 / 1.03, the last during a load spike), frame p50
52 -> 51 ms; ai=0 2.30 -> 2.05 ms per tick; free roam (ARA1 zone) 2.29 -> 2.10 ms. The low tier caps ai=0 and free roam at 30 fps, so
their frame p50 stays ~34 ms; the rAF callback went 22.6 -> 20.7 ms (ai=0) and 22.0 -> 20.3 ms (zone). An earlier A+B-only run (load
13-17): event sim per tick 10.74 -> 9.13 ms, frame p50 80.7 -> 72.7 ms.

### Profile (before)

Chrome phone 4x, event with 5 computer riders, low tier, CPU profile of the FixedStepClock ticks: WebAssembly 79%, web/ai-racers.js 7%
plus 1.5% for the WebAssembly.Global getter/setter of `__tls_base`, the rest present() (audio, skin capture, terrain detail). Per rider
(node, exclusive ms per export): the human ~0.9 ms per tick (with the rider-pair dispatcher 0.08 and the camera 0.06), each computer
rider ~0.43 ms (animation_pose 0.11, animation_post 0.10, step_rider 0.064, race_begin 0.036, palettes 0.04, snow 0.03, FX 0.025, trail
0.012, race_end 0.01, npc_tick 0.007). Hot functions, share of the ticks: WorldBodyCollision::query 10.2% (its instance loop),
originalScalarAddSub 6.2%, teeterQuery 5.9% (the rail query), pose math ~12% (LocalPose 4%, Compose + cross 6.5%, quaternion 1.2%),
advance_world_entities 7.4% inclusive (chairlifts 5.4%), queryOriginalWorldSegment 2.8%, sourceGroundContact 2.75%.

### Changes and their proofs

Every batch: web/test-ps2-captures.mjs output identical line for line to the baseline core's run (230-235 scenarios, including the
new air-release ones); the core test list (original +-zoe, long-runs, rail / rail-uber / crash gameplay +-zoe, crash-roots,
crash-recovery-input, rider-contexts, ai-racers, rider-reset, handplant, ps2-rail-boost, course-spawn, slopestyle-bigair) and for B, C
and F also set-pieces, set-pieces-locations, moving-instances, opponent-riders, lineups, rival-page, mp-pairs, mp-fx, mp-latency,
replay, shared-parse, stage-world, rider-frame / skinning / shadow, big-challenges: identical output apart from timings and the rider
TLS block size; and **web/sim-diff.mjs** (below): 11 courses with computer riders and the 6 big-air / halfpipe courses solo, tuck /
scripted / random / trick pads, 1500-2500 ticks, every tick identical. A trick-pad run against the pre-A core after B (76 runs x 2400
ticks, ~10 G words, the human's renderer poses and skin matrices and the computer riders' renderer poses included; 54,769 airborne
ticks, flips released mid-rotation, 25 trick names): identical.

- **A1, rail query (web/rail_bridge.cpp, engine/rail_motion.hpp):** 0x334680 walks every segment (822 on ARA1) in the octree order and
  rejects almost all on 0x335128's box test; a segment is 112 B read in that order, and the walk cache's key was a hash over all 182
  records on every query. The walk cache now also keeps each walked segment's bounds (`OriginalRailWalkBox`, 24 B, in walk order);
  the query tests the copied box first, then the record's flags and residency, then runs the unchanged segment query (which tests the
  box again). The three tests are pure, so the same segments are visited in the same order. Segment bounds are never edited in place
  (only whole record sets are replaced or appended); the key now includes each record's segment buffer address, and eight sampled
  boxes are compared with their segments on every query.
- **A2, reference primitives (engine/software_float.hpp):** `addReference` / `mulReference` are `[[gnu::const]]` under Emscripten
  (they read no memory and no FP environment: wasm has only nearest). The compiler then keeps a function's `originalRoundingMode`
  load across them: Compose went from 15 mode loads to 2. Native builds keep them plain (there the host rounding mode is state).
- **A3, pose clip lookup (engine/animation_motion.cpp originalAnimationLocalPose):** the clip of a layer (a scan of ~500 clips by id)
  was looked up for every bone of every layer; now on the first bone that uses the layer, and reused. The lookup is pure; a missing
  clip still throws at that first use.
- **B1, renderer poses (web/animation_bridge.cpp, web/ai-racers.js):** the renderer poses (`poses`, a compose per bone relative to the
  presented root, animation_post's return) are read only for the human (main.js's rider frame). A computer rider is drawn from its
  skin palette, which reads `cachedCrashWorld`. ai-racers.js passes rider_host bit 4 for computer riders and their contexts skip that
  loop; animation_post's return then keeps the last poses written. Older cores ignore the bit.
- **B2, chair entities (web/set_piece_gameplay.inc, engine/world_body_collision.hpp):** `composeWorldCollisionEntity` is a pure
  function of the instance's hierarchy (local matrices, parents, scale, node indices), the root and the bounds, and every rider context
  places its copy of each chair on the same root every tick. `chairEntityShared` (a shared global on web/check-rider-globals.mjs's
  list) keeps the last entity per resource with all of those inputs; a context whose inputs are bit-identical (memcmp) takes that
  immutable entity. The composition's recursion is a plain recursive lambda instead of a std::function (an allocation per call).
- **C, operand flush test (engine/software_float.hpp add / sub / mul):** `eeFlush` changes only an operand whose exponent field is 0,
  so when both exponent fields are nonzero the operands are used as they are and the two flush tests are skipped; otherwise the
  previous code runs. Proof, the new functions against the previous header verbatim: native, all 552^2 pairs of special values, 2e9
  random pairs (uniform bits, near exponents, one zero / denormal operand) and every binary32 against 16 fixed operands in both orders,
  add / sub / mul: ~4.2e11 results, 0 mismatches, NaN payloads included; WebAssembly (em++ -O3, node) the same with 5e8 random pairs
  and a stride-16 sweep: ~2.7e10 results, 0 mismatches. `tools/test_exact_float.sh 20000000 4 2000000 16`: native 3.25e10 and wasm
  1.89e10 fast/reference results, 3e8 against the arm64 FPU in toward-zero + flush-to-zero mode: 0 mismatches.
- **D, rider glue (web/ai-racers.js):** the current rider context is mirrored in JS (`module.__riderTls`; only the views write
  `__tls_base`, and rider_context_create restores it before returning), so a view call reads the mirror instead of the
  WebAssembly.Global getter; stage1's `current` Map of copied rider states became an in-place read of the earlier riders' class word,
  and endTick's per-rider `live()` copies became in-place views: the same pure exports read at the same points (stage1 no longer calls
  race_progress_info, whose value it never used); the npc_world_buffer view is cached. Proof: sim-diff.mjs --aiA old --aiB new on one
  core (6 courses x 3 pads x 2000 ticks), captures and the 14 tests that run the glue.
- **E, LTO (web/build-core.sh `-flto`):** wasm has no FMA and the sources keep `-ffp-contract=off`, so optimisation across the core's
  sources cannot change arithmetic. web/check-rider-globals.mjs strips the `.N` suffix LTO gives internalised globals and lists five
  runtime globals the LTO object now carries (libc++abi eh_globals, malloc freelist / heap, the timeout globals: shared as before).
  A web/build-core.sh run takes 30-60 s longer (single-threaded LTO link).
- **F, chairlift evaluation (web/set_piece_gameplay.inc `multiSplineShared`):** 0x35AC20 (originalMultiSplineEvaluate) is a pure
  function of the car count, mode, angle, distance, the path's length / cursor / index and the spline record's segments; it writes the
  cars' evaluated words and the path's cursor / index. The last evaluation per lift is kept with those inputs (the segments by
  content) and outputs; a context with bit-identical inputs takes the outputs. sim-diff.mjs compares `moving_instances` (the draw
  delta of every roller / chairlift car / spline piece) too; ARA1 has 2 active lifts, BRA2 3.

### Tried and dropped

- **The body query's instance loop (~10% of a phone tick, the largest item left):** memory-bound (2,254 of 3,502 ~204 B instances in
  octree order, ~9 per tick per rider copy, almost all rejected on the box). A compact copy of the fields it tests would fix it, but the
  instances are mutated in place from ~20 sites (set pieces, stage scripts, rollers, falling billboards, the streamed world's runtime
  rows, event seeds: runtime flags, entities, answer boxes and boxes), so no cached copy can be proven fresh. Measured without it:
  the hot fields moved into one cache line (0.988x), the same with 64-byte-aligned instances (1.004x), a pre-scan in memory order
  that collects candidates and sorts them into octree order (1.048x: streams all 714 KB), a box pre-reject before the route / residency
  tests (0.998x).
- LTO of the core's sources only (system libraries not LTO): 0.961-0.965x, a little less than full LTO (0.951x).

### Remaining levers (not done)

- The instance loop above, if the instance list's mutation sites were ever funnelled through one API (then a compact copy could be
  kept in step, exactly).
- The composes of the pose math (WorldPose, pose contact) and `originalScalarAddSub` (~4% each, spread over many callers).
- Per-rider duplicates of other world entities (spline pieces, stage entities): each rider context advances its own copy.
- Desktop only: the exact skin palettes are ~9% of a desktop tick and every opponent is captured every tick, on screen or not
  (presentation; approved in principle with pixel-identical checks for riders entering the view, shadows cast into it, replays,
  results and the rival card; not done).

### Tools

- **web/sim-diff.mjs** (QA, not in npm test): two core builds, the same race and pads, every tick a wide set of every rider's words
  compared (state, posed frame, world state, progress, animation info, crash / reset / route / trajectory records, camera words, both
  RNG streams, the score object, posed bones, the renderer poses, skin matrices, pose controls, moving instances, snow / trail / wake
  buffers and skin palettes); names the first differing export and tick. Pads `tuck`, `scriptN`, `randN`, `trickN` (jumps with flips
  released mid-rotation, spins, grabs, ubers); `--hostbit4 0` compares the computer riders' renderer poses too; `--aiA / --aiB` load
  two ai-racers.js; `--perturb T` is the sensitivity check (a one-tick tuck toggle after the countdown is reported at that tick).
- **web/bench-sim-browser.mjs** + **web/bench-sim-page.js** (QA): web/bench-sim.mjs's paired tick in Chrome (optionally CPU-throttled)
  or WebKit, for any core directories / ai-racers.js files: `node web/bench-sim-browser.mjs --browser webkit OLD_CORE_DIR web/runtime`
  (`--solo 1`: the human alone).
- Scratch (not kept): a parallel incremental build of the core's objects (`-MMD`, byte-identical output to web/build-core.sh), CPU
  profile summaries, the per-rider export-timing wrapper, the exhaustive old-vs-new float check.

### Field findings, 2026-09-28 (stall / hitch causes from 22 sessions)

| finding | cause | outcome |
| --- | --- | --- |
| "Missing live source skin palette" x3 on ctm-results (a hidden 1 fps pane) | the WScript builtin 64 teleport (300948 -> 1234D0, mission_gameplay.inc) clears the core's cached world pose and poses at the next tick: the catch-up capture after that tick read no palette | **fixed** (web/rider-skinning.js): such a tick draws the last palette again (the PS2 draws rider+0x2C as it stands), a reset asked then applies at the next capture; whether 1234D0 caches the placed pose (as 11D660 / 11E150) is the core owner's question |
| WebGL-fallback character select 3-8 s stalls (Firefox 140 / Windows / ANGLE D3D11) | the GLSL builder names each uniform buffer block after its node id (`NodeBuffer_<id>`): every skinned part and uniform array its own program (character select 111 shader texts, 9 distinct modulo the ids) | **fixed, pv `glslKeys` (on)**: WebGL (Chrome ?backend=webgl) character select 109 -> 10 programs, Snow Jam load 146 -> 49; link waits without KHR_parallel_shader_compile 15.0 -> 0.12 s; identical frames (WebGL cross-run, frozen intro actors) |
| iPhone Gravitude: a 1 s stall at the race start (6 sync pipelines, 6 builds) | the glare pass (its levels, final composite, the fog composite into its frame target) first runs when the painter turns glare on; the warm-up never drew it; also the light-glow query (glows counted, none queried in the warm frames) and three's canvas output pass (the first cutscene frame) | **fixed, pv `warmPost` (on)**: sync pipelines after the warm-up 8 -> 0 on Gravitude (Chrome, WebKit); no event course has any (all 17 swept) |
| iPhone character select hitches 0.26-0.66 s during the roster prefetch | new pipelines (Safari compiles each in its GPU process) and node builds; the FE morph parts had four shader shapes | **pv `feMorphTiers` (on)**: morph parts padded to 256 x 27 or 768 x 36 (zero morphs are skipped by three's morph loop): 4 -> 2 shapes. Left: Sam's GC texel domain (a literal 127.5; a uniform could fold differently) and build-order variants (the same graph in a different uniform order: 2 per shape) |
| Firefox WebGPU `gpu-stall` 1.5-6.1 s (old build) | Firefox builds pipelines serially on its GPU timeline: the queue probe waits behind them | the current build creates 3 pipelines in the first 60 s of the CTM Happiness ride, 9 for the whole character-select roster; `gpu-stall` events now carry `cause` (pipelines, async compiles pending, shaders, textures, buffers, node builds in the probe window) |
| FE "build order" shader variants (2 per FE shape) | three's MaterialNode shares one materialReference('map') for every material, re-pointed per drawn object; a build's generate stage (after compileAsync yields) read the last drawn material's map, so the uniform dedup against the material's own texture(map) gave two bindings or one by draw order | **fixed, pv `refKeys` (on; shader-keys.js currentMaterialReferences)**: character-select FE pipelines 8 -> 4, Snow Jam load 71 -> 68; identical (cross-run Snow Jam, BRA2, Gravitude in Chrome / WebKit, WebGL, the frozen intro) |
| Sam's GC texel domain (a WGSL literal) | a uniform could fold differently in the Metal compiler; not provable from WGSL | left (one pipeline, only when Sam shows) |
| LiveComp rest meshes recomposed every frame | peak-set-pieces.js / set-pieces-renderer.js used matrixAutoUpdate as the "player active" flag: ~1100 rest meshes at the CTM start recomposed by three each frame | **fixed, pv `liveRest` (on)**: rest matrix composed once (userData.lcRest); per-frame recomposed objects 1262 -> 108; every rest matrix equals a fresh compose |
| the scene's matrices updated twice a frame | the encoded snow composite renders the scene again right after the world pass: three's second scene.updateMatrixWorld | **fixed, pv `oneMatrixPass` (on; snow-composite.js)**: skipped (nothing would change at 3000 composite renders, CTM ride and Snow Jam with riders); with liveRest, scene matrix updates at phone 4x ~48 -> ~16 ms a second |

