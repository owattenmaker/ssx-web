# Terrain render/contact fidelity

The browser's installed render vertex buffer matches the native package. All
1,913 terrain patches use an8x8 grid (128 triangles each); the contact code queries
the authored bicubic surface. Flat triangles can therefore cross above or below
the surface used to place the board.

`tools/audit_terrain_tessellation.py` verifies installed triangle indexing against
patch provenance, then compares244,864 triangle-centroid samples with the analytic
surface. It also compares vertical height where the analytic X/Z lies inside the
same rendered triangle.678 patches have sampled render-above-surface separation
over10cm;183 exceed50cm. The largest sampled vertical gap is3.05m on a steep
patch whose Euclidean centroid error is0.30m. Vertical error is not equivalent to
normal separation and is amplified on steep slopes. These samples are not a
maximum-error bound and do not yet identify the exact patch in the observed
rider-clipping frame.

`tools/terrain_tessellation.py` computes a conservative Euclidean approximation
bound using the convex hulls of bicubic Bernstein second-derivative control nets.
For grid spacing h, the vector interpolation error is bounded by
h²((Muu+Mvv)/8+Muv/4), from triangle barycentric Taylor remainders. Numeric checks
cover affine, quadratic and mixed terms and11,478 authored samples. This does not
include floating-point vertex-storage error or establish seam compatibility.

A provisional uniform-per-patch2cm plan at8/16/32/64 subdivisions would require
5,594,745 vertices and10,799,104 triangles, with76 patches still above the bound
at the64 cap. It is diagnostic, not an acceptable automatic global replacement.
The next implementation needs localized/adaptive refinement, shared-edge treatment
and a rendering budget, while preserving authored UVs and event visibility.
Physics must not be lifted or snapped to compensate for a render approximation.

Artifacts: `local/browser-validation/terrain-tessellation-audit.json` and
`terrain-resolution-plan.json`. No render mesh or physics behavior was changed
by this investigation.

## Actual neutral-event replay localization

`web/audit-rider-terrain.mjs` now runs4,500 ticks of the normal Snow Jam event
start for Sam/Zoe, sampling grounded contact provenance, board center and installed
render triangles. `terrain_contact_info` is a read-only core API exposing patch,
UV, contact point/normal and grounded mode. The script also inverts the bicubic
X/Z mapping near the contact UV to evaluate the analytic surface at the actual
board X/Z, separating board penetration from render approximation error.

Both riders initially produced identical contact/board results:1,020 grounded
samples,1,010 matching patch-triangle intersections,176 samples with rendered
surface over10cm above the board and15 over50cm. The largest is tick2172
(36.2s), patch149512: render0.7554m above board, analytic0.7472m above board,
only0.00819m render error at that location. Physics signed contact distance is
-43.18cm; board normal separation is-41.47cm. This is not primarily tessellation.

That patch is authored surface2 (powder). Original depth_target3=50.0034cm,
scaled to42.5029cm for this rider. Surface3's target is50.9912cm after scale.
Thus the largest sampled sinking is consistent with intentionally deep powder;
do not lift the rider or stiffen the solver merely to remove it visually.
Surface0 still has25 samples over10cm, with render error up to24.7cm; surface4
has8 such samples, with render error up to26.3cm. Static surface targets are not
current filtered depths: larger penetration immediately after a powder transition
must be checked against live depth1/depth3 before calling it a hard-surface bug.

The Sam report includes per-surface summaries and worst frames for follow-up.
Artifacts: rider-terrain-RIDER_SAM.json / rider-terrain-RIDER_ZOE.json in
local/browser-validation. Reports are diagnostics, not runtime inputs or proof
of original frame-image parity. Current next step: distinguish material-transition
sink allowance from geometric error on the hard-surface examples, then refine
those rendered patches with seam/budget handling. No physics or mesh correction
has been applied in this localization step. Full npm/build pass.

## Filtered depths and stitched refinement builder

The contact diagnostic now also exposes current filtered depth1/depth3. This
confirms why static hard-surface targets were misleading: tick2313 on surface0
still has depth3=39.17cm after powder, with board normal penetration36.93cm.
The transition should not be replaced with an instantaneous hard-ground snap.
At tick3261, surface4 patch112136 has depth3=1.859cm and the board is0.516cm
ABOVE the contact plane, yet the coarse rendered mesh is0.247m above the board.
Its local mesh approximation error is0.263m. That is a firm render-refinement case.

`web/terrain-mesh.js` now builds a renderer-only bicubic patch with independent
edge subdivision counts. A coarse patch bordering a finer one inserts matching
edge samples and triangulates affected cells as center fans; untouched cells keep
the original two-triangle topology. It evaluates authored positions, derivatives,
base UVs in00/01/10/11 order and lightmap UVs. No collision state is altered.
The live loader does not use the builder yet: distance/adjacency selection, actual
shared-edge matching, caching and scheduling still need integration.

`test-terrain-mesh.mjs` checks a curved synthetic8/32 neighbor pair for identical
boundary vertices, complete parameter-space coverage, positive triangle winding,
no interior open edges and UV preservation. Four fixed QA sites from the actual
neutral replay reduce absolute render-height errors from0.186..0.263m at8x8 to
0.0079..0.0155m at32x32. These tests do not establish matching for every authored
edge or bound the entire course. QA sites include source hash and are never
runtime rider inputs. Full npm/build pass. Mesh/physics rendering behavior remains
unchanged until the live refinement system consumes this builder.

## Authored adjacency and bounded detail planning

`tools/prepare_terrain_render.py` publishes renderer-only patch descriptors from
the analytic coefficients and installed material/UV/color data. It validates the
8x8 source indexing and constant per-patch colors. This preserves the existing
cross-console lightmap choice rather than silently reverting to raw PS2 light UVs.
`terrain-render.json` includes material IDs, affine light UVs, base UV corners,
Bernstein control bounds, baseline vertex/triangle provenance and neighbor links.

Whole-edge matching uses cubic Bernstein control nets and a spatial endpoint hash.
Only unique reciprocal matches are linked;3,437 shared edges match within a maximum
control-point gap6.41e-6m.762 edges have no whole-edge match and16 are ambiguous.
Those are not all proven open boundaries: partial joins and overlapping variants
remain unresolved. No forced adjacency is fabricated. Tests sample every linked
curve in both orientations and verify reciprocal indices.

`web/terrain-detail.js` now plans nearby refinement, prioritizing the contact patch,
with default radius30m, maximum8 refined patches and32x32 detail. Only patches
whose four neighbors are unambiguous are eligible. Coarse neighbors receive the
matching fine edge count; the affected set is at most40 patches. Tested replay
sites are fully matched and prioritized. This is a bounded execution plan, not a
claim that32 subdivisions satisfy the conservative error bound everywhere.

The package test reconstructs154,953 base vertices: maximum positional discrepancy
0.000488m from arithmetic/storage rounding; base/light UVs match exactly. Synthetic
stitching and replay-site refinement tests still pass. Package preparation is in
setup and package/planning checks are in npm test. Full npm/build pass. The next
step is worker-side generation/cache and atomic replacement of affected render
patches while preserving the original collision assets. No live mesh replacement
has yet been enabled, and unmatched-edge handling remains an open fidelity task.

## Worker generation and bounded cache

`terrain-worker-core.js` now validates complete detail plans, checks reciprocal
edge resolutions and rejects refinement of unresolved edges. It builds meshes in
a worker-owned LRU cache with an8MiB default cap. Only cached vertex/index/color
buffers count toward that cap; returned batch copies and renderer GPU allocations
are separate. Transfers clone cached buffers first, so sending a mesh cannot
neuter the cache. A batch has at most40 affected patches.

The async worker handler yields between groups of four patches, allowing a newer
build or clear/init message to supersede in-flight work. It publishes a complete
batch or cancellation/error, never a partial mesh set. The eventual client still
must reject stale response IDs: a completed old request can arrive after the
client changes its desired plan. `terrain-worker.js` is the browser module-worker
transport. No renderer mutation is performed by these modules.

