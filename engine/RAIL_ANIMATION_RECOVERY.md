# Original rail (grind) animation recovery

Source: PS2 SSX3 `SLUS_207.72` (VA = file offset − 0x1000 + 0x100000),
disassembled with rabbitizer; lookup leaves and bank durations read from the
Snow Jam glide savestate (`local/reference/pcsx2/snow-jam-glide.p2s`).
Native transcription: `engine/rail_animation.hpp` (header-only). Oracle:
`tests/rail_animation_reference.cpp` run by `tools/test_rail_animation_native.py`.

## 1. Headline corrections to the previous assumptions

1. **The rail cycles 18/19/20 do not use `RSFS_GRIND1..4_*`.** The kind-5 driver
   `0x104238` blends `RS_FWD_CYC / RS_BAL_L_CYC / RS_BAL_R_CYC` (18, styles 1/2),
   `RSFS_FWD_CYC / RSFS_BAL_L_CYC / RSFS_BAL_R_CYC` (19, style 3) and
   `RSBS_FWD_CYC / RSBS_BAL_L_CYC / RSBS_BAL_R_CYC` (20, style 4). The exporter's
   `three_way_maps` for 18/19/20 (GRIND1/3/4) is therefore wrong and must become
   the RS/RSFS/RSBS triples in the `{negative, centre, positive}` order below.
2. **Ordinary grinds have no INTO / OUTOF / LAND clips.** Attach plays the cycle
   directly (`0x1326C8`, blend −1 = state-table 0.23 s crossfade), or one of the
   `L_RS_NORMAL / L_RSFS_NORMAL / L_RSBS_NORMAL` landings (68/70/69) when the
   attach comes from air controls 4/5; those complete through kind 6 into
   semantic 3 and control 7 re-requests the cycle on its next update. Rail leave
   `0x13BFA8` plays nothing (its callees `0x105398/0x13C140/0x107888` are not
   `0x3128E8` callers); the air controllers pick the airborne clips. Landing
   classifier `0x13A968` is the ordinary snow landing (61/62/63/66/67), not rail.
3. **The whole `RS*_INTO_FS_GRINDn / RSFS_GRINDn_CYC / _BAL_L / _LAND /
   RSFS_OUTOF_GRINDn` family (semantics 213–244, class 21) belongs to the rail
   uber trick, control 12** (`0x136268` enter, `0x136508` update, record table
   `0x45A038`), requested from control 7 by `0x132620` when an identity is held
   with a charged uber meter.

## 2. Address map

| Address | Role |
|---|---|
| `0x1036A0` | per-sequence driver dispatch: kind = state table +4, jump table `0x456950[16]` |
| `0x456950` | kind → case (`0x103700 + 0x1C·kind`): 0 `0x1048C0`, 1 `0x104940`, 2 `0x1049C0`, 3 `0x104110`, 4 `0x104178`, **5 `0x104238`**, 6 `0x1042A8`, 7 `0x104358`, 8 `0x1045D8`, **9 `0x104660`**, **10 `0x1042E0`**, 11 `0x1043F8`, 12 `0x1045B8`, 13 `0x1047F0`, 14 `0x104728`, 15 `0x1046B0` |
| `0x103918` | completion dispatch: completion kind = state table +8, table `0x456990[11]`; result is written into the animator's requested-semantic slot for that channel |
| `0x456990` | (ELF words `0x103978, 0x103990, …, 0x103A68`) 0 `0x104CA0` remove → 438, 1 `0x104A40` → 287, 2 `0x104A60`, 3 `0x104B78` → 5, 4 `0x104C18` → 5, 5 `0x104B48`, **6 `0x104C38`: 69 → 20, 70 → 19, else 18 via `0x312BD0` (the finished sequence stays and fades out)**, 7 `0x104B98` → 3, **8 `0x104BB8` → 19 via `0x312B18` (replaces)**, 9 `0x104BD8`, 10 `0x104C80` → 434. Corrected 2026-09-22: kinds 6 and 7 were swapped here, so rail entries were replaced by semantic 3 for one tick (metro-jump-tricks 1491, metro-air-tricks 887). |
| `0x104238` | kind 5: rail balance cycle (below) |
| `0x103CC8` | three-way weight-blend driver (already transcribed as `originalThreeWayAnimationStep`) |
| `0x104660` → `0x103BE0` | kind 9: two-way seek-by-magnitude pair (half-pipe balance, semantics 40/45) |
| `0x1042E0` → `0x103BE0` | kind 10: two-way seek-by-magnitude pair (uber `RSFS_GRINDn_BAL_R` / `_BAL_L`) |
| `0x1326C8` | rail cycle/landing semantic by style |
| `0x136268` / `0x136508` | control 12 (rail uber) enter / update; thunks `0x111714` / `0x11181C`; exit `0x111624` is empty |
| `0x45A038` | four 0x24-byte uber records (identity 0..3) |
| `0x132620` | control 7 → control 12 hand-off |
| `0x312B18` | completion replacement: `0x3145F8` remove, `0x3128E8(anim, semantic, −1, 0)`, copy +0x94/+0x98, `0x313A10` if +0x9C ≠ 0 |
| `0x313C50` | slot assign: +4 clip, +0x10 duration = (u16 frames − 1)·(1/30), init time/rate/weight/enabled/loop only when +0x18 was 0, seq+0xC4 seek |
| `0x313CF0` / `0x313D28` / `0x313D40` | set slot time (+seek) / weight / loop |

