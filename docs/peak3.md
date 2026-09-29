# Peak 3 (area E): events, the streamed Peak 3 mountain, the end of the career

Owner: the Peak 3 agent (2026-09-25). Peak 1 is the template (docs/peak-mountain.md, docs/slopestyle-bigair.md,
docs/backcountry.md, docs/career-events.md, docs/ctm-flow.md); this file lists what is different or new on Peak 3 and
the state of the port. A Peak 2 agent works on areas C / D in parallel (its registry entries sit next to these).
Every fact comes from SLUS_207.72 (gp = 0x4A30F0), the disc, or ARMSX2 savestates derived for this work.

## 1. Inventory

### Locations (ELF location table 0x43E250, course table 0x43D950, residency 0x442488)

| Code | Name | Course / location id | Kind | Career peak (+0x54) | SDB track | Event residency (row) | Sky |
|---|---|---|---|---|---|---|---|
| ERA5 | Gravitude | 4 | race (mode 0) | 2 | 45 | ERA5, E_ERA5, ERA5_C | ESKY |
| ESS3 | Kick Doubt | 7 | slope style (mode 1) | 2 | 48 | ESS3, E_ESS3 | ESKY |
| EBA3 | Much-2-Much | 10 | big air (mode 3) | 2 | 41 | EBA3, E_EBA3 | ESKY |
| EHP3 | Perpendiculous | 13 | super pipe (mode 2) | 2 | 44 | EHP3, E_EHP3 | ESKY |
| EBC3 | The Throne | 16 | backcountry (rival modes 4 / 5) | 2 | 42 | EBC3, EBC3_E | ESKY |
| E | Black Top Station | 21 | hub (station flag 1) | 2 | 36 | E, E_ERA5, E_ESS3, E_EHP3, E_EBA3, EBC3_E | ESKY |

- Connectors (kind 2): EBC3_E 22 (track 43), E_ERA5 23 (39), E_ESS3 24 (40), E_EHP3 25 (38), E_EBA3 26 (37),
  ERA5_C 27 (46). ERA5_C leads down into Peak 2's Yellow Mid Station C (row 19: C, C_CRA3, C_CHP2, C_CBA2, **ERA5_C**):
  Gravitude is the only road from Peak 3 to the rest of the mountain. ESKY is location 48 (track 47).
- Every race residency row also holds TRANSP (43); streaming table 0x442168 rows 0..48 are the locations by id
  (class 0 location, 1 TRANSP, 2 sky), as on Peak 1.
- The map region (+0x60) of every Peak 3 course is 2; ERA5's world string is BAM like every course.
- Verified in the PS2 event savestates (tools/ps2_navigate.py inspect): Kick Doubt resident ESS3 (track 48), E_ESS3 (40),
  TRANSP, ESKY; each event keeps exactly its residency row.

### Events, rules and tables (career.json from tools/export_career.py; addresses in docs/career-events.md)

| Event | Game mode / handler | Riders | Time limit | Tables |
|---|---|---|---|---|
| Gravitude race | 0 / 1 (23A108, results 23A760) | 6 (5 computer riders) | none | cash 0x4405A0 row 4: $100,000 / 80,000 / 40,000 / 20,000; platinum 160 s |
| Kick Doubt slope style | 1 / 0 (238E20, 239230) | player + 1 riding opponent (+ 4 posted) | 60 s (row 7) | 0x440B38 posted 650,000 / 390,000 / 225,000 / 140,000 / 95,000 (round 1); final 750,000..98,000; cash row 7 $100,000..10,000; platinum 12,000 (x100) |
| Much-2-Much big air | 3 / 0 | player (5 posted) | 60 s (row 10) | posted 160,000 / 110,000 / 80,000 / 62,000 / 45,000; final 180,000..; cash row 10; platinum 2,000 (x100) |
| Perpendiculous super pipe | 2 / 0 | player (5 posted) | 150 s (row 13) | posted 500,000 / 300,000 / 220,000 / 170,000 / 120,000; final 540,000..; cash row 13; platinum 9,000 (x100) |
| The Throne (rival time) | 4 / 5 (23B6C0, 23B8C8) | player + peak rival | none | cash row 16: $100,000 / 50,000; platinum 155 s |
| Throne Jam (rival points) | 5 / 6 (23BB98, 23BDB8) | player + peak rival | 300 s (row 29) | platinum 6,000 (x100) |
| All Peak Race | 8 / 4 (time challenge) | solo | tiers 1800 / 1637 / 1500 s | 0x440D18 rows: splits 190 / 470 / 820 / 1147 / 1490 s (tier 1), cash 250 / 500 / 1000 (x100) |
| All Peak Jam | 11 / 7 (points challenge) | solo | 2100 s | tiers 10,000 / 14,000 / 17,500 (x100 points), five split columns, cash 250 / 500 / 1000 (x100) |

- **Goal lists 0x45AAD8** (CTM Transport order): Race = Gravitude, The Throne (mode 4), All Peak Race (8);
  Freestyle = Much-2-Much, Perpendiculous, Kick Doubt, Throne Jam (5), All Peak Jam (11). Peak 3 has one standard race.
- **Single Event lists** (PS2 frames `local/ps2-capture/peak3/nav/out-race-events/race-events.png`,
  `out-fs-events/fs-events.png`) differ from the goal order: Race = Gravitude, The Throne; Freestyle = **Kick Doubt,
  Much-2-Much, Perpendiculous, Throne Jam** (slope style, big air, pipe, as Peak 1's R&B, Crow's Nest, The Junction).
  The All Peak events are not listed there without a career that opened them.
- **Peak rival** 0x145750: Psymon (8), or Elise (6) when the player is Psymon. PS2 card: "Face off against Psymon in a
  Rival Challenge!" / "The first rider to the bottom of Backcountry wins." (Jam: "... with more points than your rival.").
- **Collectible medals** 0x45AFE8 row 3: 122 / 90 / 80 / 70; Big Challenge medals row 3: 21 / 18 / 16 / 12.
- **Earnings goal** Peak 3: $1,000,000.

### PS2 savestates (derived; `local/reference/pcsx2/`, scripts and runs in `local/ps2-capture/peak3/nav/`)

- **Unlock:** a copy of `peak-1-selection.p2s` with every character block's lock word cleared of bits 6..19
  (profile 0x4A6CA8 + profile x 0x9B50 + character x 0xF88, +0x278; 145F90 reads it: bits 12/13 = Peak 2/3 pass,
  6+p / 9+p rival, 14+p / 17+p peak events). The Select Peak list reads the locks when the screen is entered, so the
  script backs out (Triangle) and re-enters (Cross) before Peak 3 (`unlocked-selection.p2s`, `scripts/p3-reenter.json`
  -> `peak-3-mode.p2s` = Select Mode of Peak 3, Zoe).
- Race list -> Cross = `the-throne-ready.p2s` (the saved list state already held the Down of the next script segment,
  so the list focus was The Throne; Gravitude comes with Up: `scripts/gravitude-load.json`). Freestyle list:
  Kick Doubt, then Down 1/2/3 = Much-2-Much, Perpendiculous, Throne Jam.
- Each ready state (phase 3, the overlay) -> `ready-start-360.json` (Cross) -> anchor (sample 40) and glides.

| State | Tick | Riders | Notes |
|---|---|---|---|
| `gravitude-ready` / `-countdown-anchor` / `-glide` | 0 / 18 (countdown 162) / 598 | Zoe, Nate, Allegra, Moby, Mac, Luther | glide = 620-sample state: every rider grounded (at 338 they are airborne) |
| `kick-doubt-ready` / `-countdown-anchor` / `-glide` | 0 / 18 / 379 | Zoe + the riding opponent | overlay: Elise 661,700, Kaori 403,260, Griff 224,100; record 750,000 |
| `much-2-much-ready` / `-glide` | 0 / 378 | Zoe | overlay Moby 162,880, Elise 118,740, Kaori 79,680; record 185,000 |
| `perpendiculous-ready` / `-glide` | 0 / 339 | Zoe | overlay Moby 509,000, Elise 310,200, Kaori 219,120; record 500,000 |
| `the-throne-ready` / `-glide` | 0 (rolling start) / 380 | Zoe vs Psymon | at 341 the human is still in control 3 (landing); 380 is control 0 |
| `the-throne-jam-ready` | 0 | Zoe vs Psymon | Rival Points |

