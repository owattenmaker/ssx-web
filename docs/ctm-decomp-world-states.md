# Conquer the Mountain: the world-state machine and the event lifecycle, decompiled (2026-09-29)

This document decompiles every world state (WS) of the game module in SLUS_207.72 (gp = 0x4A30F0) and follows a CTM event through
them. Each rule is set against the browser port. It extends the address map in [ctm-parity.md](ctm-parity.md) "Code
(SLUS_207.72)" and the flow in [ctm-flow.md](ctm-flow.md); it does not repeat them.

Method:
- The code: `local/ctm-decomp/ps2dis.py` (rabbitizer over the ELF, with gp and lui+imm annotations and names; run it through
  `local/ctm-decomp/d ADDR [N]`, `-x TARGET` for references, `-w` / `-f` / `-s` for data). `calls.py TARGET` lists every call
  with its constant arguments; `scan.py OFF bits` lists every gp word reader.
- The PS2 runs (ARMSX2, derived states only, silent): `local/ctm-decomp/caps/`. `ui_trace_capture.py` is
  `tools/ctm_flow_capture.py` with a trace of the overlay stack (+0x98 id, +0x9C timer, +0xA0 ready) at every polled tick.
- The port runs: Chrome headless (`--mute-audio`, `?mute=1`) and WebKit (`web/webkit-driver.mjs`): `local/ctm-decomp/qa/`
  (`drive.mjs` / `drivewk.mjs SCRIPT`).

Names used below:
- G = `[0x4A28A8]`. S = `[0x4A2C68]` = G+0x84, the game module (cGame).
- The rider manager C = S+0xC. The replay manager = S+0x28. The streamer = S+0x78 (cSectionMan). The "Loading..." object = S+0x94.
- nis = `[gp-0x84C]`, with lists at nis+0x54C, 0xCC each; nis+0x550 is the active list, 2 = none. GMM = G+0xC0 = `[0x4A2C6C]`.
- The settings byte 0x535C10 is the event type: 0 race, 1 slope style, 2 big air, 3 pipe, 4 free ride, 5 rival, 6 peak run.
  0x535C11 is the game type (0 CTM). 0x535C12 is the game mode.

## Ranked differences (what a player feels most first)

The status column gives the state at the end of this session. The switches are in `web/pv-flags.js`.

