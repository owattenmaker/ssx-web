# Conquer the Mountain: an event from free ride, start to finish (PS2 and browser, 2026-09-25)

How the original starts a Conquer the Mountain (CTM) event when the rider rides into a course's start area in free ride,
how it layers the cutscenes, card, HUD and audio, how the rounds chain, and how the player gets back to free ride; then
how the browser reproduces it. SLUS_207.72, gp = 0x4A30F0. "(inferred)" marks claims not proven by code or capture.

Ground truth (session scratchpad, not in the repo):
- the full code trace `ctm/trace.md` (every address below, quoted instructions);
- PS2 runs (ARMSX2, derived savestates only): `cs/ps2b/CAPTURES.md` (Snow Jam: `intro`, podiums, rival, gondola heats, lodge,
  booth) and `ctm/caps/CAPTURES-ctm.md` (card to countdown `countdown`, Metro-City entry `metro-a`/`metro-intro`, results /
  restart / transport / map back to free ride, intro audio recording);
- the sound-call logs `ctm/audio/NIS-AUDIO.md` (every 0x2906B8 voice start and speech request with the NIS script time);
- side-by-side sheets `ctm/sheets/ara1-flow-sbs.png` (Snow Jam) and `ctm/sheets/bra2-flow-sbs.png` (Metro-City).

Names: G = `[0x4A28A8]`, world S = `[G+0x84]` (world state `S+0x214`), GMM = `[G+0xC0]` (round +0, final +0x98),
nis = `[gp-0x84C]`, settings 0x535C10 event type / 0x535C11 game type (0 CTM) / 0x535C12 game mode (12 free ride).

## 1. The gate (no prompt, no load)

- Every event course has a `*_RaceRideState_0` trigger volume at its start area. Its program calls stage **builtin 67**
  (handler at 0x302048 in 0x2FC2C0): human rider only, key1 = 0, key0 through the entry table 0x445E40 (value 1 = the
  FreeRideState volumes, rejected). It ends an active Big Challenge (30B7F8) and calls **0x22D6C8**.
- 22D6C8 needs CTM, free ride (type 4), a course outside the backcountry and world state 4. It sets the event type
  (144DF0) and game mode (1451E8) of the course, commits then clears the free-ride score (12B180), swaps the free-ride
  handler for the race/freestyle one (238160 → e.g. race init 23A108) and requests **world state 1 arg 1** (`231250(S,1,1,0)`).
- There is no prompt, no pending-event record, no world load, no streaming call: the Snow Jam rows stay resident, the
  free-ride tick keeps counting. World state 10 (the Single Event / reload path) is not used.
- PS2: Snow Jam ps2b/intro WS 4 → 1 at s735; Metro-City metro-intro at s171 (a neutral pad rides into it on its own).

## 2. World state 1 (enter 0x2341D0, update 0x2343B0, exit 0x234750)

- Arg 1 queues the CTM intro list (27AAF8, table 0x481E48): **[2 venue fly-over (flags 0)] → [3 approach (flags 3)] →
  [5 start-gate idle (flags 0)]**. The approach is chosen at random every time (ARA1: 8 `_ara1fast` variants, Metro-City
  #100 then #78 and #109 on later entries).
- The computer riders are created a tick after the gate (128958 / 129E20) and load one step per tick (1296F8 → 11C298)
  while the fly-over plays. "Loading..." (0x1F30D8, world-state bit 2) stays up until every rider is loaded (12A180);
  if the fly-over ends first the list is held (279040 → 277C08). Snow Jam: loaded at #94 t≈258-262 (caption gone at
  s1035); Metro-City: a 7-tick black hold after #95 (s473-479).
- Riders are placed on the grid only when the list reaches the idle (1289F0 → 1297C8 → 11D390, start-grid control 6).

## 3. Layering, frame by frame (Snow Jam ps2b/intro, Metro-City ctm/caps/metro-intro)

| moment | what is on screen | code |
|---|---|---|
| WS1 enter (s735 / s171) | the free-ride view keeps running (Zoe rides on), the HUD is hidden (HUD cmd 0 at every NIS step start, 276388), the bars slide in (2 lines a tick, full after 30), yellow **"Loading..."** with the turning snowflake, and the view **fades to black over 30 ticks** | the fly-over's header fade-in record {out 30, hold 0, in 30}: 277980 → 2E4370; the script clock starts after out + hold (+0xB8) |
| fly-over t0-30 (s766 / s202) | fades in from black, bars full, "Loading..." bright over the fade | same record, in 30 |
| fly-over t0 | music code 21 (24 at Metro-City, 23/25 freestyle): the free-ride song fades out over 1 s, the next playlist song is forced 3 s later (PlayMusic 36) and brings the **EA RADIO BIG** box (Snow Jam t≈227, 8 s) | 280640 → 28E8C0(21,0) → 28D488, 2B3D48(1.0), 2ADCA0(3000, kind 3) |
| fly-over t30 | PA_Venue_Intro (0x20BB) | kind-7 cue 0xF → 2A19D8 → 2A39E0 |
| fly-over t0..181 | the script's own sound (bank slot 18 = the location's `scdat_<LOC>`, sound 0, 3.24 s) on the CHARACTER bus at speaker 0's gain | 280F3C 287968(audio,4,0) → 2906B8 |
| fly-over end | fades to black over its last 30 ticks (fade-out record out 30, in 0) | 277DE8 → 2E44F0 |
| approach t0 (s1043 / s480) | **hard cut, bright at once** (the fly-over's fade-out in = 0; the approach's own fade-in record is not used), "Press ✕ to skip" | 2E44F0 rewrites the running fade's in phase |
| approach t0 | PA_Rider_Race_Intro (0x20C2; freestyle approaches: PA_Rider_Intro 0x20C1) for the player | cue 2 → 2A3358 (cue 1 → 2A32B0) |
| approach end | fades to black over 30 | fade-out record {out 30, in 30} |
| WS2 (s1286 / s722) | the start-gate idle #73 fades in over 30 (the approach's fade-out in 30) under the **round card**, which grows in: title bar from the left over ~12 ticks, frame, box, text at ~33; Cross is ignored until it is open | WS2 enter 236BB0: overlay 8 "40race_pre", pause context 1 (idle and world frozen, #73 time 1.0) |
| card enter | UI sound 0xE; PA_Sponsor_Intro (qualifier only) | 1FB588 → 2A31C0 |
| Cross | card gone after 5 samples, hard cut to the race camera, race HUD (flags 0x1530C047) and the EA RADIO BIG box back; WS 2 → 3 | WS2 exit 236CD8: 2790A0 stops the idle, HUD cmd 1 + 5, race camera (vt+0x28(3)), 2871B0 race audio |
| countdown | digits every 60 ticks from t3 (3, 2, 1), GO at t182; beeps 0x4E; GO: DJ artist intro (round 1), PA_Medal_Run_Intro (final) | WS3 234AD0 / 234C68, 29C420, 29C7B0 |

