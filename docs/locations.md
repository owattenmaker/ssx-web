# Course / event locations (generalized pipeline)

The browser port used to be hard-wired to Snow Jam (ARA1). Every location-specific input now
comes from `tools/locations.py`, the asset scripts take `--location CODE`, and the browser loads
a course by `?course=CODE` (course select in the "Select Event" screen). Snow Jam keeps its
historical paths and produces byte-identical assets.

## Locations

| Code | Event | Name (ELF course table) | Peak | Resident locations (event) | Sky | Status |
|---|---|---|---|---|---|---|
| ARA1 | race | Snow Jam | 1 | A_ARA1, ARA1, ARA1_B | ASKY | full (see HANDOFF) |
| BRA2 | race | Metro-City | 1 | B_BRA2, BRA2 | BSKY | loadable, event start + race, PS2 captures gated |
| BHP1 | super pipe | The Junction | 1 | B_BHP1, BHP1 | BSKY | pipe physics exact (11 PS2 captures), freestyle HUD/timer/finish |
| ASS1 | slope style | R&B | 1 | A_ASS1, ASS1 | ASKY | playable Single Event / career; opponent rides; see [slopestyle-bigair.md](slopestyle-bigair.md) |
| ABC1 | backcountry (Rival Time / Rival Points) | Happiness | 1 | ABC1, ABC1_A | ASKY | playable Single Event (Happiness / Happiness Jam) and career rival challenge vs Mac (Griff for Mac); rolling start; PS2 captures gated; see [backcountry.md](backcountry.md) |
| ABA1 | big air | Crow's Nest | 1 | A_ABA1, ABA1 | ASKY | playable; run capture bit-exact to the end; see [slopestyle-bigair.md](slopestyle-bigair.md) |

Identity comes from the executable and disc, not from guesses:

* ELF location table `0x43E250` (24-byte `{id, name[16], kind}`; kind 0 event course, 1 peak hub,
  2 connector, 3 TRANSP, 4 sky): ARA1 id 0, BRA2 id 1, BHP1 id 11.
* ELF course table (file offset `0x33E950..`, 0x64-byte rows `{index, name[32], short[16],
  code[16], world[16], ...}`): 0 "Snow Jam"/ARA1, 1 "Metro-City"/BRA2, 12 "The Junction"/BHP1
  (short name "Disfunk"). Location codes: `?RA` race, `?BA` big air, `?SS` slopestyle, `?HP`
  half/super pipe, `?BC` backcountry; the first letter is the mountain area (Peak 1 = A and B).
* CMNAMER.LOC: "The Junction, a BEGINNER Super Pipe, happens in a fantastic city setting".
* Event residency verified in the PS2 event savestates (streaming table `0x442168`, state 2):
  Metro-City loads BRA2, B_BRA2, TRANSP, BSKY; The Junction loads BHP1, B_BHP1, TRANSP, BSKY.
  This matches `world_assets.event_locations` (course + connectors whose names contain the code)
  and confirms BSKY for both (the peak-letter rule).

## Pipeline

`tools/prepare_location.py CODE [--skip-import]` runs every step in dependency order; after it,
rebuild the core (`sh web/build-core.sh`) because the event and glide seeds are compiled in.
Snow Jam still uses `npm run setup` (every script defaults to ARA1 and writes the old paths).

Disc-only steps (no savestate needed):