## 3. Kind-5 driver `0x104238` (verified, bit-exact oracle)

```
rider  = animator+0x60
amount = rider+0x238                 (balance triplet current)
if rider+0x320 != 0: amount = -amount   (0x104250 neg.s; switch/reverse stance)
semantic = sequence+0
18 -> negative leaf 65 RS_BAL_R_CYC,   centre 63 RS_FWD_CYC,   positive 64 RS_BAL_L_CYC
19 -> negative leaf 68 RSFS_BAL_R_CYC, centre 66 RSFS_FWD_CYC, positive 67 RSFS_BAL_L_CYC
else-> negative leaf 71 RSBS_BAL_R_CYC, centre 69 RSBS_FWD_CYC, positive 70 RSBS_BAL_L_CYC
0x103CC8(animator, sequencer, sequence, f12 = timeScale, f13 = amount, a3 = negative, t0 = centre, t1 = positive)
```

`0x103CC8` per tick: slot 0 ← centre (`0x313C50`); if `0 < amount` slot 1 ← positive
with side weight = amount, else slot 1 ← negative with side weight = −amount
(zero balance therefore yields −0.0); slot 0 weight = 1 − side (EE SUB.S);
slot 0 loop = 1; slot 0 clock advances by rate·sequenceRate·(timeScale·1/60)
through `0x3135B0` (wraps, sets seq+0xC0); slot 1 time = slot0.time /
slot0.duration · slot1.duration (`0x313CF0`, seek flag); `0x313800` fade step,
and `0x3145F8` removes the sequence when a stop fade completes. The BAL clips
are **weight blends** (not seek-by-magnitude like kind 11) and both clips loop
in phase with the centre cycle. There are no thresholds or rates in the driver
itself: the shaping is in the balance triplet (control 7 target = clamp(1.2·rail
balance, ±1) at rate 1/15, or `0x113F38` rate = clamp(|Δ|·7, 0.1, 8)/60).

Blend into the cycle: `0x1326C8` plays with blend −1 → state table 0x446990
entry (class 15, kind 5, completion 0, channel 2, blend 0.23 s, no first/end fade).

## 4. Attach / landing semantics `0x1326C8` (verified)

| rider+0x328 style | grounded (a1 = 0) | airborne (a1 ≠ 0, controls 4/5) |
|---|---|---|
| 1, 2 (50-50, backward) | 18 | 68 `L_RS_NORMAL` (leaf 90) |
| 3 (frontside) | 19 | 70 `L_RSFS_NORMAL` (leaf 92) |
| 4 (backside) | 20 | 69 `L_RSBS_NORMAL` (leaf 91) |

Callers: attach `0x106848` (a1 = its airborne flag), controls 3 `0x12E778`, 5
`0x1304E0`, and the control-12 finish. Semantics 68–70 are class 10, kind 0,
completion 6 → semantic 3, blend 0.10 s. Control 7 (`0x131D30`) then re-plays
18/19/20 whenever `0x312AA0` ≠ the style's cycle. Rail spins 49–54 (class 14,
completion 6) map to leaves 72–77: `RSREG_INTO_FS`, `RSREG_INTO_BS`,
`RSFS_INTO_REG`, `RSFAKIE_INTO_FS`, `RSFAKIE_INTO_BS`, `RSBS_INTO_REG`
(rotation table in `RAIL_RECOVERY.md` 3.6).

Backward style 2 has **no mirror or root change of its own**: it uses the same
semantic 18 as style 1; only the balance sign flips through rider+0x320, and
the `0x132060` spin table sets the root quaternion when the style changes.

## 5. Rail uber, control 12 (verified against the original code in 3,000 random episodes)

Records `0x45A038 + 0x24·identity` (identity 0..3 from command bits
`(int8)((cmd << 9) >> 24)`, −1 = none):

