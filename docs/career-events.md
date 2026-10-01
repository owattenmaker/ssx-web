# Conquer the Mountain, events and rounds (browser port)

This document covers the career and progression flow as the PS2 game implements it, and how the browser port
reproduces it: the MCOMM hub, Transport, peaks and goals, the lodge, event rounds, rules, results, medals, cash,
unlocks, records and saving. Every rule comes from `SLUS_207.72` code or data, or from the disc's locale and UI
files. The flows were checked against real PS2 frames captured with ARMSX2.

- **Rules:** `web/career.js` (pure logic, tested by `web/test-career.mjs` in `npm test`).
- **Screens:** `web/career-ui.js`, hooked into `web/ui.js`.
- **Save:** `web/career-save.js`.
- **Locale lookups:** `web/locale.js`.
- **Tables:** `tools/export_career.py` writes `web/public/assets/CAREER/career.json` plus the course and map
  pictures (git-ignored, like every extracted asset). It is part of `npm run setup`.

## The original structure

### Courses and peaks

The course table is at `0x43D950`, stride `0x64`:

| Offset | Field |
|---|---|
| +0 | index |
| +4 | name |
| +0x24 | short name |
| +0x34 | code |
| +0x44 | world |
| +0x54 | career peak (0-based) |
| +0x58 | station flag |

Every progression function reads the career peak from +0x54 (`0x144C78`). Field +0x60 is the map region. So
**Intimidator is a Peak 2 race**.

| Peak | Races | Freestyle | Rival (backcountry) | Peak event |
|---|---|---|---|---|
| 1 | Snow Jam ARA1, Metro-City BRA2 | R&B ASS1 (slope), Crow's Nest ABA1 (big air), The Junction BHP1 (pipe) | Happiness ABC1 | Peak 1 Race / Peak 1 Jam |
| 2 | Ruthless Ridge CRA3, Intimidator DRA4 | Style Mile DSS2, Launch Time CBA2, Schizophrenia CHP2 | Ruthless DBC2 | Peak 2 Race / Peak 2 Jam |
| 3 | Gravitude ERA5 | Kick Doubt ESS3, Much-2-Much EBA3, Perpendiculous EHP3 | The Throne EBC3 | All Peak Race / All Peak Jam |

**Game modes** (`0x535C12`, names at `0x43E978`):

| Mode | Event |
|---|---|
| 0 | Race |
| 1 | Slope Style |
| 2 | Half Pipe |
| 3 | Big Air |
| 4 | Rival Time |
| 5 | Rival Points |
| 6–8 | Time Challenge, Peaks 1–3 |
| 9–11 | Points Challenge, Peaks 1–3 |

The per-peak **goal lists** are at `0x45AAD8` (the Transport lists its rows from the tables 0x4781D0 / 0x4786E0 instead, which order
Peak 2 / 3 freestyle slope style, big air, pipe: ctm-parity.md "Transport lists and help"):
- Race: standard races, then the rival race, then the peak race.
- Freestyle: slopestyle, big air and pipe, then the rival jam, then the peak jam.

### Rounds (GameModeMan, `*0x4A2C6C`, handlers created by `0x237CF8`)

The GameModeMan fields are:

| Offset | Field |
|---|---|
| +0 | round |
| +0x70 | next round |
| +0x74 | round just played |
| +0x84 | fresh event |
| +0x9C | event complete |

Pause **Restart** copies +0x74 into +0x70 (`0x238348`).

**Race** (handler 1: init `0x23A108`, results `0x23A760`). The career always runs Qualifier, then Semi Final,
then Final, with 6 riders each; the handler never reads the course.
- In every round, 1st–3rd advances; otherwise the same round repeats.
- In the final, 1st/2nd/3rd win gold/silver/bronze. Any other place completes the event without a medal; the results'
  Restart plays the final again, but any new entry (the gate, the map) starts a fresh qualifier: 22D6C8 ->
  cGameModeMan_initGameMode (0x22D89C) -> 0x238C80 (GMM +0x84 = 1, +0x70 = +0x74 = 0). PS2 ctm-parity/fresh: the Snow Jam
  final given up, Transport -> Snow Jam, the gate -> "Snow Jam - Race / Qualifier" with a new roster (pv `freshEvent`,
  corrected 2026-09-28; the port used to resume the final).
- The round is decided when the player crosses the line.
  - Riders still on course get the estimate from `0x122D78`:
    `raceTicks + remaining / max((origin - remaining) / raceTicks, 30 - place)`, in cm per tick.
  - DNF is 360000 ticks (`0x23A860`).
- **Single Event** forces round 3 (the final only).
- **Adaptive difficulty** is stored in profile char +0x280/+0x282 and updated by `0x147D20`/`0x147E18`.
  - Moving the level up needs two steps and moving it down needs two steps; the level stays in 0..2.
  - A win steps it up if time[slot1] − time[slot0] ≥ 601/301/181 ticks in rounds 1/2/3.
  - It steps down on a qualifier fail, 5th–6th in the semi, or last in the final.

**Freestyle** (handler 0: init `0x238E20`, results `0x239230`). There are five computer riders who never ride.
- **Posted scores.** At event start, `0x239AA0` → `0x1453D0` posts all three rounds from table `0x440B38`.
  - The table row is {course, round, 5 × score/100, time limit s}.
  - The score is scaled by the level (0.9 at level 0, 1.2 at level 2; the leader is never scaled).
  - A jitter of `trunc(s*(rand%200-100)*0.0005)` is added, and the result is rounded down to a multiple of 20.
  - Slot 1 is the peak rival.
- **Heat 1:** the player's score against the AIs' heat 1 + heat 2 totals. Top 3 goes **straight to the final**;
  otherwise heat 2 follows.
- **Heat 2:** combined scores. Top 3 goes to the final; otherwise back to heat 1.
- **Final:** a solo heat against the posted final scores. 1st–3rd wins a medal; otherwise the final repeats.
- **Time limit:** the table value × 60 ticks (GameModeMan+0x78 = `0x1454F8`(course, round) × 60 when +0x88 is set). 125228
  (per rider from 121818) ends an unfinished run once race ticks pass int(limit/60)×60 (tick 7201 for 2:00): rider+0x480 = 1
  (DNF) and the finish routine 125108; the results handler 239230 then stores the heat score as 0.
- **The race clock's start (2026-09-30):** race tick 1 is counted in the tick after the countdown reaches 0, not one later. WS3's
  update 234C68 (after the rider manager's tick) requests WS4 when the countdown +0x1C is 0, and WS4's enter 234E20 selects Race
  (0x234E30) before the clock's own 0x113D60 select runs. So in a tick, race ticks = total - 179 (ARMSX2 call probes c0a-ws13pclk /
  c0a-ws13pclk2 / c0a-ret13). core race_bridge.cpp gate_go. Its effects: +0x478, the results time, the HUD clock and this time limit
  (peak3/kick-doubt-event-tuck's 1:00 time-out is now exact). **Gap:** WS3 also requires 0x535C10 != 4 and 270280(*(S+0x28)) == 0,
  the streamer state. The port does not model 270280; whether a countdown can end while it reports busy is unconfirmed.
  [ctm-events-in-world.md](ctm-events-in-world.md) "The race clock's GO".
- **Single Event:** the final only, and the posted final scores are the round-1 values.
- **Difficulty:** +0x284/+0x286. It steps up on a heat-2 win or a final medal and down on a final fail.

**Peak rival** (`0x145750`):
- Peak 1: Mac, or Griff if the player is Mac.
- Peak 2: Nate, or Zoe if the player is Nate.
- Peak 3: Psymon, or Elise if the player is Psymon.
- Character indices follow CHARDB.DBL: 0 Moby, 1 Kaori, 2 Allegra, 3 Mac, 4 Zoe, 5 Griff, 6 Elise, 7 Nate,
  8 Psymon, 9 Viggo. The web's Sam uses Mac's slot, as in the Sam PS2 build.

### The peak rival in career events (2026-09-30, career-rival agent)

Settled from SLUS_207.72 and derived ARMSX2 states (`local/career-rival/nav`, scripts there; never an original). Rival = 0x145750:
peak index = course table 0x43D950 + course*100 + 0x54 (144C78); 0 Mac (Griff for a Mac player), 1 Nate (Zoe), 2 Psymon (Elise), else Mac.

| Event | Where the rival is | Code |
|---|---|---|
| Race qualifier / semi (career) | nowhere: 0x23A4F0's ten entries skip him | 0x23A4F0 |
| Race **final** (career, every peak) | **rides slot 1** | 0x23A108 round 3, career branch 0x23A3D8: handler+0x40 = player, +0x44 = 0x145750, +0x48.. = the previous round's top three (0x536708, human skipped; GMM+0x40 cheat id or GMM+0x18 base per slot), +0x54 entry 9; 0x23A668 copies; WS13 0x235AA0 rebuilds the riders |
| Race (Single Event) | nowhere | 0x23A384: 0x535C11 != 0 rebuilds heat 1 (0x23A4F0) |
| Slope style (career, every round, every peak) | **posted** in slot 1 with the leading column; **nobody rides** | 0x238E20 career path (0x535C11 == 0 skips to 0x238F08); round 1 (0x238F7C) sets GMM+0x10 = 5, +0x14 = 5 for every kind; rounds 2 / 3 (0x239118 / 0x23919C) keep +0x14. 0x239AA0: live slots = +0x10 - +0x14 + 1 = 1 (the human), slots 1..5 post columns 0..4 (0x1453D0) for GMM+0x1C.. ; 0x239938 writes +0x1C = rival (0x239A10) |
| Slope style (Single Event) | nowhere (not even posted) | 0x238E78: +0x14 = 4; 0x239A78: +0x1C = the LAST shuffled character, who rides; the shuffle skips the rival |
| Pipe / big air (career) | posted in slot 1 | as slope style (posting only) |
| Rival Time / Rival Points | rides (one round) | docs/backcountry.md |
| Peak runs | nowhere (solo against 0x440D18) | |

So Peak 2's career finals of Ruthless Ridge and Intimidator have Nate in slot 1, and a career Style Mile posts Nate (358740 in the
derived heat 1) without anyone riding; Kick Doubt (ESS3, Peak 3) posts Psymon / Elise the same way (same code; no Peak 3 career state was
made). A lost heat sets the race handler's +0x8 / +0xC (0x23AAE8 / 0x23AAEC: "this round's roster is built"), so the results' Restart
replays the same riders; a won round leaves them 0 for the next round's build.

