# Original forced-reset recovery

Current status: the browser now uses the recovered reset route, placement and timed controller to reset on course, retaining race time and banked score. Backspace or controller Select/Back requests manual reset. The source-derived white reset fade is connected. Remaining device feedback, full statistics and exact whole-frame initialization remain incomplete. See the latest integration section in browser-demo.md; the sections below retain the source audit and staged implementation history.

## Recovered route selection and sampling

`engine/reset_route.hpp/.cpp` implements full112D58 selection/refresh through `originalResetRoute`, plus269F18 event-aware sampling through `originalResetPathSample`.

112D58 differs from the existing112A50 ordinary human/NPC route selector:

- Calls26AF98, which supplies requireField3C=true to26AFB8, requesting at most6 candidates. It uses current position, with no velocity lookahead.
- Projects each candidate with26A428. The caller's6C0 virtual+40 result determines whether to reject a projection beyond pathLength−200cm.
- Scores squared distances to both the projection and the event-adjusted sample at projectionDistance+796cm. A lower score wins; selecting the retained path cancels a pending switch.
- Checks the **old** AB8 pointer before installing the selected candidate. A missing retained route remains missing.
- With a retained path, it invalidates the ABC cache and refreshes projection/path samples even if the winner did not change.
-269F18 updates the distance passed by reference. Consequently490 is sampled at an adjusted4C4,4A0 uses that adjusted distance+796 with another event-aware sample, and4C0 receives the adjusted4C4.

269F18 starts two candidate distances at input+50cm. For intervals inclusively containing the original input distance, event16 extends the alternate to the interval end, event12 lowers the selected distance to the interval start, and event14 raises it to the interval end. If selected equals the original input, the alternate is used. It writes the final distance back and samples26AB20. Using plain `originalRacePathSample` here loses both interval handling and the retained-distance mutation.

Validation: `python3 tools/test_reset_route_native.py` runs the original112D58 and its actual candidate/projection/sample callees against the native implementation.10,000 cases match exact selected path, distance fields, closest/lookahead points and cache:7,403 switches and9,411 refreshes. Cases include missing retained path, unchanged winner, end permission,field3C filtering and event intervals.23 additional samples test overlapping12/14/16 intervals at and around boundaries, negative distance and past-path-end behavior. All match exactly. The reference uses the captured chop rounding mode and the existing corrected EE scalar oracle; original generated source files are untouched. Log: `local/reset-route-reference.log`.

## Actual Snow Jam path data required

The glide snapshot's reset/NPC table at4D33A8/4D33AC has129 records (64-byte stride),117 with nonzero field3C. Human rider014701A0 retains route index2 at014607C0; captured4C4 is2990.40087890625cm. This is **not** the8-path race-course table currently bundled in the browser's race configuration.

`tools/reference_npc.py:extract_npcs` already decodes this AI-path table, including origin/bounds/segments/events,flags38 andfield3c, and participant route state. The current browser start package does not include it. Export the correctly hashed reset/NPC table and retained human route/cache when wiring reset placement; do not substitute race-course paths or choose a nearest floor point.

## Remaining reset lifecycle

-116120: only proceeds when requested or reason is nonzero; clears rider2E8/2EC; reasons1/4 call11A088; owner350 records whether reason>0; enters control9 then motion3; reports29A220 and270970 observers.
- Control9 update12F398: approaches stored progress toward1 by captured0.02500000223517418 × rider timeScale. Crossing0.5 calls112D58, then26A8B8 to obtain the route direction, then11D660 for actual safe placement. Vertical clearance is200cm, or1000cm for the source event-variant branch. It requests semantic287, calls11DF18, applies119368/10E098 manager feedback, and at progress1 restores playback rate1 and enters control4/motion1.12F588 gates device-specific completion work. Constructor/entry initialization and all host callbacks still need recovery/integration.
- Motion3 update136958 calls11E098 to rebuild orientation;136978 is a no-op. It is not ordinary riding/crash integration while reset control owns the rider.
-11D660 is placement and physical/control initialization (457 decoded instructions). Its placement/basis prefix through11DACC is now recovered and verified below. It uses one vertical world probe, not an iterative search; the earlier search/radius interpretation was incorrect. The tail from11DAD0 remains to be mapped into real state/animation/body/camera callbacks.
-26A8B8 obtains the path segment direction at a distance, distinct from position sampling. Source uses cumulative segment lengths and the last segment fallback.
- Full score/statistics, invulnerability/device observers, fade/presentation and source reset-position lifecycle remain unconnected.

