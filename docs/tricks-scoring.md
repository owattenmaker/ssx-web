# Trick scoring, combos and the trick HUD

The browser now runs the original score object and draws the original trick HUD from it. Both are
source ports of the PS2 routines named below. PS2 captures check them bit for bit.

## Score object

Each rider owns a score object at `*(rider+0x790)`, 0x1CC bytes long. The human rider's object is at
0x14701A0 +0x790. Its HUD message bank is at `*(score+0x1B0)`.

The port is in `engine/score_object.hpp/.cpp`. `OriginalScoreObject` holds the raw words, so the layout
is the PS2 layout. Typed views bridge it to the older helpers (`scoring`, `trickIdentity`,
`trickHistory`).

### Object fields

| Offset | Field |
|---|---|
| +00 / +04 | stance, alternate table |
| +08 / +0C / +10 | multi-trick flag, style, flag |
| +14 / +18 | pending points (acc14), multiplier |
| +1C | inverted accumulator |
| +24 / +2C / +30 | rail distance, t2C, air time |
| +34 / +38 | spin, flip |
| +3C..+48 | hold increment, hold seconds, total hold, longest hold |
| +4C..+5C | normal / tweak / Uber / super-Uber / active-Uber counts |
| +60 | grabs[3] |
| +84 | bonus |
| +88..+98 | threshold indices |
| +9C / +A0 / +A4 | combo count, combo points, combo clock |
| +A8..+F7, +F8 | repeat history[10], cursor |
| +FC..+1A8 | run statistics: +110 trick count, +114 Ubers, +118 super-Ubers, +134 combo tricks, +16C grades, +184/+188 best combo, +18C best trick, +198 total score, +1A0/+1A4 lost points, crash counters, speed statistics |
| +1AC / +1B0 / +1B8 | rider, bank, renderer bitmask |
| +1C4 | multiplier (1 after construction) |

### Message bank

The bank has 44 slots of 0x9C bytes:

| Offset | Field |
|---|---|
| +0 | type (0x34 = free) |
| +4 | maximum. -1 marks a persistent value slot. |
| +8 | value. It is the elapsed time; 116FB8 adds 1/60 each tick. Value slots store -input. |
| +C | arg |
| +10 | field10 |
| +14 | points |
| +18 | text (a trick name or "%d") |

A type below 0x22 owns slot [type]. 117A58 allocates a slot for type 0x23 and above.

### Routines ported

Each routine is a literal port with EE float semantics:
- `originalScalarAdd` for the add/sub guard bit
- chop multiply
- nearest-rounded divide
- truncating `cvt.w.s`
- vf0 = (0,0,0,1) in the VU sums

| Address | Role | Browser call site (`web/score_gameplay.inc`) |
|---|---|---|
| 117C28 | per-tick update. The combo clock A4 counts down on the ground; 117718 pays out when it runs out. Also air/hold/rail/t2C accrual, 119210 thresholds, statistics, and the 117FE0 HUD update: spin dial 0xD, direction slots 0x10-0x17, landing messages STALLED!/OFF-AXIS!/INVERTED!/LATE SPIN!/LATE FLIP! (+0x1C inverted points), boost widgets, total slot 7, combo slot 3 (+70/tick display), slots 2/0x19/4 | `score_tick()` once per tick from `tick_rail_score` |
| 11A228 | trick commit: identity 11A8C8, repeat history 1190F0 (repeat division), grade, 117638 combo add (mult = clamp((count+10)*0.05, 0.5, 2)), popups 0x23 / 0x24 (repeat) / 0x18, trick name slot 0 (118FF8), career award 119EF8 | `score_commit_raw()` |
| 119D40 via 10E910 | landing boundary: stance = (320^324) != 0, alternate = normal.z < 0.7, flag = +0x330. Uber tier progression from +0x114; 10E098 meter award; Uber landing reaction request 2 | `score_landing()` from `finish_landing` |
| 119E38 via 114298 | takeoff boundary. The alternate flag is the ramp branch at 114660 (`OriginalJumpMotion::rampTakeoff`) | `browser_score_takeoff(ramp)` from core.cpp's three takeoffs |
| 119C98 | new D-pad trick in the air (air control 133734 phase 3) | `score_trick_start()` when `frame.trickStarted` |
| 119B08 | bail: lost-point popups, crash counters (+0x12C attacked + popup 0x2D, else +0x124; crash-motion.md "Attacked bails"), reset, -0.25 meter penalty when points were pending (through 10E098) | `score_bail(attacked)` from `recordCrash` |
| 119BB0 via 10F280 | quick recovery: +0.1 meter, popup 0x21 RECOVERED! | `score_recovery()` from `crash.host.refund` |
| 119368 | reset placement: lost points (11A7A8); meter * -0.7 or -0.1 | `score_reset()` |
| 119608 | pickup | engine only |
| 117248 + 117540 | construction and run reset | `score_init()` |
| 1193E0 via 125108 | finish: pays out the open combo so it reaches +0x198 | `browser_score_finish()` from `race_end` |

