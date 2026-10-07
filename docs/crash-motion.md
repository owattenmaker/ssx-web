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

## The board detach at control 8's enter reads rider+0x9D0 (2026-09-30, c0a-ws13)

- **PS2 rule.** 12CA30 (control 8's enter) copies the cached world bones [+0x89C] (primary) and [+0x8A4] (board) and adds rider+0x9D0 to
  both (0x12CAC4..0x12CAE0); if channel 2's class is 22 (311AE8 at 0x12CAEC) it detaches the board there (jal 136D40 at 0x12CB18).
  +0x9D0 sums every 106538 translation (0x106560..0x10656C: +0x110 and +0x9D0 += d) since the rider's 120F20 cleared it (0x121020);
  121750 commits it to the cached bones (310530) and does not clear it.
- **Evidence.** local/ctm-events/probe_9d0.py on the jal at 0x12CB18 (an open-loop replay of c0a-ws13): Allegra's +0x9D0 at race tick
  5939 = (8.2325, 7.7229, 30.7344), an instance push from 105398 before 105D98 dispatched the crash. The port's board was off by exactly
  that from 5941 on, so it bounced a tick early (137138's three draws in 6080 instead of 6081).
- **Port.** web/animation_bridge.cpp `pending_pose_translation()` = the tick's contacts translation (`tickBodyTranslation`, the post's
  former local), the in-flight instance pushes (`instancePendingTranslation`, web/instance_contact_gameplay.inc) + `pairCompanion` +
  `browserLandingTranslation`, passed to `beginControl` whether or not the post is running.

## Attacked bails: who counts the crash (2026-10-01)

- **The flag.** 10EB30(rider, a1 semantic, a2 attacked, a3 impact type, t0 event) uses a2 only for 119B08(score object, a2) at
  0x10EB94. 119B08 (0x119B18): attacked -> score +0x12C += 1 and HUD popup 117B88(score, 0x2D, 0, 0, 1.5); else +0x124 += 1. Both
  then add the lost points to +0x1A0 (11A7A8), return the -0.25 bail penalty when points are pending (117948), and reset the combo.
- **Who passes it.** Of the six 10EB30 calls, only 107E70's (0x1082FC) passes a nonzero a2: its own a3 (0x1082F4). 107E70(victim,
  other, direction, a3 attack, f12 impulse) gets a3 = 1 only from 107888's attack branch (0x107E08..0x107E0C: victim = the opponent,
  other = the attacking owner); the ordinary pair impulses (0x107BA0 / 0x107BBC) pass 0. 105D98 (0x1064E4), 1311B8 / 1311D0, 13A530
  and 13F22C (surface 18) pass a2 = 0.
- **No window.** Nothing stores the attack: only the crash that the same 107E70 call enters is attacked. A later crash (a landing, an
  obstacle, an ordinary bump) counts +0x124 again. Re-hits are gated by 107888's own reciprocal +0x18 attack cooldown
  (docs/rider-pair-collision.md).
- **Credit.** After 10EB30 the crash path of 107E70 calls 10E468(victim, other) when attacked (0x10830C), else 10E2E8. 10E468 makes the
  audio calls (298138, 2A0A30, 298D00 on 28B180), the relationship event 155BF0(.., 3), then 119400 on the ATTACKER's score object: +0x128 += 1 (KO),
  popup 0x2C for 1.5 s, and returns 1.0, which 10E098(attacker, 1.0, 2) awards. The soft paths (10E3A8 attack, 10E228 not) change no
  score. So the victim counts +0x12C and the attacker +0x128, whether the human or a computer rider is either one.
- **Port.** web/npc_gameplay.inc pair_react passes the reaction request's attack flag to browserHardCrash (web/core.cpp, now
  (semantic, event, attacked)); enter_crash (web/animation_bridge.cpp) hands it to originalHardCrashEnter -> score_bail ->
  originalScoreBail (engine/score_object.cpp, 119B08). The attacker's KO stays web/ai-racers.js -> pair_knockout. Marker export
  `_pair_attacked_bail` (the capture gates key on it).