`test-terrain-worker.mjs` verifies byte-budget enforcement, cache reuse after
transfer detachment, complete batch counts, generation cancellation, clearing and
invalid edge/identity rejection. `terrain-worker-test.html` runs the actual bundled
browser worker and validates copied data/cached results. One21-patch QA request
used799,608 cache bytes and took19.4ms cold/0.3ms cached off the main thread. These
single-request timings do not establish game frame-time, GPU upload cost or power.
Report:local/browser-validation/terrain-worker-browser.json. Full npm/build pass.
Next implement the client scheduler and atomic render-only replacement/suppression
of coarse triangles. Current gameplay terrain remains unchanged.

## Live atomic render replacement enabled

`terrain-overlays.js` maps the actual coarse terrain draw indices back to authored
patch ranges. It constructs/validates an entire replacement batch before changing
rendering, then suppresses only the replaced coarse triangles using degenerate
indices. Clearing restores exact original index values. Unchanged geometry is
reused; replaced geometry is disposed while shared original materials/textures
remain owned by the course. Original collision/source files and WASM collision
buffers are not modified.

`terrain-refinement.js` now initializes the worker/cache during course loading,
plans at most10 times per simulation second (or immediately when contact patch
changes), rejects stale reply IDs and commits complete results between render
frames. Reset clears overlays and supersedes pending work while retaining worker
cache contents. Failure leaves the previous valid rendering in place and exposes
a diagnostic error. Only the previously validated fully linked patch set is
eligible; unresolved/partial edges are still excluded. DOMterrainRefinement and
terrainResources identify current overrides for QA, not gameplay UI.

Actual course tests map all1,913 patches. A21-patch batch suppresses exactly2,688
coarse triangles and restores them byte-for-byte. A raycast against the committed
replacement at the known firm-surface clipping site is within3cm of the analytic
surface. Tests also cover failed-batch atomicity, geometry reuse and material/
origin handling. Full npm/build pass. Live WebGPU and WebGL2 runs show active
nearby replacements with no worker/mapping errors; restart was exercised and QA
races paused afterward. This enables rendering refinement, not a physics change.

Remaining: actual authored partial-edge/ambiguous handling, broader crack/pop and
course coverage checks, GPU upload/frame-time measurement, and dynamic detail
quality/budget tuning. The conservative error bound is not met everywhere by the
current32x32 cap, and intentional powder sinking remains unchanged.

## Coarse index uploads restricted to dirty ranges

`markTerrainIndexRanges` now coalesces affected triangle ranges and includes any
previous pending ranges before setting BufferAttribute.needsUpdate. This matters
when reset/replacement occurs before a draw or a coarse mesh remains culled: old
pending edits must not be lost. Both installed Three backends consume these
ranges in their attribute upload implementations. No vertex/collision data changes.

The actual21-patch course fixture schedules32,256 index bytes across34 ranges in
28 buffers, instead of82,296 bytes for whole-buffer uploads. This is an upload-volume
measurement, not a frame-time or power result; extra API ranges can have overhead.
Tests simulate queued uploads, overlapping/adjacent edits and exact restoration,
alongside existing atomicity, provenance and raycast checks. Full npm/build pass,
and live WebGL2/WebGPU refinement was inspected without reported terrain errors.
Owned QA runs paused afterward. Broader performance and transition validation
remain open.

## Client reply ordering and complete-plan validation

The transport/render dependencies of the terrain client are now injectable for
ordering tests. Before committing, it verifies that every returned mesh matches
the requested index/resource/resolution/edge counts and that the batch is complete.
The original ID guard remains: results from a pre-reset, superseded or disposed
request cannot commit. Refinement statistics now describe committed meshes rather
than merely requested detail, and reset clears those statistics.

`test-terrain-client.mjs` directly controls delivery order and verifies pending
reset races, late old replies, disposal, incomplete batches and committed stats.
Included in the full npm suite. Full tests/build pass; the live demo initializes
and accepts valid worker results without terrain errors. This strengthens the
render transaction contract; it does not expand edge coverage or establish
performance/visual parity for the whole course.

## Detail-selection hysteresis

`audit-terrain-detail.mjs` records a4,500-tick neutral-event selection route. The
stateless planner changed plans240 times over727 checks, requiring1,337 new or
changed mesh keys. This is selection churn, not a measured visual pop or GPU cost.

The planner now optionally retains committed detail with a2m distance advantage
and a2m expanded exit radius. Contact priority,32x32 detail and the8-patch/40-
affected-patch limits are unchanged. Clearly nearer patches can replace retained
ones; old patches leave the expanded radius. The live client supplies only committed
refined indices, not in-flight requests, and clears history on reset.

Replaying the same positions with immediate commits reduces plan changes240→212,
new/changed mesh keys1,337→1,233 and removals430→402. These are selection-only
results; asynchronous timing, popping and frame/power gains are not established.
`test-terrain-hysteresis.mjs` covers cutoff jitter, stronger replacement, contact
priority and exit behavior. Client/edge tests, full npm/build and live refinement
startup pass. Reports:terrain-detail-churn.json and terrain-hysteresis-comparison.json
in local/browser-validation. No physics, geometry polynomial or texture change.

## Original PS2 terrain/scenery combine (colour fidelity)

The browser world looked dull because it drew terrain as `base(sRGB) * GC lightmap
* 1.75` in linear space and scenery as `texture * c5/31`, and skipped the
full-screen ScreenTint pass. The recovered PS2 pipeline works on raw GS bytes:

**Terrain patches** (`38B0F8` list walk → `38CA70`/`38CE20` DMA builders).
Patch word `+0C` holds three 3-bit layer types (ARA1: `0x29` = base type1 +
light type5, `0x1A9` adds type6 on 87 patches down the whole course, not only the start ramp; docs/visual-parity.md 41); `+1A0/+1A2/+1A4` are
the layer textures. The builders set the material state `[renderer+E84]` that
`363C20` turns into GS registers: word1 bits2..6 = ALPHA_1 enum 1, bits7..11 =
ALPHA_2 enum 8; word0 bits4..5 = CLAMP_2 clamp/clamp; TEST_1/2 = ATE with ATST
ALWAYS (`3626D8`). The blend table at `491FB0` (`362478`) gives enum1 = `0x2A`
(`Cs`, opaque) and enum8 = `0x81` (`(Cd-Cs)*As>>7`). `364CD0` mode 2 uploads VU1
GIF templates with PRIM `0x5C` for context 1 **and** `0x25C` (CTXT=1) for
context 2 plus a constant vertex RGBA `0x80`; the patch display list carries one
V3-32 position block and two V2-32 UV blocks (base UV, light UV). The saved
glide-120 render queue (`*(renderer+18F0)`) contains the terrain records
(`0xB0, 0x814404, …`, texture pair 12/927). Base and light descriptors are
TFX0/TCC1 (`strm_tex`, `strm_lpg` PSMCT32). Hence per pixel:
`C = clamp(((T_base - L.rgb) * L.a) >> 7)`, with the **PS2** light page
(PSMCT32, alpha 0..255 so the factor can reach ~2x) sampled at the **PS2** light
UV (`patch+10`), not the GameCube atlas. This is exactly the CPU environment
formula of `2EDB20` without its rider tint. The 1.75 GC gain is retired.

