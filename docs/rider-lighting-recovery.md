# Rider lighting recovery

The browser rider material is still unlit. Adding a generic directional/ambient
light would not reproduce the original data path.

World painter type11 selects tWPIGD_Lighting (constructor2BC9B0, vtable484290).
Its44-byte payload contains a blend-rate float, four8-byte named references and
two scalar floats. ARA1 has three payloads, all currently referencing APBR1, APDK1,
APTN1 and AOBR1 with scalar values1 and0.75. Exact scalar/getter roles and how
these resources feed rider/object shading still need recovery.

`engine/lighting_painter.hpp` ports2BD5A8..2BD698. It directly copies the four
current references, preserves their previous-reference slots, and blends the two
scalars using squared weight before recording their latest samples. Rate remains
unchanged. Lighting's slot218 is the empty2BDA58 method, invoked before and after.
`tools/test_lighting_painter_native.py` compares20,000 original executions: all
reference words, retained slots, scalar float bits, rate and both hooks match.

The names resolve to PS2 DATA/WORLDS/IRR.DAT (extracted from the owned ISO into
local/assets/source/ps2/irr.dat). The bank is count32, count8-byte names, then
count160-byte coefficient records. Each record has ten four-float rows. The PS2
bank is little-endian; GameCube data/worlds/irrngc.dat is big-endian. All45 names
and1,800 coefficient words match exactly after endian conversion.

`tools/export_irradiance.py` validates and exports both-bank provenance, raw float
bits, rows and the original ARA1 Lighting references to
local/assets/native/IRRADIANCE/irradiance.json. It validates exact extents, unique
names, finite values, leaf payload sizes and reference resolution. It does not
assign a guessed spherical-harmonic basis or turn the coefficients into lights.
Next trace normal-basis evaluation, palette selection/blending and shader binding.
No gameplay shading changed in this recovery pass.

## Coefficient accumulation recovered

38ACD0 loads IRR.DAT, copies the8-byte name table and160-byte records into separate
runtime allocations.38AC50 resolves names, and38ABF8 addresses a record by160-byte
stride (its out-of-range behavior must be preserved separately if exposed).

`engine/irradiance.hpp` ports389590: compute weight times modulation once, then
accumulate ten coefficient rows. Input modulation uses A,R,G,B order; coefficient
lanes use R,G,B,A. Each multiply and addition preserves the recovered EE arithmetic
order. There is no invented clamp or normalization of coefficients or weight.

`tools/test_irradiance_accumulate_native.py` executes the whole original helper
for20,000 cases and compares all40 output float words. It cycles all45 owned bank
records, signed weights and modulation values. Half the cases add synthetic
fourth-lane values so zero padding in shipped records cannot hide a lane-order bug.
All cases pass. This proves coefficient accumulation, not normal evaluation or
rider shading.389C38 calls it before389CB8; that subsequent transformation/binding
path and the final normal basis remain to be recovered. No visual change yet.

## Directional contribution projection recovered

`originalIrradianceDirectional` ports389308 (wrapper389520 supplies a direction
vector and unit weight). It projects a directional RGB contribution into the ten
coefficient rows: constant, X²,Y²,Z²,XY,XZ,YZ,X,Y,Z. The source uses separate
operations for X²-Y² and Z*(Z*3)-1, with eleven constants at gp-272C..-2704.
It accumulates only RGB lanes; fourth lanes are untouched. It neither normalizes
the input direction nor clamps color/weight. Caller normalization remains separate.

`tools/test_irradiance_directional_native.py` loads the constants directly from the
SHA1-verified owned executable and executes the full original routine.20,000 cases
match all40 coefficient words, including preserved fourth lanes, varied directions
and signed colors/weights. The tested operation constructs directional coefficients;
it is not the final per-normal evaluation or a rendered-lighting comparison.
389CB8/389840 and shader binding remain the next parts to recover. No visual change
in this pass.

## Coefficient matrix transformation recovered

`engine/generated/irradiance_transform.hpp` (git-ignored, made by `web/build-core.sh` when missing) ports389840 as native scalar arithmetic over
an inclusive channel range. It transforms the quadratic, cross and linear rows
through a9-float matrix, retaining the constant row and unselected channels.
`tools/generate_irradiance_transform.py` reads the SHA1-verified original executable
and translates the fixed load/store/arithmetic sequence; all scalar addresses and
operations must match its supported patterns. Generated code has source PCs for
review. It contains no runtime instruction decoding, guest memory or CPU emulator.

`tools/test_irradiance_transform_native.py` regenerates the helper and executes the
full original routine for20,000 comparisons. All40 words match across the45 real
bank records, synthetic coefficients/fourth lanes, identity/general matrices,
selected-channel ranges and empty ranges. Original incoming FP registers are also
randomized so an accidental dependency on caller scratch state is not hidden.
Matrix meaning at389CB8's call site and final per-normal shader evaluation still
need recovery; this is coefficient preparation, not completed rider shading.
No gameplay rendering changed in this pass.

## Environment bank operations and rider assembly path

`originalIrradianceSum` and `originalIrradianceScale` now cover389730,3897A0 and
389810. These helpers use VU ADD/MUL semantics, distinct from the scalar guard-bit
ADD used by389590. Sum and copy-scale stage ten rows before copying; the native
value-return API retains safe exact destination/input aliasing.12,000 cases per
original routine (36,000 total) match all40 words and the returned destination
pointer, with varied exponent/sign values and aliased/non-aliased destinations.
See tools/test_irradiance_bank_math_native.py.

Original rider assembly1220D8 builds rider+7C0 coefficients. It copies the160-byte
environment bank returned by2EDFB8 (`4FA3C0 + index*F0`), obtains the posed primary
bone through11FF48, retrieves a scalar through2EEFA8, and invokes389CB8. It then
visits eight optional local-light pointers at rider+794 in order through38A6A8.
If a control owner exists at+77C, its+D30 block contributes ambient RGB through
389558 and a counted set of directional RGB/vector pairs through389520/389308.
This is evidence for the assembly order, not a port of all those callbacks.

Remaining:389CB8 view-dependent preparation, local-light handling, final normal
shader evaluation and browser binding. The browser rider remains unlit; no visual
change was made in this recovery pass.

## Fourth-lane rim composition recovered

`engine/irradiance_rim.hpp` ports389620 (fourth-lane-only accumulation) and the
389CB8 tail at38A4D4..38A4EC. The temporary shape's channel3 is transformed through
389840 with inclusive range3..3, then added into destination lane3 with
`inputScale * globalRimScale`. The shipped global at gp-26E8 is0.7500065565, read
from the verified executable by the test. Destination RGB is preserved.

`tools/test_irradiance_rim_native.py` executes that original tail and its actual
389840/389620 callees for20,000 cases. All40 output words match for varied shapes,
matrices, signed scales and preserved RGB values. Shape and rotation inputs are
controlled: this does not validate the earlier389CB8 view-direction/shape-building
code, normal shader evaluation or a final rendered rim light. No gameplay visual
change in this pass.

## Rim shape and view-direction preparation recovered

`irradiance_view.hpp` ports389D50..389E94 after the renderer provides its view
matrix. It reconstructs eye position through the original transposed matrix times
negated translation, forces eye W=1, normalizes point-minus-eye using VU arithmetic,
and computes the horizontal magnitude used by the next angle calculation. This
is the source transpose procedure, not a substituted general matrix inverse.
20,000 nondegenerate affine-view fixtures match eye, direction and horizontal
magnitude bits against the original block. Angle construction is outside the test.

`originalIrradianceRimShape` recovers the initial fourth-lane scratch shape: rows
0,2,3,7 receive the constant/Y²/Z²/X terms and other fourth-lane rows are zero.
Shipped values are0.4954159856, -0.0, -1.4862500429,0.2046655864; the Y² value
comes from global4A43C0.2,000 original entry-block comparisons verify all ten words,
including signed zero. Native storage zeros unused RGB scratch, which the original
tail never consumes; those unused bytes are not claimed to match source scratch.
Both checks run in tools/test_irradiance_view_native.py with validated boundary stops.

Next recover the atan/asin branches and resulting rotation matrix in389CB8, then
connect the full rim contribution and final normal shader. No gameplay lighting
change in this pass.

## Rim angles recovered

`engine/irradiance_angles.hpp` ports389E98..389F9C using the existing recovered
atan/asin implementations. Pitch is negative atan(Z/horizontal). Yaw recomputes
its denominator with EE scalar SQRT (distinct from the prior VU horizontal
magnitude), compares against0.001, clamps the dot ratio to[-1,1], subtracts asin
from pi/2, and applies2*pi-yaw when horizontal Y is negative. The vertical branch
keeps yaw zero. It must not be replaced with a generic atan2 and assumed bit-equal.

