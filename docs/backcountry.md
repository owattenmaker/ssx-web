# Backcountry: Happiness (ABC1) and the rival modes

Owner: the backcountry/rival agent (2026-09-23). This file is the contract for the other Peak 1 agents
(freestyle events, connected mountain / free ride). It is kept current while the work proceeds; sections
marked *planned* are not in the tree yet.

## Identity (ELF)

- Course table 0x43D950 row **14**: "Happiness" / short "Happiness" / code **ABC1**, world BAM, career peak 0,
  station flag 0, map region 15. Rows 15/16 are Ruthless DBC2 and The Throne EBC3.
- Location table 0x43E250: ABC1 is an event course (kind 0).
- Single Event lists it twice (PS2 frames `local/ps2-capture/nav/out-sel-metro/sample00020.png`,
  `out-freestyle/freestyle-events.png`): Race list "Snow Jam, Metro-City, **Happiness**" (game mode 4, Rival
  Time) and Freestyle list "R&B, Crow's Nest, The Junction, **Happiness Jam**" (game mode 5, Rival Points).
- Audio: CurrentCategory 28D8A0 puts course 14..16 in music category 4 BackCountry; the tWPIGD_Mix painter of
  ABC1 selects mix 1 (effects and speech 70%).

## Game mode -> GameModeMan handler (0x238160)

Single player (`0x535C11 != 2`) maps the mode byte through the jump table 0x47C0F0 into GameModeMan+4, the
handler slot of the table 0x536668 filled by 0x237CF8:

| Mode | Event | Handler | vtable | init (+0x10) | results (+0x40) |
|---|---|---|---|---|---|
| 0 | Race | 1 | 0x47CF38 | 23A108 | 23A760 |
| 1-3 | Slope style / pipe / big air | 0 | 0x47CFA0 | 238E20 | 239230 |
| **4** | **Rival Time** | **5** | 0x47CD30 | **23B6C0** | **23B8C8** |
| **5** | **Rival Points** | **6** | 0x47CCC8 | **23BB98** | **23BDB8** |
| 6-8 | Time challenge (peak race) | 4 | 0x47CD98 | 23B268 | 23B468 |
| 9-11 | Points challenge (peak jam) | 7 | 0x47CC60 | 23C0D0 | 23C2D8 (+ per-tick 23C560) |
| 12 | Free ride | 2 | 0x47CE00 | 23B170 | - |

## Rival Time (mode 4, handler 5)

Init 23B6C0 (GMM = `*(gp-0x480)`):
- +0 = 1, +0x10 = **1** (one other rider: two in all), +0x14 = 0, +0x68 = +0x6C = -1, +0x70 = +0x74 = 1
  (round 1), +0x78..+0x94 = 0 (no time limit), +0x98 = 1, +0x9C = 0.
- Lineup +0x18[slot] (character) / +0x40[slot]: memset -1; slot `146E98(profile,0)` (the player) gets
  `147398` (its character); the next slot gets the **peak rival 145750** and +0x40 = 0.
- 145750: course peak (144C78 of 0x535C08): peak 0 -> Mac (3), or Griff (5) when the player is Mac;
  peak 1 -> Nate (7) / Zoe (4); peak 2 -> Psymon (8) / Elise (6).

Results 23B8C8(slot), called per finishing rider:
- rider +0x480 set (DNF / give up): time[slot] 0x536640 = 360000 (0x57E40), gave-up 0x5366A8 = 1; else
  time = rider +0x478 (finish tick), 0x5366A8 = 0.
- When the human finishes (vtable +0x40 == 1): every unfinished rider (+0x470 < 0) gets the **race
  estimate 122D78**, then 238BF8(GMM, 10) ranks by time; +0x9C = 1 (event complete).
- **Win = place 0** (0x536730[player slot] == 0) and +0x480 == 0: rider +0x100 = 1 (celebrate), GMM +0x84 = 1,
  +0x74 = +0x70, +0x70 = 0. Otherwise rider +0x100 = 0 and the round stays (retry). No semi/final.

## Rival Points (mode 5, handler 6)

Init 23BB98: as Rival Time (two riders, peak rival in slot 1) plus the freestyle time limit:
+0x78 = 1454F8(profile, 1) x 60 and +0x88 = 1. 1454F8 reads table 0x440B38 row (course 14, round 1):
posted scores all 0, **limit 300 s** (DBC2/EBC3 rows 28/29 are also 300 s).

Results 23BDB8(slot):
- +0x480 set: score[slot] = 0, gave-up = 1. Else score = score object *(rider+0x790) +0x198.
- When all humans are done: unfinished riders get the **points estimate 122E50**:
  `s = score+1; est = s + (int)(max(remaining - 1000, 0) * s / max(113130(rider) - remaining, 1))`
  (remaining = rider +0x4D0 in cm, 113130 = the route origin distance), then 238B70(GMM, 2) ranks by score.