| Step | Script | Output (X = code) |
|---|---|---|
| world, terrain, collision | `tools/import_world.py --location X` | `local/assets/native/X/` |
| sky dome + painted fog | `tools/import_sky.py --location X`, `tools/prepare_course_sky.py` | `X/sky.json`, web `X/sky/` |
| rails | `tools/import_rails.py --locations X` | `X/rails.json` (whole event residency) |
| environment colour lattice | `tools/export_environment_lighting.py --location X` | `X/environment-lighting.json` |
| irradiance Lighting refs | `tools/export_irradiance.py` | `IRRADIANCE/irradiance.json` `lighting_by_location` |
| authored lights | `tools/export_local_lights.py --location X` | `X/local-lights.json` |
| stage (LUN) programs | `tools/import_stage_scripts.py`, `tools/disassemble_stage_scripts.py` | `local/browser-pickups/X/` |
| web world package | `web/prepare.py --location X` | `web/public/assets/X/` + `courses.json` |
| fog / ScreenTint / Sun / glows | `tools/export_fog_tree.py`, `web/prepare-environment.py`, `tools/export_sun_flare.py`, `tools/export_light_glow.py` (all `--location X`) | web `X/`, `X/sun-flare/`, `X/light-glow/` |
| terrain render / light atlas | `tools/prepare_terrain_render.py --location X`, `web/prepare-environment.py` | web `X/terrain-render.json`, `terrain-lighting.json` |
| pickup catalog + rewards | `tools/import_pickup_catalog.py`, `tools/link_pickup_rewards.py` | `local/browser-pickups/X/` |

Disc facts established while generalizing (all verified against the Snow Jam savestates):

* The runtime race path bank equals the course AIP record (SSB kind 14, rid 0) track paths byte
  for byte (origin, bounds, segments, event start/end/value, `remaining_at_origin` = field2); the
  loader renumbers event types 0->1 (finish), 18->11 (checkpoint). The reset path bank equals the
  AIP AI paths (+0x38 = field3, +0x3C = field6) with -1->0, 100->12, 102->14, 103->15, 110->16,
  111->17, 300->20. The BRA2 paths built this way hash identically to the BRA2 countdown savestate
  (`tools/export_course_initial.py`, the provisional initial for a course without savestates).
* The AIP region table's kind-0 rows are the start grid (slot 0 = human); used as the spawn when a
  course has no riding checkpoint (`web/prepare.py provisional_start`).
* Breath/environment regions (`original_breath.regions`) are the course painter's type-12
  section verbatim.
* Stage programs start 16-byte aligned after the offset table and the last one may be followed
  by zero padding (BRA2, BHP1).
