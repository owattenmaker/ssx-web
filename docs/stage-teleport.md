# Stage builtin 34 "Teleport current player" (0x300770 -> rider vt+0x54 = 0x123210): spec

Recovered from SLUS_207.72 (disassembly of 0x123210 / 0x11D660 / 0x26E800 / 0x2F1A08) and the BRA2 stage programs;
reachability from the setpieces-bra2 and sections/bra2 PS2 runs. Status 2026-09-23: the stage side is in
web/stage_script_gameplay.inc (case 34 -> `browserStageTeleport(matrix)`); the rider side below is not wired yet.
Status 2026-09-27: the rider side is ported (web/stage_teleport.inc, pv `boothTeleport`) for the human and the computer
riders and checked against PS2 captures that inject the beam contact (section 4).

## 0. Builtin 34, 0x300770

- Argument block: one key, key 0 = instance resource, type int (types gp+0xB50), default -1 (gp+0x2598, lazily
  initialised through gp+0x25A0). Float given where int expected: copied raw (the shared parse rule).
- key0 == -1 -> instance = ctx+0x290 (current instance). Otherwise resource -> instance through the world table:
  `W=*(*(gp+0x16C8)); track=*(*(W+8)+4*(res&0xFF)); e=*(*(track+0x1C)+4*(res>>8)); inst=(e>>8)<<2` (0 when absent).
- `player = ctx+0` (ctx = *(gp+0xCE8)). No-op when player < 0 or inst == 0.
- Matrix: `inst+0xC` (entity) ? entity vt+0xC4() : `inst+0x10`. Object entity vt+0xC4 = 0x356078 returns the
  primary modifier's vt+0x94 matrix (entity+0x1C container slot +0) or else `instance+0x10`. UVScroll (builtin 21)
  sits in container slot +4, so the BRA2 destinations always resolve to **instance+0x10**. Verified in the BRA2
  countdown-anchor RAM: instance+0x10 of all five destinations equals `stage-world.json` `teleports[].matrix_bits`
  bit for bit (unscaled; the 2.65 scale of the water-tower models is not in the matrix).
- `rider = *(*(*(*(gp-0x848)+0x84)+0xC)+0x28+4*player)` (race roster; human 0, computer riders 1..5), then
  `rider->vt+0x54(rider, matrix)`. Human vtable 0x4583A8 and computer vtable 0x458660 both have 0x123210 at +0x54,
  so **computer riders teleport too**. Returns nil.
- ctx+0 semantics: 0x30A060 sets `ctx+0 = rider interface vt+0x3C()` = rider+0x86C (roster/grid slot) for the
  contacting rider, runs slot 2, restores the old ctx+0. It skips everything for a human (vt+0x44) that has finished
  (+0x480) unless game mode (S+0x214) is 4 (free ride). In the browser: this core's own rider; replayed contacts
  (`sharedWorldReplay`) must not teleport.

## 1. 0x123210(rider a0, matrix a1) — exact order

Notation: rows r0..r3 of the 4x4 matrix (row-major, r3 = translation, w=1). G=*(gp-0x848), S=G+0x84.

