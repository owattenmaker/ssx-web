# Original controller5 animation selection

`engine/air_animation_selector.hpp/.cpp` implements the full13437C..134C6C
selector after angular/scoring updates. It runs on the controller tick, before
timers, control filters and pose sampling; it replaces pose-time guesses.

The optional `OriginalAirControlFrame*` output from`originalAirControlStep`
provides phaseBefore (the source s3 captured at13364C) and effective spin/flip
(the final parent-stack controls after mode substitution, direction snapping
and phase handling). These cannot be reconstructed from raw input or from the
post-update angular state alone.

## State and selection

Every invocation first writes adjustment298 and28C targets from angular
adjustSpin/adjustFlip divided by float0x3FDF66F4, with rate float1/6. Their current
filtered values remain unchanged until the shared1211F8 pass.

The selector then follows the source priority:

1. Active grabs and semantic288 retain their current animation.
2. Nonzero angular adjustment chooses297..304 from the filtered adjustment
   direction and reverse stance, unless a class9 entry clip is still current.
3. A phase3→0/1 start chooses269..276 from effective input direction and derives
   a stat/angular-rate playback rate.
4. Phase0 with both targets0 selects287.
5. Phase1 retains its current clip unless the original extension, class2,
   unfinished-rotation and sequence-completion gates allow293..296. Failed
   phase1 gates return directly; they do not run the landing-duration query.
6. A new phase2 maps the current directional semantic through4581A0 to306..313
   or287, and derives playback rate from remaining rotation and clip duration.
7. Other paths evaluate the trajectory-based287↔305 landing transition.

`mainCompleted` means sequence+C0 returned by312AE8; it is not an AFL marker
flag. Current semantic and class are read after the grab lifecycle has applied
its requests.

## Animation rate and duration contracts

Animator+1C is a pending rate for the next animation request. It is not the
current sequence's rate. The selector can calculate a new pending rate even
when the resulting semantic is unchanged. In that case the pending value
survives. Only a new play request consumes the value and resets animator+1C to1.
`OriginalAirAnimationState.nextRate` and `reference_air_animation.py` preserve
this initial/runtime state. The optional setNextRate callback represents those
scalar stores; play receives the exact consumed rate.

The duration callback is original312790: resolve the semantic through104CF8
and311710 first, then return `(AFL frameCount-1)*float1/30`. Basic ID519 returns
positive0. The lookup can consume shared RNG for weighted variants, so it must
not be replaced with an unconditional initial-clip lookup. For rider animators,
104CF8 reads the mask from actor+364; its alternate object path uses+CD8.

All currently observed semantic mappings268..313 have one variant. The selector
itself makes no random draws, but it preserves the exact callback order for
future weighted data. In particular, valid-trajectory fallback paths can query
305's duration even when the current semantic will remain unchanged.

## Verification

`tools/test_air_animation_selector_native.py` compares60,000 complete original
selector cases: semantic, play rate, both pose-control triplets, final pending
rate and exact duration-query order. It exercises30,151 play requests and12,417
duration queries, including every directional family and phase2 mapping.
Duration/animation/stat getters are explicit typed subsystem boundaries.

`tools/test_air_control_native.py` also verifies the optional phase/effective
control output against original registers/stack while retaining its100,000
angular, entry, exit, reversal and presentation regression cases.
