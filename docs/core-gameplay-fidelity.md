# Core gameplay fidelity — active goal

> Current integration checkpoint: [Agent handoff — 2026-09-09](HANDOFF.md). The measurements and research below describe their recorded stages; older missing-feature notes and test counts are not the latest app status.

The goal remains: **get the core game working and perfectly matching emulator gameplay**, in the native Metal application. The following measurements establish progress, not completion of that goal. Sam remains a later character integration after the core game works correctly.

## Current evidence

The native engine now runs recovered input response, ground force assembly, heading/orientation, terrain contact, dynamic speed limits, jump launch and airborne translation when supplied with the original data profile. ARA1's `riding-start.json` supplies a verified Snow Jam development start to the ordinary app and R respawn. It is not the finished event initialization flow. Other locations still need their authentic initialization/profile handling.

Two comparisons must remain distinct:

1. **Requested emulator pad timeline.** P2M2 bytes describe what the host was asked to deliver. The original game may consume those commands on a different logic tick.
2. **Game-accepted command timeline.** SSX3's own RLE recorder at motion-owner+0xDF8 preserves the actual decoded commands. `tools/reference_input.py` verifies the baseline prefix and hashes the expanded stream. `tools/canonical_replay.py` builds a native comparison using that stream while retaining the requested pad timeline as provenance.

`tools/reference_suite.py --accepted --output local/native-qa/reference-suite-accepted` runs an independent accepted-command comparison for each captured endpoint. This avoids assuming that separately captured movie prefixes had identical input-delivery timing. The native telemetry's accepted command words, decoded controls and stream hash are checked independently.

The latest stable report is `local/native-qa/reference-suite-stable-race-air/comparison.json`. All14 captured endpoints have **exactly equal position, velocity and physical quaternion components**: glide1/30/120, left30, brake30, jump31/60/90, already-airborne29/59, isolated60/120, capped1 and air-spin30. The recovered race clock, path index/cache, remaining/best distance, finish timer and checkpoint masks also match at every endpoint. Every available airborne angular-control field matches, including explicit ground-to-air jump entry.

The predictor has one recorded difference at jump31: inactive `hit_position[0]` retains original bytes interpreted as8.437386209515443e-39, while native initialization used zero. Its active trajectory data agrees, and later prediction records agree in all compared fields. This retained-state distinction remains visible in the report; full hidden-state bit equality is not claimed.

The production rider owns the race session: clock dispatch before riders, finish timer update before motion, progress/course events after motion, total tick increment last. The development HUD shows this clock and splits; R respawn reloads the matching starting state. `OriginalRaceSession` also passes a controlled traversal of the authored route with two splits, one finish and the results-state transition. That traversal supplies kinematic positions and is not a full-course physics or opponent test.

Snapshots seed original animation sequence inputs, all recovered ground control triplets, prior contact/board normals, air-entry profile/prewind state and the19-material catalog. Expected bone arrays are excluded from runtime inputs. Zoe's native body pose is present through glide, turn, braking and grounded jump charge. Airborne frames explicitly clear stale grounded body data and report the missing air pose. Board normal390 is sampled before collision and filtered afterward; phase ordering is not replaced by fitted offsets.

The suite fingerprints both course and Zoe assets and removes inherited SSX environment overrides. Runs affected by concurrent binary/asset changes are rejected; changed-file hashes now accompany rejection diagnostics. The stable report above was produced after all agents held app/scene binaries and those assets fixed. Comparisons retain small discrepancies instead of rounding them to four decimal places.

Validation for this build:14 CTests and43 Python tests pass; the race oracle passes92,000 original-code cases, and the regenerated ground oracle passes12,000 cases per covered kernel. The app links directly to Apple frameworks/system libraries; no console runtime is linked. Separate original-instruction oracles remain development tools.

These are short Snow Jam cases for the captured original rider state. They do not prove full-course, collision, trick, camera, race or progression fidelity. Numerical zeros are measured equality at those checkpoints, not a claim about every frame or branch.

## Verification still required for the full goal

| Requirement | Evidence needed | Current scope / remaining work |
|---|---|---|
| Native macOS engine | Built ARM64 app with direct Metal and no console CPU/GX runtime | Native build exists; keep checking dependencies after integration |
| Faithful basic riding | Accepted-input traces over full routes, slopes, materials, stance changes, boost, launches and landings | Short ground/launch/air position and velocity checkpoints match exactly; full routes, landing, passive-flight controls and all transitions remain |
| Authentic terrain and obstacles | Original geometry/selection rules, posed collision bodies, responses and events | Terrain and static authored collision queries recovered; uniform instance scale published across all6 areas; native posed-body integration works on supported branches; collision event effects and dynamic callbacks remain |
| Correct characters and animation | Original outfit/rig/clip/blend/clock state; matching poses and bone positions | Original Zoe rig and497 basic clips recovered; native sequence blending and posed collision bodies integrated; broader transitions, airborne poses and final residuals remain |
| Spins, flips, grabs and tricks | Original air-control responses, trick recognition, scoring, Uber rules and landing outcomes | No-grab air spin/flip controller and landing predictor recovered; explicit-jump entry integrated and compared; passive-flight control4 remains separate; grab/trick recognition/scoring and landing response remain |
| Camera and visual appearance | Matching camera state, scene transforms, effects, visibility, lighting and animations in comparable captures | Camera/effects/material appearance remain provisional; authored instance scale correction published |
| Playable events and opponents | Selection/start/countdown, AI, timing, checkpoints, finish/results and reset behavior | Clock/path/checkpoint/finish session connected to native rider, short progress comparison exact; genuine ready/countdown spawn, results UI, other event modes and opponent AI remain |
| Complete game systems | Audio, menus, progression and save/load exercised through playable events | Unfinished |
| Broad conformance | Repeatable integrated emulator/native runs covering the above, including long runs and failure paths | Current narrow test suite is necessary but insufficient |

