# Event demo recovery

Current user requirements include original playable riders/opponents, correct race start/finish/timing, and loading the appropriate event/free-roam/challenge variant. These requirements remain active and incomplete.

## Verified current gaps

- Browser init_race in web/race_bridge.cpp filters all non-human participants. The captured original_race_event package contains six participants; five are discarded. The browser draws and simulates one Sam rider. Existing OriginalRaceSession handles one human; NPC scheduling/rendering must be connected, not represented by prerecorded ghost paths.
- The current race seed is already phase5 (Race), totalTicks338 and raceTicks158. reset_race recreates that captured session, so the displayed timer starts2.633seconds in. The spawn is also captured riding state, not established as an authored race grid.
- Original clock code already knows PreRace3 (entry clears raceTicks), Countdown4 (180ticks), Race5 and EndRace6. Source start-grid placement, input/motion ownership and start animations must accompany the phase setup; merely zeroing the UI timer is insufficient.
- race_end exports a finish-event pulse and requestResults, while main.js latches a FINISH caption. Complete finish/result behavior and other riders' results are not integrated.
- import_world.py selects all resources in the ARA1 location's chunks25..33 plus shared chunk0. It has no explicit event/free-roam/challenge selector. This proves missing selection plumbing, not which specific rendered objects belong to the wrong variant. Decode the original activation rules before removing objects based on names or guessing that SDB spatial subchunks are event variants.
- Captured event provenance has configuration_bytes[0,1,0,0]; browser init_race does not load these into OriginalRaceEventAsset.configuration. Original AIP routes and event callbacks are available. Determine the runtime meaning of the configuration and its resource activation before choosing the event package.

## Terrain UV correction applied

Original2EDB20 samples base UV corners in order(0,0),(0,1),(1,0),(1,1). The mesh importer had the middle corners transposed, while geometry and collision use the same u+4*v power basis as that sampler. tools/world_assets.py now uses patch_texture_uv with the original order; future imports mark terrain_uv_basis_version2.

tools/repair_terrain_uv.py validates owned-source hash, patch provenance, exact vertex positions and existing old/new UV values, then optionally repairs only base UV bytes. Applied to native and browser ARA1:1,913patches,137,736changed vertices. Position, normal and lightmap-UV bytes remain identical; indices, collision and texture images were not rewritten. The exact overwritten UV components are retained in local/browser-validation/terrain-uv-before.json and the audit is terrain-uv-repair.json. Off-diagonal corner tests pass;20,000 source environment patch cases still validate the original mapping. Production rebuilt and served UV basis2 verified.

This addresses a concrete texture-axis mismatch. Full material/lighting, all authored variant combinations and visual framebuffer parity remain unverified.

## Verified countdown grid and start controller

The existing countdown-1.p2m2_SaveState.p2s is an original six-rider Countdown fixture: totalTicks18, raceTicks0, countdownTicks162; all riders are control6/motion3 and stationary. Files named countdown-161/162/163 have the same logical clock state, so filenames are not evidence of advancing simulation. intro-anchor is already Race/tick369 and is unsuitable as a fresh start.

Re-ran audit_rider_assemblies.py against the countdown snapshot. Exact owned-model headers and bind rigs identify visual Zoe, Psymon, Allegra, Moby, Griff and Luther. Gameplay attribute IDs remain separate from visual identity (the original audit includes differing/shared gameplay IDs); do not infer profiles from model names. tools/export_event_start.py verifies that assembly audit's exact EE hash, matches the existing event-course hash and selected roster order, requires stationary control6/motion3, and exports all six source profiles, grid positions/orientations, start-delayB30 and retained start-controller state to local/assets/native/ARA1/event-start.json. This is an18-tick captured grid, not a complete new-race initializer or recorded motion replay.

engine/start_control.hpp recovers12BF68 dispatch and12C230/12C408/12C0C0/12C130, with12C028/12C078 release gates. The control lives at owner+290, not+250. State contains phase, steady time, low/high and pose. Source input is signed6-bit word0 bits12..17. Phase0 lean updates, timing-based push-off calculation, phase1 crouch/velocity/motion handoff, and phase2/3 manual-start animation rules emit ordered actions. Attribute tilt115AB0 remains a supplied external boundary; its source expression is75-90*resolved attribute. Source control/motion/animation callbacks remain caller-owned. The helper models the ordinary successful Motion0 handoff; callback-induced alternate motion changes need live host handling.

20,000 calls through the actual original controller and phase functions match state bytes, pose targets, release gates, launch velocity and callback order (local/start-control-reference.log). Comparison corrected a misread branch: the timed push-off formula applies while steadyTime<0.4, not>=0.4. Calls also verify boost is stopped during this controller.

Next: recover control6 entry12BE20 and semantic0's kind8 start-pose animation driver, initialize from real entry state rather than copying18 advanced lean ticks, connect the real roster and phase-based input/motion ownership, then implement finish/results and event resource activation. Do not merely zero the captured timer or publish the countdown fixture as a fully working race start. Browser runtime remains unchanged in this recovery pass.

## Start entry and pose driver connected to shared graph

originalStartControlEnter models12BE20 entry: copy reference stance to active/animation mirror, clear the animation graph, reset root translation, request root half-angle-pi/2 for switch or negative zero otherwise, choose semantic1 for incoming phase2 or semantic0 for other phases, and clear all four phase values.20,000 entry cases match original state/ordered reset-trig-play boundaries; trigonometry is a controlled callback in that entry oracle, not a new sine/cosine implementation.