**Static models** (`37E238`, material state `37F2A4..37F6C8`): TFX0 MODULATE
with the VIF UNPACK V4-5 colour, i.e. `Cs = T * (c5<<3) >> 7`, `As = Ta *
(bit15<<7) >> 7` (the sky's recorded unity 16/31 is the same rule). Model header
`+10` bit3 selects additive `0x48` (`Cd + Cs*As`), ATST GREATER 20. Otherwise
material word `+0C` (group flag bit3 adds `0x40000`) `& 0x660000`: 0 opaque/no
test; `0x20000` `0x44` + ATST GREATER 92; `0x40000/0x60000` `0x44` + ATST GREATER
20 (depth-sorted). All tests use AFAIL FB_ONLY. Static-model vertex alpha bit15
is clear on 1,420 ARA1 vertices (searchlight/pinlight/snowstream fades).

**Texture addressing** (same material word, `37F2BC..37F354`): `word & 0x180000`
sets `[renderer+E84]` word0 bits 2..3, which `363C20` passes to `3625C0` as GS
`CLAMP_1`: `0x80000` (flags bit3) = WMS CLAMP (u), `0x100000` (flags bit4) = WMT
CLAMP (v), neither = REPEAT (mode 1 -> 4, 2 -> 1, 3 -> 5). Every ASKY..ESKY sky
material has both bits (flags 25/27/31) and the sky's own state push (`353B10`,
word0 `|= 0xC`) is clamp/clamp too. The browser sampled the sky with Repeat (the
package default), so bilinear filtering mixed each edge with the opposite edge:
a line along the bottom (and top) of the mountain ring, whose top rows are
transparent and bottom rows opaque, a dark line at the top of the band below it,
and vertical seams between the 45-degree ring segments, on every course.
`createOriginalSkyMaterials` now applies `originalModelWrap(flags)` (2026-09-26,
`web/test-sky-addressing.mjs`). World static models do the same per material:
since 2026-09-26 the packages carry them per batch (`wrap`, docs/presentation.md section 6; CRA3 rock polys, one
ABA1 log mesh, CHP2 volumes) and `world-material.js staticModelTexture` samples them clamped.

**ScreenTint** (factory 7, ctor `2BC890`, vtable `484B70`, blend `2BD1B8`,
defaults `2BE170`, compare `2BDC78`) runs through the same driver `2C0778` as Fog
with the camera X/Y from `2ED490`. `2F00A0` copies scale/add to `gp+14F4..1508`,
`390458` into render context `+6CA4`, and `363490` draws it with `3904A0` right
after the fog composite `36AC00`. `3905E8`: skipped for identity; otherwise the
framebuffer is re-read as a PSMCT32 texture (TEX1 nearest, MODULATE, no blend)
with vertex RGB `trunc(scale*127.5)`, then an optional `0x48` sprite adds
`trunc(add*127.5)`. ARA1's only painter record (chunk 33, track 8) has 8
payloads; e.g. start `(1.1,0.85,1.0)`, glide `(0.9,0.85,1.15)`, later identity.

### Browser implementation

- `web/prepare-environment.py` now writes `terrain-light-atlas.png` (47 raw PS2
  light pages, 1-texel clamp gutters), `terrain-lighting.json` (per-patch page +
  PS2 light UV) and `screen-tint.json` (point tree + payloads).
- `web/prepare.py` tags static-model batches with the recovered blend class
  (`world-batches.py` keeps them separate) and writes `vertex-alpha.bin`
  (V4-5 alpha bit; `colors.bin` stays byte-identical). `tools/world_models.py`
  exposes the MDR group flags.
- `web/world-material.js`: TSL node materials (WebGPU and WebGL2) for terrain
  (ctx1 base, ctx2 `0x81` light), static models (byte MODULATE, per-class
  blend; FB_ONLY is two draws of the same range: passing texels write depth,
  failing texels blend without it) and the sky dome. GC uv1 maps to the PS2 page
  by a per-patch affine transform, so refined overlay meshes get the same
  attribute (`terrain-overlays.js` calls the material's geometry hook).
- `web/screen-tint.js` ports the tree query, driver and blend; `main.js` steps it
  once per camera tick after `_step_camera_head`; `fog-renderer.js` applies the
  byte-exact tint to its encoded output. `?originalWorld=0` restores the old
  materials and skips the tint for comparisons.
- Tests: `test-world-material.mjs` (all 154,953 terrain vertices map to the PS2
  light UV exactly, page/texture identity, vertex alpha, blend classes) and
  `test-screen-tint.mjs` (5 snapshot payload selections; one driver step from the
  captured glide instance reproduces glide+1 to 2e-7 and its travel distance).

### Measured against the PS2 frames

Browser tick 458 (`?qa=1&qaGlide=1`, advance 120, WebGPU; WebGL2 identical within
0.1) downscaled to 640x480 against `snow-jam-glide-120.png`, 553 16-px blocks
excluding HUD, sky and rider:

| Render | Mean RGB | Block MAE RGB |
|---|---|---|
| PS2 | 99, 102, 163 | – |
| before | 90, 98, 117 | 23.1, 21.9, 47.1 |
| PS2 terrain combine only | 114, 123, 151 | 17.6, 23.5, 14.4 |
| + ScreenTint + model classes | 104, 106, 168 | 9.7, 9.0, 9.1 |

Sampled regions (PS2 → before → now): shaded foreground snow
`(45,54,126)` → `(58,71,96)` → `(45,54,126)`; mid slope `(134,130,183)` →
`(106,108,112)` → `(134,131,186)`; start ramp snow `(160,152,202)` →
`(110,112,114)` → `(157,150,202)`; teal tent (static model) `(64,170,219)` →
`(54,152,154)` → `(63,170,222)`. Remaining block error is mostly geometric
misregistration (camera/rider differ slightly). At tick 339 the camera framing
differs more; shaded foreground is `(54,58,131)` PS2, `(64,73,97)` before,
`(46,58,139)` now. The glide QA fixture starts the ScreenTint with an initial
application (`0.9,0.85,1.15`) whereas the PS2 was still transitioning
(`0.977,0.85,1.093`); by tick 458 the browser tint is `0.912,0.868,1.132` vs PS2
`0.925,0.873,1.120`.

### Remaining gaps

- Layer type6 (texture 62) on 87 ARA1 patches along the course (3,895 on the disc, EBC3 1,712; `38D168`, patch word
  `0x600000`, "Disable Patch Reflection" debug toggle gp+0x13E8 = 0): a VU environment map (UV = (n, 1) x E, E = B(beta)
  A(alpha) M built in `38B370` at terrain `+360`, alpha = -0.0005 camY, beta = -0.0005 camX, M at `0x5049C0`) blended
  ALPHA_2 enum 17 = `0x58` (`Cd + Cs*Ad>>7`) is not drawn. Ad is the base texture's alpha: the frame setup `382AF0` sets
  FRAME_2 FBMSK `0xFF000000`, so the context-2 passes never write alpha. On Snow Jam it is a faint glint (+0.6..1.8 on
  average at the opening frames). Now ported behind pv `terrainGlint` (web/world-material.js glintBytes, terrain-glint.json):
  docs/visual-parity.md 41.6.
- Patch flag `0x800000` (`38D690`, distance-scaled per-patch extra draw, likely
  sparkle) is not drawn here. Rider shadows: see "Rider shadows" below.
- Static-model blends run in linear framebuffer space, not the GS encoded
  domain (glows/fringes differ), unless pv `encodedBlend` is on: then the world
  pass holds encoded bytes and every blend is the GS's (web/frame-space.js;
  visual-parity.md section 38).
  Per-instance distance fade (`37E238` f12 → FIX alpha) and culling are not
  ported, so e.g. a magenta searchlight beam and `cameraflash` models (black at
  rest) remain visible where the PS2 shows neither (cameraflash is now hidden
  with the runtime-bit0 helpers, see the light glow section). Env-map material variants
  (`0x200000/0x400000` flags, 9 materials) use only their base class.
- Mip/filter settings (TEX1) and exact GS rounding of bilinear samples are not
  reproduced; GS dithering is not emulated.

## Sun glow and lens flare (2026-09-22)

The PS2 frames facing the sun show a huge white glow plus coloured flare rings;
the browser drew neither. Recovered from SLUS_207.72 and now drawn by
`web/sun-flare.js` (package from `tools/export_sun_flare.py`, test
`web/test-sun-flare.mjs`).

**Painter.** tWPIGD_Sun is world-painter type 9 (factory `2C0408` case 9, ctor
`2BC910`, vtable `484700`, blend `2BD378`, compare `2BDD38`, reset `2BE1A8`,
getters `2C1560..2C15A0` reached through `2EE070..2EE2B0`, environment slot +14
of the `4FA370 + view*F0` block; views 6/7). Payload (10 words): rate,
elevation deg, azimuth deg, R, G, B, glow alpha, texture index (int), flare
alpha, glow half size. Blend weight is squared as for ScreenTint; the texture
index is lerped as a float and truncated. ARA1 paints one payload (chunk 33,
track 8): elevation 9.15, azimuth 160, white, glow alpha 0.95, texture 0,
flare alpha 0.85, half size 280; live instance values in the event-start
savestate match. Reset defaults: angles 0 (then 107/16 deg from gp-38BC/-38B8),
colour 1, alphas 1, texture 0, size 1.