## Reference harness notes

PCSX2 v2.8.2 increments its movie frame counter before overriding controller data and switches to recording mode at its declared final frame. Some captured pad-edge histories therefore differ at the boundary. Do not fit a fake fixed delay to the captures. Prefer the game's accepted RLE commands for physics conformance.

A frame-advance binding on keyboard B was added experimentally through the PCSX2 UI; its main-window automation was not reliable and is not part of the trusted harness. The previous INI is saved privately as `local/reference/pcsx2/PCSX2-before-frame-advance.ini`. No original ISO was changed.

Original-instruction test oracles also need scrutiny: the development recompiler emitted scalar `SQRT.S` using the wrong source operand. The affected tests now use opcode-guarded corrections in temporary oracle copies. See `engine/ORIENTATION_RECOVERY.md`; generated game files and the original executable remain unchanged.

The reference harness captured `snow-jam-countdown-anchor.p2s` (phase4,count162,total18) and its neutral1 continuation (count161,total19), matching the original clock. Boundary movies161/162/163/164 are prepared from that same anchor. PCSX2 loaded161 and remained paused; accessibility calls then timed out although process84980 was live in its normal event loop. The UI connection was retried/reset without restarting the emulator; remaining boundary captures are pending.

Long240-frame captures already exist, but the semantic accepted-command decoder rejects unobserved landing/passive-flight state transitions. Do not infer a landing solely from a zero word: zero is also a valid airborne grab. A source-faithful state-aware replay or additional verified boundary states is required before using those runs for conformance.

Current continuation priorities include nonneutral airborne/grab animation, soft collision control3, actual crash/recovery motion2 and reset motion3, ready/countdown initialization and opponent/event/UI completion. Control3 is also the original soft-collision controller; the long-left240 word0x04308000 does not establish that the rider was on a rail. A middle snapshot or source-backed lifecycle trace is still required to resolve that transition.

## Later integration evidence

- Runtime-state raw replay now decodes original command words through the native controller. It preserves the original stream through ambiguous landing words and exposes unsupported state branches explicitly. Original-provider30,000-case testing corrected the live air-adjust LR/FB mapping; the full angular oracle remains exact.
- Air pose replay checks now cover full charge→31/60/90 and air31→60/90. All24 body/board transforms and main sequence clocks/fades match; snapshot tests include27-bone local transforms and reversed sequence-root/mirror data. Completion events are processed after local-pose sampling, as in11EB60/312490.
- Clean landing now uses the posed board, kind2 cache864, verified contact/exit/damping/classification and normal-controller return. Separate body cache868 is restored with querykind validation. The long240 run lands on originaltick489 afterleaving428 and returnscontrol0. Generated poses remain present through its landing/default continuation.
- Long240 still differs by3.149465912m and2.406189407m/s. Retained trajectory fields and stored impact normal speed match, so flight translation up toimpact is supported independently. This is not complete landing/ground-continuation parity.
- The original long240 snapshot records a rider-to-rider impulse at tick522 between human and opponent slot3, on both actors' collision tables. Native opponents are not simulated yet. See `local/native-qa/retained-landing-checkpoint.json`; this concrete missing interaction must be implemented before attributing the endpoint gap only to landing math. No values were fitted to that endpoint.
- Boost control and meter/timer lifecycle are connected and match60,000 original component cases. Rewards/score/audio/haptic consumers remain pending, not replaced by invented meter gains.

Current work is native opponent initialization/input, authentic rider-pair contact, and actual opponent visuals. Original128AF0 updates all selected riders once per phase; complete-one-rider-then-next stepping would be wrong. A shared phased world update and global RNG are needed. NPC provider10A768 has no human-style accepted recorder; its DF8 contains NPC state, not a recorder pointer.

Latest joined baseline after landing/boost/cache/pose integration: `local/native-qa/reference-suite-landing-boost/comparison.json`. All14 position/velocity/physical-quaternion/race/boost/prewind checkpoints match; airborne angular fields match. Native body pose is available in13 of14 cases; the spin-specific pose branch remains explicitly missing.14 CTests pass after current profile/state golden regeneration. The broader four-second result is still not a conformance pass.

Shared-world requirements verified from128AF0: build the selected participant list, refresh manager proximity10F560, then loop over all actors for120F20,121068,1210B0,1211F8,1216E0,121700,121728,121750,1217F8,121818,1218D0,121950. Local-pose sampling and completion events precede world-pose generation. Pair overlap uses current native spheres; reaction points use proximity refreshed every6 manager ticks. Player-owned pair records drive checks, and reciprocal timestamps suppress duplicate impulses. The current single-rider advance must be split before using it as the full shared-world scheduler.

NPC fixture facts: provider10A768 has its own behavior/state layout and129 authored AI paths, separate from8course-progress paths. The five visual opponents are Psymon, Allegra (arielle), Moby, Griff (grommet) and Luther, established from actual model headers. Gameplay stat-character metadata is not sufficient to identify their visual resources. New NPC packages are being validated independently; they are not yet moving opponents in the app.
