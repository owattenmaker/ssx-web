# Peak 1 freestyle: R&B slope style (ASS1) and Crow's Nest big air (ABA1)

Both events run on the original freestyle event handler (handler 0, vtable 0x47CFA0), like The Junction super pipe.
What differs between them is the handler's *kind* (GameModeMan+8 = the game mode byte 0x535C12), the course table rows
and the course data. Every rule below comes from `SLUS_207.72` or from EE RAM of PS2 savestates made with ARMSX2.

## Locations and savestates

| Code | Event | Course index / location id | Resident (event) | Sky | Time limit | Computer riders |
|---|---|---|---|---|---|---|
| ASS1 | R&B - Slopestyle | 5 / 5 | ASS1, A_ASS1, TRANSP, ASKY | ASKY | 1:30 (+60 s per checkpoint) | 1 rides (the opponent) |
| ABA1 | Crow's Nest - Big Air | 8 / 8 | ABA1, A_ABA1, TRANSP, ASKY | ASKY | 1:00 | none |

- Registered in `tools/locations.py`. Built with `tools/prepare_location.py ASS1|ABA1`, then `sh web/build-core.sh`.
- Savestates in `local/reference/pcsx2/` (derived copies; the ISO is never touched):
  - `r-and-b-{ready,countdown-anchor,glide}.p2s`
  - `crows-nest-{ready,countdown-anchor,glide}.p2s`