The meter/boost side goes through `originalBoostAward` (10EBA4), called from `changeBoostMeter`.

### QA exports

- `score_object_dump`: the object followed by 44x6 slot words, in the capture layout
- `score_object_seed`
- `score_hud_slots`: 44x6 floats + [total, commits, combo payouts, last points]
- `score_hud_text`
- `boost_state_seed`

## Trick HUD

`web/trick-hud.js` ports the single-player HUD slot cases of 0x1E9A30 (jump table 0x46EC70) for the
trick-scoring types:

| Type | Draws |
|---|---|
| 0 | trick name (FEFONT, wrapped) |
| 1 | score with grade stars |
| 2 | bonus and its box |
| 3 | combo: "NX" at 2 x 2.4 scale, stopwatch and its quarter hand, "+points", "Combo" |
| 4 | multiplier |
| 7 | total, which pulses while slot 0x18 flashes |
| 0xB / 0x21 | recover state |
| 0xE | landing messages |
| 0x19 | nothing under the race flags |
| 0x1C-0x20 | BIG AIR, LONG RAIL, LONG PRESS, LONG PLANT, LONG GRAB labels |
| 0x23 / 0x24 | point popups that fly to the total, with green pulse or red for a repeat, and grade stars |
| 0x25 / 0x27 / 0x28 | lost points, with digits scattered by 1F14B0 |
| 0x26 | "NX Combo" payout |

The helpers are:
- 21E750 align
- 391FB0 measure
- 1F1B30 / 1F1E28 text and lerped text
- 1F2AA0 wrapped text
- 21F338 stars
- the sine-table pulse at 0x504FB8

`web/public/assets/UI/trick-hud.json` holds the layout descriptors 0x4768B0, sprite records, colours and
strings. `TrickHudRenderer` draws the list on the UI canvas. Within a layer, glyphs go above sprites: in
the PS2 frame the popup digits cover the grade star.

Wiring: `ui.js` draws `s.trickSlots` when present and falls back to the old text otherwise. `main.js`
reads `_score_hud_slots` / `_score_hud_text` each frame and passes the total (+0x198) as `score`. The
career freestyle result therefore uses the original total, including the payout at finish.

## Verification

| Check | Result |
|---|---|
| `tools/test_score_object_native.py` (`tests/score_object_reference.cpp`) | 40,000 randomized cases of 117C28, 11A228, 117718, 117638, 119368, 119608, 119B08, 119C98 run against the recompiled originals. The object and all 44 slot headers match byte for byte. |
| PS2 captures (`local/ps2-capture/scripts/score-*.json`) | `tools/ps2_capture.py` now records the score object (`score_000_1d0`) and the bank (`hud_slots_44x6`) every tick. `--poke ADDR:VALUE` (`POKE=` in capture.sh) sets boost state; for example `0x1470498:1.0 0x1470490:20.0 0x1470494:1` gives full boost at tier 1. `compare-ps2-capture.mjs` seeds the object from record 0 and compares every word, except the pointers and renderer bits, plus every live slot header. `test-ps2-captures.mjs` gates 8 cases (score-air-tricks, -combo, -repeat, -grabs, -rail, -uber, -uber6, -rail-uber). All are exact through the end of their compared ticks. |
| `web/test-trick-hud.mjs` (`tools/probe_trick_hud.py` runs the original case code on the snapshot) | 277/277 slot states give the same draw list: position within 0.0001 px, and matching scale, ARGB, text, font, shadow, sprite UV and size. |
| PS2 screenshots, score-air-tricks-snap2 | Frames labelled 484 and 568 (the frame is 2 ticks before its label) were overlaid with the web HUD drawn from the captured slots. "COMBO +420", the 1000 popup and star, "2X COMBO +619", 810, total 1810, "FS 360" and "BS 180" coincide pixel for pixel. |

