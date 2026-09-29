# Attacks (0x1163B0) and board press (control 1) recovery

## A. Attacks with no opponent — ported and verified

`engine/attack_control.{hpp,cpp}` is a complete port of `0x1163B0(rider, attackLeft, attackRight)`.
The animator sits behind an access-callback boundary (the same pattern as `grab_lifecycle`):

- `311AE8` class
- `312AA0` requested semantic
- `311B20` with `1446A0`/`1446B8`: latched and newly raised marker bits of the first sequence
- the `+0x90` rate write
- `+0x94/+0x98/+0x9C` weight, target and fade reads and writes
- `313A10` fade-to
- `3128E8` play (bone mask `rider+0x8C0` placed in `animator+0x20` for the call, then restored to −1)
- `311E88` fade

**Behaviour.** The upper channel is channel 1. Its semantics are:

| Semantic | Meaning | Class |
|---|---|---|
| 322 | BLOCK | 3 |
| 323 | PUNCH_TS | 13 |
| 324 | PUNCH_HS | 13 |

Button mapping:

- **Right only:** 323 in regular stance, 324 in switch. Facing `+0x340 = +0x1A0`.
- **Left only:** 324 in regular stance, 323 in switch. Facing `+0x340 = −(+0x1A0)` (all four lanes).
- **Both:** 322.

Holding the matching button when marker 2 (latched and newly raised) fires does three things:

- It freezes the channel-1 rate at 0.
- It plays a channel-0 cycle: 325 BLOCK_CYC, 326 or 327 charge (from 323/324).
- For the block cycle, it copies the channel-1 weight, target and fade onto channel 0.

While the channel-0 class is 13:

- Strength `+0x350` moves toward 1 by `timeScale·0.0111111`, using EE guard-bit add/sub.
- Strength is written to the channel-0 weight and target.

Releasing the button or switching sides restores the channel-1 rate to 1 and fades channel 0 over 0.1 s. Other rules:

- A second press is ignored until marker 1.
- Both buttons during an early punch become a block.
- The function returns 1 while the channel-1 class is 3 or 13.

**Callers.**

- Cruise `131620` reaches the attack (`1317E8`) only after the handplant, rail, jump and board-press attempts. A held attack clamps the turn target: `-0.5<=t ? min(t,0.5) : -0.5` (`131804..13182C`).
- Natural air `12F730` uses the `+0x8` latch. A held attack returns before handplant, rail attach and the targets, so the PS2 stays in control 4.
- Exits `131C30` (next control ≠ 4) and `12FB68` (next ≠ 0) fade channel 0 and restore the channel-1 rate while the class is 3/13 (`originalAttackControlExit`).

**Browser.** `web/attack_gameplay.inc` is included from `animation_bridge.cpp`.

- The cruise hook runs in `step_rider` just before `groundTurnTarget`.
- Natural air is evaluated in the controller stage, before handplant and rail. The passive step's `access.upper` reuses that result.
- The exit check runs before `graph.advance`.
- `attack_info()` is a QA export.
- Captured `rider+0x8C0` = `0x8000fffe` (the same value that `115D48` uses).

The attack pose depends on the original channel priorities `{3,2,1,0,0,0}` (`0x48D808` → sequence `+0x84`). The capture confirms these values, and they are now used by `enter()`.

A related fix: `1211F8` still approaches turn, crouch and brake on the airborne control-0 tick of a passive departure. The browser skipped this tick, which put the turn one step behind whenever it was nonzero.

**Oracle.** `tools/test_attack_control_native.py` (`tests/attack_control_reference.cpp`) runs the recompiled `0x1163B0` against the port over 200,000 random cases:

- Coverage: all class/semantic combinations, switch stance, strength near 1±step, time scales, flags and masks.
- Every case is exact, including the ordered play/fade/`313A10` calls with force flag and bone mask, `+0x340`, `+0x350`, the channel rates/weights/fades and the `animator+0x20` restore.
- Counts: 175,118 cases return 1 and 24,882 return 0. There are 128,128 plays, 75,079 fades, 980 block-cycle copies and 2,571 charge updates. All 64 six-bit turn clamps are exact.
- Mutations are caught: `<` changed to `<=` in the charge, the both-held marker-0 gate, and the force flag.
- Edge: a denormal right-vector lane is sign-flipped exactly as in the recompiled code. This never happens with a unit vector.