- **Gates.** careerrival/dra4-final: the human's crash at 229 is attacked; the human score object is exact to the end (was 228).
  attackbail/ko-attack-moby: ko-attack re-captured with Moby's score object 0x5D6600 watched (--watch 0x5d6600:0x1d0; records
  byte-equal to ko-attack before the watch window); web/ai-score-compare.mjs (a compare-ai-capture TICK_HOOK) compares it: Moby's
  +0x12C at 265 (+0x124 on the old core) and the whole object exact for all 368 ticks; the human's KO (+0x128) as in ko-attack.

## High-level play (2026-10-04, physics-jank agent)

Playtest report (an SSX speedrunner on Windows Firefox 157, xinput pad): "bailing and getting flung in weird directions
whenever I attempt high-level play".

- **Field data** (host diag.log, sessions 71f4ne0n and lgotfd1d, read on the host only): no bail events existed in the
  diagnostics. Every event race ran at about 40 fps (2,390-2,460 frames a minute, frames alternating 1 and 2 ticks). The free
  ride before the first event ran at 60. The drop is the event phase with the five computer riders, handed to performance.
- **Seeded aggressive pads** (local/ps2-capture/hl-gen.py; hl-capture.sh builds and runs one, hl-compare.sh compares one).
  The moves: tucked boosting, diagonal (0.7071, an XInput stick's circular gate) and hard carves, charged jumps into long
  spins / flips / grabs held into the landing, re-jumps, presses, plants and punches. `--rail` adds rail moves after a
  `--prefix` that reaches the Snow Jam glide rail; `--uber` adds Uber chords (run with the full meter and Tricky poked).
  The captures are in local/ps2-capture/runs/hl. The port was compared on every one, isolated and with the computer riders.
- **Found and fixed (exact, by address):**
  - **A soft collision during a board press** (hl-glide-3 682). 108388 in control 1 (0x1083D8..0x1083EC) first runs 131348
    on the control-1 object: the pivot spring 1313A8, the finalise 131428 (which can flip +0x320 back) and the end score
    119A38 -> 10E098(rider, f, 1). Then it picks the clip, reading +0x320 after the cancel, and plays it. 11FEC8(3) then
    runs the exit 12FE98.
    - The port judged the hit with control 0 and dropped the soft reaction. The rider stayed in the press, kept hitting the
      object (4 contact responses against the PS2's 1) and was pushed upward: vy +165 against +60, 29 m off by the end.
    - Now: engine/collision_event.hpp `OriginalCollisionContext::cancelControlOne` (131348; returns +0x320). It is set by
      core.cpp dispatch_body_event for control 1, by the standalone 108388 instance path and by the pair path.
    - web/boardpress_gameplay.inc `board_press_soft_cancel` / `board_press_soft_enter` run the cancel, then the soft clip,
      then 12FE98. The event control is 1 while the press owns the controller, and begin_soft_control takes control 1 from
      that path only.
  - **The boost through a crouched departure** (hl-glide-3 1430). 12E9B8 with JumpHeld (word0 0x2000) calls 114130(BoostHeld,
    0) at 0x12EB58 whatever the motion, so Cross + Square held through a passive departure keeps boosting and draining in the
    air. The port stopped it.
    - Also: the first air tick after a charged release is control 5 (133308, which stops the boost), not control 0's ride-off
      (hl-sj-1 2596, hl-sj-2 913). Fix in core.cpp's controller boost dispatch (`airCrouch`, `releasedLastTick`).
  - **One meter decay a tick after a rail loss** (hl-rail-10 702 / 703). Control 7 in the air ran 114130 / 1200D0 in the rail
    step and step_rider ran them again. `browserRailBoostTicked` skips the second.
  - **12F730 after a rail release with an attack held** (hl-rail-15 711..721). The rail step's Stop::Airborne flag
    (railReleaseFrame) was only cleared inside the rail step. An attack held in passive air (12F7AC) skips the rail step, so
    the flag stayed set, and animation_tick never ran 12F730 again for that flight. The PS2 enters control 5 at 721; the port
    kept passive air, so no tricks, and it landed 36 cm off. Fix: core.cpp clears it every tick (`browserRailTickBegin`).
  - **115B58 in a board press** (hl-ai-9 1265). 12FC80 calls 115B58 at 0x12FDF4 before 115D48, so a crash get-up's pending
    314 (+0x358 = 4) plays in a press, with its 311710 variant draw. The port's control 1 skipped it (`upper_request_play`).
  - **The crash queries use the rider's scope list.** 137860 / 138640 / 138960 pass rider+0x860 to 3342D0 (0x1378D8, 0x13869C,
    0x138A20). OriginalCrashWorldQueries now takes the scope (web/crash_runtime.hpp, `browser_rider_scope`). No capture has
    shown a difference from this yet.
- **Gates** (`ps2-captures hl/*`): hl-glide-3, -5 (Uber), -6, -7, hl-sj-1, -2, hl-metro-4, hl-metroglide-8, hl-rail-10, -13,
  -14 (Uber), -15 exact to the end in physics, score and boost; hl-ai-9 and hl-ai-16 (not isolated) exact for the human, the
  five computer riders, the RNG, ranks and pair records.
- **Open:**
  - hl-aimetro-17 2495: the human's second airborne crash body contact in a row (instance 64528) pushes 0.88 cm less upward on
    the PS2, with equal velocity; the human-only replay shows the same. The gate holds the human through 2494.
  - Posed root ~1 cm off (physics exact) on the departure tick out of a rail loss into control 4 (hl-glide-3 2426) and out of a
    nose press (hl-rail-15 1932); bones are gated through the tick before.

### Field bail reports (web/diag-bail.js)

game-tick.js calls host.bailTick after every live tick (not in replays). The watch only reads crash_info, reset_info, the
score object's +0x12C and rider_state, and sends a small diag event:

- `bail` on each crash serial increase: tick, sem, impact, sub, attacked, vIn / speedIn (m/s over the tick before), vOut (mean
  over the 15 ticks after), turnDeg / turnHDeg, placed (a reset or rescue inside the window), the last 8 pads ([buttons mask,
  lx, ly, rx, ry as -100..100, ticks left in that drawn frame]) and multi (catch-up ticks among them);
- `reset` {tick, reason} on each forced placement.

60 a session at most, inside the diagnostics budget. test-diag-bail.mjs.

### pv padRing (removed 2026-10-05) and the pad report rate

The PS2's catch-up updates each take their own vblank's pad sample (0x326B88 ring, 0x326B48); the page reads the gamepad once a drawn
frame, so a catch-up tick repeats the tick before's pad. A between-frame poller (pv padRing, 4 ms, a ring consumed per tick) was tried
and removed. The pad-rate probe (local/physics-jank/pad-rate-probe.html: rAF against the poller, distinct timestamps and stick states)
on the user's Xbox Wireless (045e:0b22, Bluetooth, macOS):
- Chrome 149: mapping standard, ~50 Hz from the pad; the poller saw ~8% more samples at load 0 and fewer under a 20 ms frame
  load (it starves when the main thread is busy). Not worth its code.
- Firefox 157: mapping '' with 7 axes, and only ~9.4 Hz reaches the page (16% of off-centre frames change, runs of 9 frames)
  from the same pad, poller or not. Gecko's macOS backend (dom/gamepad/cocoa/CocoaGamepad.cpp) is event-driven
  (IOHIDManagerRegisterInputValueCallback, no throttle), and Gamepad::SetAxis bumps the timestamp on every value, so the rate
  is what Firefox's IOHID path receives; no page-side workaround found.
- The mapping: Firefox has no remap for 0b20 / 0b21 / 0b22 (GamepadRemapping.cpp lists 0b13 and 02e0 / 02fd), and its default
  remapper numbers axes in descriptor order (X Y Z Rz Brake Accelerator hat), not by usage. The port applied Chromium's
  usage-indexed RawInput table (the right stick's Y from the right trigger, the D-pad from a missing axis 9). web/gamepad-map.js
  FIREFOX_LAYOUTS / firefoxGenericLayout now map Firefox's descriptor order (test-gamepad.mjs).

## Move-family coverage (2026-10-05, physics-jank agent)

- **Captures branch off gated runs at a feature** (local/ps2-capture/hl2.py; runs/hl2). Each one replays its source's pad to a
  rail entry, a plant, a press, a long air or a surface run, then plays one move family with skilled-player timing.
  - The families: air, uber, press, rail, carve, crash, attack, plant.
  - Timing mixes frame-perfect taps and holds with sloppy chords: onsets and releases 1..4 frames apart.
  - The PS2 is deterministic for the same savestate and pad, so a branch reaches the feature as its source did.
  - Ubers use the full meter poked (rider+0x2F8 / 0x2F0 / 0x2F4) and Zoe's lodge rows set to entry K for every slot
    (0x530EC0 + 4 x 0x1FE + slot x 6, both bands). compare-ps2-capture.mjs applies the same rows.
- **Rules found and ported** (each with its first divergent capture; ps2-captures `hl2/*` gates them):
  - 12E9B8's jump release on a rail returns before 114130: +0x2FC keeps its value for the release tick (rail-slide 737).
  - 12E9B8 in the air with Cross held and +0x328 still set steers +0x22C with RailBalance (113F38), not the turn (113E80)
    (rail-fence 707).
  - 10EB30's 11FEC8(13) runs the old controller's exit first; for control 1 that is 12FE98 (crash-eba3 1409, rail-era5 2932).
    10EB30 also clears +0x330 (rail-fence-b 811).
  - 1057B8's surface landing passes 119E38 the stance argument +0x320 != +0x324 (crash-eba3 1664, press-bra2 1221).
  - 12EE30 plays the air release clip with 3128E8 a2 = 0 (inheriting a fading copy) and not at all when channel 2 requests it
    already (rail-bra2-a 1305).
  - A control-1 departure tick does not approach the turn triplets a second time in the air pass (press-rail 1016).
  - 334680 searches the rider's scope list (rider+0x860). Its splines are the segments whose loose octree cell (328F28, padded
    0.2 cells) overlaps the scope box at the 3-tick refresh. The scope box is not the segment's own bounds: that test drops
    rails the PS2 grinds. Measured from three PS2 savestates of plant-pipe-a: 4 then 8 segments, 0 missing, 0 extra
    (plant-pipe-a 900).
  - Motion 1's landing 13A7B0 also runs in control 7 after the rail motion lost the rail; the landing's control request runs
    132048 first (uber-rail-10 2339).
  - The tick the rail motion loses the rail runs no air post: no 13A7B0 landing and no contacts (rail-bra2-a 1760,
    carve-powder-cba2).
  - A boost is not stopped a second time in a tick where 12E9B8's rail controller already ran 114130 (Stop::Airborne) or where
    12F730 returned early for an attack hold in passive air (rail-rnb-s1b, uber-row8).
  - After a landing crash in 139C88, its 105398 (106F78 first) runs with the rider as the ragdoll: 10EB30 switched the motion
    to 2, so the push and impulse land on the crash actor and 105D98 dispatches a ragdoll impact that restarts the crash
    predictor from the pushed state (uber-rail-6 1027; the 13AA48 body response already did this, docs/ai-racers.md).
  - 1211F8 approaches the rail triplets +0x22C/+0x238/+0x25C every tick, also when 12F730 returns early for an attack hold; a
    fading rail cycle 18..20 reads +0x238 (rail-rnb-s1 739).
  - A board press requesting control 2 (Cross held) runs control 2's entry 12E980: +0x200 = 1/30, +0x204 = 0 (rail-dss2-s1 1422).
  - 1211F8 approaches +0x28C/+0x298 once per tick: on the first 133308 tick the selector's approach after its targets is that one
    (uber-row6 854).
  - 120378 reads the post-controller +0x320 and owner+0xDE4: a pivot's 12FEC8 stance flip (rail-cra3-s2b 1320) and the handplant
    entry's control 11 (plant-pipe-a 900) count.
  - 12E9B8 returns after its 106848 attaches: a Cross released on the attach tick is released on the rail the next tick
    (riders/griff-uber-c 1730).
  - Every crash play (10EB30's entry; 12DCB0 / 12DD98 / 12DE80 / 12DF48 continuations and get-ups; 12E468; 12E010) calls
    3128E8 with a2 = 0: a re-request of a fading clip continues that copy when 311F00's inherit test passes (same semantic,
    clip kind, mask +0x88, mirror +0x80, root +0x60 / +0x70) (riders/fareastmyth-uber-c 2149).
  - 13AF28 on an entity-owned rail (query out+0x50 instance with an entity) takes +0x3D0 from the entity's vtable+0x154
    contact velocity at the rail point: a Snow Jam log teeter swinging under the rider (riders/fareastmyth-uber-b 2478).
  - A computer rider's provider reads an earlier rider's velocity live through its +0x6C0 getter (100F88 at 0x10105C): it sees
    that rider's takeoff from this tick's controller (core rider_world_state [16..18], marker [19]; web/ai-racers.js stage 1 uses
    it for earlier slots). hl2/attack-bra2 756: Griff boosts after Moby's ollie.
  - A ground get-up (12D848: control 0 / motion 0) whose 13F178 leaves the ground in the same tick is a passive departure: the
    next tick's control 0 requests control 4 (hl2/attack-bra2 1374).
  - 13F410 stamps owner+0x14 with 1298C8, the tick the step began with, also for a 121818 stage-trigger reset ("Wrong Way!",
    reason 4) that runs after the port's motion tick advanced (hl2/attack-bra2 1531: the next landing's 13F0F0 speed factor).
