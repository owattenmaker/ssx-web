# Web rendering performance pass — 2026-09-27

This pass removes redundant presentation work without changing simulation arithmetic, game timing, render resolution, or effect density.

## Changes

- `web/main.js`, `web/terrain-overlays.js`: cache authored world/sky mesh local transforms and refined terrain transforms. Keep the scene root and stationary world containers at their existing identity matrices. World matrices still update when a parent changes; moving scenery and LiveComp players keep their explicit matrix updates. Previously the automatically recomposed scene root forced every descendant's world matrix to be multiplied every frame.
- `web/set-pieces-renderer.js`: transform and upload flag vertices only when a cloth slot changed or a flag mesh was newly created. Accumulate changed slots across all simulation ticks of a frame, preserving catch-up, parity, section activation, and restart behavior. At 60 render frames per second, the authored 30 Hz cloth needs half as many uploads; paused/repeated ticks need none.
- `web/moving-instances.js`: reuse the resource/copy index and read matrices directly from the current WASM heap during the synchronous draw update. Remove per-instance 16-float snapshots from the hot path. The public `movingDeltas()` snapshot API retains its copy semantics. Fetch the heap after the export so memory growth cannot leave stale views.
- `web/rider-skinning.js`: rotate reusable previous/current shadow-pose buffers, reuse interpolation output, and read the physical pose once per capture. Resets invalidate the preceding pose; changed bone counts resize the buffers.
- `web/board-trail.js`: upload the active vertex prefix rather than every 2,000-vertex capacity buffer. Handle shrink, empty, and growth before the next GPU upload.

## Measurements

Headless Chrome, WebGPU, Snow Jam, Zoe, this workstation. A 10-second CPU profile first identified `updateMatrixWorld` and `multiplyMatrices` among the major JavaScript costs. These profiles are observational, with concurrent work on the machine; they are not an FPS comparison.

The isolated benchmark alternates automatic recomposition and cached transforms on the **same loaded scene**, with eight rounds of 100 updates per mode. On the first run:

| Work | Automatic | Cached |
| --- | ---: | ---: |
| Scene matrix update, median | 0.593 ms | 0.149 ms |
| Matrices differing after full recomposition | 0 | 0 |

3,498 scene objects; 3,046 cached transforms. About **75% less scene matrix update CPU time**, not 75% more FPS. No browser runtime errors occurred in either profile or the parity benchmark.

A repeat Chrome run measured 0.324 → 0.077 ms (76% reduction, 3,522 objects). System WebKit through `web/webkit-driver.mjs` measured 0.480 → 0.100 ms (79% reduction, 3,500 objects), also with zero matrix differences and no runtime errors. All browser runs were muted. Absolute timings vary with concurrent workstation load; these comparisons alternate modes within one scene.

Reproduce with the development server running:

```sh
cd web
node bench-render-transforms.mjs
```

An optional URL selects another course; include `autostart=1&perf=1`. The benchmark asserts exact equality of all world matrices before/after full recomposition and fails on browser runtime errors.

## Validation

Nine focused checks passed: moving instances, board-trail renderer, rider skinning, set-piece renderer, flag animation, terrain overlays, rider shadows, board-trail simulation, and set-piece locations. Coverage includes heap growth, clone visibility/removal/reuse, reset interpolation, changed bone counts, flag parity/repeated ticks, parent transform propagation, and queued GPU ranges.

Existing reference checks retained: 7,000 flag arithmetic cases match bit-for-bit; 20 PS2 cloth snapshots; 88 Junction traffic snapshots; eight original rider-shadow fits; actual course terrain suppression/restoration and ray intersection.

Production Vite build passed using a private output directory and `copyPublicDir:false` (the unchanged 4 GB public asset tree was not duplicated). The usual large-chunk advisory remains.

The full suite was not rerun. A pre-existing full suite/build lock occupied the shared runtime; the two WASM checks above ran against a byte-verified private copy of the existing core.js/core.wasm pair. The remaining checks use JavaScript fixtures. No core source or generated WASM was changed by this pass.

Session evidence: `local/performance-pass-20260927/` holds before/after profiles, raw benchmark measurements, the private core pair, baseline source snapshots, and the build log. It is ignored local data.

## Per-frame garbage (2026-09-29)

Owen's Safari sessions hitch. JSC lets short-lived JS garbage pile up and then collects it in large pauses, so every MB/s a race
allocates matters there. This pass removes garbage at the source. The simulation and the pixels stay the same.

**Measure** (`local/browser-validation/gcchurn/`):
- `alloc.mjs` / `runall.sh` / `rank.mjs`: Chrome's sampling heap profiler (objects that minor and major GCs collected are
  included), 20 s of a steady race after 10 s warm-up. A lean probe drives the rider along the AIP race path and allocates
  nothing itself. The set is BRA2 and ARA1, each at the phone tier (844x390, `quality=low`) and desktop (1280x800).