`tools/test_irradiance_angles_native.py` runs20,000 original block executions with
real31C228/31C128 callees and constants from the verified ELF. Vertical up/down,
tiny horizontal vectors, both yaw half-planes and general inputs match pitch/yaw
bits. The X reference axis matches the owned snapshot's4FF140 constant. Fully
coincident zero direction is not covered by these fixtures. The boundary stop is
at389F9C, the common branch exit; stopping at389F98 misses nonvertical paths.

Rotation-matrix construction after these angles and live shader binding remain
open. No gameplay shading changed in this pass.

## Rim rotation and assembled path verified

`irradiance_rotation.hpp` ports389F9C..38A4D0. It starts from identity/eye,
constructs negative-pitch Y-axis and negative-yaw Z-axis rotations in source scalar
order, combines them through the VU matrix multiply sequence, transposes, applies
the fixed axis conversion and packs the3x3 for389840. It retains translation and
zero operations rather than simplifying them away.20,000 original comparisons
match all nine words, using real31BE50 sin/cos and testing lazy conversion-matrix
initialization, signed-zero angles and varied eye positions.

`irradiance_rim_pipeline.hpp` now assembles view-direction preparation, angles,
sparse shape, rotation and fourth-lane composition. The complete original389CB8
routine and real math/transform/accumulation callees are compared in
`tools/test_irradiance_rim_pipeline_native.py`:10,000 cases match all40 output
coefficient words. The only controlled callback supplies the renderer's view
matrix. These fixtures use nondegenerate view/point inputs; they do not establish
all exceptional floating-point cases or final GPU shading.

The rim coefficient path is now connected internally, but live environment/local
light coefficient assembly and final normal shader binding remain unfinished.
No gameplay shading changed in this pass.

## Environment irradiance mix recovered

`environment_irradiance.hpp` ports2ED92C..2EDB18 after terrain/painter updates.
If the slot's+24 selector exceeds0.1, the third bank is blended directly toward
previous state. Otherwise weight=brightness*gain mixes the first two banks, then
gp+A80 supplies the incoming weight against previous coefficients. Scale/sum use
the recovered VU helpers and preserve operation order; weights are not clamped.
The semantic meaning of the+24 selector is still unconfirmed.

`tools/test_environment_irradiance_native.py` executes both original branches with
real389730/3897A0/389810 callees for20,000 cases across6 rider slots. All40 output
words match, including exact selector threshold and nonunit/signed weights. Bank,
brightness and gain getters are controlled boundaries, not claimed recovered by
this test. No live rider shader change yet.

The next brightness getter is2EDF00. It forms weighted RGB using coefficients
0.299/0.587/0.114, with lower/upper thresholds0.1/0.45; its interior curve calls
40DA10 and40D758 using normalization0.35 and exponent-like parameter0.6001175642.
Those library operations and the full brightness path still require verification.

## Full brightness curve recovered

`lighting_brightness.hpp` now ports the complete2EDF00 calculation: weighted RGB,
0.1/0.45 thresholds, normalization by0.35, then exp(log(value)*0.6001175642),
with the original arithmetic order and output clamp. Alpha does not participate.

`tools/generate_lighting_math.py` statically translates leaf kernels40E550 and
40E048 from the SHA1-verified executable into `engine/generated/lighting_math.hpp` (git-ignored, made by
`web/build-core.sh` when missing; never published). The
result uses direct arithmetic/fixed branches and a small immutable constant pool;
there is no runtime instruction decoder or guest memory. Unconditional branches
are folded during generation. This avoids substituting host std::pow/log/exp and
assuming identical results. General libc errno behavior is not part of the helper.

`test_lighting_math_native.py` verifies20,000 log and20,000 exp outputs in the
lighting domain. `test_lighting_brightness_native.py` executes original2EDF00 with
its actual log/exp wrappers, leaf kernels and classification callees for20,000
comparisons:3,080 lower-clamped,6,495 upper-clamped,10,425 nonlinear results. All
output bits match. The tests rerun after branch folding. These are native/source
comparisons; WASM integration and final rider shading remain to be connected.
No gameplay rendering changed in this pass.

## WebAssembly brightness verified

`tools/test_lighting_brightness_wasm.sh` regenerates20,000 input/output fixtures
from full original2EDF00 executions, compiles the native brightness helper for
WebAssembly and compares every output word. All20,000 match, including the lower,
upper and nonlinear cases. Run through `npm run test:lighting` in web. The isolated
WASM test exports only the batch evaluator and allocation helpers; it does not
claim that live rider shading is already connected.

The slot+24 alternate-bank selector is updated at2ED5B4..2ED5DC as
`previous * 0.8999999761581421`, plus0.10000000149011612 when rider+3FC is nonzero.
120F20 clears rider+3FC during its query update;140C28/140C30 are its getter/setter.
The selector initializes to0 and is0 in the inspected glide/jump60 snapshots.
Its gameplay meaning and complete setter dispatch remain unconfirmed. Do not
label it a shadow, tunnel or weather flag without further evidence.
No gameplay rendering change in this verification pass.

## Live assembly audit

`tools/audit_rider_lighting.py` validates the glide snapshot against Zoe's original
assembly evidence, resolves the actual rider environment index through the audited
140B80 getter contract, and records the Lighting painter, environment bank, rider
bank, eight local-light slots and control-specific extra block. Output is
local/event-activation/rider-lighting-glide.json; coefficient arrays are comparison
outputs, not assets to inject into gameplay.

The captured selector and rider+3FC are0. APBR1/APDK1 are already resolved to bank
indices12/14 with tag123400; APTN1/AOBR1 remain named references. All four resolve
to the decoded IRR bank. Three local-light slots are non-null and each has kind1.
Control-extra ambient is zero and its directional count is0. Thus the shipped
view includes local-light contributions; environment-only rider shading would omit
active original lighting. Next recover the kind1 path in38A6A8/38A530 before live
assembly. The meaning/nonzero-writer path of rider+3FC remains unconfirmed.
No gameplay lighting changed during this audit.

## Local-light geometric query recovered

`engine/local_light_query.hpp` ports38A530. It computes light-position minus sample
position, rejects only when squared distance exceeds squared radius, normalizes
the direction, and returns distance, inverse distance and negative dot with the
light's authored axis. Radius is at+1C, axis+2C and position+38. Exact-radius
points are accepted. Coincident positions use source distance/inverse1, direction
XYZ0/W1. Rejection leaves all output storage untouched.

`tools/test_local_light_query_native.py` executes the full original routine for
20,000 cases:8,932 accepted, with all output words and rejection preservation
matching. Tests include negative radii (squared by the source), exact-radius and
coincident cases. This verifies geometric preparation, not attenuation.

38A6A8's kind1 path next compares the returned axis cosine with thresholds at
light+60/+5C and applies distance/angular attenuation modes at+64/+65. Therefore
kind1 must not be assumed to be an isotropic point light. The complete spotlight
attenuation/contribution remains next. No gameplay lighting changed in this pass.

## Spotlight radial attenuation recovered

`spotlight_distance.hpp` ports38A710..38A7FC after a successful geometric query.
It rejects cosine values below the outer threshold. Radius<5000 uses full fade;
otherwise distance<=3750 uses full fade and greater distances use
`1-(distance-3750)*0.0007999999797903001`, without an invented clamp. The distance
factor uses100*inverseDistance, saturated to1 on the near side; modes1/2/3 select
linear/squared/cubed factors below1, while other signed mode values retain1. The
intensity at+14 multiplies this factor. Angular attenuation is a later stage.

`tools/test_spotlight_distance_native.py` compares20,000 original block executions:
12,077 pass the outer-cone gate and all output intensity/fade words match. Cases
include threshold equality, radius/fade boundaries and signed mode values. Query
results are controlled inputs, so these are arithmetic-stage cases rather than
20,000 full physically valid point/light configurations. Inner-cone weighting,
light RGB contribution and live rider shading remain unfinished. No visual change.

## Spotlight angular attenuation and complete contribution

`spotlight_angular.hpp` ports38A7FC..38AA74. Inside the inner cone it powers the
actual cosine. Between cones it powers the inner threshold and multiplies by
`(cosine-outer)/(inner-outer)`. Modes0..4 use their original explicit multiplication
orders; other signed8-bit modes use exponentiation by squaring, with reciprocal
handling for negative powers and the original zero-base special case. The final
weight clamps to0..5.22,000 original comparisons match, including cone equality,
zero branches, negative/large exponents and both regions.

`spotlight_irradiance.hpp` assembles the complete kind1 path: geometric query,
outer-cone gate, distance fade/factor, angular weight, RGB scaling, and directional
coefficient projection. `tools/test_spotlight_irradiance_native.py` executes the
whole original38A6A8 kind1 path with actual38A530/389520/389308 callees for12,000
cases. All40 irradiance words match, including rejection and fourth-lane retention.
This does not cover kind2, live light-list selection, or final rendered shading.
No gameplay visual change in this pass.


