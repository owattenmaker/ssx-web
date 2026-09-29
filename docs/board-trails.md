# Original board tracks

`engine/board_trail.hpp` translates the original PS2 board-track generator into
ordinary native C++. This is the persistent textured track on the snow, separate
from `SnowTrail` particles and spray. It consumes original source coordinates
(Z-up centimeters). Convert to Metal world coordinates only when uploading.

## Recovered behavior

The named `cBoardTrailFX_initialize` routine is `0x2E81E8`; reset is `0x2E8560`,
eligibility `0x2E87E8`, sample admission `0x2E86F0`, and update `0x2E8938`.
Each rider owns six vertex bands, each with 54 slices. Width, raised lips and
contact penetration depend on the posed board's axis0/axis2, contact plane,
velocity, board angle, material depth profile, and two retained jitter values.
Manual stance and sliding crashes use their original corner selection paths.
The helper retains the unusual preceding-slice rewrite, temporary end cap,
backward-edge collapse, and three-slice stop/restart bookkeeping.

Material IDs 0,5,6,12 use packed depth; 1 loose; 2,13 powder; 3 deep powder.
Other IDs do not generate tracks. Airborne motion normally stops them; the
original AC4/AD0/AFC/B00 path and grounded crash submode have their own gates.
Semantic22 suppresses tracks between its two original sequence flag markers.

The sampling gate commits after 110cm of contact displacement, or when the
projected travel direction's cosine falls below the original .999 constant.
The special state reduces the distance to30cm. Restart inserts a two-update
cooldown. This effect has a bounded ring and segment fade, not a lifetime in
seconds. Renderer `0x2EA538` sends at most48 slices, excluding the pending end.

## Native integration contract

Initialize `board_trail::State(profile)` once per rider. Populate `Input` from the
current native-generated geometry matrices and live rider/contact state, then
call `update(state,input,sharedVisualRng,profile)` in the original FX update phase.
`drawWindow(state,profile)` supplies ring start/count and fade parameters.
`Vertex` is48 bytes: float4 UVQ, uint4 RGBA, float4 source XYZW. RGBA is already
converted from the current environment's ARGB floats using source multiply and
truncation. Do not treat the color words as normals.

The world frame loop at `0x128F80` updates **all riders' board tracks** before the
snow-spray loop at `0x1290A0`; several other FX loops occur between them. Board
tracks and spray share the visual LCG word at `GP+0xA0C` (`0x4A3AFC`). Each track
commit consumes exactly two steps; this is separate from the gameplay RNG.
Other original FX may also consume this stream. Per-rider independent RNGs or
alternating track/spray updates change the original jitter sequence.

`tools/reference_board_trail.py` exports initial environment color, source bone
indices, special flags and shared visual RNG from a snapshot. Its optional
`--audit-pose` output is for comparisons only. Runtime uses native poses and live
physics, never captured trail vertices or replayed per-frame board matrices.
The environment accessor `0x140B80` returns rider+86C; the ARGB float4 is at
`0x4FA398 + environmentIndex*0xF0`. The environment table itself can change with
world lighting, so preserving the initial color does not prove lighting parity.

## Texture and renderer

`tools/import_board_trail.py` extracts `btrl` from the verified owned USA PS2
`DATA/TEXTURES/EFFECTS.SSH`, preserving all128×64 raw RGBA texels. The manifest
retains ELF/container/texture hashes and29 parameter addresses/bit patterns.
No source assets are committed to the repository. The default output package is
`local/assets/native/BOARD_TRAIL`.

Texture binding is independently proven: `0x2EFFAC` loads ID55, `0x2F0008` stores
it to GP+14E4, `0x386FD0` selects that texture, and table `0x4891B0 +55*12` names
`btrl`. Raw PS2 alpha ranges0..128; PNG preview expansion is not a change to the
source texture. The native shader must respect the GS alpha convention.

The five textured strip pairs are `(2,0),(0,3),(3,4),(4,1),(1,5)`, explicitly
submitted at `0x387A00..0x387A80`. Band U coordinates are0,.25,.375,.625,.75,1 in
that spatial order; V alternates0/1 per ring slice. Renderer uses extra depth
and fixup passes (`draw_fixup_layer`, enabled by default). These passes and the
VU1 program at byte entry0x3A00/0x3A08 need independent Metal parity validation;
matching geometry alone does not establish matching final pixels.

The original VU1 program3 (ELF DMA source0x435BD0) at0x3A00..0x3BE0 interleaves
the two bands and fades only their integer alpha. It clamps the current fade to
0..1, converts alpha with ITOF0, multiplies and converts back with FTOI0; RGB,
UVQ and position stay unchanged. `fadedAlpha(alpha,fade)` preserves this
quantization. The oldest slice uses fade0, then the fade increases by1/16 for
each slice. This pass is tested independently against10,000 executions of the
original VU1 program; the test halts before its shared clip/GS processing.
The oracle corrects a vendor MAX/MINI lowering bug using the documented PCSX2
integer-bit min/max semantics, needed to preserve packed RGB words.

## Validation

`python3 tools/test_board_trail_reference.py` executes the complete original
`0x2E8938` against the native helper for30,000 deterministic randomized cases.
It compares all six54-vertex positions/colors, ring state, depth/height,
cooldown, prior vectors/contact and both jitter values/shared RNG exactly.
Cases include stationary zero-direction frames, arbitrary posed board orientation/tilt, all materials,
stop/restart, manual/reverse stance, special motion gates, and sliding/detached
crash corners. The same tool compares all2,970 valid head/count draw windows
against original `0x2EA538`. Original AOT copies are test-only, use corrected
scalar opcode rounding, and are not linked into native gameplay.

The helper also preserves the documented PCSX2 VU reciprocal-sqrt saturation
for a zero direction, avoiding host infinity-times-zero NaNs at rest. The
final GS/VU render passes are not yet claimed bit exact. Full-frame visual
comparisons remain necessary after wiring.

## Peak 2 visibility check (2026-09-25)

"Some Peak 2 trails don't work, the start of Ruthless Ridge": the track gate `eligible` (materials 0-3, 5, 6, 12, 13) reads
the physics surface, which is exact on peak2/cra3-race-ai; the only trail-less stretch at the start is surface 4, the
start ramp (ticks 187..388), and the PS2 draws no track there either (frames of that run, start.tick168..469). Rendering:
browser frames at the PS2 ticks (headless Chrome, rider seeded from the capture record 120 ticks earlier) against
runs/peak2/cra3-full.tick1618..11618 (e.g. 10819, 11618), runs/peak2/dra4-full (2018, 3219), the Peak 2 free ride
runs/peak2/fr-dbc2 and the Peak 1 control setpieces/race 4422: wherever the PS2 shows grooves the browser draws them on
the same Peak 2 terrain materials; no rendering difference was found. The white / purple streaks under airborne riders in
those frames are the air streamers (RFX+0xAD0), now ported ([pickup-recovery.md](pickup-recovery.md)).
