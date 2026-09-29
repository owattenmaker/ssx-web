# Original rail Uber controller recovery

Entry gate132620 is now ported in engine/rail_uber_entry.hpp and tested by tools/test_rail_uber_entry_native.py. Requested identity-1 resets the remembered ordinary-rail identity and returnsfalse. Otherwise positive rider2F0 (Tricky time) plus riderB2C bit1 stores requested identity at owner394 and requests control12, without updating the ordinary rail identity latch. Ineligible changed identities notify299B70 once, then update the latch.20,000 original calls match state, return and callback order. The first prototype omitted the release latch reset; the source test caught and corrected it.

Captured riderB2C is3, so the capability exists in the starting player profile. The input-to-identity mapping still needs recovery; do not treat arbitrary grab bitmasks as table indices.

Control12 object is owner390 (state+0, identity+4, rider+8). Original control routing calls136268 on entry and136508 for continuation/update; exit routing must also be completed. Entry table is45A038, four rows of36bytes. Each row contains eight animation semantics and a final value1..4. Rows cover213..220,221..228,229..236,237..244. Raw table and variant-resolved names are in local/browser-validation/rail-uber-table.json.

For GRIND1,213..216 are BS/FS/fakie/regular entry variants,217 is RSFS_GRIND1_CYC,219 LAND and220 OUTOF.218 (and corresponding rows) is kind10, class21: its balance driver is not in browser cycle/three-way maps, even though source BAL_L/BAL_R clips are present in the library. Existing animation_variants resolves the other entry/cycle/exit states; absence from state_definitions alone does not mean they are missing, because state_properties+animation_variants are also loaded.

Next recover136268's stance/root operations,136508/exit phases and kind10 balance sampling, then connect verified button identity mapping and earned Tricky tests. Browser rail control still leaves this feature disabled/explicitly unsupported; the entry helper is not a completed gameplay implementation.


## Existing native implementation reused; kind10 connected

Further worktree inspection found engine/rail_animation.hpp and tools/test_rail_animation_native.py already contain native implementations of the kind10 driver and control12 entry/update requests. Earlier wording that these algorithms needed recovery was too broad: the browser connection was missing. Reuse those helpers rather than creating parallel implementations.

The common rider animation graph now resolves kind10 initial/duration clips from originalRailUberBalanceLeaves and runs originalRailUberBalanceStep using railBalance. This selects BAL_R for negative values, BAL_L otherwise, seeks abs(balance)*duration, advances only fades, and does not reverse for switch stance. Normal cycle clock stepping is bypassed. The generated browser graph receives the same code from rider_animation_player.mm. Converted the used two-way driver multiplications/rounding scope to the explicit portable original arithmetic.

Validation: native oracle passes30,000 ordinary rail-cycle ticks,20,000 kind9 and20,000 kind10 seek cases,16 attachment semantics,32 control12 entries and3,000 control12 episodes. Browser test-rail-uber-balance exercises all four families at seven balance values, both stances, through a copied production graph; validates clip IDs, seek positions and no clock progression. Full npm suite and6,270-frame native/browser comparison pass; production rebuilt. The QA probe does not mutate the live rider graph.

Remaining: wire control12 into the live rail adapter, preserve input identity mapping, prologue/root/stance transitions, rail loss/landing, score and Uber-counter callbacks, and verify earned Tricky->Uber grind->exit end-to-end. Merely having a driver does not enable the maneuver in gameplay.


## Physical input mapping verified

Original PS2 DATA/CONFIG/INPUT.MAP explicitly defines UberGrind1..4 as exclusive L1/L2/R1/R2 held expressions. Combined shoulders are not another rail-Uber identity.127848 queries mapped actions27..30 and returns0..3 or-1. Added originalRailUberIdentity to rail_uber_entry.hpp.

Extended tests/original_input_reference.cpp to evaluate those four actions through the original input-expression VM for all16 shoulder masks, execute127848, then pack commands through127998 for controls7 and12. All pass. Existing30,000 provider roundtrips,8,192 original pad samples and20,000 axis quantizations also pass. Optional movie recording is disabled for this mapping/packing fixture; no missing targets remain. Generated same-function calls require resuming at127EE8/127FE0 before checking the final packet.

Correction: control7 identity is bits17..24, but control12 identity is bits15..22 (127FEC..127FF8). Added originalRailUberCommandIdentity and corrected the legacy rail_animation.hpp comment. Control12's13/14 bits feed114130 boost handling, not a generic upper/jump action. Browser integration must use this controller-specific interpretation.

The live maneuver remains disabled while controller handoff, motion/score callbacks and earned-Tricky end-to-end coverage are completed.


