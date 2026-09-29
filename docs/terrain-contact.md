# Original terrain contacts

Native terrain collision retains the original bicubic coefficients separately from visual triangles. `terrain.json` preserves patch identity, source hash, authored surface ID/flags, and authored bounds. The loader verifies those records against render provenance. All six local course packages have the current metadata.

`CollisionWorld::sourceGroundContact(position, previousNormal, previousLateral, filteredTurn, geometryScale, cache)` implements the recovered cruise terrain query. Vector arguments use native axes; position is in meters. The optional `terrain_original::ContactCache` belongs to the rider and persists between queries. `RayHit` exposes original resource ID, solved UV, render triangle provenance, and `terrainQuery=3` for this path. `contactSignedDistanceCm` and `contactLateralDistanceCm` preserve source float results; the separate `.distance` field is coarse ray distance in native meters.

The generic `surfaceContact` and ordinary terrain raycasts remain double-precision analytic queries for placement/inspection. Scenery/camera collision still uses the visual triangle mesh. Those paths are not substitutes for the original cruise query.

## Recovered sequence

Original cruise function `0x13D1B8` first copies rider+0x370 to +0x380. It calculates query center as actor position plus **previous lateral tangent** times `45 cm * filteredTurn * geometryScale`. Captured Zoe geometryScale is `0.8499999642372131`. Saved +0x3B0 after a query is the newly recomputed tangent; it cannot substitute for the previous tangent when replaying a turning query.

Endpoints are center minus 100 cm and center plus 200 cm along the previous normal. Native `terrain_original::groundProbe` preserves source-coordinate float rounding and operation order. It does not normalize that stored normal again.

Terrain candidates must have authored flag bit 0 set. Original loaded candidates also require active bit 0x40; that bit is added by original world streaming. The fully loaded native course retains authored eligibility and uses the original authored bounds. Sixty-two live patch flag checks agree after accounting for the active bit.

The original collision grid is **9×9 cells**, with a 10×10 table of points. VU0 program `0x000..0x270` evaluates the bicubic using the immutable ten-row power table at EE `0x43CCB8` (ELF offset `0x33DCB8`). These powers are separately rounded. Recomputing float `u²/u³` would produce different coordinates. Native `coarseGrid` uses the verified table and operation order; its point order is outer U, inner V.

Original `0x32B6E0` checks the cached cell/triangle first when its patch identity matches. Otherwise it scans cells with U outermost and V innermost, trying triangles `(d,b,c)` then `(c,b,a)`. `0x32E4D0` intersects the coarse triangle plane, and VU0 program `0x6E0` tests triangle inclusion. The first successful cell supplies a center seed for refinement. The cache records resource, U/V cell, and triangle half (1 for first, 0 for second).

`0x32E9A0` performs **exactly four Newton updates** using source float operations. It returns early if U/V leaves `[0,1]`; original callers retain the coarse point/normal in that case. The refined derivative normal uses `cross(dv,du)`. Coarse fraction remains the candidate's ranking parameter even after the surface point is refined.

Cruise selection `0x13D42C` first favors candidates whose normal dot previous normal is at least `0.30000001192092896`. Within that group it minimizes `abs(coarseFraction−0.5)`, breaking terrain ties by packed resource ID. This additional normal preference differs from generic `0x3342D0` selection.

Signed distance +0x454 is `dot(queryCenter−surfacePoint, surfaceNormal)`. The separate clearance output from `0x13D7F8` is the length of the tangential remainder after removing that normal component. Both use **query center**, which includes the steering offset.

| Rider offset | Verified field |
| --- | --- |
| +0x430 | Packed patch `(RID << 8) | track` |
| +0xAAC / +0xAB0 | Solved U / V |
| +0x370 / +0x380 | Current / previous contact normal |
| +0x460 | Contact point |
| +0x454 | Signed query-center distance, centimeters |
| +0x864 | Pointer to coarse terrain query cache |

The original cache payload is patch pointer at +0, U/V cells as u16 at +4/+6, triangle half at +8, query kind at +12. Native cache replaces the pointer with packed resource identity. Cache updates happen as candidates are visited, including candidates that do not become the final selected contact.

## Validation

```sh
.venv/bin/python tools/test_terrain_contact.py
.venv/bin/python tools/test_terrain_reference.py
.venv/bin/python tools/terrain_cache_probe.py
.venv/bin/python tools/audit_terrain_float.py
```

Development-only original-instruction tests execute the owned game's extracted original routines and VU microprograms. Console runtime code is never linked into the Metal app.