## Gaps

- Race checkpoint/time popups 0x29-0x2B and the career popups (0x2C-0x33) are not drawn.
- The recover bar (0x21D9A0) is an approximation.
- The per-player pulse (+0x70 on the owner, used by the type 1 score) has no known source yet, so it
  stays idle.
- Career "great trick" reaction kind 1 (149778) is not ported.
- Event HUD flags for trick-mode events are assumed to equal the race flags (0x1530C047).
- The finish payout (1193E0) is not covered by a capture.
- Superpipe (BHP1): the score object and bank are exact on every pipe capture through its physics divergence
  (pipe-neutral/-air/-air-grabs/-handplant/-tricks end to end, pipe-uber through 1143 with the Uber crash and the 116120 reason
  1 "Wrong Way!" message 0x33 (11A088), the full event runs through 2076/2861; see
  docs/locations.md). Not ported: stage-script point pickups (`mdl_BHP1_pointa_*`, builtin27 type 6 -> 10F1C0 ->
  10E8B8 -> 119608, +2000 in pipe-finish 2862) and their HUD popup.
- Event mode: `compare-ps2-capture.mjs --event` now seeds the object and the bank from the first record as well (the
  bank keeps stale front-end words in field10 of slots 5/6/7/0x19 that the original never clears).
- The rail takeoff alternate flag still uses rail_gameplay.inc's takeoff normal.

## Finish-line trick bonus 0x1194C0 (2026-09-23)

The race bonus is paid as the rider's best remaining distance crosses the bonus checkpoints.

- **Crossing detection.** The progress step (`engine/race_event.cpp`, `originalRaceBonusCrossings`, 0x113014..0x1130A8) walks the checkpoint list 0x4D33B8: six `{int value, float distance}` entries, ended by a 0 value. A checkpoint is crossed when the best remaining distance (+0x4D4) passes its distance.
- **Award rules** (`engine/race_session.hpp` endTick):
  - The global flag bit 9 (*0x5308D0) disables the bonus, and so does any game mode (0x535C12) other than 1.
  - Event handlers (*(G+0xC0)+4) 1, 2, 5 and 6 pay the bonus.
  - Handler 0 (freestyle, GMM+8) pays it too. With freestyle kind 1 it also adds value x 60 ticks to the time limit, and pays only while the race is still inside the limit.
  - Any other handler throws; it has not been seen on the PS2.
- **Payment.** Each award calls `browser_race_bonus` (web/score_gameplay.inc):
  - `originalHudPostPoints(score, 0x29, value, 0, 2.5)` through 10E098, as 1194C0 does.
  - The audio event `AE_ARCADE_BONUS` (29, a = value), used for the Arcade_Bonus speech (2A3CE8).
- **Configuration.** The host passes the course's table, game mode, handler, freestyle kind and flags with `set_race_bonus(words, mode, handler, freestyleKind, flags)`. The comparer derives these from the capture pokes.
- **Gates:** tech-bonus-tricks, tech-bonus-racemode and tech-bonus-disabled (poked table, mode and flag). The score object, HUD slots and physics match to the end.

## Speedrun techniques: PS2 captures (2026-09-23)

Every technique below is a PS2 capture replayed through the production pad path. All are gated end to end in `web/test-ps2-captures.mjs`: physics, 29 bones including hair, and score. Scripts are in `local/ps2-capture/scripts/tech-*.json`; all start from the Snow Jam glide baseline.

