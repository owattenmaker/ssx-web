# Original airborne touchdown

The production path uses native C++ math, original authored collision data and
native animation poses. Original PS2 functions are executed only by private
conformance programs under `tools/` and `tests/`.

## Motion and pose order

`139A20` first updates the trajectory. At `139A70..139A8C` it copies the retained
presentation up vector (`rider+180`) into the contact normal (`+370`) and clears
the surface velocity (`+3D0`). This is the previous presentation, before this
motion phase's physical alignment. The native rider retains the generated
presentation up vector explicitly; substituting physical up only happens to
work for neutral, unrotated air poses.

The normal air presentation update advances `134DD0` exactly once per logic
frame. Native pose queries use its pure Current variant and return the dynamic
local pivot, posed board root and presentation up. `139C88` then runs its contact
probe. The probe is centered on the posed board root selected by `rider+8A0`
(Zoe bone22), with endpoints center minus/plus 200cm times presentation up.
It shares `rider+864`'s terrain-cell cache with cruise contact, uses refined
terrain kind2, and ranks against preferred fraction `0.574999988079071`.
The fraction remains the original coarse triangle fraction even when the point,
normal and UV have been refined.

A touchdown requires fraction at least0.5 and negative
`dot(velocity-surfaceVelocity, hitNormal)`. It does not use a segment between
successive actor positions or a guessed clearance.

## Contact transition

Control5 first exits through `134CB0`. This explicitly invokes the mutable
presentation function again as an exit event, bakes its position/quaternion,
normalizes through `11E098`, resets prewind triplets and quantizes the retained
manual spin. Channel1's fade is an animation event. This extra exit invocation
is distinct from the once-per-frame presentation advancement.

`139D78..13A148` clears the predicted landing time, checks authored recovery
flags, resolves penetration using the material's unscaled depth3, applies the
normal impulse and material normal-speed limit, updates the contact frame,
surface, point, patch/UV and signed distance. Classification at
`13A14C..13A4CC` uses original animation class/flags, physical orientation,
manual state and roster landing stat. The RNG-dependent upright-crash branch
is explicit; no random word is fabricated in gameplay.

For a clean ordinary landing, `13C7A8` enters ground motion, resetting the board
oscillator, scaling authored depths by body scale, and setting board normal.
Its velocity multiplier depends on original clock ticks since leaving ground:
0.7 through40 ticks; then0.7+(ticks−40)*the original float0x3C23D70B, capped at1.
The new ground-focus tick is retained. Normal control entry clears the jump
latch. Landing clips3D/3E/3F/42/43 follow the source impact/manual-spin branches.
Charge filters keep their original state and are not zeroed by a generic landing.

`13AA48` still runs as the second phase on the touchdown tick. It reads the
cached prelanding AA0 body spheres and presentation up. `11E150` only rebuilds
actor bounds, not those posed bones. Body pushes translate actor position and
cached sphere centers, then apply the separately recovered air bounce. This
query uses the distinct `rider+868` body cache and authored surface filter2.

## Evidence

- `tools/test_landing_motion_native.py`:20,000 contact-response stages,
  20,000 crash classifications and20,000 focus/leave/clip-selection cases match
  opcode-corrected original functions exactly.
- `tools/test_air_control_native.py`:20,000 complete134CB0 exit cases match
  original physical poses, axes, angular fields and control triplets exactly.
  The existing80,000 angular/entry/presentation cases remain exact.
- `tools/test_landing_contact_reference.py`:49 complete13A7B0 queries over
  seven captured states, including35 contacts and14 misses; hit fields and
  retained cache contents match exactly.
- Air13AA48 response:10,000 original cases,9,900 pushes and4,988 bounces match
  exact translation, velocity, incoming direction and return status.

The raw recorded-command long-charge replay is the integration test. Its command
words are interpreted using the native current control state, without inferring
landing time from ambiguous zero words. Until this integrated run is compared,
component conformance is not a claim of complete gameplay equivalence. The first
integrated long-charge run now lands at native frame152/source tick489, exactly
the captured original ground-focus tick, with leave tick428 and control0 return.
Its endpoint position/velocity still differ; postlanding pose, body response and
controller continuation are tested separately rather than fitting touchdown time.

## Explicit remaining branches

The native touchdown hook currently fails explicitly on original crash/recovery
continuations and manual/style continuations whose state/animation side effects
are not yet implemented. A missing pose or incomplete
world query is reported as pending. Original10E910 scoring/boost awards are a
separate pending landing event; motion does not invent an award or mutate the
meter using an approximation. Weighted landing-animation variants and their RNG
selection belong to the original animation driver.

## Reverse stance and ground visual controls

`114CC0` uses velocity projected onto contact forward(`3A0`), requires brake0
and forward speed at most−111.1111145cm/s. It pre-multiplies physical orientation
by the exact quaternion `{boardUp,0}` and normalizes; replacing this with a
trigonometric approximation to180degrees changes the result. `115168` toggles
stance, negates contact forward/lateral and current/target of turn, brake,
extraLean, animationTurn and balance280. Animation receives a separate reverse
event for its per-sequence root transforms and base root/mirror state.

The20,000-case full114CC0+115168 oracle passes its decisions, physical axes,
quaternion, root quaternion and control flips. The native caller invokes this
both at touchdown and during normal-controller1318EC, after target requests and
before the landing-class hold branch. Reverse touchdown selects40/41. The
source event timeline is used; endpoint stance is not forced to match a capture.

Ground tail13EEA0..13F064 restores boardAlignment targets after leaving the
landing animation class. Its ordinary rate isdt*3; class10/class5 and semantic2/22
disable alignment. Extra lean uses the authored gp−1FC8 curve only on surfaces2/3
outside control1, and otherwise targets0. This complete stage passes20,000
original-code cases including interpolation and class/control branches.