No DJ line, Radio_Big_Intro or Free_Ride_Intro is posted at the gate (no 2867E8 world load). PA lines do not duck.

## 4. After the race

- **Finish** (WS5 0x233C50) → **podium** (final only, 1st-3rd, WS12: header fade {out 30} over the finish view, then
  win_ps_<winner>/placeshow; PA_Medals cue 7 at t0; winner's chartune 28CDF8) → rival challenge when due → **results** (WS7).
- **Results items** (template 0x46E488 {1,5,6,7,8}, item 0 rewritten by 1E5AA0): "To semi final round" / "To Final Round"
  when advancing, else **Transport**; Restart; Replay; Records; Quit.
- **Next heat** (WS13 0x235AA0): world reset, `gond_inair` #150 → `gond_inair_<char>` #146 held until the heat is loaded
  (PS2 released at t341, fading out before), then WS1 → [5 idle] under the card (semi); the final queues **[4 start hut
  var1/var2, 5]** (0x235C58, `GMM+0x98`).
- **Restart** (confirm) from the results = the same gondola ride-up, then the card (PS2 ctm/caps sj-restart-yes,
  menus/race/r3-results-restart; the code path 0x20D7DC → 2302A8 → WS1 arg 2). From the **pause** menu there is no ride:
  the lit start-gate idle at once, then the card (menus/race/r3-restart; pv `pauseRestart`).
- **Transport** (0x20CF80 → WS14 arg 2, 0x236250): list 1 group 11 = `endevent_trans_arr` #152 where the course has one
  (Metro-City: Zoe steps off and takes out the MCOMM, fade to white; **Snow Jam's list is empty**), then the map (overlay
  0x21). Choosing a free-ride row **or the same event again** restores free ride (2018A8: event type 4, mode 12, the
  free-ride handler): the same course = WS15, `11DE60(rider, 1, 2)` (session point 1) and a **white fade** (~58 ticks);
  another location = the gondola / heli transport. The rider then rides into the gate again to start an event.
- Quit asks "Save progress before quitting?" and goes to the front end.
- Post-event DJ commentary (2A45C0 records, 2A4770 plays it at the next hub chatter).

## 5. Browser implementation

The browser's event is a separate course package with its own core (computer riders, race session, start-gate set
pieces), so the ride-in cannot run in the streamed world itself. The fly-over plays in the streamed world and the course
switch runs behind it, with no load screen:

| step | browser | files |
|---|---|---|
| gate | builtin 67 → `free-ride.js` 'gate' → `main.js` → `careerUI.rideIn(mode, course, {pause})` | main.js (gate listener), career-ui.js |
| WS1 | `cutscenes.preFade({ticks: 30, loading: true})`: bars, caption and the fade drawn over the running world on the 'game' screen (ui.js draws `cutscene.overlay` instead of the HUD); the world then stops without a pause menu (`paused = true`, the music plays on) | ctm-event.js, cutscenes.js, ui.js |
| fly-over | `playCutscene({kind: 'intro', mode: 'flyover'})` in the streamed world (the location's scdat / scfilter list 2; "Loading..." over it; bars stay full) | ctm-event.js |
| course switch | `ui.loading.world` = `cutscenes.drawCover` (black + the world's caption, no Basic Controls screen, no 7 s minimum); `gameAudio.carryWorld(true)` keeps the fly-over's song through the teardown and the next world load; `save.pending.rideIn`; `ui.cb.course(entry)` = main.js `navigateCourse` (in-page switch) | loading-screen.js (world mode), game-audio.js, ctm-event.js |
| resume | `careerUI.resume()` → `begin(mode, course, true, {rideIn})` → `ui.loadEvent` (the world-mode session continues) → `cb.intro` → `playEventIntro` plays **'career-ridein'** = [approach, idle] with the fly-over's fade-out record (bright approach) and full bars | career-ui.js, cutscenes.js |
| card | the idle's fade under the card (`cutscenes.drawFade`), the card's grow-in (`drawObjectivesOpen`: title bar 3-15 ticks, frame 24, box 27, text 33), Cross ignored for 30 ticks | career-ui.js |
| countdown | unchanged (startRun; the carry clears at `runStart`) | main.js, game-audio.js |
| results Transport | `transportAfterEvent()`: #152 via `cb.cutscene({kind: 'transport-arrive'})` (none at Snow Jam), then Select Peak; a course event or a free-ride row → `goWorld(course)` under the world cover, and free ride fades in from white (`cutscenes.fadeFrom`, 58 ticks) at session point 1 | career-ui.js |
| Restart | confirm, then `restartToCard()`: the gondola ride-up (`kind: 'heat'`), then the card | career-ui.js |
| next heat | `heatSteps(final)`: the final adds the start hut | cutscenes.js, career-ui.js |
| podium | `preFade({ticks: 30})` over the finish view before the podium list | career-ui.js |
| Transport list in free ride | an event on a ported course transports to that course in the world (the rider then rides into its gate), as the PS2 does | career-ui.js (ctm-confirm) |

Cutscene engine changes that apply to every cutscene (cutscenes.js):
- **Fades chain like 0x277980**: a list's first step uses its own fade-in record; a later step fades in with the previous
  step's fade-out record's in ticks (`fadeAt`, tested). The fade-out also runs before a looping script played once
  (gond_inair) and before a held step's known release.
- **A step's clock starts when its actors, bank and set are loaded** (`seq.ready`). Before, the clock ran during the
  asset await and the step's sounds were requested before the bank existed; the failed request was latched, so NIS
  sounds almost never played.
- NIS sounds: CHARACTER bus at speaker 0's gain (0x280F3C); actor sounds positional at the actor root in PS2 cm (they
  were passed in scene metres, so their distance gain was ~0); a request that fails because the audio or bank is not
  up is retried the next tick.
- PA cues of 0x2A19D8: 1 Rider_Intro, 2 Rider_Race_Intro, 7 Medals added (audio-speech.js `riderIntro`, `riderRaceIntro`,
  `medals`); music codes: `gameAudio.cutsceneMusic(code)` (19/20 nothing with a2 = 0; 21-25 PickNextSong, FadeOut 1 s,
  forced PlayMusic(36) after 3 s; others = a Pathfinder event).
- The world sounds keep running under a cutscene when the game ticks do not (event intros before the run): ambience of
  the course's own location, emitters and crowd loops (`gameAudio.cutscene(on)`, `nisWorldTick`); PS2 NIS-AUDIO (a).
- "Loading..." is drawn over the fades, its snowflake turns about its vertical axis; bars stay full after a pre-fade and
  across a list's steps.

QA hooks (cutscenes.js): `qaScripts[group] = script` forces a list choice, `qaAlts[script] = [...]` forces track
alternatives, `freeze()` also holds the pre-fade, `seekOverlay(t)`.

## 6. Verification

- `web/test-cutscenes.mjs`: ride-in / final-heat / restart lists, the fade chain against the PS2 frame means, the flow's
  kind-7 cues. `web/test-game-audio.mjs`: music codes 19/20/21 and the carry across a world switch.
- Side-by-side at matching script times (browser: the real flow from free ride, frozen at each time; PS2 alternative
  matched by mean difference): `ctm/sheets/ara1-flow-sbs.png` (free ride → WS1 fade → #94 → switch → #66 → card →
  countdown) and `ctm/sheets/bra2-flow-sbs.png` (Metro-City: WS1 fade → #95 (camera alternative 2) → #100 → card). The
  gate fires naturally in both (Snow Jam after the PS2 capture's pad: 286 neutral + 20 left from session point 1;
  Metro-City with a neutral pad).
- Per-bus levels (peak per second, Chrome; `ctm/chrome-*.txt`): ride-in: CHARACTER 0.20-0.35 during #94 (its bank
  sound, silent before this work), PA at #94 t30 (venue), #66 t0 (rider race intro), the card (sponsor); MUSIC off for the
  1 s fade then the new song through the switch; UI bus (ambience, crowd) throughout. Single Event intro: UI bus live
  (ambience + crowd), CHARACTER 0 (the PS2 has nothing there either). Podium #60: PA (Medals) at t0, CHARACTER from t240;
  gondola #146: CHARACTER.

## 7. Gaps

(The career around the event (start, rewards, quit, lodge, peaks) is compared with the PS2 in [ctm-parity.md](ctm-parity.md).)

- The fly-over plays in the streamed world, then the page switches course (~5-10 s of black with "Loading..." after the
  fly-over, the PS2 holds at most a few ticks). Measured and partly addressed in [presentation.md](presentation.md)
  section 3 (downloads under the fly-over, `?pv=flyover`); the load's compute (~5.5 s) remains. The computer riders do not appear in the fly-over (no event lineup in the
  free-ride world); the PS2 shows them tiny at the gate.
- The free-ride song's DJ chain can still be talking when the fly-over's PA venue intro is posted; the one speech voice
  then drops the PA line after its 180-frame request life (as the PS2 would; not seen in its capture).
- The in-air transport cut is not played on the way back to free ride from the post-event map (the course switch is under
  the world cover), and #152 plays only where the course's list 11 has it (not at Snow Jam, as on the PS2).
- The card's grow-in is a clip reveal of the existing card drawing, not the LUI "40race_pre" animation; the approach's
  "Press ✕ to skip" shows from t1 (PS2 ~t8).
