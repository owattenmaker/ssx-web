# Original snow effects

The product implementation is ordinary native C++ (`snow_emission` and
`snow_particles`). It contains no guest CPU, microcode interpreter or graphics
translation layer. Original instructions run only in development tests.

## Renderer contract

Keep one `OriginalSnowParticles` per rider and authored emitter profile. Call
`emit(request, visualRandom)` once per original logic tick, including inactive
requests, then read `particles()` for rendering without advancing clocks or RNG.
The defaults construct the original SnowTrail profile. Other profiles use their
authored values in `local/assets/native/SNOW_FX/snow-fx.json`.

- `positionCm` is source Z-up centimeters: render `(x,z,-y)*.01`.
- `halfExtentCm` is the source billboard half-size before view projection.
  Render the camera-facing quad with that extent, not twice that extent.
- `colourGs` is the original integer GS modulation/alpha scale128. Divide each
  component by128 in the native fragment shader and modulate the original texture.
- The PS2 `*.gs.rgba` files retain raw alpha0..128. Their manifest
  `gs_alpha_scale=255/128` converts a normalized texture sample exactly. Normalized
  `.rgba` and PNG files are viewing conveniences with double/saturate alpha.
  The GameCube `tmb1.rgba` uses ordinary normalized RGB5A3 alpha.
- Original projected half-size is clamped to128pixels per axis by the later VU
  view stage. Camera projection, clipping, depth policy and fog are renderer work;
  the current helper returns view-independent particle state.

`OriginalSnowRiderCache` reproduces2DF960..2DFB5C from real rider state. Its
edgeBias12C is usually0 and is selected from manualState330/animation/stance; it
is not the input turn axis. Speed comes from the source velocity vector. The
board frame is geometry+30's world matrix for rider+8A4 (bone23 in these riders):
unit axes plus source-centimeter origin. Do not multiply its axes by rider scale.
The separate board-track code uses geometry+34's scaled matrix.

The `snow_context` module owned by the terrain/contact task supplies the remaining
surface, environment-colour and impact-trigger inputs. Original emitter colour is
RGBA `(2*environmentR,2*environmentG,2*environmentB,1)`.

## Authored assets

`tools/export_snow_assets.py` publishes five original textures and ten complete
profiles. All580 parameter words match the original PS2 constructor2DE4A8..2DF178
and the owned GameCube `cmrender.h`, bit for bit. Shader texture IDs come from the
PS2 table4891B0, stride12:5=`spry`,6=`impt`,14=`tmb1`,55=`btrl`,56=`wake`.
The PS2 full-colour records have16-byte headers; the world-terrain texture loader's
128-byte header assumption must not be applied to these SSH records.

Profiles by emitter index are SnowTrail0, LargeChunky1, SmallChunky2, Rock3,
RiderBreath4, LargeImpact5, SmallImpact6, Cloudy7, Kicker8 and BodySnow9.
PS2 supplies `spry`, `impt`, `btrl`, `wake`; the owned GameCube supplies `tmb1`.
The manifest retains source hashes, offsets, encodings and conversion provenance.
Publication is atomic and preserves any prior package directory.

## Scheduling and random streams

The game's two effects random streams are independent of gameplay RNG:

- `OriginalSnowRandom.word` is gp+A0C (`4A3AFC`), a mantissa LCG using
  multiplier18FCD, incrementE9507C and23-bit state. Board tracks and snow share it.
- Particle birth seeds use3177F0's six words at4FF018. They use the same proven
  generator algorithm as gameplay, but are a separate `OriginalRandomState`.
- Each retained birth seeds the particle program's own LFSR. It makes nine draws
  per child for position, velocity, colour, size and lifetime. These draws do not
  advance either CPU random stream.

All rider board-track updates (128F80) precede all rider snow updates (1290A0).
Within each rider's2DF920 update, calls are Chunky[2,1], Rock3, SnowTrail0,
RiderBreath4, Impact[selected5/6, otherinactive], Cloudy7, BodySnow9, Kicker8.
When both impacts are inactive their call order is[6,5]. Cloudy consumes its first
LCG draw even when airborne or otherwise unable to emit. Omitting another emitter
can change the shared LCG sequence; isolated helper conformance is not a claim
that all ten effect categories already match a full frame.

## Recovered native paths