### Kind2 point-light contribution complete

engine/point_light_irradiance.hpp recovers38A618 plus the38AAA0..38ABE0 branch:
inclusive radius query, normalized direction, inverse-distance modes1/2/3,
5000/3750cm long-distance fade, intensity clamp0..5, RGB multiplication and
389520/389308 directional coefficient contribution. Coincident points follow
EE signed-max DIV saturation and zero direction rather than host infinities.

The earlier coincident-point failure came from an incomplete local oracle patch:
fixing the divide helper was insufficient because38A678's generated inline
zero-divisor branch bypassed it and assigned host infinity. The test now corrects
that exact local branch too, preserving its divide-zero flag. The original
executable/generated input files and shared oracle are unchanged. Runtime native
code was already using the established collision_scalar::divide behavior.

12,000 complete original kind2 executions now match all40 coefficient words
with real query and directional callees, including coincidence, radius rejection,
long-distance fade and all supported distance modes. The harness verifies the
owned ELF hash. This completes the helper, not live light selection or GPU rider
shading; the browser's rider lighting integration remains unfinished.


### Renderer transfer and normal evaluation recovered

Rider draw1224C8..1224E4 resolves graphics slot228/22C. In the verified glide
snapshot, renderer=61BA60, vtable=493260, adjustment0, target3954D0. That setter
copies all160 coefficient bytes into renderer+6BB0 unchanged. The read-only
rider-lighting audit now records this binding and its storage; snapshot-time
renderer contents are not assumed to belong to the human's draw.

396B40..396C28 emits the ten rows to VU data addresses7..16 (V4_32 unpack),
then MSCAL byte-entry1150. Extracted VU program2 (ELF DMA source434990) has the
matching coefficient preparation:1150..11F8 multiplies all40 floats by255 and
stores them back. Its normal-lighting block loads these rows into vf1..vf10.
The snapshot's final loaded microprogram is mostly program3; that end-of-frame
state does not establish which program was used for every earlier rider draw.
Full draw/program selection remains to be audited before claiming whole-material
binding, but program2's coefficient-consuming path is independently recovered.

engine/irradiance_evaluate.hpp ports the source upper instructions04F8..05B0:
form squares and cross terms, then accumulate constant, X²,Y²,Z²,XY,XZ,YZ,X,Y,Z
in original order. Input normal is already transformed by the preceding vertex
path; no extra normalization is inserted. Four float lanes clamp to0..255.
Source0668 FTOI0 truncates those lanes for vertex-color packing; the helper exposes
both float evaluation and byte output. Material multiplication, fourth-lane/rim
composition, final blending, normal transformation and GPU execution remain
separate requirements. No generic scene lights or artistic gain were added.

tools/test_irradiance_evaluate_native.py extracts the program from the SHA1-verified
owned ELF, verifies all45 bank records against GameCube, executes the complete
original255 coefficient preparation and the isolated original upper evaluation
instructions in VU1Interpreter. Matrix-transformed normal and coefficient registers
are boundary inputs; unrelated vertex/clip/next-vertex instructions are replaced
with NOPs in the test copy. All four float outputs and all four quantized lanes
match in12,000 cases, including axis normals, shipped banks and signed synthetic
coefficients. Test evidence and hashes live in local/rider-lighting.

The VU upper executor is compiled with frounding-math and ffp-contract=off for the
oracle. The cached library's host FMA contraction produced a one-ULP mismatch in
MADD; recompiling those original interpreter operations separately fixed the
reference configuration without changing the input microcode or native arithmetic.
This is source-operation conformance, not a console framebuffer comparison.
Browser rider materials remain unlit until live coefficient and shader binding
are connected. Next: verify draw/program/normal/material mapping, then use this
recovered evaluation in the GPU material with the live rider lighting bank.


### Rider draw callbacks and packed-normal transform

The verified renderer vtable also resolves slots31C->37A610,37C->386BD0 and
3E4->396B40, all with zero this-adjustment. Geometry draw310640 invokes31C for
active model parts;37A610 invokes3E4 at37A6A0 before its part draw loop. Thus the
rider geometry path reaches the recovered coefficient upload. The extended
read-only audit records and checks all three callback targets.

The packet-template construction branch365968..365990 emits a DMA CALL to
434990 (VU program2); matching stored templates are present in the captured
renderer allocation. This narrows program selection, but exact material/template
selection per part still requires verification and is not replaced with a guessed
constant or the end-of-frame loaded microprogram.

Added originalIrradianceTransformNormal: program2 upper04B0 decodes packed signed
normal components with ITOF15 (divide32768), then04E0/04E8/04F0 performs the
supplied three-column matrix multiply. There is no renormalization or synthesized
inverse transpose. Matrix palette selection/blending remains caller-owned.

test_irradiance_evaluate_native.py now retains and executes those four original
instructions ahead of the existing polynomial/clamp/FTOI0 block.12,000 cases
match all transformed normal floats, four lighting floats and quantized color
lanes, using signed16-bit normals, negative/positive axis extrema, identity and
general supplied matrices, all45 shipped banks and signed synthetic coefficients.
The previous normal-boundary description is superseded: packed normal and matrix
columns are now the boundary inputs. No browser shader has been connected yet;
material modulation/blending and live coefficient updates remain unfinished.


### Actual rider texture function: HIGHLIGHT2

The rider's37A610 draw path calls37CBC8 at37A950 with a2=3 and the material's
texture handle.37CBC8 forwards to3691B0, which writes TEX0.TFX bits35..36 while
preserving the other descriptor bits. This explicitly selects HIGHLIGHT2, not
MODULATE. The fourth lighting coefficient lane is a color highlight contribution;
treating it simply as opacity or ignoring it would produce different shading.

The extended audit follows active geometry parts through their selected model
header/data pair, reads signed material count at header+48 and20-byte records at
data+header[20], and resolves their texture handles through renderer+18F4. The
verified Zoe snapshot contains9 batches in8 active parts. Every descriptor has
TFX3/TCC1. TopB_H's second batch and Mop_H have flag8000; their ordinary draw path
selects blend enum5 at37A9A4, while other batches select enum1 at37AA90. The enum
setter table resolves1->3624D0 (ALPHA2A, source-color passthrough) and5->3624DC
(ALPHA44, source-alpha/128 blend). Alternate draw modes, alpha/depth test bits,
framebuffer alpha handling and final pass ordering are not yet fully audited.

engine/rider_texture.hpp adds the confirmed byte-domain HIGHLIGHT2 RGBA operation:
RGB=min(255, floor(textureRGB*lightingRGB/128)+lightingA); A=textureA.
Inputs are already sampled/interpolated byte values; this helper does not perform
texture filtering or framebuffer blending. It preserves the original encoded
color arithmetic rather than silently substituting linear-light multiplication.

tools/test_rider_texture_native.py extracts combineTexture and clampU8 from the
local GS software backend, selects TFX3/TCC1 and compares16,777,216 combinations
of texture, lighting and highlight bytes, including all alpha values and clamp
boundaries. All pass. The reference is the GS software implementation, not a new
hardware framebuffer capture or an original executable test. Hashes and material
audit records are retained in local/rider-lighting and local/event-activation.

This resolves the texture-combine equation and ordinary opaque/alpha material
branches. Live lighting coefficients, complete per-part render state, texture
variant verification and browser GPU material integration still remain; rider
materials are not yet switched to the recovered lighting path.


### Browser GPU HIGHLIGHT2 stage verified

web/rider-lighting-nodes.js now implements riderHighlight2Node in TSL. It accepts
already sampled/interpolated texture and lighting byte values, applies the
recovered integer-domain RGB multiply/divide128 plus rim term and clamp, and
preserves texture alpha. It deliberately does not insert sRGB decode/encode,
texture sampling or framebuffer blending. The eventual material caller owns
those surrounding stages.

web/rider-lighting-gpu-test.html executes this production helper in a real
RenderPipeline with output color conversion disabled and an RGBA8 render target.
The test sweeps all256 texture/lighting byte pairs at six rim values0,1,127,128,
254,255, permutes RGB channels and varies texture alpha across all256 values.
The393,216 four-channel pixels match the source-extracted GS reference bytes
exactly on both WebGPU and forced WebGL2. WebGL readback rows are normalized to
ScreenNode's documented top-left coordinate convention; channels are not altered.
Reports: local/rider-lighting/gpu-combine-webgpu.json and gpu-combine-webgl.json.

The native exhaustive test additionally emits the synthetic reference image data
at web/public/test-data/rider-texture-reference.bin (not owned game textures).
Its source/golden hashes are in texture-combine-reference.json. The GPU test is
an additional Vite entry; default gameplay does not load its data or perform
readbacks. This verifies only HIGHLIGHT2, not texture filtering, coefficient GPU
arithmetic, material pass state or live rider lighting. Main rider materials
remain unchanged pending those connections. Production build passes.


