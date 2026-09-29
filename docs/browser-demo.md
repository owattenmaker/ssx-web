# SSX3 browser continuation — September 12, 2026

Current update: original airborne trajectory/animation, posed-board landing, soft/hard crashes and ordinary get-up recovery are connected. Forced reset now uses the original129-path bank and timed control9/motion3 placement, preserving race time and banked score. Backspace or controller Select/Back resets on course; R restarts the run. The source-derived white reset fade is connected; full-game fidelity, remaining feedback and full frame parity remain unfinished.

The user explicitly rejected an invented “Sam's Snow Day” presentation and simplified feel. Going forward, preserve SSX3's UI, animation rules and physics. Do not add original branding, challenge modes, automatic steering or arbitrary animation selection. `web/` is the active browser experiment; native Metal and GameCube work remain preserved.

## Launch / build

The running packaged demo is at http://127.0.0.1:4173. Development is at http://127.0.0.1:5173. A launcher is in Downloads: `Play Sam Browser Demo.command`. These are local URLs, not a public deployment.

```
cd web
npm ci
npm run setup   # private asset import, original UI/font extraction, WASM compile
npm test
npm run build
npm run preview
```

Uses Emscripten 6.0.9 under `local/vendor/emsdk` and the existing private PS2 ISO/native assets. UI extraction additionally uses `local/dotnet/dotnet` + the existing SSX texture library. `package-lock.json` pins JS dependencies. `web/runtime`, `web/generated`, `web/public/assets`, `web/dist` are generated/ignored. No user's disc or save was modified for this browser work. The PS2 meme replay cheats are unrelated to this browser build.

Vite HMR is disabled: manually reload after edits. Never leave a second generated WASM copy under `public/runtime`: it can shadow the compiled source module and produce stale/mismatched JS/WASM. Earlier development error logs include this now-resolved mismatch.

## What now runs from recovered original code

- **Physics:** original ground forces/integration, crouch/brake/turn filtering, heading response, jump charge/takeoff, airborne drag/gravity and total speed cap. The old invented acceleration and 40m/s horizontal-only limit were replaced. The cap is recalculated at frame begin from original surface, crouch, boost and motion-state rules. Airborne mode uses ~33.3333m/s; grounded limits smooth against the preceding frame, including after material changes. The captured ~22.18595m/s value is not a universal cap. Source control/physics runs at60Hz, independent of rendering.
- **Animations:** all497 gameplay clips decoded through the original packet decoder, plus the67 frontend clips in the sampled bank. The browser no longer uses an eight-clip heuristic for gameplay.
- `generate-controllers.py` takes the original native sequence implementation directly from `engine/rider_animation_player.mm`: enter/variant selection, channel blending/fades, five-way/three-way cycles, clocks, event markers and completion callbacks. It does not copy macOS file/UI glue.
- Original ground selector; prewind targets/snapping and release selector; air spin/flip/adjust controller and selector; passive-air control4 and exit to5; all15 shoulder chords; normal/tweak grab lifecycle; landing anticipation and ordinary landing clip choice; upper-body idle reactions.
- Original local pose mixing, body-root presentation, board alignment and two-link leg contact. Browser converts actor-space world transforms back into hierarchy locals for Three's GPU skinning. Root/asset coordinate conversion is important: source +Y forward becomes native **-Z**, not +Z. The initial browser version faced Sam backward; this was corrected against the source quaternion columns.
- Default SSX3 chase camera (`engine/original_camera.hpp`): original distance/FOV/filter stages and terrain-clearance compositor using the original authored-patch segment query. Camera is advanced once per fixed60Hz tick and render-interpolated. No added follow-camera smoothing controls game motion.
- Original grab begin/hold/end point rules and authored hold thresholds. Race clock, path projection and checkpoint/finish event arithmetic use `OriginalRaceSession`. One actual human participant is configured; no opponents are fabricated.

## UI

Original PS2 FE/OV atlas pixels, logo, orange3 artwork, controller-button sprites and bitmap fonts are extracted from SHPS/FNTS files. SFN glyph metrics are read from the original font tables; font mask alpha comes from its luminance. `ui.js` reconstructs the title, limited menu flow, Sam selection, setup/event selection, HUD and pause display in640×448 coordinates presented at4:3.

This is **not** a full LUI interpreter or pixel-perfect completion of all original menus. Only Single Event → Sam → Snow Jam is active. Unimplemented modes are disabled. The previous invented title, fonts, one-minute challenge/results and course guide are removed. Menu preview uses an original frontend idle clip.

Controls: A/D or left/right steer and prewind; W/S or up/down flip input in air; S/down brakes on ground. Space = hold/release jump. Q/Z/E/X = L1/L2/R1/R2 (all15 combinations). Shift/Square = tweak request in air and boost input. Enter/Start or Escape pauses; R restarts. Touch exposes equivalent buttons. Physical phone/gamepad testing remains outstanding.

## Verified

`npm test` executes the actual generated WASM:

- All497 gameplay clips sample at start/middle/end with finite transforms on Sam.
- All15 normal grab chords and all15 tweak transitions enter their original states.
- Six prewind/spin/flip directions sample finite poses.
- Held-jump ledge departure retains original crouch/prewind control2, then enters control5 on release. Uncharged cruise departure uses passive control4.
- The WASM fixture gives identical600-tick movement/jump outputs at30/60/120/144/240Hz. A subsequent live test exposed a separate main.js bug: a50ms frame-time cap discarded elapsed time on slower renders, causing slowdown (not2× speed). The actual browser scheduler now retains elapsed time, processes at most12 physics ticks per callback, and carries any remaining debt. Its imported scheduler regression covers5–240Hz and uneven frames. Pause/resume resets the render timestamp to exclude paused wall time.
- Imported-course jump lands; velocity stays within each tick’s original dynamic cap. A dedicated regression checks the airborne cap and gradual grounded convergence after landing (36 airborne ticks, landing at tick186 in the fixture).
- Original chase camera remains finite and uses its original~0.75391rad FOV.
- Original race clock advances the expected ticks and projects course progress.
- Empty-meter boost regression compares600-tick ground/jump traces with and without holding Shift: identical movement, zero amount, preserved press feedback and reset behavior. Existing15 tweak transitions still pass.
- Native camera CTest passes after the browser-only rounding adaptation.

A separate comparison compiled the same curved-contact adapter with the original native chop-rounding helpers versus WASM nearest rounding: over600ticks with authored curved contact, maximum position difference~0.0754m, maximum speed difference~0.425m/s, zero grounded-state mismatches. See `local/browser-validation/native-vs-wasm.json`. This compares the adapter across targets, **not** complete original-game/PCSX2 parity.

## Important remaining gaps

Do not claim “every animation trigger” or “faithful complete SSX3” is finished merely because all clips are loaded.

- Hard crash/ragdoll/recovery, rails/rail tricks/transfers, handplants, attacks and rider interactions are not yet connected in this browser adapter. Their clips/state descriptors exist; world/controller triggers still need to be ported from the native implementation or recovered original code.
- Browser boost now calls original `114130` control and `1200D0` meter/timers using the captured `original_boost` profile/state. Ground jump-entry/release skip dispatch; airborne input stops thrust and remains available for tweaks. Physics and the attached animation graph share the resulting amount. The captured meter starts empty; full special landing/event handling (`119D40`/`10E910`), controller-phase parity, feedback playback and HUD meter animation still need integration. Ordinary trick landings now award earned meter through the original identity/named-bonus/repeat calculation and10E098. Full special-event/statistics parity and original HUD meter animation remain unfinished.
- Uber/super-Uber definitions and transition code are included, but earned boost progression/super-time activation is incomplete. Do not invent rewards or pretend these activate during normal play. Complete boost economy and combo commit remain work.
- Ground contact now uses authored curved patches and their surface parameters on grounded ticks with triangle fallback/swept landing. It is still a browser adapter, not the entire native analytic-contact/controller phase pipeline. Authored rigid-world/body collision, complete orientation/stance baking after aerial rotations, and original crash classification remain work.
- Procedural pose uses actor-space contact inputs; full native world-pose/contact timing and source collision rig integration still need parity validation. Camera launch/proximity/predicted-air fields and shared visual RNG are not all supplied.
- Snow spray/tracks remain simple demo implementations. Original snow/track systems, music/audio, NPC races, career, saves, UI scripting and full frontend triggers are unfinished.
- WASM only supplies nearest rounding. Generated adapters explicitly omit unsupported host-rounding switches; `terrain_contact_math.hpp` has a browser-only nearest-rounding scope. Native behavior is unchanged. Operation order is retained, but native PS2 chop-mode bit parity is not claimed.

## Integration map / next work

`fixed-step-clock.js`: live60Hz scheduler with bounded catch-up and retained time debt; `test-frame-clock.mjs` tests the actual imported scheduler.
`core.cpp`: recovered movement/jump/air/heading + authored curved contact/swept landing, source chase camera.
`animation_bridge.cpp`: original state inputs, grab/air/passive selectors, original graph, pose/contact and grab points.
`animation_graph.hpp` / generated graph: original sequence ownership and completion code.
`race_bridge.cpp`: JSON adapter for original single-human race session.
`prepare.py`: world/rider/sky assets and spatial batching.
`prepare-ui.py`: original UI/font data, full clip libraries and source scoring thresholds.
`ui.js`: source-asset browser UI; still a limited reconstruction of LUI behavior.

Next: replace the remaining adapter with the native `PrototypeRider` phase pipeline and original `GameplayAnimation`/world-pose callbacks, then connect crash/rail/handplant/attack controllers. Keep source-derived rules and an explicit trigger-coverage report. Do not restore the invented guide or a clip-name heuristic as a shortcut.

Final integrated browser check: original menu flow entered Snow Jam; keyboard jump/grab input earned1530points; no new Runtime.exceptionThrown events during that run. Pause/resume and quit-to-title exercised. Authored camera query clearance stayed positive in a30-second no-steering module check (minimum~0.38m). This is a bounded check, not full-course collision fidelity.

September11 packaged live timing check after scheduler fix:22.9167 simulation seconds over22.9511 wall seconds; pending debt0.00697s; zero rescues and no new Runtime.exceptionThrown events. Observed reported render FPS12–13: simulation speed is restored, rendering smoothness is NOT solved. Evidence: `local/browser-validation/live-timing.json`. Preview server was restarted after confirming port4173 had no listener.

September11 CPU sampling: the paused scene reported~14FPS with one triangle, and the sampled profile was dominated by idle time (~19.45s). This suggests browser/compositor pacing is involved; it does not prove that the game renderer itself costs70–80ms/frame. Do not degrade source visuals on the basis of the FPS counter alone. Current low-FPS numbers remain session observations, not established GPU throughput limits.

September11 award recovery: `engine/boost_award.cpp` now implements10E098, verified against30,000 original-instruction cases. It is not yet connected to browser landing awards. `engine/BOOST_RECOVERY.md` records the discovered11A228/11A8C8/1190F0 repeat-penalty dependency and the next integration steps.

September11 repeat-history recovery: complete1190F0 is implemented in `engine/trick_history.cpp` and verified against30,000 original-instruction cases (count, entire history and cursor), plus saturation/eviction sequences. This is still a dependency for landing integration, not a claim that browser boost rewards work. The source identity builder11A8C8 and commit path11A228 remain next.

September11 identity recovery: complete11A8C8 is implemented in `engine/trick_identity.cpp` and verified against40,000 original-instruction cases (both packed words, validity, spin/flip mutations). Authored rotation tables and state have an extractor. Landing commit11A228 and reset119D40/117838 now remain before integrating these recovered helpers into gameplay. Browser behavior is unchanged by this dependency work.

September11 landing work: the original24-row named-combination matcher11B1A8 passed30,000 instruction-oracle comparisons. Browser landings now use117838-compatible reset for the represented grab-score fields, preserving A4 combo timeout and1C4 multiplier instead of resetting them. The reset passed20,000 original-state comparisons. Full landing reward commit and earned boost are still unfinished.


### Current ordinary landing integration (September 11)

`engine/trick_commit.cpp` composes11A8C8 identity,11B1A8 named bonus,1190F0 repeat history,117990 double rounding and11A440 meter division for ordinary single-event landings. The complete original11A228 call (external notification callbacks isolated) was used as an oracle for6,000 cases. Meter delta, banked points, normalized score, history bytes and cursor matched. The oracle also exercises119898/1198D8 rotation-score updates. This is an ordinary-award comparison, not validation of every statistic/event side effect or special game mode.

Browser `animation_bridge.cpp` now feeds the original air controller's scoredSpin/scoredFlip into rotation scoring, finishes a held grab on landing, commits the ordinary award, applies10E098 to the captured boost eligibility mask, and retains repeat history across landings. Restart clears that history. Earned tier/super-time feed the grab context. Source tables/constants are imported by prepare-ui.py from the owned reference capture. The old raw pending-points bank on landing is replaced.

`npm test` now includes an actual-WASM imported-course jump/grab/landing/boost run: one landing earns~0.0213333 meter; holding boost after landing uses12 ticks and reaches26.1078m/s in that fixture. No meter is injected. Source active drain can briefly cross below zero by less than one drain step; the regression respects that original rule rather than clamping gameplay differently. Evidence: `local/browser-validation/earned-boost.json`. Public QA diagnostics expose meter/amount/tier.

Still missing: full11A228 race statistics, combo lifecycle/event dispatch and special modes; complete119D40 state restoration;10E910 Uber-count tier advancement; source stance/orientation updates for every landing; crash/rail integration and original HUD meter rendering. Those remain fidelity gaps despite ordinary earned boost now working.

Packaged live check after ordinary-award integration: controlled jump/grab run ended at5.48s with240 banked points, grounded, zero rescues, and paused. The timed CDP sampling return expired, but subsequent live inspection confirmed the inputs completed and were released. Peak earned/used meter is verified in the separate actual-WASM fixture. See `local/browser-validation/live-earned-boost.json`.


### Uber landing progression (September 11)

`originalLandingUberProgression` implements10E9B4..10E9F4.20,000 calls through the original10E910 wrapper match tier/timer outputs with unrelated callbacks isolated. The ordinary commit now returns committedUbers only after the original positive-score gate; its6,000-case oracle additionally compares original cumulative Uber count114 against this returned increment. Browser landing applies this increment before10E098, then synchronizes the physics tier.

`test-uber-animations.mjs` supplies explicit captured-state fixtures to the animation-only API for tiers1,5,10. These fixtures do not inject meter into production gameplay. Production continues to use physics-owned earned state. The test checks all15 chords against each tier's authored semantic, including original tweak fallback where an Uber is unavailable, and verifies finite poses.

These changes supersede earlier notes listing10E910's Uber-count tier advancement as missing. Full progression UI/audio, special landing branches, shared statistics, crash/rail/handplant triggers and a complete naturally earned gameplay run through the top tier remain unverified or unfinished.


### Motion-to-animation handoff fixed (September 11)

`core.cpp` now publishes resolved position, velocity, normal and heading basis after airborne integration and landing projection. Previously physicsState retained its last grounded integrator values during flight, so attached animation sampled stale speed/vertical velocity. `animation_bridge.cpp` now copies ground control filters on ground/air entry only; passive-air filters retain ownership on later flight ticks instead of being overwritten each tick. Raw animation-only fixtures still accept their explicit controls.

`test-earned-boost.mjs` checks every tick of its two600-tick runs: sampled animation speed, vertical velocity and contact normal must match resolved rider outputs. The main WASM suite additionally isolates passive-air filter ownership with a fixed ground snapshot and verifies turn converges to zero. All existing tests and the production build passed. `animation_inputs` is a read-only diagnostic export for those checks.

This corrects stale data within the current browser adapter; it does not establish equivalence to every original pre-motion/post-motion phase or solve the remaining world-pose, crash, rail, stance and contact-pipeline gaps.


### Start and restart tick alignment (September 11)

`reset_rider` now publishes its resolved initial state immediately; `rider_state` returns that snapshot without simulation. Browser resetPhysics uses it instead of calling step_rider. Previously Sam moved one physics tick before race/animation clocks began. Reset also initializes the contact normal and clears the impact output so a missing start contact cannot inherit the prior run's normal/landing data.

`test-rider-reset.mjs` checks exact captured X/Z, zero charge/airtime/distance/impact, repeated read-only snapshots, and identical first real ticks after several interleaved runs. The captured initial velocity remains intact; this does not replace it with a stationary start or change the original force constants.


### Original ground-entry landing response (September 11)

The browser now calls originalLandingGroundLeave on charged takeoff and passive ledge departure, and originalLandingGroundEnter after a resolved landing. Motion ticks and last-ground-leave ticks start from the captured landing runtime. All19 landing materials, bodyScale and landingStat are generated from the same capture. This restores source depth/bounce initialization and the source airtime-dependent velocity multiplier (0.7 through40 airborne ticks, increasing to1 by70 ticks). The previous adapter omitted this speed loss entirely.