`originalSnowTrailEmission` covers2E0EE8 and board jitter2DE398. It gates on
ground emission, speed>222.222229cm/s, surface+58 and the source suppression flag.
Its point is posed board origin+15cm along board Y, followed by original board-X
and board-Z jitter. Profile velocity, normal and speed-normal coefficients are
applied in the original VU operation order.

`originalSnowChunkEmission` covers the whole2E02B8 routine: ordinary carve and
brake, optional wake-velocity input, landing burst, inactive slots, exact chance
tests and random draw order. It returns Small2 then Large1. Surface70/74 are their
respective chance scales. Wake78/7C/80 and the actual native wake cache remain
caller inputs; no captured future wake geometry is injected.

`originalSnowImpactEmission` covers2E1598. It mutates the retained strengthE0,
buildup4 and alpha120, emits the original ordered pair of births, and exposes the
secondary2F4118 impact request rather than silently pretending that separate
effect has run. `snow_context` owns initialization through2E23E0.

`originalSnowCloudEmission` covers2E1A80: slip/turn/speed chance, board-normal
height scatter, backward position offset, carve-side velocity and all enable
gates. It uses surface50/54 and profile7's original velocity coefficients.

## Particle evaluation and evidence

The original particle program is the fifth `.vutext` DMA/MPG stream, loaded from
439A40. A00 is selected when emitter+200 is0; this is confirmed for all ten human
emitters at glide, brake30, jump30, jump90 and glide120. F10 is a distinct alternate
colour mode and is not silently substituted.

Native preparation reproduces36CBF8/36CE00/36CE28/370058 coefficients. Source
age is scaled by damping; force is divided by damping twice and random velocity
coefficients once. Position uses the source polynomial
`-.7300000190734863*t + .11299999803304672*t*t`, with `t=min(scaledAge,2.7)`.
This is not an invented gravity/drag integrator. Birth positions, velocities and
colours interpolate between adjacent retained records within each birth group.
SnowTrail has36 retained births and two particles per birth.

Development checks:

- `test_snow_emission_native.py`:20,000 whole-original cases each for rider cache,
  SnowTrail+jitter, Chunky, Cloudy and Impact, including output values and RNG.
- `test_snow_particles_native.py`: all native SnowTrail preparation coefficients
  match the live original emitter;20,000 original3717C0/3710D0 updates match every
  ring record, cursor and visual RNG word;2,000 original VU cases match size,
  lifetime, model-space position and packed GS colour.
- The VU oracle uses a test-local strict-FP upper interpreter object to prevent
  host FMA contraction. Vendor/runtime sources are not edited. Whole-history
  traversal also matches all23 visible particles in a retained36-birth test case.
  The original traversal produced59 total quads versus58 native particles: one
  additional oldest quad has zero alpha. Zero-alpha quads are explicitly excluded
  from that visible-state comparison. Larger-profile multi-batch traversal remains
  separate from this established SnowTrail proof.

`extract_fx_microcode.py` and its recovered binaries are development-only. The
native app receives authored parameters and textures, never executable microcode.

## Browser projected-size limit connected

The browser now applies program4's original projected half-size cap: D98 loads
I=128 and DA0 performs MINIi.xy after projected-size ABS. export_snow_view.py
checks those exact original words and verifies the512x448 source viewport in
three baseline states, then publishes view constraints to the snow package.
The setup command preserves this metadata through ordinary asset packaging.

snow-billboard.js caps X/Y independently in source framebuffer pixels, expressed
back in world-space billboard scale. CSS size and device pixel ratio do not define
the cap. Behind-camera centers produce degenerate geometry; standard camera
clipping still owns the visible projection. This is not a full VU clipping or
pixel-position reproduction. The renderer's cache now includes camera world and
projection matrices, because translation/FOV changes can affect the cap even when
particle state and camera rotation are unchanged.

180 projection cases check near/far distances, three FOVs/aspects, capped axes and
unchanged sub-limit particles. Full npm/build and live startup/render inspection
pass. No particle births, RNG, colors or physics changed. Depth/blend/fog ordering,
exact source projection rounding and broader snow visual fidelity remain open.

## Active-prefix instance uploads

Snow renderer updates now mark only count*16 matrix floats and, when particle
state changes, count*4 color floats. Empty emitters do not request fresh uploads.
Camera-only updates retain pending color data without rewriting it. Each active
prefix is fully rewritten, so shrinking can discard an older pending tail; those
instances are not drawn, and later growth rewrites the newly active prefix.
Initial GPU allocation can still upload full capacity. This is not a measured
frame-time/power improvement.