| +0x00 | +0x04 | +0x08 | +0x0C | +0x10 | +0x14 | +0x18 | +0x1C | +0x20 |
|---|---|---|---|---|---|---|---|---|
| `RSBS_INTO_FS_GRINDn` (style 4) | `RSFS_INTO_FS_GRINDn` (3) | `RSFAKIE_INTO_FS_GRINDn` (2) | `RSREG_INTO_FS_GRINDn` (1) | `RSFS_GRINDn_CYC` | `RSFS_GRINDn_BAL_L` | `RSFS_GRINDn_LAND` | `RSFS_OUTOF_GRINDn` | tier n |
| 213+8k | 214+8k | 215+8k | 216+8k | 217+8k kind 2 | 218+8k kind 10 | 219+8k kind 1 | 220+8k completion 8 | k+1 |

Hand-off `0x132620` (control 7, identity ≠ −1): if `rider+0x2F0 > 0` and
`rider+0xB2C` bit 1 → `owner+0x394 = identity`, request control 12 (return 1);
otherwise a changed identity only updates trick naming (`0x28B180`/`0x299B70`).

Enter `0x136268`: by style — 4: root (anim+0x30 = `0x4FF130`, anim+0x40 =
(0,0,sin,cos) of +π/4 `0x3F490FDB` about `0x4FF160`), play +0x00; 3: play +0x04
(no root); 2: rider+0x320 = 0, anim+0x18 = 0, root as above, play +0x08; 1: root,
play +0x0C. Then `0x119938(rider+0x790, tier, style)` → `0x10E098(rider,1,·)`,
**rider+0x328 = 3**, phase = 0, `0x116930(rider)`, `0x311E88(anim,1,0.33)` and
`(anim,0,0.33)` (fade the upper/head channels out over `0x3EA8F5C3`).

Update `0x136508` (after `0x116120` recovery):

```
if 0x106848(rider) attached this tick:        (rail re-attach during the trick)
    if phase == 1: play +0x18 LAND
    return
0x114130(upper bits), 0x113F38(turn bits 23..28 ·1/31), 0x113F88(0,0)
phase 0: rider+0x240 = 0, +0x23C = 0x3D6EEEF0 (3.5/60)
         if 0x312AE8(anim,2) (primary channel-2 sequence +0xC0 complete): play +0x10 CYC, seq+0x90 = 1, phase = 1
phase 1: if command identity != held identity or motion mode (0x11FE98) == 0:
             play +0x1C OUTOF, seq+0x90 = 1, phase = 2
         else if current (0x312AA0) != LAND or primary complete:
             motion 4: +0x23C = 3.5/60, +0x240 = rail balance (owner+0xC8, raw, not ×1.2); else target 0
             want = |rider+0x238| < 0.1 (0x3DCCCCCD) ? CYC : BAL_L
             if want != current: play want
phase 2: +0x240 = 0, +0x23C = 3.5/60
         if 0x311AE8 class == 21 and not complete and not 0x1446A0(seq+0xB0, 0) (event bit 0): return
         motion 4: 0x1326C8(control7, 0) [cycle by the new style 3 → 19], request control 7
         else: 0x115640 stance restore, request control 5 (motion 1) or 0
         0x119958(rider+0x790, style) → 0x10E098(rider,1,·)
         rider+0x2F4 < 10: ++, and when it becomes 10: rider+0x2F0 = 60.0 (0x42700000)
```

Completion 8 on the OUTOF clip independently replaces it with semantic 19
(`0x312B18`), so phase 2 usually finishes through "class ≠ 21" one tick later
unless the authored event marker (bit 0) fires first.

Kind 10 (`0x1042E0`, the `RSFS_GRINDn_BAL_L` states): semantic 218 → leaves
a3 = 362 `RSFS_GRIND1_BAL_L`, t0 = 363 `RSFS_GRIND1_BAL_R`; 226 → 371/372; 234 →
380/381; anything else → 389/390 (GRIND4). amount = rider+0x238 (balance
current, **not** negated by rider+0x320). `0x103BE0`: amount < 0 → slot 0 =
t0 (BAL_R) at −amount·duration, otherwise a3 (BAL_L) at amount·duration; no
clock advance (seek-by-magnitude like kind 11, unlike the kind-5 weight blend);
only the sequence fade steps and a finished stop fade removes the sequence.
The same `0x103BE0` serves kind 9 (`0x104660`, half-pipe semantics 40/45):
semantic 45 → 352/353 `HPHS_BAL_THROUGH/REFLECT`, else 346/347 HPTS, amount =
rider+0x244 (control-11 half-pipe balance triplet, written by `0x132A30`,
zeroed by `0x132F98`).

## 6. Clip map (leaf → basic bank index, packed id = index << 8)