**Sun object** (owner+`6070`+view*210, owner = `*(game+1C)`, update `2E3338`,
draw `2E3478`, both from `2E30D0` via `22C790`):
- `2F4DB8` (per camera): texture index -1 hides it. Direction
  `(cos az cos el, sin az cos el, sin el)` (Z up), point = eye + dir*(far - 500 cm)
  (far = camera +58, 300 m). Outcode test `37DBE8` hides it unless the point
  projects inside the viewport; otherwise +1F0 = screen xyz (`37DD20`, integer
  pixels of the 512x448 viewport) and a 16x16 query rect `(x-8, y-8)` is clamped to
  the viewport for reading.
- Visibility: `2E3130` (renderer callback registered by `2E2E18`) calls the query
  object at owner+`649C` (vtable `487F00`, slot +24 = `2EC478`): a GS local->host
  transfer of the Z buffer rectangle, counting pixels whose Z does not exceed the
  sun's Z. vis = open/256 when the whole rect is on screen, otherwise
  max((open-128)/128, 0), stored at sun+204.
- `2F4A08` glow: FX texture 51+index (`sun1`/`sun2`, 128x64, EFFECTS.SSH), one
  sprite via renderer `377CF0` centred on the sun with half extent +48 (320 when
  +48 == 1), vertex ARGB `trunc(128*(vis*glowAlpha, R, G, B))`.
- `2F4690` flare: nine sprites (table from `2F43E0`, colours from gp pool via
  `2F6BC0`/`2F7804`) of the 256x256 `lens` atlas (FX 45; quadrants ring / soft
  blob / star / disc) at `centre + (sun - centre)*t`, half extent `300*s`,
  A = `a*vis*flareAlpha`.
- Blend: material ALPHA_1 enum 7 = GS `0x48` (Cd + Cs*As>>7), MODULATE, and
  word2 priority 8. `363490` splits the sorted render list at priorities 6 and
  9 (`4A4058`): layer 0 (world, board track priority 3) -> fog composite
  `36AC00` -> layer 1 (priorities 6..8) -> `36C790` + ScreenTint `3904A0` ->
  HUD. So the sun is added after fog and is then tinted (the PS2 sun core is
  (243,235,255), i.e. white clamped then ScreenTint-scaled).

**Browser.** `sun-flare.js` ports the painter tree/driver (stepped next to
ScreenTint), projects the sun with the displayed camera, runs the 16x16 depth
query as a 1x1 GPU pass on the world depth right after the world pass, and adds
the ten sprites in the fog compositor's encoded byte stage before ScreenTint
(`fog-renderer.js`). `?sun=0` disables it; `ssxQA.sun()` reports state.

Measured (event-start pad replay, WebGPU, 640x480, 16 px block MAE excluding
HUD, with -> without the sun):

| PS2 frame | web tick | sun (web / PS2 memory) | full frame | top-right quadrant |
|---|---|---|---|---|
| tick618 | 618 | (377.9,51.7) / (378,52) | 4.3,3.9,4.8 <- 26.7,17.0,8.6 | 2.9,2.7,2.7 <- 97.6,59.8,18.7 |
| tick631 | 631 | (377.8,34.1) / (377,31) | 5.1,4.7,5.1 <- 24.4,15.9,8.1 | 6.2,5.7,5.3 <- 88.0,54.1,18.4 |
| tick588 (png lags ~2 ticks) | 586 | (401.1,4.7) / (396,13) | 7.8,13.9,22.4 | 11.9,7.7,4.0 |
| tick498 / 419 / 268 | same | hidden / hidden | unchanged | unchanged |

Top-right mean at tick 618: PS2 (189,187,245), web (190,187,248), before
(75,118,227); sun core (243,235,255) in both. The savestate PNG is a few ticks
older than its memory tick, so compare by sun position, not by label.

**Effect layer order.** The snow emitters (`370950`/`371380` with a3=7),
wake (`2DDC4C`) and boost strips (`2E7BDC`) also set word2 priority 7: they
are drawn after the fog composite, not fogged. `fog-renderer.js` now fogs only
the world and seeds the encoded effect composite (`snow-composite.js` `seed`)
with the fogged world, so these effects blend onto it unfogged (the board track
stays priority 3 in the world pass). Near the rider the fog alpha is ~1, so the
event-start frames change by <1 level; the difference shows on spray or trails
in front of distant terrain.

Open: GS sprite rasterisation/bilinear rounding is approximated. The light
glow halos and `36C790` are covered in the next section.

## Light glow halos, never-drawn helper instances, 36C790 (2026-09-22)

The countdown frames show two bright halos on the stadium roof lights; the
browser drew none. Recovered from SLUS_207.72 and drawn by `web/light-glow.js`
(package `tools/export_light_glow.py` -> `web/public/assets/LIGHT_GLOW/`, test
`web/test-light-glow.mjs`, `?glow=0` disables it, `ssxQA.glow()` reports the
frame's glows).

**Sources.** World records kind 7 (80 bytes, 128 in ARA1; runtime entity type 8
in the per-cell entity lists): `+0C` flags (`& 0x70` = halo class; ARA1 has 82
class 0x10 and 46 class 0x20), `+10` RGB, `+1C` position (cm, Z up), `+28..+3F`
bounds, `+40 + view*4` visibility (runtime). `22A4A8`/`22A770` append the type-8
entities of the visible cells to `+7BC0/+7BC4`; `22C790` rebuilds the per-view
glow list at owner(`*(game+1C)`)`+60+view*4` / `+68+view*3000` (0x30-byte
records) with `2E2F98` (clear) and `2E2FF8 -> 2E2FA8` (add; record `+20` =
source, `+2C/+2E/+28` cleared), then runs `2E30D0` -> `2E3338` (update) /
`2E3478` (draw). The live event-start savestate list (36 records) matches the
authored records byte for byte.

**Update `2E2B00`** (per record, view camera at owner`+10`): `37DBE8` outcode
(inside the view volume or `+2C = +2E = 0`), `37DD20` integer screen x/y and Z
(24-bit, 2^24 at the 30 cm near plane, 0 at the far plane), zf = Z * 2^-24
(gp-3A40). zf <= 0.005 (gp-3A3C, ~50 m): far glow, no query. Otherwise
`+2E = 1`, count rect `+0..+C` = (x - w/2, y - h/2, w, h) with
w = clamp(trunc(zf*800), 1, 16), h = clamp(trunc(zf*400), 1, 8); read rect
`+10..+1C` = (x-8, y-4) clamped to the viewport, 16x8; `+24` = Z of the light
pulled toward the camera by 100/80/200 cm (class 0x10/0x20/0x40, i.e. view
depth - pull); `+28` = ((x - vx)*2/vw - 1) * pi/2 (gp-3A38), the sprite
rotation.

**Visibility.** `2E3130` (renderer callback `2E3110`, registered through
renderer vfunc `3A8` = `3866E0` and called by the frame-flip state machine
`382760` state 2, i.e. after the whole frame) runs `2EC478` for every `+2E`
record: GS local->host transfer of the read rect of the PSMZ24 Z buffer
(ZBP 7168, width 512), counting count-rect pixels whose Z <= the reference Z;
vis = open/(w*h), or max((open - wh/2)/(wh/2), 0) when clipped; stored at
source`+40+view*4` (the draw uses the value of the previous frame).

**Draw `2E2868`.** Material pushed by the query object's slot `+10` (`2E3578`,
template `501420`): ALPHA_1 enum 7 (GS `0x48`, Cd + Cs*As>>7), MODULATE,
word2 **priority 7**, ATST ALWAYS (bits 20..21 = 0, AREF 0x14); ZTST mode
(word1 bits 23..24, decoded by `3626D8`: 0 GREATER, 1 GEQUAL, 2 ALWAYS) is 2
for queried glows and 1 (depth-tested at the light) for far glows, whose
visibility is 1. Texture FX 53 `shal` (class 0x10/0x40, renderer`+1024`) or
54 `mhal` (0x20, `+1028`), 64x64 from EFFECTS.SSH; half size 180/100/350 cm;
vertex RGB = trunc(128 * normalize(colour)), A = trunc(128 * vis). Renderer
vfunc `278` = `3781A0` projects the light (P) and the points light +
size*(cos a * right + sin a * up) and light + size*(-sin a * right + cos a *
up) (right/up = columns of the world->view matrix on the renderer matrix stack
`+13E4`), and emits the triangle strip P-dx-dy, P+dx-dy, P-dx+dy, P+dx+dy (ST
0/1, Z of P) **twice** (`t2 = 2`). Priority 7 puts the halos after the fog
composite (`36AC00`) and before the sun (priority 8) and ScreenTint.