### GPU coefficient evaluation and original rounding

riderIrradianceNode now evaluates the ten preprocessed coefficient rows and
quantizes vertex-color lanes on the GPU. Its inputs are the already transformed
normal and coefficients after original255 preprocessing. Normal/matrix selection,
per-vertex placement/interpolation and live coefficient ownership remain caller
responsibilities. Coefficient reads and normal monomials are retained in shader
variables rather than rebuilt independently for each color lane.

A straightforward float implementation differed from original VU output in one
of12,000 cases (case7190 green:162 instead of161). A float residual correction
was also optimized away on the tested WebGPU backend:1+(-1e-8) still returned1.
The final shader uses integer mantissas for finite VU-domain add/multiply with
toward-zero rounding, signed-zero/denormal handling and finite saturation. Adds
retain guard bits and sticky subtraction; multiplies assemble a48-bit product
from16-bit partial products. This avoids dependence on GPU float reassociation
and does not decode or execute console instructions at runtime.

irradiance-gpu-test.html draws all12,000 original VU reference cases into RGBA8
and compares every channel. Both WebGPU and WebGL2 pass with zero mismatched
pixels/lanes and maximum byte error0. Golden inputs include original transformed
normals,255-scaled coefficients and original quantized outputs; generation and
hashes are recorded by test_irradiance_evaluate_native.py. The diagnostic mode
checks signed small-addend rounding and an overshooting0.1f*0.1f product using
a float render target. Shader errors are reported explicitly in the QA page.

Scope is finite coefficient evaluation and byte output on the tested backends,
not every exceptional float input, vertex normal transformation on GPU, a power
benchmark, or final rendered-rider parity. Integer precision handling adds shader
work; performance should be measured once the live per-vertex material is wired.
The test data/readbacks are not part of the gameplay loop. The rider's live
material is still unchanged pending coefficient and material-state integration.


### Live browser environment coefficient bank

2EDA4C passes group+38 (the terrain lighting ratio, not ambient RGB) to2EDF00.
2EEFF0 resolves Lighting slot148/14C->2C15D0, returning scalar+48. The ELF's
coefficient incoming weight at gp+A80 is0.5. prepare-environment.py now exports
these inputs with the owned bright/dark/alternate banks, checking all ARA1
payloads have identical references/scalars and PS2/GC coefficient equality.
The identical payload selection is constant for the supported ARA1 normal path;
this does not implement arbitrary future area painter transitions.

After each successful live environment sample, environment_bridge.cpp computes
originalLightingBrightness(state.ratio) and originalEnvironmentIrradiance, then
retains the resulting40-float bank. It uses the current supported rider+3FC=0
normal branch; alternate-flag ownership remains unimplemented. A fresh browser
instance starts the bank at zero and rider resets retain it. Original mid-race
coefficient history/whole-level initialization parity is not claimed and no
captured target bank was pasted into runtime data. init_environment reloads start
a fresh bridge state; broader area reload semantics remain to be recovered.

API: environment_irradiance returns40 unscaled coefficient floats (before local
lights/rim/255 preprocessing). environment_irradiance_info returns ready,gap,
successful update count,brightness,gain,incoming weight,then four ratio ARGB
floats. This is an environment bank, not the complete rider shader bank. Local
lights, view-dependent rim and live GPU material binding remain unconnected.

The600-frame integration test checks every successful bank recurrence against
independent JS float/EE-subtraction arithmetic, once-per-frame ownership and
finiteness. It records599 successes,211 brightness values and one pre-existing
texture-lattice gap at frame445. That frame preserves the previous bank/count;
it is not represented as successful sampling. The underlying missing authored
guard-texel issue remains open. No edge color was fabricated to hide it.

Expanded native/browser gameplay traces now initialize the real environment
lattice and compare ambient/ratio diagnostics plus all40 coefficients. This
exposed environment_lighting.cpp's old std::fesetround-only local guard and
ordinary float operators in WASM. Replaced them with OriginalRounding and explicit
multiply/VU-add helpers while preserving scalar EE add/sub calls.60,000 original
texture/patch/filter cases remain bit exact, and all9,630 native/browser frames
across23 scenarios now match all713 fields including actual lighting/effect
colors. Previously the broad trace did not initialize environment sampling.
Full npm suite and production build pass. This is not console framebuffer parity.


### Authored local lights and ranked eight-slot selection

The captured local lights are fixed world records: their parameters do not move
between glide and jump31, and their bytes16..111 exactly match authored ARA1
kind6 records. export_local_lights.py exports all190 records from owned BAM.SSB
into local/assets/native/ARA1/local-lights.json, preserving resource IDs, chunk,
kind, intensity, stored brightness, radius, axis/position/bounds and attenuation
parameters. No snapshot pointer or captured light color becomes a runtime input.
Captured candidate/selected IDs are a separate development audit artifact.

122088 gets the posed primary bone via11FF48 and asks2F5B68 for8 lights from
rider+860's query-scope list at+210/+214. This is not a global nearest-light scan:
it filters node kind6 and ranks with2F5D30. The rank uses2F6168 radius/cone query,
attenuation/intensity and stored brightness+18, without shading's0..5 clamp.
Its axis cosine dots the unnormalized displacement first, then multiplies by
inverse distance; reusing38A530's normalized-dot operation order would differ.
Kind0 returns stored brightness; kinds1/2 evaluate spot/point influence; other
kinds return0. Coincidence uses distance/inverse1 in the ranking query.

local_light_selection.hpp ports the full rank and fixed rider capacity8 insertion
order. Positive ranks insert descending; new equal ranks precede existing equals,
but equal ranks at the full-list cutoff are rejected. Non-light nodes are skipped.
20,000 full original ranking executions and20,000 original selector executions
match, including signed attenuation modes, coincidence, far fade, ties and lists
longer than8. The selector oracle controls only the supplied ranks; ranking itself
is separately executed with the real2F6168 callee.

Three captured query scopes were mapped uniquely to authored records. Fresh
original ranking+selection and native execution agree for all three. However,
glide's retained list has3 lights while a fresh call at its captured current
primary-bone position has2: the extra retained light is now outside its radius.
This is explicitly reported as an unresolved update-phase/selection-point issue,
not forced to match by moving lights or weakening the ranking comparison.
Jump31's retained1-light and jump90's empty list match fresh selection.

120E50 refreshes query scope with332DB8 on rider+400 bounds before122088;
12B788 calls120E50 for its rider list. Recover this world-query and phase ownership
before live use. Filtering all190 lights globally would incorrectly admit local
kind0 lights and lose original traversal/tie order. Spatial candidate generation,
retained-list update timing, final rim assembly and GPU material hookup remain
unfinished. The exporter/ranking pass makes no gameplay visual change yet.


### Spatial region classification and retained-list timing evidence

332DB8 initializes separate output lists/capacities, then traverses up to8 roots
in the scope's world-region manager. Root records are20bytes: signed exponent,
three signed cell coordinates,root pointer. Empty roots may contain irrelevant
header data and must be skipped before interpreting their coordinates. The
captured glide/jump31/jump90 scopes each have4 active roots; the read-only
query-scope audit records roots and rider+400/+410 query bounds without turning
captured pointers into runtime scene data.

engine/spatial_region.hpp ports328360: scale is constructed from exponent bits,
and low/high cell edges use the ELF's0.20000000298 padding with EE arithmetic.
It preserves source return codes and asymmetric final tests (including the
repeated Y condition and absent final minimum-Z test), rather than substituting
an intuitive generic containment test.20,000 original executions match, covering
all three codes and padded boundary equality. The test verifies the owned ELF
hash and exact padding constant. Remaining traversal, leaf filtering, capacities
and candidate ordering are still unimplemented for browser lights.

The existing continuous jump capture gives stronger scheduling evidence:
resource4104 has radius5000cm; the current primary-bone point first exceeds it at
tick371 (5022.52cm), remains listed through372 (5057.71cm), and disappears373.
This does not by itself prove a fixed refresh interval or select an earlier input
point. audit_light_query_scope.py records that evidence and validates capture
continuity and the light's exact authored-body match. Do not patch in a guessed
two-tick delay.26DD90 calls12B788 after other world phases;12B788 invokes120E50
for its actor list, which refreshes332DB8 then122088. Full timing remains open.


### Native light-only spatial traversal and event index

spatial_light_query.hpp now projects332DB8/33B748/340DC0 onto the extra-node
list consumed by light selection. Node lists are visited before children. Partial
child traversal uses Gray order0,1,3,2,6,7,5,4; wholly included subtrees use0..7.
Child boxes use source half-scale/padding and strict intersection/containment
comparisons; partial recursion stops at exponent11. Omitting collision-instance
and entity result lists does not alter the extra-node visitation or light filter.

