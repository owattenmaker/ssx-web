# Conquer the Mountain: the career flow against the PS2 disc (2026-09-25)

The whole Conquer the Mountain (CTM) career flow was recorded on the original (SLUS_207.72 in ARMSX2, derived savestates
only). The logic was then traced in the recompiled code (gp = 0x4A30F0), and the browser port was driven through the same
steps and compared with it. This document has the recorded flow, the code addresses, the difference checklist (fixed or
remaining, with the reason), and the regression test. [ctm-flow.md](ctm-flow.md) covers the in-world event start (the
gate, fly-over, approach, card and countdown); this document covers the career around it.

## Tools and the savestate ladder

| tool | what it does |
|---|---|
| `tools/ps2_navigate.py build BASE.p2s SCRIPT.json HOOKED.p2s` | pad script into a derived state (the existing navigator) |
| `tools/ctm_flow_capture.py HOOKED.p2s OUTDIR --frames N --snap-every K [--save NAME@SAMPLE\|@wsN] [--poke JSON]` | runs ARMSX2 and saves, for every snapshot, the PNG and the CTM facts of the same savestate into `navigate.json`. The facts are the world state, course, NIS lists and playing script, and the career block of player 0 (`tools/ctm_flow_state.py`). `MEMCARD=file` inserts a memory card. |
| `tools/ctm_flow_trace.py` | condenses the runs into `web/ctm-flow-ps2.json`, the PS2 trace the regression test compares with |
| `tools/export_ctm_screens.py` | exports the OV.LUI PDA screens to `UI/ctm-screens.json` (in `npm run setup`) |

The runs, derived states, pad scripts and side-by-side sheets are in `local/ps2-capture/ctm-parity/`: `runs/<run>/`,
`states/`, `scripts/` and `sbs/`. Each run starts from the named state of the run before it.

