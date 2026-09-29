Terminology correction: the first fog payload value after blend_rate is **density**,
not the fog-equation selector. Older exported JSON retains the key `mode` for
compatibility. See the depth-palette recovery below. No shader should branch on
that float as if it were an enum.

# Fog painter recovery

Browser fog currently uses ARA1's captured starting payload for the entire run.
The sky clear color used to follow that fog color; the PS2 clears to black (docs/presentation.md section 7), which the
browser now does. Region selection, smoothing,
mode interpretation and original pixel output remain incomplete.

`engine/fog_painter.hpp` ports Fog vtable slot0x210, original2BCF38..2BD064.
The class vtable is484FE0 (constructor2BC7C8). Its six value pairs occupy+8..+34:
mode, near cm, far cm, red, green, blue. Each pair stores a blended value followed
by the most recently supplied sample. The separate float at+0 is unchanged by
this method, and payload+0 is not consumed here.

The source squares the supplied weight without clamping. For each component it
computes `weight² * sample + (1 - weight²) * prior`, with EE arithmetic ordering,
then stores the sample in the second slot. The class invokes slot218 before and
after the operation; for Fog this resolves to the empty2BDA48 method. The helper
is specific to this class and must not erase nonempty hooks of other painter types.

`python3 tools/test_fog_painter_native.py` executes the original routine with its
actual control flow, stubbing only that empty class hook.20,000 cases verify all
12 float bit patterns, the unchanged rate, both hooks and the return boundary.
Inputs include zero/unit, negative and greater-than-one weights; this test does
not assert that gameplay's spatial sampler emits that full range.

Next recover the section tree's spatial selection and the parent scheduler's
smoothing/order. Slot200 (2BF580) and208 (2BE518) use stream interfaces, so do not
mistake them for the frame update. Slot228 (2BE108) resets several fields from
globals; its scheduling remains to be traced. No browser fog animation or new
visual heuristic was connected in this recovery pass.

## Point-tree query recovered

`engine/painter_tree.hpp` ports original2C1CD8, reached by2BAF90. It subtracts
origin X/Y, multiplies by the stored scale, converts using the game's truncating
EE mode, and accepts integer coordinates0..32767. Small negative fractions can
truncate to zero: replacing conversion with floor or clamping would differ.
Out-of-range coordinates return the explicit outside leaf at tree+18.

Nodes are8 bytes. A low bit in the first halfword marks an internal node; its
four halfwords encode child indices shifted left one bit. Successive high bits
of the doubled15-bit X/Y coordinates choose quadrants. The header's root index
is at+14 and relocated node pointer at+20; serialized nodes begin at+28.
The host validates indices/cycles instead of following malformed memory.

`tools/export_fog_tree.py` reconstructs the ARA1 section directly from BAM.SSB,
records source hashes and exports raw section bytes plus parsed node metadata.
The real section has333 nodes and9 fog payloads. Leaf payload references and
weighted region sampling still need interpretation; a point lookup alone is not
sufficient to drive the previous weighted blend correctly.

`tools/test_painter_tree_native.py` now regenerates that fixture, then compares
30,000 queries with the original routine:9,863 outside results and197 distinct
leaves, all matching. This exercises the actual tree, arithmetic and leaf-return
addresses; it is not a visual fog comparison. No gameplay fog change yet.

## Payload mapping and transition-driver entry

`originalPainterPayload` now includes2BAF90 and2C0A10 semantics: read the selected
leaf's second32-bit word, treatFFFFFFFF as no painted payload, otherwise select
that index in the section's8-byte `(type,payload-pointer)` table. The exporter
validates all leaf indices against the9 ARA1 fog payloads. The30,000-query oracle
now also executes both original wrapper functions and compares their returned
payload-table address, without mocking the point lookup.

The parent driver is2C0778 in sub_002C0408. It stores the last sampled X/Y at+8/+C
and accumulates planar travel distance in the painter object's+0. The initialization
sentinel at49EE6C and the automatic-weight sentinel at49EE70 are both-99999.0.
It resolves the current section through level manager/current region, uses2C0A10,
invokes Fog slot220 (2BDB38 comparison), and calls slot210 with a selected weight.
With automatic weight, negative payload rates produce their negated value as the
blend weight; nonnegative values additionally participate in a travel-distance
threshold. Initial application uses-1, which squares to1 in Fog's blend method.
Missing region/payload goes through slot228 (2BE108 defaults). Full branch order,
positive-rate behavior and region fallback still need a source-tested driver port.
No runtime fog change in this pass.