1. `270970(*(S+0x28), rider+0x86C)` — race-event (checkpoint/gate) observer, **not the camera**. If event+0x61C,
   +0x620 and event+0 (mode) are all 0: `slot=270730(id)` (human roster index by rider vt+0x3C, -1 for computer
   riders); if slot>=0: `270DE0(ev,slot)` -> if ev+0==0: `26E800(ev+0x494+0xB4*slot)` (if entry+0xB0: clear bit
   `1<<entry+0` in `(entry+0xB0)+0x1E`, entry+0xB0=0: releases the human's pending gate link), then `270628(ev)`
   (ev+0x3D4=0 when set and the human count is 1). Anchor: all zero, i.e. a no-op at race start. Same observer as
   116120; the browser currently skips it for resets as well.
2. Destination point (stack sp+0), VU0 (chop, the browser's `terrain_original::add/mul` inside `OriginalRounding`):
   - offset `o = (a, b, 0, 0)` by `i = rider+0x86C`: 0:(2,0) 1:(1,0) 2:(2,-2) 3:(2,2) 4:(1,-2) 5:(2,1);
     any other i: (0,0,0,0) (the table store is skipped).
   - `t_k = add(add(add(mul(r0_k,a), mul(r1_k,b)), mul(r2_k,0)), mul(r3_k,0))` (vmulax/vmadday/vmaddaz/vmaddw,
     k = x,y,z,w; mathematically add(mul(r0_k,a),mul(r1_k,b)) except the sign of a zero result).
   - `u_k = mul(t_k, 100.0f)` (0x42C80000, vmulx), `P_k = add(r3_k, u_k)` (vadd; w = 1).
   i.e. human (slot 0) lands 200 cm along the destination's local +X row; offsets are in local X/Y, units of 100 cm.
3. Direction `D = M*(1,0,0,0)` (constant 0x4FF140), same accumulate chain -> D = r0 (w 0). Only D.xy are used by
   11D660 (heading; its zero tests are value compares, so the -0 detail is irrelevant).
4. `119368(rider+0x790, 1)` — score "reset": +0x1A4 += 11A7A8 lost points, 117838 score reset, 1175F8 combo reset,
   +0x120 (reset count) += 1. Its return (meter * gp-0x7A28, the -0.7 coefficient) is **discarded**: no boost
   penalty/award (unlike 12F398 which passes it to 10E098). Browser: `score_reset(true)` and ignore the value.
5. `11FE78(rider, 0)` -> `1112B8(rider+0x77C, 0)`: **unconditionally** runs the current motion's exit
   (table 0x456B10 by +0xDE0: 0 13F410 ground, 1 139A18 (empty), 2 136F28 crash, 3 none, 4 13C5A0 rail,
   5 139178 handplant), stores +0xDE0=0, runs motion-0 entry 13C7A8.
   - 13F410 (only if the old motion was 0): +0x208/+0x2BC/+0x2C8 decay targets 0, rates 0x3D4CCCCE/…/0x3FD55556,
     `lastLeave = lastLeave==0xFFFFFFFF ? tick-500 : tick` (tick = 0x1298C8).
   - 13C7A8: boardBouncePhase=0, depth1/depth3 = bodyScale*material, boardNormal=normal, owner+0x10 = tick
     (ground-focus stamp), and **velocity *= factor**, `factor = 0.7f (0x3F333333)` if `tick-lastLeave <= 40`, else
     `min(A(mul(float(d-40), 0x3C23D70B), 0.7f), 1)` (EE add with guard bit), per component `mul(v_k,factor)`.
     => a grounded rider always loses 30 % (d = 0); an airborne rider keeps more the longer it has been in the air.
     Browser: `originalLandingGroundLeave` / `originalLandingGroundEnter` (engine/landing_motion.cpp).
6. `11FEC8(rider, 0)` -> `111538`: current control's exit (table 0x456B90 by +0xDE4: 0 131C30, 1 12FE98, 2 12E9B0,
   4 12FB68, 5 134CB0, 7 132048 (rail), 8 12E690 (crash), 11 132F98 (handplant), 3/6/9/10/12/13 none), +0xDE4=0,
   control-0 entry 131608 (zeroes idle clock +0x35C and the +0x360 board-press latch).
7. `125038(rider)`: if `S+0x84` and rider+0x870 (device) >= 0 and `viewport(dev)=*(S+0x5C+4*dev)` has +0x78==1:
   `2E4578(viewport)` = stop that viewport's screen effect (the reset white fade). Computer riders (dev -1): no-op.
8. Camera notify **before placement**: `Z=*(*(*(S+0x84)+4)+0xA0)` (camera director, vtable 0x45B908), vt+0x1C =
   0x15CC70(Z, riderId): for every director node's algorithm: vt+0x1C = DEFAULT_3 set-target 0x176FE0, which only
   acts when the algorithm's target rider id == riderId (166C60 reset + update + finish, 166550(559.744, 300)
   offset direction, vt+0x28 update) = browser `original_camera::setTarget(node.algorithm, input)`; then
   `15E030(Y=Z+0x10)` -> `166F28(Y+0xC0)` (set-target of the outer camera's embedded DEFAULT_3, always, own
   target) and `2F41A8(Y)` (viewport render object vt+0x74). The input is the rider **as it is now**: old
   position/head/forward, velocity already scaled by step 5, motion 0. For computer riders the node set-targets
   are skipped (target is the human).
