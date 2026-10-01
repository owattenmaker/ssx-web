# Conquer the Mountain events in the world: no load screens in or out (design, 2026-09-29)

Design only. No game code was changed for it. It covers ranks 2, 3 and 10 of [ctm-decomp-world-states.md](ctm-decomp-world-states.md), plus
the free-ride implementer's open item "the world under the world-load arrival NIS" ([ctm-parity.md](ctm-parity.md), "Open from this
round"). It builds on the pause-context stack of [pause-contexts.md](pause-contexts.md) (pv `pauseContexts`), not on the old `paused`
flag.

SLUS_207.72, gp = 0x4A30F0. The names are those of ctm-decomp-world-states.md:
- G = `[0x4A28A8]`;
- S = G+0x84, the game module;
- C = S+0xC, the rider manager;
- GMM = G+0xC0;
- the event type is 0x535C10.

The traces below were made with `local/ctm-decomp/d` / `calls.py`. The scratch notes are in `local/ctm-events/`: `notes-ps2.md` holds the
traces, and `package-diff.sh` lists which files an event package has that the streamed location package lacks. "Unconfirmed" means the
code read so far does not settle the point.

## Summary and recommendation

**The PS2 never loads anything for a CTM event.** The event runs in the streamed world that is already resident. The riders load one
step a tick under the fly-over. The event is set up in place:
- at the card's Continue (WS2 exit), the rider manager's restart (C vt+0xCC+0x28(3) → 129768) empties the section activation list,
  so it rescans under the event kind, and zeroes the race clock;
- 230180, the world reset, runs at Next heat, Restart and the return to free ride. The first heat has none: it starts from the
  free-ride world as it stands.
- (2300F0, first read as a reset before every countdown, runs only online: WS3 enter tests [0x534B30] first. §6.8.)

**The port builds a second world for the event.** Each event is its own package and core instance, so both the entry and the exit are
world loads.

**Recommendation: option C, a hybrid.**
- The CTM event runs inside the streamed-world core, which is what the PS2 does. This is option A's simulation and world.
- The event package supplies only its small per-event data: the riders, lineups, race paths and start seeds, the progress meter, the
  freestyle rules and the camera triggers.
- Single Event keeps its event packages unchanged. On the PS2 a Single Event also loads the course's own residency row as its world
  (WS10), and the event package is that row merged.

