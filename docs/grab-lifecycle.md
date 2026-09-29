# Original grab, tweak and Uber lifecycle

`originalGrabLifecycle` implements1352A8 states0..5. The original state contains
only phase(owner234) and grab index(owner238); current animation flags/classes
and live boost context determine the rest.

- State0 requests the ordinary main clip, or cancels it when the identity releases.
- State1 waits for marker bit1, begins ordinary scoring, pauses the main clip,
  and optionally starts its upper-body clip.
- State2 holds the ordinary grab. Releasing ends ordinary scoring, fades upper1
  and resumes main2. Tweak input fades channels1/2 by0.1, chooses the authored
  tweak/Uber main clip, sends29A530, applies the stat-derived rate and enters3.
- State3 enters the advanced hold on matching identity+tweak+marker bit1. It
  pauses main2, begins the mapped advanced score and starts the authored upper
  clip. Otherwise marker bits1/2/3 can produce begin and end scoring in the same
  tick, then enter5—even if the input is still held but only a later marker is set.
- State4 holds the advanced trick. Releasing ends its mapped score, fades upper1
  by0.33, resumes main2 and enters5.
- State5 waits for class2 or marker bit3. It can chain directly into another
  authored Uber with a forced main request, or resets phase0/index−1.

Upgrading from ordinary state2 does **not** insert an ordinary score-end call.
The source begins the advanced score later, preserving the existing shared
hold timer until its actual end call.

Initial Uber selection requires nonzero superTime, capabilityB2C bit0 and a
non-sentinel mapping. Otherwise it selects the tweak. The tier variant is
sampled once per1352A8 invocation from boostTier>=5. State5 chaining requires
class20, tweak input, a requested index and an available mapping, but does not
recheck superTime or capability. Do not freeze the tier in the grab state.

The returned grab-active flag uses the main class cached before the state5
reset/restart: classes18..20 returntrue even if the state has just reset to0.
That result controls the separately verified angular gates.

## Typed data and callbacks

The profile retains ordinary `grabs[15]` and adds `tweak[15]`, `uber[2][15]`, and
`extendedDefinitions`. Definitions contain main semantic, upper semantic,
score ID and authored begin/hold points. `reference_grab_lifecycle.py` reads the
150xxx/14FEA8 mapping tables, including special-character overrides, and exports
live context for superTime, boostTier and capability.

The optional `mappedScore(scoreId,begin)` callback carries exact119708/1197D8
requests. Legacy `score(index,begin)` remains supported for ordinary definitions.
Advanced score requests require the mapped callback. `advancedStarted` carries
29A530 for both tweak and Uber entry; it is not an inferred boost award. Existing
five-argument ordinary calls remain compatible through default context/data.

## Leg IK weight318

120D90 is separate from1352A8. The common1210B0 timer phase calls it AFTER all
121068 controller updates and BEFORE1211F8 smoothing, so it sees the newly
requested main class. Classes20..26 approach0; other classes approach1. It uses
float step0x3D4CCCCE and authored exact snapping thresholds. The helper
`originalGrabLegWeight(previous,mainClass)` preserves those original operations.
It must not run per render or twice during an advanced-entry request.

## Validation

`test_grab_lifecycle_native.py` compares80,000 full original1352A8 cases,
including the unchanged20,000-case ordinary API suite. It checks state/index,
ordered play/fade/score/notification requests, channel rates and cached active
return. The extended run includes1,667 advanced entries and24,709 advanced score
requests. The same suite checks20,000 complete120D90 leg-weight updates.

`test_grab_definition_native.py` independently verifies1,800 original
main/upper/score mappings across six rider slots, five special-character types
and both Uber tiers. Scoring, animation creation and29A530 feedback are explicit
subsystem boundaries; the lifecycle emits their original requests rather than
claiming their downstream systems are implemented here.