| Technique | Captures | What it pins down |
|---|---|---|
| Spin timing | tech-spin-k1/3/6/10/15/20/25/30, -right-k8, -long, -max-k6, -max-long | spin pressed k ticks after takeoff; 117FE0 dial; 12F620 rates; maxed stats (rider+0xB34 = 11 poke 0x1470CD4) |
| Prewind buffer / re-prewind | tech-prewind-full/late/flip, tech-reprewind(-late, -spinland) | 12E9B8 branches (class 10, semantic 21, 114CC0 reverse turn when +0x328 == 0 and +0x2DC == 0); 131620 entry tick with Cross held through the touchdown |
| Spinboosts | tech-sb-opp/same/half/540/early/right/partial/max/reprewind | D-pad spin with the opposite or same stick on groomed and thick snow |
| Speed caps | tech-speedcap-groomed/tuck/switch/level1 | speed-limit table: thick [0..7], groomed [8..15], ice [16..23], +24 with the max stat; the switch -2% |
| Landers | tech-land-{short,mid,long}{,-cross,-crosslong,-tail,-nose} | airtime under 40 and about 60; Cross held or tail/nose press through the landing |
| Frame cancelling | tech-fc-grab-early, -rel4/8/12/16/20/26, -tap2/4/6, -hold, tech-fc-tweak-rel8, tech-fc-uber-restart/tap/late | grab lifecycle; landing crash rule for classes 18..21 |
| Rail glitch | tech-railglitch-2/4/8/nose/tail/jump/air | rail attach and leave k ticks apart |
| Strong stance | tech-strong-jump/right/passive/collide | |
| Free-fall slide | tech-freefall-tail/tail-back/spin | |
| Trip turn | tech-trip-left/right/stop | |
| Select warp | tech-select-ground/air/crash/rail/twice | ResetPath 0x1000 -> 116120; the rail exit runs 132048/13C5A0 |
| Stance dancing out of bounds | tech-oob-neutral/dance/hops | 1210B0 reason 2 (obstacle-collision.md) |

The animator's pending next rate +0x1C (setter 0x3158E0) matters for spin timing, prewinds and finish reactions:
- 311F00 gives every new sequence this rate.
- 311A50 (placement) and the air selector reset it after their own play.
- 12C678 sets 0.75 only around the finish reaction (115B58) and sets it back to 1.

## Uber tricks, Super Uber and Monster Tricks: PS2 parity (2026-09-23)

All captures below are on the Junction super pipe (BHP1), Zoe, replayed through the production pad path and gated in
`web/test-ps2-captures.mjs` end to end: physics, 29 bones, score object + 44 HUD slots, the boost words (+0x2F8 meter,
+0x2F4 Uber tier, +0x2F0 Tricky/Super time; +0x2FC amount is reported only) and, new, the **sound / speech dispatch**.

### Mechanics (recovered, all matching the port)

- **Tricky:** a full meter (10E098, 1.0) starts Tricky: +0x2F0 = max(+0x2F0, 20 s), tier 0 -> 1. Uber tricks = hold a grab
  combination and add Square while +0x2F0 > 0. Ubers of grab slot c come live from table 0x45AEB8 (0x1352A8 ->
  0x150198/0x1502C8/0x1503F8 -> 0x14FEA8): row = byte `0x530EC0 + bank*0x13EC + char*0x1FE + c*6 + (tier >= 5)`. Byte +0 is
  the hidden base Uber (tier < 5), byte +1 the lodge Ubertrick Setup selection (tier >= 5): the browser's
  `uberChoiceRows` (fe-screens.js) applies the selection to grab-profile set 1 only, which is the same rule.
- **Tier progression** (10E910 -> 10E9B4): each committed Uber adds one tier, capped at 10 (the nine SUPER UBER letters,
  pending-letter fraction count/9 in 118AF8). Tier 10 = **Super Uber**: +0x2F0 = 60 s, meter locked at 1, Ubers also count
  in +0x58 / +0x118 (super Ubers). When the 60 s run out (1200D0) the tier drops to 5 with 20 s of Tricky, and 10E028(6)
  plays the upper reaction 318.
- **Chaining (speedrun):** several Ubers in one air all commit at the landing (+0x54 = count) and advance the tier by the
  count (uber-multi: two per air, 1 -> 3 -> 5). A Square tap between them is allowed (frame cancel).