- Win = place 0 and not given up, as in Rival Time.

So the rival **rides and scores its own tricks** (its own score object); there are no posted scores.

## Rolling start (no countdown)

- 234AD0 (the objectives overlay's Continue) sends event kinds 4..6 (free ride, time challenges incl. Rival Time,
  points challenges incl. Rival Points) to 233AA0 instead of `113B10(Countdown)`; the clock goes PreRace -> Race.
- The ready state (`happiness-ready.p2s`, phase 3, total ticks 0) already holds both riders at their start spots in
  control 0 / motion 0 **with a start velocity** (human (579.2, 378.3, 0) cm/s, Mac (654.3, 200.1, 0)); on game tick 0
  both leave the ground (motion 1). ARMSX2 capture `bc-race-idle`: record 0 phase 3, record 1 phase 5 / race tick 1.
- The Cross that closes the overlay is still held on the first race ticks: control 0 requests control 2 on the
  departure tick itself (1162C8), then control 2 in the air until the release (-> control 5).
- Browser: `tools/export_backcountry.py event-start` writes `event-start.json` with `rolling_start: true` from the ready
  state; `tools/generate_event_seed.py` compiles `rollingStart`; `begin_event_rider` / `npc_seed_rider` keep control 0 and
  `start_event` skips the start controller; `begin_event_clock` leaves the clock in PreRace and `race_begin` selects Race
  before tick 0's update (race tick 1 after tick 0, as on the PS2).
- The rival's provider steers on tick 0 (no countdown ticks before it): the retained route's previous lookahead
  (`route_state.previous_lookahead_point`) is exported into `original_reset.event_route`.

## Rival computer rider

- Documents: `web/public/assets/ABC1/rivals.json` = `{documents: {race|jam: {mac|griff: npc-riders document}}}`
  (`tools/export_backcountry.py rivals`), from four ready states: Zoe (-> Mac) and Mac (-> Griff), Happiness (Rival
  Time) and Happiness Jam (Rival Points). They differ in `world.rank_mode` (1 time, 2 score), the rider's
  `score_state.role_e00` (0 / 2: the jam rival plans tricks) and the event variant 4 / 5. `web/ai-race.js prepareRival`
  picks the document by event mode and the human's base character (Mac -> Griff; the browser's Sam takes Mac's slot).
- The rival runs the unchanged NPC provider 0x10A768 (docs/ai-racers.md) with two riders (`createAiRacers` count =
  riders + 1).
- **Relationships during the race (2026-09-28, pv `rivalRelations`).** Rider-pair reactions change the relationship tables
  (0x155BF0), and 10F560 flags a pair record for the 115D48 peer reaction once relationship(a, b) >= 2. The rival events used
  to keep the document's levels all race. `web/ai-race.js` now keeps session tables for them too, starting from the fresh
  table in ARA1/lineup-sessions.json. PS2 bc-race-tuck2: Mac's record of Zoe reaches level 2 at 599 (soft attacks at 397 / 595 /
  599). The human's 1161 idle reaction is then 319 (look at Mac), not 317. The run is exact to the end in the comparer and in
  the page (test-rival-page `abc1-tuck2`).
