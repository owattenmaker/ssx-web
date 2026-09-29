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