- **Monster Tricks** (0x11B1A8, table 0x43D608, `web/monster-tricks.js`): see the table below. Detection runs at every
  commit (11A228, landing) and at every new D-pad trick in the air (11A168): the identity's direction, spin, flip kind,
  flip count, flip spin, grab and late grab must equal a row. The identity is then replaced by {0, id << 27} (the trick
  name becomes the monster name), bonus x 0.0001 is added to the pending points (acc14), HUD slot 0x32 gets the bonus
  (1.5 s) and 0x29B7E0 plays Arcade_Uber variant 8 ("Monster Trick"). **Nothing checks unlocks** in game: all 24 score
  whenever performed. The repeat history then holds the monster identity, so the same monster again is a repeat
  (points halved: monster-repeat 7020 vs 14040; popup 0x24, the bonus popup and speech still fire). A late flip after the
  Uber does not split the trick: it joins the same identity (FS Rodeo G-Money), so there is no monster and no repeat
  (monster-smurphy). Slot 0x32 is drawn only when the HUD flags lack bit 2 (free ride / backcountry); the race and freestyle
  flags (0x1530C047 / 0x1530C016) have it, so no monster popup is drawn in events, as in the browser.
- **Monster unlocks** (front-end list only, 0x1F5DA0): list position p (order 0x441B40) is unlocked when p % 3 <
  medal[p / 3]; medal[s] = profile byte R+0xBB8+s, written only by 0x155390 from the post-event personal bests (0x155420):
  the number of thresholds 0x440ED0 reached for StayOnRail 2500/12000/30000, HoldHandplant 3/5/8, StayInAir 5/8/9,
  KOPeopleRace 3/6/10, DoUberGrind 5/8/10, DoSupUber 5/8/10, GetPoints (score/100) 1500/5000/10000, DoXCombo 10/20/100.
  `web/monster-tricks.js` holds the rule (`unlockedMonsters`); the career save does not track these personal bests yet.
- **Stall bonus** (117FE0 0x1185F0..0x1188CC): in air control 5, the D-pad/stick opposite the rotation (> 157.5 degrees)
  shows STALLED! (slot 0xE) and adds b/1200 per tick to +0x1C (b = 1 spin axis, 5 flip axis, 2 diagonals), paid at the
  landing through 117908: trick-stall Back Flip 4130 (60-tick stall) vs 2740 (30 ticks).
- **Landing reactions** (10E910 0x10EA28..0x10EAA4, human riders): a combo paid at the landing -> 10E028(3), else committed
  Ubers -> (2), else a "great trick" -> (1): 149778 = landing total gain >= 0x45A2B0[clamp(level - 1, 0, 10)]
  {3000, 5000, 7000, 9000, 12000, 15000, 20000, 25000, 30000, 40000, 50000}, level = 148950 = max(1, sum(attribute byte /
  5) / 7). 115B58 plays 315 for kinds 1..3 (kind 1/2 with the 29FF80 trick speech) and, for kinds 5/6, 318 only in control 0
  with the animator's current (all-bone) mask.

| id | Monster trick | Trick | Bonus |
|---|---|---|---|
| 1 | Da Housecat | FS 540 G-Money | 10000 |
| 2 | Aphrodite | Triple Back Flip Superman | 10000 |
| 3 | Thrice | FS Rodeo 720 Smithereen | 10000 |
| 4 | Swollen Member | BS 720 Torpedo | 10000 |
| 5 | Yellowcard | BS 900 Mattrickulater | 10000 |
| 6 | Alpine Star | FS Double Back Flip 540 Karolicker | 20000 |
| 7 | MxPx | Back Flip Nosegrab To Late Judo | 10000 |
| 8 | Ultimate dnL BOOST | FS Rodeo 720 dnL BOOST | 20000 |
| 9 | Black Eyed Pea | BS 900 jib O | 10000 |
| 10 | Deepsky | FS 900 Indian To Late Method | 10000 |
| 11 | Basement Jaxx | BS Misty 900 Hand in Hand | 20000 |
| 12 | Fischerspooner | BS Back Flip 360 Kort Martial To Late Stalefish | 20000 |
| 13 | Chemical Brother | Double Back Flip Bar Hop To Late Mute | 20000 |
| 14 | X-Executioner | FS Triple Back Flip 180 SSXorcist | 30000 |
| 15 | Ultimate dnL FlipIt | FS Double Front Flip 360 dnL FlipIt To Late Indy | 20000 |
| 16 | Juana's Addicion | BS 720 Indy To Late Katana | 20000 |
| 17 | Audio Bully | FS 1080 Morgan Grinder | 10000 |
| 18 | Finger 11 | BS Back Flip 360 Slinger | 20000 |
| 19 | N.E.R.D. Fly or Die | FS Misty 720 Svelton To Late Nosegrab | 30000 |
| 20 | Overseer | Triple Back Flip Nosegrab To Late Lukeloo | 30000 |
| 21 | Stoneage | BS Double Back Flip 180 Madonna | 20000 |
| 22 | Autopilot Off | Double Back Flip Vacation To Late NIFTY Shifty | 20000 |
| 23 | The Automator | BS 360 NIFTY Shifty To Late Footloose | 20000 |
| 24 | Placebo | BS 540 Indy To Late Trickitello | 20000 |