**Captures** (ARMSX2, `--pad --zoe`; all in `test-ps2-captures.mjs`):

| Capture | Result |
|---|---|
| `passive-inputs` | L1 plus stick while carving, then a 61-tick attack-held control 4. Exact through 687 (was 608). |
| `attack-mix` (new) | Ground block → block cycle, R1 punch charge and release, re-press during a punch, L2, and L2+R2. 282/282 ticks exact. Channel 0–2 sequences (semantic, rate, weight, target, fade, stop, time) match on every tick. The 24 body bones match with 0 error. |
| `attack-air` (new) | L1 punch held through a passive departure, then released in the air (4→5 exit fade). 370/370 ticks exact, with sequences and poses exact. |

**Remaining (A).**

- `passive-inputs` 688: the landing tick is 0.09 cm/s off. The cause is the departure tick's surface frame.
  - `+0x3A0/+0x3B0`: the PS2 rebuilds the frame from the new contact normal, while the web keeps a stale or unnormalised one.
  - `retainedGroundLateral` is also one tick old.
  - Together these give a constant 1.1 cm air-pose offset when turning. It also exists in `neutral-3000` (0x3A0 at 626) but is invisible there because turn = 0.
- The pair-attack consumers of `+0x340/+0x350` (opponents) are not wired.
- The rail controller's upper-action callback is separate. It was not checked against `1163B0`.

## B. Board press (control 1) — ported and verified

Pushing the right stick up (nose) or down (tail) while riding enters control 1. The
browser now runs the port of the original routines (`engine/board_press.hpp`,
`engine/board_press_animation.hpp`) through `web/boardpress_gameplay.inc`.
The keyboard mapping already existed: TFGH is the right stick and V is R3.

### Original code (SLUS_207.72)

| Part | Address | Port |
|---|---|---|
| Entry (cruise `1317D8`, rail `131E80`/`131ED8`) | `1161D0` | `originalBoardPressEntry` |
| Enter / update / exit | `12FC60` / `12FC80` / `12FE98` | `originalBoardPressEnter/Update/Exit` |
| Airborne while in control 1 | `12FFF8` | `originalBoardPressAirborne` |
| Phase 0 (press in) / 1 (hold) / 2 / 3 (release) | `130228` / `1303E0` / `1304D0` / `1304E0` | `originalBoardPressPhase0/1/3` (phase 2 sets phase 1) |
| Phase-1 helpers | `1306B0` reverse, `1307B8` R3 ollie, `1308D8` BoardPivot, `130DD0` depth/effect, `131200` release | `originalBoardPressReverse/Ollie/Pivot/Depth/Release` |
| Pivot spring / finalise / stance flip | `1313A8` / `131428` / `12FEC8` | `originalBoardPressSpring/Finalize/FlipStance` |
| Jump request (`+0x360` latch) / jump-out | `1162C8` / `131348` | `originalBoardPressJumpRequest/JumpOut` |
| Scoring | `1199F8` press, `119AD8`+`119898` pivot, `119A38` end | `originalBoardPressScorePress/Pivot/End` |
| Animation kinds 13 / 14 / 15, completion kind 9 | `1047F0` / `104728` / `1046B0` (→`103CC8`), `104BD8` (→`312BD0`) | `engine/board_press_animation.hpp` |
| Air controller `+0x330` | `133590..133634` in `133308` | `originalBoardPressAirStyle` |
| Landing re-entry | `13A5D4` in `139C88` | `originalBoardPressLandingSemantic` |
| Rail attach with `+0x330` | `106D9C` in `106848` | `originalRailAttach` (`engine/rail_motion.hpp`) |

Rider fields: `+0x268` press depth (drives the 24/25/32/33 seeks), `+0x274` board depth
(kind-15 blend, the 1 s lock and the crash), `+0x280` pivot (−1..1 × π), `+0x330` press style
(0 none, 1 nose, 2 tail), `+0x360` jump latch, `+0x328` rail style, `+0x320` reverse stance.
The control object is motion owner `+0x1D0`: `+0` phase, `+4` time in the press, `+8` time at
full depth, `+0xC` idle time, `+0x10` R3 latch.

### Behaviour

- **Entry.** `1161D0` declines a zero press, rail styles 3/4, and (on the ground only) a rider
  travelling against `+0x3A0`. Up plays 24 with `+0x330 = 1`, down plays 32 with `+0x330 = 2`, then
  control 1. Cruise reaches it after its jump request and before the attack, so the entry tick
  runs no cruise boost, turn, crouch or animation work.
