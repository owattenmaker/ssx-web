# The race replay (behind the post-race screens, and the results' Replay item)

The PS2 shows the run again behind every post-race screen (results, records, rewards, standings), and its results menu has
a Replay item. The browser does both: `web/replay.js` (recording, playback), `web/replay-ui.js` (the full replay's overlay),
`web/replay_camera.inc` (the replay cameras), behind the `web/pv-flags.js` switch `replay`. SLUS_207.72, gp = 0x4A30F0;
G = `*(gp-0x848)`, session = G+0x84, race = session+0xC (+0 phase, +8 total ticks, +0xC race ticks), R = `*(session+0x28)`
(the replay manager, cReplay).

## 1. What the PS2 does

Ground truth: ARMSX2 runs from derived savestates (`local/ps2-capture/menus/replay/`, probe script in the session scratchpad
`replay/probe.py`: it polls race/replay/world-state words over PINE, logs the SFX voice starts of tools/ps2_audio_log.py and
saves screenshots), plus a static read of 0x26C458..0x2721D0 (cReplay), the camera director and cOVState_REPLAY.

**It re-runs the game.** When the start gate opens (0x234AD0: 0x26F228, 0x26F7B8) the replay manager snapshots the world
(0x26D818: the shared RNG 0x4FF030, the object ids, BE, the session's state objects, every rider's controller (two pointer hops, rider +0x77C -> C, C+0xDE8 -> its vtable; see below), the render
state manager, avalanche, the world-script manager, move nodes with their modifier blocks and splines, the dynamic and dead
buckets) and then records each human's 8-byte controller command per tick (0x127998 -> 0x12863C -> 0x26D178: RLE, 4096 runs
per human in a 0x8000-byte cache; when a cache fills, R+0x610 = 1 and the replay ends there). A replay restores the snapshot
(`cReplay_restoreFrame` 0x26DBF0) and runs the normal 60 Hz game with the recorded commands in place of the pad; the computer
riders simply run again. The visual random stream 0x4FF018 is not in the snapshot (its flags / crowd / splash differ from the
run's). Snapshots are also written every 60 ticks and kept only for the start, the end and up to 3 highlight buckets per human
(take-off 0x114298 -> 0x2707E0, kept by a landing 0x10E910 -> 0x270870 worth >= 1000 points or >= 5 s of air, or a crash
0x12CA30 -> 0x2708F0; a forced reset drops it).

**Behind the results (auto replay).** State 5 (EndRace, 0x233C50 -> 0x26FA50) writes the end frame: the post-finish coast is
not recorded. State 7's enter (0x236DA0 -> 0x20A8F8(7) -> 0x2706B8) starts it: R state 9, then 1 with R+0x61C = 1 (loop), after
the podium when there is one. It plays from the first countdown tick (race phase 4, total ticks back to 1) to the finish tick at
1x, then loops back to tick 1 with a camera cut (0x2703F0 -> 0x1620D0). The race HUD, rumble, the pause menu, score / reward
popups and anger effects are off (0x1E9A30 returns while R state is 1..9); the pad is ignored.
- `menus/replay/gu-loop.*`: The Junction pipe, Give Up at race tick 699 (total 879): the results stay up and the run replays
  from total tick 1 to 879, then from 1 again (loops at pad samples ~491, 1371, 2250: 880 ticks apart).
- `ctm-parity/sheet-race-f1/2.png`: the Snow Jam final: FINISH (t14369), finishov, podium (W12), then the Rewards panel over
  the replay from t1 (W3 countdown, W4 race).
- What stops it (`stopAutoReplay` 0x2706F0 restores the results-time state): Next heat, Transport, Next event, Replay, the
  confirmed Restart / Quit popups, leaving the game. Records and an open confirm popup do not.
- **What 0x2706F0 restores** (read from the code, 2026-09-30; docs/ctm-events-in-world.md stage 5). The results' Transport is overlay command 1
  with item gp-0x9F0 = 2 (0x20CFD0 -> table 0x471AC0 -> 0x20CF80): 0x20CF88 calls 0x2706F0, then command 4 (WS14 arg 2) follows one frame later.
  0x2706F0 (auto mode, R+0x61C != 0) -> 0x26F980 -> 0x26F850 -> 0x26DB88 -> `cReplay_restoreFrame` 0x26DBF0 on R+0x3D0, the **results-time
  snapshot** taken at the replay's start (state 9: 0x26F8A0 -> 0x26F7F8 -> 0x26D818, only while R+0x3D0 is empty), then 0x26EEA0 (camera
  triggers), R state 13 (0x270718). Nothing is restored when R+0x608 != 0 (set only by 0x271758), R+0x3D0 is 0, or R state is 14 / 15.
  The stop runs inside 0x20CFD0 (session update 0x2306A8 at 0x230864), before that frame's rider manager tick (0x128AF0 via 0x230CB0), so
  the frame runs one normal tick on the restored state. What the snapshot holds (save 0x26D818 / restore 0x26DBF0): the shared RNG 0x4FF030
  (0x3178E0 / 0x317908), the object id counters gp+0x2A88 / +0x2A8C, BE (0x14DF08 / 0x14DFA8), the session (vtable 0x47D110 +0x44 / +0x4C),
  the rider manager (vtable 0x458488 +0x3C 0x12B7F0 / +0x44 0x12B948: race +0x00..+0x28 with the total tick +8, the rider lists +0x40 / +0x48
  / +0x5C, each controller C = rider +0x77C through C+0xDE8's vtable +0x14 / +0x1C: human 0x458338 -> 0x111AC0 / 0x111D98, computer
  0x4585F0 -> 0x10A898 / 0x10A8E8 plus C+0xDF0..+0xF40), 0x2D9CB0 / 0x2D9D68, gp+0xCE8 (0x30BB10 / 0x30BD20), gp-0x6F0 (0x229E20 / 0x229E58),
  gp+0xF00 (0x3441A8 / 0x344240), gp+0xF38 (0x357CA8 / 0x357D28), the kind-1 and kind-8 objects of gp+0x2898 (0x26D988 / 0x26DDC0: deleted
  and rebuilt through the factory 0x4816C0; 0x26DA88 / 0x26E340), race+0xA4 (0x103480 / 0x103578), then 0x12B788 (0x120E50 per rider) and
  0x22E840. Per rider (0x111AC0 / 0x111D98): a raw copy of rider +0x000..+0x6C0 (position, speed limit, normals, route heading all in it),
  the sub-objects rider +0x780 / +0x784 / +0x788 / +0x790, +0x864 / +0x868, C+0xDE0 / +0xDE4 and C's members C+0x20 .. +0xD20, rider
  +0xAB4..+0xAC0; the restore also clears C+0x3B0 .. +0xC70 and re-poses (0x312598, 0x3103F0, 0x106828). A capture that pokes command 4
  directly (as c0a-ret did) skips 0x2706F0 and leaves the replay's frame in place.

