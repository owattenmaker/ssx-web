# Conquer the Mountain free ride: the scripted content, decompiled against the port (2026-09-29)

This document covers what the PS2 (SLUS_207.72, gp = 0x4A30F0) triggers while the player rides CTM free ride, and what the
browser port does in each case. It has six parts: the MCOMM message scheduler, the NIS triggers, Big Challenge offers,
location crossings and the Transport, radio / DJ and place audio, and the stage programs of the streamed worlds.

It is research only: no game code was changed. The port references are the tree on 2026-09-29 at about 10:45. Other agents
were already landing fixes from this audit's first report, so check the current file before acting on a line number.

**Method.**
- Static decomp of `local/output` (recompiled MIPS) and `~/Documents/ChatGPT/ssx3-decomp`.
- A LUN catalogue of every stage program in the four streamed worlds.
- PS2 evidence:
  - Silent ARMSX2 captures, all from derived states under `local/ps2-capture/ctm-decomp/`:
    - `crossing/to-c-x` and `to-c-fade`: the Ruthless -> Yellow Mid Station Transport, every tick;
    - `bigchal/tri-offer`, `down-no` and `no-offer`: a Big Challenge declined and accepted;
    - `audio/postevent`, `postevent2`: the Transport after the Snow Jam final, with the sound-call log.
  - Reads of existing savestates: the HUD mail fields, the audio director, and the live Spline modifiers.
- **Status** means:
  - `match`: the port does what the code says;
  - `differs`: the port does something else;
  - `missing`: the port has nothing;
  - `unconfirmed`: the code alone does not settle it.

**Shipped configuration.** `pv mountainRide` is **off** in `PV_DEFAULTS` (web/pv-flags.js). Every device therefore rides the
per-peak worlds PEAK1 / PEAK2 / PEAK3:
- `crossWorld` loads a new world at the DRA4_A / ERA5_C connectors;
- a Transport to another peak is a world switch.

Where MOUNTAIN mode (`?pv=mountainRide`) behaves differently, it is noted.

## Ranked differences (what a player feels, most first)