Driver dispatch456950[8] points to1037E0, which calls1045D8. That kind8 start driver seeks slot0 time=duration*owner2A0 and advances fades only. originalAnimationStartStep now implements it and the common native/browser graph exposes startPose.20,000 copied-driver cases compare seeks, fade/removal and completion preservation; browser probe verifies pose0/.1/.31/.62/1/1.2 does not advance with elapsed animation time. Semantic0's owned lookup is leaf0/clip8192; the clip was already present. Added that variant to existing private packages and the exporter, with no recorded pose arrays.

The start-control oracle now uses the actual countdown fixture and also verifies a fresh entry plus18 neutral steps reproduces all captured human control6 state bytes exactly. This connects the decoded entry/update to the retained snapshot without initializing from its advanced lean value. Original-countdown screenshot was extracted to local/browser-validation/original-countdown.png: timer0, three-rider foreground start gates, blue3 and the original race HUD.

Full browser suite and9,030-frame native/browser comparison pass. Shared graph/metadata rebuilt, but interactive race startup still uses the old captured run: event host wiring, countdown HUD, grid placement, per-rider start control, opponents, finish/results and variant activation remain pending. The new graph startPose must be updated by each rider's control6 state when that host work is connected.

## Human event start connected in the browser

Normal Snow Jam starts now call start_event, rather than entering the captured mid-race session. tools/generate_event_seed.py compiles the verified countdown human's ground profile/state, progress seed, delay and original4D8 progress origin; generate-controllers.py includes this step. The interactive player is still Sam using the audited Zoe gameplay seed. This does not add the other five riders yet.

begin_event_clock runs the recovered PreRace entry and selects Countdown. The race timer remains0 through180 countdown updates and the deferred Race transition. Control6 starts from originalStartControlEnter's fresh values; it drives the kind8 pose, stops boost, controls crouch and applies original launch velocity before relinquishing motion/control. A motion3 start holds physical position. The camera receives mode3 while held. start_input supplies quantized GateAnticipate (mapped action26,127DFC..127E04) from W/S, arrow keys, gamepad vertical stick or D-pad. Crouch state is retained into normal riding rather than reverting the browser's separate charge variable to0.

Both neutral and forward input are exercised end-to-end: first Race phase at test tick180; neutral releases motion at201 and normal control at227; forward releases at181 and normal control at206. Riders travel55/60m from the grid in the420-tick fixtures. Restart resets race time and the held grid. The pose uses original clip0 and the HUD uses original OV_1-3 numeral images. Countdown tint/animation, GO presentation, intro camera, announcer/audio and original full HUD timing remain to be recovered; do not claim pixel-perfect countdown UI.

The first visual run showed5% at the grid. Source10F500..10F50C initializes rider4D8 from rider4D0 (the113128 getter); in this snapshot both are353496.15625cm. The old browser instead normalized by paths.front().remainingAtOrigin372267cm. The exporter now retains progress_origin from4D8 and the event uses it. The gauge is verified0% through countdown. Legacy railTest/pickupTest URLs deliberately retain their old captured test start to preserve independent QA fixtures.

Full browser suite and9,630-frame634-field native/browser comparison pass, including a fresh event-start scenario. Original channel progress312AB0 divides elapsed by duration with scalar DIV.S nearest; the shared graph now uses originalScalarDivide directly, removing the old generated VU-div substitution. Browser screenshots verify0MPH/00:00:00/0% at the real start gate and subsequent riding with a running event timer. Build is published locally.

Still incomplete: original selectable riders and opponents, per-rider runtime contexts, start-grid NPC motion/input, exact full zero-tick physical/visual initialization (the ground profile still comes from the18-tick held capture), finish/results behavior, event/free-roam/challenge resource activation, original countdown/intro/audio presentation and broad fidelity/performance. The world importer still includes the entire location; starting the event does not prove correct variant filtering.

## Basic human finish/results flow connected

race_result_info exposes persistent finish status, original finishTicks, penaltyTicks, elapsed-since-finish, current raceTicks and phase. The frontend now reacts to the recovered requestResults boundary, preserves the final camera/rider pose, stops further local simulation callbacks, and displays a Single Event Results panel with Restart/Quit. Rank is deliberately unassigned because opponents are not simulated; no victory/loss is fabricated. Replay/Records/Next event remain disabled.

The panel follows the owned local/sam-ps2/roster/sam-first-race-results.png layout and uses existing font/atlas assets. It is not a port of the original frontend layout program or post-finish camera/celebration timing. These remain fidelity gaps. An initial atlas crop included unrelated icons; visual review corrected the crop to the forest strip.

race-time.mjs formats integer60Hz ticks, avoiding float-second centisecond underflow (tick42 is00:00:70). Results show minutes/seconds while retaining the full tick value. Dynamic penalty producers and ranked multi-rider results remain separate; the test supplies120 penalty ticks and verifies the original finish formula retains them.

web/test-race-finish.mjs traverses the actual authored paths3/4/5/6/7 kinematically (not snowboard physics or AI), reaches the genuine type1 finish event exactly once, receives one results request, and verifies immutability for120 further ticks, stopped EndRace clock and restart clearing. It completed7016 traversal ticks, storing7293 finish ticks including120 penalty ticks. The ?finishTest=1 browser fixture starts250cm before the authored finish interval and crosses it with real riding movement; it never injects a finish event. Browser inspection confirms results open and Quit returns to Main Menu. Restart was clicked and results reappeared rapidly because this fixture is near the finish; no separate intermediate riding screenshot was captured for that button. Core restart clearing is separately asserted.

Full npm suite and9,630-frame native/browser regression pass; production rebuilt. This provides a basic single-human finish flow. Original selectable riders/opponents, real standings, records/replay, exact results frontend/celebration flow, full event-only resource activation and broad visual/physics fidelity remain unfinished.