The imported-course short jump verifies a0.69999997 speed ratio across the actual ground-entry call. Existing movement, animation, earned-boost, Uber and reset tests pass. The earned-boost fixture now reaches23.32045m/s rather than26.1078 after landing, due to the restored landing response.

`sh web/compare-native.sh` builds the current adapter with native chop helpers and compares it with WASM. The current600-tick comparison reports maximum position difference0.24602m, speed difference0.38209m/s, and zero grounded-state mismatches. It supersedes older comparison numbers above. This is cross-target adapter evidence, not full PS2 parity.

Still unfinished: the full original board-volume landing query, original contact resolver and recovery-surface routing, crash classification/recovery and exact original phase coupling. Landing detection remains the browser's swept triangle contact; do not mistake the restored ground-entry response for complete collision fidelity.


### Authored airborne terrain intersection (September 11)

Airborne landing probes now call sourceTerrainSegment with coarse-cell selection and curve refinement, using the original patch height, normal and surface ID when available. The adapter retains its existing vertical sweep and triangle fallback; it has not yet gained the original posed board-volume query or recovery-surface handling. Grounded queries are unchanged in this step.

The course landing regression now requires terrainQuery3, verifies the original0.7 short-airtime speed response, and passes with the rest of the WASM suite. Earned boost still works (fixture peak boosted speed23.44883m/s). The refreshed600-tick native/WASM comparison reports max position difference0.05548m, speed difference0.04319m/s, zero grounded-state disagreements. These supersede the preceding comparison values and still do not establish full original-game parity. Production build passed.


### Anticipation height consistency (September 11)

The browser's height_at API and airborne contact now share terrain_down: original coarse-cell/refined patch height where present, with existing triangle fallback. Anticipation previously queried only the mesh even when touchdown resolved against authored curves. The course regression verifies prediction height agrees with touchdown within5mm. All tests and the production build pass.

The render loop also skips its two fallback-camera height queries when the original chase camera supplies the frame; their camera result was immediately overwritten. No FPS improvement is claimed without measurement. The anticipation time formula remains an approximate vertical ballistic estimate; integrating the original adaptive113648 trajectory predictor and full world/board collision is still outstanding.


### Original world-ray geometry loader (September 11)

`web/world_bridge.cpp` adds a JSON adapter for the decoded original world_collision.json. It preserves static type1 triangle meshes and type2 boxes, original hierarchy/scale/matrix composition, bounds, collision flags and surface IDs. Shared mesh resources are cached by(resource, ordinal). Dynamic/unsupported descriptors remain explicit. Static sphere trees retain their descriptors and bounds but no body-tree payload: original32E688 returns no ray hits for these, and the original ray query skips them before unsupported checks. This loader must not be used as a complete sphere-body collision loader.

`sh tools/test_browser_world_loader.sh` compares the adapter against engine/world_collision_asset.mm:3,052 instances,2,860 static nodes,30,551 triangle references match in descriptors/transforms/geometry. JSON is parsed under ordinary host rounding, as in app initialization; source transform helpers scope their own chop rounding. Forcing chop on Foundation JSON parsing itself produced a one-bit input-normal difference in an initial harness run; correcting the harness initialization restored equality.

`test-world-loader.mjs` verifies actual-WASM instance/type counts, rejects a wrong source hash without replacing the loaded world, and verifies lifetime after freeing input JSON. It reports23 unsupported-for-body entries, including15 sphere-tree descriptors; those are not equivalent to23 unsupported ray collisions.

The loader is compiled/exported but not yet invoked by main.js or the original trajectory predictor. Next: package this private collision asset, initialize browserBodies from its matching terrain source hash, connect queryOriginalWorldSegment for modes0/2 to OriginalAirTrajectory, then replace the approximate landing-time formula with verified predictor output. Full dynamic collision and body/board queries remain unfinished.


### Original trajectory predictor connected (September 11)

`prediction_bridge.cpp` owns OriginalAirTrajectory, seeds it on charged and passive air entry, and calls original113648/113200 each airborne tick. Its query is queryOriginalAirTrajectoryWorld, preserving original mode0/2 selection, preferred fraction and coarse-terrain behavior. A candidate is published only after complete queries; unsupported intersecting dynamic resources mark prediction unavailable for that flight, as an explicit diagnostic. Missing collision packages likewise cannot produce a fabricated valid prediction.

`prepare.py` packages the private original world_collision.json; main.js validates its source hash against terrain before initializing the loader. Attached animation now consumes source status/predictedTime/elapsed instead of the vertical ballistic estimate. main.js no longer computes that estimate. Animation-only fixtures retain explicit inputs for conformance testing.

The actual-WASM course jump produced34 valid predicted airborne ticks with zero incomplete queries, and still earned/consumed boost. The missing-world test checks unavailable state. All tests and production build passed. Live browser run:63 samples,8 airborne samples all valid original status1, zero failed queries,250 banked points and zero rescues; inputs released and game paused. Evidence: `local/browser-validation/live-trajectory.json`.

Scope: the predictor feeds animation only; its returned integrated motion is not yet the rider's motion owner. Full original air alignment, controller/motion/pose phase ordering, dynamic collision, board-volume touchdown query, recovery/crash and all original camera trajectory inputs remain unfinished. A valid prediction does not establish full-course or original-game physics parity. Earlier notes saying the predictor is unconnected are historical.


### Trajectory inputs connected to chase camera (September 11)

step_camera now supplies the original trajectory status1/3 gate, predictedAirTime (absolute prediction time, not remaining time), heading and normal from browserTrajectory. It uses motionTick instead of a separate camera-call counter. Ground ticks preserve the prior contact normal; touchdown sets normal and previousNormal together, matching139D78 contact resolution semantics.

The earned-boost course fixture now advances the camera every tick and compares its input status/time/vectors against the predictor, its tick against captured motion timing, and its previous-normal field against the ground/landing rule. Outputs remain finite; all tests and production build pass. camera_inputs is a read-only QA export. Original camera launch/proximity/wall inputs, shared RNG, full phase ordering and visual parity remain outstanding; these checks do not establish a pixel-identical original camera.


### Original trajectory motion ownership (September 11)

advance_prediction now returns the original113648 integrated state to the rider when world queries are complete. The adapter no longer discards that result and separately integrates air again. Incomplete queries leave the candidate unpublished and use one ordinary original air step. Both routes use the source airborne3333.3335cm/s cap, including the first airborne tick; previously that tick could use the cached ground cap. physics_info now distinguishes the frame-begin cached limit(index0) from the actual movement-stage limit(index5).

The course fixture checks airborne position and vertical velocity against the returned integration snapshot, verifies camera/prediction coupling, and still earns/uses boost. All tests and production build passed. The refreshed native comparison now loads matching terrain AND object geometry and reads start.json on both sides:600 ticks, maximum position difference0.06715m, maximum speed difference0.04367m/s, zero grounded or prediction-state disagreements,36 predicted airborne ticks on both builds. It remains an adapter comparison, not full original-game parity.

This supersedes earlier notes that trajectory integrated motion is discarded. Original air-orientation alignment, complete controller/pose phases, stance baking, body/board collision, dynamic callbacks, and crash/rail recovery remain unfinished.


### Airborne rotation-axis blend restored (September 11)

The browser previously called only originalAirPresentationCurrent, which samples without advancing134DD0's axisBlend. OriginalAirControlStep does not advance that field either, so combined spin/flip presentation remained on the initial axis. The bridge now calls originalAirPresentation once on each airborne animation tick; grounded diagnostic presentation remains read-only.

The actual-WASM combined spin/flip fixture confirms axisBlend moves away from its initial value and repeated diagnostic reads do not advance it. Existing tests and production build pass. This restores the original changing rotation axis, not full physical orientation ownership. Original121AA0/139A64 air alignment,134CB0 landing pose bake, pivot/contact timing and stance reversal still need integration.


### Held-jump ledge controller corrected (September 11)

Native controlFrame distinguishes airborne cruise0 (enters passive4) from airborne crouch2 (retains prewind until release). The browser incorrectly sent both through4; an earlier browser regression encoded that incorrect expectation. The bridge now retains control2 and its authored prewind semantic during held ledge flight, releases through originalAirControlRelease/originalAirReleaseAnimation without a second physical impulse, and preserves prewind on soft held touchdown. Source13A2E0 still requests a landing clip for hard impacts below-1388.888916cm/s.

Physics now retains and approaches held jump charge only for flights that departed in held prewind; a new press during ordinary air does not create a charged airborne jump. Release clears that held state. The original course fixture departs at tick1069 and lands at1158, retaining charge across89 airborne ticks with no emergency reset. Replaying the same departure and releasing10 ticks into flight produces identical position/velocity through landing, verifying no second impulse. Soft-touchdown animation and existing suites pass; production build passed.

This supersedes historical notes calling held departure control4. Physical orientation/stance baking and full control-filter phase ownership remain unfinished; no complete landing-orientation claim is made.


### Jump-release playback rate restored (September 11)

Both grounded and held-ledge midair release now share release_air_control. It snaps/initializes original air control, applies originalAirReleaseGroundTargets, and uses originalAirReleaseAnimationRate when prewind is nonzero; zero-prewind jumps retain source rate1. Previously both paths forced rate1. Attached animation receives the physical forward vectors required by release target calculation, and its ordinary-air turn/crouch/brake filters approach those targets.

Actual-WASM fixtures exercise both release paths with zero and combined prewind and inspect the active sequence playback rate. Existing tests, including held-ledge charge/no-extra-impulse behavior, pass; production build passed. This does not complete all control-filter phases, physical orientation baking or crash/rail handlers.


### Landing animation spin conversion (September 11)

originalAirExitRates extracts the unchanged134CB0 rate tail and is called by both the full native air exit and the browser's control5 landing selector. It quantizes spin, restores its direction from progressSpin/targetSpin, and clears flip rate. The native exit oracle still passes20,000 pose/control/rate comparisons (plus existing presentation/angular tests).

The browser previously selected a landing clip from raw spinRate, which is a magnitude in the left-spin fixture. The new actual-WASM regression observes4.70430 raw -> -4.71239 converted and selects semantic66 instead of the incorrect67. It asserts that this fixture actually distinguishes the old and corrected clip choices. Existing suite and production build passed; the sharpened left-spin regression also passes.

This only connects the rate tail to clip selection. Full134CB0 physical pose bake, adjustment control exit, pivot handling, outgoing physical manual-spin ownership, and stance reversal remain unfinished. Once full exit is connected, remove the standalone rate-tail call to avoid quantizing twice.


### Authored airborne pivot and position (September 11)

Animation presentation now uses original_animation.pivot_bone (validated against the skeleton), sampled from the current local pose at the browser's existing unit pose scale. The bridge previously supplied pivot0 and discarded the presentation position. animation_info now adds the source-centimeter offset at16..18; main.js applies that offset through the physical base rotation before applying the air quaternion. The camera head target receives the same air rotation/offset. The renderer's existing effective pose scale is unchanged; matching all original physical/skin scale and world-pose stages remains work.

The combined-spin/flip WASM fixture checks the source pivot invariant and nonzero offset across flight. All tests/build pass. Live browser check:90 samples,12 airborne samples, finite rider/camera positions, maximum air presentation offset1.88828m and zero resets; inputs released and game paused. Evidence: `local/browser-validation/live-pivot.json`.

This is a prerequisite for physical landing pose ownership, not completion of that handoff. Physical pose bake, outgoing manual-spin integration, ground stance reversal and body/board collision still need to consume the same presented pose. Do not wire only the residual rate while ignoring the incoming physical orientation/pivot.


### Consistent render interpolation (September 11)

`rider-frame.js` captures immutable root transforms and parent-local bone transforms after each simulation tick. Rendering interpolates between the last two snapshots using the same accumulator fraction as the original camera. This removes the separate exponential rider-quaternion smoothing, which introduced render-rate-dependent lag while root/pivot positions used current state. Restart and emergency rescue clear both rider and camera history; fallback FX placement follows the displayed rider position. The unused heuristic clip-name expression was removed with the old render block.

`test-rider-frame.mjs` exercises the actual rendering helper: interpolation endpoints, stable repeated renders, the fixture's local bone length, immutable snapshots, reset snapping and a known authored-pivot rotation. Existing suite and production build pass. The packaged renderer entered gameplay with finite rider/camera positions and was left paused. No controlled FPS improvement is claimed. This is render interpolation of simulation samples, not a substitute for original physics, pose baking or collision behavior.


### Physical quaternion ownership established (September 11)

The core now retains the captured physical quaternion, honors the requested reset heading through originalRotateOrientation, and advances ground heading through originalRotateOrientation/originalRebuildOrientation. Ground contact applies originalGroundAlignment using the current probe's measured lateral clearance and retained material alignment rate. Physical forward/board-up are published from originalOrientationBasis; takeoff consumes that board-up. The existing contact probe/snap is still provisional, so this is not the full original ground phase pipeline.

rider_orientation exports the physics-owned source quaternion. Rider-frame capture uses it instead of deriving the base orientation from heading/normal; step_camera likewise transforms the head through the physical basis. The prior simplified render basis remains only an optional helper fallback for standalone fixtures.

Tests cover quaternion normalization during course runs and renderer use of supplied physical orientation; all existing tests/build pass. The native/WASM600-tick comparison reports0.06583m max position error,0.03849m/s max speed error, zero grounded/prediction-state disagreements. Held-ledge fixture now departs1070, lands1158 (88 airborne ticks); no second impulse on release. These supersede prior fixture numbers.

Next: original139A64 airborne alignment with current trajectory/angular inputs, then full134CB0 physical pose bake and original landing classification/stance handoff. Manual landing spin must not be connected to a reconstructed/stale incoming orientation. Full posed board/body collision and crash recovery remain unfinished.


### Original airborne physical alignment connected (September 11)

align_air_orientation now calls originalAirAlignmentStage (139A64..139C68 /121AA0) with the latest trajectory normal/heading/status/time/flags, captured19 surfaceProperty44 values, current physical forward, and current angular adjustSpin/control state. It runs after the angular-control update and before rendering/camera consumption. main.js refreshes its rider snapshot afterward so heading and physical quaternion belong to the same published state. Missing prediction does not fabricate alignment inputs.

The actual-WASM course fixture now observes32 meaningful aligned airborne ticks, verifies normalized finite quaternions, and runs camera checks after animation/alignment as production does. All tests and production build pass. The scalar native-trace comparison does not call the animation phase and therefore does NOT validate this newly connected alignment phase; its earlier numeric errors must not be represented as full current frame-pipeline parity.

The browser still has a combined animation bridge rather than the complete original controller/motion/local-pose/world-pose phase schedule. Full134CB0 landing pose bake, classification/stance handoff, and posed body/board collision/crash recovery remain unfinished.

Live airborne-alignment build check:90 samples,12 airborne samples, finite rider/camera coordinates, no failed predictions,250 banked points and zero rescues. Inputs released and game paused. See `local/browser-validation/live-air-alignment.json`.


### Material-specific touchdown velocity (September 11)

originalLandingVelocityResponse factors the unchanged impulse and normal-speed-clamp portion out of originalLandingResolveContact. The full native resolver still passes20,000 original contact-response comparisons, alongside the existing classification/focus/animation/visual-target checks. Browser touchdown now uses this helper before original ground-entry damping instead of universally projecting normal velocity to zero. Grounded contact behavior is unchanged in this step.

The actual-WASM material fixture substitutes hard and soft material IDs on a disposable in-memory copy of the imported terrain; production authored materials/files are untouched. Surface0 resolves-841.865cm/s normal impact to approximately zero; surface2 resolves-821.360cm/s to-293.315cm/s, matching its authored impulse/clamp. All existing tests and production build pass.

This is the velocity portion only. Position penetration, recovery materials/patch flags, complete moving-surface handling, posed board/body query, and crash/recovery routing remain unfinished. In particular, this does not claim complete original powder penetration or recovery-surface behavior.


### Original grounded velocity-contact response (September 11)

Grounded contact now calls originalGroundVelocityContact instead of projecting velocity onto the contact plane. The recovered function preserves relative speed while adjusting its direction, limits the vertical correction, and skips correction on surfaces2/3/13. Static surface velocity remains zero; moving-world callbacks remain unsupported.

The hard/soft material fixture checks the following ground tick as well as touchdown: hard-ground speed20.1587467 ->20.1587448m/s (float tolerance), soft-ground18.6933422 ->18.6933422m/s with its normal component preserved. All tests/build pass. Held-ledge fixture now departs653 and lands679 (26 airborne ticks); those timing changes are a consequence of replacing the old velocity projection and supersede prior fixture numbers.

Updated scalar native/WASM trace:600 ticks,0.07144m max position difference,0.03146m/s max speed difference, zero grounded/prediction disagreements,35 airborne predicted ticks each. This still omits the animation/alignment phase and is not full original-game parity. Full position/contact-distance handling, posed collision, moving surfaces and recovery remain unfinished.


### Original distance-based ground contact (September 11)