RS 63→210, 64→211, 65→212; RSFS 66→213, 67→214, 68→215; RSBS 69→216, 70→217,
71→218; spins 72..77→219..224; landings 90→157 `L_RS_NORMAL`, 91→158
`L_RSBS_NORMAL`, 92→159 `L_RSFS_NORMAL`; HPTS 346→384, 347→385; HPHS 352→390,
353→391; GRIND1 357..365→347,348,349,350,351,352,353,355(LAND),354(OUTOF);
GRIND2 366..374→356..362,364,363; GRIND3 375..383→365..371,373,372; GRIND4
384..392→374..380,382,381 (LAND/OUTOF swap their bank order in every group).
Full table: `originalRailLeafClips` in the header.

## 7. Constants

1/60 `0x3C888889` (gp−0x7F6C/−0x7F70); 3.5/60 `0x3D6EEEF0` (gp−0x72D4..−0x72C4,
−0x72CC); 0.1 `0x3DCCCCCD` (gp−0x72C8); π/4 `0x3F490FDB` (gp−0x72E8/−0x72E4/
−0x72E0); 0.33 `0x3EA8F5C3` (gp−0x72DC); 1/31 `0x3D042108` (gp−0x72D8); 60.0
`0x42700000`; blend −1 `0xBF800000`; 1/30 duration factor `0x3D088889` (gp−0x3224).

## 8. Verified vs inferred

Verified by executing the original code (`tools/test_rail_animation_native.py`):
`0x104238`+`0x103CC8` chain with the real lookup table and bank durations
(30,000 ticks bit-identical: clips, clocks, weights, durations, loop/seek flags,
completion, fades, removal); `0x1326C8` all 8 cases; `0x104660`+`0x103BE0`
(20,000 ticks, semantics 40/45) and `0x1042E0` (20,000 ticks, 218/226/234/242, real BAL leaves); `0x136268` (32 style/identity/switch combinations: semantic,
root, switch clear, style 3, phase, fades, scoring calls); `0x136508` (3,000
random 40-tick episodes: ordered play/control/restore/score calls, phase,
balance target/rate, primary rate, uber counter/meter).

Verified by reading: dispatch tables, state-table rows, record table, `0x312B18`,
`0x132620`, absence of animation plays in `0x13BFA8` and `0x13A968`'s role.

Inferred / outside this recovery: identity → trick name mapping; the exact
prologue callees in control 12 (`0x114130`, `0x113F38`, `0x113F88`, `0x116930`,
`0x119938/0x119958` score values); whether the attach inside control 12
(`0x106848`, style forced to 3 there per `RAIL_RECOVERY.md` 3.3) should be the
native soft attach — the helper takes `attached` as an input; the −0.0 side
weight is harmless but is preserved.

## 9. Integration (for the player/riding owner)

Exporter: replace the 18/19/20 `three_way_maps` with
`['RS_BAL_R_CYC','RS_FWD_CYC','RS_BAL_L_CYC']`, `['RSFS_BAL_R_CYC','RSFS_FWD_CYC','RSFS_BAL_L_CYC']`,
`['RSBS_BAL_R_CYC','RSBS_FWD_CYC','RSBS_BAL_L_CYC']` (negative, centre, positive) and
add 49..54, 68..70 and 213..244 to `state_definitions` (their single variant
leaves are already in `animation_variants` via the `simple` list for kinds 0/1/2;
217/225/233/241 are kind 2 loops, 218/226/234/242 kind 10 (their BAL_L/BAL_R
clips come from `originalRailUberBalanceLeaves`, not from a variant), 213..216 etc. kind 1).

Player `advance()`: keep the three-way path for 18/19/20 (it is the same
`0x103CC8`), feeding `switch320 ? −rider238 : rider238` — or call
`originalRailCycleStep(sequence, originalRailCycleClips(semantic), durations,
railBalance, switch320, timeScale)` directly on the sequence; add kind-9/10
branches calling `originalTwoWayBalanceStep(sequence, id(346), dur, id(347), dur,
halfpipeBalance244, timeScale)` for kind 9 and `originalRailUberBalanceStep(sequence, semantic, durBAL_R, durBAL_L, rider238, timeScale)` for kind 10; completion kinds 6/8 →
`originalRailCompletionReplacement`. Riding: on attach use
`originalRailAttachSemantic(style, airborne)`; for ubers gate with
`originalRailUberAvailable`, apply `originalRailUberEnter` fields in order, then
run `originalRailUberUpdate` per tick and apply its requests immediately.

CMake (header-only; test links the two existing libraries):

```cmake
add_executable(ssx3_rail_animation_tests rail_animation_tests.cpp)
target_compile_options(ssx3_rail_animation_tests PRIVATE -UNDEBUG -frounding-math -ffp-contract=off)
target_link_libraries(ssx3_rail_animation_tests PRIVATE ssx3_animation_sequence ssx3_animation_cycle)
add_test(NAME native_rail_animation COMMAND ssx3_rail_animation_tests)
```