Why C:
- The existing event capture gates never see the new path. They are all Single Event states.
- There is only ever one world in memory, so phones are no worse off than today's event load, and are probably better off.
- WebKit gets no second core and no second compile of the same geometry.
- The return to free ride (#3) becomes a placement and a fade, not a load. This is how the PS2 does it.

**The cost** is the core work to run computer riders in a streamed world, and an in-place kind switch with the WS3 reset. That work
is staged below. Each stage sits behind one default-off switch and has its own parity checks.

Option B (a second core warmed in the background) is rejected:
- It doubles the world: +250 MB on the phone class, where the iOS tab limit is about 1.0 to 1.5 GB.
- It still shows black on phones and in WebKit. The event load there is 7.6 s on desktop WebKit and 30 to 60 s on a 4x phone, against
  about 8 s of fly-over and approach.
- It needs the `loadCourse` global-core refactor that [course-switch.md](course-switch.md) flags as the source of the WebKit crashes.
- It is less 1:1. The first heat would start from the Single Event seeds instead of the free-ride world, the riders would be missing
  from the fly-over, and the free-ride world would not tick.

**Stage 0 evidence (§6) backs C.**
- A CTM countdown's world equals a Single Event countdown's: the same 2203 octree nodes, every patch list in the same order, and the
  same instance sets.
- The CTM human, however, carries its free-ride history into the start: about 170 non-pointer words of the rider differ.
- Replayed on today's event package, the CTM first-heat capture is exact through the 180-tick countdown. It diverges at tick 182,
  the push-off (2.8 cm, 161 cm/s), so the event package cannot be exact for CTM starts.
- The first heat also keeps two of free ride's DeadNode fences and the hidden gate volume until the next 230180. A Single Event seed
  does not have these.

## 1. The PS2's behaviour

### 1.1 Entry: the gate to GO, all in one world

| step | PS2 (address) | what it means for the port |
|---|---|---|
| gate | builtin 67 → 22D6C8. Conditions: CTM (0x535BC8+0x49 == 0), free ride (+0x48 == 4), course `[S+0x78]+0x1BC` not 14..16, key1 != 1, WS == 4 after cBE_setState(0). Then 12B180 (score commit), C+0xC = 0, the kind table 0x47B450 → 144DF0 / 1451E8 (type / mode), 238160 initGameMode (the handler swap), 231250(S, 1, 1, 0). **No 230180, no 2300F0, no streamer call, no world state 10.** | The event starts with the free-ride world state as it stands: set pieces, section activation, entities and RNG. |
| WS1 enter 2341D0 | 113B10(C, 3) (PreRace), 1F36D8(S+0x94, 0), cBE_setState(0), 26F228 (replay), setNumberAI(GMM+0x10 − GMM+0x14), per AI setRiderCharID (GMM+0x18..) + 1473D0 (GMM+0x40..). Arg 1: 2790A0 / 27AAF8(nis, 0) (the list [2 fly-over, 3 approach, 5 idle]) / 278F38. | No pause context is pushed. The world, including the human rider, keeps ticking under the fly-over; PreRace's per-tick handler is empty and the rider pipeline 128AF0 runs. |
| WS1 update 2343B0 | Phase 1: 128958 = 12AF68 (purge mission riders when +0x88 > 0), then 129E20 (cAI_initComputerRiders). Phase 2: 12A180 (all loaded?), else **1296F8: the first rider whose 11D640 is 0 gets one 11C298 step a tick** (asset allocations and 113138). "Loading..." bit 2 while phase < 3, and a list step that ends is held (279040) until the riders are in. Phase 3: grid placement 1289F0 = 1297C8(C, 0) (10F398, C+8 = 0, every rider 11D390). | The riders appear in the world during the fly-over. The PS2 shows them tiny at the gate ([ctm-flow.md](ctm-flow.md) §7). The cost is spread one step per tick. |
| WS1 exit 234750 | 2790A0(nis, 1); 128958, 1289F0, 128A10 (1297C8(C, 0) + 128A48), 26F7B8, riders vt(3), 128A48. | |
| WS2 236BB0 | The card; pause context 1 (sim and NIS stopped, views and fade running). | Already ported (pv cardFreeze / pauseContexts). |
| **WS3 enter 234AD0** | 258968, cBE_setState, **2300F0 only when [0x534B30] (the online flag) is set** (0x234B14..0x234B24), 26F228 / 26F7B8 (the replay snapshot), types 4..6 → WS4 at once, 113B10(C, 4), 29C418 / 29C420, "StartlightBegin" (234BE8), 39F840. | Offline (CTM and Single Event) there is no world reset at WS3. The activation rescan under the event kind comes at WS2's exit (129768), §6.8. |
| **2300F0 (online only)** | 26CA90(S+0x34); 309030 (missions destroyed); 354C98 / 355118 (purge set-piece buckets 1 and 8); **103358(C+0xA4)**, which empties the section activation list (each listed instance's flags &= ~0x100, count +0xD8 = 0, +0xD0 = −1, so the next 0x101B60 pass rescans **under the new event kind**); 2D9B40 (avalanche / visual fx); 3A6800 cWorld_resetMap(S+0x10) (walks the loaded sections' 0x58-byte records); 309F18 (the global stage programs); 308C60 (missions rebuilt). | The event's fences, reset planes and startmode colliders are rebuilt by their slot-1 programs with builtin 43 seeing the event kind. That is how free ride's kind-4 fences go away at an event. |

So at the countdown both worlds have been rescanned under the event kind (WS2 exit). A Single Event world is freshly loaded; a CTM first heat is free ride's world as it stands; later CTM heats come after 230180. They can differ in five ways
(unconfirmed which of them matter; §5 R1 settles it):
1. the resident location set and its octree insertion order (streamed order against merged-load order; the walk order matters, see
   [peak3.md](peak3.md) §6 "Past the crash contacts" 1 and 7);
2. the RNG words 0x4FF030 / 0x4FF018;
3. the human's carried state (boost meter +0x2F8 and others that 11D390 keeps);
4. anything no reset touched (in a first heat: everything free ride left, e.g. the DeadNodes of set-piece bucket 8, §6.4);
5. the AI riders' creation history.

### 1.2 Between heats and back to free ride

| transition | PS2 | notes |
|---|---|---|
| Next heat / results Restart, WS13 235AA0 | cBE_setState, 30B7F8 (Big Challenge end), 2382D8 restartHeat, **230180**, the round's roster (setNumberAI / setRiderCharID / 1473D0). Then the gondola (races), the lists, 128958 / 1296F8 / 1289F0, then WS1 arg 3 → WS2 → WS3 (2300F0). No pause context. | #10: the world ticks under the gondola; the riders are in PreRace. |
| **230180** (world reset) | 26CA90, 309030, buckets 1 / 8, C vt+0xCC+0x28(3), 2D9B40, 22E7C8(S+0x84), 3A6800, 26F228, 2789E0(nis), 15DB58, 229498(S+0x38), 343BC0(S+0x3C), 357B38(S+0x40), HUD "hud" message 7, 309F18, 308C60, 28B180 → 287108. **No 103358**: the activation list is kept, and WS3's 2300F0 clears it later. | Users: WS13, WS14 to the current course, WS15, cGame_restart 2302A8. |
| **Return: WS15 236058** (results Transport → map → the same location, and Session) | 30B7F8; 238160(GMM, 12) (free ride); 230180; 11DE60(human, IF(0xA)+0x10, 2) + 11DF18; 230698; the white fade 2E4CE8 / 2E4D70 / 2E4370(view, 1, 0, 0, in 1.0). Update 236198: WS1 arg 0 → WS2 (no card in free ride: command 2 at once) → WS3 (2300F0 again: the rescan under kind 4 restores the free-ride fences) → WS4, on consecutive ticks. | The computer riders' removal is unconfirmed: in free ride WS1 phase 1 takes 12AB20 / 12AC48, which are virtual calls not traced. |
| Transport to another location | WS14 → WS11 held loop → WS10 | Already in-world ([ctm-parity.md](ctm-parity.md) "The Transport's presentation"). |
| Backcountry events (ABC1 / DBC2 / EBC3 rival races) | Not a gate (22D6C8 rejects courses 14..16). The Transport → WS10 arrival → **WS1 arg 1 at a backcountry** (WS10 enter). | These run in the world too. |

### 1.3 What is resident at the gate

- The gate volume is at the course's start. The rider reached it through the connector's Unload trigger, which requested the course's
  residency row (for example Snow Jam: ARA1, A_ARA1, ARA1_B, plus ASKY and TRANSP; table 0x442488).
- That set equals the event package: [peak-mountain.md](peak-mountain.md) "the event packages of web/prepare.py are these rows merged".
  `test-peak-world` answers 1200 height and rail probes bit-identically between the two.
- The path bank 0x4D33A0 is the course location's own, variant 0 (12A340). The computer riders race on it.
- **Unconfirmed:** whether a connector Unload / Load volume can fire during an event (builtin 68's handler at 0x302410 was not read for
  a kind gate), and so whether rows can change mid-event.

## 2. The port today

### 2.1 Entry (pv `mountainRide` on the desktop tier; the per-peak worlds on phones)

1. **The gate.** `free-ride.js drainEvents` (core event kind 3) → `main.js freeRide.on('gate')` → `cu.rideIn` → `ctm-event.js
   rideIntoEvent`.
2. **`cs.preFade(30, loading)`** plays over the running world. Then `pause()`: `contexts.push(CTX.HOLD, 'WS1 ride-in')` with pv
   `pauseContexts`, otherwise `paused = true`. **The world stops here. The PS2's does not.**
3. **The fly-over** plays in the streamed world with the human only (`cutsceneCast` leaves out the riders when `course.freeRide`).
4. **The switch.** `drawCover`, `gameAudio.carryWorld(true)`, `save.pending.rideIn`, then `ui.cb.course(entry)`, which is
   `selectCourse` → `navigateCourse` → `switchCourse`:
   - `stopRun` and `unloadCourse`: `freeRide.stop()` releases the whole streamed world;
   - `loadCourse`: a new core (`newCore`), the event world (collision.bin, eventSlices, set pieces, sections, stage world, crowd, sky,
     environment, fog, weather, lighting, rails);
   - `createAiRace`: five rider contexts built from parse caches.
5. **The event.** `afterSwitch` → `careerUI.resume()` → `begin(..., {rideIn})` → `ui.loadEvent` (warm-up: lineup, `aiRace.prepare`,
   pipelines) → `cb.intro` 'career-ridein' [approach, idle] → the card → `startRun` (`_start_event`, `aiRace.start`).

The black between the fly-over and the approach is the whole event load:
- desktop Chrome: 5.6 s;
- desktop WebKit: 7.6 s;
- Chrome 4x phone: 50 to 60 s;
- throttled Metro-City: 18 to 24 s.

([presentation.md](presentation.md) §3, [course-switch.md](course-switch.md).)

### 2.2 Exit

- Results Transport → `transportAfterEvent` (#152 where present) → the map.
- The same location: `goWorld` → cover → `cb.freeRide(course)` → `selectFreeRide` → `switchCourse` to MOUNTAIN / PEAKn (the whole
  streamed world loaded again, `worldWarm`) → `resume` → `enterWorld` → `fadeFrom(58)`.
- Another location: `rideAcrossSwitch`, the transport ride with the switch under its held loop.

### 2.3 What the streamed core has and lacks for an event

It already has (peak runs, modes 6..11, run in PEAK1..3 / MOUNTAIN solo):
- `peak_world_event_kind` / `peak_world_game_mode`: builtin 43 reads `browserEventKind` live, and the path-bank variant follows the
  kind;
- the finish rule (`update_finish_rule`, 10E5D8);
- the race session from `init_race` with paths swapped per location (`deliver_paths`), plus `race_clock_restart`,
  `browser_game_tick_restart`, `peak_run_start_seed`;
- the time limit, splits, results and HUD through the ordinary game-tick path;
- `reset_race` → `browser_reset_pickups` → `section_reset` + `stage_world_reset` + `stage_vm_reset`, which rebuilds sections so that
  slot-1 programs rerun with the current kind.

It lacks, per the core and page reads for this doc:

| missing piece | where it lives today | why it blocks |
|---|---|---|
| The event start seed | `browser_select_event_course` (generated/event_instance_seed.hpp) runs only in a plain (non-append) `init_world_collision`. The streamed world takes the fallback (`browserEventCourseSeeded = false`). `begin_event_rider` → `browserEventGroundState()` throws "No event start seed" outside the 17 event courses. | No countdown, grid control 6 or rolling start in a streamed world. |
| The countdown session | `begin_event_clock` (race_bridge.cpp) uses `browserEventParticipant()` / `browserEventProgressOrigin`, which are event-course seeds. `course.eventStart === false` skips `_start_event`. | |
| The event course's race event | `init_race(course.initial)`: a streamed world gets the **base** course's initial.json (PEAK1 → Snow Jam's). BRA2 has its own initial.json, which differs in `original_race_event` (participants, paths, clock), `original_reset`, the ground and animation seeds. | The race participants and paths for a Metro-City event are wrong. |
| Computer riders | `createAiRace` is only in `loadCourse`'s non-free-ride branch. Per context, `attachWorld` = `init_race`, `init_world_collision[_cached]`, `init_body_terrain[_cached]`, `init_rails[_cached]`. | See the next four rows. |
| - collision in a rider context | The parse caches cover only plain loads: "Streamed (append) loads never use it" (world_bridge.cpp:66, `cacheable = !(browserWorldAppend && browserBodies)`); body terrain and appended rails are not cached. `browserBodies` and the rails are RIDER_LOCAL; `world` / `cameraTerrain` and the residency / octree flags are shared. | Each context has to be fed every resident location in append mode. |
| - streaming in a rider context | `peak_stream` (rows, path banks, eventKind) is RIDER_LOCAL. `deliver_paths` runs only from its own context's `pass()` and hard-codes region kind 0 / index 0 "player 0". `peak_world_commit` / `peak_world_free_track` touch the shared `cameraTerrain`, and the free is documented for one rider context. | The riders need the course's bank without running a streamer of their own. |
| - section node states | Section slot-1 programs run only in the human's context (`section_scan` checks `browserHumanRider`). `world_event_apply` (shared_world.inc) replays kinds 1..8 only, with no section DeadNode / RestoreNode. On event courses the riders get their DeadNodes from the seed (`apply_snow_jam_dead_nodes`). | The riders' world copies would keep free ride's fences. This is a reading of the code, not a run. |
| - context lifetime | rider_context.cpp exports create / current / enter / bytes, with **no destroy**. Today a context dies with its core at the next course switch. Heap per rider is about 12-15 MB, and wasm memory never shrinks. | In-world events must reuse the contexts. |
| The GO LiveComps | tools/export_peak_world.py:577 filters `trigger != 'go'` ("does not run in free ride"). On an event course, set-pieces-renderer.js:259 fires `live.fire('go')` at `go_tick` (ARA1: 12 starts). StartlightBegin / StartgateOpen (234BE8 → 309DD0 / 309E50) have no core hook in either path. | No start-gate doors. |
| The kind switch in place | `event_kind` / `peak_world_event_kind` set the kind but rerun nothing. No core function does 22D6C8's swap, or 2300F0 / 230180 as such. | |
| Event data files | Event only (`local/ctm-events/package-diff.sh`): npc-riders.json, lineups.json, rivals.json, lineup-sessions.json, progress-meter.json, freestyle-event.json, camera-triggers.json (replay), start.json, route.json, initial.json (BRA2). The world-scope ones (CROWD, PARTICLES, SECTIONS, STAGE, FLAGS, LIVECOMP, UVSCROLL, light-glow, sky, sun-flare, glare, local-lights / light-tree, environment) exist at world level in the streamed world. | Small; fetchable under the fly-over. |
| Visual gaps that become visible in events | The streamed world's rider local lights (local-lights.json / light-tree.json) are still Snow Jam's everywhere (peak-mountain.md "Gaps"). The event package has each course's own. | An event at Metro-City would light the rider with Snow Jam's local lights. |

The page side is ready for most of it:
- the game tick (`game-tick.js`) has nothing that stops an `aiRace` and a `freeRide` ticking together;
- the pause contexts (`ws1Hold`, `ws10Hold`, `ws11Hold`, `overlay`) and `screen-phases.js` exist behind pv `pauseContexts`. `phases.advance` / `step` is not called yet (the pause-context agent's open item).

## 3. Options

| | A: the event in the streamed core | B: the event core warmed in the background | C (recommended): A for the world and the simulation, event data from the package, Single Event unchanged |
|---|---|---|---|
| Entry black | none: the riders load in slices under the fly-over | hidden only if the load fits under fly-over + approach (~8 s): desktop Chrome yes, desktop WebKit marginal (7.6 s), phones no (30-60 s) | none |
| Return (#3) | a placement and a fade (WS15 in place) | the free-ride world must be warmed again during the results, or kept alive through the event | as A |
| Existing event gates (≈250 captures, all Single Event states) | untouched if Single Event keeps its packages | untouched | untouched: they run the event-package path, which does not change |
| Streamed-world gates (peak1/2/3, allpeak, ctm/fr-dra4a-full, course-limits) | must stay exact; every change is inert until the gate fires | untouched | as A |
| New gates needed | CTM in-world event captures (§4 stage 0) | CTM captures to show a Single Event start equals a CTM one | as A |
| Phone memory | one world; plus 5 rider contexts ≈ 60-75 MB of heap, as today's event; the streamed world's other rows are released (45 s, or at once with pv peakRelease) | two worlds through the fly-over: +250 MB, phone peak about 1.1-1.2 GB (presentation.md §3) | as A |
| WebKit | no second core; only the riders' models compile under the fly-over (spread, yield-shim) | a second core, a second compile of the same geometry, and the global-core refactor (the switchGate crash class) | as A |
| Amount of change | large in the core (rider contexts in a streamed world, the kind switch, the resets), medium on the page | large on the page (prepareCourse / commitCourse, `loadCourse` without globals), none in the core | as A, minus Single Event |
| 1:1 | yes: the PS2's world, reset, rider loading and ticking | no: a fresh Single-Event world, no riders in the fly-over, the world frozen | yes |

C is A stated precisely: Single Event keeps the event packages, which is PS2-faithful (its WS10 loads the row), and the in-world event
reads only per-event data from the package folder. If stage 0 shows that a CTM start equals a Single Event start bit for bit, B's
physics would also be exact. B would still lose on memory, WebKit, the return and the ticking world.

## 4. Staged build plan

Every stage is shippable with its switch off (the default), and the switch is added to `PV_DEFAULTS` before any code reads it. The
event-package path stays the fallback until the last switch is on.

For every stage:
- Core work is built with `CORE_OUT=` to a scratch path, and the live core is rebuilt only after the targeted gates pass.
- Browser checks run in Chrome (`--mute-audio`, `?mute=1`) and in WebKit through `web/webkit-driver.mjs`.
- PS2 runs use silent ARMSX2 on derived states only.

### Stage 0: evidence (no switch, no game code)

**Done on 2026-09-29.** The results are in §6. The plan as written is kept below; §6.1 lists what was actually run.

**PS2 captures** (`tools/ps2_capture.py --ai-state` / `tools/ctm_flow_capture.py`, derived states):
- **c0a, the Snow Jam first heat from the gate.** Session point 1 of ARA1 in free ride, the ps2b/intro pad (286 neutral, 20 left), then
  a race script from the card's Cross. Record every tick from the gate to GO + 1200. Watch:
  - WS S+0x214 and 0x535C10;
  - the activation list C+0xA4 (+0xD0, +0xD8, the list at +0xDC);
  - the rider count C+0x78 and each rider's 11D640 flag;
  - the RNGs 0x4FF030 / 0x4FF018;
  - the set-piece bucket counts;
  - the human +0x2F8 and +0xAC4.

  This answers R1 to R4.
- **c0b, the same for Metro-City** (the 7-tick hold) and a freestyle heat (ASS1 slope style, one AI).
- **c0c, Next heat into the semi's countdown** (WS13's 230180, the gondola ticking).
- **c0d, results Transport → the same location → WS15 → WS4 + 600 ticks**, and the MCOMM Session.

**Memory diffs:**
- The CTM countdown `local/reference/pcsx2/characters/career/ARA1-final-zoe/countdown.p2s` against the Single Event Snow Jam countdown
  state of the event-start gates: the human +0x77C..+0xDE8, the AI records, the activation list, the course's octree cell lists (node
  +0x20), the RNG words.
- The same at c0a's WS3 enter (the first heat, which had no 230180).

**Port experiments** (scratch, no ship):
- the CTM countdown state through the existing comparer on the event package (`--course ARA1 --event`);
- a script (`local/ctm-events/paths-eq.mjs`, to write) checking that each event package's initial.json race and reset paths equal its
  location's paths.json variant-0 rows (17 courses).

**Output:** a table in this doc, "CTM start = Single Event start?" per field. If the octree order or the RNG differs, only the streamed
core can be exact, which is a further reason for C.

### Stage 1: pv `worldUnderCuts`, the world ticks under WS1, WS13, WS14 and the arrival NIS (#10, open item 3)

**PS2 rules:**
- WS1, WS13 and WS14 push no context. The riders are in PreRace (113B10(C, 3), an empty per-tick handler; 128AF0 runs).
- The world stops only for a context (card 1, menus 2, prompts 3) or for an NIS disc read (270280).
- WS10's arrival NIS ticks the world (plane #153 / #163: ticks 17 → 917), except during the movies.

**Build** (on pv `pauseContexts`):
- `rideIntoEvent`'s `pause()` stops pushing `CTX.HOLD`. As on the PS2 (stage 0, §6.2), the human rides on in its own control for the
  gate's 30-tick fade (the gate tick + 31), then the fly-over's rider actor holds it: control 13, +0xAC4 = 1, through the approach and
  the idle, until the grid placement at WS1's last tick. The port's equivalent is `nis_hold` from the fly-over's first tick.
- The gondola (WS13), the transport-arrive (#152) and the heat lists keep the game tick running, with the human held through
  `nis_hold` (the Transport's existing pv `nisTick` path).
- The arrival NIS (plane, heli drop, first-visit movie): start the run before the list with the human held in the transport limbo
  pre-state (`place_rider_region(..., transport = 1)`). Place it at the list's end, as pv transportFade already does for the heli drop.
  Pause only during the movie steps (`ws10Hold` only for `kind === 'movie'`).
- **Recommendation for the open item: no new core "world only" tick.** The PS2's "world only" is the normal game tick with the
  human held, and the port already has that hold. Stage 0 confirms it for WS1 (the gate) and WS13 (the gondola: 201 world ticks with
  +0xAC4 = 1).
  - Unconfirmed: whether the human exists and ticks during #153 in WS10. Settle it with `ctm-parity/runs/new-career`, by rider count
    C+0x78 and the human's +0xAC4 per record.

**Gates:**
- `npm test -- ps2-captures peak1 peak2 peak3 allpeak ctm course-limits` (the free-ride worlds must be unchanged: nothing ticks
  differently before the first cut);
- test-cutscenes, test-ctm-flow, test-world-warm, test-ride-warm.

**Parity:**
- the page replay of `ctm-parity/runs/new-career`: the game tick counts 17 → 917 across #153 / #163, and the RNG words;
- `ctm-decomp/crossing/to-c-fade` (the world ticks through the held loop);
- `to-final` (the gondola ticks, released at t341).

**Browser:** Chrome and WebKit. Flags, particles and set pieces move under the fly-over and the gondola; the frame times do not regress
(`?mute=1`).

This stage is independent of the rest and fixes #10 and the open item on its own.

**Built (2026-09-29), pv `worldUnderCuts`, off:**

What the switch changes:
- **WS1, the event gate's fly-over** (main.js gate listener): no `WS1 ride-in` HOLD. The rider rides the 30-tick fade in its own
  control, then `nisCutHold` holds it; the fly-over's rider actor (`onHumanActor`) moves the hold to the actor's root.
- **WS10 arrivals** (career-ui.js enterWorld `arrival`):
  - the ride starts before the list, with the audio ride start held (`arrivalCinematic`);
  - cb.cutscene pushes no `WS10 cutscene hold` while the run is up;
  - the rider is held from the list start (a pending hold is applied once a tick has posed it: main.js `nisWatch`);
  - `cb.arrivalPlace` releases it and places it as the ride start did, the game tick restarting (PS2 tick 0 at WS2 after the list).
- **Movie steps:** cutscenes.js `onMovie` → main.js pushes a `WS10 movie` HOLD. A first visit pushes it from the list's start, because
  WS10 plays the movie before anything ticks (PS2 new-career s631..2334).

Browser checks (`local/ctm-events/qa/arrival.mjs` and `ridein.mjs` through local/ctm-decomp/qa/drive.mjs / drivewk.mjs, `?mute=1`):

| check | switch off | switch on, Chrome | switch on, WebKit |
|---|---|---|---|
| new career: tick through the ABC1 movie | 0 | 0 (`WS10 movie` held) | 0 |
| new career: tick through #153 | 0 (HOLD) | 2 → 531 | 4 → 495 |
| new career: after the list | the ride | placed at the drop, tick restarted, riding | the same |
| Snow Jam gate: tick through fly-over #94 | frozen (`WS1 ride-in`) | 3631 → 3890 | 1861 → 2118 |
| the rider under #94 | its gate position | (−128427, 14434, −228771), the PS2's position under the hold to the cm (caps/c0a-gate record 636) | the same |
| contexts left open | none | none | none |

Checks run and not run:
- Node tests: test-pause-contexts, test-cutscenes, test-ctm-flow, test-presentation and test-world-warm pass with the switch off.
- test-ride-warm timed out twice waiting for the ride (machine load average 35-38). The same flow reached the ride by hand after about
  100 s with the frame clock frozen. With the switch off the code path is unchanged.
- The capture gates could not be run meanwhile: the physics agent's comparer change needs its new core (`arrival_carry_seed`).
  Stage 1 does not touch the core or the comparers.

Not done in this stage, with the reason:
- **WS13 (the gondola) and WS14's #152 after an event.** In the event-package path the run has ended and the auto replay has rewound
  the event core by then, and the Continue's startRun resets the world again. Ticking under those cuts would change nothing that
  lasts. They come with stage 5, where the heats run in the streamed core.
- **The new-career list is shorter than the PS2's.** The PS2 ticks 917 under the plane NIS; the port's #153 plays about 530 ticks.
  This is not stage 1's: the list is the cutscene owner's, and whether the PS2 plays #163 after #153 is unconfirmed.


### Stage 2: pv `eventWorldData`, the per-event data and exports (inert)

**Page:**
- at the gate, `ctm-event.js` fetches the event package's small files (npc-riders, lineups, rivals, lineup-sessions, progress-meter,
  freestyle-event, camera-triggers, initial.json where the course has its own) into an event plan while the fly-over plays;
- nothing is applied.

**Exporter** (to scratch; the coordinator copies it into `web/public/assets`):
- `tools/export_peak_world.py` keeps the GO LiveComp starts in PEAKn / MOUNTAIN SETPIECES with an event-only flag, instead of dropping
  them at :577.

**Core** (inert until called):
- `event_course_seed(code)`: `browser_select_event_course`'s start seeds (ground state, participant, progress origin, rolling start,
  camera). No dead / skip node lists: the section activation owns those in the streamed world.
- `init_race_event(text)`: replaces the race event (participants, checkpoints, clock) without touching the streamed paths.

**Tests:**
- `paths-eq` (stage 0) promoted to a web test;
- the GO starts in the streamed export == the event package's per course (ARA1: 12);
- `event_course_seed` + `begin_event_rider` in a PEAK1 core == the ARA1 core's rider state at tick 0 (a new test-ctm-event-world.mjs);
- all streamed gates unchanged.

**Built (2026-09-29), pv `eventWorldData`, off:**
- **`web/ctm-event-plan.js` `loadEventPlan(entry)`** reads, into `careerUI.eventPlan` (ctm-event.js, at the fly-over's start):
  - npc-riders, lineups, rivals, start, route, the progress meter, freestyle-event, camera-triggers;
  - the course's race-event document (its initial.json; Snow Jam's is ANIMATIONS/initial.json);
  - the GO LiveComp starts with their `go_tick`.
- **Two changes from the plan above:**
  - No exporter change. The GO starts come from the event package itself, so the peak worlds are not re-exported and no assets need
    copying.
  - No `init_race_event`. Every event course's race and reset paths are already its streamed location's variant-0 bank, in order
    (checked for all 17), so the existing `init_race` with the event's document is enough.
- **Core:** `event_course_seed(code)` (web/peak_world.inc) runs `browser_select_event_course` for the start seeds and restores the
  streamed world's dead / skip / scripted / roller lists and `browserEventCourseSeeded`. It is built in the scratch core
  `local/ctm-events/core`; the live core is not rebuilt yet.
- **Tests:** `web/test-ctm-event-world.mjs` covers:
  - paths = bank for the 17 courses;
  - the plan for the 17 courses (Snow Jam: 12 GO starts at tick 181);
  - `event_course_seed`: 1 for the 17 courses, 0 for PEAK1 / MOUNTAIN / A / "", and the start camera present / cleared. It needs
    CORE_JS for the scratch core.
- **Capture gates on the scratch core:** event-start, event-race, peak1-fr-aara1-glide, allpeak/p2r-start, ctm/fr-dra4a-full,
  peak1-arrive-bra2, peak1-green-start, event-race-ai and parity-ai/metro-race are all at their baselines.
- **Browser:** the ride-in with `?pv=worldUnderCuts,eventWorldData` in Chrome and WebKit gives `{code: ARA1, ready, riders 5, lineups,
  go 12, goTick 181}` by the switch.

### Stage 3: pv `eventInWorld`, the human's event in the streamed core (desktop QA first; not for players until stage 4)

**Page.** The gate, behind the switch:
- no `cb.course`; `careerUI.begin(mode, course, {inWorld: true})` in the current world;
- `freeRide` stays;
- `aiRace` stays null (QA: `?ai=0` semantics).

**The flow, mapped to world states:**

| WS | port |
|---|---|
| gate 22D6C8 | `peak_world_event_kind(k)`, `peak_world_game_mode(m)`, `event_course_seed(course)`, `init_race_event`, the Big Challenge end (`browser_mission_gate`), the score commit (12B180) |
| WS1 | [fly-over, approach, idle] in one list in the streamed world. The grid placement at the idle (1289F0 → 11D390 through `begin_event_rider`, control 6). |
| WS2 | the card (context 1) |
| WS2 exit / WS3 enter | at the card's Continue, `ctm_race_restart()` = the rider manager's 129768: **the section activation emptied (without leave handlers, as 103358) and rescanned under the event kind**, 101688(C+0xA8), the race clock zeroed (1297C8(C, 1)). No world reset offline (2300F0 is online only). Then `begin_event_clock`, the GO LiveComps at `go_tick`, and the replay snapshot. |
| WS4 onward | the event HUD, the finish panel, results |

**Streaming during the event:** `planAhead` is idle during an event. Rows other than the course's go at their normal release, or at once
with peakRelease.

**Gates:**
- all streamed gates;
- new `ctm/ara1-gate-solo`: c0a's human through the countdown and the race, on PEAK1 / MOUNTAIN with the comparer driving the gate at
  its recorded tick (a new `--ctm-gate TICK` in `web/peak-capture.mjs`). `--sync-rng` covers the riders' shared-RNG draws until
  stage 4.

**Parity:**
- the section list and the instance flags at WS3 + 1 against c0a (fences dead, reset planes killed per builtin 43);
- the frames at card / GO / +300 against c0a's samples.

**Browser:** Chrome and WebKit. No black frame between the fly-over and the approach; the frame times under the fly-over.

**Built (2026-09-29), pv `eventInWorld`, off (QA). It needs eventWorldData, and worldUnderCuts for the PS2's hold:**

The flow:
- **The gate** (ctm-event.js): after the fly-over, `ui.cb.eventInWorld({plan, mode, entry})` runs instead of `ui.cb.course(entry)`.
  It does:
  - `event_course_seed(code)`;
  - `init_race(the event's document)`;
  - `peak_world_event_kind` / `peak_world_game_mode` (game mode → 0x535C10 through 22D6C8's table: race 0, slope 1, pipe 3, big air 2);
  - the grid spawn from start.json;
  - the event's slope-style list and progress meter.
- **The intro:** `careerUI.begin(..., {inWorld: true})` skips the event load. `cb.introInWorld` plays 'career-ridein' [approach, idle]
  at the event's location in the streamed world, with the rider still held (nisCut). Then the card.
- **The Continue:** `startRun` runs the event start in the streamed world:
  - `_start_event`, with the countdown and control 6 at the grid;
  - the event's audio run start, not the free ride's;
  - the race HUD (freeRideHud is off);
  - no Big Challenges, gate, station or FAQ events while `worldEvent` is set.
  - `worldEvent` is cleared at a course switch. `cb.eventInWorldEnd` restores the free-ride settings; it exists for stage 5 and is not
    called yet.

Checks:
- **Browser, Chrome and WebKit** (`local/ctm-events/qa/event3.mjs`): new career → Transport to Snow Jam → Session 1 → the pad into the
  gate.
  - Fly-over #94, then approach #66 (Chrome) / #99 (WebKit), then idle #73 under the card ("Snow Jam - Race / Qualifier"), all in
    MOUNTAIN, **with no load screen at any point**.
  - Continue: the rider at the grid (−131865, 13859), the PS2's grid spot, through the 180-tick countdown, GO, then the race down
    Snow Jam with the race HUD (clock, progress meter).
- **Capture:** `local/ctm-events/capture_card.py` (ps2_capture's run answering the card through the command word 0x4A26FC = 2, the
  Cross path) gives **`caps/c0a-full`**: one record stream from the Snow Jam arrival through free ride, the gate, WS1's hold, the card,
  the countdown and 1150 ticks of race (3000 records). It is the stage-3 parity gate.

**Open, and the reason stage 3 is not exact:**
- The PS2's race-start placement keeps the rider's own state. 1289F0 → 1297C8 → 11D390's event branch (0x11D564) sets only motion 3
  and control 6 (then 120D58 in CTM, a vt call, 111890, 125038).
- The port's `begin_event_rider` replaces the rider state with the Single Event countdown anchor's (`browserEventGroundState`). That is
  why a CTM start leaves the PS2 at the push-off (c0a-race: 181).
- An exact in-world start needs a core `event_grid_start`: the grid placement's own writes applied to the current rider, the rest kept.
  Which fields 1297C8's vt call / 154A58 write is unconfirmed; the memdiff lists (§6.3) bound them.
- This is physics work of the same shape as `arrival_carry_seed`. With it comes the comparer's in-world mode for c0a-full: the
  PEAK1 arrival, the gate at the recorded tick, no compare under the hold, the event start at WS3, compare to the end.

Not tested: the results → Transport path after an in-world event (stage 5 builds it in place). The page falls back to the world
reload.

**The in-world race start, exact (2026-09-29, physics agent): core `event_grid_start`, comparer `--ctm-in-world`.**
- **PS2 (traced).**
  - The rider at the countdown: c0a-full's rider +0x100..+0xB40 at tick 1, against the Single Event anchor (event-race record 18).
    The two are equal except the words that neither WS1's hold nor the placement 11D390 writes:
    - the motion-0 object's stamps (motion owner +0x10 ground focus, +0x14 last ground leave; the hold's motion-0 exit wrote +0x14);
    - the boost meter +0x2F8, amount +0x2FC, drain +0x304 (11D390's tail zeroes only +0x2E8..+0x2F4);
    - +0x380 / +0x390, +0x360, +0x3C0 / C4, +0x3F8, the race-path cache +0x4DC..+0x4E8, +0x5B4, +0x764, +0x770, +0x9E0..+0xAE0.
  - The push-off: 13C7A8 enters motion 0 and scales the velocity by 0.7 + 0.01 × (tick − leave − 40), capped at 1. The Continue
    restarts the game tick at 0, and the leave stamp is free ride's, so a CTM start gets 0.7.
    - The anchor's leave is 0, which gives 1: c0a-race 182 was 160 cm/s fast.
  - The Big Challenge markers: 308DB8 (each WScript mission's +0x4C program, through the stage VM 2227D0) runs Hide (2FFB50 →
    350E90) on every marker at free ride's first tick (2066, entry probe on c0a-full.p2s). The event start resets no world, so they
    stay type-16 nodes through the race.
    - Example: flg_ARA1_BigCFlag_1002 at race tick 829.
  - The grid route: 11D390's 112180(rider, 1) gives the event document's `original_reset.event_route` (+0x490 / +0x4C8 / +0x4CC equal
    ANIMATIONS/initial.json's).
  - The rider manager's +8 runs on through the gate and WS1, and restarts at 0 at the Continue.
- **Core (live, see HANDOFF):**
  - `event_grid_start(x, y, z, heading)` resets the race session (reset_race), reset_rider and start_event, with the stage world
    kept (`browserEventWorldKept`: no browser_reset_pickups). It keeps the words above that the port models: the stamps, the boost
    words, +0x380 / +0x390, +0x2E4. It sets grounded: reset_rider's ground snap probes the course world, which a streamed world does
    not load.
  - `event_route_seed(docText)`.
  - `init_race` keeps the running session's total tick count.
  - The page calls: event_route_seed, _reset_pad_history, event_grid_start; no _reset_race before it.
- **Comparers.**
  - `compare-ps2-capture.mjs --ctm-in-world CODE` (with `--course PEAK1 --peak-arrival`): the page's gate (event_course_seed,
    init_race, the kind / mode), nis_hold after the tick before the hold record, no comparison under the hold. WS1's last tick is not
    stepped. event_route_seed + event_grid_start at the countdown's tick 0.
  - `--ctm-countdown` (both comparers): the kept words from record 0 of a countdown savestate.
- **Gates.**
  - `ctm-events/c0a-full`: exact through race tick 1507. 1508: a soft collision on the PS2 with no instance contact, a computer rider
    (stage 4).
  - `ctm-events/c0a-race` / `c0c-race` (human only): exact through 309 / 318. The idle upper reaction (115D48, after 1 s idle)
    picks a computer rider within 10 m.
  - Six-rider (pending eventInWorldAi): the human is exact to the end. The shared RNG leaves at 461, the riders at 1199 / 1210 / 722
    / 686 / 762 (c0c: 861 / 529 / 861 / 529 / 792).
- **Not settled:** the gate's score commit 12B180 and the event HUD bank's stale words (c0a-full score gated through 2821).

### Stage 4: pv `eventInWorldAi`, computer riders in the streamed world (players get stages 3 + 4 together)

**Core:**
- **Collision in rider contexts.** A parse cache for appended location documents, keyed per location (terrain, world collision, body
  terrain, rails), so that each context takes a resident location's parse without its text. Each context gets its own append
  `browserBodies` / rails for the resident locations.
- **The path bank in rider contexts.** `peak_world_deliver_paths_to(context, location, slot)`: the bank installed in a rider context
  from the human's, at the rider's slot, not "player 0". Rider contexts never run `pass()` and never write the shared residency or
  octree.
- **Section node states** join the shared-world replay (a new `world_event_apply` kind for slot-1 DeadNode / RestoreNode / Hide), so
  every rider's world copy matches the human's, as in the PS2's one world.
- **Context reuse.** Keep up to five rider contexts in the streamed core. `rider_context_reset(block)` re-runs the context's statics
  in place, which replaces the missing destroy, and re-feeds the resident locations when the course changes. A context's heap must not
  grow across events (checked).

**Page:**
- `createAiRace({worldKeys: streamed})` from the event plan;
- one context step per frame under the fly-over (the 1296F8 shape);
- "Loading..." bit 2 and the list held at a step end (279040) while any is unloaded;
- the riders drawn in the fly-over (`cutsceneCast` includes them in the world);
- `aiRace.start` at the WS1 grid placement.

**Gates:**
- `npm test -- ps2-captures parity-ai`: unchanged (event package);
- new `ctm/ara1-gate-ai`: c0a's six riders on the streamed core through `compare-ai-capture.mjs --course PEAK1 --ctm-gate`
  (human, AI, RNG and ranks, like the parity-ai gates);
- c0b's Metro-City and ASS1;
- test-rider-contexts with a streamed world (a context created mid-free-ride rides like a fresh one);
- test-shared-parse;
- a new appended-parse-cache equality test (by `world_load_hash` / `rail_load_hash`).

**Memory** (WebKit phone tier 844x390 quality=low, `footprint`; Chrome heap snapshots). Gate → race → results → return, three cycles,
against today's event load. Pass: no higher than today's event peak, and flat across cycles.

**Browser:** the riders at the gate in the fly-over; a whole Snow Jam final; a Metro-City qualifier; a slope style; a backcountry rival
race (entered through the Transport, WS10 → WS1 arg 1).

**Built (2026-09-29, milestone 1; scratch core `local/ctm-events/core`, not live):**
- **The resident set is the event package.** For all 17 event courses, the event package's collision instances, terrain patches and
  rails are exactly its resident locations' (`event_locations`), concatenated in order, with the same tracks and resource ids. So a
  rider context takes the event package's world, which is the streamed world's resident set for the event. The shared residency flags
  gate it by the same tracks. No per-location append feed and no appended parse cache are needed. The path bank needs no
  `deliver_paths` beyond player 0 either: every context's `init_race` holds the event document, whose paths are the bank's (stage 2).
- **Core:**
  - `event_world_bodies_only` (world_bridge.cpp): a context's event load in parts builds its body collision only. `terrain_part` and
    the seal leave the shared camera terrain, which is the streamed world's.
  - `world_event_apply` kind 9, world node state (shared_world.inc): resource, node type, collision runtime flags, stage flags. It is
    logged by the core with `shared_world_nodes(1)` (the streamed world's human) from `stage_set_flags` (stage_world.inc: every
    DeadNode / RestoreNode / Hide / flag change ends there) and from the LiveComp node replacement. It is logged also while the human
    replays another rider's contact.
  - `world_node_states(list)` / `world_node_states_apply(list)`: the whole state of a list of instances, from one context into another.
- **Page:**
  - ctm-event.js calls `cb.eventAiPrepare` at the fly-over start, as the PS2 makes the riders at gate + 2. main.js fetches the event
    package's collision / environment / local lights and cuts it on the worker. It then runs `createAiRace` with:
    - the event entry as the course;
    - `prepareWorld` (load-slices.js `feedContextWorld` + `feedEventRails` into the first context, frame-sliced; the others copy by
      key);
    - `contextSetup` (the human's `stage_object_route` / `stage_load_flags`: one world's rules);
    - `hostAtStart` (the human core's hooks and rider pairs join at `start()`: the human rides on under the fly-over);
    - `contexts` (the last in-world event's blocks: no destroy, the world, animation and NPC state are set up again in place).
  - `cb.eventInWorld` attaches the race as `aiRace`. `introInWorld` runs the lineup (`aiRace.prepare`, the warm-up's) before the
    approach's cast, which now includes the riders. The anchor RNG is off: the game RNG runs on from free ride, as no event load happens.
  - `startRun`: `aiRace.start()`, then `worldAiSync` (the human's node states into every context, after their start's own event
    seeds), then kind 9.
  - `cb.eventInWorldEnd`: `detach()` (hooks off, models disposed) and the blocks back to the pool.
  - startRun (for the physics agent): `event_route_seed(plan.initialText)` + `event_grid_start(spawn)` replace resetPhysics +
    `start_event` for the in-world event.
- **Approximation (unconfirmed):**
  - The CTM collectibles (`mdl_ARA1_collecta_*`) are live MagnetModifier pickups that the human's section programs build. The contexts
    do not hold those entities, so the sync keeps the context's own state for an instance whose human flags route it to an entity the
    context cannot answer (a DeadNode from the event load).
  - On the PS2 the magnet answers only the instance-contact query (0x357538, filter 1), and its gate 0x357660 refuses a computer rider.
  - Not traced: whether such a contact can win 105398's contact selection in the same tick.
  - Without this, a rider's crash query met an entity route with no entity (Moby, tick 453).
- **Tests and runs:**
  - `web/test-ctm-event-ai.mjs`:
    - a context's world in parts == a normal event's rider context (body / rail hashes), and the human's world is unchanged;
    - the next context copies it by key;
    - node sync (3496 instances; 83 differ between the event load and the streamed world) and kind 9 replay;
    - 4 reuse cycles == a fresh load, and the heap stays flat.
  - QA `local/ctm-events/qa/event4.mjs` (scratch core through `local/ctm-events/core-proxy.mjs`, port 5199), Chrome and WebKit:
    - 5 riders made under the fly-over;
    - the PS2 c0a roster (Psymon, Allegra, Moby, Griff, Luther) on the card, drawn at GO;
    - 40 s of racing without errors, place HUD 5th/6.
- **Six-rider parity (live core db807fea):**
  - `compare-ai-capture.mjs --in-world-ai [--node-seed]` sets the riders up by the page's path. `local/ctm-events/caps/<run>.nodes.json`
    holds the countdown savestates' 132 node states (tools/export_peak_seed.py seed_state), put into the human and synced to the
    contexts.
  - c0a-race and c0c-race score the same as the event-package path: human exact to the end, RNG through 460, riders
    1199/1210/722/686/762 and 861/529/861/529/792.
  - With the section world pass's draws taken from the capture (`--no-sections`), all five riders, the RNG, the ranks and the pair
    records are exact to the end in both heats. New gates: `ctm-events/c0a-race-riders`, `ctm-events/c0c-race-riders`.
  - So the rider contexts are exact. The 461 was the collect path, not the activation list (the R1 guess was wrong):
    - In CTM (0x535C11 = 0) 30C4A8 leaves an uncollected collectible without entity. Its section lists it and the slot-1 program builds
      the LiveComp, magnet and halo (3 x 0x341bbc at tick 460). The event-package human ran the Single Event path, where they are DeadNodes.
    - `--in-world-ai` sets `set_stage_collect_state(0, mask)`. The mask is derived from the countdown savestate: its DeadNode
      collectibles, mapped through the collection list (core `stage_collection_list`).
    - Mapping from the code: 30C4A8 stores the resource at list +0xC + 4 * count, then asks 1538E8(profile, 0, index = count, id)
      (0x30C510..0x30C548). So the bit is the list position in builtin 38's argument order; for ARA1 that happens to be collecta_10NN = bit NN.
    - Both heats are now exact to the end natively (human, 5 riders, RNG, ranks, pair records): `ctm-events/c0a-race-riders`,
      `c0c-race-riders` (the semi: 1017 / 1026 / 1027 collected, mask 0x0C020000).
- **The whole run from the arrival (`local/ctm-events/caps/c0a-full-ai`, `compare-ai-capture.mjs --ctm-full ARA1`, pending gate
  `ctm-events/c0a-full-ai`):**
  - Capture: the free-ride baseline has no riders, so `capture_card.py --ai-dynamic` writes their actor addresses into the AI hook's list
    once the roster holds them (record 802, tick 2840).
  - The riders start at 1297C8's placement: 10B790's grid command draws on the countdown's tick 0 (2 per rider). So in-world riders ride
    from tick 0, not held to the Single Event anchor tick (`anchorTick: 0`, ai-racers.js).
  - 129768's 0x103358 at the Continue empties the activation list (A+0xD8 132 -> 0, A+0xD0 = -1 at tick 0, 147 at tick 1): core
    `section_restart`, called by the page's in-world startRun and the comparer.
  - The RNG is exact from the arrival through free ride and the gate. It falls 4 draws behind at free-ride tick 2854, the first tick of
    WS1's hold:
    - The PS2's activation gets a second point there. The NIS director (vtable 0x482380: 0x2816A0 / 0x281370) registers its +0x100
      through 0x1033B0 (A+0 count++, A+0xD0 = -1).
    - 0x281100 copies the active camera's +0x20 (camera list *(*(*(gp-0x848)+0x84)+0x84)[director+0xF8] +4) into it.
    - 101B60 scans a box around every point. So the fly-over camera lists 42 more pieces (A+0xD8 132 -> 174), whose slot-1 programs
      draw (the collectibles' LiveComps).
    - The port scans around the rider only. A collectible that the PS2 built under the fly-over then builds at tick 0 in the port
      (+1 draw), and the riders leave at 185..438.
    - Next: a section source fed from the page's NIS camera (cutscenes.js). Unconfirmed: which point the NIS cameras hold at +0x20 (eye or
      target), and where the director removes its point (count 177 -> 151 at 2899).
- **Milestone 4 (2026-09-29): the whole run from the arrival is exact with six riders** (`ctm-events/c0a-full-ai`, human, 5 riders,
  RNG, ranks and pair records through all 1666 race ticks).
  - **The director's point, settled** (savestates `local/ctm-events/caps/c0a-snap.tick*.p2s`):
    - The point is camera-list index 0: the outer compositor camera 0x157eb50, its +0x20, which is the rendered eye (+0xE0 is the
      look). It lags one tick: the director's copy in tick T reads the eye as tick T-1's camera update left it.
    - A+0 stays 2 from 2853 through 3369. The drop from 177 to 151 at 2899 is an ordinary scan, not a removal. The removal is
      0x281400 -> 0x1033F8 (-1, then drop).
    - A Transport ride has one too: c0b-snap, A+0 = 2 through the ride and 1 from the arrival placement (2057, the record P that the
      arrival comparers start at).
    - Core: engine Activation points (one box per point, a scan when any point moves >= 2000 cm) and `section_point`.
    - Page: cutscenes.js `sectionFeed` (the script's camera eye at each update, re-added at a new step, removed at finish / stop) ->
      main.js `sectionPoint`, pv eventInWorldAi from the gate's fly-over, or pv `nisSectionPoint` for every streamed-world NIS.
    - Unconfirmed: 279370's test of which scripts' directors register, and the tick alignment of the page's eye (the last update's pose).
  - **The riders under WS1:**
    - 129E20 builds them fresh at gate + 2 (+0x390 0, the boost words 0, drain 1). The approach NIS places each at its actor (3130),
      and the rider manager ticks it held: +0x2E4 ramps under motion 3 to 2651.51, and +0x380 settles by 3132.
    - 1297C8(C, 0) / (C, 1) put them on the grid with 11D390, which keeps those words.
    - Core `npc_fresh_rider` and `npc_grid_start` (npc_start_event with the carried words kept, as event_grid_start). ai-racers.js
      `holdTick` / `start({gridStart})`. Page: cutscenes.js `onRaceActors` -> main.js `worldAiTick` (a game-tick.js hook after the human).
    - Remaining difference, not in any position: +0x380 (the held contact normal, written by the PS2 every held tick; the port's
      NIS hold branch in core.cpp does not write it, for the human either). Which PS2 routine writes it under control 13 is not traced.
  - **Comparer:** `--ctm-full` feeds the director's point from the record's outer camera (in from WS1's hold, renewed where A+0xD0 reads
    -1, out at the Continue) and follows the recorded rider positions under the hold (the page follows the NIS actor). It injects no
    capture section draws (the streamed section pass is native).
  - **pv nisSectionPoint and the other gates:** no existing gate moves.
    - The arrival / Transport captures start at P, where the ride's director has already removed its point.
    - The free-ride captures have no NIS inside them.
    - The human-only in-world comparer (compare-ps2-capture.mjs --ctm-in-world) scores no RNG, and its first difference (1508) was a
      computer rider's contact, which c0a-full-ai now has.
- **Open:**
  - the memory runs (R7 / R13);
  - opponent trails and relationship icons (`createOpponentFx` / `createRiderIcons`), not made in-world yet;
  - the pool is not refilled if `createAiRace` fails mid-way (contexts lost; no destroy).

### Stage 5: pv `eventReturnInWorld`, heats and the return in place (#3)

- **WS13** (Next heat, results Restart): `ctm_world_reset()` = 230180 (the set-piece buckets, missions, stage-world reset, NIS reset,
  HUD message 7; the activation list kept), the round's roster reconfigured in the existing contexts (`aiRace.prepare`), the gondola
  ticking (stage 1), then WS1 arg 3 → the card → WS3's reset.
- **WS15** (the post-event same-location pick, the booth Back, Session):
  - the Big Challenge end, then kind 4 / mode 12 (`peak_world_event_kind(4)`, `peak_world_game_mode(12)`, the finish rule);
  - `ctm_world_reset()`;
  - the riders parked and undrawn (their contexts kept for reuse);
  - 11DE60(session point 1) + 11DF18 (`freeRide.spawnFor` → `place_rider_region`), the white fade in 1.0;
  - WS1 arg 0 → WS3's reset (the rescan under kind 4 brings back the finish fences) → WS4 on consecutive ticks.
  - `goWorld`'s same-world branch calls it instead of `cb.freeRide`.
- Another location is the in-world Transport as today.
- The replay (Replay item and the auto replay): its camera-trigger core is loaded from the event plan's camera-triggers.json. That is
  unconfirmed ground (R9), so the item is disabled when the core can't load it.

**Gates:**
- `ctm/ws15-return`: c0d from the placement record, like the peak1-arrive gates;
- `ctm/ws13-semi`: c0c from the semi's countdown;
- test-ctm-flow updated.

**Parity:** the white-fade frames against c0d; the finish-fence instance flags after the return (kind 4 again).

**Browser:** results Transport → the same location with no black frame and no "Loading...", in Chrome and WebKit, three cycles, memory
flat.

**Stage 5 rules from the PS2 (2026-09-29, in progress):**
- **WS15 (236058).** In order: 30B7F8, 238160(GMM, 12), 230180, 11DE60(human, Session point 1, kind 2) + 11DF18, 230698, and the
  white fade.
  - Capture `local/ctm-events/caps/c0d-ws15` (--ai-state, from ctm-parity confirm.p2s). Record 0 is already the placement.
  - **All five computer riders are placed with the human.** They are on the same row with the same first tick: records 0..1 show
    identical positions for all six. They ride in rider pairs, push the human by up to about 60 cm, and go at the WS4 restart
    (record 8: C+0x78 6 -> 1, game tick 0).
  - The activation list is kept through WS15 (75 at record 1) and refilled by the tick-0 rescan (87).
  - Page, pv eventReturnInWorld (off): main.js keeps the race's riders (`eventInWorldEnd({keepRiders})`), places them on the human's
    row (ai-racers.js `resume`, place_rider_region) and removes them after 8 ticks (`worldAiBefore`, game-tick.js hook).
- **230180 (the world reset):** 26CA90, 309030, 354C98, 355118 x 2 (buckets 1 and 8), vt call, 2D9B40, 22E7C8, 3A6800, 26F228,
  2789E0, 15DB58, 229498, 343BC0, 357B38, 317670(0x4A2AB0), 39F9D8, a vt call, 309F18, 308C60, 28B180, 287108 (0x230198..0x23028C).
  - Core `ctm_world_reset` = the port's reset_race world part (browser_reset_pickups). Each callee is still to be checked against the
    capture.
  - 317670 on 0x4A2AB0 is not the gameplay RNG 0x4FF030. Unconfirmed: which generator it seeds.
- **WS13 (235AA0):** 30B7F8, 2382D8 (restartHeat, one vt call), 230180, the round's roster, the gondola (the world ticks, the human
  held in it), 128958 / 1296F8 / 1289F0, WS1 arg 3 -> WS2 -> WS3. Page: `ui.cb.heatReset` -> ctm_world_reset before the gondola.
  - Not built yet: the world ticking under the gondola, and the riders' state across it. That needs the long capture.
- **Long capture** (in progress): `c0a-ret` = c0a-full-ai + Start at race tick ~1700 (the tick script) + the menus (menu_pad.py, the
  pad device hook relocated to 0x9C000; capture_card.py `--menus-at`, ps2_capture's pad hook off while paused, on again after the map)
  + WS15 + the ride.
  - **c0a-ret is not a player's return.** It reached WS15 by poking command 4 (RESULTS_POKE), which skips stopAutoReplay 0x2706F0, so
    its riders and RNG were the auto replay's frame (replay counter C+8 frozen at 407; RNG at record R = race record 407's). A player's
    Transport is command 1 with item 2 (0x20CF80 -> 0x2706F0 at 0x20CF88 -> command 4): the results-time snapshot comes back first
    (docs/replay.md "What 0x2706F0 restores"). Recaptured as `c0a-ret2` (capture_card.py TRANSPORT_ITEM, RECORDS_OFF_AT_REPLAY: the
    records run through the Give Up's EndRace to the replay's start; watches on the setup slots 0x5305B0 / 0x535B20, the replay manager
    R = 0x1454440 +0 / +0x3D0 / +0x484 / +0x600..0x634 and the menu pad's sample counter 0x9C804).
- **WS15's rider placements (from the code).**
  - 230180's C vt+0xCC+0x28(3) (0x2301CC..0x2301E0; vtable 0x458488 +0x2C = 0x129768) -> 0x103358 / 0x101688, C+0x98 = C+0 = 0,
    113B10(C, 3), 1297C8(C, 1). 1297C8 (0x1297C8..0x1298BC): 10F398 -> 10F3B8 (the pair records made again: +0 = a != b and b loaded
    (11D640), +8 = 1e10 [gp-0x7CB0], +0xC..+0x20 = 0 (0x10F420..0x10F4F4); +0x4D8 = 113128(rider), +0xE8 = -1), C+8 = C+0xC = 0,
    C+0x18 = -1, C+0x1C = 0, then per rider 154A58(k, [rider+0x790] + 0xFC) and 11D390(rider). Core `race_world_pair_restart`; the
    web's pair tick restarts at 0 with it (ai-racers.js resume): no pair check into record 1, the first separations into record 2, the
    first impulse at tick 4 (107888: +0x14 < tick, +0x10 < tick - 3).
  - 11D390's free-ride branch places every rider (the human too) at its location row, then WS15's own 11DE60(human, Session point 1, 2)
    + 11DF18(human, 1) (0x2360CC / 0x2360E8) places the human again: its +0x390 is the row's normal.
  - 11DE60 is straight-line code (0x11DE60..0x11DF0C): every caller runs 11FEC8(0) / 11FE78(0). A running crash ends there: control 8's
    exit (table 0x456B90 -> 12E690: wipeout speech 2A02D8, 10E028(reaction 4) on an even logic tick, which 11D660 clears again: +0x358 = 0),
    motion 2's exit (0x456B10 -> 136F28) is empty. Core place_rider_region (it threw "Crash needs a sampled world pose" on a rider that was
    mid-crash at the Give Up).
  - Stance: 11D390's free-ride branch writes +0x324 = CHARDB[base character of setup slot +0x86C].stance (0x11D3F8..0x11D424: 14EF70 ->
    14A080 (setup slot 0x5305B0 -> 0x535B20 + i x 0x1C +0x11) -> 0x530970 + id x 0x88 + 0x44); 11D660 then +0x320 = +0x324
    (0x11DC00 / 0x11DC1C). The event branch (0x11D564) skips it. The port's placements use the reset document's stance (the anchor's
    word); core `reset_stance_seed` sets it for a host that knows the slot's character. Which characters the setup slots hold after an
    event is read from c0a-ret2's watch (the c0d map state held the final's roster in race order).
  - **The Give Up's coast (c0a-ret2 records 3032..3320, from the code and the records):** the tick script's Start is read in tick
    1699 (record 3032, script index 3033); the pause menu's Give Up (1253D0) and Yes come inside that tick, whose record is written
    after the menu (its menu-pad sample 356 is the Yes's Cross). The human finishes in 1699 (+0x470 = 1/60 in record 3033), control 10
    from 1701; the live ticks run to 1987 (finish + 288, RESULTS_TICKS_TIME_UP counting the finish tick), the auto replay starts at 1988
    (records off from there). A map-time savestate reads C+8 = 1990 with the riders 3 ticks past record 3320: after 0x2706F0's restore,
    the stop frame and the WS14 frame each ran one tick; the map then holds the world.
  - compare-ai-capture.mjs `--coast-only` scores that stretch: the five computer riders are exact through all 288 coast ticks, the
    RNG and the ranks too with a neutral coast pad (COAST_NEUTRAL). The human is not: the Yes's Cross is still held on resume (menu
    samples 354..362, ticks 1699..1705). The PS2 then shows crouch target 1 with no prewind in 1699 and no jump in control 10; the port
    enters the prewind (a fresh press edge) or, with the Cross from 1700, jumps out of control 10. Settled 2026-09-30: one history for the
    menus and the ride plus 1162C8's +0x360 latch ("Carried presses" below).
  - Page (pv eventReturnInWorld): game-tick.js stops an in-world event's live ticks at finish + 288 / + 408 (main.js
    gameHost.liveStopAt); the Transport runs the two ticks (stop frame, WS14 frame) before the return. The event-load path already
    ends its live ticks there: its auto replay starts with the results (main.js, running = false).
  - **The human's route heading +0x4CC at WS15 (settled by probe, 2026-09-30).** local/ctm-events/probe_calls.py (jal sites redirected
    to logging stubs; capture_card.py PROBE / PROBE_AT_SAMPLE) on c0a-ret6b / c0a-ret7: 0x2706F0 (0x20CF88) puts back the results-time
    +0x4CC (replay -2.9932 -> 1.6149) and +0xAB8; the stop frame's tick 1988 leaves both; **world state 14's enter 0x236250 (arg 2)**
    then clears the human's boost meter +0x2F8 (0x2362B4) and runs **11D390(human)** (0x2363DC) while the event kind still holds: its
    112180(human, 1) re-attaches the route (+0xAB8 = the bank's path 2, the start row 0's reset path, before the WS14 frame's 112338) and
    its event branch holds the rider on the grid (11FE78(3) / 11FEC8(6)); the WS14 frame's tick 1989 then runs 1125C0 there: +0x4CC =
    -2.9563, the value WS15's record shows (the only writer is 1125C0, 0x121890). compare-ai-capture.mjs models it: the stop tick,
    boost_meter_clear, event_route_seed + event_grid_start (the port's 11D390 event branch) + start_row_reattach(bank, 0, zoe, route only),
    then the WS14 tick. The heading seed is gone.
  - **112180 in the port** (core start_row_reattach, 2026-09-30): the start-row 11D660 (+0x370 = the start pad's normal, which 11DE60's
    13C7A8 copies into +0x390: every computer rider's (-0.0001, 0, 1) at record R) and the route caches (+0xAB8 = the row's reset path,
    the lanes p0/p1/p3/p4/p5 of records R; +0x4D0 = +0x4D4 = 353496). Used only by the return (free-ride branch); today's placements are
    unchanged. 1297C8 also zeroes C+8 (0x1297F0): ai-racers.js resume restarts every rider's game tick, so the tick-0 route re-pick
    (112338, tick % 60) moves the riders onto free ride's paths as on the PS2 (p113 / p105 / p106 in record R + 1).
  - Scratch-core scores against c0a-ret3 (compare-ai-capture.mjs, no seeds): record R exact for all six; tick 1 exact for the human and
    four computer riders, Psymon's (setup slot 1, Zoe's since WS15) velocity 0.1 cm/s off: his tick-0 speed limit is 2299.30 in the
    port against the PS2's 2301.00 from the same carried 2302.15 (the 11B3F8 target differs; the cause is open). Tick 2 (the first
    pair separations) 10..23 cm; the human's carried +0x380 already differs at R (0.4849 / 0.4809: the Give Up tick's held Cross,
    below). (Superseded: the gate passes, see "The WS15 gate" below.)
  - The rider that was Psymon rides as setup slot 1 = character 4 after WS15, so everything 14A080(slot 1) feeds reads Zoe: the stance
    (above) and the start-row offset (115B08 -> 14EFA8). The writer is 149A88(., 1) (0x149AE8, character 4); its WS15 caller is not
    found statically (its callers 14DE28 / 149860 are reached only from front-end states and 14DC80's lazy init: an indirect call; open,
    the probe tool can log 149A88's ra).
  - The return's first tick reads the device pad, not the tick script, in these captures: the map confirm's Cross is still held (PS2
    control 2 at record 1). A capture artifact of the menu script, fed as such by compare-ai-capture.mjs.


**The WS15 gate (passed, 2026-09-30; scratch core c894e9b5 / 99c09586).** `ctm-events/c0a-ret3` in test-ps2-captures.mjs
(`returnGate`): all six riders exact from WS15's record R to the WS4 removal, the human on all 729 records from R to the end of the
capture, the shared RNG, the ranks and (from R) the pair records exact. compare-ai-capture.mjs --ctm-full runs the page's own return
(web/event-return.js; main.js cb.freeRide / eventInWorldEnd / worldAiEnd call the same functions behind eventReturnInWorld). No seeds
are left. The full ps2-captures suite on that core: 264 scenarios clean. The rules, in the PS2's order:
- **The results' Transport** (`transportMapEnter` between the stop frame's tick and the WS14 frame's tick): WS14's enter 0x236250 arg 2
  clears the boost meter (0x2362B4) and runs 11D390's event branch (0x2363DC): 112180(human, 1) on the bank's start row, the grid hold
  (11FE78(3) / 11FEC8(6)). Core `transport_map_enter` (start_row_reattach + grid_hold_enter).
- **WS15** (`sessionReturn`):
  - 238160(GMM, 12): kind 4 / mode 12, no event course, no freestyle bonus list, no time limit;
  - setup slot 1 turns to a player setup (149A88(., 1): character 4, +0x10 |= 2). Core `rider_setup_player_reset` (slot 1's context:
    the stance CHARDB[4].+0x44, the size 85 x 0.01, the stat getters' 0.5 while 1477E8 holds: 1494C0 / 1493D8 / 148D80 / 148F50) and
    `race_world_player_setup(1)` (its pair inputs: weight CHARDB[4].+0x40 = 65 through 0x11FF98, collision and attack stat 0.5 through
    148F50). The caller of 149A88 at WS15 is still not found (an indirect call); the effect is confirmed by the setup-slot watch and
    by the ticks-5..8 pair impulses (exact only with it);
  - 230180 -> 129768 -> 1297C8(C, 1): the pair records made again (race_world_pair_restart, and each rider's own 115D48 peer view:
    core `rider_peers_restart`, 10F3B8 at 0x10F420..0x10F4F4), C+8 = 0, then per rider 11D390's free-ride branch (core
    `location_entry_place`: 112180 on the row by slot, then 11DE60 + 11DF18);
  - WS15's own 11DE60(human, Session point 1, 2) + 11DF18(human, 1) (0x2360CC / 0x2360E8).
- **WS1 after the return** (web/ai-racers.js): the riders sit out ticks 3 and 4 (12AB20 C+0x14 = 0 at 0x12AB40; phase 3 sets it again
  at 0x234634); 128A48(C, 0) at tick 3 (rank mode 0, every +0xEC = 0; 0x234570).
- **The removal** (`sessionRidersLeave`, 8 ticks after WS15): 12B030 -> 12AE38 (C+0x78 6 -> 1, 0x234714), WS1's exit 1297C8(C, 0):
  10F398 with the human alone (12B030's own 10F3B8; its peers off: without it the port's 115D48 played a look-at reaction at the removed riders at the
  return's tick 66 and zeroed the idle clock +0x35C, the first RNG difference), C+8 = 0. **One extra full-rate animation step for
  the human in that frame** (measured: the records' sequence times jump 2/60 there, sem 268 0.1000 -> 0.1333; c0a-ret11,
  probe_entry.py on 3135B0 with dt 1/60, shows the human's slot advances twice in the WS3 tick-0 window, c0a-ret10 its controllers'
  311B20 once). Without it the human's pose was 4..12 cm off from the removal and its landing 6 ticks later left the original.
  **Its caller is world state 1's exit 0x234750**, which follows the removal in the same frame: for free ride (GMM+0x48 = 4)
  128A10 -> 1297C8(C, 0), 26F7B8, no 129768, C+0x14 = 1, 128A48(C, 0), 308F38 / 308C60, C+0x8C = 0, 113B10(C, 3), 14DE68(1), then
  **129160(C)** (0x23488C; 0x129160..0x1291DC): each listed rider, only the human by then, gets 11EB60(rider, 1.0) + 11EB98 +
  3103F0(rider+0x780). Core `rider_pose_step` (the 11EB60 step; 11EB98 / 3103F0's pose is the next tick's in the port). The removal
  12B030 itself is 12AE38 + 10F3B8 (the pair records made again), so the human's peers go off there. **Correction (2026-09-30):** an
  earlier version of this note said WS2's frame takes 128AF0's paused branch (0x128CB8 -> 120ED8). Probe c0a-ret12 (the branch's
  `jal 120ED8` at 0x128CDC, from the first menu sample through the pause menu, the results, the map, WS15, the removal and 12 records
  after) logged **no call**: the paused branch does not run in any of those frames.
- **Open, not needed by the gate:** the removal record's bones (the paused pass's pose, which the comparer applies after scoring
  that record); why 1297C8(C, 0)'s 11D390 (+0x880 == 7 test at 0x11D3B4) places nobody at the removal (no 11DE60 then, probe c0a-ret9);
  the coast after the Give Up (race ticks 1700..1987, from the tick script: not gated). 128AF0's paused branch is taken for any mask
  with bit 0x1 when the game frame reaches the rider manager (0x230CB0 has no mask gate; its skips come from 270280 / 2379C8 /
  26CE50 and the 0x230B54 frame skip), and it ends at 0x129124 (section pass, 1013A8, C+8++); in c0a-ret3 it never ran (c0a-ret12),
  so those frames skip the rider manager (C+8 held 1990 through the map).
- **Tools:** local/ctm-events/probe_entry.py (function-entry hooks for functions with many call sites: FILTER on a0, LOG_F12 /
  LOG_REG), probe_calls.py per-site modes (a0v0 / a0a1, RIDER filter, entries reserved before the call). Probe captures c0a-ret9
  (11B3F8 / 11FE78), c0a-ret10 (animation entry points on the human's animator 0x58B800), c0a-ret11 (3135B0), c0a-ret12
  (0x128CDC, the paused branch: no call).


**(b), the replay behind in-world results (2026-09-30).** An in-world event replays behind its results as the event-load path
does, and the results' Transport restores the results-time state, with the PS2's two snapshots (0x26D818 at the countdown,
R+0x3D0 at the replay's start; 0x2706F0 restores it): the port's rider-context snapshot, [replay.md](replay.md) §2a. The c0a-ret3
gate runs through it (--replay-return). On the page (behind eventReturnInWorld): main.js inWorldCountdownSave / inWorldReplayRestart
/ inWorldResultsRestore, web/event-snapshot.js. Verified in Chrome and WebKit (local/ctm-events/qa/event6.mjs): the replay plays behind
the results, the Transport restores the results time, WS15 places the six at Session point 1 (the white fade over them) and they go
after 8 ticks. **The place HUD during those 8 ticks (settled from the code):** 0x1EA930 sets the HUD's flag word +0x3CC to
0x478078[GMM+0x4A]: in free ride (GMM+0x4A = 12) 0x1530C380, no place bit; then, while the rider manager lists two riders or more
(C+0x7C + C+0x80 >= 2, 0x1EAA18..0x1EAA3C), it adds 0x1 (the race place) and clears 0x30. So the PS2 draws the place over the return's
ticks beside free ride's own widgets (0x80 / 0x100 / 0x200), and not after the removal. Its value is the human's +0xEC: the race's
until 128A48(C, 0) at the return's tick 3, 0 (1ST) from there; the total is C+0x78 (6). The page shows it the same way (web/ai-race.js
hud() while the riders ride; the ranks are gated by ctm-events/c0a-ret3). The white fade covers it either way.

**The player's qualifier and Next heat (c0a-ws13, 2026-09-30).**
- Capture `local/ctm-events/caps/c0a-ws13` (local/ctm-events/ws13_capture.py): c0a-full-ai's arrival and free-ride pads, the gate,
  WS1, the card (command 2 as every c0a capture), the race ridden closed-loop by ps2_autopilot's Pilot (tuck + pure pursuit on the
  rider's route lookahead +0x4A0) from the race's index + 190, the human's own 3rd place at race tick 11975, a neutral pad from the
  finish, the results' Cross on Next heat (menu_pad.py's device pad, the pad switch off), WS13, the semi card's Cross, 1,500 semi
  records. The live ring hook (128 slots below menu_pad's arena) reads menu_pad's pad switch; pads.jsonl holds every entry written
  (0 misses), so REPLAY_PADS=RUN.pads.jsonl replays the same ride open-loop for probes (PROBE_FROM / STOP_AT / SNAP_AT; WS13_PROBE =
  9d0 | contact | entry:FUNC). Record timeline: race tick 0 at 1333, finish 13309 (tick 11975), live stop 13715 (finish + 407),
  the auto replay 13716..14016, WS13 14017 (tick 0, control 13), the semi's riders re-allocated at new addresses (re-listed at 14036),
  1289F0's tick restart 14318, WS1 arg 3 14511, WS2 14542, the semi countdown 14543. The 0x128CDC probe never fired.
- Scored with compare-ai-capture --ctm-full it exposed four live-game parity bugs (all fixed in the core, full suite clean): the
  crash board detach's +0x9D0 (crash-motion.md), 115D48 before 114CC0 (ai-racers.md), the trigger RestoreNode (set-pieces.md) and
  control 10's controller-phase draw (the celebration variant). With pv peakSplines (the EZrocketCore Spline draw at 11865 and the
  dragons at 11944, shared world event 10) the whole qualifier is exact to the live stop: gates ctm-events/c0a-ws13 (to 11864,
  splines off) and ctm-events/c0a-ws13-splines.
- Next: WS13's own path: the results' auto replay (records 13716..), Next heat's 2706F0 (0x20CCF8: 2706F0 then 231250(S, 13, 0, 1)),
  WS13's enter 235AA0 (14DE68(0), 30B7F8, 2382D8, 230180, the round's roster 144D98 / 147338 / 1473D0, 12AB20 (+12AC48 with >= 2 AIs),
  the gondola 27A860 / 2790A0 / 27AAF8 / 278F38, Loading bit 3) and update 235CC8 (phase 0: 12ABD0 -> 12B030 (+12B000), 128958; phase 1:
  12A180 else 1296F8, then 1289F0 and the streaming box 122C28 / 3A9658; phase 2: 3A9770 >= 20000, >= 120 ticks, 2791D8, 230698, 279298,
  the NIS idle -> 27A9F0; phase 3: 233AA0 -> WS1 arg 3), then the card and the semi.

**WS13 through the semi (compare-ai-capture --ws13, 2026-09-30).** The comparer runs WS13's own order (web/event-heat.js heatEnter,
web/ctm-heat-setup.mjs planHeat): the results save at the live stop, the replay skipped, Next heat's restore and stop frame at W - 1,
the gondola's NIS placement after each frame (the next frame's section scan sees it), 128958's fresh riders and 1289F0's grid at G
(their event clocks held in PreRace, core event_clock_hold), the human's hold while the record's +0xAC4 is set and 1297C8(C, 0)'s
grid placement on the first record without it (14538, WS1's last tick), the card skipped, and the Continue at C2. Rules settled:
- **The semi's push-off.** The constructor 11B718 (0x11B748) sets +0x430 = -1 and +0x434 = 0x31; 1218D0 writes +0x434 =
  22E0E0(track byte of +0x430) only while +0x430 != -1. The first heat's riders take location 0 on their first NIS-held tick
  (120F20's 1242B0 re-probe, record 1092); the semi's, placed by 1289F0 without a hold, keep 0x31 to their push-off (records
  14036..14746), and 13C948 halves the auto boost for >= 17. core npc_fresh_rider: +0x430 -1 / +0x434 0x31; npc_grid_start keeps
  +0x434. (Without the re-probe the first heat's riders kept 0x31, the qualifier's finish order flipped and the semi lineup came
  out 8 2 13 9 6 instead of the race copy 0x535B20's 2 8 7 9 6: the 8.096 cm "grid offset" was Psymon on Allegra's row.)
- **The semi lineup** is 23A108's: the qualifier's places 0x536708 (238BF8: ascending 0x536640 = each rider's finish time +0x478,
  written by 23A760), human skipped, then entries 5..7. PS2 c0a-ws13: Allegra 11708 before Psymon 11716.
- **The rows at WS13's enter.** 235AA0 -> 12AB20 -> 129768 -> 1297C8(C, 1): C+8 = 0 and 11D390's event branch for every listed rider
  (ARMSX2 entry probe local/ctm-events/caps/c0a-ws13p460: 1297C8 from 0x1297B0 at tick 12383, then 11D390 six times from 0x129860, each
  with 112180's 11D660 from 0x112250), before the gondola's NIS takes the human: its +0x460 is its start row's point (the painters
  step there under the gondola), +0x454 0, the route reattached (+0x490, +0x4C0..+0x4C8; record 14017). web/event-heat.js heatEnter
  ends with it (core event_row_enter = transport_map_enter without WS14's boost meter clear); compare-ai-capture --ws13 passes the
  location's start rows. +0x460 / +0x454 exact through the gondola since.
- **The grid-wait ranks.** 1289F0's riders are fresh (+0xEC 0) and the human keeps its place (14036: 2,0,0,0,0,0), so 10F998 over the
  equal grid distances gives 5,1,2,3,0,4 (keys -(remaining + 20 x rank), shell sort 0x3E6328). The held human's +0x4D0 stays the
  start row's 353496 (121818 skips 112338 under +0xAC4: ctm-parity.md "Each tick while held"). WS1 arg 3's update, once its NIS list has
  ended (279298 == 0), runs 128958 / 128998, 128A48(C, 0) and, in a race, 128A48(C, 1) (rank mode 1, +0xEC = the list index): ARMSX2
  call-site probe local/ctm-events/caps/c0a-ws13prank, 0x234570 / 0x234594 at tick 222, two frames after 123B48 (tick 220). The
  comparer passes the ranks into racers.start({ ranks }) and calls race_world_rank_mode(1) at that frame (the probe's: the list end is
  the NIS engine's, which it does not run). Ranks exact through the capture since.
- **The semi's route roles** are round 2's (ai-racers.md "Route roles by round": 1 1 1 0 0, not lineups.json's final 2 2 1 1 0):
  Elise's path pick at 479 (path 117) and the riders' later picks.
- **The human's start in the semi** is the normal 12BF68 (phase 1 crouch 1 at GO, the push-off at pose 0.62, tick 202); a hold
  carried past C2 (nis_hold's start clear) had stopped it.
- **The world reset re-hides the Big Challenge markers.** 230180 purges set-piece bucket 1 (355118(S, 1) at 0x2301B8), which holds
  the Hide nodes (350E90 -> 34FB00 with bucket 1, type 16), and then calls 308C60 (0x23027C). 308C60's mode-4 test (0x535C10 == 4)
  gates only its mission build loop. It always ends with 308DB8 (0x308D84), which runs each loaded stage's mission records' +0x4C
  programs again, so the markers' Hide (2FFB50) runs again too. PS2 c0a-ws13.tick0.p2s (the semi's tick 0):
  mdl_ARA1_BigCGate_1001 (599560) and flg_ARA1_BigCFlag_1001 (648456) are type-16 nodes with +8 = 0x210205 (0x20 clear: the body
  collision skips them). core ctm_world_reset now ends with browser_mission_world_reset (web/mission_gameplay.inc): 308DB8 outside
  mode 4; in mode 4 the next WScript tick's mission_build runs it once. web/event-heat.js heatEnter then copies the human context's
  node states into the rider contexts (syncWorldNodes). The rider contexts run no stage programs, so without the copy they kept the
  qualifier's kind-9 replays (266760 at 0x200004, so the PS2's semi-tick-867 Spline fire was missed). With the copy but without
  308DB8, rider 1 hit the gate at 802.
- Score (core-hide; gate ctm-events/c0a-ws13-semi, compare-ai-capture --ws13 --ticks 16100): the qualifier, the grid wait and the
  semi to the capture's end (1500 ticks) are all exact: the human, all five riders, the RNG, the ranks and the pair records. Open: the
  human's +0xAFC holds (the gondola: +0x430 / +0xB00 / +0x370 keep the placement's; the PS2 probes and misses). **Bone 22 (2026-10-01):** under the gondola's hold the PS2 plays semantic 432 on channel 2 (sequence clip 0x5A8E00), then 411 on channel
  3. Both come from the NIS's own bank: 432 has no variant record. The page already poses the cast rider with those clips
  (cutscenes.js), and its board_rootg at the model size is the PS2's bone 22 (fr-booth2: 0.09 cm), so no clip export is needed. pv
  nisBoneProbe (off) feeds it to core nis_hold_probe(2). Open: the cut's start lags the hold on the page (ctm-parity.md "Bone 22 from
  the NIS keys"). The page's WS13 path now plays the gondola in-world (below); its hold does not probe yet (nisBoneProbe off).

**The page's in-world WS13 (pv eventReturnInWorld, 2026-10-01).** The results' Restart of an in-world race and its Next heat
(career-ui.js restartToCard / resultAction(0) -> main.js ui.cb.heatInWorld) run WS13 in the streamed world, not an event load:
- The enter: requireHeatCore, inWorldResultsRestore, web/event-heat.js heatResetWorld (230180: ctm_world_reset in every context, then
  syncWorldNodes), the round's lineup (aiRace.prepare), then heatRows (the anchor RNG off, game_tick_restart(0) in every core, 1297C8(C,
  1)'s event_row_enter at the location's start rows). heatEnter is the same split; the c0a-ws13-semi gate stays exact.
- The gondola: cutscenes.js 'heat' at the event's location (worldEvent.code; without it the list did not play over the streamed world
  and the card opened at once), the held step without holdTicks. The run ticks on under it (heatRunStart: no startRun, the riders idle).