## Transition driver and Fog methods verified

`engine/painter_driver.hpp` ports2C0778 after the host resolves the current region
and section. It preserves the planar-distance accumulation, last-coordinate writes,
initialization sentinel, type rejection, explicit-weight path, automatic rate path,
and reset/compare/blend callback ordering. Initial missing-section dispatch resets
twice; missing region and missing payload have distinct paths. A nonnegative rate
below the traveled-distance threshold uses weight1 and clears distance; otherwise
automatic weighting uses the negated rate (including a positive rate whose threshold
has not yet been reached). Do not clamp or replace this with a time-based lerp.

`tools/test_painter_driver_native.py` executes original2C0778 with a controlled
region fixture and controlled sample/virtual callbacks.20,000 cases match distance
and coordinates bitwise, blend weights, lookup coordinates and callback order.
The blend stub deliberately mutates distance, verifying whether the caller clears
it before or after dispatch. Region discovery itself remains a host boundary.

The Fog comparison2BDB38 and defaults2BE108 are now ported and tested against the
original in20,000 cases each. Equality compares all six current values, not the
sample slots. Defaults replace near/far/RGB from globals, clear distance AND mode,
and retain the six sample slots. The mode clear occurs at2BE138 in the return
delay slot and was caught by the full-field comparison. Existing20,000 weighted
blend cases continue to pass.

Remaining integration work: determine which original caller coordinates and
schedule drive the Fog getter, connect region selection, seed the state, and
apply output in the browser renderer. These tested helpers are not yet wired to
live fog; the demo still uses the captured starting fog throughout its run.

## Live ARA1 camera sampling

Camera caller15EBBC..15EBD0 invokes2ED490 with final outer-camera+20/+24 X/Y,
view index+6 and automatic weight-99999. Camera collision15EE00 and shake15E460
occur earlier at15E928/15E930.2ED490's opening loop forwards these coordinates and
weight into each painter driver's virtual10 method (2C0778). Rider1218D0 uses a
separate set of painter instances; the browser's new fog state belongs to the
camera path and must not be shared with rider environment queries.

`web/environment_bridge.cpp` now loads the exported ARA1 tree/payloads, retains
Fog state, and runs the recovered point lookup, comparison, defaults, weighted
blend and transition driver. `camera_for_head` invokes it once after final camera
collision/shake. A fresh run resets Fog; ordinary camera/rider repositioning retains
its transition state. Invalid tree indices/payloads fail explicitly. The host uses
ARA1's one loaded painter section; it does not yet stream other peak regions.

The browser applies current near/far/RGB to its fog (the sky clear is black, docs/presentation.md). It still
uses the existing Three fog equation and color pipeline; source mode is exposed
but its shader behavior, original fog far-cap coupling, and exact GS output remain
unported. This is spatial fog/transition integration, not full rendering parity.

`check-camera.mjs` verifies one fog tick per camera tick, final source eye sampling,
finite values, valid distances and initial authored payload during actual charged
jump and held-ledge fixtures. Those short routes remain in region0. Separate camera
position fixtures use the largest leaf per payload and cover all9 authored payloads,
checking that later region changes blend. These are positioning tests, not a full
physical traversal of the course. The exporter records representative positions.

Validation: full npm suite and production build pass; live menu/countdown/gameplay
startup checked. The native/browser trace includes fog and matches9,630 frames
across23 scenarios and645 fields. This is native/WASM implementation agreement,
not a continuous original-console rendering comparison.

## Original depth palette recovered

2F00A0 retrieves density through2EE9C0, near/far through2EEA08/2EEA50 and RGB
through2EEA98/2EEAE0/2EEB28. It writes density to gp+1248, near/far to+123C/+1240,
and separately sets the integer equation at+1244 to1.36ABA0 copies these into
per-view render context+6C64: near/far/equation/density followed by alpha/RGB.

36A428 transforms near/far into projected depth bins, caches render settings, and
fills a256-entry CLUT. `engine/fog_depth_table.hpp` ports the fill36A6EC..36AA20
with projected bins supplied by the caller. Equation0 is integer linear;1 uses
a seventh-order exponential polynomial;2 squares the density term first. The
polynomial coefficients and0.1 density scale come from the executable. Results
clamp to0..1, scale by128, truncate and pack with RGB. CLUT indices swap bits3/4.
Unknown equation values preserve earlier entries below the near bin; later entries
still fill with alpha128. Reversed/equal bin ranges use a minimum span of1.