| run | base -> what |
|---|---|
| `new-career` | `menus/ctm/state-select-character-zoe` (fresh profile, cleaned) -> Cross, neutral pad |
| `quit-ctm`, `quit-save`, `quit-save-tri`, `quit-save-mc` | free ride at Happiness -> MCOMM Quit -> Yes -> save -> (no card / card) -> title |
| `reenter` | the title -> CTM -> Zoe again |
| `start-lodge` | `reenter/sel` with the new-career flag poked to 0 |
| `race-q` | the Snow Jam card (`cs/ps2b/intro/static.p2s`) with the computer riders poked DNF (+0x480 = 1) |
| `to-final` | the qualifier results -> Next heat, with GMM+0x70 poked to 3 |
| `race-f`, `race-f95` | the final card -> 1st; `race-f95` with earned / cash poked to 95,000 |
| `after-final`, `f95-after`, `peak2-arr` | rewards -> results -> Transport -> Peak 2 -> Go to this peak now? -> the Peak 2 first arrival |
| `lodge-return`, `lr-tri`, `lodge-quit` | the lodge door prompt -> lodge -> Return to Game / Quit |
| `explore-info` | `menus/ctm/state-mcomm` (Zoe, menu-capture hook cleaned) with 41 Happiness collectibles and 22 Peak 1 Big Challenges poked completed -> MCOMM Transport: Peak 1 / All Mountain / Freeride goal / freeride list INFO (Square) |
| `sd-goal` | `peak1/bc-sd-lastgate` with 11 more Peak 1 challenges and 40 Happiness collectibles poked: Speed Demon's completion is the 12th challenge and completes the Freeride goal |
| `pc75`, `pc75-up` | `peak1/bc-sj-arrival` with the Point Challenge (75,000) offer queued in C+0x1C4 -> Yes, neutral pad; then from its start: Up held, Cross |
| `dizzy` | `menus/ctm/state-freeride-peak1` (cleaned) with the Dizzy Spells offer queued -> Yes, neutral pad |
| `fr-idle` | the same free ride, neutral pad (no collectible on the way; the BS Rail at s300: the controller's +0xD4 = 0x607) |

## The PS2 career flow as recorded

Samples are port-0 pad samples (about one per frame). WS = world state `S+0x214` (ctm-flow.md section 1.5).

1. **Start.** Main Menu -> Conquer The Mountain -> Select Character: all ten riders are selectable. Each one has its own
   career block (below), and the stats and ranking shown are that rider's. Circle = Load game, Square = Options.
   - Cross does the white flash, then **straight to the game load**: the Basic Controls load screen (GL.LUI 110ctrl_load).
     There is no Setup Character in CTM (`new-career` s38 Cross -> s100 load).
2. **A new career** (block +0 set): the world loads at Happiness. WS10 queues NIS list 1 = [movie 29 ABC1, group 0, group 1],
   all flags 3.
   - The ABC1 movie plays between the 60-line bars with a blinking "Press X to skip" (s650-2325).
   - Then #153 `abc1_heli_arr_midway` fades in from white (s2350), and #163 `heli_arrb_zoe_midwayabc1` shows the rider in the
     plane's door jumping out (s2725-3250). Cross skips the rest of the list.
   - Then WS1 -> 2 -> 3 -> 4 in four ticks: free ride at Happiness, riding. The HUD shows 0/44 and $0.
   - The visited mask gets bit 14 at WS4. **The new-career flag stays set.**
3. **The MCOMM** (Start in free ride; OV.LUI PDATemplate + 31paus_freeride): Return / Transport / Session / Messages / Audio /
   Options / Quit. Each row has its own vector icon, and the focused row is white with its icon animated orange. The help
   text is at the bottom, with Select / Previous. The PDA badge shows the temperature.
   - **Quit** -> "Quit Game" (Yes / **No** focused) -> Yes -> "Save progress before quitting?" (**Yes** focused).
   - Yes -> the Save game screen (121Profilesave_pda). With no card: "Please insert a memory card (PS2) into MEMORY CARD
     slot 1." With an unformatted card: the slot list, then the name keyboard.
   - Then the Hints and Tips load -> **the title screen** (`quit-save-tri` s300), not the main menu.
4. **Coming back** (`reenter`): the title -> Main Menu opens on Conquer The Mountain -> Select Character opens on the rider
   last played -> Cross -> load.
   - With the new-career flag still set, the start is **Happiness again**, riding, with **no cutscene** (Happiness is
     already visited).
   - The flag clears at the **first location crossing** (WS11 enter, s3281, the rider riding down into Green Base Station)
     or at the first event results (WS7 enter).
5. **An existing career** (`start-lodge`, flag 0) starts at the **last lodge** (+0x27C = 17 Green Base Station).
   - The rider rides from the station's first session point. There is no cutscene and no prompt at the load.
   - A neutral pad rides into the lodge door about 400 ticks later -> `lodge_arr3` #148 -> "Would you like to enter the
     lodge?" (Yes focused).
   - Port (2026-09-27): the same ride is bit-exact through tick 393, and the door's program fires at 394, with two switches
     (peak-mountain.md "Station fences" / "Fresh rider at a world start"; gate `peak1-green-start`):
     - `stationFences`: stage builtin 108 kills the race fences. Before, the invisible `fenceCollision_r` line stopped the rider
       at tick 230.
     - `freshRider`: the world load's new rider starts at speed limit 0, route heading 0 and surface 0. Before, the start
       ran 3.5x faster and overshot the door.
6. **An event.** The rider rides into the start gate (ctm-flow.md). Its end is as follows (`race-q`):
   - WS5 FINISH! + time -> a "1st place" panel -> WS7 results "Snow Jam - Race / Qualifier Results". There is no reward
     list, because a qualifier pays nothing.
   - The message is "Congratulations!  You've qualified for the semi final round."
   - Items: **Next heat** / Restart / Replay / Records / Quit.
7. **Next heat** (`to-final`): WS13 queues list 0 [4, 5] and list 1 [19, 20]. Then #150 `gond_inair`, #146
   `gond_inair_zoe` ("Loading..."), #96 `ra_sgb_var2` start hut (final only; Press X to skip), and #73 under "Snow Jam - Race /
   Final Round". When the next round is the final, results item 0 reads **Final Round** (0x1E6420).
8. **The final won** (`race-f`):
   - FINISH! -> "1st place" with the gold hex -> WS12 podium [list 9, flags 1] #60 `win_ps_zoea` -> WS7.
   - Then **the reward list** "Snow Jam - Race / Rewards": "Congratulations!  You've been awarded:", then Cash: $10,000 /
     Gold medal earned / (indented) Accessory (Ripped Jeans), then X Continue.
   - Then "Snow Jam - Race / Final Results" with the gold hex beside the winner's rank and "Congratulations!  You've received
     a Gold medal.". Items: **Transport** / Restart / Replay / Records / Quit.
   - The race level moves up (+0x280). With the earnings goal reached (`race-f95`), the list is: Cash: $10,000 / Earnings
     goal complete! / (indented) Peak 2 pass, Poster (...), four Trading cards / Gold medal earned (+ its gear, below the
     scroll). Up / Down move a white row through the list.
   - Lock word +0x278 bit 12 clears (the Peak 2 pass), and a message is posted: inbox item 147.
9. **Transport after the event** (`after-final`, `f95-after`): there is no `endevent_trans_arr` at Snow Jam. The screen is
   Transport / Select Peak (cursor on Peak 1, "You are here"), with Peak 2 now unlocked.
   - Peak 2 -> "**Go to this peak now?**" (Yes focused) -> Yes -> WS14 #149 `heli_inair` ("Loading...") -> WS11 #122
     `heli_inair_zoe` -> WS10.
   - Then the **DBC2 movie** -> #124 `dbc2_heli_arr` -> #137 `heli_arrb_zoe` (list [30, 16, 17], flags 3) -> WS4 free ride at
     **Ruthless**: 0/44, $106,500, and the **mail icon** for the message posted at the final.
   - The visited mask gets bit 15.
10. **The lodge** (`lodge-return`, `lr-tri`, `lodge-quit`): Yes at the door -> FL.LUI 117loadinlodge -> "Lodge - Peak 1".
    - **Return to Game** -> "Save progress?" (Yes focused) -> the save -> the 118loadoutlodge load -> WS10 at the station ->
      riding from the top. The door prompt comes back only when the rider rides into the door again (s1092).
    - **Quit** -> "Quit to Title screen?" (**Yes** focused) -> "Save progress before quitting?" (Yes) -> the title.

11. **Collectibles** (`explore-info`, `sd-goal`). The HUD counter is the location's (n/N). The Transport INFO views count them:
    - Select Peak -> Peak N (Square): "Peak 1 Goals": Race 0 / 4, Freestyle 0 / 5, **Freeride 2 / 2** (the collectible medal and
      the Big Challenge medal), Earnings 0 / 1; a complete goal's box is ticked.
    - All Mountain: **6%**, Gold Medals 0 / 14, Rival Challenges 0 / 12, **Big Challenges 22 / 88**, **Collectibles 41 / 425**,
      Career Highlights 0 / 24.
    - Select Peak Goal -> Freeride (the map shows the tick; Square): "Peak 1 Freeride Goals / 2/2 Complete", then
      "Challenges Complete: 22/40" / "Gold medal at 32" and "Collectibles: 41/155" / "Silver Medal at 70" (the next medal).
    - Select Freeride Event (no boxes; Square): Run / Event / **Collectibles: 41 / 44** / Big Challenges: 5 / 5 (Green Base
      Station: 0 / 5, N/A; R&B: Slopestyle, 0 / 30, 1 / 8).
    - A collectible or a completed challenge that reaches a peak's first medal while the other medal is there completes the
      Freeride goal at once: the goal award (4 trading cards + a poster, the Peak 2 pass: lock bit 12 cleared, +0xF28 bit 11)
      and inbox item 249 with the mail icon beside the MISSION SUCCESS popup. No reward list in the world, but the award stays
      in the reward record 0x4C3EF0 (the end state: award 11 = {pass 1, poster 1, cards 4}) until world state 10 or the next
      reward list (corrected 2026-09-28, "Awards, the PS2 way" below).
12. **Big Challenges** (`pc75`, `pc75-up`, `dizzy`; the Speed Demon captures in `local/ps2-capture/runs/bigchal`):
    - The offer (overlay 0x1D, OV.LUI 63bc_start): the bevelled panel scales in; "Big Challenge", the challenge name, its
      description, "Accept challenge?", **Yes** focused.
    - Yes -> the rider is placed at the challenge start (1234D0) in the **manual start**: "GO!", then the rider sits at 0 MPH
      (from the first frame: semantic 1, the sitting loop) until the stick is pushed forward (`pc75-up`).
    - HUD: the badge with the challenge number; a goal challenge (Point Challenge) hides the cash and shows the score in its
      place ("GOAL: 75000", "0"); a count / timer challenge keeps the cash ("COUNT: 0 / 5", the clock); a called-trick challenge
      names the trick in red at the bottom centre ("180", Dizzy Spells).
    - Start -> overlay 2 (the MCOMM menu with Return / Restart Challenge / Messages / Audio / Options / Quit Challenge and their
      icons). A failure opens 90bc_fail (Challenge failed / Retry? / Yes / No / Challenge Info on Info).
    - Success: "MISSION SUCCESS" and the cash ($2,000 / 4,000 / 6,000 by peak), the badge turns into the collectible icon, the
      status word gets bit 3 (and the next challenge of the chain opens).

## Code (SLUS_207.72)

| what | where |
|---|---|
| FE controller: state id s0+0xC, jump table 0x460F30 | 0x1A0830 |
| CharSelect done (states 9 / 11), by game type 0x535C11: 1 Quick Play -> Setup (0x1A0AD0), 2 MP -> Setup (0x1A0B80), **0 CTM -> 0x1A0B00**: 144DF0(4), 1451E8(12), course = 145C38 ? 14 : 146D98, gp-0x1F38 = 1, cPreGameLoadScreen | 0x1A0A58 |
| Lodge Return to Game: free ride at 146D98 (the last lodge) | 0x1A11C0 |
| Career block of player 0: 0x4A6CA8 + profile x 0x9B50 + char x 0xF88 (char = byte 0x534FE0+0x1C x player+0x11); ten blocks per profile | 145C38 |
| +0 new-career flag: read 145C38 (1A0B1C, 11D454, 17B1B0); cleared by 145CB0 at **WS11 enter 0x236928** (every crossing or transport) and **WS7 enter 0x236E10** (every CTM results) | 145C38 / 145CB0 |
| +0x27C last lodge: 146D98 read, 146E10 written only at **WS10 enter 0x2356FC** when the course is a station (144CC0) | 146D98 / 146E10 |
| +0xACC visited mask (bit = course): 145D38; set at WS4 after WS10 (s3275: 0x4000) | 145D38 |
| WS10 pre 0x234F40: CTM at a backcountry, not visited -> first visit (+0x14). 0x235080 sub 0: course 14 -> 278E20(movie 29) + groups 0, 1 (flags 3); 15 / 16 -> movie 30 / 31 + groups 16, 17; a visited backcountry reached by a transport (streamer +0x1C8) -> groups 16, 17; a world load at a visited one -> nothing | 0x235080 |
| Results item 0: Transport (2) when the event is over or failed, else 1 'Next heat', relabelled kT_OVRCMNFinalRound 'Final Round' when GMM+0x70 == 3 | 0x1E6310 / 0x1E6420 |
| Reward list overlay 0x10 when the result granted anything (158F30), before the results (0x20CF1C): OV.LUI 62reward_list, line by line: kT_CMNCashEarnedNum, then per award its title (table 0x441C68: 0-4 conquer / all events / peak N conquered, 5-16 goal complete, 17-19 Gold medal earned (kT_REWPlatEarned for platinum), 20-31 rival / peak event complete) and its items indented '      %s' (formats 0x441CE8: trophy, medal, pass, poster, trading card, art, video, toy, cheat character; gear kT_REWAccessory / kT_REWBoardGraphic by the item's 0x800 flag) | 20A8F8 / 0x1FF7B8 |
| FAQ view: 'FAQ %d' (kT_MSGFAQNumber) numbered by 1E2DC0, no Delete (0x1E54A4) | 0x1E5504 |
| PDA frame: PDATemplate (39C870 from frame 0) + bganim1 looping + the temperature '%d°%C' = -10 - (G+0x1C / 3600) % 15 | 0x20A778 / 0x20A854 |
| MCOMM menu cOVTemplate_PauseMenu (31paus_freeride): every "<icon> group" hidden, then row i shows the group of its item id (table 0x441C30: transport, cont, rstart, opt, radio, hex, hex, rstart, hex, hex, session, map, mess) at y = 40 x i | 0x1F8168 / 0x1F8448 |
| Collectibles of a peak: the set bits of every course row (0..21) whose course table +0x54 is the peak (153708 / 1535C0; the row's count byte C+4+12*course is 153520); totals 0x43FA70 summed by 153390 (155 / 148 / 122) | 153708 / 153390 |
| Completed Big Challenges of a peak: status bit 3 of the rows whose course is on the peak (table 0x43EE10) | 1544D0 |
| Medals: the first of [platinum, gold, silver, bronze] the count reaches: collectibles 0x45AFE8 (155/100/70/40, 148/100/80/60, 122/90/80/70), challenges 0x45B018 (40/32/22/12, 27/24/18/12, 21/18/16/12) | 1589B0 / 158A50 |
| Freeride goal (157BF0 goal 2) = both medals. At a collectible (30B9A0 -> 10F338 -> 1599A0) or a completed challenge (307308 -> 10F2D8 -> 159B08): the count reached the session's next threshold (0x440F48 / 0x440F58, set at WS10 by 158E30) -> the goal complete with this medal as the first -> 159170 grants award 11+peak (159CD0) | 1599A0 / 159B08 |
| Transport INFO: Freeride goal panel (kT_CMNPeakFRGoals, kT_CMNChalCompNum, kT_CMNColllectNum, the next medal kT_CMNPlatinumAt / GoldAt / SilverAt / BronzeAt, the tick when a medal is held); All Mountain 20 x (golds/14 + rivals/12 + challenges/88 + collectibles/425 + highlights/24), 100 only when complete, else at most 99 | 0x205098 @0x205DC0 / 0x204D40 |
| WScript trick event: 118FF8 (the trick name of 11A228 / 11A168) -> 309918 kind 3 with the 8-byte record; builtin 78 (3036B0 -> 30AF08, jump table 0x4899D0) reads its fields; builtin 83 (3045B8) packs 16 fields into a record -> 30B520: C+0x34 = 1, C+0x38 = the called trick | 309918 / 30AF08 / 3045B8 |
| Builtin 63 (3034F8): on a rail (122EE8 4) -> (rider+0x77C)+0xD4 = the rail motion +0x24: the rail's packed id (rid << 8 \| track), else -1 | 3034F8 |
| Challenge start 1234D0: 11D660 placement, then 0x12357C writes 2 into the start controller's phase ((rider+0x77C)+0x290) and 11FEC8(rider, 6): the manual start (12BE20 keeps phase 2: wait for the stick) | 0x12357C |

## Differences, and what was done

`[x]` fixed in this work, `[ ]` remaining (with the reason).

- [x] **Select Character in CTM went to Setup Character** (Continue / Equip Gear / Rider Details / Music). The PS2 goes
  straight to the load (0x1A0B00). Now `character-select.js selectDone()` calls `careerUI.enter()` in career mode. Equip
  Gear, Rider Details and Music stay in the lodge, as on the PS2.
- [x] **The new-career flag was cleared at the first world entry.** The PS2 clears it at the first crossing or transport
  (WS11) or the first results (WS7), so a player who quits right after the drop starts at Happiness again. Now
  `courseChanged` (main.js 'course' listener) and `finish()` clear it.
- [x] **The first-visit cutscenes were keyed on that flag** (Happiness) and on `arrived` (Ruthless / The Throne). They now
  use a visited mask like +0xACC (`r.visited`; old saves migrate: a cleared first-run flag -> Happiness visited, `arrived`
  -> Ruthless / The Throne). The bit is set when the ride starts (WS4).
- [x] **The new-career plane scenes never showed.** The ride was started before the arrival list, and the deferred
  `startRun` stopped the cutscene during the movie; that is where the "Cutscene anchor 37 unavailable at undefined"
  warnings came from. Now the arrival list plays first and the ride starts when it ends (WS10 -> WS4). A CTM start is also
  always a world load (`cb.freeRide(course, {reload: true})`).
- [x] **Live-actor anchors (40 + subject)** looked up actor *index* 3 instead of the actor bound to subject 3 (binding 4). So
  the rider-following camera of every `heli_arrb_*` script (#128-137, #154-163) failed (anchor 43). Fixed in
  `cutscenes.js`: #163 now follows the jump.
- [x] **The movies (ABC1 / DBC2 / EBC3) had no bars and no "Press X to skip".** They now play under the UI canvas with the
  NIS letterbox and the blinking prompt.
- [x] **A transport into a visited backcountry** had no heli drop. `free-ride.js transport` now plays [16, 17] (0x235220).
- [x] **The last lodge** was only updated by the port's own world loads. Now every station reached counts (crossing,
  transport, lodge exit), as WS10 does.
- [x] **Results item 0** read "To semi final round" / "To Final Round". It now reads **Next heat** / **Final Round**.
- [x] **The reward screen** was one "Final Results" card with a big medal and prose. It is now the reward list (overlay
  0x10) with the PS2's rows and order, a scrolling white row and Continue. The award items carry their award id
  (`career.js grantAward`). The results after it show the medal message and the medal hex beside the winner.
- [x] **MCOMM Quit** went straight to the main menu. It is now "Quit Game" (No focused) -> "Save progress before
  quitting?" (Yes focused) -> the title screen. Results Quit and the lodge's "Quit to Title screen?" (Yes focused) ask the
  same save question.
  - The browser card saves continuously (web/career-save.js), so No does not roll anything back.
  - The memory-card screens (121Profilesave_pda, the name keyboard, format) and the Hints load are not shown.
- [x] **The main menu** reopens on the item last chosen (PS2: back on Conquer The Mountain).
- [x] **Lodge Return to Game** transported in the world and put the lodge prompt up at once. It is now the station ride-in
  (the rider rides from the top; the prompt comes back only at the door), after "Save progress?" and 118loadoutlodge.
- [x] **FAQ view** showed "Message # FOLDER" with a Delete button. It now shows "FAQ 1" (FAQ numbers) with Previous / Keep
  message only.
- [x] **Mail icon**: messages posted by an event result did not start the HUD icon. The PS2 shows it at the next ride
  (Ruthless after the Snow Jam final).
- [x] **The MCOMM, the career pause and their Yes / No popups** were hand-drawn boxes: a gradient, a white strip and no
  icons. They are now the OV.LUI screens: PDATemplate (bars, MCOMM badge, temperature, mountain) with bganim1, and
  31paus_freeride with the row icons and their focus animations, help and legend. `web/ctm-pda.js` draws them, with the
  exporter `tools/export_ctm_screens.py`.
  - 87yndialog draws Quit Game / Save progress before quitting? / Give Up / Restart / Transport to this area now? / Go to
    this peak now? / Would you like to enter the lodge?.
  - The Message Center, Audio, Session, Transport, Big Challenge and online lobby screens also use `mcommFrame`, so they
    sit in the real PDA frame now.
  - `lui-player.js` applies shape scale props for these screens only (`shapeScale`); other screens are unchanged.
- [x] **The midway plane** in #153 / #163 flies in and hovers (2026-09-26, [presentation.md](presentation.md) 1: the kind-7
  stage calls run ABC1 program 5's LiveComp on SETS/ABC1PLANE; the world's static copy is hidden meanwhile).
- [x] **The heli ride during a peak change or a post-event transport to another location** (2026-09-26, presentation.md 2):
  departure / in-air ride in the current world, then the course switch under the held `heli_inair_<char>` loop, which draws
  itself with "Loading..." until the new world is in (`web/ctm-transport.js`, `cutscenes.js acrossSwitch`).
- [ ] **The black hold after the venue fly-over and no computer riders in it** (ctm-flow.md section 7): the event package
  downloads now start under the fly-over (`?pv=flyover`, off by default; throttled Metro-City 23.7 -> 18.2 s), but the
  load itself (~5.5 s warm) still follows it. Removing it needs the event loaded while the fly-over renders (two worlds
  through the course switch, or the event in the streamed world).
- [ ] **Transport (Map), Session, lodge, results and card screens** (2026-09-27: the Transport's title and map are the Map LUI
  with pv `transportMap`, and the freestyle heats are 42freestyle_standings with pv `fsStandings`, below) still use the port's own layout inside the new PDA
  frame, not OV.LUI Map (0x5380) / 38session / the lodge FE screens / 43final_standings / 40race_pre. Texts, order and
  rules match; the art differs, for example the Transport title icon and the smaller "Go to this peak now?" popup over the
  map.
- [ ] **The memory-card flow** (121Profilesave_pda, format, name entry) and the Hints and Tips FE load: the browser has no
  card. Save writes localStorage at once.
- [ ] **The finish panel of a race** ("1st place" with the medal hex) and the blinking human row in the results are not
  drawn.

### Collectibles and Big Challenges (player reports 2026-09-26)

"There's no collectible tracking, but they do give money", "the collectables ... don't disappear from the map", "Big Challenges
are pretty bricked, but do get tracked and give money".

- [x] **The Freeride goal could never complete.** `career.js goalComplete('freeride')` returned false ("needs the free-ride
  world"). It is now 157BF0 goal 2: a collectible medal and a Big Challenge medal of the peak (`peakCollected`,
  `challengesDone`, `collectMedal`, `challengeMedal` with the exported tables 0x45AFE8 / 0x45B018).
- [x] **Nothing happened at a medal.** A collectible (`markCollected`) or a completed challenge (`big-challenges.js
  challengeStatus`) that gives the peak a new medal and completes the goal grants award 11+peak and the next peak's pass at
  once (`exploreGoal`). The message hook posts 249 / 250 with the mail icon, as in `sd-goal`.
- [x] **Transport INFO showed zeros.** Peak N Goals showed "Freeride 0 / 2", All Mountain showed "Collectibles 0 / 425" and a
  percentage of golds and rivals only, and the Freeride goal panel showed "Collectibles: 0". They now show the PS2's counts,
  ticks, the next-medal lines and the 0x204D40 percentage.
- [x] **The freeride list INFO** showed Medal / Top score rows and no collectibles, the title was blank for backcountry and
  course entries, the list had status boxes, and "Select Freeride Event" ran off the screen. It now shows Run / Event /
  Collectibles / Big Challenges (N/A without), the event type as the title, no boxes, and the title on two lines.
- [x] **Check boxes**: the Transport boxes are white with a dark outline and a black check (the goal list's check was orange).
  Earnings is not greyed.
- [x] **Collected snowflakes coming back** after a location streamed out and in: the core's copy of the career row now takes
  the bit at the award (the pickups agent's change in `stage_script_gameplay.inc`, `web/test-collect-restream.mjs`). The career
  save keeps every bit from the pickup on (`career.js markCollected`, saved at once). `collectStart` hands the saved rows to the
  core at every run start, page reload and peak change. `stage-collect.js collectedIndexes()` gives the saved list indexes to
  anything outside the core.
- [x] **Trick and called-trick challenges could not complete.** The WScript trick event (118FF8 -> 309918, kind 3), builtin
  78 (30AF08: a field of the trick record) and builtin 83 (3045B8: the called trick into C+0x34..+0x3F) were not ported. Nine
  Peak 1 challenges need them: Pop the Kitty 3, Dizzy Spells, Indy Whip, Camel Backs, Seek and Slide I-III, Tight Rope
  Artist and Flip Flop Fun. On Peak 2 and 3 there are more (Big Tricks, Rail Wizardry, Trick Sergeant, ...).
  - Now `score_gameplay.inc score_trick_post` posts the event where the score object names a trick. `mission_gameplay.inc`
    has the two builtins, and `mission_called_trick()` gives the HUD the called trick's name, drawn in red at the bottom centre.
  - Dizzy Spells calls {0x1000, 0} ("180") exactly as the PS2. The called trick's event counts, and the next trick is called
    (`test-big-challenges.mjs` 1b).
- [x] **The challenge start** launched the rider at once. The PS2 sets the start controller's phase to 2 (0x12357C), the
  manual start: the rider waits until the stick is pushed forward. `mission_rider_teleport` now enters phase 2.
- [x] **A goal challenge's HUD** drew the cash over the score. 1EB350 turns the cash off (0x100) while a Big Challenge with a
  goal runs.
- [x] **The challenge overlays**: the offer / info and the failure popups are the OV.LUI screens 63bc_start / 90bc_fail
  (`ctm-pda.js popup`). The challenge pause is the MCOMM menu with its icons (Return, Restart Challenge, Messages, Audio,
  Options, Quit Challenge); it was a white strip and plain text.
- [x] **Builtin 63** (the rail the rider grinds) returned -1: Apply Pressure (Snow Jam) and Missing Masonry (Intimidator) could
  not complete. 3034F8 reads the controller's +0xD4 = the rail motion's +0x24 (owner +0xB0): the rail's packed id (PS2 `fr-idle`:
  0x607 = ABC1_A rail 6 during the BS Rail). The port's `railMotion.railId` is that field.
- [x] **WScript event kind 5** (3099F8 from 11DF18(rider, 1)) was never posted; the Peak 3 Combat challenges and New Line
  Collectibles (The Throne) listen for it. It is now posted at the reset controller's placement (12F398 @0x12F498,
  `animation_bridge.cpp` postPlacement). Event kinds 1 / 2 / 4 have no authored use.
  - [x] The two other 11DF18 callers pass the constant 1 (2026-09-26). 235FD0's is world state 15's enter 0x236058
    (MCOMM Session "Session this area?", a Transport to the current location, the post-event Transport back to the same
    course): 30B7F8 resets every mission, then 11DE60(rider, k + 1, 2) and 11DF18(rider, 1). The port now runs
    `mission_world_session(0)` before and `(1)` after its placement (`web/free-ride.js` placeRegion for `sessionPlacement`
    spawns). 20D1D8's is the pause dialog's action 4 "Quit the event?" (overlay 0x1B), whose only opener is MCOMM item 9.
    None of the five MCOMM lists (0x478140 free ride, 0x478160 event, 0x478178 challenge, 0x478190, 0x4781A8) holds item 9,
    so the retail game never reaches it; nothing to port. A location entry (11D390) passes 0.
- [x] **Free-ride trick cash** (2026-09-26). With 0x535C10 == 4 and 0x535C11 == 0 (Conquer the Mountain free ride, Big
  Challenges included: `pc75` keeps event type 4), 11A228 / 119608 / 117718 pay min(points / 500, 20) through 119EF8 kinds
  0 / 1 / 2 (popups 0x2F / 0x2E / 0x30) instead of the points popups. 119EF8 -> 1597B0 -> 159818 then adds the cash
  (C+0xAC4, +0xAC8). When the earnings reach the first open earnings goal, 159818 grants that goal's award at once (award
  14 + peak - 1, with the next peak's pass), and 1E3760 posts 247 / 248 before.
  - PS2: `runs/peak2/fr-d-glide` 2371 (a 690-point landing posts slot 0x2F with $1, and slot 0x19 shows the cash).
  - Port: `score_environment` sets `pointsToCareer` (`web/stage_script_gameplay.inc` score_points_to_career). The core keeps
    the cash for slot 0x19 (`set_score_career_cash`) and queues the score kinds (`score_career_events`).
    `web/stage-collect.js` scorePoll hands them to `web/career.js` earnCash (159818), which collectibles and Big Challenge
    cash now use too; `web/career-messages.js` wraps earnCash for 1E3760. `web/trick-hud.js` draws case 0x30 (0x1EE690).
  - Gate: fr-d-glide's score is exact through 3141 (it was 2370). ~~At 3142, score +0x168 counts one tick fewer after the
    course switch.~~ (2026-09-26, round-2 agent) The score is exact to the end of the capture on the 10:27 core: 117C28 now
    runs while a crossing has no path bank (docs/peak3.md "Past the crash contacts" 4), which was the missing tick after the
    3134 switch. The gates (fr-d-glide, weather/frd-regions) now require the score to the end.
- [x] **After a success** (2026-09-26): 1EB350 keeps flag 0x400000 (the badge) while the MISSION SUCCESS slot 0x1B lives
  (1.5 s). The challenge no longer runs, so the counter (0x80) is back and drawn over the badge, and the badge's disk
  (+0x4D0) is 0x4C8828 green for a completed challenge (153D78, status bit 3), red before (PS2 `sd-goal` s150..221).
  The big message 0x3A draws "MISSION SUCCESS" and the cash at descriptors 58 / 59, growing 0.4 -> 1 and fading after 0.6
  (0x1F0928). `web/big-challenges.js` drawHud, `web/trick-hud.js` bigMessage.
- [x] **The sitting pose at a manual start** (2026-09-26, round-2 agent): checked, no difference. The PS2 (`runs/dizzy`
  s200 / s400 / end, ticks 1553 / 1753 / 2253, and every sample frame from the GO!) sits from the placement on: control 6,
  motion 3, start phase (owner +0x290) 2, channel 2 requesting semantic 1 = clip 0x2100 `07eaa253`, 2.333 s, kind 2 (a loop):
  the bones repeat every 140 ticks (s200 = end, s400 up to 2.4 cm apart). There is no later "sit-down" idle (the ground
  idle clock runs in control 0 only). The port enters the same (12BE20: phase 2 plays semantic 1, `engine/start_control.hpp`;
  `mission_rider_teleport`), and the browser frames of the Dizzy Spells start (`ctmleft/qa-bc2`) show the same sitting
  rider as PS2 samples 170..900.
- [x] **The race countdown's GO!** (2026-09-26, round-2 agent): world state 3's 234C68 sends HUD command 6 (0x1EC378) on the
  tick the race phase goes 4 -> 5, which starts big message 8 like the challenge start's. `web/game-tick.js` present() now
  calls `trickHud.command(6)` on that tick (race_end [5]: 4 -> 5), before the frame's HUD update advances it; rolling starts
  (3 -> 5) have none.
- [x] **The challenge start** (2026-09-26). The PS2 shows "GO!": 3071F0 -> 10F2B8 -> 11A0E0 posts score slot 0x1A (1 s),
  and the HUD update's pre-pass 0x1EC07C turns a fresh 0x1A into big message 8. That is the sprite owner +0x434 (OV_1-4) at
  descriptor 8, growing 0.4 -> 1 and fading after 0.6 over 60 frames (0x1F0818). The camera is cut behind the rider: 11D660's
  tail and 1234D0's end call director vt+0x24 = 15CCF0, which runs DEFAULT_3 set-target 0x176FE0 on every node. 125038
  only stops the reset fade.
  - The port's rider was not even at the start: `mdl_<LOC>_bcteleport_*` are type-0 instances whose matrix the world
    loader dropped, so 1234D0 read the identity and put the rider at the world origin, in the air.
    `web/world_bridge.cpp` now keeps every instance's matrix.
  - Browser (Dizzy Spells at Happiness, QA on the scratch core): head (2745.3, -41433.1, 94841.1), the PS2's lookAt to 0.1 cm.
    The eye settles at (2566.8, -41364.4, 95036.8), against the PS2's (2569.2, -41365.3, 95034.2) at s200 and
    (2565.6, -41363.9, 95038.1) at s400. GO! is drawn.
- Side by side: `local/ps2-capture/ctm-parity/sbs/sbs-5.png` (Transport INFO: Peak 1 Goals, All Mountain, Freeride goal, the
  freeride list; PS2 top, port below) and `sbs-6.png` (Dizzy Spells HUD with the called "180", the Point Challenge HUD, the
  challenge pause, the failure popup).

### Transport map, MCOMM badge, freestyle heat results (2026-09-27, front-end agent)

Switches are in `web/pv-flags.js`. PS2 frames are in `local/ps2-capture/menus/transport-map/` and the browser pairs in
`local/browser-validation/transport-map/`.

- [x] **Transport map** (pv `transportMap`, `web/ctm-map.js`). The Transport screens were the port's own picture: no routes,
  indicator, outline, "You are here", locks or title icon. They now draw the **Map LUI** (0x5380), the screen FE.LUI and
  OV.LUI share, drawn from the export Single Event already has (`fe-menus.json`). Its sprites are moved to the OV pages at
  the same UVs: FE_1-11 -> OV_1-1, FE_1-14 -> OV_1-2, FE_1-18 -> OV_1-5, FE_1-19 -> OV_1-6.
  - **Title.** The `pdatitle` group: the mountain icon, "Transport" and the sub title, with its intro (frames 5..25).
  - **Map.** The picture (MapPic), the peak tab (PEAKno), "Show INFO" / "Show MAP" (maptab). There is no maptab on All
    Mountain or Earnings (PS2 19, 24).
  - **Select Peak.** The focused peak's outline, "You are here" on the peak being ridden, and the locks. Locked peaks keep
    black text, and Select stays in the legend.
  - **Routes.** The peak's routes, node dots and station icons (`peak<n-1>indicators`) with the red set of 0x206690:
    - Goal list: the courses of the focused goal's list, from the goal records at 0x478225.. (stride 0x6C). On Peak 1,
      Race = ARA1 BRA2 ABC1 and Freestyle = ASS1 ABA1 BHP1 ABC1. Freeride = every route.
    - Event: its course. A peak run: its table (0x4714A0; 0x4A26A8..: Peak 1 Race ABC1 + ARA1 + BRA2, Peak 1 Jam ABC1 +
      ASS1, ...).
  - **Indicator.** The start indicator at the focused course's Map state. A base station's state shows its station icon
    instead (`STATION_INDICATOR`: Green 210, Blue 250, Yellow 350, Red 320, Black 430). This is new: the freeride list had
    no map before. PS2 peak1 `fr-green`, `fr-blue`; peak2 `p2-fr0` (Yellow), `p2-fr5` (Red).
  - Locked events keep black text too (PS2 25, 28).
  - Checked against 30 PS2 frames:
    - Peak 1: Select Peak, goals, the race / freestyle lists, peak runs, the whole freeride list.
    - Peak 2 (from `ctm-parity/runs/peak2-arr/end.p2s`): goals and the freeride and race lists.
  - Chrome and WebKit agree. Routes, dots, indicators and station icons land on the PS2's to about a pixel
    (`fr-cmp.png`, `p2-cmp.png`, `zoom-maps.png`).
  - Peak 3 uses the same data and code, but no PS2 frame was taken.
- [x] **MCOMM icons and badge** (pv `mcommIcons`, `web/ctm-pda.js`). The icons and the temperature were already the
  OV.LUI ones (ctm-pda). Two details remained:
  - The canvas drew each flat icon triangle by triangle, which left anti-aliased seams (lines through the hexagon and the
    arrows). They are now filled as one path per flat shape (lui-player `unionFlat`). Icon strip vs PS2 `16-mcomm-cursor-transport`:
    4.12 before, 4.03 after (Chrome), 3.99 after (WebKit).
  - The temperature was recomputed every frame. The PS2 writes it once, when the PDA template is built (0x20A778 ->
    0x20A854), so it is now set when the PDA opens (`pdaTemperature`).
  - The value itself matches: -10 - (frames / 3600) % 15. It reads -14 °C four minutes after boot.
- [x] **Freestyle heat results** (pv `fsStandings`, `web/fs-standings.js`, `tools/export_fs_standings.py` ->
  `UI/fs-standings.json`).
  - **Before:** the qualifying heats used 43final_standings with one "Score" column.
  - **After:** the PS2 draws **42freestyle_standings** (0x1E5B80 build, 0x1E64C8 show) and so does the port. It sits in
    OV_darkblue, using `results-lui.js`'s panel, which is unchanged.
  - **Columns:** Rank / Riders / Heat 1 / Heat 2 / Total.
    - After heat 1: Heat 2 '- - -' and no Total.
    - After heat 2: both heats and heat 1 + heat 2.
    - After a heat 1 that already qualified for the final (GMM+0x70 = 3): every rider's heat 2 and total, the human's heat 2
      '- - -', and still no "Total:" header.
    - 'DNF' in the given-up heat.
  - **Help:**
    - After heat 1: "You are currently in Nth place.", or kT_CMNHELPEarnedEnoughPoints when qualified.
    - After heat 2: help_advance or help_sorry.
    - After a give up: nothing.
  - **Human row:** the name and the current heat's score pulse orange -> white (`@TextPulseWhiteOrange`, mode 3).
  - PS2 `menus/pipegu/giveup.final` (The Junction, Zoe DNF) matches the browser row for row. PS2 heat 1 / heat 2 runs to
    time up are in `local/ps2-capture/menus/transport-map/heats/` (README there), and match the browser row for row:
    - heat 1: Zoe 2000, 6th, "You are currently in 6th place.", Next heat;
    - heat 2 lost: help_sorry, Transport;
    - heat 2 won (`h2-advance`: the game's own finish with three computer riders' scores poked to 0): help_advance and item 0
      **Final Round**.
  - The menu's item 0 (0x1E6310): a won heat 2 keeps state 1 'Next heat', relabelled 'Final Round' at GMM+0x70 = 3. Only a
    lost heat (or a give up) sets state = round = 2, 'Transport'. The port's `resultItems` already did this.
  - A poked human score (+0x198) is rejected: the heat ends early and the heat score posts 0, though the records keep the
    value (`heats/h2-scorepoke`).
  - The final keeps 43final_standings.
  - **The heat card** (41freestyle_pre, 0x1FBD20 / 0x1FC768), same switch. The port drew its own card with one column; it
    now draws 41freestyle_pre:
    - the round's tab (Heat 1 / Heat 2 / Final Round);
    - objText1 / objText2 with their bullets: the objective lines, the second hidden when there is only one;
    - 'Current standings': the three best posted riders, with Heat 1 / Heat 2 / Total before heat 2. Heat 1 and the final
      show one column, headed 'Score' in the final.
    - 'Up next': the human, with the heat 1 score before heat 2, else '- - -'. p0rider and p0dashes / p0run1 pulse.
    - the record score, and Continue.
  - PS2 `heats/h2.f00300` (heat 2 card: Mac 162880 165440 328320 ... Up next Zoe 2000 - - -) and `menus/rnbctm/zoe-a` (heat 1).
  - The HUD standings' human row (3-letter name over the red box) is the visual-parity agent's pv `hudStandings`.
- Tests: `web/test-ctm-map.mjs` (the red sets against 0x206690's tables, the peak-run and per-peak tables, the station
  states, the override per screen, the badge constants), `web/test-fs-standings.mjs` (every case of 0x1E5B80, the
  strings, the export's widgets and pulse).

## CTM parity audit (2026-09-28, CTM agent)

A whole-career audit against the code (four read-only sub-audits: money, progression, save / messages / relationships,
stations / cutscenes; the notes are in the session scratchpad `ctm/audit-*`). Money matched everywhere (every source and sink,
the four tables byte for byte). The differences, ranked by what a player feels, and what was done (switches in `web/pv-flags.js`):

### Big Challenges: builtin 59 (pv `bcSpeed`, on)

- **PS2:** Kick Doubt's Grinder / Pepper Grinder / Meat Grinder (rows 71-73) run a step program that, while 122EE8 key 4 (on a
  rail) holds, adds stage builtin 59 to a distance, and B75 shows distance / 100 against 250 / 400 / 600. Builtin 59 = 0x3032C0:
  the context's rider (race riders[ctx+0]), its +0x6C0 part's vt+0x14 = 140910 = rider+0x1E0 (the velocity, w 0); VU vmul.xyzw,
  vadday.x, vmaddaz.x, vmaddw.x, vsqrt; mul.s by G+0x14 (0x3C888889 = 1/60, read from the PS2 memory). The VM stores int + float as
  int + cvt.w.s, so the distance grows by trunc(|v| / 60) cm a rail tick.
- **Port:** 59 had no case, so the stage default returned nil and the three challenges could never complete. A static audit of
  all 88 missions (every builtin, stat key and event kind their programs use: `scratchpad ctm/bc_audit.py`) found no other
  missing gameplay builtin (21 is the JS-owned UV scroll).
- **Fix:** `web/mission_gameplay.inc` case 59 (`mission_speed59`, core flag `mission_speed_builtin` set per tick by
  `web/big-challenges.js` from pv `bcSpeed`); test export `mission_test_speed`.
- **PS2 parity:** a derived Kick Doubt free ride (`local/ps2-capture/ctm-parity/grinder/`: the Transport prompt `prompt.p2s` of
  peak3/nav, Yes, the Grinder offer poked into the queue C+0x1C4 at the first WS4 -> the PS2's own "Big Challenge / Grinder"
  prompt, Cross, then `c-grind`: stick up + left 700 frames, per-tick records with the context words and the distance's LUN
  node 0x54390C). The rider grinds from tick 1465; all 212 rail ticks' increments (3042 cm) equal trunc(core builtin 59 of that
  record's velocity), and COUNT = distance / 100 (`web/test-big-challenges.mjs` 1c). In Chrome (scratch core over CDP) the
  COUNT rises on rail_2088 as predicted.
- **Live core:** rebuilt 2026-09-28 08:51 with the physics agent's edits, after the PS2 capture gates passed on the same
  sources (test-ps2-captures 1/1). `bcSpeed` is on.

### Every career entry is a fresh event (pv `freshEvent`, on)

- **PS2:** a gate (builtin 67 -> 22D6C8) always calls cGameModeMan_initGameMode (0x22D89C); it first runs the old handler's
  vt+0x1C = 0x238C80: GMM +0x84 = 1 (fresh), +0x70 = +0x74 = 0, +0x98 = 0. Leaving an event for free ride (the map, 2018A8) swaps
  in the free-ride handler the same way. So every entry is a qualifier / heat 1 with a new roster and new posted scores; only
  Next heat and Restart (no initGameMode) carry the round.
- **PS2 run** `local/ps2-capture/ctm-parity/fresh/` (from `runs/to-final/card.p2s`, the Snow Jam final card): the final started,
  paused, Give Up -> TIME'S UP -> results (round 3, Transport) -> Transport -> Peak 1 -> Race -> Snow Jam -> Yes = WS15 (free
  ride: event type 4, mode 12, round 1) -> 286 neutral + 20 left -> the gate at sample 754 -> WS2: "Snow Jam - Race /
  Qualifier" (round 1), riders Zoe, Psymon, Brodi, Elise, Allegra, Kaori.
- **Port:** `career.js startEvent` reused the saved `r.events[key]`, so a failed final (or a quit semi) resumed there. Now every
  career `begin` is fresh. Chrome and WebKit: a failed final, then `startEvent` again -> "Qualifier Round" (switch off: "Final
  Round"). test-career checks both.

### A page reload during a career (pv `careerReload`, on)

- **Port before:** the world / event URL kept `?course=…&autostart=1`, `save.pending` is gone once a world is in, and
  `ui.careerMode` is only set from the main menu: a reload during a career became a plain run (no collectibles, cash, MCOMM,
  messages), in free ride, mid Big Challenge or mid event.
- **PS2 equivalent:** a reset goes to the title; Conquer the Mountain then starts with the world load at Happiness while the
  new-career flag is set, else at the last lodge (0x1A0B1C..44: `145C38 ? 14 : 146D98`; runs `reenter`, `start-lodge`).
- **Now:** `career-ui.js careerMark` records {rider, the URL's course} in this tab's sessionStorage when a career world or career
  event starts (cleared by Quit to the title and by a Single Event). At the boot's autostart, `resume()` finds the mark for
  that URL and rider and re-enters: `ui.careerMode` on, then the world load at Happiness / the last lodge (`enter()`), or
  directly the ride when that world and location are the ones the URL already loaded. An abandoned event is dropped, as a reset
  drops it. Chrome and WebKit: a reload at Green Base Station resumes the career there; a reload during a Snow Jam round (URL
  course ARA1) loads PEAK1 at the last lodge (Blue Base Station) with the career. test-ctm-flow 14.

### Transport lists and help (pv `transportLists`, on)

- **Rows:** the Race / Freestyle lists are built (cUITemplate_MAP_setupMenus 0x2012D0) from the row tables 0x4781D0 (race) and
  0x4786E0 (freestyle), 0x6C a row {course, ?, mode, peak flag, name flag, inline name}: Peak 2 freestyle Style Mile, Launch Time,
  Schizophrenia, Ruthless Jam, Peak 2 Jam; Peak 3 Kick Doubt, Much-2-Much, Perpendiculous, Throne Jam, All Peak Jam (PS2
  `allpeak/nav/out-apj-card/fs-list`). The port listed the goal-list order 0x45AAD8 (big air, pipe, slope style). A row's name
  is the course name (name flag), else its inline name ('Happiness Jam', 'Ruthless Jam', 'Throne Jam': the port said 'The Throne
  Jam'), else mode 4 the course name, else 1FD190 (the peak events). Peak 3's race table has a 4th row, course 23 'DONOTUSE',
  hidden.
- **Help (0x207430):** an open rival or peak event row reads kT_HELPChalAvail "A battle against your rival." (the port showed the
  course blurb, or nothing for a peak event); a locked peak Jam formats "%S Jam" with the course name into kT_HELPLockCompEvent
  ("LOCKED.  Complete The Throne Jam to unlock."; the port said "The Throne").
- Chrome and WebKit match the PS2 frame's rows; test-ctm-flow 15 reads the row tables from the ELF when present.

### Stations and heats (pv `stationFlow`, on)

From the stations audit (world states 13 / 14 / 15, 1F72E0, the map modes); the visual-parity agent's `stationArrival` /
`sessionFade` already dropped the walk-in and prompt after a Transport to a station.
- **Lodge door / booth (WS14 enter 236250):** the riders are held (113B10(C, 3)) under lodge_arr3 / hub_trans_arr, and a running
  Big Challenge ends (0x23626C 30B7F8). Port: `free-ride.js` posts 'stationCut' (main.js pauses the ride) and runs
  `mission_world_session(0)` before the cut. Before, the rider rode on under the cut.
- **The lodge prompt (1F72E0):** Triangle (event 6) is No. ~~No is 233AA0 -> WS4, whose enter places nothing: the rider rides on
  from the door~~: wrong, see "The lodge door on the PS2" below (pv `doorNoPlace` restores the session point 0 placement).
- **The booth map (mode 3):** Select Peak opens on the ridden peak (the port: always Peak 1); Back is WS15 at the station (session
  point 1, the white fade; 0x20220C..54, 0x2365D8), not the MCOMM (`career-ui.js openBooth`).
- **The post-event map (mode 4):** Back is ignored (0x2022A4). Before: Back -> MCOMM -> Return resumed a quit run (a frozen game).
- **A station row** asks "Transport to this area now?" (PS2 peak3 out-fr-to-black-station prompt), not the lodge question.
- **Heli:** a transport across a world switch into a visited backcountry plays the heli drop [16 <bc>_heli_arr, 17
  heli_arrb_<char>] (WS10 0x235220 with the streamer's transport flag; `save.pending.transport` -> `enterWorld`), a world load
  there nothing. Leaving a backcountry event rides the heli (0x2365CC: the event's course is the one left; `ctm-transport.js
  from`, `cutscenes.js opts.from`), and the same backcountry again is a transport, not WS15.
- **WS13 (next heat / the results' Restart, 0x235AA0):** the gondola (27A860) only for races (event type 0); freestyle queues its
  gate lists alone (27AAF8: [4, 5] before the final, else [5]); rival events (types 5 / 6) queue nothing.
- Chrome and WebKit: the ride holds under lodge_arr3, Triangle -> No at the door, "Go to this peak now?" into visited Ruthless:
  heli_dep > heli_inair > heli_inair_zoe > dbc2_heli_arr > heli_arrb_zoe (switch off: no drop). test-ctm-flow 16.
- **The lodge door on the PS2** (`local/ps2-capture/runs/peak1-green-start`, neutral pad from Green's session point 0, and the new
  `local/ps2-capture/ctm-parity/door/`: the same start, then Triangle at the prompt, states through the navigator):
  - tick 394, the door volume: world state 4 -> 14 (0x236250: 113B10(C, 3) PreRace, whose per-tick update is empty; the rider
    pipeline 128AF0 keeps running). The rider is at mdl_A_NIS_Lodge_0's position (-72133, 37460) on the ground, aligned with its
    X axis (-0.854, 0.520), control 13 / motion 3 (the NIS hold, as the transport loop's), its velocity 0 within a tick; it stays
    there for the whole cut (242 ticks, lodge_arr3), then the prompt pushes pause context 2 (mask 0xFFFFFFDF, 0x236504).
  - No (Triangle, sample 1002, tick 636): world state 14 -> 4 and the rider at the station's session point 0 (the region row kind
    1 index 0, (-65782, 33404) at Green) moving at forward x 833 cm/s: 11DE60(rider, 0, 2) + 11DF18, as a respawn. The port did
    this before pv stationFlow and then stopped (a misreading of 234E20); **pv `doorNoPlace`** puts it back. Chrome and WebKit:
    after No the rider at (-65815, 33422, -212608), the PS2's (-65814.7, 33421.6, -212607.8) at tick 637.
  - The port pauses the ride under the cut (pv stationFlow) instead of holding it at the locator: invisible (the cut's cast
    rider is drawn, and No places it anyway); the world's animations stop for those 242 ticks where the PS2's run.
- **The booth on the PS2** (ctm-parity/booth, fr-booth2 record 2644): the rider at mdl_A_NIS_Transport_0's position (-71830,
  39559) with no speed under the booth cut, the booth DJ flag set (below). The booth map's Back is WS15 (session point 1).
- **WS13's rival branch** (0x235B38..0x235B60): event type 0x535C10 5 or 6 (the rival challenges AND the peak runs) or a Single
  Event / multiplayer path: no gondola, no gate lists; the first update's sub-state 3 goes 233AA0 -> WS1 arg 3 (2341D0: PreRace,
  "Loading..." (HUD bit 2) while the computer riders are rebuilt and placed, 128958 / 1296F8 / 1289F0), then WS2 (236BB0: 20A8F8(2)
  -> overlay 0x14, `68rival_pre`). The port's card matches; **pv `ws13Rival`**: a peak run's Restart takes that branch too (it
  queued the freestyle gate lists before, which a peak world does not have).
  - A peak run's Restart on the PS2 is not WS13 at all (`local/ps2-capture/ctm-parity/restart/`: the Peak 2 Race results, Restart,
    "Restart / Are you sure?" with No focused, Up, Cross): world state 7 -> 6 at the Yes, the world torn down and loaded again
    (~420 samples of loading), world state 10 -> 1 -> 2 within 5 samples, the 68rival_pre card at the run's start (Ruthless).
    The port restarts in place without the load screen (the same start and card). Open: a rival challenge's Restart ("Loading..."
    under WS1 arg 3) is not captured.
- **WS14 enter** also zeroes a word of C+0x40[0] (+0x18) at 0x2362B4 for every arg (read as the rider's boost meter +0x2F8 by the
  static trace; the door capture's meter was 0 before and after, so unconfirmed) and runs 230180 (a world reset) only when the
  current course equals its +0x18 (a Transport to the current location).
- **The booth DJ flag (pv `boothDj`):** builtin 68's booth branch (0x302410: 231250(W, 14, 2), then 28B180 -> 2A49E8) sets
  audio+0x5818 = 1 and posts a 30 000 ms timer (2ADCA0, the pmf at 0x4A36F0 -> 2A4A68 clears it). 2A4A38 = the flag and 2A10C0(audio,
  10): the request being spoken (2B1220 -> 2AB150 -> its +0x80) is the DJ's (speaker 0xA). The travel (28E8C0 code 20) asks it
  twice: before pktrans (0x28EDF0: the speech stop 2B11B0 skipped) and before the radio big intro (0x28EDB4: 2A26F0 / 2B1758 /
  +0x6258 / +0x5754 skipped). So a DJ line playing at the booth plays on over the transport. Port: `game-audio.js booth()` (from
  main.js 'stationCut' at a booth), `boothHold`, `audio-speech.js speaking(speaker)`. PS2 (`local/ps2-capture/ctm-parity/booth/`:
  fr-clean with the first-FAQ bit 0x534FF0 bit 3 poked so Green's "?" stays shut, the autopilot from Happiness through ABC1_A to
  mdl_A_NIS_Transport_0): the booth at record 2644, world state 4 -> 14 and audio+0x5818 0 -> 1 in the same record. Chrome and
  WebKit: the flag and its 30 s timer at the booth. test-audio-timeline 7 (no booth: the radio big intro; the flag with the DJ
  speaking: none; after 30 s: again).

### The NIS rider hold under the station cuts and the Transport's ride (pv `nisTick`, on; `web/core.cpp nis_hold`)

The port pauses the ride under the lodge-door and booth cuts (pv stationFlow). On the PS2 the world runs on under them, with the
rider held by the cut's rider actor.

**PS2, measured:**
- Door (`ctm-parity/door`, Green, neutral pad):
  - World state 14 at tick 394. lodge_arr3 runs 240 game ticks with the world ticking (394 -> 634).
  - The prompt then pushes pause context 2 (0x236504) and the tick stops at 634.
  - At the prompt (`door-no/pre.p2s`, tick 634) the rider is **still held**: rider+0xAC4 = 1, velocity 0. It stands at
    (-72133.0, 37460.2, -214274.3): mdl_A_NIS_Lodge_0's x and y at ground height, facing its X axis (-0.854, 0.520, 0).
  - Tick 395 (`door.p2s`): velocity (-144, -176, -6), the actor's first tick.
  - No (tick 636): +0xAC4 = 0, and the rider is at session point 0 moving at forward x 833.
  - So the release and 11D390 come at the No, not at the cut's end: pv doorNoPlace is the right place.
- Booth (`ctm-parity/booth` fr-booth2): world state 14 at tick 4114. There is a record (the rider pass) with control 13 every tick
  through 4353 (240 ticks). Then the map pauses.
- Transport (`ctm-parity/runs/f95-after`, the post-event Transport to Ruthless): the game tick runs through the in-air ride (149:
  15398 -> 15506) and through the held loop while the destination streams (world state 11, 122: 15522 -> 15897). It stops only
  while an NIS list is read from the disc: 15398 for about 500 frames at world state 14, and 15927 for about 1850 frames at world
  state 10 before the arrival movie. So the 270280 gate does not stop the world during the held loop.

**PS2, code (static trace):**
- **Enter.** World state 14 enter 0x236250 queues the lists. For arg 0 that is 278E50 of group 0x16 (lodge_arr3, flags 3) and
  group 0x17 (flags 1), then 278F38 / 278F68. For args 1..3 it first calls 11D390 (0x2363DC).
- **Start.** The script's first tick starts the rider actor: 274A30 from the NIS tick 230BE4, which runs before the rider pass
  230CF0. The actor's vtable is 0x459B90 at rider+0x6D0. Its start 282FB8 -> 27D400 calls the handler at rider+0x6DC, slot +0x20
  = **123640**.
- **123640, the actor matrix:** +0x700 = the anchor (27A0D8) x the actor's placement. Anchor 19 is locator 7 (NIS_Lodge), anchor
  28 is locator 4 (NIS_Transport); both go through 27BB08 and the 3369D8 ground snap.
- **123640, the rider:**
  - Control and motion: 11FEC8(13) runs the old control's exit (13 has no enter); 11FE78(3) runs the old motion's exit (13F410
    for motion 0; 3 has no enter).
  - Animator: 311A50, then 3128E8(anim, 5, -1.0, 0).
  - Flags: +0xAC4 = 1, +0x2D8 = +0x320, +0xAC8 = 0, +0xB08..+0xB14 = -1, +0xB00 = 1, +0xAFC = record byte 7, +0xAD0 =
    record byte 4.
  - Key 0 (27C9B0): +0xAE0, and its angles x 0.0174533 into +0xAF0..+0xAF8.
  - Position +0x110 = M(+0x700) . key0.
  - Orientation: qEuler(+0xAF0 + +0x740, +0xAF4 + +0x744, 0) (x) qZ(+0xAF8 + +0x748 - pi/2) (1241C0, gp-0x78AC), then 11E098.
    The forward row comes out as the anchor's X axis.
  - Velocity +0x1E0 = 0 (0x4FF120). The controller is reset (111890). +0x1F0..+0x2D3 = 0, except +0x25C = +0x264 = 1.0.
  - The part toggles 30E9E0 / 30EA80 and 114130(rider, 0, 0) follow.
  - Not written: +0x370, +0x380, +0x438.
- **Each tick while held:**
  - Control 13 has no tick (table 0x456C10 -> 111824). Motion 3's tick A is 136958 (11E098 only) and tick B 136978 is empty. So
    there is no integration, gravity or contact.
  - With +0xAC4 set, these are skipped: 11EB60 / 11EB98 (121700 / 121728), 112338 and the score tick 117C28 (121818).
  - The pad is read (121068), but nothing uses it.
  - The NIS moves the rider through 123DA8 -> 124788: position += R(+0x700) . delta key, then 11E098. The velocity is delta
    position / dt, or the root bone's (+0x8A0) delta when +0xAFC is set.
- **Release 123B48** (object stop 273DC0 -> 283000 -> 27D4A0):
  - 2803F0, then 11D390(rider). For station kinds 4..6 that is 11DE60(rider, idx, kind) + 11DF18.
  - It clears +0xAC4 / +0xAFC / +0xB04 / +0xAD0, stops the speech if +0xAC8, and reverses the part toggles.
  - The door's No, the booth map's Back (world state 15, session point 1), a Transport's arrival (27A9F0 -> 279070) and the
    lodge's Yes (world teardown) all end with a placement.

**Core (`web/core.cpp nis_hold`, in the tree 2026-09-28, CTM agent; not yet in a live core):**
- **`nis_hold(1, x, y, z, fx, fy)`** is 123640's rider part:
  - It runs the running control's and motion's exits (the same clears as reset_rider: start, rail, handplant, board press, attack,
    reset, crash, soft).
  - It sets control 13 / motion 3 (`npc_motion_mode` 3, `browser_control_state` 13) and velocity 0.
  - It puts the rider at world cm (x, y, z) with +0x120 = qZ(atan2(fy, fx) - pi/2). A NaN z keeps the rider's.
- **Each tick while held,** `step_rider` runs the motion-3 tick only:
  - 1210B0's collision timers and the boost tick 1200D0 (motion 3, control 13), then the 1211F8 filters and 11E098, velocity 0.
  - `animation_pose` returns at once (121700 / 121728 skip 11EB60 / 11EB98; motion 3's second phase 136978 is empty).
  - `follow_rider_route` returns at once (121818 skips 112338 and the score tick 117C28).
  - The world passes run as usual.
- **`nis_hold(0, ...)`** releases without placing. `reset_rider`, `place_rider_region` and `fresh_rider_start` release first
  (123B48 before 11D390).
- **Not modelled:**
  - the actor's per-tick moves 124788 and their velocity (PS2 fr-booth2: (-6046, 6153, -1654) on the first tick, a few cm/s
    after, 0 by the end);
  - the animator's 311A50 + semantic 5 (the pose is not drawn under the cut or the prompt / map);
  - the +0x1F0..+0x2D3 clear (+0x25C = +0x264 = 1.0).
  - The release's 11D390 rewrites the velocity, the animation and the controls.
- **Checks:**
  - The full `web/test-ps2-captures.mjs` passes on a CORE_OUT build with this and the slot 0x19 fix (247 scenarios).
  - Chrome and WebKit, on a second dev server serving that core (scratch vite config, port 5241), `?pv=nisTick`, Green:
    - Door: held at (-72133.0, 37460.2) at ground height through lodge_arr3 while the game tick runs (~97 -> 370). The prompt
      then pauses the world. No places the rider at (-65814.9, 33421.7, -212607.9); the PS2 at tick 637 has (-65814.7,
      33421.6, -212607.8).
    - Booth: held at (-71829.9, 39558.8) with q (0, 0, 0.4279, 0.9038), as PS2 fr-booth2 record 2644. The map pauses; Back
      places the rider at session point 1.
    - On the live core (no `_nis_hold`) the cut pauses the ride as before.
- **Open:**
  - **The 15 cm (fixed):** it is key 0 of the rider actor's clip. Every station copy of lodge_arr3 (#148, scdat_A..E) and
    hub_trans_arr (#166) has the human's actor (kind 5, binding 4, anchor 19 / 28, offset 0) with z channel key 0 = -15.0 and the
    other channels 0. 123640 places +0x110 = M(+0x700) . key 0, so the rider stands 15.0 cm under the snapped locator, as both captures
    show. `web/cutscenes.js actorStart(number, location)` computes that root (anchor frame x offset x key 0, `actorRootAt` at t = 0)
    from the station's script copy (`prefetchActor`, fetched when the free ride starts), and `main.js nisHoldAt` holds there. Before
    the copy is in, it falls back to the bare anchor.
  - The game tick at placements: see the next section (pv `gameTickKeep`).

**Port (pv `nisTick`, on; inert without `core._nis_hold`):**
- `main.js` 'stationCut' holds instead of pausing (`nisHoldAt`: anchor 19 or 28 of the station, from `web/cutscenes.js anchorOf`
  with the cut engine's ground snap; `locators.json` is fetched when the free ride starts). The cut's end ('station') pauses for
  the prompt / map with the rider still held.
- `resetPhysics` releases first, so every placement does.
- A resume with the rider still held places it at the station's session point 0 (`nisResume`: 123B48 -> 11D390).
- **The Transport's ride** (pv nisTick; before: the world paused and `free-ride.js transport` pumped the streamer by hand):
  - **PS2** (`local/ps2-capture/ctm-parity/transport/`: nav/p2/out-to-c/frprompt.p2s, "Transport to this area now?" Yes from Ruthless
    (DBC2) to Yellow Mid Station C; `tools/ctm_flow_capture.py` with the new `RIDER_TRACE=1`, the rider every tick through PINE):
    - World state 14 at tick 922: WS14's enter places the rider at DBC2 (11D390 at 0x2363DC).
    - Tick 924, heli_inair #149 (120 ticks; no heli_dep from a backcountry): +0xAC4 = 1, the rider held at (-306117.1, -99303.9,
      -648440.6), q (0, 0, -1, 0). That is anchor 29 of a looping script, the TRANSP location's heli locator 5 (-306002.1, -99403.9,
      -648335.6), x offset (0, 0, -285) x key 0 (-115, 100, 180), yaw -90.
    - Tick 1044, heli_inair_zoe #122 (the held loop: world state 11 from 1045, world state 10 at 1442): held at (-306102.1,
      -99403.9, -648279.6), q (0, 0, -0.707, 0.707): offset (0, 0, -400) x key 0 (-100, 0, 456). The velocity stays under 1 cm/s.
    - Tick 1471: released and placed at C (world state 4).
    - The game tick runs throughout (921 -> 3920, no stop), and the hold moves only at the step changes (123640 per step).
  - **Port:** `main.js transportInWorld` no longer pauses.
    - It holds the rider where it stands (`nis_hold`), then at each step's human actor root at t = 0 (`cutscenes.js` calls
      `host.onHumanActor` at every step start; `main.js` holds while `nisTransport`).
    - `free-ride.js transport(dest, {ticking: true})` leaves the streamer passes to the game ticks.
    - The arrival placement (`resetPhysics(true)`) releases the hold.
    - It needs a posed frame (`nisPosed`: game-tick.js reads it for the camera head). A Transport before the run's first tick
      falls back to the pause.
    - Not modelled: WS14's enter placement at the current location (the page holds at once instead of 2 ticks later).
  - **Chrome and WebKit** (MOUNTAIN, Ruthless -> C): held at (-306117.1, -99303.9, -648440.6) q (0, 0, -1, 0) under #149 and at
    (-306102.1, -99403.9, -648279.6) q (0, 0, -0.707, 0.707) under #122, exactly the PS2's. The ticks ran throughout; the page's first
    sample after the arrival, (-176965.3, -13106.3, 229961.2), lies on the PS2's line (its record 1473: (-176965.3, -13107.1, 229961.1)). test-cutscenes checks
    the three actor roots.
  - The page's held loop is shorter: its destination rows are in sooner than a disc read.

### The game tick at placements (pv `gameTickKeep`, on)

- **PS2.** The game tick 1298C8 (rider manager +8) is 0 at a world load: peak1-green-start record 0 is tick 0 and
  green-start-t0's WS10 reads 0. It is 0 again at an event's WS2, as in peak2-arr. A peak run's crossing restarts it through
  128A10 -> 1297C8 (event type 5). Nothing else restarts it: 11D390 (every placement) leaves it alone. Door No reads 636 after
  394 + 240, and fr-dra4a-full's free-ride crossing (DRA4_A -> Green, world states 11 and 10) runs through 19,827 records without
  a restart.
- **Port before.** Every JS placement went through `reset_rider`, which reloads the world seed's landing tick
  (`browserLandingTick`): MOUNTAIN 0, PEAK1 2772, PEAK2 1722, PEAK3 14302. So after the door's No the tick was 3, not 636.
  - The absolute tick drives the 332DB8 scope refresh phase (tick % 3), the boost ring's even frames (tick & 1) and the ground
    stamps (13C7A8 / 13F410).
  - The section scan and the stage world count their own clock (the set-piece tick, `set_piece_info()[1]`, minus the restart
    base), so a reset also put the core's tick out of step with the section clock. On the PS2 the two are one counter.
- **Port (`main.js resetPhysics(keepTick)`, pv `gameTickKeep`), streamed worlds only (course.freeRide):**
  - A run's start (`startRun`: the world load, a peak run's start or its Restart) restarts the tick to 0 (`game_tick_restart(0)`,
    which also rebases the section clock to 0).
  - An in-world placement keeps it: `game_tick()` before `reset_rider`, then `game_tick_restart(tick)` before the region placement,
    so 13C7A8 stamps the running tick. The in-world placements are the lodge No, a Session point, a Transport's arrival and a held
    ride's resume.
  - From a run's start the core tick and the section clock count together, so the restore leaves the section base where it was.
  - Events (not a streamed world) keep `begin_event_rider`'s 0.
  - No core change is needed: `game_tick` and `game_tick_restart` exist.
  - A Transport's arrival placement re-attaches the route at the grid slot with a second `reset_rider` (`free-ride.js placeRegion`),
    which keeps the tick too.
  - Chrome and WebKit, Green: the tick runs through the door and booth cuts and the No / Back (327 -> 333) and equals the set-piece
    clock at every sample; the Transport Ruthless -> C keeps it through the arrival (421 = the set-piece clock). Before, 3 after the
    No and 1 / 5 after the arrival.

### Awards, the PS2 way (pv `awardCascade`, on)

- **159CD0 (a grant):** one-shot through 15A2E0 (17..19 repeat); the award's items by 0x45AA40 (0..4 and the Peak 3 goals a cheat
  character; the Peak 1 goals 4 cards + a poster, then 158F60(award, 1); the Peak 2 goals a toy + 2 art, then 158F60(award, 2);
  17..19 / 20..31 a gear item), each recorded (15A628); then the award's bit, the cheat and the gear; then **the cascade**
  (0x15A130): award 0 (1577E0), award 1 (1578A0), awards 2..4 (157A78 per peak), every complete goal (157BF0 -> 159170).
  Before, a free-ride grant (collectible, Big Challenge, cash) granted only its goal award: "Peak N conquered!" and "All events
  completed!" waited for the next completed event, or never came when the last goal completed in free ride.
- **Award 0 "Mountain conquered!"** (Far East Myth) was granted nowhere. 1577E0 = 157920 on all three peaks (gold or platinum on
  every standard event, any medal on the rival and peak events, PLATINUM on both Freeride medals; earnings not checked) and the
  eight highlight levels (+0xBB8) summing to 24. It is checked in 1591E8, 1599A0 / 159B08 and every cascade.
- **1591E8 (an event's result):** the first gold's award before anything is stored; a better medal: the event goal's state,
  159818(cash) (its earnings awards), the medal stored (152528), the goal newly complete -> 159170; else 152528 and 159818(cash / 2);
  then award 0; the interface's cash (+0x10) and medal (+0x14). So a final that completes the earnings goal and the race goal
  records the Peak 2 pass under "Earnings goal complete!".
- **158F60 (passes):** arg 2 opens the Peak 3 pass (bit 13, pass item char*3+2) and then, like arg 1, the Peak 2 pass (bit 12,
  item char*3+1), each recorded under the granting award.
- **1599A0 / 159B08:** at a medal threshold, the Freeride goal award only when the goal is complete and the count is the first
  medal's (0x45AFE8 / 0x45B018 +0xC), then award 0.
- **The reward record 0x4C3EF0** (RAM): per award a count per category (+6 + award*10 + category), per category a set of item
  indexes, per award the cheat id (category 8), one gear item (category 9). 158E30 clears it at world state 10's enter (0x2355C0:
  `career-ui.js enterWorld`, `courseChanged`) and at the reward list's close (1FF700). 20A8F8 opens the list when the record holds
  anything or the cash is not 0. 0x1FF7B8 walks it: cash, then per award its title and categories 0..9, the items taken in
  index order with one cursor per category. Collection-bonus cheats (1580F8 / 157FD0 / 158220 / 158348 -> 158618) are not
  recorded, so not listed (they were).
- **Messages:** 159CD0 posts an award's message at its case, before the items and the cascade, so the hook in
  `web/career-messages.js` posts before the grant (pre-order: 249 before 252).
- **Files:** `web/career.js` (`grant159CD0`, `complete1591E8`, `earn159818`, `exploreGoal1599A0`, `openPass158F60`,
  `mountainConquered`, the record), `web/career-ui.js` (`rewardLinesRecord`, the clears), `web/career-messages.js`,
  `web/big-challenges.js`. Test `web/test-award-cascade.mjs` (the PS2 sd-goal record bytes, the free-ride cascade, award 0, the
  event order, the unrecorded bonus). Chrome and WebKit: the Snow Jam final with the earnings and race goals lists Cash / Race
  goal complete! (poster, 4 cards) / Earnings goal complete! (Peak 2 pass, poster, 4 cards) / Gold medal earned (accessory), and
  Continue empties the record.

### Peak crossings (pv `crossWorld`, on)

- **PS2:** the mountain is one world: the bottom of Intimidator (DRA4) streams into DRA4_A, Green Base Station (Peak 1), and the
  bottom of Gravitude (ERA5) into ERA5_C, Yellow Mid Station (Peak 2). The load request (22D088) names a map id the port's peak
  world has no row for (PEAK2 has no row 17, PEAK3 no row 19), so the free ride dead-ended there: the rider rode into the
  connector and nothing loaded.
- **Port:** `web/free-ride.js drainEvents`: a load request (kind 1, a = -1, b = 2) in free ride on Peak 2 / 3 posts
  'crossWorld' (17 / 19); `web/main.js` hands it to `career-ui.js crossWorld(dest)`: the new-career flag cleared (WS11 enter),
  a world load at the station reached (`goWorld(dest, {ridden: true, reload: true})`), the last lodge set at arrival (WS10).
  It is a world switch with a load screen, not the PS2's seamless stream: that needs the whole-mountain world for free ride
  (open, the coordinator's call).
- Chrome and WebKit (`scratchpad qa/cross.mjs`: the rider 30 m above mdl_DRA4_A_Load_0 / mdl_ERA5_C_Load_0, stick up): PEAK2 ->
  Green Base Station (course 17, last lodge 17), PEAK3 -> Yellow Mid Station (19, 19). Switch off: the rider stays at the bottom
  of Intimidator. test-ctm-flow (the crossing's state).

### Small fixes (pv `ctmSmallFixes`, on)

- **Message Delete (1E3268):** deletes the first inbox entry holding the viewed message's item (the PS2 matches the item, not the
  row), so a duplicate message deletes the older copy. Before: the row under the cursor. test-messages.
- **The last lodge (+0x27C):** set only when a station is reached (WS10's enter), not when a Transport to it is chosen: a
  Transport given up at the load keeps the old last lodge.
- **An in-world transport's arrival:** goWorld no longer sets the career's course when the page answers 'transport'; it changes at
  the core's 0x535C08 change (22DF50 -> WS11 -> WS10 at the destination), so `courseChanged` marks the location visited and makes a
  station the last lodge. Before, it was already set and courseChanged skipped both (with the last lodge moved to WS10 above, an
  in-world transport set neither). test-ctm-flow.

### The whole mountain (pv `mountainRide` on the desktop tier; `mountainAudio`, `bankEvict`, `crossingArrival`, on)

The PS2 free ride runs in ONE world: the streaming table 0x442168 holds every location, the residency table 0x442488 every
course row, and riding off the bottom of Intimidator (DRA4 -> DRA4_A) streams Green Base Station (A) in through the connector's own
Unload / Load triggers, Gravitude (ERA5 -> ERA5_C) Yellow Mid Station (C). The port had one world per peak (crossWorld: a load
screen at the missing row). `mountainRide` runs the career free ride in the whole-mountain world MOUNTAIN (docs/peak3.md section 6,
until now only the Peak 2 Race / All Peak Race / Jam).

- **World choice:** `free-ride.js freeRideWorldOf(course)`: MOUNTAIN when `mountainRide` is on and the device is not a phone (iOS /
  Android, or quality=low), else the course's peak world. `?mountain=0|1` overrides the tier. Used by `main.js selectFreeRide` and
  the free-ride callback (every Transport inside MOUNTAIN is an in-world transport, world state 14 arg 1 -> 22CEA8 streaming under
  the held loop), `ctm-transport.js switchesWorld`, `career-ui.js reenter`. A free ride at a course takes that course's peak for the
  base course settings (before: always The Throne's).
- **Following the rider:** 22DF50 (a riding crossing: world state 11, then 10 at the new location) reaches `career-ui.js
  courseChanged`: the new-career flag, the visited mask (+0xACC), the last lodge at a station (146E10 at 0x2356FC), the reward
  record's clear (158E30), and now the MCOMM's peak (the Transport map opens on the peak being ridden). The HUD counters, collectibles
  (stage-collect MOUNTAIN tracks), Big Challenges, the painters / fog / sky / weather (location records), the hub song (game-audio
  `freeRideCourse`, the MusicTrigger zones), cutscenes (station / transport lists by course) and the world audio (`mountainAudio`)
  key on the course or the location, not the world.
- **The PS2's crossing** (`local/ps2-capture/ctm-parity/mountain/`: peak2/frdra4-3550 with the last lodge poked to D
  (`states/frdra4-lodgeD.p2s`), ridden by `tools/ps2_autopilot.py` with a route through DRA4_A (scratch `ps2_autopilot_fr.py`, route
  `dra4a`, mode 12: the stations' bank variant 0), resumed once from its kept state at record 18000; `runs/fr-dra4a-full.*`):
  - record 14732, the DRA4_A Unload: 0x535C08 3 -> 17 and world state 4 -> 11 at once; DRA4 / D_DRA4 2 -> 7 (-> 0 seven records
    later), A's row and ASKY 0 -> 3 -> 6, read by 14950; DSKY leaves at the skybox trigger (16376). No load screen, no placement:
    the rider's steps stay within its speed (the only jump is a crash reset, control 9, at 15233).
  - record 19215, the DRA4_A Load (A's row 1 -> 2): world state 11 -> 10 and the last lodge 20 -> 17; record 19216: world state
    10 -> 4 and the visited bit 17. So a riding crossing is world state 11 from the Unload to the Load, with WS10's writes at the
    Load; the port did them at the Unload. **pv `crossingArrival`**: `courseChanged` keeps only WS11's (the course, the new-career
    flag, the MCOMM peak) for a riding crossing; free-ride.js posts 'arrive' at a free-ride Load trigger (22D088) and
    `crossingArrived` does WS10 / WS4's (the reward record's clear, the last lodge, the visited bit). A transport's arrival stays
    at once. web/test-mountain-ride.mjs replays the capture's triggers through the core's MOUNTAIN streaming (every row, every
    record, the course) and checks these records; test-ctm-flow 17.
  - **Per-tick physics parity (gate `ctm/fr-dra4a-full` in web/test-ps2-captures.mjs):**
    - The seed: the free-ride seed MOUNTAINF (the capture's baseline frdra4-lodgeD, region DRA4). It is exported with
      `tools/export_peak_seed.py --world MOUNTAINF --state ... --region DRA4 --out DIR`; the new `--out` writes under DIR instead of
      web/public/assets and local/assets/native. It is now in web/public/assets/MOUNTAINF and local/assets/native/MOUNTAINF, and the
      live core compiles it.
    - The pads: a segments script (`runs/fr-dra4a-full.script.json`, 0 misses).
    - The capture is linked as `local/ps2-capture/runs/ctm/fr-dra4a-full.*`. The case skips on a core without the seed (`coreHas`).
    - Physics and all 29 bones are exact for 5033 ticks, through 8403, including the DRA4 free ride with its crashes and rails.
    - The trick score matches through 7217. At 7218 a combo expiry pays $1 (119EF8 kind 2, popup 0x30), and the PS2's HUD slot
      0x19 shows it on that tick: 117FE0 reads the cash C+0xAC4 after the tick's awards. The browser showed it a tick later,
      because score_environment() copied the cash before them.
    - Fix: `engine/score_object.cpp` `originalScoreHud` reads a new `OriginalScoreHooks::careerCash` (web/score_gameplay.inc).
      Live since core47 (f0945ae4); the gate's scoreThrough is 8403.
    - Open, 8404: the PS2 finds the ground on the instance mdl_DRA4_highwayRebuild_2049 (patch -1, normal (0.96, 0.28, 0)) one
      tick before the browser, which meets it at 8405. It is not the 332DB8 scope: a ±1 phase shift breaks at 7594, and a
      scratch core admitting every instance from 8380 still meets it at 8405.
  - The capture's rider then reached Green station's "?" (mdl_A_questionmark: builtin 100) and the PS2 opened the Message Center on
    FAQ 1 "How do I open up other peaks?" (`runs/fr-dra4a-full.end.png`); the port's MOUNTAIN ride does the same (screen
    ctm-message at the "?").
- **"Go to this peak now?"** in MOUNTAIN is the PS2's transport (WS14 heli -> WS11 -> WS10 at the backcountry): `free-ride.js
  transport(dest, {firstVisit})` plays the first arrival (the DBC2 / EBC3 movie, then the heli drop) for a backcountry whose +0xACC
  bit is clear (0x234F40 / 0x235080), the heli drop for a visited one.
- **Streaming across the boundary** (autopilot on the race paths, the whole ride from the top of the course; `scratchpad
  ctm/mtn/mride.mjs`, frames by course / region with `frames.mjs`): no stalled frame and no load frame at either crossing.
  | ride | Chrome: frames over 50 ms at +-10 s of the crossing / max | WebKit (desktop) |
  |---|---|---|
  | Intimidator -> Green Base Station (MOUNTAIN) | 1 / 57.5 ms | 0 / 45 ms |
  | Gravitude -> Yellow Mid Station (MOUNTAIN) | 1 / 50.2 ms | 0 / 42 ms (the whole 4.6-min ride: 2 over 50) |
  | Happiness -> Green Base Station (PEAK1, the baseline) | 1 / 57.3 ms | 6 / 63 ms |
  | Gravitude in PEAK3 (the baseline, no crossing) | | the whole 5.5-min ride: 1 over 50 |
  The WebKit Gravitude ride first counted 83 frames over 50 ms in MOUNTAIN vs 0 in PEAK3 (run at different times). Two causes:
  the whole mountain's window prefetch fetched and fed the uphill rows (E's hub and connectors) for nothing (now skipped in a
  free ride with streamAhead: planAhead already reads the rows the connectors lead down to, in the PS2's order), and the
  machine's load (other agents' emulators and browsers, load average 20-29 then). Run back to back at a load average of ~11:
  MOUNTAIN 2 over 50 on the ride (0 at the crossing), PEAK3 1.
- **Memory** (WebKit at the phone tier, 844x390, quality=low, footprints by `footprint`; Chrome heap snapshots):
  - at D on load: MOUNTAIN wasm 154-221 MB vs PEAK2 184; WebContent 1003-1231 vs 958-1101 MB; GPU the same (the same locations).
  - every location's collision fed (a session that rode everywhere): MOUNTAIN wasm 382 MB (Chrome 459) vs 184 for a peak;
    WebContent 1449 vs 1188 MB (+22%). The core never releases a location's collision (web/peak-world.js), so the phone tier keeps
    the per-peak worlds and crossWorld. The 1:1 fix is the PS2's release (next section), for the physics agent.
  - `mountainAudio`: web/game-audio.js `isPeakWorld` left MOUNTAIN out, so its world audio had no residency: the 44 locations'
    slot 8 / 9 banks were decoded at the load (+130 MB of PCM in sfx.js, Chrome heap snapshot: 97 x 640 KB, 92 x 320 KB, 44 x 896 KB
    Float32Arrays) and slots 8 / 9 held the last one's (B's). Now they follow the rows (286CA8): EBC3 at the All Peak Race start,
    then E, ERA5, C, CRA3, D, DRA4, A, ARA1, B, BRA2; JS heap at the start 150 vs 288 MB.
  - `bankEvict`: web/sfx.js drops a bank's decoded patches and AudioBuffers 30 s after no slot holds it (loaded again within that,
    the cache is kept: a rider hovering at a row boundary never re-decodes). All Peak route: cache peak ~65 MB, 25 MB after; riding
    Happiness -> ARA1: no slow decode, no dropped voice, no late music bar (Chrome and WebKit, `__ssxAudioStats`).
- Tests: test-ctm-flow 17 (the world choice, switchesWorld, the career following a crossing), test-audio-glitches (bankEvict).

### The PS2's location release: the 1:1 fix for the memory growth (spec for the core; not built)

**Built so far (2026-09-28, core43):** the eviction tick and the path bank. The memory release (item 3 below) is still open.
- **Tick confirmed with a capture:** `local/ps2-capture/runs/release/throne-evict` (fr-ebc3-late + p3-fr-throne-late, the fr-throne-unload watches plus the octree root's removal counter `0x5B21A0` and the page slots).
  - EBC3's row goes 5 -> 7 in record 14422 (T+1).
  - The counter `*(root+0xA0)` jumps 6545 -> 11500 in record 14428 (T+7). The record is taken at the provider exit, so the removal ran in that tick's world update, before its gameplay.
  - The row reads 0 in record 14429 (T+8).
  - Located with the savestate: WC `0xC3C660` = `**(game+0x10)`; the resolver `*(WC+0x3E8)` = `0x544480` (vtable 0x495090 at +8); root `*(res+0xC)` = `0x5B2100`.
- **Port (`web/peak_world.inc`):** a row in 7 stops being collidable when its countdown reaches 1 (`release()`, T+7). The row still goes 0 at T+8.
  - Capture comparisons drive the rows from the records, so `peak_world_rows_tick()` (called by `web/peak-capture.mjs` after each record's rows) runs the same countdown.
- **The AIP bank stays** (3AB498 kind 14 -> 12A490 -> 26ADA0 is `jr ra`): `pass()` no longer calls `drop_paths()` at 7 -> 0, and `peak-capture.mjs` no longer drops it when the bank's row reads 0.
- **Checked:** all 247 capture gates are unchanged (fr-throne-unload crosses this eviction). No gate has a rider touching an evicted location at T+7.
- **The memory (core46, pv `peakRelease`, off until page runs verify it):**
  - `peak_world_free_track(track)` frees an evicted location's collision data. It is refused (-1) while the track is collidable or in the section octree.
    - Its terrain patches leave both terrain systems (`CollisionWorld::releaseTrack`, `WorldBodyCollision::releaseTrack`); the other patches keep their order.
    - Its instances keep their slots, but without collision nodes. Set pieces hold pointers to them, and the hierarchy stays for entity composition.
    - Its grind / handplant rails keep their records without segments (`browser_rails_release_track`). Set-piece spline paths (flags 0) stay, because the location's MultiSplines keep evaluating them.
  - The host frees a location when it drops its draw package (web/free-ride.js; free ride only: one rider context). `web/peak-world.js releaseCore` marks the location absent, so its next want fetches and feeds it again.
  - **Re-read.** The instances and rails go back into their slots (`reinsertInstance`, the rail refill). The patches are appended.
    - Order: a re-read instance sorts first in its octree cell (`reinsertion`), as 328C20 inserts at the head; the patches are the newest insertions. Re-read rails keep the walk order of the first load (open: 358998 / a re-read inserts at the head).
  - **Test `web/test-peak-release.mjs`** (in test:all), on PEAK1 A..B with ARA1:
    - 1358 rail and 274 terrain probes answer as loaded after freeing and feeding ARA1 twice;
    - the instance slots do not grow;
    - freeing is refused while ARA1 is resident.

Read from the asm (SLUS_207.72, `ssx3-decomp/asm`; static, no capture yet). `WC` = cWorldCache `*(gp+0x16C8)`, `WV` = cWorldView
`WC+0x10`, `MM` = the memory manager `*(WC+0xC)`, the resolver `WC+0x3E8` (vtable 0x495090; its +0xC = the world octree root
`*(game+0x20)`). A frame (2306B8) runs the world update 3A6928 -> 3A8290 (3A9258 view step, 3A8668 updatePages, 3A7098 disc read),
then the streamer pass 22D8D8, then gameplay.

- **Unload start, row 5 -> 7** (22D8D8 case 5, 0x22D9E8; ported): `3A9958(WV, track)` moves the section 5/6 -> 3 (1 -> 0 cancels a
  queued read; 2, a read in flight, returns 0 and the row retries each pass); then 230360(game, track): 358700, 229408, 343C08,
  357B90, the bucket manager (354C98, 3551A8 groups 1 and 8), `103308` (the activation list compacted by the entry's +0x78 track),
  308FE0 (missions, the collectible slot). Not drawn from here (3A9D60 walks sections 5 / 6 only); still collidable.
- **View step** (3A9258 case 3, 0x3A94C4): `3A84C8` clears the main page's wanted flag (+0x3F4) and stamps its release time
  (+0x400), `3A9558` the same for every sub-page of the section tree; section 3 -> 4.
- **updatePages** (3A8668; page slots `WC+0x3F0 + 0x18 i`, count `WC+0x3EC`: +0 state 0 free / 1 queued / 2 reading / 3 loaded /
  4 evicting, +4 wanted, +8 cHullPage, +0xC priority, +0x10 time, +0x14 frame): an unwanted loaded page -> 4 with the renderer's
  frame counter (renderer vtable +0x390); 5 frames later **`3A8528` frees it**: its record list (cHullPage +0xC, count +8; {kind,
  key}) in REVERSE, each through `3A8E20` (the track's refcount byte, `WC+8[track]` or `WC+4` for track 255); at 0 the resolver's
  remove, **`3AB498(kind, key, data)`** (jump table 0x495010):
  - kind 1 terrain patches (3AB4F0): `3284B8(node, 1, patch)` unlinks the patch from its octree node's patch list; a node left
    empty (8 children and 3 lists null) is deleted (3284B8, 3AB5A0); the octree's `+0xA0` counter +1 per removal;
  - kind 3 instances (3AB664): the same, instance list (mode 0);
  - kind 8 rails (3AB798): every 144-byte segment (data+0x24, linked at +0x64, count +0x20), entity list (mode 2);
  - kinds 5 / 6 / 7 (3ABBB0 / 3AB918 / 3ABA70): entities of type 7, local lights (type 6, when vtable +0xB0 says inserted), light
    glows (type 8);
  - kinds 9 / 10 (textures, light pages; track 255, shared, refcounted): renderer vtable +0x190 releases the handle, then the
    "BxStream SHAREDMEM" buffer;
  - other managers: 13 instance sound 2B6B30, 15 painter 2BB5A8, 17 camera triggers 16CF18, 18 NIS locators 278A58, 20 sound
    banks 286D18, 21 progress path 210608, 22 avalanche / visual fx 2D9B40, 11 movie player 242570, 19 1547E0 (empty);
  - **kind 14 AIP: 12A490 -> 26ADA0 = `jr ra`: nothing.** The path bank is not dropped (it stays valid until the next 12A340
    frees it through 26AF00 and installs the new one, 26AD70). The port's `drop_paths()` at 7 -> 0 (web/peak_world.inc) differs;
  - kinds 0 materials, 2 models, 4, 12 collision meshes, 16 stage programs: no callback (3ABE10): their bytes go with the section
    memory (kind 16 was torn down at the unload start).
  Then `3A7C30` deletes the cHullPage; the slot is free (0).
- **Next view step** (3A9258 case 4, 0x3A942C): once the main page and every sub-page are 0 (3A8638, 3A95C0): section 4 -> 0 and
  `3A8230`: `3A7B98 -> 3A77D0 -> 3A7818` give every block of the track's Section Allocator (`MM+0x3C+4*track`) back to its pool
  (3A7790) and delete it; `3A7D80` frees the track's resource table (`WC+8[track]`); `3AD1A8` invalidates the light caches
  (2F5998: the world light manager's 8 slots -1; 122658 zeroes each rider's +0x794 cache). The same frame's streamer pass
  (case 7, 0x22D9B8) sees `3A9890` (state 0): **row 7 -> 0**.
- **Timing** (static; the renderer counter assumed once per frame): Unload trigger T; T+1 row 5 -> 7 and 230360; T+2 section
  3 -> 4, page 3 -> 4; **T+7 the record removals (no longer collidable), in the world update before T+7's gameplay**; T+8 section
  4 -> 0, allocator released, row 7 -> 0; T+9 the next read may start. The port keeps a location collidable until its row reads 0
  (T+8): probably one tick long (check with `*(*(game+0x20)+0xA0)`, +1 per octree removal). "Collidable" is patch +0xA & 0x41
  == 0x41 (the query 335960); activation (section 5 -> 6, 3A99D8 -> 3A8F10) sets 0x40 on every kind-1 record of the track and
  nothing clears it: a patch stops being collidable only when 3284B8 unlinks it.
- **Memory model:** three fixed pools of fixed blocks, made once per world (3A7A20 -> cWorldBlockAllocator_init 3A7598, table
  0x44C1E0): pool 0 2 x 0x4000 (the skies ASKY..ESKY, 3A92D8), pool 1 0 x 0x8000, pool 2 0x35 x 0x20000 (6.6 MB: every other
  location and TRANSP). A section's allocator is made when its read is requested (3A81A8 -> 3A7B18) and bump-allocates in its
  blocks (3A7878; 16-byte alignment for kinds 1/2/3/11/12, 128 for 9/10, else 4). No per-record free: the release is every block
  back to the pool; the next section takes them, last returned first. The records stream straight into the allocator (3A7238 ->
  3A8A90 `cWorldCache_addBxStreamDataTest` -> 3A8CD0); one already resident only takes a reference. Track-255 records (shared
  textures) and the AIP ("BxStream AIPATH") live on the global heap.
- **Guards:** no read starts while any row is 5 or 7 (22D8D8); the 5-frame wait before a page is freed; the light caches
  (3AD1A8); the AIP bank kept. None found for a rider standing on an evicted patch or rail: the Unload volume sits at the
  connector's upper end and the connector stays resident.
- **Kept for the whole world:** TRANSP (never released by 22D598), the skies except through the sky switch 22DE98, shared
  textures still referenced, the AIP bank until the next, the pools, the page slots, the octree root and its 8 octants, the rows.
- **Spec for the port's core (per track):** (1) row 5 -> 7 as now; (2) at the eviction (T+7, or at the port's 7 -> 0): unlink the
  track's terrain patches (terrain and body terrain), world collision instances with their collision meshes and render-model
  nodes, rail segments, and its lights / glows / type-7 entities from every query structure, keeping the other tracks' items in
  their order; (3) then free the track's storage as a whole (the PS2 returns whole blocks); (4) do not drop the path bank at the
  eviction (replace it when the next bank is delivered); (5) a re-read inserts its items at the FRONT of each octree list (328C20
  -> 328D00 insert at the head) if the query order must match; (6) never feed a location's data while any row is 5 or 7.
  Unconfirmed: what the renderer counter counts (the exact removal tick), the pools' runtime sizes (`*(MM+0x28+4)` vs 0x35), what
  kinds 4, 5, 11 and 19 are, who reads the octree's +0xA0, whether any rider state keeps a patch or rail pointer across ticks.

### New game (pv `newGameReset`, on)

Options > Save/Load > New game on the PS2 (from the asm; the popup's wording and the card write after it unconfirmed):
cFEStateOptionsSaveLoad_onWidgetEvent 0x18D648 (row "4new game" = 3) opens cFEPopupConfirm (0x1D99C8, kT_HELPCreateNewFilePrompt);
its result 0x18D460 (event 0x16 after Yes) resets at 0x18D4F4..0x18D5E4, **profile 0 only** (the index `this+0x44` stays -1 and
clamps to 0):
- 0x147138 clears the player name; 0x149A88(0): the slot's name = kT_MEMPlayerName 1 ("PLAYER 1"), rider +0x11 = 4 (Zoe), cheat
  skin +0x12 = 0; then 0x1567B8(0x4A6CA8, 1) = 0x151600 for the ten riders: each career block as at boot (the new-career flag 1,
  the last lodge 0x11, cash / earned / visited 0, the status words, locks, attributes, relationships +0xBC1 (0x1519E0), the
  outfit records (0x151A88 / 0x1513B8), the inbox, the award bits and the reward / unlock block +0xF30..+0xF7F (0x151988: the
  owned cheat characters, which Enter Cheat 0x187D38 grants through 158618 too)).
- Kept: the records (0x535C18, defaulted only at BE creation 0x149860; they live in the options file, 0x152758), the options
  (0x535898), profiles 1 and 2. Then cFEStateProfileLoad mode 1 (93profile_load, kT_FEProfileNew: the name keyboard and a card
  write): no port equivalent yet.
- The port (web/fe-saveload.js `newGame`): before, a fresh career with the records reset and everything else kept. Now the
  records are kept, and the player name (PLAYER 1), the selected rider (Zoe), `ssx3.relationships.v1` (bank 0), the free-play
  outfits and the cheat characters (`ssx3.cheatCharacters`) reset with the career. Open: an ai-race.js instance already in
  memory keeps its relationship tables until the next course load (the AI owner's file). test-new-game.
- The PS2's popup reads kT_HELPCreateNewFilePrompt "Create a new game?  WARNING: Any unsaved changes will be lost." (its game file
  changes only when saved); the port keeps "Erase all saved progress?", because with its autosave the old save is gone at once
  (a known difference, like the autosave). Chrome: the popup, Yes -> the career empty, the records kept (OWEN 4321), PLAYER 1,
  Zoe, the cheat characters and relationships cleared.

### Relationships (pv `relAging`, on)

The PS2 ages the rider relationship records (0x155E58, after 155A50) at two places only: WS1's update when an event loads or
restarts, but only outside Conquer the Mountain (0x234894: the game type 0x535C11 != 0), and at a race's end in every game type
(233C10 at 0x233F08: the event type 0x535C10 == 0). web/ai-race.js aged at every start and restart and never at the end. Now
(pv `relAging`) `start()` ages only for a Single Event / multiplayer load, and `results()` (the results request, 408 ticks after
the finish) ages once per race (not for freestyle or rival events). Chrome and WebKit, counting the per-record notices: a CTM
Ruthless Ridge qualifier 0 at the start, 60 at the end, 60 after a second results call; a Single Event Snow Jam 60 at the restart
and 60 more at the end. The six-rider capture gates (ONLY=parity-ai/*) are unchanged (their comparer ages at the event load, the
Single Event rule). New game's reset of bank 0: see "New game".

### Known differences and open items

- **Autosave (known difference, kept):** the PS2 saves only at the memory card screens; the port autosaves the career after every
  change, a deliberate browser safeguard (the coordinator's call).
- **Reward picks (pv `rewardRng`):** 0x157080 (cards, posters, toys, art, cheats) and 0x156C70 / 0x156EE0 (gear) count the
  category's unowned items (157210 / 157518; gear: the peak flag 0x100 / 0x200 / 0x400 or 0x800 and not in the inventory), draw
  0x3177F0 and take the (draw % count)-th unowned item in index order. 0x3177F0 is the presentation generator 0x4FF018 (lui 0x50,
  addiu -0xFE8; 0x317A08 on it), not the game RNG 0x4FF030. The port's formula was the same; its draw came from a saved xorshift.
  Now `Career.random` draws web/lineup.js `presentationDraw` (the session's model of 0x4FF018, which the career messages already
  use). test-award-cascade checks the picks against the stream. The stream's state itself is a session estimate (lineup.js).
- **Relationships:** the ageing timing is ported (above); the records are kept in `ssx3.relationships.v1` (bank 0, the profile's +0xBC1) and sessionStorage (banks 1-2, RAM on the PS2).
- **New game:** see "New game" above (pv `newGameReset`).
- **An unlisted pickup** (a collectible resource not in the stage's list): 30B9A0 checks the collection id (0x30B9C4), walks the
  list (30C3E0: 153B00 sets the bit only for a listed resource), then pays unconditionally: 29CED8 (the sound), 144C98 (the course's
  peak), 151178 (the amount), 10F338 -> 119EF8 -> 150A90 (the career cash). The core pays the score's cash but queues no
  `stage_collect_events` entry, so the career save misses it. No shipped collectible hits it. JS side done (pv `unlistedPickup`:
  index 0xFFFFFFFF = cash only, web/stage-collect.js); the core side (`stage_collectible_award`: queue {found ? index : 0xFFFFFFFF,
  amount} once the id matches) is in live core sha256 7f7fcfa3 (2026-09-28, web/stage_script_gameplay.inc).
- **Seamless crossings:** on desktop `mountainRide` streams them (above); phones keep `crossWorld` (a load) until the core release.
- WS14's velocity, WS13's rival branch and the booth audio: see "Stations and heats".

## All Peak Race / All Peak Jam (not built: the next job)

- **Unlock:** the peak event of Peak 3's goal lists (0x45AAD8: Race = Gravitude, The Throne (mode 4), All Peak Race (8);
  Freestyle = Much-2-Much, Perpendiculous, Kick Doubt, Throne Jam (5), All Peak Jam (11)) opens when every standard event of
  the list has a medal and the rival challenge (The Throne / Throne Jam) has a medal. Those are lock bits 14+peak / 17+peak
  of +0x278. Help "LOCKED. Complete The Throne to unlock."
- **Flow:** Transport -> Peak 3 -> Race / Freestyle -> All Peak Race / Jam -> "Transport to this area now?" -> the start at
  The Throne (EBC3 grid slot 0).
  - The card is 68rival_pre "Rival Challenge / All Peak Race|Jam", "You've been challenged by Psymon to a Peak Race!".
  - The run is solo: 30:00 / 35:00 limit. Five station splits E -> C -> D -> A -> B, finish at Metro-City.
  - The reward list shows "All Peak Race complete!" / "All Peak Jam complete!" (awards 28 / 31). There is no pass and no
    ending movie; the career ends with the award messages. Details: peak3.md "All Peak Race / All Peak Jam" and "The end
    of the career".

## Regression test

`web/test-ctm-flow.mjs` (in npm test "test:all") drives `career-ui.js` / `character-select.js` with a stub page and
compares with `web/ctm-flow-ps2.json`. It checks the world loads, the cutscene lists queued, the rides and courses, the
career flags, the prompts (text and focused item), the result items and messages, the reward list rows and the MCOMM
screen data. The scenarios are:

- the select -> load start;
- the new-career opening;
- quit and re-enter;
- the crossing;
- the last-lodge start;
- the MCOMM quit chain;
- qualifier / semi / final results;
- the reward lists with and without the earnings goal;
- Go to this peak now? and the Peak 2 arrival;
- the lodge Return / Quit;
- old-save migration.

- the collectible and Big Challenge INFO views, the medals and the Freeride goal award (sections 12 and 13, from
  `explore-info` and `sd-goal`: the counts, the next-medal lines, the 6%, the award, the pass and message 249).

`web/test-big-challenges.mjs` 1b checks the called trick and the trick event against the `dizzy` state (C+0x34..+0x3F). The
trace also keeps the memory facts of the named states (`memory`: HUD words, status words, awards, locks, inbox, collect rows).

Re-record: capture with `tools/ctm_flow_capture.py` into `local/ps2-capture/ctm-parity/runs/`, then
`python3 tools/ctm_flow_trace.py`.

## Browser check (Chrome, headless, 640x448)

Side by side with the PS2 frames of the same steps: `local/ps2-capture/ctm-parity/sbs/sbs-1.png` (select, load, movie,
#153, #163, free ride), `sbs-2.png` (MCOMM, Quit Game, save question, title), `sbs-3.png` (rewards, results, Go to this peak
now?, the lodge prompts), `sbs-4.png` (re-entry at Happiness with the flag set, the station prompt, the lodge exit riding
from the station top), plus `mm.png` / `dd.png` / `xx.png` (MCOMM, pause, popups, Message Center, Audio). Save and load:
- A reload after the drop starts at Happiness again (flag set), with no cutscene.
- After a crossing or results, it starts at the last station (17), riding.
- After the Peak 2 arrival, the Peak 2 visited bit and the pass persist. Old saves keep their state (the migration above).

## Audio notes (for the audio agent; not changed here)

- The PS2 plays nothing new at the quit prompts. The Save game / Hints load has its own loop (not ported).
- The mail icon's HUD event 8 now fires for event posts, which the audio side may also want (the "check your messages" DJ
  line) at the next ride.