- **Update `12FC80`.** `116378`, `116120`, `106848`, `12FFF8` and `1162C8` can each end the tick.
  Otherwise it runs `114130(BoostHeld, BoostPressed)` and `113F88(0,0)`. On a rail (`+0x328 ≠ 0`)
  it runs `113F38(clamp(RailBalance, ±.5))`; on the ground `113E80(clamp(CruiseTurn, ±||+0x280|−.5|))`.
  Then `115B58`, `115D48` and the phase.
- **Phase 0.** `+0x274` moves to 0.5 at ts/60 and `+0x268` to 1 at 2·ts/60. When `+0x268` reaches 1
  it scores `1199F8` and enters phase 1 with 28 (nose, `+0x280 = 0`) or 36 (tail, `+0x280 = 1`).
  Releasing the stick first goes to phase 3 (25/33).
- **Phase 1**, in order:
  - `1306B0`: a reverse turn (`114CC0`) swaps nose and tail through 23/31. Completion kind 9
    continues into 28/36.
  - `1307B8`: R3 is latched; releasing it finalises the pivot, plays 35 (`|+0x280| > .5`) or 27, clears
    the style, then `114298(rider, 1.0)`, motion 1 and control 5 — the ollie.
  - `1308D8`: BoardPivot. It springs back (`1313A8`) when both sticks are under 0.1, when travelling
    backwards, or while R3 is latched. Otherwise it compares the stick angle (`atan`, quadrant-fixed,
    wrapped to ±π) with `+0x280·π`. When not already pivoting and the difference is ≥ 27°, it kicks
    `+0x280` to ±.85 or ±.15. Otherwise it rotates at `Δ/60·2ts`. Inside .15..0.85 it plays
    29/30 (nose) or 37/38 (tail), flipping the stance (`12FEC8`) when a pivot starts.
  - `130DD0`: the `+0x274` target is the press, limited to `0.5+t/2` for the first second and then
    snapped to 0 or 1. The rate blends the curves at `0x4FE920`/`0x4FE940` by `min(t·0.2, 1)` (rate 1
    when crossing 0.5). **Holding full depth for more than 1 s is a hard crash**: `10EB30(rider,
    358/359, 0, +0x438, block)` with the rider position, velocity direction, −Z and speed. The earlier
    notes called this an effect.
  - `131200`: 0.5 s with no input, or pushing the opposite way past 0.8, sends `+0x274` to 0. The
    press ends (phase 3, 25/33) once `+0x274 < 0.5`.
- **Phase 3.** Waits for `+0x268` to empty (a re-press returns to phase 0 with 24/32), scores
  `119A38`, then returns to cruise (`115640`, control 0) or, on a rail, plays `1326C8` and returns to
  control 7.
- **Air.** `12FFF8` (control 1 in motion 1) plays 27/35 and requests control 5, or finishes an
  active pivot first (phase 2, scoring `119AD8`+`119A38`, 287). Control 5 writes `+0x330` every tick
  from the BoardPress step and the pre-step adjust flip. A landing with `+0x330 ≠ 0` plays 26/34 and
  re-enters control 1, skipping `115640`, the reverse turn and the landing animation.
- **Rails.** On a rail, control 7 calls `1161D0` through `131E80`/`131ED8`, and control 1 then runs
  with rail motion 4. `106848` handles two more cases:
  - A rider already in control 1 stays in control 1, with style 1 or 2 taken from `+0x320`.
  - A rider arriving from the air with `+0x330` plays 26/34 and enters control 1 instead of control 7.
- **Animation.** Kinds 13/14 set slot 0's clock and run only the sequence fade:
  - Kind 13 (29/30) seeks `|+0x280|`; 37/38 seek `1−|+0x280|`.
  - Kind 14 (24/32) seeks `+0x268`; 25/33 seek `clamp(1−+0x268)`.

  Kind 15 is the `103CC8` three-way by `clamp(2·+0x274−1)`. Its leaves are 28: 49/48/50 and
  36: 60/58/59. Completion kind 9 moves latched bit 63 into the raised flags (`144670`) and plays 28
  or 36.

### Browser integration