5,000 randomized original traversals match ordered light results (622 nonempty
queries,2,926 light visits), including empty roots, mixed extra-node kinds,
multiple regions and depth transitions. The oracle executes332DB8,328360,
33B748 and340DC0, with only collision-list collector3309D8 stubbed. Synthetic
entity lists are empty. Its test copies correct eight recursive JAL lowerings in
each traversal helper: the generated code used goto-to-entry and returned early
to an inner guest return PC. Direct C++ recursion preserves original emulated
stack/return instructions. Original executable/generated input files are unchanged.

export_light_tree.py follows the captured world index, maps every indexed light
uniquely to owned ARA1 payload bytes, removes non-light nodes/empty branches while
preserving cell depth and ordering, and checks the resulting topology across
glide, jump31 and jump90. The118-node light-only index is identical in all three
and contains188 distinct authored lights. The two other ARA1 records (IDs8 kind3
and264 kind0, both at origin) are absent from this event index; they are explicitly
listed as excluded, not silently dropped. No rider pose, selected-light list,
coefficient bank or heap pointer is exported as runtime input.

The index is stored in local/assets/native/ARA1/light-tree.json with source and
checkpoint provenance. Fresh original queries on each matching full snapshot
produce the same ordered light candidates as native queries on this pruned index:
glide5, jump315, jump909. This verifies the static Snow Jam index/query projection,
not general octree insertion, scene streaming, dynamic light activation or the
retained-list timing. The existing timing discrepancy remains recorded. Next
bind source rider query bounds/update phases and final light/rim assembly; no
browser material switch is made in this pass.


### Original rider query bounds and measured refresh calls

rider_query_bounds.hpp ports the complete11E150 bounds calculation: eight corners
in source order from physical right/forward/up, ±150cm forward, ±100cm sideways,
+250/-50cm up. Position is rider+110 unless the explicit override is supplied.
Bounds W lanes retain the position W; the query sphere copies its center and
writes radius250. These fixed query extents are distinct from bodyScale-dependent
collision radii.20,000 full original executions match all12 output words, and
rebuilding from original physical fields matches all122 captured boxes exactly.
The browser exposes rider_query_bounds; its live input-state differences produce
up to0.3389cm box error in this replay, despite the kernel's exact conformance.

To measure timing without guessing, build_light_refresh_probe.py creates a
separate patched jump31 state. A guarded hook at1220C4 records the completed
122088 call's tick, point, physical position, bounds, scope count and eight selected
pointers. It preserves all touched128-bit GPRs, SP, HI/LO and floating-point state,
replays the overwritten epilogue, and caps writes at64 records. The code/data
arena was verified zero in four supplied reference states. Neither original ISO
nor baseline state was modified. The short disposable run is not for gameplay.

Five observed refresh calls occur at ticks369,372,375,378,381. Every recorded
point, position and bounds matches that tick's unmodified completed-state capture.
The query at372 removes the light; this becomes visible in capture373. At the
probe run's completed tick384, all720 compared rider words match the unmodified
reference exactly. The probe process was stopped and the original jump31 state
reloaded; original hook bytes and zero code arena were verified through PINE.
Evidence: light-refresh-records.json/.bin, light-refresh-probe-parity.json and the
probe build/patch manifests under local/rider-lighting.

This measures the current reference run, not a universal modulo-three scheduler.
World wrappers26CC48/26CD20 and deferred26F850 include stateful job gating, so
follow that ownership before hardcoding a refresh cadence. Current browser light
selection remains unbound. The per-frame query-bounds getter is ready to supply
that driver, with no arbitrary search radius or cached probe results in runtime.
Full npm suite/production build and9,630-frame native/browser comparison pass
with725 fields, now including query bounds. General scene/job timing, final rim
assembly and visible rider shading remain unfinished.

## Complete rider assembly and camera transform recovery

`engine/rider_irradiance.hpp` now assembles the complete numeric path of1220D8:
copy the environment bank, apply the view-dependent rim contribution, visit the
eight selected local-light pointers in order, then add optional controller ambient
and directional contributions.38A6A8 ignores kind0 and kind3 contributions even
though the independent ranking routine assigns kind0 its stored brightness.
`tools/test_rider_irradiance_native.py` passes2,000 mixed assemblies, comparing all
40 words against original1220D8 and its real numeric callees. Environment-index,
bank, pose, view and rim getters are controlled boundaries. The null-pointer
pattern is independent of light kind, so non-null ignored kind0 entries are
actually exercised. Log:local/rider-lighting/assembly-test.log.

`engine/camera_transform.hpp` adds original166F90 output construction,31B7A8
matrix-to-quaternion conversion and395750 renderer Euler view construction.
The166F90 path preserves the source identity/translation, negative pitch about Y,
negative yaw about Z, VU matrix multiplication, matrix-to-quaternion branches and
final quaternion conjugation.395750 begins with its authored Y/Z permutation,
multiplies negative f12/f13/f14 rotations about X/Z/Y respectively, then transforms
the negated eye (W negated a second time). It is not interchangeable with the
algorithm matrix. Neither helper uses a host graphics-library lookAt replacement.

`tools/test_camera_transform_native.py` passes20,000 cases against full original
166F90/31B748/31B7A8 and395750 with real31BE50 sincos. Compares16 algorithm matrix
words,8 algorithm position/quaternion output words and16 renderer view words per
case, including signed-zero/cardinal angles and randomized orientations. The test
observes the matrix at31B748, then executes the real callee. Log:
local/rider-lighting/camera-transform-test.log. This is numerical kernel evidence,
not a rendered camera or lighting comparison.

The glide snapshot resolves the rim renderer to61BA60, vtable493260. View getter
slot11C is395C68 (zero this-adjustment), returning the pointer at renderer+13E4.
The Euler setter is slot10C→395750; slot114→395C38 copies a supplied matrix.
The13E4 pointer is a matrix-stack head (3956E8 pushes,395730 pops), so recovering
its live owner/caller and update timing remains necessary. Do not assume that the
Euler setter is the active gameplay producer merely because it is in the vtable.
Final local-light refresh scheduling, live view ownership and normal/material
binding remain unfinished; the browser rider materials are still unlit.

## Quaternion render-view path and live camera output

The gameplay camera's15E668 path calls166640/166F90 after collision/shake,
then15E968..15EAE8 converts the resulting outer+30 quaternion into a matrix,
left-multiplies G at4C53A0 and adds the negative outer+20 eye translation. G is
(0,0,1,0; -1,0,0,0; 0,1,0,0; 0,0,0,1), stored as four source vectors.
This path is distinct from the395750 Euler setter. Source22B23C..22B254 and
2D9260..2D9274 copy a selected camera's+40 matrix through renderer slot114.
The active camera selector and matrix stack at the instant of rider assembly
still require direct confirmation; completed savestates show identity at the
renderer stack head in glide/jump31/jump90, so those end-of-frame matrices must
not be passed off as the matrix that rider lighting consumed during drawing.

`originalCameraRenderView` now implements15E968..15EAE8, preserving unnormalized
quaternion arithmetic, all VU operation order and the final translation. Added
this stage to the original camera transform oracle:20,000 cases match all16
words, in addition to the previously verified algorithm transform and Euler
setter. The original stage runs directly with supplied quaternion/eye/G; this
is not a full compositor/collision/shake comparison.

Browser core now computes the transform and view from its final camera output
once per camera update and exposes16 source-coordinate floats through
`camera_render_view`. `check-camera.mjs` checks finite values, eye-at-origin,
positive target depth and centered target during charged/held jumps and landing
for Sam/Zoe. This publishes actual live camera data without changing the existing
visible camera renderer or prematurely selecting a rider-lighting update phase.
Native/browser traces now compare all16 view values as well:9,630 frames,
23 scenarios,741 fields,zero mismatches. Full npm suite and production build pass.
Final rider material binding and local-light refresh timing are still open.

## Direct draw-time view probe

`tools/build_rider_view_probe.py` creates a derived jump31 savestate with a
bounded hook at1220D8. It records only the human rider14701A0, preserving every
modified128-bit GPR and replaying the original stack allocation/save. The64-entry
ring is actually a saturating log: after capacity it stops recording and continues
original execution. All code/data bytes are verified empty in four baselines.
The source ISO and baseline savestate are unchanged.

A short isolated PCSX2 frame-advance run recorded five consecutive lighting-entry
calls at ticks370..374. Each sees a nonidentity matrix at renderer stack pointer
61CB40, depth0. This resolves the earlier misleading identity observed at the end
of frames: the matrix is set for drawing and restored afterward. The final logged
matrix equals the completed outer camera's+40 at1597D50 (outer camera1597D10).
The outer position/quaternion are at+20/+30. No assumption about the Euler setter
was used. This validates the final quaternion path as the matrix source for the
captured human-rider draw; it does not establish all viewport or replay modes.