9. `speed = vsqrt(((vx*vx + vy*vy) + vz*vz) + vw*vw)` of rider+0x1E0 (chop mul/add, `terrain_original::sqrt`;
   w is 0 in practice) — computed **after** step 5's factor, **before** 11D660 clears velocity.
10. `11D660(rider, &P, &D, semantic a3 = 5, clearance f12 = 0.0f)`. Same placement as reset control but semantic 5
   = RNORM_FWD_CYC (class 7, clip 8960, blend 0.23 s; reset uses 287 A_CYC air). Clearance 0 is non-negative:
   one world probe from P-7000 to P+200 (world Z), preferred fraction 0.9791666865348816, then +0; the heading from
   D.xy; physical frame 11E098; tangents +0x3A0/+0x3B0. Tail: velocity +0x1E0 = 0 (all four lanes), depth/contact
   state, the 19 filter triplets 1F0..2D0, boost amount +0x2FC=0, timeScale +0x300=1, leg weight +0x318=1,
   +0x150 (detached board)=0, +0x320<-+0x324, +0x350/358/35C=0, animation channels cleared, default mirror/root from
   stance, request semantic 5 (3128E8/312598), one full-rate step 11EB60(1), 11EB98, 3103F0, pose sample
   11E150, bounds 332DB8, 122088, 2105B0(+0x86C), rider vt+0x84, and for devices 0/1 **a second camera cut**:
   `*(Y(dev)+0xA8)` (the same director) vt+0x24 = 0x15CCF0 = set-target on every node (no rider check) + 15E030 +
   2F41A8, now with the **new** position/head/forward and **zero velocity**.
11. `v_k = mul(F_k, speed)` for k = x,y,z,w, `F = rider+0x1B0` (physical forward from 11E098; w 0), stored to
   +0x1E0. Note: z is **not** zeroed (11DF18 zeroes it for resets; the teleport does not call 11DF18), so on a
   slope the exit velocity follows the slope. No 833.33 cm/s reset speed, no animation freeze (11DF18 absent).
12. `111890(rider+0x77C)`: FX reset — trails 2DCF28(+0x3B0), sparks 2DAA78(+0x470), 2E8560(+0x520), 2E6640(+0x610),
   2EADC0(+0x9C0), 2EF6A0(+0xAD0), 2D4BE0(+0xAF0), 2E3930(+0xB00), 2DF3B0(+0xB40), fist 2F1148(+0xC70),
   2F64E8(+0xD20), 2C03E8(); then for device >= 0 the viewport object's vt+0x74. Browser equivalents already called
   by reset placement: `reset_trail(); reset_snow(false); reset_impact_fx(false); reset_boost_fx();`.
13. `2F1A00(rider+0x88C)`: stores 0 at +0xA0 of the rider bone-emitter controller (vtable 0x4881D0, updated by
   2F1A08 from 120E88 each tick; +0xA0 is set when its type-0x19 bone emits). Not modelled in the browser.

Resulting state: control 0, motion 0 (ground), placed on the probed ground at the destination (+ slot offset), facing
the destination's +X row heading, speed = |v_before| x ground-entry factor, animation RNORM_FWD_CYC, trails/FX reset,
score combo/pending points lost and reset count +1 (no boost change), camera re-targeted twice (old state, then new
state with zero velocity) and then updated normally at the end of the frame. No air predictor (motion 0).
Sibling 0x1234D0 (vt+0x64) is a different "place at matrix" (no slot offset, motion 3/control 0xD then 6, zero
speed); not used by builtin 34.

## 2. BRA2 content and reachability