**PS2 evidence (derived, local/career-rival/nav; README in each characters/career folder):**
- Career Style Mile heat 1 (peak2-arr -> MCOMM Transport -> Peak 2 -> Freestyle -> Style Mile -> Yes -> the gate -> the card -> Cross):
  card "Nate 358740 / Allegra 200600 / Moby 92060"; at the countdown GMM+0x14 = 5, 0x535C04 = 0, 0x535BF8 = 1, roster [Zoe, Nate,
  Allegra, Moby, Psymon, ...], posted rounds 358740 / 402400 / 459660 for Nate. Two rider objects from the world stay held at the grid
  (control 6) through race tick 189; the HUD shows the standings rows, no OPPONENT line. Facts: local/career-rival/dss2-career-heat1.json.
- Career Ruthless Ridge and Intimidator finals (`characters/career/{CRA3,DRA4}-final-zoe`): the qualifier, the pause's Give Up, the
  results with GMM+0x74 = 3 and handler 0x59FF00 +0xC = 0 poked (round3.patches.json), Restart -> Yes -> WS13 -> "Final Round" card:
  Zoe, **Nate**, Psymon, Brodi (on Zoe), Griff, Elise. countdown.p2s is game tick 18, ws3.p2s WS3 tick 1/2. The race level is 2
  (Zoe's career after the Snow Jam gold).

**Port:**
- Slope style: already as the PS2 (web/career.js postFreestyleScores / lineup.js freestyleSlots, ai-race.js prepareFreestyle: no rider).
  test-career-rival.mjs replays the derived heat 1's whole posting on the career generator (roster, three rounds, 117 draws) and Career.
- Race finals: web/lineup.js roundEntries already put the rival in slot 1; the Peak 2 courses had no Nate record ("No computer-rider data
  for nate (base 7) in slot 1"; ws13Rebuild's heatLineup kept the semi's riders). tools/export_lineups.py now builds CRA3 / DRA4 with
  Nate's skin part and grid[1][3f7fffff] from those countdowns (additive; scratch local/career-rival/export), behind **pv careerRival**.
- The career race level (pv **careerLevel**): docs/ai-racers.md "Difficulty by race level".
- Gates: test-ps2-captures careerrival/cra3-final (human incl. score, five riders, RNG, ranks, pair records exact to 1500) and
  careerrival/dra4-final (the same; human score to 228: the attacked bail, docs/ai-racers.md); web/test-career-rival.mjs (the page's
  assembly equals the PS2 countdowns' riders leaf for leaf).

**Rival challenges** (modes 4/5) are one round of player vs. peak rival and must be won. **Peak challenges**
(modes 6–11) are solo runs against table `0x440D18`.

### Medals, cash, goals, unlocks (`0x154EE8`)

- **Medals** (`0x155328`): encoded as 0 platinum, 1 gold, 2 silver, 3 bronze, −1 none. Rival challenges award
  gold for a win only.
- **Platinum** (`0x1456A0`, table `0x440E80`, career only) upgrades any medal that was earned:
  - Races: time ≤ T×60 ticks (150/150/160/165/160 s).
  - Freestyle: score ≥ T×100.
  - Peak events have no platinum.
- **Cash:**
  - Standard and rival events: table `0x4405A0` [course][plat/gold/silver/bronze] × 100.
  - Peak events: `0x440D18` row cash × 100; gold and platinum pay the gold row.
  - The full amount is paid only if the medal improves the stored best; otherwise half. No medal pays nothing.
  - Cash goes to current cash (+0xAC4) and lifetime earnings (+0xAC8).
- **Goals** (`0x157BF0`):
  - RACE/FREESTYLE: a medal on every entry of the goal list.
  - EXPLORE: collectible and Big Challenge medals (tables `0x45AFE8`/`0x45B018`).
  - EARNINGS: lifetime earnings ≥ $100,000 / $250,000 / $1,000,000 (`0x45B048`).
  - Completing **any** goal of peak N clears the peak N+1 pass bit (profile +0x278 bits 12/13).
- **Event locks** (+0x278):
  - The rival challenge opens when every standard event of the list has a medal (bits 6+peak / 9+peak).
    The PS2 help reads "LOCKED. Complete all race events on this peak."
  - The peak event opens when the rival also has a medal (bits 14+peak / 17+peak). The help reads
    "LOCKED. Complete Happiness to unlock."
- **Records:** the defaults at `0x43FB28` are 26 slots × 5 entries of {value, character, name}. The value is
  whole seconds for times and points for scores. The slot comes from `0x14AB68` via table `0x45A2F8` (Snow Jam
  slot 12: BOMBER 02:57, as the PS2 shows).
- **Buy Attributes** (`0x150C20`): each raw point (+0.2) costs `0x440550`[raw/5−1] = $250 … $5,000. Raw values run
  5..55 (1.0..11.0). The rider ranking is `(Σraw − Σraw%5)/35` (`0x181BD0`, Select Character only). The whole screen:
  "Buy Attributes (cFEStateBuyAttrib)" below.

## Browser flow

1. Main Menu → **Conquer The Mountain** → Select Character → the world load (no Setup Character in CTM, 0x1A0B00): a new
   career at Happiness with the plane drop, else the last lodge (2026-09-25, [ctm-parity.md](ctm-parity.md)).
   - MCOMM (Start in free ride, OV.LUI 31paus_freeride in the PDA frame, `web/ctm-pda.js`): Return / Transport / Session /
     Messages / Audio / Options / Quit, as on PS2. Quit: "Quit Game" (No) → "Save progress before quitting?" → the title.
2. **Transport** → Select Peak → Select Peak Goal → Select Race/Freestyle/Freeride Event.
   - Locked peaks show the lock.
   - Square (Shift or I) toggles MAP/INFO: goal progress, and per event Run/Event/Medal/Top time.
3. **"Transport to this area now?"** → the round objectives card → ride. **In free ride (2026-09-25, [ctm-flow.md](ctm-flow.md))**
   an event on a ported course transports to that course inside the world, and riding into its start area (the
   RaceRideState gate, builtin 67 → 0x22D6C8 → world state 1) starts it the PS2 way: no prompt and no load screen, the fade
   under the bars with "Loading...", the venue fly-over, the approach, the card over the start-gate idle, the countdown. After
   the event, results "Transport" plays `endevent_trans_arr` where the course has one and opens Select Peak; picking the
   course (event or free ride) returns to free ride at its session point 1 with a white fade. Restart and Next heat ride the
   gondola back up to the card; the final heat adds the start hut.
   - An event on another location reloads the page into it (`?course=CODE&autostart=1`, the maps pipeline),
     and the pending round resumes at its objectives card (`save.pending`).
   - The card shows the objective text by round, the Riders or Current-standings lineup, and the record.
4. In the race, **Start** opens the career pause: Return / Restart (Are you sure?) / Messages / Audio / Options /
   Give Up (Are you sure?).
   - Give Up ends the run like the time limit: TIME'S UP banner, then the results with the player DNF (race and
     freestyle; see "Give Up" under "Freestyle run in the browser").
   - Freestyle rounds use the freestyle HUD and end at the finish line or the table limit (see "Freestyle run in the
     browser" below).
5. **Results:**
   - The panel shows "<Course> - <Event>" and "Qualifier Results / Semi Final Results / Final Results" or
     "Qualifier Heat N Standings", then the ranked rows and the original message.
   - Options: Next heat / Final Round (next round = the final) / Transport, then Restart, Replay ([replay.md](replay.md)),
     Records, Quit (→ "Save progress before quitting?" → the title).
   - A result that pays shows the reward list first (overlay 0x10, OV.LUI 62reward_list): "Cash: $10,000", each award's
     title with its items (e.g. "Earnings goal complete!" / Peak 2 pass / Poster / Trading cards, "Gold medal earned" /
     Accessory), then the results with "Congratulations!  You've received a Gold medal." ([ctm-parity.md](ctm-parity.md)).
   - Records opens the Top 5 Record Times/Scores.
6. **Freeride → Green/Blue Station → "Would you like to enter the lodge?"** → **Lodge - Peak N**.
   - Items: Return to Game ("Save progress?") / Equip Gear / Buy Gear / Buy Attributes / Rider Details / Music /
     Save Game / Quit ("Quit to Title screen?").
   - Buy Attributes and Save Game work. Gear, Rider Details and Music are greyed.
7. **Single Event** (the original Select Peak / Select Mode / Select Event map screens, `web/fe-event-select.js`,
   docs/characters.md) runs the final only through the same objectives and results
   screens ("Single Event Results", Next event).

### Freestyle run in the browser (2026-09-22, super pipe agent)

Checked against The Junction PS2 captures (docs/locations.md "The Junction super pipe"):

- **HUD** (`career-ui.js` `freestyleHud()`/`hud()`, `ui.js` skips its race-only elements): 1EA930 sets owner+0x3CC from
  table 0x478078 by the mode byte 0x535C12 (race 0x1530C047, slope style 0x1530C056, pipe/big air 0x1530C016).
  Freestyle has no race place (bit 0x1) or progress meter (0x40) and adds the standings (0x10): three rows (type-7 case
  1ECFFC, descriptor 0x17 at (20,20), rows from 1EB160) over the posted scores best first, with the player's row
  (name, score +0x198 plus the heat-2 carry) inserted where it is at least the posted score. The clock (bit 0x4,
  1EC3F8, descriptor 0x1A) counts down int(limit/60)×60 − race ticks as HH:MM:SS of ceil(ticks/60) seconds and turns
  red (0x4C8688) for half of each second under 10 s (owner+0x3D0). "Time Left" belongs to the split-screen HUD only.
- **Time limit:** in the core (`OriginalRaceSession::endTick`, 125228; `race_time_limit` set by `play()` through
  `ui.cb.timeLimit`, `race_timed_out`); the results arrive through `ui.showResults({dnf})` like a finish.
- **Finish / time up:** state+0x88 = 1 or 2 and +0x80 = 0xFFEFFFFF hide every element but the 0x100000 banner (21F660,
  descriptor 0x1B at (320,180), 240×41): 'fini' (owner+0x4B4, OV_1-3 FINISH!) with the run score under it, or 'timeup'
  (owner+0x49C, TIME'S UP, no number). After 3 s of the finish timer +0x470 the "finishov" panel (ctor 21CF60, vtable
  0x473D28) scales in and fades in: '1st run' / '2nd run' / 'Final run' by round, 'Nth place' from ranking the run
  against the round's five posted scores (238B70, clamped to 6th) and '%d pts' (kT_OVRCMNPointstotal). PS2 timing from
  `pipe-finishov*.png`: banner until +0x470 ≈ 3.0 s, panel box ~0.25 s, text ~0.2 s. The panel is FINISH only: a TIME'S UP
  run (timeout or Give Up) shows nothing after the 3 s banner (PS2 `pipe-brake-dnf` ticks 7566..7625, Give Up frames).
- **Results timing (all events, `main.js`):** EndRace (one tick after the finish) requests the results, but the original
  shows them, and starts the replay, only when the finish HUD is done. Per-tick PS2 records (+0x470 per tick, replay =
  race tick counter back to 1): `pipe-finishov` finish record 4385 → replay 4793 = **408** ticks; `pipe-brake` (2:00
  timeout) 7363 → 7651 = **288**; pipe Give Up (savestate clock, total ticks 879 → 1167) = **288**. The 288/408 split
  follows +0x480 (TIME'S UP vs FINISH), not the stop: control 10 raises +0x470 to 10 when the stop clip ends (12C964, at
  164 / 254 / ~206 ticks in those runs), and the post-finish screen 0x233CD8 also waits for every human under 277.78
  cm/s (gp-0x5174). The browser shows the results `RESULTS_TICKS_FINISH` = 408 / `RESULTS_TICKS_TIME_UP` = 288 ticks
  after the finish (counting the finish tick); it used to show them on the EndRace tick, so neither banner nor panel
  was ever visible. Browser check: BHP1 neutral run finishes at tick 4403 (the PS2 pipe-finishov tick too), results
  at finish + 407.
- **Give Up (pause menu, 2026-09-22):** the original does not end the event from the menu. The pause handler case at
  0x20DA58 (menu 0x20D1D8 family; confirm "Give Up / Are you sure?") sets gp-0x9F4 = 3 (close the pause menu),
  gp-0x6A0 from table 0x4428F0 and calls **1253D0(human)** (human = G+0x84 → +0xC → +0x40 → +0x18, G = *(gp-0x848); G+0x84 +0xC is the race clock). 1253D0: if the
  rider is unfinished (+0x470 < 0), +0x480 = 1 and 0x5366D0[slot] = 1 (the give-up flag the results rows 0x1E5F6C and the
  finishov setup 0x1E8D38 read). The game resumes, and on the next tick 125228's second branch **125368** (reached when
  the time limit has not expired, timed or not) sees +0x480 == 1 with the game screen back (G+0x84 +0x214 == 4, 5 once
  finished) and calls the finish routine **125108** (no time-up sound 2A3C00). The HUD 1EB9E8 then sets state+0x88 =
  +0x480 ? 2 : 1, so the banner is **TIME'S UP** ('timeup', 21F660 with t0 = 1), exactly as for the 2:00 limit, for
  races too. Results: the player is ranked last with **DNF** (freestyle "Heat 1: DNF", race time DNF).
  PS2 evidence (ARMSX2, `tools/ps2_menu_capture.py`, CTM from `ctm/state-transport-peak-goal.p2s`: scripts
  `local/ps2-capture/menus/scripts/ctm-junction-{1,2,giveup}.json`, output `local/ps2-capture/menus/pipegu/`):
  `giveup.f01370` the dialog, `fine.f01396..f01412` TIME'S UP over the pipe, `fine.f01416`+ no banner and no panel,
  then "The Junction - Super Pipe / Qualifier Heat 1 Standings" with Zoe 6th, DNF. Savestates `ticks.f0*.p2s` /
  `t2.f0*.p2s` (EE RAM): race ticks stop at 699 = +0x478, +0x480 = 1, 0x5366D0 = 1, +0x470 counts up from 0, jumps to
  10 at the stop, and the replay (clock phase 4, total ticks reset) starts 288 ticks after the finish. The Snow Jam
  race Give Up (`menus/race/15-giveup-a.png`) shows the same TIME'S UP banner. (The Single Event pause menu has no
  Give Up: Return / Restart / Audio / Options / Quit, `pipegu/run.f00260.png`.)
  Browser: `career-ui.js` `giveUp()` → `ui.cb.giveUp` (`main.js`) → `core._race_give_up()` (`race_bridge.cpp`,
  `OriginalRaceSession::giveUp`: +0x480 on an unfinished rider) and resume; `endTick` runs the 125368 branch; the
  freestyle HUD draws TIME'S UP for 3 s and no panel, `ui.js` draws the same sprite for a race; results after 288 ticks
  with the human row DNF. `test-race-finish.mjs` covers the core path (DNF, penalty, results pulse, no effect after the
  finish, reset). Verified in the browser (BHP1 single event, QA hooks): TIME'S UP on the tick after Give Up, empty HUD
  at +0x470 3.3 s, results on finish tick + 287, "6 Sam DNF".

**Peak 1 freestyle (2026-09-23):** R&B (slope style: one computer opponent rides, 4 posted riders, checkpoint time extensions, OPPONENT HUD line, score place) and Crow's Nest (big air) are playable in Single Event and Conquer the Mountain; rules and evidence in [slopestyle-bigair.md](slopestyle-bigair.md).

**Availability.** An event is playable when its location is in `/assets/courses.json` with `ready`. Today that
is Snow Jam and Metro-City. Other standard events show their original help plus "(This location is not ported
yet.)". The Peak 1 rival challenge (Happiness, modes 4/5) is playable from Transport and Single Event ([backcountry.md](backcountry.md): `career.js rivalResult`, `web/rival-mode.js`); peak challenges need the whole-mountain runs.