**The Replay item** (0x20CE0C -> dialog 0x11 cOVState_REPLAY '64replay' -> 0x26F8A0(R, 0, 0, 0)) opens a full-screen replay
that starts **paused on its first frame** (R state 7 -> 3; `menus/replay/rmenu1.f00075.png`), plays to the finish and pauses
there (`rmenu2`: tick 879, state 3; no loop). Controls (0x20DF38 / 0x26FB88, INPUT.MAP `Replay*`):
| Pad | Action |
|---|---|
| Cross | Play / Pause |
| Circle | step one tick (or pause); held, after 10 ticks, one step every other tick (slow motion) |
| R1 / L1 | only while paused: the next / previous kept snapshot (start, highlight buckets, end); the Snow Jam final: 1 -> 6301 -> 11821 -> 14310 (end), back 11821, 6301, 1 (`menus/replay/af1`) |
| Triangle | cycle the camera (9, below) |
| Square | hide / show the timeline bar |
| D-pad down / up | slide the help panel off / back (4 px a frame between y 245 and 480) |
| Start | pause and open the Replay Menu: Save replay (memory card), **Exit replay**, Continue; Triangle closes it |
| left stick / right stick x | the Manual camera's orbit / zoom |
Exit replay restores the results-time state and the auto replay starts again from tick 1 (`rmenu2`: tot 1167 then 1).
The overlay: 'Replay' and the timeline (green flag .. checkered flag, marker at frame / length x 315 + 170) at the top, the help
panel at the bottom ("Camera - Web-cam", Skip backward L1 / Skip forward R1, No function, Exit, Menu up / down, Change camera,
Slow motion, Play/Pause, Timeline, Manual cam, Manual cam zoom, Hide help).