Ground movement now retains the integrated actor position instead of snapping to every query point. The adapter uses sourceGroundContact with original steered probe, coarse/refined terrain selection and cache; publishes signed distance/lateral clearance; applies original no-contact handling, surface selection, velocity correction and orientation alignment; then updates source visual targets, board lift and board normal. It no longer overwrites contact distance with a position==point query before every force step. Full body/world-pose reaction remains separate and incomplete.

The candidate exposed a real landing bug: world vertical velocity could be negative while velocity relative to the slope pointed away from contact (soft fixture+549.49cm/s). The landing gate now requires negative contact-normal velocity, as originalLandingImpact does. Landing also refreshes stored contact distance, preventing preflight distance from affecting the next ground step. Missing-world prediction tests now distinguish the passive-entry tick (not queried yet) from the following attempted air step; prediction never becomes falsely available.

All tests/build pass. Ground-position fixture:240 grounded ticks,218 visibly distinct from a forced surface snap, maximum actor/terrain separation0.0239258m, no resets. Hard/soft landing responses and held-ledge/no-extra-impulse checks pass. Scalar native/WASM comparison:600 ticks,0.07011m max position error,0.03662m/s max speed error, no grounded/prediction disagreements,110 predicted air ticks each. As before, this trace omits animation-phase alignment and is not full original-game parity.

The previous core is preserved at local/browser-validation/core-before-ground-pipeline.cpp for review. Remaining work includes posed board/body collision, landing penetration/pose bake, recovery flags, dynamic surfaces and full phase ownership. Updated physical visual-target fields are not a claim that every original procedural pose field is wired to rendering.

Packaged ground-pipeline run reached33.4 simulation seconds, grounded, zero emergency resets and finite rider/camera coordinates; then paused. Evidence: `local/browser-validation/live-ground-pipeline.json`. This bounded no-steering run does not verify full-course obstacles or recovery.


### Sphere-tree world geometry loaded (September 11)

world_bridge.cpp now loads type3 sphere-tree payloads rather than stopping at their descriptors: centers, compressed flag, per-level radii/offsets/strides, occupancy masks and resource identities. Shared resources are cached by(resource, model ordinal), matching the native asset loader. The existing original ray query still skips static sphere trees as it should; the geometry is now available for subsequent body queries.

The expanded native-loader comparison no longer skips type3:3,052 instances,2,884 nodes,30,551 triangle references and24 sphere-tree references match, including transforms and full tree payloads. Actual-WASM checks verify node counts and that unsupported count21 matches the descriptor/scale gates rather than missing static geometry. All tests and production build pass.

This supersedes earlier notes that sphere-tree bodies are not loaded. Actual rider/body contact generation and response are not wired yet; dynamic descriptor callbacks and other unsupported cases remain explicit. No new collision response is claimed from loading geometry alone.


### Posed rider body volume generated (September 11)

Attached animation now builds browserBodyVolume from the first22 posed bone centers transformed by the same physical quaternion and airborne presentation used by rendering. It uses bodyVolumeFromBones for the ten original body spheres and broad bound, adds the board landing center, pre-impact reaction frame, active air pivot and available main-animation metadata. Resetting animation clears the volume. Geometry currently uses the browser's effective unit pose scale; complete authored scale/phase ownership remains unresolved.

body_volume_info exposes a read-only geometry snapshot. The actual-WASM course fixture compares broad center, board point and every collision sphere center with the independent renderer hierarchy reconstruction, within1mm; count/radius checks and all existing tests/build pass. This establishes consistency with the current visible rig, not full original-world-pose parity.

No body query or collision response is applied yet. Next: supply original body-terrain grids alongside loaded instance/sphere-tree geometry, query the posed volume with original filters/caches, then connect response and landing/crash state transitions with correct phase ordering. Do not treat volume construction as completed obstacle collision.


### Body terrain and posed contact queries connected (September 11)

init_body_terrain loads all1,913 original terrain patches into WorldBodyCollision, preserving source coordinate conversion, coarse grids, flags/surfaces and prepareTerrainTraversal order. It validates the world source hash and publishes only after successful parsing. The expanded native asset comparison matches every grid coordinate and traversal entry, in addition to all instance/triangle/sphere-tree data. Actual-WASM loading/rejection/lifetime checks pass.

After posed volume construction, inspect_body_contacts now invokes the original WorldBodyCollision query with human-detail/filter2 and its own terrain cache. Ground queries use the physical contact normal; airborne queries use the posed reaction-frame up vector. Restart clears body-query and sphere-tree caches. body_query_info and browser QA diagnostics distinguish unavailable, incomplete, contact and no-contact states. No translation/bounce response is applied yet.

The normal course fixture visits1,067 body candidates in its no-boost run with zero contacts/incomplete ticks. A controlled test-only interpenetrating pose detects150 contact ticks with positive depth (maximum39.58253cm) and normalized normals. To position this fixture, only its reset/snapping mesh is emptied; authored body terrain/object geometry is unchanged, and no hit result is injected. The new positive-contact fixture passes alongside the earlier full suite; production code/build were already verified.

Next: consume these results through original ground/air obstacle response and collision event dispatch, with physical pose/landing/crash phase ordering. Contact detection alone is not completed gameplay collision or recovery.


### Original body-contact response connected (September 11)

Body contacts now call originalObstacleResponse or originalAirObstacleResponse according to the motion phase that ran this tick, including passive takeoff/landing transition cases. Accepted response applies source translation and velocity, translates cached broad/sphere centers, applies original ground impact orientation when bounced, and rebuilds air orientation as the original does. Ordinary airborne bounces reseed trajectory state so cached integration cannot overwrite the response. Pending passive takeoff seeds after ground response.

Airborne reset/spawn now explicitly seeds the predictor from the actual rider state; previously a reset with no ground could leave an origin-based default predictor. The controlled fixture checks that the first airborne step stays near its requested spawn.

body_response_info exposes applied translation/velocity and response flags; QA diagnostics report acceptance/bounce. The positive body fixture now produces2 contact ticks and1 bounce (maximum penetration44.90009cm), versus150 contact ticks without response. It verifies position changes equal response translation without extra integration, and velocity matches the response. All tests/build pass. The existing original-instruction suite also passes10,000 ground responses,10,000 air responses and10,000 collision-orientation cases, plus primitive body contact checks.

Full105D98 collision reaction classification, soft/crash animation dispatch, recovery/special-surface handling and complete pre-/post-collision render-pose ordering remain unfinished. Reseeding currently covers the ordinary airborne bounce path; full event dispatch must own special reaction decisions. The response does not imply full collision parity or complete crash gameplay.

Live response-build smoke test:90 samples during a flip attempt, finite rider/camera coordinates and zero resets. No body contact occurred in that bounded run; it does not verify a natural collision response. Positive translation/bounce is established by the controlled WASM fixture. See `local/browser-validation/live-body-response-smoke.json`.


### Pre-response pose frame retained (September 11)

Attached animation records its physical position/quaternion, pose motion flag and world-space head before body response. pose_physical exposes this snapshot. Rendering now uses that prepared frame instead of recomposing the old local pose onto post-response physics. The camera receives the cached world head through step_camera_head, while its velocity/physical-direction inputs still come from current physics, matching the native camera's split inputs.

The positive collision fixture verifies prepared position remains pre-response while actual physics moves by the recovered translation. Renderer tests verify an explicit prepared position overrides current physics position. All existing tests and production build pass. This fixes the body-response boundary within the current browser pipeline; it does not establish complete original local/world-pose, terrain-landing, crash or event phase ordering.


### Original collision classification connected (September 11)

Each bounced body response now calls originalCollisionReaction with pre-impact presentation, post-response physical frame/velocity, incoming direction, motion/control phase, animation metadata, captured surface property44 and shared animation RNG. The original history decay runs once per physics tick. Decisions are retained with a serial number; history recovery requests are recorded. Predictor reseeding now follows the reaction's resetPredictor flag and actual airborne state rather than an unconditional bounce rule.

Original reference suite passes30,000 reaction decisions,10,000 history-decay/recovery cases and30,000 RNG transitions. The WASM response fixture checks one classification per bounce; a separately positioned inverted original pose selects Crash semantic354 and predictor reseed. The positive classification fixture passes alongside the earlier full suite. Source/script logs are local/collision-reaction-validation.log and local/body-query-validation.log.

Soft/crash/recovery decisions are not yet dispatched into their controllers. This is classification/history ownership, not completed crash gameplay. Next: original hard-crash entry (including air-exit pose bake), soft-control entry, and recovery consumption. The current combined pose/response bridge still differs from the full original phase schedule.


### Ordinary soft collision control dispatched (September 12)

Classified grounded soft hits now enter original control3 and their authored55..60 clip. Core uses originalSoftControlStep for steering/boost requests and neutral crouch/brake targets, with animation completion driving the return to0 (or4 in air). Jump charging is suppressed while soft control owns the frame. Shared animation-turn/animation metadata are synchronized, and residual manual spin is passed through the existing original ground-heading routine. Current prewindStyle0 makes source stance restoration a no-op; rail/nonzero-style and recovery/finish transitions remain incomplete.

soft_collision_begin is the event-entry API used by dispatch and conformance fixtures. All six clips complete and return to normal in the actual-WASM fixture:31 active ticks for55/56/57/59/60,47 for58. Jump/brake attempts during ownership are rejected by the source targets. A test-only static box verifies actual query -> bounce -> Soft classification -> control3, without injecting a hit or reaction. Native reference checks pass20,000 soft selection cases and20,000 control3 cases, plus the existing classifier/history/RNG checks. Full suite passes; latest integration evidence is local/soft-control-integration.log.

Hard-crash entry/update/recovery, full stance handling and complete original phase scheduling are still unfinished. Soft controller fixtures are not evidence of those paths being complete.


### Stance-restoration callbacks connected (September 12)

Soft completion now invokes original115640 when the physical prewind style is nonzero. The animation bridge supplies the original physical-change, sequence-root composition, default-root reset and animation-request callbacks in source order, then publishes the resulting physical state. Style0 remains a no-op. restore_rider_stance exposes the same entry for explicit style conformance fixtures.

Native reference passes20,000 full restorations, including physical bases, six-channel sequence roots, mirror preservation, default roots and callback order. Actual-WASM tests cover styles0..4 in ground/air modes, checking no-op behavior, quarter-turn physical changes, animation requests and callback order. Full suite/build pass. Nonzero-style fixtures exercise the restore entry directly; normal current-course soft hits generally use style0, and full rail/style entry remains unfinished.

This does not complete reverse-stance gameplay, airborne landing pose bake, hard-crash controllers or recovery. It supplies the missing common restore callback for those integrations.


### Grounded reverse-stance transition connected (September 12)

Cruise now invokes original114CC0/115168 when eligible backward travel is detected, excluding crouch/release, soft control, existing reverse animation and manual-spin ownership. The original physical/control flips are paired with the native sequence-root rotation, future-root quaternion, mirror state and semantic21. Ground control targets are computed before their approach filters, matching the source ordering used by this decision. Attached animation receives reverse/style state.

Reset restores the captured default root and mirror instead of retaining changes from the prior run. The actual-WASM backward-heading fixture checks one transition, physical facing aligned with travel, mirror/semantic21, no repeated toggling, and clean defaults after reset. Native reference passes20,000 reverse decisions/physical/control cases; full browser suite/build pass.

This covers grounded cruise reversal. Full terrain-landing pose bake/reversal, hard-crash control and rail entry remain unfinished.


### Crash root-preview callbacks available (September 12)

BrowserAnimationGraph now retains sampledLocal and reuses the native previewRoot, scaledLocalRoot and offsetSequenceRoots implementations through generate-controllers.py. Preview resolves the existing single-leaf crash variant without consuming RNG or changing active sequences. Current-root access remains unavailable before a pose is sampled and is cleared on animation reset. These methods use the browser's current unit pose scale/full bone mask, consistent with its existing pose path.

Actual-WASM checks preview all34 crash semantics328..361, require finite transforms, verify current root/playback remain unchanged, and check invalid/unsampled roots remain explicit. Full suite and production build pass. This supplies required animation callbacks for10EB30 entry; hard-crash entry/control/motion dispatch itself is not yet connected.


### Live physical presentation connected to browser skin pose (September 12)

The browser previously evaluated the original ground presentation targets without advancing several corresponding filters, and rendered with fixed board-contact inputs and no live lift/extra lean/roll. Core now advances the presentationLift, extraLean, boardAlignment, presentationRoll and air-adjustment triplets once per physics tick, calls originalGroundPresentationTarget after originalGroundIntegrate, and retains its boardNormalForPose before the late board-normal smoothing. The animation bridge supplies those live presentation values and the original board bounce/alignment to the existing root and contact routines.

Browser skin output is relative to the presented physical root; native11FA10 lateral and11EB98 normal/board-direction inputs are world-space vectors. pose_space.hpp transforms these directions by the inverse presented quaternion (including airborne spin) before applying the unchanged original pose functions. Air presentation still advances once per animation tick, now before this conversion. Frontend/reset restores captured contact values and leg weight rather than retaining gameplay contact controls. pose_controls_info exposes the16 consumed inputs for QA.

Validation: full browser suite and production build pass. The expanded actual-WASM course fixture observes live lift on295/299 ticks, board bounce on600/599 ticks and nontrivial local slope directions on600 ticks per run, while retaining renderer/body-sphere agreement within1mm. `sh tools/test_browser_pose_space.sh` compares5,000 physical orientations against directly applying the original functions in world space: maximum position difference0.000549316cm and quaternion-component difference0.0000604391. `python3 tools/test_rider_pose_native.py --live` passes the existing original-instruction posed-rider reference. Integration log: local/live-pose-integration.log.

Live WebGPU smoke run exercised left/right steering, jump, original grab75 and landing61:90 samples,15 airborne, all rider/camera coordinates finite, zero rescues, observed60–63fps. This short run does not establish sustained performance or original-game visual/physics parity.

Hard-crash entry/control/recovery remains unconnected. This fixes a shared presentation prerequisite encountered while inspecting crash entry. Full original pose/contact phase ordering, terrain-landing bake/reversal and authored-scale parity remain unfinished; current browser contacts are still resolved earlier than the native world-pose pipeline.


### Air-exit pose bake and landing contact resolution connected (September 12)

Core touchdown now publishes the integrated source position/velocity before calling the animation-owned control5 exit hook. The hook invokes originalAirControlExit134CB0 using the retained sampled pivot, advances air presentation once, bakes its position/quaternion, restores the adjustment targets/rates, retains quantized manual spin, and fades channel1 by the original0.33s. The subsequent animation landing callback consumes the already-baked result without quantizing spin twice. Held control2 and passive control4 do not invoke this exit. air_exit_info records the entry/exit transforms, pivot, spin and serial; reset clears it.

The accepted touchdown now invokes originalLandingResolveContact139D78..13A148, preserving tangential position and original material depth instead of snapping the actor to the vertical query point. Material velocity response remains inside that original resolver; originalGroundEnter follows. landing_contact_info records the pre-resolve position, point/normal, depth3, result and signed distance. The speed-limit regression test now checks this projection independently and checks the authored contact point against height_at; the former zero-depth assertion described the removed snap behavior.

Non-held landings invoke originalReverseTurn after ground entry. A reversed landing applies animation root/mirror changes without inserting cruise clip21, then selects originalReverseLandingAnimation64/65. Severe impact's manual-spin change is retained in physics. The existing grounded cruise reversal still uses21.

Validation: full browser suite/build pass. test-air-exit.mjs exercises zero rotation, forward/backward flips, both spin directions and held ledge touchdown. It verifies one bake before animation, no duplicate mutation, normalized output, preserved rotation/pivot offsets, exact reversal/mirror serial changes and landing64/65 on reversed touchdowns. Native reference scripts pass20,000 air-exit cases and20,000 landing contact-response cases, plus their related control/classification/animation checks. Logs: local/browser-air-exit-reference.log, local/browser-landing-resolve-reference.log, local/browser-air-exit-integration.log. Live WebGPU jump/spin/grab smoke test:100 samples,20 airborne, finite transforms, zero resets; observed60–73fps is only a short-run measurement.

Important remaining fidelity issue: this still uses the provisional vertical touchdown sweep and the preceding sampled air pivot; the original139C88 posed-board probe and complete same-tick world-pose/contact ordering are not connected. Patch/recovery metadata and landing crash dispatch are also incomplete. Rotated/inverted touchdown can still continue riding because hard crashes/recovery remain unconnected.

The longer current native/WASM scalar trace is NOT close parity: over600 ticks max position difference3.16461m, max speed difference8.64597m/s,7 ground-state and2 prediction-state mismatches. Both first land at186 with0.0185m position difference; predictor state first differs at281; browser next lands343 versus native345, and browser has another landing355. Native uses chop rounding and WASM nearest, but the cause of the larger divergence is not yet isolated; do not ascribe it solely to rounding. compare-native.mjs now saves wasm.csv alongside native.csv and records the first positional divergences and all state mismatches in native-vs-wasm.json. This harness omits animation/air-exit dispatch, so it isolates a core-motion/contact discrepancy rather than validating the new full animation path. Investigate the original posed landing query and phase ordering next. Do not use the green unit suite or command exit status as evidence that long-run physics parity passed.