**Saving.** Progress is stored under the `localStorage` key `ssx3.career.v2` (schema v2, atomic writes, v1 saves migrate on first load; `web/save-store.js`, docs/characters.md "Saving progress"), per rider: cash, lifetime
earnings, best medal per event, best value, the in-progress round, attributes, difficulty levels and peak
passes. The global records and the RNG state are saved too. Every read and write is wrapped in try/catch; with
storage blocked, play continues in memory. Lodge Save Game writes it explicitly, and results, attribute
purchases and peak changes also persist. Options > Save/Load (`web/fe-saveload.js`) has Save game, Load game, New game and a browser Export / Import
of the whole save file.

### Hooks for the AI racers agent (race placement)

`career-ui.js` ranks a race from `result.standings`, or from `ui.cb.standings()`. Each row is:

```
{human, character|name, finishTicks|null, remaining, place, origin, dnf}
```

- `character` is the CHARDB index or a rider id string; `name` overrides the displayed name.
- `finishTicks` is the rider's +0x478.
- `remaining` is +0x4D0.
- `place` is +0x86C / the ranking place.
- `origin` is +0x4D8.

Rows for riders still on course are estimated with `0x122D78`, so `remaining`, `place` and `origin` must be
supplied. `ui.cb.lineup()` returns the names shown on the objectives card.