Stages: BRA2 track 14 = B_BRA2 (18 programs), track 16 = BRA2 (427). **All teleporters are in track 16 (BRA2, the
race world)**, none in B_BRA2. Programs (global index = track-16 first program 18 + local):

| trigger (slot 2) | local/global program | draw | destinations |
|---|---|---|---|
| 0x43510 mdl_BRA2_watertowerbeam_0001 (slot 1 = 416/434: builtin0 Object + builtin21 UVScroll 0.02/0.037) | 417/435 | builtin77(0,100) < 90 | 90 %: 0xA2C10 watertowerbeam_0002, else 0x22410 watertowerbeam_0003 |
| 0xC9810 mdl_BRA2_phoneboothbeam_0004 (no slot 1) | 419/437 | < 75 | 75 %: 0x99010 phoneboothbeam_0005, else 0x68F10 phoneboothbeam_0006 |
| 0x55B10 mdl_BRA2_phoneboothbeam_0007 (no slot 1) | 420/438 | < 101 (always) | 0x24210 phoneboothbeam_0008 |

(Destinations 0xA2C10/0x22410 have slot 1 = 418/436, same Object+UVScroll; the phone-booth destinations have no
handler row.) Every firing consumes one gameplay-RNG draw (317810 via builtin 77, already ported). No program
hides/debounces these resources, no builtin 29/43/38, and 0x535C11 (=1 in the Metro-City single event) only
affects builtin-38 collectibles — **no race-mode gate**. Countdown audit: triggers have runtime flags 0x210023
(no 0x2000, static body route: contactable in the race); descriptors 360/419/795 are type 2 with node flag 2
(trigger nodes). Watertower trigger 0x43510 gets a 30-tick Object gate (355770) after its section enters; the phone
booth triggers have no entity, so 30A060 runs every contact tick.

Trigger volumes (world AABB, cm): 0x43510 x[-198076,-197514] y[35412,36004] z[-515695,-514640] (5.6 x 5.9 x 10.6 m,
~17.6 m above the nearest AI line 43, 31 m away horizontally); 0xC9810 x[-206775,-206677] y[78158,78212]
z[-499045,-498828] (1.0 x 0.5 x 2.2 m, at ground level ~14 m from AI line 36, field3c 0); 0x55B10
x[-171143,-171049] y[-48903,-48845] z[-590253,-590036] (0.9 x 0.6 x 2.2 m, ~68 m above AI line 53 below it).

PS2 evidence: `local/ps2-capture/runs/sections/bra2-replay.txt` (the setpieces-bra2 full-course run, human only,
isolated from computer riders): the human passes 25 m from 0x43510 (tick 5243, 10 m below it), 30 m from 0x55B10
(tick 10151), 54 m from 0xC9810 (tick 3194); 0x43510/0xA2C10/0x22410 are in the activation list during ticks
4420..5260 / 5080..5860 / 6060..6840. No course capture touches a trigger (see section 4 for the injected ones).
Conclusion: the teleporters are live on the Metro-City race course (off the default line: rooftop / phone-booth
secrets). Computer riders can also fire them (AI lines pass 14..40 m away; not observed).
Suggested ground truth: from `setpieces-bra2/full.tick3218.p2s` (near 0xC9810), `full.tick10018.p2s` (0x55B10) or
`full.tick5218.p2s` (0x43510), script or poke the rider into the volume (`tools/ps2_capture.py --poke`) and record.

## 3. Browser port plan

Existing pieces:
- `web/stage_script_gameplay.inc` `stage_builtin`: no case 34 (falls to `default`, counted). `stage_target()`,
  `stage_instance()`, `stage_current_instance`, `sharedWorldReplay`, `stage_effect` pattern.
- `web/stage_world.inc`: `stageTeleports` (resource -> 16 matrix bit words, loaded from stage-world.json, unused),
  `stage_entity_matrix(resource)` (entity vt+0xC4 equivalent).
