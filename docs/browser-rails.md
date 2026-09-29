# Browser rail integration — September 12

Ordinary authored-rail attachment, control7 balance cycles, motion4 spline following and automatic exit back to air are now connected. This is an initial playable integration, not completion of the rail feature.

`web/rail_gameplay.inc` adapts the existing original rail routines using the current source physics state and the previous completed world pose's board bone23. It follows the native `riding.hpp` rail adapter. `step_rider` invokes the adapter before ordinary motion; an owned rail tick skips cruise motion/contact. `animation_tick` preserves rail controller ownership, advances the original graph/cycle, and supplies the resulting pose to the next tick. Camera input uses motion4; snow/track input uses motion4 instead of leaving tracks as ordinary snow riding. Ordinary body/landing processing is suppressed while rail control owns the pose, matching the current native adapter's limitations.

Entry runs originalRailAttach, root presentation baking, originalRailMotionBegin and originalRailControlBegin. The graph uses the corrected original RS/RSFS/RSBS cycle maps and airborne entry semantics. Balance comes from the source rail triplet. Leaving runs originalRailMotionLeave and originalRailControlStep's airborne handoff, restores stance and seeds passive air. Publication uses the host nearest boundary. Reset/restart clears rail ownership and caches.

The adapter now computes original rail distance/rotation rewards, banks them at the recovered boundaries and awards boost. Grind effects and full score notifications remain incomplete. Rail rotations now map to J/L and gamepad right-stick X. Trick identity/Uber and transfers remain unmapped. Space now requests control2, charges and jumps on release; held rail loss hands back to ordinary control2. Additional stance/style and dynamic-world cases still need validation. Scenery/body interactions on rails, dynamic-instance rails, alternate board offsets/stats and complete original host phase parity remain gaps. Do not claim full SSX3 rail behavior.

## Verification

- `node web/test-rail-gameplay.mjs` must run from the web directory as `node test-rail-gameplay.mjs`; it is now included in `npm --prefix web test`.
- Fixture: authored `spline_ARA1_RAIL_3007`, start3.5m before its first segment, source velocity900cm/s aligned with its horizontal direction. The explicit `set_rider_velocity` API is for fixture initialization; ordinary starts do not call it.
- The400-tick test attaches at tick1, first exits at192, uses original cycles for304 of305 active frames (including later reattachment), travels27.3386m in the first grind, and lands back in control0. It requires finite complete poses and control7 during active rail motion.
- The full native/WASM comparison includes a600-tick rail scenario with two attachments, two exits and480 active rail frames. Every494 recorded field per frame matches across all3,600 frames/six scenarios. This compares this adapter on both platforms, not a PCSX2 run.
- The3,885 curve-query comparisons and existing rail helper tests pass. The full browser suite passes.
- Live WebGPU screenshot showed Sam grinding with semantic18/control7 on packed rail31752 at60fps.

For a local visual test, open `http://127.0.0.1:4173/?railTest=spline_ARA1_RAIL_3007` and use the normal START → Single Event → Sam → Continue → Snow Jam flow. This opt-in fixture changes the start position/velocity; the normal URL retains the course start. The current tab is paused on this fixture.

`rail_gameplay_info` returns eight floats: active motion, rail controller ownership, attachment count, exit count, motion ticks, balance, packed rail id, observer count. `window.demoState` includes the corresponding live rail diagnostics. No new game UI styling was added.


## Charged rail jump follow-up

Direct inspection of1162C8 establishes that the legacy `upperAction` callback is a crouch request: held/pressed input and rider+360 latch lead to control2 at116344. The old attack/upper-body description was incorrect. The browser now maps Space held/pressed to command bits14/13, applies originalCrouchRequest, and preserves the first-press no-charge rule. Subsequent held ticks request crouch/prewind targets; release uses originalJumpTakeoff with motionMode4 and the board-up direction, rail focus-loss callbacks, and explicit-air control5. No custom jump impulse was introduced. The separate pressed-feedback observer remains unported.

A one-frame release flag prevents a second animation control dispatch on the transition tick. Leaving the rail while still held clears rail ownership and preserves ordinary held-air control2 and normal collision/landing processing. Release after that edge delegates to ordinary control; it must not add a rail impulse. On the flush-with-snow fixture, touchdown may occur on the same frame as leaving the rail; this is allowed.

Tests:40 held frames reach charge1, a release at tick90 produces exactly one rail jump and control5, with finite original poses. Held-to-edge run leaves at194 and releases at195 with no rail jump count. The full comparison adds a600-frame rail-jump scenario: all497 recorded fields agree across4,200 frames/seven scenarios. Live WebGPU check observed charge1/control2 on rail, then semantic268/control5 off rail at60fps.

`rail_preinput(flip)` supplies the vertical prewind input before step_rider; `rail_jump_info` returns held-rail crouch, release-frame flag and rail-jump count. Normal main input and both comparison drivers supply the same preinput. Rail jump scoring, extra feedback, rotations/Ubers/transfers and grind effects remain incomplete.


## Rail rotations follow-up

J/L (left/right) and gamepad right-stick X supply an independent rotation axis through rail_rotation_input; A/D/left stick continue balancing. originalRailControlStep uses its class14 completion/marker gates and originalRailRotation's full table. No custom rotation tween or cooldown was added. Tests pulse left at20..22 and right at70..72: exactly two rotations, original semantics49/51, completion3 and frontside cycle19 are observed. Continuous held rotation also tests leaving the rail into control5; no rotation input leaves into control4. The adapter now seeds the requested active/passive controller and skips a duplicate control dispatch on that handoff frame.