(Executable values; Yellowcard is 10000. Unlock medals: positions 0-2 StayOnRail = Da Housecat, Thrice, Swollen Member;
3-5 HoldHandplant = Yellowcard, Alpine Star, MxPx; 6-8 StayInAir = Aphrodite, Ultimate dnL BOOST, Deepsky; 9-11 KOPeopleRace
= Fischerspooner, Basement Jaxx, N.E.R.D.; 12-14 DoUberGrind = Finger 11, Autopilot Off, The Automator; 15-17 DoSupUber =
Black Eyed Pea, Placebo, Overseer; 18-20 GetPoints = Chemical Brother, Ultimate dnL FlipIt, X-Executioner; 21-23 DoXCombo =
Audio Bully, Stoneage, Juana's Addicion.) Zoe's default tier >= 5 Ubers are Indian (L1), Bar Hop (L2), G-Money (R1),
Mattrickulater (R2), score id 75 (L1+L2) and Slinger (R1+R2, slot 9), so Da Housecat, Yellowcard, Deepsky, Chemical Brother
and Finger 11 are reachable for her; in the pipe air (about 170 ticks with max stats) only Da Housecat's FS 540 was reproducible.

### Sound / speech call log (new tooling)

- `tools/ps2_audio_log.py` (`ps2_capture.py build --audio-log`, `local/ps2-capture/uber-capture.sh NAME FRAMES POKES...`):
  hooks at 2906B8 (every SFX voice start: bank slot, sound index), 2B1458 (speech requests), the arcade speech categories
  2A3DE0 / 2A3C00 / 2A3CE8 / 2A3EB8 / 2A3B18 (variant mask), 29B0E0 / 29B3C0 (pending-Uber sound start / stop with the live
  voice +0x5FDC), 299638 / 2997B8 (Tricky), 29B430 (Uber commit), 29B7E0 (monster speech). `run` streams the ring over PINE
  into RUN.audio.json.
- `web/uber-audio-compare.mjs` (compare-ps2-capture.mjs `TICK_HOOK=`): runs web/sfx-game.js against a recording voice layer
  exactly as game-audio.js does and compares per game tick every bank-0 voice start, the arcade speech calls and the
  pending-Uber stop. Test cases with `audio: true` gate it.

### Captures (all exact end to end)

| Capture | Pokes | What it covers |
|---|---|---|
| uber-chain | meter 1, Tricky 20 s, tier 1 | four Ubers in consecutive airs, tier 1 -> 5, Arcade_Uber 1 when the run count reaches 4, combo payouts; audio 41/41 |
| uber-multi | + max stats (rider+0xB34 = 11, 0x1454DD4) | two Ubers per air twice (the second pair after a Square tap), tier 1 -> 3 -> 5; audio 29/29 |
| uber-super | tier 9 | Uber -> tier 10 Super Uber, 29B738 Arcade_Uber 2 one tick after the landing, three Super Ubers (+0x58); score through 1746 (a pointa pickup, world agent) |
| uber-super-expire | tier 10, 2 s | Super Uber expiry (tier 5 + 20 s), reaction 318, Tricky countdown ticks 0x68, a second-table Uber |
| uber-bail | meter 1, tier 1 | Uber crash (bail -0.25, Tricky end 0x69, pending-Uber stop), crash / slide sounds |
| monster-housecat | tier 5, max stats | Da Housecat +10000, HUD 0x32, Arcade_Uber 8; then BS 540 G-Money (no monster) |
| monster-repeat | + history = Da Housecat (0x5DCAAC) | the monster as a repeat (7020) |
| monster-smurphy | same | Da Housecat + late back flip = FS Rodeo G-Money |
| trick-stall | tier 1, max stats | STALLED! on a back flip, +0x1C payout, the great-trick reaction 315 |

`tests/score_object_reference.cpp` now also seeds monster-trick states (2363 per 40,000 cases, 2088 awards) and checks the
29B7E0 call count against the port.

### Fixed in this pass

- Arcade_Uber 8 (monster speech) fired on every committed trick; 11A168/11A228 call 29B7E0 only for a monster (>0 bonus).
- AE_COMBO passed the run Uber count after this commit; 29B430 gets it before (Arcade_Uber 1 at the 4th Uber, not the 3rd).
- sfx-game.js: Super Uber speech 29B738 was missing; AE_ARCADE_BONUS (29) was not dispatched; the meter fill ticks 0x65
  never played (slot 6 is a value slot, maximum -1); the points tick is only evaluated while points are pending; the
  air-phase crash-loop path also plays the crash sound (12D4E8 -> 296310); 29AB40, 29B738 and 28C8C8 run in the per-frame
  audio update before the frame's game tick, so they read the previous tick (fill ticks, Tricky countdown, whoosh).
- Landing reactions 3 and 1 (combo payout, great trick) were missing; the Tricky/Super timer reaction 6 was consumed the
  same tick (1200D0 runs after 115B58) and played 318 with the upper-body mask outside control 0.

### Uber trick hint (0x1EBCA4 pre-pass, 0x1E92A8 layout, 0x1E95A0 draw) — `web/trick-hud.js` uberHint

- owner+0x560 holds "UBER TRICK = @l1 + @square" or "... @r1 ..." (table gp-0xF20 indexed by owner+0x55C). +0x55C starts as
  0x3177F0() & 1 (the visual RNG 0x4FF018 at HUD init 0x1E9AD0; the browser peeks the next word at run start) and flips on
  each pre-pass that sees the Tricky slot 9 at value/maximum == 1.
- Layout: FEFONT (owner+0x428) at 0.7; text runs split at '@' measured by 0x3921F0; icons from table 0x4C8980 (bsl1 / bsr1
  28x16, square 21x20, OV_1-2, not scaled); height = max(29 x 0.7, tallest icon).
- Drawn after the slot loop (0x1F0228) at descriptor 73 (320, 460), centred, bottom-aligned, white with the (2, 2) shadow,
  when the per-player flags have bits 21 and 24 and the pre-pass granted bit 25: slot 9 live, its ratio != 1 and no live
  slot of type 0, 0x21 or 0xB. After the finish (0x1EB9FC) nothing is drawn.
- `web/test-uber-hint.mjs` checks layout and gating; PS2 uber-chain frames 608 / 665 / 1557 match the browser frames.

### The HUD pre-pass is one tick behind the slot draws (call order traced)

A PS2 run with entry hooks on the HUD update 0x1EA930 (vtable 0x4741C4), the HUD draw 0x1EC3F8 (0x4741DC), the human provider
0x127998, the rider post 0x121750, the landing 10E910 and the score tick 117C28 (tools/ps2_audio_log.py
`PS2_AUDIO_LOG_EXTRA`) gives, every frame: draw -> update -> provider -> rider post (landing commits) -> 117C28, then the tick
counter advances. So the draw of frame N shows the bank after game tick N-1 while the pre-pass state it uses (orb / coil
palette P+0x54, flash phase P+0x64, Uber-hint gating P+0x84) was computed by frame N-1's update from the bank after tick N-2.
The browser draws after the tick, so it feeds the gauge palette / phase and the hint from the state captured before the tick
(main.js `hudPrepass`, TrickHud.prepassSlots); per tick the palette tier equals slot 9's arg (0x4C8428 gold < 5, 0x4C8448
orange < 10, 0x4C8468 red 10, 0x4C8488 purple). PS2 frame 1557 (total 5850, gold coil, hint still drawn) matches.

