# Boost controller and meter lifecycle

The native runtime now calls the original-derived boost control and meter/timer updates, using values extracted from the owned snapshot. This is connected to source-backed ground/air riders; it is not the inspector's older boost acceleration constant.

- `114130`: press feedback flag, meter thresholds, amount selection, active drain, start/denied/stop callback selection.
- `1200D0`: window/modifier decay, super timer and its airborne/handplant boundary, tier10 expiry to tier5, and meter-policy decay.
- `2F6AC8(owner+D20,0)`: feedback byte D27 OR0x10. Manager callbacks remain explicit pending events; sound/haptic systems are not claimed implemented.

The actual reference thresholds are2/3 and1/3 (their stored float values). Amount is1,.625 or.25 when boost first starts; exact boundary comparisons are preserved. Active drain comes from gp−7BF4. Natural meter decay and timer rules come from the independent1200D0 stage, after controller dispatch and before common smoothing. No clamp or refill was invented. State304 is a meter policy, despite the historical `drainEnabled` field name:0 forcesfull,1 normaldecay,2 fastdecay,other policies retain the meter here.

Control0 passes held and pressed; its jump-entry frame returns before this call. Control2 passes held and no press, and release returns before it. Control5 passesfalse,false: its encoded bit14 means tweak, not boosting. The angular controller observes the pre-timer modifier, matching original frame ordering. Ground force/animation state receives the current amount, and speed-limit computation retains its earlier frame-begin position.

`tools/test_boost_control_native.py` compares30,000 complete114130 cases plus30,000 complete1200D0 cases against corrected original instructions. It checks binary float results, threshold boundaries, feedback flags and callback order. Virtual140BC8 is a side-effect-free getter whose result is unused in1200D0. Notification10E028 updates notification state outside the meter arithmetic; it is returned as a pending expiry event.

This does not complete boost rewards or Tricky progression. Landing emits its scoring event; original119D40/10E910 awards, audio/haptic event consumption and shared race behavior still require implementation. The current ordinary snapshot starts with an empty meter, as the original does.


## Meter awards recovered September 11

`boost_award.cpp` implements the complete meter arithmetic in `10E098`. It accepts the original delta and category; it does not derive a reward from displayed points. `tools/test_boost_award_native.py` runs 30,000 cases against the original instructions with scalar floating-point corrections, comparing meter/timer bits, tier, callback order, and callback arguments. The test passed; log: `local/boost-award-validation.log`.

- Policy3 and zero delta return without events.
- Positive deltas emit149690 before testing category against rider+B28. Negative deltas bypass that eligibility mask.
- Eligible changes emit29AB08 with the old meter and delta, then clamp the original sum to[0,1].
- Positive awards reaching full meter with B2C enabled request notification5 when superTime is zero. A nonpositive tier increments once; existing positive tiers are preserved. Tiers1..9 extend superTime to at least20.
- Negative changes clear superTime only below tier10.

The extractor now records `award_context` from B28/B2C for new captures. Existing packaged snapshots have not been regenerated. The helper is compiled into the native boost library but is **not yet called from browser landings**. Browser behavior therefore remains unchanged this step.

### Landing integration map

`10E910` calls `119D40`, which calls `11A228` and resets the per-trick state through117838. `11A228` formats the original8-byte trick identity through11A8C8, tests recent repeats through1190F0, and divides the accumulated14 reward by(repeat count+1) at11A440. The displayed score uses separate getters/multipliers; dividing displayed points by10000 is not equivalent in general. `10E910` then passes that returned delta to10E098(category1), and clears modifier2EC when the award is positive.

Next: recover11A8C8 identity generation and1190F0's ten-entry repeat history, then the full11A228 commit/119D40 reset. Capture B28/B2C in browser assets, connect this award helper, and test repeated versus different tricks end-to-end. Do not skip repetition or substitute an invented meter refill. Shared notifications/audio, totals, and Uber tier progression remain separate integration work.


## Repeat history recovered September 11

`trick_history.cpp` now implements complete1190F0 over a supplied original8-byte identity. `tools/test_trick_history_native.py` passed30,000 original-instruction comparisons of return count, all80 ring bytes and cursor; sequential checks additionally cover saturation at10 repeats and eviction after10 different insertions. See `local/trick-history-validation.log`.

Nonzero scoring fields08/20/70/7C/28 skip both matching and insertion. Identity `(word1 & 0x3F800)==0x800` or `(word0 & 0x0FC00000)==0x00400000` skips matching but still inserts. Otherwise all10 entries are compared byte-for-byte before the current entry overwrites slotF8; F8 advances modulo10. No occupancy bit or extra identity normalization is added.