* Runtime rail query flags (record+0x1C): 0x30003 rail+handplant, 0x20002 handplant only (The
  Junction's coping), 0 never queried.

Savestate-derived steps (tools/locations.py `states`, made with `tools/ps2_navigate.py`):

| Step | Script | Savestate |
|---|---|---|
| riding checkpoint | `tools/export_riding_start.py STATE X/riding-start.json --location X` | glide |
| course initial.json | `web/prepare-ui.py --location X` (`build_initial`) | glide + anchor |
| pickups (runtime bindings, initial flags) | `tools/probe_pickup_bindings.py --location X`, `tools/export_browser_pickups.py` | anchor (ARA1: glide) |
| roster audit | `tools/audit_rider_assemblies.py --discover` | countdown |
| event start grid | `tools/export_event_start.py --location X` | countdown |
| countdown instance audit | `tools/export_event_instances.py --location X` | countdown |
| scripted (roller) instances | `tools/export_scripted_instances.py --location X` | countdown audit |
| light index | `tools/export_light_tree.py --location X` | glide |
| rail runtime flags | `tools/export_rail_runtime_flags.py --location X` (run by prepare.py) | glide |
| breath context | `tools/export_breath_context.py --location X --state --rider` | glide |
| event seeds | `tools/generate_event_seed.py` -> `web/generated/event_*_seed.hpp` | all of the above |
| glide seeds | `web/generate-controllers.py` -> `web/generated/physics_seed.hpp` | riding-start |

The human rider is found with `locations.human_rider(memory)` (game object roster + human
vtables): Snow Jam 0x14701A0, Metro-City 0x146BF00, The Junction 0x14542A0.

## Browser

* `web/public/assets/courses.json` (written by `web/prepare.py`): per course `code, name, event,
  label, root, sky, sunFlare, lightGlow, startfire, initial, ready, eventStart`. Snow Jam keeps
  `/assets/ARA1/`, `/assets/SKY/`, `/assets/SUN_FLARE/`, `/assets/LIGHT_GLOW/`,
  `/assets/STARTFIRE/`, `/assets/ANIMATIONS/initial.json`; other courses use `/assets/X/...`.
* `web/main.js`: `?course=X` selects the course at load (default ARA1); every course asset path
  goes through `course.*`. `selectCourse(course)` (the `course` UI callback) reloads the page into
  another course with `rider=` and `autostart=1`, so a career/event flow can call
  `ui.cb.course(courseEntry)` or navigate to `/?course=BHP1&rider=zoe&autostart=1`.
  `course.eventStart === false` rides freely from `start.json` instead of the grid start.
  The Snow Jam start-gate sparks are skipped where `startfire` is null.
* `web/ui.js`: the Select Event screen lists `courses.json`; its subtitle follows the event kind
  (Race / Super Pipe); the results header uses the course label.
* Core: `init_world_collision` selects the course's compiled seeds by `world_collision.json`
  `location` (`browser_select_event_course`: DeadNodes, type-16 skip nodes, scripted instances,
  roller masses, grid start delay/progress, grid ground state and participant;
  `browser_select_glide_course`: glide ground profile/state, boost state, landing ticks, used by
  `reset_rider`). Unseeded locations get empty tables and the Snow Jam glide seed. The rail/fog
  bridges no longer require location ARA1. `reset_rider` treats a heading within 2e-6 rad of the
  seed's as the seed (start.json headings pass through JSON/double atan2).
* `web/compare-ps2-capture.mjs` takes the course from the capture manifest `location`
  (tools/ps2_capture.py) or `--course X`; `web/test-locations.mjs` loads every prepared course
  and runs its event start.

## PS2 ground truth

`tools/ps2_navigate.py` drives the front end headlessly (pad device hook at 0x327208, after
`scePadRead`; cleaned savestates), `tools/ps2_capture.py` discovers the rider/camera/opponent
addresses per savestate and writes `location` into the manifest. Menu paths and savestate
provenance: `local/reference/pcsx2/metro-city.provenance.json`, `the-junction.provenance.json`.

### Metro-City (BRA2)

* From `snow-jam-selection.p2s`: D-pad down to Metro-City, Cross, My Rules, Cross; loading
  screen and flyby, then the race overlay (Zoe, Psymon, Allegra, Moby, Griff, Luther).
* `metro-city-ready.p2s` (phase 3), `metro-city-countdown-anchor.p2s` (phase 4, total 18,
  countdown 162, all control 6 / motion 3; countdown + anchor state),
  `metro-city-glide.p2s` (Cross on the overlay, read 360 = tick 338: every rider is airborne off
  the drop), `metro-city-glide-620.p2s` (same run at tick 620, all riders grounded: the
  registry's glide checkpoint, because the browser's glide seed must be a settled ground state).
* A pickup owned by a live entity (`mdl_BRA2_speedboost_1000`, flags 0x210325, type-1 LiveComp
  0x490B10 on a moving set piece) is left out of the browser pickups (listed as
  `unsupported_entity_pickups`).

### The Junction (BHP1)

* From `peak-1-selection.p2s`: Cross, Select Mode "Freestyle", Select Event (R&B, Crow's Nest,
  The Junction, Happiness Jam) D-pad down twice, Cross, My Rules, Cross.
* One rider (Zoe), no computer riders; the overlay shows fixed standings (Moby 162880, Nate
  98220, Kaori 44620) and a 200000 record. Same countdown/start gate as the races (phase 3 -> 4
  on Cross, 180-tick countdown, phase 5 at tick 180, push-off around 187-219 with no input). The
  HUD clock counts DOWN from 2:00 over race ticks; game+0x74 is 2 here (1 in races). 3 paths,
  3 checkpoints.
* `the-junction-ready.p2s`, `the-junction-countdown-anchor.p2s` (phase 4, total 18, countdown
  162), `the-junction-glide.p2s` (tick 338, riding the pipe, control 0).

## The Junction super pipe (2026-09-22)

The pipe has no pipe-specific controller: every difference comes from the contacted terrain patch, which the
original copies into two rider fields that the browser had hard-coded.

* **rider+0x2D4 = patch runtime flags** (patch+0xA = authored flags | 0x40), written by the ground contact 13D1B8
  (13D604; also 138960/1242B0). Readers: 114298 at 0x114A5C (a takeoff from a near-vertical wall, |+0x380 z| < 0.05,
  is scaled by 0.8 unless flag 0x20 is set; the vert patches at the lip are authored 0x29 on BHP1, so the lip launch
  keeps full speed: this was the "1.25x" lip gap), 13C948 at 0x13C9CC (flag 0x10 forces the heading boost; BRA2 has
  four 0x19 patches) and 13F178 at 0x13F248 (flag 0x2 requests the reset 116120(rider,0,1) on the ground).
  `web/core.cpp` `browserPatchFlags` (`RayHit::patchFlags` from `sourceGroundContact`) feeds every
  `originalJumpTakeoff` (core.cpp and `rail_gameplay.inc`) instead of the constant 0x49.
* **rider+0x434 = location id of the contacted patch** (`physicsState.riderType`): 1218D0 (per-rider pass from 128AC0,
  after motion) calls 22E0E0 with the track byte of rider+0x430 (= patch+0x150 = rid<<8|track, the web patch
  resource). 22E0E0 returns the index of the streaming-table 0x442168 row whose +4 holds that loaded track, i.e. the
  ELF location table 0x43E250 id (ARA1 0, BRA2 1, BHP1 11, A_ARA1 37, B_BHP1 42, ...; 50 when none; the constructor
  11B718 default is 0x31). 11..13 are the half pipes: 13C948 scales the auto boost by |normal.z| and 114298 skips the
  ground-focus launch scale; >= 17 (hubs, connectors) halves the auto boost. The event seed starts at 0x31 and turns
  11 at the first contact, which was the push-off gap (pipe-event-start 188). `web/world_bridge.cpp` maps
  `world_collision.json` `event_locations` track slots to ids.
* Found on the pipe captures, fixed for every course:
  - 105D98 (105EEC/105F50) and 1057B8's `beginPredictor` restart the flight with **1135B8 = 113198 + 113618** (clears
    hit/heading/normal/times), not 113618 alone; the reseed moved predicted-minus-elapsed by an ulp and the air
    alignment gain with it (pipe-air 402; `PRED_TRACE` on the `pipe-air-pred` capture watches the predictor).
  - **1162C8** accepts a held jump while the latch +0x360 is 0; control-0 entry 131608 clears it, so Cross held
    through a crash exit requests control 2 and control 0 skips its cruise targets (`input_bridge.inc`).
  - **108388** gates the soft collision on the motion (11FE98 == 0 or 4), not on contact: 13F178 runs 105398 after a
    passive departure while the motion is still 0 (`instance_contact_gameplay.inc`, `begin_soft_control`).
  - 139C88's touchdown pushes the rider with **106538** (+0x110 and +0x9D0, 329B40), so 13AA48 queries the body at
    the touched-down position; a crash landing keeps the air filter normal and otherwise met the terrain it had just
    landed on (pipe-uber 670).
  - The near-vertical 114298 push (+4 cm along the wall normal) happens in 13F178 after the pose, so the departure
    tick's pose is built from the pre-push position (`browserDeparturePush`).
  - **1210B0** (collision timers) requests 116120(rider,0,2) for direction changes > 4.5, more than 45 s of predicted
    air or an air bounce counter > 5 (the decay was computed but never acted on); 116120 reasons 1 and 4 post 11A088
    "Wrong Way!" (HUD slot 0x33, 1.5 s). The ground reset path of 13F178 (surface +0x44, patch flag 0x2) is wired too.
  - 116378 requests the finish control 10 only from the controller slot 0..3 (`gs.controlState`): the pipe finish
    is crossed in the air, control 10 follows the landing (`finish_gameplay.inc`).
  - **Rider query scope (rider+0x860)**: the rider's world queries (3342D0; landing probe 13A7B0 among them) only
    visit the scope lists that 120E50 rebuilds with 332DB8 from the rider query bounds +0x400/+0x410 (11E150), and
    the world job runs that refresh (26DBF0 -> 12B788 -> 120E50) once every three game ticks: after each tick whose
    record tick is a multiple of 3, with that tick's completed bounds (the scope-derived light list +0x79C/+0x7A0/+0x7AC
    changes only on those records, in every capture). 332DB8 keeps a terrain patch (node+0x24 list, box
    patch+0x158/+0x164) or an inline-box instance (3309D8, flag 0x20, box +0x60/+0x6C) when the boxes overlap
    inclusively (c.le.s); flag-0x40 entities use their virtual +0x168 box. The landing probe runs from the posed board
    root 200cm along -/+ presentation up, so for an inverted rider it reaches 200cm past the board on the head side,
    outside the bounds (-50..+250cm along the physical up, whose quaternion stays upright during a flip). pipe-tricks
    (the old "inverted landing probe" gap, browser landed at 1361): records 1361 and 1362 still read the scope built
    from record 1359's bounds (min z -463911.9), which does not reach floor patch 54799 (max z -463917.9), so the
    floor hit at fraction 0.98/0.96 is invisible; the refresh after record 1362 (min z -463934.6) admits it and the
    PS2 lands at 1363 (fraction 0.94, +0x770 = 519.01). The browser now keeps `riderScope` (web/core.cpp: rebuilt
    at the start of `step_rider` when `motionTick % 3 == npc_rider_slot() % 3`, from `originalRiderQueryBounds`, which equals the
    recorded +0x400/+0x410 bit for bit) and passes it to `originalLandingContact` ->
    `queryOriginalWorldSegment`/`sourceTerrainSegment` (`terrain_original::RiderScope`). pipe-tricks is now exact end
    to end. Oracle: `tools/test_rider_scope_landing_live.py` (`tests/rider_scope_landing_live.cpp`, pipe-tricks
    savestate + capture): 4,000 random scope boxes (332DB8 terrain and static-instance lists, 8,553 + 442 entries, equal
    `RiderScope::admits`), 2,995 landing probes (captured board roots/up, bounds 0..6 ticks old, random offsets and
    orientations; original 332DB8 + 13A7B0 vs the scoped `originalLandingContact`: all identical, 2,519 contacts; on
    16 of them an unscoped query would differ, 8 skipped on unported entity callbacks) and the captured 1361/1362 miss and
    1363 contact. Since 2026-09-28 the ground contact 13D818 (`sourceGroundContact`) and the body queries 13F488/13AA48
    (`inspect_body_contacts`, terrain and static instances) take the scope too. The refresh phase is per roster slot: 120F20
    rebuilds when tick % 3 == rider+0x86C % 3 (the human 0, computer riders 1..5), not at % 3 == 0 for everyone. Every 11D660
    placement rebuilds it (`apply_reset_placement`), and `reset_rider` clears it (docs/ai-racers.md "Parity fixes,
    2026-09-28": metro Luther 506). Still unscoped: crash 137860/138640/138960 and instance contacts 106F78/107578. The
    scope's entity (flag 0x40) boxes and 120F20's world-change rebuild are not modelled.
  - Open: pipe-uber bones 499 (physics exact to the end, 1158) is the detached board of the Uber crash (control 8 at 498, crash
    class 22 -> 136D40). PS2 facts from the capture: the detached secondary is rider+0x130 (position) / +0x140
    (quaternion) / +0x150 (flag), set at record 498 to the posed bone 22 (the browser's `cachedCrashWorld[23]` is the
    same value); from record 499 the sampled local bone 22 is identity (local-pose record 5440/5952) and the posed
    bones 22 and 23 both carry the +0x140 quaternion, but their position is neither +0x130 nor a fixed offset from it
    (499: +0x130 + (-4.16, 0.88, 7.78); 500: + (10.97, -19.86, -0.38)). The browser overrides only bone 23 with
    `crash.actor.detachedPosition` (3.2cm off at 499) and still animates bone 22 (17.6cm off). The detached-root pose
    placement in the original motion-2 pose path still has to be read.
* Oracle: `tools/test_pipe_rules_native.py` (`tests/pipe_rules_reference.cpp`) runs the recompiled 1210B0, 125228 (with
  the real __fixsfsi 4139F8) and 22E0E0 against `originalCollisionHistoryDecay`/`originalCollisionTimerReset`
  (engine/collision_event.hpp), `originalRaceTimeLimitExpired` (engine/race_session.hpp) and
  `originalStreamingLocationIndex` (engine/patch_state.hpp): 60,000 + 40,000 + 20,000 randomized cases match exactly
  (decayed fields, reset calls and arguments, the 125108/+0x480 timeout, the lookup). The flag reads themselves are
  covered by the existing 114298 oracle (`tools/test_jump_native.py`, randomized flags) and the pipe captures.
* Freestyle event rules and HUD (125228 time limit, HUD flags 0x1530C016, standings, countdown clock, FINISH! /
  TIME'S UP banners, "Final run / Nth place / pts" panel): docs/career-events.md "Freestyle run in the browser".

PS2 ground truth: `local/ps2-capture/scripts/pipe-{uber,handplant,tricks,run,timeout,brake}.json`. `pipe-finish` and
`pipe-run-event` are the first run of `pipe-timeout`/`pipe-run`, truncated before the replay restarts the tick counter;
`pipe-air-pred` is pipe-air with the predictor *(rider+0x788) = 0x5BE200 watched; `pipe-brake` (braking to the 2:00
limit: TIME'S UP at race tick 7201) and `pipe-finishov*` (banner/panel timing snapshots) are reference only. Pokes for
BHP1's rider 0x14542A0: `0x1454598:1.0 0x1454590:20.0 0x1454594:1` (full meter, tier 1). The comparer now also seeds
the score object and HUD bank from the first record in `--event` mode (the bank keeps stale front-end words).

## Verification (2026-09-22)

* Snow Jam: every generalized script re-run for ARA1 reproduces the live files byte for byte
  (web/prepare.py into a scratch `--out`, light tree, stage scripts, pickups, event instances,
  scripted instances, breath context, `build_initial('ARA1')` == ANIMATIONS/initial.json, plus the
  static exporters listed in the static-exporter work). All Snow Jam capture gates still pass.
* `web/test-locations.mjs` (npm test): BRA2 and BHP1 load through the production init path and run
  their grid start for 600 ticks.
* PS2 captures (all `--isolate`, gated in `web/test-ps2-captures.mjs`, first divergent tick):

| Capture | Baseline | Ticks | Exact through | First divergence |
|---|---|---|---|---|
| metro-event-start | metro-city-countdown-anchor | 19-707 | 707 (all) | - |
| metro-event-race | metro-city-countdown-anchor | 19-2318 | 2318 (all) | - (was 1093: tuck + boost selection 1094, soft-control rail attach 2049) |
| metro-event-race-light | metro-city-countdown-anchor | 19-2318 | 2318 (all) | Lighting bank reference exact on all 2300 ticks (`--lighting`) |
| metro-glide-neutral | metro-city-glide-620 | 621-2119 | 2119 (all) | - |
| metro-glide-carve | metro-city-glide-620 | 621-1544 | 1544 (all) | - (was 1077: 105D98 predictor restart, soft collision inherit) |
| metro-jump-tricks | metro-city-glide-620 | 621-1623 | 1623 (all) | - (was 1492: completion kind 6 = 104C38) |
| metro-air-tricks | metro-city-glide-620 | 621-1821 | 1821 (all) | - (was 887: 104C38, control 7 exit on a rail jump, air release root/style) |
| metro-mix-glide | metro-city-glide-620 | 621-2020 | 2020 (all) | - (was 1095: crash predictor restart, shared rider contact caches) |
| pipe-event-start | the-junction-countdown-anchor | 19-707 | 707 (all), score all | - |
| pipe-neutral | the-junction-glide | 339-1837 | 1837 (all), score all | bones 506 (1 ulp) |
| pipe-air | the-junction-glide | 339-1578 | 1578 (all), score/bones all | - |
| pipe-air-grabs | the-junction-glide | 339-1638 | 1638 (all), score all | bones 506 (1 ulp) |
| pipe-handplant | the-junction-glide | 339-1607 | 1607 (all), score/bones all | - |
| pipe-uber (full meter poke) | the-junction-glide | 339-1158 | 1158 (all), score all | bones 499 Uber board root (was 1143: the 105398 crashbag bounce on the departure tick now seeds the flight, 1399E0) |
| bag-bhp1/uber-bag (pipe-uber + roller pool watch) | the-junction-glide | 339-1158 | 1158 (all), score all, 15 roller ticks | - |
| pipe-tricks | the-junction-glide | 339-1898 | 1898 (all), score/bones all | - (was 1360: the inverted landing probe reads the 3-tick rider query scope) |
| pipe-finish (neutral run to the finish) | the-junction-countdown-anchor | 19-4779 | 4498, score 2861 | 2862 pointa pickup +2000 (not ported); 4499 landing ulp |
| pipe-run-event (tucked run with airs) | the-junction-countdown-anchor | 19-2589 | 2076, score/bones 2076 | 2077 "Wrong Way!" reset (stage trigger, not ported) |

* Visual: web frames at Metro-City ticks 79 (grid, countdown "2") and 618 (after the push-off,
  00:00:07, 51 mph) and The Junction tick 79 match the PS2 screenshots
  (`local/ps2-capture/runs/metro-event-start.tick79/618.png`, `pipe-event-start.tick79.png`) in
  geometry, lights, signs and camera; the PS2 Metro snow is slightly whiter (see gaps).
* Visual, pipe HUD: the browser's Single Event run on The Junction (career flow) shows the PS2 layout of
  `pipe-run.tick1220.png` / `setpieces-bhp1/full.tick4018.png`: three standings rows top left, the HH:MM:SS clock
  counting down from 00:02:00, the score with the SUPER UBER gauge, MPH, no progress meter; the Single Event Results
  table matches `pipe-run.tick1388.png` (posted scores differ by the jitter draws, a documented career RNG gap).

## Gaps

* Pipe: see "The Junction super pipe" above for the remaining divergences: stage-script triggers are not
  run (pointa point pickups: builtin27 type 6 -> 10E8B8 -> 119608; reset zones: type 5 -> 116120(rider,0,4),
  "Wrong Way!"), an immediate crashbag re-contact on BHP1 (untested; the roller itself is exact in bag-bhp1/uber-bag), the Uber board-root bone and a
  landing orientation ulp at the finish. Only the landing probe reads the rider query scope; the other rider
  queries still see the whole world (see the rider query scope bullet).
* Metro rail grind and push-out differences are closed (engine/RAIL_RECOVERY.md 3.10, docs/obstacle-collision.md
  "Metro-City contact differences closed"). Posed bones still leave the PS2 late in the Metro captures
  (gated with `bonesThrough`) without affecting physics.
* Glide seeds cover the ground profile/state, boost state and landing ticks; the idle timer
  (+0x35C) and the retained speed limit (+0x2E4) are not seeded, which is why the Metro glide
  checkpoint is the settled tick-620 state rather than the 475 post-landing state
  (`compare-ps2-capture.mjs --seed-idle --seed-limit` copies both from the first record).
* Opponents (Metro-City has the same five computer riders as Snow Jam; separate agent), the BRA2
  entity-owned pickup and BHP1's animated cars (`mdl_BHP1_caryellowanim_*`, type-17 Object
  entities live at the countdown) are not simulated; `B_BRA2`/`B_BHP1` painter records are not exported.
  BRA2's Lighting painter (5 bright banks by area) now runs live (docs/terrain-render-fidelity.md,
  "Rider Lighting painter by area").
* Web sky/fog start: BRA2/BHP1 fog start is painted entry 0 from the disc tree query; the RAM
  SkyBox/fog objects were not read.