### Career Highlights: monster-trick medals in the career save

- 0x155420 records the run statistics of the rider's score object (+0xFC block) per character at the end of every Conquer the
  Mountain run (finish 0x1251B8 -> 0x238358 -> 0x154AB8; restart / quit 0x1297C8 / 0x12B090 with no score or time), not in
  free ride, human only. Records {best, course}: KO +0x128, Ubers +0x114, super Ubers +0x118, Uber grinds +0x11C, best combo
  +0x184, handplant +0x158, rail +0x154, air +0x14C (floats as (int)(f + 0.5)), race time (min) and event score (slope style,
  big air, pipe, rival points). A value >= the best replaces it and raises the medal to the thresholds reached (never lowers).
- KO (+0x128): 10E468 (crash attack) runs 119400 on the attacker: +1, popup 0x2C (1.5 s), and 10E098(attacker, 1.0,
  category 2). The browser: core `pair_knockout` from web/ai-racers.js when a pair reaction is a crash attack.
- Browser: `web/monster-tricks.js` (runStats / recordRun), career.js `recordRunStats` / `monster(id)` (save `riders[id].monster`
  {bests[10], medals[8], bestCombo}), career-ui.js runBegin / runEnd (main.js at startRun and at the finish).
- Rider Details -> Career Highlights (`ctm-highlights`) draws the original FE.LUI screen 35car_stat
  (tools/export_career_highlights.py -> web/public/assets/UI/career-highlights.json, web/career-highlights.js on web/lui-player.js)
  filled as 0x1F5DA0 does per row r: hl<r> = locale "%s%d" of 0x441BA0[stat] (kT_STATStayOnRail1 = "Stay on a rail - 25m"),
  checkmark<r> / checkbox<r> by tier < medal, hlsec<r> = "%s Monster Trick" with the name, hlsec<r>a = the trick or
  kT_FEUnlockMonsterTrick, HL_arrowup / HL_arrowdown by the scroll position; the page switch 0x1F5A38 hides the other pages'
  groups (race_platinum, freestyle_platinum, ridersbest) and the horizontal / vertical dash on the Highlights page. The render
  equals the PS2 frame local/ps2-capture/menus/lodge/30-career-highlights.png. There is no post-event unlock message in the
  original (nothing queues one when a medal rises; the stats screen shows the medal sum "%d / 24").