These hooks are now connected for courses with computer riders (Snow Jam, Metro-City; `web/ai-race.js`, docs/ai-racers.md). Without them (other courses, `?ai=0`), a race round ranks only the player. The results then say "(No computer riders in this
run.)", and the round counts as 1st. This is the documented gap until AI standings are connected; freestyle
does not need it, because its computer riders only post scores.

The difficulty level the AI should use is `career.rider(id).level.race.level`. CPU attribute levels per peak are
1, 4 and 7 (from the progression research).

## Verification

- **`web/test-career.mjs`** (in `npm test`) checks:
  - Table values against the ELF: course peaks, cash, platinum, AI table rows, attribute costs, earnings goals,
    record slots and defaults, goal lists and CHARDB names.
  - The locale lookups.
  - `0x122D78`, `0x1453D0` (including the real PS2 R&B posted scores below), `0x147D20`/`0x147E18` and ranking.
  - A complete race event: qualifier fail and retry, semi with estimated and DNF riders, platinum final, half
    cash on a repeat, and a failed final repeating.
  - A complete freestyle event: heat 1 → heat 2 → back to heat 1 → straight to the final → silver, then
    platinum.
  - Single Event, goal locks, peak pass through the earnings goal, attributes, and save failure handling.
- **`tools/export_career.py`** refuses to export if the instruction words holding the DNF time, level-up
  margins or estimate speed differ from the expected ones.
- **ARMSX2 ground truth.** `tools/ps2_menu_capture.py` hooks the front-end pad read `0x321298` (port-0 call from
  `0x227E98`) and drives menus from scripts or sessions; the captures are in `local/ps2-capture/menus/`. It
  captured:
  - Title → CTM → rider → free ride → MCOMM → Transport (peaks, goals, race/freestyle/freeride lists, locks,
    INFO panels).
  - Green Station → lodge and all lodge submenus.
  - Snow Jam transport → race gate → Round Objectives ("Qualifier", Riders list, Record time 02:57) → pause
    → Give Up → Qualifier Results.
    - The results were Zoe DNF, "Sorry, you need a 3rd place rank or higher to advance into the semi final
      round.", Transport/Restart/Replay/Records/Quit.
  - An idle race finishing 5th (the same fail message), Top 5 Record Times (BOMBER 02:57 … BADBRAD 03:35), and
    Single Event screens.
  - The R&B Single Event card showed posted scores Nate 254500, Kaori 124080 and Elise 49800. These are
    reproduced exactly by `0x1453D0` from the round-1 columns at level 1 (jitter draws 36, 68 and −8).
  - The browser screens were compared side by side with these frames: transport info rows, record time and
    lock texts match.


## Lodge shops, awards and attributes (follow-up)

### Attributes change gameplay

Buy Attributes now feeds the original stat getters. Each getter reads a progress byte `k`:

```
stat_k = int(raw_k/5) / max_k
```

- `raw_k` comes from the runtime bank `0x535538 + bank*70 + char*7 + k`.
- `max_k` is the byte at `0x5308D8 + char*15 + 8 + k`. It is 11 for every rider on the disc.
- The division is `div.s`, rounded to nearest.

| k | Attribute | Getter | Consumers |
|---|---|---|---|
| 0 | top speed | `0x1494C0` | `0x11B3F8` speed limit, `0x13CCF0` |
| 1 | acceleration | `0x1493D8` | `0x13C948`, `0x13CCF0` |
| 2 | tricks | `0x149690` | `0x120038` grab rate (`0x10E098` calls it but does not use the value) |
| 3 | edging | `0x148D80` / `0x148E68` | `0x13CCF0`, `0x13D028`, `0x13D818` |
| 4 | spin | `0x1495A8` | air control `0x133128` / `0x133308` |
| 5 | toughness | `0x148F50` / `0x149038` | collision weight `0x11FF98`, `0x107888` |
| 6 | stability | `0x149120` / `0x149208` | landing `0x139C88`, rails `0x13AF28` |

The Buy Attributes rows map to original indices through `0x4780B0`; see `originalAttributeBytes` in `career.js`.

How the browser wires it:
- `web/rider_attributes.hpp` and `web/attribute_bridge.cpp` add `set_rider_attributes(raw[7], override)`.
- That call rewrites the stats the physics already reads:
  - ground profile `topSpeedStat`, `speedStat` and `edgeStat` in `physicsProfile` and all 19 `physicsMaterials`, reapplied after every reseed in `core.cpp`;
  - `landingProfile.landingStat` (the rail balance reads the same field);
  - `airProfile.trickStat` and `grabProfile.playbackRate`, through `browser_apply_animation_attributes` in `animation_bridge.cpp`.
- Every career or Single Event round calls it before starting (`ui.cb.attributes`).

Defaults are unchanged:
- Nothing is patched until the career sets attributes.
- Level 1 gives exactly the captured `0x3DBA2E8C` (1/11).

`web/test-rider-attributes.mjs` (in npm test) checks four things:
- default stats are bit-exact;
- explicitly setting level 1 leaves an event run bit-identical;
- Speed/Accel 11.0 raises top speed and distance (15 s tuck: 31.48 → 31.97 m/s, 213 → 227 m);
- Stability reaches the landing stat.

`test-ps2-captures.mjs` still passes (58 scenarios). Toughness has no consumer yet: rider-pair impulses are not active in the browser.

### Buy Attributes (cFEStateBuyAttrib) (2026-09-28, pv `buyAttribs`)

Owen: "the buy attribute system is not at all how it works on the PS2". The port sold a whole level per press, let the pending
points run to 11.0, took them off "You have", bought them without a popup and never gave them to the free-ride rider. Everything
below is from the code (decomp names from ssxdecomp/ssx3 `fe/festatebuyattrib`, `be/beintecon`), then checked value for value on
the PS2 (ARMSX2, `local/ps2-capture/lodge/attrs/`, read with `tools/ps2_buy_attribs_state.py`).

**Storage (per rider, career profile 0).** Rider record = `0x4A6CA8 + player*0x9B50 + char*0xF88` (Zoe 0x4AAAC8):
- cash `+0xAC4` (lifetime earnings `+0xAC8`): the currency; there are no attribute points;
- seven raw bytes `+0xBDF..+0xBE5`, original order 0 speed, 1 accel, 2 tricks, 3 edging, 4 spin, 5 toughness, 6 stability.
  Screen row k reads byte `0x4780B0[k]` = {1, 3, 0, 4, 6, 5, 2} (Acceleration, Edging, Speed, Spin, Stability, Toughness, Tricks);
- a new career (`0x151600`) copies CHARDB `0x5308D8 + char*15 + 1..7` (5 for every rider) and zeroes the cash.
- `0x148098` copies every profile's bytes into the runtime bank `0x535538` (+0x46 per profile, +7 per rider); the stat getters
  (`0x1494C0` ..) read the bank live: `int(raw/5) / limit(11)`. A 0.2 point changes nothing until a whole level.