- `web/animation_bridge.cpp` `step_reset` `cb.place` lambda: `clear_reset_contacts(); reset_body_queries();
  originalResetPlacement(point, direction, clearance, queryOriginalWorldSegment(...,0,preferred,true));
  reset_trail(); reset_snow(false); reset_impact_fx(false); reset_boost_fx(); apply_reset_placement(value,
  resetStance); crash.actor.detached=false;` + graph reset / `graph.enter(semantic,…)` / one full-rate advance.
- `web/core.cpp`: `apply_reset_placement` (sets grounded=false, controlState=9, **chaseReady=false**, reset_prediction),
  `start_ground_motion` (13C7A8 pattern: grounded=true, `originalLandingGroundEnter`, `groundFocusTick=motionTick`,
  `commit_rider_physics`), `lastGroundLeave`, `motionTick`, `publish_motion`, `camera_for_head`, `chase`
  (`OriginalDirectedCameraState`), `request_rider_reset`.
- `web/score_gameplay.inc` `score_reset(bool)` (119368), `rail_reset_leave`, `detach_rail_for_crash`,
  `attack_control_changes`, crash/handplant exits.
- `engine/original_camera.hpp` `original_camera::setTarget`, `engine/original_camera_director.hpp`
  (`originalDirectedCameraRestart` loops `setTarget` over `d.nodes`).

Steps (all arithmetic inside one `OriginalRounding` scope; use `terrain_original::add/mul/sqrt`, never
`std::sqrt`/native ops, no FMA):
1. case 34: parse `std::array<uint32_t,1> f{0xffffffffu}` with types `{1}`; `target=stage_target(f[0])`; if 0 ->
   nil. If `sharedWorldReplay` -> nil (the contacting core owns the rider). Matrix = `stageTeleports.at(target)` bits
   (fallback `stage_entity_matrix`). Call a new hook `browserStageTeleport(matrix)`; count calls.
2. Factor the reset placement lambda into `place_rider(point, direction, clearance, semantic)` shared by reset and
   teleport, without the reset-only lines (controlState 9, chaseReady=false, reset counters).
3. `teleport(matrix)` in animation_bridge.cpp:
   a. `idx` = rider+0x86C: 0 for the human (online: its grid slot), `npc.slot` for computer cores.
   b. P and D per section 1 step 2/3 (keep the full 4-term chain; P.w/D.w unused afterwards).
   c. `score_reset(true)` (discard the return; no `award_boost`).
   d. Motion exit of the current motion: grounded -> `originalLandingGroundLeave(physicsState, motionTick,
      lastGroundLeave)`; rail -> 13C5A0 path (`rail_reset_leave`/`detach_rail_for_crash` as begin_reset does);
      crash -> 136F28 path; handplant -> 139178; air -> nothing. Then 13C7A8:
      `originalLandingGroundEnter(physicsState, landingProfile.materials[surface], bodyScale, motionTick,
      lastGroundLeave); groundFocusTick=motionTick;` (use the same tick value the ground code uses this tick).
   e. Control exit (per table) + control 0 entry: `physicsState.controlState=gs.controlState=0`, idle clock 0,
      board-press latch 0.
   f. If a reset fade is active (reset control 9): stop it (125038); leaving control 9 also ends the reset.
   g. Camera cut #1 (human only): build an `OriginalCameraInput` from the current state (pre-placement) and run
      `original_camera::setTarget(node.algorithm, input)` for every `chase.director.nodes` node.
   h. `speed = sqrt(add(add(add(mul(vx,vx),mul(vy,vy)),mul(vz,vz)),mul(0,0)))`.
   i. `place_rider(P, D, 0.0f, 5)` (clears velocity, contacts, FX 111890 equivalents, animation 11D660 tail).
   j. Camera cut #2 (human devices 0/1): input from the new state with velocity 0 and the new head (the placement
      pose), `setTarget` on every node (15CCF0).
   k. `physicsState.velocity[k] = mul(physicsState.physicalForward[k], speed)` (no z zeroing), then
      `grounded=true`, controlState 0, no `begin_prediction` / `leave_reset_motion`, `commit_rider_physics()`.
   l. Bump a teleport/placement serial.