- A sample of scaled size 8196 is a V8 HeapNumber (checked with calibration loops, `calib.mjs`). JSC never allocates those
  (NaN-boxed doubles), so they are reported apart: **objects** are what Safari pays for.
- Do not measure with `ssxQA.start()` alone: it pauses the game, so the race only renders (the earlier 16 MB/s figure).

**Changes** (each checked bit-exact or pixel-exact, see Validation):
- `web/heap-views.js` (new): `heapU32(core)`, a whole-heap view reused until the buffer changes; and
  `setUpdateRange(attribute, start, count)`, clear + one range record kept per attribute. three's backends read each range's
  start / count once and then clear the list; InstanceNode copies the records into its interleaved buffer in the same frame.
- `web/set-pieces-renderer.js` update: the core logs (triggers / contacts, LiveComp starts, sections) are read in place
  from their cursors instead of sliced; `_set_piece_info`, magnets, meshanims, chunks and multi bits are read through HEAPF32
  or `heapU32`. The teeter matrices come from a pool, and Map passes use callbacks made once. The UV-scroll ids are listed once,
  the cable callbacks are made once, and the mesh loops are indexed. A large update function V8 does not fully optimize
  allocates a for-of iterator record per element.
- `web/livecomp-animation.js`: a player's matrices are recomputed into the state's own arrays (`sincosTo`, `rowInto`,
  `composeInto`: the same EE/VU float operations in the same order as `sincos` / `transformRow` / `compose`, which stay for
  construction and the tests). `s.matrixVersion` replaces array identity for the node-delta cache. `tick()` reuses its lists.
- `web/attached-setpieces.js` `apply`: the frame's deltas go into reused Float32Array slots (`childDeltaInto`, the same
  operations as `nativeDelta(rest, parentMatrix(...))`) instead of a Map with string keys and per-call arrays.
- `web/rider-shadow.js`: `fitShadowInto` / `receiverRowsInto` into per-rider records (`fitShadow` / `receiverRows` keep their
  API). Bit-exact against the old functions on 200,000 random and degenerate inputs (`eqshadow.mjs`).
- `web/weather-renderer.js`, `web/board-trail.js`, `web/snow-renderer.js`: the core records are read in place (no views per
  call).
- `web/set-piece-particle-eval.js` `readParticleEffects(core, pool)`: with a pool, the effect records and their views are
  reused while the heap buffer, word offset and ring capacity stay the same. Without one it behaves as before
  (compare-ai-capture). `web/set-piece-particles.js` passes a pool and keeps per-key lists in the frame's first-appearance
  order, which is the order the per-frame Map iterated them in.
- `setUpdateRange` in crowd-2d, fog-puffs, set-piece-halos, set-piece-particles, terrain-sparkle, snow and weather.
- **pv `sceneLightingOff` (off):** `renderer.lighting.enabled = false` (main.js init; not with `?originalWorld=0`, whose
  fallback materials use three's `lightMap`). Every game material is a MeshBasicNodeMaterial with no three light, light / AO
  map or environment, so the shaders are the same. With lighting on, `Nodes.getCacheKey` is cached per draw call
  (`renderer.info.calls`), so every draw rebuilt `LightsNode.getCacheKey(true)`: a Set, property-name arrays and a child record
  per node property.

**Results** (Chrome, objects MB/s: BRA2 phone / desktop, ARA1 phone / desktop):

| Tree | Objects | Mean |
|---|---|---|
| Before | 19.18 / 19.83 / 18.36 / 18.05 | 18.9 |
| set pieces, LiveComp, attached, shadow, weather, update ranges | 16.08 / 15.99 / 15.67 / 15.85 | 15.9 |
| all changes above | 14.25 / 14.19 / 13.81 / 14.02 | 14.1 (-25%) |
| all + pv sceneLightingOff | 13.07 / 13.02 / 13.17 / 13.65 | 13.2 (-30%) |
| all + sceneLightingOff + threeLean (web/three-patches.js) | 11.39 / 11.30 / 11.30 / 11.32 | 11.3 (-40%) |

HeapNumbers (V8 only) stay at about 25 MB/s. The set-piece update fell from 2.6 MB/s (including its iterator records) to
under 0.1. With sceneLightingOff, `needsUpdate` still allocates about 1 MB/s: three's `hash$1(...params)` takes rest
arguments, so `getDynamicCacheKey` makes an array per draw.

**What is left (objects, MB/s, after):**
- **pv threeLean (off; web/three-patches.js, generated by tools/gen_three_patches.py, applied as a Vite transform in the build and the dev pre-bundle):** removes the dynamic-key rest arrays, the bind-group key strings, the updateTexture default object and the sampler key strings. Chrome A/B on BRA2 / CRA3: identical state and pixels at every in-race checkpoint. A dev server whose deps cache predates the plugin needs `--force`. Still open inside three: render-list push / sort, uniform update-range pushes.
- **three.js internals before threeLean (about 3.5):** RenderObject `needsUpdate` / dynamic cache key (`hash$1` rest arguments); `Bindings._update` string cache keys per
  binding per draw; `Textures.updateTexture(texture, options = {})`, a default object per call; render-list `push` / `sort`;
  `addUniformUpdateRange` records. These need a patch to three r186 (not done: ask first).
- **Update-range pushes:** three clears `updateRanges` with `length = 0`, which drops the array's storage, so the next push
  allocates again whatever record is pushed.
- **Computer riders' per-tick sync (about 0.9):** `web/ai-racers.js` syncWorld / beginTick / endTick / live (slices, views,
  literal arrays). This is on the game tick path: change it only with the AI capture gates.