**State object** (`0x1F4728` constructor, state 0x29, vtable 0x473908; FE.LUI `33buyattribs`):
- `+0x4C` raw[7] (via `0x148158`), `+0x68` level[7] = raw/5 (only grows: `0x1F4EA8`), `+0x84` pending[7] (zeroed),
  `+0xA0` total, `+0xA4` bank = cash on entry (`0x150928`), `+0xAC` the prompt widget.

**Price.** `0x150E50(level−1)` = `*(0x440550 + (level−1)*8 + 4)`: $250 / 500 / 750 / 1,000 / 1,250 / 1,500 / 1,750 / 2,000 / 2,500 / 5,000
**per raw point** at levels 1..10 (a whole level is 5 points: 1.0 → 2.0 costs $1,250).

**Input** (`0x1F4C30` Left/Right on AttribMenu's focus `+0x95`, `0x1F4A90` notify 5 Cross / 6 Triangle):

| input | rule | sound (listener kind) |
|---|---|---|
| Right | level ≥ 11: error. bank < total + cost: error. raw + pending ≥ (level+1)*5: nothing. Else pending+1, total+cost, frame `hl right` | 1 / 4 / silent |
| Left | pending > 0: pending−1, total−cost, frame `hl left`; else error | 1 / 4 |
| Up / Down | engine menu AttribMenu, flags 0x2C0: wraps | engine |
| Cross | bank < total or total ≤ 0: error; else the buy popup | 4 |
| Triangle | `TransitionOut`, back to the lodge (Buy Attributes focused); pending dropped, no question | engine |
| Start, Circle, Square | nothing | |

UILeft / UIRight are `DPadL/R.repeat` (input.map): a held Right repeats (PS2 ba2 hold, sampled every 3 frames: the 2nd point ~25 frames after the press, then every ~10-15).

**Popup** (`0x1CABA8`, `0x1CAFC0`; the buy popup of the Ubertrick shop): item type line **"Buy attributes?"**
(`kT_OVRCMNBuyAttributesColon`), item name and question blank, "Cost: $total", "You have: $bank" (both `0x198AF0`), Yes / No
with **Yes focused**, Up / Down wrap, Triangle does nothing. Yes (`0x1CAF00`: +0x6C = 1, event 0x16 → `0x1F4A60` → `0x1F5300`):
- bank −= total, total = 0;
- per row, pending × `0x150C20(econ, 0, char, 0x4780B0[k])`: cost = table[byte/5−1]; fails when cash < cost or byte == 55;
  cash −= cost, byte + 1, `0x148098` (runtime bank);
- raw += pending, pending = 0, every display refreshed. **The screen stays open.**
- The econ interface's `+0xC` call after it is the empty `0x15C7D8`: **no save**. Saving is the lodge's Save Game / "Save progress?".

No keeps the pending points (the screen as before the popup).

**Display.**
- "You have:" `CurrentMoney` = bank (not reduced by pending); "Total:" `AttributeCost` = total.
- `lvl%d` = `"%d.%d"` of (raw/5, (raw%5)*2): the bought value only (pending never shows in the number).
- `cost%d` = the point price at the row's level; hidden at 11.0 (`0x1F5060`).
- Bars (`0x1F50B0`, gp−0x554C = 37/11): orange `Vector%d` raw × 3.36 px, light-blue `under%d` (raw+pending) × 3.36 px, 12 high,
  over the dark bar with the LUI's tick marks.
- The row's help text (group `09ee0c5e`); legend Cross "Buy attributes", Triangle "Previous". No rider ranking on this screen.

**Locks:** none (the lodge item is always live).

**PS2 runs** (`local/ps2-capture/lodge/attrs/`, `tools/ps2_navigate.py`; memory per step):
- `lodge-rich.p2s` = lodge-clean with Zoe's cash 20,000 and bytes spin 54 / toughness 55 / stability 9 (profile and bank).
- ba1: six Rights on Acceleration → pending 5 (the sixth ignored), Left → 4; Spin 10.8 and Stability 1.8 take one point each
  (the second ignored); Toughness 11.0 errors; a held Right on Speed stops at 5; total $7,750, "You have" $20,000; Cross → popup
  (cost 7750, have 20000, Yes focused); Triangle in the popup does nothing; Down → No.
- ba2 (from the popup): Yes → cash 12,250, bytes 10,10,5,5,55,55,10 in the profile and the runtime bank, the screen shows 2.0 /
  $500; Speed 2.x takes five $500 points; six Lefts, the last an error; Cross with total 0 does nothing; Triangle → the lodge on
  Buy Attributes; re-entry at Acceleration; Up wraps to Tricks; the hold test (repeat timing).
- ba3 (lodge-clean, $507): the third $250 point errors; Yes on $500 leaves $7 and Acceleration 1.4; the next Right errors.
- ba4: Triangle, Return to Game, "Save progress?" No, the world load. `runs/peak1-lodge-attrs` is its neutral-pad ride from the
  state before tick 0 (`lodge/attrs/bought-t0.p2s`).

**Gameplay check.** The bought ride's speed limit is 2219.7 against 2207.1 (peak1-green-start, all 1.0) from tick 0. The port
(`compare-ps2-capture.mjs --peak-fresh --attributes 10,10,5,5,55,55,10`) is exact through 390, where the PS2 takes the lodge
walk-in (391, 3 ticks sooner than the 1.0 rider); without `--attributes` it leaves at tick 1. Gate `peak1-lodge-attrs`.

**Port (pv `buyAttribs`, on).**
- `web/buy-attribs.js`: `BuyAttribSession` (the rules above), `BuyAttribs` (33buyattribs drawn by the LUI player with the runtime
  fields, `hl left` / `hl right`, the popup through `fe-screens.js drawPrompt` over the popup veil), `runAttributes`.
- `web/career.js`: `attributePointCost` / `buyAttributePoint` (0x150C20); `attributeCost` reads a 0.2-step byte by its level.
- `web/career-ui.js`: the lodge's item 3 opens the session; `ctm-attributes` keys, choose, back, items (Yes / No under the popup),
  layout and draw go to it.
- `web/main.js startRun`: every career run (free ride included) sets the career record's bytes (`ui.cb.attributes`), online the
  defaults. Before, only event rounds did (`career-ui.js play`), so free ride kept whatever the core last had.
- Tests: `web/test-buy-attribs.mjs` (the table, the ba1..ba3 values, the display rules, the LUI fields), `test-ps2-captures.mjs`
  `peak1-lodge-attrs`.
- **Transitions (closed 2026-09-28).** Triangle's `0x1F4B64` plays 33buyattribs' `TransitionOut` (label frame 75), whose control record
  `0x30 ... 0x00AB3C45` starts the `transition_flash` screen (anim `02ed6393`: a white quad, A 0 → 255 in 10 frames, → 0 in 9); the
  state switch lands at full white, and the lodge (28lodge) replays its intro under the fading white, Buy Attributes focused. The lodge's
  Cross on Buy Attributes is the same with 28lodge's `TransitionOut` (frame 85). PS2 `ba5` (named saves every 2-6 frames; PINE saves
  stall while the screen loads, so the window took three runs): Triangle at sample 22, white 0.19 / 0.4 / 0.6 at 24 / 26 / 28, full at
  32, 0.79 at 34, 0.39 at 38, the lodge's intro (no menu yet) at 41, the lodge at rest by 59. Port (`BuyAttribs.transition`,
  `drawFlash` after the lodge / this screen in `career-ui.js draw`; no input meanwhile), frame means with a frozen clock at t = 2 / 4 / 6 /
  10 / 12 / 16 / 19: Chrome 183.3 / 201.2 / 219.2 / 255 / 236.3 / 199.4 / 171.6, WebKit the same to 0.2, PS2 181.7 / 200.1 / 217.7 /
  253.8 / 236.4 / 200.8 / 175.0 (the last is the lodge intro under it). Entry: Chrome 188.3 .. 255 .. 173.9, WebKit equal.
- **The popup's veil (closed 2026-09-28).** The PS2 draws UITRICKBUY with FE.LUI `139buy_popup` (`0x1CAC30`); its veil `0885e124`
  (layer 13, anim `0f5016c0`) is a 642 × 481 quad, top (111,177,210) → bottom (200,225,238), vertex A and element A both animated
  0 → 175 over 8 frames. A joint fit over the PS2 frames outside the box (ba1 speed-hold → popup; lodge l1 uber-list → l2 uber-buy-popup,
  the Ubertrick buy popup, the same screen) gives A 0.681 / 0.678 = 175 >> 1 (87/128) and top (109.5,175.5,208.6), bottom
  (199.6,224.3,237.2): the LUI quad, drawn once. The port's buy popup (`fe-screens.js drawPrompt`, FE.LUI `popup`) has the identical
  quad (`0885e124`, anim `00438b41`) but the LUI player multiplied the element A into the vertex A (0.686² = 0.47); with pv
  `buyAttribs` the buy popups (`p.buy`: this one, Ubertrick, Rewards) draw it at the vertex A only. My first version had added the
  lodge popups' flat veil on top (0.83 in total). MAD outside the box against the PS2 popup frame: 13.41 → 6.29 (Chrome), 13.25 → 6.14
  (WebKit), equal to the screen without the popup (6.02); the whole frame 16.19 → 11.7.
