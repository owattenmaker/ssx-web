# Original airborne controls and presentation

`air_control.cpp` contains native math/state implementations recovered from the
PS2 USA executable. It does not execute console instructions in the product.

## Recovered stages

| Original code | Native entry | Scope |
|---|---|---|
| 0x1158B8 | originalAirDirection | Deadzone and angular snapping of the spin/flip input pair |
| 0x133128 | originalAirControlBegin | Air-control entry using filtered prewind values at rider+0x2A4/+0x2B0 |
| 0x1333E0..0x134334 | originalAirControlStep | Command-prefix modes and ungrabbed angular phase/target/rate/offset updates, with contact interruption disabled |
| 0x135180,0x134DD0 | originalAirPresentation | Physical-to-presentation pose and mutable flip-axis blend |

The control object is motion owner+0x230, with its actor pointer at +0x58.
The raw mapping of its angular fields is in `air_control.hpp` and
`tools/reference_air_control.py`. Spin/flip targets, progress, total angles,
air-adjust offsets, hold timers and completion phases are distinct state.
The initial target is 180° for spin and 360° for flip. Holding input extends
targets; releasing input can settle toward a completion target. Input reversal,
stat factors, prewind continuation, and the modifier at rider+0x2EC have their
original operation/branch order.

The recovered air stat is original getter0x1495A8: progression byte4 divided by
character maximum byte12, using the same slot/bank/override selection as the
other rider stats. This scalar division rounds to nearest. The Zoe fixtures have
1/11, represented as `0x3DBA2E8C`.

`originalAirControlBegin` consumes **filtered original prewind**, not an
invented multiplier on current raw input. Prewind gives different initial rates
and a continuation mode. The controller now begins from either explicit air-control checkpoints or
authoritatively seeded ground prewind/profile data on Cross release. See
AIR_ENTRY_RECOVERY.md for target/filter ordering and the distinct passive
control4 path. Reverse-turn animation effects and complete landing lifecycle
remain separate recovery work.

## Presentation is separate from physical orientation

Spins are not simply written into rider+0x120. In air control state5,
`0x11EB98` calls `0x134DD0` to form the rendered pose from physical position and
quaternion plus the angular state:

1. Shift position to the original animated skeleton pivot.
2. Append local-Z rotation by `-(totalSpin+adjustSpin)`.
3. Update the flip-axis blend from the two angular rates.
4. Append rotation by `totalFlip+adjustFlip` around the corresponding blended
   local axis.
5. Shift back from the pivot using the new quaternion.

The pivot is the scaled skeleton point indexed by rider+0x89C. It must not be
replaced by a guessed board center. The helper preserves original source Z-up
centimeters and quaternion order. Imported native assets use `(x,z,-y)`, so the
renderer converts the presentation quaternion with the same basis change.

`PrototypeRider::seedOriginalAirControl(profile,state,physical,pivot,noGrabContext)`
seeds an airborne checkpoint. It exposes const state/profile/presentation
getters. Angular state advances once per 60Hz tick before translation;
presentation is computed once after position update and then cached. Rendering,
FK and collision callbacks can read that cache without advancing the mutable
axis blend. `originalAirPresentationCurrent` reconstructs the initial cache
without incrementing blend timing.

Physical autoalignment 0x121AA0 and its trajectory predictor are now recovered
in `air_alignment.cpp` and `air_trajectory.cpp`; see AIR_TRAJECTORY_RECOVERY.md.
The native rider advances them when seeded trajectory state and complete native
world queries are available. Otherwise `airPhysicalOrientationPending` remains
true and it retains the seeded physical quaternion. Animated pivot updates still
need the airborne animation player; the current angular checkpoint path retains
the seed pivot.
Landing, grab/handplant interruption, or unsupported late-spin mode disables this
bounded angular path safely while preserving existing translation. No scoring,
grabbing animation state machine or complete landing parity is claimed.

## Accepted airborne commands

`reference_input.py` now decodes these state5 fields from the game's own accepted
RLE stream:

- word0 bits24..29: signed6-bit Spin; byte2: grab index0..14 orFF for none.
- word0 bit14: Square/Tweak (the same physical action as BoostHeld).
- word1 bits0..5: Flip; bits6..11: AirAdjRotLR; bits12..17: AirAdjRotFB.
- word1 bits18..19: signed board-press direction, -1/0/+1.