- `web/boardpress_gameplay.inc` holds the adapter.
  - `board_press_step` runs in step_rider's controller slot, after the handplant and rail steps. It
    runs the cruise entry and the control-1 update, including 12FFF8 in the air.
  - For that tick, step_rider skips the cruise boost control, targets, reverse check and ground
    animation selection. The boost tick receives control 1.
  - The ollie calls `browser_controller_takeoff(1.0)` (`web/core.cpp`: `114298`, motion 1).
  - The crash path uses the existing `enter_crash`. Motion 2 then runs in the same tick.
- `board_press_filters()` approaches `+0x268/+0x274/+0x280` (`1211F8`) at the start of `animation_tick`.
- `finish_landing` takes the `13A5D4` branch.
- In the air, `animation_tick` keeps control 1 (no passive departure) and gives control 5's first
  update to the next tick.
- Control 5 sets `+0x330` just before `originalAirControlStep`.
- The boost ribbon now receives `+0x330` (the front/back offset in `engine/boost_ribbon.hpp`).
- Rails (`web/rail_gameplay.inc`):
  - control 7 now forwards BoardPress (`command.transfer`);
  - `boardPressEntry` runs `1161D0` and the `132048` control-7 exit;
  - the attach accepts control 1 and hands `+0x330` to the rail view;
  - control 1 runs `board_press_rail_update` in place of `131D30`;
  - after a lost rail, the next tick's 12FFF8 runs in the core slot;
  - an R3 ollie on the rail uses `rail_controller_takeoff`.
- `pad_tick` routes control 1 (`web/input_bridge.inc`). `ground_state_dump` reports
  `+0x268/+0x274` (and `+0x360` in control 1). `board_press_info()` exports the state.
- **Exporter** (`tools/export_animation_samples.py`):
  - kinds 13/14/15 now export their single authored `311710` leaf (24, 25, 28, 29, 30, 32, 33, 36,
    37, 38);
  - 28/36 get three-way maps from the loaded lookup (`*(gp+0xD8C)+0x1030+leaf*4`);
  - the rail kind-5 initial clips that had been hand-patched into the package are now generated.

  Zoe's package was regenerated. The only differences are these additions.
- **Fixed on the way (verified by the captures).**
  - The air-adjust triplets `+0x28C/+0x298` were never approached in the air, so every air-adjust
    animation chose 301. They are now approached after `133308`'s targets, and on rail frames `gs`
    takes the rail step's values.
  - The PS2 capture record now also stores motion owner `+0x1C0..0x200` (record offset 9072:
    the control-1 object), and `compare-ps2-capture.mjs --board-press` checks `+0x330`, the control
    object and the channel-2 semantic on every tick. `BP_ALL_SEMANTICS=1` checks the semantic
    everywhere, and `BP_TRACE=a:b` prints `board_press_info`.

### Verification

**Instruction oracles** (recompiled originals with the PCSX2 EE FP correction against the port;
run `python3 tools/test_boardpress_{control,score,animation}_native.py`):

| Oracle | Routines | Cases |
|---|---|---|
| control | `1161D0` 60,000; `12FC60`/`12FE98` 20,000 each; `12FEC8` 30,000; `1313A8` 20,000; `131428` 40,000; `131348` 30,000; `1162C8` 40,000; `12FFF8` 50,000; `130228` 50,000; `1306B0` 30,000; `1307B8` 40,000; `1308D8` 120,000; `130DD0` 80,000; `131200` 50,000; `1303E0` 80,000; `1304D0` 10,000; `1304E0` 50,000; `12FC80` 200,000 | 1,020,000 |
| score | `1199F8` 40,000; `119AD8`/`119898` 60,000; `119A38` (real `117838`) 40,000 | 140,000 |
| animation | kinds 13/14 120,000; kind 15 60,000; completion kind 9 20,000 | 200,000 |
| **Total** | | **1,360,000** |

- Every case compares the rider, owner, animator and scorer memory, the ordered callee log, and the
  `10EB30` parameter block, bit for bit.
- `31BE50` (sine/cosine) and `31C228` (atan) are the recompiled originals.
- Mutations are caught: the kick constants, the rumble `<`, the pivot-spring `<=`, the curve segment
  choice and branch, the blend constant, the stance flip, the latch rule, the VU divide and the ±2π
  wrap branches.
- `133308`'s `+0x330` write and `13A5D4`/`106D9C` sit inside very large routines. These are verified
  by the captures only.