- **Batch 2** (46 captures, runs/hl2/batch2.log): the 11 lodge Uber rows with the full meter, Ubers off rails, more rails
  (fences, slides, the ice and event peaks, hard balance and side switches), presses and carves on ice and powder, crashes and
  resets, plants on the pipe and Snow Jam, attacks among the computer riders.
  - A branch that reaches a freestyle finish needs `--finish-place` in its job's extra_args (0x239230 sets the boost meter by
    place): carve-powder-cba2 finishes 5th.
- **Closed since:** air-eba3 (core24), fareastmyth-uber-b (rider-parity's posed pickup collision), attack-bra2-b (the same; core28 on),
  hl/hl-aimetro-17 2495 (the scope-list order below, core28 on).
- **The octree's child order** (332DB8 / 33B748, PS2 hl/hl-aimetro-17 2493 scope list rider+0x860 -> +0x10C / +0x110):
  - 332DB8 visits roots 0..7. A root the query box contains goes to 340DC0, an overlapped one to 33B748.
  - 33B748 collects a node's lists (3309D8 instances +0x20, terrain patches +0x24 with the box test, +0x28) before its children.
  - Its eight unrolled child blocks load +0x0, +0x4, +0xC, +0x8, +0x18, +0x1C, +0x14, +0x10: children 0 1 3 2 6 7 5 4, the reflected Gray order
    (child index x << 2 | y << 1 | z). A child the query box contains goes to 340DC0, which visits children 0..7.
  - The rider's scope box (rider+0x400 / +0x410, about 3.6 m) never contains a loose cell with children (1.4 x 4096 cm on every axis), so the
    rider's scope lists are in Gray order at every level: patches 171792, 245776, 64528, 293648 at 2493; the ascending order visited 64528 first.
  - That changed which patch 333EF8 last hit, so the 3342D0 cache (the cell the next tick tries first) was wrong.
  - engine/original_spatial.hpp originalSpatialBefore: used by the terrain body query, the ground probe's patch order, the collidable-instance
    order and the rail walk.
- **Matcher batch 3** (the arithmetic agent's exact-mode zoe-race 186):
  - Every hard-coded 0x3D4CCCCE rate reset is a .sdata constant in the ELF, so the value is right:
    13F410 gp-0x6FA8, 12F730 gp-0x76E4, 120D90 gp-0x7934 / -0x7928, 1328B0 gp-0x756C, 13AD20 gp-0x7184, 132F98 gp-0x754C.
  - Only 13D818's 13EFF4..13F05C compute dt x 3 (+0x20C, +0x2C0): ground_pose_motion.cpp, on the exact dt.
  - rider+0x244 .. +0x24C start at 0 (every slot at zoe-race 186): hpLean244 starts {}, not with the exit rate.
  - The attribute getters divide (div.s) at every use: seeded stats (n / 11) are recomputed in the current arithmetic (browserStatFromSeed,
    web/rider_attributes.hpp), and ps2_arith_exact recomputes them in its context. A no-op in mode 1.
- **Open:** none from the mode-1 batches. New batches are exact-mode captures (PS2_CAPTURE_FPU=exact, one ARMSX2 slot).

## Fuzz repros on mode-1 captures (2026-10-05, physics-jank agent)

The fuzzing agent's minimised pads (local/ps2-capture/runs/fuzz-mode1, docs/fuzzing.md), traced with oracle snapshots:
- **A reset in a handplant** (r5-0165-min 544): 116120 runs 11FEC8(9) (control 11's exit 132F98, phase 0), then 11FE78(3), which runs
  motion 5's exit 139178: 139548 launches from phase 0 (+0x10 stays 0) and +0x1E0 = +0x10, so the reset starts from rest.
  web/handplant_gameplay.inc handplant_reset_exit, called by begin_reset.
- **A landing with an air press pending** (r4-0005-m8 1217): 139C88's nose / tail angle test (0x15C / 0x15D) only runs with +0x330 = 0;
  an air BoardPress style lands into control 1. landing_crash now passes board_press_style() as manualState330.
- **Event delta rounding** (engine/animation_events.cpp): the event window's delta was a native float product, which rounds to
  nearest in wasm (no rounding modes there); the PS2's mul.s chops (r2-0204-m0 2585: 3CD9ED93, the port 3CD9ED94).
- **A computer rider fires a stage trigger in a human-only capture** (r1-0066): Rival Time's Psymon (slot 1) fires the EBC3 rock
  trigger 15146 at 2397 (121818 0x12186C -> 30A060, RestoreNode 0x350F60, flags 0x200022 -> 0x200304); compare-ps2-capture runs only the
  human, so the port fires it at the human's 3475. A harness gap: event seeds need --ai-state and compare-ai-capture.
- **r2-0204-m0** (six riders), fixed:
  - Mac's landing pose at 2585 (core38): the doubled animationTurn approach on a passive departure tick (web/animation_bridge.cpp).
  - The human's crash on a rail at 2663 (core39). 11FEC8(13) -> 111538 -> 111578 runs the old control's exit from table 0x456B90 by
    +0xDE4:
    - 0 -> 131C30, 1 -> 12FE98, 2 -> 12E9B0 (empty), 4 -> 12FB68, 5 -> 134CB0, 7 -> 132048, 8 -> 12E690, 11 -> 132F98.
    - Control 7's 132048 sets the +0x238 target to 0 (+0x240) and its rate to gp-0x7594 = 1/15 (+0x23C).
    - From 2664 the fading rail cycle 18 (kind 5, 0x104238) blends its slots from the decaying +0x238 (1 -> 0.9333 -> 0.8667).
    - The port's crash entry ran only control 1's exit, so +0x238 stayed at 1, the slot blend stayed 0 / 1, and the crash pose was
      1.4 cm off: a 1.1 cm translate at 105398, then the velocity.
    - Fix: enter_crash's enterControl(13) runs rail_control_exit() when the old control is 7.
    - Verified: r2-0204-m0 human, fields and bones exact to the end (2704). Full ps2-captures on core39: 531 clean.
    - With the rider pairs hosted (six riders, core41): rail_exit_contacts' pair phase rail_apply()s its rider view. The 105398 crash
      (phase 1, 1057B8 -> 10EB30) left that view holding the old triplet, which put +0x240 back to 1. Both crash routes (105398 and
      13C140's reaction) now re-read the view, as the soft route already did. r2-0204-m0 six riders: everything exact to the end.
  - QA: ground_state_dump now lists the rail triplets +0x22C / +0x238 / +0x25C (compare-ps2-capture --fields).
- **A section-built entity steps once before the next record** (r2-0249, and the baseline hl2/carve-s1-era5; core40):
  - A PS2 record is taken after the human's 11B3F8. One record interval therefore holds the computer riders, 0x101B60's scan, the
    next race_begin's entity pass (0x354F98), then the human. The port's tick runs its entity pass first.
  - So an entity a slot-1 program builds in the scan steps in that same interval on the PS2. In the port it waited for the next
    tick, and lagged one step for life.
  - Snapshots (tools/ps2-float/ee_oracle/snap_at.py, mode 1, carve-s1-era5) of the Gravitude crash billboard 332333 (LiveComp entity,
    words +0x1C time, +0x20 unclamped, +0x24 previous):
    - 545: time 1/30 + 5/60; the port had 4 steps.
    - 1150 / 1193: the port's value one record later (0x3FB332E8, 0x4007774F).
  - Effect: program 85's builtin-55 crossing at 64/30 s, its two builtin-77 draws and the ice-piece breaks came one tick late. These
    are the gravitude gate's RNG "blips" at 1195 / 1209 / 1239. In r2-0249 a rider draw interleaved, so the shared RNG diverged.
  - Fix: section_pass() updates the entities built since the scan began (stage_world_tick_newer, after the visual pass).
  - Verified: carve-s1-era5 and r2-0249 have no blips; the RNG and all six riders are exact to the end (3930).
  - Not changed, likely the same offset, unverified: the section collision players (stage_section_collision_update: now - start
    steps) and the JS section players.
- **A reset from a reset surface on the ground** (fuzz r10-0153 1210; core42, live in core-batch10). 13F178 runs 13F23C ->
  116120(rider, 0, 1) for a surface with table+0x44 or patch flag 2, then 11E150 / 13F488 / 105398 / 107888. Three port gaps:
  - 13F488 and 105398 begin with 11FEE8 (+0x77C -> +0xDE4, the control) == 9 -> return (0x13F4B4, 0x1053C8). After the reset they
    do nothing. The port ran its ground query anyway and pushed the rider 15.5 cm (animation_bridge.cpp resetInPost). 107888 has no
    such check.
  - 11FEC8(9) -> 111578 runs the old control's exit (table 0x456B90). Only control 5's 134CB0 zeroes the prewind triplets
    +0x2A4..+0x2B8, so from control 0 they keep their values (+0x2A8 / +0x2B4 stay 1/30). begin_reset now clears them only when
    leaving control 5.
  - 11D660 (the placement) zeroes every ground-control triplet +0x1F0..+0x2DC, including board press +0x268 / +0x274
    (0x11DB70..0x11DB84). The port kept the press rates; board_press_placement_clear now clears them.
  - Verified: r10-0153 fields and bones exact to the end (899 records).
- **Handplant to passive air to a landing** (r13-0191, r5-0194, r2-0299; core45): the handplant's per-tick flags (hpTick / hpAirTick,
  read by the animation tick as "handplant-owned") are cleared at the top of step_rider. A tick that never reaches the step (12F7AC: an
  attack held in passive air) used to keep the exit tick's flags and skip 139A20's orientation tail 0x139A64.
- **Upper reactions on a rail** (r7-0036, r13-0175; core45):
  - 106848's air-to-rail attach calls 10E910 (0x106CB8), whose human tail 10EA28..10EAA4 posts the 10E028 reaction (score_upper_reaction).
  - 131D30 calls 115B58 at 0x131E18 (before 115D48 and its rotation 132060), in the controller phase.
- **Soft re-entry on a departure tick** (r1-0204; core46): 13F178 runs 13F488 / 105D98 after the 13F194 departure, while the motion
  is still 0 (11FE78(1) comes at 0x13F2CC). A soft reaction there enters control 3, also right after 12E778 ended the previous soft clip.
- **The rail motion pushes its entity every tick** (r6-0121; core47): 13AF28 (0x13B4D0..0x13B534) calls the rail instance's entity
  vt+0x15C with the rail gravity (0, 0, -980) minus along x 0 at the hit point, before integrating the velocity. For an AnimTeeter
  (0x4908F8) that is 342538, the lever torque, so the log teeter tips under the rider. The port only pushed at the 106848 attach.
- **The instance record after 106F78** (r8-0165; core49): 105398 runs 106F78 (the rail-body contact; its 105D98 can change +0x1E0)
  before 32F650 / 104E70, and the record's closing speed and direction use the velocity after it.
- **The contact cache's detail mode** (r5-0160; core49): 32B6E0's cache is {patch, cell u / v, half, detail +0xC}. A query in the
  other detail mode (138640 coarse against 137860's human detailed) skips the cached cell and keeps it.
- **108388 reads the live control** (ai-idle 1797; core51): one 105398 pass can run 108388 twice, from 106F78's notify and then from the
  instance contact. After the first enters control 3 (11FEC8(3)), the second's 11FEE8 check (0x1083D0) sees 3 and returns before its
  draws. The port's run_rider_instance_contacts passes liveControl.
- **A script destroys a set-piece entity** (r4-0236, the CRA3 blimp; core51): builtin2 / builtin29 use the same deleting destructor
  (vt+0x8 mode 3) as the section leave 0x34FD90. stage_destroy_entity tears down the spline piece and attached LiveComp
  (set_piece_entity_destroyed) and clears the section entry, so the next enter runs slot 1 again. See set-pieces.md.

## Exact start seeds (2026-10-05, physics-jank agent)

- **What a seed is:** the Single Event start (core begin_event_rider, race participant, per-character mapping) copies the PS2 countdown
  anchor's rider block (generated/event_start_seed.hpp from tools/generate_event_seed.py). It is a recorded result: its bits come from
  the arithmetic that ran the race load and the motion-3 hold up to the anchor, so mode-1 seeds are 1 ULP off on the console
  (Snow Jam x / z: C800C64F / C85F68B4 against C800C650 / C85F68B3).
- **Exact set:** tools/export_exact_event_starts.py (assembly audit + export_event_start on each local/reference-exact anchor ->
  local/assets/native-exact/<code>/event-start.json), then tools/generate_event_seed.py --exact -> generated/event_start_seed_exact.hpp
  (git-ignored). web/event_start_select.hpp takes it in SSX_PS2_EXACT_FPU builds while exactArithmetic is on; mode-1 builds are
  byte-identical. Snow Jam's exact anchor is characters/zoe/countdown.p2s (the menu-path snow-jam-countdown-anchor.p2s carries Zoe's NIS
  head and fails the audit). 12 courses so far; CHP2 / EBA3 wait for anchors, the backcountry ready states are not covered.
  Exact-base riders/zoe-race on the exact core: seedErrorCm 0, exact through 490 (19 with the mode-1 seed).
- **Computer riders:** tools/export_npc_riders.py --exact writes local/assets/native-exact/<code>/npc-riders.json (never
  web/public/assets); compare-ai-capture.mjs --document takes it.
- **Running the load instead:** the PS2 places the grid in 1297C8(C, 0 / 1) -> 11D390's event branch (0x11D564: 11FE78(3), 11FEC8(6), the
  tail clears; no 11DE60 / 11DF18), then the start controller holds motion 3 (11E098 every tick) up to the anchor. The port has the
  pieces for an in-world start (event_grid_start, place_rider_region for the rolling start's 11DE60), but the Single Event anchor also
  carries words the load computes elsewhere (the speed limit's motion-3 fixed point, +0x380 / +0x390 contact normals, the route
  words, the camera block). Running 11D390 + the hold would make the seed a computation, at the cost of porting the load's grid
  placement (1297C8's grid rows) and replaying the hold ticks before tick 18. Not done; the exact seed set covers the swap.

## Computer riders' stage triggers (2026-10-05, physics-jank agent)

- **PS2 order (decomp + EE oracle, hl2/attack-bra2-b tick 1161):**
  - 128AF0 runs each rider's 121818 in slot order. Its first step (0x121854) calls the entity of the contact that 105398's 104E70 stored at rider+0xA30.
  - For a stage-script trigger that's 355770 → 34FE00 → 2D19B8 → 30A060, which runs the slot-2 program with that rider as the player object. Builtin 27 (0x2FF850) → 10F1C0(rider, type, amount) acts on that rider at once (type 1: 10E770, +0x2E8 = 5).
  - Only then do 125AD0 (progress) and 112338 / 1125C0 (the route) run, then 125228, 117C28, and 120E30, which clears +0xA30.
- **Port:** every rider context holds the stage world (815 instances in each computer rider's context on BRA2), with the posed pickup collision
  shared from the human's entity pass (stageWorldSharedPosed, docs/obstacle-collision.md).
  - A computer rider's browser_stage_triggers runs the program in its own context, in slot order, with builtin 27 on that rider.
  - The shared-world log (event 6) replays the instance changes into the other contexts, the human's included, without the effect.
  - hl2/attack-bra2-b 1162: Griff's speedboost_1000 fires in his context (fired log 1162 / 758544 / program 14 in every context). Every rider
    and the RNG are exact to the end.
- **Removed:** pv npcStageTriggers, which routed a computer rider's contact to the human's context, with its exports _stage_foreign_contact /
  _stage_apply_effect and riderHost.stageContact.
  - It only fired for a context whose stageInstances was empty, and no computer rider's context is.
  - With the shared world it never fired: 0 hand-offs over every six-rider gate (48 scenarios, all passing) with the switch on, core28.