- **The popup itself: 139buy_popup (2026-09-28).** `web/buy-popup.js` draws cUIStateBuyPopup's own screen (exported by
  `tools/export_character_select.py`, `139buy_popup` 0x09EA9440) for every buy popup (Buy Attributes, Ubertrick, Rewards; fe-screens.js
  `drawPrompt` with `p.buy`, pv `buyAttribs`; without the screen in the asset the old `popup` fit stays):
  - the timeline from the Cross: frame 1 the five box shapes grow from 20 % over 25 frames (their scale props: `shapeScale`) and the
    veil fades in over 8; frame 25 the texts fade in over 5; frame 30 the stop; then the focused item's frame (Yes 35, No 40: white
    text, the Cross icon group at y 273 / 290);
  - the widgets as `0x1CAC98` fills them: 'item type', 'item name' (the only wrapping text), 'question', 'cost_answer', 'you
    have_answer'; 'group credit' hidden (a song credit would hide 'group cash');
  - every shape at its vertex alpha only (box 150 / 200, veil 175): the PS2 box interior is opaque (fit over ba1: A 0.99..1.00;
    the port 0.99..1.00);
  - drawn in the UI canvas's own frame: the FE screens call drawPrompt inside their LUI scale (the rewards room inside two), which the
    old fit's constants had absorbed.
  PS2 check, MAD against the frames (Chrome / WebKit): Buy Attributes at rest (ba1 popup) box 3.64 / 3.61, frame 2.88 / 2.85 (was
  box 19.7); No focused (ba1 popup-down) 2.88 / 2.82; the Ubertrick popup (lodge l2 uber-buy-popup) box 4.15 / 4.08, frame
  3.24 / 2.99 (was box 11.1, frame 14.9). The intro (ba6: Cross at sample 11, saves at 12..47; the veil 0.26 / 0.44 / 0.62 / full at
  15 / 17 / 19 / 21 = popup frame 0 at 12): port t = 3 / 5 / 7 / 9 / 13 / 16 / 20 / 26 / 35 against PS2 12 + t, frame MAD 6.2 / 4.9 /
  3.5 / 2.9 / 2.8 / 2.6 / 2.5 / 2.4 / 2.9 (WebKit within 0.1); shifted by ±2 frames it rises to 12-23, so the timing is the PS2's.
  The Rewards popup sits where l11 cheat-buy-popup has it. Not checked: input during the popup's intro (the port takes it at once).