### The streamed Peak 3 world (static research; notes and scripts in the session scratchpad `peak3/inventory-static.md`)

**Trigger volumes** (slot-2 programs, builtin 68 = 0x302210; key0 through 144CE0 map id -> course, key2 -> 0x445E40 entry
mode {2,3,4,5,1,1,1,0}; each program hides itself (builtin 29) and unhides its partner (58), as on Peak 1):

| Connector | Unload (action 0 -> 22CEA8) | Load (action 2 -> 22D088) | Destination, entry mode |
|---|---|---|---|
| E_ERA5 | rid 106 | rid 98 | ERA5, 2 |
| E_ESS3 | rid 61 | rid 5 | ESS3, 3 |
| E_EBA3 | rid 66 | rid 16 | EBA3, 4 |
| E_EHP3 | rid 59 | rid 42 | EHP3, 5 |
| EBC3_E | rid 122 | rid 47 | E, 1 |
| ERA5_C | rid 113 | rid 78 | **C (Yellow Mid Station, course 19, Peak 2)**, 1 |

- ERA5_C rid 16 `mdl_ERA5_C_skybox_trigger_1000` (builtin 68 action 5): the dome switch ESKY -> CSKY. No other Peak 3
  location has a skybox trigger.
- Hub E: `mdl_E_NIS_Lodge_0` rid 82 (action 4, lodge door), `mdl_E_NIS_Transport_0` rid 138 (action 3, transport booth).
- RaceRideState gates (builtin 67 = 0x302048 -> 22D6C8): ERA5 rid 989 (k0 0), ESS3 rid 1526 (k0 1), EBA3 rid 735 (k0 2),
  EHP3 rid 255 (k0 3). EBC3 has three FreeRideState volumes (k0 4 -> value 1, rejected).
- Stage programs / handler instances: E 60 / 120, E_EBA3 14 / 33, E_EHP3 15 / 35, E_ERA5 14 / 33, E_ESS3 14 / 16,
  EBA3 121 / 192, EBC3 116 / 547, EBC3_E 46 / 77, EHP3 100 / 197, ERA5 285 / 1053, ERA5_C 40 / 83, ESS3 179 / 479.

**Counts:** Session points (0x440770 +0x18): ERA5 7, **ESS3 6**, EBA3 2, EHP3 2, EBC3 5, E 1. Collectibles (0x43FA70 via
153350): ERA5 30, ESS3 30, EBA3 5, EHP3 8, EBC3 44, E 5 (= the stages' builtin-38 lists; 122 in all = the platinum of the
Peak 3 collectible medal). Big Challenges (0x43EE10): ERA5 4, ESS3 5, EBA3 4, EHP3 3, EBC3 5 (21).

**Painters** (kind-15 records, one per location): every E location has Fog, ScreenTint, Sun, Lighting and Weather (breath);
glare (type 6) on E, E_EBA3, E_ERA5, EBA3, EBC3, ERA5, ESS3; EBC3 and E_ERA5 have no Mix / Ambience; ERA5_C has two Lighting
payloads (C bank and E bank). ESKY's record is empty. Default irradiance bank 22E180 (jump table 0x47B4E0): EPBR1 for courses
4, 7, 10, 13, 16, 21.

**Freeride Transport list** 0x478D38, Peak 3: The Throne, Black Station, Gravitude, Much-2-Much, Perpendiculous, Kick Doubt.

**Read-time estimates** (last SDB chunk / 20.5 KB per tick): E 58, E_EBA3 31, E_EHP3 23, E_ERA5 20, E_ESS3 12, EBA3 99,
EBC3 236, EBC3_E 26, EHP3 74, ERA5 213, ERA5_C 35, ESS3 185 (ESKY: the measured sky value 30).

### All Peak Race / All Peak Jam (the career finale)

- Handlers 4 / 7 (as Peak 1 Race / Jam). Init sets GMM+0x68 = 16: the run starts at **The Throne** (EBC3 grid slot 0,
  (-360807.2, 319511.6, 739662.4) cm, the 11DE60(rider, 0, 1) backcountry placement). GMM+0x6C = 4 has no reader.
- Splits fire at stations (23B5F8 / 23C560 when the current course is 17..21): five for modes 8 / 11, so the route crosses
  five stations: **E -> C -> D -> A -> B** (inferred from the connectors: EBC3 -> EBC3_E -> E -> E_ERA5 -> ERA5 -> ERA5_C -> C ->
  C_CRA3 -> CRA3 -> CRA3_D -> D -> D_DRA4 -> DRA4 -> DRA4_A -> A -> A_ARA1 -> ARA1 -> ARA1_B -> B -> B_BRA2 -> BRA2).
- Finish: 10E5D8 accepts a finish only on course 1 or 5..13; the goal lists carry course 1 for both, so **both finish at
  Metro-City**. (web/peak-run.js had GMM+0x6C as the finish for modes 7/8/10/11; see section 3.)
- Tables 0x440D18: race 1800 / 1637 / 1500 s (splits 190/470/820/1147/1490, 179/456/778/1085/1376, 175/440/680/1000/1280),
  cash $25,000 / 50,000 / 100,000; jam 1,000,000 / 1,400,000 / 1,750,000 points in 2100 s, same cash.