- The stage-script loops (bank 4 sound 17) that start at the PS2 intro's first tick come from the core's stage world,
  which does not tick before the run in the browser.

## 8. Start and streaming (2026-09-27, CTM-stream agent)

Owen's reports: the track sections of the streamed world appear late (the rider floats on nothing at Green Base Station
while the first help message is up); the first CTM movie plays the menu music instead of its own sound; T-poses on the
first load. Switches (web/pv-flags.js): `ctmWorldAudio`, `streamGate`, `streamAhead`, `streamWarm`, `sliceLoad` (all off
until the coordinator turns them on) and `lodgeWorldLoad` (on).

### 8.1 The PS2

- **New career audio** (music/runs/newcareer, samples at 60 Hz): s50 FadeOut of charsel, s82 the loading loop, s573
  pktrans starts (event 1, paused), s594 loading off and the world load 2867E8 (no song pick), s629 world state 10
  resumes pktrans, s650..2325 the ABC1 movie: **its MPC has no audio track**, pktrans is what plays under it and under the
  plane NIS; s3248 the cinematic's end sends 28E8C0(19); the spoke DJ 2.5 s and the Peak1 hub song 3 s after it.
- **Streaming** (docs/peak-mountain.md "Streaming"): one disc read at a time (sky > TRANSP > location, then the lowest
  chunk). After the ABC1_A Unload trigger the A row's reads finish at A t71, A_ABA1 100, A_ARA1 132, A_ASS1 154,
  DRA4_A 180; the Load trigger comes 368 (race) to 629..665 (free ride) ticks after the Unload. A row that is active
  without its data holds the game at the +0x1D0 wait (2306B8); the PS2 never needs it on these rides.
- **Lodge Return to Game** (ctm-parity lodge-return, lr-tri): entering the lodge tears the world down; Return to Game is
  0x1A11C0, the free ride at the last lodge 146D98, the same world load as the CTM last-lodge start: the save,
  118loadoutlodge (~8.5 s on disc), WS10 then WS4 a tick later (no cinematic), a new rider (0x125EB8) at 11D390's station
  entry (session point 0), the lodge door's world state 14 at tick 394 with a neutral pad.

### 8.2 The port's causes and the changes

| problem | cause | change (switch) |
|---|---|---|
| menu music under the movie | the free-ride world load's audio ran at the ride start (runStart), after the movie and the plane NIS: charsel played on through both | `game-audio.js freeWorldLoaded` when the load screen closes on a streamed world (main.js audioScreens): pktrans under the movie and the plane, 19 at the ride start (`ctmWorldAudio`) |
| floating on nothing at the station | the draw package was built only once the row was wanted, and `renderer.compileAsync` compiled nothing useful: it culls through the camera frustum, keys its render contexts by call depth (the world pass draws at depth 1 inside the post pipeline) and builds a transparent DoubleSide material's deferred objects after its side is restored. Every pipeline was built on the first draw: A took 36 s in WebKit, several seconds in Chrome, while its collision was already active | `fog-renderer.js compileObject` at depth 1, `free-ride.js warmSliced` (unculled, two-pass materials as BackSide then FrontSide, batches in a detached group) (`streamWarm`); the next row's draw files downloaded and built under the movie and the plane (`streamAhead`); an active row is held until drawable, as the +0x1D0 wait holds it until its data is in (`streamGate`) |
| sections late down the mountain | every wanted location was fetched, parsed and fed at once, and the whole peak's collision was prefetched at the start | `streamAhead`: the PS2's order (`aheadLocations`: the rows the current row's connectors lead to, lowest chunk first; at a station the connector nearest the rider by its terrain bounds, 25 m hysteresis), one background job at a time, none while a wanted row lacks data; the rest of the peak's collision only while nothing animates |
| hitches while building | builds and feeds ran unbudgeted; a location's rail catalog was one core call (80-220 ms at 4x); the locations a connector leaves were disposed in one frame 45 s later | per-frame build budget (`free-ride.js buildBudget`: stalled 36 ms, wanted 10, ahead 4, idle 24), the pump sized the same way; warm batches sized to the budget left at the measured cost per mesh; releases one location a frame, disposed in 2 ms steps (`streamAhead`); the rail catalog in ~24 KB parts (`sliceLoad`, core `init_rails` segment_base / partial) |
| long load-screen tasks | one core call per document: `_init_stage_world` 0.3-0.9 s, `_init_environment`, the first location's merged init 0.9-1.5 s at 4x CPU | `sliceLoad`: `stage_world_part` (the stage world in parts cut from the documents' own text in the worker), `environment_add` parts, the first location as a plain init of its first slices plus appended slices, the path banks one per frame (web/load-slices.js, web/peak-world.js beginSliced) |
| lodge exit | Return to Game respawned at session point 0 (old rider, no ride start): 2.8x too fast, and charsel played on in the world | `freeRideWorldLoad` (main.js): a new rider at the 11D390 entry and the world start's ride start (`lodgeWorldLoad`) |

The core stays bit-exact: `web/test-ctm-stream.mjs` compares the sliced loads with the one-call loads by core hashes
(`stage_world_load_hash`, `environment_load_hash`, `rail_load_hash`: PEAK1 / ARA1 / ABC1, the streamed rail catalogs A..ASS1),
and ?simtrace=1 gives 900 identical ticks with every switch on against a baseline taken next to it.

### 8.3 Measurements

Chrome headless (--mute-audio), WebKit through web/webkit-driver.mjs, ?mute=1. Frame times p50 / p95 ms and the count
over 100 ms; "phone" = 844x390 at DPR 3, quality=low, 4x CPU throttle.