1. **The peak boundary is a load screen.**
   - PS2: the mountain is one streamed world (table 0x442168). At the DRA4_A Unload the course changes and the game enters
     world state 11 with the rider riding on (22CEA8 -> 22DF50). The Load (22D088) enters WS10 for one tick, then WS4
     (fr-dra4a-full records 14732 / 19215 / 19216). There is no placement and no load screen. The only visible changes are
     the MCOMM switching to overlay 4 and the n/N counter hiding between the Unload and the Load (0x21FA10 -> 22D390).
   - Port: in PEAK2 / PEAK3 the Load names a row the world lacks, and `free-ride.js:251` posts `crossWorld`.
     `career-ui.js:168 crossWorld` then does a world load and places the rider at the station's session point 0, losing the
     speed and line.
   - Fix: MOUNTAIN on every tier (pv mountainRide), which needs the memory release (pv peakRelease; ctm-parity.md "The PS2's
     location release"). Coordinator's call. Files: web/pv-flags.js, web/free-ride.js `mountainFreeRide`.

2. **Every world switch plays load-screen audio instead of the in-world transport audio.** This covers the post-event return,
   Transports to another peak, and `crossWorld`.
   - PS2 (captured in `audio/postevent2`, ticks 15440-15442), at the confirm:
     - 28F520 stops the song;
     - code 20 (28EBAC..28EEB4) picks the next song and requests the destination hub song 10 ms later (kind 2);
     - it posts Radio BIG intro (0,1) and hub chatter pool 5 (2A4718);
     - there is no loading loop.
   - PS2 riding crossing: no director call. The connector's MusicTrigger 11 (28D988) plays the hub song after 2 s and the
     kind 1 / 0 DJ after 1.5 s.
   - Port:
     - `game-audio.js loadingStart` (:682) fades the song and loops LoadingScreen.bnk.
     - `leaveWorld` (:891) clears the song, timers, speech and every voice, including the `'nis'` heli / gondola voices.
       That last point comes from reading the code; it has not been heard in a browser.
     - The next world load runs 2867E8 / 2A4A78: DJ kind 2 with pool 8, or kind 4 with a playlist song, the Now Playing box
       and an artist intro.
     - `travel()` (:991) returns at once after an event, because `state.free` is null.
   - Fix (new default-off switch):
     - Transport world switches: call `travel(dest)` at the confirm, with a free-ride state after an event.
     - Carry the audio through the switch the way `carryWorld` does: keep the song, timers and pending DJ, keep the `'nis'`
       voices, play no loading loop, and skip 2867E8 / 2A4A78 at the next world load. Call `arrived()` at the arrival cut.
     - `crossWorld`: the same carry, without `travel()`.
     - Files: web/game-audio.js, web/career-ui.js (goWorld, crossWorld), web/ctm-transport.js, web/main.js, web/pv-flags.js.
     - Moot for crossings once MOUNTAIN ships (item 1).

3. **No Spline or MultiSpline set piece moves in the streamed worlds** (93 programs), and ParentModifier children do not
   follow their parents (21 programs).
   - What is affected:
     - Flybys: the dragon triggers on ARA1 / BRA2 / CRA3 / DRA4 / DSS2 / EBA3 / ERA5 / ESS3, Snow Jam's rockets, the ospreys
       (ABA1, CRA3, DRA4, DSS2), the EBC3 cessnas, the CBA2 / ESS3 sleds, the ASS1 train, the EZseq timers, the raven and
       eagle loops, and the blimps.
     - MultiSplines: the chairlift chairs (ARA1 x2, ABA1, CBA2, CHP2, EHP3, ESS3, EBA3 x2), the BHP1 road vehicles, the ABA1
       box vehicles and the BRA2 bins.
     - ParentModifier: the searchlight glows of BHP1, BRA2, DRA4 and ERA5.
   - PS2: slot-1 / 2 / 5 programs run in free ride (30A060 fires in game mode 4) and builtin 19 (0x2FDED0 -> ctor 0x359460)
     builds the Spline. Live SplineModifiers (vtable 0x48F250) are in CTM free-ride savestates (event type 4, game 0):
     - `peak2/frchp2-1500..2100`: 2, the CHP2 ravens / blimp;
     - `peak2/frcba2-3200 / 3550`: 1;
     - `menus/fr-courses/bhp1-freeride`: 1.
     (`local/ctm-decomp/stage/spline_scan.py`.)
   - Port:
     - `set_piece_tables()` (web/set_piece_gameplay.inc:163) has only per-course branches, so `setPieceCourse` stays false
       in PEAK / MOUNTAIN and builtins 19 / 20 return early.
     - Builtin 18 has no case (default nil); pv peakAttached is off and has no streamed export.
     - web/peak-set-pieces.js:15 says so itself.
     - The trigger programs' sounds still play: loop 201 on ospreys and cessnas, 114 on the blimp, 217 on the chairlift
       cables, which are drawn but carry no chairs.
     - The Spline constructor's gameplay-RNG draw (0x35955C) is skipped, so the shared RNG drifts after each such trigger.
   - Fix: build the streamed world's set-piece tables from the location seeds, keyed by track rather than by the world
     name. Export the spline / MultiSpline LiveComps and attached.json for PEAK1-3 / MOUNTAIN, and draw them in
     peak-set-pieces.js. Files: web/set_piece_gameplay.inc, web/attached_setpieces.inc, tools/export_peak_world.py,
     web/peak-set-pieces.js.

4. **The Transport ride.** From `crossing/to-c-x` / `to-c-fade` and the NIS decomp:
   - (a) **The held loop hard-cuts on release.**
     - PS2 (0x236AA8 -> 27A9F0 -> 279070 -> 2766D0 with a2 = 1 -> 277980): heli_inair_zoe #122 plays on about 30 ticks while
       fading to black (its fade_out record: out 30).
     - The rider is placed at the script end (tick 1470; 123B48 -> 11D390).
     - The world fades in over 30 ticks with the HUD bright over it. The bars stay full over the black (1472) and slide out
       by about 1497.
     - Port: `cutscenes.js release()` (:1007) and `advance()` call `nextStep` at once: no fade-out, instant placement, no bar
       slide-out.
   - (b) **Streaming starts about 120 ticks early.**
     - PS2: 22CEA8(dest, 7) and streamer +0x1C8 = 1 when slot 1's group is 14 / 20 (the held loop, 0x2366C4). At the in-air
       step it is not yet requested.
     - Port: `free-ride.js:503` requests it when the in-air + loop list starts.
   - (c) **The loop is released too soon.** WS11's update 0x236960 releases only when all of these hold:
     - 22D278 (every row, the sky and TRANSP resident);
     - a view slot aimed at the arrival point 123F38 (radius 45000, 3A9658) has no unread page within 200 m (3A9770 >= 20000):
       151 ticks for C;
     - the view-0 fade is idle (2E4C90);
     - the NIS is not waiting on a read (279298).

     There is no timer. PS2 DBC2 -> C: 120 ticks in the air, then 397 ticks of the held loop. The port waits for rows,
     collision and draw only (free-ride.js ~:497-504).
   - (d) **One NIS list vs three.**
     - PS2: 27A860 queues departure (12 / 18, flags 1), in-air (13 / 19) and the held loop (14 / 20, flags 8) as ONE list.
       The pushed WS10 appends the arrival movie and the heli drop [16, 17] to the same list (f95-after sample 1081:
       [14 #122, 30, 16, 17]).
     - Port: three `playCutscene` calls (free-ride.js ~:490 / 492 / 515, ctm-transport.js ~:47 / 49, career-ui.js ~:118 / 120).
       Each loads its scripts first (cutscenes.js ~:704-714); with the default `restore` the screen returns to 'game' in
       between, so the HUD shows and Start works. The bars slide in again, and the in-air step fades in over its own 60 ticks
       instead of the departure's out-record "in" 30.
     - Unconfirmed in a browser (mostly a first-Transport effect, since the files are cached).
   - (e) The Transport's dome switch is allowed at the Load (`peak_world.inc:286`). The PS2 allows it at 0x235808, once the
     Transport NIS has ended: about 30 ticks later (DSKY 2 -> 7 at 1472).
   - (f) No 30-tick skip lock (278DE8 at 0x236550: no Cross, no "Press X to skip").
   - Fix, in web/cutscenes.js:
     - `release()` gives a HOLD step with a fade_out a deadline (`holdTicks` = held + out), then passes `prevFadeOut`;
     - an append API (275ED0) so free-ride.js / ctm-transport.js / career-ui.js queue onto the active list;
     - a skip lock;
     - free-ride.js requests the rows at the held step's start and holds for the destination's page time. That page time
       needs a per-destination measure (`local/ctm-decomp/crossing/xtrace.py`, view slot +0x288); how to model it is
       unconfirmed.
     - The dome needs a core export (the host allows it when the list ends).
     - All behind a new default-off switch.

5. **The world freezes under the arrival cutscenes.**
   - PS2: the world ticks under in-engine NIS steps: the Happiness plane #153 / #163 (new-career tick 17 -> 917) and the heli
     drops (f95-after 15930 -> 16133). It stops only during the ABC1 / DBC2 / EBC3 movies (tick 15927 constant over DBC2's
     927 frames).
   - Port: `main.js:212` (`ui.cb.cutscene`, `o.hold`) pauses the world for the whole list, so flags, particles, set pieces
     and world sounds stand still.
   - Fix: pause only during movie steps (cutscenes.js knows the step kind). Overlaps the world-state owner (WS10 -> WS4).

6. **Station departures lose their stage calls.**
   - PS2: gond_dep #126 / heli_dep #147 call the hub stage callbacks (0x03852825 / 0x0B13B4E5 / 0x09BE221E; registered by
     309E50, called from NIS 280640 @0x2807FC). Each is a depart LiveComp on mdl_<hub>_depart_gond / os609_full_version_depart,
     loops 216 / 200, the heli's 60-particle snow spray (builtin 25), and SetNodeState on 7 instances.
   - The TRANSP location (track 0: 5 programs) registers the in-air callbacks 0x07B5BC64 / 0x0AE69AB4 (heli) and
     0x0B62B144 / 0x03700674 (gondola): LiveComp, loop 201 / 217, hide.
   - Port:
     - cutscenes.js (~:488-495) gives these steps no staged set, so their channel-0 calls are dropped (~:553;
       cutscene-plane-fx.js:49 returns without a set).
     - No seed holds TRANSP.
   - Fix: send every channel-0 call through `stage_global_call` for the current course's track, and add TRANSP's stage to
     the seeds (tools/export_peak_stage.py). Cutscene agent's area.

7. **Declining a Big Challenge does not reset the rider** (in progress when written: core `mission_lifecycle` bit 1,
   web/mission_gameplay.inc:354, web/core.cpp:637).
   - PS2 (captured, `bigchal/tri-offer`, `down-no`): 30B658 -> 1235F8 forces a reset. It runs on No or Triangle at the offer,
     No at the fail prompt, No at "Restart Challenge?" and Quit Challenge (30B758).
     - The reset: owner+0x350 = 1, control 9, motion 3, start phase 0. The white fade 12F230 and the route placement 12F398
       put the rider about 16 m on (-140496, 4962, -235114) at tick 3650, control 0 at 3709.
     - Unlike 116120 there is no sound 0x7B, no "Wrong Way!" and no 270970.
     - Pending op 3 then shows the declined volume again with its 60-tick guard, but the rider has left it.
   - Port at the start of the audit: `mission_rider_decline` was empty, so a slow rider got the offer again 1 s later, and a
     Quit left the rider sitting in the manual start.

8. **The Big Challenge offer / fail prompts pause music and SFX** (in progress when written: pv `bcPromptAudio`, off).
   - PS2: 0x2308AC opens overlay 0x1D with pause context 3 and sound 29CED8(4). It never calls 289B70, whose only callers are
     the MCOMM 0x1F840C, Start 0x230AEC and 0x244DE0, so the audio plays on (audio+0x5FB4 = 0 in `bc-sd-prompt`,
     `sd1-prompt`, `sd-run-stop`).
   - Port: `big-challenges.js` -> `main.js pause` -> `game-audio.js pause` stops music and SFX and cancels queued speech.
   - Retry at the fail prompt also sends no 29D6D0 (stinger warm-up, +0x5FD8).

9. **The post-event DJ commentary is missing** (PS2-confirmed).
   - PS2:
     - At every CTM finish 287070 -> 2A45C0 / 2A4660 records the place, the course and running totals of hits and score. The
       record is reset at round 1 and armed at the final or a one-round medal run (`after-final/final.p2s`: armed, place 0,
       course 0).
     - At the next hub chatter, 2A2E50 runs 2A4770 before anything else: place 0 -> Char_Progress 0x2102; hits >= 5 ->
       Aggression 0x212E; score >= 27 (24 in Big Air) -> High_Trick_Score 0x212F; a random pick avoiding the last one.
     - With nothing earned during a travel: hub -> chatter, backcountry -> Terrain_Info, course -> Event_Intro.
     - Captured: 0x2102 at tick 15614 in `audio/postevent2`.
   - Port: no record, and `hubChatter` has no commentary branch.
   - Fix: record in `game-audio.js finish()` and branch in `hubChatter()`. The hit and score inputs (rider+0x790 -> +0x128,
     +0x114) need core exports; what +0x114 counts is unconfirmed.

10. **First-visit DJ lines are tracked per page load and shared by all riders, and Free_Ride_Intro lacks its Peak 2 gate.**
    - PS2:
      - 2869E0..286A3C takes audio+0x579C at every CTM world load from the saved, per-rider visited mask P+0xACC (145D38).
      - DJ kinds 3 / 4 / 5 (28E678..28E7DC) play Free_Ride_Intro only while 146008(P, 0, 1) holds, i.e. P+0x278 bit 12:
        Peak 2 still locked. The bit goes 1 -> 0 at the Peak 2 pass (f95-after end state).
    - Port: `main.js` (the free-ride `runStart` call, ~:78) passes no `visited`, so after a reload Free_Ride_Intro replays at
      Snow Jam, Metro-City and R&B.
    - Worse, a world load into a visited backcountry after a reload replays the first-visit routine (234FE0..235058):
      - Happiness: pktrans, the Peak1 hub song and the new-career DJ with "check your MCOMM";
      - The Throne / Ruthless: pktrans event 2 / 3 and BC_Intro.
    - Fix: pass `visited: careerUI.visitedMask()` (game-audio.js:278 already accepts it) and `peak2Locked`, and gate the
      djTimer Free_Ride_Intro on it. Files: web/main.js, web/game-audio.js.

11. **The mail icon's timing after an event.**
    - PS2: the event messages post on the finish tick (125108 -> 238358 @0x2384DC -> 154EE8).
      - The icon blinks about 3 s under the finish HUD.
      - The WS5 180-frame countdown (0x233C88) then opens overlay 0xC / 0xD (HUD event 3). The icon freezes (+0x3D8 = 1)
        through the podium, results, rewards, Transport and arrival (+0x3D4 = 1).
      - Only the remaining ~2 s show at the next ride.
      - Evidence: race-f fin.p2s timer 0.050 -> res 3.033 -> f95-after 3.050 -> peak2-arr end 3.050, cleared in the Ruthless
        ride. Read with `local/ctm-decomp/mcomm/hudmail.py`, HUD vtable 0x474160.
    - Port: posts at the results screen (408 ticks later; `career-ui.js finish`) and shows a full 5 s at the next ride.
    - Also, opening the Message Center clears the icon (career-messages.js:265 / :274 `this.hud = null`). The PS2 only
      freezes it (events 3 / 4) and resumes it after.
    - Fix: post at the finish tick and freeze 180 frames into WS5. Minimal version: start the icon at `frames: 182`. Do not
      null the icon in `open()` / `openFaq()`. Files: web/career-messages.js, web/career-ui.js, web/game-tick.js.

12. **Two DJ queue rules.**
    - Radio BIG intro 2A26F0 stops the current line itself (2B11B0 at 0x2A272C) before posting; the port's
      `audio-speech.js radioBigIntro` (:216) queues behind it.
    - Pause resume 289BE4 stops speech and clears the pending DJ only when +0x5828 is set (an in-game song change 28FAE0 via
      0x208CA4). The port's `game-audio.js pause(false)` cancels queued speech on every resume, so MCOMM visits drop DJ lines.

13. **Big Challenge lifecycle edges** (some in progress: `mission_lifecycle` bits 2 / 4, `browser_mission_gate`,
    `browser_mission_load_trigger`).
    - Gate builtin 67 (0x3021B8) calls 30B7F8 (end the challenge) when 0x445E40[key0] != 1 and key1 == 0.
    - WS10 enter 0x2356A8 -> 309030 -> 308988 clears C+0x2A0 / +0x2AC / +0x2B0, the queue, the events and HUD words, and
      destroys the missions; they are rebuilt at WS10 exit 0x235884. The port kept `missionActive`, so after a Load trigger
      mid-challenge `mission_refresh_visuals` skipped that challenge's volumes.
    - The offer queue is read at the top of every game update (0x23087C), before the rider pass; the port reads it after
      `step_rider` (one extra rider pass), and only in simulate.
    - Retry sends 29D6D0.

14. **The MCOMM during a crossing** is overlay 4 (0x20CA10..0x20CB44: WS == 11 -> 4), list 0x478190 = {Return, Messages,
    Audio, Options, Quit} (5 items, 0x1F9018). The port shows the 7-item list with Transport / Session greyed
    (career-ui.js:222 / :242). In MOUNTAIN mode this is every riding crossing; in the shipped per-peak worlds, the in-peak
    ones.

15. **"Loading..." caption bit 1 is missing.** While an NIS list waits for a script read, 0x2788B0 / 0x2788D8 set HUD caption
    bit 1 (0x1F30D8 "kT_ovrcmnloading", drawn by 1F31E0). The port draws the caption on held / loop steps only
    (cutscenes.js ~:914), not while `!seq.ready`.

16. **Big Challenge markers are static and some mission props are missing.**
    - The 136 offer volumes' marker UV scroll (builtin 21 in the slot-1 program, gated on op 16 / op 15) is not in
      uv-scroll.json, so no marker scrolls.
    - 47 mission LiveComp targets (e.g. the ABA1 balloons) are in no export.

17. **The FAQ "?" (Green Base Station).**
    - Builtin 100 (0x306300 -> 1E3510) sets gp-0x1024 = 0 and bit 3 of 0x534FF0. WS4 opens overlay 0x22 at its next update
      (0x2309A4: push WS12, pause sounds 29ED18).
    - The port opens it at once, or drops it when `!running || paused`. The core has already marked it, but the career flag
      has not, so it re-arms at the next world load.
    - Unconfirmed: the PS2 keeps the first-FAQ flag in the per-player struct 0x534FE0 + 0x1C*player (+0x10 bit 3), copied with
      the profile by 145A98. The port keeps it per rider record (`cu.me.faqShown`), so a second character in the same save
      would see the FAQ again. A two-character capture would settle it.

18. **Log teeters are rigid rails** on the connectors and on DBC2, DSS2, ESS3, EBC3, CRA3_D and ABC1 (31 programs; builtin 6
    is a no-op in the streamed worlds). Overlaps the course-limits agent.

19. **Peak_Boss stays in the chatter shuffle.** 2A2F44..2A3028 drops it once the peak backcountry's kind 4 and kind 5 event
    records both exist (+0x573C bit 3, 158910; what "exists" means is unconfirmed). Port: game-audio.js hubChatter has no such
    rule.

20. **Minor or unreachable:**
    - The global stage programs run at WS10 enter (309F18 at 0x2356B0, after 30C390 frees the collectible slot) and so rebuild
      the builtin-105 MultiParticle groups at every WS10; the port runs them once per track at the read completion
      (peak_world.inc:168).
    - Builtin 99 returns int(bit clear) of 0x5308D0 bits 6 / 8 / 7; the port's nil behaves the same unless those bits get set
      (writer not found).
    - Sound 102's gate is keyed on the course, not the location record word 144BC0 (audio-world.js:196; 12 calls).
    - The ambience start waits for the slot-9 bank and hard-stops at the bank unload (29D290..29D6C4, 286D18); the port starts
      it late and has no unload stop.
    - 28EF90 (same-location transport) also clears the pending Text_Message.
    - The texture-chunk pop-in 0x22A5A0 / 0x22A698 is not ported for PEAK / MOUNTAIN (first load-in only).
    - The in-world transport into an unvisited backcountry needs `firstVisit` without pv mountainRide (main.js:83); this is
      unreachable in the shipped per-peak worlds.
    - A 180-tick minimum at a first-visit load (0x235528) is hidden under the load.
    - WS14's enter placement (0x2363DC) is not modelled (known).
    - The icon keeps its blink phase on a repost; the port resets it.

## 1. The MCOMM message scheduler

**The posters.** All 33 calls of 0x1E2FE0, in 12 functions. Every one needs CTM (0x5305F9 == 0) and the event-object state
`[[G+0x84]+0x28]` outside 1..9. That word is not a tutorial counter: it is 0 in free ride and mid-race, 10 / 11 at a load,
arrival or Big Challenge, 13 after the finish, and 1 under the rewards / results overlays.

| function (call sites) | when | gates | item | port | status |
|---|---|---|---|---|---|
| 1E1550 rival reminders (1E1634 .. 1E1A10, 12 sites) | finish of the last heat: 125108 -> 238358 (0x2384DC, event over) -> 154EE8 -> 0x15501C | mode not 4 / 5, a medal; first unposted of categories 1..3 (race) and 4..6 (jam); lock +0x278 bit 6+peak / 9+peak clear (1464D0) and no rival medal (145EF0) | first + r%3, +3 for the peak's rival rider | career-messages.js:121-128, :253 | match |
| 1E1C10 rival taunt (1E1D4C, 1E1DB4) | goal walk in 1591E8 (0x159580 / 0x15960C) | categories 28+2p / 29+2p unposted; 1E1AD0 picks r % n of records not from the human and not in the inbox | Foreshadow subject | :131-149 | match |
| 1E2220 / 1E20D0 peak-challenge notices (6 sites) | 1E1EB8 from 154EE8 (0x154FF8), the next medal event | flags 40..51, categories 34..39 | bronze first, silver first + 1 (and posted) | :105-118, :256 | match |
| 1E2578 / 1E2518 / 1E2428 (4 sites) | award cases of 159CD0 (table 0x45AA40: 0, 1, 2..4) | posted bits 56..60 (award 3 tests 58, sets 57) | 251, 255, 252..254 | :152-163 | match |
| 1E2648 / 1E2828 (4 sites) | 1E25D8 from 154EE8 (0x154FD8), every rival result | result bit 22+p / 25+p | loss 0x73 / 0x7F + 4p (+2 for the rival's own rider), win +1 | :95-103 | match |
| 1E2A08 (1) | relationships: 155BF0 @0x155D30, 155E58 @0x155F3C | event-state gate (0x1E2A28), not the human, category unposted (below 5 clears it), (r & 7) + 15 < score | first + r%count | :69-77 | match |
| 1E3760 (2) | 119EF8 @0x119F7C, any in-world award cash, before it is added | player 0, next peak locked (146008), earned + amount >= goal | 247 / 248 | :165-168, :225 | match |
| 1E38B8 (2) | award cases 11 / 12 (0x159EE0 / 0x159FC8), before 158F60 | next peak locked, Freeride goal done | 249 / 250 | :152-163 | match |

**The inbox and the icon.**

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| add: full (25) drops the oldest (1E31B8); a flag-1 record draws its variant (1E3100: kind 3 / 4 r%10, 5 r%11, 8 r&3, else 0xFF); read bit cleared; count +1; HUD event 8 only when the "hud" object exists (0x1E30B8) | 0x1E2FE0..0x1E30F8 | career-messages.js:56-67 | match | static |
| remove shifts entries and read bits | 1E31B8 | :43-47 | match | static |
| order in 154EE8: BC result -> no medal: stop -> 1E1EB8 -> 1E1550 -> 1591E8 (skipped at 0x5305F8 == 4) | 0x154FD8..0x1550B0 | eventComplete :246-266 | match | static |
| **when 154EE8 runs: the finish tick** | 125108 -> 238358 -> 154EE8 | at the results, 408 ticks later (career-ui.js finish) | **differs** (ranked 11) | fin.p2s icon timer 0.050 one tick into WS5 |
| icon: runs while +0x160 is set and +0x3D4 == +0x3D8 == 0; phase and timer +1/60; clears at 5.0; white while phase <= 0.5, else orange (0x4C87A8) | 1EB6E4..1EB7C4, 1F0F3C (bit 0x80000) | hudTint :175, drawHud :403 | match | static |
| every overlay open sends HUD event 3, close event 4 (freeze, not clear); nothing else stops the icon; no sender of event 9 | 20A380 / 20A430, 1EC3D4 | Message Center open nulls it (:265, :274) | **differs** (ranked 11) | code scan |
| the HUD object is made per world load (0x22F4D0 -> 1E9A30); no deferred message queue | | a JS object that survives loads | match in practice | 0x18910F0 survives the post-event Transport |
| no poster makes an audio call; DJ_Text_Message (0x212C, 2A2B88) comes from 28E548 kind 0 on Peak 1 and hub chatter, independent of the inbox | | | match | caller scan |
| senders: 0..9 riders, 10 "SSX 3" (FAQs), 11 Atomika, 12 FOLDER (1E2EA0); **no sponsor sender or sponsor message exists** | 0x441630, 0x4C6C08 | messages.json | match | tables |
| builtin 110 (0x307020 -> 1E3570): the "?" homes in when CTM, mode not 6 / 9, the first-FAQ bit clear | 0x307020 | stage_script_gameplay.inc:388, peak_world.inc:192 | match | static |
| builtin 100 (0x306300 -> 1E3510): gp-0x1024 = 0 and 0x534FF0 bit 3 (1475C0); only Green station's "?" uses 100 / 110 | 0x306300 | peak_world.inc:202, main.js faq -> `cu.me.faqShown` | match / flag storage unconfirmed (ranked 17) | static |
| WS4 opens the FAQ at its next update (0x2309A4: overlay 0x22, push WS12, gp-0x1024 = -1, 29ED18) | 0x2309A4 | at once, dropped when paused | differs (ranked 17) | static |
| FAQ view keeps and restores folder 7's open state (1E3C00 @0x1E3CF8, 1E3E30, 1E4338) | | openFaq :270, choose :301 | match | static |

Doc correction: docs/characters.md calls the 0x1E2A08 gate absent and the event-state word a "tutorial counter"; it is the
event object's state and 1E2A08 has it (0x1E2A28).

## 2. NIS / cutscene triggers

**List player.**
- Two list players, each 0xCC bytes at nis+0x54C (nis = [gp-0x84C]); the active one is at +0x550.
- Queue calls:
  - 278E50 queues a list (275ED0);
  - 278E20 queues a movie (275DD8);
  - 278ED0 -> 278E90 queues a direct script (kind 28).
- Ids:
  - 0..27 are scfilter lists (279528 -> 27B0C0 ScriptChoice; an empty list is skipped);
  - 28 is a direct script;
  - 29..31 are the ABC1 / DBC2 / EBC3 movies.
- 278F38 loads and 278F68(nis, p, useFadeIn, now) plays. `useFadeIn` adds only the out / hold part of the first fade-in
  record (0x2764D0).
- Step flags:
  - 1: skippable by Cross (0x277060 / 0x276F48, pad 0x6F);
  - 2: dropped from the search after a skip (0x2774A8);
  - 4: Start-pause allowed (278DA0 @0x231B04). No list sets it, so Start never pauses an NIS;
  - 8: held until advanced (0x276468 / 0x276C28).
- The skip lock +0x554 is set by 278DE8 (30 ticks at the Transport 0x236550, 1 at the WS5 exit 0x23408C) and counts down per
  NIS tick (0x2788E0).
- Fades:
  - a step's start sends its fade-in to 2E4370 (276388);
  - every transition sends the finishing step's fade-out to 2E4370 and waits out + hold (277980);
  - a movie uses 277DE8 -> 2E44F0;
  - releasing a held loop fades too (2766D0 a2 = 1).
- HUD: vt+0xC4(0) at each step start (0x276444), back at clear (0x276948).

| trigger | PS2 | port | status | evidence |
|---|---|---|---|---|
| callers of 278E20 / E50 / ED0 / F38 / F68 / 27AAF8 / 27AC60 / 27A860 / 27A9F0 in free ride | WS1, WS5, WS10, WS12, WS13, WS14, the WS2 exit, the WS7 clear, builtin 36, kind-7 advance | | | full grep of local/output |
| first visit to a backcountry | WS10 push 0x234F40 (CTM, course 14..16, +0xACC bit clear). 0x235080 sub 0: course 14 -> 278E20(1, 29, 3) + lists 0, 1 flags 3 + pktrans 28CD48 + 2B3A70; 15 / 16 -> movie 30 / 31 + lists 16, 17 flags 3; played by 278F68(1, 1, 0) at 0x2357A8 | career-ui.js ~:110-118 -> cutscenes.js arrivalSteps | match | new-career [29, 0, 1]; f95-after [30, 16, 17] |
| the same, reached by transport | appended to the in-air list during WS11 | a separate list after `release()` (career-ui.js ~:118) | **differs** (ranked 4d) | f95-after sample 1081 |
| a visited backcountry by transport | 0x235220 (streamer +0x1C8, set at 0x2366B8): [16, 17] flags 3 appended | career-ui.js ~:120; free-ride.js ~:515 | match (content) | peak2-arr |
| a world load at a visited backcountry | nothing | nothing | match | reenter |
| the visited bit | set at WS4 after WS10 | markVisited in ride() | match | 0x0 -> 0x4000 |
| 3 s minimum at a first-visit load | 0x235528 (180 ticks) | none | unconfirmed impact | static |
| lodge door | builtin 68 action 4 (0x302430) -> WS14 arg 0: lists 22 (flags 3) + 23 (flags 1), 278F68(1, 1, 0) (0x236334..0x236378), prompt 0x1F (0x2364FC) | free-ride.js ~:242 | match | door capture |
| booth / post-event | WS14 arg 2 (booth 0x302400; results 0x20D0F0, 0x236E88): list 11 flags 3, 11D390, map overlay 0x21 (mode 3 at a station, else 4, 0x2364E8) | free-ride.js ~:242, career-ui.js ~:414-416 | match | list 11 exists only at BRA2, the freestyle venues and hubs A-F; Snow Jam's is empty |
| Transport ride | WS14 arg 1 sub 1: skip lock 30, 27A860(gondola when neither end is a backcountry): [12 / 18 departure, flags 1] [13 / 19] [14 / 20 held, flags 8] (15 / 21 multiplayer), 278F68(1, 0, 1), 28E8C0(20, 1) | free-ride.js, ctm-transport.js -> cutscenes.js | content match, structure differs (ranked 4) | f95-after |
| a transport to the same location (not a backcountry) | WS15, no NIS (0x236614) | `sameLocation` | match | |
| held-loop release | WS11 0x236AA8 -> 27A9F0 with a 30-tick fade-out | nextStep at once | **differs** (ranked 4a) | to-c-fade 1458..1497 |
| station arrival by transport | nothing (WS14 arg 1 queues no 22 / 23) | pv stationArrival on: nothing | match | |
| crossing, Session, lodge exit, career start | nothing | nothing | match | |
| podium / rival (WS5 0x233EDC -> 27AC60, lists 9 / 7 / 8 / 6 or 10; rival [24, 25] when CTM, type < 4, unlocked, 146150 unshown, not a cheat skin; 146320 marks it) | | career-ui.js ~:501-516 | match | earlier captures |
| stage builtin 36 (0x300C78, an NIS from a stage program) | used by 0 of 11816 programs in all 21 seeds | none | match | scan_builtin.py |
| peak unlock, Big Challenge, career-ending NIS | none exist (the ELF has only eabig / thx / intro / intro_dj / xsell + ABC1 / DBC2 / EBC3) | none | match | ELF |
| the world under an NIS | ticks under in-engine steps; frozen only under movies | holds (pauses) the whole arrival list (main.js:212) | **differs** (ranked 5) | new-career, f95-after |

Per-location lists (scfilter): 0 / 1 exist only at ABC1; 2 (fly-over) and 24 (rival) at the 14 event venues; 3, 5-10 and
25 at the 15 venues; 4 at the six race venues; 11 at BRA2, the freestyle venues and hubs A-F; 12 / 18 / 22 at hubs A-F;
13-15 and 19-21 everywhere; 16 / 17 at ABC1, DBC2 and EBC3; 23, 26 and 27 are empty. Every id 0-25 is queued by code.

Doc corrections:
- peak-mountain.md "Cutscene hooks" lists `plane-drop` (0x1D) and `location-intro` (0x1E / 0x1F / 0x10 / 0x11) as not called.
  They are 0x235080's movie ids 29-31 and groups 0 / 1 / 16 / 17, and they are implemented.
- cutscenes.md says the Peak 2 / 3 heli arrivals are not triggered; they are.
- ctm-parity.md reads the ~1850-frame stop at WS10 in f95-after as an NIS read; it is the DBC2 movie playing.

## 3. Big Challenge offers

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| 136 offer volume pairs on 17 courses (88 new + 48 replay), all on course tracks (none on a connector, so no offer during a crossing) | stage programs | stage VM | match | `bigchal/offer-table.txt` |
| slot 1 (section enter): op16 == 1 and op15 == 0 (new) / 1 (replay) -> builtin 0 + UV scroll 21 + builtin 87(1.0) (a 60-tick guard on both); else hide both | builtins 0 / 21 / 87 (304E38) / 29 | stage_script_gameplay.inc:272 / 322 / 340 | match (scroll: ranked 16) | listings |
| slot 2 (contact): op 8, then hide both; no Debounce (only the 60-tick guard and 355770's 30-tick guard) | 30A868 | same | match | |
| the post: a ring of 16 {id, type, player}, no overflow check; only with 0x535C10 == 4 and one human; blocked while the replay state is 1..9 | 30A868 @0x30A950, 30C7C0 | mission_gameplay.inc ~:191-200 | match in practice | |
| programs post only 8 (136 sites) and 9 (158) and query 15 / 16 / 19; there is no op 11 | | | match | scan40-*.txt |
| queue read at the top of every game update, overlays included; skipped under the replay modal 2CC098(S+0xA4) | 0x23087C..0x2309A0 | only in simulate, after step_rider | differs (ranked 13) | |
| type 8: overlay 0x1D, pause context 3 (mask 0xFFFFFFDF), 29CED8(audio, 4, 0, 1.0), rest of the frame skipped | 0x2308AC..0x2308FC | big-challenges.js ~:99-102 / 126 | match except the audio | world tick held at 3630 |
| the prompt keeps music and SFX (no 289B70) | | the port pauses audio | **differs** (ranked 8) | audio+0x5FB4 = 0 |
| 0x1D Yes: a running challenge -> 30B6F8 restart, else C+0x2A0 = id and 30B540; 29D6D0 | 1F75A0 | mission_prompt | match | no-offer: control 6 at tick 3631 |
| 0x1D No / Triangle: 30B658 (26CDF8, 26CBB0, 26F228, 26F7B8, **1235F8**, C+0x2A0 = -1, pending op 3) | 1F7660, 0x30B658 | the reset was missing | **differs** (ranked 7; in progress) | tri-offer, down-no |
| 1235F8: owner+0x350 = 1, control 9 (12F230 fade / 12F398 route placement), motion 3, start phase 0; no 0x7B, no Wrong Way, no 270970 | 0x1235F8 | | differs (ranked 7) | captures |
| pending op 3: quiet stop, 308DB8 refresh (the declined volume shows again after 60 ticks), 29DBB0(1) | 0x30919C | | match | |
| 26CDF8 / 26CC48 restart point: unreachable (P+0x70 is set only by 26CC48 <- op 5 <- 30B590, which has no caller) | | omitted | match | xrefs |
| fail: type 9 -> 3074C0 (bit 2, callback +0x70 / +0x74, quiet stop), 29DBB0(0), C+0x2A0 kept | 0x23091C | ~:518-521 | match | |
| fail overlay 0x1E (context 3, no audio pause): Yes -> 30B6F8 + 29D6D0; Info -> 0x1D; No / Triangle -> 30B658 (the reset) | 1F7738, 1F7800 | no 29D6D0, no reset, audio paused | **differs** (ranked 7, 8, 13) | |
| success: 307240 / 307308 (bit 3, cash, next challenge's bits, callback +0x6C, refresh, C+0x2A0 = -1), 29D8E0; a chain's last challenge gets a $0 replay volume | | ~:142-173 | match | |
| challenge pause (overlay 2, audio paused): Restart -> 0x1D (0x20D8F4); Quit -> 30B758 (reset + op 5 fail + refresh + 29DBB0(1)) | 0x20D944 | no reset | differs (ranked 7) | |
| end 30B7F8 (quiet stop, queue cleared 30C790) at WS14 0x23626C / WS15 0x236074 | | free-ride.js | match | |
| end at the event gate builtin 67 (0x3021B8) | | missing at audit time | differs (ranked 13; in progress) | |
| WS10 enter 0x2356A8 -> 309030 -> 308988 full reset, rebuilt at WS10 exit | | missing at audit time | differs (ranked 13; in progress) | |
| the crossing itself (WS11) does not end a challenge; unload 308FE0 drops the location's missions, keeps C+0x2A0 | | ~:285 | match | |

Unconfirmed (CTM screens agent): a D-pad press in the first ~6 samples of the offer prompt was ignored on the PS2 (`no-offer`).

## 4. Location crossings, the Transport and area loads

Names (corrections to earlier notes):
- World state 11's object is W+0x178 (vtable 0x47D2C8): enter **0x2368A0**, update 0x236960, exit 0x236AF0 (empty).
  0x236928 is the clear-new-career loop inside its enter.
- WS10's vtable is 0x47D428:
  - 0x234F40 / 0x235080 are its "push" enter / update. 22DF50 pushes WS10 at every course change, so these run at every
    crossing and Transport, not only at first visits.
  - The real enter / update / exit are 0x2355C0 / 0x2357F8 / 0x235868.
- The streamer's course-change machine is S+0x1B4 (S = W+0x78):
  - 22CEA8 sets 0 -> 1;
  - 22D088 sets 0 / 1 -> 3, 2 -> 4;
  - pass 1 runs 22DF50;
  - pass 4 waits for no row in 4 / 8 and the pushed WS10 done (2312D8), then 22DFF0 (22E180, WS10).

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| riding crossing = WS11 from the Unload's pass to the pass after the Load; WS10 one tick, then WS4 | 22CEA8 -> 22D640; 22D8D8 cases 0x22DD80 / 0x22DDAC; 22DFF0 | career-ui.js courseChanged / crossingArrived; free-ride.js ~:252 | match (±1 tick) | fr-dra4a-full |
| the peak boundary DRA4_A -> A, ERA5_C -> C streams in one world | 0x442168 | crossWorld world load (free-ride.js:251, career-ui.js:168) | **differs, shipped** (ranked 1) | fr-dra4a-full |
| WS11 enter: cBE_setState(0), 26F228, 145CB0 (new-career flag cleared for every player); the update is idle unless +0x1C8 | 0x2368A0, 0x236990 | career-ui.js (firstRun) | match | static |
| gameplay stall +0x1D0 = 0 while a row is 4 / 8 or the course change is in 3 / 4; the frame still runs 3A6928, 22E840, the WS tick and 22D8D8; the render draws the world with the rider frozen and **no caption** | 0x22DDD8..0x22DDF8, 0x230768..0x2307B4 | peak_world.inc:229-230, free-ride.js ~:377-379, game-tick.js:65 (plus the port's own draw gate pv streamGate) | match | the PS2 never stalled on a measured ride (+0x1D0 = 1 through to-c-x) |
| MCOMM overlay: a mission -> 2, a CTM event -> 1, **WS 11 -> 4**, free ride -> 3, else 5 | 0x20CA10..0x20CB44 | career-ui.js greys two items while `!22D390` | **differs** (ranked 14) | static |
| overlay 3 = {1, 11, 10, 12, 4, 3, 5} (0x478140); overlay 4 = {1, 12, 4, 3, 5} (0x478190) | 0x1F8FB0 / 0x1F9018 | 7 items | overlay 4 differs | static |
| the n/N counter is drawn only while 22D390 (22D278 and +0x1C0): hidden from the Unload to the Load; the course switches at the Unload's pass | 0x21FA10 | free-ride-hud.js:24-27 | match | static |
| a location draws only while its section is 6 (row 2); the old one stops one pass after the Unload (3A9958) | 22D8D8 case 5, 3A9D60 | free-ride.js ~:363-368 | match (1 frame earlier) | static |
| texture-chunk pop-in: a static instance / patch draws only with its chunk resident (outer camera, 1.5 x far) | 0x22A5A0 / 0x22A698 | not ported for PEAK / MOUNTAIN | differs (load-in only) | static |
| section activation: ±150 m box, every 20 ticks or 20 m; slot 1 enter, slot 3 leave; slot 2 contact; 0 / 4 / 5 from entity events; the unload start drops without leave handlers (103308) | 0x101B60, 0x30A298 / 0x30A540 / 0x30A060 | engine/section_streaming.hpp, section_gameplay.inc | match (gated) | port gates |
| sky: 22CEA8 clears +0x1A4, builtin 68 action 5 sets it, 22DE98 switches | 22D014, 22DE98 | peak_world.inc:137 / 182 | match | static |
| the Transport's dome is allowed by the WS10 update once the Transport NIS ends (27AA80 false) | 0x235808..0x235838 | allowed at the Load (peak_world.inc:286) | **differs** (ranked 4e) | to-c-x: Load 1441, allowed 1471, DSKY 2 -> 7 at 1472 |
| WS14 arg 1: the enter places (11D390 @0x2363DC); sub 1 -> 27A860 + 28E8C0(20) | 0x236250, 0x2365A8..0x236650 | main.js transportInWorld | match except the enter placement (known) | to-c-x |
| "Loading..." caption 0x1F30D8, bit mask [W+0x94]+4: bit 0 set by WS14 at in-air (0x2366F0) and cleared at WS10 / WS1 enter; **bit 1 while an NIS list waits for a read** (0x2788B0); bit 2 WS1; bit 3 WS13; bit 4 233C50 | as listed | cutscenes.js ~:914 | bit 0 match, **bit 1 missing** (ranked 15) | to-c-x: bit 0 from 926 through 1441 |
| streaming starts at the held loop (22CEA8(dest, 7), +0x1C8 = 1 when slot 1's group is 14 / 20) | 0x236658..0x2366C4 | at in-air (free-ride.js:503) | **differs** (ranked 4b) | to-c-x: 22CEA8 at 1044 = #122's start |
| the loop's release conditions: 22D278 + the page wait around 123F38 + an idle fade + no NIS read; then 27A9F0, +0x1C8 = 0, 22D088(dest, 7) | 0x2369C8..0x236AC8, 0x3A9658, 0x3AA028 | rows + collision + draw ready | **differs** (ranked 4c) | to-c-x: rows 1053 -> 1290, page wait to 1441 |
| after the release: #122 fades out 30, the rider placed at the script end, WS4, a 30-tick fade-in with the HUD bright, the bars slide out over ~25 ticks | 279070 -> 2766D0, 123B48 -> 11D390 | a hard cut, instant placement, no slide-out (main.js arrivalFade 30 stays) | **differs** (ranked 4a) | to-c-fade frames |
| arrival placement: courses 11DE60(rider, 1, 2), backcountry grid slot 0, stations session point 0 | 11D390 | free-ride.js arrivalFor / placeRegion | match | peak1-arrive-* gates, to-c |
| the page-wait viewpoint uses 123F38's own table (course < 14 -> (2, 1); 14..16 -> (2, rider+0x86C); 17..21 -> (2, 0)) | 0x123FDC..0x124068 | n/a | info | static |
| WS10 enter: 158E30, 146E10 at a station, caption bit 0 cleared, 2B3A98 (music resume), 2A4B68 (stop pktrans) | 0x2355C0..0x2357CC | career-ui.js enterWorld / courseChanged, gameAudio.arrived | match | static |
| a location-name banner at a crossing | none found (22DF50 sets only 0x535C08 and the 4A11B8 dirty flag; WS10 / WS11 post no HUD message; the only crossing message is the peak-run split 0x2A / 0x2B, 23B5F8) | none | match (unconfirmed: not exhaustive) | static |

## 5. Radio / DJ and place audio

| rule | PS2 | port | status | evidence |
|---|---|---|---|---|
| DJ timer kind 0: stop the line, Radio BIG intro (1, 0), flush; outside a challenge First_Spoke; on Peak 1 also Text_Message (2) | 28E56C..28E5FC | game-audio.js ~:408-410 | match | static |
| kinds 1 / 2 (hub): Radio BIG intro (1, 1) / (0, 1), flush, hub chatter pending (+0x574C) | 28E600..28E674 | ~:411-413 | match | static |
| kinds 3 / 4 / 5: song -1 -> nothing; 3 / 4 Radio BIG intro; in a challenge only the artist intro; **Free_Ride_Intro only while P+0x278 bit 12 (Peak 2 locked, 146008)**, on Peak 1 with 579C[course] = 1 and course 0-7; 579C cleared either way | 28E678..28E7DC, 145F90 | ~:414-423 (no Peak 2 gate) | **differs** (ranked 10) | f95-after: bit 12 1 -> 0 at the pass |
| kind 7 (backcountry): Radio BIG intro (1, 0), artist intro, BC_Intro; kind 6 and > 7 nothing | 28E7E0..28E85C | ~:424-426 | match | static |
| idle queue 2A43B8 (fixed order; Outro falls through to Finish_Line); request life 10 slots x 180 frames (2B1458) | 2A43B8..2A4538 | ~:522-538, audio-speech-events.js ~:847 / 853 | match | static |
| 579C taken at every CTM world load from the saved per-rider +0xACC | 2869E0..286A3C | per page load, shared by all riders (main.js passes no `visited`) | **differs** (ranked 10) | savestates: 579C = the career mask |
| backcountry first visit: hubFirst 0, cinematic flag, BC-intro flag | 234FE0..235058 | introStart ~:372-384 (session mask) | differs (ranked 10) | static |
| world-load DJ 2A4A78; world load 2867E8 (PickNextSong + PlayMusic(0), skipped under pktrans) | | ~:391-394 | match | static |
| MusicTrigger 28D988 (kinds 11 / 13 / 14-17 / 18 / 42, rearm and latch) | 28D988..28DED0 | ~:571-600 | match | static |
| ChangeSong 28DF18 (DJ busy -> retry 100 ms; DJ off -> 2 s fade; event 18; request kind 1 at 2 s; DJ kind 3 at 1.5 s) | | ~:603-613 | match | sjB run |
| requests 28E088 / 28E100 (kinds 0-4), PlayMusic 28CF98, radio modes 28BF78, DJ gate 287558 | | ~:296-350, audio-engine.js:93 | match | static + to-c capture |
| code 20 (travel) in the world: stop, pick, request kind 2 at 10 ms, pktrans for an unvisited backcountry, Radio BIG intro (0, 1) + chatter pool 5 when the destination differs | 28EBAC..28EEB4, 2A4718 | travel() | match in the world; **world switches take the load path** (ranked 2) | audio/postevent2 |
| Radio BIG intro stops the current line first (2B11B0) | 0x2A272C | queues behind it (audio-speech.js:216) | **differs** (ranked 12) | static |
| code 19 (arrival): backcountry kind 4 / 5 + event 0; BC intro -> Peak1 hub song / DJ kind 0 at 3 s / 2.5 s or kind 7; hub Radio BIG intro + chatter; course nothing | 28E8E0..28EBA8 | cinematicEnd ~:444-463 | match | Metro-City: nothing |
| booth flag +0x5818 for 30 s keeps a speaking DJ line over the travel | 302410, 2A49E8, 2A4A38 | pv boothDj | match | earlier capture |
| hub chatter 2A2E50: **post-event commentary first**, then Char_Stories, Peak_Boss forced, shuffle without repeat (8 or 5 categories) | 2A2E50..2A3140 | hubChatter: no commentary | **missing** (ranked 9) | 0x2102 at 15614 |
| the post-event record 2A45C0 / 2A4660 and the commentary 2A4770 | 287070, 2A4770..2A49C4 | none | **missing** (ranked 9) | after-final/final.p2s, postevent2 |
| Peak_Boss dropped once the backcountry's kind 4 and 5 records exist (+0x573C bit 3) | 2A2F44..2A3028, 158910 | none | missing (ranked 19) | static |
| same-location transport 28EF90 also clears the pending Text_Message | | ~:999 | differs (negligible) | static |
| pause resume stops speech / clears the pending DJ only after an in-game song change (+0x5828) | 289BE4..289C04 | cancels queued speech on every resume | **differs** (ranked 12) | static |
| ambience: start only with the slot-9 bank loaded, 5.03 s fades at connectors, keep-alive, the bank unload hard-stops the voices | 29D290..29D6C4, 286CA8 / 286D18 | audio-world.js:98-185 | mostly match (starts late, no unload stop) | static |
| Now Playing box on every PlaySong with audible music, non-empty title only | 28F478, 1EC2E0 | game-audio.js:213, audio-menu.js:181 | match | 9.11 |
| DJ duck 65 % over 1 s, 0.5 s release | 29F000, 285BF8 | audio-speech.js | match | |
| NIS DJ / PA cues: scripts use only cues 1, 2, 7 and 0xF (2A19D8) | | cutscenes.js ~:536-545 | match | nis_cues.py |
| no idle, time-of-day, money, collectible or peak-unlock DJ line exists | callers of every category function | | match | caller scan |

Doc corrections:
- docs/audio-logic.md 9.9 calls 146008 "always 1"; it is the Peak 2 lock, and it clears at the pass.
- The peak-mountain.md Gaps note about the artist intro after the plane is stale (fixed in 9.11).

## 6. The stage programs of the streamed worlds

The seeds (web/generated/peak_stage_seed.hpp, peak2_, peak3_, mountain_) are word-for-word the disc's bam.ssb stages:
- 1579 / 1491 / 1004 programs in PEAK1 / 2 / 3, 3995 in MOUNTAIN;
- the PEAK handler rows are a subset of MOUNTAIN's.
- All 43 location rows of 0x442488 / 0x442168 are in MOUNTAIN, except **TRANSP (track 0, 5 programs), which is in no seed**:
  - two section hides on the heli / gondola;
  - one global program with the 4 in-air NIS callbacks;
  - two empty globals.
- All 5,157 instances with a slot-1 or slot-3 program are in MOUNTAIN/SECTIONS/sections.json with matching slots and cells.

**Entry kinds.**

| entry | PS2 | MOUNTAIN programs |
|---|---|---|
| slot 0 | 30A610 via 34FCA0 -> 2D1A78 | 0 |
| slot 1, section enter | 30A270 / 30A298 from 30A3A0 (0x101B60) | 1,136 |
| slot 2, contact | 30A060 from 121818 and 34FE00 -> 2D19B8 | 1,382 |
| slot 3, section leave | 30A2E8 / 30A548 from 30A460; also 34FCE0 -> 2D1A30 when a MultiSpline's count reaches 0 | 128 |
| slot 4, finish | 30A598 / 30A5C0 via 2D19E8 <- 34FCC0 (LiveComp 341F38, Debounce 342D10, MeshAnim 352230) | 577 |
| slot 5, timer | 30A660 / 30A688 via 2D1AC8 <- 34EBA0 (LiveComp tick) | 162 |
| global programs | stage +0x10 / +0x14, run by 309F18 (230158, 230274, WS10 enter 0x2356B0, 236FFC) after freeing the collectible slot (30C390) | 162 |
| named callbacks | 309E50: NIS 280640 @0x2807FC (list +0x1C, count +0xEC); race start 234BE8 "StartlightBegin" | 41 programs register |
| missions | stage +0x20 / +0x24, run by 308C60 | 891 |

**Programs by purpose** (PEAK1 / PEAK2 / PEAK3 / MOUNTAIN).

| purpose | count | port |
|---|---|---|
| Big Challenge mission / task programs | 317 / 332 / 242 / 891 | mission_gameplay.inc; the 47 prop LiveComps and the marker scroll are not exported |
| node state (DeadNode / RestoreNode), mostly finish | 214 / 201 / 127 / 510 | exact |
| MeshAnim breaks (signs, trees) | 232 / 190 / 114 / 503 | exact (all 1,328 targets exported) |
| Big Challenge offer volumes / markers (40, 87, 21) | 124 / 84 / 64 / 272 | status ops ported; marker scroll missing |
| LiveComp animation | 116 / 79 / 43 / 238 | handled (the 14 ERA5 `searchlight_blue` targets are in no export; reason unconfirmed) |
| particles (16 / 25 / 26 / 69 / 106) | 81 / 75 / 50 / 206 | handled |
| camera shake / lightning (91 / 92) | 22 / 77 / 50 / 149 | handled |
| UV scroll (21) | 49 / 39 / 32 / 120 | JS from uv-scroll.json (unconditional instances, from the load, not per section) |
| sounds only (30 / 31 / 73) | 44 / 55 / 9 / 108 | handled (AE_SCRIPT_SOUND -> sfx-game.js -> audio-world.js `script()`) |
| crowd 2D (88) | 36 / 38 / 34 / 108 | handled on the event courses |
| **Spline pieces (19)** | 25 / 32 / 17 / 74 | **inert** (ranked 3) |
| **MultiSpline (20)** | 13 / 2 / 4 / 19 | **inert**, only the cables are drawn (ranked 3) |
| **ParentModifier (18)** | 11 / 2 / 8 / 21 | **unhandled** (ranked 3) |
| **log teeters (6) / RailModifier (48)** | 11 / 16 / 9 / 36 | 6 is a no-op in the streamed worlds; 48 in the streamed worlds is unconfirmed (ranked 18) |
| mode fences / reset planes (43 / 108), Wrong Way (27 type 5), teleports (34) | | course-limits agent |
| collectible pickups (37 / 38 / 39, 90 / 97) and setups | 26+8 / 26+8 / 20+6 / 72+22 | handled; setup timing differs (ranked 20) |
| streaming / station triggers (68: actions 0 / 2 / 3 / 4 / 5) | 23 / 25 / 15 / 57 | handled (key0 ignored for 3 / 4) |
| event gates (67) | 6 / 5 / 5 / 16 | handled (main.js gate listener) |
| point / multiplier / boost pickups (99) and pickup effects (27) | 17+17 / 19+19 / 18+18 / 54+54 | handled; 99 approximated (ranked 20) |
| flags (12), one-way volumes (7), crashbags (15), rail groups (35), tunnel light (74), avalanche triggers (94 / 95) | 47, 23, 14, 18, 1, 9 | handled |
| NIS callbacks: StartgateOpen (race GO doors) | 5 / 5 / 4 / 14 | n/a in free ride |
| **NIS callbacks: station departures (gond_dep #126, heli_dep #147)** | 6 / 6 / 3 / 15 | **not run** (ranked 6) |
| NIS callbacks: backcountry heli arrival, the new-career plane | 3 / 2 / 2 / 7 | run through the staged sets (pv bcHeli, planeFx) |
| avalanche definitions (93 / 96), MultiParticle groups (105) | 9+4 (MOUNTAIN) | data-driven (avalanches.json) / built once (ranked 20) |
| FAQ "?" (100 / 110) | 2 / 0 / 0 / 2 | handled |
| empty global setups | 32 / 34 / 24 / 86 | n/a |

**Builtins these programs call, and the port** (dispatch lines in web/stage_script_gameplay.inc unless noted):
- **Exact:**
  - 0 (2FC0D0) :340, 1 (2FC7D0) :336, 2 (2FC420) :321, 7 :271, 12 :277, 13 :332, 15 :350;
  - 16 / 25 / 26 / 69 :250-251, 28 / 54 / 55 :281-283, 29 / 58 :322-323, 30 / 31 / 73 :308-310, 35 :305;
  - 44 / 52 / 61 / 77 / 87, 67 / 68 / 100 / 110 (peak_world.inc:174-202), 74 / 91 / 92 / 94 / 95, 88 / 90 / 97, 105 / 106.
- **Approximated:**
  - 3 (LiveComp; section starts drawn by the JS player, unexported targets inert);
  - 21 (JS plays only the unconditional exports);
  - 37 / 38 / 39 (timing);
  - 99 (nil).
- **Inert in the streamed worlds:** 6, 19, 20.
- **Unhandled:** 18.
- **Skipped by the setup filter:** 93 / 96 (replaced by avalanches.json).
- **Correct no-op:** 101 (the only consumer is the stub 11A0C0).
- **Other owners:** 27 / 34 / 43 / 108 (course-limits agent), 40 / 47 / 50 / 51 / 59 / 63-65 / 75 / 76 / 78 / 79 / 81 / 83 / 86 /
  98 / 107 (mission_gameplay.inc).

Full per-program dumps: `local/ctm-decomp/stage/dump_{PEAK1,PEAK2,PEAK3,MOUNTAIN}.txt`.

## Overlaps with other agents

- The world-state machine / event lifecycle: WS10 -> WS4 under the arrival lists (ranked 5), the finish-tick posting
  (ranked 11), the gate / WS10 mission resets (ranked 13).
- CTM screens / menus: overlay 0xC / 0xD (the finish panel, which freezes the mail icon), the ignored early D-pad input at
  the offer prompt, the pause gating during the Transport loop (0x231840; can the player pause there? unconfirmed).
- Course limits: builtins 2 / 27 / 34 / 43 / 108, the log teeters (ranked 18).
- The post-Transport freeze / area-entry stalls: the load-screen cost at crossWorld and Transports (ranked 1, 2), and the
  port's own draw gate.

## Scratch tools and captures

- `local/ctm-decomp/`:
  - `BRIEF.txt`;
  - `asm.py`, `lun.py`, `progof.py` (copies of the course-limits listers);
  - `mcomm/hudmail.py` (HUD mail fields and the inbox from a savestate);
  - `nis/all.s`, `findword.py`, `scan_builtin.py`;
  - `bigchal/` (`offers.py`, `offer_table.py`, `scan40.py`, `bc_trace.py`, pad scripts, `offer-table.txt`);
  - `crossing/xtrace.py` (a per-tick watch of the rows, WS11 / WS14 substates, streamer +0x1A4 / +0x1B4 / +0x1C8 / +0x1D0,
    caption bits and view slots);
  - `audio/` (`state_fields.py`, `career_bits.py`, `nis_cues.py`, plans);
  - `stage/` (`catalog.py`, `classify.py`, `coverage.py`, `spline_scan.py`, dumps).
- `local/ps2-capture/ctm-decomp/`:
  - `crossing/to-c-x` (rider-trace.json: the full timeline), `crossing/to-c-fade`;
  - `bigchal/{tri-offer,down-no,no-offer}` (with `*.hooked.p2s`, `tri-sheet.png`);
  - `audio/postevent`, `audio/postevent2` (`events.jsonl`, `after-yes.p2s`).
- All runs were silent (volumes 0, no PS2_CAPTURE_AUDIO) and used derived states; no original savestate or the ISO was
  touched.