- Card (1FD190 "68rival_pre"): "Rival Challenge / All Peak Race|Jam", "You've been challenged by Psymon to a Peak Race!"
  (145750 of the current course's peak), "Race from the top of Backcountry to the bottom of Metro-City." (the Peak 1 string),
  "Beat the rival's time to win the event." (mode 8 never offers a pass), "Time to beat: 30:00".
- So the finale needs the whole mountain streamed: Peak 3, the Peak 2 stations and courses on the route, Peak 1.

### The end of the career

- Peak 3 goals give cheat characters (159CD0, table 0x45AA40): race Unknown Rider, freestyle Churchill, explore Canhuck,
  earnings Cudmore; all Peak 3 goals: NW Legend; all goals on all peaks: Gutless.
- "Mountain conquered!" (1577E0): golds in every standard event, any medal in the rival and peak events, freeride medals
  and the stat-medal sum >= 24 on all three peaks -> Far East Myth. There is no ending cutscene or movie; the ending is the
  award messages ("Peak 3 conquered!", "All Peak Race complete!", "All Peak Jam complete!").
- First arrival (world state 10, 0x2351A8): only the first CTM arrival at The Throne plays FMV 31 (EBC3.mp4) then
  `ebc3_heli_arr` (#127) and `heli_arrb_<char>`; later arrivals with the transport flag play the two lists without the movie.
  Black Top Station has no arrival cinematic of its own.

## 2. Single Event: the five Peak 3 locations (2026-09-25)

**Playable:** Select Peak > Peak 3 is open (web/fe-event-select.js `SINGLE_EVENT_OPEN_PEAKS`; the PS2 asks for a CTM Peak 3
pass, the port opens it because its events are ported). Race: Gravitude, The Throne; Freestyle: Kick Doubt, Much-2-Much,
Perpendiculous, Throne Jam (the Map LUI row order, which matches the PS2 list; Jam rows use the course's short name). Each
event loads its own course package, plays the objectives card, the countdown (rolling start at The Throne), its HUD and
the results, like the Peak 1 events. Checked in Chrome (scratchpad `peak3/qa/event.mjs COURSE[:rivalMode]`).

**Pipeline per location** (all under git-ignored dirs):
1. `tools/locations.py` entries (ERA5, ESS3, EBA3, EHP3, EBC3; states named gravitude-*, kick-doubt-*, much-2-much-*,
   perpendiculous-*, the-throne-*).
2. `tools/prepare_location.py LOC` (disc + savestate evidence, the web package, courses.json).
3. `local/peak3-logs/post.sh LOC` (the Peak 2 agent's chain: stage scripts, splines, set pieces, flags, UV scroll,
   LiveComp, particles + their textures, stage world, attached set pieces, crowd, progress meter, section ready state,
   far painter, sections from `local/ps2-capture/runs/peak3/<name>-full.*`, computer riders / rivals, the freestyle
   configuration, web/prepare.py again).
4. `sh web/build-core.sh` (event, glide and set-piece seeds are compiled in; the Peak 3 blocks in
   web/stage_script_gameplay.inc, web/set_piece_gameplay.inc and web/attached_setpieces.inc are guarded by
   `__has_include` / `BROWSER_STAGE_HAVE_<CODE>`).

**Per-location facts from the exports:**
- Kick Doubt: the riding opponent of the Quick Play anchor is Moby (slot 1); checkpoints {70, 134959.88} and
  {60, 65396.14} (+70 s, +60 s); 60 s limit (3600 ticks); crowd 69 + 21 flashes; 30 collectibles in the stage world.
- Gravitude: grid Zoe, Nate, Allegra, Moby, Mac, Luther (tools/audit_rider_assemblies.py now names any roster character
  by web/public/assets/riders.json and reads its rig from local/assets/native/CHARACTERS).
- Perpendiculous: 150 s (9000 ticks); three `spline_EHP3_slide_70..72` rail records exist twice in EE RAM (an unpatched
  disc copy and the live one); tools/export_rail_runtime_flags.py keeps the patched one.
- The Throne: `rivals.json` has race/jam documents for Psymon (Zoe's states) and Elise (Psymon's states:
  `the-throne-psymon-ready.p2s`, `the-throne-jam-psymon-ready.p2s`); tools/export_backcountry.py
  `RIVAL_STATES_BY_LOCATION`; web/ai-race.js picks the rival with rival-mode.js `rivalCharacter(course.peak, human)`.
- The rival card wraps a headline wider than the box ("Face off against Psymon in a Rival" / "Challenge!", as the PS2).

## 3. Exactness gates (web/test-ps2-captures.mjs, `peak3/*`)

Captures: `local/ps2-capture/runs/peak3/cap.sh NAME STATE FRAMES` (tuck scripts `scripts/p3-<name>-full.json`, --isolate,
section / chunk / viewer watches, kept states every 400 records); the gates are the first runs
(`local/peak3-logs/first_run.py`).

| Gate | Ticks | Exact | Open |
|---|---|---|---|
| peak3/much-2-much-event-tuck | 3148 | physics, bones, score, boost to the end | - |
| peak3/perpendiculous-event-tuck | 4385 | physics to the end; score/boost through 3995; bones through 3866 | 3867 posed root ~3 cm; 3996 the PS2 clears the boost meter |
| peak3/kick-doubt-event-tuck | 3999 | through 3781 (score 3780) | the 1:00 time-out: PS2 control 10 one tick earlier |
| peak3/gravitude-event-tuck | 11999 | through 6883 (score, bones too) | 6884 the browser leaves the ground one tick early (2964, the fall-reset plane found only once the rider+0x860 query scope holds it, is fixed) |
| peak3/the-throne-tuck | 11999 | through 696 (score 698) | 697 first instance contact closing speed 1e-4 off (pose-dependent; bones are the rolling-start seed's, as bc-race-idle) |
| peak3/gravitude-race-ai | 1504 | through 1504 (score, bones) | 1505 a passive landing with a soft collision: the PS2 enters control 3, the browser stays in control 0 |
| peak3/fr-throne-unload (PEAK3 world) | 1398 | through 14468 (the EBC3 Unload at 14422 and the start of the E reads) | 14469 the first control-3 tick: the PS2's 11B3F8 limit already follows the falling crouch (2099.448 vs 2099.865), the 0.27 cm step follows from it |

Computer-rider gates (the AI table): peak3/the-throne-race-idle (human, Psymon, RNG, ranks and pair records exact for 2900
ticks), peak3/gravitude-race-ai (human 1504; Nate / Allegra 1128, the others 1230..1477; RNG 1194). The real game path
(`web/test-rival-page.mjs`: Single Event > The Throne in the page) replays the-throne-race-idle exact for all 2900 ticks too
since the page starts Psymon from the ready state's game RNG (2026-09-26, docs/backcountry.md "Rival computer rider"; before,
Psymon left the PS2 at 176 and the human at 187).

An airborne soft collision (control 3) landing into control 0 used to diverge on the next tick in two of these captures
(Kick Doubt 2236, Gravitude 625); fixed in the core (see HANDOFF).

## 4. The connected Peak 3 world (PEAK3)

**Tooling generalized by peak** (Peak 1's outputs unchanged; the Peak 2 agent builds PEAK2 with the same switches):
- `tools/export_peak_world.py --peak 3 [--no-setpieces]`: `local/assets/native/PEAK3/<LOC>/`, `web/public/assets/PEAK3/`
  (12 locations: E, the five E connectors, ERA5_C, the five courses), `peak.json` (`peak: 3, name: 'PEAK3'`; residency rows
  4, 7, 10, 13, 16, 21; streaming rows with estimated read times), environment (course scalars from the ERA5 package),
  painters, light glows, AIP path banks, merged set pieces (`SETPIECES/`, from the five event exports + the Peak 3 stage
  tables). A new peak needs two passes: `--no-setpieces` first (the stage seed needs the packages), then
  `tools/export_peak_stage.py --peak 3` (`web/generated/peak3_stage_seed.hpp`, `browser_stage_peak3`, collectible award
  2000), then the full run.
- `tools/export_peak_instances.py --peak 3`, `tools/export_peak_sections.py --peak 3`, `tools/export_peak_rail_flags.py
  --peak 3` read the Peak 3 free-ride states (`local/ps2-capture/peak3/*.p2s`, kept states of `runs/peak3/fr-*`), the rail
  flags also the event glides; `tools/export_peak_seed.py --peak 3 --state <abs path> --region EBC3` writes the PEAK3
  glide seed (web/generate-controllers.py compiles its ground state into the core: rebuild after every seed change, or a
  compare runs the old ground state at the new position). The current seed is
  `local/ps2-capture/peak3/derived/fr-ebc3-14302.p2s` (`tools/clean_capture_state.py` of `runs/peak3/fr-throne-late.tick14302.p2s`:
  hooks restored, arena zeroed), the baseline of the fr-throne-unload gate (`runs/peak3/fr-throne-unload.bin` = fr-throne-late from
  record 14302). The earlier seed fr-ebc3-late (8701; every control rate at 0.0333, which looks like the tick after an
  11D660 placement, not verified) leaves on its first tick (lateral velocity change 3.7 vs 9.2 cm/s), so it is not used.
- `tools/export_peak_missions.py`: the Peak 3 stages' 21 Big Challenge missions join the one mission seed (a track's
  missions are built only when its stage is loaded).
- Core: `web/streamed_world.hpp` `browser_streamed_world()` ("PEAK1".."PEAK3") replaces the `== "PEAK1"` checks
  (section residency, per-track collectible rows, stage track setup / teardown, Big Challenges); `StageCoursePEAK3`.
- Browser: `web/free-ride.js` `COURSE_PEAK` / `peakWorldOf(course)` / `PEAK_DEFAULT_STATION` / `PEAK_BASE_COURSE` /
  `peakRunWorld(mode)`; `?course=PEAK3[&peakCourse=N]` (default 21, Black Top Station); the world slices carry the world
  name (`peak-world-batches.js`, from the package root); `stage-collect.js` `PEAK3_COLLECT_TRACKS`
  {36: 21, 41: 10, 42: 16, 44: 13, 45: 4, 48: 7}; crowds on the Peak 3 course groups.

**PS2 free-ride states** (derived; `local/ps2-capture/peak3/`):
- From a CTM MCOMM Transport state (Peak 1 free ride) with every profile's lock word cleared: the Transport list reads
  the passes when Transport opens, so the script backs out to MCOMM and re-enters. Select Peak on another peak asks "Go to
  this peak now?" (OVAMER 0x079ACB07); Yes plays the first-arrival cinematic at that peak's backcountry. Peak 3: heli
  interior with "Loading...", the EBC3 movie ("PEAK 3"), the heli arrival, then free ride at The Throne
  (`fr-ebc3-arrival.p2s`, resident EBC3, EBC3_E, TRANSP, ESKY; HUD 0/44, $0).
- MCOMM Transport > Peak 3 > Freeride > Black Station / Gravitude: `fr-e-arrival` / `fr-e-rode` (E and its five
  connectors + EBC3_E resident), `fr-era5-arrival` / `fr-era5-rode` (ERA5, E_ERA5, ERA5_C). The same path into Kick
  Doubt / Much-2-Much / Perpendiculous opened their CTM events instead (`ctm-events/*-card.p2s`: the career Kick Doubt
  heat 1 posts Psymon, the peak rival, first).
- For the Peak 2 agent: `for-peak2/fr-dbc2-arrival.p2s` and `runs/peak3/for-peak2/fr-throne-neutral.*` are the Peak 2
  first arrival at Ruthless (DBC2) and a 9000-tick neutral free ride there.
- Collectibles pay $2,000 on Peak 3 (151178 peak 2; PS2 frames `runs/peak3/fr-throne-tuck.tick*.png`).

**Browser, checked in Chrome** (scratchpad `peak3/qa/fr.mjs`, `ctm.mjs`, `ctm2.mjs`):
- `?course=PEAK3&peakCourse=21`: free ride at Black Top Station (0/5 collectibles, $2,000 per collectible, rails, the
  station signs); `&peakCourse=16`: The Throne.
- Conquer the Mountain with the Peak 3 pass: MCOMM > Transport opens on the ridden peak; Race list Gravitude / The Throne
  (locked until every race has a medal) / All Peak Race (locked); Freeride list The Throne, Black Station, Gravitude,
  Much-2-Much, Perpendiculous, Kick Doubt; "Transport to this area now?" > the gondola cut > arrival at Gravitude with
  ERA5 / E_ERA5 / ERA5_C resident (in-world transport, no reload). A Peak 3 Big Challenge offer ("Giant Slalom
  Qualifier") opens when riding through its volume.
- Select Peak on Peak 3 from the Peak 1 world: "Go to this peak now?" > the Peak 3 world loads at The Throne with the
  first-arrival cinematic (`arrivalSteps('EBC3', true)`: FMV 31, `ebc3_heli_arr`, the rider's jump); the career marks
  `arrived[16]` so it plays once. (`career-ui.js ctm-gopeak`, `enterWorld`; main.js loads another peak's world instead of
  an in-world transport when the destination's world differs.)

## 5. Status and what remains (2026-09-25)

**Playable in the browser:** every Peak 3 Single Event (Gravitude, Kick Doubt, Much-2-Much, Perpendiculous, The Throne,
Throne Jam) from Select Peak > Peak 3; the PEAK3 free-ride world (`?course=PEAK3`, Black Top Station / The Throne, in-world
transports, collectibles, Big Challenges); Conquer the Mountain on Peak 3 (Transport lists, "Go to this peak now?" with the
first arrival, Peak 3 session points).

**Remaining:**
- ~~All Peak Race / All Peak Jam (modes 8 / 11) need one whole-mountain world~~: done 2026-09-26, section 6 (the MOUNTAIN
  world; the Peak 2 Race runs in it too).
- Exactness: the open ticks in the table in section 3. The web race tick runs one tick behind the PS2's (lead 0x234E20),
  which is the Kick Doubt 1:00 time-out tick. +0x2F8 by finishing place (0x239230) is not ported.
- Gravitude lineups by human character (lineups.json grid scales) and the Kick Doubt Quick Play roster; the Peak 3 intros
  side by side with the PS2; free-ride instance audits for EBA3 / EHP3 / ESS3 (there are no PS2 free-ride states at those
  courses: their Transport rows opened the CTM events).

## 6. The whole mountain: All Peak Race / All Peak Jam and the Peak 2 Race (2026-09-25/26, All Peak agent)

### PS2 ground truth (ARMSX2, derived states only; `local/ps2-capture/allpeak/`, `local/ps2-capture/runs/allpeak/`)

**Unlock and flow (CTM).** From `peak3/fr-ebc3-arrival.p2s` (lock words cleared): Start > Transport > Peak 3 > Race lists
Gravitude, The Throne (medal tick), **All Peak Race**; the help reads "A battle against your rival."; "Transport to this area
now?" Yes > the heli interior with "Loading..." > the heli fly-in at The Throne > the rider in the heli > the card over the
ride-in (`nav/out-apr-card/`: race-list, item, prompt, after). Freestyle lists Kick Doubt, Much-2-Much, Perpendiculous,
Throne Jam, **All Peak Jam** (`out-apj-card/`); Peak 2's Race list Ruthless Ridge, Intimidator, Ruthless, **Peak 2 Race**
(`out-p2r-card/`, from `peak3/for-peak2/fr-dbc2-arrival.p2s`). The cards (1FD190 "68rival_pre"):
- All Peak Race: "Rival Challenge / All Peak Race", "You've been challenged by Psymon to a Peak Race!", "Race from the top of
  Backcountry to the bottom of Metro-City.", "Beat the rival's time to win the event.", "Time to beat: 30:00", Continue.
- All Peak Jam: "... by Psymon to a Peak Jam!", "Make your way down the Backcountry and Slopestyle runs.", "Beat the rival's
  score to win the event", "Score to beat: 1000000".
- Peak 2 Race: "... by Nate to a Peak Race!", the Metro-City route line, "Beat the rival's time to win the event.", "Time to
  beat: 19:00".
- At the card: 0x535C08 = 16 (15), 0x535C10 = 5 (6 for the Jam), game mode 8 / 11 / 7; GMM 0x59FE00 +0x68 = 16 / 15, +0x78 =
  108000 / 126000 / 68400 ticks; the handler (0x5404F0 time, 0x540500 points; +4 limit, +8 split counter, +0xA tier);
  world state 2; resident EBC3, EBC3_E, TRANSP, ESKY.

**Closed-loop captures** (`tools/ps2_autopilot.py`): ps2_capture's derived state and 16 KiB records, the pad hook swapped for a
256-entry live ring the host writes 10 ticks ahead (the record's watch window says which entry the game consumed), a tuck
and pure-pursuit pilot on the location's own AIP race paths toward the next connector's trigger volume, a detour round
Gravitude's finish corral (its end fences close it in a peak run), `--resume` from a kept state. `pads` rebuilds the
consumed pad script for the comparer; `analyze` writes RUN.analysis.json (every course / world-state / split / row change).

**All Peak Race run** (`runs/allpeak/apr-full.bin`, 125,001 ticks from Continue, Zoe, tier 0 = the 1800 s row; assembled from
`apr-try3` + `apr-part4` + `apr-part5` + the last part through `--resume`, one continuous record stream):

| Tick | Trigger (0x535C08) | Rows (PS2) | Split |
|---|---|---|---|
| 11871 | EBC3_E Unload -> 21 (E), world state 4 -> 11 | EBC3 2->7->0 (+7); E, E_ERA5, E_ESS3, E_EHP3, E_EBA3 0->3->6 | |
| 12471 | EBC3_E Load, world state 11 -> 10 -> 4 | E row 1->2 | 1: E at 207 s, +17 (HUD 0x2A, slot 35, 5 s) |
| 15010 / 15765 | E_ERA5 Unload -> 4 / Load | E, EBC3_E, E_ESS3, E_EHP3, E_EBA3 -> 0; ERA5, ERA5_C read | |
| 50122 | **ERA5_C Unload -> 19 (C)**: the peak boundary | ERA5, E_ERA5 2->7->0; C, C_CRA3, C_CHP2, C_CBA2, CSKY 0->3->6; ERA5_C stays | |
| 50160..50312 | reads | CSKY (6->1->2), C, C_CBA2, C_CHP2, C_CRA3 | |
| 51197 | ERA5_C skybox trigger | ESKY 2->7->0 (the dome is CSKY) | |
| 51374 | ERA5_C Load | C row 1->2 | 2: C at 856 s, +386 |
| 53211 / 54108 | C_CRA3 Unload -> 2 / Load | | |
| 73607 / 74347 | CRA3_D Unload -> 20 / Load | | 3: D at 1239 s, +419 |
| 76363 / 77019 | D_DRA4 Unload -> 3 / Load | | |
| 93833 | **DRA4_A Unload -> 17 (A)**: the second boundary | DRA4, D_DRA4 -> 7 -> 0; A, A_ARA1, A_ASS1, A_ABA1, ABC1_A, ASKY -> 3 -> 6; DRA4_A stays | |
| 94309 | DRA4_A skybox trigger | DSKY 2->7->0 | |
| 95245 | DRA4_A Load | A row 1->2 | 4: A at 1587 s, +440 |
| 97480 / 98332 | A_ARA1 Unload -> 0 / Load | | |
| 108003 | the 30:00 limit: "TIME'S UP", world state 5 | | |
| 108290 | results, world state 7 | | |

- Every Unload puts the world in state 11 (the location crossing: MCOMM without Transport / Session) until the Load (state
  10 for one tick, then 4). The game-info tick (+8) restarts during a crossing; the race clock (game info +0xC) and the pad
  hook's tick counter run on (the comparer numbers a peak run's ticks by the latter).
- Splits (23B5F8) at every station's Load: `int(ticks x 0.016666668) - split` posted as HUD message 0x2A for 5 s
  (1195A8 -> 117B88); the HUD draws the "CHECKPOINT" sprite (OV_1-3) and "+HH:MM:SS" in red (0x4C88C8, value >= 0) or
  green (0x4C8888, ahead).
- Measured reads (ticks, this run; `local/ps2-capture/allpeak/measured-reads.json`, now in MOUNTAIN/peak.json): E 68,
  E_EBA3 37, E_EHP3 24, E_ERA5 34, E_ESS3 15, ERA5 218, ERA5_C 40, C 63, C_CBA2 43, C_CHP2 24, C_CRA3 18, CRA3 272, CRA3_D 27,
  D 61, D_DRA4 27, D_DSS2 32, DBC2_D 18, DRA4 239, DRA4_A 21, A 73, A_ABA1 24, A_ARA1 27, A_ASS1 25, ABC1_A 30, ARA1 239,
  ARA1_B 25; skies 29..32.
- The pilot lost ~9 minutes at Gravitude's finish: its race path runs through the finish corral, which the end fences
  (`mdl_ERA5_fencecollision_end_1002..1004`, flags 0 = entity instances) close in a peak run; the rider has to go round the
  wall's south end (-184960, 30050) into ERA5_C. So this run times out in Snow Jam; the Peak 2 Race run reaches Metro-City.

**Peak 2 Race run** (`runs/allpeak/p2r-full.bin`, 69,004 ticks from Continue, Zoe, tier 0 = 1140 s with splits 230 / 530 /
840; `p2r-part1` + the resume at the DRA4_A crossing):

| Tick | Trigger (0x535C08) | Rows (PS2) | Split |
|---|---|---|---|
| 13817 | DBC2_D Unload -> 20 (D), world state 4 -> 11 | DBC2 2->7->0; D, CRA3_D, D_DRA4, D_DSS2 0->3->6 | |
| 14703 | DBC2_D Load | D row 1->2 | 1: D at 245 s, +15 |
| 16789 / 17430 | D_DRA4 Unload -> 3 / Load | D, CRA3_D, DBC2_D, D_DSS2 -> 7 -> 0; DRA4, DRA4_A 0->3->6 | |
| 36232 | **DRA4_A Unload -> 17 (A)**: the boundary the PEAK2 world lacked | DRA4, D_DRA4 -> 7 -> 0; A, A_ARA1, A_ASS1, A_ABA1, ABC1_A, ASKY -> 3 -> 6 | |
| 37607 | DRA4_A Load | A row 1->2 | 2: A at 626 s, +96 |
| 39686 / 40580 | A_ARA1 Unload -> 0 / Load | A, DRA4_A, ABC1_A, A_ASS1, A_ABA1 -> 7 -> 0; ARA1, ARA1_B -> 3 -> 6 | |
| 61348 / 61997 | ARA1_B Unload -> 18 / Load | ARA1, A_ARA1 -> 0; B, B_BRA2, B_BHP1, BSKY | 3: B at 1033 s, +193 |
| 63763 / 64888 | B_BRA2 Unload -> 1 (Metro-City) / Load | B, ARA1_B, B_BHP1 -> 7 -> 0; BRA2 -> 3 -> 6 -> 1 -> 2 | |
| 68403 / 68690 | 19:00 "TIME'S UP" (world state 5), results (7, 287 ticks later) | | |

Reads measured on this run: CRA3_D 44, D 55, D_DRA4 27, D_DSS2 21, DRA4 239, DRA4_A 21, A 69, A_ABA1 24, A_ARA1 27, A_ASS1
27, ABC1_A 30, ARA1 239, ARA1_B 21, B 61, B_BHP1 34, B_BRA2 27, BRA2 187, skies 29 (the same location reads a few ticks
apart from run to run: the texture sub-chunks in flight).

**All Peak Jam run** (`runs/allpeak/apj-full.bin`, 19,001 ticks from Continue, `--jam`: charged jumps with R1 grabs; the run
was stopped after the first split): the HUD reads "GOAL: 1000000" and the countdown (00:30:29 at the split); EBC3_E Unload
15579 -> 21, Load 16275, split 1 at 16276 (E at 271 s): HUD message 0x2B, slot 35, value -196242 (score 3758 - 2000 x 100),
5 s. The streaming is the race's (same connectors and rows).

**Finish and results** (derived continuations: a kept state of the run with the race clock, game info +0xC = 0x5B070C, pulled
back so the pilot finishes in time; `runs/allpeak/*-finish.bin`, navigation from their results state in `allpeak/flow/`):
- Peak 2 Race: from `p2r-full` 64890 (Metro-City) with the clock 64889 -> 44000, the pilot on BRA2's race paths to
  `mdl_BRA2_finish_gate_1000` (a detour round the buildings where it pinned itself after a crash's reset): **FINISH!
  00:16:58** (world state 5), results 407 ticks later (7). Cross: **Top 5 Record Times** (PLAYER 1 / Zoe 16:58, BARRY / Zoe
  19:01, ANDREA / Elise 19:55, DARCY / Nate 20:00, CRAIG / Psymon 21:11; "Congratulations, you've got a top time!",
  Continue / Save Records) -> **Rewards** ("Cash: $40,000": silver, 970 < 1018 <= 1075 s) -> **Rival Challenge / Peak 2 Race,
  Event Results** ("Time to beat: 19:00", "Your time: 16:58", "Congratulations!  You have won this challenge.", Transport /
  Restart / Quit) -> Transport: the transport cut ("Press X to skip"), then Select Peak with Peak 1 "You are here".
- All Peak Race: from `apr-full` 102001 (Snow Jam) with the clock 102000 -> 50000: split 5 at B (+14545), Metro-City (+16292),
  **FINISH! 00:22:10** (+29805), results +407. Top 5 (PLAYER 1 / Zoe 22:10, CHRIS / Griff 30:01, DOM / Moby 30:36, BLAIR / Kaori
  30:50, BASIL / Allegra 31:19) -> Rewards: "Cash: $100,000", "Earnings goal complete!" (a poster and four trading cards: the
  cash completed Peak 1's earnings goal), "**All Peak Race complete!**" (a board graphic) -> Event Results 30:00 / 22:10, won ->
  Transport. (This save does not meet "Peak 3 conquered!" / "Mountain conquered!".)
- Time up (`p2r-full` 68691): no records, no rewards; Event Results "Your time: DNF", "Sorry, you didn't win.  You must beat
  the posted time to complete the challenge."
- The in-run clock counts down from the limit (00:08:34 at the Peak 2 Race's A split; rounded up); FINISH! shows the time.

**Single Event.** The Map LUI's Single Event rows include the peak runs (Map states 280 / 290, 390 / 400, 480 / 490), but the
Select Event lists never show them: Peak 1 and 2 lists (`menus/single`, `nav/p2/out-lists`) and Peak 3's with every lock
word cleared (+0x278), with every character's All Peak medal records set (`allpeak/nav/out-single-medalled`) and with every
award bit set (+0xF28, `out-single-awarded`) list only the standard and rival events. The peak runs are Conquer the Mountain
events only (the port hid them in Single Event too: web/fe-event-select.js).

### The whole-mountain world MOUNTAIN

- **Data** (`tools/export_mountain_world.py`, from the per-peak exports; `web/public/assets/MOUNTAIN/`): `peak.json` with all 43
  locations (roots in their per-peak folders: ERA5_C from PEAK3, DRA4_A from PEAK2, the copies are identical but for the rail
  flags' provenance), the 49 streaming rows (read times measured on the All Peak Race run where it measured them), the 22
  residency rows (courses 0..21) and `requests`: every trigger volume's builtin 68 call decoded from the stage seed (21
  Unload + 21 Load, 5 booths, 5 lodge doors, 5 skybox triggers; ERA5_C -> 19, DRA4_A -> 17); `SECTIONS/` (5157 instances,
  903 programs), `SETPIECES/` (the three peaks' merges merged again: flags with cloths rebased and the wind by peak, UV
  scroll, LiveComp, particles, stage lists), `LIGHT_GLOW/`, `lighting-banks.json`, `AUDIO/world/MOUNTAIN.json`, and the
  environment split per location (`environment.json` = Peak 3's scalars without patches, `ENV/<LOC>.json/.bin` = a
  location's patches and lattice textures with global ids track << 12 | index).
- **Stage tables:** `tools/export_peak_stage.py --mountain` -> `web/generated/mountain_stage_seed.hpp` (43 stages, 3995
  programs); core `browser_mountain_world()` ("MOUNTAIN", or "MOUNTAIN<x>" for another glide seed) selects them; the
  collectible award follows the current course's peak (144C98 -> 151178: 500 / 1000 / 2000).
- **Glide seed:** `tools/export_peak_seed.py --world MOUNTAIN --state <All Peak Race card> --region EBC3`
  (`web/generate-controllers.py` compiles every `local/assets/native/MOUNTAIN*` seed).
- **Streaming** is the peak worlds' (web/peak_world.inc, web/free-ride.js): the core's state machine owns residency; the page
  fetches a location's collision / draw package / environment slice when its row is wanted, and for the whole mountain it
  prefetches only the rows of the run's next two courses (`PEAK_RUNS[mode].route`), never the whole world; draw packages and
  environment slices are released 45 s after their location leaves.
- **Core memory fixes (apply to every streamed world):** a streamed terrain append used to reserve the exact size per
  3-patch slice (the whole BoundedPatch list moved every slice) and to merge each location into a new traversal list: both
  grow geometrically now (engine/collision.hpp, engine/world_body_collision.hpp; the order is unchanged: every probe and every
  streamed-world capture gate identical). The All Peak route's 32 locations: core heap 266 -> 154 MB; a whole peak 154 -> 128 MB.
- **Environment:** `environment_add` / `environment_drop` (web/environment_bridge.cpp): the lattices of the three peaks are
  93 MB as slices (68 MB merged), a location's 0.3..6 MB; a missing patch keeps the last colour (the rider lighting is
  presentation only).

### The events in the port

- **Worlds:** `peakRunWorld(mode)` (web/free-ride.js) gives MOUNTAIN for the Peak 2 Race (7) and the All Peak Race / Jam (8 /
  11); the Peak 1 runs and the Peak 2 Jam stay on their peak. `PEAK_RUNS[mode].route` (web/peak-run.js) lists the course rows
  a run crosses (All Peak: 16 21 4 19 2 20 3 17 0 18 1; Peak 2 Race: 15 20 3 17 0 18 1): the whole mountain fetches only the
  rows of the current course and the next two on it. The finish rule is the core's (10E5D8: course 1 in a peak run).
- **Splits:** 23B5F8 / 23C560 post HUD message 0x2A (time) / 0x2B (points) through the core's score object
  (`score_hud_post`, web/score_gameplay.inc); web/trick-hud.js draws cases 0x29 / 0x2A / 0x2B like the original (the
  "CHECKPOINT" sprite pulsing, "+H:MM:SS" red behind / green ahead, "+%d" for points; tools/probe_trick_hud.py oracle cases,
  test-trick-hud).
- **Career:** the Transport lists open them with the career's lock (web/career.js goalEvents: every standard event and the
  rival challenge medalled); Transport -> "Transport to this area now?" -> the peak's load -> the 68rival_pre card (Rival
  Challenge / All Peak Race, "You've been challenged by Psymon to a Peak Race!", Time to beat 30:00) -> the run. Results
  (web/career-ui.js, the PS2 flow above): a new top-5 time shows Top 5 Record Times first ("Congratulations, you've got a top
  time!", Continue / Save Records greyed: the port saves at once), then the rewards (cash, "All Peak Race complete!" and its
  item, goals completed by the cash, "Peak 3 conquered!" / "Mountain conquered!" by web/career.js checkAwards), then "Rival
  Challenge / <run> / Event Results" with Time to beat / Your time (or scores) and Transport / Restart / Quit.
- **Jam HUD:** a points challenge draws "GOAL: %d" (0x46EB50, 1ED4C4, the handler's target) on the clock's line from the
  left edge, as the PS2's "GOAL: 1000000 00:30:29" (web/free-ride-hud.js drawPeakRunHud; the Peak 1 / 2 Jams get it too).
- **Single Event:** the peak runs are no longer listed (PS2 above).
- **Resets:** a reset between the eviction of the path bank's location and the next bank (a fall in a connector just after
  its Unload) re-attaches to the evicted bank instead of stopping the core ("Original reset route data unavailable"; the
  PS2's eviction callback 12A490 -> 26ADA0 is empty, so its old bank stays too).
- **Dynamic entities in the streamed worlds:** the event seeds of the scripted dynamic instances (flag 0x40000000,
  generated/event_instance_seed.hpp) were chosen by event course only, so in PEAK1..3 / MOUNTAIN every one was unsupported: a
  crash flight, a crash body query or a reset placement meeting one threw and stopped the core mid-tick (the browser froze
  ~6 minutes into the All Peak Race at Gravitude's `mdl_ERA5_CRbillboard_1000`: "Native air trajectory world query is
  incomplete: cause=2", "Crash body query intersects unsupported ...", "Reset placement intersects unsupported world
  resources", then "Race tick already open" every frame). Now a streamed world takes each instance's course event seed and
  roller masses (web/world_bridge.cpp scripted_instance, web/roller_gameplay.inc; 251 of the mountain's 256 have one; the
  stations' five `os609_full_version_depart` models take their free-ride seed since 2026-09-26), and a crash there leaves any remaining unsupported instance out of its
  queries instead of throwing (engine/crash_world.hpp partialEntities, web/crash_runtime.hpp). Every streamed-world capture
  gate is unchanged.

### Gates (npm test)

- **`web/test-mountain-world.mjs`:** the manifest (43 locations, 22 residency rows, 49 streaming rows; the 21 connectors each
  request a row holding them; ERA5_C -> 19, DRA4_A -> 17), both crossings streamed in the core (ERA5 / E_ERA5 and DRA4 /
  D_DRA4 evicted, the next station's row active), 2550 height + rail probes across both boundaries identical to the event
  packages (Gravitude's / Intimidator's rows) and to the PEAK2 / PEAK1 worlds (Yellow / Green station), the environment
  slices, the All Peak route's memory (32 locations, 154 MB), and the PS2's own streaming: the core driven only by the
  captures' trigger ticks reproduces the current course on every tick and every location row: the All Peak Race exactly
  (125,001 ticks, 5,375,000 row states), the Peak 2 Race to Metro-City up to read ends a few ticks apart (69,004 ticks, 79 skews).
- **`web/test-ps2-captures.mjs`** (whole-mountain world, seeds MOUNTAIN / MOUNTAIN2 / MOUNTAINJ = the three cards' states;
  since 2026-09-26 the comparer also loads the world's stage world, as the browser does: `web/peak-capture.mjs`,
  `PEAK_STAGE=0` leaves it out). The crash-contact divergences are fixed (section "Past the crash contacts" below).
  - `allpeak/apr-start`: exact to the end (13,999), score through 12471 (round 2; before: 11946, and 784 before the crash
    contacts).
  - `allpeak/p2r-start`: exact to the end (5,999), score to the end (round 2; before: 1840). Before the crash contacts: 1261.
  - `allpeak/apj-start`: exact to the end (5,999), score to the end (round 2; before: 5942). Before the crash contacts: 2660.

### Past the crash contacts (2026-09-26, crash-contact agent)

Found by the first differing word at each divergence: the comparer's `--fields`, the capture's 105D98 hook record (record
+8920: the event point, direction, normal and closing speed) and the recompiled/disassembled originals.

1. **The order of the instance walk: apr 785, p2r 1262, and three other gates.**
   - At apr 785, 104E70 summed the normals of six contacts with the air body: five triangles of `mdl_EBC3_icechunk_00019`
     and one of `mdl_EBC3_rock_1010`. The sum differed in the last bit of y (0x3ef700da vs the PS2's 0x3ef700db), so the
     closing speed and the crash bounce differed by 1-2 ulp.
   - The PS2 walks the instances in its octree order. 328660 / 328C20 insert each instance at the head of its cell's list
     (node+0x20, the cell of the box +0x60/+0x6C). 332DB8 / 3309D8 and 334458 visit a node's own list before children 0..7.
     rock_1010 sits in the level-12 parent of the icechunk's level-11 cell, so the PS2 visits it first; the browser walked
     the load order.
   - Fix: `WorldBodyCollision::collidableInstances()` (engine/world_body_collision.hpp) is now sorted like the terrain
     traversal: `originalSpatialCell` / `originalSpatialBefore`, and later insertions first within a cell. This also cleared
     the p2r 1262 crash among DBC2's rocks, peak3/the-throne-tuck (696 -> 4056) and peak3/fr-throne-unload
     (14468 -> 15036).
2. **LiveComp-animated collision: apj 2661, bc/bc-race-idle 1032.**
   - At apj 2661 the crash's sliding body query met `mdl_EBC3_fallingpatha_1000` 39.9 cm deep at its authored pose and
     pushed the rider 43.9 cm. On the PS2 the path had already fallen: program 19 on the contact with
     `fallingpathatrigger_1000` (tick 2326) builds its LiveComp.
   - 334888 composes an instance whose entity answers vt+0xD4 (every LiveComp: 0x361090) from the entity's node matrices
     (vt+0xE4 -> 0x361098), whatever the broad-phase route. The static inline box stays: flags 0x210023 -> 0x210125, as in
     the PS2 state at tick 3000.
   - Fix: `stage_livecomp_collision()` in web/stage_world.inc copies every core LiveComp's node matrices into its
     instance's collision nodes at the end of the entity pass and when the LiveComp is built. The authored nodes come back
     when the entity goes, and on a new race. The rigid predicate (0x355420: 1) and the selected-contact node velocity
     (0x34E698) are in web/roller_gameplay.inc. Snow Jam's billboard already did this (web/falling_billboard_collision.inc).
   - The same fix makes bc/bc-race-idle exact to the end: the treehevbump soft collision at 1032 now wobbles its collision,
     and every rider is exact through 2400.
3. **13AA48 after a touchdown: apr 3581, peak3/gravitude-race-ai 1505.**
   - 139C88 runs 13AA48 after its own touchdown too (13A718 -> 11E150 -> 13AA48 at 13A744, also after a landing crash).
     The query filters with rider+0x180 (query+0x10), never the new ground normal +0x370, and keeps 13AA48's own response.
     Only the 105398 after it sees the ground motion 11FE78(0), and 105D98 / 108388 see motion 0 (a soft collision enters
     control 3).
   - At apr 3581 the rider landed on a steep wall patch (EBC3 patch 817). The browser's ground-normal filter dropped the
     wall triangle; the PS2 bounced 18 cm off it and played a soft clip.
   - Fix: web/animation_bridge.cpp (the post phase) and classify_body_contact's motion argument in web/core.cpp.
     peak3/gravitude-race-ai is now exact to the end: physics, bones and score (its 1505 was this landing).
4. **The score tick without a path bank: apr 11879.**
   - 121818 runs 117C28, the score tick, every tick (rider+0xAC4 == 0), after the route passes.
   - The browser skipped it whenever the reset paths were empty. That is between a crossing's eviction of the old path
     bank (EBC3 at 11878) and the next delivery, so the rail's grind distance stopped counting.
   - Fix: web/animation_bridge.cpp follow_rider_route and web/npc_gameplay.inc npc_follow_route.

Fixed in round 2 (2026-09-26, round-2 agent). PS2 probes on kept states: `tools/ps2_entry_probe.py` (a stub at chosen function
entries logging `ra`, the stack, 1298C8 and fixed EE words) and `tools/ps2_poll.py` (EE words over PINE, printed on change):

5. **The game tick restarts at a crossing: apr 11947.**
   - A landing after a rail's passive exit (11890) is scaled by 13C7A8: `0.7 + 0.01 * (tick - leave - 40)`, capped at 1, the
     tick 1298C8 = the rider manager's +8 (game info 0x5B0700 is the cAI object; 128AF0 increments +8 at its end).
   - The writer is 1297C8(rider manager, 0): +8 = 0 (+0xC, the race clock, only with a1 = 1). Probe on `apr-full.tick17458.ws11`:
     one call at tick 17469, ra 0x128A28 (128A10), whose caller is 0x235504 (235080) from 0x2310C0 (230F40, the cGame update).
   - 22DF50 (the Unload's first streamer pass) requests world state 11 and the background state 10 (231278 -> S+0x208 = 10);
     230F40 enters it (234F40: 278B98 opens the destination's NIS script, 12AB20 / 12AC48) and runs its vt+0x18 235080 every
     tick. Sub-state 0 waits for the script read (278D58: +0x538 cleared), 1 and 2 take one tick each (2791D8, 12ABD0 / 12ACF8,
     initGameMode, 128958), 3 waits for 12A180 (every rider loaded) and calls 128A10 when 0x535C10 == 5 and 0x535C11 == 0 (1289F0
     when 0x535C10 == 0): the time challenges restart the tick, the Jam (6) and free ride (4) do not (no restart in apj-full or
     fr-throne-unload). Polled on the same state: sub-state 0 for 9 calls, 1, 2, 3, then 1298C8 = 0.
   - The read's time is disc timing (seek), so the Unload -> 0 delay is 12 into a hub / station, 13 into a course, 9 into
     Metro-City and 24 twice (apr E_ERA5, p2r DBC2_D: the read waited).
   - Port: `browser_game_tick_restart` (web/animation_bridge.cpp) sets motionTick (the port's 1298C8) and the section scan's
     game tick (web/section_gameplay.inc `sectionTickBase`: 0x101C80's `tick - A+0xD0 < 20` is signed, so after a restart only
     the distance or a -1 rescan triggers a scan, as on the PS2). The comparer calls the export `game_tick_restart` where the
     record's tick field (1298C8) jumps (web/peak-capture.mjs); the page's model (web/peak_world.inc) restarts it the measured
     number of passes after the Unload's 22DF50 pass (`tick_restart_delay`). apr-start: physics exact to the end.
6. **The collectibles' magnets: p2r 1841, apj 5943.** The page's peak runs always use the world name MOUNTAIN, and there the
   collectibles' slot-1 programs (builtins 3, 90, 97) build their MagnetModifiers (apr-start: up to 10 live magnets). The
   comparer's seeds MOUNTAIN2 / MOUNTAINJ name the same world, but the section activation and the stage world compared their
   export's "location" (MOUNTAIN) with the collision world's name, so neither ran: no slot-1 program, no magnet. Fix:
   `browser_same_world` (web/streamed_world.hpp) in section_reset and the stage world reset. p2r-start and apj-start: score
   exact to the end.
7. **Two tree rails with one end point: the-throne-tuck 4057.** The rider grinds `spline_EBC3_treerailhevbb_1055` backwards to
   its start; `_1056` starts at the same point, and 0x334680's search returns the same t and distance (0x40D2CF72) on both. The
   winner is the first strictly closer segment in the walk, and 0x334680 walks the rider's scope list (+0x210 / +0x214, 332DB8
   from the octree: node list before children, later insertions first in a cell), not the load order: the PS2 takes `_1056`.
   Fix: `originalRailWalkOrder` (engine/rail_motion.hpp) sorts the segments by `originalSpatialCell` of their bounds like the
   instance walk; web/rail_bridge.cpp caches it per record set. the-throne-tuck: 4056 -> 4902.
8. **1057B8's landing on a scenery top: the-throne-tuck 4903, fr-throne-unload 15035.** Motion 1 with channel-2 class
   1/2/9/11 and up . n > 0.3 rebuilds the velocity and then (unless clip 268 is past 0.2) calls 10E910(rider, 0, 0, 0),
   119E38(score, stanceDiffers, 0), 11FEC8(13), 11FEC8(5), 3128E8(268, force). These were stubs in the browser (counted as
   unsupported). PS2: entry probe of 11FEC8 on fr-throne-unload 15035: ra 0x132828 (132770: 4), 0x105CC0 (13), 0x105CCC (5).
   - Port: the rider actor's callbacks in web/instance_contact_gameplay.inc, `surface_landing_*` in web/animation_bridge.cpp:
     the old control's exit (control 5: 134CB0 `landing_air_exit`, the pivot bake; 0 / 4: 131C30 / 12FB68), control 5's entry
     133128 (+0x2DC / +0x2E0 from the prewind), clip 268, `score_landing_args(0, 0, 0)`, the 119E38 commit (its meter
     return is dropped). The pose of that tick ran in the old control, so the pivot for the next 134CB0 bake is taken from it
     (fr-throne-unload 15036's control-5 re-attach bakes the root 1 ulp; probe of 106848 / 13AD20 / 134CB0 / 134DD0 / 115358).
   - the-throne-tuck: exact through 6104 (6105: the browser lands one tick early, open); fr-throne-unload: through 15246 (15247:
     a soft collision, control 3, the browser does not enter, open); peak2/cra3-race-ai's first computer rider 3291 -> 3443.
- **apr 12472:** the E station split (HUD 0x2A at the Load's world state 10 exit 235868 -> 238510) is posted by the page
  (web/free-ride.js), not by the comparer: the score gate ends there.
- **`web/test-trick-hud.mjs`:** the split cases 0x29 / 0x2A / 0x2B against the original's draw lists.

### In the browser (Chrome for Testing, 60 Hz display, the dev server; scratchpad `allpeak/qa/ride2.mjs`)

An in-page pilot (the PS2 autopilot's guides and detours, a fake standard gamepad, Select to reset when pinned) rides the
events from Conquer the Mountain: a save with every standard and rival event medalled (web/career.js locks), Transport ->
Peak 3 -> Race -> All Peak Race -> the Peak 3 world at Black Top Station -> the MOUNTAIN load -> the 68rival_pre card ->
the run. Frame statistics leave out the frames that the harness's own screenshots stall.

- **All Peak Jam** (standalone `?course=MOUNTAIN&peakMode=11`, the pilot's jumps and grabs, 5.5 minutes): E at 240.8 s, the
  points split "-192927" in red, then Gravitude; 19,811 frames, p95 <= 18.7 ms.
- **All Peak Race** (a whole run, 30:00): the card, E split at 219.7 s (+00:00:11), E_ERA5 -> Gravitude, **ERA5_C -> C** at
  675.6 s (split 2, +00:03:07), CRA3, D (split 3, +00:03:38), DRA4, **DRA4_A -> A** at 1413.0 s (split 4, +00:04:26), ARA1;
  TIME'S UP in Snow Jam as on the PS2 run, then "Rival Challenge / All Peak Race / Event Results", Your time DNF, Transport
  -> Select Peak. 107,659 frames after the start: p50 16.7 ms, p95 <= 18.7 ms in every 5 s window, 3 frames over 33 ms
  (48-82 ms, at the C / D / A station Loads, when the station's location first draws) and none at the two peak
  boundaries' Unloads; core (wasm) memory 153.6 MB at The Throne, 184 MB from Gravitude, 221 MB from Ruthless Ridge,
  265.5 MB in Snow Jam (the heap never shrinks); JS heap 452 MB median, 735 MB peak.
- **Peak 2 Race** (a whole run, 19:00): Ruthless, the D split (+00:00:08), DRA4, **DRA4_A -> A**
  (no void; the A row is requested at the Unload as on the PS2), A split (+00:01:49), ARA1, TIME'S UP in Snow Jam, Event
  Results with DNF. 68,033 frames: p95 <= 18.7 ms, 2 frames over 33 ms (48.5 ms at D's Load, 66.7 ms at A's Load); core
  memory 153.6 -> 184.4 MB; JS heap peak 689 MB.
- **Peak 2 Race to the finish** (the detour below, and the race clock restarted at B like the PS2 finish captures, QA only:
  core `race_clock_restart`): ARA1_B -> B (split "-00:13:49"), B_BRA2 -> Metro-City, the finish, then **Top 5 Record Times**
  ("Congratulations, you've got a top time!", Continue / Save Records) -> **Rewards** ("Cash: $75,000", "Race goal
  complete!" and its items, "Peak 2 Race complete!" and a board graphic) -> **Rival Challenge / Peak 2 Race / Event Results**
  (Time to beat 19:00, Your time 04:32, won) -> Transport -> Select Peak (the cursor now starts on Peak 1, the finish
  course's peak, as on the PS2; the ride showed Peak 2 before that fix). 73,795 frames:
  p95 <= 18.6 ms; core memory 184.4 MB peak.
- The pilot's two time-ups in Snow Jam were its own: it followed the race path into Snow Jam's finish, whose challenge reset
  planes (active in kinds 4..6: `mdl_ARA1_challenge_reset_plane_finish_2000 / 2003`, stage programs 10 / 11) reset it onto the
  same spot again and again; the PS2 pilot turned north-east earlier. Both pilots now take a detour (ARA1_B vias).
- The run's first frames after the card: two long frames (116-217 ms); the PEAK3 world has the same at its run start
  (three.js builds the node materials of what the world pass first draws). The station Loads' 48-83 ms frames happen when
  the station first draws (the split shows at the same tick).

### Remaining (whole mountain)

- ~~Exactness at the crash contacts~~ (2026-09-26): see "Past the crash contacts" above. Still open:
  - ~~apr 11947, p2r 1841 / apj 5943~~: fixed in round 2 (items 5, 6 above).
  - apr 774: the passive-air controller hands the flight to control 5 (the browser's animation side does too). The
    browser's physics state keeps control 4 and +0x2DC = -pi, where the PS2's control-5 entry clears +0x2DC to 0. Neither
    breaks exactness in these captures.
- Frame times: the run start's two long frames and the 48-83 ms frame when a station first draws (three.js builds the node
  materials of objects on their first world-pass draw; free-ride's compileAsync warm does not cover them).
- ~~The records' player name~~ (2026-09-26): the entry is named "PLAYER 1" (0x534FE0, `sprintf("PLAYER %d", 1)`; old saves'
  'YOU' shows as it). A top time opens Top 5 Record Times first in the standard events too, and every heat enters the
  records, not only the final. PS2 derived runs `local/ps2-capture/ctm-left/runs/final-top` and `qual-top`: the Snow Jam final
  (03:55) and qualifier (03:45) with the records poked to 15:00. The flow is records -> Rewards -> Final Results, and records
  -> Qualifier Results. 20A8F8(7) opens overlay 0xF when the result's record rank (+0x18 time / +0x1C score) is set, in
  any event. `web/career.js` heatRecord, `web/career-ui.js`, `web/test-ctm-left.mjs`.
- ~~The stations' five `os609_full_version_depart` dynamic models~~ (2026-09-26) take a free-ride seed. The PS2 free-ride
  audits give no entity and runtime flags 0x50215023 (static route); their handler row has no slot-2 program (contact class
  0). `tools/generate_event_seed.py` streamed_scripted -> `browser_streamed::scripted`, and `web/world_bridge.cpp`
  scripted_instance uses it. `test-mountain-world` checks that the route's unsupported instances are only the flag-0 stage
  entities (ARA1 endmode colliders, ERA5 finish fences), which a query skips unless a program routes them.
- PS2: the All Peak Jam past its first split and its finish (the finish flow is the race's); a natural (unpoked) finish of
  either race; "Peak 3 conquered!" / "Mountain conquered!" with a complete save.
- Safari was not measured.