`tools/test_fog_depth_table_native.py` executes the original fill from36A6EC to
a boundary stop at36AA24 (before upload).5,000 cases match all256 packed words,
including unsupported-equation preservation and negative/large density inputs.
Projection, cache/allocation/upload and final compositing are explicitly outside
this test. Do not assume these alpha values are direct browser fog opacity; the
subsequent depth-texture pass must establish their interpretation.

The current live browser still uses generic fog. This new CLUT routine is not
connected until projection-bin calculation and compositing have been recovered.
Also,2EE3B8's camera far-cap query reads a different painter slot from Fog, so
fog far distance must not simply replace camera far clipping.

## Projected depth bins and packed color verified

`originalFogDepthParameters` now ports36A478..36A618. It transforms
`(0,0,distance,1)` through four projection columns using VU arithmetic order,
divides by W using EE scalar division, converts projected Z with truncation,
shifts right8 and clamps to0..255. RGB multiplies by255 and truncates before
packing; no extra gamma transform or component clamp is introduced. Nonfinite or
out-of-int32 conversion inputs are explicitly outside the host helper's support.

`tools/test_fog_projection_native.py` compares20,000 executions of the original
prefix, stopping before cache lookup at36A61C. It covers4 render-view slots,
varied matrices, equal/reversed distance ranges and color values. The first case
uses the actual Snow Jam countdown render context: pointer at(gp-854)=61BA60,
projection at context+5930, Fog near/far/RGB at context+6C64. Captured bytes and
snapshot SHA256 are saved in local/event-activation/fog-projection-captured.*.
All near/far-bin integers and packed RGB match. The test boundary insertion is
validated explicitly; absent labels cannot silently run into later callbacks.

The original palette fill is separately verified. Full cache/allocation behavior,
final screen-space compositing and browser integration of this depth palette are
still incomplete. Gameplay shader remains unchanged in this recovery pass.

## Composite packet and alpha interpretation verified

36AC00 builds the CLUT, runs36AA60/36AE20 preparation, then36B158 draws the final
textured sprite pass.36B300..36B35C emits GS ALPHA_1(register42)=1 and PABE(3F)=0.
36B374..36B3E4 emits TEX0_1(register06) with PSM27 (T8H), TCC1 (RGBA), TFX1
(DECAL), PRIM=156 (textured, alpha-blended, fixed-UV sprite) and unity RGBAQ
3F80000080808080. The buffer/CLUT addresses are variable; the listed mode fields
are invariant in this path.

ALPHA selectors for value1 are A=destination, B=source, C=source alpha,
D=source. Thus the palette alpha is scene transmittance: each encoded RGB byte
is `fog + (((scene - fog) * paletteAlpha) >> 7)`, clamped to0..255.128 retains
the scene and0 gives fog. `engine/fog_composite.hpp` implements this byte operation;
it must not receive linear-light colors. This interpretation is consistent with
the GS backend's selector/shift logic in the local PS2Recomp gs_cpu_backend.cpp.

`tools/test_fog_composite_packet_native.py` executes the original two packet setup
blocks with validated boundary stops for1,000 cases. It verifies emitted register
IDs/values, texture mode, primitive and RGBAQ. Separate endpoint tests cover every
scene/fog byte pair. This is not a rasterizer or full screen-image comparison.

The browser's generic fog shader remains unchanged. Still required:36AE20's
intermediate depth-image preparation and sampling coordinates, live original
projection construction, encoded-space postprocessing and final output validation.
Do not apply the CLUT alpha directly as browser fog opacity.

## Intermediate depth-pass state verified

36AE20 packet blocks36AF10..36AFD8 and36AFF0..36B048 use FRAME_1 PSMCT16 with
FBMSK=00003FFF, TEX0_1 PSMZ16 with DECAL/RGBA, TEXA=0000008000000000
(TA0=0, TA1=128), PABE=0, PRIM=116 (textured fixed-UV sprite, no alpha blend),
and unity RGBAQ. `OriginalFogDepthPass` records those invariant fields.

`tools/test_fog_depth_packet_native.py` executes both original packet blocks with
validated stops before virtual calls/sprite iteration:1,000 cases pass. Address,
width and format-independent inputs vary. This verifies modes and masks, not the
final pixel mapping. Source emits two-step tiled sprite coordinates in36B054..
36B0E4; the PSMZ16/PSMCT16 memory reinterpretation and preserved framebuffer bits
must be accounted for before replacing the pass with a shader depth-byte lookup.
No claim of a simple copy or direct `depth >> 8` mapping is established by this test.

