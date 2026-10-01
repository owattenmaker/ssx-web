# Peak 2: Ruthless Ridge, Intimidator, Style Mile, Launch Time, Schizophrenia, Ruthless

Owner: the Peak 2 agent (2026-09-25). Peak 1 is the template (docs/locations.md, slopestyle-bigair.md, backcountry.md,
peak-mountain.md); this file lists what differs on Peak 2 and where it stands. A Peak 3 agent works on area E in
parallel (docs/peak3.md); shared registries (tools/locations.py, courses.json, stage/set-piece seed selectors,
fe-event-select.js) carry both peaks' entries side by side.

## 1. Inventory (SLUS_207.72 tables, PS2 savestates)

### Locations

Peak 2 = mountain areas **C** and **D** (course table 0x43D950 +0x54 = 1).

| Code | Course index / location id | Event (Single Event name) | Game mode 0x535C12 / kind 0x535C10 | Handler (0x238160) | Event residency (0x442488) | Sky | SDB track |
|---|---|---|---|---|---|---|---|
| CRA3 | 2 / 2 | Race "Ruthless Ridge" (short Ridge) | 0 / 0 | 1 (0x47CF38) | CRA3, C_CRA3, CRA3_D | CSKY | 24 |
| DRA4 | 3 / 3 | Race "Intimidator" (Intimid) | 0 / 0 | 1 | DRA4, D_DRA4, DRA4_A | DSKY | 32 |
| DSS2 | 6 / 6 | Slope style "Style Mile" (Style) | 1 / 1 | 0 (0x47CFA0) | DSS2, D_DSS2 | DSKY | 35 |
| CBA2 | 9 / 9 | Big air "Launch Time" (Launch) | 3 / 2 | 0 | CBA2, C_CBA2 | CSKY | 22 |
| CHP2 | 12 / 12 | Super pipe "Schizophrenia" (Schizo) | 2 / 3 | 0 | CHP2, C_CHP2 | CSKY | 23 |
| DBC2 | 15 / 15 | Backcountry "Ruthless" (Rival Time) / "Ruthless Jam" (Rival Points) | 4 / 5, 5 / 6 | 5 / 6 | DBC2, DBC2_D | DSKY | 30 |
| C | 19 / 19 | Yellow Mid Station | free ride | 2 | C, C_CRA3, C_CHP2, C_CBA2, ERA5_C | CSKY | 18 |
| D | 20 / 20 | Red Mid Station | free ride | 2 | D, D_DRA4, D_DSS2, CRA3_D, DBC2_D | DSKY | 27 |

- Every PS2 event savestate confirms the residency (streaming table 0x442168 state 2 plus TRANSP and the sky) and the
  mode/kind bytes (checked on every countdown anchor / ready state below).