`tools/audit_rider_view_probe.py` validates the source-state hash, actor, consecutive
call ticks and final matrix. All720 rider words at completed tick374 match the
unmodified continuous reference. It exports four original view fixtures from
baseline glide/jump31/jump90 and the final probe frame. The native camera transform
reference now compares these captured position/quaternion→matrix pairs as well:
all16 words match in all four, alongside20,000 original numerical cases.
Artifacts: rider-view-records.bin, rider-view-memory.bin, rider-view-probe-audit.json,
camera-view-snapshot-fixtures.bin, camera-transform-test.log inlocal/rider-lighting.
These are reference evidence, never browser runtime assets.

The probe emulator was stopped and the isolated instance restored to unmodified
snow-jam-jump-31.p2s, paused. PINE verified original1220D8 prologue bytes and a
zero C0000..C3FFF probe arena. Actual rider material integration, light-selection
refresh scheduling and draw-time assembly ownership still remain open. Live camera
view calculation is implemented; the draw-time source uncertainty for this short
human-rider sequence is now resolved.

## Authored light world and retained selection integration

`engine/rider_light_world.hpp` now combines the recovered spatial query, local
ranking/eight-slot selection and complete rider coefficient assembly using authored
resource IDs. `OriginalRiderLightWorld::refresh` returns both ordered candidates
and selected IDs. `shade` resolves precisely that retained list and accumulates
its contributions at the current point; it never re-ranks lights during drawing.
Selections belong to individual riders and the immutable world can be shared.
`rider_light_asset.hpp` provides one native/browser JSON field mapping for the
catalog and static index, checking matching versions/source hashes and index IDs.
The shading path includes optional original controller ambient/directional inputs.

The local-light selection oracle now also loads this combined world and compares
its queries/selections for all three authored snapshot scopes against the original
candidate/ranking results. Existing20,000 rank and20,000 selection comparisons
still pass. The retained glide list remains3 lights while a fresh query selects2;
that scheduling discrepancy is not erased by this integration. The complete
rider-assembly oracle now additionally exercises retained resource-ID shading
for2,000 cases against all40 original coefficient words. It asserts shading does
not mutate selection. These checks cover the separate refresh and draw phases,
not a claim that the browser's world-job scheduling has been recovered.
Logs:world-selection-test.log and assembly-test.log inlocal/rider-lighting.

Source120E50 calls332DB8 then122088 without a local tick gate.12B788 iterates
all primary riders and invokes120E50;26DBF0 calls12B788 at26DD90 after its
stream/world update work. Refresh cadence therefore remains a caller/job ownership
question, not something to encode in the selection algorithm as tick%3.
The new combined asset/backend layer is not yet invoked by browser materials.

## Browser bridge and authored package startup

`web/rider_lighting_bridge.cpp` now compiles the shared light-world backend into
the production WebAssembly core. APIs: init_rider_lighting(catalog,tree),
reset_rider_lighting(), refresh_rider_lighting(bounds,rankPoint),
shade_rider_lighting(environment,view,point,rimScale,constants,extra,count),
rider_lighting_selection() and rider_lighting_info(). Bounds use min4/max4;
rankPoint is source primary-bone XYZ. Shading uses explicit current inputs and
optional controller ambient plus up to5 direction/color pairs. It never refreshes
selection or advances environment history. Counters distinguish refresh/draw
phases; reset clears retained IDs and counters without discarding the static world.
Invalid asset loading constructs a replacement before publishing it.

`web/prepare-rider-lighting.py` publishes190 authored records and the118-node
static light index, requiring matching source hashes/versions. Snapshot provenance
and reference selection/pose fixtures are not runtime inputs. The normal demo
startup now loads both assets into the bridge; race restart resets lighting state.
The bridge is not yet called for gameplay refresh/draw, and the GPU material still
uses the old unlit path. Thus startup integration is not visible shading completion.

`tools/build_rider_lighting_bridge_reference.py` produces192 native bridge draws
using authored assets, four captured view matrices and explicit test phase events.
The phase intervals are synthetic test coverage, not inferred game scheduling.
`web/test-rider-lighting.mjs` compares all7,680 coefficient words and selected
resource IDs exactly against native output, including repeated draws between
refreshes, empty lists, resets and controller extras. This checks native/WASM
integration; the original executable evidence remains the separate query/ranking
and2,000 full-assembly oracle tests. All checks pass, as do full npm/build and the
existing9,630-frame/741-field gameplay comparison. Browser title/menu load succeeds
with no DOM loadError after the new authored package initializes.
Remaining: recover refresh caller cadence and publish per-draw bank to source-normal
GPU material evaluation, including original material blend/texture behavior.

## GPU original normal decode and transform

`riderTransformNormalNode` inweb/rider-lighting-nodes.js now implements the
original ITOF15 decode and three-column normal transformation04B0/04E0/04E8/04F0.
It uses the VU-domain chop arithmetic, with no normalization or synthesized
inverse-transpose. Matrix selection/blending remains caller-owned.
The original VU oracle now emits a second golden file,rider-normal-reference.bin:
12,000 records of20 floats (packed XYZ/pad, three XYZ/pad columns, original
transformed XYZ/pad). Provenance hashes and record sizes are in evaluation-source.

`rider-normal-gpu-test.html` compares float words rather than byte colors. Both
WebGPU and WebGL2 pass all36,000 XYZ words. The initial WebGPU run found two
signed-zero mismatches: shared chopAdd preserved-0 when adding-0 and+0, whereas
the original result is+0. The zero-exponent branch now keeps the negative sign
only when both input signs are negative. No tolerance was introduced. Existing
12,000-case irradiance evaluation/quantization also passes on both backends after
the shared helper correction. Reports:gpu-normal-{webgpu,webgl}.json and updated
gpu-irradiance-{webgpu,webgl}.json. Production build passes.

Current rider assets retain GameCube MNF normal components decoded/16384;
PS2 packed normals for this VU path decode/32768. Zoe's exported lengths are
0.999903..1 rather than normalized again. Sam includes authored/interpolated
normals. Do not equate these formats or add normalization to the source shader.
Cross-console normal correspondence, original matrix palette selection/blending,
per-part material state and live bank invocation are still not complete. This
GPU stage is verified independently; the visible rider material is still unlit.

## Original skin palette blending

Recovered386BD0 (renderer slot37C, called from310640): each12-byte group header
points to4-byte influences. It reads signed16-bit percentage weight and8-bit bone
index, converts with VITOF0 then multiplies by immediate0x3C23D70A (0.01f).
The first matrix initializes the result; subsequent matrices accumulate after
multiplying the prior accumulator by1, preserving VU order. It processes all16
matrix words and does not normalize weights, matrices or normals.
`engine/skin_palette.hpp` implements this operation with explicit source weights.
`tools/test_skin_palette_native.py` executes original386BD0 for20,000 cases:
all output words match for zero/multiple groups,1..4 influences, signed/zero/
percentage weights, arbitrary/identity matrices and default/explicit destinations.
No palette producer or shader matrix-selection behavior is inferred from this test.

310640 first multiplies geometry+34 and+38 matrices into the renderer palette
(3106B8..310778), then the draw path invokes386BD0. Ownership/layout of those
input matrices and per-vertex palette selectors still needs full integration.
The browser currently uses normalized float weights with Three's skinning.

Audited Zoe's eight source MNF parts:200 weight groups (41 single,99 two-bone,
60 three-bone), all totals100, weights15..100. Thus weight-sum normalization is
not a large asset-weight error here; source0.01 conversion and VU rounding remain
different. The importer now retains source_weight rather than losing the integer
percentage. Exported rider.json gains source_skin and source_skin_weight_units.
`tools/export_rider_skin_weights.py` updates the existing Zoe metadata without
rebuilding art, matching each source part SHA and all3,058 installed vertex skin
mappings before writing. Both native/browser packages retain existing normalized
skin data for the current renderer. Sam's procedural/interpolated weights have
not been reclassified as original integer weights. Zoe gameplay test/build pass.
Next connect the verified palette math and its source inputs to GPU skinning;
this metadata/kernel work does not yet change the visible character material.

## Pose matrix construction and pose/bind multiplication

`engine/pose_matrix.hpp` now translates310120. It constructs the quaternion
matrix directly from geometry+2C's posed position/quaternion, writes the unscaled
cache at geometry+30, then scales each complete column by geometry+140 XYZW
into geometry+34. The quaternion is not normalized. Geometry+140 in the three
reference states is(.8499999642,.8499999642,.8499999642,1). The position column
therefore remains untranslated by that scale step; pose translation has its own
prior ownership.310530 adds late contact displacement to the pose and BOTH
unscaled/scaled cached position columns. This supersedes any interpretation that
only the scaled matrix cache receives the late translation.