- **Start state = the ready state, including the game RNG (2026-09-26).** The rival's record (position, start velocity,
  control 0 / motion 0, provider and route state, bank-2 stats) does not depend on the human: the Ruthless ready states of
  Zoe, Moby (direct menu path) and the reference Zoe state give the same Nate record (only allocation addresses and the
  human's own pair weight differ). What does depend on the human is the **game RNG 0x4FF030 at race tick 0**: seeded 0 before
  the load, then a few draws (4 or 5 in the reference states), and the provider draws from it on tick 0. The page raced from the core seed's RNG (a glide
  state's: ABC1 928, DBC2 624, EBC3 436 draws), so every rival left the PS2 at its first draws that mattered: Nate at DBC2
  tick 132 (he then never landed in the 1413 crash, so no reset placement at 1656 and no painter re-seed reached the human,
  who left at ~2650: docs/weather.md section 10), Mac at ABC1 535, Psymon at EBC3 176.
  - `tools/export_backcountry.py rivals` writes each document's `anchor_rng` (`{seed 0, draws, words, menu_extra}`), the
    rule `load_draws` and, for Ruthless, `direct_path` (the PS2 evidence below). `web/lineup.js rivalAnchorWords` gives the
    words; `web/ai-race.js prepareRival` sets them (`racers.setAnchorRng`, applied at race tick 0 of every start and restart),
    and `createAiRace` sets the default document's for a start without prepare (`ssxQA.start`).
  - Counts on the PS2: Happiness Zoe 4, Mac 5; Ruthless Nate 4; The Throne Psymon 4 (their own ready states). A player's
    direct path (Select Character -> Setup -> Select Peak -> event) follows the lineups' rule (Zoe -1, Moby +1, a cheat skin
    +4 around a per-course base): **Ruthless Zoe 3, Nate 4, Moby 5**, checked with two new states derived from
    `characters/{zoe,moby}/select.p2s` through ruthless-nate-ready's menu script (`local/ps2-capture/nav/p2/
    out-ruthless-{zoe,moby}-direct/ready.p2s`, `ruthless-direct.provenance.json`, `scripts/derive_ruthless_direct.py`).
    The reference Zoe states of Ruthless and The Throne (`ruthless[-jam]-ready`, `the-throne[-jam]-ready`: peak-1-selection,
    pass patched, Select Peak left and entered again) hold one draw more (`menu_extra` 1; the lineups'
    `reference_anchor_extra` on Ruthless Ridge is the same path). The browser starts a document's own human from its ready
    state (so the PS2 captures replay exactly) and any other human from the direct path.
  - `web/test-rival-page.mjs` (npm test, headless Chrome): the real Single Event path (`ui.startSingleEvent` -> load ->
    prepareRival -> objectives -> `careerUI.play`) replayed with the captures' pads stays as exact as the gate: Ruthless
    human 4435 / Nate 5141 / RNG 4581 (Nate's 1656 reset placement and the human's surface-18 crash at 3911 included; was
    3910 / 6425 / 3932), Happiness all 6700, The Throne all 2900,
    Happiness Jam 389 / 389 / 401. `web/test-rival-mode.mjs` checks every document's anchor words, the eight rival captures'
    record-0 RNG and the rule (both documents of a course agree for every other human).

## HUD, speech, music, FX

- HUD flags 0x478078[4] = [5] = 0x1530C047 (race HUD: place "1ST/2", clock, score, progress); 1EA930 adds the OPPONENT
  line (0x08000000) for event kind 6 only. Rival Time = the race HUD (clock counting up). Rival Points = the slope-style
  layout (career-ui.js `slopeHud`): place by score (rank mode 2), countdown clock from 5:00, OPPONENT +N, progress.
  PS2 frames: `local/ps2-capture/nav/bc/out-start/sample00360.png` (race), `out-jam-start/sample00360.png` (jam).
- Audio 2872A8 (event start without countdown, kinds 5/6): song event 0 (or a new playlist song), the GO tick; for the
  rival modes outside multiplayer the rider speech BC_Challenge is queued (0x5788: rival slot 1 speaks to the human,
  2A1138), otherwise DJ Radio BIG intro; then the artist intro (0x5778 = 1). Finish 286EA0 -> 2A4078: kind 5 (Rival Time)
  PA_Finish_Line, kind 6 in round 1 PA_Rider_Position. `game-audio.js rivalStart`, `finish({challengeKind})`.
- Locator beam over the rival (RFX+0xB00, update 0x2E39D8, draw 0x2E3AF8; `web/rival-beam.js createRivalBeam`): an 8-vertex
  strip along the camera up from the rival's bone 5 (base alpha 0, +130 cm and +20130 cm at the colour, +20260 cm alpha 0),
  half width 85 cm along the camera right, texture 'beam' (renderer+0xFFC, white with a triangular alpha across S), colour
  (a 0.5, r 1, g 0, b 0) = the component's initial +0x28 (the pulse update runs for the human only), additive. Only in rival
  modes on courses 14..16, single player. PS2 vs web: `bc-race-idle.tick401.png` vs scratchpad `peak1/idle-tick401.png`.
- Relationship icon '!' (RFX+0xAF0, update 0x2D4C08 per game tick, draw 0x2D5048; `createRiderIcons`, all AI races): level
  = the rider's relationship record about the human (155B50; 3 for the peak rival), colour by level (>=4 red, 3 orange,
  2 yellow, else white), visibility ramps +-0.1 (level >= 2) / +-1/15, pulse tables (41-tick pop on a level change, 31-tick
  loop at level >= 4, 24-tick pop on a rise; tables 0x488EA0.., 0x488F48.., 0x488FF0.., 0x489070.., 0x4890F0.., 0x489150..),
  quad from bone 5 + 35 cm up, 18 (sx + 0.003 z) wide and 18 (sy + 0.003 z) tall, visible 0 < z < 20 m, fading past 15 m,
  texture 'exlm' (renderer+0xFAC), alpha blended. Both textures: `tools/export_rival_fx.py`.
- Finish: Rival Time shows the race FINISH banner; Rival Points the freestyle FINISH! banner with the score, then the
  finishov panel with only "Nth place  N pts" (no run label), place against the rival (PS2 `nav/bc/out-jam-finish/
  sample04620.png`). Results: "<course> - Race|Jam / Single Event Results", Rank / Riders / Time|Score, "Sorry, you didn't
  win." on a loss (PS2 `out-tuck-finish2/sample00600.png`, `out-jam-finish/sample04860.png`).

## Data formats (for the other agents)

- `tools/locations.py` entry `ABC1` (event `backcountry`, course index 14, location id 14, sky ASKY; states
  `happiness-ready.p2s` (countdown/anchor/ready), `happiness-glide.p2s` (tick 340)). Event residency ABC1, ABC1_A,
  TRANSP, ASKY.
- `web/public/assets/ABC1/`: the course package like BRA2/BHP1 (`tools/prepare_location.py ABC1`), plus `rivals.json`,
  `npc-riders.json` (race/Mac), set pieces (`UVSCROLL`, `LIVECOMP`, `PARTICLES`, `SECTIONS`, `STAGE`, `FLAGS`, `CROWD`).
  `courses.json` has one ABC1 entry (event `backcountry`); `main.js` lists it twice in Single Event ("Happiness" with
  `rivalMode: 4`, "Happiness Jam" with `rivalMode: 5`).
- `web/rival-mode.js`: the rules above as pure functions (`rivalCharacter`, `rivalResult`, `estimateTicks`,
  `estimatePoints`, `hudFlags`, `timeLimitTicks`, `objectives`), used by `career-ui.js` / `career.js rivalResult`.
- For the mountain agent: a free-ride entry into ABC1 is event kind 4 (no rival, no rolling-start seed needed: the rider
  arrives riding). The rival challenge itself always loads its own event world (course row 14: ABC1, ABC1_A).

## PS2 ground truth and gates

Savestates (`local/reference/pcsx2/`, derived with `tools/ps2_navigate.py` from `snow-jam-selection.p2s` /
`out-freestyle/freestyle-events.p2s` / `characters/mac/select.p2s`; scripts `local/ps2-capture/nav/scripts/happiness-*.json`):
`happiness-ready.p2s` (Zoe vs Mac, Rival Time, phase 3), `happiness-jam-ready.p2s` (Rival Points), `happiness-mac-ready.p2s`
and `happiness-jam-mac-ready.p2s` (Mac vs Griff), `happiness-glide.p2s` (tick 340), `happiness-glide-600.p2s`,
`happiness-race-1.p2s` / `-22.p2s`. `tools/clean_capture_state.py` turns a kept capture savestate back into the plain game
(finish/results frames were ridden from `setpieces-abc1/full.tick11600` and `bc-jam-tricks2.tick13601`).

Captures (`local/ps2-capture/runs/bc/`, `tools/ps2_capture.py build ... --ai-state`, not isolated unless noted; the pad scripts
come from `web/bc-autopilot.mjs`, which steers the browser rider along the route with the PS2 pad decode and dead zone, so a
script replayed on ARMSX2 follows the same line while the physics is exact). `PS2_CAPTURE_SCALAR=0.1..0.4` slows the emulator so
the PINE reader keeps up with the 32 KiB records on a loaded host.

| Capture | Ticks | Gate (`web/test-ps2-captures.mjs` aiCases) |
|---|---|---|
| `bc/bc-race-idle` | 3001 | human through 1031, Mac through 2342, RNG through 1340, ranks exact, pair records through 1031 |
| `bc/bc-race-tuck` | 12400 | human through 5228, Mac through 6622, RNG through 5288, ranks exact, pair records through 5231 |
| `setpieces-abc1/full` | 12400 | kept states every 400 ticks: `test-stage-world.mjs` ABC1, 141 particle effects word-exact through 5202 |
| real game path (`test-rival-page.mjs`) | the captures above | the page's Single Event path from the menus' call: bc-race-tuck all 6700, bc-jam-tricks2 389 / 389 / 401 (as `compare-ai-capture.mjs --document` with the jam document); Ruthless and The Throne in docs/peak2.md, docs/peak3.md |

Divergences (physics, not rival rules): idle 1032 = a soft collision with `mdl_ABC1_treehevbump_0_000129` whose second
instance contact on the PS2 picks another face (normal (-0.96, -0.24, 0.12), closing 37 cm/s) one tick before the browser; tuck
5229 = the landing after an airborne soft collision; `bc-jam-tricks2` 390 = a human/Mac pair contact while Mac is in board
press (control 1): both riders end 0.3 cm apart from the PS2 (107888 translation).
