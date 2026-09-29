# Native hard-crash motion and recovery

Hard impacts call `10EB30`, enter control 8, and then enter motion 2. Motion 2 has sliding and airborne submodes; control 8 has five animation/recovery phases. The native helpers preserve these separate state machines. They remain standalone until the game binds their real animation, scoring, boost, query, and control-entry callbacks.

## Entry and retained state

`crash_entry.hpp` preserves the full `10EB30` sequence: peak impact, crash statistics/boost penalty, observers, temporary control 13, optional prewind compensation, presented-root bake, input resets, selected-clip preview/root compensation, animation play, control 8, and finally motion 2. Control 5 interruption can use the already verified `originalAirControlExit` air-spin bake. `116930` is a no-op. Prewind compensation includes `11DFE0`'s orientation normalization before the subsequent presented-root bake.

`crash_bake.hpp` implements the entry and `12E010` reset-clip transform. It composes the current scaled local root with the inverse of the selected preview root, applies that delta to the physical transform, and supplies inverse(delta) to `311BF0`. Current local data comes from geometry `+24/+28`, before world FK/IK; the preview is a separate `30ECD8` sample at time zero with the original sequence/root/mirror settings. Existing animation roots must be compensated before playing the new semantic.

Control 8 enters before motion 2. Its class-22 board detachment therefore sees the previously retained crash submode. Do not reconstruct the entire crash state on every entry. Angular vectors and the previous progress sample survive until their specific update stages. Control field `+54` is an integer impact flag, `+60` is impact velocity, and `+70` is the recovery meter.

## Motion and geometry

`crash_motion.hpp` implements motion entry `136C40`, detachment `136D40`, detached-root integration `136F30`, and airborne predictor/alignment `137750`. Detached position `130` and quaternion `140` evolve independently when flag `150` is set, with gravity −1800 cm/s², XY drag −0.2, the original speed cap, angular damping, and quaternion integration order.

`crash_ground.hpp` implements sliding forces and terrain response in `137D18`. It uses the existing surface `+90` slip curve, crash-specific damping and gravity, angular speed driven toward linear speed × 0.0125, and the original speed cap. Position integrates before acceleration. Terrain correction retains the old material's `+18` depth (`depthTarget3`) even when the hit selects a different surface. Shallow/deep penetration, moving-entity force, hazardous-surface reset, and delayed airborne flag are explicit outputs.

`crash_contact.hpp` implements `138960`: actor position ±200 cm along the supplied normal, refined terrain, preferred fraction 0.575, and rider cache `864`. This differs from ordinary landing's animated board-root probe.

`crash_collision.hpp` implements the main sliding and airborne body responses. Sliding uses coarse 3×3 terrain even for humans, the ground-normal filter, and half-speed restitution bounded to 27.777779–555.555542 cm/s. After scenery/pair callbacks it commits the pending airborne flag and zeros speeds below 50 cm/s. Airborne contact requires a body hit before probing terrain; valid landing updates impact flags, surface state, and sliding submode. Other collisions push and restart prediction.

`crash_detached.hpp` reconstructs the detached board as four unscaled 20 cm spheres at +70, +30, −30, and −70 scaled centimeters along posed bone `8A4`'s X axis (Zoe bone 23), with broad radius 90 × scale. Its contact response preserves bounce/stop thresholds, entity forces, and all shared RNG draws for angular jitter.

`crash_world.hpp` enforces the different query policies: sliding is coarse with cache `868` and ground-normal filtering; air uses human/NPC detail with cache `868` and no normal filter; detached board uses coarse generic world queries without a rider cache. Incomplete geometry/entity callback coverage raises an explicit error.

Body pushes correspond to `106538`: physical position, `9D0`, cached AA0 centers, and AABBs `400/410` move. Posed geometry and cached presentation metadata remain unchanged. The current `PrototypeRider` world adapter does not yet model `9D0` or those AABB fields; this remains an integration gap. Do not apply physical displacement twice when also updating companion body geometry.

## Control and animation

`crash_control.hpp` implements control entry, initial clip release, recovery meter, playback-rate selection, get-up phase 3, and reset-clip phase 4. Initial clip completion derives angular velocity from consecutive posed quaternions; class 23 derives detached board velocity from its own consecutive poses.