`originalSkinPoseMatrix` translates3106CC..310778: scaled pose times the supplied
geometry+38 matrix, in original VU column/lane accumulation order.310640 writes
these matrices to4FC420 before the later per-part weight palette work. The new
skin-palette oracle runs the original310640 prefix through310790, not its later
rendering callbacks.20,000 random pose/bind palette loops match every emitted
matrix word, including empty and multiple-bone loops.

The same oracle executes20,000 complete310120 calls with arbitrary quaternion,
position and nonuniform XYZW scales. It also reconstructs87 captured bone matrix
pairs (29 each in glide/jump31/jump90) from actual geometry+2C poses and+140 scale;
all32 words per pair match. The current browser rig has27 bones, while original
geometry has29 slots; mapping unused/extra source slots still needs auditing.
This is not permission to discard source slots merely to fit the browser rig.

The existing20,000386BD0 weighted palette cases still pass. Evidence:
local/rider-lighting/pose-matrix-fixtures.bin and skin-palette-test.log;
tools/test_skin_palette_native.py generates and verifies both. Source geometry+38
bind-data construction/mapping, shader palette selection and final browser pose/
lighting hookup remain unfinished. These helpers do not yet change live rendering.

## Active bone mapping and live Zoe inverse binds

The29/27 slot difference is now explained: original geometry includes inactive
part6 eye_r/eye_l at slots24/25. Zoe's active hair part19 uses26/27/28; its browser
rig uses24/25/26. Body/board slots0..23 align. `export_rider_bind_matrices.py`
checks each active part/file/bone/name and all eight authored bind components
against the original arrays in glide/jump31/jump90. Active mapping and all static
geometry+38 inverse-bind words are identical across the three states.

As an independent semantic check, reconstructed authored rest hierarchy times
these inverse binds is identity to1.28e-6 in linear components and0.000111cm in
translation (double-precision sanity check, not bit-exact inverse reconstruction).
The exporter publishes only static bind words and slot mapping to Zoe metadata;
no animated pose/per-frame skin matrix is shipped. Report:bind-matrix-audit.json.
Setup reruns the exporter before browser asset packaging.

`web/rider-bind.js` converts the source static matrices by exact axis permutation
and translation units matching the already-scaled mesh. Zoe's live Three Skeleton
now receives these inverse binds. Mesh.bind is given its explicit bind matrix so
Three does not silently call calculateInverses and overwrite the source data.
The importer validates the source slot mapping and matrix words. The test checks
rest-pose consistency, hair slots and inverse preservation through binding and
animation. Current Sam uses different Dangle hair bones and lacks this Zoe bind
metadata; it retains its own calculated inverses. Do not blindly share Zoe's bank.

Full npm/build pass, and live Zoe character selection and Snow Jam animation were
inspected with the new inverse binds. This is a live static bind-data improvement,
not full source skinning: Three still performs pose/weight arithmetic, source
palette selectors and GPU weighted matrices remain to connect, and rider shading
is still unlit. The owned browser QA tab was paused after inspection.

## GPU source-weight palette column blending

`riderSkinColumnNode` now translates386BD0's weighted matrix column calculation
into GPU nodes. It accepts four selected bone columns, raw integer percentage
weights and the actual influence count. It initializes with the first weighted
column, then conditionally accumulates subsequent columns with the original
0.01f conversion and VU multiply/add order. It does not normalize weights or
blend unused influences. All four lanes, including translation/W, are preserved.

The existing original palette oracle now emits12,000 column records directly
from386BD0 output, covering all four columns of groups with1..4 influences,
identity/arbitrary matrices and signed/zero/non-unity-sum weight cases. Each
record is28 floats: count vec4, weights vec4, four input columns and original
output vec4. `rider-skin-gpu-test.html` compares the float words exactly. WebGPU
and WebGL2 both pass all48,000 words. Reports:gpu-skin-webgpu.json,
gpu-skin-webgl.json, skin-gpu-reference.json inlocal/rider-lighting. Golden data
is web/public/test-data/rider-skin-reference.bin and is QA-only. Existing20k
source pose/bind/weighted-palette checks and87 captured matrix checks still pass.
Production build passes; no shared arithmetic helper change was required.

This completes the GPU arithmetic for supplied weighted matrix columns, not the
live skinning integration. The source pose and static bind matrices must still
be supplied in the correct space, then selected per vertex. The browser continues
using Three's skinning (with verified original Zoe inverse binds), and rider
lighting is still not connected to the visible material. No frame-time or power
claim is made from offscreen conformance tests.

## Live source-space pose/bind matrix publication

The animation bridge now loads original static bind words when the rig supplies
them and exposes `rider_skin_matrix_count()` / `rider_skin_matrices()`. For Zoe,
the latter returns27*16 floats: original310120 scaled matrices constructed from
`cachedCrashWorld`, multiplied by each mapped original inverse bind using310640
arithmetic. The cache is the final world pose used by contacts/effects, including
late landing/body-contact translation. No extra quaternion normalization or
conversion to browser units is inserted into this API. Data remains source
centimetres/Z-up; consumers must not apply the already-scaled browser vertex
positions or object transform again without the appropriate conversion.