- main.js heatTick, once per game tick before its passes (gameHost.worldAiBefore), from WS13's enter W (PS2 c0a-ws13 record 14017):
  - W+301: 1289F0's grid (PS2 14318): npc_fresh_rider for each rider, racers.start({gridStart, hold, ranks: [human, 0, 0, 0, 0, 0]}).
  - R = W+492: 27A9F0, cutscenes.js releaseFade (the held step plays on under its 30-tick fade-out). WS13's phase 2 sets +0x10 = 3
    and returns (0x235EF4: the branch skips 233AA0), phase 3's 233AA0 runs at R+1, and the record shows WS1 at R+2 (14511). R is the
    capture's: the page models phase 2's 120 ticks after the grid, the NIS idle and the view fade, not 3A9770's page wait (>= 20000).
  - R+29 = W+521: the list's stop (276CC8 -> 276868: 123B48 -> 11D390), as the Transport's release (to-c-fade 1441 -> 1470): the human
    on its grid row (event_route_seed, event_grid_start; PS2 14538). It is counted on the game tick (the cut's clock runs per frame:
    the next step's onStep came 1 to 2 ticks late). A list that ends without a release (a skip) ends it at its step change.
  - W+523: 128A48(C, 1), rank mode 1 (PS2 14540); W+525: WS2's card (14542).
- Checked in Chrome (local/ctm-events/qa/heatqa.mjs, both branches; Next heat with FORCE_WIN=1, a QA hook that scores the qualifier's
  Give Up as a first place): the gondola plays over the ticking world, then grid 301, release 492, list end 521, rank mode 523, card
  525; the heat's Continue runs the countdown and the riders leave the grid. WebKit is owed (the screen was locked).

**The race clock's GO: WS3 selects Race (2026-09-30).** Every human finish on the web came out one race tick short of the
PS2's +0x478: the c0a-ws13 qualifier 11794 vs 11795, the c0a-ret2 Give Up 1519 vs 1520, and the rider contexts 11707 / 11715 vs
11708 / 11716. That is 1/60 s on the results time.
- The rule. WS3's update (cGFGateState 234C68) runs in WS_tick after the rider manager's tick and its total-tick increment. With the
  clock's countdown +0x1C at 0 (and 0x535C10 != 4, 270280(S+0x28) == 0) it requests WS4 at once (233AA0). WS4's enter 234E20
  selects Race (0x234E30 -> 113B10). The next tick's 113C20 (0x128B14) then enters Race and counts race tick 1, so the clock's own
  countdown select (0x113D60) never runs. In a tick, race ticks = total - 179.
- Evidence (ARMSX2 call probes, local/ctm-events/probe_calls.py):
  - c0a-ws13pclk2: 0x234E30 at total 180 with +0x1C 0; the Race handler's 12A250 call (0x113DCC) from total 180; no 0x113D60.
  - c0a-ws13pclk: 113C20 takes +0xC from 0 to 1 at total 180.
  - c0a-ret13: the Give Up's finish (125228's 125368 branch, 0x125390 -> 125108) reads +0xC 1520 at total 1699.