### Gameplay touchdown uses the original posed-board query (September 12)

With animation_use_physics enabled, step_rider now leaves airborne contact resolution to the posed contact phase. After original animation sampling/world pose and body response, resolve_posed_landing calls originalLandingContact13A7B0/139C88: endpoints are the posed board center plus/minus200cm along the final presentation-up vector, mode0, original preferred fraction0.574999988 and refined terrain query with the retained source contact cache. OriginalLandingImpact owns the coarse fraction>=0.5 and incoming-normal-velocity gates. The returned world/terrain contact, including patch flags/metadata, feeds the existing source resolver. Missing/incomplete resources are explicit; gameplay never substitutes the old vertical ray.

Air exit now uses the current sampled pivot during this contact phase. Source bake, material response, ground entry, reverse landing, animation request and scoring happen once on accepted touchdown. The prepared world pose/physical frame remains the pre-contact frame for rendering and head-camera input; landing animation starts affecting geometry next tick. previousGround and controller/animation metadata are updated after acceptance, avoiding duplicate next-frame touchdown work. Current-frame animation input diagnostics still describe the controls that generated the cached pose rather than later contact mutations.

landing_probe_info exposes17 fields: available, complete, fraction, accepted, center3, presentation-up3, hit-position3, hit-normal3, surface. It resets each motion tick; lastPosedContactTick prevents duplicate contact execution if a pose is sampled repeatedly without advancing motion. A passive ground-mode ledge departure does not run airborne contacts until its air motion phase; diagnostics do not reuse the previous hit.

Validation: full npm suite and production build pass. Expanded actual-WASM air-exit tests reject pre-pose gameplay touchdowns, match probe center to the posed board, check presentation-up normalization/coarse gate/query kind4, and check event/stance/score timing. Gameplay renderer/body agreement uses the retained physical pose rather than post-touchdown physics. The deliberate body-collision fixtures now clear only ray-query terrain while retaining original body-query terrain, isolating body response from the independently tested landing controller. `python3 tools/test_landing_contact_reference.py` matches49 full original board queries exactly, including35 contacts and0 unsupported callbacks. Reference log: local/browser-landing-probe-reference.log. Full integration log: local/browser-validation.log.

Live WebGPU jump/grab test:110 samples,32 airborne, original75/61 clips observed,290 points banked, finite rider/camera transforms, zero rescues, observed60–63fps. This is a bounded smoke test, not sustained performance or full original-game parity.

The no-animation scalar diagnostic path retains its explicitly labeled vertical query. Consequently compare-native.sh still does NOT exercise the gameplay landing pipeline and cannot establish that the previously observed long-run divergence is fixed. A native/WASM full-frame trace including the same graph/pose/contact/event stages is still needed.

Hard landing/crash classification dispatch and recovery remain unconnected: the forward-flip stress fixture now makes22 ordinary air-exit transitions in600 ticks, evidence of repeated transitions where original crash handling is needed, not a fidelity success. Ground contact/visual stages still precede the cached world pose in the browser, and complete source phase ordering/scaling, special controls, UI/audio/FX and original-game parity remain unfinished. The next meaningful gameplay integration is hard-crash entry/control/motion/recovery, including landing crash selection before ordinary landing events.


### Original hard crashes and ordinary get-up recovery connected (September 12)

Landing now runs originalLandingClassify after air-exit/contact resolution and before ordinary landing/scoring events. It consumes the shared RNG only on the original random branch. Crash choices enter originalHardCrashEnter10EB30; body-collision Crash reactions use the same entry. Entry bakes the presented/local/preview roots, clears failed trick points, applies prewind compensation, plays the selected original clip and establishes control8/motion2. Already-baked landing air exit is not repeated. Current visible pose remains the prepared pre-contact frame.

web/crash_runtime.hpp drives the recovered five-state controller and separate crash motion: original recovery meter, clip-derived angular release, continuation selection/playback/seek, airborne predictor/alignment, sliding forces/material contact, crash-specific body response, detached-board query/response, get-up and reset-clip branches. It uses the existing engine/crash_*.hpp functions at the same host boundaries as PrototypeRider. The animation bridge now supplies native channel progress/rate/duration/seeking plus cached world primary/secondary bones. Ordinary riding/grab/air selectors do not run while control8 owns the frame; detached board transforms are applied to the rendered board bone. The main crash trajectory is also published for camera diagnostics.

Ground/air get-up callbacks restore real motion/controller state, including material ground entry or a fresh original air-control state. Collision-history decay is once per core tick, including a crash exit tick. Crash entry clears held jump charge, and holding jump during crash drives the original recovery meter. Crash feedback observers are recorded; original crash HUD, sounds and particles are not yet rendered. Public crash_info and window.demoState expose phase/submode/entry clip/ticks/detached/recovery/reset reason for QA.

Validation: full npm suite and production build pass. test-crash-gameplay.mjs runs three actual-WASM gameplay cases with original terrain/body geometry: forward flip with recovery input (entry187, exit288), backward flip with recovery input (189..320), and forward flip without recovery input (187..290). They traverse initial crash350/354 -> sliding continuation364/368 -> get-up391/395 -> normal control, with finite state, zero respawns, no failed-trick points, no jump charging, and no grab/steer animation override. Existing inverted-body-collision fixture now also asserts actual control8 entry. Clean grab/scoring fixture uses grab from150 through179 and releases before touchdown; the previous late grab correctly crashes under source landing classification. Flip coverage moved from the clean-air-exit test to crash lifecycle tests rather than expecting inverted landings to remain ordinary landings. Results: local/browser-validation/crash-gameplay.json and local/browser-validation.log.

Original-instruction reference scripts passed:20,000 entry sequences;40,000 root bakes;20,000 cases each for recovery meter, playback, get-up, reset-clip, recovery phase1, recovery phase2, airborne crash motion, sliding forces, sliding contact, sliding body response and airborne body response;1,300 crash animation selections. Logs are local/browser-crash-{entry,bake,control,recovery,motion,collision,animation}-reference.log. These establish the underlying recovered routine behavior, not whole-browser/PCSX2 bit parity.

Live WebGPU verification triggered a natural inverted landing into350/364 at60fps, visually inspected the upside-down sliding crash pose, and observed recovery back to riding with zero respawns. Rebuilt and reloaded the latest demo afterward.

Remaining limitations: detached-board and phase4 reset routes are implemented but not yet covered end-to-end by these browser tests. Reset requests currently route through the browser's existing spawn reset on the next tick, not the recovered source reset-position/controller lifecycle. Crash penalty/refund mirrors the existing native prototype's simplified amount adjustment, not the full119B08 manager/statistics integration. Scoring-stance/feedback observers remain incomplete. Host timer/control/motion scheduling, authored scale, unsupported dynamic world callbacks, original crash UI/audio/FX and a full native/WASM/PCSX2 frame trace still need work. In particular the scalar compare-native.sh path still omits animation and crash dispatch and cannot prove gameplay physics parity. This supersedes earlier claims that hard-crash dispatch or ordinary recovery are wholly unconnected; it does not mark the demo or all crash routes complete.


### Crash tick ordering and recovery handoff corrected (September 12)

Crash control and motion are separate browser host calls. Core now performs frame-begin speed-limit work before crash control, then the original boost timer and retained control-filter pass before crash motion. The earlier combined host advanced motion before ticking boost and skipped these filters. Hard entry also now initializes control8 and motion2 in their separate10EB30 callbacks, rather than beginning motion inside control entry.

browserCrashExitFrame records when the control8 handler has already dispatched on the recovery tick. Newly selected ground/air motion still advances, but ordinary steering/crouch/boost/jump control and animation selection do not dispatch a second time that tick. This preserves original riding-entry semantic5 and zero jump charge on ground recovery; held input is accepted on the following tick. Animation reset clears the ownership flag. Ground recovery material selection preserves the frame-begin dynamic speed limit, as native applyGroundMaterial does.

Camera input now reports original motion mode2 during crashes, then0/1 after recovery, instead of mislabeling a sliding crash as cruise or an airborne crash as ordinary air. camera_inputs keeps the prior12-field prefix and appends motion mode at index12.

Validation: full npm suite and production build pass. All three crash gameplay fixtures now assert normal control0/5 (not crouch2) on the recovery tick, zero jump charge, no second steering update, ground-entry clip5, and functional crouch input on the following tick. They drive the camera throughout the crash and verify finite results plus correct2->0/1 mode transitions. A standalone animation reset test verifies recovery ownership cannot suppress later normal animation control. Ground recovery occurs at ticks288/320/290 in the existing forward/backward/automatic fixtures. Live WebGPU check:130 samples, crash phases0/2/3 followed by recovery, finite motion/camera, zero resets and observed60–63fps. Current demo is rebuilt and paused after the smoke run.

These fixes narrow the crash scheduling gap; they do not establish full original frame parity. Detached/phase4 reset handling, original feedback/statistics/HUD/effects and full native/WASM/PCSX2 frame comparisons remain outstanding as described above.


### Forced-reset route selection recovered (September 12)

Added engine/reset_route.hpp/.cpp with complete original112D58 route selection/refresh and269F18 event-aware path sampling. These are distinct from ordinary NPC selection and position sampling: requireField3C filtering, no velocity lookahead, retained-path refresh, inclusive event12/14/16 handling, and the sampler's by-reference distance mutation all matter. The original-executable comparison passes10,000 complete route cases exactly plus23 event-boundary samples; see local/reset-route-reference.log and tools/test_reset_route_native.py.

The source audit also establishes that reset placement uses the129-record AI/reset path table (117 eligiblefield3C records), not the browser's8 race-course paths. Captured human retained route is index2. tools/reference_npc.py already decodes the required table; it still needs bundling for browser reset use.

No browser gameplay behavior changed in this pass. Forced reset still respawns at the demo start:11D660 safe placement, control9 initialization/update and motion3 lifecycle must be recovered/integrated before replacing it. See docs/reset-recovery.md for source addresses, exact verified behavior, data provenance and next steps. Do not describe the new route helper as a completed course-preserving reset.


### Reset placement and timed-control routines recovered (September 12)

Added engine/reset_placement.hpp/.cpp for11D660..11DACC placement/basis and engine/reset_control.hpp for complete12F398 update/callback ordering.20,000 executable-reference cases pass for each, plus an exact sequential reset timeline: at scale1, placement crosses on tick21 and passive-air handoff occurs on tick41. Logs:local/reset-placement-reference.log and local/reset-control-reference.log.

Corrected the previous interpretation of11D660: the200/1000cm argument is vertical clearance. The routine makes one vertical world probe; it is not an iterative safe-point/radius search. Negative clearance bypasses the probe and offset. The test verifies probe arguments with controlled hit results and stops at11DAD0; real world coupling and the later control/animation/body/camera initialization callbacks remain unverified/unconnected.

Browser reset behavior did not change in this pass. Remaining integration details, including control9 entry/fade and the11D660 state-clearing tail, are in docs/reset-recovery.md. Do not claim course-preserving reset until those operations and the actual129-path reset dataset are connected and exercised in gameplay.


### On-course forced reset connected (September12)

The browser now imports the original129-record reset/AI path bank and retained human route through tools/reference_reset.py and the normal prepare-ui pipeline. It validates the captured140BC0 path-end predicate, preserves field3C flags/segments/events/cache data and records EE provenance. The current Snow Jam event variant0 always uses200cm clearance; other variants remain explicitly unsupported by this extractor pending their event-mode predicate integration. This is separate from the8 race-progress paths.

Forced crash requests and request_rider_reset enter reset control9/motion3 instead of calling reset_rider at spawn. Main animation freezes, motion3 rebuilds orientation without integrating translation, the source route/placement functions run on tick21, and control4/motion1 resumes on tick41. Requests made from crash control enter reset motion immediately without dispatching a second controller. Placement clears represented motion/filter/contact state, restores stance/default animation roots and reattaches a detached board. Original11DF18’s represented post-placement behavior is now applied: freeze the new animation and initialize velocity to physical-forward ×833.333374cm/s, with vertical velocity zero. The original119368 coefficient is applied through originalBoostAward:−10% of current meter for a nonzero reset reason,−70% for manual reason0, followed by normal timer decay. The additional manager/statistics effects are not fully represented.

Race clock, progress session and UI banked score are retained. No respawn counter is incremented. A placement serial clears renderer/camera interpolation history and the current trail connector so teleporting does not interpolate across the course. Camera receives motion mode3 during reset. Keyboard Backspace and standard gamepad Select/Back(button8) request manual reset on a press edge; R continues to restart the run.

Validation: full browser suite passes, including test-forced-reset.mjs:21/41 timing, frozen position between placement and handoff, no return to spawn, unchanged banked210 points, continuously advancing race time, original forced/manual boost deductions and successful passive-air landing. test-detached-reset.mjs drives the common hard-crash event entry with authored360/361 clips against actual world/body geometry:360->372->287 for quick recovery,361->372->409->287 for automatic phase4 recovery. Both use on-course reset with zero emergency respawns. The injected entry events validate these lifecycles, not natural selection of360/361. The route oracle now also checks26A8B8’s inclusive direction boundaries alongside24 event-boundary samples;10,000 full route cases still match exactly. Logs:local/browser-validation.log, local/detached-reset-integration.log, local/reset-route-reference.log.

Live Backspace test:120 samples, one placement/completion, no restart,210 points retained and5.98 seconds elapsed after the six-second run. Finite rider/camera state and observed60–63fps. The updated production demo is built and paused after verification.

Remaining: original white fade/device effects are not drawn; several111890/11D660 statistics/body/camera observers are represented only by existing native/browser state refreshes or diagnostics. The complete original local/world-pose initialization schedule is not proven. Normal-frame shared AI-route progress is not yet wired; reset selection refreshes the retained route from current position when requested. Full native/WASM/PCSX2 frame parity, unsupported dynamic collisions and the other UI/audio/snow/rail/opponent requirements remain outstanding. This supersedes earlier notes that forced reset always returns to spawn; it does not mean the complete original reset presentation or game is finished.


### Source-derived white reset fade connected (September12)

12F230 constructs white in/out color effects with durations(.5,0,.5), no captured-frame texture, and a pointer to reset progress.2E4760 converts that progress binding into elapsed fade time.2E48AC..2E492C selects/clamps the opacity;2EBB10 multiplies the color alpha by it and draws the viewport quad. The browser now uses engine/reset_fade.hpp for this exact reset specialization and draws a white gameplay-viewport quad from its result. There is no independent fade clock or added easing.

reset_info retains its8-field prefix and appends alpha at index8. Alpha is available only for an active reset with an enabled nonnegative human device, matching12F588 entry gating, and clears on completion. The UI draws it during gameplay; pause/menu screens remain readable. window.demoState.resetFadeAlpha exposes the current source opacity.

Validation:20,000 original-render-prefix comparisons match opacity and in/out selection exactly, including0/.5/1 and clamping. Script:tools/test_reset_fade_native.py; log:local/reset-fade-reference.log. The comparison starts at2E48AC with the reset-specific effect configuration and stops before graphics dispatch; it does not prove full renderer/compositor pixel parity. Full browser suite and production build pass. The actual-WASM reset fixture verifies a near-opaque peak and zero residual alpha after handoff. Live browser verification visibly filled the gameplay viewport white at progress0.49999982/alpha0.99999964 while preserving the black outer bars. The fade then cleared and the on-course reset completed without a restart.

Remaining: complete original renderer layer/viewport parity, other device effects, audio/rumble and the broader fidelity gaps remain unverified. This supersedes the earlier statement that no reset fade is drawn.


### Human route progress connected to the shared frame (September12)

The browser now invokes originalNpcRouteProgress1125C0 for the human after course progress, matching rider_world.hpp/native_world.mm. race_end supplies bestRemaining and the pre-increment source tick (the current race-session wrapper has already advanced its clock, so the callback receives totalTicks−1). The context is explicitly human; an unexpected NPC random draw throws. This stage runs during ordinary riding, crashes and resets as in the native shared-frame callback.

The retained129-path route now updates projection/cache, previous/current distance, lookahead, path selection and heading while riding. physicsState.headingOffset receives that heading for the next groundForwardDrive call. The captured initial route heading is imported from rider4CC and restored on restart. route_info exposes the path/distance/heading/update count/cache/points for QA. This supersedes the earlier limitation that normal-frame route progress was absent.

Validation: full browser suite/build pass. test-route-progress.mjs runs600 gameplay ticks, checks one route update per race tick, finite/bounded route data and heading consistency with authored lookahead. It observes588 advancing-distance ticks,103 heading changes and a switch from path2 to119; progress continues through41 reset frames, and restart restores the captured route state. The existing forced-reset, crash and detached-reset tests still pass with this shared stage enabled. Log:local/route-progress-integration.log.

Live WebGPU junction/reset check:250 samples, paths2/119/118 observed, one completed on-course reset, zero restarts, finite rider/camera and observed60–63fps. Full native/WASM/PCSX2 frame parity and the remaining gameplay/rendering/UI/audio/FX work are still outstanding. The scalar comparison harness still omits these animation/race host stages and is not evidence of complete gameplay parity.