`crash_recovery.hpp` implements complete phases 1 and 2, retaining post-callback reads, progress preservation across clip changes, impact handling, and ordered reset/get-up requests. `crash_animation.hpp` supplies the concrete air/ground continuation, get-up, special-landing, and reset-clip selections. Play requests use group 0 and default blend argument −1. These callbacks must invoke the actual native player and motion/control entry routines.

## Conformance

All comparisons below use exact float equality against original instructions, with the described external boundaries controlled.

| Test tool | Cases | Coverage |
| --- | ---: | --- |
| `test_crash_motion_reference.py` | 120,000 | Entry, detachment, detached integration, air predictor/alignment caller, sliding forces/contact |
| `test_crash_collision_reference.py` | 40,000 | Complete sliding/air body responses, interaction ordering, landing flags and entity force |
| `test_crash_detached_reference.py` | 40,000 | Board spheres, complete contact response, 23,664 shared RNG draws |
| `test_crash_control_reference.py` | 120,200 | Entry, initial clip, meter, playback, phases 3/4, initial continuation table |
| `test_crash_recovery_reference.py` | 40,000 | Full phases 1/2, 8,003 and 4,769 phase transitions |
| `test_crash_animation_reference.py` | 1,300 | All continuation/get-up/landing selections, including no-op/detached cases |
| `test_crash_bake_reference.py` | 40,000 | Hard-entry and reset-clip physical/root compensation |
| `test_crash_entry_reference.py` | 20,000 | Complete hard-entry sequencing, including 6,666 prewind compensations |
| `test_crash_contact_reference.py` | 49 | Full original terrain probes and cache, 38 hits |
| `test_crash_world_query_reference.py` | 49 | Full detached-board world queries, 11 hits |

The generic world-query oracle repairs eight self-recursive JALs that the AOT generator incorrectly emitted as `goto`, in ignored test copies only. No original ELF/ISO data or runtime source is patched.

These tests validate the recovered physics and callback contracts. They do not constitute an end-to-end native crash animation/recovery replay. Remaining work is binding the real player, score/boost/audio observers, complete world state, and native mode transitions, then testing original crash checkpoints. `reference_crash.py` exports typed initial states, including the crashing NPC in the long-charge tick-578 checkpoint; it does not supply captured per-frame poses to the runtime. Forced reset `116120`/control 9/motion 3 remains a separate required lifecycle.


## Gameplay integration (2026-09-09)

The standalone helpers above are now bound into `engine/riding.hpp` and run in ordinary play:

- **Entry.** `originalLandingClassify` results other than `0x1b6` no longer throw; the touchdown state is committed and `enterHardCrash` runs the complete `10EB30` sequence (peak impact, `119B08` crash count/penalty, observers, transient control 13, prewind compensation, `11FA10` presented root, `30ECD8` preview root bake, `311BF0` sequence-root offset, clip play, control 8 then motion 2). Cruise and airborne body bounces now dispatch `105D98` through `originalCollisionReaction`: soft reactions enter control 3, crash reactions enter control 8, hazard surfaces and the direction-change accumulator raise an explicit reset request.
- **Control 8.** `crashControlFrame` runs the `12CB68` recovery meter (held jump = command bit `0x2000`, human category 1) and phases 0..4 through `originalCrashInitialControlStep`, `originalCrashAirRecoveryStep`, `originalCrashGroundRecoveryStep`, `originalCrashGetUpStep` and `originalCrashResetClipStep`. Continuation/get-up/special/reset clips come from `originalCrashSelectAnimation`; playback rate from `originalCrashPlaybackRate`; reset clips rebake the root through `originalCrashRootBake`. Get-up enters control 0/motion 0 (`13C7A8` ground entry) or control 5/motion 1.
- **Motion 2.** `crashMotionFrame` integrates the detached board, sliding forces/alignment (`137D18`) with the `138960` probe and `originalCrashSlidingContact`, or the airborne first phase (`137750`) on the shared predictor. `crashContactFrame` applies the coarse sliding body response, the airborne body/terrain landing response, detached-board contacts with the shared RNG, and `1388D4` finish.
- **Animation.** `OriginalRiderAnimation` gained `presentRoot`, `previewRoot`, `scaledLocalRoot`, `offsetSequenceRoots`, `seekChannel`, channel progress/rate/duration reads and a detached-board bone override. Crash semantics 328..410 resolve through the rig's variant tables; their completion pulses (`+C0`) drive the phase machine.
- **Application.** `PrototypeRider::resetRequested` is honoured by the preview as a respawn (`116120`/control 9/motion 3 remain unrecovered). The HUD shows the recovery meter while crashing. Effects receive motion mode 2.

