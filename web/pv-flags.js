// Presentation-leftover switches (docs/presentation.md). Deploys ship the whole shared tree, so each item stays OFF
// (the shipped path unchanged) until it is verified against PS2 frames and in WebKit, then its default flips here.
//   ?pv=1              every item on          ?pv=0   every item off
//   ?pv=flyover,-heli  the defaults, with flyover on and heli off (a name turns it on, -name turns it off)
// Items: heliLight (the backcountry heli lit as a flag-0x4000 instance: the object bank per vertex, docs/presentation.md 16),
// heliHover (the heli hovers on (551..677) after the arrival NIS instead of the world's frame-0 copy), bcHeli (the backcountry heli of the <bc>_heli_arr / heli_arrb_<char> arrivals, SETS/<LOC>HELI; docs/presentation.md 16),
// nisProjection (a NIS view's projection is the letterbox block's 0.75 / 0.63 scales, P11 / P00 = 1.12, idle steps the
// player's; docs/presentation.md 15), help (Select Peak help wrap), cheat (Enter Cheat dimming), skyClear (black GS clear behind the sky),
// boostLight (trick-boost rider light), aiFx (computer riders' streamers / aura), worldWrap (static-model CLAMP),
// plane (new-career plane), heli (heli ride over a peak change), flyover (no black hold after the event fly-over),
// rivalIcon (the rival's '!' in GS byte space), planeFx (the plane's snow spray and engine loop), heliSky (the departure sky
// behind the transport loop over a world switch), regionTick (Fog / ScreenTint / Sun / glare / rider Lighting of the streamed
// worlds switch regions on the core's painter-region tick, docs/weather.md 10), planeCam (a NIS camera anchored on a live actor
// takes the actor's rider as it is at the cut: the #163 door close-up), sparkle (the terrain snow sparkle of patch flag 0x800000, its density the Surface painter,
// 0x38D690), acrossLoop (the transport loop drawn over a world switch writes canvas alpha 1, as the page's fog pipeline does),
// raceHud (the race clock 0x1F16C0 and the speed widget 0x2200C0 in the original HUD text, docs/visual-parity.md),
// hudText (HUD font draws at the renderer's vertex RGB colour x 204, 0x378808: white HUD text at 204 as on the PS2),
// luiResults (the in-race cards and results in the original OV.LUI panel OV_darkblue / 43final_standings, web/results-lui.js).
// replay (the run re-simulated from its recorded pad behind the post-race screens with the replay cameras, and the results
// Replay item: web/replay.js, docs/replay.md).
// On (verified against PS2 frames in Chrome and WebKit, docs/presentation.md): help, cheat, skyClear, boostLight, aiFx,
// worldWrap, plane, heli, rivalIcon, planeFx, heliSky, regionTick, planeCam, acrossLoop, sparkle, nisProjection, bcHeli, heliLight, heliHover; raceHud, hudText, luiResults, finishBanner, recoverMeter, hudStandings, luiLights, cashGap, streamers, resultsMenu, uberLayout, additiveNoZ (docs/visual-parity.md); replay (docs/replay.md). Off: flyover (the event package prefetch under the fly-over:
// measured +70-80 MB on the phone-class peak, docs/presentation.md section 3).
// finishBanner (the race / freestyle finish and TIME'S UP banner 0x21F660 with the finish time or score under it, web/trick-hud.js;
// docs/visual-parity.md).
// Round 2 (docs/visual-parity.md sections 12..): recoverMeter (the crash recover label and bar 0x21D9A0, web/trick-hud.js), hudStandings
// (the freestyle standings rows 0x1ED104, web/trick-hud.js standings), luiLights (OV_darkblue's header lights on the panel's own
// timeline, web/results-lui.js), cashGap ("Cash: $ 10,000": 0x198AF0's '$ ' in the rewards list), streamers (the rider's air
// streamers 2EF950: strm's rows as the GS samples them and the render-matrix scale, web/boost-renderer.js + core
// set_rider_fx_render_scale), resultsMenu (the results menu's item layout and initial focus, web/career-ui.js), uberLayout (the Uber
// hint laid out from the 0x1E92A8 record, web/trick-hud.js), byteBlend (static-model additives in the encoded pass, off), additiveNoZ
// (the static-model additive class drawn without depth writes, ZBUF ZMSK 1: web/world-material.js).
// sprayReset (the rider FX reset 111890 -> 2DF3B0 zeroes the kicker buildup FX+0x10 at a placement and at the CTM plane drop:
// no carry-off snow rising from a rider placed in the air; web/animation_bridge.cpp set_fx_reset_kicker / rider_fx_reset, web/main.js).
// luiWrap (a results-panel text with flag 0x80 wraps where 0x3A0D00 does: the font's advances x the scale % against the element width, both
// in PS2 units; the rival card's 'Face off against Nate in a Rival / Challenge!': web/lui-player.js ps2Wrap, web/results-lui.js).
// dropCamera (the CTM plane drop's rider orientation and camera as the PS2 has them on its first tick: the rider lies head first and
// the camera looks down the slope, not out at the horizon; web/plane-drop.js, web/main.js resetPhysics, web/free-ride.js PEAK_STARTS).
// dropStreamer (the plane drop's air streamers as the PS2 has them on that tick: scroll 0.81 and 4 rows, so the ribbon behind the falling
// rider is strm's long bright band, not its short dark end; web/plane-drop.js streamer, core rider_fx_streamer_seed).
// dropPose (the plane drop's rider in the air controller with that tick's air clips 287 / 416 and zeroed ground-control triplets, as the
// PS2 has it: the body and the camera's look point on the PS2's through the fall; core drop_air_seed, web/plane-drop.js animation).
// liveCompObject (a LiveComp owner drawn by its Object player, livecomp.json draw 'object': The Throne's summit flag pole at the rival
// card's left; web/set-pieces-renderer.js, tools/export_livecomp.py).
// readyLight (the rival card's rider shaded with the ready state's lighting, not the unlit menu fallback: web/main.js readyView).
// switchIcon (the switch-stance 'S' under the boost meter: faint while regular, full while riding switch; web/trick-hud.js, web/ui.js).
// painterWorldLoad (a Conquer the Mountain world load's ride start: Fog, Sun, ScreenTint and glare start at their class defaults and
// the start location's values blend in at its rates, as on the PS2 after the lodge's Return to Game; web/free-ride.js placeRegion).
// On (2026-09-27): the painter values equal the watched PS2 re-capture of peak1-green-start; Chrome and WebKit frames match it better.
// bindPoseProbe (not a presentation item: field diagnostics, web/diagnostics.js): for 60 s after a load screen opens or closes, a cutscene
// or a ride starts, a few skinned draws a frame are checked for the bind pose (CPU skeleton and the uploaded bone matrices); the first
// hit of a session goes to the /mp/diag telemetry ('bind-pose'): the T-poses reported at a first CTM start, not reproduced in the lab.
// On (2026-09-27): at most 3 checks a frame, 7 us each (64 bones, desktop) while armed; no false hit over the new-career flow (Chrome,
// WebKit), a bind-pose control mesh reported in both.
// shadowAtlasInit (not a presentation item: a render fix, web/rider-shadow.js): the rider shadow atlas is made a render target when the renderer
// starts, before any material binds it. A streamed world compiled under the load screen bound it first, and the first shadow then made three
// replace its texture: every later frame failed validation (the rider vanished at a PEAK1 station, the frame froze in PEAK2 Ruthless).
// On (2026-09-27): no failed submit, the rider drawn, Chrome and WebKit.
// hudHints (the HUD's hints bit 0x1000000: the RECOVER = button label and the Uber hint): off once the rider's profile has visited a Peak 2 or
// Peak 3 location, as 0x1EBA10 clears it (PS2 free-ride states on Peaks 2 / 3: clear mask 0x01000000; Peak 1: 0); career and free ride.
// singlePause (web/career-ui.js, ui.js): a Single Event's pause is the MCOMM PDA with Return / Restart / Audio / Options / Quit (no Messages,
// no Give Up) and their help lines; Quit asks 'Quit Game' (No focused) and Yes goes to the title (PS2 menus/single/11-single-pause, r3-pause-*).
// The online race pause (ui.js 'pause') is drawn in the same PDA. On (2026-09-27): rows, help, Restart / Quit popups, Chrome and WebKit.
// pauseRestart (web/career-ui.js restartToCard): the pause menu's Restart plays only the start-gate idle under the round's card (cutscene
// kind 'restart'); the gondola ride back up (kind 'heat') stays for the results' Restart (PS2 menus/race/r3-restart vs r3-results-restart).
// On (2026-09-27): the lit gate at once, then the card, as the PS2 (Chrome, WebKit).
// loopFadeOnce (web/cutscenes.js fadeAt loops): a looping step's fade-in plays on its first pass only; the start-gate idle under a heat's
// card (after the start hut's fade-out record) dipped to black on every loop (PS2 menus/race/r3-rr-dense: the gate stays lit under the card).
// On (2026-09-27): steady under the card (luma 184 vs PS2 179), Chrome and WebKit.
// lodgeLui (web/career-ui.js drawLodge, fe-screens.js drawLodgeMenu / drawLodgePrompt): the career lodge's menu is FE.LUI 28lodge (cFEStateLodge:
// the orange frame, 'Lodge - Peak N', rows, focus bar, help lines 'Save your progress.' / 'Quit out to Title screen.'), and its Yes / No questions
// the FE popup with its veil, sized as the PS2 draws it (menus/ctm/62-64). On (2026-09-27): Chrome and WebKit.
// startRules (web/start-rules.js, main.js, gamepad-menus.js, career-ui.js back): Start opens the pause only from the ride (the game update's
// 0x230A34 gates: not after the finish, not in a cutscene or a transport); on every menu (the pause and MCOMM too, results, the lodge,
// Transport, cards, prompts) it is the menu's accept, the LUI's UINext (Cross or Start); Triangle closes the free-ride MCOMM. Owen: the
// lodge's pause dropped him outside the lodge; Start paused after a finish. On (2026-09-28): the 17-row Start matrix in Chrome and WebKit
// (tools/startmatrix.mjs) and the PS2 pad captures (menus/startprobe).
// sessionMap (web/session-map.js, career-ui.js drawSessionMap, main.js freeRideSessionMap): the MCOMM Session is OV.LUI 38session: the rows,
// the location's map picture (ses_<x>), its session points (orange dots; the focused one's highlight) and the rider (0x2086A8..0x209E78),
// opening on the point nearest the rider (0x26B680). On (2026-09-28): PS2 menus/ctm/r3-session-* (open, every row, the question), Chrome
// and WebKit.
// pdaOptions (web/audio-menu.js pda-options / pda-more, ui.js): the in-game Options (every pause / MCOMM "Options") is the PS2's PDA page
// OV.LUI 37beoptions: HUD Options, Camera 1, Camera 2 (greyed), the three sliders, DJ Speech, Arcade SFX, Save game (Conquer the Mountain
// only), and the port's Widescreen / Keyboard / Display & Touch behind "More options" in the online-only EA Talk row.
// On (2026-09-27): every row, value and help line as PS2 menus/single/r3-options and menus/ctm/47-options, Chrome and WebKit.
// genericFogOff (web/main.js): with the original fog pass (fog-renderer.js), three's linear scene fog is detached from the scene. Materials made
// after the course load (every streamed free-ride location, static cell and set piece) kept it on top of the original fog: fully fogged at
// the Fog painter's far (ABA1 arrival, far 80 m: near-white trees and signs; BHP1: a white city). On (2026-09-27): transports to ABA1, ASS1,
// BRA2, BHP1 match the PS2 frames' trees, signs and buildings in Chrome and WebKit.
// finishLui (web/career-ui.js finishLui, tools/export_audio_menus.py -> UI/audio-menus.json): the freestyle finish panel is OV.LUI finishov,
// set up as 0x1E8200: the notched 3D Ov frame growing in, the run label by round, 'Nth place', '%d pts', the medal of a final or Rival
// Points run (career), 'New Record!'. It was a flat rounded box. On (2026-09-28): PS2 pipe-finishov2 4589..4634 panel MAD 12.6-39.7 ->
// 2.1-8.4 (Chrome, WebKit), cba2-full 2818, nav/bc/out-jam-finish.
// riderPoseGate (web/main.js): a rider model without a pose is not drawn. The page's placeholder rider (RIDER_SAM, built by every course load
// before the chosen rider) and a newly loaded rider start hidden, and in the ride screens the rider shows once a tick has posed it
// (currentRiderFrame), as the PS2 draws its rider after the update that poses it. It had drawn at the bind pose: the placeholder under
// the held transport loop across a world switch (the field bind-pose probe: 26 bones, 'loading', PEAK1/1), and the career rider on the
// ride's first frame after a switch (in view, Black Top Station). On (2026-09-28): 0 bind-pose draws over PEAK1 -> 2 -> 3 -> 1 -> 2 -> 1
// switches, Chrome and WebKit.
// stationArrival (web/free-ride.js, main.js): a Transport to a station plays no lodge walk-in and raises no lodge prompt (world state 14 arg 1:
// lists 22 / 23 and overlay 0x1F are arg 0's, the lodge door's); the station fades in from black like a course (30 ticks, the HUD over it).
// On (2026-09-28): PS2 stations-sj-to-green and stations-to-c, Chrome and WebKit.
// sessionFade (web/main.js): world state 15 (MCOMM Session Yes, a Transport to the current location) fades in from white over 60 ticks
// with the HUD over it (2E4370(mgr, 1, 0, white, 0, 0, 1.0), 2E47E8 alpha 1 - t / 1.0 at the 60 Hz timer). On (2026-09-28): PS2 stations-ws15.
// arrivalFade (web/main.js, cutscenes.js fadeFrom hud, ui.js): a Transport arrival at a course fades the world in from black over 30 ticks with
// the HUD drawn over it (PS2 aba1 arrival: luma 6 % at +1, 40 % at +12, clear at +30; the port cut from black to the full world).
// On (2026-09-27): within a tick or two of the PS2 ramp in Chrome and WebKit.
// menuRiders (web/main.js, opponent-riders.js): the computer / online riders hide while a menu covers the world (pause, options, audio,
// popups), as the player's rider already does; they drew over the MCOMM pause. On (2026-09-27): results and replay keep them.
// Front end (docs/intro-movies.md, docs/ctm-parity.md): attract (the title's 1801-frame idle plays intro.mpc, 0x1948A8, web/fe-attract.js),
// bootMovies (the power-on intro: the DJ cut once per page load after boot:ready; the movie mask 0x1A27A0 without its EA / THX bits), transportMap (the CTM Transport
// map's routes, start indicator, station markers and peak outline from the Map LUI, web/ctm-map.js), fsStandings (the CTM freestyle
// heat results in OV.LUI 42freestyle_standings: Heat 1 / Heat 2 / Total, web/fs-standings.js).
// mcommIcons (the MCOMM row icons filled as one path per flat shape, no canvas seams; the badge temperature set when the PDA opens,
// 0x20A778 / 0x20A854, web/ctm-pda.js).
// On after PS2 frames in Chrome and WebKit (2026-09-27, front-end agent): attract, transportMap, fsStandings, mcommIcons, and
// bootMovies as Owen chose it: the DJ intro once per page load after boot:ready (no EA SPORTS BIG / THX; skippable; dropped
// when it has not started within 2 s; not for event / online links, ?qa=1, automated browsers, docs/intro-movies.md).
// Lodge and career (docs/career-events.md "Lodge shops, awards and attributes"): careerRider (the human wears the profile's
// committed gear and performs its Ubertrick Setup selection in every run: the rider entry is resolved again from the current
// mode's record at world / event loads and run starts, web/career-rider.js), lodgeDetails (the lodge's Rider Details: Career
// Highlights, Player Name, Rider Profile and the 66ut_btnmap Ubertrick Setup with buying), playerName (Player Name: "PLAYER 1",
// 8 characters, and the name in the records, 0x147170 / 0x154DDC), riderMusic (radio mode and custom playlist per rider,
// R+0xF80 / R+0xF78, loaded at each world load 0x2867E8). All four on after the PS2 lodge runs (local/ps2-capture/lodge) were
// matched in Chrome and WebKit.
// lazyCourse (not a presentation item: the first load, docs/first-load.md): the title and the menus start with the front end
// only; the page's course loads behind them from Press START (web/main.js lazyStart / backgroundCourse, web/boot-screen.js). On:
// measured and verified in Chrome and WebKit (docs/first-load.md "Title before the course").
// lodgeCheats (the lodge's Rider Details > Cheat Characters once the rider owns one: the 131cheat_char list, and the pick is the
// career rider's skin in play, setup slot +0x12; docs/career-events.md "In play").
// bigChallengeAudio (docs/audio-logic.md 3.9): a Big Challenge's start / completion / stop audio as 29D6E0 / 29D8E0 / 29DBB0 (the
// song's loop bank attached before the event, so its challenge stinger plays, Wobble's own one included; the hub-song branch;
// 0x6D + Arcade_Prompts 1 at completion), the overlay track at its latched byte on the master, random part heads from the
// audio clock ((ms / 23) & 0x7F); web/game-audio.js, web/pathfinder.js. On after the ARMSX2 runs (Wobble 649 / entry 66, Avalanche
// 488, completion 0x6D + 0x20A8) and headless Chrome.
// lodgeRewards (the lodge's Rider Details > Rewards is the rewards room 128rewardsroom / 129rewardgallery that sells, with the buy popup).
// freshRider (not a presentation item: a free-ride world start places a fresh rider, core fresh_rider_start: +0x2E4 speed limit,
// +0x4CC route heading, +0x438 surface (controller depths), +0x380, +0x244 at 0 before 11D390, as the world load's new rider
// 0x125EB8 has them; web/free-ride.js placeRegion, docs/peak-mountain.md "Fresh rider at a world start").
// finishFences (not a presentation item: the barriers before the Snow Jam / Gravitude finishes in free ride and the peak runs,
// core stage_object_route: their collision instances load unrouted (instance+8 = 2) and builtin 0's Object with key 1 (356DB0)
// turns on the 0x20 static route, as on the PS2; web/free-ride.js, docs/peak-mountain.md "Finish barriers").
// stationFences (not a presentation item: the stations' stage builtin 108, 0x302870 -> 303E60, game mode byte 0x535C12 == table
// 0x446618[n]; core peak_world_builtin108, web/free-ride.js): the peak-race fences and challenge reset planes of every station die in
// free ride as on the PS2 (Green Station's invisible walls before the lodge, docs/peak-mountain.md "Station fences").
// stationFences and freshRider on (2026-09-27): the PS2 capture peak1-green-start (CTM last-lodge start, neutral pad) is bit-exact
// through the lodge door with both; every station's race fences die in free ride as in the PS2 savestates; Chrome and WebKit.
// finishFences on (2026-09-27): the PS2 All Peak Race pushes into the ERA5 / ARA1 finish barriers (apr-part4, apr-finish) stop /
// crash the rider at the same spot in the page (Chrome and WebKit); off, it rides through and is reset.
// loadFlags (not a presentation item: core stage_load_flags, web/free-ride.js): an instance no program has touched holds the
// load's runtime flags (authored high half copied down with bit 1, 34FC1C: 0x200000 -> 0x200022), so builtin 7's one-way
// volumes (flags | 0x100) keep their static route and push a rider riding back up a connector, as on the PS2 (0x200322, entity 8);
// off, the authored word: 0x200100, no route, no push (docs/peak-mountain.md "Course limits").
// loadFlags on (2026-09-29): PS2 capture course-limits/p3b-zig3000 (The Throne -> E, zigzag) is pushed 41.667 cm/s a tick by
// mdl_EBC3_E_onewayvolume_1000; the comparer follows it position-exact with the volume built (PEAK_SEED_BOOSTS), without the push it is 23 m off in 12 s; the streamed-world
// capture gates are unchanged; Chrome and WebKit show 0x200122 on every connector volume.
// CTM start and free-ride streaming (docs/ctm-flow.md "Start and streaming", CTM-stream agent): ctmWorldAudio (the free-ride world
// load's audio 2867E8 / 234F40 runs when the load screen closes, so a new career's plane intro plays pktrans under the ABC1 movie
// and the plane NIS and 28E8C0(19) comes at the ride start, as the PS2 does; the load-screen loop no longer runs on under them),
// streamGate (a streamed location is collidable only once it is also drawable: the +0x1D0 wait 2306B8 holds the game while an
// active row's draw package is still building), streamAhead (the streamed world reads ahead in the PS2's order: one location at a
// time, the rows the current row's connectors lead to first, their draw packages downloaded before the Unload trigger and built
// once the rider rides into the connector; with a single next row it is built under the arrival movie, and the rest of the
// peak's collision is fed only while nothing animates: a movie, a pause), streamWarm (a streamed location's pipelines are compiled
// for the world pass's target and MRT, mesh by mesh across frames, as the cutscene actors are: its first draw builds nothing).
// sliceLoad (the load screen's long core calls cut into small ones: a streamed world's start row fed as its first slices plus appended
// slices like any later location, an event's environment lattice added in slices; the same core state, docs/ctm-flow.md).
// eventSlices (an event course's core world loaded in parts that end in the whole documents' state, docs/ctm-flow.md 8.4): the
// collision.bin triangle tree built across frames (also a streamed world's base course), terrain / world collision / body terrain
// cut in the peak-world worker and fed in parts (one parse of terrain.json for both terrains), the rail catalog in parts, and the
// computer riders' contexts set up from the parse caches by key (no multi-MB texts copied per rider, rails parsed once); the rider
// animation documents parsed a frame ahead (core animation_prepare), the course build's PNG decodes spread, the warm-up's new materials
// per frame following their cost.
// feCompileSpread (web/fe-preview.js): a front-end preview model (the character select's rider, the cutscene cast) compiled one part a
// frame, not the whole model in one task (300-500 ms at 4x CPU after the character select's Cross and under the CTM world load).
// eventSlices and feCompileSpread on (2026-09-28): the core state equals the whole-document loads (web/test-event-slices.mjs, in-page load
// hashes), and a hang hunt found no hang in 45 Chrome (production, repeat visits: warm, evicted, backgrounded, course switch, shared
// rider keys) + 36 WebKit loads (docs/ctm-flow.md 8.4).
// bootChain (web/main.js): the page's first course load (?autostart=1) is on the course-switch chain: a switch asked for while it loads
// (Back / Forward, a course, CTM or a lobby from a callback, an invite) waits for it and replaces its event, never runs alongside it (two
// loads wrote the same globals: a stuck page on 'game' with no core). docs/ctm-flow.md 8.4. On (2026-09-28): the switch variants of the
// stress harness (a course, CTM, Back, the lazy course, a switch in the event load) 0 stuck of 108 Chrome + 48 WebKit loads (8 of 15 before).
// lodgeWorldLoad (the lodge's Return to Game is a world load on the PS2: 0x1A11C0 free ride at the last lodge 146D98 ->
// cGameLoadStateOutLodge 118loadoutlodge -> WS10 -> the ride, PS2 ctm-parity lr-tri): the world start's ride under the load-out
// screen (a fresh rider, the 11D390 station entry, the ride start and the world load's audio) instead of a respawn at session point 0.
// On (2026-09-27): the ride out of the lodge equals the PS2 world load's (peak1-green-start records within 0.02 cm through tick 285,
// the door at ~394 as in lr-tri) and the lodge's charsel no longer plays on in the world (the hub song and DJ kind 2, as the PS2).
// riderPrefetch (the event load, docs/first-load.md): the human rider's files start downloading when the event is picked (alongside a
// course still loading), the lineup is planned before the human rider loads so its riders download alongside it, the intro's
// cutscene data under the warm-up. sharedParse: the human core parses the course packages' own text (as the node gates do), so
// the computer riders' contexts copy that parse instead of parsing again (web/world_bridge.cpp parse cache).
// boothTeleport (not a presentation item: stage builtin 34, docs/stage-teleport.md): the Metro-City phone booths and water towers
// teleport the rider (0x123210: the human and the computer riders, web/stage_teleport.inc, turned on per rider by
// core._stage_teleport_enable). Off: the old path (the booth program runs, the teleport is counted unsupported). On after the
// PS2 booth captures (local/ps2-capture/runs/booth) matched in the comparer, Chrome and WebKit (2026-09-27, booth agent).
// rivalCard (a rival challenge's objectives card over the ready state: the riders at their start spots and the placement camera,
// as the PS2's WS2 at race tick 0; main.js readyView).
// rivalCardAi (the rival computer rider at its start spot under a rival challenge's objectives card: web/ai-race.js readyPose,
// the placement of start() without its relationship ageing; and the card's camera is the ready state's, the event camera seed
// (core camera_event_seed_view; WS2 does not step it). PS2 local/reference/pcsx2/{happiness,ruthless,the-throne}-ready; on
// 2026-09-27 after Chrome + WebKit, docs/career-events.md "Round 3").
// staticWorld (not a presentation item: free-roam frame cost, docs/sim-performance.md "Free-roam steady state"): a streamed location's
// static batches keep their matrices and sit in 128 m cell groups hidden per render when outside the camera frustum (web/static-world.js);
// the same draws, order and pixels. On (2026-09-27): identical frames in Chrome and WebKit (event, hub, connector, zone), render JS -10..-35%.
// musicStream (loading, docs/audio-logic.md "Music streaming"): a song's .mus streams bar by bar with range requests (the PS2
// streams it from the disc) instead of downloading whole first (charsel.mus 44 MB), reading ahead behind the game's own downloads.
// sharedWorldMaterials (not a presentation item: load and streaming cost, docs/sim-performance.md): the world materials of every package
// share one colour graph per structure (terrain, instance modulate / blend class, UV-scroll group) whose texture nodes read the drawn
// material's textures (web/world-material.js), so three builds one node program per structure instead of one per material. On (2026-09-27):
// identical frames in Chrome and WebKit (same-page legacy swap: event, hub, connector, zone; cross-run: Snow Jam, Metro City, Ruthless).
// rideWarm (loading, docs/ctm-flow.md "Ride start"): a Conquer the Mountain world load loads, initialises and compiles the
// career rider under the load screen, so the ride's first frames do not (main.js warmRideRider).
// staticRefresh (not a presentation item: frame cost, docs/sim-performance.md "Free-roam steady state"): the frozen static world batches of the
// staticWorld cells skip three's full per-draw refresh of node materials (only the shared, camera / rider-shadow uniforms refresh; web/static-world.js).
// On (2026-09-27): identical frames in Chrome and WebKit (same-frame full vs shared refresh: event, hub, connector, zone; cross-run Snow Jam,
// Metro City, Ruthless); render JS -5..-10 %. A per-frame uniform added to the world materials must sit in a shared group (renderGroup).
// gpuRestore (a fix, not a presentation item: web/gpu-copies.js, web/gpu-recovery.js beforeResume): after a GPU device loss the arrays the page
// dropped once uploaded (an event world's vertices, colours, light UVs, vertex alpha) are read again before drawing resumes; without it an
// event course lost its static world (stadium, gates, scenery) after a recovery (iOS Safari loses the device in the background). On (2026-09-27):
// the frame after a simulated loss is identical to the frame before (Snow Jam, Metro City, R&B, Happiness, free roam; Chrome and WebKit).
// gpuRelease (memory, not a presentation item: web/gpu-copies.js releaseWorldCopies): a streamed location's vertex / colour / light-UV / alpha / index
// arrays and texels, and an event world's texels, are dropped once the GPU holds them (restored for a new device, pv gpuRestore). On (2026-09-27):
// free roam low tier Chrome heap 343 -> 238 MB, WebKit footprint ~ -100..-150 MB; a device loss restores identical frames (Chrome, WebKit).
// refreshCap (presentation rate, web/quality.js frameGate): on a display faster than 60 Hz the 60 fps gate draws every n-th animation frame
// (n = floor(Hz / 60 + 0.1)): the frames in between redrew the same 60 Hz ticks, interpolated.
// setPieceSkip (frame cost, web/peak-set-pieces.js): the streamed world ticks only the UV-scroll groups' representatives (the drawn values) and
// skips the LiveComp mesh matrices of hidden locations (set the frame they show). On (2026-09-27): representatives equal the full set over 20000
// ticks with random activations; same paused frame skip on / off identical after a location hides and shows (Chrome, WebKit); sp.update -1..-1.5 ms at 4x.
// skipEmpty (frame cost, web/snow-renderer.js): a snow emitter with no live particles is hidden (it drew nothing; three still ran its
// per-object work: 65 of 690 render objects a frame in a six-rider event; also impact-fx and startfire batches). On (2026-09-27): Snow Jam and
// Metro City cross-run identical in Chrome and WebKit; empty render objects 65 -> 0 a frame with five computer riders.
// staticRefreshWide (frame cost, web/static-world.js): every render object drawn with a shared world material without a UV-scroll uniform
// (blended batches, event terrain, LiveComp / MeshAnim / script-shown batches too) takes the static refresh, and gets a full refresh whenever
// what the static path skips has changed since its last one (world matrix, material opacity / alpha test / version, texture versions: the
// crowd-2d.js frames of world texture 9-161, geometry index / position / colour versions: terrain refinement, flag cloth).
// On (2026-09-28): full refreshes a frame at the Peak 1 start 383 -> 62, Snow Jam with computer riders 554 -> 229; render JS (same page,
// paused frame) Chrome phone 4x hub 33.9 -> 26.7 ms, Snow Jam 26.8 -> 23.1; WebKit hub 8.8 -> 7.3, Snow Jam 6.4 -> 5.5. Same frames: static vs
// full refresh at hub / connector / zone / event (Chrome, WebKit), cross-run Snow Jam and BRA2 (both), device loss (both, event and hub).
// warmSpread (load frames, web/main.js warmupRender): a race warm-up step waits for a frame the frame gate drew (the 30 fps / auto tiers
// skip animation frames: a slice revealed on a skipped frame was never drawn, so its first draws, hundreds of render objects, bind groups and
// index buffers, all fell into the whole-scene frame, 500-800 ms at phone 4x); the post passes build in a frame of their own and the course's
// shared vertex buffers (tens of MB) upload one per frame before the first slice; on a 30 fps tier a drawn frame takes two animation frames'
// slices. On (2026-09-28): longest warm-up frame WebKit desktop 172-257 -> 73-88 ms, WebKit phone tier 199-960 -> 105-124 ms (warm-up time
// the same or shorter), Chrome phone 4x 706-999 -> 310-323 ms; cross-run Snow Jam and BRA2 identical in Chrome and WebKit; no race-frame builds.
// softSprites (WebKit frame cost, web/sprite-canvas.js): Safari's offscreen UI sprite canvases (glyph tints, LUI / HUD caches) are software,
// like the UI canvases they are drawn into: an accelerated one is an IOSurface that WebKit's GPU process locks and converts (BGRA ->
// RGBA, the whole surface) on every drawImage into a software canvas; the load screen draws ~1000 glyphs a frame (60-70 ms frames).
// sharedIndex (frame cost, web/main.js asset, web/gpu-copies.js): a world package's batches draw from one index buffer (the package's
// indices.bin; each batch's draw range and groups at its first index) instead of a copy each: the same draws, fewer index-buffer switches
// (WebGPU calls, each an IPC message in Safari). The event course's terrain keeps its copies (terrain refinement edits them).
// On (2026-09-28): Peak 1 hub, same page and frame: setIndexBuffer 648-692 -> 245-274 a frame, WebGPU calls -6%, WebKit render JS -3..-12%;
// identical: same page shared vs own copies (hub Chrome / WebKit, zone WebKit), cross-run Snow Jam / BRA2 (both), device loss (both, hub and
// event), leave / return paths (both: no validation errors).
// glslKeys (WebGL2 backend load / hitches, web/shader-keys.js stableGlslBufferNames): the GLSL builder names each uniform buffer block after
// its node id (`NodeBuffer_<id>`, `buffer<id>`), so every skinned part and every uniform array had its own GLSL text, program compile
// and link (character select: 111 shader texts, 9 distinct modulo the ids); here as the WGSL path: the buffer's index in the shader
// being built (per builder; shared-group buffers keep their names). Only identifiers change; the WebGL program cache then shares.
// On (2026-09-28): WebGL2 (Chrome) character select 109 -> 10 programs, cycling riders max frame 743 -> 77 ms; Snow Jam load 146 -> 49 programs,
// main-thread link waits (no KHR_parallel_shader_compile, as Firefox) 15.0 -> 0.12 s, longest load frame 4.8 s -> 0.14 s. Identical: WebGL cross-run
// Snow Jam and BRA2 (Chrome), Snow Jam (WebKit), the intro cutscene's skinned actors frozen (Chrome, WebKit); no GL errors.
// warmPost (loading, main.js warmupRender): post-pass variants a course only reaches later are built under the load screen: the glare
// pass (web/glare-pass.js warm, every other warm frame: its levels and final composite, and the fog composite into its frame target;
// Gravitude's glare turns on at the race start: 6 pipelines there, a 1 s iPhone stall), the light-glow occlusion query (web/light-glow.js:
// the warm frames had glows counted but none queried) and three's canvas output pass (the first cutscene frame draws the scene directly).
// A disabled glare draws with zero alphas and every query texel read is written the same frame, so later frames are unchanged.
// On (2026-09-28): Gravitude, sync pipelines after the warm-up 8 -> 0 (race start 6, first cutscene frame 2; Chrome and WebKit); cross-run
// Gravitude and Snow Jam race frames and the frozen intro identical in Chrome and WebKit.
// feMorphTiers (character select / intro hitches, web/fe-preview.js padMorphPart): a morphing FE part (NIS head, hands, board flex) is
// padded to one of two shapes, 256 vertices x 27 morphs or 768 x 36 (zero vertices no index uses, zero morphs whose weight is 0: three's
// morph loop skips a zero weight), so the roster's four morph variants (27x256, 8x256, 36x768, 36x512) share two shaders and pipelines
// (Safari compiles each new one in its GPU process: the iPhone's character-select hitches during the roster prefetch).
// On (2026-09-28): FE morph shader variants 4 -> 2 (character select: 1 pipeline fewer, the boards' flex variant where they show);
// the frozen intro (six riders' heads, hands) identical in Chrome and WebKit; Snow Jam cross-run identical.
// refKeys (pipelines, web/shader-keys.js currentMaterialReferences): three's material property nodes (MaterialNode: materialReference('map')
// ...) are one shared node per property, re-pointed at each drawn object's material, but their setup read the value without re-pointing:
// a material built after another one drew compiled the previous material's map. When that texture differed from the material's own
// texture(map) node the shader bound the map twice, when it matched once: two shader variants of the same graph, chosen by draw order
// (the FE previews: 2 per shape). Here generate points the node at the material being built (its value is replaced per object anyway).
// On (2026-09-28): character-select FE pipelines 8 -> 4 (Zoe 3, Sam 1: his GC texel domain), Snow Jam load 71 -> 68; identical: cross-run
// Snow Jam, BRA2, Gravitude (Chrome, WebKit), Snow Jam WebGL, the frozen intro (Chrome, WebKit, WebGL).
// liveRest (frame cost, web/peak-set-pieces.js, set-pieces-renderer.js, attached-setpieces.js): a LiveComp mesh at rest (no player, the static
// draw) keeps its composed matrix (matrixAutoUpdate off, composed once, userData.lcRest) instead of three recomposing it every frame (the
// streamed Peak 1 world: ~1100 rest meshes, every frame); a player writing it clears lcRest, as before it cleared matrixAutoUpdate.
// On (2026-09-28): objects recomposed per frame at the CTM start 1262 -> 108; every rest mesh's matrix and world matrix equal a fresh
// compose over a ride; cross-run Snow Jam, BRA2, Gravitude identical (Chrome, WebKit).
// oneMatrixPass (frame cost, web/snow-composite.js): the encoded snow composite draws the scene a second time in the frame, right after the
// world pass, and three recomposed every matrix of the scene again for it (scene.updateMatrixWorld: half of that cost in a ride); nothing
// moves between the two, so the second render skips it (scene.matrixWorldAutoUpdate off for that call).
// On (2026-09-28): at 3000 composite renders (CTM ride, Snow Jam with riders) no matrix would have changed; cross-run Snow Jam, Gravitude
// identical (Chrome, WebKit). With liveRest: scene matrix updates at phone 4x ~48 -> ~16 ms a second.
// softGlyphs (WebKit frame cost, web/sprite-canvas.js glyphSource): the same for the glyph tint atlases only, drawn from a software copy of
// the tint made as today (same bytes); the tint canvases themselves keep their kind. On (2026-09-27): the UI canvases identical in WebKit
// (same page and frozen time, today's vs the copy: title, main, event, character, options, setup, details, load screen x4, results, pause);
// WebKit load to the intro BRA2 22-25 -> 11.6 s, CTM Happiness start to control 15.3 -> 10.3 s. Chrome / Firefox: not active.
// Audio glitches (docs/audio-logic.md 9.13, audio agent; not presentation items): audioDeclick (music, speech and the whole mix ramp over
// 5 ms where they stopped or started mid-waveform: pause / resume, an event's cut, Stop, level writes, a context suspend; the song's
// first bar 40 ms ahead; the Big Challenge stingers and their parts prepared at the prompt's Yes), audioInterrupt (a stopped context takes
// no new voices, holds for a movie / the hidden page, resume retried on focus / pageshow / a timer / any input), musicWorkerDecode (every
// music bar decoded in the audio-decode worker, 6-channel bars folded there), sfxStartAfterDecode (a voice's start time read after its
// decode), musicLookahead (the music scheduler 2.5 s ahead, input audio started first: the same schedule, stalls up to ~3 s absorbed),
// sfxWarmFirst (the intro's banks warmed first), musicPrefetchNext (a picked song streams its opening before its request plays it).
// On (2026-09-27) after Chrome (desktop, phone 4x) and WebKit runs: audioDeclick, audioInterrupt, musicWorkerDecode, sfxStartAfterDecode,
// musicLookahead, musicPrefetchNext. Off: sfxWarmFirst (the phone's crowd-loop decode at the intro was not removed: inconclusive).
// heatSong (a CTM race / slope event's Next heat picks a new song from round 2 on: WS13 -> 27A860 -> 28E8C0(20, 1), 0x28EC90..0x28ED18;
// web/game-audio.js heat, web/cutscenes.js). On (2026-09-27): the PS2's qualifier -> Next heat (ARMSX2 music log heat2-audio: 28E8C0(20,1),
// PickNextSong 14 -> 15, PlayMusic 36) is the port's director sequence (test-audio-glitches).
// ctmRestartAudio (a CTM Restart keeps the world's audio: the pause's Restart 2302A8 resumes the song, no event, GO sends 0; the results'
// Restart runs WS13; plus 29C420's countdown rule: at "1" a finished / hub / chartune song is replaced by a paused new one for GO;
// web/game-audio.js restartRun / countdown, career-ui.js restartToCard). On (2026-09-28): the director equals the PS2 music logs
// pause-restart-audio2 and results-restart-audio (test-audio-glitches).
// stallCap (frame pacing after a hitch, not a presentation item: web/main.js frame, web/ps2-frame-pacing.js): offline, a drawn frame
// advances as the PS2's frame loop 0x316F00 does: one update per elapsed 60 Hz tick, at most 12 a drawn frame (app +0x20), the rest
// of the backlog dropped (0x227E68 -> 0x326C60), a 30+ tick backlog lapped by the 30-slot pad ring (stall savestates, test-frame-clock).
// Without it the clock kept the whole debt and replayed it at 12 ticks a frame: a Safari stall mid-rotation fast-forwarded the air
// auto-complete, so the rider seemed to snap from inverted to upright (docs/HANDOFF.md, air release). Online races (mp session
// simulating) keep the full catch-up; the replay clock is its own. Race results are tick-counted: unchanged.
// buyAttribs (the lodge's Buy Attributes, cFEStateBuyAttrib 0x1F4728 / FE.LUI 33buyattribs; docs/career-events.md "Buy Attributes"):
// one Right = one raw point (+0.2) at 0x440550[level-1] per point, pending capped at the next whole level, Cross -> the buy popup, Yes
// buys point by point (0x150C20) and stays; bought levels reach the rider in free ride too (runtime bank, int(raw/5)/11).
// On (2026-09-28): PS2 lodge/attrs ba1..ba3 matched value for value (screen state, profile, runtime bank) in Chrome and WebKit; the
// bought-level ride gate peak1-lodge-attrs is exact to the lodge door (test-buy-attribs, test-ps2-captures).
// fsCelebrate (web/game-tick.js, main.js finishHost, career-ui.js freestyleFinishPlace): at a single-player freestyle finish 0x239230
// ranks the player's run with the round's posted values (0x536640, 238B70) and sets rider+0x100 for a place < 3, so the top three raise an
// arm (finish reaction 315 over 314, web/finish_gameplay.inc), and sets the boost meter by place. Off: the race rule alone (computer
// riders racing), else the flag stays set.
// On (2026-09-28): the real Single Event flow of The Junction (vp-stations/pipe-fov2, ?presentationSeed=0x182200, 6th place): the
// page's world bones equal the PS2 record's at 4580 / 4604 / 4634 in Chrome and WebKit (off: the raised arm, 41-57 cm).
// sectionClock (web/livecomp-animation.js syncSectionClocks): a section LiveComp with a slot-4 / slot-5 program is a core entity too
// (web/stage_world.inc sectionPlayer), and stage builtins 28 / 54 act on its clock: Gravitude's crash billboards play on to frame 149
// and fall when a rider hits their trigger. The JS player of such a resource takes the core's clock each frame.
// On (2026-09-28): PS2 run billboard/grav-bb (the Gravitude race with snaps at 1169..1389): with the exact core clocks the page draws the
// billboard falling where and when the PS2 does (1192 / 1208 / 1229 / 1249, Chrome and WebKit); off, it stands and keeps wobbling.
// rivalRelations (a sim fix: web/ai-race.js): the events without lineups.json (backcountry rivals, Gravitude, Kick Doubt) keep the
// relationship tables too, from the fresh table (web/public/assets/ARA1/lineup-sessions.json) plus the session / profile copies, aged at
// the load (0x155E58) and changed by the rider-pair reactions (0x155BF0). 10F560's record flag +0x1C (relationship >= 2) then follows
// the PS2: Happiness Rival Time (bc-race-tuck2) Mac's soft attacks raise his record of Zoe to level 2 at 599, and the human's 115D48
// picks the peer reaction 319 at 1161 (was 317, RNG off from 1302). Off: those events keep the document's levels all race.
// eventAnchorRng (a sim fix: web/event-anchor-rng.js, web/main.js soloAnchor, web/ai-race.js): the shared game RNG 0x4FF030 at the countdown
// anchor of the events without lineups.json / rivals.json (big airs, super pipes, Gravitude, Kick Doubt) as the PS2 has it (seed 0 + the
// load's draws), not the core seed state's. Before, every random pick differed from the PS2 (Crow's Nest release run: 1776).
// stallKeys (input timing after a hitch, not a presentation item: web/main.js stallKeyInput, web/game-tick.js advance): offline, when a
// drawn frame runs several ticks, each tick reads the keyboard as it was at that tick's 60 Hz sample time (keydown/keyup event
// timestamps), as the PS2's 30-slot pad ring holds one sample per vblank (0x326B88); a key released mid-stall lands on its tick.
// Gamepads cannot be sampled during a stall: they keep the frame's one sample. Touch too.
// bcSpeed (CTM agent, docs/ctm-parity.md "Big Challenges: builtin 59"): stage builtin 59 (0x3032C0, |rider+0x1E0| x G+0x14 = the tick's
// distance in cm) answers in the core (web/mission_gameplay.inc, core mission_speed_builtin set by web/big-challenges.js): Kick Doubt's
// Grinder / Pepper Grinder / Meat Grinder count the metres ground on a rail. Off: nil, as before (they never complete).
// bcBanner (CTM agent): the Big Challenge fallback MISSION SUCCESS banner (web/big-challenges.js, no HUD sprites) skips the cash line at $0
// (a repeated challenge), as 1F09D0 does (blez +0x3C8).
// awardCascade (CTM agent, docs/ctm-parity.md "Awards, the PS2 way"): career awards as 159CD0 / 1591E8 / 1599A0 / 159818 grant them: every grant
// ends with the cascade (award 0 "Mountain conquered!" 1577E0, award 1, awards 2..4, every complete goal), an event's result in 1591E8's
// order (first gold, cash 159818, the goal, award 0), the passes under the award that opened them (158F60), and the reward list read
// from the reward record 0x4C3EF0 (cleared at world state 10 and at the list's close; collection-bonus cheats unrecorded).
// freshEvent (CTM agent, docs/ctm-parity.md "CTM parity audit"): every career entry into an event (its gate, the map, a resume) starts it
// fresh: 22D6C8 -> cGameModeMan_initGameMode (0x22D89C) -> the old handler's vt+0x1C 0x238C80 (GMM +0x84 = 1, +0x70 = +0x74 = 0): a
// qualifier / heat 1 with a new roster and new posted scores. Only Next heat and Restart carry the round. Off: the saved round resumes.
// careerReload (CTM agent, docs/ctm-parity.md "CTM parity audit"): a page reload during a career re-enters it as the PS2 does after a reset
// (the title -> Conquer the Mountain, 1A0B00): the world load at Happiness while the new-career flag is set, else at the last lodge, with
// the career running (web/career-ui.js resume / reenter, a sessionStorage mark). Off: the reload was a plain run without the career.
// transportLists (CTM agent, docs/ctm-parity.md "CTM parity audit"): the Transport Race / Freestyle rows in the order of the PS2's row tables
// 0x4781D0 / 0x4786E0 (Peak 2 / 3 freestyle: slope style, big air, pipe), and 0x207430's help: kT_HELPChalAvail on an open rival / peak
// event, "%S Jam" in a locked peak Jam's kT_HELPLockCompEvent.
// stationFlow (CTM agent, docs/ctm-parity.md "CTM parity audit"): world state 14 at the lodge door / booth holds the ride under the cut
// (113B10(C, 3)) and ends a running Big Challenge (30B7F8); the lodge prompt's Triangle is No (1F72E0) and No places nothing (WS4 enter);
// the booth map opens on the ridden peak and its Back is world state 15 at the station; Back on the post-event map does nothing (map mode 4,
// 0x2022A4); a station row asks "Transport to this area now?"; a transport into a visited backcountry across a world switch plays the heli
// drop [16, 17] (0x235220) and leaving a backcountry event rides the heli (0x2365CC); a freestyle next heat / restart has no gondola (WS13
// 27A860 for races only) and a rival one no cutscene.
// ctmSmallFixes (CTM agent, docs/ctm-parity.md "CTM parity audit"): the message view's Delete removes the first inbox entry of that item
// (0x1E5800 -> 1E3268), and the last lodge is written only when a station is reached (WS10 0x2356FC), not before a world load starts.
// crossWorld (CTM agent, docs/ctm-parity.md "CTM parity audit"): the PS2 mountain is one world, so riding down Intimidator (DRA4 -> DRA4_A)
// crosses into Green Base Station (A) and Gravitude (ERA5 -> ERA5_C) into Yellow Mid Station (C). The port's PEAK2 / PEAK3 worlds have no
// row for those stations (builtin 68's map id is unknown there: the ride ran into unloaded ground). Now that connector's load request
// (22D088) switches world to the station's peak and the ride goes on from the station's entry (a load screen, not streaming).
// mountainAudio (CTM agent, docs/ctm-parity.md "The whole mountain"): the MOUNTAIN world's audio follows the streaming rows like a peak
// world's (web/game-audio.js isPeakWorld): slots 8 / 9 take a location's banks when its data arrives (286CA8). Off: every location of
// world/MOUNTAIN.json counted as resident (44 banks decoded at the load, slots 8 / 9 holding the last location's).
// bankEvict (CTM agent): web/sfx.js drops a bank's decoded patches and AudioBuffers 30 s after no slot holds it (a bank loaded again
// within that keeps its cache). Off: both caches kept every bank the session loaded.
// unlistedPickup (CTM agent): a collect event with index 0xFFFFFFFF (a pickup whose resource is not in the stage list: 30B9A0 pays it
// through 10F338 -> 119EF8 -> 150A90 without a bit) adds its cash to the career (web/stage-collect.js). Inert until the core queues such
// events (web/stage_script_gameplay.inc stage_collectible_award, next core build).
// crossingArrival (CTM agent, docs/ctm-parity.md "The whole mountain"): a riding crossing (a connector's Unload changes 0x535C08: world
// state 11) makes the new location the last lodge / visited only at the connector's Load trigger (22D088: world state 10, then 4), as the
// PS2 free-ride capture ctm-parity/mountain/fr-dra4a shows. Off: both at the Unload's course change.
// newGameReset (CTM agent, docs/ctm-parity.md "New game"): Options > Save/Load > New game resets what the PS2's does (0x18D4F4: profile 0's
// ten career blocks, the player name to PLAYER 1, the rider to Zoe, relationships, outfits, cheat characters) and keeps the records (the
// options file) and the options. Off: a fresh career save with the records reset, everything else kept.
// boothDj (CTM agent, docs/ctm-parity.md "Stations and heats"): the transport booth sets the audio's booth flag for 30 s (2A49E8); while it
// holds and the DJ is speaking, a transport's travel (28E8C0 20) keeps the DJ line (no stop before pktrans, no radio big intro).
// ws13Rival (CTM agent, docs/ctm-parity.md "Stations and heats"): a peak run's results Restart takes WS13's rival branch like a rival
// challenge (event type 5 / 6: no gondola, no gate lists, the card). Off: it played the freestyle gate lists.
// doorNoPlace (CTM agent, docs/ctm-parity.md "Stations and heats"): No at the lodge prompt places the rider at the station's session point 0
// (11DE60(rider, 0, 2) + 11DF18, PS2 ctm-parity/door), as the port did before stationFlow. Off (stationFlow alone): the rider stayed at the door.
// nisTick (CTM agent, docs/ctm-parity.md "The NIS rider hold"): under the lodge door / booth cut (world state 14) the world runs on and the
// rider is held at the cut's anchor (123640: control 13 / motion 3; PS2 door-no ticks 394..634, fr-booth2 240 ticks), paused only at the
// prompt / map. Needs the core's nis_hold (web/core.cpp); a core without it, or off: the ride pauses under the cut (stationFlow).
// gameTickKeep (CTM agent, docs/ctm-parity.md "The game tick at placements"): in a streamed world the core's game tick (1298C8: the scope
// refresh phase, the ground stamps, the boost ring parity) is 0 at a run's start (the world load / WS2; PS2 peak1-green-start record 0 =
// tick 0) and runs on through the in-world placements (11D390 leaves it: door No 636, fr-dra4a-full's crossing). Off: every placement
// reset it to the world seed's landing tick (reset_rider).
// peakAttached (CTM agent, docs/visual-parity.md 41.6): the streamed worlds' ParentModifier children (<peak>/SETPIECES/attached.json,
// tools/export_peak_world.py attached_package; the children's batches split on node 0) drawn with their LiveComp parent's node matrix
// (0x357108, web/attached-setpieces.js): BRA2 / BHP1 / ERA5 searchlight glows on their turning bases. Off: they stayed put.
// hangWatch (CTM agent, web/diagnostics.js): a tiny worker gets the screen / course / last marks every 250 ms and, when the pings stop for
// over 8 s while the page is visible, posts a 'hang' diag event itself (again every 30 s, 'hang-end' when they resume). A dead WebContent
// kills the worker too, so a hang reports and a crash does not. Silent; field telemetry only (diag on).
// relAging (CTM agent, docs/ctm-parity.md "Relationships"): rider relationships age (0x155E58) at an event's load / restart only outside Conquer
// the Mountain (WS1, 0x234894) and at every race's end (0x233F08, any game type), not at every start. web/ai-race.js start / results.
// rewardRng (CTM agent, docs/ctm-parity.md "Awards, the PS2 way"): the reward / gear picks (0x157080, 0x156C70, 0x156EE0) draw from the
// presentation generator 0x4FF018 (0x3177F0 -> 0x317A08; web/lineup.js presentationDraw, as the career messages do). Off: a saved xorshift.
// mountainRide (CTM agent, docs/ctm-parity.md "The whole mountain"): the Conquer the Mountain free ride runs in the whole-mountain world
// (MOUNTAIN), so the bottom of Intimidator streams into Green Base Station and Gravitude into Yellow Mid Station through the connectors'
// own Load / Unload, as on the PS2 (one world). Desktop only: phones (iOS / Android, or quality=low) keep the per-peak worlds + crossWorld
// (the core keeps every location's collision it was fed: the whole mountain is ~2x a peak's wasm). ?mountain=0|1 overrides the tier.
// Also in MOUNTAIN free ride with streamAhead: no window prefetch of the rows uphill (planAhead already reads the rows the connectors lead to).
// peakRelease (AI-parity agent, docs/ctm-parity.md "The PS2's location release"): in a streamed free ride (web/free-ride.js, one rider
// context) a location whose draw package is released (out of the world for RELEASE_MS) also frees its collision in the core
// (core peak_world_free_track: terrain patches, instance nodes, grind rails; refused while collidable or in the section octree) and is
// fed again at its next want (web/peak-world.js releaseCore), as the PS2 frees a location's Section Allocator blocks at T+8 and reads
// it again. Keeps a whole-mountain session's core memory bounded. ON since 2026-09-29 (page runs: docs/mobile.md "Whole-mountain memory").
// Since 2026-09-29 (whole-mountain memory agent, docs/mobile.md "Whole-mountain memory"): the PS2's rule, in free ride and the peak runs
// alike (both one rider context): a location no row wants and nothing reads ahead is released at once (draw package, environment
// slice, collision; the 45 s hold stays only for a station's other ways on), unfed read-ahead slices nothing wants are dropped, a peak
// run reads ahead only its route's next row (no 2-row window, no off-route rows), and a single peak no longer feeds the rest of the peak.
// xboxRiders (web/quality.js riderTextures, web/texture-archive.js, web/fe-options.js 'Texture set'; docs/xbox-textures.md section 8):
// the Xbox HD rider texture set (WARDROBE/<ID>/textures-xbox.tex / gear-xbox.tex: the Xbox's own DXT blocks, 2x the PS2 texels)
// is the default on desktops (PS2 on iOS / Android / the low tier) and Options > Display & Touch gets its 'Texture set' row.
// Off: PS2 textures everywhere unless ?riders=xbox. Only the texels change (UVs, materials, alpha test, blending as before).
// sharedSamples (web/wardrobe.js riderSamplesUrl, main.js asset / prefetchRiderPackage): the original riders' gameplay clip table is one
// file, /assets/ANIMATIONS/animation-samples.json, instead of 30 identical per-package copies (1.4 MB each; Sam's packages keep their own).
// Off: each package's own animation-samples.json (needs those files).
// zoeBoot (web/main.js bootRider, web/ui.js; docs/iso-pipeline.md "Sam"): the model every course load starts with, before the
// chosen rider replaces it (hidden by riderPoseGate), and the Select Character start index, is Zoe (the PS2's first roster entry,
// always on the disc) instead of Sam (the port's own rider, whose files a public build does not have). Off: Sam, as before.
// trophyLui (web/trophy-room.js, web/lodge-ui.js; docs/visual-parity.md 36): the lodge's Rider Details > Trophies is the PS2's three FE.LUI
// screens (125mountainroom: Peak 1..3 / Peak Pass over the mountain with a marker per event and the pass popup; 126peakroom: the peak's
// goals, the focused goal's events with medal icons and checks, the goal trophy thumbnails; 127trophyroom: a complete goal's trophy and
// events with the trophy / medal picture and the best time, score or earnings). Off: the page's list with trophy boxes.
// riderDrawState (web/rider-material.js riderDrawState; main.js asset, opponent-riders.js, fe-preview.js; docs/xbox-textures.md section 9):
// the PS2 rider draw state per material (37A610 -> 363C20 -> 3626D8): 'alph' / 'ea*' materials blended (GS ALPHA 0x44) with Z written
// only above alpha 92 of 128 (ATST GREATER 92, AFAIL FB_ONLY), every other material opaque with its alpha unused (ATST ALWAYS), instead
// of one alphaTest 0.35 for every batch.
// compileAbort (web/compile-abort.js; main.js unloadCourse): at a course unload, the pipeline warms still being built (three's compileAsync
// builds its items one at a time between yields, against the render context of the call) stop after their current item, before the
// course's passes and objects are disposed. Off: they carried on after the dispose and uploaded the disposed geometry and textures
// again (nothing disposes them again), and their pipelines failed on the destroyed depth-stencil texture (WebKit, a world switch).
// sortedClass (web/main.js asset; docs/visual-parity.md 44): the render queue key 364240 puts a state's sort mode (word1 bits 0..1) under
// its priority: 0 (opaque) 1023, 1 (static-model class 1, ALPHA 0x44 AREF 92) 1022, 2 (classes 2 / 3, the depth-sorted translucents) its
// depth key, so class-1 models draw before every sorted one. three sorted them together by distance: a large glass pane (class 2)
// drawn first wrote depth and hid the stadium crowd (class 1) behind it at Metro. The class 2 / 3 world batches draw after class 1.
// frame8 (web/frame-space.js frameBufferType, main.js renderer outputBufferType; docs/visual-parity.md 43): with the encoded frame, the
// world / sky pass targets are 8-bit unorm (4 bytes a pixel instead of half-float's 8), each draw rounded to bytes as the GS frame buffer
// (PSMCT32) does. Off: half-float targets.
// cutsceneBytes (web/cutscenes.js; docs/visual-parity.md 43): the cutscene sets, skies and the PDA prop combine their texels and vertex
// colours in GS bytes, as the static-model draw does (MODULATE Cs = T x (c5 << 3) >> 7; the PDA's PS2 texels x 2), not on linear light.
// envMap (web/world-material.js envPassMaterial; world-batches.py batch.env from web/prepare.py / tools/export_peak_world.py; docs/visual-parity.md
// 43): the static-model second texture. A material whose word +12 has 0x200000 / 0x600000 (37F2A4..37FD2C) draws its triangles again in GS
// context 2 with the material's second texture (record halfword 1): VU1 program 3 UV mode 256 (0xCE8) from the camera-space normal through
// the matrix at 0x504760, u = 0.5 n.x + 0.5, v = -0.5 n.y + 0.5; MODULATE by the vertex colour; ALPHA_2 enum 2 (Cs + Cd) or 17 (Cs x Ad + Cd,
// Ad = the first pass's alpha). Glass, windows and polished panels (Metro's stadium glass, CRA3 / ARA1 station windows). Off: base pass only.
// encodedBlend (web/frame-space.js, chosen once per page before any material or Color is built; docs/visual-parity.md 38): the frame
// holds encoded 0..255 values, as the GS frame buffer does, so every blend in the world pass, the sky pass and the front end is the GS's
// (Cs - Cd) x As + Cd on encoded bytes instead of on linear light. Byte-domain materials write their bytes / 255 (frame-space toFrame),
// the fog / encoded composites read the frame without an OETF, three's Color values stay encoded (ColorManagement off), the canvas gets
// no output conversion. Opaque pixels are unchanged; world additives stay in the world pass (byteBlend is moot). Off: linear light.
// cables (web/set-piece-cables.js; docs/visual-parity.md 41): the chairlift / gondola cables and the Metro-City mill lines, the black
// 1-pixel line strip every MultiSplineModifier with builtin-20 key 10 draws along its path (0x35B418 -> 0x345430 -> 0x381F10 -> VU1
// 0x3BF0: 10 points a segment, PRIM LINESTRIP, guard-band ADC). Off: no cable (the chairs hang in the air).
// On (2026-09-28): PS2 frames of the Snow Jam race opening, Much 2 Much and the Metro-City mill, Chrome and WebKit (WebGL2 too).
// fogPuffs (web/fog-puffs.js, tools/export_fog_puffs.py; docs/visual-parity.md 41): the fog-particle puffs of the SSB kind-5 instances
// (cPS2FogParticleMan 0x4882E0: 0x22A270 cull, 0x2DC190 sprites with the 18000..25000 instance and 500..2500 puff fades, 0x2DBF98 sorted
// far to near, fog0 texture, GS 0x44, depth tested without Z write, priority 7 in the encoded composite). Off: no puffs (the forest haze
// of Snow Jam, Metro-City, Peak 2 and Peak 3 is missing). Needs <package>/fog-puffs.json (none: nothing drawn).
// terrainGlint (web/world-material.js glintBytes, tools/export_terrain_glint.py; docs/visual-parity.md 41.6): the patch "reflection" pass
// 38D168 over the patches with word +0x0C & 0x600000 (layer type 6): a camera-position environment map of the patch normals (E at terrain
// +0x360, 38B370) in the grey / blue glint texture, added as Cd + Cs x Ad >> 7 with the base texel's alpha. Off: no glint. Needs
// <package>/terrain-glint.json (none: the plain terrain graph).
// avalanche (web/avalanche-state.js, web/set-pieces-renderer.js, web/audio-world.js; docs/avalanche.md "Draw" / "Audio"): the recorded
// avalanches the core plays back (builtin 94): the AvaSpline pieces move with their tumblers (core moving_instances(), batches split by
// prepare.py), a piece hidden at the start shows while its tumbler drives it, a released one (entity vt+0x08(3)) goes; the rumble loop
// 0x29DEF0 (bank slot 8 sound 2, bus 5, volume 0x29E438 from the human's nearest tumbler, 2 s fade out). Off: the pieces stay put.
// heliWorld (web/set-pieces-renderer.js; docs/set-pieces.md "Moving lit instances"): the backcountry heli os609 in the air (ABC1 / DBC2 /
// EBC3), a LiveComp with flags & 4, drawn at its world place while its player runs (the hover 551..677 over the start), hidden with its
// copy while an arrival set stands in. The countdown audit had it 'none' from the renderer list's 0x100 / 0x200 parity.
// avalancheTrails (web/avalanche-trails.js; docs/avalanche.md "Trails"): the avalanche emitters' dust trails (0x2D9130 -> 0x2D8EA8 ->
// 0x371688 colour emitters, particle entry 0xA00, fog0, GS 0x44, priority 7) from the core's avalanche_trails() rings.
// beamEncoded (web/rival-beam.js; docs/visual-parity.md 41.9): the rival locator beam (0x2E3AF8, priority 7) drawn in the encoded pass
// after the fog composite in GS bytes (MODULATE, ALPHA 0x48), not in the world pass where the fog composite fogged it.
// sparkleWorld (web/terrain-sparkle.js; docs/visual-parity.md 41.9): the terrain sparkle (0x38D968, priority 4) drawn in the world pass,
// before the fog composite, instead of the encoded pass after it.
// effectOrder (web/ps2-draw-order.js; docs/visual-parity.md 41.9): every post-fog effect (priorities 6..8: fog puffs, set-piece
// particles, wake, boost strips, aura, streamers, impact sprites, rival beam and icon, snow, snowfall, halos, camera splash) takes its
// renderOrder from the PS2 render-list key 0x364240 (priority, word1 sort mode, word0 bits 6..9 rank, texture handles; stable
// ascending radix sort 0x364050), with the FX texture handles of PS2 RAM (event boot or Conquer the Mountain). Off: the fixed orders
// 650..716 (fog puffs after the wake and boost, halos before the snow, snowfall after the set-piece particles).
// snowBuckets (web/snow-renderer.js; docs/visual-parity.md 41.8): the rider snow emitters drawn in the PS2's render-list order: the flush
// radix-sorts its merged buckets by ~(priority, modes, texture handle) (0x362DE8 / 0x364240 / 0x364050), i.e. by descending handle =
// ascending FX texture id: the snow cloud (5) and impacts (6) before the chunky sprays (13..21). Off: emitter index order (the dark
// chunks drawn under the cloud, missing in the forest).
// lodgeSave (web/save-game.js, web/lodge-ui.js, web/career-ui.js; docs/visual-parity.md 37): the lodge's Save Game is the PS2's Save
// game screen (cFEStateProfileLoad mode 3 on FE.LUI 93profile_load: the card's row, the name keyboard, the memory-card popups Checking /
// overwrite? / Saving / Save complete, Continue back to the lodge). Off: the career is written at once and 'Save complete.' shows.
// fePopup (web/fe-popup.js, web/fe-screens.js drawLodgePrompt; docs/visual-parity.md 39): the lodge's Yes / No questions are cFEPopup
// boxes laid out as its code does (0x1C6B08: the message wrapped and measured with the font box of 0x3921F0, the box scaled (w + 20) / 504 x
// 1.05 by (h + 15) / 204 at 92 + h / 2 - 7.5, the menu centred on the widest option) and opened as cFEPopupConfirm (the box grows over 25
// frames, the texts from the 'Start' label, the veil's own fade). Off: the fitted layout (LODGE_POPUP).
// litLiveComp (web/world-material.js litWorldMaterial, web/set-pieces-renderer.js; docs/visual-parity.md 40): a lit LiveComp instance
// (authored flag 0x40000000 = runtime 0x4000: Gravitude's crash billboards, Metro's falling tram) is lit per vertex from the location's
// object bank on its node-rotated normals (37E238 -> 2F5400, VU1 program 3 0x8B8), not drawn with its baked colours. Off: baked colours
// (the billboard's poster stays bright as it tips over, where the PS2 draws it near black). On (2026-09-28): PS2 grav-bb frames, Chrome and WebKit.
// lodgeFlash (web/lui-flash.js, web/career-ui.js lodgeGo; docs/visual-parity.md 42): every lodge screen change is the PS2's FE state change:
// TransitionOut's white flash (transition_flash: A up over 10 frames over the old screen, the switch at full white, down over 9 over
// the new screen's intro), no input meanwhile. The lodge and its Rider Details, Trophies (125 / 126 / 127), Save Game, Career
// Highlights, Rewards / Ubertrick Setup / Rider Profile opened from it, Music (the fall on entry). Off: those screens cut at once.
// On (2026-09-28): PS2 tout-troA / tout-detD / tout-saveC frames, Chrome and WebKit.
// stateCursor (web/career-ui.js memoCursor / restoreCursor; docs/visual-parity.md 42): the lodge, Rider Details, Buy Attributes and
// Trophies' mountain room reopen on the item last used, as each FE state keeps its menu cursor (0x1865A8 -> 1A0708 on exit, 0x186518
// -> 1A06F0 on activation). Off: they open on the item the caller sets (mostly the first).
// introLead (web/lui-flash.js introStart): a screen a lodge state change opens shows its intro from frame 2 (0x39ED4C / 0x39EED4: two
// LUI updates in the new state's first pass), and a restoring state's focus 2 frames after its intro label (0x42, frame 25). Off: the
// intro from frame 0. Both on (2026-09-28): PS2 tout-detD / tout-kbd frames, Chrome and WebKit.
// litInstances (web/world-batches.py lighting, web/world-material.js litWorldMaterial; docs/visual-parity.md 40): every lit static-model
// instance (runtime flag 0x4000: crashbags, trains, trams, cars, blimps, planes, avalanche and rockslide pieces, the billboards) draws its
// texture lit per vertex from its light-cache rows (2F5400: the object bank at its position plus up to 4 local lights; 199 instances
// bit-exact against the PS2 caches, tools/export_lit_instances.py), one shared program. Needs the re-split world packages. Off: baked colours.
// equipLoading (web/wardrobe.js EquipGearScreen, preloadEquipGear; web/lodge-ui.js preloadGear; docs/visual-parity.md 42): the lodge's
// Equip Gear switches at the flash's full white like every lodge state, and the rider loads behind "Loading..." (PS2 tout eqg-k*: full
// white at +12, Equip Gear under the fall with "Loading...", the list and no rider from +14, the rider and the help line at +36; 0x199938:
// the help waits for the rider's gear data 19E238 -> +0xA60, "Loading..." for the preview's +0xCC8). Before: full white held until the
// outfit package was built.
// titleStart (web/game-audio.js init, web/ui.js leaveTitle, web/audio-menu.js watchSounds; docs/audio-menus.md "Title Press START"): the
// title's Start plays what the PS2 plays, on the press that also unlocks the browser's audio. PS2 (local/ps2-capture/menus/title-start,
// call log on 294F78 / 2906B8): one sample after the press, cFEStateTitle's notify 0x1946A8 plays FE event 15 (0x1946E4: snd 7, the
// whoosh) and the menu's UINext accept FE event 0 (0x1A2F28: snd 3), both SSX3Menu; the snowflake burst shows from 5 after the press.
// The port played only event 0, and nothing on the first press: SSX3Menu was fetched on the audio unlock, i.e. by that same press.
// Now the bank loads (and decodes) before the title, and ui.js leaveTitle plays both events.
// startConsume (web/gamepad-menus.js taken, web/main.js pad / keyboard pause paths; docs/visual-parity.md section 31): a press a menu used
// is spent. The PS2's pause (0x230A34) needs a new Start press (action 0x3C, input.map edge) with no overlay up and no transition
// running (0x20CBE8 / 0x20CBA0), so the Start that accepts a card or prompt can never also pause: when the overlay has gone the
// button is held, not newly pressed. The port read the pad in two loops (the menus' and the game frame's); when the game frame saw
// the press only after the menu had switched to the ride, the one press did both (Owen, Safari + Xbox pad: BRA2's heat card ->
// game -> ctm-pause at one instant). Now the menus' press stays spent until the button is released, and the menus' own synthetic
// keys never pause through the keyboard path.
// finishSkip (web/game-tick.js): a new Cross from the finish + 228 closes the finish panel at once (the PS2's 1E8160 event 5); the
// port always waited 408 / 288 ticks.
// ws13Rebuild (web/career-ui.js ws13Riders; main.js ui.cb.heatLineup; docs/ctm-decomp-world-states.md): a race's Next heat / Final
// Round and the results' Restart rebuild the round's computer riders before the gondola, as world state 13 enter (0x235AA0) does; a
// rival challenge's Restart shows its card over the ready state (it was over black).
// ps2MenuInput (web/gamepad-menus.js; docs/ctm-decomp-screens.md): menu directions repeat at the PS2's 24 / 12 frames (0x321298;
// the port's pad repeated every 110 ms, 1.8x too fast), held arrow keys repeat the same way (the port ignored them), and the stick
// counts as held from about 0.38 deflection (raw < 79 / > 176), not 0.5.
// threeLean (web/three-patches.js, set as globalThis.__ssxThreeLean by main.js before the renderer exists): three r186's per-draw
// dynamic cache key, bind group key, updateTexture options and sampler key without per-call garbage (the same results).
// sceneLightingOff (main.js init; docs/web-render-performance.md "Per-frame garbage"): renderer.lighting.enabled = false. The game
// draws with MeshBasicNodeMaterials and adds no three.js light, light / AO map or environment (the PS2 lighting is the game's own
// nodes), so the shaders are the same; with lighting on, three rebuilt every draw's dynamic cache key (Nodes.getCacheKey is cached per
// draw call, renderer.info.calls) through LightsNode.getCacheKey(true): a Set, arrays and a record per node property, per draw.
// ?originalWorld=0 (the fallback materials take the batch lightmap through three's lightMap) keeps three's lighting.
// worldWarm (CTM stalls agent, docs/course-switch.md "World arrivals warm under the load screen"): a Conquer the Mountain world load
// (menu -> career, a Transport across a world switch) waits under the load screen for the start row's pipeline compile (free-ride.js
// rewarm, before: fired and not awaited, so the ride's first frame built 3000 buffers / 7000 bind groups / 20 pipelines, 3.3 s in
// Owen's Safari), then warms what the world pass draws besides the locations (sky, set pieces, rider shadows, post passes) with the
// race warm-up's frames behind the opaque load screen (compile-only under the Transport's visible held loop). Also: web/yield-shim.js
// (three's compile yields without a whole frame per item where scheduler.yield is missing: WebKit), the start row compiled first, a
// DoubleSide pass for two-pass materials (their 'backSide' render objects), cutscene sets and actors compiled per side. Event loads unchanged.
// bcDecline (web/big-challenges.js -> core mission_lifecycle; docs/ctm-parity.md "Big Challenge lifecycle"): No / Triangle at the offer,
// No at the fail prompt or at "Restart Challenge?", and Quit Challenge reset the rider (30B658 / 30B758 -> 1235F8: control 9, the
// white fade, the route placement, 41 ticks) so the offer does not come straight back; an event gate (builtin 67, 0x3021B8 -> 30B7F8)
// ends a running challenge; world state 10's enter (0x2356A8 -> 309030 / 308988) clears the active challenge, its HUD and the offers.
// switchGate (CTM stalls agent, docs/course-switch.md "The course being built runs nothing"): during an in-page course switch the page's
// global core is the new course's instance from the start of loadCourse, while its init runs in slices across frames. The Transport's held
// loop keeps drawing over the switch (cutscenes.js acrossSwitch) and, every frame, lit its actors through host.core (fe-preview light:
// _malloc, _reset_rider_lighting, _shade_rider_lighting) and ran the fade's painter reset (_weather_fade_reset) on that half-built core,
// and its draw of the whole scene ran the new course's render hooks (set pieces, fog puffs) before they were set up. WebKit runs showed
// the result: "Out of bounds call_indirect" inside the new core's _init_environment / _reset_rider (the load fell back to the title), and a
// TDZ error in set-pieces-renderer's fog-puff residency. Now host.core is null while a core is being built (as it already was during the
// unload), and the held loop's own draw leaves out what the switch added (the list's set, actors, skies and alpha fill still draw).
// On (2026-09-29): 0 calls into a new core during its loadCourse in WebKit (3 Transports) and Chrome (6, with and without mountainRide);
// off: 97-1498 calls per Transport, and 3 of ~12 WebKit Transport loads crashed in the new core.
// CTM audio / DJ / mail agent (docs/ctm-decomp-freeride.md ranked 2, 9, 10, 11, 12; docs/audio-logic.md 9.14):
// worldSwitchAudio (web/game-audio.js travelSwitch / carry, career-ui.js goWorld / crossWorld / transportAfterEvent): a Transport that
// changes the page's world (the post-event return, another peak on the per-peak worlds) runs code 20 in the world at the confirm (28F520
// Stop, 28EBAC..28EEB4: pick, the destination song 10 ms later, Radio BIG intro (0,1) + hub chatter pool 5; PS2 ctm-decomp/audio/postevent2
// ticks 15440-15442), and the switch carries the audio (the song, timers, pending DJ, the NIS voices; no LoadingScreen loop, no 2867E8 /
// 2A4A78 at the next world load; WS10 enter 2B3A98 resumes a paused pktrans); crossWorld carries it without code 20. The podium song plays
// on under the results' map until the confirm. Before: the load-screen loop, a new world-load song and DJ kind 2 / 4.
// postEventDj (web/game-audio.js finish / hubChatter): the post-event record 2A45C0 / 2A4660 at every CTM finish (place, course, KOs +0x128
// in races, Ubers +0x114; reset at round 1, armed at the final or a one-round event) and the commentary 2A4770 at the next hub chatter
// (2A2E50): Char_Progress 0x2102 (1st), Aggression 0x212E (>= 5 KOs), High_Trick_Score 0x212F (>= 27 Ubers, 24 in Big Air / backcountry),
// a random one avoiding the last; nothing earned during a travel: hub chatter / Terrain_Info / Event_Intro by the destination.
// djVisited (main.js gameAudio.context, web/game-audio.js visited / djTimer): the first-visit flags 579C come from the rider's saved visited
// mask P+0xACC (145D38, careerUI.visitedMask) instead of a per-page mask, and Free_Ride_Intro (and its 579C clear) needs Peak 2 locked
// (146008 = P+0x278 bit 12, 28E730).
// djQueueRules (web/audio-speech.js radioBigIntro, web/game-audio.js pause): Radio BIG intro 2A26F0 stops the current line first (2B11B0 at
// 0x2A272C); a pause resume (289BB8) stops speech and clears the pending DJ only after an in-game song change (+0x5828, 28FAE0), so an
// MCOMM visit no longer drops queued DJ lines.
// mailFreeze (web/career-messages.js): the event's messages post on the finish tick on the PS2 (125108 -> 238358 -> 154EE8) and the icon
// runs 182 frames under the finish HUD before WS5's overlays freeze it (race-f fin 0.050 -> res 3.033 -> f95-after 3.050), so the port's
// results-time post starts the icon at 182 frames: the next ride shows its last ~2 s, not 5 s; opening the Message Center freezes the
// icon (HUD events 3 / 4) instead of clearing it.
// faqDefer (web/main.js 'faq' / faqOpen, web/game-tick.js): the Green Base Station "?" (stage builtin 100 -> 1E3510) touched while a
// pause, prompt or menu is up opens the Message Center's FAQ once the ride runs again (world state 4 0x2309A4 polls gp-0x1024 each
// update after the offer read); the port dropped it. The FAQ is marked shown at the contact (1475C0), as on the PS2.
// transportFade (web/free-ride.js transport, web/cutscenes.js 'transport-ride' / releaseFade / skipLock, main.js transportInWorld, ui.js
// hudSqueeze, core peak_world_transport phases 2 / 3; docs/ctm-parity.md "The Transport's presentation"): the in-world Transport as the
// PS2 plays it: one NIS list (27A860: departure, in-air, held loop, the appended heli drop), the destination requested at the loop's start
// (0x2366C4), the release fading the still-playing loop to black over 30 ticks (27A9F0 -> 2766D0 -> 277980) with the placement at R+29,
// the world fading in under the HUD while the bars slide out over 30 ticks, the dome switch after the list (0x235808), no skip for the
// first 30 ticks (0x236550). Before: three lists with a hard cut at the release.
// departCalls (web/cutscenes.js hubCall, main.js stageTrack): a Transport step's kind-7 channel-0 stage calls without a staged set run on
// the script's hub location (0x2808E8: the scdat container's track globals), e.g. gond_dep #126 / heli_dep #147 at a station (the depart
// LiveComp, the sound loops, the heli snow spray, SetNodeState), with the recorded cleanup at the step end. Before, they were dropped.
// eventReturnInWorld (docs/ctm-events-in-world.md stage 5; needs eventInWorld): back from an in-world event (results Transport -> the
// map) with no world load: main.js cb.freeRide ends the event in place (cb.eventInWorldEnd) and transports inside the world (the same
// location: WS15's Session point 1 and white fade). QA.
// nisSectionPoint (docs/ctm-events-in-world.md stage 4): every NIS the streamed world plays (arrivals, station cuts, Transport rides, the
// events' fly-over / approach) adds its director's camera point to the section activation, as the PS2 does (0x281370 -> 0x1033B0,
// 0x281100 the outer camera's +0x20; cutscenes.js sectionFeed -> core section_point). pv eventInWorldAi turns it on for the events alone.
// eventInWorldAi (docs/ctm-events-in-world.md stage 4; needs eventInWorld): the in-world event's computer riders run in rider contexts fed
// from the streamed world's resident locations (per-context collision, path banks, section node states), made at the gate while the
// fly-over plays, reused (reset, not destroyed) for the next event. QA, not for players yet.
// eventInWorld (docs/ctm-events-in-world.md stage 3; needs eventWorldData, and worldUnderCuts for the PS2's hold): the CTM gate's event runs
// inside the streamed world with no course switch: main.js cb.eventInWorld (core event_course_seed, init_race with the event's document,
// the event type / game mode, the grid spawn), WS1's approach and idle in the world (cb.introInWorld), the card, then the countdown and the
// run through startRun's event start. The human only (no computer riders until eventInWorldAi); QA, not for players yet.
// eventWorldData (docs/ctm-events-in-world.md stage 2; web/ctm-event-plan.js): at the event gate the event package's own small data (the
// computer riders, the race event document, the grid spawn, the progress meter, the slope-style list, the camera triggers, the GO LiveComp
// starts) is read while the fly-over plays, into careerUI.eventPlan. Inert: stage 3 (eventInWorld) uses it.
// worldUnderCuts (docs/ctm-events-in-world.md stage 1, section 6.2 / 6.8): the world ticks under the CTM cuts as on the PS2, the human held by
// the cut's rider actor (core nis_hold) instead of a HOLD context that stopped the simulation:
// - WS1 (the event gate's fly-over in the streamed world): no 'WS1 ride-in' HOLD; the rider rides the 30-tick fade in its own control, then
//   is held from the fly-over's first tick (PS2 caps/c0a-gate: gate 3440, control 13 / +0xAC4 1 from 3471).
// - WS10 arrivals (the new-career plane, the backcountry first-visit lists, the heli drops): the ride starts before the list with the rider
//   held and is placed when the list ends; only the movie steps (FMV, lists 29..31) push a HOLD (PS2 new-career: 917 ticks under #153 /
//   #163 with +0xAC4 = 1, none during the ABC1 movie).
// Off: the world stops for the whole cut (the HOLD contexts of docs/pause-contexts.md).
// peakSplines (core set_piece_streamed from web/free-ride.js / web/peak-capture.mjs PEAK_SPLINES; docs/set-pieces.md "Streamed worlds"):
// the event locations' Spline set pieces (the dragon / osprey / eagle / cessna / rocket flybys ...) launch in the streamed worlds at
// their contact / timer programs (builtin 19, one 0x317830 draw each), and a location's pieces go at its unload (0x3551A8).
// speechRange (memory, docs/audio-logic.md 9.15): a speech line loads as its own byte range of the bank's .dat (a Range request, like
// musicStream) into a small LRU, fetched when the line is resolved or predicted, instead of the whole .dat kept for the session
// (DJ_Hub_Char_Stories_eng.dat alone is 35 MB).
// gcWatchdog (memory, WebKit only; docs/mobile.md "Hangs"): web/gc-watchdog.js. When JavaScriptCore has stopped running full collections
// (a load-time full GC left its timer unarmed: the WebKit memory runaway), a few one-page WebAssembly.Memory objects make it run one.
// loadCopies (memory, docs/mobile.md "Load spikes"): web/downloads.js reads a body of known size straight into one buffer (no chunk list
// and join) and hands each caller its bytes without the Response body copy (arrayBuffer: one copy of the shared bytes; json / text:
// decoded from them). Before: 3-4 copies of every asset body while a load read it.
// switchGC (memory, WebKit only; docs/mobile.md "Load spikes"): web/switch-gc.js. Before a course's new core is made, while an earlier
// course's core is still alive (JavaScriptCore frees a wasm memory only in a full collection), the gc-watchdog kick asks for one.
// padCarry (CTM agent, docs/ctm-events-in-world.md "Carried presses"): one pad history for the menus and the ride, as the PS2's
// cSSXApp_preUpdate (0x227F20) runs 0x321298 on every app update whatever runs: while the game does not tick (pause menu, cards,
// prompts, results) each 60 Hz frame's pad goes into the core's history (core pad_history_sample), and the menus' open / close and
// a run's start no longer clear the held keys or settle the history. So a Cross pressed on a menu and still held is held on the ride
// with no new press edge: the Give Up's Yes crouches without a prewind (PS2 c0a-ret2, gate ctm-events/c0a-ret2-coast). Needs a core with
// pad_history_sample. Off: the history restarts settled at each run's start and the held keys are cleared at every menu.
export const PV_DEFAULTS = Object.freeze({ speechRange: false, heliWorld: true, avalancheTrails: true, beamEncoded: true, sparkleWorld: true, effectOrder: true, peakRelease: true, fsCelebrate: true, sectionClock: true, rivalRelations: true, bigChallengeAudio: true, help: true, cheat: true, skyClear: true, boostLight: true, aiFx: true, worldWrap: true, plane: true, heli: true, flyover: false, rivalIcon: true, planeFx: true, heliSky: true, regionTick: true, planeCam: true, sparkle: true, nisProjection: true, bcHeli: true, heliLight: true, heliHover: true, acrossLoop: true, raceHud: true, hudText: true, luiResults: true, replay: true, finishBanner: true, recoverMeter: true, hudStandings: true, luiLights: true, cashGap: true, streamers: true, resultsMenu: true, attract: true, bootMovies: true, transportMap: true, fsStandings: true, mcommIcons: true, byteBlend: false, careerRider: true, lodgeDetails: true, playerName: true, riderMusic: true, lazyCourse: true, uberLayout: true, lodgeCheats: true, additiveNoZ: true, lodgeRewards: true, stationFences: true, loadFlags: true, freshRider: true, finishFences: true, ctmWorldAudio: true, streamGate: true, streamAhead: true, streamWarm: true, sliceLoad: true, eventSlices: true, feCompileSpread: true, bootChain: true, lodgeWorldLoad: true, riderPrefetch: true, sharedParse: true, boothTeleport: true, rivalCard: true, sprayReset: true, rivalCardAi: true, luiWrap: true, dropCamera: true, staticWorld: true, sharedWorldMaterials: true, staticRefresh: true, gpuRestore: true, gpuRelease: true, refreshCap: false, setPieceSkip: true, skipEmpty: true, staticRefreshWide: true, warmSpread: true, softSprites: false, softGlyphs: true, sharedIndex: true, glslKeys: true, warmPost: true, feMorphTiers: true, refKeys: true, liveRest: true, oneMatrixPass: true, dropStreamer: true, musicStream: true, dropPose: true, liveCompObject: true, readyLight: true, rideWarm: true, switchIcon: true, painterWorldLoad: true, bindPoseProbe: true, shadowAtlasInit: true, hudHints: true, singlePause: true, menuRiders: true, audioDeclick: true, audioInterrupt: true, musicWorkerDecode: true, sfxStartAfterDecode: true, musicLookahead: true, sfxWarmFirst: false, musicPrefetchNext: true, heatSong: true, ctmRestartAudio: true, arrivalFade: true, genericFogOff: true, pdaOptions: true, pauseRestart: true, loopFadeOnce: true, lodgeLui: true, stallCap: true, eventAnchorRng: true, stallKeys: true, buyAttribs: true, startRules: true, sessionMap: true, finishLui: true, stationArrival: true, sessionFade: true, riderPoseGate: true , bcSpeed: true, bcBanner: true, awardCascade: true, freshEvent: true, careerReload: true, transportLists: true, stationFlow: true, ctmSmallFixes: true, crossWorld: true, mountainAudio: true, bankEvict: true, mountainRide: true, unlistedPickup: true, rewardRng: true, crossingArrival: true, newGameReset: true, boothDj: true, ws13Rival: true, doorNoPlace: true, relAging: true, nisTick: true, gameTickKeep: true, peakAttached: true, hangWatch: true, xboxRiders: true, zoeBoot: true, sharedSamples: true, trophyLui: true, riderDrawState: true, encodedBlend: true, envMap: true, cutsceneBytes: true, frame8: true, sortedClass: true, compileAbort: true, cables: true, fogPuffs: true, terrainGlint: true, avalanche: true, snowBuckets: true, lodgeSave: true, fePopup: true, litLiveComp: true, lodgeFlash: true, stateCursor: true, introLead: true, litInstances: true, equipLoading: true, titleStart: true, startConsume: true, sceneLightingOff: true, threeLean: true, worldWarm: true, switchGate: true, ps2MenuInput: true, ws13Rebuild: true, bcDecline: false, finishSkip: false, worldSwitchAudio: true, postEventDj: false, djVisited: false, djQueueRules: false, mailFreeze: false, faqDefer: false, transportFade: false, departCalls: false, peakSplines: true, gcWatchdog: true, worldUnderCuts: false, eventWorldData: false, eventInWorld: false, eventInWorldAi: false, nisSectionPoint: false, eventReturnInWorld: false, loadCopies: false, switchGC: false, padCarry: true});
const overrides = new Map();
function fromQuery() {
  const q = new URLSearchParams(globalThis.location?.search ?? '').get('pv');
  if (q == null) return null;
  const all = (on) => new Map(Object.keys(PV_DEFAULTS).map((k) => [k, on]));
  if (q === '1' || q === 'all') return all(true);
  if (q === '0' || q === '') return all(false);
  const out = new Map(Object.entries(PV_DEFAULTS));
  for (const w of q.split(',')) { const off = w.startsWith('-'), k = off ? w.slice(1) : w; if (k in PV_DEFAULTS) out.set(k, !off); }
  return out;
}
const query = fromQuery();
export function pv(name) {
  if (!(name in PV_DEFAULTS)) throw new Error(`Unknown presentation switch ${name}`);
  if (overrides.has(name)) return overrides.get(name);
  return query ? query.get(name) : PV_DEFAULTS[name];
}
// Tests: force a switch (null restores the default / query).
export function setPv(name, on) { if (on == null) overrides.delete(name); else overrides.set(name, !!on); }