**Original-game captures** (ARMSX2, `--pad --zoe --sync-rng --board-press`; scripts
`local/ps2-capture/scripts/boardpress-*.json`). Every listed tick matches position and velocity bit
for bit, with `+0x330`, the control-1 object and the channel-2 semantic every tick
(`BP_ALL_SEMANTICS=1`):

| Capture | Exercises | Result |
|---|---|---|
| `boardpress-nose` | entry (24), phase 0/1 (28), release, phase 3 (25), cruise | 239/239 exact |
| `boardpress-tail` | tail press 32/36/33 | 329/329 exact |
| `boardpress-ollie` | R3 ollie (1307B8), air `+0x330`, landing into control 1 (26), release | 250/250 exact |
| `boardpress-pivot` | BoardPivot kick/rotation (29/30), stance flip, spring back, finalise | 330/330 exact |
| `boardpress-circle` | full right-stick circle (±π wrap, 37/38 and 29/30), 12FFF8 in the air | 329/329 exact |
| `boardpress-repress` | release then re-press in phase 3, nose then tail | 330/330 exact |
| `boardpress-long` | full depth held > 1 s: the 130DD0 crash `10EB30(358)`, recovery | 380/380 exact |
| `boardpress-jump` | Cross during a press: 131348, control 2 (245), charged jump | 250/250 exact |
| `boardpress-air` | tail press in the air (`+0x330 = 2`, air adjust), landing into control 1 (34) | 249/249 exact |
| `boardpress-pivot-ollie` | tail pivot (38/37) then R3 ollie from the pivot (35) | 330/330 exact |
| `boardpress-rail` | press on the fence rail: control 7 → 1161D0, control 1 on motion 4, rail lost, 12FFF8 (27) | exact through 781; 782 is a web-only air instance contact |
| `boardpress-railair` | tail press in the air onto the rail: 106D9C (34, control 1), phase 1 (36) on the rail, 12FFF8 (35) | exact through 791; 792 is an air instance contact |
| `boardpress-railjump` | rail press then Cross: control 2 on the rail (245), jump off | exact through 824; 825 is a ground rail re-attach the web does not make |

All thirteen are in `web/test-ps2-captures.mjs`. Positions stay exact, and so do the board-press
fields and semantics.

`web/test-board-press.mjs` (in `npm test`) replays nine of the scripts without the private captures. It checks 46 bit-exact
original checkpoints: position/velocity words, control, semantic, `+0x330` and phase. The existing
capture baselines still pass, and `jump-tricks` no longer shows its `+0x28C/+0x298` field differences.

### Remaining gaps

- **Rail R3 ollie and rolling onto a rail in control 1 are not capture-verified.** Both are
  implemented: `rail_controller_takeoff` and the control-1 `106848` restyle. The Snow Jam fence rail
  is too short to reach phase 1 before it ends (`boardpress-railollie` shows R3 held through phase 0,
  which does not latch), and no ground-level rail was scripted.
- **Crash entry from the ground.** In `boardpress-long`, `+0x20C/+0x2BC` differ on the crash tick
  only. The browser's `10EB30` motion-2 entry zeroes the board-alignment and extra-lean triplets. The
  original runs the motion-0 exit `13F410` (target 0 at rate 0.05). Positions stay exact through the
  recovery. The same applies to any ground-motion hard crash.
- **Soft collisions during a press are dropped.** In the original, `108388` runs the jump-out
  `131348` and then the soft control 3. The browser's soft-collision callers (body contacts, instance
  contacts, rail exits) still skip reactions that carry `cancelControlOne`, so the rider stays in
  control 1 and only the physical response applies.
- **Unmodelled side effects.** The `2A1560` rumble and the `294170` ollie event are only counted
  (`board_press_info`). The `2F6AC8` jump statistic is ignored. `116378` (`+0x470`) is not modelled,
  as in cruise.
- **Reverse turn during a press** (`1306B0`: 23/31, completion kind 9) is oracle-verified but not
  captured. It needs a rider travelling backwards while pressing.
- **Branches inside large routines.** `133308`'s `+0x330` write, the `13A5D4` landing branch and the
  `106D9C` attach branch are verified by the captures, not by instruction oracles.
- **Where the rail captures stop.** They end at existing non-board-press divergences: air instance
  contacts at 782/792, and a ground rail attach at 825.
