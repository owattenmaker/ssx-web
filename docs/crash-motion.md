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
- **Open:** air-eba3 823, rail-bra2-a 1760, attack-bra2 756, plant-pipe-a eye bones 901 (see docs/HANDOFF.md).