**Browser.** `light-glow.js` evaluates the 2E2B00 rules on the CPU per
rendered frame (displayed camera, same half-pixel `37DD20` convention that the
four live glows and the sun reproduce: pixel = trunc(continuous - 0.5)),
writes the rects into a float texture, runs the 16x8 query for every queried
glow as one 128x1 pass on the world depth right after the world pass, renders
the rotated quads additively (exact byte math, two copies, far glows
depth-tested) into a half-float target, and adds it in the encoded stage before
the sun (`fog-renderer.js`). Tests: package vs the live list, projection of
four live records to their exact count/read rects, rotation (EE `mul.s` rounds
toward zero: `ps2Mul`), clamps, visibility rule, colour bytes and corners.

**Never-drawn helper instances.** The first browser query found every glow
occluded: the 11-triangle `mdl_ARA1_RaceRideState_0` box (texture 17, orange,
GS alpha 36 > ATST 20, so it writes depth) encloses the start area at ~3 m and
was drawn by the browser, tinting the whole countdown view orange. The PS2
event-start Z buffer (decoded from the savestate GS dump, PSMZ24 at block 7168)
holds ~19 m at the light where the box would write 3.3 m. Its countdown runtime
flags (`+8`) have bit0 clear, as do 312 other instances, all helpers: reset
planes, fcollision/bcvolume, RaceRideState, emitter/trigger placeholders
(startfirePop/Trig, EZseq*, fireGush, riversplash, fallsmist, snowwind,
chasingdragon/spintwin trigger models) and the cameraflash models the section
above already listed as wrongly visible. `web/prepare.py` now passes them to
`world-batches.py` (`hidden_resource`, 347 batches / 5,080 triangles of 285
resources with geometry) and `main.js` does not add those meshes. The flag is
inferred from the captured flags and the Z buffer; the renderer-side test
(instance list `22C708` -> renderer slot `+10`) is not traced.

**Measured** (WebGPU, 640x480, 4:3; WebGL2 identical within 0.1). Savestate
screenshot (event-start.p2s, capture tick 0 = event tick 18, PNG ~2 ticks old)
against web tick 16, where the glow list matches the savestate records
(147,133)/(401,132): 80x80 boxes on the two halos, MAE RGB
before (helpers drawn, no glows) -> helpers hidden, no glows -> now:
left 70.2,36.0,56.1 -> 49.0,41.7,44.4 -> 14.4,16.0,22.7 (mean web
(93,92,141) vs PS2 (95,95,146)); right 71.1,39.3,65.2 -> 56.1,46.7,52.6 ->
20.2,18.7,28.6; 16-px block MAE (HUD excluded) 53.0,24.8,34.6 -> 32.4,24.2,29.3
-> 29.7,22.1,27.6. `event-start.tick79.png` (capture record 79 = event tick
97) against web tick 95: halos 65.6,35.1,57.3 / 72.4,38.4,64.5 -> 16.5,16.2,22.3
/ 22.5,18.5,29.8, blocks 40.9,13.5,24.2 -> 16.3,11.0,16.9. The browser
countdown camera sits ~1-2 m closer to the rider than the PS2 frame at that
tick (halos 6 px lower); the remaining block error is mostly the missing
computer riders and that framing. Far glows (e.g. two lights at event-race
tick 2316) draw as small depth-tested halos.

Not reproduced: the list comes from all 128 sources inside the view volume,
not from the visible-cell list (`22A4A8`), which only matters for glows whose
cells the PS2 culls; visibility is same-frame (PS2: previous frame); Z is
compared in view depth with the browser depth buffer, not 24-bit GS Z; the
second list (`+7FC4`, a3 = 2 in `22C790`) is empty in the captured states.