36AA60 is the palette upload/setup preceding this pass, not an ordinary scene
color/depth copy. Avoid inferring framebuffer roles from context fields5A80/5A84
without tracing their setup. In the countdown those fields hold E0 and0.
The generic browser fog shader remains live while this recovery continues.

## Depth sprite coordinates verified

`engine/fog_depth_sprites.hpp` ports36B04C..36B0E4, including the empty-column
branch and packet-pointer advance. The caller computes columns8 from the source
width/8. The loop advances its column counter by2 and emits one8-pixel-wide sprite
per iteration. UV starts at(0.5,0.5), ends at(8.5,height+0.5), and shifts16 pixels
horizontally each iteration. Destination X starts8 pixels beyond the supplied
origin and spans8 pixels; destination Y spans the supplied height. GS coordinates
are12.4 fixed point and UV/XYZ2 register tags are retained in each packet record.

`tools/test_fog_depth_sprites_native.py` executes the original strip-generation
block with a validated stop before packet finalization.10,000 inputs match317,349
emitted sprites byte-for-byte and the final packet pointer. Coverage includes
empty/negative/odd column counts and varied heights/origins. The normal layout
therefore cannot be replaced by a naive one-to-one full-screen copy.

Next resolve the PSMZ16 source addressing, PSMCT16 masked destination writes and
subsequent T8H addressing together. The local GS memory implementation has separate
C16/Z16 block tables in ps2_gs_memory.cpp, useful for cross-checking that mapping;
its correctness must not be assumed to establish original rendered-image parity.
The browser shader remains unchanged in this pass.

## Depth-byte address mapping cross-check

`tools/test_fog_depth_address.py` applies the source-verified strip layout through
the local GS memory library's distinct Z32/Z16/C16/C32/T8H address tables. It
initializes independent random depth and color values, samples nearest texels at
the recovered half-texel UV origin, and applies the masked16-bit writes. The
RGBA-layout framebuffer mask00003FFF corresponds to packed5551 mask00FF, not
3FFF: it preserves the low byte of each selected16-bit destination pixel.

Across851,968 pixels and5 buffer sizes, the resulting scene alpha byte equals
`(rawZ >> 8) & 255`, and every RGB byte remains unchanged. T8H reads that same
byte. `originalFogPaletteIndex` records this mapping. Pixel index extraction wraps;
near/far projection-bin clamping must not be applied to every pixel instead.

This is a cross-check against the local GS memory model, using the recovered
packets/layout; it does not emulate GS rasterization, prove texel-center behavior
against hardware, or compare an original framebuffer. Those limits remain explicit.
The original-framebuffer check and live projection construction are still needed
before connecting the full browser fog compositor. No gameplay shader change here.

## Live reverse-depth coefficient construction recovered

386688 copies active matrix context+6AF0 into per-view+5930 before subsequent
rendering can replace the active projection.376C58 constructs the perspective
matrix: depth format at context+5A40 equal31 (PSMZ24) selects16777215; other values
select65535. The constants are at gp-27C0/-27B4. With near/far and the viewport
depth offset, the Z coefficients are evaluated in this order:

- delta = far - near (EE scalar subtraction)
- slope = (-range * near) / delta (multiply then EE nearest division)
- offset = (-far * slope) + viewportDepthOffset

`originalFogDepthProjection` preserves that order. Projected W is view-space Z;
the renderer still must preserve the multiply/add/divide order when evaluating a
pixel, rather than algebraically rearrange it and assume bit equality.

`tools/test_fog_depth_projection_native.py` compares20,000 executions of the
original coefficient-construction segment, stopping after its Z/W stores. Both
range inputs, reversed planes and viewport offsets match slope/offset bits. Range
selection is source-audited separately; viewport/FOV computation is outside this
segment's test. The saved countdown context uses format31. Its active6D20 projection
parameters at snapshot end have near0/far1 (later rendering), so those values must
not be mistaken for the gameplay projection copied to5930 earlier.

Full browser fog compositor integration and an original framebuffer comparison
remain unfinished. Gameplay rendering is unchanged in this recovery pass.

## Render-ready live palette exports

The browser core now prepares the source-derived palette after each camera/fog
step, using current compositor near/far, the recovered24-bit reverse-depth
coefficients (default viewport depth offset0), and live Fog values. Equation1 and
PSMZ24 follow the recovered normal render setup. `_fog_palette_rgba()` exposes
1,024 RGBA bytes in linear index order (the GS CLUT bit3/4 swizzle is undone for a
normal GPU texture). `_fog_palette_info()` exposes validity, monotonic revision,
near/far bins, slope/offset, density, equation and depth format.