`tools/test_crash_gameplay.py` (`native_crash_recovery`) lands a held forward flip upside down: control 8 enters airborne (frame 123), slides, gets up and returns to control 0 at frame 239 with every collision pose present; holding jump fills the meter.

Documented approximations (not source-verified): the `117948` boost-loss test is represented as "boost meter above zero" (penalty −0.25), the `+1C` clip scale is 1, the sliding contact's surface `+18` depth uses the landing material `depth3`, control-13 entry does not alter the physical transform, the `30ECD8` preview does not draw the variant RNG (all crash semantics have a single leaf), and rumble/scoring observers are recorded only.

## Surface 18 and the detached-board frame (2026-09-27, PS2 peak2/dbc2-race-tuck)

- **Entry from the ground (ported).** 0x13F178, the ground-motion post stage, runs when the rider is not departing. On surface 18
  (+0x438) it enters the hard crash at once, 10EB30(rider, 360, 0, 18, {+0x110, unit +0x1E0, +0x370, closing 0}). Its
  13F488 / 105398 / 107888 then see the ragdoll, and the 13F358 clamp applies to the crash actor's velocity. Other surfaces
  take the 116120 reset branch instead (a table+0x44 surface or patch flag 2). The port: web/core.cpp sets
  `browserGroundCrashPending` from the ground contact, and web/animation_bridge.cpp enters `enter_crash(360, ...)` in the
  post stage. This made Ruthless exact from 3911 through 4435 (docs/career-events.md "Round 3").
- **Fixed 2026-09-28: the rider frame after the board detaches.** Before the fix:
  - At crash tick 22 the class-23 clip ends (`originalCrashInitialControlStep` -> `originalCrashDetach`, 136D40).
  - The browser then builds +0x160 / +0x170 / +0x180 from `boardRoot = {detachedPosition, detachedQuaternion}` (11EB98 with
    +0x150), so the frame jumps to the board's world rotation (dbc2 3933: up (0.98, -0.15, 0.11)).
  - The PS2 keeps the frame of the crash entry on that tick (up (-0.78, 0.23, 0.58)) and turns it smoothly from the next one.
  - That `up` is 1057B8's `presentationUp`, whose gain feeds the air-bounce counter +0x3F4 (1210B0: reset above 5).
  - So the reset out of the second crash (dbc2 4341..4436) comes one tick late: counter 4.94 against the PS2's 5.17 at 4435.
  - Fix (web/animation_bridge.cpp `frameBoardRoot`): once 136D40 has detached the board, 11FA10 builds the frame from the
    identity local board, i.e. the physical transform (+0x120 / +0x110), not the free board. Ruthless (dbc2-race-tuck) human, Nate,
    RNG, ranks and pair records are exact to 7000 (were 4435 / 5141 / 4581).

## Sliding crash bounces are not collision events (2026-09-28)

The sliding crash post 138640 queries the body (3342D0), pushes it (106538), and adds n × (closing + clamp(0.5 × closing)) to +0x1E0.
For an entity hit it calls only that entity's contact-velocity callback (instance+0xC vt+0x154). It never calls 105D98, so the
collision history (+0x3E0 normal, +0x3F0 direction changes), the peak impact and the crash impact flag (+0x54 / +0x60) are left
alone. `BrowserCrashRuntime::contacts` (web/crash_runtime.hpp) used to dispatch an impact on every sliding bounce. PS2 Gravitude
Mac 977: the velocity bounces the same way, but +0x3E0 only decays. The airborne crash contact (137860) keeps its landing and
impact handling.