- 998 real-patch refinement cases: **zero difference** in point, normal, or UV against original `0x32E9A0` instructions.
- 10,000 original VU triangle-inclusion cases: matching results (2,191 inside and 7,809 outside).
- 1,000 original ray-plane cases: matching membership; all 201 hits have identical point and fraction.
- 3,000 original cruise candidate-selection cases: matching selected candidate.
- 11 original cached grids: **all 3,300 coordinate floats identical**.
- Eight live glide/brake/charge/jump/turn checkpoints queried through the native API: original contact points, normals, and UVs **identical**, with source-to-native unit conversion applied only after the source query.
- Eight live neutral/brake/charge/jump/turn snapshots: source-operation contact points identical, normals within 1.46e-7. This secondary audit uses saved original UV only to identify the coarse seed cell, so it does not independently establish candidate selection.
- 339 independent polynomial probes, synthetic curved/stacked surfaces, out-of-domain misses, and a folded-patch cached-cell preference case pass.

The turn-left frame-30 query uses the independently captured frame-29 tangent. Its point, normal, and UV match exactly. This resolved the 1.23 cm discrepancy caused by using the post-query tangent. Scalar DIV/SQRT use nearest rounding and scalar ADD/SUB preserve the EE alignment guard bit; VU arithmetic retains chop. Remaining fidelity work includes scene/streaming candidate visitation order and nonterrain priority, full surface/material response, scenery collision primitives, and broad gameplay validation. These tests do not establish a fully matching game.

The airborne trajectory kind1 ray follows `336850 / 335960 / 32B6E0`: it retains raw endpoint bounds, uses the 9×9 grid, returns coarse triangle point/normal and cell-center UV, and skips Newton refinement. F12 at `32E100` is the preferred fraction (1), not a collision radius. Mode2 (`3378C0`) omits terrain. `test_trajectory_terrain_reference.py` verifies 300 queries against original captured caches, including 111 contacts with exact point/normal/fraction/UV. The native adapter also queries authored static scenery through separately recovered ray kernels; dynamic callbacks and saturated candidate arrays remain explicitly incomplete.

## Surface selection and material transitions

The authored patch surface is the signed 16-bit field at patch+8. Original 0x335960 copies it into query result+4C; 0x13D1B8 writes that value to rider+438, mapping -1 and missing contact to surface 0. `TerrainPatch::authoredSurface` and `RayHit::surface` now preserve that path, including generic terrain queries. The mesh loader requires the existing authored_surface_id metadata.

The force phase 0x13D818 caches the surface pointer at 0x13D8F4 before movement. Contact 0x13D64C changes the surface ID, and 0x13EBF4 velocity correction branches on the new ID. However, passive takeoff height at 0x13ED28 and alignment at 0x13EE84 still read the old cached surface's +10/+38. The native rider therefore passes the new ID separately to velocity correction and applies the new material after alignment. Next-frame forces use the newly contacted material. Applying a material deliberately preserves character statistics, global control curves and the speed limit already computed at frame begin.

`reference_ground_profile.py` exports all 19 surface records, including used material fields and all 44 raw words per record. `replay_io.mm` loads and binds the catalog. Missing catalog entries during a transition produce `groundSurfaceProfileMissing`; they are not silently reported as supported. Original airborne landing surface selection remains separate pending the original landing controller.

`test_surface_ground_reference.py` compares 12,000 complete original force-prefix/postcontact cases across all 19 actual captured material records with bit-identical results. It uses isolated local oracle copies and writes separate artifacts, preserving the shared ground goldens. `test_ground_surface_transitions.py` checks 437 loaded floats against original raw words and six constructed two-frame crossings between adjacent planar patches carrying real material profiles: 0→2,2→3,3→7,7→0,0→13,13→0. The tests verify old-material first-frame forces, new-material next-frame forces and retained frame-begin speed limits. These constructed crossings do not replace future live course-transition captures.

## Board landing probe

`landing_contact.hpp` recovers the query constructed by `13A7B0`. The center is the current posed board-root bone selected by rider+8A0 (index 22 for Zoe). Its endpoints are center minus/plus 200 cm times the final presentation-up at rider+180. These are not actor-position rays and do not use the physical board-up or previous terrain normal. The source kind is 2, so the original 9×9 coarse grid is followed by four Newton refinements with the rider's cached cell preference. Candidate selection uses coarse fraction nearest 0.574999988079071, with the generic instance/terrain resource tie break; it does not use the cruise normal-alignment preference.

The adapter preserves source point, normal, surface velocity, authored surface, instance/node or patch ID, and UV. It supports authored static rideable instances and exposes incomplete coverage for unsupported callbacks and saturated candidate arrays. `BodyCollisionVolume::landingCenterCm` and `reactionFrame.up` provide the native-generated pose inputs; `airPivotCm` keeps the airborne presentation pivot on that same pose boundary. The touchdown state machine is a separate consumer of this query.