`test-snow-renderer.mjs` exercises the actual renderer construction with owned
texture/profile inputs and controlled particle buffers. It checks unchanged,
camera-only, shrink-to-one, empty and regrow cases, active ranges and restored
colors. Full npm/build pass. Birth/RNG/color/size/physics behavior is unchanged.

## Missing tumble texture frames recovered

The renderer/package previously contained only tmb1, although profiles1/2/3
specify NumFlipTextures=8. The pinned executable's4891B0 table maps IDs14..21
exactly to tmb1..tmb8. `export_snow_flipbooks.py` decodes all eight distinct owned
GameCube particle.gsh frames (matching the pre-existing tmb1 byte hash) and
publishes them to native/browser snow packages. It preserves existing metadata,
including view limits. The manifest now records each emitter's texture sequence
and authored rate: LargeChunky30, SmallChunky45, Rock45; static emitters retain
single-frame sequences. Export runs during setup.

`test-snow-flipbooks.mjs` checks every emitter binding, frame extent and image hash.
Renderer construction, full npm/build pass with the expanded package. This does
NOT animate the chunks yet: original frame/phase selection and renderer switching
remain unimplemented. Do not guess that frame=floor(age*rate), because the source
may include scaled age, random phase or emitter-wide timing. The current browser
continues sampling the first frame until that owner is recovered. Final GS blend/
depth/fog ordering also remains open.

## Original shared-emitter flipbook phase connected

Recovered the correct owner:3705E0 stores NumFlipTextures at emitter+180 and
FlipTextureRate at+188;370018 initializes phase+184 to zero.370788 advances
phase by rate*passedElapsed when the emitter's initial lifetime is nonzero.
370838..370860 truncates the candidate for the frame-count comparison and resets
phase to ZERO on overflow; it does not preserve a modulo remainder. The draw path
370A3C..370A60 selects base texture ID plus integer phase once for the emitter.
This is shared emitter animation, not per-particle age/random texture selection.
The separate370DC8 single-particle constructor has random phase and is not this
retained snow-emitter path.

snow_flipbook.hpp implements the phase step.20,000 original370788 cases match
exactly for dead/infinite/expiring inputs, rates, overshoots and frame counts;
particle-kernel callbacks are controlled test boundaries. Current10 authored snow
profiles all have infinite duration, and the browser advances their phase once per
visual simulation tick, separately from birth submissions. Finite-duration host
lifecycle remains explicitly unsupported. Reset clears phase. snow_flipbook_info
exports10 selected texture IDs plus10 fractional phases.

The snow renderer switches the existing TextureNode binding for each emitter,
retaining instance batching and checking the frame belongs to its authored sequence.
All sequence frames must share alpha scaling. Core phase tests reach all8 frames
and verify reset/read-only behavior. Actual-renderer GPU QA produces8 distinct
images on both WebGPU/WebGL2. Reports:snow-flipbook-webgpu/webgl.json. Native/browser
trace now includes the20 phase/ID fields:Sam6443 and Zoe3785 fields,9630frames each,
zero mismatches. Full npm/build pass. This supersedes the earlier first-frame-only
limitation. Original full-scene callback phase alignment, blend/depth/fog ordering
and framebuffer fidelity remain unproven; no overall goal completion claim.

### Clamp snow RGB before blending