- The port saves the career on Yes (the port's autosave, as for every purchase); the lodge's Save Game / "Save progress?" still
  write as before.

### Rewards shop

The catalog is `DATA/BE/RWRDPS2.DAT`, parsed by `0x15A818`:

| Category | Items |
|---|---|
| Art | 100 |
| Posters | 43 |
| Toys | 28 |
| Trading cards | 116 |
| Cheat characters | 20 |
| Videos | 2 |

Rules:
- Each item has an original price. It is sold only in its own peak's lodge; elsewhere the help reads "Buy this item in Peak N lodge.".
- Ownership is stored per character; the PS2 bits are at +0xF30..0xF57.

Screens: Lodge → Rider Details → Rewards, then a category, an item list, "Buy item?", and viewing the picture. Trophies shows the 12 trophy pictures, one per completed peak goal.

`tools/export_career.py` exports the reward pictures from `DATA/CHAR/RWRDPS2.BIG` to `CAREER/REWARDS/`. Its SHPS decoder undoes the PSMT8 swizzle marked by flag `0x2000`.

### Awards (`0x159CD0`)

| Award | Trigger | Grant |
|---|---|---|
| 0–4 | conquer / all goals / all goals of Peak 1, 2, 3 | cheat characters Far East Myth, Gutless, Jurgen, Snowballs, NW Legend |
| 5–16 | peak goal (`5 + goal*3 + peak`) | Peak 1: 4 random cards + 1 poster. Peak 2: 1 toy + 2 art. Peak 3: a cheat character (Unknown Rider / Churchill / Canhuck / Cudmore) |
| 17–19 | first gold or platinum in a standard event of that peak (`0x1591E8`); these ids repeat | a random unowned gear item from that rider's peak pool (flag `0x100 << peak`) |
| 20–31 | first gold or platinum in a rival race, rival jam, peak race or peak jam | a random unowned item from the rider's special board pool (flag `0x800`) |

- Random picks (`0x157080`) take any unowned item in the category, whatever its price or peak.
- Collection bonuses unlock cheat characters: all cards give Hiro, all toys Svelte Luther, all posters Stretch, all art Bunny San.
- Awards are one-shot (bits at +0xF28), except 17–19.
- The awards granted by a result are listed on the medal card.

### Gear (`DATA/CHAR/BOLTPS2.DAT`)

`tools/export_lodge_shop.py` writes `CAREER/shop.json`. It regenerates the helper research and checks every code immediate it relies on.

The data (4829 entries):
- per rider: items, menu tree, prices (×10), and the lodge tier (sold in the Peak 1/2/3 lodge, or reward pools 11–13/14);
- equip rules (7327);
- default outfits.

`web/lodge.js` ports the logic:

| Function | Address |
|---|---|
| inventory init | `0x1513B8` |
| equip | `0x151C90`, with rules at `0x151EF0` |
| buy (includes bundled items) | `0x14B560` |
| Buy Gear list | `0x19B180` / `0x19B098` |

Verification:
- For all 10 riders, the starting owned/equipped flags equal those in the fresh-career lodge savestate.
- The Zoe Peak 1 Buy Gear menus equal the PS2 frames: root, Head, Hats (Peacekeeper $50,000), and Boards (Stuff/Element/Stuff II/dnL $1,000).

Lodge → Buy Gear and Equip Gear share the list walker. Square toggles between them, as the PS2's "□ Equip Gear" does.

Equipped gear is drawn on the rider (docs/characters.md "Equip Gear and outfits": the outfit package is assembled from the
committed rows). In play it follows the profile, see "In play" below.

### Uber tricks (table `0x45AEB8`, lists read by `0x14FF90`)

- Setup rows are Mute (category 1), Indy (3), Stalefish (2), Method (0), Nose Grab (4) and Tail Grab (9).
- Prices: $10,000 / $15,000 / $30,000.
- Starting state comes from per-rider `0x150558` and matches the savestate bytes. Zoe's Mute row is Bar Hop owned, with SSXorcist, dnL FlipIt and Katana for sale.
- Buying (`0x184F40` → `0x14FE08`) clears the lock bit. Selecting writes the entry (`0x14FD80`).

`career.uberSelection(riderId)` returns the chosen trick ids per category (for example SSXorcist is `[129, 179]`). The
selection reaches the trick system: see "In play" below.

### Songs

- The list is the 35 `MUSIC.INF` entries with `ADDTOFE=1`.
- The first 6 are bought with free credits, then $5,000 each (`0x158558` / `0x1988D8`).
- Lodge → Music → Edit Playlist buys songs (the 140audio / 16radio screens of web/audio-menu.js, docs/audio-menus.md).
- Radio BIG plays all 35 songs (PLAYLIST.INF `[SSX Mix]`, owned or not). A bought song joins the rider's custom
  playlist, which Custom Playlist [DJ] / [No DJ] play (web/game-audio.js, the Pathfinder player). See "In play" below.

### In play (2026-09-27, lodge agent)

The PS2 keeps none of the lodge's choices in the race's own state: every world load re-reads the profile record, and the
uber rows are read in the air. Recorded on ARMSX2 in `local/ps2-capture/lodge/` (scripts `scripts/l1..l10`, derived from
`menus/ctm/state-lodge-peak1` with its menu-capture hook removed: `states/lodge-clean.p2s`; cash poked to $200,000; the
facts of each named state in `runs/<run>/`):

| run | PS2 | fact |
|---|---|---|
| l1 / l2 | Rider Details → Ubertrick Setup → Nose Grab → Madonna → "Buy this ubertrick?" Yes → Cross | $10,000 paid; lock mask 0x3FC → 0x3F8; the selection byte, the save's R+0xBE6+4*6+1 and the runtime **0x5316D1**, = 2 |
| l3 / l4 | Rider Details → Player Name: "PLAYER 1" (caret on the 1, Done focused), 'a' x 3, Done | the slot name 0x534FE0 = "PLAYER a" (8 characters at most, a letter at the limit replaces the last one) |
| l5 / l6 | Music → Edit Playlist → two songs with free credits → Triangle | owned R+0xF70 = 0x3, playlist R+0xF78 = 0x3 (written at the exit) |
| l7 | Music → Custom Playlist [DJ] | radio mode R+0xF80 = 1 |
| l7 / l8 | Buy Gear → Boards → Stuff ($1,000) → Square → Equip Gear → Stuff | the Stuff board equipped (buying does not equip) |
| l9 / l10 | Return to Game → "Save progress?" No | the rider rides out of Green Base Station on the Stuff board; audio+0x608C = 1, custom mask audio+0x6098 = 0x3 |
| name-records | the ctm-left final-top race with the slot name poked to "OWEN" | Top 5 Record Times lists OWEN (0x154DDC copies 0x147170) |
| (earlier) | Rider Details → Career Highlights, Rider Profile (`menus/lodge/24`, `30`) | both live in the lodge |

The monster-stoneage / monster-swollen / monster-xexec capture gates (test-ps2-captures.mjs) perform the uber of a poked
selection byte in the air: 0x14FEA8 reads 0x530EC0 + char*0x1FE + slot*6 + (tier >= 5) live, set 1 of the browser grab
profile.

What the browser did not do, and the fixes (all behind web/pv-flags.js switches, now on):

- **pv `careerRider`** (web/career-rider.js, main.js, web/character-roster.js, web/wardrobe.js `outfitStamp`): the rider
  entry main.js loads carries the uber rows of the profile's Ubertrick Setup selection (`uberRows`: web/lodge.js
  `uberChoiceRows`, rows only where the selection differs from the rider's package) and the outfit of the current mode's
  record, plus a stamp of what it was resolved from (play mode, gear record, uber selection). A stale entry is resolved
  again in `ensureRider` (event loads) and `startRun`, and the lodge exit (`ui.cb.refreshRider`) loads it under
  118loadoutlodge, as the PS2's lodge exit is a world load. An uber-only change re-initialises the rider on the same model.
  - Before: the selection reached the grab profile only through the front end's Ubertrick Setup, in memory, and main.js
    kept the loaded rider when only the ubers changed (same id / outfit), so the lodge selection, a page reload or a
    career load played the package defaults. The career's world load kept the rider resolved before Conquer the
    Mountain started (the free-play outfit), and the lodge's Equip Gear and Return to Game resumed the ride without
    loading the new outfit: the equipped gear appeared only at the next event.
  - Online races keep the package rows (the lobby profile has no selection). Cheat skins use their base rider's record and
    keep their own Nose / Tail Grab overrides.
- **pv `lodgeDetails`** (web/lodge-ui.js, web/fe-screens.js): the lodge's Rider Details has every item but Cheat
  Characters (no cheat owned) live, and Up / Down skip the greyed one (PS2 l1). Ubertrick Setup is the 66ut_btnmap screen
  (the front end's, with the rider on the board in FE_A_CYC) with buying: the popup 0x1854A0 (Ubertrick / name / Cost /
  You have / "Buy this ubertrick?", Yes focused) laid out on the PS2 frame, "This trick is available to buy." / "Save
  more cash to buy this trick."; Player Name is the keyboard over the screen (0x1F45C4); Rider Profile is 14rid_prof;
  Career Highlights was already ported but greyed. The lodge's Ubertrick Setup and Equip Gear animate their preview while
  the free ride is paused under the lodge (main.js). Before: a canvas-drawn Ubertrick Setup, and the rest greyed.
- **pv `playerName`** (web/fe-screens.js, web/career.js): "PLAYER 1" by default (0x147170: kT_MEMPlayerName "PLAYER %d"),
  8 characters (1CD088(kb, 8)), the caret on the last one when the name is full and a letter there replaces it (1CD2F0);
  the name is the records' player entry (`Career.playerName`). Before: "PLAYER1", 15 characters, records always "PLAYER 1".
- **pv `riderMusic`** (web/audio-menu.js, web/game-audio.js): the radio mode and custom playlist are the rider's
  (`songState(id).radioMode` / `.playlist`, the R+0xF80 / R+0xF78 of 1587F8 and 158848); every world load (2867E8 →
  `worldLoaded`) applies the human's before the song is picked. A record from before keeps the global mode it was played
  with; a custom mode needs a playlist (0x196B08). Before: one global radio mode and playlist for every rider.

Checked in Chrome and WebKit (the scratch driver drives WKWebView frames by timers when its window is hidden): a seeded
career (Zoe: Stuff, Bandito, Wicked Spex; Madonna / dnL BOOST; three songs) enters Conquer the Mountain wearing the outfit,
with set-1 rows 138 (Nose Grab) and 118 (Indy); the lodge's Ubertrick Setup buys and selects Vacation / Footloose with the
PS2 popup, Equip Gear drops the beanie, and Return to Game rides out with the new outfit and rows; Player Name gives
"PLAYER a"; Music keeps Custom Playlist [DJ] on Zoe's record; a Single Event picks from the rider's settings.
`web/test-career-rider.mjs` (npm test) covers the four switches against the PS2 numbers above.

#### Round 2 (2026-09-27): Cheat Characters, the Rewards room, the Transport lists, the rival challenges

PS2 runs (the same ladder, `local/ps2-capture/lodge/runs/l11..l13`, from `states/lodge-clean.p2s` with cash $200,000):

| run | PS2 | fact |
|---|---|---|
| l11 | Rider Details → Rewards (the rewards room) → Cheat Characters (page 1/1: '$' on Brodi, JP and Marisol, the Peak 1 ones, '?' on the rest) → Brodi → "Cheat Character: / Brodi / Cost: $20,000 / You have: $200,000 / Buy item?" (Yes focused) → Yes | owned, cash $180,000, help "Select this rider in the Cheat Character screen." |
| l12 | Rider Details: Cheat Characters is live now → "Select Cheat Character" (Zoe, Brodi; no arrows with two faces) → Brodi | the setup slot 0x535B20 +0x12 = 10 (+0x11 stays 4 = Zoe); nothing is written to the profile record; Rider Details still reads "Zoe" |
| l13 | Return to Game → "Save progress?" No | the rider rides out of Green Base Station as Brodi, on Zoe's career (cash, collectibles) |

- **pv `lodgeCheats`** (web/lodge-ui.js, web/character-select.js, main.js `careerId`, web/game-tick.js): Cheat Characters in
  the lodge's Rider Details is live once the base rider owns a cheat character (`canOpenCheats`), opens the 131cheat_char
  list over the screen (its arrows only when the faces scroll), and the pick is the career rider: the skin on its base
  rider's gameplay with the Conquer the Mountain scale (web/character-roster.js `composeCheat` career), loaded under
  118loadoutlodge. The career block stays the base rider's: the collectibles, the free-ride cash, the Big Challenges and
  the HUD read `careerId()` (a cheat skin's `base`), where they used to read the skin's id (a new, empty record: $0).
- **pv `lodgeRewards`** (web/fe-options.js `lodge` / `askBuy`, web/fe-screens.js): the lodge's Rewards is the rewards room
  (128rewardsroom / 129rewardgallery / 130rewardposter, the front end's) that sells at the lodge's peak: the slot's own '$'
  for an item for sale here, the career's help lines ("This item is for sale." ...), and the buy popup laid out as the
  PS2's (the same box as the Ubertrick one). The category title stays on one line (flags without 0x80). Before: a
  canvas-drawn list over black.
- **Transport freeride lists** (checked): the three peaks' lists are the table 0x478D38 (peak * 0x360 + row * 0x6C: +0
  course, +0xC station) in order: Peak 1 Happiness, Green Station, R&B, Snow Jam, Crow's Nest, Blue Station,
  Metro-City, The Junction; Peak 2 Yellow Station, Ruthless Ridge, ...; Peak 3 The Throne, Black Station, Gravitude,
  Much-2-Much, Perpendiculous, Kick Doubt (rows 6 and 7 are course 23, none). They equal the browser's and the PS2 frames
  (menus/ctm/53-freeride-list, ctm-parity peak2-arr, peak3/nav/out-fr-to-*/list). The old "order differs" note was stale.
- **Rival challenges** (checked against the PS2 ready states and the capture gates): Happiness, Ruthless and The Throne
  (Race / Jam) play with their rivals; test-ps2-captures bc/bc-race-*, peak2/dbc2-race-tuck, peak3/the-throne-* and
  web/test-rival-page.mjs (the Single Event page path) hold them exact as far as documented (backcountry.md, peak2.md,
  peak3.md; Ruthless 3911 is fixed in round 3 below). One difference was found and fixed:
  **pv `rivalCard`** (main.js `readyView`, web/career-ui.js `begin`): the card of a rival challenge was drawn over black,
  where the PS2 draws it over the ready state (WS2 at race tick 0: the riders at their start spots, the chase camera
  settled behind them; `local/reference/pcsx2/{happiness-mac,ruthless,the-throne}-ready`). readyView places the run as
  startRun does, poses the rider with an animation step of 0, settles the camera behind it, puts the game / visual RNG
  words back, and the Continue starts the run from scratch: test-rival-page stays exact with it (all four cases). The
  rival rider and the card's own camera are round 3 below (pv `rivalCardAi`). The headline wrap went to the
  visual-parity agent (web/results-lui.js).
- **Round 3 (2026-09-27): the rival under the card, the ready camera, Ruthless 3911.**
  - **pv `rivalCardAi`** (web/ai-race.js `readyPose`, main.js `readyView`): the card shows the computer rider at its start
    spot. `readyPose` runs `racers.start()` only: the placement, without `start()`'s relationship ageing, saves or race
    notes. It poses each rider with an animation step of 0, puts every rider's game / visual RNG words back, and
    captures the renderer. The PS2 ready states hold these positions: happiness-ready Mac (2082.2, -46250.5, 96310.4),
    ruthless-ready Nate (47610.7, -244601.6, 278176.2), the-throne-ready Psymon (-360496.8, 319284.7, 739797.4).
  - **The card's camera is the ready state's.** WS2 does not step the camera while the card is up: a PS2 run from
    happiness-ready (local/ps2-capture/lodge/runs/rc1) keeps the same eye (1717.0, -46087.2, 96572.2) for 900 frames.
    The core now exports `camera_event_seed_view`: the event camera seed's compositor eye (outer+0x100) and look-at
    (outer+0xE0). `readyView` shows that view. Before, it settled the chase camera, which lands about 60 cm away. With the
    seed view, Mac's board nose is at the right edge on Happiness, as on the PS2. Nate and Psymon stand beside the
    camera, off screen, on both. Checked in Chrome and WebKit at 4:3 (WebKit's default is widescreen on a wide display).
  - **Continue is unchanged.** test-rival-page is exact on all four cases with the switch on and off. On the live core
    it gives 3910 / 6425 / 3932 before the fix below, and 4435 / 5141 / 4581 on the rebuilt core.
  - **Ruthless 3911** (web/core.cpp, web/animation_bridge.cpp): the PS2's 0x13F178 (the ground-motion post stage), when
    not departing, checks the rider's surface +0x438. Surface 18 is a hard crash right away, 10EB30(rider, 360, 0, 18,
    {+0x110, unit +0x1E0, +0x370, closing 0}), before its 13F488 / 105398 / 107888 and the 13F358 clamp. Only the other
    branch is the 116120 reset (a table+0x44 surface or patch flag 2), and that was the only branch ported. At 3910 the
    soft-collision slide (control 3) reaches patch 0x4DB1E, which is surface 18. The capture's 105D98 hook count does not
    move, so no collision event is involved. The 88 cm jump at 3911 is the crash entry's posed root.
    - peak2/dbc2-race-tuck is now exact through 4435 for the human (was 3910), including the crash, the reset at 4120
      and the second crash at 4341. The RNG is exact through 4581 (was 3932) and the pair records through 4439.
    - Nate is exact on 6982 of 7000 ticks (was 6425) and never 1 cm off. His provider words differ from the PS2 from
      5004 in both builds, so his first inexact tick is now a 1-ulp velocity at 5142 (the gate reads 5141).
    - Only three captures ever put the human on surface 18 (dbc2-race-tuck, dbc2-full, peak3/for-peak2/fr-throne-neutral)
      and no computer rider touches it.
- **Race placement** (checked): every race course has its computer riders (npc-riders.json for ARA1, BRA2, CRA3, DRA4,
  ERA5; rivals.json for the three backcountries), and `ui.cb.standings` gives the six rows the results rank (Gravitude:
  Nate, Allegra, Moby, Mac, Luther; Ruthless Ridge and Intimidator likewise).


## Gaps (not ported)

- **Free-ride world** (checked 2026-09-27): Conquer the Mountain starts in the streamed world (ctm-flow.md, ctm-parity.md);
  events are ridden into at their gates, a station's door asks "Would you like to enter the lodge?", and the MCOMM's
  Return / Transport / Session / Messages / Audio / Options / Quit are all live. Collectibles and Big Challenges are earned
  there and complete the Freeride (EXPLORE) goal. The Transport freeride lists follow the table 0x478D38 on all three
  peaks (checked 2026-09-27, "Round 2" above).
- **Peak challenges** (checked 2026-09-27): the rival challenges of all three peaks are playable and gated against PS2
  captures, and their cards lie over the ready state with the rival and the ready camera (pv `rivalCard`,
  `rivalCardAi`); the peak runs (Peak 1 / 2 Race and Jam, All Peak Race / Jam) are peak3.md section 6. Ruthless 3911
  (the surface-18 crash) is fixed ("Round 3" above).
  - Open: Ruthless 4436, where the human's reset out of the second crash comes one tick late. After the crash detaches
    the board (crash tick 22), the browser's rider +0x160 / +0x170 / +0x180 frame jumps to the detached board's world
    rotation. The PS2 keeps the entry frame on that tick and then turns it smoothly. That +0x180 is the `up` of the
    1057B8 instance response, whose gain feeds the air-bounce counter +0x3F4 (web 4.94, PS2 5.17 at 4435, reset above 5).
    It needs 11EB98 / 136D40's detached-board frame (crash-motion.md).
  - Open: the card headline's wrap (visual-parity agent).
- **Race placement** (checked 2026-09-27): connected on every race course; see "Round 2".
- **Lodge** (2026-09-27, "In play" above, PS2 local/ps2-capture/lodge): ported and checked in play.
  - The equipped gear is worn in races and in free ride, also right after the lodge's Return to Game.
  - The Ubertrick Setup selection (lodge or front end) is the uber performed at tier >= 5.
  - Bought songs join the rider's custom playlist; the radio mode and the playlist are per rider.
  - Player Name sets the records' name.
  - The lodge's Rider Details has Rider Profile and Career Highlights; the highlights show the stat-medal tiers +0xBB8
    (web/monster-tricks.js 0x155390 / 0x155420, recorded at each career event's end).
  - The Message Center (inbox, FAQ folders, the HUD mail icon) is web/career-messages.js.
  - (2026-09-27, round 2) Cheat Characters in the lodge (pv `lodgeCheats`): live once a cheat character is owned, and the
    pick is the career rider in play; the lodge's Rewards is the rewards room with buying (pv `lodgeRewards`).
  - Still open: online races keep the package's uber rows, because the lobby profile carries no selection. The pick of a
    cheat skin lives in the setup slot (0x535B20 +0x12), not in the profile record (PS2 l12); the browser keeps it for
    the page session and does not save it.
- **Not reproduced:**
  - (2026-09-27, pv `transportMap`, ctm-parity.md "Transport map, MCOMM badge, freestyle heat results") The Transport map's
    routes, start indicator, station icons, peak outline, "You are here" and locks are drawn from the Map LUI.
  - (2026-09-27) The MCOMM icons and temperature badge are the OV.LUI ones (ctm-pda); pv `mcommIcons` removes the canvas
    seams in the icons and sets the temperature when the PDA opens, as 0x20A778 does.
  - (2026-09-27, pv `fsStandings`) The CTM freestyle heat results are 42freestyle_standings with Heat 1 / Heat 2 / Total and
    the pulsing human row. The HUD standings' player row (name over the red box) is pv `hudStandings` (visual-parity.md).
  - (2026-09-27, pv `attract` / `bootMovies`, [intro-movies.md](intro-movies.md)) The title's idle attract (intro.mpc after
    1801 frames) and the power-on EA SPORTS BIG / THX / DJ intro.
  - (2026-09-26, pv `replay`, [replay.md](replay.md)) The replay of the run behind the post-race screens and the Replay item
    are ported: the run re-simulated from its start on the same core with its recorded pad, the PS2's replay cameras
    (Web-cam triggers), the full replay's '64replay' overlay. Online races keep the finish camera.
  - Save replay (no memory card; greyed in the Replay Menu).
- **RNG:** the original game RNG is shared with gameplay, so posted-score jitter uses a saved xorshift stream.
  The formula matches exactly; the particular draws do not.
- **Unresolved data:** `DATA/BE/{STATDB,GLOBAL,MODECHAL,BEHILOC}.DBL` are not referenced by name in the ELF and
  remain unresolved. The in-game medal thresholds are placement-based; the "Gold medal at %d" strings belong to
  the Freeride goal screen.