- Port: core race_bridge.cpp gate_go at the end of race_end, after its info is filled: Countdown entered with 0 left, event kind
  != 4 -> select Race. The info still reports the tick's own phase 4, so the page's GO HUD / sounds keep the next tick's 4 -> 5
  (the monster-* audio gates: the GO sounds at the same tick). A first try that selected before the info moved them one tick
  early (monster-yellowcard / deepsky / swollen / xexec failed at 179).
- Not modelled: 270280, the streamer state *(S+0x28). Unconfirmed whether a countdown can end while it reports busy.
- Results: c0a-ret2's finish ticks are 1520, as the PS2's. peak3/kick-doubt-event-tuck (a timed event: 125228's limit reads the
  race ticks) went from exact through 3781 to exact to the end.
- +0x470 itself was already right: the PS2's 1/60 at record 3033 is the web's from tick 1700 on.

**Carried presses (pv padCarry, 2026-09-30).** Two rules from the code:
- **One pad history.** cSSXApp_preUpdate (0x227E98; the jal at 0x227F20) runs 0x321298 on each port's history once per app update
  with a pending sample (326B48), whatever runs: the front end, a card, the pause menu, the results. The game's provider reads the
  same history, so a button pressed in a menu and still held is held on the ride with no new press edge. core pad_history_sample
  (the history only); the page feeds every 60 Hz frame the game does not tick (main.js padCarryFeed) and keeps the held keys and the
  history through the menus and a run's start (a replay starts settled).