### More monster tricks on the PS2 (pipe, tier 5, max stats)

| Capture | Trick | Notes |
|---|---|---|
| monster-chembro | Chemical Brother +20000 | double back flip, Bar Hop (L2+Square), late Mute (L2) with the flip held |
| monster-finger11 | Finger 11 +20000 | diagonal D-pad (back flip + BS 360), Slinger (R1+R2) |
| monster-stoneage | Stoneage +20000 | Zoe's slot 4 tier >= 5 byte poked to Madonna (0x5316D0 word; the lodge selection, read live by 0x14FEA8 on the PS2 and applied to the grab profile by compare-ps2-capture.mjs) |

Crow's Nest (ABA1) kickers, from the countdown anchor with the stage world (`--event`), tier 5 + max stats (rider 0x1455E00):

| Capture | Trick | Notes |
|---|---|---|
| monster-yellowcard | Yellowcard +10000 | BS 900 Mattrickulater off the first kicker (213-tick air) |
| monster-deepsky | Deepsky +10000 | FS 900 Indian, late plain Method held 80 ticks |
| monster-swollen | Swollen Member +10000 | Torpedo poked into slot 2 (0x5316C4 word) |
| monster-xexec | X-Executioner +30000 | second kicker (244-tick air), SSXorcist poked into slot 1 (0x5316BC word) |

Eleven monster tricks are now confirmed on the PS2 (score, popup 0x32, name, Arcade_Uber 8). The Crow's Nest captures start in
the countdown: they also pin the countdown beeps (0x234C..: 29C420 when the countdown state's remaining ticks are a multiple of
60, also at 0 on the GO tick, one tick after the core's race info).

### Gaps

- Knockdown (ko-attack, six riders, `--ai-state`, pad recorded from a closed-loop punch in the browser): Moby knocked down
  at tick 264; the KO +0x128, popup 0x2C and the attacker's meter award (10E098 category 2, 1.0) match the PS2 (score, HUD
  bank and boost words exact through 266, compare-ai-capture.mjs now compares them). From 266 the post-knockdown pair
  separation of the human and Moby is 0.005 cm off (rider-pair physics, the same without pair_knockout; AI/physics agents).
