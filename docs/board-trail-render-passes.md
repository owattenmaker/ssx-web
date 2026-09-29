# Original board-track GS passes and Metal contract

The original uses **MODULATE with RGBA and standard source-alpha blending**.
The blue rails in the current preview do not justify switching to DECAL or
HIGHLIGHT. The preview omitted the original destination-alpha mask/depth passes,
which let the sunken center of the track replace the terrain depth. It also kept
an environment color sampled under the start canopy after moving outside.

## Verified material

The bound original `btrl` texture descriptor is `0x587380` in the glide fixture;
its TEX0 template at+38 is `0x000000059C008000`. Running the actual binder
`0x368970` produces `0x000000059C00BAE9`: TFX=0 (MODULATE), TCC=1 (RGBA),
PSMCT32, width128, height64. The extra low bits are its VRAM address.

The track material sets blend enum5 at `0x386FC8`. The deferred state builder
`0x363F30..0x363F68` extracts this enum from material word4 bits2..6 and calls
`0x362478`. Table entry5 at `0x491FB0` selects `0x3624DC`, which emits
ALPHA_1=`0x44`: `(Cs-Cd)*As/128 + Cd`. The original binder and blend setter are
executed and asserted by `tools/probe_board_trail_passes.py`.

For a normalized raw RGBA8 texture sample `t`, source vertex integer color `v`,
the continuous normalized equivalent before GS quantization is:

- `Cs.rgb = t.rgb * v.rgb /128`.
- `As = (t.a *255/128) * (v.a/128)`.
- `result.rgb = Cs.rgb*As + destination.rgb*(1-As)`.

The texture's raw alpha remains0..128. The already-verified VU1 prefix applies
segment fade to vertex alpha with integer truncation first. The common VU1
continuation at0x12B8 loads/stores RGBA unchanged while transforming positions,
perspective-correcting UVQ, and clipping. No additional normal lighting or hidden
RGB whitening occurs there. The exact GS integer saturation/rounding and sampling
remain relevant for final pixel comparisons.

## Pass sequence

`0x386E78` submits these passes for the entire supplied list of trails, not a
complete sequence separately for each rider. Each descriptor carries its original
ring draw window. All GS alpha tests are disabled (ATE=0); transparent texels are
not discarded and can still affect depth and the destination-alpha mask.

The GS uses larger Z for nearer geometry. Metal equivalents below assume the
native renderer uses smaller depth for nearer geometry and clear depth1.

| Pass | Source call | Bands | GS state changes | Metal equivalent |
|---|---|---|---|---|
| Clear track mask |0x3872F8|0,1, untextured|FRAME RGB write mask0x00FFFFFF; ZBUF.ZMSK=1; TEST0x30000(always); initial alpha0|No RGB writes; depth always, no depth writes; stencil replace0 wherever top strip covers|
| Mark visible track top |0x387490|0,1, untextured|TEST0x50000(GEQUAL); FBA1; still alpha-only/no depth writes|No RGB writes; depth lessEqual, no depth writes; stencil replace1 on depth pass, keep on depth fail|
| Clear terrain depth under visible top |0x387750|0,1, untextured|ZBUF.ZMSK=0; TEST0x3C000(always, DATE1/DATM1); FBA0; viewport Z scale and bias both0|Stencil equal1; depth always; write far depth1; replace stencil0; no RGB writes|
| Render trough and raised edges |0x387A00..0x387A80|2,0;0,3;3,4;4,1;1,5; textured|Restore viewport; FRAME unmasked; TEST0x50000; FBA1; depth writes enabled|Normal source alpha blend; depth lessEqual and depth writes; stencil replace1 on passed fragments|
| Fix gaps |0x387C30|0,1, textured|TEST0x54000(GEQUAL, DATE1/DATM0); FBA0|Stencil equal0; normal source alpha blend; depth lessEqual and depth writes; stencil remains0|

The first three passes call `0x386D10` with both fade parameters0, ensuring their
vertex alpha is0. FBA overrides its high bit during the visible-top pass. The
five shaded passes and fixup call `0x386DD0`, which uses the per-trail fade
parameters. Fixup is conditional on GP+14DC (`draw_fixup_layer`, default1).

A separate stencil attachment is the natural Metal replacement for GS framebuffer
alpha-bit tests. Stencil0 corresponds to framebuffer alpha's high bit clear;
stencil1 to it set. The preparatory passes preserve scene RGB. Drawing only the
five shaded strips against unchanged terrain depth hides the sunken center and
leaves just the raised lips visible as two ribbons.

## Geometry and color inputs

The live glide fixture confirms board23 geometry+30 has unit matrix axes;
geometry+34 contains the same axes multiplied by model scale. Positions are
identical. Snow particles use+30; board tracks use+34. Derive the track frame from
the live native board quaternion/position, multiply axis0 by modelScale.x and
axis2 by modelScale.z, and keep its world position unchanged.

At glide, the newest original track slices actually contain RGBA `(31,39,70,127)`,
matching environment ARGB `(.5,.125412,.156419,.277877)` multiplied by255 and
truncated. Earlier retained slices have warmer colors. This demonstrates that
color changes over time in the original. An initial environment snapshot is not
a replacement for the live environment updates. Blue under shadow is source
behavior; a frozen blue tint outdoors is not established as correct.

The exposed debug `Top Bias` and `Normal Offset` globals are not read by the
recovered PS2 track generator/renderer path. Do not invent an extra normal offset
or z-bias from their names. The active depth adjustment is the explicit viewport
Z clear above. Final pixel parity still requires comparing joined native renders
against the original after these state transitions and live lighting are wired.

## Reproducing the evidence

Run `python3 tools/probe_board_trail_passes.py`. It executes the original renderer
command construction with only geometry submission intercepted, then executes the
original texture binder and blend setter. Original copied code and the resulting
packet trace remain ignored under `local/reference/terrain/board-trail/`.
The fixture, ISO, original executable and native renderer are not modified.

## Stencil now exists in the default browser pipeline (September 22)

The five-pass sequence above was silently degraded in the normal browser path.
`fog-renderer.js` renders the world through a Three `pass()` whose render target
had a depth-only attachment, so every stencil state in `board-trail.js` was
ignored. Consequences: pass 2 cleared depth under the whole top strip regardless
of occlusion, and the fix-up pass (bands 0,1 textured, meant only for stencil 0
gaps) drew the flat lip-to-lip roof over the whole trough. The result was a flat
raised band with sloped sides (the "mound"), not the PS2 groove between two lips.
Only the `originalFog=0` diagnostic path (canvas with `stencil:true`) was right.

`web/depth-stencil-target.js` now gives the world pass a DEPTH24_STENCIL8 depth
texture. Three r186 binds sampled textures with aspect `all`, which WebGPU
rejects for combined formats, so sampled views (binding path; attachment views
leave `dimension` unset) of `*-stencil8` textures are narrowed to `depth-only`.
Fog view-Z and the encoded snow depth copy keep reading depth unchanged.

Evidence (glide QA frame, WebGPU): with the trail drawn, pixels along the trough
are identical with and without the fix-up mesh, i.e. it now only fills gaps;
before the change, hiding the fix-up visibly exposed the trough walls near the board. WebGL2 renders identically.
`snow-composite-gpu-test.html` (fog lifecycle) and `fog-gpu-test.html` still pass.
No trail geometry, colour, texture or blend constants changed. The trail's
per-vertex colour is the live environment ambient (e.g. browser (54,57,75) vs a PS2
glide slice (31,39,70)); that difference comes from rider position/lighting, not
from this renderer.