The browser snow material now clamps textureRGB * colourGsRGB/128 to[0,1]
before converting to linear RGB. Previously values could approach2 in encoded
space, becoming nearly5 after EOTF conversion, and then enter alpha blending
as over-range brightness. Original MODULATE limits RGB to255 before blending
(GS software combineTexture's TFX0 clampU8 branch). This fixes the missing
saturation only; byte rounding and fixed-point texture sampling remain open.

`tools/audit_snow_material.py` records four original base-texture descriptors
(5,6,14,25) as TFX0/TCC1 in snow-jam-glide.p2s, with snapshot hash. These are
saved bindings, not proof of every live draw. `snow-colour-gpu-test.html` uses
the actual snow renderer with controlled white textures/half alpha, verifies
sub-limit RGB and saturation before blending for multipliers0.5,1,255/128.
WebGPU and WebGL2 both produce27,128,128 byte RGB in the linear target.
Reports: local/browser-validation/snow-colour-webgpu.json and snow-colour-webgl.json.

Current framebuffer blending remains linear-space, unlike original encoded
GS blending; the GPU test states that limitation explicitly. Full-scene
particle order, depth/clip behavior and original image parity remain open.
No particle emission, sizes, alpha, snow physics or authored textures changed.
Focused snow-renderer regressions and production build pass.

### Original snow alpha equation and blend-space error quantified

`tools/audit_snow_blend.py` verifies the pinned ELF chain for all ten current
profiles: BlendMode1 -> table44B420[1]=5 -> setter table491FB0[5]=3624DC ->
instruction3624E4=24020044 -> GS ALPHA0x44. The selector fields are
A=source,B=destination,C=source-alpha,D=destination, yielding
clamp(((Cs-Cd)*As >>7)+Cd) on encoded bytes. The GS CPU backend's generic
ALPHA selector implements the same equation.

The audit compares that byte equation with ideal linear-space blending for
165,120 one-channel combinations (all source bytes, destination0/64/128/192/255,
alpha0..128). Maximum difference is74 encoded levels; white over black at
alpha64 is127 original versus188 after linear blending. These are calculated
blend examples, not measured full-scene pixel errors. Report:
local/browser-validation/snow-blend-audit.json, with ELF hash and per-profile
setter bindings.

The production renderer is still linear-space. Correcting this requires a
pass that blends snow into an encoded scene destination while preserving
depth and overlapping-particle order, then converts back for downstream
passes as needed. Simply sampling an opaque backdrop for each particle would
lose inter-particle blending; lowering opacity would change source behavior.
No emission/alpha tuning or incomplete blend workaround was applied.

### Encoded snow compositing is live in the normal fog pipeline

`web/snow-composite.js` now converts the world pass's linear color into an
encoded RGBA8 destination and copies its depth with an ALWAYS-depth fullscreen
quad. It then renders the existing snow meshes, in their existing700..709
order, into that shared destination with encoded RGB output. All particles
blend with preceding particles and keep opaque terrain/rider occlusion. Fog
consumes the encoded result directly, without a second OETF conversion.
The source world pass remains responsible for the fog depth sample.

The wrapper hides snow only during the world render and restores visibility.
The snow pass reserves camera layer1 (snow meshes retain layer0 too) and
restores render target, auto-clear, camera mask, background/backgroundNode and
the material's output mode after drawing, including exceptions. Target size
follows the source target. Menu/paused/default standalone snow renders retain
linear output. The originalFog=0 diagnostic bypass still uses the old pipeline.

`snow-composite-gpu-test.html` uses the real snow material and controlled
textures: midtone source/destination, single/overlapping half-alpha particles,
opaque occlusion, target resize, state restoration and actual fog integration.
WebGPU/WebGL2 pass with and without renderer antialiasing. Reports are
local/browser-validation/snow-composite-{webgpu,webgl}[-msaa].json. Half-alpha
white over black is128 in RGBA8 (original truncation127); two overlaps191.
Hardware UNORM rounding is not full GS integer-blend parity. Original instance
order and antialiased silhouette edges still need source-frame comparison.

Production build, focused snow regression and full npm suite pass. Live Sam
Snow Jam renders with this path and was left paused after a jump smoke check.
No physics/emission/opacity parameters changed. This adds a fullscreen
color/depth copy and a color/depth target; performance/power impact has not
been measured, and empty-emitter pass elision remains useful follow-up work.

### Empty snow frames skip the encoded pass

The fog wrapper now detects visible, nonempty snow meshes before rendering the
world. When none are present it skips both the encoded color/depth copy and
snow draw, and feeds the ordinary world texture through OETF in the final
shader. An initialized1x1 dummy texture supplies the inactive binding, so the
first empty frame does not sample an unwritten snow target. Populated frames
resume the encoded destination path. Render/skipped counters are available
through fogRenderer.snowCompositeStats for QA.

The GPU test now steps separate animation frames through empty->active->empty,
resize while empty, restart, hide and show. It checks exact pass-count deltas
and pixels, including a gray sky on empty frames to reject black/stale fallback
output. WebGPU/WebGL2 pass with and without antialiasing (gray-sky checks added
to the antialiased runs). Reports: local/browser-validation/snow-empty-pass-*.json.
The earlier intermittent WebGPU QA failures came from rendering twice in one
frame: PassNode uses FRAME update semantics. The test now awaits the next RAF;
the production scheduling was not changed to accommodate that test.

Focused snow-renderer/billboard regressions and production build pass. This
removes the extra copy/draw work only on empty or hidden snow frames. It does
not establish a measured FPS/power improvement, free the previously used target,
or skip populated particles that happen to project offscreen/transparent.

### Live rider emitter identity corrects flipbook ownership

`tools/audit_original_snow_emitters.py` identifies all ten live rider emitters
in snow-jam-glide.p2s by the owner/config binding and4930D0 vtable at+1F8.
They are the0x210-byte class initialized by370DC8, with texture base+4, count+C,
phase+10, rate+14. Draw371380 reads that phase at3713A8..3713C4. This supersedes
the earlier claim that370788/3708C0 was the live rider emitter's timing owner.
Those functions belong to another class; their passing oracle proved the
recurrence but not this host binding.

Live3710D0 advances phase at371220..371248 after each birth call, including
inactive births, using that call's F12 elapsed value. The browser now advances
phase inside submit() with birth.stepSeconds, not in the later per-visual-tick
output loop. Current emitters normally receive one birth call per tick, so this
is an ownership correction, not a claim of changed cadence on every frame.
20,000 full3717C0/3710D0 birth tests now compare phase too, varying frame count,
rate and elapsed (including zero); ring, color, velocity and visual RNG checks
remain exact. Baseline phases are nonzero (chunks1/2/3 are1.5/4.5/3.0); the
browser still resets phase0. Original initialization/reset phase remains open.

The particle VU traversal test matches visible output order for its supplied
ring payload, but that payload is assembled in host order. CPU upload ordering
and inter-emitter scene submission are still not directly compared, so it
does not prove full original snow draw order.

Causal clarification: originalSnowParticle already clamps RGB to128 before
exposing colour/128 to the renderer. Thus the recently added RGB saturation
guard does not explain overbrightness for ordinary current core particles.
Encoded framebuffer blending was the substantive brightness correction;
the controlled over-range-color GPU test alone was not evidence of such
over-range colors in real gameplay.

Validation of birth-owned phase update: full npm suite, production build and
rebuilt native/WASM Sam/Zoe traces pass (23 scenarios/9,630 frames per rider,
zero mismatches). Original birth/particle oracle passes. Logs:
local/browser-validation/snow-birth-phase-*.log.

### Development start restores captured texture phases

reference_snow_context.py now exports state.emitter_flipbook_phases from the
live0x210-byte emitters' +10 fields. prepare-ui.py publishes these from the
same snow-jam-glide snapshot already used for effect initialization. The
browser validates all10 phases against their frame counts and restores them
when clearing its development particle history. Chunk emitters1/2/3 start at
1.5/4.5/3.0 rather than all0; other captured phases are0.666666687.

The published values match the independently audited live emitter bindings
exactly. test-snow-flipbook-phase.mjs checks the nonzero initial state,64 ticks
of phase/texture selection, all8 chunk frames and restoration after reset.
No new random draws are added when restoring this saved start.

Original fresh construction is distinct:370DFC skips randomization for
frameCount<2;370E14 calls3177F0 for larger counts, then370E30..370E48 forms
((word&7FFFFF)|3F800000)-1 and multiplies by frameCount. Fresh event/object
construction with that RNG ownership is not yet integrated. Current browser
restart semantics still restore the development start, not that lifecycle.

Validation: full npm suite and production build pass. Rebuilt native/WASM Sam
and Zoe traces each pass23 scenarios/9,630 frames with zero mismatches.
Logs: local/browser-validation/snow-phase-seed-*.log. Saved phase values also
match original-snow-emitter-bindings.json exactly for all ten emitters.

### Encoded composite shared by all post-rider GS effects (September 22)

`attachEncodedSnowComposite` now also renders effects registered with
`registerEncodedEffect({object,setEncodedOutput,populated})`: the wake and boost
strips register themselves; the ARA1 start-gate sparks are a child of the snow
group. The pass is skipped only when none of them has geometry. The world pass
now carries a depth-stencil attachment (see board-trail-render-passes.md).
`test-snow-renderer.mjs` filters emitter meshes because the snow group now also
contains the start-gate spark group.