## Scoring boundaries and exit-body masks

Added originalRailUberScoreBegin/End to score_boundary.hpp.119938 initializes activeSeconds6C, active70, activeUber5C and scoring styles without resetting accumulated points.119958 increments uberCount54/field74 before11A228, resets through117838, then restores distance24/airSeconds30 and the new style.20,000 original entry/exit comparisons pass, alongside the existing20,000 ordinary/takeoff boundary cases. Commit arithmetic remains isolated in this boundary oracle and covered by its separate existing oracle.

Found the active-score accrual block117E58..117E84: if activeSeconds6C>=0, add source dt to that timer and dt*.04999999701976776 to accumulated14. This must be included when wiring controller12; merely setting its identity would miss time-based points.

Corrected interpretation of13BFA8's458230 table. It contains temporary body-sphere masks, not an animation/event table: control12 identities0..3 select141/16/2/2 hex for the105398 contact phase; other controls select16.13C078 then sets3 for13C140;13C094 restoresFFFFFFFF before107888. engine/rail_exit_mask.hpp exposes the first mask selector; tools/test_rail_exit_mask_native.py verifies all56 control/identity cases against the original block. Full ordered contact-phase execution is still caller-owned and must be connected before removing the control12 rail-exit guard.

Live Uber grinds remain disabled. Next apply these score boundaries, active-time accrual, controller state/root requests, and original ordered exit-contact phases in the browser adapter, then validate earned Tricky and successful exits.

## Active-time scoring connected

Added originalRailUberScoreTick for117E58..117E84 and connected it after distance scoring in the browser tick_rail_score path. It adds dt to activeSeconds6C and dt*.04999999701976776 to accumulated14 only while that timer is nonnegative. Uses explicit original arithmetic; it does not infer activity from a renderer animation or held button.

tools/test_rail_uber_score_tick_native.py compares20,000 original block executions, covering inactive negative timers, zero/new timers, positive timers and varied dt. Both original branch exits are intercepted: negative timers skip directly to117E8C, while active timers reach117E88. An earlier single-exit fixture accidentally ran unrelated downstream code; the final fixture has no such fallthrough and passes. Full browser suite and6,270-frame native/browser comparison pass; production rebuilt.

Live control12 activation/collision handoff remains incomplete. This scoring hook remains inactive until the controller calls the verified entry boundary, so no new playable Uber grind is claimed.

## Ordered rail-exit contact phases verified

Added originalRailExitContactPhases to rail_exit_mask.hpp. It sets the family/control-specific mask before105398, sets3 before13C140, then setsFFFFFFFF before107888. It intentionally does not restore the incoming mask or clone the body between callbacks. Expanded test_rail_exit_mask_native.py executes13BFF0..13C09C and checks all56 control/identity combinations, exact query/rebuild ordering, owner/mode arguments and final mask. All pass. Actual contact-response math remains a callback boundary in this test.

Inspection of13C140 confirms it is more than generic obstacle response: after accepted separation and orientation rebuild it maintains rail motion+30 position/+40 timer, with a2cm movement threshold and.5s stuck branch before velocity/contact notification. Do not replace this with an ordinary-ground bounce or drop its state handling. Source105398 is the non-solid selected-contact phase;13C140 uses the physical world query. Live control12 integration still requires those responses/refreshes, so the existing guard remains until the complete path is wired. No new playable Uber grind is claimed by the sequencing helper.

## Rail physical-contact response recovered

Added engine/rail_contact_response.hpp for13C140 after its physical query. It rejects opposing contacts below-.9999, projects/normalizes negative-alignment normals, translates by1.1*depth, invokes companion translation/orientation rebuild, tracks the previous contact position and stationary timer, and performs the source stuck response after.5seconds within2cm. Stuck direction follows physical forward or its projection, at1111.111cm/s. Closing velocity receives the original max(.5*closing,27.77778) rebound and emits the source direction/normal/impulse notification. This is deliberately separate from the ordinary-ground .05 rebound path.

The callback receives the exact translation vector: deriving it from two large rounded positions would lose precision when updating AA0/9D0/bounds. The callback must apply106538 companion fields and11E098 orientation rebuild, updating the physical forward used by later stuck logic. Rail motion entryPosition/+30 and word40 (interpreted as float timer) supply persistent state.

tools/test_rail_contact_response_native.py executes full13C140 with controlled world-query, companion-volume, orientation-rebuild and collision-notification boundaries.20,000 cases match position, velocity, previous position, timer, exact translation, rebuild presence and notification fields;9,162 notifications and4,936 stationary responses are exercised. Query geometry and the actual11E098/105D98 implementations remain covered separately, not claimed part of this response oracle. The helper is not yet called by the browser rail exit path, so no live Uber-grind completion claim is made.