### Original board-track geometry, texture and lighting connected (September12)

Removed the browser's generic LineSegments trail. Attached animation now supplies board_trail::update2E8938 with the posed board23 frame, live crash-corner bones10/15/18/21, contact point/normal/distance, velocity, material, current animation markers, stance and crash submode. Core retains the original ground/touchdown point for this phase. The source profile and visual LCG seed come from original_board_trail; six54-slice bands retain the original eligibility, spacing, depth/width, jitter, takeoff caps and draw-window fading. Restart and forced placement clear the trail component. Trail geometry is packed into ribbon/roof buffers; trail_info reports vertex counts, serial and ring state. No gameplay RNG is consumed by trail generation.

web/board-trail.js draws the original btrl texels with separate PS2 texture-alpha and vertex-colour scales. The five depth/stencil passes follow the recovered/native-adapter mask -> visible roof -> depth clear -> trough -> hole fixup sequence. They render after terrain and before rider geometry: rendering depth-clear after the rider erased its board in the first visual check. Custom blending keeps these passes together in the opaque ordering while preserving alpha blending. The renderer now allocates stencil; per-frame buffers update only when the source trail serial changes. This is a browser adaptation of the source pass strategy, not proven GS destination-alpha/pixel parity.

The first visual check also exposed an incorrect captured-only blue tint. Added environment_bridge.cpp and prepare-environment.py to package/load the original CPU lighting lattice (4,214,450 bytes) and use originalEnvironmentUpdate with live ground/predicted patch UVs. Valid samples now tint tracks from the current terrain. Missing guarded texels retain the prior sample and report an explicit environment_info gap, matching the existing native adapter policy. Captured light colour remains a fallback only when the lighting package has not been loaded, as in some narrow headless fixtures. Browser setup includes the lighting package.

Validation: full browser suite/build pass. test-board-trail.mjs observes588 frames with geometry,118 airborne closed-trail frames and a bounded maximum1,410 ribbon vertices; buffers are finite and stay near the rider, and reset clears them. With the real lighting package it observes600 updates and one retained-sample gap; this is an unresolved coverage limit, not zero-gap parity. Native references pass2,970 draw windows,30,000 full board-trail updates,10,000 VU strip payload/fade/order cases, and20,000 cases each for original texture sampling, patch colour and filtering. Logs:local/board-trail-integration.log, local/browser-board-trail-reference.log, local/browser-environment-reference.log.

Live WebGPU inspection confirmed terrain-lit textured tracks and a visible board after correcting draw order. A jump/reset run produced130 samples,1,410 maximum track vertices,33 airborne samples, one completed reset, zero restarts, no sampled lighting gaps and steady observed60fps.

Remaining: the snow spray is still the older JavaScript placeholder emitter. Thus the visual LCG is not yet shared with original snow-emission calls and its later jitter sequence is not original full-frame parity. Rail/manual/special tracking flags still depend on unfinished controller integration. Full original renderer blending/gamma/destination-alpha order, environment guarded-edge coverage and whole-game/PCSX2 visual parity remain unverified. Do not describe all snow effects or rendering as complete.


### Original snow emitters and particle kernels connected (September12)

Removed the JavaScript Math.random/Points snow emitter. The browser now loads the original10 particle/emission profiles and source spry/impt/tmb1 textures, and runs originalSnowContextStep plus chunk, trail, impact, cloud and BodySnow producers in the recovered caller order. The seven rendered channels are0/1/2/5/6/7/9;3/4/8 receive the original inactive requests, and unsupported visible activation is explicit. GPU rendering uses one instanced billboard batch per rendered channel, original half extents and GS colour/texture alpha conventions, with source UV orientation and terrain-derived lighting.

Track jitter and snow producers now share the same visual LCG word, in track-before-snow order. Particle births use their separate captured six-word RNG; gameplay random state is not substituted. Visual updates run once per motion tick, so render reads and high display refresh do not advance birth histories. OriginalSnowParticles owns source birth-ring interpolation, lifetime, damping/force polynomial, size, colour and LFSR traversal. Zero-alpha quads are omitted as in the native renderer.

Ordinary landings feed the original impact trigger. Crash entry and actual slide/air contact impacts feed the crash wrapper; later control-observer reporting does not generate duplicate impact events. BodySnow uses the recovered30-entry bone table over live world poses. Reset clears the represented context/body cursor through the source-style initialization; forced placement retains existing world-space particle histories to expire naturally, while a demo restart clears them. Camera rotation only updates billboard orientation.

Validation: full npm suite/build pass. test-snow.mjs exercises carving, braking, jumping and landing with live lighting; observed channels0/1/2/6/9, up to71 board-spray particles,204 small chunks,23 small-impact particles and47 BodySnow particles. Surface0 correctly has no cloud emitter; an isolated authored surface1 fixture produces71 cloud particles, while surface4 suppresses spray/clouds and retains its authored small chunks. Inactive channels remain empty, buffers are bounded/finite, reads are stable and restart clears histories. Material substitutions are test-only; shipped terrain is unchanged.

Original references pass20,000 birth-ring/RNG cases;2,000 particle-VU cases plus an exact visible36-birth traversal;20,000 cases each for rider cache, chunky emission, cloud emission, impact emission, trail requests, context and impact triggering; and20,000 BodySnow requests. Logs:local/browser-snow-{particles,emission,context,crash}-reference.log and local/browser-validation.log. These test the recovered routines, not whole-browser bit parity.