rail_rotation_info returns count, current style, accumulated source spin and mirror flag. The full comparison adds600 rail-rotation frames and records501 floats/frame; all4,800 frames/eight scenarios match. Full browser tests and scalar parity pass. Live keyboard test shows semantic49/control7/style3, one rotation, rail active at60fps. Physical gamepad behavior has not been tested.

Still incomplete: transfers, rail Ubers/identity/scoring, grind FX/audio, broader stance/style/dynamic-world contact coverage and original host-phase verification against PCSX2.


## Scoring recovery in progress

`engine/rail_score.hpp` recovers119918->119898 rotation updates and117D74..117DFC distance scoring/thresholds. These helpers are not yet connected to the live score. Rotation changes pending reward by removing the prior absolute spin contribution and adding the new one; its immediate boost return is0. Style20 is updated. Rail distance24 advances by source speed(cm/s)*scaled1/60; accumulated14 receives distance*0x3851B717. Negative board-up Z also updates separate inverted accumulator1C with distance*0x3951B717. Do not interpret field24 as seconds.

Threshold table459F68 has11 distance thresholds10000..30000cm in2000cm steps with points1000,3000,5000,7000,9000,12000,16000,20000,30000,40000,50000, followed by a negative sentinel. At most one threshold is processed per tick, using source multiplier/rounding and event29. Event clearing order28,29,30,31,32 and display duration1.5 match119210.

`sh tools/test_rail_scoring.sh` compares20,000 randomized cases against actual original code, including6,299 bonus events, then compares an optimized WASM module to the original output corpus bitwise. All pass. The source harness initializes VU0's constant register to(0,0,0,1); omitting that initially invalidated its speed calculation and was corrected. The source run stops at both117E00/117E04 after the rail stage, before unrelated manual/Uber stages. Logs:local/rail-score-reference.log and rail-score-wasm.log.

Still required before enabling live scores:119D40 commit/reset/seed behavior on ground/air rail entry,10E910 caller effects, rail exit/pending-state lifetime, the role of inverted1C in commit, and correct banked-points/boost delivery across core->animation phases. Current rail observers still award no points. Do not wire a flat points-per-second or approximate boost reward in place of those calls.


## Commit/reset boundary and inverted bonus recovered

`score_boundary.hpp` represents the per-trick fields reset by117838 and119D40's commit-before-reset ordering. A nonzero incoming style adds0.13 to an already-positive pending reward before commit. The wrapper forwards the original five commit arguments, saves active70 before the callback, resets/initializes the represented fields, seeds stance/field04/style/flag (including flag10), starts distance24/manual timer2C when applicable, and restores active70/activeUber5C/activeSeconds6C. Persistent timeoutA4 and rider multiplier1C4 survive with any values updated by commit. Statistics/history outside the represented reset range remain caller-owned.

20,000 source-wrapper tests pass;11A228 is deliberately an isolated callback there. A separate6,000-case full11A228 test now covers rail/short-rail/active-Uber identities, reference stance and nonzero inverted accumulator1C. That test exposed a missing bonus in the old ordinary helper.117908 converts1C*10000 to the original rounded multiple of10;11A458 adds it after normal repeat division, with no trick/rider multiplier and no increase to the meter award. originalOrdinaryTrickCommit now accepts optional invertedReward (default0) and reports invertedPoints. Both the original6,000 ordinary tests and6,000 extended rail tests pass.

Existing grab-score, identity and commit arithmetic was migrated to software-aware scopes/operators for browser fidelity.20,000 grab cases and all4,800 gameplay comparison frames/full browser tests pass. Production rebuilt. The new rail boundary/distance scorer is still not wired to live rail points: exit/pending lifetime and10E910 caller/boost bookkeeping must be finished first. Logs:score-boundary-reference.log,rail-commit-reference.log,trick-commit-reference.log,grab-score-reference.log.


## Live rail scoring connected

rail_scoring.inc connects119D40 entry commits,119E38 takeoff commits,119918 rotations and117D74 distance accrual to the existing score/identity/history and10E098 meter award. Takeoff-boundary tests cover ordinary commit/reset and active-Uber preservation;6,000 full rail-commit tests also cover t1=1. Saved style/flag seed the airborne trick.

Distance scoring runs after race-end route progress; the HUD reads refreshed pending points afterward. Core-step awards are queued and consumed once at animation update. Same-frame landing awards add instead of overwriting an earlier commit. Generic score clearing resets the represented extra fields. OriginalTrickIdentityState now defaults distance/manual timers to source117838's -1: the reset regression caught zero-initialized identity accidentally accruing rail distance in air, and that was fixed.

Explicit jump and passive rail loss invoke the takeoff score boundary. Passive rail loss also applies114298's negative-charge arithmetic, previously omitted by the adapter. Threshold bonuses and separately rounded inverted points are retained; rotation returns0 immediate boost. Air-to-rail awards use existing Uber progression/award helpers. Meter is not computed from displayed points.

The first authored grind reaches1380 pending points and banks1380 exactly once, with raw meter award0.1381329447. The earned meter can power original boost control. Reset/jump/held-edge/rotation tests pass, and all509 recorded fields match across4,800 native/WASM frames. Live HUD: score1380, pending0, meter0.1356331 after decay, one rail exit and60fps. Production rebuilt.

rail_score_info returns distance24, pending reward14, inverted1C, total rail-banked points, boundary count, threshold-bonus count, total raw meter award and queued points.

Still incomplete: full score-name/bonus notifications, totals/timeout side effects outside the ordinary commit helper, original boost-meter HUD filling/animation (current meter art is static), rail Ubers/transfers/grind FX/audio, and full PCSX2 host/frame fidelity. Basic rail points and usable earned boost are now live; older notes that rail scoring is unconnected are superseded.