Next implementation: map11D660’s remaining state-clearing/animation/body/camera tail and control9’s entry/fade callbacks, bundle the actual reset-route data, and replace the browser spawn-reset branch with control9/motion3. Verify route progress survives reset, original287/passive-air entry occurs, and detached crash/phase4 routes complete. Ordinary crash/get-up browser behavior remains as documented in `browser-demo.md`.


## Verified placement and timed control (September12)

`engine/reset_placement.hpp/.cpp` implements11D660 through11DACC. For nonnegative clearance, it constructs one segment from target−7000cm world-Z to target+200cm world-Z, with constructor flags2 and preferred fraction0.9791666865348816, and calls336850. Constructor flags2 must not be confused with world-query mode2:336850 includes terrain. A valid result supplies position/normal; a miss retains the requested point and world-up normal. Positive/zero clearance is added vertically afterward. Negative clearance bypasses both the query and the offset. Heading comes from the input direction’s horizontal components; source heading/slope quaternions and11E098 rebuild produce the physical frame, then source tangent projection/normalization produces3A0/3B0.

`python3 tools/test_reset_placement_native.py` compares20,000 cases exactly against the executable prefix, including every probe argument, heading quadrants, hit/miss, clearance bypass, position, quaternion and projected tangents. This test stops original execution at11DAD0 and substitutes controlled336850 hits; it does not verify the coupled world query or the later reset callbacks. It confirms the original clears both actor contact caches before querying; the native placement helper leaves cache ownership to its caller. Log:local/reset-placement-reference.log.

`engine/reset_control.hpp` implements complete12F398 control-step ordering with explicit host callbacks. Its20,000-case executable comparison verifies progress,0.5 crossings,200/1000cm clearances, reset-score/boost calls, device completion, animation-rate restoration and control4/motion1 handoff. A sequential zero-to-completion trace also matches every intermediate value: placement at tick21, completion at tick41 for timeScale1. Script:tools/test_reset_control_native.py; log:local/reset-control-reference.log. Callback implementations remain the browser integration’s responsibility.

Entry12F230 sets progress0 and freezes main-channel rate0. If12F588 permits the device, it constructs the source fade effect using white color values and a binding to the progress field; this is not yet rendered in the browser. The helper covers update, not this effect allocator/entry lifecycle.

11D660 tail audit: after the verified basis prefix, it clears velocity, depth/contact state, the19 filter triplets at1F0..2D0 (25C current/rate become1), spin/flip/boost/style/manual fields and several body timers; sets timeScale and leg weight1; restores320 from324; clears animation channels; restores default mirror/root from stance; plays requested semantic (287 for reset); samples local/world pose; commits body geometry/bounds and calls camera/player observers. Those real operations must be connected rather than treating the placement transform alone as a completed reset.

At this recovery stage the helpers were not yet connected. They are now used by the browser on-course reset; see browser-demo.md’s September12 integration section for tests and remaining limitations.

The browser now renders the reset-specialized white fade derived from12F230/2E48AC/2EBB10. See the latest browser-demo.md section for20,000 opacity-reference cases and live viewport verification. The fade uses the controller progress directly and is not a separate animation timer.

Normal-frame human route progress is now connected after course progress through originalNpcRouteProgress1125C0, including crash/reset frames. The retained path/cache/heading no longer remain at their capture values between resets. See browser-demo.md’s latest route-progress section for junction, reset and restart verification.