`reference_grab_score.py` now extracts this history and its gating fields for new captures. The original11A8C8 identity builder and full11A228 commit are still missing, so browser landing behavior remains unchanged. Do not manufacture an identity from the browser's clip name or shoulder-button mask.


## Trick identity recovered September 11

`trick_identity.cpp` implements complete11A8C8 as typed native operations, including its eight-byte result, validity return and in-place spin34/flip38 sign adjustments. `tools/test_trick_identity_native.py` passed40,000 original-instruction comparisons for all those outputs under source chop rounding. The initial test harness accidentally ran the original under host-nearest rounding; setting both paths to the source mode removed that harness discrepancy. Evidence: `local/trick-identity-validation.log`.

The builder uses authored319-byte rotation tables at43D388/43D4C8 and the two captured degree scales. `tools/reference_trick_identity.py` extracts these and the scoring inputs, validates the scoring-owner backlink, and records rider stance. Tested capture: `local/reference/pcsx2/trick-identity.json`.

Identity cases include active70, flag28/style20 special records, ordinary rotations and cork combinations, stance flags, up to three grab identities and field7C. The source normalizes rotations by signed180-degree buckets, maps recognized combinations through the original tables, and uses an explicit fallback record for out-of-table rotations. No clip-name mapping is substituted.

The remaining landing dependency is full11A228 commit plus119D40/117838 reset semantics and caller-driven state updates. Identity, repeat history and meter award helpers are now present and independently oracle-tested, but they are not yet connected to browser landings. Avoid claiming earned boost or complete scoring until that connection and end-to-end validation pass.


## Named combinations and per-trick reset recovered September 11

`trick_bonus.cpp` implements11B1A8 using the24 authored16-byte rows at43D608. It compares seven bit fields of the packed identity, takes the first match, clears the ordinary identity and writes the named ID into word1 bits27..31, returning the authored bonus. The identity extractor now includes that table. `tools/test_trick_bonus_native.py` passed30,000 original-instruction cases, including15,000 hits covering all24 combinations, comparing both the returned points and rewritten identity.

`originalResetGrabScore` implements117838 for the fields represented by OriginalGrabScoreState. The browser's ordinary landing now calls it instead of assigning a fresh struct. Crucially,117838 leaves comboTimeoutA4 and multiplier1C4 unchanged. The extended original grab-score oracle now checks reset at the end of each of its20,000 cases, including randomized persistent values; it passed. This does not claim the entire scoring object is represented yet.

Full11A228 commit (score multipliers, totals, event dispatch, timeout/history and special-mode branches),119D40 caller setup and browser state updates remain before earned boost is complete. Identity, named-combination matching, repeat history and meter application are available; do not bypass the remaining commit logic with an arbitrary reward.


## Ordinary award connected September 11

`trick_commit.cpp` and `tools/test_trick_commit_native.py` now cover the ordinary award arithmetic.6,000 complete original11A228 calls match returned meter, score total increment198, normalized14, repeat history and cursor; notification-only callbacks are isolated. Rotation inputs use the original air controller's scored30/34 fields and119898/1198D8 replacement arithmetic. The browser imports all authored identity/named/award constants and calls these helpers on ordinary landings, then10E098, allowing real earned boost. The exact gameplay regression is in web/test-earned-boost.mjs.

Earlier notes saying browser awards are unconnected are historical. Remaining scope: full totals/event/timeout side effects of11A228,119D40 restoration for all modes,10E910 Uber tier advancement, complete source stance coupling, original HUD animation and nonordinary landings. Do not describe the ordinary arithmetic helper as a full port of every11A228 side effect.


## Uber landing tier increment connected September 11

`originalLandingUberProgression` handles10E9B4..10E9F4 before the meter award. A positive change in committed Uber count adds at most(10-currentTier), and reaching10 sets superTime to60. No increase or an existing tier>=10 leaves tier/timer unchanged.20,000 original10E910 wrapper cases verify this arithmetic; unrelated score/effect callbacks are isolated. The ordinary commit's positive-score gate controls whether Uber counts are committed, verified against original114 statistics in the6,000-case commit oracle.

Browser landing now invokes this stage before10E098 and exposes the resulting state to original grab selection and next-frame physics. Animation-only fixtures may supply captured tier/time through init_animation; production attached animation reads the earned physics state. This is not a full UI/audio or special-mode progression port, and natural gameplay progression through every tier is not yet verified.