- **1162C8's +0x360 latch** (131620 at 0x131794 with held = bit 15, pressed = bit 14): a press, or a held jump while +0x360 is 0,
  requests control 2; otherwise +0x360 = 1. Control 0's entry 131608 clears it (also 12FC60, 131D08). With no request and the jump
  held, 131620 runs its cruise targets with crouch 1 and brake 0 (0x1317B4). core input_bridge.inc (prewind.jumpGate; cleared on
  control 0's entry, reset_pad_history, reset_rider, place_rider_region).
- PS2 c0a-ret2: the Give Up's Yes Cross (menu samples 354..362) reaches tick 1699 as word0 0x8000 (no 0x4000): crouch target 1,
  control 0, then the finish's control 10. Gate ctm-events/c0a-ret2-coast (--coast-only --coast-device --pad-carry): human, riders,
  RNG and ranks exact through all 1987 race ticks (without --pad-carry the human prewinds at 1700 and jumps out of control 10).
- Page check (Chrome --mute-audio and WebKit, ?mute=1, keyboard and a scripted standard gamepad, padCarry on / off): a jump held
  through pause -> resume stays crouched with no jump and jumps on release (off, keyboard: a jump on resume); a steer held through
  pause applies on the first resumed tick (off, keyboard: lost until re-pressed); a Cross tap on a restart gives no jump at GO
  (the start's own phase-1 crouch, as off). The gamepad path behaved already with padCarry off (its history was never settled
  mid-run); the menus' own 0x321298 model (web/gamepad-menus.js) is separate and unchanged.

### Rollout checks (2026-10-01, CTM events-in-world agent)

Scripts: local/ctm-events/qa/fxpix.mjs (frames), memevent.mjs (memory; run through local/peak-splines-qa/run.mjs), bcprobe.mjs.

**Trails / icons / beam, in-world vs event-load.** Chrome --mute-audio and WebKit (webkit-driver), ?mute=1. Both paths run the same
flow: a new career, Snow Jam, the gate, the card, then race ticks 300 / 600 / 900 on a frozen frame clock with a neutral pad. At each
tick: a full frame and an FX-only frame, with the HUD off.
- The rendering is the same. The world, lighting, HUD, snow and opponent FX look alike, and the FX probe counts match: five trails,
  wakes and sprays, plus the icons. In WebKit at tick 300 the frames differ on 8 % of pixels (FX-only: 5 %); in Chrome on 24 % (12 %).
- The differences come from where the riders are, which is by design. The event-load path holds its riders to the Single Event anchor
  tick (doc.anchor_tick). The in-world riders ride from the countdown's tick 0, as the PS2's CTM race does (c0a-full-ai, exact). At
  tick 300 the human and riders 2 and 5 are at the same positions; riders 1, 3 and 4 are not. From 600 on, the human differs too
  (a rider-pair contact).
- Each path is deterministic across runs and across browsers: Chrome and WebKit give the same rider positions. The WebKit shots are
  1920x1080 and Chrome's 960x720, so the two browsers are not compared pixel to pixel.
- The backcountry beam: there is no in-world backcountry event on the page. A career Transport to a rival event switches course
  (career-ui.js transport -> cb.course). rideIntoEvent, the only in-world entry, is the CTM gate's. So the beam stays on the event-load
  path, and there is nothing to compare.

**Memory, phone policy** (WebKit 844x390, quality=low, __XPC_JSC_forceRAMSize 6 GB, the WebContent footprint outside the page). One
cycle: the gate, the qualifier (25 s), Give Up, the results, the results' Restart (WS13), the heat, Give Up, the results, Transport,
the map, Snow Jam.
- In-world, 3 cycles: the cycles peak at 1111 MB, with medians 916 -> 958 -> 980; the lifetime peak is 1323 (at boot). The wasm heap
  goes 184 -> 221 MB in cycle 3. No world loads (3 course loads, all at boot).
- Event-load, 3 cycles: the cycles peak at 1400 MB (two worlds at the gate); the lifetime peak is 1477. Two course loads per cycle.
- In-world, 8 cycles (another run, with a higher baseline): the medians rise 1180 -> 1430 MB, about +35 MB per cycle, and the
  lifetime peak is 1540. The wasm heap stays flat after cycle 4 (221). So the growth is outside the wasm heap (JS heap or graphics),
  not flat. This is R8, and it fails the phone gate. Desktop is not affected at this size.

**In-world WS13 on the page is not the PS2's yet.**
- The results' Restart and Next heat call ui.cb.cutscene({kind: 'heat'}). In a free-ride course that kind is not in the playable
  list (main.js cb.cutscene), so the gondola never plays: the card opens at once, over the rider where it gave up.
- event-heat.js heatEnter (the start rows, 1297C8(C, 1), the rider sync) runs only in the comparer.
- The event-load path plays the gondola.

### Turning it on

1. Stage 1 as soon as its gates pass (it needs pv `pauseContexts`).
2. Stages 3 + 4 + 5 together on the desktop tier.
3. Phones once the stage-4 memory runs pass. The per-peak worlds (PEAK1..3) are enough, since an event needs only its course's row.
   `peakRelease` helps but is not required.
4. Keep the event-package CTM path for one release as the fallback. Then drop `rideIntoEvent`'s switch branch and `goWorld`'s
   same-world reload.

## 5. Risks and unknowns

| # | unknown / risk | how to settle it |
|---|---|---|
| R1 | Does a CTM WS3 state equal a Single Event WS3 state? | **Settled (§6.3, §6.4).** The world is equal: the octree, the patch order and the instance sets. The differences are the RNG, the game-type content (collectibles, Big Challenge markers, the TRANSP in-air models), the human's carried free-ride state, and, in the first heat only, free ride's DeadNodes. The event package replays a CTM countdown exactly for 180 ticks and diverges at the push-off (182). Still open: which carried word moves the push-off (poke them one at a time into c0a-cd and rerun c0a-race). |
| R2 | Does the human keep riding under the fly-over (WS1, PreRace), and what feeds its controller? | **Settled (§6.2).** It rides in its own control for the gate tick + 31 (the fade). From the fly-over's first tick it is held by the NIS actor (control 13, +0xAC4 = 1), and it is placed on the grid (control 6) at WS1's last tick. Same on ARA1, BRA2 and ASS1. |
| R3 | Do computer riders tick or draw RNG between 129E20 and the grid placement? | **Settled (§6.2).** All the AI riders are created at once, 2 ticks after the gate (C+0x78 1 → 6; 3 on ASS1). Under the hold the shared RNG changes only 3 times on ARA1 / BRA2 (the NIS step starts) and 14 times on ASS1; while the human rides it changes every tick. The per-rider load step (11D640 / 11C298) was not watched, so its timing is unconfirmed; no list was held (WS1 lasted 542-546 ticks on all three). |
| R4 | Are the AIs removed at WS15 (12AB20 / 12AC48 are virtual calls)? | **Settled for timing (§6.5).** The 5 AIs are still there at WS15 and at WS1 arg 0 (C+0x78 = 6) and gone at WS4 (1), which is about 10 ticks later. 12AB20 walks the players list (C+0x48, count +0x80), not the AIs. The removing call is unconfirmed (candidate: 12AF68 cAI_purgeMissionRiders from 128958, or WS1's free-ride branch). |
| R5 | Can rows change during an event (builtin 68 Unload / Load in event kinds)? | **Partly settled.** Builtin 68's dispatch (0x302344, table 0x489790) has no event-kind gate: its only test is the rider check at 0x302330. So a connector volume streams in an event too. Whether a race can reach one (a rider coasting past a finish) is unconfirmed; settle it with a capture riding past Snow Jam's finish towards ARA1_B. |
| R6 | What 230180 / 2300F0 / 3A6800 reset | **Settled (§6.4, §6.8).** 2300F0 runs only online ([0x534B30], 0x234B14). 230180's bucket-8 purge (0x2301C4) destroys the DeadNodes (destructor 0x360A28, probe: 47 calls from 0x2301CC). The activation list is emptied by the rider manager's restart 129768 (C vt+0xCC+0x28(3)) at WS2's exit (the card's Continue) and inside 230180. 3A6800 rewrites every loaded instance's flags as `(flags & 0xFFFF0000) \| (flags >> 16) \| 2` (the port's `stage_load_flags` rule). |
| R7 | The appended collision in five rider contexts: memory, time per context under the fly-over (WebKit), exactness | Stage 4 memory runs. An appended-parse-cache equality test. Time the per-context feed at the 4x phone profile, with a budget per frame from `free-ride.js buildBudget`. |
| R8 | A rider context cannot be destroyed; reuse may leak | `rider_context_reset` plus a 10-cycle heap check (the wasm heap high-water mark and the JS heap after GC). |
| R9 | The replay in a streamed world (`replayTriggersCore`, the snapshot at WS3, camera triggers per course) | Read replay.js / `replayTriggersCore`. Test a Replay after an in-world event in Chrome. Until then, disable the Replay item under the switch. |
| R10 | Per-course settings: an event at BRA2 / CRA3 / DRA4 / ERA5 needs its own `original_race_event` and reset / ground seeds, and the streamed world runs on the base course's initial.json | Stage 2 `init_race_event` + `event_course_seed`, with a tick-0 rider-state equality test per event course (17). |
| R11 | Visual gaps in the streamed world that events expose: the rider local lights are Snow Jam's; the start-gate GO LiveComps; the crowd (CROWD_COURSES exists) | Visual-parity side-by-sides at the card and at GO against c0a / c0b frames. The local-lights split per location (tools/export_peak_world.py) is a prerequisite for events outside Snow Jam. |
| R12 | pv `pauseContexts` is not on, and `phases.advance` is never called | Stage 1 waits for the pause-context agent. Stage 1's changes are expressed as "who pushes what" and need no flag of their own beyond `worldUnderCuts`. |
| R13 | Phones: per-peak worlds hold every fed location's collision (a peak ≈ 154 MB of wasm) plus the rider contexts | Stage 4 memory runs on the phone tier with and without pv peakRelease. If it is over, the phone tier keeps the package path until peakRelease ships. |
| R14 | The fly-over's frame times in WebKit while five rider models compile | The fe-preview compile spread (pv feCompileSpread) and the yield-shim budget. Measure `webkit-driver.mjs` frame p95 under the fly-over. |
| R15 | The kind switch leaves stale core state keyed on the free-ride kind (the stage load flags, the finish-fence object routes, builtin 108, `mission_lifecycle`) | Stage 3: a test that switches 4 → 0 → 4 in a PEAK1 core and compares every exported world / stage hash with a core that started in that kind (`world_instance_states`, the stage world hashes). |