**36C790** (called by `363490` just before ScreenTint) is a painter-driven
framebuffer glare/bloom pass: gated by gp+12D8 (1 in all captured states),
parameters at render context `+6CD4 + view*1C` (7 floats, copied by `36C740`
from gp+12E4..12F4/1300/1304, which `2F00A0` fills from the environment slot
`+8` painter = **world-painter type 6**, ctor `2BC830`, vtable `484DA8`,
getters `2EEDB0..2EEF60`). It returns immediately unless one of
gp+12F8/12FC or context `+14/+18` (x127) is non-zero, otherwise downsamples
the framebuffer (`36B9D8`, 1<<gp+12E0), filters (`36C188`) and composites back
(`36C398`) over three levels. ARA1 authors painter types 1,2,4,5,7,9,11,12 but
**no type 6**, so the pass is inert on Snow Jam (context = 1,1,1,1,1,0,0 in the
savestates); 16 other locations author type-6 painters and 14 of them (e.g.
BRA2, CRA3, DBC2, EBC3, ESS3) have payloads with a non-zero last word. Now ported: see
[Framebuffer glare pass](#framebuffer-glare-pass-36c790-world-painter-type-6-2026-09-22).

**Other layer-1 producers (priorities 6..8)**, from the word2 priority stores
(`and -0x3E1 / ori p<<5`): priority 7 - snow emitters `370950` (ported),
wake `2DDC4C` and boost strips `2E7BDC` (ported), light glows `2E3578` (now
ported), `sprk` board sparks RFX+0x470 `2DABC8`/`2DB478` (now ported, see
[Impact and contact effects](#impact-and-contact-effects-2026-09-22)), the kind-5 fog-particle
puffs (`22C708` -> `2DC190` sprites -> `2DBF98` depth sort and `fog0` draw: now ported, pv `fogPuffs`,
docs/visual-parity.md 41), the avalanche draw `2D9130` (vtable `488664`, class ctor `2D5778`; docs/avalanche.md) and the static-model
path at `37ECB0`; priority 8 - sun glow/flare `2F4A08`/`2F4690` (ported),
`2EF950` (air streamers RFX+0xAD0, `strm`/`prbn`, not impact related), a timed
full-screen 2D overlay `2E47E8` (vtable `48817C`, ctor `2E4228`, 640x480 ortho,
alpha fade from +48/+54/+58/+5C: the control-9 forced-reset white fade, `engine/reset_fade.hpp`) and the model path `38057C`. None of these
shows in the captured event-start / event-race frames beyond what is already
drawn, so they remain unported and are listed here for the next pass.

## Framebuffer glare pass 36C790, world painter type 6 (2026-09-22)

Metro-City and The Junction frames bloom their bright snow; the browser drew nothing. Recovered
from SLUS_207.72 and drawn by `web/glare-pass.js` (package `tools/export_glare.py` ->
`web/public/assets/<X>/glare.json`, test `web/test-glare-pass.mjs`, GPU check
`web/glare-gpu-test.html`, `?glare=0` disables it, `ssxQA.glare()` reports the state).

**Painter (type 6).** Factory `2C0408` case 6, ctor `2BC830` (+0 = -99999, vtable `484DA8`,
+8..+2C = 1, +30..+3C = 0), blend `2BD068` (vtable slot 66; `w = weight^2`,
`cur = w*payload + (1-w)*cur` for seven current/sample pairs +8/+C .. +38/+3C; EE mul/add),
compare `2BDBD0` (slot 68, all seven equal), reset `2BE140` (slot 69: +0 = 0, values
1,1,1,1,1,0,0), no-op notify `2BDA50` (slot 67, called before/after the blend), serializers
`2BF700`/`2BE698`. Getters `2C14F0..2C1520` (slots 13..19) are reached through
`2EEDB0..2EEF60` (painter at `*(*(0x4FA370 + view*0xF0 + 8))`). The payload is 8 floats: rate,
then the seven values. Their names come from the debug menu built at `249200`
(gp addresses): `12D8` Enable, `12DC` Override World Painter, `12E0` PS2 Capture Size (2^x),
`12E4` Minimum Intensity Cutoff, `12E8` Post-Cutoff Scale, `12EC` Copy Intensity, `12F0` Frame
Source Intensity, `12F4` Frame Blend Intensity, `12F8`/`12FC` Blend Texture 0/1, `1300`/`1304`
Blend Texture 2/3, `1308` Texture Jitter. `2F00A0` copies the painter to `12E4..12F4/1300/1304`
unless Override is set; renderer vfunc `386640` (with `36ABA0` fog and `390458` ScreenTint)
runs `36C740`, which copies them to render context `+6CD4 + view*0x1C`. Retail values:
Enable 1, Override 0, capture 8, blend textures 0/1 = 0, jitter 2.0 (ELF and every savestate).

**Pass `36C790`** (per view, from `363490` after layer 1 and before ScreenTint `3904A0`):
1. Return if Enable is 0. `A = trunc(127.5 * (BT0, BT1, BT2, BT3))` (EE `cvt.w.s`); return if
   all four are 0. So only BT2/BT3 (painter values 6/7) can enable it in retail.
2. L0 (256x256, PSMCT24, in the Z buffer at block `ctx+5A80<<5` = 7168; L1/L3 at +0x400):
   `36B9D8` copies the 512x448 viewport (TBP 0) with bilinear filtering, eight 32-px column
   sprites, UV `(0.5 + 2x, 0.5 + 1.75y)` texels, MODULATE by `trunc(FrameSource*127.5)`, no blend.
3. `36C188`: untextured sprites over L0 with ALPHA `(Cd - Cs)*FIX >> 7`, `Cs = trunc(Cutoff*127.5)`,
   `FIX = trunc(gp+12E8 * 127.5)` (Post-Cutoff Scale): a threshold (colour clamp to 0..255).
4. For i = 0..2: if `A[i]` composite L_i; then `36B9D8` halves it (L1 128, L2 64, L3 32):
   four bilinear samples at `(-J,-J),(+J,+J),(+J,-J),(-J,+J)` with `J = trunc(Jitter*16)` = 32
   (1/16 texel units, i.e. +-2 texels) over a source span shrunk by 2J, each MODULATE
   `trunc(trunc(Copy*127.5)/4)` (`div.s` then `cvt.w.s`); sample 0 is written, 1..3 added with
   ALPHA `Cs + Cd` (FIX 128). Columns are `dstW/32` (signed divide: a 16-wide level draws
   nothing, only reachable with a debug capture size of 2^7).
5. If `A[3]`: composite L3. Composite `36C398`: 16 column sprites over the viewport, TEX = level
   (region clamp), UV `u = 0.5 + x*W/512`, `v = y*H/448` (the v start has no half-texel offset),
   MODULATE by RGBA `(A,A,A,A)`, ALPHA `Cd*FIX >> 7 + Cs` with `FIX = trunc(FrameBlend*127.5)`:
   every composited level also scales the frame by FrameBlend.
6. Restore FRAME/XYOFFSET (`36C790` tail) and chain the static restore packet at `0x44B200`
   (`368138`).

So with the painters actually authored (BT2 = 0 everywhere, BT3 = 1 or 0), the retail effect is:
threshold the half-resolution frame, blur it down to 32x32 with three 2x2-jittered box passes,
scale the frame by FrameBlend (0.75..1) and add the 32x32 glow scaled by BT3.

**Authored painters** (`export_glare.py --survey`, 16 of the 43 SDB world painter records):
ABC1, BHP1, BRA2, CBA2, CHP2, CRA3, DBC2, DRA4, DSS2, E, E_EBA3, E_ERA5, EBA3, EBC3, ERA5,
ESS3; E and E_EBA3 only author inactive payloads. ARA1 (and its connectors) has no type-6
section, so its painter stays at the reset values and the pass never runs. BRA2 (chunk 56 track
16) has 5 payloads, e.g. payload 1 = rate -0.1, cutoff 1.6, scale 1, copy 1, source 1.5, blend
0.9, BT2 0, BT3 1; BHP1 has 2 (cutoff 1.1/1.2, BT3 1). ESS3 is the only one with BT2 0.6.

**Verification.**
- `tools/test_glare_pass_native.py` runs the recompiled `36C790`/`36C740`/`36B9D8`/`36C188`/
  `36C398`/`368138` (`tests/glare_pass_reference.cpp`; `395330`/`395350` transcribed, the DMA
  allocator `38F460`/`38F668` replaced by an in-place writer) on the Metro-City glide-620 EE RAM
  (live context `1.2406, 1, 1, 1.2005, 0.9599, 0, 0.4010`) plus 39 synthetic painter/debug
  states (capture 2^7, jitter, BT0/BT1) and dumps the GS packets to
  `local/browser-validation/glare-pass-packets.json`. `test-glare-pass.mjs` decodes them and
  requires `glarePlan()` (FRAME, SCISSOR, TEX0, CLAMP, ALPHA, RGBAQ and every sprite UV/XY) to
  match all 40; the live case is also embedded.
- Painter blend against live memory: in the Metro-City glide-620 savestate the painter object
  holds sample = BRA2 payload 1 and current values exactly 9 driver steps (weight 0.1^2) ahead of
  the render-context copy; in The Junction glide state 14 steps (weight 0.08^2). The browser
  painter reproduces both bit for bit from the context values.
- `glare-gpu-test.html` (WebGPU and `?backend=webgl`) runs the GPU pass on a synthetic 512x448
  frame with the live Metro-City parameters and compares L0..L3 and the composited frame with
  the byte model `glareReference()` (GS bilinear with 4-bit weights, MODULATE `>>7`, blend
  clamps): 0 mismatched bytes on both backends.
- In game (`?course=BRA2&qa=1&qaGlide=1&rider=zoe`, 4:3, HUD hidden, painter seeded with the
  live context values) against the Metro-City glide-620 savestate screenshot, 640x480, 16-px
  block MAE RGB outside the HUD: glare off 25.9, 24.6, 40.2 -> on 22.3, 25.2, 31.0 (WebGPU and
  WebGL2 identical). Snow below the rider (140..300 x 380..470): PS2 (138,148,251), off
  (156,155,212), on (154,153,233); the right slope (380..520 x 300..460): PS2 (131,146,254), off
  (144,147,212), on (138,142,232). The rest is the base BRA2 world colour and framing (no
  opponents, camera offset), not the pass.
- The GS VRAM in the savestates is not usable for a buffer check (the frame/level pages are
  zero in `GS.bin`, hardware renderer).

**Browser.** `glare-pass.js` ports the painter (driver identical to ScreenTint/Sun, stepped
with them once per camera tick), `glareBytes()`/`glarePlan()` (the original draw list) and the
GPU pass: when the pass is active, `fog-renderer.js` renders the untinted pre-ScreenTint bytes
into a frame target, the pass builds L0 (fused copy + cutoff) and L1..L3 in 8-bit targets with
manual GS bilinear sampling (`textureLoad`), and a final pipeline composites the levels onto the
frame and applies ScreenTint. When inactive (ARA1, or BT2 = BT3 = 0) the fog renderer's single
pass is unchanged. The frame target has the canvas resolution: L0 samples it at the GS texel
positions scaled to that resolution (exact at 512x448) and the composite evaluates the GS UV at
each canvas pixel's continuous 512x448 position.

Not reproduced: GS dithering (DTHE state unknown), the exact GS UV interpolation precision (the
per-pixel UV is taken in float then floored to 1/16), widescreen viewports (the pass always maps
the 3D band to the 512x448 viewport), the debug-only capture sizes other than 2^8 in the GPU pass
(the plan and byte model handle them) and the level buffers living in the Z buffer (nothing
after the pass depth-tests against it in the browser).

## Event residency: connector terrain and the original draw rule (2026-09-22)

The Snow Jam race event keeps the course and both connector locations resident (A_ARA1 track 3,
ARA1 track 8, ARA1_B track 9; see [obstacle collision](obstacle-collision.md#snow-jam-race-event-world-membership-2026-09-22)).
The browser package now holds all 2238 patches (132 + 1913 + 193): `tools/import_world.py --event`
(default) imports the three locations in load order with their GameCube-verified light UVs (every
connector patch matched its GC surface), `export_environment_lighting.py` / `prepare-environment.py`
add their PS2 light pages (atlas 56 pages, still 1024x1024), `prepare_terrain_render.py` their edge
adjacency (3992 shared edges), and their 444 instances render through the same batches. Kind-9 texture
RIDs are global (the copies in every location chunk are byte-identical, the importer checks it); kind-10
light pages are unique per location. A_ARA1 lies uphill of the start gate (behind the race camera),
ARA1_B at the finish; none of the existing PS2 screenshots looks at them.

Draw eligibility now follows the original collectors instead of runtime bit0 alone: static instances need
`(flags & 3) == 3` (`22A5A0`), dynamic entities draw through vtable+0x20 (empty for DeadNode, RestoreNode
and type-16 nodes; `0x356298` needs flags & 4). This removes the 74 free-ride Big Challenge gates, flags
and arrows the browser drew in the race (absent in the PS2 frames, e.g. `setpieces/race.tick718` and
`.tick2719` with `BigCGate_1003`/`_1022` in view) and the 52 connector helpers (reset planes, load/unload
volumes, `hubsigninfobreak` 0x2).

Visual check (4:3): the browser replaying the `setpieces/race` pads to tick 718 lands within 0.1 mm of the PS2 rider and its frame matches `race.tick718.png` (ramps, gates, no Big Challenge gate at the ramp where `BigCGate_1003` stands 97 m ahead). A new capture `local/ps2-capture/runs/membership/uphill` (countdown anchor, 150 neutral, 40 ly, 240 hard left, rider turned to face uphill under the stadium) shows the uphill horizon: 58 A_ARA1 sample points are in its frustum at tick 469, but the slope crest occludes nearly all of them (a sliver at the left edge), and the browser frame of the same pads matches the PS2 composition. The connector terrain matters for collision/lighting and distant views, not for the race camera.

Not reproduced: the original streams texture chunks by race progress (`W+0x3F0`; ARA1 chunks 30-32 at
the start, 29/26/25/28/27 later, ARA1_B's 34 only near the finish) and does not draw items of
non-resident chunks, so it shows less distant geometry than the browser. The driving `mdl_*_Load` /
`mdl_*_Unload` volumes (helpers 0x200022) are known but not ported.

## Impact and contact effects (2026-09-22)

Every impact, crash, landing, rail and attack effect the rider code spawns was traced from its
gameplay event to its emitter. Sources: the rider FX container `RFX = *(rider+0x77C)`, whose
components are reset by `111890` and updated by `111948` / the rider-manager passes `128F20..1290F0`
in this order: `D30` light list, `3B0` wake, `470` board sparks, `520` board track, `610` boost,
`9C0` power-up aura, `AD0` air streamers, `AF0` rival indicator, `B00` locator beam, `B40` snow FX,
`C70` fist sparkle, `D20` cheat toggle. Only `470`, `B40` and `C70` react to contacts.

**What each impact event triggers** (all verified in the instructions):

| Event | Visual effect | Sound/HUD (not ported) |
|---|---|---|
| Crash entry `10EB30` (scenery `105D98`, landing crash `139C88`, board press `130DD0`, rider pair `107E70`) | `111AA0` -> snow impact `2E23E0(kind 0)` at the event point/normal, strength = closing speed (ported earlier); crash camera shake index 4 from `12D160` (ported earlier) | `28B180` = SSXAudioSystem singleton: `296310`, `29F660`, `2961F0` |
| Ragdoll air/slide contacts `137860` / `137D18` | `111AA0` snow impacts (ported earlier) | `12CB68`/`12CD20`/`12D4E8` sounds |
| Ordinary landing `139C88` -> `10E910` | `111AA0` with **\|rider+0x1E0\| before the response** (f22 at `139D54`), the landing probe point/normal/surface. The browser passed the relative normal speed; fixed (`browserLandingSpeed`) | `2948D0` |
| Air rail attach `106848` -> `10E910` | `111AA0` at the rail point, tangent toward v, strength = new rail speed, rail surface (ported now; on Snow Jam rails the surface's impacts are off, so it only resets the retained strength) | |
| Scenery "land on top" `1057B8` -> `10E910` | `111AA0` (not reachable: the land-on-top branch itself is still unported) | |
| Instance contact `105398` | none besides `105D98` | contact sound `296088` |
| Soft collision `108388` | none | speech `2A0E70` |
| Crashbag hit (program 49, RollerModifier) | none of its own | the `296088` contact sound |
| Hazard surface / forced reset `116120` -> control 9 `12F230` | white fade `2E47E8` (already `engine/reset_fade.hpp`) | |
| Grinding / sparking surfaces (`2DABC8`) | board sparks, glints, grind chunks (ported now) | |
| Attack charge `rider+0x350` in a punch window (`2F1150`) | fist sparkle (ported now) | |
| Large impact burst near the camera (`2E1598 -> 2F4118 -> 2F4260`) | lens snow splats (`2F3810`), only when the Weather painter value exceeds 1.0: never on Snow Jam (0..0.3), storm courses only. Not ported | |

`1242B0` (the fifth `111AA0` caller) is a playback-rider ground re-probe (`rider+0xAC4`), whose
strength is the hit's surface velocity: zero on static ground. The scripted camera shake is course
builtin 91 (`0x3050F0`, index 0, amplitude 0.4, radius 200 m); ARA1/BRA2/BHP1 never call it. The
28B180 handler family is audio only (static call closure, depth 14): no particle, overlay or shake.

### Board sparks, glints and grind chunks (RFX+0x470)

`engine/board_sparks.hpp`, `web/impact_fx_gameplay.inc`, drawn by `web/impact-fx-renderer.js`.
Update `2DABC8` each tick (before the board track, so its visual-LCG draws come first):

- gates: speed `> 277.778` cm/s; sparks when the surface record `+0x8C` is set (surfaces 7, 8, 10,
  16, 17, 18: metal), glints additionally on rails (motion 4), grind chunks on surface 9 (or rails with
  `+0x88`, none here). The surface is `rider+0x438`: the rail surface on rails (10 fence/metal, 9 wood;
  `-1` on disc, patched at load: `tools/export_rail_runtime_flags.py` now exports `runtime_surface`),
  and it is kept in the air, so the PS2 keeps spraying after a grind until the next landing;
- emit point `rider+0x460 + v*0.01`; spark kernel (`36D428` age/seed advance every tick, then
  `36D318` colours (77,77,255,153)->(255,51,51,102) (Tricky: magenta), `36D1F0` velocities, `36CEF8`
  box +-row2*10/15 +-row0*10/90 of the scaled board matrix);
- glints: 30% of rail ticks, three `sprk` quads 20..30 cm, +-4 cm;
- chunks: `GrindParticlesFX` emitter (tmb1..8, 27 fps flipbook, brown), chance min(speed/1666.67*0.1, 0.1).

Draw `2DB478`: chunks (snow program, GS `0x44`), then the kernel through renderer slot `0x290`
(`380518`, VU1 program 4 entry 0: 30 slots x up to 8 trail sprites 0.012 s apart, colour per slot,
alpha fading 1/8 per drawn sprite, half size capped at 64 px, GS `0x48`), then the glints (`377CF0`).
Random streams: visual LCG `gp+A0C` only (2 chunk draws, 1 + 12 glint draws), particle-birth `3177F0`
for active chunk births; never the gameplay generator. The sparks are small (half size 0..11 cm) and
rarely stand out in the PS2 screenshots; the purple haze near the board at a grind is not them.

### Fist sparkle (RFX+0xC70)

`engine/fist_sparkle.hpp`. While `rider+0x350 > 0` and clip 326 (channel 0) or 323 in the channel-1
hit window (class 13, marker 0, not marker 1) plays (327/324 for the other hand), four `ospk` sprites
(texture 24, GS `0x48`, 15..15+15s cm, grey 0..0.9s) are rebuilt each tick at hand + 8 cm along the
hand axis, +-3s cm; 20 visual-LCG draws after the snow pass.

### Verification

New ARMSX2 captures with `--watch` windows on the sparks component, its kernel, the LCG, the fist
sparkle and the snow FX block (`local/ps2-capture/runs/fx/fx-{rail,attack,race}`, pads of
rail-balance-lr, attack-mix and event-race, screenshots at the grind/punch/crash ticks).
`web/test-impact-fx.mjs` (in `npm test`, `FX_DUMP` in `compare-ps2-capture.mjs`):

- fx-rail: spark/chunk gates, `rider+0x438` and the emit point on every tick, every kernel word
  bit-exact on 228 ticks (673..900; 901 is the capture's known ground-attach pose difference);
- fx-attack: fist sparkle active on exactly the 80 PS2 ticks, sprites within the +-3s box of the PS2 ones;
- all three captures (3,255 ticks incl. both event-race crashes and six landings): the snow impact
  strength `FX+0xE0` bit-exact (was wrong on every landing and at rail attaches before).

Glint/fist/chunk positions depend on the visual LCG, which the PS2's computer riders also draw from,
so they are compared by gate and bounds, not bit-for-bit.

Not ported: rail chunks on a `+0x88` rail surface (environment colour record `0x4FA398`, absent on
Snow Jam; counted in `impact_fx_info()[12]`), the lens splats, the audio family, the scripted
shake builtin 91, the land-on-top path. The kernel/chunk seeds start from the glide savestate for
both free ride and the race (the countdown-anchor state has other seeds). The VU program's clip
rejection is approximated with the browser camera's clip volume.

## Rider Lighting painter by area, Metro-City (2026-09-22)

On the PS2, the rider's scene lighting in Metro-City changes by area. The browser used only the start value.
The source is the course painter's Lighting section (type 11, class table 0x484290). It is a region blend like
fog (type 5) and ScreenTint (type 7). BRA2 has five payloads that differ only in the bright bank:
`BPBRR`, `BPBRB`, `BPBRG`, `BPBRY` and `BPBR2`, each with `BPDK2`/`BPTN2`/`BOBR2`, scalars (1, 0.65) and rate -0.05. A 1013-node
point tree selects among them. ARA1 and BHP1 are uniform.

Original path:
- `2ED490` runs each environment block's nine property wrappers. Block 0 is the human rider (`0x4FA370 +
  index*0xF0`, Lighting at +0x1C -> wrapper `0x542F60`, vtable `0x483E00`, slot 10 = `2C0778`). It uses
  rider+0x460/+0x464 (the contact point) and the -99999 weight sentinel. This is the same driver as the breath
  (`engine/painter_driver.hpp`). Blocks 1/2 hold the computer riders' own coordinates.
- Blend `2BD5A8` (`engine/lighting_painter.hpp`, 20,000-case oracle) copies the four 8-byte bank references
  and blends the two scalars (+0x48 gain, +0x50 rim). Compare `2BDE30` checks all four references
  (`2BB100`) and both scalars. Reset `2BE1F8` stores the empty reference `gp+0x22E0` in all four slots and
  sets the scalars to (1, 0.75).
- `2EDA4C` builds the rider bank from references 0/1/2 (`2EE318`/`2EE340`/`2EE368` -> `2EE010`) and the gain
  (`2EEFF0`). `2EE010` resolves an empty reference to the IRR index at `gp+0x12D4`, a per-course runtime value:
  BPBR1 (3) in the Metro-City and The Junction savestates, APBR1 (12) in Snow Jam's.

Browser: `web/environment_bridge.cpp` runs the Lighting wrapper when the package's painter is
`spatially_varying`. `browser_lighting_painter_step` is called from `update_trail` with the trail contact point,
the same source +0x460/+0x464 that the breath uses. It is called before `browser_environment_colour` builds
the bank, so bright/dark/alternate and the gain follow the selected payload. `web/prepare-environment.py` adds
`painter.default_reference` (read from the glide savestate's `gp+0x12D4`) and that bank.
`environment_lighting_info()` reports the state. `web/rider-material.js` takes the live rim scalar.
Uniform courses keep the packaged start payload, so ARA1's `environment.json` is byte-identical.

Ground truth: `metro-event-race-light` is the metro-event-race script re-captured with watches on the human's
Lighting property object (`0x58BF00`, 0x60 bytes) and its wrapper (`0x542F60`). `compare-ps2-capture.mjs
--lighting` loads the environment package and compares each tick. The selected bank reference matches the PS2
on all 2300 ticks: BPBRR 977, BPBRB 1113 and BPBRY 210 ticks, switching where the tree says. The physics is
exact end to end. The wrapper's accumulated distance matches. The gain and rim scalars match through tick 1550.
At 1551 the PS2 re-seeds the wrapper: distance 0, rim exactly 0.65 (a blend(-1), which needs the -99999
sentinel). The trigger is not found. After that the browser's rim is 2.3e-5 lower. The gate is in
`test-ps2-captures.mjs`. The savestates agree: the countdown anchor holds BPBRR, the entry the tree selects at the grid, and
glide-620 holds BPBRY for the human and BPBRB/BPBRY for riders 1/2, all as the tree selects.

Remaining: the 1551 re-seed trigger; opponents' cores run their own wrapper but shade with the packaged rim;
`gp+0xA74` (the override that returns `gp+0x24C0` from `2EF0E8`) is 0 in every inspected state and is not modeled.

## Rider shadows (2026-09-23)

The PS2 shadow is a projected silhouette of every visible rider (human and computer riders), not the `shad` texture
(FX 50, never loaded). Per rider and sim tick (`1225F0` -> `122898`, gated by the visibility flag rider+0xB18):

- **Fit `374D00`**: light L = (0, 0, -1) always; N = the rider's up column rider+0x1C0; C = the feet midpoint (bones
  18/21); A = normalize(N x L), B = normalize(L x A); the six bones head, hands, feet, hips (5, 10, 15, 18, 21, 0)
  in light space, +/-70 cm margins. UV u = (A.p - A.C - minx)/w, v = (maxy - (B.p - B.C))/h; culling slab from the
  feet down 30 m.
- **Silhouette**: a 128x128 PSMCT16 texture per rider, the LOD-3 meshes in flat 38 (PSMCT16 -> reads 32), painter's
  order by part slot, no depth test; the board and bindings paint 0 while grounded (motion 0), 38 otherwise.
- **Receivers `38CA08` / `38D448`**: near terrain patches only (tier-2 and stitched lists), redrawn with the light
  layer's draw and blend 0x81 with As 0x80: **Cd = max(Cd - T, 0)** per channel, before the fog composite and
  ScreenTint (so the shadow is fogged and tinted: PS2 frame deltas (34, 33, 28.5) near, (20, 18, 23) fogged).

Browser: `web/rider-shadow.js` (fit ported with fround, one 768x128 atlas of six tiles rendered once per frame from
the riders' own source skins, `web/rider-skinning.js` `worldNode`; rank per vertex from the original part order) and
`riderShadowReceiver` in the terrain branch of `web/world-material.js` (uniform loop over the active slots, explicit
level samples, one pipeline). Riders tag their scene group `userData.shadowRider` (main.js asset(), opponent-riders.js).
`?riderShadows=0` disables it. Check: `web/test-rider-shadow.mjs` (bounds within 0.016 cm and UV rows within 1.3e-4 of
8 PS2 shadow objects at setpieces/full 418/969/1621/2019); screenshots `after/shadow/` in the world report.
Differences: the full-detail meshes stand in for LOD 3; the receiver set is all terrain within the slab instead of the
near patch lists (the patch LOD rule `38B178/38B190` is not recovered); rider visibility is a frustum sphere test
(the occluder part of `122278` is not recovered); PSMCT16 dithering unconfirmed (T = 32 used).