- **The station on arrival** (WebKit dev): A's pipelines 36 s after its activation, with the FAQ over the undrawn station
  -> built under the movie, drawn at the Load trigger; the load's worst frame 935 -> 243 ms, the station's 540 -> 186 ms.
  Production build at 6 Mbit/s (Chrome): A drawn 7 s after its activation -> downloaded under the movie, built in the
  plane, drawn at the Load trigger.
- **The new-career flow, final tree** (dev server, no screenshots during the run; all five switches off -> on):

  | phase | Chrome phone 4x | WebKit desktop |
  |---|---|---|
  | load | 17.6 / 103.7, 22 (worst 995) -> 17.0 / 73.4, 12 (worst 246) | 28 / 76, 4 (worst 813) -> 21 / 40, 1 (worst 114) |
  | movie | 16.7 / 18.0, 2 -> 17.6 / 36.2, 5 (builds under the movie) | 17 / 18, 0 -> 17 / 23, 1 |
  | plane | 16.7 / 25.1, 2 -> 16.7 / 20.8, 4 | 17 / 18, 2 -> 17 / 22, 0 |
  | ride Happiness | 28.4 / 64.7, 2 -> 19.3 / 32.0, 1 | 17 / 19, 0 -> 17 / 21, 0 |
  | ride station | 24.5 / 91.6, 10 (worst 331) -> 18.5 / 39.0, 5 (worst 132) | 17 / 21, 0 -> 17 / 27, 1 |

  (WebKit's own screenshots stall its main thread ~100 ms each: runs with shots every 250 ms show periodic 100 ms frames.)
- **PEAK1 load screen at 4x** (Chrome, the long tasks by CPU profile): worst 1092 ms (`_init_world_collision` +
  `_init_body_terrain` + `_init_terrain` of the first location), 633, 592 (`_init_stage_world`), 558, 425
  (`_init_environment`), 367 (`_init_world`) -> worst 417 (the rider's `_init_animation` / `_init_race`, first-load agent),
  258 (`_init_world` of the base course), the rest under 160 ms; frames over 100 ms 15 -> 12; load 16.2 -> 14.0 s. The
  builds give the load screen a frame at least every 20 ms (task yields alone let Chrome run them back to back).
  ?simtrace: 900 ticks identical, off vs on.
- **Connector crossings** (scratchpad connectors.mjs: placed at the connector's AIP path start, the PS2 music-drive
  autopilot through it; the worst case places the rider into A_ARA1 the moment it arrives at A from Happiness): worst stall
  **0** at every Peak 1 connector with all the switches (ABC1_A, A_ARA1, A_ASS1, A_ABA1, ARA1_B, B_BRA2, B_BHP1 and the
  worst case), phone and desktop. With only `streamGate`: 1.27 s at A_ARA1. The riding frames' collision feed: at most
  13.7 ms in a frame at 4x (was 200 ms: BRA2's rail catalog in one call); releasing the locations a connector left (45 s
  later, several at once) at most 4.7 ms a frame (was 165 ms). Frames after each placement's first 1.5 s (the teleport's
  own first draws), p50 / p95, count over 100 ms:

  | connector | phone 4x | desktop |
  |---|---|---|
  | ABC1_A | 43.6 / 88.7, 22 | 16.7 / 16.8, 1 |
  | A_ARA1 | 20.2 / 33.0, 0 | 16.7 / 16.8, 0 |
  | A_ASS1 | 29.2 / 59.7, 18 | 16.7 / 16.8, 0 |
  | A_ABA1 | 22.1 / 36.3, 0 | 16.7 / 17.1, 0 |
  | ARA1_B | 22.7 / 89.6, 18 | 16.7 / 17.1, 1 |
  | B_BRA2 | 20.5 / 34.8, 0 | 16.7 / 16.8, 0 |
  | B_BHP1 | 28.6 / 44.4, 8 | 16.7 / 18.4, 0 |
  | ABC1_A then A_ARA1 at once | 16.7 / 19.8, 33 | 16.7 / 17.9, 1 |
- **The lodge exit** (`lodgeWorldLoad`, neutral pad after Return to Game, against the PS2 world-load ride
  peak1-green-start): 0.02 cm through tick 285 (then the known gameplay-RNG divergence), speeds 2.67 / 7.526 / 13.876 m/s
  at ticks 3 / 43 / 203 as on the PS2, the door at ~394; Peak1 and DJ kind 2 after the fade. Before: 3-10 m off, 7.57 m/s
  at tick 3, charsel for good.

### 8.4 Event loads in parts (2026-09-27, pv `eventSlices`, `feCompileSpread`)

An event's load screen ran the course's core world as single calls. At 4x CPU on a phone each was several hundred ms:
`init_terrain`, `init_world_collision` and `init_body_terrain` together made one 1-1.8 s task (terrain.json parsed twice), plus
`init_world` and `init_rails`. Each computer rider's context also copied the multi-MB course texts into the core and parsed the
rail catalog again. The human rider's `_init_animation` parsed three documents in one call.

**Core** (checked by `web/test-event-slices.mjs`, in test:all):
- `init_world_begin` / `init_world_step`: the collision.bin triangle tree built a few nodes a call. The nodes come out in the
  recursive build's order: pre-order, left subtree first, a range's `nth_element` when its node is made. `world_load_hash` is
  equal. The same switch steps a streamed world's base course.
- `event_world_begin`, then the head documents through the ordinary calls (no patches or instances: course selection, track
  locations). Then in append mode: the world parts (`init_world_collision`: whole instances with the descriptors they use,
  renumbered in the part, and their meshes and hierarchies) and `terrain_part` (one parse feeds both terrains).
  `event_world_seal(worldKey, terrainKey)` commits the terrain orders, then runs the whole calls' tails in their order: the
  collision parse cache (no terrain in it), object routes, set pieces, then the body terrain and its parse cache.
- Rails: the head through `init_rails`, the parts ("partial", `segment_base`), then `rails_seal(key)`: `init_rails`' tail
  (the teeters, the static records) and a shared rail parse cache.
- `init_world_collision_cached` / `init_body_terrain_cached` / `init_rails_cached(key)`: a computer rider's context takes the
  parse caches by the whole documents' parse keys (`parse_key`, computed off the main thread over the files' bytes), without
  their text.