| R16 | Stage 4: the CTM collectibles' MagnetModifier pickups live in the human's context only; the riders' contexts keep the event load's DeadNode for them | Trace 105398's contact selection against the magnet gate 0x357660 (and 0x357538's filter-1 answer) for a computer rider's slot: can a refused magnet contact still win the selection over another contact in the same tick? If it can, the contexts need the magnet boxes (per tick) |

Other agents' areas this design touches:
- the pause-context / screen-phase refactor (stage 1);
- the whole-mountain memory agent (pv peakRelease, R13);
- the CTM screens agent (the "Loading..." reasons, rank 7);
- the scripted free-ride agent (the Big Challenge end at the gate, rank 9: `browser_mission_gate` is already in the core).

## 6. Stage 0 results (2026-09-29)

All runs were silent ARMSX2 on derived states; no original was touched. The runs and states are in `local/ctm-events/caps/`, and the
tools in `local/ctm-events/`:
- `gate_pilot.py`: ps2_autopilot steered at one trigger volume;
- `gatesum.py` / `readcap.py`: record summaries;
- `memdiff.py` / `memdiff_detail.py`: savestate diffs by resource name.

Every record also carries windows at:
- C 0x5B0700 (+0x100);
- S+0x200;
- 0x535C08;
- the activation object +0xD0 (C+0xA4 = 0x1454AD0).

