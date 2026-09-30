# The connected Peak 1 mountain: free ride, streaming, stations, peak runs

Owner: the Peak 1 mountain agent (2026-09-23). This is the contract for the other Peak 1 agents: freestyle (ASS1/ABA1),
backcountry (ABC1), world visuals, audio and characters. Research notes with every address and PS2 run are in the
session scratchpad (`peak1/streaming.md`, `freeride.md`, `peakrun.md`, `captures.md`). The facts below were checked
against them.

## Files

| File | Role |
|---|---|
| `tools/export_peak_world.py` | One package per Peak 1 **location** in `local/assets/native/PEAK1/<LOC>/` and `web/public/assets/PEAK1/<LOC>/`, using the course package format. Each holds world.json/bins/PNGs, terrain.json, world_collision.json, rails.json, terrain-render/-lighting/atlas, fog-tree.json, screen-tint.json, paths.json and instance-flags.json. It also writes `PEAK1/peak.json` (locations, streaming rows with read times, residency rows, map ids) and `PEAK1/environment.json/.bin` (every location's colour lattice). Disc only, except instance-flags. |
| `tools/export_peak_instances.py` | Free-ride instance audit from CTM free-ride savestates (`menus/fr`, `streamres`, `ctm/state-*`, then `menus/fr-courses/*-freeride.p2s`, which only fill locations no older state covers): the draw class hides instances. It also writes the DeadNode / type-16 runtime flags to `instance-flags.json`, but the browser no longer applies them: the section activation produces those nodes itself. |
| `tools/export_peak_sections.py` | `PEAK1/SECTIONS/sections.json`: the 0x101B60 section activation for the whole peak (1914 instances with a slot-1/3 program, 369 programs). Slot ids are `(track << 16) \| program`. Cells are checked against the live octree of 77 free-ride / peak-run savestates, and `creates` comes from the entities those savestates hold. |
| `tools/export_peak_rail_flags.py` | Rail runtime flags and surfaces (+0x1C/+0x28), taken from every PS2 savestate that has the location resident. |
| `tools/export_peak_stage.py` | `web/generated/peak_stage_seed.hpp`: the stage (LUN) tables of all 16 locations (`browser_stage_peak1`, world location `PEAK1`). |
| `tools/export_peak_seed.py` | The PEAK1 glide seed and `PEAK1/initial.json` + `start.json`, built from a free-ride capture baseline (the capture comparer's mid-run start). Also `PEAK1/seed-state.json`: the baseline's node entities, the entity class of each listed instance, and the 0x101B60 list. |
| `engine/world_residency.hpp` | Per-track resident flag. Every world query skips non-resident tracks: terrain patches (ground, segment, body, roller), instances (body, air, roller), rails. All tracks are resident by default, so race events are unchanged. `worldTrackOctree()` is the per-track "in the activation octree" flag read by the section activation (`engine/section_streaming.hpp` `Activation::present`). |
| `web/peak_world.inc` (in `world_bridge.cpp`) | Append mode for the ordinary init functions, the streaming state machine, path banks, stage builtins 68 and 67, instance flags, and comparer hooks. |
| `web/peak-world.js`, `peak-world-worker.js`, `peak-world-batches.js` | Collision data. A worker fetches a location and cuts it into small append documents (3 patches / 10 instances). The main thread feeds them to the core within a per-frame budget. |
| `web/free-ride.js` | The streamed world in the page. The start row loads under the loading screen, the rest prefetches in the background. Draw packages are built in slices, their pipelines are compiled while detached, and each is shown exactly while its row is active. Also: the painter region (Fog / ScreenTint per location), the sky dome swap, station prompts, the builtin 67 gates, splits, and the `?course=PEAK1` entry. |
| `web/free-ride-hud.js` | Free-ride HUD (collectible counter, cash) and peak-run HUD (countdown clock, split). |
| `web/peak-run.js` | Peak challenge rules (tiers, limits, splits, results). `career.js` `peakSetup` / `peakRunResult` and `career-ui.js` use them. |
| `web/peak-capture.mjs` | `compare-ps2-capture.mjs --course PEAK1`: loads the whole peak and drives the rows from the capture. |
| `web/peak-set-pieces.js` | The stage world of the streamed world (section "Set pieces and painters"): one core stage world for PEAK1 (`init_stage_world` with `PEAK1/SETPIECES/`), particles and halos for the whole world, LiveComp / UV scroll / flag cloth players and instance states over the location draw groups `free-ride.js` attaches and releases. |
| `web/cutscenes.js` | `playCutscene({kind, id, rider, location, until})`: the cutscene hook of the transport, station and lodge flows (section "Cutscene hooks"). A stub here; the cutscene agent owns NIS playback. |
| `PEAK1/SETPIECES/`, `PEAK1/LIGHT_GLOW/`, `PEAK1/lighting-banks.json`, `PEAK1/<LOC>/{lighting,sun-painter,glare-painter}.json` | Written by `tools/export_peak_world.py` (`setpiece_packages`, `light_glow_package`, `painter_packages`). The location packages split their batches by the merged set pieces (LiveComp / MeshAnim nodes, UV-scroll groups, script-changed instances, flag cloths), like an event package. The exporter now also re-runs `export_peak_rail_flags.py` for the exported locations (a plain copy of the native rails.json has no runtime flags and the core refuses it). |
| Tests | `web/test-peak-world.mjs` (npm test) and `web/test-peak-run.mjs` (npm test). |

## Locations and residency (SLUS_207.72)

- **Location table `0x43E250`** (24-byte `{id, name[16], kind}`), Peak 1 entries:
  - Courses: ARA1 0, BRA2 1, ASS1 5, ABA1 8, BHP1 11, ABC1 14.
  - Hubs: A 17 (Green Base Station), B 18 (Blue Base Station).
  - Connectors: DRA4_A 35, ABC1_A 36, A_ARA1 37, A_ASS1 38, A_ABA1 39, ARA1_B 40, B_BRA2 41, B_BHP1 42.
  - TRANSP 43, ASKY 44, BSKY 45.
- **SDB tracks** (resource & 0xFF) use a different numbering:

| Location | Track |
|---|---|
| A | 1 |
| A_ABA1 | 2 |
| A_ARA1 | 3 |
| A_ASS1 | 4 |
| ABA1 | 5 |
| ABC1 | 6 |
| ABC1_A | 7 |
| ARA1 | 8 |
| ARA1_B | 9 |
| ASS1 | 11 |
| B | 12 |
| B_BHP1 | 13 |
| B_BRA2 | 14 |
| BHP1 | 15 |
| BRA2 | 16 |
| DRA4_A | 33 |

- **Residency table `0x442488`** has 23 rows of 40 bytes, one per course index: `{course, count, sky, TRANSP, <=6 location ids}`.

| Course | Locations | Sky |
|---|---|---|
| 0 Snow Jam | ARA1, A_ARA1, ARA1_B | ASKY |
| 1 Metro-City | BRA2, B_BRA2 | BSKY |
| 5 R&B | ASS1, A_ASS1 | ASKY |
| 8 Crow's Nest | ABA1, A_ABA1 | ASKY |
| 11 The Junction | BHP1, B_BHP1 | BSKY |
| 14 Happiness | ABC1, ABC1_A | ASKY |
| 17 Green Base Station | A, A_ARA1, A_ASS1, A_ABA1, DRA4_A, ABC1_A | ASKY |
| 18 Blue Base Station | B, B_BRA2, B_BHP1, ARA1_B | BSKY |

- A race event keeps its row resident; the event packages of `web/prepare.py` are these rows merged. Free ride and the peak runs switch rows while riding.

## Streaming (the recovered state machine, ported in `web/peak_world.inc`)

**Streaming table and streamer.** The table `0x442168` has 50 rows of `{id, track, state, class}`. Class 0 is a location, 1 is TRANSP, 2 is a sky. The streamer is at `W+0x78`, where `W = *(*(gp-0x848)+0x84)`.

**Row states:**

| State | Meaning |
|---|---|
| 0 | absent |
| 3 | load wanted |
| 4 | load wanted, activate on arrival |
| 6 | reading |
| 8 | reading, then activate |
| 1 | resident, inactive: in the octree, but patch flag 0x40 is clear, so neither collidable (0x41) nor drawn |
| 2 | resident, active: collidable and drawn (track state 6) |
| 5 | unload wanted |
| 7 | unloading: no longer drawn, still collidable until its chunk slot is evicted |

**Transitions:**
- 22CEA8 is called by the Unload trigger (builtin 68 action 0):
  - Rows of every location outside the new row: 1/2/6 → 5, 3 → 0; rows in 4/8 are kept. Skies and TRANSP are never released.
  - Rows the new row wants: 0/7 → 3, 5 → 1.
- 22D088 is called by the Load trigger (action 2) and by the initial load:
  - Wanted rows: 0/3/7 → 4, 6 → 8, 1/5 → 2 **in the trigger tick**.
- 22D8D8 makes one pass at the start of every game tick. In the browser, `peak_world_tick()` runs after `race_end` and stands for the next tick's pass. In order:
  1. **Unloads:** 5 → 7; 7 → 0 at the eviction, 7 passes later. Any row still in 5/7 blocks new load starts in that pass.
  2. **Wanted rows:** 3 → 6 and 4 → 8.
  3. **Reads:** from the next pass, one disc read at a time. Order is by slot priority: sky 0.92 > TRANSP 0.91 > location 0.90, then the lowest chunk index.
  4. **Read completion:** the resolver delivers the path bank (12A340). On the following pass the row goes 6 → 1 or 8 → 2, and the next read starts.
  - A sky that reaches 1 is activated at once (22DE58). It replaces the dome (22DE98) when the switch is allowed. The switch is allowed by +0x1A4, which is set by the `skybox_trigger` volumes through builtin 68 action 5.
  - 0x535C08, the current course, changes on the first pass after a request (22DF50).

**PS2 timings** (frA1/frA2, frB1..3, fr-tuck, peak1-fr-aara1):
- The old locations stop drawing on the pass after the Unload trigger. They leave the octree 8 passes after it.
- New reads start at +9.
- **Read times** (main chunk = the location's last SDB chunk):

| Location | Read time (ticks) |
|---|---|
| A | 75 |
| A_ABA1 | 23 |
| A_ARA1 | 36 |
| A_ASS1 | 21 |
| DRA4_A | 25 |
| ARA1 | 238–250 |
| ARA1_B | 21–25 |
| B | 57 |
| B_BHP1 | 34 |
| B_BRA2 | 27 |
| BSKY | 30 |

  - These are deterministic for the same history. They vary with the texture sub-chunk reads in flight.
  - Unmeasured locations use the chunk size at 20.5 KB/tick; `peak.json` marks them `measured: false`.
- The Load trigger came 629–665 ticks after the Unload trigger on every run. The reads had finished long before, so the ride never waited.

**Trigger volumes** (collision node flag 2, slot-2 programs, 121818 → 30A060):
- Every connector has an Unload volume and a Load volume. The Unload one is on the hub/upper side and is met first going downhill. Each program hides its own volume (builtin 29) and unhides its partner (58).

| Connector | Unload → 22CEA8 | Load → 22D088 | Destination, entry mode |
|---|---|---|---|
| A_ARA1 | rid 115 | rid 42 | ARA1, 2 |
| A_ASS1 | rid 89 | rid 108 | ASS1, 3 |
| A_ABA1 | rid 138 | rid 228 | ABA1, 4 |
| ABC1_A | rid 94 | rid 181 | A, 1 |
| DRA4_A | rid 59 | rid 28 | A, 1 |
| ARA1_B | rid 36 | rid 135 | B, 1 |
| B_BRA2 | rid 78 | rid 101 | BRA2, 2 |
| B_BHP1 | rid 153 | rid 81 | BHP1, 5 |

- Hubs A and B: `mdl_A_NIS_Lodge_0` triggers builtin 68 action 4 (lodge door) and `mdl_A_NIS_Transport_0` triggers action 3 (transport booth); B's volumes do the same.
- Builtin 68 keys:
  - key0: the course table `+0x5C` map id (ARA1 0, BRA2 1, ASS1 6, ABA1 9, BHP1 12, ABC1 15, A 18, B 19).
  - key1: the action (1 does nothing).
  - key2: the entry index into table 0x445E40 = {2,3,4,5,1,1,1,0}.
  - Human riders only.
- Builtin 67 (22D6C8) is triggered by the RaceRideState volumes of ARA1, BRA2, BHP1, ASS1 and ABA1. In CTM free ride on world screen 4, outside the backcountry, it turns the ride into that course's event: the PS2 opens the race card.

**Path banks.**
- When a location with id < 22 finishes its read, 12A340 replaces the race/reset bank 0x4D33A0:
  - hubs use variant 1 in a time challenge;
  - hubs use variant 2 in a points challenge (1 in mode 11);
  - everything else uses variant 0.
- 112180 then re-attaches the human to the region row's paths: 26B5E0(bank, 1, player 0) = exported AIP kind 0, index 0 (runtime kinds are the exported kinds + 1; the first row when none matches):
  - `+0x20` is the reset (AI) path and `+0x24` is the race path;
  - +0x4D0 = +0x4D4 = remaining-at-origin − 26A638 distance, or remaining-at-origin itself when it is smaller.
- The old bank is dropped when its location is evicted (12A490). Until the next read there is no bank, and neither the race progress nor the route moves.

**Other per-location effects.**
- Painters (kind 15) are per location: every Peak 1 location has one record with Fog (5), ScreenTint (7), Sun (9), Lighting (11) and breath (12) sections, plus glare (6) on BRA2, BHP1 and ABC1. The painter region is the track of the rider's last contacted patch; a region change swaps every section's tree and keeps its blend state (see "Set pieces and painters").
- 22E180 (from 22D088: the Load trigger and the initial load) sets the default rider irradiance bank gp+0x12D4 = xPBR1 by course (jump table 0x47B4E0: A for 0, 5, 8, 14, 17; B for 1, 11, 18; ...). An empty Lighting reference (the painter reset 2BE1F8) reads it.
- Stage programs (kind 16) are built at the read completion and torn down at the unload start (see "Collectibles").

**Section activation (0x101B60) in the streamed world.** One activation octree holds every resident location:
- A location's instances go into the octree when its read completes (resolver kind 3 → 328C20), while the row is still 6/8.
- 3A9858 (row 6 → 1 / 8 → 2) calls 230338, which sets A+0xD0 = −1, so the next pass rescans.
- The unload start (row 5 → 7) calls 230360. That runs 103308, which drops the location's instances from the list without leave handlers, and tears down its stage programs and entities.
- The eviction (3AB498 → 3284B8) takes the instances out of the octree. A later read brings them back without entities, at their authored flags (`browser_stage_track_reset`).
- The slot-1 programs are what kill the challenge reset planes, mode fences and startmode colliders in free ride. For example, `mdl_A_challenge_reset_plane_s_1020` runs `builtin43(6)`; the kind is 4, not 6, so it calls `builtin2` and becomes a DeadNode. Without this, touching the plane calls `builtin27` effect 5, "Wrong Way!", and a reset (116120 reason 4).
- In the browser, `peak_world.inc` sets `worldTrackOctree` from the row transitions and calls `browser_section_rescan` and `browser_section_drop_track`. `free-ride.js` loads `SECTIONS/sections.json`; `main.js` already runs `section_pass` after every tick. The PEAK1 slot-1/3 programs run without a stage-world package (`stage_world_section_program`).

**Browser data.**
- Collision is fed in slices from a worker. A location's data is always in the core before its row can become active. If a row is active, or waiting to activate (4/8), without its data, the game waits the way the original's +0x1D0 wait does (2306B8).
- Draw packages are built for every wanted row: `main.js asset()` in slices of about 4 ms, pipelines compiled detached (`renderer.compileAsync`). They are released 45 s after their location leaves.
- Chrome, building BRA2's draw package while riding: median 13 ms, worst frame 31 ms.
- **Read-ahead, the draw gate and the budgets** (2026-09-27, pv `streamAhead` / `streamGate` / `streamWarm` / `sliceLoad`; [ctm-flow.md](ctm-flow.md) section 8):
  - `streamAhead`: one background job at a time in the PS2's order (the rows the current row's connectors lead to, lowest chunk first; at a station the connector nearest the rider), the draw files downloaded ahead and built once the rider rides into the connector (or under the arrival movie when there is a single next row); the rest of the peak's collision only while nothing animates.
  - `streamGate`: an active row is also held until its draw package is built (never collidable and undrawn).
  - `streamWarm`: pipelines compiled for the world pass's render context (call depth 1), unculled, two-pass materials both sides, in batches sized to the frame's build budget (stalled 36 ms, wanted 10, ahead 4, idle 24).
  - `sliceLoad`: a location's rail catalog in ~24 KB parts (`init_rails` segment_base / partial, `rail_load_hash`); one whole catalog was 20-55 ms of core time at 1x.
  - Worst stall at every Peak 1 connector: 0, phone 4x and desktop.

## Free ride (game mode 12, kind 4)

- **Rules** (handler 2, vtable 0x47CE00, init 23B170): no rounds, results or AI. The event counts as complete from the start (+0x9C = 1). There is no time limit, and 125228 never DNFs a free-ride rider.
- **Finish events** (10E5D8, `race_finish_eligible`): they count only on Metro-City (1) and the freestyle courses (5..13). There is no finish on Snow Jam or Happiness.
- **Start** (1A0720): a new career starts at Happiness (14), dropped from the plane. Later starts are at the last lodge (profile +0x27C, 17 at first).
- **Region placement 11DE60(rider, index, kind)** → 26B5E0(bank 0x4D33A0, kind, index): the row with `+4 == kind` and `+0 == index`, or **the bank's first row** when none matches. The runtime kinds are the exported (disc) kinds + 1 (PS2 fr/sj-01, no-a, race18001): grid slots are kind 1, session points kind 2. Then 11D660 (semantic 5, clearance 0) and 11DF18: velocity = forward (+0x1B0 after the placement) × 833.333 with **z = 0** (`sw zero, 0x1E8`). `free-ride.js spawnFor(course, index, runtimeKind)`; the velocity comes from the placed orientation (`forwardVelocity`).
- **Arrival at a location** (11D390, kinds 4..6; `arrivalFor`): courses < 14 → 11DE60(rider, 1, 2) (session point 1), backcountry → 11DE60(rider, 0, 1) (grid slot 0), stations → 11DE60(rider, 0, 2) (first row = session point 0 at A). Verified by the PS2 Transport arrivals `local/ps2-capture/runs/peak1-arrive-{ass1,aba1,bra2,bhp1}` (the placement record equals session point 1 of each course to the cm, velocity (forward.x, forward.y, 0) × 833.333). Before this fix Metro-City, R&B, Crow's Nest and The Junction (which have no kind-1 index-0 row) fell back to the Snow Jam start and the rider fell through the void. The placement is the original's: core `place_rider_region` (11D390 score reset, 11DE60 control/motion 0, 11D660 semantic 5 clearance 0, 11DF18 chop-rounded velocity, the 11D390 tail; a transport arrives from the loop's limbo state), called by `free-ride.js placeRegion` from `main.js resetPhysics`; a location entry first re-attaches the route at the grid slot (112180). Gated: `peak1-arrive-{ass1,aba1,bra2,bhp1}` exact to the end.
- **Session** (MCOMM → Session, overlay 0x20): the course's points from table 0x440770 (+0x18 count by 1545F8: ABC1 7, A 1, ARA1 7, ASS1 7, ABA1 2, B 1, BRA2 8, BHP1 2). Item k is "%s %d" (kT_MAPSessionPt, k + 1), the first kT_CMNTopOfRun, the last kT_CMNBottomOfRun unless there are two (208840); the item's value (+0x18) is k + 1, which the confirm stores in P[0xA]+0x10 (overlay +0xD8). "Session this area?" Yes → world state 15 (236058): 11DE60(rider, k + 1, 2) at the current course, 11DF18. Every course has exactly its count of kind-1 rows 1..count (point 1 = the entry at the top, the last = the bottom; ARA1's and ABC1's kind-1 index 0 rows, the bottom exit and the plane drop, are not session points). `career-ui.js` `ctm-session` / `ctm-sessconfirm`, `main.js freeRideSession`. The SessionViewPoints map and the white fade are not drawn.
- **Transport inside the world** (MCOMM Transport or the booth → "Transport to this area now?" Yes: world state 14 arg 1, 236250/236418; `free-ride.js transport`, `main.js transportInWorld`): to the current location (outside the backcountry) it is state 15 with Session point 1; otherwise the transport cut, 22CEA8(dest, 7) (`peak_world_transport(dest, 0)`), the crossing streams the destination rows behind the loading loop (screen 11 with +0x1C8: 236960 waits for 22D278, `peak_world_request_resident`), the arrival placement (123F38 → 11D390), then 22D088(dest, 7) (`peak_world_transport(dest, 1)`, the dome switch allowed). The world is paused meanwhile; the streamer passes run one per 1/60 s, so the reads take their disc time (Green → Metro-City ≈ 4 s in Chrome). A station destination ends at the lodge prompt (PS2 ctm/60-green-arrive, 61-cut). Audio: `gameAudio.travel(dest)` (27A860 → 28E8C0(20)) and `arrived()`. No page reload.
- **Collectibles** (builtins 37/38/39): each location's stage opens the ONE collectible slot ctx+0x2C0 (30B928: only when it is free) when the location's read completes (the global handler programs rerun, `browser_stage_track_setup`), and the slot is freed at the location's unload start (230360 → 308FE0, `browser_stage_track_teardown`). At a run start the first resident stage in load order takes it. The career row is that location's course (profile record (setKey + *0x5305F0) × 12): `web/stage-collect.js PEAK1_COLLECT_TRACKS` (track → course), `set_stage_collect_row(track, lo, hi)` per location, `stage_collect_track()` for the collect events; `Career.markCollected` saves them (`ssx3.career.v1`). Free ride and the peak runs are always the CTM path (0x535C11 = 0), so the rows and the path are set after the run's stage reset (`free-ride.js afterReset` → `collectStart` → `peak_collect_refresh`, which rebuilds the slot with them). Chrome, Green station: riding through `mdl_A_collecta_0004` gives 1/5, $500 (peak-1 award) in the save; the pickup's LiveComp, magnet and halo come from its slot-1 program.
- **HUD** (flags 0x1530C380):
  - 0x80, collectible counter: snowflake plus "n/N" for the current course. N comes from table 0x43FA70: Snow Jam 30, Metro 35, R&B 30, Crow's Nest 2, Junction 4, Happiness 44, stations 5.
  - 0x100: cash "$ n".
  - No clock, score, place or progress meter.
- **Stations:**
  - The lodge door (action 4) leads to world state 14 arg 0, the walk-in cut, then overlay 0x1F "Would you like to enter the lodge?". Yes goes to Lodge - Peak N. No rides in again from session point 0.
  - A transport arrival at a station parks the rider at the lodge door with the same prompt.
  - The transport booth (action 3) opens the Map overlay 0x21.
- **MCOMM** (Start, overlay 3): Return / Transport / Session / Messages / Audio / Options / Quit. During a location crossing (screen 11) it is overlay 4, without Transport or Session.
- **Freeride Transport list** (table 0x478D38), Peak 1 order: Happiness, Green Station, R&B, Snow Jam, Crow's Nest, Blue Station, Metro-City, The Junction.

### Station fences: stage builtin 108 (2026-09-27, pv `stationFences`)

Owen reported invisible walls before the lodge area at Green Station in Conquer the Mountain.

**Cause.**
- Every station (A Green, B Blue, C / D on Peak 2, E on Peak 3) carries two families of peak-run fences and challenge reset planes:
  - `_r`: the Peak Race route (`fenceb_r`, `fencebuv_r`, `challenge_reset_plane_r`, and at A the collision-only `fenceCollision_r_*`);
  - `_s`: the Peak Jam route.
- Their slot-1 programs kill them at the section enter:
  - race: `if 43(5)==0 and 108(11)==0: builtin2()`;
  - jam: `if 43(6)==0 or 108(11)!=0: builtin2()`.
- Builtin 108 is 0x302870 -> 303E60. It returns `n < 15 (unsigned) and table 0x446618 {0..13, 1}[n] == (s8)0x535C12` (the game mode byte). Its parse default is -1, which gives 0.
- The stage VM had no case for 108 and answered nil. `nil == 0` is false, so the race family never died in free ride (mode 12).
- The draw audit hides those fences, because the PS2 free-ride savestates have them dead. Their collision stayed, so they were invisible walls.
- The worst case is A's `mdl_A_fenceCollision_r_1016..1022`: a line along y ~35000 between the station start and the lodge. With a neutral pad from the last-lodge start, the rider hit resource 1 at tick 230, crashed (control 7 -> 9) and was reset.
- The PS2 (state-green-cutscene, start-lodge) has all of them as DeadNodes (flags 0x200204 / 0x200304, node type 6).
- The same gap left the race fences alive in the Peak Jams (mode 9 / 10). In All Peak Jam (mode 11) it kept the `_s` fences alive, where the PS2 kills them.
- No event course program calls 108 (scan of every stage table).

**PS2 per mode** (fences.py over the savestates; the counts are the fences listed at that moment):

| mode | `_r` | `_s` |
|---|---|---|
| free ride 12 | all DeadNode (A 35, B 16, C 31, D 25, E 31) | DeadNode |
| races 6 / 8 | alive | DeadNode |
| All Peak Jam 11 (E) | alive | none at E |

**Port.**
- The core is `web/peak_world.inc` `browser_peak_builtin108`, stage VM case 108 in `web/stage_script_gameplay.inc`, and export `peak_world_builtin108(on)`.
- It is off by default in the core. `web/free-ride.js start` turns it on with pv `stationFences`, and `web/peak-capture.mjs` does the same (`STATION_FENCES=1/0` forces it).
- Checked in the page at every station after a free-ride start: every listed race fence is a DeadNode (A 35 + 42 jam, B 16, C 20, D 16 + 17, E 24); with the switch off none are.
- In a streamed free-ride run, builtin 21 (0x2FE2C0, the UV scroll that the JS renderer owns) is the only builtin still answered nil.

### Fresh rider at a world start (2026-09-27, pv `freshRider`)

**PS2.** A world load builds a new rider. Its constructor is 0x125EB8, over zeroed memory (PS2 `start-lodge` saves: all zeros at sample 340, placed by 380). Before its first placement 11D390, these fields are 0:
- +0x2E4 speed limit;
- +0x4CC route heading;
- +0x438 surface;
- +0x380 previous normal;
- the +0x244 triplet;
- +0x2E8..+0x2FC; the drain word +0x304 is 1.

As a result:
- 11B3F8 ramps the limit from 0: L += 0.1 x (target - L), giving 220.9, 419.3, 598.1 and so on.
- The 833.333 cm/s push of 11DF18 is held to about 213 cm/s.
- 13C948 reads heading 0 on tick 0.
- The placement's 13C7A8 (controller = the motion owner) sets depths +4/+8 = bodyScale x material 0 = 0.425 / 2.1285 for Zoe.

**Before the fix.** The port placed its rider from the world's glide seed: limit 2219.7, heading -2.96, depths 25.1 / 51.0. The station start ran 3.5x faster and overshot the lodge door.

**Port.**
- Core `fresh_rider_start()` (`web/animation_bridge.cpp`) sets those fields.
- `web/free-ride.js placeRegion` calls it before the world start's first `place_rider_region` (free ride, kind 4, not a transport; pv `freshRider`).
- Peak runs, transports, sessions and events are unchanged.
- The Happiness plane drop is not covered, and should not be. The PS2 rider there (ctmstart f02700) comes out of the plane cut's limbo (control 13 / motion 3): +0x4CC 3.0498, +0x380 (0, 0, 1), +0x2E4 3333.33 (the air limit, recomputed every air tick). It is not fresh. `fresh_rider_start` before the drop changes nothing (R10 and R11 setups bit-identical through 235 / 400 ticks).

**Gate.** `peak1-green-start` (`compare-ps2-capture.mjs --peak-fresh`, npm test):
- The capture is the CTM last-lodge start with a neutral pad (`local/ps2-capture/peak1/green-start-t0.p2s`).
- Physics and boost are exact through 393. At 394 the PS2 takes the lodge walk-in (world state 14); the page fires the door's program on the same tick.
- Before the change, tick 1 already differed, and with only the fresh rider the fences stopped it at 230.
- Without the RNG sync, a soft collision at 286 draws a different gameplay RNG word. The PS2's RNG at a world start depends on the menu history.
- Not seeded: the hair spring (bones 24..26) and the cash HUD slot (no career in the comparer). +0x390 is the creation spot's normal, which the port does not reproduce; it did not change this ride.

### Finish barriers: builtin 0's Object collision route (2026-09-27, pv `finishFences`)

Owen reported that in the All Peak Race, at a course finish on the way down, the barriers are drawn but the rider goes
through them and is then reset.

**PS2.**
- In free ride and the peak runs (kinds 4 / 5 / 6), fences close the finish areas of the courses a route passes:
  - Snow Jam: `mdl_ARA1_mode_fence*_finish`, `challenge_reset_plane_finish`, `endmode_collide`;
  - Gravitude: `mdl_ERA5_mode_fence*_r`, `challenge_reset_plane_finish`, `fencecollision_end`;
  - Ruthless Ridge: `CRA3 *_end`;
  - Intimidator: `DRA4 end_*`.
- The drawn fences and reset planes die in races and freestyle: `if 43(4)==0 and 43(5)==0 and 43(6)==0: builtin2()`.
- At CRA3 and DRA4 the collision pieces are ordinary static collision (flags 0x200000; PS2 APR 0x200022).
- At ARA1 (`endmode_collide_1000..1005`) and ERA5 (`fencecollision_end_1000..1004`), the collision pieces are the only ten instances of any package authored with flags 0. They load unrouted (instance+8 = 2).
- Their slot-1 program is `if 43(4) or 43(5) or 43(6): builtin0(key1 1)`.
- Builtin 0 (0x2FC0D0) builds an Object entity through 356DB0 (type 0x11). With key 1 != 0, 356DB0 sets instance+8 = (flags & ~0x40) | 0x20, the static collision route. With key 2 == 0 and (flags & 3) == 3, it also sets (flags & ~2) | 4.
- Confirmed in the PS2 savestates:
  - APR at ERA5 (`apr-part4.end34176`): 0x122, node type 17;
  - APR at ARA1 (`apr-finish.rec114000`): 0x322, type 17.
- In `apr-finish`, the autopilot crashes into ARA1's barrier (control 8 at 114631) and then scrapes along it (control 3 ...).
- In a derived push (apr-part4 state, rider accelerating east), it is stopped at x -184141 by `fencecollision_end_1001` and deflected back.

**Before the fix.**
- The port marked those ten instances unsupported ("Dynamic entity callbacks unavailable").
- A body query meeting their box was therefore incomplete, and the contact phase answered nothing that tick.
- The rider went through, then met the finish reset planes and was reset.
- Builtin 0 changed no flags.

**Port.**
- `web/world_bridge.cpp stage_object_route(on)` / `object_route_instances`: the ten instances load as static instances with runtime flags 2 (no route).
- Case 0 of `web/stage_script_gameplay.inc` applies 356DB0's key-1 route through `stage_set_flags`.
- Both are gated by pv `finishFences`, set by `web/free-ride.js start` and `web/peak-capture.mjs` (`FINISH_FENCES=1/0`).
- Event courses need nothing: their countdown skip list (event_instance_seed.hpp) already gives the ten instances runtime flags 2. Every query tests that Skip route before the "unsupported" label, so ARA1 / ERA5 event queries through those boxes are complete and find nothing, as on the PS2 (flags 2). Checked: 0 incomplete ticks, 8 pushes per box.
- The key-2 draw change of 356DB0 is not ported (it changes no collision).

**Checked.** The PS2 runs replayed in the page, with the same position and velocity, in PEAK3 / PEAK1 free ride (kind 4 routes the barriers too), identical in Chrome and WebKit:
- **ERA5 push.** Stopped by resource 358701 at x -184131 with control 3, then deflected back along the PS2's line; x at +100 ticks within 14 cm of the PS2.
- **ARA1 crash.** Control 8 at tick 72-80 into `endmode_collide_1005`, as the PS2 at 72.
- **Switch off.** The rider rides through, the body query is incomplete inside the boxes, and ARA1 resets it (control 9).
- **Routes.**
  - All Peak Race and All Peak Jam (same route, kinds 5 / 6) meet the ERA5, CRA3, DRA4 and ARA1 finish barriers.
  - The Peak 1 Race meets ARA1's; the Peak 2 Race meets DRA4's and ARA1's.
  - The kind test is the same for all of them. The APJ at ERA5 / ARA1 has no PS2 state yet.

### Course limits in free ride (2026-09-29, pv `loadFlags`)

Owen: CTM course limits "feel slightly off" (boundaries, walls, resets). Tools in `local/course-limits/`:
`scan_builtins.py`, `asm.py` MIPS listing from local/output, `lun.py` LUN listing, `ps2inst.py` / `scan_ps2.py` PS2 instance
states, `port-states.mjs` + `compare.py` page-vs-PS2 instance diff, `capture-all.sh` / `cmp-all.sh` captures and comparisons,
`page-replay.mjs` page replay of a world-start capture, `seed_boosts.py`. Captures: `local/ps2-capture/runs/course-limits/`.

**Unported builtins 99 and 101: no effect in CTM.**
- 99 (0x3061B0) answers the game options word *0x5308D0: selector 0 / 1 / 2 -> bit 6 Multipliers / 8 Power-ups / 7 Point
  icons clear (others 1). Written only by the options setter 192088 (options screen 192380). All 162 calls are
  `if 99(sel)==0: builtin29, return`; 0x5308D0 is 0 in all 794 PS2 free-ride savestates, and the port's nil (nil==0 false) takes
  the same build branch.
- 101 (0x306438) adds an attention point (instance, radius 1000, seconds) to the 64-slot manager W+0x84->+0xC->+0xA8
  (0x101728 / 0x1013A8). Its only consumer ends in rider+0x5B0 (122CF0), read by nothing else; the change callback 11A0C0 is an
  empty stub. It returns nil, as the port does.

**The one-way volumes never pushed (fixed, pv `loadFlags`, on).**
- Every hub connector (and ABA1) has a `*_onewayvolume_*` whose slot-1 program runs builtin 7 (0x2FBEC8, Boost entity type 8):
  a human moving against the volume's +Y is pushed along +Y by 100 km/h x 0.9 per second (41.667 cm/s a tick) once armed.
- The load's runtime flags of an untouched instance are the authored high half copied down with bit 1 (34FC1C:
  0x200000 -> 0x200022, 0x210000 -> 0x210023); this holds for every untouched instance of the 794 PS2 free-ride savestates.
- `stage_flags()` (web/stage_world.inc) fell back to the AUTHORED word for an instance nothing had changed, so builtin 7's
  `| 0x100` gave 0x200100: no static route, the rider list (rider+0x5B8) never held the volume and it never pushed. PS2:
  0x200122 / 0x200322 on every connector volume.
- Port: core `stage_load_flags(on)`: the fallback is `originalPickupRestoredFlags(authored)`. `web/free-ride.js start` sets it
  from pv `loadFlags` (stop clears it); `web/peak-capture.mjs` too (`LOAD_FLAGS=1/0`). Event courses do not set it and need
  nothing: their volumes keep the static route (no runtime flags written; ABA1 and ARA1 checked in the page after 400 ticks).
- Checked: `course-limits/p3b-zig3000` (Peak 3 seed fr-ebc3-14302, zigzag) is pushed by `mdl_EBC3_E_onewayvolume_1000` from
  14525. Without the push the port is 30 cm off after 10 ticks and 23 m after 12 s; with the volume built (a mid-run seed only
  lists it as a plain entity: `PEAK_SEED_BOOSTS=local/course-limits/boosts-p3b.json`, core `stage_seed_boost`, words from the
  savestate) positions are exact for all 3001 ticks (the velocity sample is one push early during the 34 push ticks: the
  record is written after the entity pass). The streamed-world gates (peak1 glide / green start / lodge / arrivals / race start,
  peak2 fr-d-glide, peak3 throne, ctm/fr-dra4a-full, allpeak, weather/frd) are unchanged with it on. Chrome and WebKit, Crow's
  Nest start: 0x200122 on A_ABA1 x2 and ABA1 with the switch, 0x200100 without.

**Back-to-back soft collisions ended early (fixed, core).**
- Scraping along a wall gives soft collisions a few ticks apart. The port's control-3 exit asked whether ANY channel-2 sequence
  of the soft clip had completed; the previous soft's clip (completed, fading out behind the new one) answered yes, so the new
  control 3 ended after 2 ticks and the rider left the wall. 312AE8 reads the first channel-2 sequence (a new play is inserted
  first). `course-limits/p3b-right3000` (The Throne -> E ice blocks): 474 -> 3000 of 3000 ticks exact; full capture suite
  (253) unchanged. See [obstacle-collision.md](obstacle-collision.md) "Back-to-back soft collisions".

**Everything else matched.**
- Instance state (PS2 instance+8 / +0xC node type over 794 savestates vs the page after every Peak 1-3 course and station
  start, core export `world_instance_states`): the station and course-start fences and challenge reset planes die as on the
  PS2 once their section lists them; Load volumes the PS2 has hidden are ones a ride through the connector hid; Big Challenge
  gates follow the challenge status (not status-matched here).
- Streaming (`PEAK_AUTO=1`: the core's own streamer instead of the capture's rows): A -> A_ARA1 -> ARA1, D -> DRA4 and E rows
  change on the PS2's ticks; only unmeasured read times differ (11-25 ticks, the ride never waits) and the ARA1 path bank
  arrives 2-4 ticks after the PS2's AIP dispatch (0.95 cm/s).
- 20 new captures of 3000 ticks (hard left, hard right, zigzag, neutral from the PEAK1 / PEAK2 / PEAK3 / MOUNTAINF seeds and from
  the CTM world start `peak1/green-start-t0`): walls, glass fences, crashes and 12 resets (request tick, the 20-tick freeze,
  placement) are exact. The page itself replays the world-start captures position-exact for 3000 ticks (`page-replay.mjs`).
- Left open: `p2-right3000` 3312 (D station, a rail exit: the PS2 spends one tick in motion 1 at the old position, the port
  lands at once; physics / rail owners); `mt-left3000` 6152 (a reset requested in the air the tick after a soft contact: the
  port applies the contact push-out, 28 cm, before the freeze; the placement is exact).

## Peak 1 Race / Peak 1 Jam (modes 6 / 9, kinds 5 / 6)

- **Handlers:** 4 for the time challenge (init 23B268, split 23B5F8, results 23B468) and 7 for the points challenge (init 23C0D0, split 23C560, results 23C2D8). Both are solo, with no AI.
- **Tiers and table:**
  - The tier comes from the stored medal. Time challenge: gold/silver → 2, bronze → 1, else 0. Points challenge: platinum/gold/silver → 2.
  - Table 0x440D18 gives:
    - Race: 800 / 760 / 670 s (gold ≤ 670, silver ≤ 760, bronze ≤ the tier limit).
    - Jam: 130,000 / 350,000 / 700,000 points in 12:00.
    - Cash: $5,000 / $10,000 / $25,000.
  - A failed run pays nothing and repeats.
- **Start:** Happiness row 14, ABC1 grid slot 0 = AIP region kind 0, index 0 at (1883.2, -45983.4, 96369.2). There is no countdown; the clock starts on the objectives card's Continue.
- **Route:**
  - Race: ABC1 → ABC1_A → A (split) → A_ARA1 → ARA1 → ARA1_B → B (split) → B_BRA2 → BRA2, finishing at Metro-City.
  - Jam: ABC1 → ABC1_A → A → A_ASS1 → ASS1, finishing at R&B.
- **Splits:** when a 22D088 enters a station, the difference to the tier row's split k is shown for 5 s: time `int(ticks × 0.016666668) − split` as ±H:MM:SS, or points `score − split × 100`.
- **HUD** (0x1530C006 | 0x22): the clock counts down; the score shows; there is no progress meter. A Jam also shows
  "GOAL: %d" (the target) on the clock's line from the left edge (2026-09-26, peak3.md section 6). Without a career run (the `&peakMode=` QA URL) the tier-0 row supplies the clock and the splits (`main.js peakSetup`). The HUD (`ui.freeRideHud`) takes the same row as the core's `peakSetup` hook: the active career event's only when it is a peak run (mode 6..11), else the tier-0 row of the world's own mode. Before 2026-09-28 it took any active event's, so a single event left active (a QA switch from an event to `cb.peakRun`) indexed a missing row and threw a TypeError every HUD frame.
- **Start in the browser:** the grid slot placement, 11DF18's 833.333 cm/s along the placed forward (as on the PS2), then `peak_run_start_seed` (the state the pre-card frames leave).
- **Objectives card** ("68rival_pre"): "Rival Challenge / Peak 1 Race", "You've been challenged by Mac to a Peak Race!" (Griff for Mac), the route bullet, the pass bullet, and "Time to beat: 13:20".

## Browser flow

- `?course=PEAK1` starts free ride at the Green station. `&peakCourse=14` gives the Happiness plane drop. `&peakMode=6|9` gives Peak 1 Race or Jam.
- Entering the world, leaving it for a race course and the peak runs load in the page (2026-09-25, docs/course-switch.md): the URL gets the same query (pushState), the streamed world is stopped and released (`freeRide.stop`, the peak worker, locations, set pieces) and a fresh core instance loads the next one; Back returns to the previous entry's course at a menu.
- The Snow Jam package supplies the shared settings (animation, lights, sky); the Peak 1 locations replace the course world.
- **Conquer the Mountain:**
  - Character select leads into the world. A new career starts at Happiness, later careers at the last station.
  - Start opens MCOMM. Return resumes. Transport → Freeride → any Peak 1 course or station transports inside the world (no reload). Session places the rider at a session point of the current course.
  - The lodge door asks "Would you like to enter the lodge?" before the lodge opens. The lodge's Return to Game rides in again.
  - RaceRideState gates open that course's objectives card.
  - Peak 1 Race and Jam are playable in the goal lists, with their original lock rules.

## Captures and checks

- **`node web/test-peak-world.mjs`** (npm test):
  - Snow Jam's three locations streamed one by one answer 1200 height and rail probes bit-identically to the ARA1 event package.
  - The residency gate hides ARA1 and restores it.
  - The whole peak loads: 16 locations, 16,403 instances, 154 MB of core memory.
- **`node web/test-peak-run.mjs`** (npm test): tiers, limits (48000 ticks = the PS2 GMM words), medals, splits, HUD tables.
- **PS2 captures** (`local/ps2-capture/runs/`):
  - **peak1-fr-aara1:** A → A_ARA1 → ARA1, with streaming rows watched. Unload at tick 3268, eviction at 3276, ARA1 bank at 3527, Load at 3902.
  - **peak1-fr-aara1-glide** (gated in `test-ps2-captures.mjs`): the same crossing from a settled glide (baseline `local/ps2-capture/peak1/fr-aara1-glide.p2s`, the PEAK1 seed).
    - Rows: Unload at 3420 (A, DRA4_A, ABC1_A, A_ASS1, A_ABA1 2 → 7; ARA1 and ARA1_B 0 → 3), eviction at 3427, ARA1 AIP dispatched at 3664 (12A340: the bank, 112180 re-attach, +0x4CC at 3665), loader idle 3667, row 6 → 1 at 3668, Load at 4042.
    - Physics were bit-exact from 2773 through 3665. The 3666 step (PS2 0.95 cm/s faster along the motion) is the route heading +0x4CC, which 13C948 (the drive) reads as the fall line: its factor 1 − |atan(forward) − heading| / limit adds speed when the board points within the limit of the path direction. Two causes:
      - **Region row:** 112180 attaches through 26B5E0(bank, 1, rider+0x86C = 0). The runtime region kinds are the exported kinds + 1, so that is exported kind 0 index 0 (ARA1: reset path 2, race path 3; PS2 +0xAB4 = path array + 3 × 0x3C). `peak_world.inc deliver_paths` took exported kind 1 index 0 (the last region: reset path 108, race path 7), so the heading was 1.0946 instead of −2.9563.
      - **Timing:** the AIP record is dispatched before the row leaves 6 when the chunk has more to read (ARA1: 4 records before the row change in both captures; A in peak1-race-abc1a-glide: 1, with the loader idle). `web/peak-capture.mjs` now delivers a bank when the record shows the human's +0xAB8 moving while that location reads (else at the row change).
    - With both (the region fix is applied in `peak_world.inc deliver_paths`, with 26B5E0's first-row fallback), physics are bit-exact for all 1849 ticks (gated to the end) (2773..4621: the ARA1 bank, the 3760 crash, the Load at 4042, the first ARA1 patch at 4544), boost words too.
    - Bones (the root pose differs from the first tick) and the trick score/HUD slots (hud25.points) are not seeded for this baseline, so they are reported but not gated.
  - **peak1-race-start** (gated): Peak 1 Race from Continue. `--course ABC1 --event --peak-run`: the ABC1 grid slot 0 is the Happiness rival rolling start (11DE60(rider, 0, 1) → 11D660 semantic 5 clearance 0 → 11DF18 velocity = +0x1B0 × 833.333 with z = 0; control 0, motion 0), but the run's setup had already placed the rider there and run frames before the card (peak1-race-objectives.p2s: +0x770 = 220.8, a landing). `--peak-run` seeds what those frames left: +0x2E4 = 3333.33 (0x45505556, 11B3F8 in motion 1; the ready seed had 817.8), +0x380 = +0x370 (11D660 writes +0x370 only), and the route words +0x490..+0x4CC (core `peak_run_start_seed`, `reset_route_seed`). The heading differs from the rival ready state's by 3.5e-6 rad, enough to move tick 1 by 6e-5 cm/s. Physics, score and boost are bit-exact for all 1599 ticks. The bones differ from tick 1 (0.66 cm at the root); bc-race-idle shows the same difference, so it comes from the rolling-start seed. The camera is not seeded.
  - **peak1-arrive-{ass1,aba1,bra2,bhp1}** (gated): free-ride Transport arrivals, neutral pad from the screen-10 state (`--course PEAK1 --peak-arrival`). The comparison starts at the placement record (the first after control 13; the world update of that tick ran 11D390) and the browser places with core `place_rider_region(x, y, z, dx, dy, dz, transport)`:
    - 11D390: 117540, 112180(rider, 1) re-attaches the route at the grid slot (runtime kind 1 index 0, 115B08 offset 0 for player 0), then 11DE60 at the entry row (courses < 14 (1, 2), backcountry (0, 1), stations (0, 2)): 119368(score, 1), control 0, motion 0 — its entry 13C7A8 runs before 11D660, so +0x390 is the old +0x370 — then 11D660 (semantic 5, clearance 0; `reset_place_at`, shared with the reset control), 11DF18 (velocity = +0x1B0 × 833.333 under chop rounding, z = 0; animator rate 0), and the tail's +0x2E8..+0x2F4 = 0. The meter +0x2F8, amount +0x2FC and the retained +0x2E4 carry over (the comparer seeds them from the record before).
    - `transport = 1`: the Transport loop's limbo pre-state (+0x370 = +0x380 = (0, 0, 1), surface 0).
    - Physics, score and boost are bit-exact for all 223–224 ticks of the four captures, and body bones 0..23 are too. The hair (24..28) carries the pre-transport spring state, which the baseline does not hold, so it is reported but not gated. The camera is not seeded.
    - The same placement at the grid slot does not reproduce peak1-race-start by itself (the pre-card frames' +0x390 and animation/route state; tick 1 is 21 cm/s off), so the peak run keeps `--peak-run`.
  - **peak1-race-abc1a:** ABC1_A → A during the race.
  - Comparer: `node web/compare-ps2-capture.mjs RUN.bin --pad --zoe --sync-rng --course PEAK1` for mid-run captures; `--course ABC1 --event --peak-run` for the race start. `CORE_JS=dir/core.js` compares with a private core; `SEED_FIELDS=1` lists the start fields that differ from record 0.

## Set pieces and painters (2026-09-24)

- **One stage world for the peak.** The original keeps one stage context for every resident location (resource ids carry the track), so the core runs a single stage world for PEAK1: `init_stage_world` with `PEAK1/SETPIECES/` = the event packages' exports (ARA1, BRA2, BHP1, ASS1, ABA1, ABC1) merged by resource, plus what the Peak 1 stage tables give for the tracks those own-track exports skip (hubs A / B, the connectors, DRA4_A: `peak_stage_setpieces`, the tools/export_stage_world.py decoder over `peak_stage_seed.hpp`): 907 particle owners and carriers, 1041 LiveComp models (219 of them hub/connector: e.g. B_BRA2 29 starts, ARA1_B 21), 734 UV-scroll instances (43 hub), 531 MeshAnim models, 2107 script-changed instances, the collectible lists of all 8 stages (A 5, ABA1 2, ABC1 44, ARA1 30, ASS1 30, B 5, BHP1 4, BRA2 35 = table 0x43FA70), 193 flags. Nothing is seeded from a race-tick-0 savestate (no initial effects, crowd slots, magnets, halos, one-way volumes; MultiParticle group 0 starts without members): in free ride the section activation builds those entities when their section is entered, and the unload/eviction tears them down. The race GO starts are dropped (no race start in free ride).
- **Browser (`web/peak-set-pieces.js`).** Particles (builtins 16/25/26/69, MultiParticle 105/106) and halos (97) are drawn once for the whole world; LiveComp players (section / contact / timer starts from the core logs), UV scroll, flag cloth (since 2026-09-26 the world's flag manager runs in the core, `init_streamed_flags`: its grid builds and 1 s wind draw from the one visual stream in the original order, the wind mode following the current course; the JS cloth consumes the same words, docs/weather.md section 11), DeadNode / Hide / RestoreNode states, MeshAnim break pieces and magnet pickups apply to the meshes of every attached location group; the event courses' CrowdMan2d (crowd texture 9-161 frames and camera flashes, `crowd-2d.js`) runs per attached course group (R&B: 6 crowd slots registered at the arrival) (`free-ride.js` attaches a location when its draw package is built and detaches it at release). Chrome: Snow Jam 15 flags, 10 LiveComps, particles and halos live after 30 s; Metro-City 10 flags, 12 LiveComps, 11 effects / 65 sprites, 3 halos at the start.
- **Painters by region.** On a region change: Fog and ScreenTint (as before), **Lighting** (type 11: `PEAK1/<LOC>/lighting.json` → core `lighting_region`, driver state kept, uniform sections run the driver too; a missing section takes the missing-section reset → the 22E180 default bank), **Sun** (type 9, `sun-flare.js setTree`) and **glare** (type 6, `glare-pass.js setTree`). The IRR banks are loaded once (`PEAK1/lighting-banks.json` → `lighting_banks`), the default bank follows 22D088 (`browser_lighting_default_course`). Blue station: region B, payload 0 = BPBR1 with rim 0.45 (it was Snow Jam's APBR1 / 0.75 everywhere); Metro-City: BPBRR…BPBR2 by area.
- **Light glows.** `PEAK1/LIGHT_GLOW/` holds the kind-7 sources of every Peak 1 location (512: ARA1 128, ASS1 174, ABA1 72, BRA2 68, BHP1 61, ARA1_B 5, DRA4_A 4; the hubs have none); `light-glow.js setTracks` draws those of the drawn locations.
- **Sky.** The dome state reached during `start()` is replayed to the late 'sky' listener, so a start on the B side (Metro-City, The Junction, Blue station) shows BSKY (it showed ASKY before).

## Audio

Implemented in `web/game-audio.js` / `audio-world.js` (details and addresses: docs/audio-logic.md 9.9): the free-ride `runStart` (station: the Peak1 song, event 12 of 28CF98, then DJ timer kind 2; course: a playlist song and DJ kind 4 with Free_Ride_Intro on a first visit), the DJ timer 28E548 kinds 0-5 and 7 with the 2A43B8 queue order, the per-location MusicTrigger zones 11/13/18 (on the PS2 the station song and hub chatter come from these zones, not from the crossing: `freeRideCourse` only updates the course), pktrans (plane intro, `travel`), Peak1Amb in radio mode 2, the world sounds of the resident locations (`AUDIO/world/PEAK1.json`), and `travel(dest)` / `arrived()` (27A860 -> 28E8C0(20)) around the in-world transport.

## Cutscene hooks

`web/cutscenes.js playCutscene({kind, id, rider, location, until})` is awaited at:

| Call site | kind (id) | Original |
|---|---|---|
| `free-ride.js` station trigger (builtin 68 action 4), before the lodge prompt | `lodge-walkin` (0x16) | state 14 arg 0: 278E50 cinematics 0x16 + 0x17 |
| `free-ride.js` station trigger (action 3), before the Map | `transport-booth` (0xB) | state 14 arg 2: cinematic 0xB, 11D390 park |
| `free-ride.js transport`, before 22CEA8 | `transport-depart` (0xB) | state 14 arg 1 after the overlay: 278DE8 / 27A860 / 279148 |
| `free-ride.js transport`, while the rows stream (`until` resolves when placed) | `transport-loop` | screen 11 with the transport flag (236960) |
| `free-ride.js transport`, after a station arrival | `station-arrival` | ctm/60-green-arrive, 61-cut |

Not called yet (the cutscene agent can hook them where the flow starts): `plane-drop` (the first CTM start, state 10 cinematic 0x1D) and `location-intro` (state 10 arrival intros 0x1E/0x1F/0x10/0x11; none on the PS2 runs so far).

Implemented by the cutscene agent (docs/cutscenes.md, 2026-09-24): `lodge-walkin` / `station-arrival` play `lodge_arr3`, `transport-booth` `hub_trans_arr`, `transport-depart` `heli_dep`/`gond_dep` of the station being left (none from a course), `transport-loop` `heli_inair`/`gond_inair` then the rider's held in-air loop (heli when either end is a backcountry), released by `until`. The new-career plane drop is started by `career-ui.js enterWorld` (`kind:'arrival'`, ABC1 first visit). main.js no longer plays its own copies of these cuts.

## Big Challenges

The original WScript mission system, driven by the location stages' own mission records and programs (40 on Peak 1).
Core: `web/mission_gameplay.inc` (included at the end of `web/stage_script_gameplay.inc`); browser: `web/big-challenges.js`;
data: `tools/export_peak_missions.py` → `web/generated/peak_mission_seed.hpp` + `web/public/assets/BIGCHAL/big-challenges.json`.

- **Data.** Stage +0x20/+0x24: 0x54-byte mission records (+0 challenge id, +4/+8 steps, +0xC start / +0x10 tick / +0x14 stop
  programs, 10 mission event programs, +0x40..+0x4C four callbacks: +0x44 complete, +0x48 fail, +0x4C status/visual, +0x50 misc
  2); +0x30/+0x34: 0x38-byte steps (tasks: start, tick, end, event kinds 0..9). Program words are `program << 8 | track`. Table
  0x43EE10 (88 × 0x24): id, title / objective / description LOC hashes, course, cash, flags (+0x18 s16 = peak), next, +0x22 music type.
- **Objects.** WScriptMission (vt 0x489A30 over WScriptProcess 0x489B70), tasks (vt 0x489AE0). 308C60 builds one per record of
  every loaded stage at the world start and at each world state-10 exit (two ticks after a Load trigger, builtin 68 action 2);
  308FE0 drops a stage's at its unload start; a stage loaded later (Transport arrival) gets its own. Lists C+0x2B4 inactive /
  +0x2B8 active (push front, next +0x18). Each run gets a fresh 8-bucket LUN table (m+0x1C) as the programs' globals.
- **WScript tick 309270** (entity pass, after the rider pass of the previous tick): pending op (309118), HUD words C+4/+0x10/+0x1C/
  +0x28/+0x34 zeroed, queued events (C+0x40: kind 0 = 30A060 contact, dispatched to the mission's event programs and the current
  task's), then each active mission: state 1→2, current task tick, mission tick program.
- **Builtins.** 40 (3012F0 → 30A868 ops 0..19: 0/1/2/18 stop/start/next task/quiet stop, 3 restart, 4 success, 5 fail, 8/9/11
  post to the offer queue C+0x1C4, 12..16 status bits (16 = available and nothing running), 19 last of chain), 47 next task,
  50/79 rider stat 122EE8 (key through table 0x4897B0), 64 rider to the instance (1234D0: placement 11D660 semantic 5, then the
  start-grid control 6), 65 rider in volume (rider +0x5B8), 75 "Count", 76 timer, 98 "Goal", 81/86 height, 107 gate order
  (30BFC0: in the gate → 1; beyond 1.1 / 1.5 × the gate spacing → -1 missed). Not ported: 63 (rail id), 78, 83 (called tricks),
  110, event kinds 1..5 (tricks, crashes: no poster wired).
- **Status** per challenge (character block +0x118 + 4·index): bit0 locked follow-on, bit1 new, bit2 failed, bit3 completed,
  bit4 available; new character (151600) = flags bytes +0x1A → bit0, +0x1B → bit4, bit1 set. Complete (307308): bit3, cash
  1511B0(peak) = $2,000 / 4,000 / 6,000 (not the table's +0x14), 10F2D8 popup "MISSION SUCCESS" + 119EF8 kind 4; with a next
  challenge: own bit4 off, next bit0 off / bit1 / bit4 on. Fail (3074C0): bit2. Career save: `ssx3.career.v1` rider
  `bigChallenges` (88 words), cash to the rider; INFO panels "Big Challenges n / N" (1542E0 / 1542A0, N/A without).
- **Flow.** Offer volume contact → slot-2 program → op 8 → the next tick's queue read opens overlay 0x1D (Big Challenge / title /
  description / Accept challenge? Yes / No, sound 29CED8 kind 4): Yes → C+0x2A0, pending op 1 → start (rider to the challenge's
  bcteleport, timer, HUD), No → op 3. Tick program failures (op 9) open overlay 0x1E (Challenge failed / Retry? Yes / No /
  Challenge Info, cursor on Info). Start during a challenge: overlay 2 (Return, Restart Challenge, Messages, Audio, Options,
  Quit Challenge). HUD (1EB350): OV_1 'hud ' badge with the challenge number, the challenge clock `HH:MM:SS.hh` (red at 0),
  "COUNT: n / N", "GOAL: n", "HEIGHT: n.nnm"; the collectible counter is hidden. Music: 29D6E0 event 33/34/38 by type,
  29D8E0 / 29DBB0 event 39 (`web/game-audio.js` challengeStart/End/Stop).
- **PS2 checks** (`node web/test-big-challenges.mjs`, npm test; captures `local/ps2-capture/runs/bigchal/`, states
  `local/ps2-capture/peak1/bc-sd-*`, frames `local/ps2-capture/menus/bigchal/`): Speed Demon (Snow Jam #21, 12 gates, 60 s):
  the arrival ride 2836..3609 (no offer, then the green volume's contact queues it); the run 3631..4278 — context words, C+0x2A0,
  mission state and task identical on all 648 ticks (count 1/12, timer 3599..2952), the fail at 4279 (gate 2 missed); the success
  (poked last-gate state) 141 ticks identical, completion at 7376 with status 0x12 → 0x1A and +$2,000 as on the PS2. The gate
  contacts come from the PS2 task transitions (the recorded rider +0x5B8 is cleared before the record); timers, counts, the
  missed-gate distance test and every fail condition come from the core. Browser (Chrome, `?course=PEAK1&peakCourse=0&qa=1`):
  prompt, run HUD, teleport to the start, pause and fail overlays against the PS2 frames.

## Gaps

- **Placement:** the arrivals are exact (gates); the Session (state 15: 11DE60 only) and the lodge "No" ride-in use the same placement without a capture of their own; `reset_rider` reloads the glide seed's boost meter (0.005) where the PS2 carries the rider's own; the peak-run start still needs `peak_run_start_seed` (frames before the card).
- **Set pieces:** the MultiSpline chairlifts / cars, ParentModifier children and the spline pieces are not run in PEAK1; MultiParticle groups of different courses share slot 0 (ARA1's roadflare is kept; the original rebuilds the group per location load). The particle / halo pipelines of the streamed world are not in the loading-screen warm-up.
- **Painters:** per-location local lights (rider lighting `local-lights.json` / `light-tree.json`) are still Snow Jam's; the initial environment ambient/ratio is Snow Jam's.
- **Session / transport:** no SessionViewPoints map, no white fade (2E4CE8); a transport to the current station uses Session point 1 (the lodge Return to Game on the PS2 reloads the world and rides in from point 0: ~1 m apart). The loading loop and the cuts are the cutscene agent's (hooks above).
- **Audio:** first visits (audio+0x579C) are tracked per page load (the career P+0xACC visited mask is not passed); after the plane intro the discarded world-load song's artist intro plays after the Text_Message (on the PS2 it plays during the plane video).
- **Big Challenges:** the trick event (kind 3, 118FF8 -> 309918) and builtins 63 / 78 / 83 are ported (2026-09-26, docs/ctm-parity.md); event kind 5 is posted at the reset placement (12F398 -> 11DF18(rider, 1)); kinds 1 / 2 / 4 have no authored use; the 1234D0 teleport's camera cut, 26CDF8/26CC48 restart point, 1235F8 decline push and the in-world fail order (the browser pauses one rider pass later) are approximations; only Speed Demon is verified against the PS2 (offer, run, fail, success); overlays are drawn to match PS2 frames, not from the LUI layouts.