## Exit callback can return live actor changes

Added OriginalRailAccess.exitContacts(OriginalRailRider&) and invoke it before the rail-exit conditional air transition and speed clamp. Control12 still throws if the callback is absent; no browser callback is installed yet. This avoids an API design where contact handling updates a global actor and stale local rail data overwrites it on return. A regression verifies position/control/motion/velocity updates survive, that a callback switching to crash motion2 suppresses the helper's air request, and that the final source speed clamp uses corrected velocity.

Call-site audit finds13C140 referenced by13BFA8 in generated source, with no stored function-pointer occurrence in the owned ELF. It was therefore not added indiscriminately to every rail frame. Its exact exit use is established; any broader use requires evidence. Existing ordinary rail callbacks remain unchanged.

Native rail helper tests, full browser suite and6,270-frame native/browser comparison pass; production rebuilt. Browser integration must still implement exitContacts, honor changed motion/control in its caller (avoid unconditional leave_rail_motion after a crash), apply exact companion translations and dispatch original collision/pickup events. Interface availability alone does not enable or validate a live Uber grind.

### Rail-to-crash ownership handoff

Browser enter_crash now captures previous motion4 while grinding, rather than deriving ground0 from the grounded display flag. Original136C40 enters airborne crash motion for any previous mode other than0; the caller now preserves that distinction. It releases browserRailActive and rail controller/crouch ownership without resetting accumulated rail diagnostics. Air animation exit baking remains limited to prior air motion1.

A regression reaches spline_ARA1_RAIL_3007, injects original crash-entry event350/speed900 after an authored attachment, asserts airborne crash submode and released rail ownership, then checks that rail motion does not keep stepping through the crash. This is an explicit collision-entry test, not proof that scenery collision automatically dispatches on rail exit. Added rail-crash to native/browser parity using the same typed event at tick21:6,630frames,16scenarios,606fields agree. Full browser suite and production build pass.

Still needed before live Uber controller activation: install exitContacts, preserve the same masked AA0 body across trigger/physical passes, translate companion AA0/9D0 fields by the exact source delta, dispatch rail collision reactions with motion4 and source incomingDirection/impulse, and resynchronize the mutable rail view after callbacks. Both step_rails leave call sites must stop their normal air/control handoff if crash/reset took ownership; otherwise they overwrite the corrected crash. Original107888 final phase also needs its actual attachment/pair-dispatch semantics, not a pose-only replacement. Do not remove the control12 exit guard prematurely.

### Light collision control while grinding

Browser control3 now supports motion4. begin_soft_control accepts rail control7; entry performs the recovered ordinary-rail control leave balance targets and clears rail crouch ownership. step_rails calls originalSoftControlStep with motion4, forwards its boost and113F38 steer requests, and restores1326C8's style-selected rail animation followed by originalRailControlBegin when the impact clip completes. A rail departure during the impact relinquishes rail ownership so normal soft/landing logic can continue.

All six original light impact semantics55..60 were entered through the existing collision-entry API during a real spline_ARA1_RAIL_3007 grind. They retain rail motion and return to control7 after31ticks (47for58). A late58 hit at175 leaves the rail and touches down at180, releasing rail ownership and returning to normal riding. This latter fixture does not demonstrate sustained airborne soft control; do not mislabel it. Full browser suite passed before the additional late-impact case, which also passed separately. Original20,000 control3 cases and30,000 scenery collision classifications pass. Native/browser parity adds a360-tick rail-soft-collision scenario:6,990frames,17scenarios,606fields agree. Production rebuilt.

Correction to earlier final-phase note: call-site inspection of generated107888 finds rider-pair separation/impulse/attack dispatch, not106848 rail attachment. The existing OriginalRiderPairSystem models that dispatch. A single-rider roster has no enabled pair records; opponents remain missing in the browser. The remaining exit hookup must still supply source rail event impulse/direction/motion4, masked body queries and exact companion translations, and preserve callback-owned crash/reset state. The hook is not installed and Uber grinds are still disabled.

### Ordinary rail-exit contacts connected

The browser now binds OriginalRailAccess.exitContacts. Ordinary exits use the same cached body for source mask0x16 trigger dispatch, mask3 physical query/13C140 response, and finalFFFFFFFF pair phase. The current browser has only one rider and no enabled pair records, so the final pair phase has no counterpart; NPC interactions remain absent. Incomplete physical queries are counted and not used to synthesize a response.