G, S, C and that object sit at the same addresses in every CTM state used here.

### 6.1 What was run

| run | base → what | output |
|---|---|---|
| c0a-gate | `ctm-parity/states/sj.p2s` (Snow Jam free ride, tick 2836) → the pilot steered into `mdl_ARA1_RaceRideState_0` → WS1 → the card | 1151 records; states at WS1 and at the card (`c0a-gate.tick*.p2s`, hooked) |
| c0a-cd | `ctm-parity/states/sj-card-q.p2s` (the first-heat card from an earlier ride-in) → Cross | a clean countdown state `c0a-cd/countdown.p2s` (WS3, tick 1) |
| c0a-race | c0a-cd countdown → `scripts/setpieces-full.json`, `--ai-state` | 1400 records: countdown + GO + 1220, all six riders. The first CTM six-rider capture; its comparison needs a career lineup document (`tools/export_lineups.py export-career`) for compare-ai-capture.mjs. |
| c0b-bra2, c0b-ass1 | `menus/fr-courses/{bra2,ass1}-screen10.p2s` (the Transport arrival) → the pilot into the course's RaceRideState → the card | 876 / 911 records; states at WS4, WS1 and the card |
| c0c-semi | `ctm-parity/runs/race-q/res.p2s` (the qualifier results) → Next heat → the semi card → Cross (ps2_navigate, RIDER_TRACE) | clean states `semi-ws13`, `semi-card`, `semi-countdown`; the human per polled tick |
| c0d-ret | `ctm-parity/fresh/runs/a/confirm.p2s` (the post-event map, Snow Jam picked) → Yes → WS15 → WS4 + 590 ticks (RIDER_TRACE) | clean states `ret-ws15`, `ret-ws1`, `ret-ws4`, `ret-ride`, `final` |

### 6.2 The gate and WS1 (R2, R3)

| | ARA1 (race) | BRA2 (race) | ASS1 (slope style) |
|---|---|---|---|
| gate: WS 4 → 1, event type set, C+0xC (race clock) 0 | 3440 | 2383 | 2448 (type 1) |
| AI riders created (C+0x78) | +2 ticks: 1 → 6 | +2: 1 → 6 | +2: 1 → 3 |
| human in its own control (control 0, +0xAC4 0) | +0..+30 | +0..+30 | +0..+30 |
| NIS hold (control 13, +0xAC4 1) | from +31 | +31 | +31 |
| grid placement (control 6, at the grid slot), then the card | +546 | +542 | +542 |
| shared-RNG changes under the hold | 3 (+31, +39, +347) | 3 | 14 |