Palette rebuilding is cached by the source-relevant bins, RGB and density.
Projection coefficients still update when the camera clipping changes, even when
no palette upload is needed. Reset invalidates the palette and advances revision;
it does not reuse a prior revision1 and risk leaving an old GPU texture bound.
This prepares the data interface; the current renderer still uses generic fog.

Camera tests check palette alpha range, constant RGB across entries, alpha128
above the near bin, finite projection values and unchanged bytes while revision
is unchanged. Existing all9-region positioning tests continue to pass. The
native/browser trace now includes palette metadata and a hash of all1,024 bytes.
GPU postprocessing APIs are available in the installed Three version (RenderPipeline
and PassNode with scene depth); shader wiring and output comparison remain next.

Validation: full npm suite and production build pass; native/browser palette
metadata and bytes match across9630 frames/656 fields, with0 mismatched frames.
This still does not validate a GPU fog compositor, which is not yet connected.

## GPU fog compositor is live by default

`web/fog-renderer.js` now consumes scene depth, the live reverse-depth coefficients
and cached256-entry RGBA palette through Three RenderPipeline/TSL. It extracts the
wrapped depth index, samples with nearest filtering, and applies the recovered
encoded-byte blend. Scene color first quantizes like an8-bit framebuffer; blend
uses floor for the signed GS shift. The final pass outputs encoded RGB directly,
with automatic output color transform disabled to avoid a second conversion.
Material fog is disabled in this path, so the old generic fog is not applied twice.

The sky pass is sampled as the world pass's background using explicit screenUV.
Leaving that UV implicit incorrectly used the background-direction context. A
second early bug used TSL's chained mix(receiver-as-weight) instead of the standard
mix(a,b,t) function; the live dark-output failure was fixed before default rollout.
The sky background node is created once and reused, not allocated each frame.

Normal runs now use this path. `originalFog=0` retains generic fog for comparison;
`fogStage=color`/`palette` isolate diagnostic stages. These are developer query
parameters, not new game UI. Runtime uses offscreen GPU scene/sky passes and a
fullscreen GPU composite; there is no frame readback. Palette upload is1KB per
changed revision. Performance/power improvement has not been benchmarked.

`fog-gpu-test.html` is a separate verification entry built by Vite. It reads a
single pixel only in the test harness and checks seven alpha values against the
byte blend, exact red preservation, nine depth-index cases including wrapping,
and no repeat upload for an unchanged revision. Both WebGPU and forced WebGL2
(`?backend=webgl`) pass. Live default menu/countdown/race and sky rendering were
checked. Full npm suite and production build pass.

Still unproven: original-console framebuffer equality, GS rasterizer depth/texel
rounding at primitive boundaries, exact placement of every effect relative to the
original screen-space pass, and the original game's full lighting pipeline. The
GPU implementation follows the recovered math; it is not a GS emulator and does
not establish perfect visual parity. Other goals remain active.

## Foreground fog depth-test omission fixed

The earlier live shader reproduced the wrapped CLUT lookup but omitted the final
GS sprite depth test.36B330/36B334 emit TEST_1=0x70000: ZTE enabled, ZTST strict
GREATER.36B440..36B464 sets both sprite vertices' Z to0xFFFF. Consequently the
fog overlay affects only scene reverse-depth values LESS THAN65535. Nearby
24-bit depth values at or above that threshold fail the overlay test and retain
the scene RGB. Feeding those rejected pixels through the wrapped8-bit CLUT
produced repeated foreground fog bands and washed-out riders.

`originalFogDepthPasses` records the gate. The original packet oracle now verifies
both TEST fields and emitted sprite Z across1,000 setup/sprite cases; explicit
boundary checks cover65534/65535/65536 and24-bit extremes. This supplements, rather
than replaces, the earlier valid byte-address mapping: the extraction still wraps,
but it is not applied to pixels rejected by the depth test.

`web/fog-renderer.js` now bypasses blending for depth>=65535. The scene-color
texture uses explicit screenUV and is materialized before the conditional. The
new pass-through test initially returned black with implicit sampling; the explicit
sampling/materialization fixes that integration issue. No fog colors, distances
or curves were retuned. GPU tests now check both sides of the strict threshold
and large depths that would otherwise wrap into fogged entries. WebGPU/WebGL2
pass all blend, lookup and rejection cases. Reports:local/fog-depth-gate-*.json.
Full npm/build pass; live foreground/rider inspection shows the bands/washout
removed. Terrain/rider clipping remains visible and is a separate open issue;
this does not establish full original framebuffer/rasterization parity.