13C140 uses originalRailContactResponse, exact AA0 companion translation via translateOriginalPairBody, offset9D0 accumulation, and11E098 orientation rebuild. Rail motion+30/+40 retain source previous-position/stationary-time state. 121020 clears offset9D0 at the next motion tick. A shared dispatch_body_event accepts the source impulse, already-normalized incoming direction, and explicit motion4; ordinary ground/air calls preserve their previous behavior. Original callbacks can select light impact, crash or reset. Leave callers retain these control changes and apply the final velocity clamp to the current crash/reset actor instead of replacing it with an air-control request. Charged rail release no longer overwrites a contact-selected light impact with control5.

Pickup timers use logical motionTick+1 before the extra trigger query and deduplicate against the normal post-pose pass. rail_exit_info exposes four counters: passes, accepted physical responses, incomplete queries, notifications. The ordinary rail fixture completes one exit query with no physical hit or unsupported geometry. A synthetic solid box intersects the real cached body at charged rail release: one pass, one accepted response, zero incomplete queries, one notification, original soft semantic56/control3, and released rail ownership. No hit result or reaction is injected in this fixture.

Validation:20,000 original13C140 response cases and56 original mask/phase sequences pass; full browser suite and6,990-frame606-field native/browser comparison pass; production rebuilt. The current hook explicitly rejects control12 until its identity is connected. Uber entry/update/scoring is still not enabled. Dynamic entity callbacks, multiple-rider dispatch and full original host phase parity remain outstanding. Synthetic solid collision coverage does not establish every authored rail/obstacle combination or visual fidelity.

## Browser Uber grinds enabled

Control7 now invokes caller-owned132620 after recovery/upper-action gates and returns immediately on successful entry. Browser forwards the same four-bit shoulder mask already used by grabs; only exclusive1/2/4/8 selects identities0/1/2/3. Entry uses boostState.superTime and the owned start's B2C=3 ability flag. Unavailable input is latched/counted, but its original audio feedback is not implemented. Main keyboard mappings are Q/Z/E/X for L1/L2/R1/R2.

Browser control12 runs recovered136268 entry and136508 ordered requests: entry variants, cycle, kind10 balance, land-on-reattach, out-of animation, stance restoration, score commit/award, and final Uber-tier increment. 111578's exit dispatch table456B90 maps control12 to111624 (no extra exit callback). 136888..136890 explicitly forwards119958's returned award to10E098 before the counter update. Ordinary motion4 balance/steering and the existing source masks now consume the retained Uber identity.

Controller ownership persists off the rail. Only control12 gains the additional airborne/grounded host paths: airstream physics/landing, original ground score boundary retaining active-Uber state, no premature control5 landing-exit bake, and no duplicate boost/control triplet ticks. Crash/reset clears rail controller ownership even when the Uber rider was already airborne. Ordinary rail touchdown behavior remains at its previous phase; the regression suite initially caught an overbroad change here, which was corrected. Full original host scheduling/visual parity is still not established by these bindings.

Gameplay integration tests enter all four choices through actual rail attachment and shoulder input. The original text formatter produces Handstand, Edge Grind, Butt Stand and Foot Surf names; entry/cycle/balance/out-of semantics and exactly one completion/tier increment are checked. Tests grant a meter award through originalBoostAward at a defined boundary to establish Tricky; this is not a new proof of earning a full meter through an entire unassisted run. Without Tricky, the held action notifies once and does not enter; multi-shoulder chords remain invalid. Holding Handstand beyond the rail exercises eight off-rail Uber frames and a source-classified crash, with ownership released and no completed-Uber credit.

Validation: full npm suite; original20,000 entry gates;32 entry variants and3,000 controller12 episodes;20,000 kind10 seeks;20,000 rail-Uber scoring boundaries (plus ordinary boundary checks); native rail callback-order regression. Native/browser trace now includes four Uber scenarios plus held-edge crash/reset:9,030frames,22scenarios,616fields all agree. This exposed two raw std::sqrt calls in reset_placement.cpp; both now use portable terrain_original::sqrt, preserving native source chop behavior on WASM. The20,000 original reset placement/basis oracle cases still pass. Production rebuilt; normal menu/Sam/Snow Jam rendering inspected in the in-app browser. No new continuous PCSX2 framebuffer comparison of Uber grinds has been performed.

Remaining: full original game/visual fidelity, detailed trigger/phase coverage, dynamic entities and opponent collisions, earned progression across complete runs, original audiovisual feedback, multirow trick HUD, boost FX geometry, materials/lighting/background rendering and broader rail capture/performance coverage. This supersedes earlier notes that browser control12 was disabled.