Results are cached per animation tick for repeated renders. Initialization and
animation reset clear both pose availability and stale matrix cache. Missing bind
metadata (currently Sam's different rig) returns count0/null rather than borrowing
Zoe's matrices. `check-camera.mjs` now checks availability, finite values, repeat
stability and reset invalidation through charged/held jumps for Sam/Zoe.

Native/browser replay now supports a separate `--zoe` comparison and optional
RIDER_ZOE argument to the native trace binary. Both9,630-frame/23-scenario runs
pass with zero mismatches: Sam742 fields; Zoe1,188 fields, including all432 source
skin-matrix floats. Separate native/wasm and parity artifacts have a-zoe suffix.
These are host-pipeline arithmetic checks, not proof that all world poses match
PS2 captures. Full npm/build pass. The visible mesh still uses the existing Three
skinning path; these matrices are ready for the GPU producer/vertex hookup but
are not yet driving visible source-weight skinning or rider lighting.

## GPU source vertex position stage

`engine/skin_vertex.hpp` and `riderTransformPositionNode` translate the supplied
four-column matrix times source float XYZW vertex at program2 upper04C0..04D8.
The original lower LQ at04C0 replaces vf14 with the translation column after the
first-column multiply; the native oracle preserves this real instruction/data
dependency. The stage precedes projection and performs no unit conversion.

`tools/test_skin_vertex_native.py` runs12,000 original VU cases (identity/affine
and general matrices/vertices), compares all four output words, and emits24-float
GPU records: point4, supplied matrix16, original result4. The new
rider-position-gpu-test.html passes all48,000 float words on WebGPU and WebGL2.
Reports are gpu-position-{webgpu,webgl}.json; program/golden hashes and exact scope
are in skin-vertex-source.json. Production build passes. This is matrix/vertex
arithmetic conformance; palette selection, projection, live mesh integration and
material shading are still separate unfinished work. No rendered-image parity
or runtime performance improvement is claimed from these offscreen comparisons.

## Live Zoe source GPU skinning

`web/rider-skinning.js` now connects Zoe's original source-space pose/bind matrices
to the live mesh. It publishes unscaled source-centimetre vertex attributes,
original integer weights and active-rig bone IDs. A4-by54 RGBA32F texture holds
previous/current27-bone matrices; the simulation captures them after each final
pose. Repeated render frames only change interpolation alpha. Reset/placement
snaps both matrix sets to the new pose to avoid blending stale world positions.
The texture is disposed when the rider asset is replaced.

Zoe's MeshBasicNodeMaterial now runs verified386BD0 weighted columns and the
verified VU float vertex transform in gameplay. It converts source XYZ/W to the
renderer-relative coordinate system and applies camera view/projection directly,
bypassing the mesh/model transform so scale/translation are not doubled. World W
is retained when subtracting the origin. Menu display selects the existing Three
pose/MVP; Sam remains on its existing rig path. Source normal/irradiance/material
lighting is still unconnected, so this is live position skinning, not completed
rider shading. Source positions still originate in the GC-derived mesh package;
projection and presentation interpolation are not PS2-bit-exact claims.

Important integration finding: wrapping the custom vertex calculation in a TSL
uniform If/Else produced severely stretched geometry on both backends, despite
separate arithmetic conformance tests passing. That attempted optimization was
removed. The final material uses select between the original menu MVP and source
clip result. Longer restarted runs were inspected on both WebGPU and WebGL2 with
intact rider/board geometry, including airborne/ground poses. Do not reintroduce
the branch without a full material regression test. The precise compiler/graph
cause is unresolved; no performance/power improvement is claimed.

Main now accepts backend=webgl for explicit fallback QA. DOMdata-source-skinning
indicates which live position path is active; it is diagnostic, not product UI.
New test-rider-skinning.mjs checks authored attribute/axis/weight mapping, input
immutability, missing-pose handling and restart lifecycle; included in npm test.
Full suite/build pass, with focused lifecycle and live GPU rechecks after branch
rollback. The two owned race QA tabs were paused. Potential optimization: match
original CPU per-weight-group palette reuse instead of recomputing shared blends
per vertex; benchmark before claiming smooth/power-efficient rendering.

## Reuse original weighted palettes per animation tick

The live path now matches386BD0's work ownership more closely: instead of
repeating matrix weighting for each rendered vertex, the animation bridge groups
identical ordered source influences once at rig initialization. Zoe's3,058
vertices map to161 distinct groups. `rider_skin_palette_count`,
`rider_skin_palette_indices` and `rider_skin_palette` expose the mapping and
original weighted matrices. The latter runs originalSkinPalette once per animation
tick and caches the result; init/reset invalidate stale palettes. Equal ordered
influences produce identical arithmetic, so coalescing across source parts does
not reorder any individual matrix sum.

The vertex shader now fetches four columns from that palette for each of the two
presentation frames and runs the already verified vertex transform. It no longer
calls weighted-column blending per vertex. The texture is4-by322 RGBA32F (161
previous/current matrices); this increases upload size from3,456 to20,608 bytes
per captured tick, while reducing the nominal matrix fetches from32 to8 per
rendered vertex and removing repeated weighting arithmetic. CPU palette work is
added, so these structural counts are not a measured frame-time/power claim.
Unused source weight/index/count GPU attributes were removed; each vertex carries
one palette-group index. Capture validates all vertex indices against the core.

Tests cover161-group mapping, source coordinates, invalidation and repeat draw
stability. Native/browser trace compares every palette float, not just a hash:
Sam743 fields and Zoe3,765 fields (including2,576 weighted-palette floats),
9,630frames/23 scenarios each,zero mismatches. Full npm/build pass. Longer live
runs were inspected on WebGPU and WebGL2 with source skinning active and intact
rider geometry; owned QA tabs paused afterward. The select-based material path
remains in place; the prior If/Else graph regression was not reintroduced.
Lighting/material, source normal-palette selection and measured performance
validation remain unfinished.

## Normal palette ownership confirmed

`tools/test_skin_normal_palette_native.py` now executes full original program2
entry0080, rather than only isolated vertex arithmetic. With1..25 supplied
weighted matrices and independently varied projection/extra transform matrices,
VU memory117+ retains each weighted matrix's first three complete columns
unchanged.2,000 executions match all312,000 checked words. The projection path
writes a separate matrix bank; it does not define the lighting normal transform.
The original vertex loop loads the normal matrix through its own palette pointer
before04E0..04F0. Reports/logs:skin-normal-palette-source.json and
normal-palette-test.log. This verifies palette preparation, not all per-vertex
PS2/GC selector correspondence or all program/template choices.

The live skinning interface now exposes `lightingNormal`, using those first three
weighted columns from the existing previous/current palette texture. It transforms
source normal components with the verified VU shader helper and interpolates only
for display, with no renormalization or camera/object transform. Source normal
attributes preserve the GC-derived mesh's decoded binary fixed-point components
and exact axis permutation. Multiplication by32768 supplies equivalent ITOF15
units to the helper; GC's/16384 encoding is not being relabeled as original PS2
signed16 bytes. Position and normal share the imported MNF weight-group selector.
The attribute mapping/lifecycle test and build pass. `lightingNormal` is ready for
the coefficient evaluator but is not yet consumed by the visible material;
shading coefficients, material byte-domain sampling/combination and refresh timing
remain unfinished. Existing position rendering is unchanged by this addition.

## Live Zoe lighting material enabled

`web/rider-material.js` now connects the recovered normal, coefficient evaluator
and HIGHLIGHT2 combiner to Zoe's actual material. The core assembles environment,
view-dependent rim and selected local lights, then exposes original255-scaled
coefficients through rider_lighting_gpu_coefficients. The shader evaluates and
quantizes lighting at vertices before interpolation. A separate NoColorSpace
texture view supplies encoded samples for HIGHLIGHT2; the original sRGB map stays
available for menu display. Final encoded RGB is converted to linear for Three's
output/fog pipeline, preventing double gamma. Raw texture clones are disposed
with their rider materials. Menu and gameplay output use select, consistent with
the earlier vertex-branch regression workaround.

prepare-environment now exports the five rim constants directly from the pinned
ELF and the uniform ARA1 rim scalar.2EEFA8's getter resolves2C15D8, which returns
painter+50 (not+4C); this is the second authored scalar,0.75. The core's GPU bank
scaling is included in the192-frame native/WASM bridge comparison. Its reset also
clears the scaled bank. The live updater supplies source primary-bone XYZ/W=1 and
the final recovered camera matrix after each pose/camera tick.

Important fidelity limits remain explicit: the synchronous web integration
refreshes source spatial query/selection once per simulation tick. Captured source
job scheduling sometimes retains lists across several ticks; that asynchronous
cadence is not yet reproduced. This is an unfinished temporal difference, not a
new claim that the original refreshes every tick. Live controller ambient/directional
effects are currently zero; their bridge/math exists but owners remain unwired.
Normal-rider environment selector0 remains the existing scoped path. Exact equipped
texture variants, GS bilinear sampling, per-part alpha/blend states and Sam's
different rig/material remain unfinished. Zoe is now lit; Sam still uses the old
material. The overall goal is not complete.

`rider-material-gpu-test.html` tests the real MeshBasicNodeMaterial with nearest
RGBA8 texture, vertex lighting, encoded combiner and sRGB output. Both WebGPU and
WebGL2 pass all bytes in menu/lighting-enabled cases (64x64 pixels each). The first
16-wide QA target exposed WebGPU readback row padding; using64 pixels aligns each
RGBA row to256 bytes. This was a test readback-layout issue, not a material fix.
Saved reports:gpu-material-{webgpu,webgl}.json. This test does not assert GS
bilinear or blend equivalence. Live Zoe races were inspected on both backends and
paused afterward. check-camera --zoe now runs actual environment/light assets and
updater across charged/held jumps, verifying finite banks and exactly one refresh/
assembly per web simulation tick. Full npm/build pass.

## Sam custom-content compilation and live lighting

Sam now uses the source-style skinning and lighting path as well. His26-bone
custom rest hierarchy is not Zoe's27-bone rig: Dangle hair bones are different,
and1,066 of5,814 vertices had fractional percentage weights after mesh editing.
`tools/compile_sam_skin.py` compiles inverse binds from Sam's own normalized rest
hierarchy and converts weights by largest remainder to integer percentages summing
to100, preserving influence order. Metadata explicitly labels this authored
compilation, not original captured PS2 data. Existing authored vertices and legacy
skin weights are retained. Sam's source-normal attributes are quantized to signed
ITOF15 components at load for the game-style normal path; menu normals stay as-is.

Sam has329 distinct weight groups. Maximum individual weight change is0.007016
(0.7016 percentage points). Compiled inverse/rest identity error is3.43e-8 linear
and1.79e-6cm translation. `audit_sam_weight_error.py` isolates quantization on81
sampled replay poses: maximum vertex displacement0.17051cm (1.71mm), not a guarantee
for every possible pose. Reports:sam-skin-compilation.json and
sam-weight-displacement.json. Setup runs the compiler before browser packaging.

Sam's425 tinted vertices are preserved: authored tint is applied in linear space
before returning texture RGB to the encoded HIGHLIGHT2 calculation. Stock Zoe
skips that extra conversion. Material QA now supports tint=1, compares unchanged
menu pixels against the actual pre-hook material, and lit pixels against the
software byte-combination expectation. Ideal double-precision sRGB math differed
from the existing hardware menu by occasional bytes; the pre-hook pixel baseline
correctly tests menu preservation. No byte tolerance was added. Tinted material
passes on WebGPU/WebGL2, with reports gpu-material-sam-{webgpu,webgl}.json.

Expanded Sam native/browser trace now checks6,423 fields including416 source
pose/bind floats and5,264 weighted-palette floats:9,630 frames,zero mismatches.
Full npm/build pass; actual Sam races and material/tint were inspected on both
backends and paused afterward. check-camera now exercises live lighting for both
riders. This supersedes earlier statements that Sam remains unlit or must fall
back to Three-only skinning. Original light-refresh cadence, controller lighting,
GS filtering/blending, broader rendering/fog issues and performance validation
remain unfinished; authored Sam compilation is not an original-character parity
claim and does not complete the overall goal.