- **rider-frame capture (0.35),** trick-HUD text, game-audio `relSnapshot`, the weather layer records, then a long tail
  under 0.1 each (about 7 MB/s together).

**Validation:**
- Targeted tests: livecomp-animation (ARA1, BRA2, BHP1, CHP2), attached-setpieces, set-pieces-renderer, set-pieces,
  presentation, rider-shadow, weather, board-trail-renderer, fog-puffs, snow-renderer, stage-world,
  set-piece-particle-sprites (20,459 GS sprites bit-exact), light-glow. The ERA5 livecomp failures (2 Gravitude
  crash billboards) are pre-existing: the unchanged code fails them the same way.
- **Old vs new in the browser** (`cmpstate3.mjs`, `mkfarm2.sh`): two Vite servers over symlink farms of web/ with the
  pre-change and the changed copies (main.js / pv-flags.js frozen on both). The same race is stepped by `ssxQA.advance`, and
  at every checkpoint the script hashes each scene object's world matrix, visibility, instance matrices and flag vertices, the
  UV-scroll offsets and the weather state, and screenshots the frame. BRA2, CRA3, DRA4 and ERA5, 12 in-race checkpoints each
  (tick 150..1800): identical state and pixels. Tick 0 differs between any two runs, the old tree against itself included.
- **Comparison pitfalls:**
  - Pass `&lineupSeed=` / `&presentationSeed=`: the lineup is clock-seeded (web/lineup.js `clockSeed`), so two pages
    otherwise race different computer riders (CRA3).
  - Compare scene objects as a multiset: the object count can shift by one between runs.
  - Riders are drawn at a real-time interpolation (`renderAlpha = acc * 60`), so a few rider pixels can differ by 2-5
    levels between any two runs.
- sceneLightingOff, on against off (Chrome): state identical and pixels identical at every in-race checkpoint on BRA2 and
  CRA3 apart from that rider noise (12 pixels, 2 levels, on one checkpoint).
- **WebKit (2026-09-29, screen unlocked):** `wkcmp.mjs` on BRA2, all-original tree against the current one, and both switches off against on: identical scene state and pixels at all 8 in-race checkpoints. sceneLightingOff and threeLean are on.
- **Earlier, not yet checked in WebKit:** `wkcmp.mjs` (the same comparison through web/webkit-driver.mjs, one side after the other)
  is ready. On 2026-09-29 the Mac's screen was locked (`CGSSessionScreenIsLocked`), and a locked session stops WebKit
  rendering: every page, the unchanged tree included, stayed on its loading screen (offscreen mode too). Run it with the
  screen unlocked before turning sceneLightingOff on. A new origin loads slowly the first time in WebKit (its HTTP cache is
  per origin). Then repeat `wkrace.mjs` (WebKit footprint, frames over 34 ms, p95 / p99 over a race; the baseline is in
  `runs/wkbase-BRA2-phone.log`).

## Computer riders outside the view (pv riderCull, pv fxCull; 2026-10-04, lag agent)

A playtester on Windows Firefox (diag 71f4ne0n) ran free ride at 60 fps and every race with five computer riders at ~40 fps (frames
alternating 17 / 33 ms).