- `animation_prepare(text)`: an `init_animation` / `init_race` document parsed in its own call. The next call with that text
  takes the parse (the same json). An unprepared text is parsed in the call as before.

**Page:**
- `web/load-slices.js`: `cutEventWorld` (the files come through downloads.js, their bytes go to the peak-world worker,
  which cuts the parts from the files' own text), `feedEventWorld`, `feedEventRails`, `initWorldStepped`. The work per frame
  is 36 ms less the last yield (the frame's render), between 16 and 30 ms.
- `main.js`:
  - the course asset build pauses on the same budget; its PNGs download together and decode one per slice;
  - the human rider's documents are prepared one a frame (`prepareRiderAnimation`);
  - the warm-up's new materials per frame follow their measured cost.
- `ai-race.js` / `ai-racers.js`: the rider contexts copy the parse caches by key (the texts are fetched only on a miss). The
  settings and animation documents are prepared ahead, and the setup yields a frame once a frame's work is done.
- pv `feCompileSpread` (`fe-preview.js`, `character-select.js`):
  - A preview model compiles part by part, with frames between; its pipelines are awaited together.
  - The character select's roster prefetch stops when the rider screens close. It had kept compiling nine riders under the CTM
    world load: 73 builds, 0.9 s at 4x.
  - The hitch after Cross was the shown rider's own compile finishing: 8 builds, 345 ms in one task.

**Exactness:**
- Plain == parts for all 17 event courses (`world_load_hash`, `body_load_hash`, `rail_load_hash`), parse caches included.
- A rider context from the caches by key == one from the texts.
- init_animation from prepared parses == from the texts (Zoe, Mac, Psymon: 90 ticks of pose, info, RNG).
- In Chrome the ARA1 load hashes of the human and the five computer riders are equal off vs on (`?simtrace=1` records them).
- Targeted gate on the scratch cores, including test-ps2-captures, long-runs, lineups, rival-page, ai-racers, stage-world,
  shared-parse, peak worlds and course-spawn: all pass.

**Measured** (Chrome headless, `--mute-audio`, `?mute=1`):

Snow Jam, phone 844x390, quality=low, 4x CPU:

| | before | after |
|---|---|---|
| frames over 100 ms | 31 | 12 |
| frames over 50 ms | 51 | 40 |
| worst frame | 1017 ms | 708 ms |
| load | 15.0 s | 14.9 s |

What remains over 100 ms is not the course world:
- the first draw's GPU uploads of the shared vertex buffer (createBindGroup / createAttribute, ~700 ms): performance agent;
- full-scene warm-up draw frames;
- the human rider's `_init_animation` build.

Production build over the shaped link (`ctmedge.mjs`), desktop Chrome, first visit / repeat visit:

| case | before | after | frames over 100 ms, before → after |
|---|---|---|---|
| Snow Jam event, 50 Mbit/s | 15.2-16.4 / 9.5-10.3 s | 15.4-16.8 / 9.1-10.8 s | 5-21 → 1-3 |
| Snow Jam event, 6 Mbit/s | 71.3 / 18.7 s | 71.5 / 18.3 s | 5 → 2 |
| CTM at Green Base Station, 50 Mbit/s | 15.1 / 9.8-10.7 s | 14.4-15.6 / 10.0-11.3 s | 5-6 → 0-1 |
| CTM at Green Base Station, 6 Mbit/s | 87.8 / 17.4 s | 73.4 / 15.6 s | 6 → 1 |

- CTM "before" is without `sliceLoad`: the base course's terrain / collision / rails were downloaded and built.
- The machine was shared with other agents' runs. The 50 Mbit/s times vary by ±1 s, so the load times are equal within that
  noise.

WebKit (web/webkit-driver.mjs, desktop):
- Every load completes with the switches on (Snow Jam, Metro-City, CTM at the station).
- Snow Jam on an unloaded machine: frames over 100 ms 6 → 2, worst 274 → 202 ms.

**Hang hunt before the switches went on (2026-09-28).** Once, in the load-time runs, a repeat-visit page stopped answering CDP:
`Runtime.evaluate` and `Debugger.pause` got no reply for over 10 minutes, including from a fresh connection. A JS or wasm busy loop
takes `Debugger.pause`, and no renderer of that Chrome showed the CPU time a busy loop would build up, so a crashed or wedged renderer
fits the symptoms better. It did not recur.
- **Stress runs** (scratchpad `stress.mjs`, switches on, every 500 ms a state query with a 10 s limit; no answer = hang, with
  `sample` of the renderer / GPU processes and the last CPU profile piece): no hang in any of these.
  - 45 Chrome repeat-visit loads on the production build at 50 Mbit/s, across Snow Jam, Metro City and CTM at the station. Variants:
    a warm cache; a cleared cache with a random half of the course files fetched back; the tab behind another for 4 s mid-load; a
    course switch mid-load; `?rider=psymon`, whose rider.json the computer riders share, so their prepared-parse keys are shared.
  - 36 WebKit loads (web/webkit-driver.mjs: warm, course switch, shared keys).
  - A WebKit new career through the menus (character select, preview compiles spread).
- **Loop audit:**
  - Every frame yield (`nextFrame`) falls back to a 50 ms timer, so it also advances in a background tab, throttled.
  - `init_world_step` pops one task per node and splits a range into two strictly smaller ones.
  - The text cutters' loops are bounded by their input.
  - The warm-up slice always advances by at least one drawable.
  - A failed worker cut falls back to the whole documents.
- **Found, not caused by these switches: a course switch during the boot load left the page stuck.** The screen stayed on 'game'
  with no core. The errors were "Some terrain patches cannot be replaced" and `_malloc` of null in loadCourse or
  `initEnvironmentSliced`. It was not a busy loop: the page still answered. It happened with the switches off, and on the build of
  2026-09-27 12:19, before this work. The boot load was not on the switch chain, so the second load ran alongside it and both wrote
  the course globals.
- **Fixed, pv `bootChain` (on 2026-09-28; `web/main.js`):**
  - Every course switch asked for before `init` has loaded and set up the page's first course waits on a gate. That covers
    `ui.cb.course`, CTM (`cb.freeRide`), a career world, Back / Forward and a lobby. The switch runs after the boot course, never
    alongside it, and the boot course's event is skipped because the switch replaces it.
  - The lazy first course opens the gate at once; it is already on the chain and is abandoned by a switch. Its sliced loads
    (`initWorldStepped`, `feedEventWorld`, `feedEventRails`) now check the abandon guard after every frame yield, so it stops
    within a frame.
  - Back / Forward is handled from the start. During the boot, going back to the page's own course drops the switch asked for
    meanwhile; any other entry is queued.
  - A switch away from the streamed world lets its pipeline warms that are still in flight settle before the course's passes are
    disposed (`free-ride.js` `stop` / `settle`, at most 1.5 s). They had failed on a disposed depth target.
  - Online: the lobby's course switch (`net/mp-game.js`) exists only once init has created the online game (after the boot
    load), and it is a `navigateCourse` on the chain. An invite link is a page load.
- **Checked** (stress harness, dev server and production build, Chrome and WebKit). Switch variants: a course switch 2.5 s into
  the boot load; a switch during the boot course's event load; CTM picked mid-boot; a switch then Back; the lazy course
  replaced from the menus.

  | | stuck |
  |---|---|
  | before, same harness | 8 of 15 in Chrome, 8 of 15 in WebKit (switch 3/3, CTM 3/3, Back 2/3) |
  | after, `bootChain` | 0 of 108 in Chrome (78 dev, 30 production), 0 of 48 in WebKit |

  After the settle change, no page errors either. Targeted tests pass with the switch on: lazy-course, world-loader,
  course-spawn, downloads, the mp tests, ctm-flow, event-slices, peak-world, visual-parity, rider-prefetch, ride-warm,
  shared-parse, ai-racers, boot-progress: 25/25.

### 8.5 Open

- **Long frames while a location builds on a phone**: single node-material builds inside `compileAsync` (NodeBuilder),
  40-55 ms each at 4x, which a batch cannot split; with ~30-60 ms of render and 20-40 ms of tick those frames are
  ~150 ms. Cheaper or shared node materials across locations would remove them.
- **Event loads**: done, section 8.4 (pv `eventSlices`).
- **T-poses on the first load**: not reproduced (Chrome dev at the screencast's full rate, Chrome on the production build at
  6 Mbit/s with 988 shots over the whole flow, WebKit dev); the load screen, the black gap and the movie show no model.
  Candidates: the cutscene actors' warm-up frame after the load screen, a transparent cover during the heli
  across-switch.
  Hunted again with a probe on every skinned draw (visual-parity agent, 2026-09-27). The probe hooks
  `SkinnedMesh.onBeforeRender` and flags a bind-pose skeleton. At the next frame it also checks the uploaded bone matrices,
  which catches a stale GPU buffer. For the in-game rider it checks the core palette. Compiles are counted apart. Every frame
  was screencast.
  - **Runs, 0 bind-pose draws in all:** fresh profiles, silent. "Slow GPU" = `createRenderPipelineAsync` and
    `renderer.compileAsync` resolve 0.5-2 s late, and WebGL links complete 0.5-2 s late.
    - Chrome phone 844x390 DPR 3, 6x CPU, slow GPU: a new career, then a resume at the station.
    - The same at 6 Mbit/s: a 193 s load.
    - The same on the WebGL backend.
    - Chrome 1600x900: both flows.
    - WebKit, slow GPU: a new career.
    - Every one of the 11 characters at 1x (heli_arrb scripts 154-163).
    - The production build of the current tree through the shaped edge (6 Mbit/s, phone, 6x, slow GPU): both flows.
  - **Where skinned meshes draw:** only the character preview, the arrival NIS actors (posed) and the ride's rider. Nothing
    skinned draws under the load screen, the white pre-fade or the movie.
    - A CTM world load runs no warm-up: `career-ui resume` -> `enterWorld`, no `ui.loadEvent`.
    - `FrontEndPreview.fetch`'s visible-while-compiling group is never in the scene.
  - **Field probe:** pv `bindPoseProbe` (on) in `web/diagnostics.js` reports the first hit of a session as a `bind-pose` event
    on the /mp/diag telemetry.
    - The event: screen, mesh path, FE preview root, 'skeleton' or 'gpu-buffer', frames / ms since armed and by what,
      backend, adapter.
    - Armed for 60 s after a load screen opens or closes, a cutscene or a ride starts; at most 3 checks a frame, ~7 us each.
    - A bind-pose control mesh is reported in Chrome and WebKit; the new-career flow gives no false hit (2,700+ checks).
    - Lab tools: `scratchpad tpose/flow2.mjs` (--phone --cpu --slow --throttleNet --passes --reload --route --inject) and
      `tprobe2.js`.
  - **Found (2026-09-28, visual-parity agent; pv `riderPoseGate`, on).** The field probe's first hit (Owen's Safari: 'loading',
    PEAK1/1, a `SkinnedMesh<Group<Scene` with 26 bones, spread 3.4e-8, not in view, 3.2 s after the load opened) is reproduced
    exactly by a world switch through the heli ride: MCOMM Transport from another peak's world (`careerUI.goWorld(19)`, then
    `goWorld(1)` = PEAK1/1). Lab tool `scratchpad tpose/flow3.mjs --then 'JS;;JS'` with `tprobe4.js` (every bind-pose draw, the
    mesh's group, its distance and on-screen size).
    - **The 26-bone mesh** is the page's placeholder rider. Every course load builds `RIDER_SAM` (26 bones) into the scene before
      the chosen rider replaces it (`loadCourse`, `loadedRider` reset to Sam). It is posed nowhere: its source skin is off and its
      three.js skeleton was never animated. It is visible because the frame's visibility pass does not run during a course
      switch (`frame` -> `idleFrame` while not live), while the held transport loop draws the whole scene itself
      (`cutscenes.js acrossSwitch` -> `host.render`). It stands at the scene origin, 4.7-11.5 km from the TRANSP camera (below
      or above the view), so it is drawn but never seen. Chrome and WebKit, every switch tried: PEAK1 -> 2 -> 3 -> 1 -> 2 -> 1.
    - **A visible one, on the ride's first frame after a switch** (Chrome, PEAK2 -> Black Top Station): the career rider (Zoe, 27
      bones) at its bind pose, in view 4.9 m in front of the chase camera, for one frame. `startRun` clears the rider frame and
      the source skin; the ride screen's visibility pass showed the rider before the first tick had posed it, and with the world
      paused nothing posed its skeleton. A slow device draws that frame for longer.
    - **The PS2** draws only the TRANSP set and the NIS actors during the held loop (the world streams behind it), and draws its
      rider after the update that poses it.
    - **Fix:** the placeholder and a newly loaded rider start hidden (`sam.visible=false` before `scene.add`); in the ride screens
      the rider draws once a tick has posed it (`currentRiderFrame`), the rule `opponent-riders.js` already applies to the
      computer riders. After: 0 bind-pose draws over the same switches in Chrome (PEAK1 -> 2 -> 3 -> 1 -> 2 -> 1) and WebKit
      (PEAK1 -> 2 -> 3 -> 1); only the NIS actors draw during the loads, and the ride draws the rider as before. Test:
      `test-visual-parity.mjs` R27.
    - The field probe reports the first hit of a session only, so this invisible draw had hidden any later one. With it gone, a
      further field report would be a new case.