- How they were made (`tools/ps2_navigate.py`, scripts in `local/ps2-capture/nav/scripts/`):
  - From `out-freestyle/freestyle-events.p2s`, the Select Event list (R&B, Crow's Nest, The Junction, Happiness Jam).
  - `rnb-load.json`: Cross, then My Rules, then Cross. `crows-load.json`: DPadDown first.
  - Sample 3000 is the ready overlay (phase 3).
  - `ready-start-360.json`: the anchor is sample 40 (tick 18, countdown 162). The glide is sample 400 for R&B (tick 380, both riders grounded; at 338 they are airborne) and sample 360 for Crow's (tick 338).
- The R&B savestates are **Quick Play**, with Zoe as the player. The opponent that rides is **Moby** (character 0), whose score object is at 0x5D8200 and rider at 0x18B5E20.

### Pipeline fixes made for these courses

- `tools/reference_reset.py`: modes 1 and 3 keep the 200 cm reset clearance. 12F468 selects 1000 cm only for mode 2.
- `tools/export_scripted_instances.py`:
  - The ASS1 `mdl_ASS1_trainboxes_*` slot-2 handlers only break a MeshAnim (builtin13, run by the stage VM). They are classified `none`, not roller.
  - The ABA1 `mdl_ABA1_cadilackc_*` are inert at the countdown: the runtime flags lack 0x60 and every route is skip.
- `web/rail_bridge.cpp`: accepts runtime rail flags 0x10001 (rail only). There are 28 such ASS1 splines.
- `tools/export_freestyle_event.py` writes `web/public/assets/<X>/freestyle-event.json` from the countdown anchor. It carries:
  - the game mode, handler, kind and posted-rider count;
  - the time limit GMM+0x78 and GMM+0x88;
  - the checkpoint list 0x4D33B8;
  - game-info +0x74 (rank mode) and +0x78 (event riders).
- `tools/export_npc_riders.py --location ASS1` writes the opponent's `npc-riders.json`.
- The set pieces, stage scripts, stage world, sections and particles were ported by the set-piece agent (docs/set-pieces.md, docs/stage-scripts.md).

## The rules

### Posting and roster

**Posting 0x239AA0.** Let n = GMM+0x14 posted riders; 0x238E20 sets it to 4 for kind 1, else 5. Then live = GMM+0x10 − n + 1:
- **Slope style:** 2 live slots. Slot 0 is the player and slot 1 is the **opponent who rides**. Slots 2..5 post table columns 0..3 through 0x1453D0; the table's 5th column is never used.
- **Pipe and big air:** slot 0 is the player, and slots 1..5 post columns 0..4.
- Checked against the savestates: R&B 0x536640 = [0, 0, 254500, 124080, 49800, 35360] and Crow's Nest [0, 61080, 43420, 19920, 12120, 5040]. Both are reproduced by 0x1453D0 at level 1 with draws 36, 68, −8, 21, (21).

**Roster 0x239938.**
- GMM+0x18 is the player and +0x1C the peak rival (0x145750).
- The other characters are shuffled into +0x20...
- A Single Event (0x535C11 ≠ 0) puts the **last shuffled** character in +0x1C (0x239A78), and slope style rides it. In a career
  event +0x1C stays the peak rival (0x239A10), but **nobody rides**: 0x238E20's career round-1 path (0x238F7C) sets GMM+0x14 = 5 for
  every kind, so 0x239AA0 has one live slot (the human) and the rival is **posted** in slot 1 with the leading column (Peak 1 Mac / Griff,
  Peak 2 Nate / Zoe, Peak 3 Psymon / Elise). PS2: rnbctm/zoe-a (Mac 254500) and the derived career Style Mile heat 1 (Nate 358740 /
  402400 / 459660; GMM+0x14 = 5, 0x535C04 = 0), docs/career-events.md "The peak rival in career events". The rival never appears in a
  Single Event slope style.
- The number of computer riders spawned is GMM+0x10 − GMM+0x14 (→ 0x535C04): race 5, Single Event slope style 1, career slope style, pipe and big air 0.

### Slope style during the run

- **Checkpoints:** R&B has two, {60, 176456.97} and {60, 75402.55}, at 0x4D33B8.
  - When the human's best remaining distance passes one, 112FB0 → 10E558 → 1194C0 runs. That is human only, and only in mode 1.
  - The 0x29 HUD popup is posted through 117B88 with the value and 2.5 s.
  - The Arcade_Bonus speech is `AE_ARCADE_BONUS` 29.
  - The handler accept 0x2398E8 (kind 1) adds value × 60 ticks to GMM+0x78, and accepts only while the new limit is at least the race ticks. So each checkpoint adds a minute to the clock.
  - The core does this in `OriginalRaceSession::endTick` (engine/race_session.hpp). The browser passes the list with `set_race_bonus` (`web/freestyle-event.js`, `web/main.js startRun`). The HUD clock reads the live limit (`race_time_limit_now`).
- **Place:** 10F998 rank mode 2 (game-info +0x74 = 2) keys each rider on its score +0x198, or 0xFFFF0001 when +0x878 is set.
  - `web/race_world.cpp` has mode 2. `race_world_score` is fed each tick start by `web/ai-racers.js`.
  - The place HUD 21E1B0 shows "1ST/2" by score.
- **HUD:** 1EA930 sets owner+0x3CC to 0x1D31C047 while riding: place, clock, score, progress meter 0x40, and the OPPONENT line 0x08000000; the standings rows (0x10) are off.
  - The OPPONENT line is type-7 case 1ED5C8..1ED78C. The label is the ASCII literal "OPPONENT" (0x46EB60) at (20,65); the value N is at (20,78), both at scale 0.7.
  - N = opponent +0x198 − player +0x198, formatted "+%d" when N ≥ 0.
  - Colour: red 0x4C88C8 when N > 0; pale yellow 0x4C88A8 when −5000 ≤ N ≤ 0; green 0x4C8888 below −5000.
  - Browser code: `web/career-ui.js opponentLine`, `web/ui.js` (progress meter kept for slope style).
- **The opponent** runs the full NPC pipeline in its own core, the same code as the race computer riders.
  - Its score object accumulates like the human's (117C28, 11A228, 1193E0).
  - It gets no checkpoint bonus and no time extension, and it is never timed out: 125228 requires +0x874.
- **Point icons** (builtin27 type 6 → 10E8B8 → 119608, 2000/3000/5000/10000) and **multiplier icons** are stage programs.
  - The multiplier icons are builtin27 type 3 with x2/x3/x5/x10 → 10F1C0 → rider vt+0x78 **10E830**. In the air or on a rail only (11FE98 == 1 or 4), it calls **119448**:
    - +0x130++;
    - if the bank is present and +0x18 < value: HUD slot 4 = 1171A8(type 4, int(value), 0, 0) and +0x18 = value;
    - then 10E098(rider, 4, 0) and the pickup sound 29CED8(audio, 2, rider, value).
  - Ported in `engine/score_object.cpp originalScoreMultiplierPickup` and `web/stage_script_gameplay.inc` case 3.

### Round decision (results 0x239230, per rider at its finish)

- scores[round][slot] = score +0x198, or 0 when DNF.
- The round is decided when every human has finished (12A250):
  - **Heat 1** ranks the player's heat 1 against everyone's heat 1 + heat 2. The top 3 go straight to the final.
  - **Heat 2** ranks combined totals. The top 3 go to the final; otherwise it is back to heat 1.
  - **Final** ranks final-round values; 1st–3rd earn a medal.
- In heats, an opponent still riding keeps 0 (or an older heat's value: a heat-1 retry clears only slot 0).
- In the final it gets **0x122E50**: s1 = score + 1, result = s1 + cvt.w.s(max(rem − 1000, 0) × (s1 / max(orig − rem, 1))), with rem = +0x4D0 and orig = +0x4D8.
- Browser code:
  - `web/career.js`: `postFreestyleScores`, `freestyleResult(score, opponent)`, `Career.opponentEstimate` and `slopeStandings`.
  - `web/ai-race.js`: `opponent()` and `opponentAtFinish` (the opponent latched on the player's finish tick, with its own finish score latched on its finish tick).
  - `web/career-ui.js`: `syncOpponent` and `finish`.

### The finish (rider+0x100 and the boost meter; pv `fsCelebrate`)

- Handlers: 0x238160 maps modes 1-3 (slope style, pipe, big air) to handler 0 in single player (0x535C11 != 2): vtable 0x47CFA0,
  init 0x238E20, results 0x239230. That covers every Single Event and every career heat. In multiplayer (0x535C11 == 2) its jump
  table 0x47C0D0 gives handler 9 instead (vtable 0x47CE68, init 0x239D88, results 0x239EC8).
- **0x239230** at the finish: 238B70 ranks the round's values 0x536640, with the player's run (plus heat 1 in heat 2) and ties in
  slot order. Then:
  - +0x100 = place 0x536730 < 3 (0x239548 `slti 3`; a second ranking with one more slot at 0x23955C decides the same way). The
    top three raise an arm (finish reaction 315), everyone else gets 314.
  - 0x239854 sets the human's boost meter +0x2F8 by place: 1st gp-0x515C = 0.7, 2nd gp-0x5158 = 0.35, else 0. This happens before
    121818's 117C28, so HUD slots 5/6 follow on the same tick.
  - The winner-only clear 0x23A05C belongs to 0x239EC8 (multiplayer).
- Browser:
  - `race_end` (web/race_bridge.cpp) asks the host for the place on the finish tick (`Module.finishHost.place(score)` → main.js →
    career-ui.js `freestyleFinishPlace` → career.js `finishPlace`: the round's posted values, plus the slope-style opponent's run
    once it has finished). It then applies `finish_standing(place)` (web/finish_gameplay.inc).
  - `compare-ps2-capture.mjs --finish-place N` stands in for the host.
- PS2 checks:
  - crows-invert (5th), perpendiculous (6th) and Schizophrenia (6th) clear +0x100 and the meter together on the finish tick. Their
    gates now run score and boost to the end.
  - New capture `celebrate/pipe-2nd`: The Junction with the posted slots poked to 162880 / 1000 / 0 / 0 / 0, so the 2000-point run
    is 2nd. +0x100 stays 1 and the meter becomes 0.35 at 4403. It is gated exact to the end, bones included.
  - The real Single Event flow (tools/modeshot.mjs --bones, the page's posting set to the same values) has bones equal to the PS2 at
    4607 / 4622 / 4653 in Chrome and WebKit. With the page's own posting the run is 6th, and the bones are 38-65 cm off.

### The seed decides the posted scores

A page session boots its presentation generator from the local clock, as 0x31AE94 does from the RTC. Its posted scores therefore
differ from any one PS2 capture, just as they do between two PS2 sessions booted at different times. The reference captures were
booted with seed 0x182200. `?presentationSeed=0x182200` gives the page the same first roster seed (0xB57109A9), and the page's
Single Event flow then posts exactly what the Peak 2/3 anchors hold. For example, Launch Time is 91620 / 51700 / 29880 / 24240 /
14140 (the HUD sweep's PS2 frame) and Schizophrenia is 223960 / … (test-slopestyle-bigair).

### Big air

- Nothing in the executable separates big air from the pipe except:
  - the table row (60 s; 60000/42000/20000/12000/5000 in heats, 70000.. in the final);
  - GMM+0x94, which has no reader;
  - the 200 cm reset clearance;
  - the pause-menu case.
- There is no jump counting or best-jump rule: a run is scored like the pipe, with trick points plus the 2000/3000/5000 point icons.
- HUD flags are 0x1530C016: the standings rows, the clock and the score.

## PS2 captures and gates (`web/test-ps2-captures.mjs`)

| Gate | Capture | Result |
|---|---|---|
| `peak1/crows-event-tuck` | Crow's Nest from the countdown anchor through GO, the kickers, a reset plane and an in-air finish to the landing (first run of `setpieces-aba1/full`) | physics, all 29 bones, score object and HUD bank bit-exact to the end (2446 ticks), incl. the 2000-point icon 1103 and the post-finish landing 2073: **12A250** (every human finished) makes 11A228 return at once, so that landing commits nothing and leaves the combo clock closed (`web/score_gameplay.inc` `commitBlocked`; computer-rider cores get the humans' state from `web/ai-racers.js` via `set_humans_finished`). The same gate made `pipe-finish`'s score exact to its end. |
| `peak1/rnb-event-tuck` | R&B from the anchor, a tuck/weave run through checkpoint 1 to the finish (`setpieces-ass1/full` first run, 11390 ticks; Moby rides, `--isolate`) | physics exact through 6671, score object/HUD through 6675: trick and rail points, the x5 multiplier icon 845 (10E830/119448), the trainbox crash 981, checkpoint 1 at 3302 (0x29 '+60' popup, +60 s); bones through 1560 (bones 24/25 from 1561, open); 6672 a control-3 soft collision lands 0.03 cm off (open) |

- PS2 results screens (`setpieces-ass1/restart/full.tick212.png`, `setpieces-aba1/restart/full.tick88.png`) show the same rows as the browser: R&B ranks the opponent Moby by the score of his own run (109865) among the four posted riders.
- Screenshots (Chrome, Safari, PS2 side by side): `scratchpad/peak1/{ass1,aba1}-{start,midrun,results}.png`, `ass1-card.png`; neutral runs at the same event tick match the PS2 frames (clock, speed, camera, HUD).
- Browser: `web/test-slopestyle-bigair.mjs` (npm test) checks the configuration, the posting, the opponent estimate and the results rules. `web/test-locations.mjs` loads both courses.

## Gaps

- The progress meter (20EDA0) is the race HUD's static approximation: no opponent marker or red fill; its percent can read 1 lower than the PS2 on R&B (the meter's own path math is not ported).
- Results background: the PS2 replays the run behind the results; the browser keeps the finish camera (as for every event).

- **R&B opponent lineups (2026-09-25):** `ASS1/lineups.json` is built from all 30 derived countdowns in `characters/lineups-ASS1` plus the anchor (`tools/export_lineups.py export|build --course ASS1`). It covers the ten base humans, ten cheat humans and all nine possible Quick Play opponents (0x239938: anyone but the human and the peak rival, so Griff rides for e.g. `viggo-875b32bb` and `allegra-fd7b3a05`), and a grid spot for every human. The first build (2026-09-23 19:43) ran while the states were still being made and saw only 8 of them, so it had no Griff: a Quick Play R&B that drew Griff threw "No computer-rider data for griff" in `ai-race.js prepareFreestyle`, and 27 of 31 humans had no grid spot of their own. `CAREER/freestyle-rosters.json` (`tools/export_freestyle_rosters.py`) was re-exported the same way (36 states). A career slope-style heat spawns no computer rider (0x535C04 = 0 in `rnbctm/zoe-a`).
- **Raven B section re-entry (fixed 2026-09-25).** The R&B held-out captures (`local/ps2-capture/runs/lineups-ASS1`) lost the shared RNG at 221: the PS2 draws once at 220, 260 and 300 from 0x35955C (the Spline ctor 0x359460, builtin19) in the section pass 0x101B60. `mdl_ASS1_ravensplineanimb_1000` (65035, slot-1 program 66) is a looping Spline resident from the load. Its entity is relocated in the octree with its bounds every entity pass (0x3568B0, in every entity vtable at +0x194); a section leave destroys it (0x34FD90), and the entity dtor 0x3553C0 -> 0x3567E0 puts the instance back into the octree cell of its own bounds (+0x60/+0x6C). That authored cell is inside the box again at the next scan, so program 66 runs: a new Spline (start distance 50, -45 km/h, loop) and one draw. The PS2 activation list of `setpieces-ass1/full` shows the cycle: listed 120-180 (the flying raven), left 200, 220 in, 240 out, 260 in, 280 out, 300 in and listed from then on.
  - Browser: `web/section_gameplay.inc section_scan` relocates a resident loop that has a slot-1 seed of its own resource (`loop_relaunch_seed`); `section_stop_piece` (`web/set_piece_gameplay.inc`) removes it, drops its LiveComp (`attached_destroy`) and puts the section cell back to the authored bounds, for launched spline pieces too; the next enter relaunches it from the seed. `tools/export_location_set_pieces.py` now writes that slot-1 seed for residents (drawn from the restored flags `high | high >> 16 | 2`); only `set_piece_seed_ASS1.hpp` was regenerated (one seed added), so the blimps and the CRA3 / CHP2 / EHP3 resident birds keep their static cells until their headers are regenerated and checked against their captures.
  - Result: four of the five captures (the Zoe anchor, Mac, Psymon, Stretch) are exact for all 900 ticks (opponent, RNG, ranks).
- **The held-out Griff capture (fixed 2026-09-27).** `ass1-griff-87e9ff58` failed `test-slopestyle-bigair` (the "known baseline failure" of npm test). Two independent causes, plus a third found on Psymon on the way. All five captures are now exact for all 899/900 ticks: human physics, human score object and HUD bank, the opponent, the RNG and the ranks.
  1. **The human's landing depths used Zoe's body scale.**
     - PS2: the touchdown's ground entry 13C7A8 sets the motion owner +4/+8 (the smoothed depths that 13D818's normal response 13C878 reads) to the rider's own scale (geometry+0x140) x the material depths.
     - Griff (0.7) on R&B patch 84491 (surface 3: 29.574 / 59.990) gives 20.702 / 41.993. That's the capture's `owner_00_40` from record 575 on.
     - Port: `resolve_touchdown` / `leave_crash_motion` (web/core.cpp) passed `landingProfile.bodyScale`. For the human that was still the compiled Zoe seed, 0.85, giving 25.138 / 50.991. The next ground ticks then approached 20.70 / 41.99 at 1.667 cm/tick, which changed the normal acceleration. Record 576's velocity was off by 5.7 cm/s, almost entirely along the normal.
     - `browser_apply_ground_attributes` (web/attribute_bridge.cpp) put `browserHumanBodyScale` on `physicsProfile` / `physicsMaterials` only. It now sets `landingProfile.bodyScale` too; `npc_seed_rider` already did for computer riders. The crash sliding contact and detached body (`web/crash_runtime.hpp`) read the same field. A later Zoe in the same rider context gets the seed scales back.
     - This was also Psymon's 589 and Stretch's 610. Zoe and Mac have Zoe's scale.
  2. **The opponent's held jump on a rail steered with the wrong command field.**
     - PS2: 12E9B8 (control 2) decodes PrewindTurn (word1 bits 0..5) into f22 and RailBalance (bits 6..11) into f21, each clamped to +-0.5. On a rail (rider+0x328 set) it calls the rail steer target 113F38 with f21 (12EC1C / 12EDF0); off the rail it calls 113E80 with f22.
     - Port: `rail_gameplay.inc`'s held-jump branch passed `steering`, i.e. PrewindTurn.
     - The human's INPUT.MAP packs the same stick into both fields, so only computer riders differed. Allegra's word1 0xD at tick 700 gave a steer target of 13/31 instead of 0, which slid her 0.29 cm sideways (`r.right x slide x dt`) at 701.
     - Now `rail_held_balance()` (web/input_bridge.inc) uses this tick's control-2 RailBalance, with the host steering as a fallback when no words were decoded (a `ride_command` from the host).
  3. **Goofy riders' air spin sign.**
     - PS2: 133308 at 134334 negates the scored spin before 119898 stores it in score+0x34 when rider+0x324 is set (Psymon: +0x320 = +0x324 = 1). An idle air control then leaves -0.0 there.
     - Port: `animation_bridge.cpp` passed `air.scoredSpin` unnegated. It now negates it for +0x324 (`state320Equals324 ? reverseStance : !reverseStance`).
     - Psymon's human score went from its first difference at tick 303 (481/899 exact) to 899/899.
  - **Tools:** `compare-ai-capture.mjs` (per-tick `ground_state_dump` vs the record's `rider_100_b40`, the human owner window `owner_00_40`, the computer rider's `actor_000_b40` +0x22C steer triplet) and the recompiled 12E9B8 / 133308.
- If the opponent crosses the line after the player but before the race stops, 239230 runs again for slot 1. That quirk is not reproduced.

- **Career slope-style opponent (settled 2026-09-30, career-rival agent):** there is none on any peak. The brief that the career heat rides the
  peak rival (Nate on Style Mile) does not match the code: see "Posting and roster" above. The port already posts the rival and races the
  human alone (web/career.js postFreestyleScores, web/ai-race.js prepareFreestyle); web/test-career-rival.mjs replays the derived Peak 2
  heat's roster and all three rounds' posting exactly. Kick Doubt (ESS3) follows the same code with Psymon / Elise (no Peak 3 career state made).