Live WebGPU carving/braking inspection showed original textured spray/chunks. A bounded jump/reset run recorded130 samples,153 peak visible particles, impact and BodySnow activity, one completed reset and zero new emergency rescues (the session's cumulative rescue counter started at2). Rider/camera/particle values remained finite; observed60–61fps. Current build is ready and paused.

Remaining: no native wake cache is connected. snow_info[21]/demoState.snowWakeUnavailable explicitly reports surfaces requesting wake velocity; the original no-wake branch is used, so emissions and later shared-LCG history are not full source parity where a real wake result would exist. Secondary effect requests, unused/special emitters, dynamic tracking/visibility controls and exact renderer/gamma/GS layering remain incomplete. The captured-start demo does not reconstruct historical particle births from the snapshot. This supersedes earlier notes that snow spray uses a JS placeholder; it does not establish complete snow-effect or whole-game fidelity.


### Wake-row construction recovered (September12)

Added engine/wake_row.hpp/.cpp and an original-executable oracle.20,000 complete2DDD30 row births and2DE058 cursor-prefix updates match exactly, including noise-driven velocity fan, drag, payload, repeated render anchors/alpha and untouched UV/RGB.48 noise boundary/octave cases also match. See tools/test_wake_row_native.py and local/wake-row-reference.log.

Source inspection establishes that snow reads the newest retained row's final vector from the rider's own owner3B0 wake ring; it does not perform a fresh world query. The snow context comment now reflects that layout. The live2DD0B8 driver and remaining row updates are still missing, so the browser wake-gap flag and no-wake branch remain unchanged. Full source map and next steps are in docs/wake-recovery.md. This is a verified prerequisite, not completed wake integration.


### Wake aging and profile constants recovered (September12)

Added originalWakeAgeRows and originalWakeProfile. The original aging prefix now matches20,000 cases exactly, including8,747 expiry cutoffs, position/velocity updates and untouched owner/render fields. Device/non-device row dimensions, lifetimes, vertical increments and initial texture coordinates also match the executable; prior row-birth/noise checks still pass. The empty-cache oracle boundary was corrected before accepting the results. See docs/wake-recovery.md and local/wake-{age,row}-reference.log.

These remain prerequisites: the live wake input/coefficient calculation and row-creation state machine are not connected, so snowWakeUnavailable and browser behavior remain unchanged. Do not treat aging alone as completed wake support.


### Live wake physics connected to snow chunks (September12)

Recovered and verified the remaining live target and control stages, then composed them with wake aging/birth/cursor code in OriginalWakePhysics. Source comparisons pass20,000 target cases,30,000 control/callback cases and10,000 composed physics frames. The composed oracle excludes2DE058’s render colour/UV preparation; it does not prove wake rendering parity.

Snow now receives the newest retained wake tip generated from current board pose, velocity, steering, extra lean, brake, presentation roll and surface settings. The original noise table is packaged through prepare-ui. The initial zero-vector cap row is valid; only an empty ring yields no wake result. Carving cessation expires rows, and reset clears the cache. wake_info and demoState expose count/active/tip data. The misleading extra-lean mapping for the snow secondaryBrake274 input was corrected to presentation roll.

Full browser suite/build pass. test-wake.mjs observes30 rows and a1370.7cm/s peak tip in its fixture, verifies initial zero tips, expiry and reset. Live WebGPU check:150 samples,32 peak rows,1851.5cm/s peak tip, no unavailable flag, one completed reset, no new rescues and60–63fps. The separate original wake ribbon mesh is not rendered yet, and full colour/UV, scaling and frame parity remain unverified. See docs/wake-recovery.md for exact scope and logs.


### September 12: carving wake render integration

The live browser now draws original wake texture 56 over the source-generated wake rows, using recovered row UV/RGB and lifetime draw-window math. Source preparation/window comparison passes 20,000 cases. Actual-WASM tests cover finite/bounded geometry, expiry and renderer generation invalidation when restarting with populated geometry. A short live WebGPU run observed visible wake, snow particles and subsequent complete expiry at 60 fps without new emergency respawns. Production preview was rebuilt and left paused at http://127.0.0.1:4173/.

Full original graphics-state/blending and PCSX2 visual parity are still outstanding, especially the bright ribbon appearance. See the latest section of `docs/wake-recovery.md` for source addresses, ABI and scope. This work does not establish full physics or animation parity.


Wake fade follow-up: the browser now accumulates per-row fade in original VU1 order, replacing ordinal multiplication. 20,000 original GPU strip passes validate board and decreasing wake alpha, RGB/UV/positions and ordering. The complete wake draw wrapper also passes 20,000 dispatch/material/gate checks; downstream GS pixel parity is still unproven. See docs/wake-recovery.md for details.


### September 12: native/WASM drift isolated to rounding in scalar trace

`sh web/compare-native.sh [original|nearest]` now supports a diagnostic nearest-only native executable as well as the unchanged original rounding policy. The nearest build links a local diagnostic `fesetround` no-op; it is never linked into the game, does not change engine source, and a fresh process begins in nearest mode. This tests the rounding-policy hypothesis without altering game speed or force constants.

The original-policy 600-tick no-animation trace still differs from WASM by up to 3.1646105 metres / 8.6459675 m/s, with seven ground-state and two prediction-state mismatches. With nearest rounding on both platforms, all 16 scalar output fields match exactly in every frame and prediction statuses also match. The comparator now reparses CSV fields as float32, avoiding artificial differences from decimal printing. Nearest mode fails if any scalar frame or prediction status differs.

Evidence files: local/browser-validation/native-original-vs-wasm.json and native-nearest-vs-wasm.json; native-original.csv and native-nearest.csv; local/native-browser-comparison.log and native-nearest-comparison.log. The older native-vs-wasm.json name is historical. The nearest result isolates rounding as the source of divergence in this fixture; it does not establish full animation/pose/controller parity against PCSX2, nor prove all drift in actual gameplay has the same cause.

Next physics requirement: preserve original directed arithmetic in WASM (including the distinction between EE scalar nearest DIV/SQRT and EE/VU chop arithmetic), then compare against the original-policy trace and the component oracles. Merely making native use nearest is a diagnostic and is not the requested fidelity fix. Fixed-step scheduling already checks 60 simulation ticks per elapsed second across 5..240 Hz render rates.


### September 12: directed arithmetic implementation verified in WASM

`engine/software_float.hpp` provides binary32 toward-zero add/sub/mul/div/sqrt while the host remains in nearest mode. Addition retains the FastTwoSum residual so an operand smaller than binary64 precision can still cause the correct one-ULP truncation. Products of two binary32 operands are exact in binary64; division uses a remainder-sign check; square root compares the squared rounded candidate. Overflow reduces infinity to the largest finite binary32 value for finite inputs. Signed zero and subnormal results are included in validation.

`sh tools/test_software_float.sh` builds an ARM native reference corpus under actual FE_TOWARDZERO, then runs the new code in a real optimized WASM module. 250,324 operand pairs (1,251,620 operation results) match bitwise, allowing different NaN payloads. Inputs include a targeted edge cross-product and deterministic random finite bit patterns. Artifacts are in local/browser-validation/float-corpus.bin and software-float.mjs/.wasm; log local/software-float-reference.log.

This implements IEEE directed operations, not all PS2 exceptional-value rules, flush-to-zero policy or EE ADD/SUB alignment behavior. Existing original_float.hpp implements the separate EE alignment masking. These new helpers are not yet called by gameplay. Required next integration is explicit rounding scopes/call sites: original EE/VU arithmetic generally chops, EE scalar DIV/SQRT use nearest, and presentation/conversion arithmetic must retain its intended policy. Do not replace every float operation indiscriminately or treat the primitive corpus as full physics parity.


### September 12: scoped directed arithmetic enabled in shared helpers

`OriginalRounding` now preserves/restores either native hardware rounding or the browser's explicit software policy. `terrain_original::Rounding` uses it; shared add/sub/mul/div and EE-aligned scalar ADD/SUB invoke the verified software operations when a chop scope is active. Default and nested nearest scopes remain nearest. Original scalar DIV/SQRT keep their separate nearest behavior. The optimized-WASM arithmetic test now also checks scope nesting/restoration and scalar-DIV exception behavior.

Full browser suite and production build pass. This is a partial arithmetic integration, not complete motion parity: direct float operators and the other controller-local rounding guards still need migration. The 600-tick scalar trace's maximum position difference changed from about 3.16461 m to 3.15700 m, speed from 8.64597 to 8.64357 m/s; seven grounded-state and two prediction-state mismatches remain. Most drift is therefore still present. Latest software primitive corpus remains 1,251,620 matching results.

The nearest-only native comparison is now a diagnostic contrast rather than an equality gate: browser software chop intentionally differs from an entirely nearest native run. The exact nearest/nearest match documented above is the pre-integration baseline. Original-policy parity remains the target, and no force/speed constants were reduced. Production was rebuilt with the shared-helper integration.


### September 12: airborne integration and trajectory arithmetic migrated

`engine/air_motion.cpp` now uses OriginalRounding and explicit shared add/mul/sqrt for position integration, drag, speed length and cap rescaling. Source scalar cap division remains nearest. Native coordinate conversion retains a nearest scope. `engine/air_trajectory.cpp` now preserves its own chop scope and explicit arithmetic for direction normalization, dot products, query expansion, interpolation, elapsed-time calculation and logic-step scaling. EE ADD/SUB remain separately alignment-masked.

`sh tools/test_air_motion_rounding.sh` compares 20,000 optimized-WASM airborne steps to native directed rounding, including speed cap cases. All position/velocity/speed/cap fields match bitwise. Inputs and expected results are retained in local/browser-validation/air-input.bin and air-expected.bin. The existing original-code trajectory oracle also passes 10,000 complete predictor/update states and ordered query requests. Full browser tests pass; production rebuilt.

Full scalar trace parity remains unresolved: max error is now 3.18652 m / 8.65145 m/s, with seven ground-state and two prediction-status mismatches. The slight increase versus the previous partial integration does not contradict component parity; ground-motion direct arithmetic and other scope migrations remain unfinished. Do not claim the integrated trajectory is bit-identical to PCSX2 gameplay from these component tests. Next major arithmetic migration is ground_motion.cpp, preserving scalar versus VU operations and integer indexing.


### September 12: ground-motion rounding migration removes metre-scale drift

`engine/ground_motion.cpp` now uses OriginalRounding and explicit arithmetic for all 106 floating multiplications, vector add/sub/cross/dot, normalization, square roots, ground integration, friction/drive, turn filters and speed limits. Integer speed-table indexing remains ordinary integer arithmetic. EE scalar alignment-masked ADD/SUB and nearest DIV/SQRT retain their distinct helpers.

Native behavior is unchanged across all 600 saved baseline frames (all float32 output fields/statuses). The source executable comparison passes 12,000 cases each for normal/compression, forward friction/braking, lateral response, forward drive, atan, crouch/brake filters and full cruise translation/postcontact across all 19 source materials and speed limits. Log: local/surface-ground-reference.log.

The rebuilt actual-WASM 600-tick scalar trace now has max position difference 0.001760523 m (1.76 mm) and max speed difference 0.001199722 m/s. Grounded-state and prediction-status mismatches both fall to zero (105 predicted-air ticks on both sides). This supersedes the prior approximately 3.19 m / 8.65 m/s mismatch. 599 frames still differ in at least one scalar field: full bit parity is not achieved, and this trace still omits animation/controllers in actual gameplay.

Full browser suite passes. Rebuilt production preview was reloaded and tested for five seconds of live gameplay: 50 samples all reported 60 fps, zero emergency rescues, and less than one simulation tick of pending time. Left paused at port 4173. This short check does not prove sustained/mobile performance or power usage.


### September 12: first-frame terrain normal discrepancy corrected

The first trace mismatch had both yaw and normal differences. Newton-refined and coarse-triangle normalization in terrain_contact_math.hpp still called std::sqrt directly, bypassing software chop in WASM. Both now use the scoped square-root helper. At ticks 0..7 the previously different normal components now match; only yaw differs (tick 4 matches all fields).

Original terrain verification passes: 998 refinements with zero point/normal/UV error, 10,000 VU0 triangle inclusion cases, 1,000 coarse-plane cases (201 hits), and 3,000 cruise candidate-selection cases. Full browser suite passes and production rebuilt.

The end-to-end scalar trace remains at 1.760523 mm max position difference / 0.00120163 m/s speed difference, with zero grounded/prediction-state mismatches. This correction removes an identified local discrepancy, not all drift. The next earliest difference is yaw, consistent with unmigrated orientation_motion.cpp arithmetic and its local Round guard.


### September 12: orientation and heading rounding migrated

`orientation_motion.cpp` now uses OriginalRounding and explicit float operations throughout quaternion rotation, slope alignment, ground heading, asin polynomial, physical basis and quaternion normalization. The migration covers 128 resolved float expressions. Existing EE scalar ADD/SUB and DIV/SQRT helpers retain their own semantics. `tools/migrate_float_arithmetic.py` produces a separate reviewable output/report using Clang AST types, including desugared float aliases; it does not select integer/double operators, and rejects compound float assignment. It is a development migration aid, not a build-time transformation. Scope changes and side-effect review remain manual. Report: local/browser-validation/orientation-migration.json.

Original executable checks pass: 20,000 cruise-heading stages, plus 20,000 sin/cos, asin, world-axis quaternion and cruise-alignment stages with zero float error. Full browser suite passes; production rebuilt.

The 600-tick scalar trace now first differs at tick 92 in field 9 (jump charge: native 0.19000327587127686, WASM 0.19000329077243805), instead of differing in initial heading. Max position difference is 0.001544251 m, max speed difference 0.000698090 m/s, with zero grounded/prediction-status mismatches. 479 scalar frames differ overall. This is not full animation/gameplay parity. Jump/control arithmetic is the next directly evidenced discrepancy.


### September 12: scalar physics trace reaches exact native/WASM agreement

Migrated 24 typed float expressions in jump_motion.cpp and 37 in landing_motion.cpp, with OriginalRounding scopes. Source-specific EE helpers remain distinct. The jump migration removed the tick-92 charge difference; the next first mismatch was the landing frame at tick 186. The landing migration removed that discrepancy. No launch/speed/force constants or landing thresholds changed.

The 600-tick scalar trace now matches all 16 float output fields in every frame, plus prediction statuses: zero position/speed error, zero scalar-frame mismatches, zero grounded/prediction-state mismatches, and 105 predicted-air ticks on both platforms. `compare-native.mjs original` now fails on any scalar or prediction discrepancy; nearest mode remains a diagnostic contrast. Current evidence: local/browser-validation/native-original-vs-wasm.json and local/native-browser-comparison.log.

Original-code validation passes 20,000 takeoff cases (zero velocity error), 120 charge ticks, and 20,000 cases each for landing contact response, crash classification, ground focus/leave/landing animation choices, and ground board/lean targets. Logs local/jump-reference.log and local/landing-reference.log. Full browser suite passes; production rebuilt.

This closes the rounding discrepancy in the scalar no-animation fixture only. It is not proof of animation/pose/controller scheduling, full live PCSX2 gameplay, UI, graphics or complete game fidelity. Other animation/control/FX routines still contain direct float arithmetic/local native-only rounding guards. The next parity harness must include the real animation/pose path and meaningful scenarios, rather than treating this scalar equality as overall completion.


### September 12: gameplay parity harness includes animation and posed contact

Added web/native-gameplay-trace.cpp, web/compare-gameplay.mjs, tools/build_gameplay_trace.py and web/compare-gameplay.sh. After rebuilding web/runtime/core.js, run `sh web/compare-gameplay.sh`. It compiles the browser host bridges natively with the original engine/controller source (not stripped generated rounding guards), restores the native graph root/sequence scopes in a separate generated native graph file, and replays identical JSON input sequences on native and WASM.

Scenarios: 600 ticks each of neutral riding, jump+grab, and steering+braking. Each frame records 267 floats: rider state16, animation diagnostics19, posed physical frame12, race8, camera9, reset9, crash12, and all26 bones x7 pose values. The sequence is race_begin -> step_rider -> animation_tick -> posed-head camera -> race_end, with the sampled pose copied before later callbacks. It initializes original animation packets, rig, physics attachment, terrain/object collision and body terrain. Environment lighting/renderer output and FX geometry are not recorded; environment texture sampling is not initialized in this harness. This is a native engine/host comparison, not a live PCSX2 capture or proof of original host scheduling.

First run: all animation diagnostic fields agree for all 1,800 frames, but all scenarios have pose/camera numerical differences from tick0. Initial posed head X is -134887.53125 native vs -134887.546875 WASM (source cm). Max rider position differences: neutral0.000610542m, jump-grab0.005535577m, steering/braking0.001660774m. Max relative pose-position differences: neutral31.108785cm, jump-grab0.892609cm, steering/braking0.399291cm.

Largest neutral pose difference is tick362, bone10: native[-0.2145357,60.2121239,92.6200104] versus WASM[-3.2422142,65.7523422,123.0813904]. Largest jump/grab difference is tick543 bone17; steering/braking tick584 bone10. Reports retain first mismatch per field group and largest pose discrepancy with tick/bone/values. Evidence: local/browser-validation/gameplay-parity.json, native-gameplay.bin, wasm-gameplay.bin, gameplay-inputs.json; local/gameplay-parity.log.

This identifies a meaningful gap beyond the now-exact scalar trace. Next investigate animation_motion.cpp and rider_pose_motion.cpp arithmetic/scopes and the leg-contact solver around neutral tick362. Do not change animation triggers based solely on this result: their exported diagnostic fields agree in these scenarios, and the pose math differs.


### September 12: pose arithmetic removes 31 cm neutral discrepancy

Migrated animation_motion.cpp (60 float expressions), rider_pose_motion.cpp (109), ground_pose_motion.cpp (59), and animation_sequence.cpp (10) to explicit arithmetic/scopes. Replaced ad hoc native rounding switches with explicit originalScalarSqrt for animation quaternion reconstruction and originalScalarDivide for layer weights and the leg cosine, preserving the original nearest exceptions. Original packet sampling/composition, leg solve, board lift/normal/lean, and sequence fade/prewind operations now honor software chop.

The AST migration helper originally counted Clang byte offsets as Unicode characters; the sin-squared comment in rider_pose_motion.cpp exposed it. Compilation rejected that output. The helper now uses byte-preserving source offsets and syntax-checks its separate generated output with the arithmetic header before success. The rider file was restored/regenerated and all subsequent builds passed. No failed binary was published to preview.

Original-code checks pass: 10,000 quaternion/mirror cases, 10,000 FK cases, 10,000 mirror translation cases, all 2,759 original packets / 219,561 scalar samples, root/board pose stages (20,000 bounce, 20,000 root presentation, 10,000 board correction), 20,000 sequence fade cases, 20,000 airborne landing-animation choices/rates, and five-/three-way cycle checks. Logs local/animation-reference.log, rider-pose-reference.log, landing-reference.log, animation-cycle-reference.log. Full browser suite passes and the scalar no-animation 600-tick original-policy comparison remains exact.

Full gameplay comparison now reports neutral max pose error 0.515728 cm (previously31.108785 cm); the old tick362/bone10 divergence no longer dominates. Largest remaining neutral difference is tick455/bone17. Jump/grab max pose error1.249246 cm at tick543/bone17; steering/braking0.177185 cm at tick580/bone15. Motion max differences remain submillimetre neutral/steering and about6.06mm jump/grab. These are remaining failures of full parity, not acceptable-error completion claims. The first reported difference is now route/reset distance, and some animation cycle/controller/host arithmetic is still unmigrated. Production preview rebuilt with these changes.


### September 12: animation cycles and airborne controls migrated

Migrated animation_cycle.cpp (11 float expressions; five-/three-way blend/phase clocks), air_control.cpp (75; airborne spin/flip/presentation), passive_air_control.cpp (4), and air_entry.cpp (34; prewind, reverse/exit stages) to explicit arithmetic and OriginalRounding. Cycle cleanup now uses scoped restoration on both normal and exception paths. Source integer choice/command calculations remain unchanged.

Original-source comparisons pass: cycle choices/weights/clocks, 20,000 airborne presentation/axis-blend cases, 20,000 direction/angular stages, 20,000 passive entries/updates/exits with ordered external calls, and 20,000 prewind target/rate cases plus 2,000 crouch latch cases. Full browser suite passes, and the original-policy scalar 600-tick comparison remains exactly equal. Production rebuilt.

Gameplay comparison after both passes: neutral max motion difference0.000015259m and pose0.139282cm; jump/grab motion0.001786777m and pose1.174327cm; steering/braking motion exactly equal and pose0.000058229cm. The first pose mismatches occur at tick305 neutral, tick152 jump/grab, tick338 steering/braking. Cycle migration removed the tick3 initial pose discrepancy. Remaining jump/grab pose differences and route/reset/camera numerical differences are still open, and no full source/PCSX2 gameplay parity is claimed.


### September 12: grab/air-selector rounding checked; residual needs finer trace

Migrated air_animation_selector.cpp (14 float expressions and scope) and grab_lifecycle.cpp (playback-rate multiply and scoped originalGrabLegWeight arithmetic). Original-source checks pass 60,000 complete air-selector cases, 20,000 leg-weight updates across animation classes, and 80,000 grab FSM cases including tweak/Uber chaining. Full browser suite and exact scalar trace pass; production rebuilt.

This did not resolve the residual jump/grab pose discrepancy: max remains1.174327cm, first pose mismatch tick152. Neutral remains0.139282cm; steering/braking max pose difference drops to0.000030037cm. These helpers now preserve source rounding, but they were not the sole cause of the remaining pose mismatch. Do not claim the grab-pose issue fixed.

Next diagnostic should compare sequence slot clocks/weights, sampled local pose and poseControls/legWeight before contact at the first mismatch, then inspect the earliest differing input. Current whole-frame trace only localizes it to final pose and is insufficient to assign the cause to the leg solver. Animation events and generated graph/native root scopes also warrant checking; avoid changing blend constants or triggers without evidence.


### September 12: pre-contact trace identifies airborne alignment discrepancy

Added read-only sampled_local_pose export and extended pose_controls_info from16 to17 floats (new index16 is current leg weight; existing prefix unchanged). The gameplay trace now records466 floats/frame, adding all182 sampled-local pose values and17 pose controls. Local-pose sampling occurs only when the diagnostic accessor is called; it does not advance the graph.

Before the alignment change, local poses matched across neutral/steering runs, while pose-control vectors differed first (neutral tick293 lateral, jump tick152 lateral). This localized the mismatch upstream of the leg solver. Migrated air_alignment.cpp's66 float expressions and native-only Round scope to explicit arithmetic/OriginalRounding. Original verification passes20,000 trajectory-gated physical orientation tails and30,000 full alignment/basis cases. Full browser suite and scalar exact-parity gate pass; production rebuilt.

Gameplay results: neutral motion, local pose, pose controls and final pose all match exactly across600 frames. Other groups such as route/reset/camera still have discrepancies, so the whole scenario is not bit-identical. Jump/grab max final-pose error0.175699cm (about1.76mm), motion0.000272958m; first pose-control mismatch tick200, first local/final pose mismatch tick221. Steering/braking motion exact, local pose exact, final-pose max error0.000016628cm; first board-lift input mismatch tick338. Reports retain all first differing groups.

Remaining diagnostic: jump/grab's sampled root rotation begins differing at tick221 (local field5), around the post-air transition; inspect root baking/air-exit/sequence-root inputs. The browser generator still strips four graph root/progress Round scopes, and host operations remain potential sources. Do not infer an IK defect from final pose error alone.


### September 12: collision normalization migrated; crash entry isolated

Thirteen remaining direct square roots in body_collision.hpp, sphere_tree_collision.hpp, obstacle_collision.hpp, world_body_collision.hpp, crash_collision.hpp and collision_event.hpp now use terrain_original::sqrt, preserving the active native/software policy. Separate EE scalar squareRoot calls remain nearest. Source checks pass body-world query/cache cases,20,000 sphere-tree/body cases, collision response/orientation/history/reaction checks, and20,000 each sliding/airborne crash body responses. Full browser suite and scalar equality gate pass; production rebuilt.

These changes do not alter the remaining jump/grab maximum errors (motion0.000272958m, pose0.175699cm). Inspecting the native trace establishes that tick191 enters hard crash semantic351/control8, beginning in airborne crash submode1; it lands into crash submode0 at198. The physical field first differs at191, while sampled root rotation first differs at221. Earlier shorthand describing this as a normal landing-animation transition was inaccurate; it is a crash-entry/recovery path.

Next trace source-space physics position/quaternion before and after originalHardCrashEnter/commit/publish, and compare crash-root inputs. Native host coordinate conversions may run inside an outer chop scope, including double arithmetic/narrowing not represented by the binary32 software helpers; distinguish host presentation-coordinate differences from actual source-state differences before editing root algorithms. Existing graph preview/scaled-root/offset helpers are called inside originalHardCrashEnter's chop scope here, so stripped inner guards alone do not establish causation.


### September 12: crash publication boundary and crash arithmetic corrected

The gameplay audit now includes20 source-motion floats (physics position3/velocity3/quaternion4 and crash actor position3/velocity3/quaternion4), bringing each record to486 floats. At jump/grab tick191 the crash actors matched exactly, but native physics position/velocity changed during publication: actor X=-140387.53125cm became physics X=-140387.515625cm. WebAssembly retained the actor value. This was a native host round-trip defect, not evidence that the original crash actor algorithm differed.

`publish_motion` now enters a nearest scope for host metre/centimetre conversions and host math, restoring its caller's policy on exit. Original source helpers retain their own nested chop scopes. Both sides of the gameplay comparator now assert that a newly entered crash publishes the actor's exact source position and velocity. This removed the tick191 source-state loss; next mismatch was actual crash quaternion at200.

Migrated remaining direct square roots in crash_ground/control/motion/recovery/detached headers to scoped arithmetic. Source crash force/contact/control/recovery/detached tests pass (20,000-case stage comparisons), full browser tests pass, and the scalar600-tick original-policy gate remains exact. Production rebuilt.

Gameplay result: rider positions exactly equal in all three600-tick scenarios; sampled local poses exactly equal throughout all1,800 frames; crash actor state exactly equal throughout. Final pose positions: neutral exact, jump/grab max0.000017714cm, steering/braking max0.000016628cm (both less than0.2 micrometres). This supersedes the prior1.76mm jump/grab pose discrepancy. Remaining first source-physics differences are velocity at jump/grab tick304 and steering/braking tick336; route/reset/camera fields still differ, so full-frame parity is not complete. Original-game host scheduling, rendering/UI, all gameplay features and PCSX2 validation remain separate requirements.


### September 12: all three full gameplay traces match every recorded field

Migrated race_event.cpp (26 float expressions), reset_route.cpp (8), and npc_path.cpp (26) plus native-only rounding scopes. Original timer ticks/integer indices remain unchanged. Source path/event/progress/reset/NPC tests pass, including human route updates and authored event sampling. This removed every non-camera discrepancy in the three gameplay fixtures.

The final camera discrepancy came from original_camera::vsqrt calling std::sqrt directly. It now uses scoped terrain_original::sqrt. The native camera test passes constructor/spline cases, five PCSX2 single-step fixtures, synthetic motion, collision lift and shake. Some PCSX2 fixture fields use existing documented tolerances, so that test is not zero-bit-error proof against the original game.

The native-vs-WASM full gameplay harness now matches every recorded float/status in all1,800 frames (600 neutral,600 jump/grab including crash,600 steering/braking). All486 fields/frame agree: physics, animations, final/local poses, camera, race/reset/crash state, pose controls and source actors. compare-gameplay.mjs now throws if any frame differs, after writing its diagnostic report. The scalar600-tick gate and full browser suite also pass. This is agreement between our native and browser implementations, not a complete original-game fidelity/completion claim.

Production rebuilt and reloaded at port4173. A six-second live WebGPU check collected60 samples, all60fps, maximum simulation debt0.0150334 seconds (less than one tick), no new rescues. Preview left paused. Full original host scheduling, authored scale, graphics/FX/UI, more courses/controls/game modes, and sustained mobile/power validation remain outstanding. Broaden the parity fixtures to resets, reverse stance, spins/flips/advanced grabs and crash variants next; do not equate the three passing fixtures with all possible gameplay.


### September 12: repeated resets and spin/flip regression expanded to3,000 frames

The gameplay input schema now accepts reset reason (-1 means none) and flip input after turn/held/brake/grab. Added600-frame repeated manual resets at ticks120/420 and600-frame combined spin/flip with grab input. Reset scenario asserts both placements and recoveries complete; spin/flip asserts actual axis blending occurs (max0.76097524). All original scenarios remain.

The spin/flip scenario initially found39 mismatching frames. Migrating ground_animation_control.cpp's11 float expressions and scopes fixed the release-rate mismatch at tick110. Original40,000 selector cases and20,000 prewind/release-rate checks pass. Remaining22 differing frames began during a later inherited blend at194.

The browser graph generator now retains its original root/progress rounding scopes and uses policy-aware operations for inherited fade duration, tick duration, scaled local root, and channel progress. Inherited fade multiplication was previously raw WASM-nearest even when called inside a chop-mode controller callback. Generation asserts each expected source expression exists exactly once. This removed the remaining spin/flip mismatch.

All486 recorded fields now match across all3,000 frames/five scenarios. Repeated resets finish twice; spin/flip blending is exercised. The full browser suite and original-policy scalar gate pass. Production rebuilt with these changes. These finite scenarios still do not prove complete original-game feature coverage, advanced trick/rail/attack behavior, visual/UI/audio fidelity, or native host scheduling against PCSX2.


### September 12: authored rail query/data bridge added; grinding not yet connected

Current native riding.hpp already contains actual rail integration (tryRailAttach, railRiderView/applyRailRider, railAccess, railMotionFrame/controlFrame), despite the older RAIL_RECOVERY integration-plan wording. tools/test_rail_gameplay.py uses authored spline_ARA1_RAIL_3007 to exercise attach/grind/leave in the native session. The browser still has no rail motion state and passive access.rail returns false. Use that working native integration as the next reference.

Added web/rail_bridge.hpp/.cpp: init_rails validates matching ARA1 SSB hash, record kind, raw source units/axes, finite curves/bounds, counts and linkage, then atomically installs171 records/777 segments. Runtime descriptor flag1 enables the source mask; file header flags are loader placeholders. Exported segment `index` is local to each rail, while previous/next are global; the bridge assigns global indices in source order and validates the chain. Original native mesh_asset.mm retains local labels in its debug index; query geometry is unaffected by that label difference. Failed loads preserve the valid catalog.

prepare.py now packages rails.json, and main.js initializes it with the terrain hash before readiness. The full gameplay harness does likewise. browserRailQuery exposes originalRailWorldQuery with source mask1/radius300cm. Diagnostic rail_info is[ready,railCount,segmentCount]; rail_query takes source cm/Z-up and returns14 floats: ready,found,packedId,globalSegment,t,distance,point3,tangent3,surface,flags. The rail vuSqrt helper now honors software chop.

`sh tools/test_browser_rails.sh` builds a native curve-query corpus and compares actual WASM results. All3,885 queries (five offsets along each segment) match bitwise; off-course miss and wrong-world rejection are checked. Existing rail helper tests pass. Full browser suite passes; production rebuilt and reload reaches enabled START. Rail attachment, control7/motion4, scoring observers and rail-specific animation handoffs remain unconnected in the browser. This adds the necessary authored query path, not playable grinding.


### September 12: ordinary rail gameplay connected

The browser now attaches to authored rails, follows original motion4, plays source control7 balance cycles, and exits into airborne/ordinary riding. New adapter web/rail_gameplay.inc follows native riding.hpp callbacks. See docs/browser-rails.md for phase ownership, diagnostics, fixture URL and material gaps.

Automated authored-rail run: attach tick1, first exit192, first-grind travel27.3386m, balance cycles present, eventual control0 landing. Live WebGPU screenshot confirms semantic18/control7 on rail31752 at60fps. A600-tick rail scenario is now part of the full native/WASM parity gate: all494 fields agree across all3,600 frames/six scenarios. Full browser tests pass.

Not complete: voluntary jumping/transfers, rotations/rail tricks/Ubers, rail scoring and grind FX/audio, control2 held-jump continuation, body/scenery/dynamic-rail interactions, and complete source host phase parity. The test/demo URL is an opt-in authored-rail start; ordinary course start is unchanged.


### September 12: Space can charge and jump from a rail

Connected the verified1162C8 crouch latch (previously mislabeled as an upper/attack callback) to rail control2, original held prewind/crouch updates, and mode4 original takeoff on release. Leaving a rail while held now hands collision/animation ownership to ordinary control2 instead of retaining rail ownership through the fall. A release-frame guard avoids double dispatch.

Automated rail jump reaches charge1 and launches exactly once at tick90; held-edge fixture hands back at194 and releases at195 without a rail impulse. All4,200 frames/seven full gameplay scenarios match native, and the existing scalar/full browser gates pass. Live test shows control2 on rail -> semantic268/control5 airborne at60fps. Production rebuilt; current rail-test tab is paused after the jump. Details/gaps:docs/browser-rails.md. Rail rotations, transfers/Ubers/scoring/grind effects and broader original-game fidelity are still outstanding.


### September 12: rail rotations are playable

Added independent J/L and gamepad right-stick-X rotation input while preserving A/D balancing. Original rail rotation/marker/stance/animation routines perform the action. Short pulse test observes exactly two source rotations and49/51 -> cycle19 transitions. Fixed active-air handoff when leaving with rotation held (control5), preserving passive4 without input and avoiding duplicate dispatch.

The full native/WASM comparison now checks501 fields across4,800 frames/eight scenarios with zero mismatches. Full browser suite and scalar parity pass. Live WebGPU keyboard test shows semantic49/control7/style3 on rail at60fps. Production rebuilt and rail-test tab left paused. Remaining rail gaps: transfers/Ubers/scoring/effects and broader original-game parity.


### September12: original rail scoring arithmetic recovered, not enabled yet

Added original rail distance/rotation/threshold scoring helpers and source/WASM conformance. All20,000 cases match original output bits, including6,299 ordered long-grind bonus events. Field24 is distance in cm; scoring is not a flat time-based increment. Source constants/table are checked against the capture. Details and integration requirements are in docs/browser-rails.md. Live points/boost still require entry/exit commit/reset wiring; gameplay has not changed in this recovery pass.


### September12: score boundary and inverted rail bonus verified

Recovered119D40 commit/reset/seed ordering with20,000 original-code cases. Extended full11A228 tests to6,000 rail-style cases and corrected the missing separately rounded inverted-points bonus; it does not alter boost reward. Existing ordinary commit/grab tests remain green. Grab/identity/commit math now preserves directed rounding in WASM. All4,800 gameplay frames and browser tests pass; production rebuilt.

Rail distance/rotation score and the new boundary helper remain unconnected to live rail awards pending exit-lifetime/caller bookkeeping. See docs/browser-rails.md; do not claim live rail scoring yet.


### September12: live rail points and earned boost

Wired original entry/takeoff boundaries, distance/rotation scoring, inverted/threshold bonuses and10E098 boost awards through rail_scoring.inc. Pending score refreshes after route progress; committed core-step points are delivered once through animation output. Fixed inactive distance defaults (-1), preventing phantom reset points. Restored passive-takeoff arithmetic on rail loss.

The first27m grind banks1380 points once; earned meter can power boost. Live HUD shows1380/pending0/positive decaying meter at60fps. Full browser tests and all4,800 native/WASM frames (509 fields) pass. Production rebuilt; rail-test tab paused after the scored grind. Score notifications/names and boost-meter fill visuals remain incomplete; current meter artwork is static despite working meter logic. Further rail tricks/transfers/FX/audio and original host/UI fidelity remain. Details:docs/browser-rails.md.


### Boost HUD display values recovered

Recovered117FE0's1188F8..118A24 stored/pending-preview widget update.20,000 original-code cases match both ordered values exactly. Added boost_hud.hpp and wired simulation-tick updates into the browser and parity trace (511 fields). Full browser tests and all4,800 frames match. Production rebuilt. Visible meter art is still static pending original sprite/colour recovery; details:docs/boost-hud-recovery.md.

The ordinary boost gauge now uses captured original coil/stem draw geometry and live smoothed stored/preview values. See `docs/boost-hud-recovery.md` for regeneration, source comparison and remaining flash/letter/pixel-fidelity gaps. Geometry check: `node web/test-boost-gauge.mjs` (requires the local original draw capture).

Original SUPER/UBER glyph placement and ordinary activation/pop rendering now replace the clipped placeholder words. The source-tested letter timing runs per simulation tick. Flash, removal transitions and font shadows remain incomplete; current live visual verification covers inactive letters only.

### Ground steering direction fix

Normal ground steering now maps positive keyboard/gamepad/touch steering to camera-right through the browser host's ground controller input. The old extra negation made controls appear reversed. Applied the same sign to grounded soft recovery and attached ground animation selection. Rail balance, airborne steering and jump prewind inputs retain their mappings. `test-steering-direction.mjs` and `test-steering-gameplay.mjs` check left/right displacement against neutral, including the real chase camera at the normal start and on ordinary ground after the first rail; both run in npm test. Avoid testing the later rail fixture at tick320 as ordinary ground—it has reattached to a rail there. Full browser tests and4,800-frame native/browser trace pass. Passive-air filter test now asserts nonzero magnitude instead of assuming the old ground-input sign. Production rebuilt and current demo tab reloaded.

### Ground/air input audit and missing jump-camera input

The ground-only sign fix was insufficient: main.js still negated steering into animation_tick, reversing visible airborne rotation/prewind relative to camera-right. main now passes positive-right steering directly to the animation controller; removed the compensating negation previously added only to attached ground animation selection. Native/browser trace and gameplay regression drivers use the same convention. Low-level source controller helpers remain unchanged. Rail balance has its separate mapping and still requires independent screen-relative audit.

`test-air-steering-direction.mjs` evaluates the rendered rider orientation (physical quaternion multiplied by air presentation quaternion), projected on the chase camera's right axis. It verifies both left/right inputs with and without ground prewind. Both pass, as do camera-relative ground tests at normal start and after the first rail. The prior sign made leftNose+0.9576 and rightNose-1.0119; corrected mapping reverses those to the requested visible direction. Included in npm test.

Camera audit found launchValue permanently0, suppressing DEFAULT_3's source takeoff ramp300..500. `originalJumpCameraLaunch` ports114B78..114C6C and exposes rider+5A4 from takeoff. The existing20,000-case original jump oracle now also checks this field bit-for-bit, all passing. Browser retains the launch value at ground/rail departure and passes it plus physics riderType into the original chase camera. camera_inputs index13 exposes launchValue. Charged-jump integration test requires it>300. Existing camera prediction and landing splines remain in use. This corrects a missing input, not every camera feature: wall/proximity input, alternate algorithms/blending, full original-game visual comparison and host scheduling remain open. Full npm tests and4,800-frame native/browser parity passed; production rebuilt.

### Expanded user objective

Preserve original executable behavior, optimizing only for the web platform. Remaining explicit requested areas: snow animations/particles; clipping and collision mesh; friction and speed; Uber system end-to-end; lighting/shading; sporadic backgrounds; enabled powerups. These remain requirements, not completed claims. Prioritize source-derived implementation and gameplay-visible verification over standalone arithmetic-only coverage.

### Sky alpha/pass correction

SKY metadata marks textures283..286 `has_alpha=true`, but the browser created all sky materials as opaque. These batches now enable transparency. Sky renders in a separate scene before the world, with depth writes/tests disabled as before; the world pass preserves the resulting color buffer. This is necessary because Three's transparent queue runs after opaque world geometry even with a low renderOrder. A single mixed scene would let camera-centered sky layers blend over the rider/course. Menu rendering retains transparent canvas behavior. Production build passed; a WebGPU gameplay screenshot showed sky behind the course/rider, then game paused.

This fixes a host renderer issue, not every reported background problem. Original per-material PS2 blend modes, sky layer order, scenery streaming, fog transitions and exact screenshot comparison still require verification. No new sky art or alternate scenery was introduced. Physics/core unchanged.

User clarified that the particle-effects scope explicitly includes boost effects as well as snow. Both remain tracked requirements; existing snow/wake and HUD glow work is not a claim that boost particle effects are complete.

### Attached Uber investigation and air-stance input fix

Existing test-uber-animations exercises all advanced chords with physics detached and an explicit tier context; it does not establish earned Tricky/Uber/landing progression. `web/probe-earned-uber.mjs` now attempts that path through attached physics and rail scoring and exits nonzero unless it actually earns Tricky, enters an advanced semantic, lands and advances tier. It is an unresolved diagnostic, not included as a passing npm test. Current fence3012 spawn does not earn boost; the original short3007 probe also did not reach Tricky. A gondola-rail attempt raised OriginalAirTrajectoryUnavailable due to incomplete world query. These failures must not be described as an Uber gameplay pass. Log `local/browser-validation/earned-uber-probe.log`.

Investigation found that the attached air-animation selector hardcoded reverseStance=false despite maintaining the actual state in gs.reverseStance. It now receives that state, preserving source mirrored spin/air-adjust animation choices. Air-control direction helpers are unchanged. Full npm regressions and4,800-frame native/browser comparison pass; core and production builds passed. Earned-Uber gameplay verification and the gondola query coverage gap remain open.

### Gondola query failure identified

`OriginalAirTrajectoryHit` now preserves host coverage cause/resource diagnostics. World segment queries distinguish missing world(1), unsupported instance(2), and capacity exhaustion(3), retaining the first cause. Original trajectory failure reports that identity/mode; it still throws/catches the same unavailable-query type and never converts unsupported geometry into a miss. No physical contact-selection behavior changed.

Reproduction: `SSX_RAIL_PROBE=spline_ARA1_GondolaRail_1 SSX_RAIL_ROTATE=0 node web/probe-earned-uber.mjs`. Current failure: cause2/resource30472/mode0. Manifest maps30472 to track8/rid119, `mdl_ARA1_crashbag_0_0002`, dynamic flags40210000, type3 and surface-1. Original334888 checks an attached entity's override before ordinary node filtering, so simply skipping it based on surface/type would be unproven. Next recover that dynamic callback or its proven ray behavior. With periodic rotations the rider takes a different path and does not reproduce this collision failure; preserve the neutral-input reproduction. Full browser suite passes; production rebuilt with diagnostics. Earned Uber probe remains failing/unverified.

### Crash-bag ray capability traced

Captured crashbag_0_0002 instanceF46E80 has entity0 and type3 sphere geometry. Its authored handler row has only slot2 populated (program49); bytecode is exactly builtin0(), builtin15(), return. Builtin0 creates Object node356DB0 (vtable490E80), not DeadNode as an early hypothesis suggested. Its override predicate356A28 delegates to the attached modifier. Builtin15 creates the35DA70 particle modifier (vtable48F080); its override predicateA4→360BC8 is a literal zero-return, as is queryAC→360BD0. Thus this known object/particle callback path does not replace the type3 ray callback32E688, also a literal zero-return.

`tools/probe_crashbag_ray_capability.py` verifies the ELF table/stub words and exact authored scripts, identifying13 type3 instances with this capability. Evidence `local/browser-validation/crashbag-ray-capability.json`. This supports a precise per-capability ray policy, not ignoring arbitrary dynamic objects. Production query still uses the old blanket unsupported classification; next integrate this verified ray capability while retaining missing body/destruction behavior as an explicit separate gap. No body collision or break FX is claimed implemented.

### Verified crash-bag ray behavior integrated

`tools/apply_ray_capabilities.py` reruns the executable/script capability checks, then annotates exactly13 matching ARA1 instances in private native/browser collision packages with `sphere-tree-no-override`. Both loaders validate the policy/type and store rayAlwaysEmpty independently of unsupported body behavior. Trajectory queries return the original empty type3 ray result for those capabilities, while retaining explicit incomplete results for other dynamic resources. Re-run this tool after regenerating world_collision.json; web/prepare.py preserves the annotated native package.

The neutral GondolaRail_1 reproduction now completes its3600 simulation ticks without the previous resource30472 ray exception. It still fails the separate earned-Uber success condition, so Ubers are not declared verified end-to-end. `tests/ray_capability.cpp` confirms both query modes, unchanged unsupported body contacts and continued failure for unrelated dynamic objects. Native/browser loader parity, full npm suite and4,800-frame gameplay parity pass. Core and production builds passed. Crash-bag body collision, breaking behavior and particles remain unfinished; the fix is strictly the source-proven ray behavior.

### Snow arithmetic portability

Snow emission and particle math still used raw float operations plus local fesetround scopes, which cannot reproduce native FE_TOWARDZERO behavior in WebAssembly. Migrated98 emission expressions,44 particle expressions and the impact-context square root to existing explicit terrain/source arithmetic helpers, replacing local Round structs with OriginalRounding. Reports: local/browser-validation/snow-{emission,particles,context}-arithmetic.json. Original algorithms, constants, textures and birth/visibility rules unchanged.

Original emission suite passes (including20,000 impact and20,000 trail/cloud requests); original snow-context/impact40,000 cases pass; original particle kernel/visible-traversal/VU tests pass, including2,000 A00 cases. Logs local/snow-{emission,particles,context}-portability.log. Browser regression suite and production build pass. This establishes preservation against those original native oracles and explicit WASM arithmetic use, not full framebuffer/pixel parity or all ten emitters. Rock/breath/kicker and boost particle effects remain unresolved; wake math still needs its own portability audit.

### Wake portability and expanded FX regression trace

Replaced four remaining std::sqrt calls in wake_input/row/control with explicit scope-aware square roots. Original wake input20,000, row20,000 and control30,000 cases still pass (logs local/wake-{input,row,control}-portability.log).

Native/browser gameplay trace now records23 snow diagnostics,13 wake diagnostics and two16-bit halves of a32-bit FNV-1a fingerprint for each of10 particle buffers plus the wake vertex buffer. Total580 fields/frame. Hashes cover every byte of each populated buffer, not just first/last particles; this is a regression fingerprint, not a collision-free mathematical proof of equality. The first expanded run found27 mismatched frames, all BodySnow buffer fingerprints. snow_crash.hpp still used its own fesetround wrapper and two raw float multiplications. Replaced with OriginalRounding and explicit multiplies; original BodySnow20,000 cases pass (local/snow-body-portability.log).

After the fix all4,800 frames across eight scenarios match, including FX diagnostics/fingerprints. Observed maximum live counts by emitter0..9:71,40,246,0,0,0,19,0,0,47; max wake vertices288. Thus this gameplay trace exercises emitters0/1/2/6/9 and wake geometry, not live output from every emitter. Full npm regressions and production build pass. Missing emitter triggers, boost FX and pixel-level comparison remain open.

### Authored powder particle coverage

CloudySpray's zero count in the old replay was not an initialization bug: original surface0 disables cloud and large-impact emission. Authored surfaces1/2/3/13 enable clouds, and2/3/13 enable large impacts. Added `web/test-powder-snow.mjs` using the center of original ARA1 powder patch119816 (surface2), with carving, braking and charged jump inputs. It verifies finite particle buffers and nonzero cloud/large-impact counts: peaks71 cloud and8 large impact in360 ticks. No surface flags, probabilities or appearance constants were changed to force emission.

Added the same powder-snow scenario to native/browser full gameplay comparison, now5,160 frames across nine scenarios and580 fields/frame with all FX-buffer fingerprints. All match. Full npm suite passes. This adds gameplay coverage for emitters5/7 beyond the previous0/1/2/6/9 coverage; rock/breath/kicker remain inactive/unimplemented in the browser adapter. Renderer/pixel comparison on the powder route remains a separate check; this test establishes simulation emission and buffer consistency.

### Ground crash camera feedback and shared particle RNG

Source audit of12D160 found12D218 calls15E360 (camera requestShake), index4, fade0, scale=clamp(speedCmps*.03599999845/100,0,1). It is gated by rider+870>=0 and rider+87C!=0. The recovered callback had been mislabeled `rumble`; browser converted it to a discarded observer number and also left the device gate at its disabled defaults. Renamed it `cameraShake`, wired captured original_reset device_index/device_enabled through BrowserCrashHost, and queue the request into the DEFAULT_3 compositor before its tick. Rider reset clears the queued request. This is grounded crash-recovery feedback, not a newly invented ordinary landing impulse. Native PrototypeRider's frontend callback remains a no-op; the browser implementation is connected.

The browser chase camera now receives the same six-word3177F0 random state used by snow particle births instead of the camera's private fallback. This does not prove original scheduling or complete RNG parity with all original producers, several of which remain unported.

camera_inputs retains indices0..13 and adds14=shake index,15=active request,16=fade timer,17=cumulative crash-camera callback count. Crash gameplay tests require active index4 feedback in all three recovery fixtures. The original crash oracle now also checks the source request's index4/fade0 contract:20,000 air and20,000 ground recovery cases pass. Full browser tests and production build pass. The5,160-frame native/browser trace matches all580 fields per frame, including camera and particle-buffer fingerprints. This is implementation agreement; no new continuous PS2 jump/landing framebuffer comparison has been completed. Ordinary jump camera phase scheduling, wall/proximity inputs, alternative algorithms, and full visual fidelity remain open.

### Terrain-edge passive takeoff restored

The browser ground-departure branch previously entered air motion without114298. Original13F178 explicitly invokes114298 with charge-1 at13F194..13F1A0 when the ground controller's departure flag is set. The browser now calls the recovered routine with the current source position/velocity, contact normal, previous normal, surface-forward vector, speed limit and rider type before its existing air transition. It preserves held crouch. The routine supplies the camera launch value directly; charged ground jumps likewise retain the returned value instead of recomputing after host coordinate conversion. This restores source passive impulse/projection/speed-limit behavior. Existing host body-contact/animation scheduling is unchanged and is not established as fully source-identical by this fix. The existing browser takeoff flags remain73; generalized rider flag-state recovery remains separate.

Source trace also narrows the previously generic description of rider+5AC:13AF28 clears it at each rail-motion tick, queries within300cm using the board bone, then sets it only when the hit instance has an entity whose virtual170 predicate returns nonzero. This is not simply an arbitrary nearby-wall test. Static authored spline queries have no such instance entity. Do not populate the flag from a generic collision-distance heuristic. Current dynamic rail entities remain unsupported.

Validation for passive takeoff:20,000 original takeoff cases (including negative-charge cases) match position, velocity and camera launch value;120 charge ticks match. Full npm suite and production build pass. The5,160-frame,580-field native/browser trace remains exact. Continuous PS2 gameplay/visual comparison is still outstanding.

### Breath in gameplay

Connected original2E1120, class12 environment transitions/quadtree and head bone5 to emitter4; original brth indexed PS2 texture is imported and renderer enabled. Low-speed test passes with peak2 particles; full suite and5,160-frame native/browser parity pass. Live startup/menu/race check passes after fixing the missing texture binding. See docs/breath-recovery.md for evidence, scheduling/reset limits and remaining close-up PS2 comparison.

### Original rock spray connected

Added originalRockSprayEmission (2DFE88) with the surface6C chance, original ground gate, board-forward15cm spawn+jitter, speed/brake/skid/turn chance, and ordered velocity/normal/side components. The routine retains inactive emission argument updates and the original LCG draw count.20,000 direct original-code requests match position, velocity, colour, active status, dt and random state (3,142 active births in the current fixture). Breath and existing source snow suites also pass.

Browser emitter3 now runs between chunks and snow trail; renderer enables its already-imported texture14. No surface chances were changed. Authored surface7 patch100104 gameplay fixture produces peak9 rock particles over360 ticks, with finite buffers. Added the same fixture to the native/browser comparison:5,520 frames across10 scenarios,580 fields/frame including camera/particle fingerprints, all exact. Full browser suite and production build pass. This is implementation agreement and authored gameplay emission coverage; close-up original-game pixel/texture comparison remains outstanding. Kicker8 and boost effects remain missing.

### Kicker carry-off snow connected

Recovered2E1F70 as originalSnowKickerEmission. FX+10 builds by.0100000007 each call on eligible ground, capped by surface+5C (2/3/4/5 on authored snow types), otherwise decays1/60. Emission occurs during carry-off, with original277.77777cm/s speed and-3333.3335cm/s vertical gates, buildup alpha/descent fade, board-forward velocity components and2DE398 jitter.20,000 direct source calls match all requests, RNG and buildup state (8,758 active in the deterministic fixture).

Browser restores captured1.36999869 buildup with its existing snow reset path, imports the original surface capacity and runs emitter8 after BodySnow. General original reset/constructor timing remains a broader lifecycle question. Renderer now connects all10 rider DynamicSpray families using existing authored texture bindings; separate boost effects are not part of this completion.

Added web/test-kicker.mjs:peak29 particles and65 airborne frames with kicker particles in a300-tick charged-jump fixture. Replaced the old test-snow placeholder expectation that emitters3/4/8 stay empty with explicit packed-snow no-rock and carry-off emission checks. Full browser suite passes. Native/browser comparison remains exact across5,520 frames/10 scenarios/580 fields, including FX fingerprints; production rebuilt. This does not establish original-game pixel/host-scheduling parity. Boost particles, collection/lifecycle of powerups, and the full goal remain unfinished.

### Speed/trick boost collection enabled

All five authored ARA1 boost pickups now collect from posed-body contacts, award the original type/amount once, hide their owned geometry, and remain consumed until a new race. No fixed-radius pickup check. All five gameplay fixtures pass; full npm tests and6,270-frame/606-field native-browser trace pass. Production rebuilt and browser fixture smoke-tested. See docs/pickup-recovery.md for source evidence and remaining script/feedback/scheduling/other-collectible limits.

### Updated gameplay priorities and visual boost recovery

The active user objective now explicitly includes grinding quality/performance, Uber grind tricks, and trick-list naming with rotation/flip degrees. Preserve these alongside the existing physics, particles, lighting/background/collision and powerup requirements.

Confirmed implementation gaps: engine/rail_motion.hpp originalRailControlStep rejects nonnegative trick identity (source132620/control12 path); the browser rail adapter does not dispatch that Uber path. HUD main.js currently derives trick text only from scoreLabels[animationInfo14], while originalTrickRotationScore and packed identity machinery already track spin/flip state. Original name formatting and the list must be connected, not replaced by guessed UI text. Prioritize these user-reported gaps next.

Boost visual parameter recovery is now source-tested in engine/boost_effect.hpp (20,000 cases). See docs/boost-effect-recovery.md; geometry and renderer integration remain open. No browser behavior changed by this parameter-only pass.

### Rail attachment anchor and captured tolerance fixes

Browser rail_view had used cachedCrashWorld23 (board mesh) for rail query/proximity/style selection. Original108A48/1086B8 use rider8A0, captured22 (board root); corrected the anchor and quaternion. It also discarded the captured attachment tolerance by clearing all rail triplets to zero. New tools/reference_rail_context.py and original_rail_context import the starting board-root index, query offset9D0 and steer/balance/tolerance triplets. Tolerance is(current1,rate0,target1), not0. The browser restores these on its existing rail reset path; this is captured initialization, not invented tolerance tuning. Source width/reach formulas are unchanged.

Full browser tests and6,270-frame native/browser comparison pass; production rebuilt. This does not prove all rails attach correctly: dynamic/model-only grindables, host phase ordering, transfer/impact tolerance restoration and wider route coverage still need investigation. User priorities remain Uber grind support, original rotation/flip trick naming, grind performance, and broader visual/physics fidelity.

### Original trick names, spin degrees and flip combinations in HUD

Located116950, the original packed-identity formatter. engine/trick_name.hpp concatenates17 source text-table fields in exact bit/order sequence; tools/export_trick_names.py exports the English strings, including spin degrees, flip counts/types, named combinations, grab/late transitions and rail styles.20,000 original formatter calls match byte-for-byte. Invalid/unmapped indices fail rather than synthesizing names.

Browser trick_name previews from a copy of the live source scoring identity, applies the original named-trick mapping and preserves the actual committed identity name after landing/rail commit. main.js now reads that text instead of deriving names from animation filenames. Name reads leave gameplay scoring unchanged. Original strings use counts for repeated flips and degrees for spin components; no raw Euler-degree label was invented.

web/test-trick-names.mjs exercises live spin, flip and mixed-control naming. The short ordinary jump reaches BS180/BS360 but insufficient flip angle for a full flip; the flip fixtures use the existing trick-boost reward to exercise Front Flip, BS Front Flip360, BS Misty and BS Misty720 without lowering source thresholds. Full npm suite and6,270-frame native/browser physics/FX/state comparison pass; production rebuilt. Full multi-row trick-list layout, source HUD timing, all visual text cases and Uber grind support remain incomplete.

### Rail Uber entry recovery

Ported/tested132620 entry gate with20,000 original state/callback comparisons. Located control12 entry136268, update136508 and four36-byte animation rows at45A038. The source clips are present; kind10 balance states (218/226/234/242) still lack a browser driver. See docs/rail-uber-recovery.md. No Uber grind gameplay activation is claimed yet.

### Uber grind balance driver connected

Reused the existing source-tested rail_animation.hpp kind10 implementation in the shared/native-generated browser graph. All four balance states now enter and seek the original left/right poses without advancing their clock or flipping for switch stance. Native oracle, WASM driver checks, full npm suite and6,270-frame parity pass. Production rebuilt. Live control12/input/scoring integration remains open; see docs/rail-uber-recovery.md.

### Jump/landing camera follow-up

User clarified that jump and landing movement is the main camera complaint. Replaced the obsolete unposed check-camera.mjs with two 900-tick full motion/animation/head-camera scenarios: charged release and held ledge departure. Both exercise flight and a grounded landing, finite camera/projection output, and one camera tick per physics tick; charged launch requires the recovered amplitude input. Included in npm test. These are integration safeguards, not a continuous PS2 camera comparison or a new camera behavior fix. Ground/air screen-relative steering tests remain passing. The next camera fidelity work must compare original host timing and full jump/landing trajectories before changing smoothing or adding an invented landing impulse.

### Restored wall-launch camera direction

Original114998 stores the normalized horizontal takeoff normal into rider+3C0 unconditionally, including the +X fallback on a flat surface. The jump port computed that vector but discarded it; browser camera input wallNormal stayed zero. OriginalJumpState now exposes cameraWallNormal, checked bit-for-bit against the source instruction oracle in20,000 takeoff cases, with explicit vertical-normal cases. Ground charged/passive and rail charged/passive calls retain it for the chase camera; rider reset clears it. camera_inputs adds indices18..20 for the source vector without changing earlier indices.

Four synthetic wall directions exercise the recovered swing and landing branch; the missing-vector comparison differs by up to377.4cm in eye position for the +/-Y cases. Zero input still produced finite output in these tests, so this is an incorrect-direction fix, not a demonstrated NaN/crash fix. Full browser tests, the existing camera source fixtures (with their documented tolerances), and6,270-frame606-field native/browser parity pass; production rebuilt. Ordinary jump/landing camera feel still needs continuous original-game comparison. The native PrototypeRider frontend has separate camera input plumbing and is not established fixed by this browser hookup.

### Preserve source takeoff position and rail camera amplitude

Charged ground jumps now consume OriginalJumpState.position as well as velocity. The previous host discarded the source position (including the horizontal four-centimetre wall separation) and unconditionally added four centimetres in world-up. Removed that invented lift. The charged call now supplies source previousNormal separately from the current contact normal, plus riderType, as the original114298 fields require. Existing source takeoff arithmetic is unchanged.

Rail leave no longer recomputes cameraLaunchValue after13BFA8 exit response/speed clamp. It retains the value already produced inside114298; the rail callers all retain it before leaving. This preserves original ordering and works with the horizontal camera direction restored in the preceding change.

The speed-limit integration fixture landed at tick186 and exposed its fixed0.01cm projection tolerance being smaller than one float32 position ULP at the course origin. Changed the double-precision projection comparison to a coordinate-derived one-ULP bound (0.015625cm here), retaining tangential position, material depth, contact distance, authored contact, speed reduction and cap assertions. The measured difference was0.013026cm; no gameplay collision tolerances changed. Native/browser comparison matches6,270frames across606fields. Ordinary camera visual parity and the full original host schedule remain unproven.

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

### Boost FX preparation

Recovered the original20-slot paired side-history updater and verified20,000 instruction cases; extracted all five owned32x32 boost ribbon textures into SNOW_FX with executable ID-table verification. Fixed short indexed CLUT decoding while confirming the existing six snow textures/profiles remain byte-identical. Production assets rebuilt. Main30-entry ribbon geometry and renderer binding are still missing, so there is no new visible boost effect yet. See boost-effect-recovery.md for exact source boundaries and limitations.

### Main boost ribbon geometry recovered

Complete source2E6C08 history/endpoint/trimming update now lives in engine/boost_ribbon.hpp.20,000 full original calls match, in addition to stage-specific checks. Corrected boost parameter fields+14/+18 to integer primaryCount/float primaryDistance; parameter and side-history oracles still pass. Drawing2E7A10 and browser pose/camera hookup remain unfinished, so this turn does not add a visible boost ribbon or change the playable runtime. See boost-effect-recovery.md.

### Terrain texture-axis correction and expanded event scope

Corrected the importer UV corner order to match source2EDB20 and repaired137,736 base-UV vertices across1,913ARA1patches. All other vertex bytes remain unchanged. The live preview serves basis2. See event-demo-recovery.md for the new original-rider/start/finish/timing/variant requirements, verified gaps, and repair evidence. NPCs remain filtered from the current race loader and the timer still starts from captured tick158; neither is claimed fixed by the UV change.

### Fresh human event start is live

Normal demo starts now use the verified original grid, PreRace/Countdown clock, control6 start pose and source-controlled push-off. Timer and progress begin at0; W/S or vertical pad input affects anticipation; restart returns to the grid. Full browser tests and9,630-frame native/browser comparison pass. Original numeral sprites are used, but countdown tint/GO/audio/intro-camera fidelity remains open. Other original riders/opponents, complete finish/results and event-specific world activation remain missing. See event-demo-recovery.md.