**Cameras.** A replay locks the view's director (0x161FA0: flags bit 1; every change is a cut, 0x15D078 forces rate 1) and
cycles 9 cameras (0x445438[R+0x630], names 0x20E6B0 from ASCII in the ELF): **Web-cam** (0x5D, the default and the only one
behind the results), Manual-cam 0x0B, Near/Mid/Far-cam 0x3C..0x3E, Relative 2:00 / 10:00 / 4:00 / 8:00 (0x5E..0x61).
- Web-cam follows the course's replay camera triggers (manager 0x4C5830, world-stream chunk kind 17 in BAM.SSB; 846 triggers,
  `tools/export_camera_triggers.py`): 0x16D320 tests the human's rider+0x110 against each volume (ellipsoid
  |S^-1 Rz Ry Rx (p - pos)| <= 1, or box |Rx Rz (p - pos)| <= extent) from the rider mover (not in the countdown, a reset or a
  handplant: the first test is at GO), keeps a stack of entered ids and requests the entered volume's camera (0x16E1D8):
  Bounded 0x5B (692: a fixed point, or a line, looking at the head + lead + height with the action's fov), or a switch camera
  (DEFAULT_3 / DEFAULT_2, Look Back 0x20, Idle 7, Lazy 0xD, Direction N 0x12, SPOKE 0x42); leaving the last volume fires its
  exit action. Snow Jam: trigger 0 fires at GO (tick 202) with a point camera at the start.
- Web-cam cuts back to the preferred camera (Pause Options, DEFAULT_3 by default) after 10 s on other cameras (0x161D80).
- Every camera goes through the compositor (terrain lift 0x15EE00, lens clamps); no shake in a replay.

**Sound.** Nothing is recorded; the re-simulation fires the sounds again. While R state is 1..9 (0x288AE0) music changes, the
DJ, every speech category, the countdown beeps and GO 0x5E, boost, Tricky, Uber, points, combo, overtake, reset, the crash meter
and the big-air duck are blocked; board, carve, take-off, landing, crash and grunt sounds play. PS2 audio log (`gu-audio`):
the landings (bank 10) and a world sound (bank 3) at the same ticks in every loop, GO's UI sound 0x62 at tick 180, no beeps.
The auto replay keeps the results music; the Replay item plays the replay music (0x28F200 / 0x28F2C0).

**Which modes.** 0x20A8F8 skips the auto replay for the peak races and jams (0x535C12 modes 6-11: results dialog 0x16,
Transport / Restart / Quit; `allpeak/flow/*results*`: R state 13, the world runs on); free ride has no results. Race, slope
style, big air, pipe and the rival challenges (modes 0-5) replay (`nav/bc/out-tuck-finish2`, `out-jam-finish`: Happiness rival
time / points replaying from tick 1, a rolling start). Online results show item 0 only.

## 2. The browser

**No second copy of the core.** A replay re-initialises the race on the same core and feeds the recorded pad:
- recording (`web/replay.js createRecording`, from `web/game-tick.js simulate`): the 24-channel pad each tick fed the core as a
  byte stream of changes (every channel web/pad-input.js builds is a pad byte: a record is a varint tick delta, the changed
  channels' mask and one byte each; a value that is no pad byte, e.g. a QA pad, is kept as float32). A keyboard run is a few
  bytes a key change (Snow Jam, 1500 scripted ticks: ~200 bytes), an analog stick that moves every tick ~10 bytes a tick
  (~100 KB for a 3-minute race). Plus the calls made between ticks (pause Give Up, the Pause Options camera). It stops at the
  finish tick.
- the start state (`main.js createRaceReplay snapshot`, taken at the end of a live `startRun`): the time limit, input map and
  camera option, what `collectStart` gave the core (career cash, collected rows), the relationships after the start's aging
  (`web/ai-race.js replaySnapshot`), the shared game RNG, the visual RNG and gp+0xA0C. A few hundred bytes.
- a replay (`startRun(R)`): the restart path again (reset animation / race / rider, `start_event`, the computer riders'
  start), without the career run end, the music, the UI or the pad clear, then the start words back. The relationships run
  on a copy (no aging, saving or messages; `replayEnd` puts the live tables back); `collectPoll` is not called (the queues are
  drained unpaid); no HUD, rumble, results or career end in `present`.
- input latency: none (the recording is a copy of the pad the tick already uses; nothing changes for a live run).
- the restart path was not exact: a debounce's DeadNode / RestoreNode (a collected pickup, 0x3506D8 / 0x350F60) wrote the
  collision world's flags without noting the countdown value, so a restarted run found the pickup gone (`web/stage_world.inc
  stage_note_countdown_flags`). With that fixed every replayed tick equals the live run's `?simtrace` hash (the riders, the
  posed frames, the world states, both RNG streams, the score object): Snow Jam to its finish (14060 ticks, neutral pad),
  9000 ticks with a scripted pad, all 17 ported events (standard and rival) for 2500 ticks, a second loop too.
- the gameplay camera keeps running under the replay (its eye drives the fog / weather painters and the section activation,
  which draws the shared RNG); the replay view is a second camera stepped from the same input (`web/replay_camera.inc`):
  Bounded (point / line), DEFAULT_2/3/4, Idle, Lazy, SPOKE, Relative, Manual as decoded; Look Back and Direction N use DEFAULT_3
  plus their final step (the eye flipped past the rider / turned to the compass bearing), not their own chase constants.
- the Web-cam against the PS2 (`menus/replay/bhp1-neutral`: The Junction, neutral pad from the countdown anchor to the finish at
  race tick 4223, then the replay; the browser runs the same neutral run, finishing on the same tick): the same cuts on the same
  ticks (DEFAULT_3 through the countdown, the Bounded trigger cameras at 244, 870, 1536, 2362, 3237, 3901, the 10 s returns to
  DEFAULT_3 601 ticks later: the timer adds 1/60 with the EE's chopped add.s), the Bounded eyes / look-ats / fovs equal to
  0.03 cm (the metre conversion), DEFAULT_3 median 1.7 cm (p90 16 cm right after a cut). `web/replay_camera.inc` carries the
  view's outer camera (its terrain lift) on from the run's last frame, as the PS2 view does.
- the triggers load with each run (`main.js createRaceReplay prepare`, from liveStart and replay.load): the event's location (an
  in-world event replay: worldEvent.code), never a streamed world (course.freeRide: MOUNTAIN / PEAKn have no camera-triggers.json and
  never replay on the PS2). A missing file is no triggers (the Vite dev server answers it with index.html and 200: before 2026-10-01
  every CTM free ride handed that HTML to the core, "Replay cameras unavailable" = nlohmann parse_error 101 '<'; the race replays
  were not affected).
- the full replay's overlay is the original '64replay' (`tools/export_replay_screens.py` -> /assets/UI/replay-screens.json)
  through web/lui-player.js; its keys are the menu keys (Space Cross, Backspace Circle, Escape Triangle, Shift Square, Q / E
  L1 / R1, Enter Start, arrows the D-pad; J L / I K / U O the Manual camera on a keyboard).

**Differences from the PS2** (the no-extra-memory rule):
- R1 / L1: the original restores a kept snapshot at once; the browser runs the simulation to it (about 40 ms of ticks a frame,
  the last one drawn), from the start when going back. The kept points are the PS2's (start, highlight buckets, end).
- The full replay's first frame is the run's first tick (the riders and the view need one step), not the gate snapshot.
- Save replay is greyed (no memory card).
- The fog / weather painters and the section activation follow the gameplay camera of the re-run (the PS2 drives them with the
  replay camera): that keeps the re-run exact (the section scan draws the shared RNG), and the painters differ only where the
  replay camera is far from the rider.
- The timeline marker (percentcomplete, textured at run time) is drawn as the PS2's flat grey 11 x 11 square.
- The recording is the pad, not the 8-byte command: the 4096-run cache limit is not reproduced (a replay is never cut short).
- The visual stream is restored with the rest (the PS2 replay's flags / crowd / splash differ from the run; the browser's are
  the run's).
- Online races keep the finish camera behind their results (the other riders come from the network; the PS2's online results
  have no Replay either).

## 2a. In-world events: the snapshots (pv eventReturnInWorld (b); inventory 2026-09-30, not built yet)

An in-world Conquer the Mountain event (docs/ctm-events-in-world.md) starts on the streamed world as free ride left it (no world
reset at an offline event start), so the port's restart path (startRun(R)) cannot rebuild its start state: measured with
compare-ai-capture.mjs REPLAY_PROBE on c0a-ret3 (restart as startRun(R) would, then all 1989 recorded ticks again), tick 0 already
draws 18 shared-RNG words against the live run's 10 and every rider ends metres off. The port therefore takes the PS2's two
snapshots (decision C, coordinator 2026-09-30): the **countdown snapshot** (0x26D818 at the gate open, 0x234AD0: 26F228 / 26F7B8)
for the replay's restart and R1 / L1, and the **results-time snapshot** (R+0x3D0, saved at the replay's start, state 9) that the
Transport's stopAutoReplay 0x2706F0 restores (no re-simulation).

**Scope, from 0x26D818 (save) / 0x26DBF0 (restore), in their order:**

| PS2 item (save / restore) | what | port subsystem |
|---|---|---|
| 26E9C0 / 26E9B0 | the snapshot stream's cursor (R+0x08 / +0x10) | (the snapshot object itself) |
| 0x3178E0 / 0x317908 | the shared game RNG 0x4FF030 | `rng` (animation_rng_words), shared by the contexts |
| jalr x2: gp+0x2A88 / gp+0x2A8C | the object id counters | no port state found yet (to check: the world-entity ids of shared_world.inc) |
| 14DC80 -> 14DF08 / 14DFA8 | BE: 14 sub-objects of the game object (14DD58(i), vt+0x18) | Big Challenge / mission state (mission_gameplay.inc; to confirm which 14) |
| G vt (0x47D110 +0x44 / +0x4C) | the session | race session (race_bridge.cpp `race`, the clock) |
| C vt+0x3C 0x12B7F0 / +0x44 0x12B948 | the rider manager: race +0x00..+0x28 (phase, total tick +8), the lists +0x40 / +0x48 / +0x5C, per rider through C+0xDE8's vtable (human 0x458338 -> 0x111AC0 / 0x111D98; computer 0x4585F0 -> 0x10A898 / 0x10A8E8 + C+0xDF0..+0xF40) | each rider context: physics, controllers, animation, rails, handplant, board press, attacks, boost, score, pair / peer records, route and reset state (core.cpp, animation_bridge.cpp, the *_gameplay.inc files, npc_gameplay.inc; race_world.cpp for the shared records) |
| per rider (0x111AC0) | raw rider +0x000..+0x6C0; sub-objects +0x780 (skeleton), +0x784 (animator), +0x788, +0x790 (score); +0x864 / +0x868; C+0xDE0 / +0xDE4 (motion owner, control); C+0x20..+0xD20; rider +0xAB4..+0xAC0 (the paths) | as above (the carried words event_grid_start / npc_grid_start keep are among them) |
| restore also | clears C+0x3B0..+0xC70, re-poses (0x312598, 0x3103F0, 0x106828) | the riders' FX objects are cleared, not restored |
| 0x2D9CB0 / 0x2D9D68 | avalanche | avalanche_gameplay.inc (engine/avalanche.hpp originalAvalancheSave / Restore, already ported) |
| gp+0xCE8: 0x30BB10 / 0x30BD20 | the world-script manager WScriptMan (0x28C header, 0x10C-byte script slots 30C6C8, 30BC80 lists) | stage_script_gameplay.inc, stage_world.inc (script state) |
| gp-0x6F0: 0x229E20 / 0x229E58 | CrowdMan2d (0x200 bytes from +8) | stage_world.inc crowd |
| gp+0xF00: 0x3441A8 / 0x344240 | a world manager of 5 slots x 0x1F4 (344FC0 per used slot; reset by 230180's 343BC0(S+0x3C)) | to identify (candidate: the move nodes / modifiers) |
| gp+0xF38: 0x357CA8 / 0x357D28 | a world manager of 4 lists (358380 each; reset by 230180's 357B38(S+0x40)) | to identify (candidate: the splines / dead buckets) |
| 0x26D988 / 0x26DDC0 | gp+0x2898 entity group 1 (every entity: count, then each one; the restore deletes and rebuilds through the factory 0x4816C0) | stage world entities (set pieces, LiveComps, MeshAnim, pickups as DeadNodes): stage_world.inc, set_piece_gameplay.inc, pickup_gameplay.inc, shared_world.inc |
| 0x26DA88 / 0x26E340 | entity group 8 | to identify |
| race+0xA4: 0x103480 / 0x103578 | the section activation (C+0xA4) | section_gameplay.inc |
| not in it | the replay camera: the trigger manager 0x4C5830 (the loaded triggers, the entered stack, the active volumes) and the view's director (0x161FA0); the loop's 0x1620D0 keeps the trigger lists | web/snapshot-policy.mjs SNAPSHOT_CURRENT: every replay_camera.inc variable (R9, 2026-10-01) |
| restore only | 354C98 / 355118 (group 1 queues), 3A6800(gp+0x16C8: the world cache), 26DE58 (group walk), 2C03E8 (every painter wrapper reset), 12B788 (120E50 per rider: the scope lists), 22E840 (the views) | re-derived after a restore, not saved |

**Cross-check against the fingerprint** (REPLAY_PROBE, the core's info exports at the countdown, live against the naive restart):
stage world info / flag words / script info / builtin counts, set-piece sections, missions, pickups, world events, race world, score
object and rider state are in the PS2's list. Not in it: weather (the painters: 2C03E8 resets every painter wrapper at the restore,
so they are not restored but reset), camera state words (the cameras are re-derived: 22E840), peak-world events and stage teleports
(the streaming and the location's path banks: not part of the snapshot; to check that nothing of them changes in a race).

**Built (2026-09-30; compiled in with SSX_SNAPSHOT=1 / SSX_SNAPSHOT_REGISTRY, pv eventReturnInWorld on the page):**
- **The registry.** web/generate-snapshot-registry.mjs (run by web/build-core.sh) registers every file-scope RIDER_LOCAL of the core's
  units (983), with its compile-time type (web/generated/snapshot/<unit>.inc, included at each unit's end). web/check-snapshot-registry.mjs
  (in the build) checks the link map: every TLS variable is registered, a function-local output buffer (155, the info exports'),
  or one of two engine thread_locals (the input map variant, the rounding mode). A new one fails the build with what to do.
- **A context's snapshot** (web/world_snapshot.hpp, web/rider_context.cpp): a save copies the 167 KB TLS block and deep-copies each
  variable that is not trivially copyable (holders made once, at the first save of that context, then reused); a restore puts the
  trivially copyable ones back as bytes and copy-assigns the others. The default is snapshot; web/snapshot-policy.mjs classifies the
  rest, each with its reason:
  - SNAPSHOT_KEEP: tables a race does not change (the location's rail records and walk caches, the streamed world's path banks /
    rows / residency / map ids, models, the section template, the rider model's bind data, constant tables). Under QA their hash is
    taken at each save and checked at each restore (checked on c0a-ret3, avalanche/eba3-rock-hit, setpieces/full, allpeak/apr-start,
    peak3/fr-throne-unload, peak2/dss2-full: compare-ps2-capture.mjs SNAPSHOT_KEEP_CHECK). resetPaths / evictedResetPaths went back
    into the snapshot (a location unload changes them).
  - SNAPSHOT_CURRENT: left as they are because the PS2's snapshot does not hold them: the visual stream 0x4FF018 (snowParticleRandom,
    stageWorldVisualOwn) and the LCG gp+0xA0C (trailVisualRandom); the streaming's event queue (drained by the page); the drawn
    frame's skin matrices / palette (re-derived by the next pose: the PS2's restore re-poses, 0x312598 / 0x3103F0).
  - SNAPSHOT_REDERIVED: restored, then re-derived by a restore hook (the painter wrappers and the painter trees' views).
  - SNAPSHOT_OWN: the hooks' own storage.
- **Hooks** (a subsystem's own save / restore, after the variables; each has a check that runs before anything is restored):
  browserBodies' run-time instance state (flags and bits of every instance; entity, answer box and matrix of the dynamic ones; the
  geometry kept, the instance count required), the stage VM's run-time tables (its programs kept: engine/stage_script_vm.hpp
  RuntimeSnapshot), the avalanche world through the PS2's own 0x2D9CB0 / 0x2D9D68 (the saved slots triggered again and brought to t
  without the group speed factor: a playing avalanche comes back as the PS2 brings it back; code-ported, not capture-verified), the
  painter trees' views (pointed at their restored node lists) and every painter wrapper reset (0x2C03E8, as 0x26DBF0).
- **The restore checks every context first** (core snapshot_check changes nothing), then restores: a restore that cannot run leaves
  the state as it is (web/event-snapshot.js).
- **The page** (web/main.js, pv eventReturnInWorld): an in-world event replays behind its results (web/replay.js allowed()); the
  countdown snapshot is taken at the live start (replay.liveStart -> snapshot()), the results-time one at the replay's first start
  (inWorldReplayRestart: 0x26F8A0 state 9's save), the replay's restart and seeks restore the countdown one, the results' Transport
  stops the replay and restores the results time before the stop frame's tick (inWorldResultsRestore, 0x20CF88 -> 0x2706F0).
  web/ai-racers.js saveState / restoreState carry the riders' orchestrator; web/ai-race.js replayRestore the relationship tables.
- **Sizes and memory.** The page, six contexts: the results-time copy frees 4.05 MB when dropped (the heap in use 162.64 -> 158.59
  MB at the map); snapshot_bytes 3.3 MB for both slots (it does not see containers inside user structs; mallinfo is the measure).
  The per-variable heap attribution (snapshot_entry_heap / snapshot_slot_heap) is made only with snapshot_qa: mallinfo walks the
  whole heap, and without qa it cost 3.6 s at the page's first countdown save (2026-10-04, ctm-events-in-world.md "The card freeze").
  The copies hold references (a rider's rig and clips through its animation graph), so two rules keep the memory flat: the
  countdown save also overwrites slot 1 (snapshotCountdown: the last event's results copy no longer keeps its data through this
  race), and the event's end drops the rider contexts' copies (snapshotReleaseRiders, at the riders' leave; the next countdown save
  makes them again, about 3 MB, in freed memory: a deliberate exception to "no allocation at the countdown", coordinator 2026-09-30,
  because measured flat beats the one-time peak). Without the second, the next event's riders loaded beside the last event's (a
  one-time peak: WASM memory 184 -> 221 MB). Measured (Chrome, 3 in-world events, the replay on, QA checks off): WASM memory 184 MB
  flat, the heap in use 158-163 MB at every step, its high-water mark 174.7 MB, as the replay-off baseline (184 MB). The QA checks
  (?qa, snapshotQa=0 turns them off) hash the kept tables at each save / restore (the path banks as JSON).
- **Gates (c0a-ret3):** REPLAY_PROBE (compare-ai-capture.mjs): the countdown snapshot back and all 1989 recorded ticks run again equal
  the live run on every tick (the six riders, the shared RNG; the visual stream differs, as on the PS2), under two conditions the
  capture meets: no avalanche playing at the restore (0x2D9D68 brings it back re-triggered) and no painter between its payloads (the
  2C03E8 reset jumps it). ctm-events/c0a-ret3 (test-ps2-captures.mjs, --replay-return): the results-time snapshot, 600 replayed ticks
  from the countdown snapshot, the Transport's restore, then the return: all six exact to the removal, the human to the end, the RNG
  and ranks everywhere, the pair records from the return.

## 2b. Online records (pv onlineRecords, docs/online-records.md)

A finished run's recording and start state travel as a replay file (web/server/replay-file.mjs: `createRecording().exportBytes()`,
`importBytes(bytes, ticks, calls)`); `replay.load({recording, snapshot, finishTick, highlights})` puts a downloaded run in place of the
live one and the full replay plays it (Watch Replay, web/online-replay.js). Checked: a Snow Jam run downloaded and replayed equals the
live run's `?simtrace` on every tick, also after a course switch and with another rider selected (web/test-online-records.mjs).

## 3. Verification

- `web/test-replay.mjs` (npm test; CORE_DIR / ASSETS_DIR check a scratch build): the recording (the change stream, analog
  sticks, calls, skip points); in Chrome, Snow Jam / Happiness (rival, rolling
  start) / The Junction runs with a scripted pad and a Give Up: every replayed tick's `?simtrace` equals the live run's, a
  second loop too, the career save / relationships / results unchanged, the Replay item (paused first frame, Play, camera
  cycle, timeline, Replay Menu -> Exit replay back to the results and their replay), the Web-cam's triggers firing.
- `web/test-replay.mjs` BHP1-PS2 (2026-10-01): the neutral The Junction run finishes at 4402 (the PS2's length 4403) and its auto
  replay's Web-cam makes the PS2's 23 changes (DEFAULT_3 / Bounded and which trigger's camera, matched by the PS2's Bounded eye =
  the trigger's bound point and its fov) on the PS2's ticks (page tick = PS2 replay frame + 1); two fall in the capture's poll gaps
  (frames 397..429, 3597..3629). MOUNTAIN: a CTM free ride asks for no triggers. `web/test-online-records.mjs`: Watch Replay's
  Web-cam has ARA1's 48 triggers and fires them, also after a course switch from BRA2.
- PS2 frames: `local/ps2-capture/menus/replay/` (gu-loop, rmenu1, rmenu2, af1, gu-audio, bhp1-neutral with the camera log
  `bhp1-neutral.log.jsonl`: race clock, replay manager, view director and outer camera per poll).
- 2026-09-26 (live core 23:13, switch on): full suite 161/162 (the known test-slopestyle-bigair baseline); test-replay in Chrome
  (Snow Jam 1501 ticks from 233 pad bytes, Happiness, The Junction: exact, second loop exact, triggers firing); WebKit
  (web/webkit-driver.mjs): Snow Jam exact, the replay behind the results, the full replay's overlay, camera cycle, Replay Menu,
  Exit replay; the default page (no ?pv) on the live core and assets: the BHP1 Web-cam cuts on the PS2's ticks.