| # | PS2 rule (address) | port | fix (files) | status |
|---|---|---|---|---|
| 1 | **Every round races its own riders.** WS13 enter 0x235AA0 sets the AI characters from GMM+0x40 (the round's roster: the semi is the qualifier's top three plus entries 5..7; the final is the peak rival in slot 1, then the semi's top three, then entries 8, 9). It then rebuilds and places the riders: 128958 / 1296F8 / 1289F0. | Next heat and Final Round kept the qualifier's five riders and grid. `aiRace.prepare` only ran in `ui.loadEvent`'s warm-up. The final never had the rival, and lineups.json had no rival record ("No computer-rider data for mac (base 3) in slot 1"). | `career-ui.js ws13Riders` → `main.js ui.cb.heatLineup` (coordinator, pv **ws13Rebuild**). Rival data: `tools/export_lineups.py export-career` / `build` (this session), `web/public/assets/{ARA1,BRA2}/lineups.json` (install pending). | Chrome: semi and final = `roundEntries` exactly, the final with Mac (qa/heat-lineup2.mjs). Peak 2 finals (CRA3 / DRA4, rival Nate): exported 2026-09-30, pv careerRival (see "Rounds"). |
| 2 | **No load between the free ride and the event.** The gate calls WS1 arg 1 in the same world. The riders load one step a tick (1296F8) while the fly-over plays, and the list is held at a step end only while they load (279040). Snow Jam: at most a few ticks. | The fly-over, then a course switch: 5-10 s of black and "Loading..." (known, [ctm-flow.md](ctm-flow.md) §7). | Structural: the event must load under the fly-over, or run in the streamed world ([presentation.md](presentation.md) §3). Design: [ctm-events-in-world.md](ctm-events-in-world.md). | differs (known) |
| 3 | **Back to free ride with no load.** Results Transport → the map → the same location = WS15 0x236058: world reset 230180, the rider at session point 1 (11DE60(rider, IF(0xA)+0x10, 2)), a white fade-in of 1.0 (2E4370 with in = 1.0) → WS1 arg 0 → WS2 → WS3 → WS4 on consecutive ticks. | The page reloads the free-ride world under a black cover, then fades from white over 58 ticks (`career-ui.js goWorld` / `enterWorld`). | Structural (the event is a separate package). The white fade matches. Design: [ctm-events-in-world.md](ctm-events-in-world.md). | differs (known structure) |
| 4 | **The rival and peak-run cards lie over the ready state.** WS1 arg 3 or the world load → WS2, the riders at their start spots. PS2 `ctm-parity/restart/runs/p2r-restart-yes/sample00580.png`. | The rival card was black after both Restarts. The Peak run card was black on the first entry and after both Restarts. readyView was skipped: `course.eventStart === false` in the streamed world. | `career-ui.js restartToCard` and `begin`'s card call `ui.cb.readyView`; `main.js readyView` only skips `_start_event` without an event start (coordinator, pv **ws13Rebuild**). | fixed behind the switch (Chrome and WebKit: every card 101-112 mean brightness, was 26-27) |
| 5 | **Cross skips the finish panel.** Overlay 0xC (race) / 0xD (freestyle) opens at WS5 + 179. Its update starts at +225, and a new Cross edge (1E8160, event 5, +0xA0 set) closes it: WS_advance 0x231320 → WS12 (podium) or WS7. Otherwise it closes after 3.0 s of its timer (+405), or 1.0 s with the human's +0x480 set (+285). | A fixed 408 / 288 ticks; no skip (`game-tick.js:52`, :134). | `game-tick.js`: accept a fresh Cross from finish + ~228 and open the podium or the results that tick (coordinator, next). | missing |
| 6 | **The world holds still under the card.** WS2 enter pushes pause context 1 (mask 0xFFFFFFDB). Bit 8 skips the NIS tick and bit 1 the sim; the view keeps drawing and the view fade keeps running. | The start-gate idle kept looping under the card (`cutscenes.js update`). | `cutscenes.js` `idleFrozen` / `seq.fadeT` (coordinator, pv **cardFreeze**). | fixed behind the switch (idle clock held at 1-2.5 ticks; Chrome and WebKit) |
| 7 | **"Loading..." whenever the game waits.** S+0x94 is a bit mask with a reason per bit, drawn at once while it is non-zero (1F3188 / 1F31E0). Bit 0: a transport's departure step until WS10. Bit 2: WS1 until the riders are loaded. Bit 3: the whole of WS13 (Next heat / Restart). Bit 4: WS5 from the finish until the podium list is loaded (PS2 race-q s13741-13761 at the finish line). | Drawn only inside a held or looping cutscene step (`cutscenes.js:914`) and the pre-fade. There is none at the finish, and none over #150 gond_inair. | `ui.js` / `cutscenes.js`: a caption owner with the PS2's reasons. The finish case is a caption for ~20-40 ticks after the line in a qualifier, longer before a podium. | missing |
| 8 | **Peak-run Restart reloads the world.** Popup kind 0 at 0x20D664: mode 6..11 → the run's start course (14 / 15 / 16), initGameMode, cGame_exit(S, 1) → WS6 → PreGameLoadScreen → WS10 → WS1 → WS2 (~420 samples of load). | Restarts in place, no load screen. | Owner's call: the port is faster. | differs |
| 9 | **Starting, restarting or leaving an event ends a Big Challenge.** 30B7F8 runs at builtin 67 (gate), WS13 enter, WS14 enter and WS15 enter. | Only the door and booth (`free-ride.js` stationFlow). The gate leaves it running until the course switch unloads the core. | Scripted free-ride agent's lane (overlap). | differs |
| 10 | **The world ticks under the heat and transport cuts.** WS13 and WS14 push no pause context; the riders are in PreRace (3). | `cb.quit` stops the run (`running = false`) for the gondola, heat and podium lists. The door, booth and transport cuts keep ticking (pv nisTick). | Low: the gondola cut is an interior. | differs (low) |
| 11 | **A transport places the rider first.** WS14 enter args 1..3 place the human with 11D390 before the NIS (0x2363DC). | Holds the rider where it stands ([ctm-parity.md](ctm-parity.md) "The NIS rider hold"). | known | differs (low) |
| 12 | **Session (MCOMM) is WS15 too** (0x208C28: IF(0xA)+0x10 = the point, WS15). WS15 also resets the world (230180: stage world, set pieces, NIS, replay, HUD) and ends a Big Challenge. | `freeRideSession`: the white fade over 60 ticks. The world reset is unconfirmed. | Check what the port resets. | unconfirmed |

Not differences (checked):
- Pause contexts under the pause menu (ctx 2).
- Countdown skipped for rival and peak runs (WS3 enter: event types 4..6 → WS4 at once; the port's rolling start).
- New-career flag cleared at WS7 / WS11 enter.
- Visited bit at WS10 exit (the port's WS4 ride start is the same tick).
- Last lodge at WS10 enter.
- Reward record cleared at WS10 enter.
- Results item dispatch.
- The door / booth / Transport flows ([ctm-parity.md](ctm-parity.md)).

## 1. The machine

### 1.1 States, their objects and vtables

The game module's constructor 0x22E968 builds sixteen state objects inside S. `WS_tick` 0x230F40 indexes them 1..16 (jump tables 0x47B7B0 / 0x47B7F0).

Each object holds:
- +0 the default next state;
- +4 its argument;
- +8 a done flag the overlays set;
- +0xC the vtable, whose slots are +0x10 pre-enter, +0x18 pre-update, +0x20 cancel, +0x28 enter(arg), +0x30 update, +0x38 exit, +0x40 ready.

The base vtable is 0x47D6E8, all empty, with +0x40 returning 1.

| WS | object | vtable | enter / update / exit | default next | name (what it is) |
|---|---|---|---|---|---|
| 1 | S+0xB0 (ctor 234188) | 0x47D530 | 2341D0 / 2343B0 / 234750 | 2 | intro lists + rider build (PreRace) |
| 2 | S+0xD0 | 0x47D270 | 236BB0 / 236C48 / 236C88 | 3 | the round card |
| 3 | S+0xE0 | 0x47D4D8 | 234AD0 / 234C68 / - | 4 | countdown (cGFGateState) |
| 4 | S+0xF0 | 0x47D480 | 234E20 / - / - | 5 | riding (race or free ride) |
| 5 | S+0x100 (ctor 233C10) | 0x47D5E0 | 233C50 / 233CD8 / 234008 | 7 (12 with a podium) | finish |
| 6 | S+0x118 | 0x47D690 | 233B60 / - / 233B88 | 8 | module exit (cGame_exit) |
| 7 | S+0x128 | 0x47D218 | 236DA0 / 236E60 / 236EA0 | 8 | results |
| 8 | S+0x138 | 0x47D638 | all empty | 11 | no requester found |
| 9 | S+0x148 | 0x47D588 | - / - / 236B38 | 10 | no requester found |
| 10 | S+0x158 (ctor 234EF8) | 0x47D428 | pre 234F40, pre-update 235080, cancel 235570, enter 2355C0, update 2357F8, exit 235868, ready 2355B0 | 4 (1 in a backcountry / outside CTM) | world load / arrival |
| 11 | S+0x178 (ctor 236868) | 0x47D2C8 | 2368A0 / 236960 / 236AF0 | 10 | crossing / held transport loop |
| 12 | S+0x18C | 0x47D1C0 | 236F08 / 236F40 / 236F90 | 7 | podium |
| 13 | S+0x19C | 0x47D3D0 | 235AA0 / 235CC8 / 235F20 | 1 arg 3 | next heat / results Restart |
| 14 | S+0x1B8 (ctor 236208) | 0x47D320 | 236250 / 236418 / - | 4 | station cut, lodge door, transport, post-event map |
| 15 | S+0x1DC | 0x47D378 | 236058 / 236198 / - | 10 (the update requests 1 arg 0) | same-location return (session point) |
| 16 | S+0x1EC | 0x47D168 | 236FE8 / - / 237038 | 4 | no requester found (closes overlay 0x11) |

A static scan finds no request of WS8, WS9 or WS16 (no 231250 with those constants, no 16 / 8 / 9 written to an object's +0). Only the defaults of WS6 / WS7 point to 8, and WS7's update always requests 14 instead.

### 1.2 Transitions (0x230F40, called once a frame from the game frame 0x230B84 / 0x230BA0)

- `WS_request` 0x231250(S, state, arg, now) writes S+0x210 / S+0x21C. With now it runs the tick at once; otherwise the next tick applies it.
- In one tick:
  1. the current state's update runs (skipped only in WS3 with a request pending);
  2. the pushed state (below) runs;
  3. if a request is pending: current exit → S+0x218 = the old id → the new object's enter(arg).
- So an update that requests (233AA0 = request obj+0 with arg obj+4) changes state within the same tick.
- `WS_push` 0x231278(S, 10) queues WS10's +0x10 (once) and +0x18 (every tick) while another state runs. The streamer does this at a crossing (22DF50: WS11 now, then push 10), so WS10 pre-loads the arrival lists under WS11. 0x2312D8 asks the pushed state's +0x40 (WS10: phase 5); 0x231280 cancels it (+0x20).
- The tick is skipped entirely while the pause mask has bit 0x20 set (no context sets it), or with S+0x224 set outside the demo.

Every direct request (`calls.py 231250`):

| from | request |
|---|---|
| 113DB0 (race phase tick, 12A250 = race over) | WS5, and riders phase 6 EndRace |
| 22D6C8 gate | WS1 arg 1 |
| 22DF50 streamer crossing | WS11 now, then push 10 |
| 22DFF0 streamer loaded | WS10 |
| 22EBC8 module init | WS10 |
| 2302A8 cGame_restart (pause Restart) | WS1 arg 2 now |
| 230488 cGame_exit | WS6 |
| 233B60 WS6 enter | WS10 |
| 2018A8 map pick | WS14 arg 1, or WS14's done flag and +0x18 = the destination when a WS14 runs |
| 208C28 Session | WS15 |
| 20CCF8 results Next heat | WS13 now |
| 20D0DC command 4 (results Transport / free-ride results) | WS14 arg 2 |
| 20D590 popups: Restart | WS13 now (results) or cGame_restart (pause) |
| 20D590 popups: kind 4 | WS1 arg 1 at session point 1 (free ride: probably Quit Challenge, unconfirmed) |
| 236198 WS15 update | WS1 arg 0 |
| 236418 WS14 update | WS15 (same location) |
| 236E60 WS7 update | WS14 arg 2 |
| 302210 builtin 68 | WS14 arg 2 (booth), WS14 arg 0 (door) |

### 1.3 Pause contexts and the game frame

- **The stack.** 0x5366E8[`gp-0x69C`] is the context stack; mask `gp-0x6A0` = table 0x4428F0[context].
  - The contexts: 0 → 0, 1 → 0xFFFFFFDB, 2/3/5 → 0xFFFFFFDF, 4/7 → 9, 9 → 1, 10 → 0xFFFFFFD7, 12 → [0x442920].
  - The pushers: WS2 enter (1), the pause menu 0x230A94 (2), the lodge prompt / map WS14 0x236504 (2), a Big Challenge offer / fail 0x2308B4 / 0x230954 (3), the controller overlay 0x2309BC (12).
- **What each bit stops** in the game frame 0x230B00..0x230D50:

  | bit | stops |
  |---|---|
  | 0x1 | the whole simulation: stage world 309270, 2294C8, 3440C8, 357BF8, timer channels 1 / 5 / 6, and the replay manager's 26F4A8 |
  | 0x2 | timer channel 1 and 355028 |
  | 0x4 | timer channel 3 (purpose unconfirmed) |
  | 0x8 | the NIS tick (nis vt+0x28 at 0x230BDC) |
  | 0x10 | timer channel 2 |
  | 0x20 | the world-state tick |

  The views update 0x22E840 (render and camera output) is unmasked, so a paused frame keeps drawing.
- **Opening the pause.** Start is accepted at 0x230A74 when:
  - the replay manager is not in 1..9;
  - the playback isn't 1;
  - 0x20CBE8 finds no pause menu and the current UI state allows it (+0x1C bit 0 clear, its vt+0xC8).
  
  Which screens block Start is not re-derived here ([visual-parity.md](visual-parity.md) §31, start-rules.js).

### 1.4 Overlays

- **Commands.** `gp-0x9F4` (0x4A26FC) is the command word, read once a frame by 0x20CFD0 (table 0x471AE0):

  | value | handler | effect |
  |---|---|---|
  | 1 | 20CCF8 | an overlay closed |
  | 2 | 20D03C | the card is done: the current WS's +8 = 1, stop the auto replay |
  | 3 | 20D0B8 | close the pause |
  | 4 | 20D0DC | WS14 arg 2 |

  In free ride, 20A8F8(2) opens no card and writes 2 at once. So WS1 → 2 → 3 → 4 pass on consecutive ticks (new-career start, WS15 return).
- **Opening an overlay.** 20A8F8(kind) opens nothing while the replay manager is in 1..9.
  - Kind 2, the card: 8 for a race, 9 freestyle, 0x14 rival / peak (68rival_pre), 0x15 multiplayer, 0x11 demo.
  - Kind 7, results:
    1. overlay 0xF (Top 5 records) when IF(8)+0x18 / +0x1C (the record ranks) ≥ 0;
    2. else 0x10 (reward list) when 158F30 > 0 or cash;
    3. else 20A8B0(mode, round): 0x16 for peak runs, 0xA / 0xB for the others.
    
    Outside modes 6..11, kind 7 also calls 2706B8 (the auto replay behind the results).
- **The overlay-close dispatcher** 20CCF8 (table 0x471A50 by overlay id − 8):
  - cards 8 / 9 / 0x14 / 0x15 → command 2;
  - 0xF → 0x13 or rewards / results;
  - 0x10 → results;
  - results 0xA / 0xB / 0x16 → the item (gp-0x9F0, table 0x471AC0):

    | item | action |
    |---|---|
    | 1 | Next heat: WS13 now |
    | 2 | Transport: command 4 |
    | 3 | map mode 1 + the map (Single Event?) |
    | 4 | overlay 0x23 (Records) |
    | 5 | Restart: overlay 0x17 |
    | 6 | Replay: overlay 0x11 + 26F8A0 |
    | 7 | overlay 0xF |
    | 8 | Quit: overlay 0x18 |
- **Popups** (20D590, kind at +0x9C, table 0x471B80; Yes = +0x18 == 0):

  | kind | action |
  |---|---|
  | 0 | Restart (peak run → world reload; pause → cGame_restart; results → 238348 + WS13 now) |
  | 1 | quit → save profile |
  | 2 | overlay 0x1D |
  | 3 | pop, then 30B758 (Restart Challenge) |
  | 4 | free ride at session point 1, WS1 arg 1 |
  | 5 | Give Up (1253D0) |
  | 6 | cGame_exit(S, 0) (to the front end) |

## 2. Per-state rules

The port column cites the file and function (main.js is minified: function names instead of lines). "PS2 run" names the evidence
under `local/ctm-decomp/caps/` or the existing `local/ps2-capture/` runs.

### WS1: intro lists and rider build (enter 0x2341D0, update 0x2343B0, exit 0x234750)

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Enter | riders phase 3 (113B10); 1F36D8(S+0x94, 0); cBE_setState(0); 26F228(replay); number of AIs = GMM+0x10−0x14 and their characters from GMM+0x18.. / +0x40.. (setRiderCharID, 1473D0) | `career-ui.js begin` → `ui.loadEvent` (the event package) | match (by construction) | - |
| Arg 1 | if event type ≠ 4: 2790A0 / 27AAF8(nis, 0) (table 0x481E48 = [2 fly-over, 3 approach, 5 idle]) / 278F38. Free ride: nothing | `ctm-event.js rideIntoEvent` + 'career-ridein' ([ctm-flow.md](ctm-flow.md)) | differs (split by the course switch; rank 2) | ctm-flow |
| Arg 2 (pause Restart) | 27AAF8(nis, 1) = [5] | `restartToCard(false)` pv pauseRestart: [GATE_IDLE] | match | menus/race/r3-restart |
| Args 0 / 3 | nothing queued (WS15 return; WS13's rival branch) | - | match | - |
| Phase 0 | wait while the active list is loading (279298) | `seq.ready` | match | - |
| Phase 1 | play list 1 else 0 (234910); free ride / rival / peak (event types 4, 5 unless mode 4, 6 unless mode 5): 12AB20 + 12AC48, flag +0x10; else CTM: 128958 (create the AIs), 128998 when none; 128A48 x2 | the event load | match (structure) | - |
| Phase 2 | 12A180 all loaded? else 1296F8, one rider step a tick | the load screen / world cover | differs (rank 2) | - |
| "Loading..." | bit 2 while phase < 3. A list step that ends (status 4) is held (279040(nis, 0, 1)) until the riders are in, then released | the cover's caption | differs (rank 2) | Snow Jam: caption gone at s1035 |
| Phase 3 → WS2 | wait for list 1 to end and list 0 to reach its idle (step kind 5) or end; place on the grid (1289F0) outside free ride; free-ride branch: 12ABD0 && 12ACF8 then 12B030 / 12B000 | `onIdle` → card | match | ctm-flow §3 |
| Exit | 2790A0(nis, 1); race: 128958 + 1289F0 again; else 128A10 (game tick restart) + 26F7B8; riders phase 3; cBE_setState(1); 129160; outside CTM: 155E58 relationship ageing | `startRun` (on Continue); pv relAging | match | ctm-parity "Relationships" |

### WS2: the round card (0x236BB0 / 0x236C48 / 0x236C88)

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Opens | 20A8F8(2) (overlay by mode, §1.4); pushes context 1 (0xFFFFFFDB) | `ui.set('ctm-objectives')` | match | - |
| World under it | frozen: NIS tick (bit 8) and sim (bit 1) stopped; views draw; the view fade (2E4370) runs | pv cardFreeze: the idle clock held once t ≥ 1, the fade on `seq.fadeT` (`cutscenes.js:675`, :782) | fixed behind the switch (rank 6) | caps/card-idle (card-final-95k.p2s, neutral 360 samples: pixels outside the card identical s30..s360); qa card-freeze.mjs: t 1.00 → 1.00 (switch on) vs 31 → 10 (off); WebKit the same |
| Rival / peak card | over the ready state | readyView (rank 4) | fixed behind ws13Rebuild | qa rival-*, peak-*; PS2 p2r-restart-yes s580 |
| Update | waits for +8 = 1: the card's close → command 2 → 20D03C | Cross / Start → `play()` | match | - |
| Input gate | the overlay's own open (Cross ignored until it is open) | 30 real-time ticks (`career-ui.js` cardOpenAt) | unconfirmed (overlay timing: CTM-screens agent) | - |
| Exit | pops the context; 236CD8: 2790A0(nis, 0) stops the idle, race camera (C vt+0xCC+0x28(3)) and C+0x14 = 1 outside free ride, 2871B0 race audio. The transition to WS3 happens in the same tick | `startRun`: `cutscenes.stop()`, the chase camera, `gameAudio.runStart` | match | ctm-flow §3 |

### WS3: countdown (cGFGateState, 0x234AD0 / 0x234C68)

| rule | PS2 | port | status |
|---|---|---|---|
| No countdown for event types 4 / 5 / 6 | 233AA0 → WS4 in the enter | rolling start (`race_bridge.cpp` rollingRacePending) | match |
| Enter | riders phase 4; 29C418, 29C420(audio, 3); stage event hash "StartlightBegin" (234BE8 → 309DD0/309E50) | `begin_event_clock` → Countdown (`race_event.cpp:25`) | match |
| Update | waits while the replay manager (270280) is busy; C+0x1C (the countdown, counted by the riders' phase tick) at 0: "StartgateOpen" stage event, HUD cmd 6 (GO), 29C7B0 + 28C8C0(audio, 120), → WS4; every multiple of 60: 29C420(audio, t / 60) | 180 ticks, digits and beeps at 60-tick steps (`sfx-game.js`, `game-audio.js countdown`) | match ([ctm-flow.md](ctm-flow.md) §3) |

### WS4: riding (0x234E20)

- Enter: riders phase 5 (Race), C+0x14 = 1 and C+0x8C = 0 outside free ride, cBE_setState(1), HUD 39F840 unless a replay plays. No update, no exit: the race's end (113DB0) and the streamer / scripts request the next state. Port: `running`. Match.

### WS5: finish (0x233C50 / 0x233CD8 / 0x234008)

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Entry | the race phase tick 113DB0: 12A250 == 1 (race over) → riders phase 6, WS5 | `game-tick.js` finished | match | - |
| Enter | 26FA50(replay); 2790A0(nis, 0); counter +0x14 = 180; "Loading..." bit 4 on | no caption | missing (rank 7) | race-q s13741 / s13761: "Loading..." at the finish line, gone by s13781 |
| Phase 0 | every human under 277.78 cm/s ([0x49DF7C]); in a final (GMM+0x98) wait for GMM+0x9C; then 27AC60 (the podium / rival lists), 278F38; obj+0 = 12 when list 0 has steps; race: 155E58 | `finishCutscenes` (`career-ui.js finish`) | match (structure) | - |
| Phase 1 | when list 0 is not loading: HUD "finishov" (1), "Loading..." bit 4 off | - | missing (rank 7) | - |
| Counter 0 (+179) | 20CC60: overlay 0xC (race, multiplayer), 0xD (freestyle 1..3, 6); free ride (CTM): command 4 | `finishPanel` / finishLui | match | caps/fin-idle-ui: 0xC at tick 13888 = +179 |
| The panel's update | from +225 (46 ticks after the open; assumed to be its transition-in); +0x9C += 1/60 per update | - | - | fin-idle-ui: +0x9C 0 through 13930, 0.0833 at 13939 |
| Auto close | +0x9C ≥ 3.0 (1.0 when the human's +0x480 is set) → 39F840 + WS_advance 0x231320 | 408 / 288 ticks after the finish (`game-tick.js:52`) | match within ~3 ticks | idle: WS7 at +405; +0x480 poked: +285 (caps/fin-dnf-ui) |
| Cross skip | 1E8160: UI event 5 (a new press) with +0xA0 set → close + WS_advance | none | missing (rank 5) | fin-mash-cross: WS7 at +231; fin-hold-ui (held from +60): +405, no skip |
| Exit | 162290 per view; phase 2; 278F68(nis, 0, 1, 0) plays the podium list; obj+0 = 7 | - | - | - |

### WS6: module exit (0x233B60)

- Enter: request WS10 (never reached: cGame_exit 0x230488 switches the module first to PreGameLoadScreen (reload) or PreFELoad (front end)). Users: the results / popups Quit (arg 0), the peak-run Restart and the lodge Yes (arg 1, a reload). Port: `quitToTitle`, the lodge load screens. Match; the peak-run restart differs (rank 8).

### WS7: results (0x236DA0 / 0x236E60 / 0x236EA0)

| rule | PS2 | port | status |
|---|---|---|---|
| Enter | CTM: 145CB0 clears the new-career flag of every player; 2790A0(nis, 0); +8 = 0; 20A8F8(7): the chain 0xF (records) → 0x10 (rewards) → results | `career-ui.js finish`: firstRun = false, records → award → results | match |
| Update | +8 == 1 → WS14 arg 2 (Transport); the other items act from the overlay dispatcher (§1.4) | `resultAction` | match |
| Items | Next heat (WS13 now), Transport (command 4 → WS14 arg 2), Restart (overlay 0x17 → 238348 GMM+0x70 = +0x74, WS13 now), Replay, Records (0x23), Quit (0x18) | `resultItems` / `resultAction` (`career-ui.js:435`) | match (labels: ctm-parity) |

### WS10: world load and arrival (0x234F40, 0x235080, 0x2355C0, 0x2357F8, 0x235868)

| rule | PS2 | port | status |
|---|---|---|---|
| Pre-enter (push) | +0x18 = 0; 26F228; NIS list 0 cleared and the location's lists reloaded (278CD8 / 278B98); 12AB20 / 12AC48. CTM at a backcountry not yet visited (145D38): audio +0x5790 = 0, +0x6254 = +0x578C = 1, +0x14 = first visit. Outside CTM: 27AAF8(nis, 0) | `enterWorld` | match (the audio flags: audio agent) |
| Pre-update phase 0 | first visit: ABC1 movie 29 + groups 0, 1 (flags 3), 28CD48, 2B3A70; DBC2 / EBC3 movie 30 / 31 + 16, 17. A transport (streamer +0x1C8) into a visited backcountry: 16, 17 | `enterWorld` arrival lists, pv stationFlow | match |
| Phases 1..3 | lists loaded; riders 12ABD0 / 12ACF8; peak runs (modes 6..11): initGameMode; rebuild the AIs (128958 / 1296F8), place them (1289F0) for a race; rival: 128A10 | the peak-run / rival loads | match |
| Phase 4 | first visit: at least 180 ticks since the push | the movie + plane are longer | match (no visible effect) |
| Enter | +0x1C = 27AA80 (arrival pending); 158E30 clears the reward record; CTM: next = WS1 arg 1 at a backcountry, else WS4; outside CTM WS1 arg 0. Free ride: riders phase 3, cBE_setState(0), stage world 309030 / 309F18, a station course → 146E10 last lodge; 1F36D8(S+0x94, 0) ("Loading..." bit 0 off); plays list 1 on a first visit | `enterWorld` / `courseChanged` / `crossingArrived` | match |
| Update | the arrival done → streamer +0x1A4 = 1; list 1 not playing → next | `ride()` | match |
| Exit | 308F38 / 308C60; 26F228 / 26F7B8; CTM: 145DD0 sets the visited bit; event types 5 / 6: 238510(GMM, 0, 0) | `ride()` marks visited | match |

### WS11: crossing and held transport loop (0x2368A0 / 0x236960)

| rule | PS2 | port | status |
|---|---|---|---|
| Enter | cBE_setState(0); 26F228; CTM: clears the new-career flag (145CB0) | `courseChanged` | match |
| Update (transport only, streamer +0x1C8) | 22D278(destination ready) → 3A9658 (a 45000 cm streaming box at the rider's arrival point, [0x49DF88]) → wait until 3A9770 ≥ 20000 ([0x49DF8C]), the view fade idle, view+0x44 = 0 and the NIS not loading → 27A9F0 releases the held loop; 22D088(dest, 7); +0x1C8 = 0 | `free-ride.js transport` held loop until the rows are resident | match (structure; the port's rows arrive sooner) |

### WS12: podium (0x236F08 / 0x236F40 / 0x236F90)

- Enter: 28CDF8 (the winner's chartune), C+0x8C = 1. Update: list 0 ended (status 0 or 5) → WS7. Exit: C+0x8C = 0. Port: `finishCutscenes` 'podium' after preFade 30 (ctm-flow §4). Match.

### WS13: next heat / results Restart (0x235AA0 / 0x235CC8 / 0x235F20)

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| Enter | cBE_setState(0); 30B7F8 ends a Big Challenge; restartHeat 0x2382D8 (the handler's vt+0x10 = the round's heat init, give-up flags 0x5366D0 cleared); 230180 world reset | `restartToCard` / `resultAction(0)`, `cb.quit` | match (reset by the run restart) | - |
| Rival / peak / non-CTM | phase 3 at once → WS1 arg 3 → WS2 | pv ws13Rival; readyView (rank 4) | fixed behind ws13Rebuild | qa rival-restart |
| Riders | setNumberAI + the AI characters from GMM+0x40.. (the round's roster), 12AB20 (+12AC48 with ≥ 2 AIs), later 128958 / 1296F8 / 1289F0 | pv ws13Rebuild (`career-ui.js ws13Riders`) | fixed behind the switch (rank 1) | qa heat-lineup2: [0,11,10,5,2] → [0,11,6,7,1] → [3,0,11,8,9] = roundEntries |
| Lists | race: 27A860(nis, 1, 0) the gondola; 27AAF8(nis, GMM+0x98 ? 3 : 2) = [4, 5] before the final, else [5]; "Loading..." bit 3 on | heatSteps / stationFlow | match (caption: rank 7) | ctm-parity "Stations and heats" |
| Update | +0x14 counts ticks. Phase 0: 12ABD0, 12B030 (12B000 with ≥ 2 players at 0x535C04), 128958. Phase 1: all loaded → grid 1289F0, the streaming box at the start (122C28, 45000). Phase 2 (gondola): 3A9770 ≥ 20000, at least **120 ticks**, list 0 loaded, the view fade and the NIS idle → 27A9F0 releases the gondola. Phase 3: → WS1 arg 3 | the gondola held step `holdTicks: 340` (`cutscenes.js heatSteps`) | match (measured: PS2 released at t341, driven by the disc; the code minimum is 120) | to-final |
| Exit | "Loading..." bit 3 off | - | missing (rank 7) | - |

### WS14: station cut / transport / post-event map (0x236250 / 0x236418)

The door, booth, map and transport rules are in [ctm-parity.md](ctm-parity.md) "Stations and heats" and "The NIS rider hold". Additions:

| rule | PS2 | port | status |
|---|---|---|---|
| Enter | 30B7F8; riders phase 3; the human's +0x2F8 = 0 (cGame_restart zeroes every rider's +0x2F8 too); +0x18 = 23 or (arg 1) the destination `gp-0x1F34`; world reset 230180 when the destination is the current course | - | unconfirmed (+0x2F8) |
| Map mode | arg 2: `gp-0xB5C` = 3 when the course is a station (≥ 17) in free ride, else 4 | `openBooth` / `transportAfterEvent` (post-event Back ignored) | match |
| Departure | step kinds 13 / 19: "Loading..." bit 0 on; kind 14 / 20 (in the air): streamer +0x1C8 = 1 and 22CEA8(dest, 7) | the transport ride | match (the caption: rank 7) |

### WS15: same-location return (0x236058 / 0x236198)

- Enter: 30B7F8; initGameMode(GMM, 12); 230180 world reset; 11DE60(human, IF(0xA)+0x10, 2) + 11DF18(human, 1); the white fade effect (2E4CE8(0x14), colour 1, 1, 1, 1) started by 2E4370(view, 1, out 0, hold 0, in 1.0).
- Update: WS1 arg 0.
- Users: the map's same-location pick (post-event, booth) and Session.
- Port: `goWorld` same world → a world reload under the cover, then `fadeFrom 58`; Session → `freeRideSession` 60 ticks. The fade matches; the reload differs (rank 3); the Session world reset is unconfirmed (rank 12).

## 3. The event lifecycle by mode

### 3.1 The gate (0x22D6C8)

- **Conditions.** CTM, event type 4, the current course not a backcountry (14..16), volume kind ≠ 1 (0x445E40 entries 4..6), and WS == 4 (checked after a cBE_setState(0)).
- **On entry.** 12B180 commits and clears the free-ride score; C+0xC = 0.
- **The volume kind.** Its kind (table 0x47B450) sets event type / mode:

  | kind | event type / mode |
  |---|---|
  | 2 | 0 / 0 race |
  | 3 | 1 / 1 slope style |
  | 4 | 2 / 3 big air |
  | 5 | 3 / 2 pipe |
  | 6 | 5 / 4 rival |
  | 0 | nothing set: initGameMode(12) and WS1 arg 1 in free ride, i.e. no event |

- **Then** initGameMode (0x238160) and WS1 arg 1.
- **Port:** `main.js` gate listener. It takes the mode from the course (`standardMode`), not from the kind. Unconfirmed whether any volume has kind 0 or 6: equal on the shipped courses.

### 3.2 GameModeMan (0x237CF8, handlers at 0x536668)

- **Handler by game mode** (table 0x47C0F0):

  | handler | vtable | game modes |
  |---|---|---|
  | 0 freestyle | 0x47CFA0 | 1..3 |
  | 1 race | 0x47CF38 | 0 |
  | 2 free ride | 0x47CE00 | 12 |
  | 4 peak time | 0x47CD98 | 6..8 |
  | 5 rival time | 0x47CD30 | 4 |
  | 6 rival points | 0x47CCC8 | 5 |
  | 7 peak points | 0x47CC60 | 9..11 |
  | 8 / 9 multiplayer | 0x47CED0 / 0x47CE68 | - |

- **Handler slots** (fn at vt+4+8k):
  - +0x14 the heat init: 238E20 fs, 23A108 race, 23B170 free ride, 23B268, 23B6C0, 23BB98, 23C0D0;
  - +0x1C = 238C80, the shared reset: GMM+0x84 = 1, +0x70 = +0x74 = +0x98 = 0, rosters cleared, then vt+0x2C = 238DA8;
  - +0x44 the results: 239230, 23A760, 23B468, 23B8C8, 23BDB8, 23C2D8.
- **initGameMode(mode):** the old handler's +0x1C, then the mode's handler, GMM+0x84 = 1, restartHeat. Every gate, map pick, WS15 and peak-run load is a fresh event (pv freshEvent: match).
- **Only two paths keep the round:** Next heat and the results / pause Restart (restartHeat or 238348).

### 3.3 Rounds

- **The rosters.** Race: qualifier → semi → final. The rosters come from 0x23A4F0 / 0x23A668; the round entries are ported in `web/lineup.js roundEntries`, docs [career-events.md](career-events.md).
- **The final's rival.** It rides slot 1: PS2 `local/reference/pcsx2/characters/career/ARA1-final-zoe` (the final card → Cross → WS3): [mac, psymon, allegra, kaori, marisol].
- **Its data is ordinary.** Every leaf of those riders equals the Single Event tables (27 slot, 9 grid, 13 base, 317 const leaves). Only the rival's 13-leaf skin part was missing. The race courses' skin parts are course independent: ARA1 / BRA2 / CRA3 / DRA4 share 15-16 identical skins, and Mac's CRA3 / DRA4 part equals this extraction.
- **The exporter** (`tools/export_lineups.py`): `export-career` extracts the career countdowns; `build` takes the skin parts no Single Event state has from them, same course or course independent. It raises on any other difference and requires the same-course career riders back from the parts. `build --out` writes to scratch.
- **Tests.** test-lineups passes on the scratch data (123 states assembled exactly). JS `assembleLineup([3,8,2,1,14])` equals the PS2 riders.
- **Peak 2 finals (done 2026-09-30, career-rival agent):** derived career finals `characters/career/{CRA3,DRA4}-final-zoe` (Zoe, Nate, Psymon,
  Brodi, Griff, Elise) give Nate's skin part and grid[1][3f7fffff]; `build` checks their round / race-level leaves by rule (0x10C450 / 0x10C4F8)
  instead of the slot tables. New CRA3 / DRA4 lineups.json in scratch `local/career-rival/export`, behind pv careerRival. A Nate human's rival
  (Zoe) already had slot-1 data. docs/career-events.md "The peak rival in career events".

### 3.4 Pause, Restart, Give Up, Quit, DNF, time-out

- **Pause** = context 2 (the WS machine keeps ticking; everything else is frozen). Port: `paused`, match.
- **Restart** (popup kind 0):
  - peak runs: the world reload (rank 8);
  - from the pause: cGame_restart = 230180 + restartHeat + every rider's +0x2F8 = 0 + WS1 arg 2 (now) + 2870A0 audio;
  - from the results: 238348 then WS13 now.
  - Port: `restartToCard`. Match, apart from ranks 1 / 4 / 8.
- **Give Up** (kind 5): pop, command 3, 1253D0(human). Port match ([career-events.md](career-events.md)).
- **Quit:** results item 8 → overlay 0x18; popup kind 6 → cGame_exit(S, 0). Port: `ctm-quitsave` → title. Match.
- **DNF / time-out** (125228 / 125368): port match ([career-events.md](career-events.md)). The finish panel closes after 1.0 s instead of 3.0 (above).

### 3.5 Return to the world

| from | PS2 | port | status |
|---|---|---|---|
| results Transport → the map → same location | WS14 arg 2 (#152 group 11 where present) → map mode 4 → WS15 (session point 1, white fade 1.0) | `transportAfterEvent` → `goWorld` → reload + fade 58 | differs (rank 3) |
| another location | WS14's departure + in-air steps, streamer load, WS11 held loop, WS10 arrival | the heli across the world switch | match (ctm-parity) |
| a backcountry again | 27A860 heli, not WS15 (0x2365CC) | stationFlow | match |

## 4. Fades and transition timings (ticks at 60 Hz)

| transition | PS2 | port |
|---|---|---|
| gate → the fly-over | 30 out + 30 in (the fly-over's header record, 277980 → 2E4370) | preFade 30 + the fly-over's record |
| the fly-over's end → the approach | out 30, hard cut in | the same |
| the approach → the idle under the card | out 30, in 30 (runs under the frozen idle) | the same; cardFreeze keeps the fade on its own clock |
| card Cross → countdown | same tick (the WS2 exit and the WS3 enter in one 230F40 call); the card gone after ~5 samples | `startRun` on Continue |
| countdown | 180, digits at 60-tick steps, GO at C+0x1C = 0 | 180 |
| finish → panel | 179-180 | ~180 |
| panel → results | +405 / +285 (DNF), or the Cross edge from +225 | 408 / 288 |
| Next heat, gondola | ≥ 120 and the load (PS2 341) | holdTicks 340 |
| WS15 return / Session | white, in 1.0 (2E4370 f14) | 58 (return) / 60 (Session) |
| the first-visit arrival | ≥ 180 since the WS10 push | the lists' own length |

## 5. Runs made for this document

| run | what |
|---|---|
| `caps/card-idle` | card-final-95k.p2s, neutral 360 samples: WS2, #73 playing, the frame identical throughout |
| `caps/fin-idle700`, `fin-mash-cross`, `fin-idle-ui`, `fin-hold-ui`, `fin-dnf-ui` | race-q/fin.p2s (WS5 at tick 13709): idle, Cross every 10 frames, the UI trace, Cross held, the human's +0x480 poked 1 |
| `caps/final-countdown` → `local/reference/pcsx2/characters/career/ARA1-final-zoe` | the Snow Jam career final countdown (Mac in slot 1) |
| `qa/heat-lineup.mjs`, `heat-lineup2.mjs` (LINEUPS=… serves a scratch lineups.json) | the rosters per round |
| `qa/rival-restart.mjs`, `rival-pauserestart.mjs`, `peak-restart.mjs`, `peak-pauserestart.mjs`, `peak-probe.mjs` | the cards over the ready view |
| `qa/card-freeze.mjs` | the idle under the card |

## 6. Overlaps (other agents)

- The finish panel, results, card and reward overlays' own art and animation: the CTM screens agent. Here only their timing and the state flow.
- The Big Challenge end at the gate / heat / transport (rank 9) and MCOMM / NIS triggers: the scripted free-ride content agent.
- The world-load stalls and the post-Transport freeze: the area-entry agents. Ranks 2 and 3 are the structural cause they work around.