- Connectors (location table kind 2) and their SDB tracks: C_CBA2 19, C_CHP2 20, C_CRA3 21, CRA3_D 25, D_DRA4 28,
  D_DSS2 29, DBC2_D 31, DRA4_A 33 (also in the Peak 1 world: Intimidator runs down into Green station A), ERA5_C (track 46,
  location id 27: Gravitude's exit into C; Peak 3). Skies CSKY 26, DSKY 34.
- The mountain order: E -> ERA5 -> ERA5_C -> **C** -> {C_CRA3 -> CRA3 -> CRA3_D -> **D**, C_CHP2 -> CHP2, C_CBA2 -> CBA2};
  **D** -> {D_DRA4 -> DRA4 -> DRA4_A -> A (Peak 1), D_DSS2 -> DSS2}; DBC2 -> DBC2_D -> D (the helicopter backcountry).
- Sky by the peak-letter rule (tools/import_sky.py): C* -> CSKY, D* -> DSKY; matches the residency rows.

### Single Event lists (PS2 frames `local/ps2-capture/nav/p2/out-lists/`)

- Race: Ruthless Ridge, Intimidator, Ruthless (then Peak 2 Race once the career opens it).
- Freestyle: **Style Mile, Launch Time, Schizophrenia**, Ruthless Jam (then Peak 2 Jam). This is the order of the Map
  LUI rows (Peak2FreestyleLocations: Map states 330, 360, 380, 310, 400), which differs from the career goal list
  0x45AAD8 (Launch Time, Schizophrenia, Style Mile, Ruthless Jam, Peak 2 Jam). On Peak 1 both orders agree.
- Help texts: "Ruthless Ridge is a wicked course perfect for INTERMEDIATE riders wanting to race.", "Style Mile is an
  INTERMEDIATE slopestyle run filled with shark fins and spine hits." (kT_HELP<code>).

### Rules and tables (career.json, from the ELF)

| Course | Cash 0x4405A0 plat/gold/silver/bronze | Platinum 0x440E80 | Freestyle table 0x440B38 (rounds 1/2/3: posted scores, limit) |
|---|---|---|---|
| Ruthless Ridge | 50000 / 25000 / 15000 / 5000 | 160 s | - |
| Intimidator | 50000 / 40000 / 20000 / 10000 | 165 s | - |
| Style Mile | 50000 / 25000 / 15000 / 5000 | 1,000,000 pts | 350000.. / 400000.. / 450000,220000,95000,75000,55000; 75 s |
| Launch Time | 50000 / 20000 / 10000 / 5000 | 200,000 pts | 90000,50000,30000,24000,14000 (x2) / 120000,62000,45000,30000,20000; 60 s |
| Schizophrenia | 50000 / 20000 / 10000 / 5000 | 800,000 pts | 220000,170000,95000,75000,60000 (x2) / 260000,180000,120000,75000,60000; 120 s |
| Ruthless | 50000 / 25000 / 5100 / 2600 | 195 s (time) / 500,000 (points) | Rival Points: posted 0, 300 s |

- Peak rival (0x145750, peak index 1): **Nate** (7), or **Zoe** (4) when the player is Nate. Rival Time / Rival Points
  (DBC2) ride him; the career finals of Ruthless Ridge and Intimidator ride him in slot 1; the career freestyle events post him in slot 1
  (nobody rides a career slope style). docs/career-events.md "The peak rival in career events".
- Peak 2 Race / Jam (modes 7 / 10, handlers 4 / 7, table 0x440D18): race tiers 1140 / 1075 / 970 s, splits 230 / 530 /
  840 s (tier 1), cash $20,000 / $40,000 / $75,000; jam 350,000 / 600,000 / 850,000 points in 12:00, split 1500 x 100,
  cash $20,000 / $40,000 / $75,000. Start course 15 (Ruthless, grid slot 0). The finish rule 10E5D8 accepts only courses
  1 and 5..13 in a peak run, so the **Peak 2 Race cannot end at Intimidator**: the route is DBC2 -> D (split 1) -> DRA4 ->
  DRA4_A -> A (split 2) -> ARA1 -> B (split 3) -> BRA2, finishing at Metro-City (objectives bullet "Race from the top of
  Backcountry to the bottom of Metro-City"; the hub path banks send time challenges D -> D_DRA4, A -> A_ARA1, B -> B_BRA2).
  Peak 2 Jam: DBC2 -> D (split) -> D_DSS2 -> DSS2, finishing at Style Mile. GMM+0x6C (3 for mode 7) is the peak's race
  course, not the finish (nothing reads it in 10E5D8).
- Collectibles (0x43FA70, equal to the stages' builtin-38 lists): Ruthless Ridge 30, Intimidator 25, Style Mile 30, Launch Time 4, Schizophrenia 5, Ruthless 44,
  Yellow / Red station 5 each; award 1000 (0x151178 peak level 1).

## 2. PS2 savestates (derived; the ISO and existing states untouched)

`local/reference/pcsx2/peak2.provenance.json` (sha256 of each state, menu paths); scripts in `local/ps2-capture/nav/p2/`.

- **Unlock:** Single Event Select Peak reads the Peak 2 pass through 146008 -> 145F90 (profile character block
  0x4A6CA8 + bank x 0x9B50 + char x 0xF88, +0x278 bit 12 set = locked; bank = race copy 0x535B20[+0x10] & 1, char = +0x11)
  **when the screen is entered**. `peak-1-selection.p2s` -> `tools/reference_replay.py patch` (bit 12 cleared in every
  character block of banks 0..2: 0xFFFC0 -> 0xFEFC0) -> Triangle, Cross (re-entry reads the locks), DPadUp ->
  `peak-2-selection.p2s` (Peak 2 unlocked, focused).
- Per event: `<x>-ready.p2s` (overlay, phase 3), `<x>-countdown-anchor.p2s` (game tick 18, countdown 162), `<x>-glide.p2s`
  (first sampled tick with the human grounded, control 0 / motion 0):

| Code | ready | anchor | glide (tick) | Riders |
|---|---|---|---|---|
| CRA3 | ruthless-ridge-ready | ruthless-ridge-countdown-anchor | ruthless-ridge-glide (338) | Zoe + 5 computer riders |
| DRA4 | intimidator-ready | intimidator-countdown-anchor | intimidator-glide (381) | Zoe + 5 |
| DSS2 | style-mile-ready | style-mile-countdown-anchor | style-mile-glide (338) | Zoe + the opponent who rides |
| CBA2 | launch-time-ready | launch-time-countdown-anchor | launch-time-glide (338) | Zoe |
| CHP2 | schizophrenia-ready | schizophrenia-countdown-anchor | schizophrenia-glide (338) | Zoe |
| DBC2 | ruthless-ready (Rival Time, rolling start = anchor), ruthless-jam-ready | - | ruthless-glide (460), ruthless-jam-glide (601) | Zoe vs Nate |

- Nate as the player (Ruthless rival = Zoe): `characters/nate/select.p2s` with the same pass patch -> `ruthless-nate-ready.p2s`,
  `ruthless-jam-nate-ready.p2s` (`nav/p2/ruthless-nate-{race,jam}.json`).
- The ready state's two DEFAULT_3 cameras are both referenced in the Ruthless states, one only by a stale word on the EE stack
  (0x1FFF990); `tools/ps2_capture.py discover` now ignores references from 0x1FF0000 up.

## 3. Packages (per location)

`tools/prepare_location.py <X>` (world, terrain, collision, rails, sky, painters, lights, stage scripts, pickups, event start,
instance audits, seeds), then the per-location exporters in this order (the Peak 1 set-piece chain):
`export_stage_scripts.py` (all courses; new courses are picked up automatically and get `BROWSER_STAGE_HAVE_<X>`),
`export_spline_setpieces.py`, `export_set_pieces.py`, `export_flags.py`, `export_uv_scroll.py`, `export_livecomp.py`,
`export_set_piece_particles.py --header`, `export_stage_world.py`, `export_attached_setpieces.py`, `export_crowd.py`,
`export_progress_meter.py`, `export_section_ready_state.py`, `export_far_painter.py`, `export_sections.py` (needs the full-run
capture), `export_npc_riders.py` (races, slope style) / `export_backcountry.py npc|rivals` (DBC2), `export_freestyle_event.py`
(freestyle), then `web/prepare.py --location <X>` again (set-piece batch split) and `sh web/build-core.sh`.

Core registrations (additive, guarded by `__has_include` / `BROWSER_STAGE_HAVE_*`): `web/stage_script_gameplay.inc`,
`web/set_piece_gameplay.inc`, `web/attached_setpieces.inc`; the capture/snapshot registries in `tools/set_piece_location.py`,
`tools/export_sections.py`, `tools/export_location_set_pieces.py` point at `local/ps2-capture/runs/peak2/<loc>-full.*`.

Exporter changes the Peak 2 data needed (Peak 1 outputs unchanged):
- Spline pieces without the builtin-16 end program: Style Mile's osprey (slot 4 = builtin2 + builtin73: node killed, sound
  stopped) and Schizophrenia's rocket cores (no slot-4 row).
- Crowd texture 9-161 is also used by `mdl_DSS2_eb_cp_sli_3`, which has no builtin88 call: listed as
  `shared_texture_users` (the PS2 swaps the crowd instances' records only; the browser's texture swap animates it too).
- Stage event kind 0x535C10 of a course comes from its code's event letters (RA 0, SS 1, BA 2, HP 3, BC 5), checked on every
  Peak 2 anchor.
- `tools/generate_event_seed.py` skips a location whose evidence is half exported (no `scripted-instances.json` yet), so a
  course being prepared by another agent cannot break the core build.

## 4. PS2 captures and gates

Full runs (`local/ps2-capture/runs/peak2/<loc>-full.*`, `--isolate`, watches for the activation manager, texture chunks and
viewer, kept states every 400 ticks; the tuck/weave scripts `local/ps2-capture/scripts/p2-<loc>-full.json`, Ruthless from
`web/bc-autopilot.mjs --course DBC2 --doc race --seg 20`); gate captures are their first runs (cut before the event restart)
or `--ai-state` re-runs. `web/test-ps2-captures.mjs`:

| Gate | Result |
|---|---|
| `peak2/stylemile-event-tuck` | 9000 ticks: physics, bones, score, boost exact to the end (opponent rides, checkpoints +90 / +60) |
| `peak2/launch-event-tuck` | 2826 ticks to the Final run panel: exact to the end |
| `peak2/schizo-event-tuck` | 3881 ticks to the pipe end: physics exact to the end, score to 3491, bones to 3105 (3106 only: rail hop) |
| `peak2/cra3-race-ai` (physics) | 5999 ticks with the five computer riders racing (`--ai-state --isolate`, `rng-order.json` places the human's draws among theirs): exact to the end |
| `peak2/dra4-race-ai` (physics) | exact to the end, bones and score too (2026-09-28: the departure seed; was 834) |
| `peak2/cra3-race-ai` (six riders) | human 1383, computer riders 1683..3291, RNG 1151 |
| `peak2/dra4-race-ai` (six riders) | human 834, computer riders 1224..2975, RNG 2298 |
| `peak2/dbc2-race-tuck` (rival) | human 4435, Nate 5141 (exact on 6982 of 7000 ticks, never 1 cm off), RNG 4581 (Rival Time, rolling start, not isolated; the blizzard's wind push, docs/weather.md; the surface-18 crash at 3911, docs/crash-motion.md; was 3910 / 6425 / 3932, before that 2073 / 2852 / 2200) |
| the same capture, real game path | `web/test-rival-page.mjs`: Single Event > Ruthless in the page (prepareRival with the ready state's game RNG, docs/backcountry.md) -> human 4435, Nate 5141 (the 1413 crash and 1656 reset placement), RNG 4581, as the gate (was Nate 131, human ~2650 before 2026-09-26) |
| `weather/dbc2-weather` | the same line with weather watches: human, Nate, RNG exact on all 3299 ticks; Weather painters and snowfall counts exact |

`web/test-peak2-events.mjs` (npm test): courses.json entries with compiled event starts, the freestyle configurations, the
computer riders' stat bytes, the Ruthless rival documents, every course's sections / stage world / flags loading in the core,
the selector rows. `web/test-locations.mjs` loads and starts all six.

### Fixes found on Peak 2 (they apply to every course)

- **Computer riders' stats.** The stat getters 0x1494C0.. read attribute bank 2 for computer riders (race copy +0xC == -1):
  Peak 2 riders are level 4 (raw 20, stat 4/11 = 0.3636) against the human's 5 raw. The browser's computer riders took the
  human seed's surface profiles on every surface change (`physicsProfile = physicsMaterials[surface]`) and had no attribute
  bytes: `web/npc_gameplay.inc npc_seed_rider` keeps the rider's own stats in every material, `tools/export_npc_riders.py`
  exports the bank row (`attributes`), `web/ai-racers.js` applies it through `set_rider_attributes`. Ruthless Ridge's
  computer riders went from 183..222 to 1683..3291 exact ticks, Nate from 388 to 2852; Snow Jam unchanged (all 5).
- **Rail takeoff score flag.** 114298 passes s1 (1 only on its ramp branch) to 119E38 as the identity field +0x4; the rail
  takeoffs passed "takeoff normal z within +-0.05". Schizophrenia's pipe rails give a horizontal +0x380 (z = 0): score
  exact from 3106 to 3491 now (`web/rail_gameplay.inc`).
- **Single Event rows** follow the Map LUI rows (`fe-event-select.js mapRowOrder`), and Peak 2 opens in Single Event
  (as Peak 3: the port lets ported peaks be played without the Conquer the Mountain pass the PS2 asks for).

## 5. Status (2026-09-25)

- **Playable in Single Event:** Ruthless Ridge, Intimidator (races with the five computer riders), Style Mile (slope style,
  the opponent rides), Launch Time (big air), Schizophrenia (super pipe), Ruthless / Ruthless Jam (vs Nate). Objectives card,
  intro, countdown / rolling start, HUD, results as on Peak 1 (headless checks: scratchpad `p2/shots`).
- **DRA4 835 (analysis):** the passive departure at 834 starts the air predictor (web `advance_prediction`, 113648 /
  113200: available, status 0, predicted landing 0.2 s ahead, speed limit 3333.33); its first integrated step lands the
  velocity 1.6e-7 smaller in every component than the PS2 (a uniform scale: 1750.66528 / -622.30304 / -1314.83313 against
  1750.66553 / -622.30316 / -1314.83337). The plain step (drag 0xBB5A740F, gravity 0xC1FD5556) gives the browser's value,
  so the PS2 took another branch of 113200 there (the sub-tick blend or its time bookkeeping at the departure is the
  candidate); needs the recompiled 113200 oracle with this state (tools/test_*_native.py pattern).
  **Fixed 2026-09-28.** The branch is fine; the seed is not. 13F178 seeds the flight at 13F2CC (11FE78(1) -> 1399E0 -> 1135B8)
  after its contacts and before the 13F358 speed clamp, so the predictor starts from the unclamped velocity. The browser seeded
  it after the clamp. The factor is the clamp's limit / |v|. Fix: core.cpp keeps `groundUnclampedVelocity` on a departure, and the
  post stage (animation_bridge.cpp `departureClamp`) re-seeds before clamping. DRA4 human, bones and score are now exact to the end
  (ai harness: human 5396, RNG 2639). ARA1 Luther 3799 had the same cause.
- **Open:** CHP2 3106 bones (one tick) and the 3492 finish boost award (the Snow Jam setpieces/full 12297 gap); computer riders after 1224..3291 on the
  races (RNG draw count, pair distance); Ruthless human 4436 fixed 2026-09-28 (the detached board's +0x180 frame, docs/crash-motion.md; 3911 was the surface-18 crash of 13F178, fixed 2026-09-27; 2074 the missing wind push, docs/weather.md); the career slope-style "opponent" does not exist
  (Nate is posted, nobody rides: docs/career-events.md "The peak rival in career events"); the career race finals' Nate is exported (CRA3 /
  DRA4 lineups.json career parts, pv careerRival, 2026-09-30). Lineups per human are done (`lineups.json` for CRA3, DRA4 and DSS2; docs/characters.md "Peak 2 lineups"); the connected Peak 2 mountain (section 6).

## 6. The connected Peak 2 mountain (streamed world PEAK2)

Built on the per-peak world tooling the Peak 3 agent generalised (`--peak N`: `tools/export_peak_world.py`,
`export_peak_stage.py`, `export_peak_sections.py`, `export_peak_instances.py`, `export_peak_rail_flags.py`,
`export_peak_seed.py`; `web/free-ride.js` `peakCourseEntry` / `PEAK_DEFAULT_STATION` / `PEAK_BASE_COURSE`; core
`web/streamed_world.hpp`). Peak 2 adds:

- **World (17 locations):** C, D, the six courses, their connectors, DRA4_A (Intimidator's exit into Peak 1's A) and
  ERA5_C (Peak 3's entry into C, part of C's residency row); CSKY, DSKY, TRANSP. Base course CRA3 (environment scalars,
  sky); the start is Yellow station (course 19).
- **PS2 free-ride evidence** (derived; `local/ps2-capture/peak2/`): the CTM "Go to this peak now?" arrival at Ruthless
  (`fr-dbc2-600.p2s`; from the Peak 3 agent's `ctm-peakgoal-unlocked` Transport state), MCOMM Transport -> Freeride ->
  Yellow Station (`fr-c-arrive.p2s`, then a tuck ride C -> C_CRA3 -> CRA3, `frc-*.p2s`) and -> Red Station (the heli
  arrives through DBC2_D, `frd-*.p2s`), a Ruthless free-ride run (`runs/peak2/fr-dbc2.*`, streaming table watched). They
  feed the rail flags, the free-ride instance audit and the section cells (`PEAK_STATES[2]`).
- **Browser tables:** `SESSION_POINTS` (C 1, CRA3 7, CBA2 2, CHP2 2, D 1, DRA4 7, DSS2 7, DBC2 6),
  `PEAK2_COLLECT_TRACKS` (SDB track -> course), the CSKY / DSKY domes (`main.js skyRoots`; each world starts with its
  base course's dome), `CROWD_COURSES` (the Peak 2 courses with crowds), the station of a lodge door / transport booth is
  the current course (0x302210 ignores key0 for actions 3/4; C's and D's volumes carry 19 / 18), Lodge - Peak N of the
  station's peak.
- **Core:** `browser_stage_peak2` (`generated/peak2_stage_seed.hpp`, guarded).
- **Controller seed:** `web/generate-controllers.py` appends the `PEAK2` glide seed (without it the free-ride comparer has
  no original reset route data and stops at tick 1).

### Status of the PEAK2 world

- **Playable:** CTM Transport -> Freeride -> Yellow / Red station starts PEAK2 at that station; the rider streams C ->
  C_CRA3 -> CRA3 and D -> D_DRA4 -> DRA4 (and the other connectors), the residency rows load / unload like the PS2.
  Lodge doors / transport booths use the current station and collectibles map to their course (wired, not yet driven
  end to end in the browser). The Peak 2 Jam starts at
  DBC2 with the 12:00 clock. Checked headless in Chrome (scratchpad `p2/drive.mjs`): Yellow station arrival, streaming
  into CRA3.
- **Gates:** `web/test-peak2-world.mjs` (npm test): manifest of 17 locations, the residency rows (2, 3, 6, 9, 12, 15,
  19, 20), C_CRA3 + CRA3 + CRA3_D streamed one by one answer 1200 height / rail probes identically to the CRA3 event
  package, residency toggling, the whole peak loads (17 locations, 15879 instances, 154 MB).
  `web/test-ps2-captures.mjs peak2/fr-d-glide`: free ride from Red station (seed `frd-1800`, region D) across D ->
  D_DRA4 -> DRA4 (course 20 -> 3 at 3134): physics and bones exact for all 2600 ticks, score through 2370.
- **Open:**
  - ~~The Peak 2 Race falls into the void at DRA4_A~~ (the PEAK2 manifest has no request row for DRA4_A -> A): fixed
    2026-09-26. The Peak 2 Race runs in the whole-mountain world MOUNTAIN (`peakRunWorld(7)`, peak3.md section 6), whose
    DRA4_A Unload requests row 17 (A) as on the PS2. PS2 ground truth: a whole Peak 2 Race capture (Ruthless -> D -> DRA4 ->
    DRA4_A -> A -> ARA1 -> B -> Metro-City, peak3.md section 6), replayed row by row by web/test-mountain-world.mjs; the
    start is gate `allpeak/p2r-start` (exact through 1261).
  - Free-ride score at 2371 of fr-d-glide (not analysed).
  - The CTM flow on Peak 2 end to end (arrival cut, goals, Big Challenges, the pass to Peak 3) is untested; the
    career session points and collect tracks are in, the Peak 2 Big Challenges come from `export_peak_missions.py`.
  - ~~Career slope opponent (test-slopestyle-bigair's R&B opponent list)~~: fixed 2026-09-25 (R&B `lineups.json` rebuilt
    from all 30 countdowns, slopestyle-bigair.md "Gaps"); the test now stops at the R&B held-out captures (raven B, same place).