## 9. Ride start (pv rideWarm, 2026-09-27, lazy-course agent)

A world load into a free ride (a new or resumed Conquer the Mountain career, a peak run) has no event load to prepare the
rider. So the ride's `startRun` used to load and set up the career rider (`ensureRider`: careerRider's re-resolve, the model,
`_init_animation` + `_init_race`: 250-400 ms on a phone profile), and the ride's first frames then built the rider's node
materials (340-475 ms).

With `web/pv-flags.js` `rideWarm` (on), `main.js switchCourse` runs `warmRideRider` under the load screen, after the world
has loaded:
- `ensureRider()`. careerRider's stale check decides, as before, whether the rider is re-resolved; only the time it runs
  moves.
- The rider's materials compile for the world pass as the ride draws them: `fogRenderer.compileObject(sam, {depth: 1})`,
  at the world pass's render-context depth (see 8.2), with a two-pass transparent material compiled once per side as
  `free-ride.js` streamWarm does.
- A wait until the GPU has run it (`gpuIdle`).

`startRun` then finds the rider ready: no `ensureRider`, no rider builds.

- **Same ride.** A new career from the menus, with the switch off and on, the frame clock frozen so only
  `ssxQA.advance` ticks: the human's reference motion, rider state and game RNG are bit-identical for 900 ticks, both
  without the arrival cutscene and with it (skipped after 4 s). `web/test-ride-warm.mjs` repeats it for 300 ticks.
- **Chrome, 4x CPU throttling.** Node-material builds after the ride starts went from ~1 s (146-157 builds, the rider's 6
  skinned materials among them) to 25-44 ms. The warm-up costs 380-410 ms (rider) + 584-655 ms (compile) of load screen
  instead.
- **WebKit (system WebKit, desktop).** The ride's first frame gap went from 182 ms to 56-64 ms. The compile under the load
  screen waits 2.2-2.4 s for the GPU: Metal pipeline builds that previously landed on the ride's first frames.