Handplant, late-spin and unknown/reserved fields still fail closed. All 16
four-shoulder combinations were evaluated through the original compiled
INPUT.MAP VM. Grab indices0..14 correspond to native L1/L2/R1/R2 masks
`[1,2,4,8,3,5,9,6,10,12,7,11,13,14,15]`;FF corresponds to zero. A zero grab
index is an actual selection and must not be confused with neutral input.

## Evidence

`tools/test_air_control_native.py` executes five groups of 20,000 original-code
comparisons: direction snapping, angular updates, prewind entry, command-prefix
continuation and presentation poses. All compare with zero float error. External
stat getters receive explicit test profiles; animation/scoring/contact/active-grab
systems are isolated and are not claimed by these tests.

The development copies use `tools/original_fp_oracle.py` to enforce the verified
PCSX2 scalar DIV.S/SQRT.S nearest policy, including SQRT's Ft operand. VU
DIV/RSQRT and other arithmetic remain unchanged. Native helpers use
`original_float.hpp` at the corresponding scalar sites, preserving VU chop.

`native_original_air_control` is an independent live-emulator golden. Starting
from `snow-jam-air-isolated.p2s`, it consumes the actual accepted stream: one
neutral tick, then 29 positive-spin ticks. All angular fields/rates/timers and all
position/velocity floats match `snow-jam-air-spin-30.p2s` exactly.

- Baseline EE SHA256: `dc494bd8de22d74c1147a87470af22fceedd9dae72f10e6c75d66d33802e3fb6`
- Outcome EE SHA256: `28a485bacc12f43f76501476a416ec267c05cb05de68c8a899a5bf664a74b448`
- Accepted command SHA256: `ea1cbd91b55297a543e4cf738846fbed96c047fe09940527a1d076fb4f2879cb`

The same test verifies 60Hz stepping, immutable pose reads, and safe handling of
currently unsupported grab input. Broader live flip/combined/prewind/landing
comparisons remain necessary for whole-game fidelity.

The EE scalar ADD.S/SUB.S call-site audit now applies the verified single
alignment guard bit through `originalScalarAdd`/`originalScalarSubtract`.
Opcode-corrected development copies independently apply the same instruction
policy. All 100,000 air-control conformance cases and the live spin checkpoint
pass after this correction. VU vector/quaternion arithmetic remains unchanged.

## Release, auto-complete and cancel (PS2 captures, 2026-09-28)

Releasing the D-pad mid-rotation is PS2-exact in the port: physics, all 29 world bones and the owner+0x230 air state.

- **Phase 1 idle** (released after the trick started). `idleTime` counts up.
  - While the idle time is below 0.3 s, the rate path with no input uses the auto-complete speed-up: max(value, max)
    with value = blend·4.9999 + (1−blend)·7.5 over |progress−target|/2π. A released back flip turns 6–9° a tick instead
    of 3.
  - After 0.3 s the target is kept (reduced by π for a spin or 2π for a flip when it is more than that ahead), and phase 2
    starts within π/4.
  - Phase 2 picks 306..313 through 4581A0, finishes at the target, then goes to phase 3 (progress and target back to 0;
    the totals keep the whole rotation).
- **Phase 0 cancel.** A release before the progress is within π−5° (spin) / 2π−5° (flip) of the target zeroes the targets.
  The rider then turns back to 0 and enters phase 3, so a new trick can start.
  - A trick start folds the air-adjust lean (+0x28/+0x2C) into the progress. A left-stick lean held opposite to the D-pad
    trick therefore makes phase 0 last long (e.g. flip lean −72° then a front flip: 405° from its target).
  - A short tap then cancels, and the rider visibly returns to neutral: PS2 air-release/crows-adjust 682..694.
- **A grab blocks the trick start** in phase 3: R2 pressed with the D-pad grabs and does not flip.

Gates (web/test-ps2-captures.mjs, scripts local/ps2-capture/scripts/air-release-*.json):
- air-release/crows-invert: releases while inverted, a spin pressed during the auto-complete, a grab released
  inverted;
- air-release/crows-adjust: the lean-and-cancel cases;
- air-release/glide-land-rotating: landing mid-spin, and flip and diagonal releases;
- air-release/rail-exit-spin: Cross + D-pad on a rail, the rail jump into a spin, and a flip interrupted by a rail
  attach. The rail crouch selects its prewind clip from the retained currents before 1211F8 approaches them; this was
  fixed on 2026-09-28 in web/rail_gameplay.inc and was 1 tick early before.