**Costs** (Chrome 1x on this Mac, cold profile, in-world Snow Jam race vs free ride on the same stretch, main thread per frame):
- 6.6 vs 3.5 ms.
- The five rider contexts' simulation: +1.07 ms (bit-exact core work).
- The riders' presentation: +2.0 ms. Measured by hiding them in alternating windows:
  - the five models: 0.9 ms (render 0.8);
  - their trails, wake, spray and streamers: 1.35 ms (the core's spray build 0.37, the trail ribbons 0.18, draws 0.5).
- At 4x CPU: 33 vs 14 ms.
- Firefox (headless here, which falls back to three's WebGL backend): 10.2 vs 5 ms of main thread. Its parent-process CanvasRenderer
  thread adds about 0.2 s of CPU a second.
- The event-load path costs the same per game tick: in-world adds nothing of its own.

**The waste:**
- opponent-riders.js builds every part with frustumCulled = false (the bounds are the bind pose, not the drawn pose). So all five
  models draw every frame wherever the riders are: about 106 draw calls, skinned, many in two passes.
- opponent-fx.js reads and draws every rider's effects every frame.
- The shadows already cull: rider-shadow.js uses a 3 m frustum sphere, its stand-in for the PS2's rider visibility flag.

**The switches:**
- **riderCull** (main.js cullOffscreenRiders): a model whose 5 m sphere around the rider's world position (core rider_world_state)
  is wholly outside the frustum is hidden for the frame's draw. This runs after the shadows took their silhouettes. Every capture
  (palettes, lighting) is unchanged.
- **fxCull** (main.js fxAway, web/opponent-fx.js update's `away`): a rider whose 100 m sphere is out of view has its effects
  neither read nor drawn. They are built when read (web/animation_bridge.cpp), so the next frame that shows them reads the core's
  current state.
  - The trail is the longest effect: at most 48 slices, one per tick (engine/board_trail.hpp drawWindow, the 54-slice ring). 100 m
    holds it up to 125 m/s.
  - The spray's and the wake's reach is not derived from the code.

**Checks:**
- Chrome, Snow Jam with 5 riders and the frame clock frozen, 9 checkpoints from tick 200 to 4800, with 0-5 models and 0-5 riders'
  effects culled. Both switches on vs off: identical pixels at every checkpoint. Scratch: local/lag-latency/qa/cullab.mjs.
- When all five riders are out of view the saving is the hidden cost above (2.2 ms at 1x). Measured in an in-world race where
  the scripted rider rides behind the pack, so most riders stay in view: at 4x, render 14.9 -> 13.4 ms and busy 34 -> 31.7 ms per
  frame; at 1x lost in noise.

- Re-entry with history (local/lag-latency/qa/cullhist.mjs): three separate pages step a tick and a drawn frame at a time to tick
  4800: culls ON, with every rider's effects forced away for ticks 1800-2400 and 3400-4000 (?qa `window.__fxForceAway`); culls OFF;
  OFF again. Identical pixels at all 11 checkpoints, including the frames 2401 / 2420 / 2440 / 2500 / 4020 / 4040 after 600
  unread ticks, and OFF == OFF.

**Multiplayer remote riders (riderCull):**
- net/mp-game.js drawnRiders hands main.js each drawn remote model with the translation of its drawn skin palette's first matrix
  (source cm, the eased offset included). The same 5 m test applies.
- Check (local/lag-latency/qa/mpab.mjs): two headless Chrome clients race Snow Jam through a private mp-server. Both pages' clocks
  are frozen at each of 8 checks (the second client held on every other check, so it falls behind the first's camera). Riders culled
  at 4 checks, identical pixels at all 8.
- fxCull does not cover remote riders: net/remote-fx.js updates their effects inside mpGame.render, before the frame's camera is
  set, so a skip decided there cannot be made exact against the drawn view.

**WebKit (2026-10-05):**
- cullab, one page with a frozen clock: identical pixels at all 9 checkpoints. Ticks 3000 and 3600 need 20 settle frames
  (SETTLE_FRAMES=20); otherwise the first OFF shot differs from the second OFF shot as well.
- cullhist cannot be judged in WebKit: separate pages draw a sub-tick apart (OFF vs OFF2 differ over ~1.9 M pixels). The history
  check rests on Chrome's exact cross-page run.
  - Not a simulation difference: two WebKit pages give identical ?simtrace hashes on all 1500 ticks (local/lag-latency/qa/wkdiverge.mjs).
  - The cause is the harness: the fixed clock's leftover when the frame clock is frozen (0 vs 16.3 ms) depends on real time. Every frozen
    frame draws at that page's own render alpha: ssxQA.advance sets acc 0, then frame() sets it from simulation.pending again.
  - Fixed (QA only): ssxQA.advance now also resets the fixed clock's leftover (simulation.reset()), so frames after a step draw the same
    render alpha on every page. Two WebKit pages at tick 1500 now give 0 differing pixels (wkdiverge.mjs). test-world-warm, test-replay
    and test-frame-clock pass. Cross-page WebKit pixel A/Bs work after an ssxQA.advance.

**Not covered:** set-piece flags (about 15 meshes / 30 draws, static, often off-screen; not worth it).