4. Do not use `chaseReady=false` (it rebuilds the camera with `originalDirectedCameraBegin`, which is not what
   15CC70/15CCF0 do). Compositor lift/firstFrame are not touched by 15CC70/15CCF0.
5. JS (`web/main.js`): the teleport happens inside `race_end`, after `animation_tick` and `_pose_physical()` were
   read. Re-read the pose/physical frame (head for `_step_camera_head`) after `race_end` when the serial changed,
   and clear the interpolation history like the `resetState[2]` check does. Multiplayer: bump the rider-packet
   placement counter (web/net/rider-packet.js "reset placements") so puppets do not interpolate across.
6. Computer-rider cores (web/ai-racers.js / npc): same path with their slot offset; skip camera cuts and 125038.
   The human core's replay (event 6) must not teleport; the RNG draw of builtin 77 stays as today.
7. Validation: a reference test in the tests/*_reference.cpp style running 0x123210 against the recompiled original
   with stubbed 270970/119368/11FE78/11FEC8/125038/camera/11D660/111890/2F1A00 (P, D, speed, final velocity), plus a
   PS2 capture per section 2.

Open items: exact camera-input head at each cut (pose bookkeeping), 11D660 tail items the reset port still
represents only approximately (docs/reset-recovery.md), 2F1A00 emitter controller and the Y+0xC0 embedded camera
algorithm are unmodelled; the 270970 gate link release only matters when a human gate link is pending.

## 4. PS2 ground truth and the port (2026-09-27)

**Symptom before:** riding into a phone booth (Metro-City race, Single Event or career, and the BRA2 part of the Peak 1 /
whole-mountain free ride) did nothing. The contact reached program 419 (builtin 77, then builtin 34), but nothing assigned
`browserStageTeleport`, so every call was counted as unsupported. The rider stayed in the booth, bumping the back wall. The
contact fired again on almost every tick in the beam, and each firing drew the shared gameplay RNG once more; the PS2 draws
once and the rider is gone.

**PS2 captures** (derived states only, silent; `local/ps2-capture/runs/booth/`, built by `tools/ps2_booth_inject.py`).
Reaching booth 0004 by riding is a secret route. Its door faces north into a bowl, which is reached only down the valley east
of the rampsides wall. Closed-loop PS2 runs along the AI lines never got in. In the browser, a rider placed with
`place_rider_region(-207300, 84910, ...)` heading SE, or at (-206450, 79000) heading (-0.3, -0.95) and steered at (-206740,
78330), rides into the beam. The captures therefore **inject the contact**: the setpieces-bra2-full pads from the countdown anchor, and a guarded hook at 0x121820 (inside
121818, after its prologue) that on a given game tick sets rider+0xA30 = the beam instance (from the world table as in
section 0). 121818 then runs the beam's slot-2 program exactly as for a real contact: 30A060 -> program -> builtin 77 ->
builtin 34 -> 0x123210.

| capture | injections (tick: beam, rider) | PS2 result |
|---|---|---|
| `teleports` | 1700, 1800, 1900, 2000: 0004; 2100: 0007; 2200: water tower 0001 (human) | 0005, 0005, **0006** (the 25 % outcome), 0005, 0008, 0002 (a2c10) |
| `from-crash` / `from-passive-air` / `from-rail` | 1300 / 1900 / 2300: 0007 (human in motion 2 control 8 / motion 1 control 4 / motion 4 control 7) | 0008 |
| `computer-riders` | 1700..1740: 0007 for roster 1..5; 1750..1790: 0004 for roster 1..5 | 0008 + slot offset; 0006 (roster 1, 4) / 0005 + slot offset |
| `camera-log` | as `teleports` to 2100, plus entry logs of 0x166C60 and 0x1624E8 | see "Camera" below |
| `teleports-ai` | `teleports` with --ai-state: the per-draw RNG attribution (`teleports.rng-order.json`) | |

- Destination: P = r3 + 100 (a r0 + b r1) by rider+0x86C exactly (human (2,0): 200 cm along the destination's +X row;
  computer riders slot 1..5 as in section 1 step 2), on the probed ground; motion 0 / control 0; speed kept along the
  placed forward (z not zeroed). Computer riders teleport too (they share 0x123210).
- RNG: one draw per firing, in the contacting rider's 121818 (ra 0x303674 inside builtin 77), after every rider's 121068
  controller draws and before the section draws of the destination (101B60).
- Camera: director set-target 0x166C60 runs twice per camera per teleport, for the node algorithm (0x19EA930) and the
  outer camera's own DEFAULT_3 (Y+0xC0 = 0x1593E10): first with the rider as it is (the 13C7A8-scaled velocity), then from
  11D660's tail with velocity 0. After the 0007 and water-tower teleports (1466 m / 1300 m away) the PS2 **stops the game
  for about 30 frames** while the destination streams in. The game tick stays at 2101, but the camera algorithm steps every
  frame (0x1624E8 logged 30 times) and its velocity filter converges. The port has the whole course resident, so there is
  no stall: its camera catches up over the next ticks instead. This is the only camera difference; after the 0004 teleports
  the algorithm words are exact.

**Port** (`web/stage_teleport.inc`, included at the end of web/animation_bridge.cpp; case 34 in stage_script_gameplay.inc):
section 1 steps 2..12 in order: the slot offset (the human 0, `npc.slot` for the computer riders), 119368, the motion exit
(13F410 ground leave, crash off, handplant reset) and 13C7A8 (`start_ground_motion`), the control exits (control-5 air exit,
rail 132048) and control 0 (idle clock 0), camera cut 1 (`browser_camera_teleport_cut`, web/core.cpp: set-target on every
node with the input of `camera_input_for_head`), the VU speed, 11D660 (`reset_place_at` semantic 5 clearance 0, keeping the
13C7A8 depths, surface 0, the camera not rebuilt; `reset_place_at(..., contactPose)` gives the placed pose 11EB98's board
alignment and 11F3D8 leg solve), the placed pose cached (rider+0x2C, its board frame +0x170/+0x180) and posed-physical frame, camera cut 2 with velocity 0, v = F x speed. `core._stage_teleport_enable(1)` per rider core (main.js
`applyPresentationFast` for the human and the computer riders; the capture comparer by pv / `STAGE_TELEPORT` / the
capture's `booth_injections`), `_stage_teleport_info` (QA), `_stage_contact_inject(resource)` (the comparer's injection).
web/game-tick.js re-reads the rider state and posed frame after race_end when the teleport count changed, and marks the tick
as a placement (no interpolation across). The outer camera's own DEFAULT_3 (Y+0xC0) and 2F1A00 stay unmodelled.

**Verification:**
- `web/compare-ps2-capture.mjs` on the captures (all in `test-ps2-captures.mjs` as `booth/*`):
  - `teleports`: position, velocity, score, bones and boost are exact on all 2329 ticks. The human's RNG draw count equals
    the PS2's non-computer-rider draws on every tick, with both outcomes of booth 0004.
  - `from-crash` and `from-passive-air`: exact end to end.
  - `from-rail`: bones exact, physics exact through 2302. Open: from 2303 the speed differs by 0.04 cm/s (the extra
    lean and board lift of the ground motion; some rail state the port keeps). A rail inside a beam volume does not
    occur on the course.
- Chrome and WebKit (`web/webkit-driver.mjs`), in a Single Event Metro-City race started from the menus (`ui.startSingleEvent`)
  and in the Peak 1 free ride (`?course=PEAK1&peakCourse=1`):
  - riding into booth 0004 gives 0005 and 0006 (both outcomes), and injected 0007 / water-tower contacts land where the PS2
    puts the rider;
  - the five computer riders land at the PS2's slot destinations, to 0.02 cm;
  - no page exceptions (Chrome console clean).

Online: the finish plausibility judge accepts these jumps from a trigger to its destinations only
(`web/server/teleport-beams.mjs`, [multiplayer.md](multiplayer.md) "Stage teleports"); the teleport bumps the packet's
reset placements, so remote riders never interpolate across it.