- The rider pipeline runs every tick of WS1: one record per tick, no gaps. The world ticks under the fly-over, as §1.1 says.
- The first-heat card's grid positions equal the Single Event anchor's to the bit, for all six riders.
- At every CTM card, the activation list is already empty (count 0, cursor +0xD0 = −1).
  - +0xD0 is the last scan's tick. −1 forces a rescan.
  - NIS step starts set −1 without emptying the list.

### 6.3 The first heat's countdown against Single Event (R1)

`memdiff.py c0a-cd/countdown.p2s snow-jam-countdown-anchor.p2s` (CTM tick 1 against the anchor's tick 18):

| part | result |
|---|---|
| resident rows | equal: ARA1, A_ARA1, ARA1_B, ASKY, TRANSP |
| octree | 2203 nodes on both sides, the same structure. Every node's patch list is in the same order. The instance lists differ only in 4 nodes, where the moving trams / rockets stand in other cells. |
| instance runtime flags (the 0x300 draw-list parity masked) | 37 differ, all game-type content (0x535C11 0 against 1) or free-ride carry-over (the next rows) |
| - collectibles `mdl_ARA1_collecta_*` | CTM: 0x210023, no entity; Single Event: type-6 entities. The port's event package gives a CTM race Single Event's pickups. |
| - Big Challenge markers, the TRANSP in-air heli / gondola | type-16 entities in CTM only |
| - `mode_fence{b,buv}_start_1001 / 1002` | **DeadNodes (0x491B00) in the CTM first heat**, as in free ride; drawn static (0x10003) in Single Event and in every later CTM heat |
| - `mdl_ARA1_RaceRideState_0` | hidden (type 16) in the CTM first heat, restored in later heats |
| activation list | 147 against 146 entries |
| the human rider | the grid position, heading and zero velocity are equal. About 170 non-pointer words differ, all zero in the fresh Single Event rider. |
| - which words | +0x248, the normals +0x380..+0x398, +0x3C0, +0x3F8 (−103.1), the route cache +0x4DC..+0x4E8, +0x5B4, +0x764, +0x770 (691.4), +0x7C0..+0x858 (secondary motion), +0x9E0.. / +0xA20.. / +0xA5C.. (the last free-ride positions and contacts near the gate) |
| RNG 0x4FF030 / 0x4FF018 | different |

**Replay on today's event package** (`compare-ps2-capture.mjs c0a-race.bin --pad --sync-rng --zoe --event`):
- exact through tick 181, the whole countdown with the rider in control 6;
- first inexact at **182**, the push-off: 2.77 cm, 161 cm/s. The comparer's seed differs from record 0 in +0x248 and +0x380..+0x398.

So the event package, which is seeded from Single Event, cannot reproduce a CTM start.

The later heats (`semi-countdown`, and the career final `ARA1-final-zoe/countdown.p2s`) match Single Event in the fences and the gate
volume. Their remaining differences are the collectibles, the markers and TRANSP. The final also carries the human's boost meter
+0x2F8 = 0.5878 from the semi: WS13 has no cGame_restart, so the +0x2F8 clear does not run.

### 6.4 The resets (R6)

- **3A6800 cWorld_resetMap** walks every loaded section's instance records (0x58-byte track records, count +0x26). For each instance it
  sets `flags = (flags & 0xFFFF0000) | (flags >> 16) | 2`. That is the load-flags rule the port already has (`stage_load_flags`, pv
  loadFlags).
- **Nothing resets the first heat's world offline** (2300F0 is online only, §6.8). The first-heat countdown therefore still has free ride's
  DeadNodes (the start fences) and the hidden gate volume.
- **230180 (WS13 / WS15) removes them**: its purge of set-piece bucket 8 (355118(0x4A5988, 8) at 0x2301C4) calls the DeadNode destructor
  0x360A28 (vtable 0x491B00 slot +0xC) for each; a probe on a poked WS13 logged 47 such calls, all from return 0x2301CC. `semi-countdown`
  has the start fences back because nothing re-kills them near the grid (their slot-1 program 485 kills them in kinds 0..4, but only
  when their section is activated).
- For the port: no reset at an offline event start; `ctm_world_reset` (230180) purges bucket 8 (the DeadNodes) and bucket 1.

### 6.5 Next heat and the return (#10, #3, R4)

**WS13 (Next heat):**
- It lasted 512 frames. The game tick restarted at 0 and ran 201 ticks in them, with the human held (+0xAC4 = 1, placed in the TRANSP
  gondola interior). The rest of the frames the tick stood still (NIS / disc waits, unconfirmed which).
- WS1 arg 3 then took 32 ticks, and the human was placed on the grid.

**WS15 (the return), `c0d-ret`:**
- The confirm frame goes WS14 → WS15.
- On the next sample the human is at Snow Jam session point 1 (−111389, 13422, −226413), moving (−787.6, 197.2, 0): 11DF18's z = 0.
  The game tick is 0.
- WS1 (arg 0) then runs 7 ticks (2..8), with the human riding on (control 0, +0xAC4 0), and WS2 comes at tick 0 again. WS3, then WS4 at
  tick 1: 11 samples from the confirm in all.
- C+0x78 is 6 at WS15 and WS1, and 1 at WS4, so the AI riders go within those ticks.
- The activation list is kept through WS15 (75 entries). The rescan under kind 4 refills it (87 at WS4, 98 once riding).
- No load at any point. The PS2's return is exactly stage 5's in-place path.

### 6.6 Consequences for the plan

- **Stage 1:** the human under WS1 is held from the gate tick + 31 (not "riding on"). WS13 ticks the world with the human held.
  Both are the existing `nis_hold` shape.
- **Stage 3:**
  - At the card's Continue: `ctm_race_restart` = 129768 (empty the activation list, rescan under the event kind, zero the race
    clock). No world reset offline: DeadNodes, set pieces and missions carry over from free ride.
  - The first heat keeps free ride's fences.
  - The human's carried state must come from the streamed ride itself. That is only possible in the streamed core, and it needs the
    WS1 hold (control 13 from +31) and the 11D390 grid placement to be exact. A new gate `ctm/ara1-gate` would replay c0a-gate
    (free ride → gate → hold) and then c0a-race.
- **Stage 4:** c0a-race is the first six-rider CTM capture. It needs a lineup document of its roster (export_lineups export-career)
  before compare-ai-capture.mjs can gate it.
- **Stage 5:** WS15 is 230180 + session point 1 + 11DF18. The riders go within about 10 ticks, and the human rides through the
  white fade.

### 6.7 The stage gates, ready before the stages (2026-09-29, tools-only prep)

**Career lineups.** Two more derived career countdowns sit beside `ARA1-final-zoe`, each with a README, and both are exported:
- `local/reference/pcsx2/characters/career/ARA1-qual-zoe/countdown.p2s`, copied from c0a-cd;
- `.../ARA1-semi-zoe/countdown.p2s`, copied from c0c-semi.

`tools/export_lineups.py export-career --course ARA1` writes `local/assets/native/ARA1/lineups-career/ARA1-{qual,semi}-zoe.json`. The
final's document is byte-identical to before. `build` was not run; if it is, it will also see these two states.

**Six-rider comparer.** `compare-ai-capture.mjs --document DOC` already took a lineup document. The career semi exposed a mapping bug:
- tools/ps2_capture.py writes the computer riders' record blocks in actor-address order (`manifest.others`, sorted);
- a document lists the riders in roster order (C+0x28);
- WS13's 128958 allocates the semi's riders out of address order, so every rider was compared with another rider's grid spot
  (146-733 cm on tick 2).

The fix:
- `ps2-capture-ai.mjs rosterOrder(manifest)` maps roster slots to record blocks;
- `readAiCapture` returns `ai`, `others`, `owners` and the RNG attribution slots in roster order;
- compare-ai-capture's own `others` read uses the same map.

It changes nothing for the 62 existing --ai-state captures: all have the identity order, checked. Re-run and still exact to the end:
event-race-ai, ko-attack, parity-ai/metro-race.

**c0c-race**: the semi-final from its countdown, `--ai-state`, 1400 records.

**Registered in `web/test-ps2-captures.mjs`** as `pending: SWITCH` cases. They are skipped unless `PENDING=1` or `ONLY` names them. When
run, they are scored and printed, never failed, next to `today` (the current result).

| case | stage (pv) | comparer today | today | target |
|---|---|---|---|---|
| `ctm-events/c0a-race` | eventInWorld | `--zoe --event` (the ARA1 package) | exact through 181 (the push-off leaves at 182) | the end, on the in-world path |
| `ctm-events/c0c-race` | eventReturnInWorld | `--zoe --event` | 181 | the end |
| `ctm-events/c0b-ass1-arr` | eventInWorld | `--course PEAK1 --peak-arrival` | 2478: the arrival, the free ride, **the gate tick 2448 and its 30 riding ticks exact**; leaves at the NIS hold (2479) | the end (the grid placement at 2990) |
| `ctm-events/c0b-bra2-arr` | eventInWorld | the same | 2057: 0.09 cm/s off from the placement tick with the pilot's pad (open; peak1-arrive-bra2 with a neutral pad is exact) | the end |
| `ctm-events/c0a-gate-arr` (replaces the unscored c0a-gate, §6.8) | eventInWorld | `--course PEAK1 --peak-arrival` | 2852: the Snow Jam arrival, the free ride, the gate tick 2822 and its 30 riding ticks exact; leaves at the NIS hold (2853) | the end |
| `ctm-events/c0a-race` (six riders) | eventInWorldAi | `--document .../ARA1-qual-zoe.json` | human 181; riders 722 / 309 / 723 / 508 / 479; RNG 298 | the end |
| `ctm-events/c0c-race` (six riders) | eventReturnInWorld | `--document .../ARA1-semi-zoe.json` | human 181; riders 721 / 318 / 683 / 516 / 1344; RNG 298 | the end |

- The c0b captures were recaptured for this (`local/ctm-events/gate-arrival.sh`). They use the peak1-arrive-* watch set that the
  PEAK1 comparer needs, and the pilot's consumed pads (`ps2_autopilot.py pads`) replace the manifest's segments.
- All the captures are linked from `local/ps2-capture/runs/ctm-events/` into `local/ctm-events/caps/`.
- The computer riders in the career runs print "the aged fresh table does not give the document levels": the career relationship
  levels are not modelled from the document, so in-race relationship changes are off in these compares.
- The WS15 return (c0d) and the WS1 of c0a are navigate runs (no ps2_capture records). Their gates come with stage 5 / stage 3, from
  captures started at `ret-ws15.p2s` and at a PEAK1 seed.

### 6.8 R6 settled, the Snow Jam gate scored, the Metro arrival difference (2026-09-29)

**2300F0 runs only online.**
- WS3 enter tests `[0x534B30]` (the online flag, [ctm-decomp-screens.md](ctm-decomp-screens.md)) at 0x234B14..0x234B24 and calls 2300F0
  only when it is set.
- It is 0 in every offline state read here: free ride, CTM cards and countdowns, the Single Event anchor, WS15.
- A probe (`tools/ps2_entry_probe.py`, hooks 2300F0 / 103358 / builtin 2's 0x2FC420) confirms it. The first-heat card had a poked
  WS3 request (`local/ctm-events/caps/r6-ws3-poke.p2s`); 2300F0 was never entered.
- The correction is applied through this doc: §1.1, §4 stage 3, §6.4 and §6.6 no longer say "2300F0 at every countdown".

**What does reset an offline event:**
- **At the card's Continue (WS2 exit, 236CD8 → C vt+0xCC+0x28(3) at 0x236D28) the rider manager's restart 129768 runs:**
  - 103358(C+0xA4) empties the activation list (probe: ra 0x129788, chain 0x236D30 / 0x236CCC / 0x2310EC);
  - 101688(C+0xA8);
  - C+0x98 = 0, C+0 = 0 (the phase);
  - 1297C8(C, 1): the grid placement and the race clock C+0xC = 0.

  The rescan under the event kind follows on the next section pass. 230180 makes the same vt call.
- **230180 destroys the DeadNodes.** Probe on `r6-ws13-poke.p2s` (the first-heat countdown with WS13 poked into S+0x210):
  - 47 calls of the DeadNode destructor 0x360A28 (vtable 0x491B00, slot +0xC), all from 355118 (return 0x35517C);
  - 355118 is called at 0x2301C4 with bucket 8 (return 0x2301CC).

  So set-piece bucket 8 holds the DeadNodes and bucket 1 the other entities.
- **Nothing resets a first heat.** Its world is free ride's, with the DeadNodes free ride made. Only the activation rescan runs at the
  Continue.

**The Snow Jam gate, scored.**
- The Snow Jam Transport confirm `menus/ctm/state-transport-confirm.p2s` still held ps2_menu_capture's hook at 0x321298
  (`j 0x96000`), so no pad reached the menu. `local/ctm-events/caps/sj-confirm.clean.p2s` restores the original bytes (0x27BDFFC0
  0x7FB00030) and zeroes 0x90000..0x100000.
- From it, ps2_navigate Yes gives the Transport, then `sj-transport/ara1-screen10.p2s` (WS10 at tick 2039, course 0).
- `c0a-gate-arr`: ps2_capture from that state, open loop (314 neutral + 20 left, as ctm-parity/fresh), watches as the peak1-arrive
  gates plus the world-state / rider-manager / activation windows.
  - WS1 at 2822;
  - 6 riders at 2824;
  - the hold at 2853 (the same +2 / +31 as §6.2).
- `compare-ps2-capture.mjs --course PEAK1 --peak-arrival`: exact 787 ticks, from the placement through 2852 (the gate and its 30
  riding ticks). It leaves at 2853, where the PS2 hands the rider to the NIS. That is the stage-3 gate, `ctm-events/c0a-gate-arr` in
  test-ps2-captures.
- A "gate-aware" comparer mode means the comparer taking the page's WS1 path at the recorded gate tick. It needs stage 3's core entry
  (`ctm_gate` / `nis_hold` at the fly-over's first tick), so it comes with stage 3.

**The Metro-City arrival's 0.09 cm/s: comparer seeds, settled by the physics agent (2026-09-29).** My reading above was wrong and is
withdrawn.
- A record is taken at the provider exit 0x128630, after 11B3F8, so record k holds the limit that tick k uses. The original P−1 seed
  of +0x2E4 was right, and my switch to P was reverted by the physics agent.
- With P−1, +0x2E4 is exact on both tuck captures (`caps/bra2-tuck-{late,early}`).
- The 0.09 cm/s came from the missing route-heading seed +0x4CC.
- The split at 2227 came from the rider manager's game tick (+8) being seeded wrong (2773 against 2057).
- The physics agent fixes compare-ps2-capture.mjs (P−1, plus seeds for +0x4CC, +0x434 and the game tick) with a new core export,
  `arrival_carry_seed`. The pending ctm-events arrival cases are re-scored once that core is live.
