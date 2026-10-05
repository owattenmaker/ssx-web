> **Deployed 2026-10-04 (coordinator): pv tickLock and eventRiderWarm ON, bail diagnostics live.** tickLock (user-approved): on a ~60 Hz display each frame runs one tick and draws it, no interpolation between the last two ticks (as the PS2 draws its newest update); sim hashes identical on / off. eventRiderWarm: the five computer riders' race models compile under the gate approach (WebKit: card max 35 ms, race max 52 ms, no block over 150 ms; Chrome the same). web/diag-bail.js: a 'bail' event per crash (cause, attacked, speeds, launch direction, last 8 pads) and a 'reset' event per placement. Still off, owed a WebKit check: boardFlex, finishHudHide, setBlendClass; padRing is being built (off).

> **The gondola cabin seen from outside: pv setBlendClass (new, off) (2026-10-04, gondola agent; recorded by the coordinator):** see visual-parity.md section 46. The TRANSP cabin (one class-2 material, word +0x0C 0x70001) was drawn as one blended pass without depth writes, so from outside (gond_inair #150) the far walls covered the near ones. PS2 37E238: class from +0x0C & 0x660000 (0x20000 ATST GREATER 92; 0x40000 / 0x60000 ATST GREATER 20), Z written (ZMSK only for additive model headers), class 2 sorted per node by the node origin's view depth (37F6E8, key 364240), so the far side draws first. Port: tools/export_cutscene_sets.py splits class-2 batches per node with `blend` / `sort_pivot`; cutscenes.js draws the course renderer's two passes with the switch on. Data copied in (coordinator): CUTSCENES/SETS/TRANSP/world.json (binaries unchanged; old file backed up in the coordinator scratchpad). Checked in Chrome against PS2 ctm-parity/runs/to-final; WebKit owed (screen locked). Open: the heli / plane sets' class-2 windows.

> **Deployed 2026-10-04 (coordinator): core3 with board + hand morphs (inert), assets in.** web/runtime core.wasm `5b247d6a…`, core.js `920f941b…` from local/board-flex/core3 (276 clean; includes the rider_context snapshot_qa fix). Copied into web/public/assets: RIDER_*/board-flex.{json,bin} (29 packages, 524 KB) and RIDER_*/hand-morphs.{json,bin} (29 packages, 1.4 MB; RIDER_NWLEGEND has hands only, its board has no morphs). Still off, waiting on a WebKit check with the screen unlocked: boardFlex, finishHudHide, eventRiderWarm, setBlendClass (gondola; needs a TRANSP re-export copy); tickLock awaits the user's decision.

> **Board flex and hand morphs: the board bends and the hands grip, bit-exact weights (2026-10-04, playtester-bugs agent; pv `boardFlex`, off until the WebKit visual check):** see [characters.md](characters.md) "Board flex". Playtest report: "boardflex doesn't work".
> - **Cause:** the board (board_BoardFlex<X>, file 2, 8 morphs) and the race hands (HandsX, file 7, 18 morphs) are morph-target parts. The PS2's pose blender 30F2B0 (from 312598) blends their weights from the clips' file-2 / file-7 streams into *(geometry+0x3C), with the bones' layer weights:
>   - the first covering layer writes; later layers blend (1 - w) out + w s;
>   - a mirrored layer reads part+0x40 = the MNF morph_ids;
>   - the slot bit is the slot count + the morph index;
>   - with no covering layer, the weights are zeroed (0x3100D0).
>   The port computed none of these and its race packages had no morph targets, so the board was always straight and the hands always open.
> - **What the PS2 does:** the board bends in board presses (morph 5 at 0.99), rail presses, grinds and some landings. It does not bend on plain rail balance: Zoe's rail semantics 68 / 18 have no file-2 stream. The hands are non-zero on most ticks (bar grip at the gate, grabs).
> - **Core** (inert unless configured, nothing in the simulation reads it):
>   - engine/animation_motion.cpp `originalAnimationMorphWeights`;
>   - web/animation_bridge.cpp `board_morph_configure` / `morph_part_add` / `morph_upper_bit` / `board_morph_slot` / `board_morph_count` / `board_morph_weights`;
>   - sampled with every pose: the tick, the 11D660 placement (via the stage teleport), the mission placement.
>   - Scratch core local/board-flex/core3, to install.
> - **Page:**
>   - web/board-flex.js loads `board-flex.json/.bin` and `hand-morphs.json/.bin` and configures the core after each init_animation: the human in main.js, a computer rider at its capture in opponent-riders.js. The hands' bit comes from rider+0x8C0 (11C298).
>   - web/rider-skinning.js adds sum(w x delta) before each palette's skin: 26 columns, one shader for every rider; the shadow uses the same node.
>   - web/wardrobe.js builds both pairs for outfits (GameCube deltas).
> - **Assets** (tools/export_board_flex.py): the deltas come from the PS2 MPF morph packets (VIF UNPACK V4-8, 4 mm units), assigned to morphs in order.
>   - Board A equals an independent decode (0.0 cm). The GameCube twins are within 0.8 cm (board) and 0.39 cm (hands).
>   - board-flex: in web/public/assets (coordinator, 2026-10-04).
>   - hand-morphs: also in web/public/assets (coordinator, 2026-10-04; 29 packages; RIDER_NWLEGEND has hands only, its BoardFlexD has no morphs).
> - **Evidence:**
>   - 4 new ARMSX2 captures watching 0x5DC000: local/board-flex/runs/bf-{boardpress-nose,boardpress-rail,rail-balance-lr,tech-land-mid}, linked as runs/boardflex.
>   - Gates `ps2-captures boardflex/*` (web/board-flex-compare.mjs): board and hand weights bit-equal on every tick (239 / 499 / 800 / 274).
>   - PS2 frames: local/board-flex/snaps.
>   - Chrome: local/board-flex/shots, side-chrome.png and hands-side-chrome.png (off | on; the tail curls in a nose press) and nose438-chrome.png (PS2 | off | on).
>   - WebKit: the weights and the renderer's uniforms are equal to Chrome's (shots/bf-boardpress-nose.wk-data.json). WebKit frames are not taken: the screen was locked (CGSSessionScreenIsLocked), so the WKWebView is hidden and rAF is stalled.
> - **Tests:** test-board-flex (new, in test:all), rider-skinning, opponent-riders, wardrobe, sam-gear, line-length and comment-code pass. Full ps2-captures on core3: 276 clean (wasm 5b247d6a…, js 920f941b…).
> - **To turn on:**
>   - install core3;
>   - a WebKit look with the screen unlocked: `RUNS=$PWD/local/board-flex/runs/ node local/board-flex/qa/vpshot.mjs bf-boardpress-nose OUT 438 --pin --side 2.6 --up 0.9 --calib 400 --browser wk --params pv=boardFlex` against `local/board-flex/qa/overlay.mjs`, or the live dev server once the core and assets are in;
>   - then set `boardFlex` true.
>   Online remote riders draw the rest shape.

> **Finish HUD: everything but the banner goes on the finish tick (2026-10-04, finish-HUD agent; pv `finishHudHide`, off until the WebKit check):** see [visual-parity.md](visual-parity.md) section 45. Playtest report: "The game HUD doesn't disappear when the FINISH text appears."
> - **PS2 (dis.pkl):**
>   - Once rider+0x470 >= 0 (FINISH or TIME'S UP; not event type 4), 1EB9E8 sets the per-player mask +0x80 = 0xFFEFFFFF (0x1EB9FC).
>   - Once every human has finished, 12A250 (0x1EB91C) cuts owner+0x3CC to 0x170000.
>   - The draw 0x1ECB04 (flags = 0x3CC & ~mask | +0x84) then leaves only 0x100000, the banner 21F660.
>   - The clock, meter (gp-0x994), mail icon, place, gauge (0x200000), trick slots, "S" and hints cut out on the finish tick, with no fade.
> - **Evidence:**
>   - setpieces/full re-run with per-tick snapshots (local/ps2-capture/runs/finishhud/, 12,306 records byte-equal): +0x470 first >= 0 at tick 12297; snapshot 12297 shows the full HUD, 12300 onward the banner alone.
>   - Also ctm-parity race-f 14341 / 14351 and race-q 13720 / 13741.
>   - Peak runs follow the same code (no PS2 frame).
> - **Port:**
>   - web/ui.js `finishHide` gates the collect counter, place, clock, score fallback, speed, progress meter, boost gauge / orb / letters and "S", and passes flags & 0x100000 to the trick-slot frame.
>   - web/career-messages.js `drawHud(c, racing, draw)` keeps the mail icon's timer running while it is hidden (mailFreeze timing).
>   - web/main.js `ui.freeRideHud(c, level, finishHide)` cuts the peak-run clock / split.
>   - Freestyle was already right.
> - **Checks:**
>   - Chrome, real Single Event flow (modeshot.mjs setpieces/full, position error 0): with the switch on, full HUD at 12296 and the banner alone from 12297, as on the PS2; with it off, the bug.
>   - Staged finish / TIME'S UP (fin.mjs): banner only.
>   - WebKit not done (screen locked): run `modeshot.mjs setpieces/full OUT 12297,12298,12300 --browser wk --extra '&pv=finishHudHide'`, then flip `finishHudHide` to true.
> - **Tests:** test-visual-parity R37 (new); visual-parity, messages, fe-screens, ctm-flow, ctm-left, peak-mountain, career-rider pass.
> - **Left:** the PS2's "Loading..." caption from about finish + 6 ticks (S+0x94 bit 4, ctm-decomp-world-states.md rank 7).

> **The in-world card freeze, the riders' warm-up, tickLock, the diag gpu tag (2026-10-04, lag agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) "The card freeze", [workers.md](workers.md) "One tick per drawn frame at 60 Hz", [mobile.md](mobile.md) "Hardware acceleration".
> - **Report:** a Windows **Firefox** playtester (diag 71f4ne0n, the only in-world session since 10-01) at the Snow Jam qualifier card.
>   - The main thread was blocked for 19.45 s, from 0.8 s after the card opened (hangWatch).
>   - Then a 9.2 s frame gap at GO: 9 pipelines, 58 builds, 66 textures.
> - **Cause 1, fixed (core2, installed 2026-10-04):** the countdown snapshot's QA heap attribution.
>   - The first save of each slot called dlmalloc mallinfo() twice per holder, and each call walks the whole heap (1.1 ms on 156 MB).
>   - That cost 3.8-4.3 s at 1x and 17.4 s at Chrome 4x on the old core.
>   - web/rider_context.cpp: the attribution now runs only with snapshot_qa. core2 wasm 062e4076…, js 3b34df40… (it carries the board-flex agent's inert board-morph exports). Full ps2-captures: 272 clean.
> - **Cause 2 (pv eventRiderWarm, new, off):** the computer riders' race models were first built at GO (53 builds and 7 pipelines: 0.23 s at 1x, 1.0 s at 4x on core2).
>   - main.js warmEventRiders compiles them mesh by mesh under the approach and the card. cutscenes.js playEventIntro got an optional onStep.
>   - Chrome, cold profile, core2 + switch: the worst frame of Continue +5 s is 17 ms at 1x (225 without the switch) and 125 ms at 4x (992 without), with no main-thread block.
>   - Fly-over and card frames are unchanged.
>   - **Proposed on** after a WebKit smoke. WebKit was not checked (screen locked). Firefox could not be launched here (macOS privacy blocks ~/Library/Application Support/Firefox from the agent shell).
> - **Input latency:**
>   - The port reads input, ticks and draws in one animation frame, adds no queue of its own and pages the catch-up as the PS2's frame loop does.
>   - The added lag is the render interpolation (alpha = leftover x 60): 0..16.7 ms, mean 5-10 ms on recorded rAF times. The PS2 draws its newest update with no interpolation.
>   - **pv tickLock (new, off; web/tick-lock.js, test-tick-lock.mjs in test:all):** a ~60 Hz frame runs exactly one tick and draws it (alpha 1). The real-time debt is paid a tick at a time past 4 ticks. Online and the replay clock are untouched.
>   - Chrome ?simtrace: identical on and off over 1406 ticks. WebKit not checked.
>   - Not confirmed from the code: the PS2's draw-to-display depth (game vt+0x3C's GS kick / display-buffer swap).
> - **30 fps:** no cap (the user's decision). PS2 facts:
>   - The game runs one update per vblank (app +0x10 = 60 Hz, +0x14 = 1/60) and has no 30 Hz mode in 0x316F00.
>   - A frame that takes 2 vblanks runs 2 updates before the next draw (the stall savestates, ps2-frame-pacing.js). So a "framelimited" Metro City on the PS2 is the draw falling to 30 fps while the game stays at 60 updates a second, which is what the port does.
> - **Diag:** the renderer event and every heartbeat carry `gpu` ("webgpu nvidia/lovelace", "… fallback …", "… software …", "compat"). Firefox, with its empty adapter info, makes one more requestAdapter for its isFallbackAdapter and sends 'gpu-adapter'.
>   - Field data: every Windows WebGPU session so far was on a real GPU. The tester's Firefox adapter had BC, f16 and timestamps, at 17 ms free-ride frames.
> - **Left (4x):**
>   - aiRace.prepare's install configures the 5 riders in one task: 441 ms at the fly-over -> approach join. Spreading it needs an exactness check, since the free ride ticks between the riders.
>   - The cast's FE builds: 150-400 ms frames.
>   - Opponent lighting init: 121 ms.
>   - Levers not tried: WebGPURenderer's powerPreference (unset: a dual-GPU laptop may get the integrated GPU) and the canvas's alpha: true.
> - QA: local/lag-latency/qa (cardprobe.mjs: the cold-profile gate -> card -> Continue probe, BROWSER=chrome|webkit THROTTLE=4 PV= QX=; recorder.js, sum.mjs, builds.mjs, prof.mjs; latprobe.mjs: the tickLock A/B; the runs' JSON), local/lag-latency/core2 and its capture log.

> **No pause on focus loss; the FAQ's way out (2026-10-04, focus-pause agent):** see [pause-contexts.md](pause-contexts.md) section 5.
> - **Removed (no switch):** main.js opened the pause overlay on window blur and on visibilitychange to hidden. Playtesters hit it in the Transport ride, the gondola and NIS cuts, where the PS2 takes no Start (WS10 / WS11 / WS14, local/transport-stall/ps2/run1). They got the pause menu over the gondola while the race started without them, and a stuck camera when a cutscene's start was paused.
> - **Now, on blur or hidden:** main.js `releaseInput()` runs clearInput() (keys, the touch deck, the key log) and web/gamepad.js `setPadsFocused(false)`.
>   - While unfocused, pollPads returns a neutral pad: no reads, no 'use' events. Chromium kept feeding the pad to a visible unfocused window.
>   - On 'focus' the pad comes back. Its first poll re-reads the rest state, so a button already down counts as held, not as a new press.
> - **On show:** `last = performance.now()`. The first frame back no longer runs ps2FrameTime of the hidden seconds (up to 12 ticks).
> - **Unchanged:** the audio's hidden suspend (audio-engine.js installAudioUnlock), and the online race's worker ticks while hidden (server-paced, neutral input). `?perf=1` keeps the input as before.
> - **Every other way into the overlay** (pad / keyboard / touch Start through startOpensPause with cutscene || transporting and the stack's HOLD contexts; the FAQ and station opens) was checked. None opens during WS1 / WS10 / WS11 / WS14. The countdown keeps Start, as on the PS2.
> - **FAQ "?" stuck:** the cause was career-ui.js back() (the hotfix below). The FAQ view's list can only be left with Triangle, and Back did nothing there.
> - **Tests:**
>   - test-pause-contexts "losing focus never pauses": the blur / focus / visibility / pagehide listeners of every page module open no screen and push no context, and main.js releases the input and resyncs the clock;
>   - test-gamepad: the unfocused pad;
>   - test-ctm-flow "The FAQ can be left": Triangle from the FAQ view, and each of its three buttons then Back, return to the ride with one resume, and the folder closes again.
> - **Browser:** scratchpad focusprobe.mjs (blur + hidden + visibilitychange, rAF held as in a hidden tab, then back) in Chrome: 22/22. It covered the new career's arrival cut, free ride, an MCOMM Transport ride to 18, the round card, the countdown and a race. In each: no pause, the stack unchanged, after the return 1 tick per 16.7 ms frame, arrival at 18 with nothing held. WebKit could not run: the screen was locked (visibilityState 'hidden' from the load).

> **Deployed 2026-10-04 (coordinator): the in-world card freeze fix and no pause on focus loss.** Core local/lag-latency/core2 (wasm `062e4076…`, core.js `3b34df40…`, 272 clean): rider_context.cpp snapshot_save runs the mallinfo heap attribution only with snapshot_qa (it walked the whole heap 3192 times at the in-world Continue: 4.3 s at 1x, 17.4 s at 4x, the tester's 18.4 s on Windows Firefox; now ~225 ms). It also carries the board-flex agent's inert board-morph exports (used only behind pv boardFlex, off). main.js: blur / visibilitychange no longer open the pause; they release input (gamepad.js setPadsFocused) and resync the frame clock on return (user decision; Chrome probe 22/22, WebKit owed: the screen was locked). diagnostics.js tags the GPU adapter (fallback / software) in the heartbeats.

> **Audit for swallowed statements: none left (2026-10-04, cleanup agent):** started after the career-ui back() hotfix (next entry).
> - **Comments that hold code:** every `//` comment in web/*.js, net/*.js, server/*.mjs, tools/*.mjs and web/*.mjs tests was parsed as JS, along with each suffix of it that starts at a code-like token. A hit is a parse with a call, return, if or assignment, scored by `;`, `return`, `if (`, `this.` / `ui.`, or code glued to the prose before it. In the pre-cleanup mirror tree (a770508), only back() scores high (13). The current tree has no swallowed code; its 20 hits at score 3 or more are prose and usage docs (ctm-map's region shape, the now-playing usage example, the audio-painters formula).
> - **Statements that disappeared:** every leaf statement and import specifier was compared, comments ignored, across a770508 -> 9630f89 (the cleanup) and 9630f89 -> 448e679 (ps2MenuInput and the unused exports). A statement counts as explained when it, or an enclosing test, uses a retired switch, a variable or getter bound to one (effectOrder, psRelease, bytesMode, this.on = transportMap...), or comes after an `if (pv(x)) return` guard.
>   - The rest are accounted for:
>     - helpers and constants that only the deleted off paths used (byteBlend's registerWorldAdditive / fog-shared / rival-beam's linear path, effectOrder's fallback render orders, luiLights' OPEN / LIGHTS / CLOSE, psRelease's prefetchOrder, awardCascade's checkAwards);
>     - the old pad-menu model and screen-phases advance() (ps2MenuInput);
>     - the 25 exports removed on purpose;
>     - other agents' feature edits in the same window (online records: topRecord / online() / MAIN_HELP / the sixth main item; career rival: assembleLineup's level; the race card round tab);
>     - &&-|| regrouping.
>   - No unexplained statement loss was found.
> - **Why the AST-equal proof missed it:** the code was already a comment in the file the reformat started from (the online-records edit, 09-30 before 21:39 HST; the cleanup began at 21:50). The proof only shows a reformat equals its input, so it cannot catch a bug that was already there. The reformat put the swallowed code on its own lines, and the comment re-wrap split it into two `//` lines.
> - **Fixed in passing:** set-piece-particles.js's unused `let order = 0` (left by the effectOrder fold); test-stage-world passes.
> - **New guard `web/test-comment-code.mjs`** (second in test:all, 1.3 s): runs this scan with rolldown/experimental parseSync (oxc, already in web/node_modules) over web/*.js, web/*.mjs, net, server and tools/**.mjs, and fails at score 8 or more, naming file:line with "Move the comment to its own line". It checks itself first: the career-ui case scores 13 and ctm-map's region note scores 6, which is the highest score in the tree today.

> **Hotfix deployed 2026-10-04 (coordinator): Back on the Message Center / FAQ / lodge / Big Challenge prompts.** An online-records edit on 2026-09-30 (mirror a770508) put a `//` comment mid-way through the one-line minified back() in web/career-ui.js, which swallowed its three dispatches (`// pv onlineRecordsif(this.lodge.owns(s))...`); the later cleanup reformat only made it visible, so Triangle / Escape / touch Back did nothing on those screens (playtest report: stuck in the FAQ). Restored by the focus-pause agent; the cleanup agent is auditing the tree for other swallowed statements.

> **Phones on the in-world set + returnGC (2026-10-01, CTM events-in-world agent; core-rcam2 installed, deploy held for these):** PV_DEFAULTS: worldUnderCuts, eventWorldData, eventInWorld, eventInWorldAi, nisSectionPoint, eventReturnInWorld and returnGC true on every tier; PV_DESKTOP / desktopTier and test-pv-desktop.mjs removed. snapshot-policy.mjs: fastSkinBones (the low tier's drawn-frame skin matrices) moved from KEEP to CURRENT. On the live core the phone tier's replay failed the ?qa keep check, so the earlier phone memory runs had no replay. core-rcam2 (wasm 4e80f70b…, js d116e349…, with the attacked-bail edits): full ps2-captures 272 clean. Smokes on the page defaults: Chrome Android preset and WebKit 844x390 quality=low, gate -> race -> Give Up -> results replay (triggers firing) -> Restart heat -> Transport -> return, no errors. WebKit phone policy, 4 cycles, the replay playing in every results phase: event-phase medians 946-999 MB (p95 of phase medians 990), cycle peaks <= 1123, WebKit Malloc 607-633 flat, returnGC full in about 155 ms at each map, longest frame 39 ms at the map and 41 ms in the ride after (none over 50). Boot / first Transport peaks (1464 / 1367) are the existing load spikes (event-load boots at 1395-1439). The event-load CTM path stays the fallback (?pv=-eventInWorld) for one release.

> **Deployed 2026-10-01 (coordinator): in-world CTM events on every tier, returnGC on, core-rcam2.** The earlier hold is lifted: the WebKit phone smoke passed (gate, race, Give Up, results replay with triggers, Restart heat, Transport, return) and the 4-cycle WebKit phone memory run with the replay playing stayed at 946-999 MB per event phase, cycle peaks 992-1123, WebKit Malloc flat, returnGC ~155 ms on the map, no frame over 50 ms. Boot / first Transport (1367-1464) are the pre-existing load spikes. Event-load stays the fallback (?pv=-eventInWorld) for one release. Open: a real-iPhone try of returnGC's fast-slot ordering.

> **pv returnGC (new, off; 2026-10-01, CTM events-in-world agent):** the in-world path's phone memory step is JSC heap headroom: no course switch collects the event's garbage. Shown by a collection at each return (KICK=1: flat, WebKit Malloc 570-615 against 900-985 without). web/main.js returnCollect: switch-gc.js collectNow when the results' Transport map opens (career-ui.js transportAfterEvent -> ui.cb.eventMapShown; WS14, the world held, not the ride). The coreTracker's markers now start under switchGC or returnGC. newCore awaits a running collection (its memories dropped before any instantiation). WebKit phone policy, 8 cycles: medians 951-983 MB flat, cycle peaks <= 1059, the collection full in about 150 ms, longest map frame 34 ms, ride after the return 41 ms (none over 50). Event-load for comparison: 818-994 / peaks up to 1281. Proposed: phones get the desktop in-world set plus returnGC.

> **Replays solid: the verifier's replay was stopped under it; Watch Replay x3 and across courses exact (2026-10-01, online records agent):** see [online-records.md](online-records.md) "The verifier".
> - **Cause:**
>   - The ?verify path never set the `replay` screen, so main.js `replayFrame` stopped its replay on the first frame, and `aiRace.replayEnd` handed the live relationship tables back.
>   - The verify loop stepped on regardless, on this browser's stored tables (localStorage `ssx3.relationships.v1`, re-saved by every rider-pair reaction).
>   - The first run in a fresh profile was exact; every later one diverged.
>   - Watch Replay sets the screen, so it was never affected.
> - **Fix (web/online-replay.js):**
>   - verify sets the replay screen;
>   - the loop gives up ('could not run') if the replay stops under it;
>   - the verifier is back to a fresh page every 5 runs (`--reload 5`).
> - **Tests:**
>   - web/test-online-records.mjs now watches three times in a row on Snow Jam, then after a course switch from Metro-City, then with Psymon selected, each to the finish (15,171 ticks), all exact; also WebKit (scratch web/.online-records-webkit.mjs: x3 and cross-course, 14,363 ticks each).
>   - web/test-records-verifier.mjs verifies all five runs on one page (`--reload 5`): genuine Snow Jam and big air verified, the faster claim, the forged replay and the inflated score pulled.
>   - The live-run tick budget in both tests is 22,000: tuck runs vary 13,000-17,100 ticks with the computer riders.

> **Records verifier installed on the host (2026-10-01, coordinator):** Chrome for Testing 154.0.8037.92 in ~/ssx-host/chrome (npx @puppeteer/browsers, user-approved; not in /Applications, no auto-update), token ~/ssx-host/state/verifier-token (0600), MP_RECORDS_VERIFIER_TOKEN_FILE added to the server plist (backup state/server.plist.bak-20261001-verifier), LaunchAgent <SSX_LABEL>.verifier bootstrapped (running; --chrome points at the Chrome for Testing binary; log ~/ssx-host/logs/verifier.log). The queue answers with the token on loopback ({"items":[]}) and 404s without it and through the tunnel. Not yet seen: a live submission verified end to end on the host.

> **Online records anti-cheat: the world-record floors, flagged runs, the admin CLI, and the replay verifier (D5) (2026-10-01, online records agent):** see [online-records.md](online-records.md) "Anti-cheat floors" / "The verifier" and [hosting.md](hosting.md) "The records verifier". Server and page; pv onlineRecords (on).
> - **Floors:** web/server/record-floors.json (tools/fetch-record-floors.mjs, speedrun.com API, 2026-10-01). All eight timed boards have a level there.
>   - The floor is 0.9 x the fastest verified racing run of any category (No Restrictions included) on any platform: the port is bit-exact, so the runners' glitches work in it.
>   - A run under its floor is stored flagged and not listed; the player sees "Your run is under review.".
>   - Score boards have no floor.
>   - Found: the old tier-0 route floor hard-rejected runs faster than real records (Metro City 87.2 s vs 85 s; also Intimidator, Happiness, The Throne). It is gone.
> - **Admin:** web/server/records-admin.mjs (flagged / list / show / approve / delete) edits board.json; the server reads the changed file on its next request.
> - **Verifier:**
>   - web/server/records-verifier.mjs (a LaunchAgent, niced, one run at a time, skips on load or an online race; template deploy/ssx.verifier.plist.example) drives the page's ?verify path (web/online-replay.js) in headless Chrome.
>   - It calls /mp/records/verifier/* (loopback + MP_RECORDS_VERIFIER_TOKEN_FILE, refused through the tunnel).
>   - Rules (records.mjs verifyEntry): reproduced -> verified, and a flagged run is listed; not reproduced on its own core -> pulled; on another core -> stale (D7), one try per core; a new core re-verifies the verified runs.
>   - The first finish decides: an early finish fails at once. A page timeout counts as not reproduced.
> - **Claim change:** score events now claim the score latched at the finish tick (game-tick.js `replay.finish(rec.finish)`, replay.js `finishInfo`), which the verifier reproduces; races claim the same race ticks as before.
> - **Tests:**
>   - test-records-server (floor: under -> flagged and hidden, at / above -> listed; the CLI as child processes; the verify rules; the endpoints' token / loopback / tunnel checks);
>   - new test-records-verifier (npm test, about 3 min): a genuine Snow Jam run uploaded through the page is verified; the same replay with a 2.5 s faster claim is pulled; a forged neutral-pad replay is pulled ("finished at tick 12867, the run ends at 14835");
>   - test-online-records, test-replay, test-ctm-left, test-career-rider, test-line-length pass.
> - **Cost per verification (this Mac):** ~20 s wall (12-15 s simulation), 16-29 s Chrome CPU, Chrome's process tree ~2.6-2.8 GB resident (summed, an upper bound), plus a 30-60 s page load per batch.
> - **Open:** host install (docs/hosting.md: Chrome, the token file, the server env key, the agent). A score-event run was not part of the end-to-end test (the latched-score path is the same code).

> **Deployed 2026-10-01 (coordinator): attacked bails core + in-world CTM events on desktop.** web/runtime core.js `d116e349…`, core.wasm `a9e6a9df…` from local/attacked-bail/core2 (272 clean): an attack crash counts +0x12C and popup 0x2D (119B08 via 10EB30's a2 from 107E70). pv-flags.js PV_DESKTOP turns worldUnderCuts, eventWorldData, eventInWorld, eventInWorldAi, nisSectionPoint and eventReturnInWorld on for desktop only (not iOS / Android, not quality=low at load); phones keep the event-load path, which also stays the fallback for one release. Phone memory: no leak, a one-time +300 MB WebKit Malloc step, steady ~1.33 GB in-world; the event-load comparison run is pending before phones.

> **In-world CTM events on for desktop (2026-10-01, CTM events-in-world agent; to deploy):** web/pv-flags.js PV_DESKTOP + desktopTier(): worldUnderCuts, eventWorldData, eventInWorld, eventInWorldAi, nisSectionPoint, eventReturnInWorld default on when the page is not iOS / Android and not the low quality tier (the old mountainRide split, read once at load); phones keep the event-load path. nisPreload / nisBoneProbe / transportFade stay off. New test-pv-desktop.mjs (in test:all). Tests: line-length, nis-after-scan, ctm-stream, ctm-map, replay, presentation, visual-parity, diagnostics-bindpose, pv-desktop, career(-rival / -rider), cutscenes, ctm-flow, ctm-left, ctm-audio, ctm-event-world, ctm-event-ai, mountain-ride: 19/19. Chrome smokes on the page defaults (r9replay.mjs NOPV=1 HEAT=1 [MOBILE=1]): desktop takes the in-world path end to end; the Android phone setup takes the event-load path. See [ctm-events-in-world.md](ctm-events-in-world.md) "Turning it on".

> **Attacked bails: the victim's crash counts +0x12C, the attacker's +0x128 (2026-10-01, attacked-bail agent):** see [crash-motion.md](crash-motion.md) "Attacked bails".
> - **Rule (dis.pkl):** 10EB30's a2 only reaches 119B08 (0x10EB94): attacked -> +0x12C and popup 0x2D (1.5 s), else +0x124. Only 107E70 passes a nonzero a2 (its a3, 0x1082F4), and only 107888's attack branch calls 107E70 with a3 = 1 (0x107E0C). No time window: only the crash entered by that same call counts as attacked. The attacker's credit is 10E468 -> 119400 (+0x128 KO, popup 0x2C, 10E098(attacker, 1.0, 2)), which the port already had (pair_knockout).
> - **Core:** web/npc_gameplay.inc pair_react passes the attack flag to browserHardCrash(semantic, event, attacked) (web/core.cpp), and enter_crash (web/animation_bridge.cpp) passes it to originalHardCrashEnter. It was always false before. Marker export `_pair_attacked_bail` (web/build-core.sh).
> - **Scratch core** local/attacked-bail/core2 (current tree): core.wasm a9e6a9df1d843f11639e97f736d8d1bea034a6df75630ca96285d919721a00de, core.js d116e3491ff797ced2dd523121bec42976c6b4113bc0dd7a85155ac63b25c56f. Full ps2-captures on it: 272 clean.
> - **Gates (keyed on the marker, so the live core stays green):**
>   - careerrival/dra4-final scoreThrough is 1500 on the new core (the human's attacked bail at 229), 228 on an older core.
>   - New attackbail/ko-attack-moby (coreExport `_pair_attacked_bail`): ko-attack re-captured with `--watch 0x5d6600:0x1d0` (Moby's score object). Its records are byte-equal to ko-attack before the watch window. Capture: local/attacked-bail/runs, linked from local/ps2-capture/runs/attackbail. The new web/ai-score-compare.mjs (a compare-ai-capture TICK_HOOK; aiCases take `tickHook` / `aiScoreThrough`) compares Moby's score object: exact for all 368 ticks, with +0x12C at 265; the old core counts +0x124 there.
> - No asset changes. A scan of every capture with a score layout found attacked bails (+0x12C) only in these two captures.

> **WebKit checks and the 8-cycle phone memory run (2026-10-01, CTM events-in-world agent; core-rcam live):** see [ctm-events-in-world.md](ctm-events-in-world.md) "Rollout checks".
> - WS13 on the page, WebKit (heatqa.mjs; the page logs kept in window.__hl for the WebKit driver): Restart and Next heat both give grid 301 / release 492 / list end 521 / rank mode 523 / card 525, then the heat runs.
> - pv nisPreload, WebKit: the booth's t 0 pose is in the firing tick (off: 26 ticks late). The switch stays off (the booth probe is parked).
> - Memory, WebKit phone policy, 8 cycles: medians 930 / 955 / 974 / 1297 / 1265 / 1248 / 1240 / 1234 MB, lifetime peak 1519 (boot), wasm flat at 221, no world loads. The per-cycle growth is gone (before: +35 MB per cycle). One step of +320 MB comes inside cycle 4's results, outside the wasm heap; it is not repeated and its category is not measured.

> **Versioned asset URLs: measured, shelved (2026-10-01, versioned-assets agent):** the idea: game files fetched as `/assets/<path>?v=<md5 prefix>` from a build manifest, answered `immutable` by the origin (checked against an md5 index that deploy-staged.sh writes from the host's staged files), so a repeat visit makes no revalidation requests. Not built; the user chose to skip it for now. The pv switch `versionedAssets` and `web/asset-versions.js` are removed again. Kept: `web/webkit-driver.mjs` `startWebKit({store, trustLocal})` (`--store UUID`: a persistent data store of its own; `--trust-local`: accept https://127.0.0.1's certificate), for first / repeat visit runs.
>   - **How it was measured:** a production build served by the real web/server/mp-server.mjs (gate on, edge secret) behind a local HTTP/2 stand-in that runs the real deploy/edge-worker.js over a small CDN cache (CDN-Cache-Control max-age / stale-while-revalidate / no-store, ETag 304s). Added delays: the edge round trip on every request and 60 ms per trip to the origin; 50 Mbit/s link. "Immutable" = a throwaway stand-in flag marking every /assets response `private, max-age=31536000, immutable` (the upper bound of what versioning buys). Flow: Press START, 3 s in the menus, Snow Jam Single Event; medians of 4 repeat visits.
>   - **Chrome, repeat visit, today -> immutable:** edge RTT 50 ms: Press START 649 -> 227 ms (65 requests, 64 of them 304s -> 1), Press START -> course ready (course:live) 4.9 -> 1.8 s (175 requests -> 2). Edge RTT 120 ms: Press START 1384 -> 312 ms, course ready 9.9 -> 2.3 s. The event load screen stays at its 7 s minimum either way (the saving shows when the course is still loading behind the menus: an early pick, CTM, course switches). Before the title the requests come from 34 `<img>`, 25 fetch, 6 preload links; after it 226 of 235 are fetch (web/downloads.js).
>   - **WebKit (WKWebView, the driver):** no measurable difference: between launches its store drops 60-70 MB of the HTTP cache (even with a 20 s wait before quitting; even the immutable hashed core.wasm is fetched again), so its repeat visits are bound by re-downloads. Real Safari cannot be driven.
>   - **Field telemetry** (diag.log aggregates on the host; repeat = an address + user agent seen in an earlier session; small samples, 09-24..10-01): title ready (lazyCourse) first p50 2.55 s (n=13), repeat p50 1.75 s / p90 7.5 s (n=12; Chrome 3.4 s, Safari 1.36 s); foreground course loadMs first p50 14.0 s (n=41), repeat p50 6.6 s / p90 15.7 s (n=22).
>   - **If picked up:** smallest version = one hashed manifest in dist-online (54 KB gzip for 5,291 files, grouped by directory) referenced from index.html; downloads.js rewrites both fetch paths (shared and Range / signal pass-through); the server checks ?v= against `public/assets/.md5-index` written by deploy-staged.sh (mismatch or no entry = today's headers + CDN no-store); edge-worker.js keeps a valid ?v= in the cache key apart from CACHE_GEN; deploy-staged.sh refuses when the built manifest differs from the staged tree. Then the ~20 `new Image()` / `<video>` call sites through an explicit `assetUrl()` helper and the boot title's pictures / preload links; the workers that fetch themselves (fe-preview-prepare.js, peak-world-prepare.js) last. Costs: everything downloads once when it is turned on (new URLs); deleted files still need the wait rule for old tabs.

> **Live core installed and deployed: core-rcam (2026-10-01, coordinator):** web/runtime core.wasm `baae9957ee806bb4…` (core.js unchanged, 7734196e…), from local/ctm-events/core-rcam. core3 plus web/snapshot-policy.mjs keeping the replay camera's state (0x4C5830 trigger manager, the director) out of the countdown / results snapshots, as 0x26D818 / 0x26DBF0 do; only used behind eventReturnInWorld. Full ps2-captures on that build: 271 clean.

> **R9 follow-up: the replay camera outside the snapshot, the triggers cleared at the return (2026-10-01, CTM events-in-world agent):** settled from the code. 0x26D818 / 0x26DBF0 hold neither the trigger manager 0x4C5830 nor the view director; the restore's 22E840 only steps the cameras (0x15DF98), and 0x1620D0 keeps the trigger lists across a loop. web/snapshot-policy.mjs: every replay_camera.inc variable in SNAPSHOT_CURRENT. Core build **local/ctm-events/core-rcam** (CORE_OUT from the current tree), to install. On it: ps2-captures c0a-ret3, c0a-ws13-semi and the four keepCheck gates (eba3-rock-hit, apr-start, fr-throne-unload, setpieces/full) pass, as does test-replay. Chrome r9replay: the in-world loop carries the active trigger as event-load does. web/main.js: the trigger reload after the countdown restore now runs only when a core's restore emptied the list (the live core until core-rcam is in). eventInWorldEnd clears the triggers (_replay_camera_triggers(0), and the prepare guard), so free ride after the return holds 0, as on the PS2.

> **R9, the replay behind in-world results (2026-10-01, CTM events-in-world agent; behind eventReturnInWorld):** Chrome, Snow Jam qualifier, Give Up, the auto replay, Transport, the c0a-ret3 return (local/ctm-events/qa/r9replay.mjs; EVENTLOAD=1 runs the event-load path). Fixed in web/main.js: the in-world replay drew no trigger camera. inWorldReplayRestart's countdown restore emptied the core's replay camera triggers, because replay.js liveStart saves before prepare's fetch lands. The fetched text is now kept ({core, code, text}, cleared with the core) and loaded again after each restore. The in-world replay now fires ARA1's triggers 0, 1, 3, 4, 7 like the event-load path. prepare loads no triggers in free ride (no fetch). Left as differences (ctm-events-in-world.md R9): at a loop the restore resets the replay-camera trigger state, where event-load carries the active trigger over; and the human core keeps ARA1's triggers after the return (unused). Also parked: the booth probe (ctm-parity.md, the scope-list lead).

> **"Replay cameras unavailable Exception": a CTM free ride's request, not the race replays (2026-10-01, replay-cameras agent):** see [replay.md](replay.md) section 2 (the triggers' load) and section 3.
> - **Cause:**
>   - core getExceptionMessage: nlohmann parse_error 101, "invalid literal; last read: '<'".
>   - main.js createRaceReplay prepare (replay.liveStart) also ran for a CTM free-ride run, with course.code MOUNTAIN / PEAKn. Those streamed worlds have no camera-triggers.json; only the 17 locations have one, and each parses in the core (node, all 17).
>   - The Vite dev server answers the missing file with index.html and 200, so the core was handed HTML. The production mp-server returns 404, which the page drops without a warning.
>   - The event-load path showed it only because its free ride came first. The races were not affected: ARA1 Single Event and CTM event-load both load 48 triggers, and the auto replay fires them.
> - **PS2:** a streamed world never replays (free ride has no results; 0x20A8F8 skips modes 6-11), so it needs no triggers.
> - **Fix (main.js prepare, no switch):** no fetch for course.freeRide outside an in-world event replay; a response with an HTML content type counts as no triggers. No core change, no assets.
> - **Parity:**
>   - test-replay BHP1-PS2: a neutral The Junction run finishes at 4402 (the PS2's bhp1-neutral length is 4403).
>   - Its auto replay's Web-cam makes the PS2's 23 camera changes on the PS2's ticks: DEFAULT_3 / Bounded, and which trigger's camera (the PS2's Bounded eye is the trigger's bound point, and its fov). Two of the changes fall in the capture's poll gaps.
> - **Watch Replay:** test-online-records now checks the Web-cam: ARA1's 48 triggers load and fire, also after the course switch from BRA2.
> - **Tests:**
>   - test-replay passes: the 3 cases plus the new BHP1-PS2 and MOUNTAIN (the free ride asks for no triggers and logs no warning).
>   - test-online-records passes; test-line-length passes.
> - **Browsers:**
>   - Chrome (headless, muted): checked.
>   - WebKit not checked: the screen was locked (webkit-driver page `visibilityState hidden`, no frames). Re-run: `cd web && node ../local/replay-cameras/wk.mjs` (BHP1 cuts vs the PS2 + the free ride).

> **In-world WS13 on the page and the station-cut preload (2026-10-01, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) "The page's in-world WS13" and [ctm-parity.md](ctm-parity.md) "Bone 22 from the NIS keys".
> - **pv eventReturnInWorld (off):** the results' Restart and Next heat of an in-world race run WS13 in the streamed world (main.js ui.cb.heatInWorld / heatTick, career-ui.js intercepts). Fixes: the heat cut's location (it did not play and the card opened at once); the release at W+492 (27A9F0) instead of WS1's W+494; the list stop counted on the game tick at R+29. Chrome, both branches: grid 301, release 492, list end 521, rank mode 523, card 525 = PS2 c0a-ws13 14318 / 14509 / 14538 / 14540 / 14542, then the heat runs. QA: local/ctm-events/qa/heatqa.mjs (NEXT=1 FORCE_WIN=1 scores the Give Up as a win). WebKit is owed (screen locked).
> - **pv nisPreload (new, off):** on entering a station course, cutscenes.js preloadStation loads the door / booth step (#148 / #166 with clips), the human's cast model and the NIS bank. The booth cut now starts in the firing tick, so the hold tick has the t 0 pose (before: 18 ticks late). Open, so nisBoneProbe stays off: the core's 1242B0 hits from the hold tick, where the PS2 misses twice and then hits with the impact. It does so even fed the PS2's bone 22, so the cause is in the probe's other inputs (scope list / segment), not the bone. Needs a booth comparer run.
> - Edited: web/main.js, web/cutscenes.js, web/pv-flags.js (nisPreload), local/ctm-events/qa/heatqa.mjs. node --check and the line-length guard pass. No core change.

> **Deployed 2026-10-01 (coordinator):** the rider-shadow leak fix (web/rider-shadow.js prunes entries whose group left the scene; live on event-load career heats with changed lineups and multiplayer sessions), pv semiFresh ON (career semi riders start with +0x434 = 0x31; gate careerrival/ara1-semi), the career race card's round tab (Qualifier / Semi Final / Final Round), and the cleanup follow-ups (ps2MenuInput's old pad-menu model removed, 25 unused exports, 10 tests added to test:all). The WebKit menu smoke is still owed (screen locked during the run); live menus already ran the PS2 model since 09-29.

> **Career follow-ups: the race card's round tab, its record, the semi's fresh riders; pv semiFresh (off, proposed on) (2026-10-01, career-rival agent):** see [career-events.md](career-events.md) "The race card's round and record", [ai-racers.md](ai-racers.md) "The semi's fresh riders".
> - **Round tab (shipped, no switch):**
>   - PS2: 40race_pre's setup 0x1FB874..0x1FB928 hides tab_qualifier / tab_semifinal / tab_final, then in CTM shows the round's tab and hides 'title': 'Qualifier' for rounds 0 / 1, 'Semi Final', 'Final Round'. A Single Event keeps 'title'.
>   - Port: web/results-lui.js raceCard({ round }); web/career-ui.js passes a career race's round.
>   - Checked: Chrome's live Ruthless Ridge final card reads "Final Round"; WebKit drew all three tabs offscreen (local/career-rival/qa/tabs2.mjs).
> - **Record time:**
>   - PS2: 155130(slot, 0), entry 0 of the event's record table (14AB20 by course and mode); for Ruthless Ridge that is the default DAVE 02:59.
>   - The port's local table path is the same rule. The 02:13 was my QA's injected 8000-tick heats entering the local table (heatRecord), not a port bug.
>   - With onlineRecords on, the card prefers the online board when online (a port feature; the PS2 has no online board).
> - **ARA1 semi build:**
>   - Cause: +0x434 (ground.state.rider_type / identity.rider_type434) is the location track id, but WS13's new semi riders keep the constructor's 0x31 until the push-off. tools/export_lineups.py now checks those leaves by rule. ARA1 / CRA3 / DRA4 build again, byte-equal to the live files.
>   - Page: today's career semi gives its riders 0, the Single Event value.
>   - **pv semiFresh** (off) gives a round-2 rider 0x31. web/test-career-rival.mjs: all three Snow Jam career countdowns assemble leaf for leaf with it.
>   - New gate careerrival/ara1-semi (c0c-race on the event-load path): with 0x31 the human is exact to the end, the RNG to 460 and the riders to 530..862. With 0 the riders leave at 182..204, the RNG at 296, the human at 310.
>   - **Proposal:** turn semiFresh on. It reaches every career semi on every course; the data and rule come from the code and the ARA1 semi capture.

> **Live 2026-10-01 (coordinator): onlineRecords, careerRival + careerLevel ON; the password gate removed.** Online records: host plist has MP_RECORDS_DIR=~/ssx-host/state/records (backup state/server.plist.bak-20260930; repo template updated). Career rival: CRA3 / DRA4 lineups.json from local/career-rival/export (sha256 2a51d488… / f9502428…) copied in, new inodes. **Public site:** the user chose to remove the gate. Origin: MP_GATE_PASSWORD removed from the host plist (createGate returns null), every response carries `x-robots-tag: noindex, nofollow` and /robots.txt disallows all (web/server/mp-server.mjs). Edge: deploy/edge-worker.js checks a session only while GATE_SECRET is set; the GATE_SECRET worker secret is deleted. To restore the gate: put MP_GATE_PASSWORD back in the plist, `wrangler secret put GATE_SECRET` from ~/ssx-host/state/gate-secret, reload.

> **Cleanup: 176 switches retired, minified files reformatted, a line-length guard (2026-09-30, cleanup agent):**
> - **Switches:** PV_DEFAULTS 216 -> 40 (17 on, 23 off; ps2MenuInput retired later, below). Each retired switch's off path is deleted (pv('x') folded to its on branch; dead branches, helpers and imports removed). Every name was grepped across web, tools, tests and engine first; tests that forced an off path lost that half.
>   - Deleted with their code (off, superseded): byteBlend (with `web/fog-shared.js` and world-material's `registerWorldAdditive`: moot with encodedBlend), softSprites (softGlyphs replaced it), sfxWarmFirst (inconclusive since 09-27).
>   - Kept on purpose (their off path is used): rideWarm, worldWarm (test-ride-warm / test-world-warm compare both), encodedBlend (test-frame-space), lazyCourse (boot-files.mjs record), the field kill switches bindPoseProbe, hangWatch, gcWatchdog, and threeLean. Owners' switches untouched (nisTick, nisAfterScan, nisBoneProbe, gameTickKeep, padCarry, heatRoles, ws13Rebuild, onlineRecords, careerRival, careerLevel, versionedAssets, the eventInWorld* family); the pending off ones stay. ps2MenuInput (on since 09-29) was retired in its own batch on 10-01 (below).
>   - `web/pv-flags.js` is rewritten (688 -> 205 lines): the syntax header, one paragraph per remaining switch, **PV_DEFAULTS one key per line** (add a switch as its own line). In code comments `pv <retired name>` became just the name.
> - **Formatting (behaviour-free, each file proven AST-equal to the file it replaced: rolldown's parser, positions / comments / raw literals / parentheses ignored, &&-|| chains normalised):** main.js (765 -> 5911 lines), career-ui.js (1214 -> 3914), ui.js, lodge-ui.js and 8 minified renderers fully reformatted (prettier 140 columns, single quotes); every other file had only its statements on lines over 200 characters reformatted. Comments sit on their own `//` lines above the code they describe, text and addresses kept. Lines over 200 characters in web/*.js, net/*.js, server/*.mjs: 881 -> 0 (outside string / template literals).
> - **New guard `web/test-line-length.mjs`** (first in test:all): fails on a line over 200 characters outside literals; generated files are listed in it (three-patches.js).
> - **Source-text checks:** tests that match a module's text use `web/test-source.mjs` `sourceOf('file.js')` (comments and whitespace removed, `(o)=>` -> `o=>`, a space kept between a word and a quote), so formatting does not break them. Use it for new checks.
> - **Old files:** `deploy/deploy.sh` stays, documented (its header, docs/hosting.md) as the first-install / sandbox profile / plist path; deploy-staged.sh is the everyday deploy. Moved out of the tree (agent scratch, recoverable): `web/.{bc-replay,cmp-bones,p2-longrun,p2-stage-probe,p2-trace-hook,p2-trace-hook2,p2-trace-hook3}.mjs`, the stale `web/dist` build (09-22, 95 MB), `gi.txt`.
> - **Checks:** the full `npm test` 210/210 at the end (the new guard included); full ps2-captures 270 clean after each simulation-touching batch (main.js, cutscenes / free-ride, career-ui + ai-race + compare-ps2-capture); the targeted tests of every batch; Chrome and WebKit smoke (?mute=1): Snow Jam from the event pick to its results by riding, and a CTM free ride from Green Base Station with an in-world Transport to Snow Jam (14 s), no page errors. One regression during the work (an import line removed with its neighbours in minified files: fog-renderer's trackCompiles, the effects' drawOrder) was fixed within the batch window; every batch now runs an import diff against the pre-edit file and an undeclared-identifier scan.
> - **Follow-ups (2026-10-01):**
>   - test:all now also runs the 10 tests that were missing from it: test-attached-core, test-attached-setpieces, test-boost-gauge, test-boost-letters, test-boost-orb, test-rails, test-boost-coil-glow, test-boost-orb-glow, test-title-start and test-ps2-menu-input.
>   - **ps2MenuInput retired** (out of PV_DEFAULTS; other agents' new switches have since brought it back to 40). Removed the port's own pad-menu model: in gamepad-menus.js, MENU_REPEAT, repeatRule and createPortPadMenus; createPadMenus is now a thin wrapper over createPs2PadMenus, and the stick threshold is MENU_REPEAT_PS2.stick. In screen-phases.js, the `rules` option and advance(). ui.js, career-ui.js, audio-menu.js and big-challenges.js take the PS2 branch only. test-gamepad, test-pause-contexts, test-start-rules, test-ps2-menu-input and test-ctm-flow now test the PS2 model only; they pass, and so do test-fe-screens and test-audio-menu. The Chrome menu smoke passes (title -> main -> character -> main, no errors). The WebKit menu smoke could not run: the driver's page reports `visibilityState hidden` and gets 0 rAF frames in 3 s, so the title never takes input. That is the environment, not the code (the same page went through in Chrome); re-run it once the display is awake.
>   - **Removed 25 unused exports** (each grepped in web, tools, docs and the HTML pages first): audio-crowd MIDX_TICK_SECONDS; audio-decode UTK_TABLES, _internal (and its now-unused `td`); audio-menu AUDIO_SCREENS; audio-painters createAudioPainterState, stepAudioPainters, sourceXY; bc-texels bcEntry; buy-attribs RAW_MAX; compile-abort compilesInFlight; diagnostics diagRideStats; gamepad-map STD_AXES, hatValue; glare-pass GLARE_FIELDS; gpu-copies gpuRestoreCount; heap-views heapI32 (and its WeakMap); lodge songCount; quality RENDER_SCALE_LABELS, resetQuality; set-piece-particle-sprites spriteTexture; stage-collect collectedIndexes; terrain-sparkle SPARKLE_FLAG; texture-archive loadedTextureArchives; wardrobe outfitWorn. The doc mentions are updated (web-render-performance.md, ctm-parity.md).
>   - **Kept on purpose:** net/remote-riders setSecondOrder (the only switch for the measured second-order prediction, docs/multiplayer.md); world-material worldMaterialGraph (a documented QA tool); set-piece-particle-sprites KERNEL_ROWS (documents the layout); asset-versions.js (versionedAssets is pending); cutscenes RIDER_BITS and event-return sessionPointRow (CTM work in progress); server/replay-file.mjs REPLAY_FILE_VERSION (records).
>   - **Checks:** node --check, the import diff, the undeclared-identifier scan and test-line-length are clean. These tests pass: audio-decode, audio-menu, audio-engine, audio-sfx, buy-attribs, gamepad, glare-pass, stage-collect, collect-restream, pickup-collection, set-piece-particle-sprites, texture-archive(-decode), wardrobe, diagnostics-bindpose, gpu-recovery, remote-riders, career, career-rider, fe-screens, touch-controls and world-warm. The Chrome smoke is clean: Snow Jam from the event pick to its results by riding, then a CTM free ride with a Transport (arrived in 14.1 s).
> - **Still for later:** the test and tool .mjs files keep their long lines (they are outside the guard's scope).

> **The peak rival in career events: Nate rides the Peak 2 race finals, nobody rides a career slope style; pv careerRival + careerLevel (off, proposed on) (2026-09-30, career-rival agent):** see [career-events.md](career-events.md) "The peak rival in career events", [ai-racers.md](ai-racers.md) "Difficulty by race level".
> - **Decomp:**
>   - Career slope style posts the rival (0x145750) in slot 1 with the leading column, and **no computer rider rides** on any peak. 0x238E20's career round-1 path 0x238F7C sets GMM+0x14 = 5, so 0x239AA0 has one live slot. Only a Single Event rides its last shuffled character (0x239A78), and there the rival never appears. The brief's "career opponent = Nate rides Style Mile" is not the PS2; the port already did this right, so there is no code change for slope style.
>   - Every career race final rides the rival in slot 1: 0x23A108 round 3, career branch 0x23A3D8 (+0x44 = 0x145750). Qualifiers, semis and Single Events never do. A lost heat sets the handler's +0x8 / +0xC (0x23AAE8) so that a Restart reuses the roster.
>   - Separately: the race level sets the riders' +0xDF8 / +0xDFC (0x10C4F8 by slot and level, then 0x10C758's course factor). Checked on all 128 exported countdowns.
> - **PS2 evidence** (derived from ctm-parity peak2-arr, local/career-rival/nav):
>   - career Style Mile heat 1 (Nate posted 358740 / 402400 / 459660; GMM+0x14 = 5, 0x535C04 = 0): characters/career-freestyle/DSS2-heat1-zoe;
>   - career Ruthless Ridge and Intimidator finals, Zoe / Nate / Psymon / Brodi / Griff / Elise at race level 2: characters/career/{CRA3,DRA4}-final-zoe. Path: Give Up, then GMM+0x74 = 3 and handler +0xC = 0 at the results, then Restart.
>   - --ai-state captures (1500 records): local/ps2-capture/runs/careerrival -> local/career-rival/caps.
> - **Gates:**
>   - test-ps2-captures careerrival/cra3-final: human (score too), Nate and the other four, RNG, ranks and pair records all exact to the end.
>   - careerrival/dra4-final: the same, with the human score only to 228. The human's crash there is an attacked bail on the PS2 (+0x12C); the port's enter_crash always passes attacked = false. That is open, not career-specific; a task chip was spawned for it.
>   - Full ps2-captures: 270 clean.
>   - New web/test-career-rival.mjs (in test:all): 0x10C4F8 on 128 documents; the page's assembly of both finals equals the PS2 riders leaf for leaf; the career Style Mile posting replayed exactly.
> - **Code:**
>   - tools/export_lineups.py: export-career records the round and race level; build checks the round / level leaves by rule and prefers same-course provenance; the shared-RNG search bound is raised.
>   - tools/ps2_capture.py: drops an unreferenced dead DEFAULT_3 camera.
>   - web/lineup.js: npcDifficulty, assembleLineup({ level }), careerSkinGated.
>   - web/ai-race.js: heatLevel. web/career.js: ev.raceLevel, a non-saved getter.
>   - web/pv-flags.js: careerRival and careerLevel, both off.
> - **For the coordinator to copy** into web/public/assets (scratch local/career-rival/export, additive: skin nate, skin_scale, grid[1][3f7fffff], career_skins, coverage):
>   - CRA3/lineups.json sha256 2a51d4882f805617...
>   - DRA4/lineups.json sha256 f9502428bee34c69...
>   - Then CAREER_RIVAL_DATA is no longer needed for test-career-rival's final checks.
> - **Page checks** (local/career-rival/qa: server.mjs serves the scratch files; final.mjs runs qualifier -> semi -> final):
>   - Chrome, Ruthless Ridge with the switches on: the final card shows Zoe, Nate, Luther, Viggo, Moby, Mac, and Nate races; at level 2 the riders' DFC are 1 / 0.9095 / 0.7790 / 0.6594 / 0.5170.
>   - WebKit, Intimidator: Zoe, Nate, Psymon, Elise, Viggo, Moby, the same.
> - **Seen on the way, not changed:** the port's final card says "Final" where the PS2 says "Final Round", and it shows the record time 02:13 on both Peak 2 races where the PS2 shows 02:59 on Ruthless Ridge. The tree's set-piece-particles.js / weather-renderer.js threw "drawOrder is not defined" during the QA runs (another agent's edit in progress?).

> **CTM events: nisAfterScan (off, proposed on) and the rollout checks (2026-10-01, CTM events-in-world agent):** see [ctm-parity.md](ctm-parity.md) "The NIS teleport after the section scan" and [ctm-events-in-world.md](ctm-events-in-world.md) "Rollout checks".
> - **pv nisAfterScan:** the lodge door / booth cut fired inside simulate and held the rider before that tick's _section_pass. The PS2 places it at the next update's NIS tick, after the scan. main.js nisHoldAt / nisStart and game-tick.js s.nisStart now defer it to the next tick's start. Chrome and WebKit at the Green lodge door: off, the hold lands before the scan; on, after it. test-nis-after-scan passes; compare-ps2-capture --station-hold (STATION_HOLD_EARLY, SECTION_TRACE). No PS2 capture has the firing tick on a scan tick.
> - **Rollout checks:** in-world vs event-load frames render the same; the remaining differences are rider positions, by design (the Single Event anchor tick). There is no in-world backcountry event, so no beam to compare. Phone memory: in-world peaks lower than event-load (1111 vs 1400 MB in the cycles) but grows about 35 MB per cycle over 8 cycles, outside the wasm heap (R8). In-world WS13 on the page has no gondola and no heatEnter (the card opens at once).
> - **Proposal sent:** desktop first with worldUnderCuts + eventWorldData + eventInWorld + eventInWorldAi + nisSectionPoint; eventReturnInWorld waits for the page's WS13; phones wait for the memory growth.
> - **Bone 22 (pv nisBoneProbe, off):** derived from the NIS keys without a clip export. The cast rider's board_rootg, posed by cutscenes.js from the NIS bank's clip at the rider's model size, is the PS2's bone 22: fr-booth2, within 0.09 cm over 235 ticks. cutscenes.js humanBoard() feeds main.js nisStart -> core nis_hold_probe(2). Open: the page's cut starts about 18 ticks after the hold on a first visit, so the PS2's first miss and the following hit's 111AA0 snow impact are not reproduced. Proposed fix: load the human's cast model and the step before the hold (ctm-parity.md "Bone 22 from the NIS keys").

> **In-world Transport stalls: a crashbag roller on freed collision (core) and the boot rider's dropped held loop (page); the Start block, the avalanche teardown order, the 'loading' hang on a failed init (2026-09-30, Transport-stall agent):** see [course-switch.md](course-switch.md) "In-world Transports that stalled or took the page down".
> - **Core (core2 then core3, both installed by the coordinator; core3 wasm c22ef974…, full suite 268 clean):**
>   - web/roller_gameplay.inc `browser_rollers_track_teardown`: a crashbag roller (raw pointer to its node's sphere tree) outlived the location; pv peakRelease freed the nodes, so the game tick threw "Original roller collider has no sphere tree", then bad_alloc and out-of-bounds traps ("Transport failed").
>   - Now the track's rollers go at the unload start, as on the PS2: web/peak_world.inc set_state(->7) mirrors 230360 -> 3551A8(gp+0x2898, 1 / 8, track) -> 361038 -> 3553C0 -> 34FBF0 flags.
>   - `browser_avalanche_track_reset` moved there too (it ran at 7 -> 0, a tick after the free; no freed read, but 6 ticks late), with 34FBF0's flags.
> - **Page (main.js, no switch):**
>   - `cutsceneRider` takes the character from ui.riders by id. The boot rider object has none, so every rider-masked cut step was dropped, the Transport's held loop among them: the game screen showed while the rows loaded, and a Transport asked for then was dropped.
>   - Start opens no pause while `transporting`. PS2 ARMSX2 run local/transport-stall/ps2/run1: Start in WS14 / WS11 / WS10 does nothing, in WS4 it pauses.
>   - `init().catch` guards getExceptionMessage: a dev server's stale core.js beside a new core.wasm left the page at 'loading' with no error (the "MOUNTAIN autostart sits at loading" report). Fresh servers load MOUNTAIN in 8.9-9.6 s.
> - **Repro (before / after, 18 runs, Chrome + WebKit desktop / phone policy, QA and normal):**
>   - before: R&B's roller outlived R&B every time; the trap came in 1 of 9 runs; Throne -> BRA2 was dropped in 8 of 8.
>   - after: 45 of 45 Transports arrive, 0 core errors; no WebContent crash or reload anywhere.
> - **Also checked:** core-clock in WebKit (phone and desktop): 9 of 9 Transports, remaining steady through the NIS hold.
> - **Tests:** new web/test-crashbag-release.mjs (in test:all); test-ctm-flow / ctm-stream / peak-release / ride-warm / world-warm / lazy-course / presentation / cutscenes / start-rules / avalanche-collision pass.
> - **Open:**
>   - the lodge door cut was not driven;
>   - transportInWorld still returns 'transport' for a dropped request (unreachable now);
>   - the computer-rider contexts (eventInWorldAi) keep their own rollers.
> - **Tools:** scratchpad `tq/` (tq.mjs, nis.mjs, lanes.sh, summ.py, loadcheck.mjs), local/transport-stall/ (builds, capture logs, ps2/).

> **Online course records: the online top 5, Save Records' name entry and upload, the boards, main-menu Leaderboards, Watch Replay (2026-09-30, online records agent):** see [online-records.md](online-records.md). **pv `onlineRecords` ON** (2026-09-30, after the coordinator applied `MP_RECORDS_DIR` on the host: [hosting.md](hosting.md) "Online records"); npm test 208/208 with it on. Without a loaded board (offline from the start) the records screen is the PS2's exactly (test-ctm-left, both switch states).
> - **Decomp:** the records table is 26 slots x 5 at 0x535C18 (defaults 0x43FB28). 0x238358 (each finish, from 125108) calls 0x154AB8 for human players, not during a replay, without Single Event rules (0x14F810 on 0x5308B8), in CTM and Single Event alike, every heat. Times are whole seconds (`cvt.w.s(f32(ticks) x 0x3C888889)`, gp-0x6DEC), and a tie ranks the new run above the old (0x154D58). The record stores the base rider (setup +0x11). There is **no name entry on the PS2**: the record takes the Player Name (0x147170).
> - **Fixed, on for everyone:** career.js `addRecord` used ticks and a strict compare; it now follows 0x154D58 on whole seconds (test-ctm-left: 02:57.50 ties BOMBER's 177 s and goes first).
> - **Built (behind the switch):**
>   - web/online-records.js (data, cache, rank rule);
>   - web/online-records-ui.js (61toptimes online rows, Save Records -> the game keyboard -> upload, the `ctm-board` screen);
>   - web/online-replay.js (the run's replay file, Watch Replay);
>   - web/server/records.mjs + web/server/replay-file.mjs (API, storage, tier-0 checks, rate limits);
>   - hooks in career-ui.js, results-lui.js (third item / rank text / focus row), fe-screens.js (keyboard kind 'record'), fe-main-menu.js + ui.js (sixth row Leaderboards), fe-event-select.js (Leaderboards mode, online top time on INFO), main.js (install, riderStale, replay exit / allowed / afterSwitch 'replay', REPLAY_SCREENS + ctm-board), ai-race.js (`fixedNext` / `prepareFixed`, lineup tables), replay.js (`exportBytes` / `importBytes` / `load`), replay-ui.js (D7 note).
> - **Checked:**
>   - Chrome, web/test-online-records.mjs (in npm test): a Single Event Snow Jam run uploaded through the keyboard; Watch Replay equal to the live run's `?simtrace` on all 15,000+ ticks, after a course switch (1200 ticks) and with another rider selected (600 ticks); the boards; Leaderboards; the offline fallback.
>   - WebKit (web/webkit-driver.mjs, scratch `web/.online-records-webkit.mjs`): the same flow, Watch Replay 2000 ticks exact; screens match Chrome.
>   - Server: web/test-records-server.mjs (in npm test), including a standalone start of web/server/.
>   - Switch off: test-replay, ai-racers, lineups, career, career-rider, ctm-left, fe-screens, fe-attract, mp-gate pass; `vite build --config server/vite.online.config.js` clean.
> - **Open:**
>   - The verifier (D5, second pass) is not built; entries are listed after tier-0 checks only.
>   - In-world CTM events and peak runs keep the local table (D1).
>   - A Give Up's PS2 record entry is not confirmed (online: never submitted).
>   - Single Event keeps the last career run's attribute bytes (existing port rule; the replay carries the bytes, so playback is exact).

> **Outage and guard (2026-09-30, coordinator):** the core3 deploy took the site down (502) for a few minutes: web/server/records.mjs imported ../net/replay-file.js, and the host runs web/server/ on its own, so mp-server.mjs died with ERR_MODULE_NOT_FOUND. Rolled back, the records agent moved the code into web/server/replay-file.mjs and made the records mount lazy and never fatal, then redeployed (core3 live, site 200). **Guard:** deploy/deploy-staged.sh now copies web/server/ alone into a temp dir and starts mp-server.mjs on a free loopback port before staging; if it does not print its 'multiplayer server on' line, nothing deploys. Rule for agents: web/server/* imports only from web/server/ and node:.

> **Live core installed: core3, the avalanche teardown at 5 -> 7 (2026-09-30, coordinator):** web/runtime core.js `7734196ec197ac76…`, core.wasm `c22ef97470deec76…`, from local/transport-stall/core3 (built after core-clock; no core source changed after). browser_avalanche_track_reset runs in peak_world set_state(->7) beside the roller teardown, with 34FBF0's flags on collidable pieces (PS2 230360 -> 3551A8(gp+0x2898, 1, track)); browser_stage_track_reset no longer runs it at 7 -> 0. Full ps2-captures 268 clean; test-crashbag-release passes on the live core.

> **Deployed: core-clock live; pv nisAfterScan ON (2026-09-30, coordinator):** core bd9b55d6… deployed after the Transport-stall agent's WebKit check (9 Transports, phone + desktop, no errors, remaining steady through every hold). nisAfterScan: the lodge door / booth station cut's nis_hold applies at the next simulate's start, after the firing tick's _section_pass (PS2: update U's section pass 0x101B60, then U+1's NIS tick 0x230BE4); Chrome + WebKit checked by the CTM events agent; test-nis-after-scan now in test:all. Also shipping: main.js init().catch guards getExceptionMessage (a core.js / core.wasm mix now shows Load failed, not a silent 'loading'); Start opens no pause during transportInWorld (PS2 run local/transport-stall/ps2/run1: no MCOMM before WS4).

> **Live core installed: core-clock (2026-09-30, coordinator), not deployed yet:** web/runtime core.wasm `bd9b55d6f3e751e6…` (core.js unchanged), from local/ctm-events/core-clock, superseding core-hide. Adds race_bridge gate_go: WS3's 234C68 -> WS4 enter 234E20 selects Race (0x234E30) the tick the countdown reaches 0, so race ticks = total - 179 and every finish / results time is one tick (1/60 s) later than before, as on the PS2. Full ps2-captures 268 clean (kick-doubt-event-tuck now exact to its end). Not modelled: 270280 (streamer busy). Deploy still held on the WebKit Transport check of the NIS-hold rule.

> **CTM events in the world: the WS13 semi is exact end to end; the race clock's GO is one tick earlier (2026-09-30, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) "WS13 through the semi" (the world reset re-hides the Big Challenge markers) and "The race clock's GO: WS3 selects Race".
> - **core-hide (installed by the coordinator):** 230180 purges set-piece bucket 1, which holds the Hide nodes, then its 308C60 always ends with 308DB8, so the Big Challenge markers are hidden again (PS2 c0a-ws13.tick0.p2s: 599560 / 648456 are type-16 nodes, +8 0x210205). ctm_world_reset now ends with browser_mission_world_reset (web/mission_gameplay.inc). heatEnter's node sync into the rider contexts stays. Gate ctm-events/c0a-ws13-semi (compare-ai-capture --ws13 --ticks 16100): human, five riders, RNG, ranks and pair records exact through the qualifier, the grid wait and the whole semi.
> - **core-clock (scratch local/ctm-events/core-clock, wasm bd9b55d6…, core.js unchanged e9478b54…; full ps2-captures 268 scenarios pass):** WS3's update 234C68 runs after the rider manager's tick. Once the clock's countdown is 0 it requests WS4, whose enter 234E20 selects Race (0x234E30), so race tick 1 is counted one tick before the clock's own 0x113D60 select would count it. ARMSX2 call probes c0a-ws13pclk / c0a-ws13pclk2 / c0a-ret13 confirm the order. core race_bridge.cpp gate_go, at the end of race_end after its info, so the GO HUD and sounds keep their tick. Every finish's +0x478 now matches the PS2's (it was one tick short: 1/60 s on the results time). peak3/kick-doubt-event-tuck (a timed event) improves from 3781 to the end; its baseline can be raised.
> - **Comparer / tests:** the HEAT_SYNC_DEBUG diagnostic is gone (event-heat.js, compare-ai-capture.mjs). New gate c0a-ws13-semi (symlinks c0a-ws13-semi.* -> c0a-ws13.* in local/ps2-capture/runs/ctm-events). QA hooks local/ctm-events/qa/finish-elapsed-hook.mjs, finish-all-hook.mjs, bone22-hook.mjs.
> - **Not browser-checked:** core-clock moves the HUD race clock and the results time by one tick (+1/60 s). The GO timing is unchanged, as the monster-* audio gates show.
> - **Bone 22 (open; blocked on data):** the gondola hold does not play semantic 5. Its channel 2 plays semantic 432 (clip 0x5A8E00 from the NIS's own bank: 432 has no variant record), and bone 22 sits 43 cm forward of and 18 cm above +0x110. Semantic 5 posed at the hold transform (new QA export core nis_hold_board_root(semantic, steps), in the tree, no simulation effect) lands about 2 cm from +0x110. Deriving bone 22 needs the NIS rider clips exported and their selection traced ([ctm-parity.md](ctm-parity.md) "Not modelled"). The semi stays exact with the record's bone (HOLD_BONE=record). kick-doubt-event-tuck's gate is raised to the end; [career-events.md](career-events.md) has the race-clock rule and the 270280 gap.

> **Live core installed: core-hide (2026-09-30, coordinator), not deployed yet:** web/runtime core.wasm `38e95006b44e9738…` (core.js unchanged, e9478b54…), from local/ctm-events/core-hide, superseding core-row (b7d43d3d…). Over core2 (the roller teardown): event_row_enter (WS13's 1297C8(C, 1) start rows); NIS holds follow 121818's +0xAC4 rule (112338 skipped, 1125C0 runs, 117C28 skipped), which touches every live Transport / station hold; ctm_world_reset runs 308C60's 308DB8 (browser_mission_world_reset: the Big Challenge markers hidden again); QA exports. Full ps2-captures on that build: 268 clean (CTM events agent). **Deploy held** on a WebKit in-world Transport check of the NIS-hold change (asked of the Transport-stall agent): MOUNTAIN would not load in the driver while the machine was loaded.

> **pv heatRoles ON (2026-09-30, coordinator):** career qualifiers and semis now give the computer riders the round's route roles (0x10C758 -> 0x10C450 by GMM+0 round: qual 1 1 0 0 0, semi 1 1 1 0 0, final 2 2 1 1 0; +0xE04 only slot 1 in round 3) instead of the final's from the Single Event slot tables (web/lineup.js npcRoundRole, ai-race.js heatRound: career mode 0 only, so the backcountry rival events keep today's tables). Evidence: test-lineups' derived qual / semi / final countdowns, the c0a-ws13 semi through heatEnter, ps2-captures 267 with it on (CTM events agent). No gated capture covers a career heat on the event-load page path yet (open coverage item, docs/ai-racers.md).

> **Live core installed: the crashbag roller use-after-free fix (2026-09-30, coordinator):** web/runtime core.js `e9478b5462c66b90…`, core.wasm `690babaafb39e931…`, from local/transport-stall/core2 under the build lock (built from the tree with the CTM events agent's event_row_enter / painter_point_info / rider_control_info exports; no core source changed after it). Fix: web/roller_gameplay.inc browser_rollers_track_teardown at peak_world set_state(->7), as 230360 -> 3551A8(gp+0x2898, 1 / 8, track) -> 361038 -> 3553C0 / 34FBF0 do: a hit crashbag's RollerModifier no longer runs on the sphere tree peakRelease frees (the in-world Transport stalls / page reloads). Full ps2-captures on that build: 267 clean. New gate web/test-crashbag-release.mjs, now in test:all, passes on the live core. Also in the tree: main.js cutsceneRider falls back to the ui.riders entry by id (the boot rider had no character, so rider-masked cut steps dropped). Open: transportInWorld drops a Transport asked for while one runs but returns 'transport'.

> **Live core installed: core-pad (carried presses), and pv padCarry ON (2026-09-30, coordinator):** web/runtime core.js `75ca82f2d349f8aa…`, core.wasm `7ad4dc250849268e…`, from local/ctm-events/core-pad under the build lock. Holds the 1162C8 +0x360 latch for control 0 (every ride), pad_history_sample (0x321298 at every app update), the opt-in 1242B0 NIS-hold re-probe (no live hold opts in). Full suite on that build 202/204 against PV_DEFAULTS with peakSplines / peakAttached on; the two browser failures (ride-warm, fe-texture) pass alone. The tree's build-core.sh adds the QA-only _rider_control_info export after this build (core-pad2); no page code uses it. `npm test -- ps2-captures c0a-ret2-coast` passes on the live core; padCarry flipped after the Chrome / WebKit, keyboard / pad page checks (events agent); input tests (start-rules, gamepad, pause-contexts, ctm-flow, pad-input, touch-controls, frame-clock, replay) pass.

> **CTM events in the world: WS13 through the semi, carried presses, the NIS hold's ground re-probe (2026-09-30, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) "WS13 through the semi" and "Carried presses", [ctm-parity.md](ctm-parity.md) "120F20's re-probe under the hold".
> - **Core (scratch local/ctm-events/core-pad, wasm 7ad4dc25…; full suite running for the coordinator):**
>   - 1162C8's +0x360 jump latch in control 0 (input_bridge.inc): a held jump without a press after control 0 has run is a crouch (1, brake 0), not control 2; cleared on control 0's entry, reset_pad_history, reset_rider, place_rider_region;
>   - pad_history_sample: 0x321298 alone (cSSXApp_preUpdate updates the one history every app update, menus included);
>   - 1242B0, the NIS hold's ground re-probe (all its writes), opt-in per hold (nis_hold_probe 1 = +0x110, 2 = a given board-root bone); only the CTM approach's computer riders run it; npc_fresh_rider sets the constructor's +0x430 -1 / +0x434 0x31 (the semi's push-off at 13C948's halved drive).
> - **Page:** pv padCarry (default off): the menu frames feed the core history, overlay open / close and startRun keep the held keys and the history, a finished ride reads the pad. Checked in Chrome and WebKit, keyboard and pad (local scratch harness padcarry-check.mjs): no jump on resume with a held jump, steer applied on the first resumed tick, no stray jump at GO. ai-racers.js holdTick opts the riders into the re-probe (eventInWorldAi only).
> - **Comparer (compare-ai-capture.mjs):** --ws13's hold follows the record's +0xAC4 and places the human on its grid row at WS1's last tick (the hold had leaked past C2 and cleared the human's start: the "crouch" was this, not carried presses); --pad-carry, --coast-device; HOLD_BONE=record (diagnostic); PAD_TICKS (diagnostic).
> - **Gates:** new ctm-events/c0a-ret2-coast (the Give Up's coast exact through 1987 race ticks with the device pad and one history); c0a-ws13-splines drops --peak-splines (peakSplines is on).
> - **WS13 semi now:** human exact to 1442, riders to 481 / 656 / 671 / 958 / 1245, RNG to 490. **Queue:** bone 22 from the NIS keys (the page's +0xAFC holds), the human's +0x460 after Next heat, Elise's path pick at 479, the grid-wait rank, NIS-before-scan on the live arrivals, the +0x470 start after a Give Up.

> **Live: pv peakSplines + peakAttached ON, with the re-split streamed packages (2026-09-30, coordinator):** the 42 changed + 4 new files of local/peak-splines-qa/ship-files.txt (from local/ctm-fix/export) copied into web/public/assets with new inodes and verified byte-equal, both switches flipped in PV_DEFAULTS, deployed together (46 game-data files, edge generation 36). Evidence: the peakSplines QA agent's matrix (set-pieces.md, last section) and the full ps2-captures suite with PEAK_SPLINES=1 (266). **Never ship apart:** switch on with the old packages (moving collision drawn at rest: R&B train, ESS3 sled, CRA3 osprey / blimp, ABC1 tumbler; rocket sparks with no body), or peakSplines without peakAttached. A rollback (deploy-staged.sh --rollback) swaps code and data together. Open: the eagle / ravens relaunch cadence against PS2 video; in-world Transports that stall or reload the page in QA mode (MOUNTAIN to ABA1 / Throne / BRA2, either data), not investigated.

> **peakSplines rendering QA: ship C + peakAttached with the 42 + 4 files in one deploy; B and C-alone must never ship (2026-09-30, peakSplines QA agent):** see [set-pieces.md](set-pieces.md) "Streamed worlds: page QA of peakSplines, peakAttached and the re-split packages".
> - **Matrix** (Chrome --mute-audio and WebKit, frozen frame clock, same ticks; 5 peak-world locations + MOUNTAIN at 2): A2 = A, B = A and D0 = A (new packages minus env / lighting) to 0 px before any launch; nothing vanishes or doubles.
> - **The new packages also bring the env pass and lit instances** the live streamed packages predate; the streamed frames then match the event packages at the same camera (R&B stand glass, Ruthless Ridge billboard, Perpendiculous stadium).
> - **Flybys** draw on their splines only with C + peakAttached (ravens, eagles, osprey, blimp, cessnas, tumbler); C alone moves the train, sled and dragonworks.
> - **Draw / collision:** the R&B train cars, the ESS3 sled, the CRA3 osprey / blimp and the ABC1 tumbler have collision; under B their draw stays at rest, under C without peakAttached the last three do. Today (A) a hit crashbag already moves in the core while drawn static; the new packages fix it. The Intimidator 0.22 m under B is the launch's shared-RNG draw, not a contact.
> - **Memory / load** (WebKit phone policy): A vs C + peakAttached within run-to-run noise (MOUNTAIN peak 1057-1135 vs 1096-1118 MB, steady ~610 vs ~620); +1.95 MB on the wire; ready 8.2-8.7 s alike.
> - **Files:** local/peak-splines-qa/ship-files.txt (from local/ctm-fix/export). PV_DEFAULTS not changed.
> - **Open:** the Ruthless Ridge eagle / R&B ravens relaunch every 40 / 120 ticks near the rider (not compared with a PS2 sequence); some QA-mode in-world Transports stall or reload (either data).

> **Live core installed: core-suite4, the celebration clip fix (2026-09-30, coordinator):** web/runtime core.wasm `07d1a7b7baf44f04…` (core.js unchanged, 40f4d369…), from local/ctm-events/core-suite4 under the build lock. Adds to core-suite3: finish_step (web/finish_gameplay.inc) opens ControllerDraws, so control 10's 115B58 variant draw for 315 is a controller-phase draw as on the PS2 (12C678 in 121068). Full ps2-captures on that build: 266 clean; `npm test -- ps2-captures c0a-ws13` passes on the live core. pv peakSplines stays off until the rendering check of switch × packages (local/ctm-fix/export) reports.

> **CTM events in the world, WS13 groundwork: a player's full qualifier on the PS2 and four live-game parity fixes it exposed; peakSplines proposed on (2026-09-30, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) stage 5 "The player's qualifier (c0a-ws13)", [crash-motion.md](crash-motion.md), [set-pieces.md](set-pieces.md) "Streamed worlds".
> - **Capture:** `local/ctm-events/caps/c0a-ws13` (local/ctm-events/ws13_capture.py build / run / pads; 16,043 --ai-state records): the Snow Jam arrival, the gate, the card, the race ridden closed-loop by ps2_autopilot's Pilot to the human's own 3rd place (no pokes), the results' Next heat by the device pad, WS13, the semi card, 1,500 semi records. 0 pad misses; open-loop replays of its pads reproduce it (probes: probe_9d0.py, probe_contact.py, probe_entry.py). PS2_CAPTURE_SCALAR=0.6 on a loaded host.
> - **Core fixes (no switch; core-suite4, full suite 266 clean, also with PEAK_SPLINES=1):**
>   - control 8's enter 12CA30 adds the whole rider+0x9D0 (the tick's 106538 translations, instance pushes included) to the posed board before 136D40 detaches it (Allegra 5940);
>   - 131620 runs 115B58 / 115D48 before its 114CC0 reverse turn (Allegra 9368);
>   - 342E98's restore-3 RestoreNode is listed, so its section leave restores the trigger (dragontrig_1000 re-fired by Griff at 11944);
>   - control 10's update (finish_step) draws in the controller phase: the celebration's 315 variant (0x311710) took the motion cursor's word (clip 6400 for the PS2's 6144);
>   - shared world event 10 (Spline piece released in the human's streamed context; the other contexts drop the piece and its builtin52 guard; ai-racers.js forgets the owner key) -- logged only with peakSplines on.
> - **Gates:** `ctm-events/c0a-ws13` (to race tick 11864, peakSplines off), `ctm-events/c0a-ws13-splines` (compare-ai-capture --peak-splines, to the live stop: human, five riders, RNG, ranks and pair records exact on all 12,382 race ticks).
> - **Proposal:** pv peakSplines on (its captures and c0a-ws13 pass with it; without it c0a-ws13 loses the RNG at 11865, the EZrocketCore draw).
> - **Next:** WS13's own path (results replay, 2706F0, 235AA0 / 235CC8, the semi's riders, WS1 arg 3, the card, the semi) in the comparer and the page.

> **Live core installed: core-suite3, the trigger RestoreNode fix (2026-09-30, coordinator):** web/runtime core.wasm `3efe9e62ab291901…` (core.js unchanged, 40f4d369…), from local/ctm-events/core-suite3 under the build lock, new inode. Adds to the qualifier fixes: the RestoreNode made by 342E98's restore 3 is section-listed, so its section leave restores the trigger (every world); shared world event 10 (the Spline guard release, logged only with peakSplines on; web/ai-racers.js drops the knownTriggers key on it). Full ps2-captures on that build: 266 clean (peakSplines off); `npm test -- ps2-captures c0a-ws13` (c0a-ws13 and c0a-ws13-splines) passes on the live core.

> **Live core installed: the qualifier fixes (2026-09-30, coordinator):** web/runtime core.wasm `0764568541ac1a12…` (core.js unchanged, 40f4d369…), from local/ctm-events/core-suite2 under the build lock, new inodes. Core-only fixes, no switch, reaching every race: control 8's enter (12CA30) poses the detached board with the whole rider+0x9D0 (instance / pair / contact / landing pushes) before 136D40; 115B58 / 115D48 before the 114CC0 reverse turn in control 0. Full ps2-captures on that build: 265 clean (the crash captures and the new ctm-events/c0a-ws13 qualifier gate); `npm test -- ps2-captures c0a-ws13` passes on the live core. Not deployed yet.

> **Live core installed: the events core with the in-world replay snapshots (2026-09-30, coordinator):** web/runtime core.js `40f4d3698f4296d5…`, core.wasm `2842f2cf26e331d7…` (was c894e9b5… / 99c09586…), from local/ctm-events/core-final under the build lock, new inodes. No core source changed after that build; every live export is still there except `_rider_paused_pass`, renamed `_rider_pose_step` (no page code uses the old name); every export the switch path needs is present. The snapshot calls (web/event-snapshot.js) only run behind pv eventReturnInWorld (off). Full ps2-captures ran on that exact build (264 clean); `npm test -- ps2-captures c0a` passes on the live core. Not deployed yet.

> **CTM events in the world, stage 5 (b): the in-world replay behind the results and the Transport's results-time restore, with the PS2's two snapshots; one core for both (the gate and (b)); eventReturnInWorld stays off (2026-09-30, CTM events-in-world agent):** see [replay.md](replay.md) §2a and [ctm-events-in-world.md](ctm-events-in-world.md) stage 5.
> - **The rider-context snapshot** (web/world_snapshot.hpp, web/rider_context.cpp): a context's TLS block copied, its containers deep-copied into holders made at the first save; the registry of all 983 file-scope RIDER_LOCAL comes from web/generate-snapshot-registry.mjs in the build, and web/check-snapshot-registry.mjs (in the build) fails on any TLS variable it does not cover, saying what to do. **Now compiled into every core** (web/build-core.sh defines SSX_SNAPSHOT_REGISTRY).
> - **Policy** (web/snapshot-policy.mjs, reasons beside each): the default is snapshot; KEEP (static tables, hash-checked at every restore under QA; checked on c0a-ret3, eba3-rock-hit, setpieces/full, allpeak/apr-start, fr-throne-unload, dss2-full, and in the suite's keepCheck scenarios), CURRENT (not in the PS2 snapshot: the visual stream / LCG, the stream's event queue, the drawn frame's skinning), REDERIVED, OWN. Hooks: the collision world's run-time state, the stage VM's tables, the avalanche through the PS2's own 0x2D9CB0 / 0x2D9D68 (code-ported, not capture-verified), the painter trees' views and 0x2C03E8's painter reset.
> - **Page** (main.js, behind eventReturnInWorld): the in-world results replay (countdown snapshot at the live start, results-time snapshot at the replay's first start, restored at Transport). web/event-snapshot.js is shared with compare-ai-capture.mjs.
> - **Memory:** the results-time copy is 4.05 MB on the page (mallinfo); the countdown save overwrites slot 1 and the event's end drops the rider contexts' copies (their references kept the last event's rider data alive: a one-time 184 -> 221 MB peak without it). Chrome and WebKit (phone policy), 3 in-world events with the replay: WASM 184 MB flat, as the replay-off baseline.
> - **Gates:** REPLAY_PROBE every tick equal after the countdown restore (riders, shared RNG; conditions: no avalanche playing and no painter mid-blend at the restore); ctm-events/c0a-ret3 now runs --replay-return (results-time snapshot, 600 replayed ticks, Transport restore) and passes; full ps2-captures on the handed core. Page: Chrome (--mute-audio) and WebKit (webkit-driver, phone policy, 3 in-world events): the replay behind the results, the six at Session point 1, the removal after 8 ticks.
> - **Also changed in shared code** (covered by the full suite): step_rider's wasAirControl0 moved to file scope (core.cpp); engine/stage_script_vm.hpp gains RuntimeSnapshot / programCount; ai-racers.js saveState / restoreState; ai-race.js replayRestore; free-ride.js bankFor / sessionReturned.
> - **Place HUD during the return (settled from the code, 2026-09-30):** 0x1EA930 adds the place bit 0x1 to free ride's HUD word 0x1530C380 while two or more riders are listed (0x1EAA18..0x1EAA3C); the value is +0xEC (1ST from 128A48 at tick 3). The page's 1ST/6 matches; no change.
> - **Open:** the coast after a Give Up (carried presses); the 149A88 caller; WS13.
> - **Tools:** local/ctm-events/qa/scratch-server.mjs (the page with a scratch core, no file watching), event6.mjs (the in-world return driven on the page), probe_entry.py; headless Chrome needs the display awake (caffeinate -d -u) or its frames stop.

> **Live core installed: the WS15-gate core (2026-09-30, coordinator):** web/runtime core.js `c894e9b5dd1c3842…`, core.wasm `99c0958656063da4…` (was 991a2184…), copied byte-identical from local/ctm-events/core under the build lock, new inodes. No core source changed after that build, every live export is still there, and no page code calls the removed `reset_stance_seed` / `boost_meter_clear` / `npc_stats_seed`. Full ps2-captures ran on that exact build (264 clean); `npm test -- ps2-captures c0a` passes again on the live core. The new exports are used only behind pv eventReturnInWorld (off).

> **CTM events in the world, stage 5: the WS15 gate passes with the page's own return; core to install; eventReturnInWorld stays off until (b) (2026-09-30, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) stage 5 "The WS15 gate".
> - **Gate:** `ctm-events/c0a-ret3` (test-ps2-captures.mjs, `returnGate`): all six exact from WS15's record to the WS4 removal, the human on all 729 records to the capture's end, RNG / ranks exact, pair records from the return. Full ps2-captures on scratch core `local/ctm-events/core` (core.js c894e9b5..., core.wasm 99c09586...): 264 scenarios clean.
> - **One code path:** web/event-return.js (`transportMapEnter`, `sessionReturn`, `sessionRidersLeave`) is what compare-ai-capture.mjs --ctm-full and main.js (cb.freeRide's in-world branch, eventInWorldEnd({session}), worldAiEnd, new `returnRun`) both call. No seeds: the old reset_stance_seed / boost_meter_clear / npc_stats_seed exports are gone.
> - **New core exports (no values):** `rider_setup_player_reset` (149A88's slot-1 player setup: stance, size, stat getters 0.5), `race_world_player_setup(slot)` (its pair inputs: weight 65, stats 0.5), `location_entry_place` / `transport_map_enter` (112180 + 11DE60 / WS14's enter), `race_world_rank_mode` (128A48), `rider_peers_restart(loaded)` (10F3B8 on a rider's own 115D48 view), `rider_pose_step` (129160, the rider manager's pose pass that WS1's exit runs after the removal: one full-rate animation step).
> - **Found by probe (ARMSX2, derived states):** the removal frame gives the human one extra full-rate animation step without physics (c0a-ret10 / c0a-ret11). **Correction:** it is *not* 128AF0's paused branch (0x128CB8 -> 120ED8): probe c0a-ret12 logged no call to it from the pause menu through the removal; the caller is WS1's exit 0x234750 -> 129160 (0x23488C: 11EB60(rider, 1.0) + 11EB98 + 3103F0 per listed rider), port `rider_pose_step`. The removed riders' pair records go off at the removal (12B030 = 12AE38 + 10F3B8), else 115D48 reacts to them (the old "RNG at tick 68").
> - **Tools:** local/ctm-events/probe_entry.py (function-entry hooks, FILTER / LOG_F12 / LOG_REG), probe_calls.py per-site modes; capture_card.py reads the probe's own log size.
> - **Page:** loads clean in Chrome (--mute-audio) and WebKit (webkit-driver) with the switches off and on. The in-world return itself has not been driven through the page's results -> map -> confirm yet.
> - **Next:** (b) the replay behind in-world results and the Transport's re-run to the results-time frame (required before eventReturnInWorld goes on); then the carried-presses item (the coast after the Give Up, race ticks 1700..1987, is from the tick script and not gated), WS13, the WebKit trails re-probe.

> **CTM events in the world, stage 5 (WS15 return) in progress: a player-faithful capture, the WS14 / WS15 rider rules from probes; gate not yet passed, eventReturnInWorld stays off (2026-09-30, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) stage 5 and [replay.md](replay.md) "What 0x2706F0 restores".
> - **c0a-ret was an artifact:** its command-4 poke skipped stopAutoReplay 0x2706F0, so its riders were the auto replay's frame. `c0a-ret3` (local/ctm-events/caps) is a player's return: a real results Cross on Transport, the real map Crosses, records through the Give Up's 289 coast ticks (finish + 288), map savestate C+8 = 1990 (the stop frame and the WS14 frame tick once each).
> - **Capture tool fix:** menu_pad.py's pad-hook switch was ps2_capture's F_SCORE word (DATA+0xF0), rewritten every record. It now has its own word 0x9C8F0 (asserted, and reserved in ps2_capture.py). Captures affected: c0a-ret2, the ret2diag runs; c0a-ret3's coast came from the tick script. No ps2-captures scenario uses any of them.
> - **New probe tools:** local/ctm-events/probe_calls.py (jal sites -> logging stubs, t4..t9 only), route_hook.py, fill_manifest.py; capture_card.py RECORDS_OFF_AT_REPLAY / TRANSPORT_ITEM / PROBE / PROBE_AT_SAMPLE / CAPTURE_STOP_AT_SAMPLE.
> - **Rules ported (scratch core only; full ps2-captures on f6e8d28d...: 263 clean):**
>   - place_rider_region ends a crash (11DE60's 11FEC8(0) / 11FE78(0)) and a finished rider's control 10;
>   - race_world_pair_restart (10F3B8), reset_stance_seed, start_row_reattach (112180), boost_meter_clear (WS14 enter 0x2362B4).
>   - ai-racers.js resume: pair restart, every rider's game tick to 0 (1297C8, 0x1297F0).
>   - compare-ai-capture.mjs: --ctm-full through the Give Up, coast, Transport ticks, WS14 grid hold, WS15 (all six) and the WS4 removal; --coast-only.
> - **Page (behind eventReturnInWorld, off):** game-tick.js liveStopAt (in-world live ticks end at finish + 288 / + 408), the Transport's 2 ticks in cb.freeRide. The page's WS15 placement sequence (the WS14 grid hold, 112180, the stance rule, no rider reset of the human) is **not** ported to main.js yet.
> - **Open for the gate:** Psymon's (setup slot 1 = Zoe) tick-0 speed limit (port 2299.30 against PS2 2301.00); the human's coast from the Give Up tick (the pause menu's Yes Cross is still in the shared pad history on resume: crouch without an edge, the port can't show it yet, separate item); the 149A88 caller at WS15 (indirect). Then (b): the replay behind in-world results with a re-run to the results-time frame at Transport.

> **gcWatchdog never holds its kick memories across a core instantiation (2026-09-29, coordinator):** web/gc-watchdog.js gains `state.release()`, and main.js newCore calls `window.__gcWatchdog?.release?.()` before instantiating. JSC has few wasm fast-memory slots (3 without the large gigacage, i.e. iPhones). A kick held during a load (the load screen counts as a safe moment) could have left the new course core on a bounds-checked memory for the session. Dropped, unreferenced kick memories are fine: with no slot left, BufferMemoryManager runs a synchronous full collection and reclaims them. pv switchGC already drops its memories before instantiating. Still unverified on iPhone hardware: that the course core gets a fast memory after a kick (check `WebAssembly.Memory` mode / wasm perf on a device). switchGC stays OFF until then (docs/mobile.md "Load spikes").

> **CTM events in the world, stage 4 milestone 4: the whole run from the Snow Jam arrival exact with six riders; pv nisSectionPoint (off) (2026-09-29, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) §4 stage 4 "Milestone 4".
> - **Result:** `ctm-events/c0a-full-ai` (compare-ai-capture.mjs --ctm-full): human, 5 riders, RNG, ranks and pair records exact through all 1666 race ticks, from free ride through the fly-over, approach, card and race.
> - **NIS director's section point** (0x281370 / 0x281100 / 0x281400; savestates c0a-snap / c0b-snap): the outer camera's eye becomes a second activation point while a script plays.
>   - Core `section_point`; engine Activation points.
>   - cutscenes.js feeds it, gated by pv eventInWorldAi from the gate, or by new pv `nisSectionPoint` (off) for every streamed-world NIS. No existing gate moves.
> - **Riders under WS1:** fresh (129E20), held at their approach actors, grid-placed keeping +0x2E4 etc.
>   - Core `npc_fresh_rider`, `npc_grid_start`.
>   - ai-racers.js `holdTick` / `start({gridStart})`; cutscenes.js `onRaceActors`; main.js `worldAiTick` via a new game-tick.js hook.
> - **Remaining (no position effect):** +0x380 under the NIS hold is not written by the port's hold branch (core.cpp).
> - **Captures:** `local/ctm-events/caps/c0a-full-ai.*` (capture_card.py --ai-dynamic), `c0a-snap.tick*.p2s`, `c0b-snap.tick*.p2s` (capture_card.py --snap).

> **Load spikes on WebKit: attributed; two plain fixes free the old and boot courses' cores; pv `switchGC` and `loadCopies` (off) (2026-09-29, load-spike agent):** see [mobile.md](mobile.md) "Load spikes" (tools, attribution, tables) and [course-switch.md](course-switch.md).
> - **Two heap policies:** this Mac has 64 GB, so JavaScriptCore runs its "Aggressive" growth (about 3.9x the live heap before a full collection). A phone runs the default (2x / 1.5x). `__XPC_JSC_forceRAMSize=6442450944` on the driver gives the phone policy (JSC options reach WebContent as `__XPC_JSC_*`).
> - **Attribution:**
>   - The "30 s into the world ride" spike is the booth map's per-frame LUI garbage (lui-player.js, ctm-map.js; ~47 MB/s in Chrome, +100-110 MB/s WebKit Malloc) under the Mac policy. It is not a load, and it did not spike under the phone policy.
>   - The event-load spikes were old cores still resident (3 cores, WebAssembly Memory 371-429 MB).
>   - After every load no full collection runs for 20-40 s, so the load's garbage stays resident.
>   - Fetch bodies were held 3-4 times over while a load read them.
> - **Plain fixes (web/main.js):**
>   - `acrossBefore` (pv switchGate) is pruned to what is still in the scene right after unloadCourse (it kept the old course, and its core, through the whole next load). unloadCourse drops `worldRewarm`.
>   - `ui.cb.standings` / `ui.cb.lineup` are module-level functions. As closures made inside loadCourse (which the streamed world never replaced), in JavaScriptCore they kept the menu boot course's whole load scope, its 128 MB core and ~130 MB of its load, through the first career ride (every run; FinalizationRegistry counting; JIT off too). This settles the runaway agent's open item: real, not a probe artifact. After the fix it is freed at the first full collection (menu -> career and menu -> Peak 2 Race, both policies, 4 of 4).
> - **pv `switchGC`** (off; new web/switch-gc.js, test-switch-gc.mjs in test:all; JavaScriptCore only): the gc-watchdog's one-page WebAssembly.Memory kick.
>   - Before a new core, while an earlier core's memory is alive (FinalizationRegistry), until it is freed or a full collection has passed.
>   - Once more under the load screen before `ready`.
>   - Cost: 150-250 ms plus 50-70 ms per switch.
> - **pv `loadCopies`** (off; web/downloads.js, test-downloads.mjs section 11): one buffer per body of known size, and each reader's one copy from the shared bytes (json / text decode them). Its runs had the menu boot course's load 0.9-1.3 s longer (4 of 4, not understood).
> - **Results** (career shape, 2 cycles, WebContent lifetime peak MB):
>   - phone policy: before 986 / 1633; fixes 1120-1688 (median 1266); + switchGC 1014-1229; + switchGC, loadCopies 983-1225.
>   - Mac policy: before 2242; fixes 1231-1370 (one 2845: a full-GC stall after the load); with the switches 1418-1487 at the menu boot, and one 3090 (a 20 s world load with the Transport map's LUI garbage).
>   - Single loads, phone policy: menu boot 1030-1140, All Peak Race direct boot 1008-1058, BRA2 direct boot 1108-1200. Mac policy: 1135-1580.
>   - A whole All Peak Race, phone policy, both switches on: p50 689 / p95 821 / max 971 MB, lifetime peak 988 MB, results reached.
> - **Recommendation:** switchGC on after a check on iOS hardware (same fast-memory kick as gcWatchdog). loadCopies stays off.
> - **Measuring pitfall:** `WeakRef.deref()` polled during a concurrent collection keeps its target for that cycle. Count cores with a FinalizationRegistry; the loadpeak RECORDER's `cores` did deref.
> - **Open:**
>   - the LUI player's per-frame garbage (map and load screens; the Mac-policy spikes);
>   - the first career load's own transient (phone policy 1.19-1.25 GB);
>   - switchGC on iOS hardware.
> - **Checks:** node --check; test-downloads, test-switch-gc (new), test-lazy-course and test-ride-warm (headless Chrome) pass. The capture gates are not affected: page-side JS only, the core is unchanged.
> - **Tools:** `local/browser-validation/load-spikes/` (probe, run / bootcore / chralloc / sum / tl, the probe and bisect vite configs).

> **CTM events in the world, stage 4 milestone 3: both heats natively exact with six riders; the whole run from the arrival captured and compared (2026-09-29, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) §4 stage 4.
> - **The 461 was the collect path.** In CTM (0x535C11 = 0) the uncollected collectibles are listed and their slot-1 programs draw. `compare-ai-capture.mjs --in-world-ai` sets it, with the mask derived from the countdown savestate's DeadNode collectibles through the new core export `stage_collection_list` (index = 30C4A8's list position, 0x30C510..0x30C548).
>   - `ctm-events/c0a-race-riders` / `c0c-race-riders`: human, 5 riders, RNG, ranks and pair records exact to the end. c0c needs a core with `stage_collection_list` and is skipped before that.
> - **Whole run:** `local/ctm-events/caps/c0a-full-ai` (capture_card.py `--ai-dynamic`) and `compare-ai-capture.mjs --ctm-full ARA1` (web/ctm-in-world-setup.mjs), pending gate `ctm-events/c0a-full-ai`.
>   - The riders ride from the countdown's tick 0 (`anchorTick: 0`).
>   - 129768's list clear at the Continue: new core `section_restart`, called from startRun.
>   - Remaining: the PS2's NIS director adds the active camera's point to the section activation under the fly-over (0x281370 -> 0x1033B0, 0x281100). The port scans around the rider only: RNG 4 draws behind from free-ride tick 2854, riders 185..438.
> - **Browser:** Chrome and WebKit race cleanly on live bfff10bb with the new startRun order.
> - **Files:** main.js, ai-racers.js / ai-race.js (anchorTick), compare-ai-capture.mjs, ps2-capture-ai.mjs (the tick-0 restart of an `ai_dynamic` capture), test-ps2-captures.mjs; core section_gameplay.inc (`section_restart`), stage_script_gameplay.inc (`stage_collection_list`), both scratch; local/ctm-events/capture_card.py.

> **CTM events in the world: the in-world race start is exact (c0a-full through race tick 1507); core event_grid_start keeps the rider and the world (2026-09-29, physics agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) stage 3 "The in-world race start".
> - **Live core** core.wasm bfff10bb0a642cb7… / core.js ff6e42f6744cac9c…. Full ps2-captures on the same build: every gate at its baseline or better (the events agent's new c0a-race-riders / c0c-race-riders pass on it).
> - **Cause (traced):**
>   - The CTM countdown's rider equals the Single Event anchor except the words WS1's hold and 11D390 leave (rider block diffed): the motion-0 stamps (owner +0x10 / +0x14), the boost words, +0x380 / +0x390, ...
>   - 13C7A8 scales the push-off velocity by 0.7 + 0.01 × (tick − leave − 40). The game tick restarts at the Continue, so a CTM start keeps 0.7. The anchor's leave 0 gave c0a-race 182's 160 cm/s.
>   - 308DB8 (the WScript missions' +0x4C programs, through the stage VM 2227D0, Hide 2FFB50 → 350E90) hides every Big Challenge marker at free ride's first tick (entry probe). The in-world event start reset the stage world (browser_reset_pickups), so the port hit flg_ARA1_BigCFlag_1002 at race tick 829.
> - **Core:**
>   - `event_grid_start(x, y, z, heading)`: reset_race + reset_rider + start_event with the stage world kept (`browserEventWorldKept`). It keeps the stamps, boost words, +0x380 / +0x390 and +0x2E4, and sets grounded (a streamed world's reset_rider snap misses).
>   - `event_route_seed(docText)`: the grid route from the event document.
>   - `init_race` keeps a running session's total tick count.
>   - QA exports: `ground_state_seed`, `ground_tick_seed`, `stage_instance_flags`.
> - **Page** (events agent, wired): event_route_seed, _reset_pad_history, event_grid_start; no _reset_race first (it would reset the world).
> - **Comparers:**
>   - web/ctm-in-world-setup.mjs: the arrival seeds and the in-world gate / hold / start, shared by compare-ps2-capture.mjs `--ctm-in-world CODE` and the events agent's compare-ai-capture.mjs `--in-world-ai`.
>   - `--ctm-countdown` (both comparers): the kept words from a countdown savestate's record 0.
>   - compare-ai-capture.mjs also seeds the career cash (HUD slot 0x19).
>   - The +0x2E4 gate is by row (the in-world tick restarts).
> - **Gates:**
>   - `ctm-events/c0a-full` (new, runs/ctm-events link): exact through race tick 1507. 1508 is a PS2 soft collision with no instance contact: a computer rider, stage 4.
>   - `ctm-events/c0a-race` / `c0c-race` (human, now gated): exact through 309 / 318. The idle upper reaction looks for computer riders.
>   - Six-rider (pending): the human is exact to the end; the RNG leaves at 461.
> - **Open:** c0a-full's score from the gate (12B180 commit, the event HUD bank's stale words; gated through 2821). engine/riding.hpp keeps its own start.

> **CTM events in the world, stage 4 milestone 1: pv `eventInWorldAi` (off, QA), the Snow Jam qualifier with its 5 computer riders in the streamed world (2026-09-29, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) §4 stage 4 "Built".
> - **Key fact:** each event package's collision, terrain and rails are exactly its resident locations' (all 17 events, same order and tracks). So each rider context loads the event package in parts (`event_world_bodies_only`: the shared camera terrain stays the streamed world's), and the others copy it by key.
> - **Core (scratch, not live):**
>   - `world_event_apply` kind 9, node state: the human's DeadNode / Hide / flag changes reach the contexts;
>   - `world_node_states[_apply]`: the start sync.
> - **Page:** ctm-event.js / main.js make the riders under the fly-over and attach them in `cb.eventInWorld`. The contexts are reused through a pool (no destroy). The startRun in-world start uses the physics agent's `event_route_seed` + `event_grid_start`.
> - **Approximation (unconfirmed):** the CTM collectible magnets live in the human's context only (see the doc).
> - **Tests:** `web/test-ctm-event-ai.mjs` (new, in test:all). QA `local/ctm-events/qa/event4.mjs`: Chrome and WebKit race 40 s clean.
> - **Six riders (live db807fea):** `compare-ai-capture.mjs --in-world-ai --node-seed` (the page's rider path) scores the same as the event package. With the capture's section draws (`--no-sections`) all five riders and the RNG are exact to the end in both heats: new gates `ctm-events/c0a-race-riders` / `c0c-race-riders`. The remaining 461 is the human's section pass (3 x 0x341bbc at tick 460 on the PS2), not the contexts.
> - **startRun (pv eventInWorld):** no `_reset_race` before `event_grid_start` (the physics agent's order: an offline start keeps the stage world). It needs the physics agent's next core.

> **pv gcWatchdog ON (2026-09-29, coordinator):** the WebKit full-GC stall watchdog (web/gc-watchdog.js; docs/mobile.md "The WebKit memory runaway") is switched on after the runaway agent's report. No-op while healthy (26 game runs, 0 kicks); a rescue costs one frame of up to about 60 ms. The stall is a JSC bug seen on the 64 GB Mac's Aggressive heap policy (about 8 / 43 menu -> MOUNTAIN runs), which is Owen's desktop Safari; with the phone policy it didn't occur (0 / 34). The WebKit bug report draft is in docs/mobile.md. ~~Open: the menu flow's boot-course core (128 MB) stays alive in WebKit through its 5 computer-rider groups~~ Settled by the load-spike agent (entry above): two loadCourse closures (`ui.cb.standings` / `lineup`) held it; fixed.

> **The WebKit memory runaway is JavaScriptCore no longer running full collections; watchdog behind pv `gcWatchdog` (off); main.js keeps no old scene / core after a switch (2026-09-29, WebKit runaway agent):** see [mobile.md](mobile.md) "The WebKit memory runaway".
> - **Cause (JSC, not our code):**
>   - The full-GC timer's delay is lastFullGCLength / timeSlice(bytes x deathRate). deathRate is 0 when a full collection ends with sizeAfter >= sizeBefore.
>   - Extra memory allocated during a concurrent full collection counts as visited, but the cycle's allocation counter is reset. So a load that allocates big buffers during a full collection can leave deathRate 0 and the timer never armed again.
>   - After that only Eden collections run, and everything a full collection frees piles up: WebKit Malloc, dead cores (the "WebAssembly Memory" / JS VM Reservations growth), jettisoned JIT code (the 512 MB pool, then LLInt), GPU / audio wrappers.
>   - Detector: an in-page FinalizationRegistry on old-generation markers. Healthy WebKit frees them every 3-7 s. Every runaway frees none after the load.
> - **Where:** this 64 GB Mac's JSC policy (Aggressive growth):
>   - menu -> MOUNTAIN, ~8 of 43 runs;
>   - menu -> Peak 2 Race, 1 of 6. The audio agent's "DRA4 crossing" runaway is the same stall: it starts at the load and only shows at DRA4's streaming.
>   - Under the phone policy (`__XPC_JSC_forceRAMSize=6442450944`): 0 of 24 game runs and 0 of 10 repro runs. iPhone hardware not checked.
> - **Recovery:** 1-4 one-page `new WebAssembly.Memory({initial: 1})` held together. JSC's BufferMemoryManager requests a full collection once half its fast-memory slots are in use.
>   - On a live runaway: footprint 1589 -> 927 MB, or 1251 -> 838 MB. The longest frame was 62 ms, once.
>   - 1 GB / 4 GB ArrayBuffers do nothing.
> - **pv `gcWatchdog` (off, `web/gc-watchdog.js`, one line in main.js after installYieldShim, JSC only; QA `window.__gcWatchdog`):**
>   - It kicks when an old marker has waited 40 s while young markers are still freed. 40 s is over twice the longest healthy gap measured: 17 s on the Mac, 13 s under the phone policy.
>   - The kick is one held one-page memory a second until a full collection happens, at most 6. It waits up to 5 s for a load screen / pause / cutscene, with 30 s between episodes.
>   - Results with it on: 26 game runs with 0 kicks and 0 runaways; minimal repro page: 1 stall, recovered 3 s after the kick with a 21 ms longest frame.
>   - New `web/test-gc-watchdog.mjs` in test:all.
> - **main.js (plain fix):** `acrossBefore` (pv switchGate) and `loadBefore` were never cleared. They kept the previous course's scene and, through its closures, its core. Now cleared at `live=true`.
> - ~~**Open (WebKit):** the menu flow's boot-course core (128 MB) stays alive for the whole ride through its 5 computer-rider groups.~~ Settled (load-spike agent): the holder was `ui.cb.standings` / `ui.cb.lineup`, closures made inside the boot course's loadCourse; fixed in main.js.
>   - Not reachable from window, not pending GPU promises, not listeners, not three's caches, not the audio leak.
>   - Next step: a Web Inspector heap snapshot.
> - **WebKit bug report draft** (mechanism, minimal repro `rw/reprosrv.mjs`, both policies) is in mobile.md, for Owen to file.
> - **Files:** web/gc-watchdog.js (new), web/test-gc-watchdog.mjs (new), web/main.js, web/pv-flags.js, web/package.json, docs/mobile.md.
> - **Tests:** test-gc-watchdog, test-lazy-course, test-ride-warm pass.
> - **Scratch:** `rw/` in this agent's scratchpad:
>   - a probe dev server on 5281 (injects `probe.js`: instantiations, GPU object counts, full-GC markers, load events, listeners);
>   - `trial.mjs` (repeated WebKit trials, shapes apj / apr / fr / free / p2r, `--exp` kicks, `--old` retention, `--clear`);
>   - `kicksrv.mjs` / `reprosrv.mjs` (test pages).

> **CTM events in the world, stage 3: pv `eventInWorld` (off, QA), the Snow Jam event in the streamed world with no load; live core 06c4862e (2026-09-29, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) §4 stage 3 "Built".
> - **Live core** core.wasm 06c4862ef8410d47... / core.js 1f34a77b7fe5b076...: scratch build of the current tree (only web/peak_world.inc's `event_course_seed` new). Full ps2-captures 257/257. test-ctm-event-world is in test:all.
> - **Flow:** main.js cb.eventInWorld / introInWorld / eventInWorldEnd, `worldEvent` guards (gate / station / FAQ / Big Challenges / free-ride HUD), startRun's event start and audio. career-ui begin `inWorld`, ctm-event.js.
>   - Chrome and WebKit: gate → fly-over → approach → idle + card → countdown at the PS2 grid spot → race, all in MOUNTAIN, no load screen.
> - **Capture:** `local/ctm-events/capture_card.py` + `caps/c0a-full` (arrival → free ride → gate → hold → card → race, 3000 records).
> - **Not exact yet:** core `begin_event_rider` seeds the Single Event anchor over the carried rider. The PS2's 11D390 event branch (0x11D564) keeps it. Needs a core `event_grid_start` (physics) and a comparer in-world mode for c0a-full.

> **CTM events in the world, stage 2: pv `eventWorldData` (off), the event's own data read at the gate; core `event_course_seed` (scratch) (2026-09-29, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) §4 stage 2 "Built".
> - New `web/ctm-event-plan.js`, called from ctm-event.js at the fly-over: riders, lineups, race-event document, spawn, route, meter, slope-style list, camera triggers, GO starts. Inert.
> - All 17 event courses' race / reset paths are their streamed location's bank, so the plan's exporter change and `init_race_event` were not needed.
> - Core `event_course_seed(code)` (web/peak_world.inc): the event start seeds, with the streamed world's lists kept.
>   - Built in local/ctm-events/core. Targeted ps2-captures (9) pass on it; **the live core is not rebuilt** (the coordinator's call, or the next stage's).
> - New `web/test-ctm-event-world.mjs` (not yet in test:all). Chrome and WebKit: the plan resolves by the switch.
> - Also: test-ride-warm's `course === 'PEAK1'` is stale since pv peakRelease is on (quality=low now rides MOUNTAIN).

> **The tuck-onset +0x2E4 difference settled: 11B3F8 was exact; the comparer's arrival seeds, the route tick and the reset-tick order were not (2026-09-29, physics agent):** see [core-gameplay-fidelity.md](core-gameplay-fidelity.md) "Speed limit".
> - **Cause (traced):**
>   - A record is taken at the provider exit 0x128630, after 120F20's `jal 11B3F8` (0x121024). So record k holds L(k), the limit tick k uses, computed with the crouch +0x220 as tick k-1 left it.
>   - `--peak-arrival` seeded record P's L(P), one step ahead. That is the 0.42 cm/s on the tuck onset (bra2-tuck-late 2071). Seeding from P-1, as before the §6.8 change, is exact.
>   - The early tuck's 0.09 cm/s: the unseeded route heading +0x4CC. The browser had the course seed's -2.98, the PS2 the carried -0.4016. 13C948's heading weight was 0 with a location id (+0x434) of 17 or more, so the drive and its crouch term were dropped.
>   - The late tuck's 2227 is not the limit (1150 cm/s against a 2335 limit). 112338 switched paths at 2225 instead of 2280. Its `lateral > 500 && tick % 60 == 0` uses the rider manager's +8 (1298C8, the capture's tick field). The browser's copy was 2773 against the PS2's 2057.
>   - The event-race "gap from 187" was the same one-record misreading. Its tick-244 miss was physics_info[0]'s m/s rounding.
> - **Core (live, wasm sha256 c9d783585a62e1d93907572f32b0f5b35825331c0bd43b95eb605438ea43541f):**
>   - `step_rider` runs 11B3F8 before this tick's reset requests: Select, 1210B0, the controllers. A reset tick's limit was motion 3's: tech-select-* and tech-oob-dance 3297, setpieces/full 5544. Physics were exact because the limit never binds there.
>   - `browser_game_tick_restart` also moves the race clock's totalTicks. The port keeps 1298C8 twice; a peak-run crossing restarted only motionTick.
>   - New `arrival_carry_seed(heading, location)` and `speed_limit_seed_held`.
>   - physics_info[6] now gives the limit exactly, in cm/s; race_result_info[6] gives totalTicks; ground_state_dump now includes +0x470 and +0x4CC.
> - **compare-ps2-capture.mjs:**
>   - `--peak-arrival` seeds the limit from record P-1 (`LIMIT_AT_PLACEMENT=1` gives the old seed), and +0x4CC / +0x434 / the game tick from record P (`NO_ARRIVAL_CARRY=1` skips them). An arrival now needs the new core.
>   - `--seed-limit` holds record 0's value for tick 0.
>   - The limit is read exactly, and `--fields` compares +0x2E4 with record i.
>   - New summary fields `speedLimitAgreed` and `firstSpeedLimitDivergence`.
> - **test-ps2-captures.mjs:** +0x2E4 is gated from its first equal tick while the physics are exact, and a limit that never becomes equal fails. Event-start course seeds can hold a different retained limit than the capture's savestate; it becomes equal during the countdown (crows-invert 132, monster-deepsky 136).
>   - New gates: `ctm-events/bra2-tuck-late` and `-early` (runs/ctm-events links), both exact to the end.
>   - Pending c0b-bra2-arr: exact through 2413 (the NIS hold 2414), was 2057.
> - **Checked:**
>   - ps2-captures 257/257 on this core: 255 at their baselines plus the 2 new gates; the 32 that first failed on the new limit gate are now exact on it.
>   - node: speed-limits, rider-reset, forced-reset, detached-reset, route-progress, crash-gameplay (both), rail-gameplay, steering-gameplay, race-finish, peak-release, peak-run, peak-world.
> - **Left:** +0x248 (the +0x244 lean rate) is 0.05 in the browser and 0 on the PS2 after an arrival. Not gated and not investigated. engine/riding.hpp `beginFrameState` (the prototype rider) keeps the old order.

> **CTM events in the world, stage 1: pv `worldUnderCuts` (off), the world ticks under the fly-over and the arrivals (2026-09-29, CTM events-in-world agent):** see [ctm-events-in-world.md](ctm-events-in-world.md) §4 stage 1 "Built".
> - **What changes:** the WS1 fly-over and the WS10 arrival lists play over the ticking world, and the rider is held by the cut's rider actor (core nis_hold) instead of a HOLD context. Only movie steps push a HOLD (`WS10 movie`).
>   - The arrival's ride starts before the list and the rider is placed after it (career-ui.js enterWorld `arrival`, main.js cb.arrivalPlace).
>   - Files: main.js (nisCut / nisCutHold / nisCutEnd, onMovie, the gate pause, cb.cutscene, cb.arrivalPlace), cutscenes.js (onMovie), career-ui.js, pv-flags.js.
> - **Checked, Chrome and WebKit:** new career: tick 0 through the ABC1 movie, then 2 → 531 under #153. Snow Jam gate: 3631 → 3890 under #94, with the rider at the PS2's hold position to the cm. No context left open.
>   - With the switch off, test-pause-contexts, cutscenes, ctm-flow, presentation and world-warm pass.
>   - test-ride-warm timed out under load (35-38).
>   - The capture gates are waiting for the physics agent's core.
> - **Deferred to stage 5:** WS13 / #152 (the event core has already been replayed or ended by then).
> - **Separately:** the §6.8 speed-limit claim is withdrawn (it was comparer seeds; the physics agent is fixing it).

> **Whole-mountain memory on phones: the location release follows the PS2's rule, pv `peakRelease` ON (2026-09-29, whole-mountain memory agent):** see [mobile.md](mobile.md) "Whole-mountain memory" (target, tools, every run) and [ctm-parity.md](ctm-parity.md) "The PS2's location release" (the rule).
> - **Measured** (WebKit through the driver, phone tier, footprint sampled outside the page, page polled every 3 s):
>   - All Peak Race, All Peak Jam and Peak 2 Race, landscape and portrait, direct and from the menus: all reach their results;
>   - the long MOUNTAIN free ride with 3 Transports (phone and desktop);
>   - the career shape with 4 event cycles.
> - **Before (switch off), All Peak Race:** WebContent p95 1187 / max 1318 MB, core 154 -> 319 MB.
>   - A peak run never freed collision (a `!route` guard).
>   - It read 2 rows ahead plus every hub connector's row (39 locations loaded by the finish).
>   - Stations built the nearest connector's course, and packages stayed 45 s after their location left.
> - **Now (web/free-ride.js, web/peak-world.js; JS only, the core is unchanged):**
>   - A location no row wants and nothing reads ahead is freed at once, as the PS2's eviction (T+8) frees the track: draw package,
>     environment slice, collision.
>   - A peak run reads ahead only its route's next row. Unfed read-ahead slices are dropped (`dropQueued`).
>   - Draw releases wait while a needed package builds.
>   - All Peak Race: p95 878 / max 1076 MB, core 128 MB flat, 99 of 125,776 frames over 34 ms, 0 stalls.
>   - Every shape measured is smaller: career-shape lifetime peak 2620 vs 3044 MB.
> - **pv peakRelease is ON.** Every tier now rides the career free ride in MOUNTAIN (`mountainFreeRide`: no tier split).
> - **Checks:**
>   - Capture gates pass: allpeak apr / p2r / apj, ctm/fr-dra4a-full, peak3/fr-throne-unload, peak2/fr-d-glide, peak1 fr / race /
>     green.
>   - Tests pass: test-peak-release / ctm-flow / ctm-stream / ctm-left / mountain-world / mountain-ride / peak-mountain / peak-run /
>     collect-restream / big-challenges / world-warm.
>   - Re-entry heights after release and re-feed: 0 lost. Gravitude's 28 cm changes are the world's own moving collision; they also
>     appear without any release.
>   - Desktop A/B: 0 stalls both ways.
> - **Target** (headroom reasoning in mobile.md): at most 1.0 GB steady and 1.2 GB for peaks. The peak runs now meet the steady
>   target. Not yet met, and none of it comes from the location release:
>   - course-switch spikes of +1.2-1.9 GB WebKit Malloc for 1-3 s (event load / return), with the switch on or off;
>   - 1.3-1.47 GB load transients (menu boot, direct boot);
>   - a WebKit Malloc climb through a run;
>   - about +200 MB kept from the menus (a second 128 MB wasm memory, probably the FE stage core);
>   - the WebKit runaway (2 of 9 runs; its own agent now).
> - **Measuring pitfall:** two WebKit windows side by side throttle the occluded one to about 28.5 fps. Take frame numbers from runs
>   alone.
> - **Tools and runs:** `local/browser-validation/whole-mountain-memory/`: apr.mjs (WebKit pilot + footprint), analyze.mjs,
>   loadpeak.mjs, chr.mjs / abuf.mjs / retainers.mjs (Chrome heap), dyn.mjs, era5-release.mjs, runs/.

> **Audio memory: voices let go of their nodes, only the playing / next songs keep their bars, speech lines by range (pv `speechRange`, off) (2026-09-29, audio leak agent):** see [audio-logic.md](audio-logic.md) 9.15 and 9.12 "Memory".
> - **Node leak (no switch; nothing plays differently):**
>   - A finished sfx voice disconnected only its output. Its source, gains, LFOs and panner stayed wired, and ended voices were held by `audio-world.js contactVoices` / `scriptVoices`.
>   - Speech lines, music bars, stopped songs' chains and the LoadingScreen loop were never disconnected.
>   - The 3BB588 pool-failure path left LFO sources looping forever.
>   - Now every finished, stopped or failed chain disconnects, and an ended handle drops its nodes.
>   - Chrome, Peak 2 Race from the menus (phone tier, live after GC): sources 17 -> 227 in 16 min before, 12 -> 10 after; panners 8 -> 218 before, 5 -> 5 after; gains 58 -> 220 before, 67 -> 69 after. Free ride stays flat too.
> - **Range cache:**
>   - `pathfinder.js musStreams` kept the last 4 songs' bars: 38.6 MB at 15 min. It now keeps only the song playing (loadSong role `play`) and the next one (role `next`, prefetchPicked): 15.9 MB.
>   - Inside a song, a 24 MB safety cap: played bars go first, then LRU. Lower caps made the read-ahead fetch 2-4x the song again (simulated).
> - **Speech (pv speechRange, off: the coordinator decides):**
>   - The whole `.dat` of every bank used stayed for the session (21.6 MB of whole files at 15 min in the race; DJ_Hub_Char_Stories is 35 MB). Now each line is its own Range request into a 2 MB LRU (race 1.3 MB, free ride 1.7 MB).
>   - Lines are fetched at dispatch and predicted at each post: the scheduler's next dispatch runs on a copy of its state.
>   - Same ticks and PCM when a range answers within ~30 ms. Beyond that, a line is +1 tick per ~16 ms (70 ms: +3 ticks). Before, a bank's first line waited for its whole `.dat`.
>   - `web/server/mp-server.mjs` now answers Range requests from the file, not its precompressed .gz: `.dat` files gzip to 80-84 %, so the host would have sent whole bodies.
>   - Not checked: Cloudflare's answer to a Range request on a cache miss. A 200 keeps that bank whole, as before.
> - **WebKit (phone tier, 10 min):** WebContent ran away from the DRA4 crossing in 4 of 6 runs, old audio and new alike, with wasm memory growing alongside (not audio). The flat runs are within the runs' spread (p50 1191 / 1164 MB).
> - **Tests:** game-audio, audio-timeline, audio-sfx, audio-glitches (new "node lifetime"), ctm-audio, music-stream (roles), audio-decode, challenge-audio, audio-menu, and new test-speech-range (in test:all). audio-timeline, ctm-audio, game-audio and challenge-audio give the same output with speechRange on.
> - **Files:** web/sfx.js, pathfinder.js, audio-speech.js, game-audio.js, audio-world.js, pv-flags.js (`speechRange`), server/mp-server.mjs, test-audio-glitches.mjs, test-music-stream.mjs, test-speech-range.mjs (new), package.json.
> - **Open:** the game-audio `once()` cache keeps each sfx bank's bytes after its slot unloads (~0.1 MB per location bank).
> - **Scratch:** `leak/` in this agent's scratchpad: nodes.mjs (Chrome node counts + snapshots), bigpaths.mjs / mapkeys.mjs / rootpath.mjs (snapshot retainers), churn2.mjs (stream-cap traffic), vite.old.config.mjs (serves the pre-fix audio modules on :5291).

> **Pause contexts: one path, switches retired; the card hold and Continue on frames; the CTM overlays' sound set (2026-09-29, pause-contexts agent):** see [pause-contexts.md](pause-contexts.md).
> - **Deleted:**
>   - main.js's `paused` flag and `pause()`, the overlay's legacy halves, transportInWorld's `if(paused)pause(false)`;
>   - cutscenes.js `idleFrozen`;
>   - big-challenges.js's `pause` parameter;
>   - pv `pauseContexts` / `cardFreeze` / `bcPromptAudio` (PV_DEFAULTS and every read).
> - **The card on frames:**
>   - The update that first sees a stopped NIS runs it to its next whole tick, then holds it, so the idle holds at t = 1.0 exactly (it was 1.0 or 2.0 with rAF jitter).
>   - Continue is refused outside the ctm-objectives phase 5 on every input path (the wall-clock `cardOpenAt` guard is gone; `cardTicks` still times the card's draw).
> - **The pad menus drive the UI frame counter:** one `phases.step()` before each PS2 pad update (web/gamepad-menus.js), so on a dropped display frame each key meets its own frame's phase.
> - **QA:** `ssxQA.start()`'s hold is SIM-only (HOLD), so cutscene probes play under it.
> - **Port bug fixed (finalpass "session" / "quit-save"):** ctm-quitsave, ctm-bcsure, ctm-session and ctm-sessconfirm played the front end's sounds (0 / 2 / 4) instead of the overlay's (9..13).
>   - Evidence: 0x39B5E4 blocked end = snd 0xD; 0x20DB64 Triangle = kind 6 -> ev 9.
>   - Fix: audio-menu.js PAUSE_SCREENS, plus `sfx()`.
>   - Not added (unconfirmed): ctm-gopeak, ctm-enterlodge.
> - **Checked:**
>   - node: 33 related suites, including test-pause-contexts, test-ctm-flow, test-gamepad and test-start-rules.
>   - ps2-captures: 1/1 (255 scenarios).
>   - pauseprobe: 6/6 in Chrome and WebKit.
>   - cardprobe: t = 1.0 in both.
>   - finalpass: 15/15 in Chrome, 13-14/15 in WebKit. WebKit's misses are presses its headless rAF delivers a frame late; the gate records are exact.

> **CTM events in the world: R6 settled, the Snow Jam gate scored, a free-ride speed-limit difference found (2026-09-29, CTM events-in-world agent; tools, tests and docs only):** see [ctm-events-in-world.md](ctm-events-in-world.md) §6.8.
> - **Correction:** 2300F0 runs only online (WS3 enter tests [0x534B30], 0x234B14). Offline, nothing resets a first heat's world.
>   - The card's Continue runs the rider manager's restart 129768 (via C vt+0xCC+0x28(3) at 0x236D28): the section activation is emptied and rescanned under the event kind, and the race clock is zeroed.
>   - 230180's set-piece bucket-8 purge (0x2301C4) destroys the DeadNodes (destructor 0x360A28; entry probe: 47 calls).
> - **Snow Jam gate scored:** `ctm-events/c0a-gate-arr` replaces the unscored c0a-gate.
>   - Built from menus/ctm/state-transport-confirm with a stale ps2_menu_capture hook removed, then ara1-screen10, then an open-loop 314 neutral + 20 left.
>   - Exact on `--course PEAK1 --peak-arrival` through 2852 (the gate at 2822 and its 30 riding ticks). It leaves at the NIS hold (2853).
> - ~~compare-ps2-capture.mjs --peak-arrival: +0x2E4 seeded from the placement record~~: withdrawn. The physics agent showed that P−1 was right (records are taken after 11B3F8). The 0.09 cm/s is the missing +0x4CC seed and the game-tick seed; that agent is fixing the comparer (ctm-events-in-world.md §6.8).
> - ~~For the physics owners: the port's 11B3F8 disagrees on a tuck-onset tick~~: withdrawn (comparer seeds, not the port). The details below are kept for the record.
>   - +0x2E4 is 0.42 lower; physics diverge when the limit binds (bra2-tuck-late: exact through 2226, then 834.26 vs 831.58 cm/s).
>   - With the tuck held through the arrival, the first tick's drive already differs (c0b-bra2-arr's 0.09 cm/s).
>   - event-race shows the same +0x2E4 gap from tick 187 (ungated).
>   - Repro: `local/ctm-events/caps/bra2-tuck-{late,early}.bin`.

> **CTM events in the world: stage gates ready, six-rider career compares fixed (2026-09-29, CTM events-in-world agent; tools and tests only):** see [ctm-events-in-world.md](ctm-events-in-world.md) §6.7.
> - **Career lineups:** derived career countdowns `local/reference/pcsx2/characters/career/ARA1-{qual,semi}-zoe` (with READMEs); `tools/export_lineups.py export-career` → `local/assets/native/ARA1/lineups-career/`. The final's document is unchanged. `build` was not run.
> - **`web/ps2-capture-ai.mjs` `rosterOrder`** (used by `compare-ai-capture.mjs`): ps2_capture stores the computer riders' blocks in actor-address order, while documents list them in roster order.
>   - WS13-rebuilt heats allocate their riders out of order, so every rider was compared with another's grid spot.
>   - All 62 existing --ai-state captures have the identity order; event-race-ai, ko-attack and parity-ai/metro-race still pass exact to the end.
> - **`web/test-ps2-captures.mjs`:** new `pending: SWITCH` cases, skipped unless `PENDING=1` / `ONLY`, and scored without failing:
>   - `ctm-events/c0a-race`, `c0c-race` (human and six riders);
>   - `c0b-ass1-arr` (exact through the gate tick + 30);
>   - `c0b-bra2-arr` (open: 0.09 cm/s at the placement tick with the pilot pad);
>   - `c0a-gate` (unscored until a PEAK1 seed).
>   - Captures are linked from `local/ps2-capture/runs/ctm-events/`.

> **Pause contexts and screen phases: the PS2's freeze stack and phase machine replace the port's pause flag, wall-clock lock and setTimeout outros (2026-09-29, pause-contexts agent):** see [pause-contexts.md](pause-contexts.md) (as built).
> - **pv `pauseContexts` (off; proposed on together with ps2MenuInput):**
>   - `web/pause-contexts.js`: the stack 0x5366E8 with the masks 0x4428F0. `paused` is derived (`isPaused()` = top mask bit 0x01), and the tick loop reads `simFrame()`: a pop runs the tick from the next frame (E11).
>   - Owners:
>     - main.js `overlay` (context 2 + the audio pause): pause menu / MCOMM / station prompt / map / FAQ. It pops at Return, Give Up and the Transport's Yes, and drops at Quit, Restart, a world load and the results.
>     - the `ctm-objectives` screen: context 1.
>     - big-challenges.js: context 3, audio:false.
>     - port HOLDs named by world state: WS1 ride-in / event transport, WS10 cutscene hold, WS11 transport (no NIS hold), WS14 station cut (no NIS hold).
>   - cutscenes.js: the NIS clock obeys bit 0x08 and the fade keeps its own clock.
>   - startRun / stopRun `console.warn` any leaked owner and then clear the stack.
> - **The phase machine always runs** (`web/screen-phases.js`, 0x39ECB0 phases 2/4/3/5/6/7; a 60 Hz UI frame counter from the pad menus' rAF loop). Its locks and outros (menu-rules.js data) apply with pv ps2MenuInput. Deleted: ui.js `inputLockUntil` and career-ui.js's `setTimeout` outro. With both switches off nothing changes.
> - **Frames:** MCOMM +60 dead / +61 taken (PS2 +60 / +62), Yes / No +23 / +31, Map +23 / +31, Rider Details +32 / +38, No -> pause +83 (+82 dead): two passes, no +2 constant.
> - **Checked:**
>   - node: test-pause-contexts (new, in test:all).
>   - test-ctm-flow (the stub steps the phases, plus a +21 / +83 block), test-gamepad and test-start-rules (PS2 pad-history variants): all pass with {}, {pauseContexts}, {ps2MenuInput} and both.
>   - Also: cutscenes, big-challenges, ctm-*, fe-screens, audio-*, presentation (one regex updated).
>   - ps2-captures: 1/1 (255 scenarios, at their baselines; the comparer imports none of the edited files).
>   - Browser, Chrome and WebKit, muted: local/ctm-decomp/screens/pauseprobe.mjs 6/6 each (E11 on the UI clock; an MCOMM Transport to Yellow on MOUNTAIN, 9.1 s / 11.2 s, nothing held after). cardprobe.mjs: the idle held at t = 1.0 with fadeT running. finalpass.mjs with `PV=ps2MenuInput,pauseContexts`: 12/15 Chrome, 11/15 WebKit. The misses are session and quit-save (failing before), the rAF-flaky repeat, and WebKit's rAF-counted +60.
> - **Follow-up once on:** delete the legacy branches (the `paused` flag, `pause()`, cutscenes `idleFrozen`, pv bcPromptAudio, the transportInWorld `if(paused)pause(false)`).
> - **Notes:**
>   - finalpass's first MCOMM check pressed at Start + 61, not +60 (fixed).
>   - career-ui.js's card guard `cardOpenAt` is still wall-clock (untouched, covered by the ctm-objectives lock under ps2MenuInput).
>   - The `seq.t >= 1` card rule is rAF-jitter sensitive (legacy froze at t = 1.998 in one Chrome run).

> **CTM events in the world, stage 0 evidence (2026-09-29, CTM events-in-world agent; no game code changed):** see [ctm-events-in-world.md](ctm-events-in-world.md) §6. Silent ARMSX2 on derived states, runs in `local/ctm-events/caps/`.
> - **The gate on the PS2, Snow Jam / Metro-City / R&B slope style:**
>   - WS1 at the gate tick; all AI riders created 2 ticks later (C+0x78 1 → 6, or 3 at R&B);
>   - the human in its own control for 31 ticks, then NIS-held (control 13, +0xAC4 = 1) until the grid placement 542-546 ticks after the gate;
>   - the shared RNG nearly still under the hold (3 changes at ARA1 / BRA2, 14 at ASS1).
> - **A CTM countdown's world equals Single Event's** (octree, patch order and instance sets). The human carries about 170 words of free-ride state.
>   - Replayed on today's ARA1 event package, the CTM first heat is exact through the countdown and diverges at the push-off (tick 182).
>   - The first heat keeps free ride's DeadNode start fences and the hidden gate volume (2300F0 does not remove DeadNodes; 230180 does).
>   - The CTM collectibles and markers differ from Single Event's (game type).
> - **WS13:** 201 world ticks under the gondola with the human held.
> - **WS15:** session point 1 at once, the AIs gone within about 10 ticks, WS4 11 samples after the confirm, no load.
> - **New capture:** the first CTM six-rider capture (`c0a-race`, --ai-state) waits for a career lineup document before it can gate.
> - Unknowns R1-R4 settled, R5 / R6 partly settled (builtin 68 has no kind gate; 3A6800 = the load-flags rewrite). The rest need code stages.

> **CTM events in the world, no load screens in or out: design (2026-09-29, CTM events-in-world agent; no game code changed):** see [ctm-events-in-world.md](ctm-events-in-world.md).
> - **PS2 (traced):** the gate 22D6C8 resets nothing; WS1 builds the riders one 11C298 step a tick under the fly-over with the world ticking. ~~WS3 enter always runs 2300F0~~ (corrected in the entry above: 2300F0 is online only). The card's Continue runs the rider manager's restart 129768 (the activation list emptied, a rescan under the event kind, the race clock zeroed); WS13 / WS15 run 230180 (world reset). WS15 is 230180 + session point 1 + white fade, then WS1 arg 0 → WS2 → WS3 → WS4.
> - **Recommendation, option C:** CTM events run inside the streamed-world core; the event package supplies only per-event data (riders, lineups, race event, start seeds, progress meter, freestyle rules, camera triggers); Single Event keeps its packages (as the PS2's WS10 load does). B (a second core warmed under the fly-over) is rejected: +250 MB on phones, still black in WebKit and on phones, and the global-core refactor.
> - **Staged plan**, one default-off switch each: stage 0 evidence (PS2 captures c0a-c0d, CTM-vs-Single-Event countdown memory diffs); `worldUnderCuts` (#10 and the arrival-NIS item: the normal tick with the human held, no new core entry); `eventWorldData`; `eventInWorld`; `eventInWorldAi`; `eventReturnInWorld`.
> - **Core blockers found:** no event start seed in streamed worlds (`browser_select_event_course` only in a plain init); parse caches skip appended loads; `peak_stream` / path banks are per rider context and `deliver_paths` is "player 0"; section DeadNodes are human-only with no shared-world replay kind; no rider-context destroy; the GO LiveComps are filtered from the peak exports.
> - Scratch: `local/ctm-events/` (notes-ps2.md, package-diff.sh).

> **CTM free-ride fixes from the scripted-content decomp: Big Challenge decline reset, the Transport's one-list fade, station stage calls, the deferred FAQ; streamed-world Spline pieces in the core (2026-09-29, CTM fixes agent):** see [ctm-parity.md](ctm-parity.md) "Free-ride fixes from the scripted-content decomp" and [set-pieces.md](set-pieces.md) "Streamed worlds". All switches are off until the coordinator turns them on. The live core has the core parts (see ctm-parity.md for its sha256).
> - **pv `bcDecline`** (core `mission_lifecycle`):
>   - a declined / quit Big Challenge resets the rider (30B658 / 30B758 -> 1235F8: control 9, white fade, route placement at tick 21, control 4 at tick 41), so the offer does not come straight back;
>   - an event gate ends a running challenge (builtin 67's 30B7F8);
>   - world state 10 enter clears the active challenge, its HUD and the offers (309030 / 308988).
>   - Checked: test-big-challenges "lifecycle"; Chrome = WebKit per tick against PS2 bigchal/tri-offer.
> - **pv `transportFade`:**
>   - The in-world Transport is one NIS list: departure, in-air, the held loop and the backcountry heli drop.
>   - The destination is requested when the loop starts.
>   - The release fades the still-playing loop to black over 30 ticks, and the placement comes at R+29.
>   - The world then fades in under the HUD while the bars slide out over 30 ticks and the HUD is squeezed inside them. The dome switches after the list.
>   - A 30-tick skip lock at the start.
>   - Checked in Chrome and WebKit against PS2 crossing/to-c-fade.
> - **pv `departCalls`:**
>   - gond_dep / heli_dep channel-0 calls now run on the station's stage track, with their cleanups.
>   - The gondola still does not move: the station depart LiveComps are in no livecomp.json.
>   - TRANSP has no stage seed, so the in-air calls have no functions.
> - **pv `faqDefer`:** the Green "?" FAQ opens once the ride runs, instead of being dropped under a pause / prompt.
> - **Found, not fixed (reported):** main.js transportInWorld's held path never unpauses. A Transport picked from the MCOMM never ticks: the rows never load, and after the 2-minute cap the rider is placed without them.
> - **pv `peakSplines`** (core `set_piece_streamed`; see set-pieces.md "Streamed worlds"):
>   - A streamed world's set-piece tables are its event locations' Spline pieces, trigger owners, spline LiveComps and ParentModifier children. A location's pieces go at its unload (0x3551A8).
>   - Draw counts: fr-dra4a-full's spline trigger ticks and apr-start's EBC3 cessna now match the PS2; apr-start is draw-count exact on every tick.
>   - Drawing needs the re-split packages in local/ctm-fix/export: PEAK1..3 `--batches-only` and MOUNTAIN attached.json. The coordinator copies them.
>   - Not built: the MultiSplines (chairlifts, traffic, bins, trams) and the resident blimp loops.
> - Tests: test-big-challenges, test-cutscenes; ps2-captures on the scratch cores (streamed worlds and resets, all at their baselines).

> **Post-event Transport freeze found and fixed (pv `switchGate`, ON); world-arrival stalls (pv `worldWarm`, off: the coordinator decides) (2026-09-29, CTM stalls agent):** see [course-switch.md](course-switch.md) "The course being built runs nothing" and "World arrivals warm under the load screen", [ctm-parity.md](ctm-parity.md) "The post-event Transport freeze".
> - **Freeze (Owen's Safari session t93ez0j6: MOUNTAIN/18 "to game", then silence):** during a course switch the global `core` is the new instance from the start of `loadCourse`, while its init runs in slices across frames. The Transport's held loop (`cutscenes.js acrossSwitch`) kept lighting its actors (`fe-preview light`: `_malloc`, `_reset_rider_lighting`, `_shade_rider_lighting`) and resetting painters (`_weather_fade_reset`) through `host.core` every frame on that half-built core, and drew the new course's objects (render hooks) early.
>   - Counted: 97-247 calls per Transport in WebKit, 145-1498 in Chrome, MOUNTAIN and per-peak worlds alike.
>   - WebKit: 3 of ~12 Transport loads crashed in the new core (`call_indirect` / memory OOB in `_init_environment`, `_reset_rider`, `_init_animation`; the ARA1 fallback crashed too -> title). A corruption that does not trap can hang inside wasm: Owen's log.
>   - Fix (main.js): `coreLoading` from `newCore()` to `live`; the cutscene host's `core` is null and `freeRideCourse()` -1 meanwhile; the held loop's draw (`renderAcross`) leaves out what the switch added. QA `window.__coreLoading`.
>   - Checked: 0 calls into a loading core, WebKit 3/3 and Chrome 6/6 Transports (with / without mountainRide). Tests: presentation, cutscenes, ctm-flow, ride-warm, lazy-course, ctm-stream, gpu-recovery.
>   - Open: `loadCourse` keeping the instance local until its init ends (its helpers read the global: a separate, test-gated refactor). hangWatch cannot report a wasm loop in WebKit (its worker's I/O goes through the frozen main thread); a Service Worker could.
> - **Harness note:** every earlier WebKit / Chrome run of this flow never started the AudioContext (no gesture: decodeMaxMs 0). A synthetic keydown unlocks it; `?mute=1` keeps the master gain at 0 (silent).
> - **Stalls (pv worldWarm, off):** the ride's first frame after a world load built what the never-awaited start-row rewarm had not reached: 2897 render objects + 28 blocking pipelines (Owen: 3.3 s + 3.3 s). In WebKit three's compile yields a whole rAF per item (no `scheduler.yield`), so the rewarm took 30-50 s.
>   - `web/yield-shim.js` (task yields within a 4 / 8 / 12 ms frame budget, WebKit only); the start row compiled first with a DoubleSide pass for two-pass materials (their 'backSide' render objects); the rest of the world pass compiled alongside rideWarm; then 0.5 s of warm frames behind the opaque load screen; cutscene sets / actors compiled per side.
>   - WebKit: first MOUNTAIN arrival load 3.8-5.0 -> 6.2-8.2 s, the 400-520 ms first-frame hitch gone (0 render objects, 1 pipeline after the load screen); Transport arrival 3.7-4.8 -> 4.1-5.1 s, 800 -> 27 render objects; BRA2 event load unchanged (7.6 s); lodge-door cut 5 -> 1 blocking pipelines.
>   - `web/test-world-warm.mjs` (new, in test:all): 300 ticks identical and 0 pixels different, off vs on.
>   - Left: the held loop's straight-to-canvas draw (~20 pipelines at the switch), the Transport arrival's last 11 pipelines, the character select's FE builds.
> - **Files:** `main.js`, `free-ride.js`, `cutscenes.js`, `yield-shim.js` (new), `pv-flags.js`, `test-world-warm.mjs` (new), `package.json`. Scratch tools: `fz/` in this agent's scratchpad (`owen2.mjs` Owen-shape WebKit flow with audio, `flow.sh` / `chromeflow.mjs` core-call counting, `buildlog.js` render-object / pipeline creation log, `oldserve.mjs` serves an old deployed bundle over the dev server's data).

> **CTM audio: world switches carry the audio, post-event DJ commentary, saved first visits, DJ queue rules, mail icon freeze (2026-09-29, CTM audio agent; every switch off):** see [audio-logic.md](audio-logic.md) 9.14 and [ctm-decomp-freeride.md](ctm-decomp-freeride.md) ranked 2, 9, 10, 11, 12.
> - **pv `worldSwitchAudio`:** a Transport into another page world (the post-event map, or another peak on the per-peak worlds) now runs code 20 at the confirm, as the PS2 does (postevent2 ticks 15440-15442): Stop, pick, Radio BIG intro + pool-5 chatter, the destination song 10 ms later.
>   - `career-ui.js goWorld` calls `gameAudio.travelSwitch`; `transportAfterEvent` calls `eventMap()`, so the chartune plays under the map; `crossWorld` carries without code 20.
>   - Over the switch, leaveWorld keeps the song, timers, speech, pending DJ and `'nis'` voices. There is no LoadingScreen loop, and the next free-ride worldLoaded makes no 2867E8 / 2A4A78 and runs WS10 (`ws10`: first-visit flags, the pktrans resume).
>   - `freeRideCourse` runs WS10 too. That fixes the desktop in-world Transport into an unvisited backcountry: pktrans had stayed paused and code 19 took the wrong branch.
>   - Checked in Chrome and WebKit: the timeline matches, with no `loading` / `worldload`. The unswitched path does two world loads when the run starts before the load-screen notification.
> - **pv `postEventDj`:** the 2A45C0 / 2A4660 record in `finish()`. main.js passes the KOs (+0x128) and +0x114 (the score object port calls it the Uber count) from `score_object_dump`, and the freestyle place. The 2A4770 commentary runs first in hubChatter: Char_Progress / Aggression / High_Trick_Score, or by the destination when nothing was earned during a travel.
> - **pv `djVisited`:** main.js `gameAudio.context` passes `visited` (careerUI.visitedMask) and `peak2Locked`. Free_Ride_Intro is gated on 146008.
> - **pv `djQueueRules`:** Radio BIG intro stops the current line. A pause resume leaves the DJ queue alone unless +0x5828 is set. Its setter 28FAE0 (menu case 0x208C28) has no port equivalent identified, so for now it is never set.
> - **pv `mailFreeze`:** the event's posts start the icon at 182 frames, and the Message Center freezes it instead of clearing it. The blink under the finish HUD is still missing (the port decides the result at the results).
> - **Files:** web/game-audio.js, audio-speech.js, sfx.js (`stopAll({ keep })`), career-ui.js, career-messages.js, main.js, pv-flags.js; new web/test-ctm-audio.mjs (in test:all); test-messages.mjs extended.
> - **Tests:** test-ctm-audio, game-audio, audio-timeline, audio-menu, audio-glitches, challenge-audio, messages, now-playing and audio-sfx all pass, with the switches off and on.
> - **Not checked:** the NIS voices over the switch (headless shows none in the held loop either way); a real career post-event run in a browser (the commentary is covered by the unit test only).
> - **Harness:** `local/browser-validation/ctm-audio/switch-audio.mjs chrome|webkit [pv]` (ARA1 -> map carry -> heli ride -> MOUNTAIN), with the run logs beside it.

> **CTM screens and menus decompiled against the port; the input layer fixed behind pv `ps2MenuInput` (2026-09-29, CTM screens decomp agent):** see [ctm-decomp-screens.md](ctm-decomp-screens.md).
> - **Decomp:** every CTM screen, overlay and menu, traced with addresses:
>   - the MCOMM and pause family (overlays 1..5), the Yes / No popups and the other popups;
>   - the Transport map and Session;
>   - the lodge and its sub-screens, and the memory-card screens;
>   - the card, finish panel, results, records, reward list and replay menu.
>   - Plus the shared engine: the pad history 0x321298, UIMenu 0x39B000, the LUI thread ops 0x42 / 0x43 / 0x41, and the state phases 0x39ECB0.
> - **Measured on the PS2** (silent ARMSX2, `local/ctm-decomp/screens/menuprobe.py`: memory and UI sound calls per pad sample):
>   - the 24 / 12 repeat and a 3-update edge debounce;
>   - wrap on the MCOMM, pause, lodge and Yes / No; the error at the non-wrapping ends;
>   - the intro input lock (MCOMM +62, Yes / No +31, Map +31, Rider Details about +40) and the Yes / No outro (No -> pause input at +83);
>   - the MCOMM intro replay on returns, and the cursor memory;
>   - the world frozen through the pause stack.
> - **Landed by the coordinator from this work:**
>   - behind pv `ps2MenuInput` (off): web/menu-rules.js, gamepad-menus.js, ui.js, career-ui.js, audio-menu.js, big-challenges.js;
>   - with no switch: a Chrome-only bug where generic-menu moves were silent (ui.preKey).
>   - Final pass in Chrome and WebKit (`finalpass.mjs`): GO, except that the lodge FE lock should be 27 frames from the switch, not 40 (ranked 1a).
> - **Still open** (ranked list at the top of the doc):
>   - the finish panel's Cross skip;
>   - the Transport cursor / go-to-peak rule / INFO;
>   - the skipped Save game flow;
>   - Triangle doing things on the card, records and lodge questions;
>   - the buy popup's double action;
>   - the lodge cursor across visits;
>   - the gallery grid; Career Highlights;
>   - sounds (replay-ui kinds, silent lodge Triangles);
>   - the results Quit confirm;
>   - the flat screen re-entry.
>   - Open capture requests are listed in section 8.
> - No game code edited by this agent. Scratch: `local/ctm-decomp/screens/`.

> **CTM world states and event lifecycle decompiled; four fixes landed behind switches (2026-09-29, CTM world-state decomp agent):** see [ctm-decomp-world-states.md](ctm-decomp-world-states.md).
> - **Decomp:** all 16 world states (WS1..WS16: objects, vtables, enter / update / exit, default next states, every requester of 0x231250). Also the pause contexts and what each mask bit stops, the overlay command word gp-0x9F4 and the close dispatcher, the popups, the "Loading..." reason bits (S+0x94), and the GameModeMan handlers by mode. WS8 / WS9 / WS16 have no requester in retail.
> - **Ranked differences** (the doc's table). Fixed by the coordinator behind default-off switches, verified in Chrome and WebKit (`local/ctm-decomp/qa/`):
>   1. **Next heat / Final Round kept the qualifier's riders** (pv `ws13Rebuild`): now the semi / final = lineup.js roundEntries.
>   4. **Rival and peak-run cards over black** (pv `ws13Rebuild`): readyView now runs there too.
>   6. **The start-gate idle kept moving under the card** (pv `cardFreeze`): PS2 context 1 freezes the NIS; the fade runs on its own clock.
> - **Next for the coordinator:** 5, **Cross skips the finish panel** (a fresh edge from WS5 + 225; auto-close +405 / +285 DNF; PS2 runs `caps/fin-*`).
> - **Rival lineup data:** a PS2 Snow Jam career final countdown (Mac in slot 1) proved every rider leaf equals the Single Event tables. Only the rival's skin part was missing.
>   - `tools/export_lineups.py` gains `export-career` and `build --out`; the career skin parts are taken strictly (same course, or course independent).
>   - New ARA1 / BRA2 lineups.json in `local/ctm-decomp/lineups/out/` (test-lineups passes on them); the coordinator installs them.
>   - Peak 2 finals (Nate) still need a CRA3 / DRA4 slot-1 grid spot for his scale.
> - **Still open:**
>   - the event entry and exit loads (ranks 2 / 3, structural);
>   - the missing "Loading..." reasons: at the finish, through WS13 (rank 7);
>   - the peak-run Restart without a reload (8);
>   - the gate / heat / transport not ending a Big Challenge (9, scripted-content agent);
>   - the Session world reset (12, unconfirmed).
> - **Tools:** `local/ctm-decomp/{d, ps2dis.py, calls.py, scan.py, nextset.py, ui_trace_capture.py}`.

> **CTM course limits: wall scrapes held like the PS2, the one-way volumes push (pv `loadFlags`, on), builtins 99 / 101 decoded (2026-09-29, course-limits agent):** see [peak-mountain.md](peak-mountain.md) "Course limits in free ride" and [obstacle-collision.md](obstacle-collision.md) "Back-to-back soft collisions". **Live core rebuilt** (core.wasm sha256 1057624cb92bf3aa..., core.js c491dafd8b1861fd...), identical to the scratch build: the full capture suite (253) passed on it with CORE_JS, so it is not stamped yet.
> - **One-way volumes:** in the port no connector's `onewayvolume_*` ever pushed back. `stage_flags()` fell back to the authored word, so builtin 7's `| 0x100` gave 0x200100 with no static route (PS2 0x200322). Now it falls back to the load's runtime flags (authored high half + bit 1, the rule for every untouched instance in 794 PS2 savestates): core `stage_load_flags`, pv `loadFlags` (on), set in `free-ride.js` and `peak-capture.mjs` (`LOAD_FLAGS`). PS2 capture `course-limits/p3b-zig3000`: pushed 41.667 cm/s a tick at The Throne -> E; position-exact with it, 23 m off without. Chrome and WebKit: 0x200122 on every volume.
> - **Walls:** a soft collision starting while the previous soft's clip still faded ended after 2 ticks (the completion check took any channel-2 sequence of that clip; 312AE8 takes the first). `animation_bridge.cpp`, no switch (core physics, full suite unchanged). New gates `course-limits/p3b-right3000`, `course-limits/gs-zig3000`.
> - **Builtins 99 / 101:** 99 = game options (Multipliers / Power-ups / Point icons, *0x5308D0, 0 in every CTM savestate): nil takes the same branch. 101 = an attention point whose chain ends in an empty stub. No effect; documented in stage-scripts.md.
> - **Matched:** the per-instance collision state over all 22 course / station starts; the streamer's row timing (new `PEAK_AUTO=1` diagnostic in peak-capture.mjs); 20 new 3000-tick captures (PEAK1 / 2 / 3, MOUNTAINF, the CTM world start) with walls, crashes and 12 resets exact; the page replays world-start captures position-exact (`local/course-limits/page-replay.mjs`).
> - **Open (not course limits, for the physics / rail owners):** `course-limits/p2-right3000` 3312 (a rail exit: the PS2 spends one tick in motion 1); `mt-left3000` 6152 (28 cm of contact push-out applied before an air reset's freeze; the placement is exact).
> - **Diagnostics added:** core exports `world_instance_states` and `stage_seed_boost` (comparer `PEAK_SEED_BOOSTS`); tools in `local/course-limits/`.

> **mountainRide back ON (desktop), career-final rival data installed (2026-09-29, coordinator):**
> - **pv mountainRide: ON again**, at Owen's request. It had been switched off without a note. It's desktop only (free-ride.js mountainFreeRide: iOS / Android / quality=low keep the per-peak worlds + crossWorld until pv peakRelease is on). The PS2 streams the peak boundaries seamlessly. The post-Transport freeze fix (pv switchGate) is still in verification; Owen's freezing session had mountainRide on.
> - **Peak runs:** All Peak Race / Jam and the Peak 2 Race always run in MOUNTAIN on every device (peakRunWorld). The phone memory for them (pv peakRelease) is being measured and fixed by the whole-mountain memory agent.
> - **Installed:** web/public/assets/{ARA1,BRA2}/lineups.json from tools/export_lineups.py (the world-states decomp agent: Mac's skin part + skin_scale + career_skins, taken from a derived Snow Jam career-final countdown; additive only; sha256 c7476e7f... / bd25e18b...; new inodes). A career final now has the rival (pv ws13Rebuild). Peak 2 finals (CRA3 / DRA4, rival Nate) still need a derived countdown for grid[1][3f7fffff].

> **CTM free-ride scripted content decompiled against the port: messages, NIS triggers, Big Challenge offers, crossings / Transport, DJ and place audio, every stage program of the streamed worlds (2026-09-29, CTM free-ride decomp agent):** see [ctm-decomp-freeride.md](ctm-decomp-freeride.md) (a ranked list of 20 differences at the top, one rule table per system). Research only, no game code changed.
> - **Shipped config:** pv `mountainRide` is OFF, so the peak boundary (DRA4_A / ERA5_C) is a world load (`crossWorld`) where the PS2 streams one world. That is the top difference.
> - **Next biggest:**
>   - every world switch plays load-screen audio instead of the PS2's in-world code-20 travel audio;
>   - no Spline / MultiSpline / ParentModifier set piece moves in PEAK1-3 / MOUNTAIN: flybys, chairlift chairs, traffic, searchlights. PS2 free-ride savestates hold live SplineModifiers (vtable 0x48F250);
>   - the Transport's held loop hard-cuts on release, streams about 120 ticks early, skips the PS2's page wait, and is three NIS lists instead of one;
>   - the world freezes under the arrival cutscenes;
>   - station departures drop their stage calls, and TRANSP is in no seed;
>   - the Big Challenge decline reset (1235F8) and the prompt audio (fixes were landing during the audit: core `mission_lifecycle`, pv `bcPromptAudio`);
>   - the post-event DJ commentary (2A4770, captured);
>   - first-visit DJ lines per page load instead of the saved mask, and the Peak 2 lock gate;
>   - the mail icon posting at the results instead of the finish tick.
> - **Matches:** all 33 message posters, every NIS list trigger / flag / skip rule, the offer volumes and queue, the riding-crossing WS11 / WS10 timing, the arrival placements, the section coverage, and the seeds against the disc.
> - **New silent PS2 captures:** `local/ps2-capture/ctm-decomp/{crossing/to-c-x,crossing/to-c-fade,bigchal/tri-offer,bigchal/down-no,bigchal/no-offer,audio/postevent,audio/postevent2}`. Scratch tools are in `local/ctm-decomp/`.
> - **Doc corrections listed there:**
>   - peak-mountain.md "Cutscene hooks" (the location intros are implemented);
>   - cutscenes.md (the Peak 2/3 heli arrivals are triggered);
>   - ctm-parity.md (the ~1850-frame WS10 stop is the DBC2 movie);
>   - audio-logic.md 9.9 (146008 is the Peak 2 lock);
>   - characters.md (the "tutorial counter" is the event object's state);
>   - WS11's enter is 0x2368A0.

> **Per-frame JS garbage cut 40% (pv `sceneLightingOff` + `threeLean`, both ON after the WebKit check: BRA2 identical state and pixels at 8 in-race checkpoints, changes and switches) (2026-09-29, GC-churn work, continued after the coordinator hit its usage limit):** see [web-render-performance.md](web-render-performance.md) "Per-frame garbage".
> - **Measure:** Chrome sampling heap profiler, steady race, BRA2 + ARA1 at the phone tier and desktop (`local/browser-validation/gcchurn/`). Real objects, which Safari pays for too: **18.9 -> 14.1 MB/s** (13.2 with sceneLightingOff). V8-only HeapNumbers stay at about 25 MB/s.
> - **Changed (live, behaviour identical):**
>   - new `web/heap-views.js` (`heapU32` / `heapI32`, `setUpdateRange`);
>   - set-pieces-renderer update (logs read in place, pools, indexed loops);
>   - livecomp-animation (matrices recomputed in place, same EE/VU ops);
>   - attached-setpieces (delta slots, no Map / string keys);
>   - rider-shadow (fit / rows into per-rider records, bit-exact on 200k inputs);
>   - weather, board trail and snow read in place;
>   - set-piece particle records pooled (`readParticleEffects(core, pool)`);
>   - setUpdateRange in 8 renderers.
> - **pv sceneLightingOff (off):** `renderer.lighting.enabled = false` (main.js init, not with `?originalWorld=0`). No game material uses three lights, maps or environment. With lighting on, three rebuilt the LightsNode cache key on every draw (the dynamic key is cached per `renderer.info.calls`).
> - **Checked (Chrome):**
>   - targeted tests pass (livecomp x4 courses, attached, set-pieces(-renderer), particle sprites 20,459 bit-exact, rider-shadow, weather, board-trail, fog-puffs, snow, stage-world, presentation, light-glow). ERA5's 2 livecomp failures are pre-existing.
>   - Old vs new trees in the browser, BRA2 / CRA3 / DRA4 / ERA5, 12 in-race checkpoints each: identical scene state and pixels.
>   - sceneLightingOff on vs off: identical apart from rider interpolation noise.
> - **pv threeLean (off):** three r186 patched through a Vite plugin (web/three-patches.js, from tools/gen_three_patches.py; Owen approved): 11.3 MB/s with both switches (-40%); Chrome A/B pixel-identical on BRA2 / CRA3. The 5173 dev server needs a restart with `--force` to re-bundle three.
> - **Not checked in WebKit:** the screen was locked, which stops WebKit rendering (every page, the old tree included, stays on its loading screen). With the screen unlocked, run `wkcmp.mjs` and then `wkrace.mjs` (baseline in `runs/`), then switch sceneLightingOff on.
> - **Comparison pitfalls:**
>   - pass `&lineupSeed=` / `&presentationSeed=`: the lineup is clock-seeded, and CRA3 otherwise races different riders;
>   - compare objects as a multiset;
>   - rider pixels vary by a few levels between any two runs (real-time `renderAlpha`).
> - **Left (MB/s):**
>   - three r186 internals, about 3.5: the `needsUpdate` dynamic key (`hash$1` rest args), `Bindings._update` string keys, `updateTexture(t, options = {})`, render-list push / sort. These need a three patch: Owen decides.
>   - `ai-racers.js` per-tick sync, about 0.9: game tick path, change only with the AI capture gates.
>   - rider-frame, 0.35.
>   - trick-HUD text, game-audio relSnapshot, weather layer records.
>   - a long tail under 0.1 each, about 7 in total.

> **Warms that outlived their course fixed (pv `compileAbort`, off until the coordinator switches it on); peak-run HUD TypeError fixed (2026-09-28, blend-space agent):** see [course-switch.md](course-switch.md) "Warms that outlive their course".
> - **Cause of the per-switch WebKit growth and of the "GPUDepthStencilState.format is required" errors:** three r186 `compileAsync` builds its items one at a time between yields. `unloadCourse` waited 1.5 s for the world warms; on WebKit the rest kept building after the dispose. That re-uploaded disposed geometry and textures and failed pipelines on the old world pass's destroyed depth texture.
> - **Fix:** `web/compile-abort.js`:
>   - `fog-renderer.js` installs the wrapper.
>   - `main.js unloadCourse` calls `abortCompiles(1000)` before `freeRide.stop()`.
>   - A dropped item leaves no state; its object builds on first draw.
> - **Numbers** (WebKit, 25 loads, cutscenes on, polled once per 3 s):
>   - WebContent footprint at the event: 1741 MB off vs 1024 MB on after 24 loads.
>   - Live textures: +32 off vs +5 on.
>   - Depth-format errors: 2-3 per event return off, 0 on.
>   - Load times unchanged.
> - **Peak-run HUD:** `main.js ui.freeRideHud` now takes the active event's tier row only for a peak run (mode 6..11), as the core's `peakSetup` hook does. A single event left active threw `undefined (r[3])` every HUD frame on MOUNTAIN / PEAK peak runs.
> - **Left:**
>   - One DataTexture per peak-world visit is never destroyed.
>   - three's pipeline cache grows about 9 per cycle, with the switch on or off; the GPU process stays flat.
> - **Tools:** `local/browser-validation/blend-space/hang/`:
>   - `memwk.mjs`: vmmap footprint and regions per load.
>   - `leaktex.mjs`: live textures by the course that created them.
>   - `depthfmt.mjs`: depth-format failure trace.
>   - `mem.mjs`: Chrome heap snapshots.

> **CTM batch: Transport held ride, 15 cm, game tick, release leaks, hang watchdog, WebKit load memory (2026-09-28, CTM agent):** see [ctm-parity.md](ctm-parity.md) (NIS hold + Transport ride, game tick, page runs, "WebKit WebContent peaks") and [mobile.md](mobile.md) "Hangs".
> - **Transport held ride (pv nisTick, on):** the Transport holds the rider through `nis_hold` while the world ticks, as on the PS2 (anchor 29 heli locator 5; heli_inair #149 / heli_inair_zoe #122 roots match the capture, `test-cutscenes.mjs`). The lodge / booth rider sits 15 cm under the locator (key 0 z = -15).
> - **pv gameTickKeep (on):** JS placements inside a streamed world keep the game tick (1298C8 restarts only at a world load, WS2 or an event type-5 crossing), including placeRegion's grid reset.
> - **Release leaks (pv peakRelease, still off):** read-ahead collision is now freed (`releaseIdleCores`), and gpu-copies restores are forgotten on release and dispose (`forgetGpuRestore`). The JS heap is flat over Transport cycles. WebKit still holds about 300-500 MB after a Transport; re-measure with pv compileAbort.
> - **Hardening (deployed):** a pause try/catch around the NIS release, `nisWatch` (a door / booth hold left on in 'game' is released after 30 frames), the `nisPosed` guard.
> - **pv hangWatch (on, web/diagnostics.js):** a worker posts `hang` / `hang-end` when the page's 250 ms pings stop for 8 s while it is visible. In WebKit all worker I/O goes through the main thread, so the events leave only once the page recovers; a permanent freeze shows as the next session's `previous-session-died`.
> - **WebKit load memory:** clean peaks are in ctm-parity.md. The largest excursions are per-frame JS garbage (16 MB/s in a race) that JSC lets pile up by about 1 GB, and dead wasm cores. The coordinator fixed `replayTriggersCore` (main.js unloadCourse). Proposed: cut per-frame garbage, reuse one wasm Memory across loads (core build), release the Transport departure (to be checked against a PS2 row trace first).
> - **Open:** the combined peak re-export (env split + lit banks/regions + moving splits incl. drawn_spline_pieces + ParentModifier children) is next. peakAttached still needs its browser check.

> **Regenerated set-piece seeds landed; ravens / eagles section-streamed, blimp rebuild programs guarded (2026-09-28, AI-parity agent):** see [set-pieces.md](set-pieces.md) "Regenerated location seeds". **Core62** (core.wasm sha256 24aaa0e955f7c664..., core.js c8f6b928258c... unchanged) is live, identical to the scratch build.
> - **Seeds:** new `web/generated/set_piece_seed_{BHP1,CHP2,CRA3,EHP3}.hpp`. They add slot-1 seeds BHP1 63247, CHP2 203799 / 152343 / 231191, CRA3 332056 (eagle a) and EHP3 19756 / 28460. Old rows are verbatim and the rest is byte-identical.
> - **jsons:** `local/event-activation/{BHP1,CBA2,CHP2,CRA3,EBA3,EHP3}/set-pieces.json` updated too. They gain the tram `multisplines` (CBA2 152086, CHP2 226327, EBA3 91433, EHP3 38444; previously `[]`), which tools/export_peak_world.py event_moving reads at the next prepare. Backups are in the AI-parity scratchpad.
> - **Ravens / eagles (all real):** resident looping Splines whose own slot-1 program rebuilds them. A section leave destroys them.
>   - PS2 savestates: CHP2 203799 gone between 2820 and 3218, CRA3 332056 between 818 and 1218, EHP3 19756 between 3618 and 4018.
>   - Port now: 3161, 1161, 3881. Before, they flew on for the whole run.
> - **Blimps:** real seeds, but their owners' programs (BHP1 52495 / 39, CHP2 96279 / 82) return at `builtin52(blimp) == 1`, because the blimp is resident.
>   - `engine/section_streaming.hpp` Event gets `guarded`, and `web/section_gameplay.inc` skips `section_start_piece` for it: no VM run, no spline launch.
>   - Without this, the new seeds launched a second blimp Spline at tick 19.
> - **Check:** new `web/test-set-piece-seeds.mjs` (in test:all; core60 fails it). Every spline / position / MultiSpline entity at 33 kept PS2 states of bhp1 / chp2 / perpendiculous / cra3-full is exact.
> - **Gates:** 33 capture scenarios on these courses pass, sim-diff is identical, and stage-world / set-pieces / set-pieces-locations / attached-core pass.

> **Backcountry helis drawn in the world (pv `heliWorld`, off; the audit's list-parity bug; 2026-09-28, visual-parity agent):** see [set-pieces.md](set-pieces.md) "Moving lit instances", os609.
> - **PS2, derived capture** `local/ps2-capture/runs/heli/abc1-moved2` (happiness-ready, the heli instance moved 30 m in front of the camera by poke, silent): the PS2 draws the os609 LiveComp at flags 0x...305 and 0x...105 alike. An entity in the renderer's dynamic list carries 0x100 or 0x200 (the list's parity, 0x1030F4..0x103160), so the countdown audit's 'none' (0x50015205) was wrong.
> - At its real place the heli hovers about 5 m behind and above the start camera for the whole race, so no race frame shows it (PS2 and page alike).
> - `tools/export_event_membership.draw_class` accepts 0x200. A re-audit also unhides ASS1 ravensplineanimb, CHP2 blimpa / blimpads, BHP1 blimpads / blimplights, ABC1 snowsheet_1000 and the EBC3 summit pole; packages change only when re-prepared.
> - Page: `set-pieces-renderer.js` draws the heli meshes while their player runs; `cutscene-stage-sets.js adoptWorldCopy` keeps them hidden while an arrival set stands in.
> - Checked in Chrome and WebKit: the moved heli at the PS2's place and pose (the rotor phase differs); switch off, absent; ABC1 race frames unchanged. Tests: presentation, set-pieces-renderer, visual-parity, peak2-events.
> - Ready to switch on.

> **Avalanche trails drawn (pv `avalancheTrails`, off; 2026-09-28, visual-parity agent):** see [avalanche.md](avalanche.md) "Trails", Draw.
> - `web/avalanche-trails.js` evaluates particle entry 0xA00 (the rider snow's model) on the core's `avalanche_trails()` rings; fog0, GS 0x44, priority 7 in the encoded pass. Particles per birth = kernel N / ring slots.
> - The retail trails are nearly invisible on the PS2 too: the kernel colour base holds the flake colour with alpha 0 (tumbler +0x1E0), so sprite alpha is 0..2 of 128. PS2 RAM: EBA3 820 15 of 180 sprites with alpha 1, ERA5 none. Alpha-0 sprites are skipped (same pixels).
> - Checked against PS2 RAM (counts, cursors, rings, colours equal; birth seeds differ: presentation stream). No pixel change on the PS2 frames. `test-avalanche-trails.mjs` (new, in test:all).

> **Stage builtin 74 (DBC2 tunnel lighting) ported, PS2-exact (2026-09-28, AI-parity agent):** see [avalanche.md](avalanche.md) "Port status of these builtins". **Core60** (core.wasm sha256 0c3c8448fa0c964f..., core.js c8f6b928258c...) is live, identical to the scratch build.
> - **What:** builtin 74 sets rider+0x3FC, and 120F20 clears it. 2ED490 (race_end, after the stage triggers) eases the environment selector (x 0.9, + 0.1 while set). Above 0.1 the rider irradiance takes the alternate bank; before, the selector was 0.
> - **PS2:** the dbc2 tuck line held to 11000 enters a tunnel volume at 9830. New capture `runs/tunnel/dbc2-tunnel-ai` watches the block 0x4FA370.
> - **Check:** selector bit-exact on all 11000 records; human, Nate and the RNG exact. Gate `tunnel/dbc2-tunnel-ai`; `web/test-tunnel-lighting.mjs` (in test:all).
> - **Limit:** in a solo run the bank switch is one tick late, because the solo FX pass runs before race_end.

> **Moving set pieces the page drew static; rival beam and terrain sparkle layers (2026-09-28, visual-parity agent):** see [set-pieces.md](set-pieces.md) "Moving lit instances" and [visual-parity.md](visual-parity.md) 41.9.
> - **Object spline pieces outside Snow Jam now move** (packages deployed): the exporter wrote only names, so prepare.py never gave them moving batches while the core moved them. `tools/export_location_set_pieces.py` now writes `drawn_spline_pieces`. Newly moving: the ESS3 sled, the ASS1 train (6 cars), CBA2's 2 sleds, DRA4's 6 dragonworks + 2 rockets, EBA3's 2 rockets. ESS3 818 / 1219 / 1618: the core delta equals the PS2 Spline modifier +0x60 matrix within 1e-4.
> - Of the 15 lit movers: the BHP1 traffic already moved (MultiSpline seeds), the ospreys and cessnas are spline LiveComps (attached.json), the os609 helis stay hidden (the audit's draw_class needs 0x100; open until a PS2 frame shows them).
> - **pv `beamEncoded` (off):** the rival beam (priority 7) in the encoded pass, unfogged, in GS bytes; neutral on the only in-view frame (ABC1 2400, near the rider).
> - **pv `sparkleWorld` (off):** the terrain sparkle (priority 4) in the world pass before the fog; 13 aligned frames, 73 of 126 changed pixels closer to the PS2, 39 further. Ready to switch on.
> - Files: `tools/export_location_set_pieces.py`, `local/event-activation/*/set-pieces.json`, `rival-beam.js`, `terrain-sparkle.js`, `pv-flags.js`; docs set-pieces.md, visual-parity.md 41.9.

> **Metro flicker fixed (env pass depth bias); world-load hang bisected, no switch reproduces it (2026-09-28, blend-space agent):** see [visual-parity.md](visual-parity.md) section 43 "Depth bias on the pass".
> - **Flicker:** the env-map pass (pv `envMap`) z-fought its base pass in WebKit on grazing glass. `world-material.js envPassMaterial` now sets polygonOffset -1 / -1.
> - **Hang bisect (Safari freeze on a world load; the sweep harness stuck on EBA3):** the shared WebKit driver, on the dev tree and on a frozen copy of the production bundle. Setup:
>   - defaults, and -frame8 / -envMap / -cutsceneBytes / -sortedClass / -encodedBlend, on EBA3 and BRA2;
>   - a fresh load, then event -> PEAK1 -> event -> MOUNTAIN (peak run 7) -> event;
>   - plus 24 loads through the post-event transport ride with cutscenes on.
>
>   No hang, device loss, validation error or stuck GPU promise in any run.
> - **Still open:**
>   - WebContent RSS grows about 130 MB per world switch in every variant (1.45 -> 4.6 GB over 24 loads) and the GPU process 234 -> 716 MB. Not yet traced.
>   - `fog-renderer.js compileObject` warm compiles fail after a switch back to an event: "GPUDepthStencilState.format is required", because the world pass's depth-stencil texture has no GPU backing yet. The objects then build on their first draw.
> - **Tool and runs:** `local/browser-validation/blend-space/hang/hang.mjs` (runs beside it) (heartbeat, rAF vs timers, device.lost, uncapturederror, pending GPU promises, the driver's own GPU and WebContent CPU/RSS).

> **Avalanche trails emitters ported, bit-exact; their births now draw the presentation stream (2026-09-28, AI-parity agent):** see [avalanche.md](avalanche.md) "Trails". **Core59** (core.wasm sha256 294fd6347a14..., core.js 3982f195546b...) is live, identical to the scratch build. Full mirror capture suite 252/252, sim-diff identical.
> - **What:** every pool tumbler's colour dynamic emitter (+0xD0, vtable 0x4930D0): 0x371600 / 0x370DC8 (seed 2.0, the builtin-96 block) at the trigger, the flake colour as its colour base, then per tick the alpha into the kernel and 0x3717C0 -> 0x3710D0. The emitter is cleared at the release.
> - **Parity:** each live birth draws one presentation-stream value (0x4FF018): 3 a tick in EBA3, 8 in ERA5. The port made none, so its visual stream fell behind from every trigger.
> - **Checks:**
>   - Live oracle: every emitter (image, rings, colours) and the stream words bit for bit after every tick, 19 runs / 6852 ticks, 0 failures. The oracle corrects the recompiled div.s infinity for 1 / NumBlur to the EE's 0x7F7FFFFF.
>   - Core against the much-2-much-full savestate: bit-exact, except the packet words the draw rewrites and the solo run's birth seeds.
> - **Export for the draw:** `avalanche_trails()` (layout in web/avalanche_gameplay.inc).
> - **Also:** the core now resets its avalanche world in place. A whole-world temporary with the emitters overflowed the wasm stack; the first build crashed at load. Rider TLS grows ~35 KB per context.
> - `compare-ai-capture.mjs` takes a TICK_HOOK like compare-ps2-capture.
> - **Not ported:** the replay-restore catch-up points (+0x2EC); the restore is not wired.

> **A spent Start (pv `startConsume`, on; 2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 31 "A spent press".
> - **Owen's Safari + Xbox pad telemetry** (t93ez0j6): the BRA2 heat card went card -> game -> ctm-pause in the same moment.
> - **PS2:** the pause needs a new Start press with no overlay or transition (0x20CBE8 / 0x20CBA0). The press a card or prompt used is held, not new, once the overlay has gone.
> - **Cause in the port:** two pad loops with their own edges: gamepad-menus.js and main.js's frame. If the frame saw the press only after the menu had switched to the ride, the one press did both.
> - **Fix:**
>   - `gamepad-menus.js taken('start')`: a press sent as a menu key is spent until the button is released;
>   - `main.js`'s pad path skips a spent Start;
>   - the menus' synthetic keys (`ssxPadMenu`) never pause through the keyboard path. In Chrome, at-target listeners on window run in registration order, so a synthetic key could be judged on a stale screen.
> - **Checked:** `startmatrix.mjs` in WebKit and Chrome 17 / 17 (lodge, finish banner, card, pause items), and live WebKit 17 / 17. Tests: test-start-rules (the late-sighting order), test-gamepad, test-touch-controls.
> - **Not reproduced:** the exact interleaving (a shared fake pad cannot stage it).
> - **Files:** `pv-flags.js`, `gamepad-menus.js`, `main.js`, `test-start-rules.mjs`.

> **One draw order for every post-fog effect (pv `effectOrder`, off; released avalanche pieces; 2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) 41.9 and [avalanche.md](avalanche.md) "After the release".
> - **The rule:** the PS2 flush sorts its merged buckets by the key 0x364240 (priority, word1 sort mode -> t0, word0 bits 6..9 -> rank through 0x492010, texture handles incl. the second slot's low bits), ascending and stable (0x364050). `web/ps2-draw-order.js` holds it once: `drawKey`, `drawOrder` (renderOrder 660..720 by priority), each effect's fields (`EFFECT`), both FX texture-handle tables from PS2 RAM (event boot and Conquer the Mountain differ; `main.js loadCourse` calls `setDrawOrderWorld(course.freeRide)`), and the world's sorted-class constant (`WORLD_SORTED_ORDER`, which main.js's SORTED_CLASS_ORDER now takes).
> - **Converted behind the switch:** fog puffs, set-piece particles, start fire, wake, boost strips, aura, streamers, the '!' icon, impact chunks / sparks / glints / fist, snow, snowfall, camera splash, halos. Off = the old fixed orders.
> - **Checked:** `test-ps2-draw-order.mjs` (new, in test:all) against 115 sorted RAM buckets of 14 states (`local/reference/draw-order/buckets.json`): exact keys, RAM order. Page frames (ABC1 2000, DBC2 weather 1000, BHP1 1619 / 3219, ARA1 1219; Chrome and WebKit, camera pinned): every effect both draw is in the PS2's order. No pixel changes on or off in those frames (the moved effects do not overlap there). Ready to switch on.
> - **Open:** the set-piece particles' second texture is inherited (they get 'spec' in most frames, not all: the helper uses 'spec'); the rival beam and light glows draw outside the encoded pass; the terrain sparkle is priority 4 (before the fog) on the PS2.
> - **Released avalanche pieces:** the static collectors 0x22A5A0 / 0x229FC8 test only (flags & 3) == 3, residency and the frustum; 0x100 is not a test there. PS2 RAM: the EBA3 rocks at 0x40214123 are in that frame's static list. A piece drawn at the start is drawn again at its authored place after its release (ABC1 10 / 11 / 13, DBC2 71, ESS3 58); `avalanche-state.js` now keeps it. The core56 poses match the PS2's AvaSpline +0x40 (EBA3 820 .. 2819, Chrome).
> - **Files:** `ps2-draw-order.js` (new), `test-ps2-draw-order.mjs` (new), `pv-flags.js`, `fog-puffs.js`, `wake-renderer.js`, `boost-renderer.js`, `impact-fx-renderer.js`, `rival-beam.js`, `set-piece-particles.js`, `set-piece-halos.js`, `startfire-renderer.js`, `weather-renderer.js`, `snow-renderer.js`, `main.js`, `avalanche-state.js`, `test-avalanche-state.mjs`, `test-snow-renderer.mjs`, `package.json`.

> **Core58 fixes a core57 regression: avalanche definitions lost at the race start (2026-09-28, AI-parity agent):** core57 keyed the shared definitions by the location name (`setPieceLocationName`). A human whose avalanches.json loaded before its world, or a streamed world (which renames the location with every append), lost its own definitions at the next race start (`avalanche_info` [0, 0, 0, 0]: the rendering agent's ABC1 QA flow). **Core58** (core.wasm sha256 631c00d263fd71f6...) is live, identical to the scratch build.
> - **Fix:** a core that loaded its own definitions keeps them. The shared copy carries a generation, and a context that never loaded its own takes the latest.
> - **Check:** web/test-avalanche-collision.mjs loads ABC1 in both orders, resets the race and triggers 10. It fails on core57 and passes on core58.
> - **Gates:** the avalanche and AI gates are unchanged.
> - No loop anywhere on this path (sync, share, release, track reset are bounded).

> **The title's Start sound (pv `titleStart`, on; 2026-09-28, visual-parity agent):** see [audio-menus.md](audio-menus.md) "Title Press START".
> - **PS2** (`local/ps2-capture/menus/title-start`: call-log hooks on 294F78 / 2906B8, and a silent SDL-disk recording with the music channel zeroed): one sample after Start, the title state's notify 0x1946A8 plays FE event 15 (snd 7) and the menu's UINext accept plays event 0 (snd 3), on the same frame, both from SSX3Menu. The snowflake burst shows from +5.
> - **Why the port was silent:** SSX3Menu was fetched only when the audio unlocked, and the unlock was that same first press, so the voice found no bank. The port also never played event 15.
> - **Fix:**
>   - `game-audio.js` loads SSX3Menu at init (fetch and decode need no AudioContext);
>   - `ui.js leaveTitle` starts the transition, then plays events 15 and 0;
>   - `audio-menu.js` leaves the title to it.
> - **Checked:** Chrome (trusted key) and WebKit: the bank is in before the press, and both voices start in the press's handler. Tests: `test-title-start.mjs` (new), test-audio-sfx, test-game-audio, test-audio-menu.
> - **Files:** `pv-flags.js`, `game-audio.js`, `ui.js`, `audio-menu.js`, `test-title-start.mjs`.
> - **Open:** Chrome may not count a gamepad press as a user gesture; it unlocks the audio only through the existing pad path, which is browser policy.

> **Avalanches for the computer riders; the avalanche replay snapshot ported, not wired (2026-09-28, AI-parity agent):** see [avalanche.md](avalanche.md) "Open" and "Replay snapshot". **Core57** (core.wasm sha256 57e2a2c154812...) is live, identical to the scratch build.
> - **Computer riders:** their contexts loaded no avalanches.json, so their worlds kept the static rocks. Now the definitions a core loads are shared (`avalancheShared*`, on the rider-globals shared list), and a context of the same location takes them (`avalanche_sync`).
>   - parity-ai/era5 plays avalanche 28 in all six cores alike.
>   - The 26 gates on avalanche locations and sim-diff (ARA1 x 4 pads, ERA5) are unchanged.
>   - `compare-ai-capture.mjs` now reports `summary.avalanches` per core.
> - **Replay snapshot:** `originalAvalancheSave` / `originalAvalancheRestore` (0x2D9CB0 / 0x2D9D68), bit-exact in the live oracle: 8 playing states, then 60 ticks each; 19 runs / 1072 ticks / 6808 matrices in total.
>   - Not wired: the browser's replay re-simulates, so after an R1 / L1 skip it lacks the PS2's restore quirks (speed factor dropped, one tick of start-pose pieces). This is a known replay difference.

> **Colour space and translucency: what future agents need to know (2026-09-28, Xbox-texture agent):** see [visual-parity.md](visual-parity.md) sections 38, 38a, 43, 44.
> - **The frame holds encoded bytes (pv `encodedBlend`, on).** Every blend is the GS's own maths on bytes, within 1 level of the PS2 (`test-frame-space.mjs`: 16,384 blends, Chrome / WebKit, WebGPU / WebGL2). The old linear-light frame was off by up to 74.
> - **`web/frame-space.js` is the only place that decides colour space.** `test-frame-space.mjs` fails on any `sRGBTransferEOTF` / `OETF` / `SRGBColorSpace` elsewhere in shipped modules. For a new material:
>   - GS bytes -> `toFrame(bytes / 255)` as the output colour (the usual case: compute the PS2 combine in bytes);
>   - a pass reading the frame (a composite) -> `fromFrame(value)`;
>   - three's own linear-light maths kept on purpose -> `linearToFrame(linear)`, or `material.outputNode = linearOutput()` for a stock material, with textures in `linearTextureSpace`;
>   - a stock material sampling a texture straight into the frame -> `texture.colorSpace = frameTextureSpace`;
>   - `Color` values stay the bytes they were written as (`ColorManagement` is off with the encoded frame).
> - **Targets:**
>   - the world / sky pass targets are 8-bit (pv `frame8`, on; `frameBufferType`);
>   - the front end renders straight to the canvas with no output pass (`outputColorSpace` linear);
>   - effects drawn after the fog on the PS2 (priority 7: snow, wake, boost, beams, particles) stay in the encoded composite (`snow-composite.js registerEncodedEffect`), unfogged; everything else draws in the world pass and the fog composite fogs it.
> - **Static models:**
>   - blend classes from the material word & 0x60000;
>   - the env-map second pass (word & 0x660000 in 0x2x0000 / 0x6x0000; pv `envMap`, on) is tagged per batch by `web/world-batches.py triangle_env`. Any package re-split must keep it: `web/prepare.py`, `tools/export_peak_world.py`; `SSX_ENV_SPLIT=0` only for comparisons;
>   - class 1 draws before the depth-sorted classes 2 / 3 (render queue key 364240; pv `sortedClass`, off, verified: renderOrder 0.5 for classes 2 / 3).
> - **Cutscene sets / skies / PDA modulate in bytes** (pv `cutsceneBytes`, on).
> - **Checking colours against the PS2:**
>   - use aligned frames (`vpshot --pin`) and score only the pixels a change moves;
>   - hide batches by texture to attribute a gap;
>   - drive a derived capture with EE pokes (e.g. palettes painted magenta) to isolate one draw's contribution;
>   - frames with a large whole-frame error (BHP1 pipe runs, some ARA1 event-race ticks) are camera-misaligned, not colour.
> - **Tools:** `tools/vudis.py` (VU1 micro-mode disassembler); `local/browser-validation/blend-space/` (scripts, per-frame numbers).

> **Avalanche pieces' collision, bit-exact against PS2 rock hits; followers need their builtin-0 entity (2026-09-28, AI-parity agent):** see [avalanche.md](avalanche.md) "Collision". **Core56** (core.wasm sha256 ad4b7d71cb85c390...) is live, identical to the scratch build: full mirror capture suite 252/252, sim-diff identical.
> - **Before:** every collidable piece had its countdown seed (static route 0x20 at the authored place), so nothing threw; after the trigger the rocks answered where they started instead of where they tumble.
> - **PS2:** the trigger program's builtin 0 (Object, group 1) and 95 (AvaSpline 0x48F338 via 355A78, attach 3554B0) put the rocks on the entity route (0x40214145). Bounds are the modifier's +0x10 / +0x20; the hierarchy is composed on +0x40; the rigid predicate is 1; the selected callback does nothing. Each tick in group 1, 2D9C00 then 3568B0 (bounds = translation -/+ r).
> - **Order (from the records' watches):** the AvaSpline reads the tumblers *before* that tick's step. The port runs `avalanche_entities_tick` first in race_begin. A first build with it after the step touched a rock the PS2 missed by 1.8 m.
> - **Release** restores the static route at the authored place (34FBF0: 0x40214145 -> 0x40214123). EBA3's rocks (type 2) are never released.
> - **Followers (for the draw):** `moving_instances` / `avalanche_pieces` treat a piece as a follower only once builtin 95 attached, which needs the builtin-0 entity (0x305D90). ABC1 ava1bitB_1003 is no longer one. The released list keeps only pieces whose entity the release destroyed. The draw delta is the AvaSpline +0x40.
> - **Captures:** `runs/avalanche/eba3-rock-hit{,-a,-b}` (derived from the much-2-much countdown anchor, silent; watches of every rock's AvaSpline and instance header).
>   - -a / -b hit rockslide_1001 at 1113 / 1112: exact to the end. core50 leaves at the contact.
>   - Every rock bit-exact on every record (6390..6395 checks); the much-2-much-full savestates 30/30.
> - **Tests:** `web/test-avalanche-collision.mjs` (in test:all); gates `avalanche/eba3-rock-hit`, `-a`, `-b`.
> - **Not modelled:** the 3291E0 octree move, and the scope list's entity admission at its every-third-tick rebuild. Computer-rider contexts still keep static rocks (they load no definitions).

> **Equip Gear's load from the lodge (pv `equipLoading`, on; 2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 42 "Equip Gear's load".
> - **PS2** (`local/ps2-capture/menus/eqg-k*`, `eqg2-k*`, first and second entry, same frames): no white hold. The switch is at full white (+12), then Equip Gear under the fall with "Loading..." over the list: no rider, help line, 'equip btm left' dashes or row highlight.
>   - +37: all of those appear together, with "Loading..." still behind the rider for one frame.
>   - That is phase 3 of CharEquip (vt+0x30 = 0x1993A0: 19A238, 19E538(slot, 1), 186518) at the intro's 0x42 label + 2, not a disc load.
> - **Port** (`web/wardrobe.js`):
>   - The screen is up at the switch, and the outfit package builds behind "Loading...".
>   - The rider draws from phase 3 once loaded; "Loading..." goes a frame later.
>   - The highlight, the dashes and the help line wait for phase 3 (the help line also for the outfit package).
>   - `preloadEquipGear` is called while the lodge menu is up (`lodge-ui.js preloadGear`, `career-ui.js`). Setup Character's Equip Gear behaves the same.
> - **Before:** the white was held until the package was built: a 42-frame hold at 200 KB/s.
> - **Checked:** stepped frames in Chrome and WebKit against the PS2 (+16, +26, +34..+38, +41); the lodge and Setup flows including an equip reload; no errors.
> - **Tests:** R36; test-wardrobe, test-fe-screens, test-career-rider, test-career.
> - **Files:** `wardrobe.js`, `lodge-ui.js`, `career-ui.js`, `lui-flash.js` (comment only), `pv-flags.js`, `test-visual-parity.mjs`; docs characters.md.
> - **Open:** input between the flash and phase 3 is not gated. Setup Character's PS2 load length has not been re-captured.

> **Env-map second pass (pv `envMap`), cutscene bytes (pv `cutsceneBytes`), 8-bit frame targets (pv `frame8`), all off (2026-09-28, Xbox-texture agent):** see [visual-parity.md](visual-parity.md) section 43.
> - **Cause of the translucent gap:** env-mapped static models draw a second time.
>   - The material word & 0x660000 in 0x2x0000 / 0x6x0000 (37F2A4..37FD2C) selects it: GS context 2, the record's second texture (halfword +2), UV from the camera-space normal (VU1 program 3 UV mode 256 at 0xCE8, matrix 0x504760: u = 0.5 n.x + 0.5, v = -0.5 n.y + 0.5).
>   - ALPHA_2 is Cs + Cd or Cs x Ad + Cd. The port never drew it.
> - **Port:**
>   - `world-batches.py` `triangle_env`; `prepare.py mesh_env` and `export_peak_world.py` tag batches `env` and add the second textures. The event packages carry it since the lit re-export (not ARA1); the peaks get it at their next re-split.
>   - `world-material.js envPassMaterial`. Packages without tags draw no pass.
> - **Aligned PS2 frames** (30 frames / 9 runs, BHP1's misaligned pipe excluded), on the pixels the pass changes:
>   - Chrome: |port - PS2| 31.06 -> 27.56, bias +24.9 -> +0.7;
>   - WebKit: 31.21 -> 27.66, +25.1 -> +0.9;
>   - WebGL2: 31.12 -> 27.61, +25.0 -> +1.0.
> - **Cost** (Metro, WebKit): +2 pipelines; frame times and load the same, desktop and phone tier.
> - **cutsceneBytes:** cutscene sets, skies and the PDA prop modulate in bytes. EBC3 heli arrival luma MAD, Chrome 21.6 -> 20.7, WebKit 18.7 -> 16.6 over 3 frames.
> - **frame8:** 8-bit world / sky targets with the encoded frame.
>   - At most 2 levels off half-float except 24 stacked-glass pixels; no banding.
>   - -8.1 MB texture memory desktop, -2.2 MB phone tier.
> - **Helper bypasses closed:** the `?originalWorld=0` fallback and the old rival icon go through `frame-space.js linearOutput()`.
> - **Tools:** `tools/vudis.py` (VU micro-mode disassembler), `tools/export_env_split.py` (scratch check only). Test: `test-env-map.mjs`.

> **Public release: deploy config out of the repo, ELF-lifted and savestate code out, domain and personal paths gone, GPL-3.0 (2026-09-28, public-release agent):** see [hosting.md](hosting.md) "Deploy configuration" (and the sync command). Full notes, local-only: `local/public-release/public-release.md`.
> - **Deploy config:** host, host folder and launchd label come from the git-ignored `deploy/.env.local` through `deploy/env.sh`. The environment still wins, and `sh deploy/env.sh` prints the resolved values.
>   - `deploy-staged.sh` / `deploy.sh` are otherwise unchanged: a stubbed dry run gives identical command traces in 7 scenarios.
>   - The real `wrangler.toml`, plists and `.wrangler/` are git-ignored (`deploy/.gitignore`); `.example` templates are committed.
> - **Lifted code in `engine/generated/` (git-ignored):** `set_piece_particle_bounds.inc`, `lighting_math.hpp` and `irradiance_transform.hpp`, from `tools/test_set_piece_particles_lift.py`, `generate_lighting_math.py` and `generate_irradiance_transform.py`.
>   - `web/build-core.sh` generates them when missing (the lift uses `.venv` for rabbitizer). Include them as `generated/...`.
>   - The core is byte-identical (CORE_OUT builds before and after).
> - **Savestate data:** `engine/original_camera_oracle.hpp` (a savestate dump with no generator; only the native camera test uses it) moved to `local/reference/`.
> - **Keep the repo clean:**
>   - No `/Users/...` or session-scratchpad defaults: take the ISO as `--iso`, use `Path.home()` / `os.homedir()`.
>   - The site's domain is not named in code or docs; it lives in the local `wrangler.toml` / plist.
>   - New code lifted from the ELF goes to `engine/generated/` or `web/generated/`, never beside the sources.
> - **Licence:** the whole port is GPL-3.0-only. The 32 files adapted from SSX-Library are relabelled `GPL-3.0-only` (as upstream). The public repo root has LICENSE, README and CONTRIBUTING (kept in `local/public-release/root/`).

> **Avalanche playback in the core, bit-exact (2026-09-28, AI-parity agent):** see [avalanche.md](avalanche.md) "Port". **Core50** (sha256 a58cbe29...) is live, identical to the scratch build.
> - **Engine and oracle:** `engine/avalanche.hpp` covers the 0x2D97A8 trigger, the 0x2D7EF8 / 0x2D7CA8 / 0x2D5778 per-tick integration, the 0x2D7C00 envelope, release and the 0x2D9C00 AvaSpline matrix.
>   - New live oracle `tests/avalanche_live.cpp` (`tools/test_avalanche_live.py`, the recompiled original): 4026 ticks and 25817 matrices bit for bit on 11 EBA3 / ERA5 states, covering triggers, running slots, the fade-out and release.
> - **Core wiring** (`web/avalanche_gameplay.inc`):
>   - builtin 94 triggers;
>   - the step runs as group 3 after the entity pass (the end of `advance_world_entities`);
>   - `init_avalanches` loads the data through the new `web/avalanche-load.js` from `set-pieces-renderer.js`, `peak-set-pieces.js` and both comparers.
> - **Page-flow check:** new PS2 capture `runs/avalanche/eba3-slots` (watching the slot table and tumbler 0).
>   - The trigger is at record 541 in both.
>   - Slot t is exact on 878 ticks, and tumbler 0's matrix / alpha on 555 (through its release).
> - **Exports for the draw / audio:**
>   - `moving_instances()` carries the AvaSpline followers (key = resource, delta against the authored matrix);
>   - `avalanche_pieces()` gives the 0x2DA1C0 piece set and state, the released (hidden) instances and the 0x29DEF0 loop refcount changes. The per-tumbler sound cues are dead (0x29E560 is `jr ra`).
> - **Open:**
>   - the trails' emitter (0x370DC8 draws the presentation stream, 0x3717C0 points);
>   - the rocks' collision (still unsupported instances);
>   - save / restore;
>   - definitions in the computer riders' contexts.
> - **Tests:**
>   - Full test-ps2-captures on the mirror with core50: 249 scenarios, all pass. ctm/fr-dra4a-full's score now runs to 8403, from the tree's other changes.
>   - sim-diff against core47: identical.
>   - Also pass: test-peak-release, test-ctm-stream, test-set-pieces-locations, test-stage-collect, test-mountain-ride, test-peak2-events, test-presentation.

> **Sam's head rebuilt from an original roster head (2026-09-28, Sam agent):** see [sam_character/model/README.md](../sam_character/model/README.md) "Fourth pass". Owen approved the likeness.
> - **Head:** Mac's HeadA_NIS + Eyes_NIS + his "Skint" scalp (the original part that closes HeadA's cranium), warped to Sam.
>   - Landmarks are fitted on four photos with EXIF-focal-length cameras (1.7–2.8 px RMS). A biharmonic RBF warp maps Mac's landmarks onto Sam's, plus a cheek/temple sculpt fitted to the profile silhouette and design targets (long bridge, hooded eyes, narrow chin, slim neck).
>   - It keeps the roster topology, UV layout and head/neck/upper-spine weights.
>   - The 128x256 map is painted in the roster layout (the scalp is its bottom half) with baked soft shading; the photos give landmarks only.
>   - `tools/sam_head.py` (rewritten); `sam_mesh.py`, `sam_model_details.py` and `sam_wardrobe.py` fit the cap, beanie, hair, collar, glasses, side hair, afro and helmet around it. Mac's head-worn gear moves over through the same warp.
>   - `sam_textures.HAIR` is (44,30,22).
> - **Normals:** the authored normals go through the warp's Jacobian. Recomputed normals broke the split normals at the lids and lips, and the FE rim term (389CB8) drew them as pale bands.
> - **Size:** RIDER_SAM is 2,843 v / 3,621 t / 168 weight groups (was 2,806 / 3,788 / 171). The head is 700 v / 1,071 t; Mac's FE head + eyes is 597 / 887. Map memory is unchanged; the rig and bind matrices are unchanged.
> - **Assets:** 128 Sam files change, 26 are identical, none added or removed (RIDER_SAM* with fe/, WARDROBE/SAM including both texture sets, UI/sam-roster.png). They were exported to scratch and copied by the coordinator. `character-select.json` is unchanged (same silhouette layout).
>   - `textures-xbox.tex` must be rebuilt with every Sam texture change (`tools/export_xbox_riders.py --rider SAM`): it holds Sam's PS2 texels for the HD set.
> - **Tests:** `test-rider-skinning.mjs` and `check-camera.mjs` expect 168 Sam palette groups (was 171). That patch lands together with the assets.
> - **Checked (Chrome + WebKit, silent):**
>   - Select Character (default and `?pv=encodedBlend`, within 2.6 levels), Equip Gear, Equip Gear from the lodge.
>   - A Snow Jam race with head-bone close-ups through a ride, a grab and a crash; the replay Near-cam; the Snow Jam intro cutscene.
>   - 18 targeted tests, and `validate_sam.py` (40 native renders).
> - **Not done:** the PS2 disc was not rebuilt. `SamWardrobeParts.cs` still moves Mac's hat LODs with an affine `head_fit` (a least-squares fit of the warp), which needs an ARMSX2 check.

> **MultiSpline section count (the BRA2 bin traced), location collision release behind pv peakRelease, streamed static rails (2026-09-28, AI-parity agent):** see [set-pieces.md](set-pieces.md) "MultiSpline section count and the Object-entity destroy", [ctm-parity.md](ctm-parity.md) "The PS2's location release" (the memory), [avalanche.md](avalanche.md) (builtin 35). **Core47** (sha256 f0945ae4...) is live, identical to the scratch build.
> - **MultiSpline leave:** a section leave or enter only changes the modifier's +0x34 count (the port no longer calls `section_stop_piece` for actions 4 / 5, so trams stay live).
>   - The Object entity's update 0x356198 destroys it at count <= 0, or runs its slot-3 program instead. That happens in the next tick's entity pass, before the modifier update.
>   - This applies to the bins (builtin 0 + 20) only; LiveComp owners (builtin 3 + 20: the trams, BRA2 820240) are never destroyed this way.
>   - Traced with new PS2 captures (`runs/multispline/bra2-bin2` and savestates at 11338 / 11341 / 11343): bin 707856 leaves at 11341 and is destroyed at 11342. Texture chunk 53 stays resident, so eviction is not the cause. The port did it at 11341 before; now at 11342.
> - **Collision memory:** `peak_world_free_track(track)` frees an evicted location's terrain patches, instance nodes and grind rails. Instance and rail slots stay (held pointers); set-piece spline paths stay. A re-read refills the slots.
>   - JS side: `web/peak-world.js releaseCore`, and `web/free-ride.js` when a location's draw package is released (free ride only).
>   - pv `peakRelease` is **off** until page runs verify it. New `web/test-peak-release.mjs` (in test:all): ARA1 freed and fed twice, 1358 rail and 274 terrain probes as loaded, no slot growth.
> - **Streamed static rails:** the static copy is rebuilt after each appended catalog, without the grouped-off / bound rails. Before, it was dropped, so builtin 35's off state and RailModifier-bound rails were lost at the next append.
> - **Tests:**
>   - Full test-ps2-captures on the mirror with core47, 249 scenarios: all pass, no gate moved.
>   - sim-diff against core43: identical.
>   - Also pass: test-ctm-stream, test-peak-release, test-set-pieces-locations, test-collect-restream, test-stage-collect, test-mountain-ride, test-mountain-world.

> **CTM: the NIS rider hold (pv `nisTick`, on; core `nis_hold`), the slot 0x19 cash fix, the MOUNTAINF free-ride gate, the same-peak transport fix (2026-09-28, CTM agent):** see [ctm-parity.md](ctm-parity.md) "The NIS rider hold under the station cuts" and "The whole mountain".
> - **The NIS rider hold.** On the PS2 the world runs under the lodge-door and booth cuts, with the rider held by the cut's rider actor (123640: control 13 / motion 3 at anchor 19 / 28, ground-snapped). Only the prompt / map pauses it.
>   - PS2 evidence: door-no, 240 ticks, with the rider still held at the prompt (pre.p2s rider+0xAC4 = 1) and the release + 11D390 at the No. fr-booth2 has control-13 records every tick.
>   - Core (`web/core.cpp nis_hold` plus one-line guards in npc_gameplay.inc, input_bridge.inc and animation_bridge.cpp; `_nis_hold` exported in build-core.sh): inert until JS calls it.
>   - JS: `main.js nisHoldAt / nisRelease / nisResume` and `cutscenes.js anchorOf`. Behind nisTick, which falls back to the pause on a core without `_nis_hold`.
>   - Checked in Chrome and WebKit on a scratch core (second vite server serving CORE_DIR, port 5241). Door and booth hold at the PS2's x, y; the booth quaternion is identical to record 2644. No / Back place the rider as before.
>   - Open: the PS2's rider stands 15 cm below the snapped anchor (the clip's key 0, not applied). The transport's ride still pauses.
> - **HUD slot 0x19** (`engine/score_object.cpp originalScoreHud`, `OriginalScoreHooks::careerCash`, web/score_gameplay.inc): the career cash shows on the award's tick. 117FE0 reads C+0xAC4 after the tick's 119EF8, e.g. a combo expiry's $1 (fr-dra4a-full 7218). The full test-ps2-captures passes on its CORE_OUT build (247 scenarios).
> - **Gate `ctm/fr-dra4a-full`** (seed MOUNTAINF; the capture is linked under local/ps2-capture/runs/ctm): physics and bones exact through 8403, the score through 7217 (8403 once the slot-19 fix is live).
>   - Open at 8404: the PS2 lands on mdl_DRA4_highwayRebuild_2049 a tick earlier. It is not the rider scope.
>   - `tools/export_peak_seed.py --out DIR` exports a seed to scratch.
> - **`free-ride.js freeRideHolds`** (main.js cb.freeRide and ctm-transport.js switchesWorld): a free ride in a world that already holds the destination transports inside it. Fixes test-presentation's same-peak case under mountainRide.
> - For the physics agent's next live build: the core changes above (MOUNTAINF is already compiled). Then bump the gate's scoreThrough to 8403.

> **Draw paths the port missed: MultiSpline cables (pv `cables`, on), fog puffs (pv `fogPuffs`, on), the draw-path inventory, Peak 2/3 car seeds and the MultiSpline section-leave rule (2026-09-28, visual-parity agent, draw paths):** see [visual-parity.md](visual-parity.md) section 41 and [avalanche.md](avalanche.md).
> - **Cables** (`web/set-piece-cables.js`, hooked in set-pieces-renderer.js / peak-set-pieces.js): the black 1-pixel line strip every MultiSpline with builtin-20 key 10 draws along its path (0x35B418 -> 0x345430 -> 0x381F10 -> VU1 0x3BF0). Checked with the real VU program (`tools/test_cable_vu_native.py`) and PS2 frames (Snow Jam opening, EBA3, the BRA2 mill) in Chrome, WebKit and WebGL2. Deployed.
> - **Fog puffs** (`web/fog-puffs.js`, `tools/export_fog_puffs.py`): SSB kind-5 instances pointing to kind-4 puff sets, drawn by cPS2FogParticleMan (0x22A270 cull, 0x2DC190 sprites and fades, 0x2DBF98 sort far to near, fog0 with GS 0x44, priority 7 in the encoded composite).
>   - The model matches the last PS2 frame in 14 savestates over 12 locations: 534 sprites, 521 identical in draw order, 13 with the depth one off (EE rounding) (`web/test-fog-puffs.mjs`, in test:all).
>   - ARA1 4818 (the forest haze): MAD 39.6 -> 27.8 in Chrome and WebKit; CHP2 2820 20.5 -> 10.1, DBC2 4800 20.5 -> 12.7, EBA3 420 9.2 -> 6.8; no frame-time change in WebKit on either tier.
>   - Assets (25 `fog-puffs.json`) copied and switched on by the coordinator.
> - **Peak 2/3 cars:** construct replays bit-exact for CBA2, CHP2, EBA3 a/b, EHP3 (`tools/test_multi_spline_location.py` GROUPS). The seed headers are in scratch (`cable/cars/deliver`) for the physics agent. With them every PS2 MultiSpline state is bit-exact once the section leave stops releasing the lift.
> - **MultiSpline section leave** (for the physics agent): 0x30A460 -> 0x356CC8 -> 0x35AAE0 only counts modifier +0x34 down (never read), so the core's `Action::MultiSplineRelease` must not deactivate the lift. The Metro-City bin's disappearance at 11219..11618 has another cause, not traced yet. The JS cable life already follows the rule.
> - **Terrain glint** (pv `terrainGlint`, on; `web/world-material.js glintBytes`, `tools/export_terrain_glint.py`, `web/test-terrain-glint.mjs`): the patch "reflection" pass 38D168 (layer type 6): UV = (n, 1) x E with E checked against RAM in 5 states; on EBC3 frames it correlates with the PS2 better than every variant tried. Assets copied and switched on by the coordinator. Costs 4 bytes a vertex while on.
> - **Avalanches** (docs/avalanche.md): the runtime asset `<LOC>/avalanches.json` (+ PEAK copies; 14 files, copied); audio traced (only the rumble loop plays: bank 8 sound 2, bus 5, volume from the human's nearest tumbler; the per-tumbler sound calls hit the empty stub 0x29E560); prepare.py splits the AvaSpline pieces into moving batches (7 re-split packages to scratch `cable/ava/prep/deliver`, pixel-identical); `web/avalanche-state.js` (visibility by the builtin-0 key-2 rule + the rumble; pv `avalanche`, ready to switch on) verified with core50: EBA3 piece poses equal to the PS2 tumblers at 6 kept states (Chrome and WebKit), tumble / release / never-drawn emitter nodes on DRA4 and ABC1, rumble against PS2 RAM. Trails (0x2D8EA8 colour emitters) still need the core's emitter-ring export.
> - **Snow chunks in the forest** (section 41.8, pv `snowBuckets`, off): the PS2 flush sorts its render buckets by descending texture handle (0x364240 / 0x364050) = ascending FX texture id, so the rider's chunky sprays draw over the snow cloud; the page drew them under it (emitter index order). Verified on ABC1 2000 in Chrome and WebKit.
> - **Final sweep** (section 41.7): nothing new missing; one CTM item to check with a real ride (fr-aara1 4296 / 5046: ARA1 not resident in the page under vpworld's teleport).
> - **Inventory** (section 41.2): what is still missing, by visibility: avalanches (docs/avalanche.md, core first), plant sway (in-engine cutscenes only; skipped, a known gap), camera shake / scripted lightning / builtins 35 and 74 (with the physics agent).
> - **Corrected** in terrain-render-fidelity.md: the type-6 patches run the whole course (not only the start ramp), and 0x2DBF98 is the fog-puff draw.

> **Eviction at T+7, the path bank kept, stage builtins 91 / 92 / 35, camera shake pending (2026-09-28, AI-parity agent):** see [ctm-parity.md](ctm-parity.md) "The PS2's location release" (Built so far) and [avalanche.md](avalanche.md) "Port status of these builtins". **Core43** (sha256 082b7ef3...) is live in web/runtime, identical to the scratch build.
> - **Location eviction** (`web/peak_world.inc`, `web/peak-capture.mjs`):
>   - Confirmed with the new PS2 capture `runs/release/throne-evict`, which watches the octree removal counter `*(root+0xA0)`: EBC3 goes 5 -> 7 at 14422, the counter jumps at 14428 (T+7, before gameplay), and the row reads 0 at 14429.
>   - A row in 7 is no longer collidable from T+7 (`release()`); captures run the same countdown through `peak_world_rows_tick()`.
>   - The AIP bank is no longer dropped at 7 -> 0 (26ADA0 is `jr ra`).
>   - Still open: freeing the location's memory.
> - **Builtin 91, camera shake:** queued per call for this core's human and applied before the camera update (`request_stage_camera_shake`).
>   - **`requestShake` fix** (engine/original_camera.hpp): pending is set on every call that is not suppressed. Checked with the new gate `cam-boost-slow` (boosting at 1.7 km/h: the PS2 starts the shake at amplitude 0). It is exact on 599/599 camera ticks; the old core leaves at 650.
>   - The Throne's index-0 run at 4111..4138 matches the PS2 tick for tick. The port's extra run at 2405 comes from a trigger that a computer rider hit first on the PS2 (the solo comparer has no computer riders).
> - **Builtin 92:** a lightning strike for a human contact.
> - **Builtin 35:** RailMan group off/on (the DRA4 / ABC1 fallen-tree rails are no longer grindable before the fall).
> - **Builtin 74** (tunnel lighting): not ported.
> - **Tests:** full test-ps2-captures on the mirror with core43, 248 scenarios, all pass (the only new one is cam-boost-slow).
>   - sim-diff against core39: the only difference is ARA1 rand1 at 2763, camera word 244 (shakeWasPending): the pending fix.
>   - The deploy stamp needs the coordinator's live-tree run.

> **Encoded frame built behind pv `encodedBlend` (off; Owen chose option A) (2026-09-28, Xbox-texture agent):** see [visual-parity.md](visual-parity.md) section 38a.
> - **`web/frame-space.js`** (new) decides once per page what the frame holds. New materials must go through it:
>   - `toFrame(bytes / 255)` for byte-domain output;
>   - `fromFrame` to read the frame;
>   - `linearToFrame` for three's linear-light maths;
>   - `frameTextureSpace` for stock materials.
>
>   `test-frame-space.mjs` (in test:all) fails on any other `sRGBTransferEOTF` / `OETF` / `SRGBColorSpace` in shipped modules.
> - **With the switch on:**
>   - the frame holds encoded bytes / 255;
>   - `ColorManagement.enabled = false`;
>   - `outputColorSpace` is linear, so there is no front-end frame-buffer target;
>   - the fog and encoded composites read the frame without an OETF;
>   - world additives stay in the world pass, so `byteBlend` is moot.
>
>   The encoded-composite effects stay after the fog, since they are priority 7 and unfogged.
> - **Touched:** main.js (3 spots), world-material, fog-renderer, snow-composite, rider-material, fe-preview, opponent-riders, cutscenes, rival-beam, board-trail, crowd-2d, boost / impact-fx / snow / startfire / wake / weather renderers, set-piece halos / particles. Also the test pages rider-material-gpu-test and snow-colour-gpu-test, plus the new frame-space-gpu-test.
> - **Verified in both states:**
>   - the GS blend: linear mean 10.9, max 74 vs encoded 0.34, max 1, in Chrome and WebKit on WebGPU and WebGL2;
>   - all 12 GPU test pages;
>   - the 15 node tests of the touched modules;
>   - opaque race pixels unchanged (every changed pixel is a blend);
>   - Select Character: 1.4 % of the rider's pixels ±1 (rounding);
>   - MSAA neutral against the PS2.
> - **Blends against aligned PS2 frames:**
>   - world blend 1/2 and additives get closer where the models' own colour matches the PS2 (PS2 - port -4.9 → +2.6 Chrome, -7.4 → -1.1 WebKit);
>   - they get further where the port's translucent models are darker than the PS2 (by 4-7.5 levels overall, up to 45: Metro stadium glass, CRA3 gondolas / trees), a separate content issue;
>   - net |port - PS2|: 34.0 → 34.7 (Chrome, with Metro), 35.3 → 34.9 (WebKit).
> - **Cost:**
>   - WebKit frame and load times are within run-to-run noise, on desktop and on the phone tier;
>   - GPU textures are 12.2 MB smaller with MSAA and 3.2 MB smaller on the phone tier.
> - **Follow-ups:**
>   - the 8-bit world target (check fog and sky banding first);
>   - the translucent models' colour;
>   - the cutscene sets' linear-light modulate.

> **Air-release gates: pipe wall, Pro late spin, handplant after a flip; the departure-tick soft collision (2026-09-28, AI-parity agent):** see [obstacle-collision.md](obstacle-collision.md) "A soft collision in the departure tick". **Core change** (core39, sha256 9a4ffa73...; live `web/runtime` rebuilt, identical to the scratch build).
> - **New gates** in `web/test-ps2-captures.mjs`, all exact to the end (scripts `local/ps2-capture/scripts/air-release-*.json`, runs in `runs/air-release`):
>   - **pipe-wall** (The Junction glide, 1345 ticks): charged wall jumps with flips released near the wall.
>     - A back flip released at -89 stays in phase 1 until the 499 crash.
>     - A back flip released inverted at -192 crashes into the wall 16 ticks later (1127).
>     - Front and diagonal flips are held into the wall.
>   - **pro-late-spin** (the Pro map, `snow-jam-countdown-pro.p2s`, `--pro`; the build needs `SSX3_CAPTURE_DERIVED=1`, 1064 ticks). It covers the Cross + stick late spins:
>     - mode 2 -> 1 at -88 and a phase-2 auto-complete at -180;
>     - Cross released with the tilt held, then a crash;
>     - a flip+spin landing mid-rotation;
>     - a jump in late-spin mode 1;
>     - a held back flip into a crash.
>   - **handplant-flip** (the Snow Jam handplant spot, 625 ticks): a back flip right after the jump, with Circle held.
>     - 0x133308 tests the handplant only in air phase 3, and 0x107578 rejects an inverted rider (presented up z < 0) or one falling faster than 555.6 cm/s.
>     - The flip therefore carries the rider past the spot. A handplant can only follow a completed flip, and none of the known spots gives an air long enough for that (8 trial captures).
>   - **pipe-depart-soft** (The Junction, 1268 ticks): a body contact in the departure tick enters control 3 in the air (503).
> - **Fix** (`web/animation_bridge.cpp`, the 13F488 soft-entry gate): 108388 gates on motion 0 / 4, and the departed rider is still in motion 0 until 13F2CC. The gate is now `grounded || poseLanded || groundDeparturePending`.
> - **Tests:** the full `test-ps2-captures` on the mirror with core39 (247 scenarios) passed; sim-diff against core38 was identical (4 pads x 3000 ticks x 6 riders).
>   - The deploy stamp still needs the coordinator's live-tree run.

> **Lit static-model instances lit exactly (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 40. Not deployed; needs the 16 re-split world packages (their world.json / indices.bin also carry the env-map split already in web/prepare.py). Until they are copied, `test-visual-parity` R35 fails.
> - **What the PS2 does:** each lit instance's light-cache rows (2F5400) are the painter's object bank at its x/y plus up to 4 local lights ranked there (2F5AF0 / 2F5B68 with capacity 4 / 38A6A8).
> - **New code:** `engine/lit_instance_lighting.hpp` (new, not in the core build yet), `tools/lit_instance_rows.cpp`, `tools/export_lit_instances.py` -> `local/event-activation/<LOC>/lit-instances.json`.
> - **Checked:** 199 instances in 16 locations are bit-exact against the PS2 caches found in the savestates.
> - **Page:** `web/world-batches.py` / `web/prepare.py` give each lit instance its own batches tagged `lighting` (+5 batches in total). `web/world-material.js litWorldMaterial` uses one shared program, with the rows as object uniforms. Switched by pv `litInstances` (on).
> - **Results:** BHP1 crashbag box MAD 27.3 / 27.1 -> 19.8 / 17.9; with encodedBlend 31.9 / 31.7 -> 28.8 / 22.1.
> - **Correction to section 40:** runtime flag 0x1000 does not choose the bank. It makes the instance relight as it moves.
> - **Open:**
>   - moving lit instances: flag 0x1000, core `lit_instance_rows`, task d;
>   - the mountain world's lit instances.

> **Lodge states, round 2: cursors, intro timing, gear and Buy Attributes on the one flash (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 42 "Round 2". Not deployed.
> - **pv `stateCursor`** (on): the lodge, Rider Details, Buy Attributes and Trophies' mountain room reopen on the item last used. The PS2 stores each state's menu index on exit (0x1865A8 -> 1A0708) and these states restore it (0x186518). Implemented in `career-ui.js` (`cursorMemo`).
> - **pv `introLead`** (on): a screen opened by a lodge state change shows its intro from frame 2 (two LUI updates in the new state's first pass, 0x39ED4C / 0x39EED4). A restoring state's focus shows 2 frames after its intro label (frame 25). This fixes Rider Details' menu text coming up 2-3 frames late (PS2 tout-detD).
> - **Routing:**
>   - Equip / Buy Gear (and Square = Buy Gear) now go through the flash. While the gear screen loads, the flash holds at full white.
>   - Buy Attributes uses `lui-flash.js`; the constants moved there and `buy-attribs.js` re-exports them.
>   - The Player Name keyboard and Cheat Characters are **not** routed. On the PS2 they are child states pushed with 0x39F290, with no TransitionOut, and PS2 tout-kbd shows no white on either opening or closing the keyboard.
> - **Files:** `lui-flash.js`, `buy-attribs.js`, `career-ui.js`, `lodge-ui.js`, `wardrobe.js`, `fe-screens.js`, `trophy-room.js`, `save-game.js`, `audio-menu.js`, `pv-flags.js`. Test R34. Tool `tools/lodgecursor.mjs`.
> - **Measured with encodedBlend on.**

> **The lodge's screen changes play the PS2's TransitionOut flash (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 42. Not deployed.
> - **The PS2 (decoded):** a lodge screen change is an FE state change (0x39F400).
>   - The old state's LUI plays TransitionOut: control 0x30 starts `transition_flash`, white up over 10 frames.
>   - The end label's 0x41 (0x39CE98) makes the switch.
>   - The new state's intro plays under the 9-frame fall, and its menu focus plays from the intro's 0x42 label (frame 25).
> - **The PS2 (frames):** new captures `local/ps2-capture/menus/tout-*` (Trophies' Triangle, the lodge's Rider Details and Save Game) give identical frames.
> - **Port:**
>   - new `web/lui-flash.js` (one shared object, `careerUI.lodgeFlash` / `lodgeGo`), pv `lodgeFlash` on;
>   - routed: the lodge <-> Rider Details / Save Game, Rider Details <-> Trophies / Highlights, the three trophy rooms, the save screen's return, Rewards / Ubertrick Setup / Rider Profile opened from the lodge (`fe-screens.js`), Music's fall (`audio-menu.js`, one draw line);
>   - no input during the flash;
>   - the lodge and Rider Details replay their intros on each entry, with the focus from frame 25.
> - **Results:** MAD to the PS2 frames 5.9 / 3.0 / 0.8 / 1.6 / 3.6 (rise, full white, fall with the intro); WebKit is the same within the harness's precision.
> - **Open:**
>   - the PS2's per-state menu cursor memory (0x1865A8);
>   - Equip / Buy Gear, the Player Name keyboard and Cheat Characters are not routed;
>   - Buy Attributes keeps its own copy of the same flash (`buy-attribs.js`);
>   - Rider Details' menu texts come up 2-3 frames late.
> - Tests R33 (R30 updated). Tool `tools/lodgeflash.mjs`.

> **Lit LiveComp instances: the Gravitude billboard's dark board is lighting, not culling (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 40. Not deployed; needs the re-exported `web/public/assets/ERA5/LIVECOMP/livecomp.json` and `CRA3/LIVECOMP/livecomp.json` (they add `lighting` to the lit instances). Until they are copied, `test-visual-parity` R32 fails.
> - **No back-face culling on the PS2:** VU1 program 3 (the static / LiveComp draw 37E238) has no facing test, so `DoubleSide` stays.
> - **Poses are exact:** new PS2 states `runs/billboard/grav-bb-states.tick1209 / 1230` show the billboard's node matrices bit-exact with the page's.
> - **The cause:** the billboards are lit instances. Their runtime flag 0x4000 equals authored flag 0x40000000 in all 35,349 audited instances. 37E238 -> 2F5400 lights them per vertex from the object bank (EOBR1 in their PS2 light cache) on the node-rotated normal. At 1208 that makes the poster near black.
> - **Port:**
>   - `tools/export_livecomp.py` adds a `lighting` record for these instances.
>   - `web/world-material.js litWorldMaterial` builds the lit colour (Cs = T x L >> 7, through `toFrame`).
>   - `web/set-pieces-renderer.js` applies it (pv `litLiveComp`, on).
> - **Results:** grav-bb 1169 / 1192 / 1208 / 1229 luma MAD 20.2 / 24.0 / 28.1 / 23.3 -> 15.5 / 21.5 / 24.1 / 22.4; WebKit within 0.3. CRA3's falling tram (COBR1) renders lit.
> - **Open:**
>   - static lit instances (crashbags, rockslides; their batches are not split per instance);
>   - ABA1's lit cars in the mountain world (needs the peak re-export and a `peak-set-pieces.js` hook).
> - Test R32.

> **The FE popup box is decoded, not fitted (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 39.
> - **New module `web/fe-popup.js`:** cFEPopup's layout (0x1C6B08 .. 0x1C7C20: message wrap and font box, EnterTextBox and option heights, scale (w + 20) / 504 x 1.05 by (h + 15) / 204, y 92 + h / 2 - 7.5, the menu centred on the widest option) and cFEPopupConfirm's 25-frame opening.
> - **Checked** against the live popup objects in PS2 RAM (save-s3/s4/s7/s8, lodge-quitprompt / saveprompt; values exact) and against opening frames with states.
> - **Save popups** (`save-game.js`) now use it: MAD 2.6-4.0 (the fit: 3.2-4.6).
> - **pv `fePopup`** (on; `fe-screens.js drawLodgePrompt`): the lodge's Yes / No questions went 11.9 / 11.5 -> 2.6 / 2.4.
> - **Options > Save/Load "Save game"** (`fe-saveload.js`, pv `lodgeSave`) opens the Save game screen and returns to Save/Load.
> - **Not cFEPopup:**
>   - 139buy_popup is cUIStateBuyPopup, with its own LUI box; nothing changed there.
>   - The Options save question's class is not traced yet and stays fitted.
> - Test R31 (R30 updated).
> - Pre-existing failure, not mine: `test-presentation.mjs` fails at `rideWanted(peak1, inFreeRide, 18)` ("another station of the same peak") in the live tree.

> **Rider draw state on; linear vs encoded blending measured (2026-09-28, Xbox-texture agent):** see [xbox-textures.md](xbox-textures.md) section 9 and [visual-parity.md](visual-parity.md) section 38.
> - **pv `riderDrawState`** is on. WebKit event load, 6 alternating loads each way: median 7636 ms off, 7681 ms on (noise). It adds 8 pipelines (82 -> 90), all compiled before the objectives card. Checked in Chrome and WebKit: Select Character, Equip Gear and a six-rider race.
> - **Blend space (nothing changed; Owen decides):** the world pass blends linear light, while the GS blends encoded bytes. The same GPU draw, read back in Chrome and WebKit on WebGPU and WebGL2, against the GS formula:
>   - today: mean 10.9, max 74 levels;
>   - an sRGB-format target: the same (the hardware blends linear);
>   - encoded values in a half-float or 8-bit unorm target: mean 0.34-0.41, max 1.
> - Particles, snow, boost, fog and the HUD / LUI already blend encoded. Still linear: world translucents and additives, rider hair edges, the board trail, crowd flashes and sky blends. On their partial-alpha pixels the error is p95 38-76 levels; they cover about 1 % of a typical frame (3 % on additive-heavy views).
> - Options A (port-wide encoded world pass), B (per class, as `byteBlend`) and C (leave it) are in section 38.
> - Evidence: `local/browser-validation/blend-space/`.

> **CTM station details from PS2 captures: the lodge door's No, the booth DJ flag, peak-run Restarts, relationships after New game (2026-09-28, CTM agent):** see [ctm-parity.md](ctm-parity.md) "Stations and heats". JS only.
> - **pv `doorNoPlace`** (on): No at the lodge prompt places the rider at the station's session point 0 moving at forward x 833 cm/s, as the PS2 does (new capture `local/ps2-capture/ctm-parity/door/`: tick 636, (-65782, 33404) at Green). The port did this before stationFlow; stationFlow had dropped it on a misread of 234E20. Chrome and WebKit match the PS2 to the centimetre at tick 637. Under the cut the PS2 holds the rider at the NIS locator (control 13, no speed); the port's pause is equivalent on screen.
> - **pv `boothDj`** (on): the booth's audio flag (2A49E8: audio+0x5818 for 30 s) keeps a DJ line playing over a transport (2A4A38 in the travel code 28E8C0 20). PS2 booth capture (`ctm-parity/booth/`, the autopilot from Happiness to Green's booth): the flag set at the booth trigger. game-audio.js / audio-speech.js; test-audio-timeline 7.
> - **pv `ws13Rival`** (on): a peak run's results Restart takes WS13's rival branch (event type 5 / 6: no gondola, no gate lists, the card).
> - **WS14's hold**: PreRace's per-tick update is empty; the PS2 NIS holds the rider at the locator. No speed change to port beyond the No placement.
> - **pv `relAging`** (on; ai-race.js, narrow): relationships age at a Single Event / multiplayer load or restart (WS1, 0x234894) and at every race's end (0x233F08), not at every start: a CTM race ages once, at its results. Chrome and WebKit; test-ai-racers and the seven parity-ai capture gates pass.
> - **A peak run's Restart** on the PS2 reloads the world (WS7 -> 6 -> load -> 10 -> 1 -> 2, the card): new capture `ctm-parity/restart/`. The port restarts in place (known difference: no load screen).
> - **ai-race.js** (narrow, pv newGameReset): an ai-race instance already loaded at New game reloads bank 0 of its relationship tables at its next start (`relationshipsReset`, called by fe-saveload.js) instead of saving the old ones back. Chrome and WebKit: 100 marked records -> 0 after New game and the next start; switch off: 100.

> **The lodge's Save Game is the PS2's Save game screen (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 37.
> - **pv `lodgeSave`** is on (`web/save-game.js` new; `web/lodge-ui.js`, `web/career-ui.js`). It follows cFEStateProfileLoad mode 3 on FE.LUI 93profile_load: the card's row, the name keyboard, then the memory-card popups (Checking, overwrite? with No focused, Saving, Save complete. / Continue back to the lodge).
> - The popups are fitted to the PS2 frames: box by lines, options and longest line, and the PS2's gradient veil.
> - **MAD** vs the PS2 is 3.2-6.0 (the old screen: 55.3), in Chrome and WebKit.
> - **No asset change.**
> - **Owen's call:** the PS2's rows 2..6 and Delete are not drawn (one browser save; autosave).
> - **New PS2 captures** with a memory card: `menus/save-s1..s8`. `tools/ps2_menu_capture.py` now takes `MEMCARD=` / `MEMCARD_OUT=` and retries snapshots while the card is busy.
> - Tool `tools/savegame.mjs`. Test R30.

> **The lodge's Trophies are the PS2's three FE.LUI screens (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 36.
> - **pv `trophyLui`** is on (`web/trophy-room.js`, `web/lodge-ui.js`). It draws the three screens:
>   - 125mountainroom: the peaks over the mountain with a medal marker per event, and the Peak Pass popup;
>   - 126peakroom: the goals, with each goal's events, medal icons and checks, and the trophy thumbnails;
>   - 127trophyroom: the trophy, or the event's medal picture with its best time, score or earnings.
> - These follow 0x1D2990.. / 0x1D3890.. / 0x1D4368...
> - **Not deployed.** It needs the re-exported `web/public/assets/UI/character-select.json`. That export adds the three screens,
>   `trophy_sprites` and the strings; the existing screens only gain labels. Without it the old list is drawn.
> - PS2 captures `menus/trophy-*` (patched medal / best / earned bytes). MAD after the change is 5.0-7.7 against 47-52 before
>   (Chrome, WebKit), and the popup timing matches.
> - Tool `tools/trophy.mjs`. Test R29.

> **Xbox HD rider textures built as an option; reward pictures fixed (2026-09-28, Xbox-texture agent):** see [xbox-textures.md](xbox-textures.md) section 8. Deployed by the coordinator (62 archives copied, sha256 OK); **pv `xboxRiders`** on: Xbox HD by default on desktops, PS2 on iOS / Android / the low tier, and Options > Display & Touch "Texture set: PS2 / Xbox HD". test-texture-archive-decode / test-fe-screens pass with the switch either way.
> - **Assets:** `tools/export_xbox_riders.py` -> `WARDROBE/<ID>/textures-xbox.tex`, `gear-xbox.tex`: 3,759 Xbox DXT1 / DXT3 entries (2x the PS2 texels, the Xbox's own blocks behind a 16-byte 'SXBC' header), 83 PS2 entries kept (Sam's 65, 4 other layouts, 14 without gain). 247.6 MB raw; brotli 40 MB (gzip 126 MB, PS2 55.5 MB): `server/precompress.mjs` writes `.br` for `*-xbox.tex` only and `mp-server.mjs` serves it to br clients (`test-precompress.mjs`).
> - **Browser:** `web/texture-archive.js` reads the twin while `quality.riderTextures` is 'xbox' (falls back to PS2 if it is missing); BC entries decode in the new **texture-decode worker** (`web/bc-texels.js`, `texture-decode-job.js`, `texture-decode-worker.js`): a CompressedTexture with the stored blocks + a mip chain on WebGPU with `texture-compression-bc` (Chrome and Safari on Apple silicon), exact RGBA texels otherwise (WebGL, `?bc=0`). `rider-material.js` / `fe-preview.js` halve the 'xbox' domain, so colours equal the PS2 set; UVs, materials, alpha test and blending unchanged. Rider archive cache also held to 96 MB (32 MB and 3 archives for HD on a phone). `?riders=ps2|xbox` for QA. `main.js`: one call (`configureTextureDecode`) after the renderer init.
> - **Checked (Chrome + WebKit, silent):** Select Character, Equip Gear, the lodge, a six-rider Snow Jam; every BC level read back from the GPU within 2 of the JS decoder (`web/bc-texture-test.html`, in `test-texture-archive-decode.mjs` when the archives exist); race rider textures 2.35 MB GPU (PS2 4.1 MB; HD as RGBA 16.4 MB); event load and frame times the same as PS2; no long task cycling Select Character. Targeted tests 13/13 on the live tree.
> - **Reward pictures:** `export_career.export_reward_images` reads the full GS CLUT; 13 medals / sketches re-exported (holes filled), copy script in scratch.
> - **Rider draw state (pv `riderDrawState`, off):** see xbox-textures.md section 9. 37A610 -> 363C20 -> 3626D8: `alph` / `ea*` materials (flag 0x8000, bind 0x386920) blend with GS ALPHA 0x44 and ATST GREATER 92 / AFAIL FB_ONLY; all others ALPHA 0x2A, ATST ALWAYS. PS2 frames with the VRAM CLUT alpha changed: suit alpha 0 changes 0 pixels, hair alpha 0 hides the hair. `web/rider-material.js riderDrawState` (main.js asset, opponent-riders.js, fe-preview.js): opaque materials alphaTest 0 with alpha 1, `alph` / `ea*` two blended passes (Z above 92). The port matches both experiments with it on (before: suit alpha 0 hid the suit). New `test-rider-draw-state.mjs`. Open: linear-space blending (hair edges lighter than the PS2); WebKit event load +0.9-1.7 s in 2 of 3 runs, to check before turning it on.

> **The whole mountain in free ride, world audio residency, bank eviction, reward RNG, the PS2 release rule (2026-09-28, CTM agent):** see [ctm-parity.md](ctm-parity.md) "The whole mountain" and "The PS2's location release". No core change.
> - **pv `mountainAudio`** (on, deployed): web/game-audio.js `isPeakWorld` includes MOUNTAIN: the All Peak Race's slots 8 / 9 follow the rows (EBC3 at the start, then E, ERA5, C, CRA3, D, DRA4, A, ARA1, B, BRA2; before: B's banks from the start), JS heap at the start 150 vs 288 MB (44 location banks were decoded at the load).
> - **pv `bankEvict`** (on, deployed): web/sfx.js drops a bank's decoded patches / AudioBuffers 30 s after no slot holds it (a re-load within that keeps the cache). No slow decode, dropped voice or late bar riding Happiness -> ARA1 (Chrome, WebKit). test-audio-glitches.
> - **pv `mountainRide`** (on): the career free ride in the MOUNTAIN world on desktop (phones: iOS / Android / quality=low keep the per-peak worlds + crossWorld; `?mountain=0|1`). The bottom of Intimidator / Gravitude streams into Green / Yellow station with no load screen and no stalled frame (Chrome, WebKit); in-world "Go to this peak now?" plays the heli and a first visit's movie; the career (visited, last lodge, the MCOMM peak), collectibles on both peaks, cash amounts by peak checked in Chrome. Memory at the phone tier (WebKit): +22% WebContent / 2x wasm when every location's collision is fed (the core never releases it): the reason for the tier split.
> - **WebKit Gravitude** (the coordinator's 83 frames over 50 ms vs 0): the whole mountain's window prefetch fetched and fed the uphill rows (E) for nothing: skipped in a MOUNTAIN free ride with streamAhead. Run back to back at an equal machine load: MOUNTAIN 2 over 50 ms on the whole ride (0 at the crossing, max 42) vs PEAK3 1 (without the skip: 6, 2 at the crossing). The 83 was mostly other agents' load (load average 20-29). Happiness -> Green in MOUNTAIN: Chrome 1 / WebKit 0 over 50 ms. Career flows in MOUNTAIN (Chrome): a new career (the ABC1 movie, the plane, Happiness), careerReload, an event round trip (C -> Ruthless Ridge race package -> the post-event Transport to Green in MOUNTAIN), collectibles on two peaks, "Go to this peak now?" first visits.
> - **pv `ctmSmallFixes`** (fix inside the deployed switch): an in-world transport no longer pre-sets the career's course, so the arrival (WS10) marks the location visited and makes a station the last lodge (with ctmSmallFixes on, an in-world transport set neither).
> - **pv `rewardRng`** (on): reward / gear picks draw 0x3177F0 (the presentation generator 0x4FF018; web/lineup.js presentationDraw), not a saved xorshift; same formula (r = draw % unowned, the r-th unowned). test-award-cascade.
> - **pv `unlistedPickup`** (on, inert): JS side of the unlisted-pickup cash (30B9A0 pays 10F338 -> 119EF8 -> 150A90 without a bit); the core side (`stage_collectible_award` queuing index 0xFFFFFFFF) is for the next core build.
> - **PS2 free-ride crossing capture** (`local/ps2-capture/ctm-parity/mountain/`, Intimidator -> Green, Zoe, the last lodge poked to D): the DRA4_A Unload (record 14732) changes the course and enters world state 11 with the rider riding on (no load screen, steps within its speed); world state 10 comes at the DRA4_A Load (19215: the last lodge 20 -> 17), world state 4 the next record (the visited bit). New `web/test-mountain-ride.mjs` replays its triggers through the core's MOUNTAIN streaming in free ride: every row on every record (852,389 row states, 20 read-end skews), the course, and those records. **pv `crossingArrival`** (on; Chrome and WebKit: course 17 at the Unload, the last lodge 17 at the Load ~27 s later, as the PS2): the last lodge / visited bit / reward record clear of a riding crossing at the Load trigger (free-ride.js 'arrive' -> career-ui.js crossingArrived), not at the Unload.
> - **pv `newGameReset`** (on; Chrome and WebKit): Options > Save/Load > New game as the PS2's 0x18D4F4 (profile 0 only): the ten career blocks as at boot, the player name PLAYER 1, the rider Zoe, relationships (bank 0), free-play outfits and cheat characters reset; the records (the options file) and the options kept (before: the records reset, the rest kept). web/fe-saveload.js; new web/test-new-game.mjs.
> - **Release spec** for the physics agent (ctm-parity.md): 3A8528 -> 3A8E20 -> 3AB498 unlinks a location's patches / instances / rails / lights from the octree 5 frames after its page goes unwanted (T+7, before gameplay), 3A8230 returns its Section Allocator's blocks (pool 2: 0x35 x 128 KB) at T+8 (row 7 -> 0). Two port differences found: the AIP bank is NOT dropped at the eviction (12A490 -> 26ADA0 is `jr ra`; web/peak_world.inc drop_paths does), and the octree removal is one tick before the row reads 0.

> **Xbox SSX 3 textures evaluated; only rider textures gain (2026-09-28, Xbox-texture agent):** see [xbox-textures.md](xbox-textures.md). Evaluation only: no web code, no assets. Owen's own Xbox disc image is extracted to `local/xbox/` (new read-only `tools/xdvdfs.py`; SHPX / world kind-9 decoder `tools/xbox_textures.py`).
> - **Riders:** 3,749 of the port's 3,842 rider textures (suits, heads, boots, boards, hair, hats) are 2x per axis on the Xbox (DXT1 / DXT3), same art and UV layout (gain 2.0 = PS2 half intensity, PSNR 35 dB, corr 0.997), with real extra detail (2.4x the energy an upscaled + BC1-recompressed PS2 texture has). 4 have a different layout (excluded), Sam's 65 have no Xbox copy. Cost: +21 MB memory in a six-rider race; download 52 -> 110 MB (DXT + brotli) for all riders, 2.2 -> 4.5 MB for the default outfits.
> - **Everything else** (788 world textures, sky, UI atlases, fonts, pictures, gear icons, FX) is the same resolution on the Xbox, DXT-compressed: equal or worse.
> - **Black specks on rider hair / hats fixed in the exporter (not deployed; archives in scratch for the coordinator):** see xbox-textures.md section 6. Cause: `export_career.shps_image` read an 8-bit CLUT only up to its count and zero-filled the rest; 535 PS2 TXP textures index entries past the count, which the GS gets (the full 16x16 CLUT from the block data: checked in GS VRAM of a Select Character savestate, and by recolouring those VRAM entries: magenta dots in Allegra's hair). `shps_image(gs_clut=True)` in `ps2_texel_rgba`: 532 port textures (60,065 texels) change RGB from black to the PS2 colour; alpha and every other texel unchanged. 22 archives (WARDROBE/<ID>/textures.tex / gear.tex). Rider draw state recovered too: `alph` / `ea*` materials (flag 0x8000, 0x386920) blend 0x44 with ATST GREATER 92, all others opaque 0x2A with ATST ALWAYS. Open: 17 reward pictures (RWRDPS2) have the same issue (`export_reward_images`).
> - **Waiting on Owen:** whether to export an "Xbox HD" rider texture set (Options > Display & Touch, default PS2, pv switch).

> **The freestyle finish panel is OV.LUI finishov; HUD per mode checked in the real flow (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 35. Not deployed; needs the re-exported `web/public/assets/UI/audio-menus.json` (adds OV.LUI finishov and kT_OVRCMNPointstotal; without it the old box draws). **pv `finishLui`** (on; `web/career-ui.js finishLui`, `tools/export_audio_menus.py`): the panel after FINISH! as 0x1E8200 sets it up (run label by round, Nth place, pts, the medal of a final / Rival Points career run, New Record!), the notched 3D Ov frame growing in. PS2 pipe-finishov2 panel MAD 12.6-39.7 -> 2.1-8.4 (Chrome, WebKit). New tool `tools/modeshot.mjs` (the real Single Event flow against a PS2 event capture, own-camera shots with the HUD). Test: R28 (R22 regex relaxed).

> **Computer riders exact on four more six-rider captures (2026-09-28, physics-parity agent):** see [ai-racers.md](ai-racers.md) "Parity fixes, 2026-09-28". Live core rebuilt (08:5x, sha 8c3a013d, with the CTM agent's builtin 59). No pv switch: core fixes checked against PS2 captures.
> - **Scope list per roster slot:** the ground query 13D818 and body queries 13F488/13AA48 see only the rider's 332DB8 scope list. 120F20 rebuilds it at tick % 3 == rider+0x86C % 3, and every 11D660 placement rebuilds it. Metro race: Luther 506 / Moby 554 / RNG 563 → all six riders, RNG, ranks, pair records and score to the end.
> - **Landing stance restore:** 139C88 runs 115640 on every landing except board presses. A soft collision off a rail keeps stance 4 to touchdown (ESS3 Moby 815).
> - **Air query filter:** 13AA48 filters with the +0x180 of the same tick's pose (121728 poses before 121750 moves). ESS3, DSS2 and ARA1 are now exact to the end except ARA1 Luther from 3799 (was human 3799, riders from 1644, RNG 2404).
> - **Departure seed:** 13F178 seeds the flight (13F2CC 1135B8) before its 13F358 speed clamp, so the predictor starts from the unclamped velocity. ARA1 Luther 3799 → the whole Snow Jam capture exact to the end. DRA4 human 835 → the end (physics gate, bones and score too). ERA5 Luther 1472 → 1804. CRA3 rider 3 1869 → 2137. DRA4 riders 2/3 gates lowered to 3141 / 2955: they now run on a shared RNG that already differs (2640). Live core rebuilt again (sha e4ca0e7b).
> - **Crash fixes (live core 437304d8):** a rider crashed by an earlier rider's pair keeps +0x390, because the 13F2E0 board-normal update belongs to 13F178. A sliding crash bounce (138640) no longer dispatches 105D98, so the collision history and impact flag stay. Gravitude Nate/Allegra 1129 → 1232/1273. ASS1 Moby 3673 → 5035, human and RNG exact to the end (new gate `parity-ai/ass1`).
> - **Stale soft-frame flag (live core 77207a22):** a rail taken right after a soft collision kept 115D48 (the idle clock +0x35C) off, because `browserSoftFrame` survived on ticks that return early. CRA3 human 1384 → 2279, RNG 1152 → 1696. Gate moved; Psymon/Moby lowered, since they now leave after the RNG difference.
> - **Landing rider's pair view (live core 9f426a88):** the pair system saw a just-landed rider's pre-touchdown spheres. CRA3 six riders, RNG, ranks and pair records are exact to the end (were 1384 / 1152 this morning). Gate raised to the end.
> - **In-race relationships (no core change; pv `rivalRelations`, web/ai-race.js):** events without lineups.json (backcountry rivals, Gravitude, Kick Doubt) now keep the relationship tables, so rider-pair reactions raise levels as on the PS2 (0x155BF0), and 10F560's peer-reaction flag follows. compare-ai-capture applies them too (`--no-relations` for the old behaviour). bc-race-tuck2 and ko-attack are exact to the end (gates raised; new test-rival-page case `abc1-tuck2`).
> - **Air orientation after a rail (live core aa554c7e):** a rider airborne in control 7 after leaving a rail now runs 139A20's orientation tail. The Throne wind capture's Psymon 2941 → the whole capture exact (7999); gate raised. sim-diff differs in 1 of 16 runs (ARA1 rand2 2440, the human leaving a rail). Note: test-rails.mjs fails on the live core and on the earlier one: ARA1/rails.json now has 182 rails / 822 segments against the test's 171 / 777. That comes from an asset re-export, not this work.
> - **Crash billboards (live core 48d771d9):** a section-started LiveComp with slot-4/5 programs is now a core entity too, and stage builtins 28 (LiveComp properties, 0x341FE8) and 54 (entity time) are ported. Gravitude's billboard trigger plays the billboard on, and its timer program breaks the ice pieces with the shared RNG. ERA5 six riders are exact to the end (new gate `parity-ai/era5`). Gravitude race: human 1506 → 3915, riders → 4442+. The comparer lists entity-pass RNG sampling blips separately. Open: the JS billboard player does not see builtin 28 (its fall is not drawn).
> - **Rail airborne exit (live core 00014627):** 1211F8's turn / brake / crouch approach on the tick 0x132770 ends control 7. Gravitude race: the human, Nate, Mac and Luther exact to the end (5999); Allegra 5054, Moby 5139, RNG 5678. Gate raised. Heads-up: test-rails.mjs still fails on the ARA1/rails.json counts (an asset re-export).
> - **Held jump onto a rail (live core 5ce8e16e):** 12E9B8 returns as soon as its 106848 attaches (no targets, no new clip on the attach tick), and a held jump never enters control 7's 131D08. Gravitude Moby 5139 → exact to the end; gate raised. sim-diff differs only in BRA2 trick1 995 (the human attaching with Cross held).
> - **A crash stops the boost (live core b1c99224):** 12CB68 begins with 114130(rider, 0, 0), so +0x2FC is 0 through a crash and the get-up tick's ground drive has no boost term. Gravitude race exact to the end: human, five riders, RNG, ranks, pair records (was Allegra 5054, RNG 5678). sim-diff identical.
> - **Crash billboards fall on screen (live core sha256 8ce1dfe4; pv `sectionClock`, on):** the core exports the section players' clocks (`stage_world_section_clocks`), and the JS LiveComp player takes them each frame, so builtin 28's play-on is drawn. Checked against new PS2 snaps (runs/billboard/grav-bb, 1192–1249) in Chrome and WebKit; see [set-pieces.md](set-pieces.md). The dark falling board at 1208 is lighting, not culling (fixed by pv `litLiveComp`, visual-parity.md section 40).
> - **Freestyle finish (pv `fsCelebrate`, on; core finish_standing):** single-player freestyle uses handler 0 (0x239230): the top three keep rider+0x100 (the arm), and the boost meter goes to 0.7 / 0.35 / 0 by place on the finish tick. The winner-only clear 0x23A05C is the multiplayer handler 9. race_end asks the page's `finishHost` for the place (career.js finishPlace). Gates: crows-invert, perpendiculous and Schizophrenia now have score and boost to the end; new `celebrate/pipe-2nd` (2nd place, poked posting) is exact incl. bones, and the real Single Event flow matches its bones in Chrome and WebKit. The HUD sweep's posted-standings difference is the boot seed, not a bug: with `?presentationSeed=0x182200` the page posts the PS2 anchors' scores exactly (new Peak 2/3 check in test-slopestyle-bigair). See [slopestyle-bigair.md](slopestyle-bigair.md).
> - **test-rails fixed (no core change):** ARA1/rails.json has 182/822 because tools/import_rails.py exports the whole event residency on purpose (A_ARA1 + ARA1 + ARA1_B, re-exported 2026-09-22, docs/locations.md); the oracle and the test were stale. The oracle build links new `tests/rail_query_browser_stubs.cpp`, skips samples of rails without query mask 1 (11 in ARA1), and the regenerated local/browser-validation/rail-queries.bin has 4110 queries; test-rails.mjs reads the counts from the file. See engine/RAIL_RECOVERY.md.
> - **Soft collision off a rail (live core sha256 87389ac1):** the rail post's pair phase restored the pre-reaction +0x238 target, undoing 108388's control-7 exit, so a rail cycle's fading blend froze. Kick Doubt six riders (new gate `parity-ai/ess3-long`, 3700 ticks) are exact to the end; it was Moby 2945. sim-diff is identical and all 241+1 gates pass.
> - **Rail jump release (live core sha256 7f7fcfa3):** 1211F8 approaches the rail steer / balance / tolerance on the tick 12E9B8's release leaves a rail; ASS1 six riders are exact to the end (was Moby 5035). The same core queues an unlisted collectible's cash (`stage_collect_events` index 0xFFFFFFFF, the CTM agent's spec; JS pv `unlistedPickup`). 243 gates, sim-diff identical.
> - **Pair pushes reach the cached bones (live core sha256 af304327):** a rider's 121750 commits its pair pushes (rider+0x9D0) to its
>   cached world bones, as 310530 does, so the next rail step starts from the pushed board bone. DRA4 riders 1856 → 3927+, RNG
>   2640 → 3773. The DRA4 human gate was lowered to 5087: it now runs after the RNG difference. 243 gates pass. sim-diff differs in
>   pose_physical at pair pushes, which is intended.
> - **Post-finish stage contacts and timer splines (live core sha256 f556dfb6):**
>   - A finished human fires stage contacts again. Only +0x480, the DNF from a time-out or Give Up, blocks them (30A060).
>   - A LiveComp replaces a Hide / RestoreNode node on its instance.
>   - A finished (Position) spline piece is relaunched by a timer.
>   - Result: the Schizophrenia page's shared RNG is exact to the end (was 3609); see set-pieces.md "Timer splines after the finish".
> - **Non-Zoe anchor check:** a new Mac countdown at The Junction (local/reference/pcsx2/characters/mac-junction, navigated from
>   characters/mac/select.p2s) and the run nonzoe/junction-mac. Record 0 holds 17 shared-RNG draws from seed 0, against Zoe's 16:
>   event-anchor-rng.js's rule (base 17, Zoe -1, Mac 0) holds for another character. test-lineups checks it.
> - **Mission teleport 1234D0:** 11D660 caches the placed pose; `mission_rider_teleport` does too (web/mission_gameplay.inc).
> - **Gates:** `parity-ai/metro-race`, `ess3`, `dss2` and `ara1-full` added to aiCases (web/test-ps2-captures.mjs). All 235 capture gates pass. sim-diff against the old core is identical apart from the ground-cache words (new `--skip` option). Open: DRA4 1856 (a pair contact on rails), Gravitude Allegra 5054, Gravitude RNG 1195 (world pass 0x1013A8 timed callbacks, not ported). The NPC rail +0xE20 uninitialised-stack read is documented but not modelled.

> **CTM parity audit and fixes: awards, Grinder, fresh events, reloads, Transport lists, stations, peak crossings (2026-09-28, CTM agent):** see [ctm-parity.md](ctm-parity.md) "CTM parity audit". Whole-career audit against the code (money, progression, save / messages / relationships, stations / cutscenes); money matched everywhere. Switches in `web/pv-flags.js`, all ON after PS2 / Chrome / WebKit checks:
> - **pv `bcSpeed`** (core): stage builtin 59 (0x3032C0, |rider+0x1E0| x 1/60) was missing, so Kick Doubt's Grinder / Pepper Grinder / Meat Grinder could never complete; the only missing gameplay builtin of the 88 missions (static audit). PS2 `local/ps2-capture/ctm-parity/grinder`: 212 rail ticks equal the core per tick (test-big-challenges 1c). Live core rebuilt 08:51 (exports `mission_speed_builtin`, `mission_test_speed`).
> - **pv `awardCascade`**: 159CD0's cascade after every grant (Peak N conquered / All events completed at once in free ride), award 0 "Mountain conquered!" (never granted before), 1591E8's order, passes under the granting award (158F60), the reward list read from the PS2 reward record 0x4C3EF0 (cleared at WS10 and the list's close). New `web/test-award-cascade.mjs` (the PS2 sd-goal record bytes).
> - **pv `freshEvent`**: every career entry is a fresh qualifier / heat 1 (initGameMode 0x22D89C -> 0x238C80); PS2 `ctm-parity/fresh`: the Snow Jam final given up, re-entered -> "Qualifier". Before: the final again.
> - **pv `careerReload`**: a page reload during a career re-enters it (Happiness / the last lodge, as the PS2 re-entry) instead of a plain run.
> - **pv `transportLists`**: the Race / Freestyle rows from 0x4781D0 / 0x4786E0 (Peak 2 / 3 freestyle order, 'Throne Jam'), 0x207430's help lines.
> - **pv `stationFlow`**: WS14 holds the ride under the door / booth cuts and ends a Big Challenge; lodge Triangle = No, No stays at the door; the booth map on the ridden peak, Back = WS15; the post-event map ignores Back; the heli drop into a visited backcountry after a world switch, the heli out of a backcountry event; WS13 lists by event type.
> - **pv `bcBanner`**: the fallback MISSION SUCCESS banner skips "$ 0".
> - **pv `crossWorld`**: riding off the bottom of Intimidator / Gravitude (DRA4_A / ERA5_C, no row 17 / 19 in PEAK2 / PEAK3) switches world to Green Base Station / Yellow Mid Station (a load screen; the PS2 streams one mountain world). Chrome + WebKit both crossings; switch off stays put.
> - **pv `ctmSmallFixes`**: message Delete removes the first entry with the viewed item (1E3268); the last lodge is set at arrival only.
> - **Open (reported to the coordinator):** a seamless whole-mountain free ride (crossWorld is a world switch); relationship ageing timing (ai-race.js, AI owner) and their storage; New game resets (fe-saveload.js); reward picks use a saved xorshift, not BXrand. The PS2 never autosaves; the port keeps its autosave (known difference).

> **No rider at its bind pose, and stations after a Transport (2026-09-28, visual-parity agent):** see [ctm-flow.md](ctm-flow.md) "T-poses on the first load" and [visual-parity.md](visual-parity.md) sections 33 and 34. Not deployed. `web/main.js`, `web/free-ride.js`, `web/pv-flags.js`, `web/test-visual-parity.mjs` (R26, R27; the arrivalFade check updated).
> - **pv `riderPoseGate`** (on): the field probe's hit (26 bones, 'loading', PEAK1/1) is the page's placeholder RIDER_SAM, drawn at its bind pose by the held transport loop across a world switch (`cutscenes.js acrossSwitch` renders the scene; the frame's visibility pass does not run during the switch). A switch's first ride frame also drew the career rider at its bind pose, in view. Now the placeholder and a new rider start hidden, and the ride shows the rider once a tick has posed it: 0 bind-pose draws over PEAK1 -> 2 -> 3 -> 1 -> 2 -> 1 (Chrome) and PEAK1 -> 2 -> 3 -> 1 (WebKit).
> - **pv `stationArrival`** (on): a Transport to a station no longer plays the lodge walk-in or raises the lodge prompt (world state 14 arg 1 queues neither; they are the door's, arg 0). The station fades in from black under the HUD, like a course (PS2 `menus/stations-sj-to-green`, `stations-to-c`).
> - **pv `sessionFade`** (on): MCOMM Session Yes and a Transport to the current location fade in from white over 60 ticks, with the HUD over it (world state 15, 2E4370 / 2E47E8; PS2 `menus/stations-ws15-*`).

> **Online finish checks: no more false rejections of honest runs (2026-09-28, booth / plausibility agent):** see [multiplayer.md](multiplayer.md) "Finish plausibility". The live server rejected four honest online finishes. The coordinator hot-fixed the server default to 'flag'; it is now back to **'reject'**.
> - **finish-early (3 runs):** the finish packet came at the claimed clock +1 / +2 ticks, so these runs had no countdown. Backcountry is the only online event without one (Happiness / Ruthless / The Throne, 3-5 min on a tuck run); the log does not name the course. Backcountry events have a rolling start: 234AD0 skips the countdown, and the race clock reads 1 on the tick of GO. The check still assumed the 3-2-1 (180 ticks), so any backcountry finish failed.
>   - Fix: `plausibility.mjs countdownTicks(course)` returns 180 for races and freestyle events. It returns 0 for backcountry codes (`ABC1`, `DBC2`, `EBC3`: `/^[A-Z]BC\d$/`), `PEAK?` and `MOUNTAIN*`, and applies that to finish-early, clock-behind and coverage.
> - **clock-ahead (2 runs):** the leads were 46 -> 92+ ticks, gaining 57 ticks in 11 wall ticks, not a steady bias. The cause is in the client pacing:
>   - `web/fixed-step-clock.js` runs at most 12 ticks a frame and keeps the rest as debt. `mp-game.js pace` counted only the ticks already run.
>   - So during a catch-up after a hitch it asked for the same backlog every frame. The surplus then ran the race tick ahead of the server clock, and pace only takes back 1 tick a frame.
>   - Headless Chrome, online solo race, 2 s main-thread hitch: the old pacing went +100 ticks ahead, the new pacing +1.6. A model gives 100-580 ticks for 2-6 s hitches.
>   - The clock-offset estimate and the GO mapping are fine. stallCap / stallKeys do not touch the online path.
> - **Fixes:**
>   - Client: the pace moved to **web/net/race-pace.js** and counts the frame clock's debt. `main.js` passes `simulation.pending` to `mpGame.pace` at both call sites; an old mp-game.js ignores the extra argument.
>   - Server: a lead above 45 ticks must come back within 30 s (1800 ticks) and never exceed 1200. Clients from before the deploy still overshoot, and this rule accepts them. Only in-order packets count, so a replayed old packet does not end a lead.
>   - Server: `mp-server.mjs` logs the course and verdict stats with every rejection. It also logs accepted finishes that ran ahead.
> - **Tests:** `test-mp-plausibility.mjs` adds:
>   - a real Happiness run (14766 ticks);
>   - every courses.json course's countdown checked against its event, and the core's race clock checked for one course of each event;
>   - the live cases: finish packet at claimed +1 / +2 ticks, and a 92-tick transient lead;
>   - 2-6 s hitches paced by race-pace.js on the real FixedStepClock: old clients accepted (leads 101-580), current clients at or behind the server clock (lead -7);
>   - new forgeries, all found: a clock held 2 s ahead, the same with stale packets replayed in between, and a lead beyond the cap;
>   - the server run now uses the default mode.
>   - `npm test -- mp-plausibility mp remote-riders`: 12/12.
> - **Known limit (not new):** BEHIND_TICKS (300) still lets a forged stream claim up to about 5 s less than the wall clock at its finish, if the physical bounds hold.

> **Field stall causes (2026-09-28, performance agent):** see [sim-performance.md](sim-performance.md) "Field findings, 2026-09-28".
> - Skin palette error on ctm-results fixed (web/rider-skinning.js: a tick without a pose redraws the last palette). Core question (does 1234D0 cache the placed pose?) with the core owner.
> - **pv `glslKeys`** (on; shader-keys.js): stable GLSL uniform-block names, WebGL programs 109 -> 10 (character select), 146 -> 49 (Snow Jam).
> - **pv `warmPost`** (on; glare-pass.js warm, light-glow.js warmQuery, main.js warmupRender): no sync pipelines after the warm-up on any event course (Gravitude had 8).
> - **pv `feMorphTiers`** (on; fe-preview.js padMorphPart): FE morph shader shapes 4 -> 2.
> - diagnostics.js `gpu-stall` events carry a `cause`.
> - **pv `refKeys`** (on; shader-keys.js): three's shared materialReference re-pointed at the material being built; FE pipelines 8 -> 4.
> - **pv `liveRest`** (on; peak-set-pieces.js, set-pieces-renderer.js, attached-setpieces.js) and **pv `oneMatrixPass`** (on; snow-composite.js): per-frame matrix work at the CTM start ~1/3.

> **Online finish checks: no more false rejections of honest runs (2026-09-28, booth / plausibility agent):** see [multiplayer.md](multiplayer.md) "Finish plausibility". The live server rejected four honest online finishes. The coordinator hot-fixed the server default to 'flag'; it is now back to **'reject'**.
> - **finish-early (3 runs):** the finish packet came at the claimed clock +1 / +2 ticks, so these runs had no countdown. Backcountry is the only online event without one (Happiness / Ruthless / The Throne, 3-5 min on a tuck run); the log does not name the course. Backcountry events have a rolling start: 234AD0 skips the countdown, and the race clock reads 1 on the tick of GO. The check still assumed the 3-2-1 (180 ticks), so any backcountry finish failed.
>   - Fix: `plausibility.mjs countdownTicks(course)` returns 180 for races and freestyle events. It returns 0 for backcountry codes (`ABC1`, `DBC2`, `EBC3`: `/^[A-Z]BC\d$/`), `PEAK?` and `MOUNTAIN*`, and applies that to finish-early, clock-behind and coverage.
> - **clock-ahead (2 runs):** the leads were 46 -> 92+ ticks, gaining 57 ticks in 11 wall ticks, not a steady bias. The cause is in the client pacing:
>   - `web/fixed-step-clock.js` runs at most 12 ticks a frame and keeps the rest as debt. `mp-game.js pace` counted only the ticks already run.
>   - So during a catch-up after a hitch it asked for the same backlog every frame. The surplus then ran the race tick ahead of the server clock, and pace only takes back 1 tick a frame.
>   - Headless Chrome, online solo race, 2 s main-thread hitch: the old pacing went +100 ticks ahead, the new pacing +1.6. A model gives 100-580 ticks for 2-6 s hitches.
>   - The clock-offset estimate and the GO mapping are fine. stallCap / stallKeys do not touch the online path.
> - **Fixes:**
>   - Client: the pace moved to **web/net/race-pace.js** and counts the frame clock's debt. `main.js` passes `simulation.pending` to `mpGame.pace` at both call sites; an old mp-game.js ignores the extra argument.
>   - Server: a lead above 45 ticks must come back within 30 s (1800 ticks) and never exceed 1200. Clients from before the deploy still overshoot, and this rule accepts them. Only in-order packets count, so a replayed old packet does not end a lead.
>   - Server: `mp-server.mjs` logs the course and verdict stats with every rejection. It also logs accepted finishes that ran ahead.
> - **Tests:** `test-mp-plausibility.mjs` adds:
>   - a real Happiness run (14766 ticks);
>   - every courses.json course's countdown checked against its event, and the core's race clock checked for one course of each event;
>   - the live cases: finish packet at claimed +1 / +2 ticks, and a 92-tick transient lead;
>   - 2-6 s hitches paced by race-pace.js on the real FixedStepClock: old clients accepted (leads 101-580), current clients at or behind the server clock (lead -7);
>   - new forgeries, all found: a clock held 2 s ahead, the same with stale packets replayed in between, and a lead beyond the cap;
>   - the server run now uses the default mode.
>   - `npm test -- mp-plausibility mp remote-riders`: 12/12.
> - **Known limit (not new):** BEHIND_TICKS (300) still lets a forged stream claim up to about 5 s less than the wall clock at its finish, if the physical bounds hold.

> **PS2 softness option (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 30. Deployed 2026-09-28. Options > Display & Touch > 'PS2 softness' Off / On (row 3; `quality.ps2Output`, saved in ssx3.quality, default Off, `?ps2soft=`). `fog-renderer.js setSoftness`: a one-640-frame-pixel horizontal low-pass of the scene colour before the fog composite, built only when switched on (Off: the page's 52 WGSL modules and frames identical to before, Chrome and WebKit). `snow-composite.js seeded / useSeed`, `quality.js`, `fe-options.js`, `main.js` (2 calls). Test: R25.

> **Simulation core about a third cheaper in WebKit, a quarter in Chrome, bit-exact (2026-09-28, sim-CPU agent):** see [sim-performance.md](sim-performance.md) "Simulation core, round 3". Six-rider tick after / before, paired in one page: WebKit 0.652x, Chrome 0.74x (4x throttled, phone tier 0.754x), node 0.79x. Six batches, each landed in the live core only after ps2-captures (230-235 scenarios) and the core / set-piece / AI / online tests gave output identical to the baseline core's, plus a per-tick differential over 17 courses:
> - A: rail query scans a compact copy of the segment bounds in walk order; `addReference` / `mulReference` are `[[gnu::const]]` (wasm only); the pose clip lookup is done once per layer.
> - B: computer riders skip the renderer bone poses (rider_host bit 4 from web/ai-racers.js; they draw from the skin palette); chair entities are shared across rider contexts on bit-identical inputs (`chairEntityShared`).
> - C: engine/software_float.hpp add / sub / mul skip the operand flush tests when neither exponent field is 0 (old vs new exhaustive: ~4.2e11 native and 2.7e10 wasm results, 0 mismatches).
> - D: web/ai-racers.js mirrors the current rider context in JS and drops its per-tick state copies.
> - E: web/build-core.sh links with `-flto` (the build takes 30-60 s longer; check-rider-globals.mjs reads the LTO map).
> - F: chairlift car evaluation shared across rider contexts on bit-identical inputs (`multiSplineShared`).
> - New QA tools: **web/sim-diff.mjs** (two core builds, same race and pads, first differing export and tick; trick pads with flips released mid-rotation; renderer poses included) and **web/bench-sim-browser.mjs** (paired core benchmark in Chrome or WebKit). The body query's instance loop (~10% of a phone tick) stays: its instances are mutated in place from ~20 sites, so no compact copy can be proven fresh.

> **A course switch during the boot load no longer leaves the page stuck (2026-09-28, CTM-stream agent):** see [ctm-flow.md](ctm-flow.md) section 8.4. pv `bootChain` (on).
> - **Cause:** the page's first course load (`?autostart=1`) ran outside the course-switch chain. A switch asked for during it ran alongside it, and both wrote the course globals. This happened on builds from before today too.
> - **Fix:**
>   - Switches asked for before init has loaded the first course wait on a gate and run after it (the boot course's event is skipped).
>   - Back / Forward is listened to from the start; back to the boot course drops the pending switch.
>   - A lazy first course dropped for another stops within a frame.
>   - The streamed world's in-flight warms settle before its passes are disposed.
> - **Checked:**
>   - Stuck switch loads: 8 of 15 before (both browsers) -> 0 of 108 in Chrome (dev and production) and 0 of 48 in WebKit. The runs covered a course switch, CTM, Back, a switch in the event load and the lazy course.
>   - Targeted tests 25/25.

> **Safari load and frames: glyph atlases, warm-up spread, wider static refresh, stall attribution (2026-09-28, performance agent):** see [sim-performance.md](sim-performance.md) "Safari load and frames (round 3)".
> - **Cause of the ~30 s Safari loads:** not the pipeline cache (cold = warm in Chrome and WebKit). WebKit's GPU process converted the whole accelerated glyph tint atlas for each of ~1000 load-screen glyph draws a frame (70-80 ms frames, the page and WebGPU idle). **pv `softGlyphs`** (on, deployed; web/sprite-canvas.js glyphSource, ui.js text): WebKit draws glyphs from a 1:1 software copy of the same tint; UI canvases identical on 15 screens. BRA2 load to intro 22-25 -> 11.6 s, CTM start to control 15.3 -> 10.3 s.
> - **pv `warmSpread`** (on; main.js warmupRender, rider-shadow.js warmLimit): warm steps wait for a drawn frame (gated tiers skipped slices, which then all drew in the whole-scene frame), post passes / sky / rider shadows / shared vertex buffers spread before the slices. Longest warm frame WebKit phone 199-960 -> 105-124 ms, Chrome phone 4x 706-999 -> 310-323 ms.
> - **pv `staticRefreshWide`** (on; static-world.js): every shared-world-material object without UV scroll on three's static refresh, with a full refresh when its matrix, material, textures (crowd frames) or geometry versions move. Render JS hub -21% (Chrome 4x) / -17% (WebKit). Also the static path's SHARED constant fixed.
> - **pv `sharedIndex`** (on; main.js asset, gpu-copies.js): a package's batches draw ranges of one index buffer (event terrain keeps copies): setIndexBuffer -60% at the hub, WebGPU calls -6%; identical everywhere checked. staticRefreshWide's check now also compares attribute / texture identities.
> - **diagnostics.js:** `stall` and new `hitch` events carry `cause` (busyMs, long tasks, pipelines sync/async/compiling, shaders, textures, buffers, bind groups, node builds, marks, cutscene / stream stall).
> - **Open:** Safari in play is per draw call twice (JS, then the GPU process drains ~6500 WebGPU calls a frame at the hub while the main thread waits in prepareForDisplay); exact levers left: shared index buffers per package (setIndexBuffer), fewer calls. Render bundles would cut the most but change draw order.

> **Page = PS2 at Crow's Nest: event anchor RNG, keyboard per tick during a hitch (2026-09-28, air-release agent, follow-ups):** see [ai-racers.md](ai-racers.md) "Anchor RNG without lineups.json" and [workers.md](workers.md) "Keyboard input during a hitch". No core change.
> - **Page-only divergence (Crow's Nest 1776).** The page feeds the core exactly what the comparer does: the page equals the comparer without `--sync-rng` on every tick, in motion and RNG.
>   - The difference was the shared game RNG at the countdown anchor. Eight courses had no lineups.json or rivals.json, so the core kept its seed state's words: Crow's Nest seed 0 + 109 draws where the PS2 has + 9; Gravitude 1381 for 26.
>   - Every random pick then differed: landing and upper variants shift the 115D48 idle draws, and the 1776 soft collision picks another reaction.
>   - **pv `eventAnchorRng`** (on; web/event-anchor-rng.js with PS2-measured counts, main.js `soloTickStart` + a one-line call in game-tick.js, ai-race.js for Gravitude and Kick Doubt).
>   - The real page flow at Crow's Nest is exact against the PS2 on all 2148 ticks (Chrome, WebKit). Seeded once, 7 solo captures are exact to the end without per-tick sync.
> - **pv `stallKeys`** (on): each tick of a multi-tick frame reads the keyboard at its own 60 Hz sample time (event timestamps), as the PS2 ring does. A 250 ms stall with the D-pad released inside it stays PS2-exact; without the switch the release acted 4 ticks early. Gamepad and touch keep the frame's sample.
> - Tests: `npm test -- test-replay test-race-finish test-rival-page test-pad-input test-frame-clock test-touch-controls test-gamepad` all ok. New tool `web/compare-page-capture.mjs` (the real page flow against a capture: motion and shared RNG, first differing tick; not in npm test); the stall harness was a scratch script.

> **MCOMM Session map (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 32. Not deployed; needs the re-exported `web/public/assets/UI/audio-menus.json` (adds OV.LUI 38session and the OV sprites location / dot_visited / indicator). **pv `sessionMap`** (on; new `web/session-map.js` = table 0x440770 + the marker math of 0x209970 / 0x2096A8 + the nearest point 0x26B680; `career-ui.js drawSessionMap` / `sessionFocus`, `audio-menu.js screenLui`, `main.js freeRideSessionMap`). Test: R24.

> **Buy Attributes as the PS2 runs it (2026-09-28, Buy Attributes agent):** see [career-events.md](career-events.md) "Buy Attributes (cFEStateBuyAttrib)". **pv `buyAttribs`** (on; new `web/buy-attribs.js`, `career.js buyAttributePoint`, `career-ui.js` ctm-attributes hooks, `main.js startRun`). Not deployed; no core change, no asset re-export (33buyattribs is already in UI/character-select.json).
> - **From the code** (cFEStateBuyAttrib 0x1F4728 / 0x1F4C30 / 0x1F4A90 / 0x1F5300, 0x150C20, 0x150E50, 0x148098): one Right = one raw point (+0.2) at 0x440550[level−1] ($250..$5,000 a point, 5 a level); pending stops at the next whole level; "You have" is the cash until Yes; the level number shows bought points only; Cross → the buy popup ("Buy attributes?", Cost / You have, Yes focused, Triangle inert); Yes buys point by point and the screen stays; no save; bytes per rider at profile +0xBDF, synced into the runtime bank 0x535538 that the stat getters read live (int(raw/5)/11).
> - **What the port did wrong:** a whole level per press at the point price (5x cheap), pending up to 11.0 in one visit, "You have" minus pending, pending in the level number and the cost, 11-box bars, no popup (Cross bought and left), a "Rider ranking" line, and free ride never got the bought levels (only event rounds called `cb.attributes`).
> - **PS2** (`local/ps2-capture/lodge/attrs/` ba1..ba4, memory per step with `tools/ps2_buy_attribs_state.py`): the session's pending / total / bank / bytes equal the PS2's at every step in Chrome and WebKit (frames alike). New gate `peak1-lodge-attrs` (Speed/Accel/Stability 2.0, Spin 11.0 bought, Return to Game, world-load ride): speed limit 2219.7 vs 2207.1 from tick 0; the port with the bytes is exact through 390 (the lodge walk-in), without them it leaves at tick 1. `compare-ps2-capture.mjs --attributes` sets the bytes.
> - **Tools:** `tools/ps2_capture.py discover` tells the live DEFAULT_3 camera from the compositor copy when a stale heap word references both (the lodge exit's world load); unchanged when only one is referenced.
> - **Tests:** `npm test -- ps2-captures buy-attribs career lodge fe-screens rider-attributes save-store` 7/7 (ps2-captures with ONLY=peak1-lodge-attrs,peak1-green-start). New `web/test-buy-attribs.mjs` (in test:all).
> - **Closed (same day):** the TransitionOut flash (transition_flash, white 10 frames up / 9 down, the switch at full white, the lodge's intro under it; also the lodge's Cross into Buy Attributes) matches PS2 ba5 frame for frame in Chrome and WebKit; the buy popup's veil is 139buy_popup's 0885e124 drawn once (A 175>>1, gradient (111,177,210) -> (200,225,238)), fitted on the PS2's attribute and Ubertrick popups; `fe-screens.js drawPrompt` no longer squares its alpha for buy popups (pv buyAttribs), MAD outside the box 13.4 -> 6.3. See career-events.md.
> - **The buy popup is the PS2's 139buy_popup (same day):** `web/buy-popup.js` (box grow, veil, text fade, focus, widgets) for Buy Attributes, Ubertrick and Rewards buy popups (fe-screens.js `drawPrompt`, pv buyAttribs). Needs the re-exported `UI/character-select.json` (adds `139buy_popup`; without it the old fit stays). Box MAD vs PS2 19.7 -> 3.6 (attributes), 11.1 -> 4.1 (Ubertrick); the intro matches ba6 frame for frame in Chrome and WebKit. `tools/export_character_select.py`: UI_OUT=DIR exports to scratch.
> - **Note:** online after a career run now resets to the default attributes at run start (it kept the career's bytes before).

> **Start only pauses from the ride; in menus it is the accept (2026-09-28, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 31. Not deployed. **pv `startRules`** (on; new `web/start-rules.js`, `main.js` keyboard / pad / touch pause paths, `gamepad-menus.js` startAccepts, `career-ui.js back` closes the free-ride MCOMM). PS2: the pause opens only at 0x230A34 in the game update (action 0x3C; gates 0x231840 / 0x231AB8, game phase 0 or >= 10, 0x20CBE8 overlays); every LUI screen takes Start as UINext (Cross or Start). Fixes Owen's reports: the lodge's Start dropped him outside the lodge; Start paused after a finish. Test: `web/test-start-rules.mjs` (in test:all); matrix `local/browser-validation/visual-parity/tools/startmatrix.mjs`.

> **Air release: Owen's "reset to neutral" mid-air, PS2 frame pacing, a rail prewind fix, 4 new air capture gates (2026-09-28, air-release agent):** see [workers.md](workers.md) "Frame pacing after a hitch" and engine/AIR_CONTROL_RECOVERY.md "Release, auto-complete and cancel".
> - **Cause.** The auto-complete is PS2-exact. The new capture air-release/crows-invert (released at -195°, a spin during the auto-complete, a grab released inverted) is exact on physics, 29 bones and the owner+0x230 air state. The page draws the core's skin palette of the last two ticks, and Chrome and WebKit do the same.
>   - The snap is a **stall plus the clock's catch-up**. Offline, the fixed clock kept a slow frame's whole debt and replayed it at 12 ticks a frame. After release the auto-complete turns 6-9°/tick, so a 250 ms Safari hitch drew -210° → -291° → -319° in two frames, and a 3 s one took four frames from inverted to upright, then fast-forwarded.
>   - A second, genuine PS2 case: a trick cancelled in phase 0 (a left-stick lean folded into an opposite D-pad trick, then a short tap) turns the rider back to neutral and allows a new trick (PS2 crows-adjust 682-694).
> - **pv `stallCap`** (on; web/ps2-frame-pacing.js, main.js frame, offline only): the PS2 frame loop 0x316F00 decoded and measured with stall savestates.
>   - One update per 60 Hz pad sample (30-slot ring 0x326B88); catch-up updates run back to back without drawing; at most app+0x20 = 12 updates a drawn frame; the backlog is then dropped (0x227E68 -> 0x326C60); 30+ ticks lap the ring.
>   - Online keeps `mpGame.pace`; the replay clock is its own.
>   - Tests: test-frame-clock (17 PS2 data points), test-replay, test-race-finish. A BHP1 finish with six 900 ms stalls matched the unstalled run (4439 ticks, result 4222). Chrome and WebKit page runs of the Crow's Nest stall show 12 ticks then 1 a frame.
>   - New tools: `tools/ps2_stall_state.py` (derives a stall savestate from a capture build: K extra vblanks at a tick, optional stack dump) and the `PS2_CAPTURE_EECYCLE` opt-in (ps2_capture.py EE clock).
> - **Rail prewind fix (core; web/rail_gameplay.inc):** Cross + D-pad on a rail chose the prewind clip 255 from the already approached currents. 12E9B8 selects from the retained currents and 1211F8 approaches afterwards, as on the ground; the PS2 plays 245, then 255 a tick later. air-release/rail-exit-spin was exact through 680 (bones 681 off by ~0.6°) and is now exact on all 1059 ticks. This is not a regression: the line predates the mirror (640adb0).
> - **New gates** (web/test-ps2-captures.mjs; local/ps2-capture/runs/air-release, scripts air-release-*.json):
>   - crows-invert (score and boost through 2078: the PS2 clears the boost meter at the finish, open as perpendiculous 3996);
>   - crows-adjust;
>   - glide-land-rotating (landing mid-spin at -126°);
>   - rail-exit-spin (rail jump into a spin; a flip interrupted by a rail attach).
> - **Covered since (2026-09-28):** pipe-wall, pro-late-spin, handplant-flip and pipe-depart-soft (see the entry at the top). The pacing-* and stall runs in runs/air-release are evidence, not gates.

> **Audio glitches: clicks, late stingers, interruptions, main-thread decodes, a new song per CTM heat; field counters (2026-09-27, audio agent):** see [audio-logic.md](audio-logic.md) 9.13. Not deployed; no core change.
> - **Found** (silent probe: every source start / stop, the music bars tagged, the MUSIC bus and the whole mix recorded by an AudioWorklet; Chrome desktop, a phone at 4x, WebKit):
>   - hard clicks at every pause / resume: 5-8 steps per 6 pauses, up to 0.29 full scale;
>   - the same at a tab hide, a Big Challenge's cut (up to 0.39), a Stop / PlaySong (0.43 at a free-ride ChangeSong) and a context suspend (0.1-0.25);
>   - the stinger 2.7-5.3 ms late with its attack skipped (WebKit, phone);
>   - an iOS-style interruption: 27-42 voices all starting at once on resume;
>   - showing the page (or a key) during a movie resumed the game audio under it;
>   - 6-channel charsel bars decoded on the main thread (49 ms at 4x); 4.4-4.8 s of silence at a song change at 6 Mbit/s;
>   - Owen's report: every CTM heat replayed heat 1's song;
>   - no late or missed bars anywhere (lookahead margin p50 1.47 s, a 1.6 s stall on the phone).
> - **pv on:**
>   - `audioDeclick`: 5 ms ramps on music, speech and the whole mix where they stop or start mid-waveform (the PS2 pauses with pitch 0, 2B2018 -> 3B7FB8); stingers and their parts prepared at the prompt's Yes;
>   - `musicLookahead`: 2.5 s ahead, the input's audio first; the audible schedule is identical to 1 s (tested);
>   - `musicWorkerDecode`: every bar in the worker, bit-identical fold, main-thread fallback;
>   - `audioInterrupt`: a stopped context takes no voices; movie / hidden holds; resume retried on a timer, focus and any input;
>   - `sfxStartAfterDecode`;
>   - `musicPrefetchNext`: the song change at 6 Mbit/s 4.8 -> 1.8 s;
>   - `heatSong`: WS13 0x235A18 -> 27A860 -> 28E8C0(20, 1); from round 2 PickNextSong + PlayMusic 36, matching the PS2 music log `music/runs/heat2-audio`;
>   - `ctmRestartAudio`:
>     - the pause's Restart (2302A8: world state 2, no 28E8C0) keeps the song: resumed at the Yes, event 0 at GO;
>     - the results' Restart runs WS13;
>     - 29C420's countdown rule: at "1" a finished / hub / chartune song is replaced by a new, paused one for GO;
>     - career-ui.js restartToCard calls `gameAudio.restartRun` before its quit (one line, approved);
>     - PS2 logs: `pause-restart-audio2`, `results-restart-audio`.
> - **pv off:** `sfxWarmFirst` (inconclusive).
> - **Field counters:** `web/audio-stats.js` in the /mp/diag heartbeat (`audio`: late / missed bars, pump stalls, slow decodes and what, stolen / dropped / gated voices, late speech, interruptions, refused resumes) and the totals at pagehide.
> - **After:** 0 hard steps (≥ 0.08) in pause / hide / challenge in Chrome, WebKit and the phone; 0 at a suspend; bursts 0; stingers whole; margin p50 2.9 s; music decode off the main thread.
> - **Open:** a painter event during a challenge's 1.2 s wait jumped to an unstreamed bar (181 ms late, phone, once); the crowd loop's first-play decode at the intro on phones (70-97 ms).
> - **PS2 output** (silent ARMSX2 dumps, SDL disk driver):
>   - the pause holds the stream's sample (step 0.0006-0.0017, then the emulator's 4 ms DC decay) and the resume continues from it: no click;
>   - the challenge flush holds too, and the stinger starts 21.6 ms after the cut, without a step;
>   - the big-air landing's x6 level rise shows no step (one landing, near a zero crossing);
>   - the port's 5 ms fades stand in for the hold and continuation (audio-logic.md 9.13).
> - **Tests:** `web/test-audio-glitches.mjs` (in test:all); targeted `npm test -- audio pathfinder music-stream game-audio challenge diagnostics cutscenes` green. Tools: session scratchpad `audio/` (probe.js, analyze.js, drive.mjs, table.mjs, clicks.mjs, calib.mjs, fadetest.mjs).

> **Event loads in parts, the riders' setup, the FE preview compiles (2026-09-27, CTM-stream agent, round 2):** see [ctm-flow.md](ctm-flow.md) section 8.4. Not deployed. Live core rebuilt 23:02 (new exports: `event_world_begin`, `terrain_part`, `event_world_seal`, `rails_seal`, `init_world_begin` / `init_world_step`, `init_world_collision_cached` / `init_body_terrain_cached` / `init_rails_cached`, `animation_prepare`, and the test hashes `world_load_hash` / `body_load_hash` / `parse_key_of`).
> - **Both switches on (2026-09-28)** after a hang hunt: 45 Chrome and 36 WebKit repeat-visit loads, no hang (ctm-flow.md 8.4). It also found a pre-existing stuck load when the course is switched during the boot load. That is not from these switches, and the owner of the load flow should take it.
> - **pv `eventSlices`**:
>   - An event's collision.bin tree, terrain, collision world, body terrain and rail catalog are loaded in parts cut by the peak-world worker. Each core call is at most ~10 ms at 1x; the whole calls were up to 320 ms.
>   - terrain.json is parsed once for both terrains.
>   - The computer riders' contexts copy the parse caches by key: no multi-MB texts per rider, and the rails are parsed once.
>   - The rider animation documents are parsed one a frame ahead of `init_animation` / `init_race`.
>   - The course asset build and its PNG decodes are spread over frames, and the warm-up's new materials per frame are adaptive.
>   - Exact: `web/test-event-slices.mjs` (all 17 event courses, rider contexts, animation) and the in-page load hashes.
> - **pv `feCompileSpread`**: an FE preview model compiles part by part, and the roster prefetch stops when the rider screens close. It had been compiling nine riders under the CTM world load, 0.9 s at 4x.
> - **Measured:** Snow Jam at phone 4x: frames over 100 ms 31 -> 12, worst 1017 -> 708 ms, same load time. Load times on the production build at 6 / 50 Mbit/s, first and repeat visit, are equal within noise (the table is in ctm-flow.md 8.4). CTM at the station at 6 Mbit/s: 87.8 -> 73.4 s against no sliceLoad.
> - **Left:**
>   - The first draw's GPU uploads, ~700 ms: performance agent.
>   - The human rider's `_init_animation` build, after its parses moved out.
>   - The computer riders' `_init_race`, ~30-60 ms at 4x.
>   - Once, a repeat-visit page hung in headless Chrome with the switches on (main thread unresponsive). It did not happen in 9 later runs.

> **Restarts: the pause's Restart has no gondola ride; the gate stays lit under the card (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 29 item 9 and [ctm-flow.md](ctm-flow.md) 4. Not deployed. **pv `pauseRestart`** (on; `career-ui.js restartToCard`: cutscene kind `restart` from the pause, `heat` from the results) and **pv `loopFadeOnce`** (on; `cutscenes.js fadeAt` `loops`: a looping idle / flag-8 step fades in on its first pass only). Before, the card sat over a gate dipping to black on every idle loop. PS2 captures `menus/race/r3-restart`, `r3-results-restart`, `r3-rr-dense`. Test: R22.

> **In-game Options: the PS2's PDA page (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 29 item 3. Not deployed; needs the re-exported `web/public/assets/UI/audio-menus.json` (adds OV.LUI `37beoptions`; `tools/export_audio_menus.py` exports it). **pv `pdaOptions`** (on; `ui.js set` routes every in-game 'options' to `audio-menu.js` `pda-options` while the page is loaded, else the old list). Rows as PS2 `menus/single/r3-options` / `ctm/47-options`: HUD Options, Camera 1, Camera 2 (greyed), three sliders, DJ Speech, Arcade SFX, Save game (CTM only). The port's Widescreen / Keyboard / Display & Touch are behind "More options" in the online-only EA Talk row (`pda-more`). Test: R21.

> **Pale trees / signs / buildings in streamed free roam: generic scene fog (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 29 item 8. Not deployed. **pv `genericFogOff`** (on; `main.js`: after the fog renderer is made, `scene.fog` is detached to `scene.userData.genericFog`, which the painter update still writes). The course load's `m.fog=false` traverse only covered meshes present at load. Every material made later (streamed locations, static cells, set pieces: 396 after an ABA1 transport) got three's linear fog on top of the original fog pass: fully fogged at the painter's far. After a transport, trees and signs turned near-white and the BHP1 city white. Checked against PS2 arrivals at ABA1, ASS1, BRA2 and BHP1 in Chrome and WebKit. `test-presentation.mjs` accepts the renamed fog colour copy. Test: R20.

> **Browser-port optimizations round 2: GPU-loss restore, dropped CPU copies, empty FX batches, set-piece skips, refresh cap (2026-09-27, free-roam perf agent):** see [sim-performance.md](sim-performance.md) "Browser-port optimizations (2026-09-27, round 2)". No core change.
> - **pv `gpuRestore`** (on; web/gpu-copies.js, gpu-recovery.js `beforeResume`, main.js): fixed a live bug, every event course lost its static world after a GPU device loss (iOS backgrounding). The dropped arrays are re-read before drawing resumes; the shadow atlas is re-made on the new device (rider-shadow.js keyed by device). Frames after a loss identical on 7 scenes, Chrome and WebKit.
> - **pv `gpuRelease`** (on; releaseWorldCopies, main.js asset(), free-ride.js): uploaded world arrays and texels dropped (restorable): free roam low tier Chrome heap 343 -> 238 MB, WebKit footprint ~ -140 MB.
> - **pv `skipEmpty`** (on; snow / impact-fx / startfire renderers): empty FX batches hidden: 65 of 690 render objects a frame in a six-rider event were empty.
> - **pv `setPieceSkip`** (on; peak-set-pieces.js): UV-scroll representatives only (734 -> 23), hidden locations' LiveComp meshes skipped: -1..-1.5 ms at phone 4x.
> - **pv `refreshCap`** (off, Owen's call): 60 fps on 120 Hz+ displays (half the render work in Chrome / Firefox; Safari already caps at 60).
> - Measured and not done: WGSL already deterministic, CPU-bound (no dynamic resolution), render bundles change draw order, decode 62 ms per load, allocation mostly V8 boxing. The WebKit "device mismatch" after a recovery comes from the destroyed device itself (harmless).

> **Transport arrival fade (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 29 item 5. Not deployed. **pv `arrivalFade`** (on; `main.js transportInWorld` -> `cutscenes.fadeFrom({ticks:30, colour:'black', hud:true})` for a course arrival, `cutscenes.js overlayUnderHud`, `ui.js` draws the fade before the HUD). The PS2 fades the world in from black over ~30 ticks after the arrival placement, with the HUD over it (watched re-capture of the ABA1 arrival). The port cut straight to the full world. Painter regions and records at the arrival already matched. The Big Air finish HUD item was a harness context: the real Single Event flow matches. Tools: `tools/arrive.mjs` (a real in-world transport with the region / fog logged every frame), `spause.mjs --finish --banner`. Test: R19.

> **Round 3 visual parity: the Single Event pause and riders over menus (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 29. Not deployed. **pv `menuRiders`** (on; `opponent-riders.js` tags its groups `userData.opponentRider`, `main.js` hides them while `playing` is false): the computer and online riders no longer draw over the pause, options, audio and popups; results and replay keep them. **pv `singlePause`** (on; `career-ui.js singlePause()` / `pauseItems`, `ui.js pdaPause`): a Single Event pauses into the MCOMM PDA with Return / Restart / Audio / Options / Quit and the PS2 help lines; Quit asks 'Quit Game' (No) and Yes goes to the title (PS2 `menus/single/r3-pause-*`); the online / fallback pause is drawn in the same PDA. Also landed earlier today: **pv `hudHints`** (R17) and **pv `shadowAtlasInit`** (the rider shadow atlas bound before its first render: 'Destroyed texture used in a submit'). Open, ranked: the in-game Options screen (PS2 PDA page `menus/ctm/47-options.png` vs the port's placeholder), the Big Air finish HUD, the Peak 1 transport arrival fade. Tools: `local/browser-validation/visual-parity/tools/spause.mjs` (the real Single Event flow: rows, `--flow restart|quit|quitno|options|audio`, `--finish`), `vpworld.mjs`. Test: `test-visual-parity.mjs` R18.

> **T-poses at the first CTM start: not reproduced; a field probe instead (2026-09-27, visual-parity agent):** see [ctm-flow.md](ctm-flow.md) 8.4. No bind-pose skinned draw (CPU skeleton, uploaded bone buffer or core palette) in any lab run. The runs, all from a fresh profile with every frame screencast: Chrome phone 6x CPU with slow GPU compiles (+0.5-2 s), at 6 Mbit/s, on WebGL, and at 1600x900; WebKit; all 11 characters; the production build of the current tree through the shaped edge. Each covered a new career and a resume. **pv `bindPoseProbe`** (on; `web/diagnostics.js`, `fe-preview.js` names the preview root) arms for 60 s after a load screen opens or closes, a cutscene or a ride starts. It checks up to 3 skinned draws a frame (~7 us each) and sends the first bind-pose hit of a session to /mp/diag as `bind-pose`: screen, mesh path / model, CPU or GPU-buffer, frames since armed, backend, adapter. A control mesh is caught in Chrome and WebKit, with no false hit over the flow. Test: `web/test-diagnostics-bindpose.mjs` (in test:all).

> **World texture LOD K: found, not drawn (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 28. No runtime change. The terrain's per-draw TEX1 K is per texture. The streamer `0x37CA30` sets it from the SSB kind-9 header s16 +8 (v): K = trunc(-16 log2(240 / (v / 16384))), clamped to -2047 .. -135; v = 0 keeps -185. It equals the live `strm_tex` descriptors of 8 PS2 states (334 textures). Level = log2(depth cm) + K/16, i.e. v / 16384 texels per cm and a 240 focal constant. `tools/export_world_texture_lod.py` exports it. A depth-only level drawn with it in `world-material.js` (both graph paths) measured worse against the PS2 frames: world MAD 13.49 -> 13.88 over 15 frames, still worse at +1 / +2 bias, with more aliasing. It is kept in `local/browser-validation/visual-parity/round2/lod/`, and world-material.js is untouched. Test: `test-visual-parity.mjs` R16.

> **A world load's painters: the sun and fog fade in at the ride start (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 27. Not deployed; live core rebuilt (new exports `environment_world_load`, `environment_world_loads`). **pv `painterWorldLoad`** (on; `free-ride.js placeRegion` -> core `environment_world_load`, `painter-regions.js followWorldLoad` in `sun-flare.js` / `screen-tint.js` / `glare-pass.js`). The PS2 world load (the lodge's Return to Game, a station start) places the new rider during the load, and the load's painter steps run in a stale, unloaded region. So Fog / Sun / ScreenTint / glare start the ride at their class defaults and the location's record blends in at its rate. The port jumped to it at tick 1. Watched PS2 re-capture of peak1-green-start: Sun A 1 %/tick, Fog A 0.25 %/tick (density 0 -> 1.2, far 300 -> 70 m); the port now equals it to the float at 101 / 201 / 304. The t203 "sun through the sign" was this: the PS2's sun is still off screen there. Frame MAD t103 / 203 / 306: 14.4 / 17.7 / 24.4 -> 11.7 / 12.1 / 21.2 (Chrome; WebKit the same). Not changed: Transport arrivals (no watched run) and the plane drop (it snaps on the PS2 too). Tools: vpshot / vpeval arm the fresh rider on `?peakCourse` pages by default (`drv.mjs FRESH_WORLD_ENTRY`); `--route` / `proxy2.mjs` take `KEY$` path suffixes. Test: `test-visual-parity.mjs` R15.

> **Free-roam frame cost: static world cells, shared world materials, static refresh, the render pass verified (2026-09-27, free-roam perf agent):** see [sim-performance.md](sim-performance.md) "Free-roam steady state". No core change; the simulation is untouched.
> - **Measured** (web/bench-free-roam.mjs + web/perf-probe.js: Chrome desktop / phone 4x, WebKit): steady free roam costs about what an event on the same ground costs. "Between zones" was streaming work (the streamGate stall and the collision feed on the connector: handed to the CTM-stream agent), the hub row's ~2x draws (the PS2 draws those six locations too), and three's per-object overhead (~50 % of a phone frame, events too). Not causes: collision over every loaded location (bit-identical, same ms/tick), set pieces, bookkeeping, GC.
> - **Render pass (web-render-performance.md, live):** verified pixel-exact (pre-pass tree vs live on a deterministic run; same-frame frozen vs recomposed in Chrome and WebKit: event, hub, connector, zone). free-ride.js: the `peak1-world` container frozen too (one line).
> - **pv `staticWorld`** (on; web/static-world.js, peak-set-pieces attach/detach, main.js for the event world, rider-shadow riders()): static batches in 128 m cells hidden per render outside the frustum; same draws and order; render JS -10..-35 % (phone 4x zone 25/22 -> 20/14 ms).
> - **pv `sharedWorldMaterials`** (on; web/world-material.js): one colour graph per structure with object-updated texture nodes: a location's node builds 75-170 -> 0-17, PEAK1 start 832 -> 132, Snow Jam 431 -> 129 (load-to-ride at 4x ~2-3 s shorter). Identical frames (same-page legacy swap, cross-run Snow Jam / Metro City / Ruthless) in both browsers.
> - **pv `staticRefresh`** (on; web/static-world.js overrides NodeMaterial.setupObserver in our code, web/rider-shadow.js receiver uniforms in the render group): opaque frozen cell members with a shared world material take three's static refresh path instead of a FULL refresh per draw; identical frames (same-frame full vs shared, cross-run) in both browsers; render JS -5..-10 %. Rule: a per-frame uniform added to world materials must sit in a shared group.
> - **Dropped:** worldMerge (one draw per cell and material: depth ties on ~10 % of batches changed pixels, gain marginal); PEAK1 texture-chunk gating (with far <= 300 m and the chunk range 450 m it changes only pop-in: fidelity note).
> - **Next:** peak-set-pieces ticking all 728 UV-scroll instances and 490 LiveComp meshes; `_animation_tick`.

> **Fresh sweep, CTM free-ride frames, the switch-stance 'S' (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 26. Not deployed. **pv `switchIcon`** (on; `trick-hud.js switchIcon`, `ui.js`, `main.js stanceRegular`): the 'S' under the boost meter the PS2 draws on every frame (faint regular, orange riding switch; 0x1EC3F8, HUD flags 0x10000000, descriptor 0x4F, alpha x 0.2 while rider +0x320 == +0x324). Needs the UI/trick-hud.json re-export (`sprites.switchIcon`, owner +0x470). Sweep: no rendering regressions; dss2 4018 moved because the rider diverges differently. The Green Station free ride matches the PS2 line exactly once the harness re-arms the fresh rider. Open: the sun flare through the station sign (Z query; world-material.js, on hold).

> **CTM ride start: the career rider under the load screen (2026-09-27, lazy-course agent):** see [ctm-flow.md](ctm-flow.md) section 9. Not deployed; no core change.
> - **pv `rideWarm`** (on): a free-ride world load runs `ensureRider` and compiles the rider for the world pass (`fogRenderer.compileObject(sam, {depth: 1})`, two-pass materials per side, then `gpuIdle`) under the load screen; `startRun` finds it ready. Races identical for 900 ticks with and without the arrival cutscene (`test-ride-warm.mjs`). Chrome 4x CPU: ~1 s of node builds after the ride start -> 25-44 ms; WebKit: the first ride frame gap 182 -> 56-64 ms (2.2-2.4 s of Metal pipeline builds moved under the load screen).
> - FE preview builds on the load screen (~0.8 s reported): not reproduced on the Single Event or CTM world loads (Chrome 4x CPU: ~25 ms of FE preview JS in the whole load screen; WebKit: build() ~1 ms, the compile is asynchronous). Needs the reporting profile's exact path.

> **The rival card's rider brightness (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 25. Not deployed; live core rebuilt 15:48 (new export `environment_settle`). **pv `readyLight`** (on; `main.js readyView`): the card's rider was the unlit menu fallback (no source skin, no lighting before tick 0); now the source skin in the ready pose, lit by the environment settled at the start spot (the PS2's ready irradiance is 0.95 of the dark bank) and the card's camera, so it reads dark through the panel as on the PS2. The panel blend itself matches. Test: test-visual-parity R13.

> **CTM start and free-ride streaming: the movie's music, the station drawn on arrival, the read-ahead, the load screen's long calls, the lodge exit (2026-09-27, CTM-stream agent):** see [ctm-flow.md](ctm-flow.md) section 8 and [peak-mountain.md](peak-mountain.md) "Browser data". Not deployed. Live core rebuilt 15:35 (new exports `stage_world_part`, `stage_world_load_hash`, `environment_load_hash`, `rail_load_hash`; `init_rails` takes catalog parts).
> - **pv `ctmWorldAudio`** (off): the free-ride world load's audio when the load screen closes (`game-audio.js freeWorldLoaded`, main.js audioScreens): pktrans under the ABC1 movie (its MPC has no audio track) and the plane NIS, 28E8C0(19) at the ride start, as PS2 music/runs/newcareer. Before: charsel played on through both.
> - **pv `streamWarm`** (off): a streamed location's pipelines compiled for the world pass (`fog-renderer.js compileObject` at call depth 1; `free-ride.js warmSliced`: unculled, two-pass materials as BackSide then FrontSide, batches in a detached group sized to the frame's build budget). Before: compileAsync compiled almost nothing, so every pipeline was built on the first draw (A: 36 s in WebKit).
> - **pv `streamAhead`** (off): the PS2's read order, one background job at a time; the next row's draw files downloaded and built ahead (under the movie for A; at a station the connector nearest the rider); per-frame build budget (stalled 36 / wanted 10 / ahead 4 / idle 24 ms); the rest of the peak's collision only while nothing animates.
> - **pv `streamGate`** (off): an active row is held until its draw package is built. Turn it on only with `streamAhead` + `streamWarm` (alone it stalled 1.27 s at A_ARA1 on a phone).
> - **pv `sliceLoad`** (off): the load screen's long core calls in parts ending in the same core state (`stage_world_part`, `environment_add`, the first location sliced, path banks one per frame) and a streamed location's rail catalog in ~24 KB parts (one whole catalog was 80-220 ms on a riding frame at 4x). web/load-slices.js; the cutting runs in the peak-world worker.
> - **pv `lodgeWorldLoad`** (on): the lodge's Return to Game is the PS2's world load (a new rider at the 11D390 station entry, the world start's ride start): the ride equals the PS2 world-load ride (peak1-green-start) within 0.02 cm through tick 285, and the hub song replaces the lodge's charsel (before: 2.8x too fast, charsel on in the world). career-ui.js: one line at the ctm-saveprompt call site.
> - **Measured** (details in ctm-flow.md 8.3): worst stall 0 at every Peak 1 connector with all switches, phone 4x and desktop (the worst case too: into A_ARA1 the moment the rider reaches A); the station drawn exactly at the Load trigger in Chrome (production, 6 Mbit/s) and WebKit; Chrome phone 4x p50 / p95: the station 24.5 / 91.6 -> 18.5 / 39.0 ms, Happiness 28.4 / 64.7 -> 19.3 / 32.0; the load's worst frame 995 -> 246 ms (WebKit 813 -> 114); ?simtrace 900 ticks identical.
> - **Also** (streamAhead): the locations a connector leaves are released one a frame in 2 ms steps (one frame was 165 ms at 4x); the builds give the load screen, a movie or a pause a frame every 20 ms.
> - **Tests:** `web/test-ctm-stream.mjs` (in test:all): the read-ahead order, the sliced loads == the one-call loads by core hashes, the new-career world-load audio. Targeted runs green (ps2-captures, peak-world, peak2/3-world, mountain-world, rails, rail-gameplay, ctm-flow, audio-timeline, career, presentation, collect-restream); ?simtrace 900 ticks identical with all switches.
> - **Open:** phone long frames while a location builds are single node-material builds (40-55 ms at 4x, performance agent); event loads' core inits are still single calls (terrain / body terrain ~0.2-0.3 s each at 1x; plan in ctm-flow.md 8.4); the T-poses on the first load are not reproduced.

> **The Throne's summit flag pole (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 24. Not deployed. **pv `liveCompObject`** (on; `web/set-pieces-renderer.js`, `tools/export_livecomp.py draw_class`): a LiveComp owner the static collector skips (flags & 3 != 3) but its Object player (vtable 0x490B10, flags & 4) draws through 0x356298, `mdl_EBC3_summit_flag_pole_1000` at The Throne's start, is drawn while its player runs. Needs the EBC3 LIVECOMP re-export (the pole's draw 'none' -> 'object'). Test: test-visual-parity R12.

> **The plane drop's air streamer and air pose (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 23. Not deployed; live core rebuilt 14:41 (new exports `drop_air_seed`, `rider_fx_streamer_seed`). **pv `dropPose`** (on): the drop starts in the air controller with the PS2's air clips and the placement's zeroed ground-control triplets, not a passive departure carrying the seed's 8.5 cm lift: the skeleton is on the PS2's to 0.01 cm at f02747 and the camera within 1 cm through the fall. **pv `dropStreamer`** (on): the streamer ring as that tick has it (scroll 0.81, 4 rows), so the ribbon is strm's long bright band. `web/plane-drop.js` (`tools/export_plane_drop.py`) gains `animation` and `streamer`. Test: test-visual-parity R11.

> **Music streams bar by bar (2026-09-27, lazy-course agent):** see [audio-logic.md](audio-logic.md) "9.12 Music streaming". Not deployed; no core change.
> - **pv `musicStream`** (on): a song's `.mus` loads with range requests, one bar when the player commits it, read-ahead along the song graph (depth 2, 1 while the game's downloads run, priority low), the opening bars first (`pathfinder.js createMusStream / streamSongStart`; the loading layer only, the players untouched). charsel.mus no longer downloads whole (44 MB) at Press START.
> - Menu music at 6 Mbit/s 70-74 s -> 1-2 s after Press START; START -> race 124-144 -> 66-75 s; START -> CTM ride 190-194 -> 128-138 s; the race / ride song plays 0.3-1.6 s after the race / ride starts (was > 15 s). `.mus` on the wire 44-70 MB -> 2-4 MB. No bar late in the checked runs (`song.mus.late`).
> - To check on the live site: Cloudflare's answer to a `Range` request on a cache miss (206, or the whole file = the old behaviour).

> **test-slopestyle-bigair passes: the R&B held-out captures are exact (2026-09-27, slopestyle agent):** see [slopestyle-bigair.md](slopestyle-bigair.md) "Gaps". Core change, no switch. It is in the live core of 14:24 (built with the lodge agent's surface-18 patch; checked: griff / Psymon exact, dbc2-race-tuck human through 4435). Not deployed. No longer a known baseline failure.
> - **Human landing depths** (`web/attribute_bridge.cpp`):
>   - The touchdown / crash-exit ground entry 13C7A8 scales the material depths into motion owner +4/+8 by the rider's own scale. For the human, the port used the compiled Zoe seed (0.85): Griff (0.7) got 25.14 / 50.99 instead of 20.70 / 41.99, so the normal response was off after every non-Zoe-scale landing.
>   - `browser_apply_ground_attributes` now puts the human's scale on `landingProfile` too, which the crash sliding contact and detached body also read. A later Zoe in the same context gets the seed back.
> - **Held jump on a rail** (`web/rail_gameplay.inc`, `web/input_bridge.inc` `rail_held_balance`): control 2 on a rail steers with RailBalance (word1 bits 6..11), as 12E9B8's 12EC1C / 12EDF0 do, not PrewindTurn. Only the computer-rider provider packs them differently: Allegra slid 0.29 cm at 701.
> - **Goofy air spin sign** (`web/animation_bridge.cpp`, one line): 134334 negates the scored spin passed to 119898 when rider+0x324 is set. Psymon's score+0x34 was +0.0 vs -0.0 from tick 303.
> - **Captures:** `lineups-ASS1`: all five exact for all ticks (human physics and score, opponent, RNG, ranks). Before, griff failed at human 576 / Allegra 701 / RNG 821, Psymon's human at 589 and its score at 303, Stretch's human and score at 610.
> - **Tests** (scratch core, same base without the fixes as the reference): `npm test -- ps2-captures slopestyle original long-runs rail crash forced-reset detached-reset rider-reset ai-racers lineups rival-page` 22/22. The test-ps2-captures output (229 scenarios) is identical line for line to the reference.
> - **Follow-up, peak2/dss2-full** (the visual-parity sweep's 4018: 75 vs 44 MPH): the capture replay is exact for all 9000 ticks, both on the live core (15:48) and on a scratch core without these three fixes and without the surface-18 crash. The two reports are identical. The page run's divergence is harness-side (not a synced replay; visual-parity.md section 26). The capture is now gated (`peak2/dss2-full`, exact to the end) in test-ps2-captures.

> **Green Station's invisible walls, a fresh rider at a world start, the All Peak finish barriers (2026-09-27, station-fences agent):** see [peak-mountain.md](peak-mountain.md) "Station fences", "Fresh rider at a world start" and "Finish barriers"; [ctm-parity.md](ctm-parity.md) item 5. Not deployed. Three core changes: exports `peak_world_builtin108`, `fresh_rider_start`, `stage_object_route`, `stage_builtin_counts` (QA). All three switches are on.
> - **pv `stationFences`**:
>   - Stage builtin 108 (0x302870 -> 303E60, game mode byte 0x535C12 == table 0x446618[n]) answered nil.
>   - As a result, every station's peak-race fences stayed solid in free ride. The draw audit hides them, so they were invisible walls.
>   - At Green Station, the collision-only `mdl_A_fenceCollision_r_*` line stopped a neutral ride to the lodge at tick 230.
>   - The PS2 has them dead: flags 0x200204/0x200304.
>   - Checked at A, B, C, D and E: every race fence listed at the start is dead with the switch on, as in the PS2 free-ride savestates.
> - **pv `freshRider`** (`web/free-ride.js placeRegion`):
>   - A world start's rider is the world load's new one (0x125EB8). Speed limit, route heading, surface, previous normal, lean and boost words are 0 (drain word 1) before 11D390.
>   - The port started from the glide seed (limit 2219.7, heading -2.96, depths 25/51), 3.5x too fast, and overshot the lodge door.
> - **pv `finishFences`** (`web/world_bridge.cpp`, builtin 0):
>   - ARA1's `endmode_collide` and ERA5's `fencecollision_end` (the only flags-0 collision instances) were "unsupported". A query touching their boxes was incomplete (no contact at all).
>   - Builtin 0 key 1 (356DB0) never set the 0x20 route. So in free ride and the peak runs, the rider went through the drawn finish barriers and was reset (Owen's All Peak report).
>   - PS2 apr-part4 / apr-finish: 0x122 / 0x322 Object type 17. The same pushes stop / crash the rider in the page at the PS2's spot, in Chrome and WebKit.
>   - CRA3 / DRA4 finish barriers are ordinary static collision and already worked.
> - **PS2:**
>   - New capture `local/ps2-capture/runs/peak1-green-start` (CTM last-lodge start -> world load, derived state `peak1/green-start-t0.p2s` before tick 0, neutral pad).
>   - New gate in `test-ps2-captures.mjs`, with `compare-ps2-capture.mjs --peak-fresh`: exact through 393, into the lodge door at 394.
> - **Tests:** targeted `npm test -- ps2-captures peak mountain course-spawn stage` 11/11 with the switches on.
> - **Left:**
>   - The station start's +0x390 is the creation spot's normal, which is not reproduced.
>   - The Happiness plane drop does not take the fresh rider, on purpose.
>     - The PS2 rider at the drop (menus/fr/ctmstart f02700) is not fresh: it comes out of the plane cut's limbo (control 13 / motion 3). Its +0x4CC is 3.0498, +0x380 (0, 0, 1) and +0x2E4 3333.33 (the air limit, recomputed every air tick). The glide seed's values differ too, but none of them is read before they are overwritten.
>     - With `fresh_rider_start` first, test-visual-parity R10 and the R11 drop setup (drop_air_seed, streamer seed) are bit-identical (rider, bones, camera words) through 235 / 400 ticks.
>   - Builtin 0's key-2 draw change is not ported.
>   - Events need no change (corrected 2026-09-27). The Snow Jam / Gravitude event seeds' countdown skip list gives the ten flags-0 instances runtime flags 2 (`{156168u,0u,2u}`...). Body, ray and roller queries test that route (Skip) before the "unsupported" label, so they ignore the instances as the PS2 does.
>     - Checked in the page: ARA1 and ERA5 event races, 8 pushes through each box, 0 incomplete queries, no contact.
>     - The label only shows in the QA list `world_collision_unsupported`.
>   - In a streamed free-ride run, only builtin 21 (UV scroll, JS-owned) still answers nil.

> **Rival card: the rival rider and the ready camera; Ruthless 3911 (2026-09-27, lodge agent, round 3):** see [career-events.md](career-events.md) "Round 3" and [crash-motion.md](crash-motion.md). Not deployed.
> - **pv `rivalCardAi`** (on): the objectives card of a rival challenge now shows the computer rider at its start spot.
>   - web/ai-race.js `readyPose` runs `racers.start()` only (no relationship ageing), poses with an animation step of 0, puts the RNG words back and captures the renderer. main.js `readyView` calls it.
>   - The card's camera is the ready state's: the event camera seed, read through a new read-only core export `camera_event_seed_view`. A PS2 run from happiness-ready keeps the same eye for 900 card frames. Mac's board nose sits at the right edge as on the PS2; Nate and Psymon stand beside the camera, off screen.
>   - Checked in Chrome and WebKit at 4:3. test-rival-page is exact with the switch on and off.
> - **Ruthless 3911** (web/core.cpp, web/animation_bridge.cpp): 0x13F178 crashes a grounded rider on surface 18 at once (10EB30 semantic 360); the port only had the reset branch.
>   - peak2/dbc2-race-tuck: human 3910 -> 4435, RNG 3932 -> 4581, pair records 3911 -> 4439.
>   - Nate: exact ticks 6425 -> 6982 of 7000, but his first inexact tick moves 6426 -> 5142 (a 1-ulp velocity). His provider words differ from the PS2 from 5004 in both builds. The gates in test-ps2-captures.mjs and test-rival-page.mjs are updated, with a comment.
>   - Open: 4436. The detached board's +0x180 frame feeds the 1057B8 bounce counter +0x3F4, so the reset out of the second crash comes a tick late (crash-motion.md).
> - **PS2 evidence:** local/ps2-capture/lodge/runs/rc1 (the card camera over 900 frames). The capture's 105D98 hook (record 8920) shows no collision event at 3910. The decomp asm of 0x13F178 has the surface-18 branch.

> **Metro-City phone booths and water towers teleport (stage builtin 34 -> 0x123210) (2026-09-27, booth agent):** see [stage-teleport.md](stage-teleport.md) section 4 and [stage-scripts.md](stage-scripts.md). Not deployed. The live core was rebuilt at 13:54.
> - **Symptom:** riding into a Metro-City phone booth did nothing in the race (Single Event and career) or in the Peak 1 / whole-mountain free ride. The rider stayed in the booth. Each tick in the beam ran program 419 again and drew the shared RNG again.
> - **Cause:** the rider side of builtin 34 was never written. `browserStageTeleport` was never assigned, so every call was counted as unsupported.
> - **Scope:** these are the BRA2 beams, not the station transport booths (builtin 68):
>   - booth 0004 goes to 0005 (75 %) or 0006, drawn from the gameplay RNG;
>   - booth 0007 goes to 0008;
>   - water tower 0001 goes to 0002 (90 %) or 0003.
>
>   Metro City is on Peak 1.
> - **pv `boothTeleport`** (on): `web/stage_teleport.inc` ports 0x123210 for the human and the computer riders:
>   - the slot offset, 119368, the motion and control exits, 13C7A8, the two camera cuts (`browser_camera_teleport_cut`, web/core.cpp `camera_input_for_head`), the speed, 11D660 with the 11EB98 leg solve, and v = forward x speed;
>   - `core._stage_teleport_enable` per rider core (main.js `applyPresentationFast`);
>   - web/game-tick.js re-reads the state and posed frame after race_end on a teleport, and marks the tick as a placement;
>   - `ssxQA.aiRace()` for QA.
> - **PS2** (`local/ps2-capture/runs/booth/`, `tools/ps2_booth_inject.py` / `ps2_booth_camlog.py`, derived states, silent): no route to the booth was found on the PS2, so a hook at 0x121820 sets rider+0xA30 = the beam instance on chosen ticks. The captures:
>   - `teleports`: 0004 x4 (0005 x3 and the 25 % 0006), 0007, and the water tower from the air;
>   - `from-crash`, `from-passive-air`, `from-rail`;
>   - `computer-riders`: all 5, both booths;
>   - `camera-log`, and `teleports-ai` for the RNG attribution.
> - **Checks:**
>   - Comparer, pad replay: physics, score, bones and boost are exact on every tick of `teleports` / `from-crash` / `from-passive-air`. The human's RNG draws equal the PS2's on every tick (one draw per firing, in its 121818).
>   - Chrome and WebKit: a Single Event Metro-City race from the menus, and the Peak 1 free ride. Riding into 0004 gives both outcomes, 0007 and the water tower land at the PS2 positions, and the five computer riders land at the PS2's slot destinations.
>   - Tests: `npm test -- ps2-captures stage-world` pass: 2/2 on the live core, switch on, 225 capture scenarios including the new `booth/*` gates. The earlier capture gate with `STAGE_TELEPORT=1` on the first build also passed; no course capture crosses a beam, so every BRA2 / PEAK1 / MOUNTAIN capture is unchanged.
> - **Gaps:**
>   - After a far teleport (0007, water tower) the PS2 stops the game for ~30 frames to stream, while the camera keeps stepping. The port has no stall, so its camera catches up over the next ticks instead.
>   - `from-rail` leaves the PS2 by 0.04 cm/s at 2303.
>   - The outer camera's own DEFAULT_3 (Y+0xC0) and 2F1A00 are not modelled.
>   - The online human uses slot 0.
> - **Online (follow-up, same day):** three of the five outcomes jump further than a reset may (200 m): 0007 -> 0008 438 m, and the tower -> 0002 / 0003 210 / 387 m. The online judge rejected an honest finish through them.
>   - `web/server/plausibility.mjs` now accepts such a jump only when `web/server/teleport-beams.mjs` explains it: from a beam's trigger box to one of its destinations (any slot offset, the probe's z range), within the packet interval's travel + 3 m, on BRA2 / PEAK1 / MOUNTAIN*. This works with the placement counter +1 (what the core does) or +0.
>   - `mp-server.mjs` passes the lobby's course.
>   - Clients: the teleport bumps `placements`, so remote riders do not interpolate across it and the FX puppet resets.
>   - `test-mp-plausibility.mjs`: real core teleports for all five outcomes pass, the same jumps moved 60 m or on another course are found, and the table equals the stage data.
>   - `npm test -- mp-plausibility mp remote-riders`: 12/12. See [multiplayer.md](multiplayer.md) "Stage teleports".

> **The CTM plane drop's camera (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) section 22. Not deployed. **pv `dropCamera`** (on; `web/plane-drop.js` from `tools/export_plane_drop.py`, `main.js resetPhysics`, `free-ride.js PEAK_STARTS[14] drop: true`): the drop now starts as the PS2's first ride tick has it: the rider lying head first (quaternion +0x120, forward 68 degrees down) and the camera's words 10 ticks into the lock of the drop's set-target, so it looks down the slope instead of at the horizon. The rider's orientation also makes the landing follow the PS2: rider and camera within 0.5 cm through tick 235 (20 cm before). Checked in Chrome and WebKit; test-visual-parity R10.

> **Rendering performance pass (2026-09-27):** see [web-render-performance.md](web-render-performance.md). No core/source arithmetic or quality-setting change; not deployed.
> - Cache static world/sky and refined terrain local transforms; stop the identity scene root from forcing every descendant to recompute. Moving/LiveComp matrices and parent propagation remain live.
> - Upload flags only after cloth parity updates/new attachment; upload only active board-trail prefixes. Reuse shadow pose buffers and moving-instance matrix offsets instead of per-frame copies.
> - Same-scene paired matrix benchmark: Chrome ~75–76% less matrix-update CPU time; WebKit ~79%, zero matrix differences in both. These are operation timings, not FPS gains. Repeatable Chrome harness: `cd web && node bench-render-transforms.mjs`.
> - Nine targeted checks and private-output Vite build passed; flag/traffic/shadow PS2 references unchanged. Shared full-suite/build lock prevented the filtered runner; two WASM checks instead used a byte-verified private runtime copy. No full suite launched. New tests registered: `test-moving-instances.mjs`, `test-board-trail-renderer.mjs`.
> - Evidence and baseline source snapshots: `local/performance-pass-20260927/`. Concurrent plane-drop work in main.js was preserved.

> **Kicker snow at a free-ride start, the rival card's headline wrap, the player's standings row (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) sections 14 and 21. Not deployed; live core rebuilt 13:22.
> - **pv `sprayReset`** (on; `animation_bridge.cpp` `set_fx_reset_kicker` / `rider_fx_reset`, `main.js` `resetPhysics` / `applyPresentationFast`): Owen's "spray going the wrong direction" after the first CTM spawn was the kicker (emitter 8) spraying off a charged buildup (the seed's 1.37) while the rider falls from the plane, so the snow rose away from it. The PS2's FX reset 2DF3B0 zeroes FX+0x10 at every placement (ctmstart: 0 through the fall). Not the fast path (`presentation_fast.hpp` matches the exact path to 0.08 cm), not a position delta.
> - **pv `luiWrap`** (on; `lui-player.js` `ps2Wrap`, `results-lui.js`): a results-panel text with flag 0x80 breaks where 0x3A0D00 does (font advances x scale % against the width), so the rival card reads 'Face off against Nate in a Rival' / 'Challenge!' as on the PS2; Griff stays on one line.
> - **Checked, no change:** `hudStandings`' player row against the PS2 frame `heats/h2-scorepoke/sample00303` ('ZOE 402000', red box x 20..205).
> - **Open:** the plane drop's camera looks at the horizon where the PS2 looks down the slope; the air streamers (drawn on both) therefore show above the rider in the port.

> **Lodge round 2: Cheat Characters, the Rewards room, the rival challenge cards; the Transport lists and race placement checked (2026-09-27, lodge agent):** see [career-events.md](career-events.md) "In play" > "Round 2" and the gaps list. Switches `lodgeCheats`, `lodgeRewards`, `rivalCard` on (web/pv-flags.js) after the PS2 runs were matched in Chrome and WebKit. No core change. Targeted tests: `npm test -- career rival-page rival-mode ctm-flow ctm-left fe-screens characters replay` 9/9 (test-rival-page stays exact with the card view).
> - **PS2** (`local/ps2-capture/lodge/runs/l11..l13`):
>   - l11: Rewards > Cheat Characters sells Brodi / JP / Marisol ($) in the Peak 1 lodge.
>   - l12: Rider Details > Cheat Characters becomes live and the pick writes setup slot 0x535B20 +0x12 = 10 only.
>   - l13: the rider rides out as Brodi on Zoe's career.
> - **pv `lodgeCheats`** (lodge-ui.js, character-select.js `overlay`, main.js `careerId`, game-tick.js):
>   - The lodge's cheat list over the Rider Details; the pick is the career rider.
>   - The collectibles, free-ride cash, Big Challenges and HUD read the base rider's record. They read the skin's before ($0).
> - **pv `lodgeRewards`** (fe-options.js `lodge` / `askBuy`, fe-screens.js): the lodge's Rewards is the rewards room that sells, with the '$' slots and the PS2 buy popup. The canvas list over black is gone.
> - **pv `rivalCard`** (main.js `readyView`, career-ui.js `begin`): the rival challenges' cards were drawn over black. They now lie over the ready state as on the PS2 (`local/reference/pcsx2/*-ready`).
> - **Checked, no change:**
>   - The Transport freeride lists equal table 0x478D38 on all three peaks (now tested).
>   - Race placement is connected on every race course.
>   - The Peak 2/3 rival challenges play with their capture gates.
> - **Open:** the rival computer rider is not drawn under the card; the card headline's LUI wrap (Nate); Ruthless's PS2 crash at 3911.

> **The event's riders alongside the course, and one course parse fewer (2026-09-27, lazy-course agent):** see [first-load.md](first-load.md) "The event's riders and the course parse". Not deployed; no core change.
> - **pv `riderPrefetch`** (on): the human rider's files download from the event pick (alongside a course still loading), the lineup is planned before the human rider loads (`ai-race.js plan`, the same one roster seed per load) so its riders download alongside, the intro's cutscene data under the warm-up (`cutscenes.js prepareData`). `downloads.js prefetchDownload(url, {keepMs})` / `peekDownload`. Pick -> race at 6 Mbit/s with the course in: Chrome 10.4-18.6 -> 7.5-7.9 s, WebKit 15.0 -> 10.6 s; START -> race with an early pick 3-4 s sooner.
> - **pv `sharedParse`** (on): the human core parses the course packages' own text (as the node gates), so the first computer rider copies that parse instead of parsing again: two of the 100-200 ms menu hitches gone, identical races on nine courses (`test-shared-parse.mjs`). The other four big calls (`_init_world/terrain/world_collision/body_terrain`) need a core change to split.

> **Big Challenge start stinger, Wobble's own mix (2026-09-27, challenge-audio agent):** see [audio-logic.md](audio-logic.md) 3.9. Not deployed; no core change, no asset change.
> - **PS2:** the start sound is the song's loop-bank "long ending" played by its own Pathfinder graph: `29D6E0` attaches the loop bank (`2B4620`) and posts 33 / 34 / 38 by the challenge type (row +0x22 of 0x43EE10; 49 of 88 challenges are type 0: no stinger); the event cuts track 0, plays the stinger on track 1 and starts the challenge part 1200 ms later. Every bank has the same two stingers; **Wobble's `wbloops0.mus` has a third, entry 66 (2766 ms), picked on 47 / 128 of (now_ms / 23) & 0x7F**. A hub song playing gets a playlist song instead. `29D8E0` adds sound 0x6D + Arcade_Prompts 1. ARMSX2 (silent, derived states, scratch `ps2/`): Wobble starts committed 649 / entry 66 twice and 650 / 64 twice, Avalanche 488 / 489; completion 0x6D + 0x20A8.
> - **Port before:** 33 / 34 / 38 were sent without the attach, so the player never scheduled track 1: music cut + 1.2 s silence, no stinger for any song.
> - **pv `bigChallengeAudio`** (on): `web/game-audio.js` challengeStart / End / Stop / Accepted = 29D6E0 / 29D8E0 / 29DBB0 / 29D6D0 (hub branch, forced 8, type 0 flag, the 28D630 zone gate, 28BF78 clear); `web/sfx-game.js` challengeStops / challengeComplete; `web/big-challenges.js` kind 7; `web/pathfinder.js` player options `loopDestination` (track 1 on the engine master at the 3D41A8 byte: the PS2 overlay does not take the live MUSIC channel) and `clockRandom` (random heads = (audio ms / 23) & 0x7F). New `web/test-challenge-audio.mjs` in npm test.

> **"Press START" before the course (2026-09-27, lazy-course agent):** see [first-load.md](first-load.md) "Title before the course". Not deployed; no core change.
> - **pv `lazyCourse`** (on): the title waits for the front end only (code, core compile, menus, GPU: 4.0 MB on the wire instead of 36.2 MB); the page's course loads behind the menus from Press START (`web/main.js` `lazyStart` / `backgroundCourse` / `feFrame`), the event load screen covers the rest with its percentage held to the work done (`boot-screen.js` `courseProgress`, `loading-screen.js` `workFraction`). Another course picked meanwhile abandons it and loads first. `?course=&autostart=1` unchanged; `?qa=1` publishes `ssxQA` once the course is live; scripts that used `ui.ready` as "course loaded" should wait for the `course:live` mark or pass `&pv=-lazyCourse`.
> - **Press START:** 6 Mbit/s 60.8 -> 6.5 s (Chrome) / 61.0 -> 6.2 s (WebKit) first visit, 12.2 -> 1.7 s repeat; 50 Mbit/s 10.1 -> 1.0 s, 4.6 -> 0.5 s. First race (5 s in the menus) 4-8 s sooner. Memory: lower until the course is in (Chrome 874 -> 567 MB, WebKit 770 -> 537 MB at Press START), the same at the race.
> - **Left:** the menus miss a few frames while the course builds behind them (100-157 ms gaps: single core calls and the computer riders' set-up); `boot-files.json` re-recorded (`boot-files.mjs record` forces `pv=-lazyCourse`); `web/test-lazy-course.mjs` in npm test.

> **Front-end and career screens: the Transport map, the MCOMM icons and badge, the freestyle heat card and results, the boot / attract movies (2026-09-27, front-end agent):** see [ctm-parity.md](ctm-parity.md) "Transport map, MCOMM badge, freestyle heat results" and [intro-movies.md](intro-movies.md). Not deployed; no core change.
> - **pv `transportMap`** (on; `web/ctm-map.js`, hooks in `career-ui.js`): the Transport screens draw the Map LUI (0x5380, the FE.LUI export with its sprites moved to the OV pages):
>   - the title icon, the picture and tabs;
>   - Select Peak's outline, "You are here" and locks;
>   - every route with 0x206690's red set, the start indicator, the base-station icons.
>   Checked against PS2 Peak 1 and Peak 2 frames (`local/ps2-capture/menus/transport-map/`) in Chrome and WebKit.
> - **pv `mcommIcons`** (on; `web/ctm-pda.js`): the MCOMM icons without canvas seams (one path per flat shape) and the badge temperature set when the PDA opens (0x20A778 / 0x20A854). The icons and badge themselves were already ported, so career-events.md's "not reproduced" line was stale.
> - **pv `fsStandings`** (on; `web/fs-standings.js`; new asset `UI/fs-standings.json` from `tools/export_fs_standings.py`, to be copied):
>   - the CTM qualifying heats' results are 42freestyle_standings: Heat 1 / Heat 2 / Total, the help lines, the pulsing human row;
>   - the heat cards are 41freestyle_pre, with 'Current standings' in Heat 1 / Heat 2 / Total.
>   New PS2 heat 1 and heat 2 runs (`transport-map/heats/`) match row for row in Chrome and WebKit. `results-lui.js` is untouched; the final keeps 43final_standings.
> - **pv `attract`** and **`bootMovies`** (both on; `web/fe-attract.js`, hooks in `ui.js`; `fe-movie.js` gained `skippable` / `reveal`):
>   - `attract`: the title's 1801-frame idle plays intro.mpc.
>   - `bootMovies`, as Owen chose it: the DJ intro once per page load after `boot:ready`.
>     - No EA SPORTS BIG / THX: they stay behind the mask, `?bootlogos=1`.
>     - Skipped by the first Start / Cross / click / tap.
>     - Dropped when it has not started within 2 s.
>     - Not played for event / online links, `?qa=1`, automated browsers, or a later return to the title.
>   - Assets `MOVIES/EABIG.mp4`, `THX.mp4`, `INTRO_DJ.mp4` are live. `tools/export_movies.py` gained those three keys and `--out`.
> - **Tests:** `test-fe-attract.mjs`, `test-ctm-map.mjs`, `test-fs-standings.mjs` (npm test).
> - **Open:**
>   - (Resolved) After a won heat 2 the menu's first item is 'Final Round', on the PS2 as in the port: PS2 `transport-map/heats/h2-advance`, the game's own finish with three computer scores poked to 0. Only a lost heat 2 shows 'Transport'.
>   - No Peak 3 Transport frame was taken.

> **Conquer the Mountain lodge in play: equipped gear, Ubertrick Setup, songs, Player Name, the lodge's Rider Details (2026-09-27, lodge agent):** see [career-events.md](career-events.md) "In play" and its corrected gaps list. Not deployed; no core change, no asset re-export. Four switches in `web/pv-flags.js`, all on after the PS2 runs were matched in Chrome and WebKit. Full npm test 166/167, the known test-slopestyle-bigair.
> - **PS2** (ARMSX2, `local/ps2-capture/lodge/`: `states/lodge-clean.p2s` = menus/ctm/state-lodge-peak1 without its menu-capture hook, runs l1..l10 and name-records):
>   - Madonna bought and selected in the lodge's Ubertrick Setup writes 0x5316D1 = 2, which 0x14FEA8 reads in the air (capture gates monster-stoneage / -swollen / -xexec).
>   - Player Name is "PLAYER 1", 8 characters ("PLAYER a").
>   - Two songs bought give owned / playlist 0x3; Custom Playlist [DJ] sets R+0xF80 = 1, applied at the lodge exit's world load.
>   - A board bought and equipped is ridden out of the station.
>   - A poked name shows on Top 5 Record Times.
> - **pv `careerRider`** (new `web/career-rider.js`; main.js `selectRider` / `riderStale` / `ensureRider` / `startRun`, `ui.cb.refreshRider` under 118loadoutlodge; character-roster.js `uber_choice`; wardrobe.js `outfitStamp`). The rider entry carries the profile's uber rows and the outfit of the current mode. It is resolved again when the mode, the gear record or the selection changes. Before, three things kept the defaults:
>   - CTM free ride wore the free-play outfit.
>   - After the lodge's Return to Game, the Equip Gear and uber changes showed only at the next event.
>   - An uber selection reached the rider only through the FE screen, in memory.
> - **pv `lodgeDetails`** (lodge-ui.js, fe-screens.js `openFromLodge` / `askBuyUber` / `BUY_POPUP`):
>   - The lodge's Rider Details has Career Highlights, Player Name (keyboard) and Rider Profile live, and Down skips Cheat Characters.
>   - Ubertrick Setup is 66ut_btnmap with the PS2 buy popup.
>   - The lodge's Ubertrick Setup and Equip Gear previews animate over the paused free ride (main.js).
> - **pv `playerName`** (fe-screens.js, career.js `playerName`): the PS2 default, length and full-name overwrite; the name goes on the records.
> - **pv `riderMusic`** (audio-menu.js `riderMusic` / `worldMusic`, game-audio.js `worldLoaded`): radio mode and custom playlist per rider, applied at each world load.
> - **Tests:** new `web/test-career-rider.mjs` (npm test); test-fe-screens expects `defaultPlayerName()`.
> - **Open:** the lodge's Cheat Characters (a skin for the career); online races keep the package's uber rows. WebKit checks here used a scratch copy of webkit-driver.swift that drives rAF by timers, because the driver window was hidden (display asleep).

> **Visual parity round 2: the air streamers (Metro City "light shafts"), the crash recover meter, freestyle standings, OV_darkblue header lights, "$ " gap, the results menu wrap / focus (2026-09-27, visual-parity agent):** see [visual-parity.md](visual-parity.md) sections 12-17. Not deployed. Full npm test with the rebuilt live core (00:31): 162/163, the known test-slopestyle-bigair.
> - **pv `streamers`** (on): the "light shafts" are the rider's air streamers 2EF950 (PS2 draw bisection, `local/browser-validation/visual-parity/round2/shafts/`). `strm` sampled with its bright rows at T = 1 (boost-renderer.js); nose / tail offsets and width from the 0.85-scaled render matrices (core export `set_rider_fx_render_scale`, boost_gameplay.inc; main.js sets it every frame for the human and computer riders).
> - **pv `recoverMeter`** (on): 0x21D9A0 label / bar / red fill (trick-hud.js `recoverMeter`, `iconLayout`, `quad` items). Needs `round2/recover/trick-hud.merged.json` copied over `web/public/assets/UI/trick-hud.json` (sent to the coordinator); the old placeholder draws until then.
> - **pv `hudStandings`** (on): freestyle standings rows 0x1ED104 (trick-hud.js `standings`). **pv `luiLights`** (on): OV_darkblue's six header lights (exporter now pairs frame-0 records by name). **pv `cashGap`** (on): "Cash: $ 10,000". **pv `resultsMenu`** (on): LUI texts wrap only with flag 0x80 (lui-player `flagWrap`: "Next event" on one line); a Single Event's results open on Restart (0x1E7558).
> - **pv `uberLayout`** (on): the Uber hint from the 0x1E92A8 record (text at x 215, was 212; box error over the new PS2 frame `runs/uber-chain.tick929.png` 12.3 -> 6.3).
> - **pv `byteBlend`** (off): the static-model additive class drawn in the encoded pass with the fog scale (world-material.js, fog-shared.js); no visible gain measured: the searchlight beams' remaining gap is elsewhere (section 17).
> - **Tools:** `local/browser-validation/visual-parity/tools/vpeval.mjs` (variants, `--route` CDP request routing, `--init`, `--base`), `proxy2.mjs` (WebKit routing proxy), `r2stage.mjs` (staged standings / results / rewards), `cc.mjs` crops now resize 448 -> 480 like diff.mjs.
> - **Open** (visual-parity.md 19): searchlight beams 1.77x brighter on the PS2 (not blend space, not CLUT alpha), board spray near the camera, terrain mid-distance LOD (analysed, section 18), the results menu wrap-around. The WebKit sweep pass could not run after ~00:45 (display asleep: no rAF in WebKit windows); rerun `OUT=../round2/sweep3 BROWSERS=wk ./batch-first.sh` in `local/browser-validation/visual-parity/tools` when the display is awake.

> **The race replay behind the post-race screens and the results' Replay item (2026-09-26, replay agent):** see [replay.md](replay.md). pv `replay` **on** (web/pv-flags.js); live core rebuilt 23:13 (web/replay_camera.inc, a restart fix in stage_world.inc / stage_script_gameplay.inc, 6 exports); new assets `<CODE>/camera-triggers.json` (17 courses, `tools/export_camera_triggers.py --assets`) and `UI/replay-screens.json` (`tools/export_replay_screens.py`). Not deployed.
> - **PS2:** the original re-runs the game (a start-gate snapshot + each human's controller command); the auto replay plays from the countdown to the finish tick, 1x, looping, no HUD, pad ignored, Web-cam (trigger cameras, 10 s return to the preferred camera); the Replay item opens '64replay' paused on the first frame (Cross, Circle, R1/L1 snapshot skips, Triangle 9 cameras, Square timeline, Start Replay Menu -> Exit replay back to the results); peak runs, free ride and online have none. ARMSX2 frames / logs in `local/ps2-capture/menus/replay/`.
> - **Browser:** `web/replay.js` records the run's pad (a byte change stream, ~200 bytes for a keyboard run) and the start words the restart path does not reset; a replay calls `startRun(R)` on the same core and feeds the pad (no second copy of the core, no input latency). Every replayed tick equals the live run's ?simtrace (all 17 ported events). `web/replay_camera.inc` is the replay view (Bounded / switch cameras from the course triggers, Relative, Manual, Near/Mid/Far); `web/replay-ui.js` the overlay; game-tick.js / sfx-game.js skip the HUD, rumble, career, results and the 288AE0-blocked sounds during a replay. Verified: PS2 bhp1-neutral camera cuts on the same ticks, Bounded eyes to 0.03 cm; Chrome + WebKit; `web/test-replay.mjs` (npm test).
> - **Gaps:** Look Back / Direction N use DEFAULT_3's chase constants; R1/L1 re-simulate to the kept snapshot (~40 ms of ticks a frame); Save replay greyed; online races keep the finish camera.

> **Presentation round 3: the #163 plane camera, the transport loop's canvas alpha, terrain snow sparkle, the NIS projection (2026-09-26, presentation agent):** see [presentation.md](presentation.md) sections 12-16. Switches in `web/pv-flags.js`, all on after PS2 / Chrome / WebKit comparisons.
> - **planeCam:** a NIS camera anchored on a live actor (anchors 40..60) takes that actor's frame at the cut (`cutscenes.js liveCutFrame`): #163 camera 5 sits at the door, PS2 eye within 1.5 / 3 cm (was 3.25 m back).
> - **acrossLoop:** the across-switch draw writes canvas alpha 1 (`opaqueAlphaFill`); WebKit was not darker on screen (the 2D-canvas QA capture unpremultiplied a translucent canvas).
> - **sparkle:** the "falling snow" under the plane NIS is the terrain snow sparkle (0x38D968 -> 0x38D690, VU1 program 4 at 0x1460, patch flag 0x800000), drawn on the near snow in every ride. `web/terrain-sparkle.js` (CPU selection 0x22C410, the VU RNG chain, twinkle rows, GS 0x49 in the encoded composite) with `terrain-sparkle.bin` per terrain package (`tools/export_terrain_sparkle.py` v2). The count's last factor renderer+0xC4 is **world painter type 10 (Surface)**, ported: 1.0 without a section (ARA1, DBC2, EBC3, ...), ABC1 1.8 / 4.0, CRA3 1.5 / 3.0, BRA2 2.0, CBA2, CHP2. Checked on ARA1, ABC1, CRA3, EBC3 against PS2 on/off frames at the same camera (glint energy within ~20%, 50-73% of the sprites on PS2 dots), free ride on all three peaks; 0.05 ms / frame (0.2 ms at CPU 4x), shaders 240 / 36 B.
> - **nisProjection:** a NIS view takes the PS2 letterbox block's projection (2EAA28: scales 0.75 / 0.63, slid with the bars by 2EA900; Anamorphic keeps its frame), so P11 / P00 = 1.12 instead of 4:3 (the page's NIS pictures were 19% too tall), and idle steps (objectives card) the player's own block (were 1.33x too wide). Verified on #163, #153, #137, #96 and the podium against PS2 frames in Chrome and WebKit (presentation.md section 15).
> - **bcHeli / heliLight / heliHover (on):** the backcountry heli arrivals (#123-137) moved no heli: their stage calls animate each location's locator-5 `os609` LiveComp (frames 0..185 / 186..550, cleanup = the 551..677 hover loop, sound 201), now `SETS/<LOC>HELI` via the ABC1PLANE mechanism. The heli is a lit instance (flag 0x4000: per-vertex irradiance from the location's object bank xOBR1, VU1 program 3 x128, replacing the baked colour) and hovers on after the NIS (`cutscenes.linger`). DBC2 / EBC3 PS2 frames: MAD e.g. 40 -> 16 -> 9 (heli, then lit); Chrome = WebKit (presentation.md section 16).
> - **Tests:** `test-presentation.mjs` (plane camera, acrossLoop, sparkle: counts / LOD / VU sprites / twinkle / Surface densities; NIS projection P00 / P11 of 4 PS2 states), `test-shader-budget.mjs` (sparkle pipeline on DBC2 / PEAK1). Tools and PS2 pairs: `local/browser-validation/presentation/sparkle/`, `local/ps2-capture/presentation/sparkle/`.

> **Visual parity sweep vs PS2 frames: race HUD text, UI atlas alpha, the OV.LUI results panels, the finish banner (2026-09-26, visual-parity agent):** see [visual-parity.md](visual-parity.md) (ranked list, method, fixes). 64 PS2 / browser pairs over 17 capture runs (every Peak 1-3 event course, night, weather), browser camera pinned to the PS2 record camera, Chrome and WebKit; tools in `local/browser-validation/visual-parity/tools/` (`vpshot.mjs`, `batch.sh`, `analyze.mjs`, `res.mjs` for the result screens). The world matches (bias within +/-5 levels, no gamma / fog / tint error; WebKit = Chrome); the differences were in the HUD and the menus.
> - **pv `raceHud`** (on): the race clock 0x1F16C0 -> 0x1F1840 at descriptor 0x1A, the speed widget 0x2200C0 at descriptor 25, the freestyle clock (red override 0x4C8688) and the slope style OPPONENT line (descriptors 0x4A / 0x4B, colours 0x4C88C8 / 0x4C88A8 / 0x4C8888) through `web/trick-hud.js` (`raceClock`, `speed`) instead of `ui.text`. HUD clock / speed region error vs PS2 roughly 3x smaller.
> - **pv `hudText`** (on): HUD font draws at trunc(colour x 204) (renderer 0x378808: white HUD text peaks at 204 on the PS2).
> - **UI atlas alpha** (asset fix, copied in by the coordinator): FE_1 / OV_1 / SU_1 carried 4 x the PS2 alpha (the SSH handler doubles, `tools/sam_ps2 Program.cs` doubled again, now fixed with `AlphaFix`); 25 PNGs re-derived. Uber coil and every FE screen closer to the PS2.
> - **pv `luiResults`** (on): `web/results-lui.js` draws the in-race panel `OV_darkblue` and `43final_standings` / `61toptimes` / `70peakchal_results` / `40race_pre` from `tools/export_results_screens.py` -> `/assets/UI/results-screens.json`; `career-ui.js` falls back to its own drawing without the export. `lui-player.js` gained opt-in `unionFlat` (no WebKit seams in translucent fans). The rewards list (`62reward_list`: indented items through the new opt-in `keepLead`, no focused row under 10 lines as 0x1FFD08) and the rival / peak run card (`68rival_pre`) are wired and need the 7-screen export (staged in `local/browser-validation/visual-parity/results-lui/`). OV_darkblue's two header lights are left out (they landed in the top-left corner).
> - **pv `finishBanner`** (on): the finish / time-up banner 0x21F660 through `trick-hud.js finishBanner()`: 'fini' at descriptor 0x42 (213 x 41 at (320, 190)) with the finish time (races, `finishTicks` from `main.js`) or score (freestyle) under it at 1.9076 x; 'timeup' at descriptor 0x1B. Matches the PS2 frames to the pixel in Chrome and WebKit.
> - **Tests:** `web/test-visual-parity.mjs` (npm test): layout numbers from the disassembly, the 204 text byte, software-rendered clock / speed / OPPONENT line against PS2 frames, the UI atlas alpha rule against the SSH palettes, the LUI panel positions against PS2 frames, the finish banner layout and its pixels over three PS2 frames.
> - **Open** (visual-parity.md section 3): Metro City start light shafts (source not found), crash recover meter 0x21D9A0 (traced), freestyle standings rows (0x1ED100.., traced), translucent world models blending in linear space, the freestyle pre-event card, the OV_darkblue header lights, terrain mip detail, spray shape; terrain sparkle belongs to the sparkle agent.

> **Presentation round 2: the plane's spray and engine, the sky behind the transport loop, the fly-over prefetch measured on phones, painter regions on the core's tick (2026-09-26, presentation agent):** see [presentation.md](presentation.md) sections 1, 2, 3, 11. Not deployed. New switches in `web/pv-flags.js`, on after PS2 + Chrome + WebKit checks: `planeFx`, `heliSky`, `regionTick`; `flyover` stays off. Core: `web/presentation_core.inc` (new, included by animation_bridge.cpp), `environment_bridge.cpp` (located Fog / Lighting records), a 2-line keep in `stage_world.inc stage_world_reset`, 7 exports in build-core.sh (compiled into the 13:35 live core).
> - **planeFx:** `web/cutscene-plane-fx.js`. The engine = script sound 201 (TRANSPORT 1, 300 m, following the plane) from the plane calls; the spray = ABC1 global program 2 run in the core (`stage_global_call`, a private stage VM before the first run, the plane's position for the 300 m rider test), ticked under the NIS and kept through the run start. PS2 new-career fr.p2s: the same two Particles on ospreySpray_1001, 90/100 emitter words identical; ride-start clouds match (`plane-spray.png`, `wk-plane-spray.png`).
> - **heliSky:** the gondola's see-through windows showed the clear colour during the switch; `cutscenes.js acrossSwitch` now draws its own copy of the departure's sky dome on black (`heli-sky.png` vs PS2 to-final s320, `wk-heli-sky.png`). The heli cabin's windows are opaque textures (no change).
> - **flyover (off):** Chrome 390x844 DPR 3 CPU 4x and WebKit, renderer phys_footprint: the round-1 prefetch held three copies of the 68 MB package (+300-400 MB under the fly-over, +190 MB on the peak); `downloads.js prefetchDownload` (one copy, kept until the first request) and a one-buffer read (behind pv flyover) bring it to +70-80 MB on the ~1.0 GB peak: not accepted for phones. The phone-speed hold is 50-60 s (Chrome 4x) / 30 s (WebKit): the compute. Proposal (prepare / commit split of `switchCourse`, for the sim-worker agent) in presentation.md 3.
> - **regionTick:** Fog / Lighting records in the core by gp+0x770 (camera block the same tick, the human's Lighting the next), ScreenTint / Sun / glare via `web/painter-regions.js` in their per-tick step; fed with each location's collision data (`peak-world-prepare.js`). PS2 frd-regions re-run with savestates 2841..2847: the camera Sun switches at 2842 (390.1 / 390.3 / 390.4 at 2842 / 2844 / 2845), the browser 390.098 / 390.293 / 390.389; physics exact on all 2601 ticks.
> - **Found, not fixed:** the #163 cabin camera is further back than the PS2's (s3100..3175); no weather snowfall under the NIS; the WebKit across-switch draw is darker than Chrome's (before and after this change).
> - **Tests:** `test-presentation.mjs` (plane FX logic and the core's spray words, painter regions), `test-downloads.mjs` (prefetch, one-buffer read).

> **Round-2 gameplay leftovers: the crossing's game-tick restart, the streamed collectibles' magnets, the Throne rails, the countdown GO!, fr-d-glide, the manual-start pose (2026-09-26, round-2 agent):** see [peak3.md](peak3.md) section 6 "Past the crash contacts" items 5-8 and [ctm-parity.md](ctm-parity.md) "Collectibles and Big Challenges". Not deployed. web/runtime rebuilt with web/build-core.sh (13:35; byte-identical to the core that passed the full suite: the tree of 12:34 plus this work, which also carries other agents' core edits that were in the tree but not in the 10:27 runtime: web/presentation_core.inc, the Fog records in environment_bridge.cpp, stage_world.inc's kept cutscene entities).
> - **Game tick 1298C8 (apr 11947):** it is the rider manager's (cAI) +8. A time challenge's Unload runs world state 10 in the background (22DF50 -> 231278); its 235080 waits for the destination's NIS script read, reloads the rider list and calls 128A10 -> 1297C8(+8 = 0), 9..24 ticks after the Unload (disc time). Found with the new `tools/ps2_entry_probe.py` (ra of 1297C8 on `apr-full.tick17458.ws11`) and `tools/ps2_poll.py`. Core `browser_game_tick_restart` (motionTick + the section scan's game tick); the comparer drives it from the record's tick field (web/peak-capture.mjs), the page from a measured-delay model (web/peak_world.inc; not in the Jam / free ride). allpeak/apr-start: physics exact to the end, score through 12471 (12472 is the E station split, which only the page posts).
> - **Magnets (p2r 1841, apj 5943):** the page's peak runs (world MOUNTAIN) already built the collectibles' MagnetModifiers; the comparer's seeds MOUNTAIN2 / MOUNTAINJ ran neither the sections nor the stage world because their names did not match the exports' "MOUNTAIN" (web/streamed_world.hpp `browser_same_world`). p2r-start / apj-start: score exact to the end.
> - **The Throne rails:** the-throne-tuck 4057 was a tie between two tree rails sharing an end point: 0x334680 walks the segments in octree order (later insertions first in a cell), now `originalRailWalkOrder` (engine/rail_motion.hpp, cached in web/rail_bridge.cpp). 4903 and fr-throne-unload 15035 were 1057B8's landing on a scenery top, whose 10E910 / 119E38 / control 13 -> 5 / clip 268 were stubs: now wired (web/instance_contact_gameplay.inc, animation_bridge.cpp `surface_landing_*`: control 5's 134CB0 exit bake, 133128 entry, the pose pivot for the next bake). the-throne-tuck 4056 -> 6104 (score too), fr-throne-unload 15035 -> 15246, peak2/cra3-race-ai's first computer rider 3291 -> 3443. Open: the-throne-tuck 6105 lands a tick early (its lift target +0x2D0 is -1e-6 instead of 0 after the 6077 reset placement), fr-throne-unload 15247 a soft collision (control 3).
> - **GO!:** `web/game-tick.js` present() sends `trickHud.command(6)` on the tick the race phase goes 4 -> 5 (234C68 -> HUD command 6), before the HUD update; rolling starts have none. No main.js change.
> - **fr-d-glide 3142:** already exact on the 10:27 core (117C28 now runs while a crossing has no path bank); gates fr-d-glide and weather/frd-regions tightened to the end.
> - **Manual-start pose:** checked, no difference: PS2 `dizzy` sits from the first frame (control 6, start phase 2, semantic 1 = the 2.33 s sitting loop, bones repeating every 140 ticks); the port plays the same.
> - **Tests:** test-ps2-captures thresholds (apr, p2r, apj, the-throne-tuck, fr-throne-unload, fr-d-glide, frd-regions, cra3-race-ai), test-mountain-world 2b (the restart model), compare-ps2-capture `gameTick`. Full npm test on that core (a mirror of the tree with it as web/runtime, 13:25): 158/160, the known test-slopestyle-bigair (R&B griff score 576) and test-edge-worker (passed on rerun: deploy/edge-worker.js changed during the run). Capture gates: every case at or past its old threshold, no regression.

> **Simulation worker removed (2026-09-26, sim-removal agent):** the user scrapped it: "we can't increase memory and input delay for marginal gains". It gave 1.37x on the 4x phone model for +167 MB and +1 frame of input latency, and was never on by default. See [workers.md](workers.md) "Simulation worker: scrapped". Not deployed.
> - **Removed:**
>   - the modules `web/sim-worker.js`, `sim-server.js`, `sim-host.js`, `sim-record.js`, `sim-race.js`, `sim-trace.mjs`, `sim-capture.js`, `sim-shadow.js`, `sim-game.js`, `game-sim.js` and `ai-race-sim.js`;
>   - the harness `sim-worker-test.html` / `.js`, and `test-sim-worker.mjs` / `test-sim-game.mjs` with their test:all entries;
>   - the flags and hooks: `?simworker=`, quality `simWorker`, the diagnostics `sim` heartbeat field (`diagnosticsContext`), the `sim-*` events, `?perf=1` `__perfFrames`, `gamePhase`, `?simslow=`;
>   - main.js's hand-off, shadow and pipelining wiring (the frame loop, startRun, stopRun and course load are back to their stage-A code);
>   - `ai-racers.js` attach, `exportState` and `importState`; `ai-race.js` simState, presentTick and simReturn. `ai-race.js` is its code from before the split, with the race state inline again;
>   - the handshake's core-file check: worker-guard `core` / `quiet`, the child's hello `info`, the plugin's `sim` entry and its `new URL()` graph following.
> - **Kept:**
>   - worker-guard, build-id, build-check and the four guarded workers with their main-thread fallbacks (`?workers=0`, `?localWorkers=`);
>   - `web/game-tick.js`, now `createGameTick(host, {trace})` with one host;
>   - `?simtrace=1`, `__simTrace` and `__memoryHash`.
> - **Verified:**
>   - `?simtrace=1` 3000-tick traces are identical before and after on Snow Jam (tuck and scripted), Metro City, Ruthless and a Peak 1 free ride.
>   - A whole Snow Jam race with a restart is identical to the results screen: 14,471 ticks and the same rows.
>   - Whole-memory hashes are equal on Metro City and Ruthless. On Snow Jam tuck the after code reproduced the before hash in 2 of 7 runs; the rest is run-to-run async noise, also seen between two before runs.
>   - Full npm test: 159/160, only the known test-slopestyle-bigair griff capture fails.
>   - A production build has only the four workers.

> **Weather leftovers: fade painter resets, streamed Weather records by track, the peak worlds' flag manager on the visual stream, EBC3 wind and Kick Doubt's area on PS2 captures, the rolling-start camera (2026-09-26, weather agent):** see [weather.md](weather.md) sections 2, 8-12. Not deployed; web/runtime rebuilt with web/build-core.sh (10:27, after all 221 capture gates passed on a private core).
> - **Region resets:** 0x2C03E8's other callers. **Fades:** the fade render 0x2E47E8 resets every painter on each drawn frame at opacity >= 0.93; the rider reset fade (out 0.5 / in 0.5 on the reset progress) = ticks 19-21 of a reset (`web/weather.inc weather_fade_render` after the camera update; `web/cutscenes.js` calls `weather_fade_reset` for the page's NIS / white fades). PS2 era5-reset: both painters -99999 on exactly the port's records at all 4 resets. **"World load" 26DBF0** is the replay's snapshot restore (26F8A0 / 26F980 / 270478) plus an unreachable Big Challenge checkpoint (op 5 posted only by the uncalled 30B590); the port has no replay, and a real world load builds fresh wrappers (-99999, already the port's default).
> - **Streamed Weather records:** 2C0778 uses the record of gp+0x770 (set by the human's 2ED490 after its own step from rider+0x430). `weather_location(track, json)` fed by `web/peak-world.js` with each location's collision data; the core switches on the PS2's tick (PS2 frd-regions: D -> D_DRA4 at 2842, the rider painter exact on all 2601 ticks). The painters' point is now rider+0x460 as the PS2 keeps it (crash contacts, placements, the grid contact).
> - **Flag manager (peak worlds):** `init_streamed_flags` (`web/peak-set-pieces.js`): grid builds + 1 s wind on the one visual stream, mode = course table 0x43D950[0x535C08] each update; the JS cloth consumes the core's words (Math.random stand-in gone). PS2: timer exact every tick, all 43 wind draws are mode 2 words of their tick.
> - **The Throne wind:** `weather/ebc3-wind-rail2` (autopilot line): human exact on all 7999 ticks through 324 pushes in the 14.5 km/h area. **Kick Doubt:** `weather/ess3-weather` / `ess3-lightning` (time limit poked to 4:00): physics exact through the finish; PS2 strikes 8266 / 8581 / 12491, the port reproduces 8581 and 12491 with the synced stream.
> - **Rolling-start camera** (found by a camera investigation agent): the Continue's 0x11D390 places the rider twice before tick 0 (two DEFAULT_3 set-targets + the placement animation). `start_event` now runs `place_rider_region` at the course row (`tools/generate_event_seed.py rollingPlacement`) and `web/core.cpp` the two set-targets: the dbc2-weather camera eye / painter / splash speed exact on every tick; bones of the-throne-tuck through 699, peak1-race-start to the end (gates tightened). Split screen: none in the port (Multi Play greyed), so camera 1's layers stay unported.
> - **Tests:** test-weather 4-6, test-course-spawn (DBC2 / EBC3 added), five new capture gates, `compare-ps2-capture.mjs --weather / --time-limit`. `npm test -- ps2-captures course-spawn weather` 3/3 on the 10:27 core. test-long-runs slot 5/1/2 fails on the 10:27 core from the crash patch (bisected: tick 7843 crash-body position; the 09:56 core passes), reported to the coordinator. Open (weather.md 10): dbc2-weather 1657 splash speed, the countdown-event camera painter, the camera after a mid-run reset placement (ebc3 4157), Fog / Lighting / ScreenTint region hooks still asynchronous.

> **Presentation leftovers vs PS2 frames (2026-09-26, presentation agent):** see [presentation.md](presentation.md). Not deployed. Every item is behind a switch in `web/pv-flags.js` (`?pv=0` all off, `?pv=a,-b` the defaults with a on, b off); on by default after PS2 + Chrome + WebKit comparison, except `flyover`.
> - **plane** (new career): the kind-7 stage calls run ABC1 program 5's LiveComp on a new cutscene set `CUTSCENES/SETS/ABC1PLANE` (`tools/export_cutscene_sets.py --plane`, **new asset**; `web/cutscene-stage-sets.js`): the tilt-rotor flies in and hovers like PS2 new-career s2400..2925; the world's static copy is hidden meanwhile.
> - **heli**: Transport > another peak / post-event Transport to another world plays the departure + in-air ride, then the course switch runs under the held `heli_inair_<char>` loop, which draws itself (`cutscenes.js acrossSwitch`, main.js host `render`) with "Loading..." (`web/ctm-transport.js`, `career-ui.js goWorld` / `enterWorld`).
> - **flyover** (off): the event package downloads under the fly-over (`ctm-event.js prefetchEvent`): throttled Metro-City hold 23.7 -> 18.2 s; warm-cache hold (~5.5 s: course load 3.2 s, warm-up 1.6 s) unchanged — removing it needs the event loaded while the fly-over renders.
> - **boostLight**: RFX+0xD30 (1218D0 clear, 2EADD0 adds (0,0,1) x (2,2,2) while rider+0x2EC > 0) = shade_rider_lighting's extra, unchanged units (the 1220D8 oracle writes that layout); PS2 gp+0x1630 A/B gain +19.7/+14.7/+16.9 vs browser +18.5/+14.1/+16.0. Core: `boost_gameplay.inc rider_controller_lights`, QA `set_trick_fx_force` (2 exports in build-core.sh). Computer riders too.
> - **aiFx**: computer riders draw their own streamers / aura (`opponent-fx.js`, `boost-renderer.js createRiderFx*`), matching PS2 forced frames of the Happiness rival race.
> - **worldWrap**: static-model CLAMP_1 bits exported per batch (`world-batches.py triangle_wrap`, `prepare.py`, `export_peak_world.py --batches-only`); ABA1 / CHP2 / CRA3 and their PEAK copies re-split (triangles verified identical); `world-material.js staticModelTexture`.
> - **rivalIcon**: the rival's "!" in GS byte space (`rival-beam.js`: r, g, b x 255, a x 128, modulate + blend in the encoded pass): orange with a dark outline like the PS2, not a pale see-through triangle.
> - **skyClear**: the PS2 clears to black (382AF0, renderer+6AE0 = 0); main.js sky scene. **cheat**: one dimming shape (back_com): Enter Cheat 12.7 -> 5.9. **help**: Map HelpText's own 375 / 50 % box: every PS2 wrap.
> - **Tests:** `web/test-presentation.mjs` (npm test). Full npm test 159/162: test-slopestyle-bigair (known), test-mp (lobby timeout under load, passes alone), test-long-runs slot 5 pattern 1 (core-only; the live core of 09:56 is another agent's build).

> **Whole-mountain runs past their crash contacts (2026-09-26, crash-contact agent):** see [peak3.md](peak3.md) section 6 "Past the crash contacts" and [obstacle-collision.md](obstacle-collision.md). Core change built into the 10:27 live core (not deployed); full npm test 159/162: test-slopestyle-bigair fails at tick 576 as before, test-mp timed out and passes on rerun, and test-long-runs passes with its cap raised to 60*60*13 (the octree order changes the chaotic scripted run 5/1/2, which now finishes at 42421).
> - **Instance walk order:** `collidableInstances()` follows the PS2 octree (cell of the box +0x60/+0x6C, node list before children, later insertions first). 104E70's normal sum is order-dependent.
> - **LiveComp-animated collision:** a core LiveComp moves its instance's collision (334888 on vt+0xE4 nodes): `stage_livecomp_collision()` in web/stage_world.inc, plus the rigid predicate and contact velocity in web/roller_gameplay.inc. Fixes EBC3's falling path and ABC1's tree bumps.
> - **13AA48 after a touchdown** keeps the +0x180 filter and its own response (web/animation_bridge.cpp, web/core.cpp classify_body_contact motion argument).
> - **Score tick without a path bank:** 117C28 runs every tick even between a crossing's eviction and the next path bank (web/animation_bridge.cpp, web/npc_gameplay.inc).
> - **Gates** (measured on a private core with every capture gate green, no regressions):
>   - allpeak/apr-start 784 -> 11946 (score 811 -> 11946).
>   - allpeak/p2r-start 1261 -> end (score 1840).
>   - allpeak/apj-start 2660 -> end (score 5942).
>   - peak3/the-throne-tuck 696 -> 4056; peak3/fr-throne-unload 14468 -> 15035 (its command words differ at 15036).
>   - peak3/gravitude-race-ai: physics, bones and score to the end; its six-rider human 1505.
>   - bc/bc-race-idle: every rider to the end.
>   - peak2/dra4-race-ai: computer riders 3156 / 3082 / 3343, RNG 2438.
> - **Tests:** new gate thresholds in web/test-ps2-captures.mjs. The comparer now loads the streamed world's stage world by default in web/peak-capture.mjs (`PEAK_STAGE=0` leaves it out); apj needs it.
> - **Open:**
>   - apr 11947: the game-info tick (1298C8) restarts during every location crossing, 9..24 ticks after the Unload. 13C7A8's landing speed scale then reads 64 ticks and gives 0.94 where the browser gives 1. The restart's writer was not found.
>   - p2r 1841 / apj 5943 (score only): the streamed collectibles get no MagnetModifier.
>   - the-throne-tuck 4057 and fr-throne-unload 15037 leave the PS2 on rails.

> **Conquer the Mountain leftovers: free-ride trick cash, records, the station seeds, the Big Challenge start / success, the Session reset event (2026-09-26, CTM-leftovers agent):** see [ctm-parity.md](ctm-parity.md) "Collectibles and Big Challenges" and [peak3.md](peak3.md) section 6 "Remaining". web/runtime rebuilt; `UI/trick-hud.json` regenerated (additive: `sprites.go`, `strings.missionSuccess`; tools/probe_trick_hud.py).
> - **Free-ride cash:** CTM free ride (0x535C10 == 4, 0x535C11 == 0) pays tricks / point pickups / combos min(points / 500, 20) through 119EF8 (popups 0x2F / 0x2E / 0x30, HUD slot 0x19 = the cash). The core queue `score_career_events` feeds `web/career.js earnCash`, which is 159818: an earnings goal reached completes at once (award 14 + peak - 1 and the pass). Collectible and Big Challenge cash use it too. The fr-d-glide score gate went from 2370 to 3141.
> - **Records:** the player's entries are named "PLAYER 1", and every heat enters the top 5. A new record opens Top 5 Record Times first in every event: new ARMSX2 runs `local/ps2-capture/ctm-left/runs/final-top` and `qual-top` show it in the Snow Jam final and qualifier.
> - **Stations:** the five `os609_full_version_depart` models take a free-ride seed (PS2 audits: static, no contact program). The mountain route now has no unsupported dynamic instance (test-mountain-world).
> - **Big Challenge start:** the rider used to be teleported to the world origin, because the `bcteleport` markers' type-0 matrices were dropped (web/world_bridge.cpp). Now the start has the camera cut (15CCF0 set-target, core `browser_camera_set_targets`) and "GO!" (big message 8 from score slot 0x1A, web/trick-hud.js). In the browser the camera matches PS2 `dizzy` s200 / s400 to 2-3 cm.
> - **Success:** the badge (with the green completed disk) stays under the collectible counter while MISSION SUCCESS (big message 0x3A) shows.
> - **Session:** world state 15 (Session / a transport to here) runs 30B7F8 and posts WScript kind 5 (`mission_world_session`). The other caller ("Quit the event?") is unreachable in retail.
> - **Tests:** new `web/test-ctm-left.mjs`; extended `test-big-challenges`, `test-mountain-world` and `test-ps2-captures` (fr-d-glide). Full capture gates 219/219 on the new core.
> - **Open:** the race countdown's own GO! (HUD command 6 at 234C68) is not drawn: `trickHud.command(6)` exists but main.js does not call it. The sit-down idle at a manual start is unchecked. fr-d-glide score +0x168 is one tick short after the course switch.

> **Safari texture regression fixed (2026-09-26, shading agent):** Metro City in Safari had black snow and gates, silver/pink rider outfits and no crowd since the texture-archive deploy (2026-09-25). WebKit uploads the archives' indexed PNGs to WebGPU wrong (revoked blob-URL `<img>`: palette indices as grey; decoded `<img>` / ImageBitmap: premultiplied where alpha < 255); Chrome passes the bytes through. Not the fog/glare `.toVar()` change. Archive entries are now decoded by the game (`web/png-texels.js` -> DataTexture in `web/texture-archive.js`; FE preview worker; crowd frames). See [asset-formats.md](asset-formats.md) "Texture archives".
> - **Verified** in the macOS system WebKit (`web/webkit-driver.mjs`: WKWebView over stdin, because safaridriver session creation times out on this Mac): 30 probe textures exact on the GPU (the browser decoder got 19 / 11 / 11 wrong); Metro City, Snow Jam, Peak 1 look as in Chrome; Metro City race 1842x960 CSS (buffer 1593x896, dpr 2) p50 17 / p90 18 / p99 32 ms. Gate: `test-texture-archive-decode` (all 8,786 entries + Chrome and WebKit GPU readback). Real Safari.app and a real iPhone not run.
> **Simulation worker switchover, stages A-E (2026-09-26, sim-worker agent, piece 3 of 3):** **Scrapped (2026-09-26): all but stage A was removed; see the removal entry at the top.** see [workers.md](workers.md) "Simulation worker, piece 3". **Off by default everywhere** (`?simworker=1`, `?simworker=main`, quality setting `simWorker`).
> - **A:** `web/game-tick.js` (simTick split into `simulate` / `present`, `advance`, `?simtrace=1` per-tick traces, `gamePhase`). Deployed as stage A.
> - **B/C/D:** a run starts on the page as always; at the end of `startRun` a byte-exact copy of the core's memory and the race's JS state (`ai-racers.js` attach / `exportState`, `web/ai-race-sim.js`) go to `web/sim-game.js` (in `web/sim-worker.js`, or on the main thread), which runs the ticks and captures every dynamic export the page reads (`web/sim-capture.js` record: per-tick npc / tick sections + a frame section); the page's core answers them from the record (`web/sim-shadow.js`), so renderers / HUD / audio / `present()` are unchanged; the run comes back at its end, a quit, a restart. Pipelined (+1 frame). A worker lost mid-run: replayed exactly on the page's core under the load screen, then main thread. Streamed worlds and online races stay on the main thread.
> - **Exact:** traces identical in every mode (Snow Jam tuck / scripted, Metro City, Ruthless, R&B, Superpipe; whole events with restart / Give Up; a worker killed at 1000); Metro City / Ruthless screenshots 0 pixels. `web/test-sim-game.mjs` in npm test.
> - **E:** desktop Chrome / Firefox / WebKit: 60 fps both ways (1-3 ms less main-thread work, +1 frame latency). Chrome phone model 4x: 18.1-20.3 -> 25.2-26.2 fps (1.37x, p95 70 -> 45 ms); 2x: 26-27 -> 30 (gate cap). Memory +167 MB (the worker's core copy). Not the 1.4x bar and a real memory cost: off until an iPhone says otherwise; `frames` diagnostics carry `sim` for the comparison.
> - **Harness pitfall:** relationship tables persist in local/session storage: compare runs with fresh profiles.

> **All Peak Race / All Peak Jam and the Peak 2 Race on one whole-mountain world (2026-09-26, All Peak agent):** see [peak3.md](peak3.md) section 6. Not deployed: needs an **asset deploy** (new `web/public/assets/MOUNTAIN/`, `AUDIO/world/MOUNTAIN.json`, the regenerated `UI/trick-hud.json`) and the rebuilt core (`web/runtime`).
> - **PS2 ground truth** (ARMSX2, derived states only; `local/ps2-capture/allpeak/`, `runs/allpeak/`): closed-loop autopilot captures (`tools/ps2_autopilot.py`) of a whole All Peak Race (125,001 ticks: The Throne -> EBC3_E -> E -> ERA5 -> **ERA5_C -> C** -> CRA3 -> D -> DRA4 -> **DRA4_A -> A** -> ARA1; splits at every station's Load; 30:00 time up in Snow Jam), a whole Peak 2 Race (69,004 ticks to Metro-City, 19:00 time up), the All Peak Jam to its first points split, and finishes of both races at Metro-City from kept states with the race clock pulled back: FINISH! -> Top 5 Record Times ("you've got a top time!") -> Rewards ("All Peak Race complete!", cash, goals) -> "Rival Challenge / Event Results" -> Transport. Every Unload / Load, row state, read and split is tabled in peak3.md. Single Event never lists the peak runs (locks cleared, medals and award bits set: still absent).
> - **World** `MOUNTAIN` (`tools/export_mountain_world.py` from the per-peak exports): 43 locations, 49 streaming rows (PS2-measured reads), 22 residency rows, 57 trigger-volume request rows from the stage seed (ERA5_C -> 19, DRA4_A -> 17), per-location environment slices, merged set pieces / sections / glows / world audio, stage tables `--mountain`, glide seeds MOUNTAIN / MOUNTAIN2 / MOUNTAINJ. Streams like the peak worlds but fetches only the current course's row and the next two on the run's route.
> - **Wiring:** `peakRunWorld(7 / 8 / 11)` = MOUNTAIN (the Peak 2 Race no longer falls into the void at DRA4_A); split HUD 0x2A / 0x2B via the core's score object and the trick HUD; the Jams' "GOAL: %d"; the PS2 peak-run results flow in career-ui.js (records first on a top time, rewards, "Rival Challenge / Event Results", Transport / Restart / Quit, then Select Peak on the finish course's peak); peak runs hidden in Single Event.
> - **Core fixes for every streamed world:** dynamic entities (flag 0x40000000) now take their course event's scripted seed (they were all unsupported, and a crash flight / crash body query / reset placement meeting one stopped the core: the browser froze ~6 min into the All Peak Race at Gravitude's billboard); a crash leaves any remaining unsupported instance out instead of throwing; a reset between a path bank's eviction and the next bank re-attaches to the evicted bank instead of throwing; streamed terrain / body appends grow geometrically.
> - **Browser (Chrome, 60 Hz, from Conquer the Mountain):** whole All Peak Race (card, 4 splits, both peak boundaries, time up, Event Results) and Peak 2 Race (DRA4_A -> A; finish with Top 5 / Rewards / Event Results), All Peak Jam start: p95 <= 18.7 ms in every 5 s window; 2-3 frames of 48-83 ms per run when a station first draws (none at the peak-boundary Unloads); two 116-217 ms frames at the run start (PEAK3 the same); core memory 153.6 MB at The Throne -> 265.5 MB in Snow Jam; JS heap ~450 MB median, 735 MB peak.
> - **Gates (npm test):** `test-mountain-world` (manifest, both boundaries' request rows and crossings, 2550 probes = the per-course packages, the PS2 streaming of both captures row by row), `test-ps2-captures` allpeak/apr-start exact through 784, p2r-start 1261, apj-start 2660 (all three stop at crash contacts), `test-trick-hud` split cases. npm test (04:31, the 154 commands without headless Chrome): 153 pass; test-slopestyle-bigair fails as in the 02:05 full run (R&B lineup capture, score at 576). The four headless-Chrome tests (rival-page, shader-budget, fe-texture, gpu-recovery) hang since ~04:00 because requestAnimationFrame never fires in headless Chrome on this machine, even on a blank page; they passed in the 02:05 full run.
> - **Open:** the three crash-contact divergences; the run start's long frames and the station-draw frames (three.js node builds on first draw); the records' player name ("PLAYER 1" on the PS2) and whether a top time opens the records first in the standard events too (CTM flow); the stations' five `os609` dynamic models have no seed (a reset placement there would still throw); the All Peak Jam not captured past its first split; Safari not measured.

> **Simulation worker, the worker side (2026-09-26, sim-worker agent, piece 2 of 3):** **Scrapped (2026-09-26): removed; see the removal entry at the top.** see [workers.md](workers.md) "Simulation worker". New modules only (main.js untouched; nothing on by default).
> - **Modules:** `web/sim-race.js` / `sim-server.js` / `sim-worker.js` / `sim-host.js` / `sim-record.js`. The six-rider race runs in a module worker from the page's compiled core module, with frames in and per-tick traces plus a transferable frame record out: 0.37-0.49 MB mean, a pool of three. A record-backed core has the same export API. A worker that dies mid-race is replayed exactly on the main thread.
> - **Exact:** worker = main thread = node (per-tick traces + whole-memory hashes) in Chrome 149, Firefox 156 and iOS 26.5 Safari (Simulator): tuck, scripted pad and fast mode, up to 1800 ticks. macOS Safari was not run (the screen was locked).
> - **Cost:** the worker is +3-5% per tick (Chrome, Firefox), +8-20% (WebKit, Simulator). DevTools cannot throttle workers. Phone frame model: 16.4 -> 27.2 fps (1.66x).
> - **Next:** the staged main.js integration plan (A: simTick extraction, no behaviour change; B: record reads; C: commands and loading; D: worker switch; E: measure, default on for phones) is in workers.md: about 6.5-8 days, to run while main.js is quiet.
> - **Tests:** `web/test-sim-worker.mjs` and `sim-worker-test.html` + `sim-trace.mjs` (harness).

> **Stale-safe workers (2026-09-26, sim-worker agent, piece 1 of 3):** see [workers.md](workers.md). Deployed 2026-09-26, with the edge worker (wrangler version c44befe7).
> - **Guard:** `web/worker-guard.js` + `worker-guard-child.js`. Every worker (peak-world, terrain, fe-preview, audio-decode) starts from its content-hashed URL (`workerUrl()`) and sends a hello with its build (`web/build-id.js`, filled in by `web/vite-build-id.js`; on the dev server, a hash of the worker's own module graph). On another build: terminate, one refetch with `?v=`, then the same handler on the main thread. A start failure (404, HTML, script error, no hello in 15 s) takes the same path. At run time: error / messageerror / hung (ping) -> one restart with the requests in flight replayed, then the main thread. A back/forward restore pings (5 s). Diagnostics `worker-*`. `?workers=0` forces the main thread; `ssxWorkers()` lists them. Before this, a missing peak-world / fe-preview worker hung Peak 1 / the preview forever.
> - **Deploy check:** `web/build-check.js`. `/build.json` (emitted per build, `no-store`) -> `build-stale`; the page reloads once into the new build on the title screen. Any service worker on the origin is removed (the game never had one).
> - **Headers:** mp-server `hashed` = exactly Vite's `/assets/<name>-<8>.js|css|wasm`, and `/build.json` is `no-store`. The edge worker takes `immutable` from the origin's header, no longer from its own looser regex (a 404 is never immutable).
> - **Verified:**
>   - Chrome 149, Firefox 156, Safari 27 and iOS 26.5 (Simulator): same build, stale once -> refetch -> worker, stale twice -> main thread, 404 / HTML -> main thread (`web/worker-guard-test.html`, QA server in the scratchpad `qa-workers/`).
>   - The built game: Snow Jam and Peak 1 run with every worker file 404 or stale.
>   - New deploy -> exactly one reload on the title; a controlling service worker removed; dev-server edit of a worker file -> that worker only falls back.
>   - `web/test-worker-guard.mjs`, test-edge-worker and test-mp-gate extended. Full npm test 156/157: only the known test-slopestyle-bigair griff capture.

> **First load: the title from the first paint, a load meter, front-end textures vs the PS2 (2026-09-26, first-load agent):** see [first-load.md](first-load.md). Not deployed by this agent.
> - **Title / first load:** index.html carries an inline boot script (web/boot-plugin.js bundles web/boot-screen.js + boot-progress.js + title-screen.js + lui-player.js; vite.config.js) that paints the original FE.LUI 06title (+ snow loop) while the page parses: first paint 0.07-0.4 s instead of black for 1.2-7.5 s. The Select Character "Rider ranking" meter + percentage + phase line ("Loading Snow Jam...") show honest progress: files weighted by bytes on the wire (web/boot-files.json, sizes inlined at build) + a round trip each, compute steps by typical time; never backwards, no 98-99% wait. main.js reports steps and hands over (`ssxBoot.handoff()`); ui.js draws the title through `ssxBoot.draw`. The core compiles while the front end loads; ui.load() loads side by side. downloads.js `onDownloadBytes`. The in-game load screen is untouched.
> - **Front end vs PS2** (new ARMSX2 cold-boot captures in local/ps2-capture/menus/fe-texture): LUI draw order tie-break by top-level group layer (Main Menu panel over the mountain), FEFONT glyph aspect (x1.17 taller) and colour (x0.8), greyed rows alpha 128, Select Peak/Mode/Event right-hand white ramp, the title's snow-burst transition, disabled keyboard arrows. Tests: web/test-boot-progress.mjs, web/test-fe-texture.mjs (in npm test).
> - **Edge:** server/mp-server.mjs gives edge copies `stale-while-revalidate=604800` (a repeat visit made ~46 tunnel round trips one after another when the edge copies were stale: +4.6 s at 100 ms each). Asset deploys must now bump `CACHE_GEN` in deploy/edge-worker.js (docs/hosting.md). Next levers (not done): "Press START" before the course and the computer riders load (docs/first-load.md).

> **Sky line / seams fixed: the area sky textures clamp like the PS2 (2026-09-26, sky agent):** see [terrain-render-fidelity.md](terrain-render-fidelity.md) "Texture addressing". Not deployed.
> - **Cause (long-standing, every course):** the browser sampled every ASKY..ESKY texture with Repeat (main.js asset() default). The mountain ring's top rows are transparent and its bottom rows opaque, so the bilinear filter drew a half-transparent line along the ring's bottom (and top) edge, a dark line where the band below starts, and vertical seams every 45 degrees. The PS2 draws them with GS CLAMP_1 = clamp/clamp (material flags bits 3/4 -> 37F2BC..37F354 -> 363C20 -> 3625C0; the sky push 353B10 sets the same).
> - **Fix:** `web/world-material.js` `originalModelWrap(flags)`; `createOriginalSkyMaterials` sets each sky texture's wrapS/wrapT from its material flags. Headless before/after on ARA1, BRA2, CRA3, DRA4, ERA5, PEAK2 and PEAK3 free ride: only the sky seams change. Remaining: a faint dash at the right end of each ring segment's bottom (authored bottom-row alpha 230..236 over the last 17 columns) shows the clear colour, which the port takes from the fog colour; the PS2 clear colour is unverified.
> - **Test:** `web/test-sky-addressing.mjs` (npm test, <1 s): all five skies clamp/clamp after the materials are built from the Repeat default; every edge sample equals its edge texels; fails on the old code.

> **Rival computer rider set up like the PS2 in the real game (2026-09-26, rival-setup agent):** see [backcountry.md](backcountry.md) "Rival computer rider". Not deployed.
> - **Cause:** the page's rival events (Happiness / Ruthless / The Throne, race and jam) raced from the core seed's game RNG (a glide state's, 436..928 draws) instead of the ready state's (seed 0 + 4 or 5 load draws); the capture gate seeds the capture's RNG itself. Ruthless: Nate left the PS2 at tick 132, never landed in the 1413 crash, so no 1656 reset placement / painter re-seed, and the human followed at ~2650 (weather.md 10). Everything else of the rival's set-up (document, bank-2 stats, event kind, pair inputs, relationships) already matched the gate.
> - **Fix:** `tools/export_backcountry.py rivals` exports each document's `anchor_rng` (+ `menu_extra`, `load_draws`, Ruthless `direct_path` evidence); `web/lineup.js rivalAnchorWords`; `web/ai-race.js prepareRival` (and the default document without prepare) sets it through `racers.setAnchorRng` (race tick 0 of every start). PS2: two new derived Ruthless ready states (Zoe 3 / Moby 5 draws on the direct menu path, Nate 4) confirm the lineups' load-draw rule; the reference Zoe states of Peak 2 / 3 carry one menu-path draw more. `web/bc-autopilot.mjs` seeds it too.
> - **Tests:** new `web/test-rival-page.mjs` (npm test, headless Chrome, ~50 s): the real Single Event path with the captures' pads = the gate: Ruthless human 3910 / Nate 6425 / RNG 3932, Happiness all 6700, The Throne all 2900, Happiness Jam 389 / 389 / 401. `test-rival-mode.mjs`: anchor words of every document, 8 capture starts, the rule.
>
> **Field crashes fixed from the players' reports (2026-09-26, crash agent):** see [mobile.md](mobile.md) "Field crashes and diagnostics". Not deployed.
> - **iPhone WebGPU "private address space exceeds 8192 bytes" (Metro City, Ruthless, Peak 1):** the fog composite and glare-final WGSL had grown to 11.8 / 12.1 KB of `var<private>` (three.js makes every TSL temporary one, `select()` duplicates its input per branch; the ScreenTint lightning-flash select of 2026-09-25 took it from an estimated ~6.5 KB over the limit: the tint part of the old shader is 166 KB of its 193 KB main body). Stage results now `.toVar()` in `fog-renderer.js` / `glare-pass.js`: 1.3 KB / 0.6 KB, WGSL 234 KB -> 27 KB; Chrome frames pixel-identical; macOS Safari 27 loads Metro City without a pipeline error. `web/wgsl-budget.js` + `test-shader-budget.mjs` (headless Chrome, BRA2 with glare / DBC2 / PEAK1, budget 6144 B).
> - **GPU device loss:** `web/gpu-recovery.js` (wired in main.js init): new device on the same renderer once visible, else reload (2 per 5 min), else the title card "Graphics lost. Reload the page". `test-gpu-recovery.mjs` (same frame after a loss, hidden page waits, no adapter -> reload, clean reload = no false death).
> - **Diagnostics (`web/diagnostics.js`):** the false `previous-session-died` on pause screens (pagehide then the auto-pause rewrote the clean mark) is gone; deaths carry cause / hidden time / memory / last 12 events; course + screen on every event; repeat counting; priority budget; texMB / wasmMB; `pipeline-failed` and `shader-over-budget` with material/object and WGSL sizes. Post pipelines are named `FogComposite` / `GlareFinal`.
> - **Also:** `audio-engine.js` resume/suspend no longer reject unhandled (iOS "Failed to start the audio device"); `downloads.js` retries with backoff + 20 s stall watchdog (`test-downloads.mjs`; another agent's `onDownloadBytes` sits in the same loop); an unknown course at boot falls back to Snow Jam; a network failure says "Connection lost. Reload the page".

> **Gamepads work on Windows and in the menus: every slot, a mapping layer, pad test / remap (2026-09-26, gamepad agent):** core fix reported to the coordinator for deploy.
> - **Causes:** the game read only `navigator.getGamepads()[0]` (main.js inputs(), rumble.js); on Windows the pad is often in slot 1+ (slot 0 empty or held by a headset / wheel driver, Steam Input's virtual pad, vJoy / ViGEm, a pad unplugged earlier). Non-standard (DirectInput / RawInput) pads were read as if standard. The front-end menus had no gamepad input at all (keyboard / touch deck only).
> - **Shared reader `web/gamepad.js`:** the only `getGamepads()` caller. Re-reads all slots every poll (Chromium snapshots vs Firefox live objects), hot-plug, active pad = last used (button edge / axis half travel from its rest; standard pad preferred before any input; mirrored pads do not flap), `onPads` events, per-pad remaps (`ssx3.padmap`, key vendor:product). Used by main.js inputs(), `input-glyphs.js`, `rumble.js` (active pad; Firefox `hapticActuators` fallback), `cutscenes.js` (Cross skip), `audio-engine.js` (unlock), `diagnostics.js` ('gamepads' events: id / vendor:product / mapping / counts / layout / slot, max 8).
> - **Mapping `web/gamepad-map.js`:** 'standard' pads are copied unchanged (PS2 channels bit-identical). Known raw layouts from Chromium `gamepad_standard_mappings_*` / Firefox `GamepadRemapping.cpp`: DS4 / DualSense / SCUF, DS3, Logitech D, Xbox BT via RawInput, Xbox One S BT, 8BitDo BT, HORIPAD, SmartJoy / XGEAR / boom PS2-PSX adapters, DragonRise, SNES USB. Generic fallback: buttons in standard order, hat D-pad on axis 9 (usage-indexed axes, steps of 2/7, neutral 9/7), Rx/Ry triggers resting at -1, right stick Z/Rz. Not fixable: Firefox's Switch Pro over Bluetooth (its remapper drops the axes).
> - **Menus `web/gamepad-menus.js`:** off the race screen the pad sends the touch deck's `MENU_KEYS` (new presses only, D-pad / stick repeat 400/110 ms, Start is not Enter while a run is going); installed from main.js.
> - **Options > Controller Settings > Gamepad (`web/fe-gamepad.js`, hooks in fe-options.js):** a fifth 22control row (its own focus state 55) opens a 19game_opt copy: Gamepad (Left / Right picks a connected pad), Layout, Buttons (Cross = modal pad test), Left / Right stick, Remap buttons ("Press the pad's button for Cross", 5 s skip, saved per pad), Square = reset with the Yes/No box.
> - **Tests / checks:** `web/test-gamepad.mjs` (npm test): slot 1 / 2 with slot 0 empty or another device, Steam mirror, DirectInput generic + known layouts, Firefox hat on a live object, trigger axes, Chromium snapshots, hot-plug, two pads, remap storage and state machine, menu keys, rumble, hints, the 22control / Gamepad screen rows. Headless Chrome with a mocked `getGamepads` (pad in slot 1 behind a headset, standard and DirectInput): title -> event -> race, cutscene skip, Start pause / D-pad in pause / resume, Controller Settings -> Gamepad -> test -> remap -> saved (`local/browser-validation/gamepad/qa.mjs standard|dinput`, `qa2.mjs` for the Gamepad screen; dev server on :5173).

> **Weather: the rider wind push, snowfall / blowing snow, lens snow, lightning (2026-09-25/26, weather agent):** see [weather.md](weather.md). Not deployed; web/runtime rebuilt with web/build-core.sh (2026-09-25 16:00 HST, under the build lock); no core change since.
> - **Wind (gameplay):** 0x125970 pushes the human (rider+0x874) toward the Weather painter's wind (type 12, properties 3/4: km/h, degrees; gate 10 km/h, cap 50) first in the ground motion 13D818 and the air motion 139A20 (0x1250A8 + predictor restart in the air). It was not ported. Now `engine/wind_push.hpp` + `web/core.cpp browser_wind_push`; the rider painter steps every tick next to the Lighting wrapper. Winds: DBC2 40 km/h (the blizzard), EBC3 14.5; ERA5 / EHP3 stay under the gate; Peak 1 none. Painter resets 0x2C03E8 (any rider's reset placement / teleport re-seeds every painter, shared-world event 8; the rolling start too).
> - **PS2:** `peak2/dbc2-race-tuck` human 2073 -> **3910**, Nate 2852 -> 6425, RNG 2200 -> 3932 (3911: a control-3 soft collision crash, open); new `weather/dbc2-weather` capture (watches on the painters, layers, splash): everything exact on all 3299 ticks, 519 pushes. Oracles `tools/test_weather_native.py`: wind push 200k, layers 200k, splash 60k cases, 0 mismatches; tweakables = ELF.
> - **Visuals:** `web/weather.inc` + `web/weather-renderer.js`: the snowfall object's 4 flake + 2 fluff layers (VU1 program 5 reproduced in the vertex shader, checked against the program on 6230 sprites), the camera splash (lens drops / ice crystals, spawns above snowfall 1.5), the ScreenTint lightning (EBA3 Much-2-Much, ESS3: flash in `screen-tint.js`, thunder via `gameAudio.thunder`). `tools/export_weather.py` writes weather.json for the 17 courses (with the ready state) and the 45 streamed-world locations. `?weather=0` turns it off. Frames (weather.md section 8): `local/browser-validation/weather/tri-*.png` = PS2 | browser `?weather=0` (before) | browser (after) for Ruthless 1700/2300/2600/3001, Gravitude, The Throne, Perpendiculous, Much-2-Much lightning, Snow Jam, R&B, Style Mile, Intimidator.
> - **Cost (2026-09-26, section 7):** the blizzard (10.3k flakes, 8 instanced draws, 20.7k triangles): weather draws alone 0.024 ms GPU at 1280x960 (high / medium) and 0.012 ms at 640x448 (low), 2-4 % of the world scene drawn the same way; in the running game weather on / off is within noise on every tier (paired -0.1 +- 0.3 ms). CPU < 0.01 ms. No tier cuts the look. Safari not measured (safaridriver times out). No weather / fog / ScreenTint shader has a private or function-scope array (all 60 DBC2 WGSL modules scanned; the big arrays are uniform instance matrices).
> - **Tests:** `web/test-weather.mjs` (npm test), the two capture gates. Full `npm test` 2026-09-26 (twice; latest run 152/153): only the known `test-slopestyle-bigair` R&B griff capture (slopestyle-bigair.md, fails with `WEATHER=0` too). Open (section 10): ~~the page's DBC2 Nate is not the capture's~~ (fixed: rival-setup entry above); EBC3's push and ESS3's area have no PS2 run; peak-world region swaps are async; 26DBF0 / 2E4678 resets, split-screen layers and the flag manager's own wind are not ported.

> **Conquer the Mountain: collectibles tracked and the Big Challenges playable, as the PS2 (2026-09-26, CTM parity agent):** see [ctm-parity.md](ctm-parity.md) "Collectibles and Big Challenges". Not deployed; web/runtime rebuilt with web/build-core.sh.
> - **PS2:** new runs in `local/ps2-capture/ctm-parity/runs/`. `explore-info` has the Transport INFO views with 41 collectibles and 22 challenges poked. `sd-goal`: Speed Demon as the 12th challenge completes the Freeride goal. `pc75` / `pc75-up` / `dizzy`: offers queued in C+0x1C4, the goal / count / called-trick HUDs and the manual start. The menu-capture hook of the old `menus/ctm` states must be cleaned before a pad script works (`clean_menu_state` logic).
> - **Collectibles:** the Freeride goal was hard-wired false. It is now 157BF0 goal 2, the collectible medal (0x45AFE8) and the Big Challenge medal (0x45B018) of the peak. A collectible or a completed challenge that completes it grants award 11+peak and the pass, with message 249 / 250 (1599A0 / 159B08). The Transport INFO (Peak Goals, All Mountain %, the Freeride goal panel with its next-medal lines, the freeride list) shows the PS2's numbers and check boxes.
> - **Big Challenges:** the WScript trick event (118FF8 -> 309918), builtin 78 (30AF08), builtin 83 (3045B8, the called trick, drawn in red) and builtin 63 are ported, so the trick / called-trick / grind challenges can complete. Other fixes:
>   - The challenge start is the manual start (0x12357C: phase 2, the rider waits for the stick).
>   - A goal challenge hides the cash.
>   - The offer / failure popups are OV.LUI 63bc_start / 90bc_fail, and the challenge pause is the MCOMM menu with its icons.
> - **Files:** web/career.js, career-ui.js, big-challenges.js, ctm-pda.js, free-ride-hud.js, stage-collect.js, mission_gameplay.inc, score_gameplay.inc, animation_bridge.cpp (one call), build-core.sh (2 exports), tools/export_ctm_screens.py (+2 screens), tools/ctm_flow_trace.py; tests web/test-ctm-flow.mjs 12-13, web/test-big-challenges.mjs 1b.
> - Builtin 63 is the grind rail's packed id (`railMotion.railId`, owner +0xD4), for Apply Pressure and Missing Masonry.
> - WScript event kind 5 is posted at the reset controller's placement (12F398 -> 11DF18(rider, 1), for the Peak 3 Combat / New Line challenges).
> - **Remaining:** free-ride trick cash (pointsToCareer, a score-object change), the post-success badge animation, kind 5 from the two other 11DF18 callers, the GO! and the camera cut at a challenge start. Side by sides: `local/ps2-capture/ctm-parity/sbs/sbs-5.png`, `sbs-6.png`.

> **Conquer the Mountain career flow = the PS2 disc (2026-09-25, CTM parity agent):** see [ctm-parity.md](ctm-parity.md). Not deployed.
> - **PS2 ground truth:** 18 ARMSX2 runs through the career (new career, quit / save / title, re-entry, last-lodge start, qualifier -> final with forced places, reward lists, earnings goal -> Peak 2 pass -> "Go to this peak now?" -> the DBC2 arrival, lodge Return / Quit), derived states only, in `local/ps2-capture/ctm-parity/`. New tools: `tools/ctm_flow_capture.py` (+ `ctm_flow_state.py`: world state, NIS lists, career block per snapshot), `tools/ctm_flow_trace.py` (-> `web/ctm-flow-ps2.json`), `tools/export_ctm_screens.py` (OV.LUI PDATemplate / bganim1 / 31paus_freeride / 87yndialog / 98enterlodge -> `UI/ctm-screens.json`, in npm run setup).
> - **Fixed:** CTM Select Character goes straight to the world load (no Setup Character, 0x1A0B00); the new-career flag clears at the first crossing / results (0x236928 / 0x236E10), first-visit cuts key on a visited mask (+0xACC, old saves migrate), the arrival list plays before the ride (the plane scenes were killed by startRun: "anchor 37 unavailable"), live-actor anchors 40+subject (heli_arrb_* rider camera), movies letterboxed with the skip prompt, transports into a visited backcountry play the heli drop, the last lodge follows every station reached; results "Next heat" / "Final Round"; the reward list (62reward_list order) + medal message / hex; Quit -> "Save progress before quitting?" -> the title (MCOMM, results, lodge); lodge Return to Game rides from the station top (no instant prompt); FAQ view "FAQ n" without Delete; the mail icon after event posts; main menu remembers its item. The MCOMM, career pause and their Yes / No popups are the OV.LUI screens now (`web/ctm-pda.js`; every `mcommFrame` user gets the PDA frame).
> - **Files:** web/career-ui.js, character-select.js, career.js (award ids on items), career-messages.js, cutscenes.js, free-ride.js (one call), main.js (course listener, reload option), ui.js (main menu index), lui-player.js (opt-in shape scale), new web/ctm-pda.js, web/test-ctm-flow.mjs (in "test:all").
> - **Remaining:** the midway plane model in #153 / #163, the heli ride shown during a peak change / post-event transport (the page load screen instead), the black hold after the fly-over and no computer riders in it (shared-world work), Transport / Session / lodge / results art still port-drawn inside the PDA frame, the memory-card screens. All Peak Race / Jam unlock and flow recorded in ctm-parity.md (not built).

> **Pickups look and animate like the PS2; Peak 2 trail check (2026-09-25, pickup agent):** see [pickup-recovery.md](pickup-recovery.md#pickup-effects-and-hud-vs-ps2-frames-2026-09-25), [board-trails.md](board-trails.md#peak-2-visibility-check-2026-09-25). Not deployed; web/runtime rebuilt (web/build-core.sh).
> - **PS2 frames:** dense snaps of a BHP1 point pickup, Peak 3 free-ride snowflakes, ARA1 / CRA3 trick boosts (`local/ps2-capture/runs/pickups/`), browser frames at the same ticks (pad replay or record-seeded rider in headless Chrome).
> - **Fixed:** point / multiplier award burst born at the frozen magnet matrix (was the authored instance; PS2 savestates `bhp1-pickup-burst`, test-stage-world); HUD 'Collect +$ n' (0x31), career cash popups 0x2E/0x2F (oracle cases in tools/probe_trick_hud.py, test-trick-hud 385/385), green pulsing collectible counter / cash after a collect, the counter during a career-race collect; the air streamers (RFX+0xAD0, white `strm` under every jump, purple `prbn` with a trick boost) and the trick boost's power-up aura (RFX+0x9C0, `psmr`) in web/boost_gameplay.inc + web/boost-renderer.js (FX textures 61..63 via tools/export_fx_textures.py). New test-pickup-fx.mjs in test:all.
> - **Trails on Peak 2:** no rendering difference; the Ruthless Ridge start ramp (surface 4) has no track on the PS2 either.
> - **Collected snowflakes (2026-09-26, same agent, user report "visually bugged when you pick them up, and then don't disappear"):** see [pickup-recovery.md](pickup-recovery.md#collected-snowflakes-break-pose-and-staying-removed-2026-09-26). (1) The break pieces started from the collectible LiveComp's bind pose (the JS player's spin / bob phase was not in the core): they popped up to ~0.7 m down at the collect; the MeshAnim now rebuilds the section-started player (`stage_section_livecomp_nodes`). (2) A collected snowflake came back when its streamed location unloaded and read again: the award now sets the bit in the core's copy of the career row (as 30C3E0 -> 153B00 does; `test-collect-restream.mjs`). PS2 reference: DBC2 free-ride collects with kept savestates (MeshAnim nodes) and the ASS1 multiplier, sheets in `local/ps2-capture/runs/pickups/compare/`. web/runtime rebuilt.
> - **Gaps:** the trick boost's extra rider light (RFX+0xD30, 392D90; `shade_rider_lighting` already takes extra lights, but the (2,2,2) +Z unit scale is unverified); streamers / aura of computer riders; no PS2 frame of a career-race collect; the break pieces' spin phase against the PS2 is only as exact as the section start (a teleported replay starts it elsewhere).

> **World texture library + per-rider texture archives: 267-299 fewer requests per race load (2026-09-25, texture-library agent):** see [asset-formats.md](asset-formats.md) "World texture library", "Texture archives", "Rider texture archives". Not deployed: needs a **full asset deploy** (new `.tex` archives, rewritten world.json / wardrobe.json, the per-location PNGs are gone).
> - **ID scheme (disc + SLUS_207.72):** one texture table for the whole world stream. BAM.SDB +0x2A = 788 textures, +0x2C = 623 light pages; SSB kind-9 records (track 255) register texture handle = resource id (chunk resolver 0x494FB0 -> 0x3AAF5C "strm_tex" -> renderer 0x37C8C0 -> texture manager 0x367440, slot manager+8+4*handle), kind 10 handle = 788 + id ("strm_lpg", 0x3AAF88). All 6,203 kind-9 records of an id are byte-identical (376 ids in more than one location); light pages are per location. Materials / patches name the id. Matches SSX-Library's shared Textures/ + Lightmaps/ extraction.
> - **Exports:** `tools/texture_archive.py` (`SSXTEX01` + JSON index + indexed-PNG payload), `tools/export_world_textures.py` (library `TEXTURES/world.tex`, 788 textures, 4.91 MB; `--repackage` converted the 80 exported world packages after a texel-for-texel check; GameCube lightmaps per package in `lightmaps.tex`), `tools/export_rider_textures.py` (per rider `WARDROBE/<ID>/textures.tex` = default outfits 0.06-0.4 MB, `gear.tex` = other Equip Gear textures 2.8-11 MB, `icons.tex`; `--single` = one archive). prepare.py / prepare_course_sky / export_peak_world / export_cutscene_sets write library refs; every rider exporter packs at its end; `npm run setup` builds the library. 13,800 PNGs removed (backups: `local/texture-library-backup`, `local/rider-texture-backup`).
> - **Loader:** `web/texture-archive.js` (one fetch per archive through downloads.js, kept as a Blob, entries decoded on demand from Blob slices by the package's old decoder: `<img>`/TextureLoader or createImageBitmap). main.js asset() (starts the library fetch with the vertex data; skips the unused `10-` GameCube lightmaps for original world/sky materials), cutscenes.js sets, opponent-riders.js, fe-preview(.js/-worker), wardrobe.js (Equip Gear packages, icons). Course switches, streamed peaks and cutscenes reuse the library.
> - **Measured** (Chrome for Testing, local mp-server + edge-worker cache headers, first visit; throttled = 40 ms / 50 Mbps): Snow Jam race (+5 computer riders, fixed lineup) 579 -> 297 requests, texture requests 292 -> 8, texture bytes 3.80 -> 5.82 MB, time to race 14.9 -> 14.9 s (throttled 36.6 -> 35.3 s), repeat visit 571 -> 289 revalidations; CRA3 635 -> 336 req (texture 338 -> 13, 4.73 -> 6.21 MB, throttled 43.3 -> 39.2 s); PEAK1 free ride 590 -> 323 (273 -> 3, 2.50 -> 5.39 MB, 39.0 -> 38.1 s). JS heap / GPU texture count unchanged (139.1 -> 139.6 MB, 466 -> 469). Select Character through 10 riders 149 -> 82 requests (74 -> 7 texture); online race (both riders in gear) 558 -> 308 requests, texture 1.06 -> 10.1 MB (the two gear archives). One archive per rider with everything (`--single`) cost a Snow Jam first visit 39 MB of textures and +7 s throttled, hence the split. Safari: not measured, safaridriver cannot open a session ("timed out while connecting to a Safari instance"); `local/texture-archives-qa/safari-measure.mjs` is ready.
> - **Identical rendering:** deterministic frames (ssxQA start/advance) 0 differing pixels before/after: Snow Jam t60/400/1200, CRA3 t60/600, `?originalWorld=0` t400, Snow Jam with computer riders t30/300, gear outfits Mac/Moby/Kaori t60/400; PEAK1 differs only in the board-trail region that varies run to run with the same code. GPU readback: all 777 used world textures, 118 lightmaps, 4,276 rider textures and 2,862 icons byte-identical (WebGPU + WebGL2 / `<img>` + createImageBitmap). Scripts, results, screenshots: `local/texture-archives-qa/`. New test `web/test-texture-archive.mjs` (in test:all); test-opponent-riders / test-fe-preview read `{pack, id}` entries. Full npm test: 142/144 ok; test-slopestyle-bigair (known baseline) and test-mp (lobby timeout under load; passes on its own).
> - **Open:** Safari numbers; world textures still decode through `<img>` (createImageBitmap would decode off the main thread; texels already shown identical); 11 library ids unused by any exported package; the lightmaps.tex files (22 MB) only serve `?originalWorld=0`.

> **Firefox load: race in 16.5-25 s instead of 46-84 s, no frozen frames (2026-09-25, Firefox load agent):** see [firefox-load.md](firefox-load.md). Not deployed.
> - **Cause:** Firefox's WebGPU builds every pipeline serially in its GPU process (100-150 ms each with a cold Metal cache). A Snow Jam load made 264-278 pipelines for 56 distinct shaders: three names unnamed buffers by node id, which gives every skinned mesh and every uniformArray its own WGSL; morph attribute ids are in the pipeline key; skin row, snow capacity and morph width were written into the shader text. On top of that, the intro compiled its actors for the canvas instead of the world pass (24-57 pipelines on the first intro frame: the 11-23 s frozen/black frame), and in Firefox the load screen's accelerated sprite canvases waited on GPU readbacks.
> - **Changes:** new `web/shader-keys.js` (stable `NodeBuffer<k>` names, morph ids out of the pipeline key; `?shaderKeys=0` turns it off); `rider-skinning.js` palette row as a uniform; `snow-renderer.js` one instance capacity; `fe-preview.js` `padMorphPart`; `fog-renderer.js` `compileObject` (world-pass target) for the cutscene host; `main.js`/`ui.js` `gpuIdle()` after the warm-up frames (mark `warm:gpu`) and before the intro, with the intro prep overlapping the GPU wait (`ui.warmFramesDone`); the race canvas hidden under the load screen (`style.css`, `loading-screen.js`, world mode excepted); new `web/sprite-canvas.js`: software sprite canvases in Firefox only; `diagnostics.js` `stall` / `gpu-stall` events and `warm:*` marks.
> - **Result:** pipelines 264-278 -> 62-63 (Firefox and Chrome). Firefox first visit: race at 46.4-84.2 s -> 16.5-25.3 s; repeat visit: 33.0-57.9 s -> 15.0-19.0 s. Load screen: no frame over 0.3 s once the warm-up starts (was 16-71 frames over 250 ms, up to 2.8 s). Longest intro frame: 0.25-0.77 s (was 10.7-22.8 s). Chrome unchanged: race at 15.0-15.7 s, warm-up 1.1-2.5 s. The 85 s CRA3 warm-up seen in the Claude app's browser pane comes from the hidden pane throttling requestAnimationFrame; desktop Chrome takes 1.1-1.6 s before and after. Firefox WebGL2 is slower (26-29 s with a 6-8 s freeze), so Firefox stays on WebGPU.
> - **Same pixels:** Chrome A/B of the intro (both start-hut variants pinned), race, HUD and UI text: 0 px differ. Firefox: 0 px, except 35 HUD px at 1/255. Images are in `local/browser-validation/firefox-load/`. The start-hut variant and the flag cloth phases come from Math.random, so unpinned A/B runs can differ there. `npm test`: only the known test-slopestyle-bigair failure.
> - **Open:** Safari was not measured (safaridriver session creation times out even with Remote Automation on); about 60 genuine pipelines remain (3-7 s of hidden GPU work on a cold Firefox load); the cutscene switch frame still builds the output-transform pipeline.

> **CTM free-ride / station music, DJ and the MCOMM routine = PS2 (2026-09-25, audio agent):** see [audio-logic.md](audio-logic.md) 9.11. Not deployed; core rebuilt (web/build-core.sh).
> - **PS2 recordings** (`tools/ps2_music_drive.py` + `ps2_music_log.py`, plans `tools/ps2_music_plans/`, logs `local/ps2-capture/music/runs/`): new career plane drop to Green Base Station, lodge in/out, in-world MCOMM / Messages / Transport map, MCOMM Transport, course and connector MusicTriggers, Single Event start.
> - **Fixed:** the in-world MCOMM / Options started the menu song (charsel) and left it playing in the world (the reported "menu song, 2 s of Go, menu song"); the new career's world load picked a playlist song (Now Playing box + an artist intro for a song that never played) and ran the ride start during the plane cutscene; the lodge left charsel playing after Return to Game (now a world load: hub song, DJ, hub chatter); Transport now stops the song first; first GO sends event 0 (0x627C); Peak_Boss hub chatter was silent.
> - **MCOMM routine:** after the DJ's Text_Message, Green Base Station's "?" homes in (stage builtin 110 + magnet) and its contact (builtin 100) opens the Message Center on FAQ 1 (the homing only until the career has shown it: `faqShown` on the career rider; touching the "?" later opens it again, as program 9 does); core `web/stage_script_gameplay.inc` / `web/peak_world.inc`, `web/free-ride.js` 'faq', main.js, `web/career-messages.js openFaq`.
> - **Test:** `web/test-audio-timeline.mjs` (in `test:all`) vs `web/ps2-audio-timelines.json` (`tools/ps2_music_timeline.py`). Chrome checked; Safari automation was refused here (sandbox). Gaps: 9.11 "Known differences".

> **Continuous testing, agents don't wait for full runs (2026-09-27):** `web/test-loop.mjs` runs the full `npm test` (niced) whenever the tree changed, at most one start per 10 minutes, reruns each failure once on its own (FLAKY if it then passes), and appends one line per run to `web/node_modules/.cache/ssx-tests/loop/events.log` (NEW / STILL / FIXED / FLAKY); `node web/test-loop.mjs status` shows the latest run. **Agents:** run only the targeted tests of your change (`npm test -- word ...`, e.g. your new test and the tests of the files you touched) and move on; do not start or wait for full runs. The coordinator follows up on NEW failures from the loop. Everything else in the entry below still applies (deploy-safe edits, the core build lock, the known baseline failure).
>
> **`npm test` runs in parallel, queued (2026-09-25):** `web/run-tests.mjs` runs every command of package.json `"test:all"` (the && list; add new tests there) in up to 6 lanes scaled to the free cores (load average; also sets test-ps2-captures PAR), longest first, continuing past failures (summary + log tails; logs in `web/node_modules/.cache/ssx-tests/`). `npm test -- word ...` filters, `TEST_LANES=1` runs one at a time. **Agents share the tree, so (`web/test-lock.mjs`):** full runs go one at a time, and a full run requested during another reuses the next run that starts after the request; test runs never overlap a live-core build (`web/build-core.sh` takes an exclusive lock, waiting for running tests); a run that still sees core.wasm change is reported INVALID. Agents: start `npm test > LOG 2>&1` in the background, then call `node web/test-lock.mjs wait LOG 300` repeatedly until it exits 0 (never end the turn to wait: an idle agent is stopped by the 10-minute stall watchdog, and a queued full run can take longer); run heavy tests only through it (`npm test -- ps2-captures`); `node web/test-lock.mjs status` shows the queue. No known baseline failure: `test-slopestyle-bigair` passes since 2026-09-27 (the held-out `ass1-griff-87e9ff58`: the human's landing body scale and the computer rider's rail held-jump steering; see the 2026-09-27 slopestyle entry and slopestyle-bigair.md "Gaps"). Fixed 2026-09-25: the Quick Play roster (ASS1 `lineups.json` / `freestyle-rosters.json` rebuilt from all 30 states) and the R&B raven B section re-entry (RNG at 221; the other four captures are exact for 900 ticks; `web/section_gameplay.inc`, `set_piece_gameplay.inc`, `attached_setpieces.inc`, `tools/export_location_set_pieces.py`, `generated/set_piece_seed_ASS1.hpp`; web/runtime rebuilt 18:17).

> **Simulation CPU cost: exact speed-ups + phone presentation fast mode (2026-09-25, performance agent):** see [sim-performance.md](sim-performance.md). Deployed 2026-09-25 (code only); web/runtime rebuilt with web/build-core.sh (14:31).
> - **Result (final tree, paired/interleaved):** six-rider Snow Jam tick 0.37x in node (`web/bench-sim.mjs`: 4.45 -> 1.67 CPU ms, 1.38 with the fast mode); Chrome 390x844 at 4x throttle 9.0-9.6 -> 16.4-19.7 fps (13.9-16.4 exact), simulation 17-18 -> 6.3-7.1 ms/tick, 36-39 -> 58-60 game ticks/s; Chrome desktop 4.4 -> 2.0-2.2 ms/tick. Safari not measured (no remote automation).
> - **Exact:** fast toward-zero primitives (`engine/software_float.hpp`; old vs new: exhaustive sweeps + billions of random/edge pairs, native and wasm, 0 mismatches; `tools/test_exact_float.sh`), no volatile/fesetround in the wasm helpers, FX presentation buffers built when read, one packet decode per pose part, indexed set-piece/instance lookups, compact collision bounds scans, leaner ai-racers.js glue. Per-tick six-rider traces identical to the baseline.
> - **Low tier only:** `presentationFast` (`?fastfx=0|1`, `web/presentation_fast.hpp`): palettes and snow sprites in plain float; simulation identical on vs off over 6000 ticks.
> - **Gates (live tree):** all 141 npm test commands give the baseline's results and output (ps2-captures 215 scenarios, rider contexts, AI racers, lineups, long runs, stage world, mp plausibility); test-slopestyle-bigair fails and test-mp flakes in the baseline too. Native ctest 25/25. Next: the Worker (sim-performance.md "Phase 4").

> **Peak 2 connected world (PEAK2) (2026-09-25, Peak 2 agent):** see [peak2.md](peak2.md) section 6. Not deployed.
> - **World:** 17 locations (C, D, the six courses, connectors, DRA4_A, ERA5_C; CSKY / DSKY) built with the per-peak tooling (`export_peak_world.py --peak 2`, stage / sections / instances / rail flags from the PS2 free-ride states in `local/ps2-capture/peak2/`, seed `frd-1800` region D, `generated/peak2_stage_seed.hpp`); `generate-controllers.py` adds the PEAK2 glide seed.
> - **Browser:** CTM Transport -> Freeride starts at Yellow / Red station; `free-ride.js` station triggers use the current course; `career-ui.js` Peak 2 session points, STREAMED_PEAKS 1..3; `stage-collect.js` PEAK2_COLLECT_TRACKS; `main.js` CSKY / DSKY / ESKY domes; `peak-set-pieces.js` Peak 2 crowds; `peak-run.js` the Peak 2 Race finishes at Metro-City.
> - **Gates:** new `web/test-peak2-world.mjs` (npm test: residency rows, streamed CRA3 = event package on 1200 probes, whole world 154 MB); `test-ps2-captures.mjs peak2/fr-d-glide` (Red station -> D_DRA4 -> DRA4, 2600 ticks exact, score through 2370).
> - **Open:** the Peak 2 Race needs Peak 1's locations in the same world (DRA4_A -> A has no request row in PEAK2; the rider falls at the bottom of Intimidator); CTM on Peak 2 end to end is untested.

> **Peak 3 connected world (PEAK3) + CTM on Peak 3 (2026-09-25, Peak 3 agent):** see [peak3.md](peak3.md) section 4. Not deployed.
> - **Generalized by peak** (Peak 1 outputs unchanged; the Peak 2 agent is building PEAK2 on the same switches): `tools/export_peak_world.py --peak N [--no-setpieces]`, `export_peak_stage.py --peak N` (`web/generated/peak3_stage_seed.hpp`), `export_peak_instances.py` / `export_peak_sections.py` / `export_peak_rail_flags.py --peak N`, `export_peak_seed.py --peak N --state <abs> --region <LOC>`, `export_peak_missions.py` (Peak 3's 21 Big Challenges join the one mission seed). Core `web/streamed_world.hpp` `browser_streamed_world()` replaces the `=="PEAK1"` checks; `StageCoursePEAK3`. Browser: `free-ride.js` `COURSE_PEAK` / `peakWorldOf` / `PEAK_DEFAULT_STATION` / `PEAK_BASE_COURSE` / `peakRunWorld`, `?course=PEAK3[&peakCourse=N]`, world slices carry the world name, `stage-collect.js PEAK_COLLECT_TRACKS`, `peak-capture.mjs` / the comparer take `--course PEAK3`.
> - **PS2 free-ride states** (`local/ps2-capture/peak3/`): the CTM Transport reads the passes when Transport opens; Select Peak on another peak = "Go to this peak now?" and that peak's first arrival (Peak 3: heli "Loading...", the EBC3 movie, the heli arrival at The Throne). Transport arrivals at Black Station / Gravitude. A Ruthless (DBC2) arrival for the Peak 2 agent is in `peak3/for-peak2/`.
> - **CTM:** the Transport opens on the ridden peak, the Peak 3 Freeride list transports inside the world, "Go to this peak now?" loads another peak's world with its first-arrival cut (`career-ui.js ctm-gopeak`, `arrived[]`), Peak 3 collectibles pay $2,000, Big Challenge offers work.
> - **Tests:** new `web/test-peak3-world.mjs` (npm test): every Peak 3 event row streamed location by location answers 1650 height + rail probes identically to its event package, residency gates each, the whole world loads (12 locations, 9497 instances, 128 MB). Peak 1 world gates re-run exact on the generalized core.
> - **Core regression (coordinator report, 11:21 core):** caused by this work's helper. First diverging tick 18502 vs the previous core: the PS2-verified control-3 soft-landing fix (`web/animation_bridge.cpp` finish_landing) changed two scripted Snow Jam lines, which then wedge in a pocket (the original's direction-change reset would not fire there either). `web/test-long-runs.mjs` now presses Select after 60 s without course progress. `test-mp-plausibility` ARA1 boost exposed a handplant (motion 5) -> reset hang: `web/core.cpp` 1210B0 counted the handplant / rail / reset-hold states as air and re-requested the reset every tick; now only true air (11FE98 == 1) does. After the fix: test-long-runs 8/8, test-mp-plausibility plausible (ARA1 boost finishes at 15422), test-ps2-captures 214 scenarios green.
> - **PEAK3 capture gate:** `peak3/fr-throne-unload` (The Throne free ride across the EBC3 Unload, seed `local/ps2-capture/peak3/derived/fr-ebc3-14302.p2s`): exact through 14468; 14469 the first control-3 tick's speed limit (open).
> - **Open:** All Peak Race / Jam need the whole mountain in one world (PEAK3 + PEAK2 + PEAK1: the route crosses ERA5_C into Peak 2's C); see peak3.md section 5.

> **Keyboard control hints: key caps in place of the PS2 icons (2026-09-25, input-glyphs agent):** see [input-glyphs.md](input-glyphs.md). Not deployed.
> - **What:** every control hint now follows the last input used. With the keyboard, the icon is drawn as a key cap in the load screen panel's style: dark rounded key, light FEFONT text, `drawKeyCap`, now shared with `loading-screen.js`. Gamepad and touch (the deck is a DualShock) keep the PS2 icons. The switch shows on the next frame. The title reads "Press Enter" with the keyboard.
> - **Device:** new `web/input-glyphs.js`: trusted keydown = keyboard; gamepad buttons / sticks, edge-triggered = gamepad; touch `pointerdown` or deck shown = touch. `main.js inputs()` feeds it the pad it reads, `touch-controls.js show()` calls `noteTouchShown`, and `loading-screen.js` / `fe-options.js` use it instead of their own copies. QA: `?glyphs=keyboard|gamepad|touch`.
> - **Keys:** menus use the handlers' keys, which are the deck's `MENU_KEYS`: Space, Esc, Shift, Backspace, Enter, Q E Z X, arrows. In-race hints use `KEYBOARD_BUTTONS` for Classic / Simple, with Esc as pause and WASD as the Simple D-pad, as on the load screen panel. The name-entry keyboard shows Shift / Caps Lock / Enter.
> - **Where:** `LuiScreen.sprite` (all LUI screens: FE_1-14 icon rects recognised by `glyphButton`; an icon stacked under another is dropped in keyboard mode) and `OriginalUI.sprite` (OV_1-2 cells: ui.js legends, career / lodge / messages / Big Challenge / audio confirm / lobby). Four sites are drawn by hand: the cutscene "Press [Space] to skip", the Uber trick HUD hint (`frame({keys})`, still centred), the phone prompts and the title. The message viewer row and `career-ui help()` make room for wide caps (`glyphKeyRect`).
> - **Checked:** headless Chrome for Testing over CDP. In keyboard mode, 40 screens were driven with real key presses: title, main, character select / setup, peak / mode / event, Options + 8 sub-screens, cheat keyboard, Yes/No popup, previews, CTM pause / messages / audio / options / quit, lodge / gear / uber / songs / peaks / goals / Big Challenge / rewards / trophies / attributes, online lobbies, load screen, intro skip, objectives card, race pause / give up, Uber hint, phone prompt. In gamepad mode (`getGamepads` override) every menu legend region is pixel-identical to before. Screenshots (before / kb / pad + `report/` sheets) and the CDP scripts: `local/browser-validation/input-glyphs/`.
> - **Tests:** new `web/test-input-glyphs.mjs` in `npm test`. It checks the mapping (menu / race, Classic / Simple) against `MENU_KEYS`, `PAD_KEYS`, `buildPad` and the panel rows, icon recognition, cap placement and width (= `keycapWidth`), the Uber hint and device switching. `test-uber-hint`, `test-trick-hud`, `test-loading-screen`, `test-fe-screens`, `test-touch-controls` and `test-messages` are unchanged and pass.

> **Peak 2 Single Event playable: Ruthless Ridge, Intimidator, Style Mile, Launch Time, Schizophrenia, Ruthless / Ruthless Jam (2026-09-25, Peak 2 agent):** see [peak2.md](peak2.md). Not deployed.
> - **Inventory** (peak2.md section 1): areas C / D, residency rows, event kinds and handlers, medal / cash / platinum / freestyle tables, rival Nate (Zoe for Nate), collectibles; the Peak 2 Race can only finish at Metro-City (10E5D8 accepts courses 1 and 5..13: DBC2 -> D -> DRA4 -> A -> ARA1 -> B -> BRA2), the Peak 2 Jam at Style Mile.
> - **PS2 states** (derived only, `local/reference/pcsx2/peak2.provenance.json`): Peak 2 pass = profile block +0x278 bit 12, read when Select Peak is entered; ready / countdown anchor (tick 18) / glide for every event, Nate as the player for the rival documents, CTM free-ride arrivals at Ruthless and Yellow station (`local/ps2-capture/peak2/`).
> - **Packages:** `tools/prepare_location.py` plus the set-piece chain for all six (peak2.md section 3); registry entries in tools/locations.py, the capture registries and the core selectors are additive (Peak 3's entries sit next to them). courses.json lists all six with compiled event starts; Single Event opens Peak 2 (like Peak 3) and orders rows by the Map LUI (Style Mile, Launch Time, Schizophrenia on the PS2).
> - **Gates** (`web/test-ps2-captures.mjs`, peak2/*): Style Mile 9000 ticks, Launch Time 2826 and Ruthless Ridge 5999 (five computer riders, `--ai-state` + rng-order) exact to the end incl. bones and score; Schizophrenia physics to the end (score 3491, bones 3105); Intimidator through 834; six-rider and rival gates (computer riders 1224..3291, Nate 2852). New `web/test-peak2-events.mjs` in npm test.
> - **Fixes that apply to every course:** computer riders' stat getters (attribute bank 2; Peak 2 riders are level 4): `npc_seed_rider` keeps their stats on every surface, `export_npc_riders.py` exports the bank row, `ai-racers.js` applies it (Snow Jam AI gates unchanged); rail takeoffs pass 114298's ramp flag to 119E38 (+0x4) instead of a normal test; `ps2_capture.py discover` ignores stale stack references; `generate_event_seed.py` skips half-exported locations; stage event kind by the code's event letters.
> - **npm test:** everything passes except test-slopestyle-bigair (pre-existing R&B opponent list) and test-long-runs / test-mp-plausibility (a Snow Jam human run wedges at remaining 48092). Not from the Peak 2 changes: lockstepping the 10:52 core against the live core and against a core with both Peak 2 core changes reverted, all diverge at the same tick 18502 of the stuck run: the landing after an airborne soft collision (reaction 59) gets +0.1 on physics_info[2], i.e. the `finish_landing` soft-collision change in web/animation_bridge.cpp (10:57; the Peak 3 agent's own bisect, `nosoft`, finishes). Left to the Peak 3 agent, who owns that change (note in scratchpad `peak3/stuck/FROM-PEAK2-AGENT.txt`). **Update:** fixed by the Peak 3 agent's helper (fall-reset timer); on the 13:59 core test-long-runs (8/8 finish) and test-mp-plausibility pass, and the Peak 2 gates are unchanged.
> - **Open:** peak2.md section 5; the connected Peak 2 mountain is next (section 6).

> **Peak 3 Single Event playable: Gravitude, Kick Doubt, Much-2-Much, Perpendiculous, The Throne / Throne Jam (2026-09-25, Peak 3 agent):** see [peak3.md](peak3.md). Not deployed.
> - **Inventory** (peak3.md section 1): area E locations, residency rows, connectors (ERA5_C runs from Gravitude into Peak 2's Yellow Mid Station C), trigger volumes, session / collectible counts, painters, the All Peak Race / Jam (start The Throne, five station splits E -> C -> D -> A -> B, finish Metro-City), the end-of-career awards (no ending movie).
> - **PS2 states** (derived only; `local/ps2-capture/peak3/nav`): peak-1-selection.p2s with every profile's lock word +0x278 cleared of bits 6..19 (Select Peak reads the locks on entry, so the script backs out and re-enters) -> `gravitude-*`, `kick-doubt-*`, `much-2-much-*`, `perpendiculous-*`, `the-throne-*` (+ Psymon's `the-throne[-jam]-psymon-ready` for the Elise rival) in local/reference/pcsx2. CTM: the MCOMM Transport reads the passes when Transport opens (back out to MCOMM first); "Peak 3 > Go to this peak now?" plays the first-arrival cut at The Throne and starts free ride there (`local/ps2-capture/peak3/fr-ebc3-arrival.p2s`).
> - **Pipeline:** tools/locations.py entries, `tools/prepare_location.py`, then `local/peak3-logs/post.sh LOC` (set pieces / stage world / sections from `local/ps2-capture/runs/peak3/<name>-full`), `sh web/build-core.sh`. Tool fixes on the way: audit_rider_assemblies.py names any roster character (riders.json + CHARACTERS rigs), export_rail_runtime_flags.py skips an unpatched duplicate rail record (EHP3), export_backcountry.py `RIVAL_STATES_BY_LOCATION`, the set-piece exporters know the peak3 runs, core stage / set-piece / attached includes for the E locations.
> - **Browser:** Select Peak > Peak 3 opens (`SINGLE_EVENT_OPEN_PEAKS`; the PS2 wants a CTM pass), rows in the PS2 order, "Throne Jam"; the rival is the course peak's (`rivalCharacter`, Psymon / Elise); a rival headline wider than the card wraps like the PS2. Checked in Chrome (scratchpad `peak3/qa/event.mjs`).
> - **Core fix (helper):** an airborne soft collision (control 3) that lands used to be treated as a control-5 air exit (web/animation_bridge.cpp `finish_landing`): now the retained spin +0x2DC and the prewind triplets stay, the soft controller ends and the jump latch clears, as 139C88 -> 11FEC8 (control 3 has no exit, 0x456B90). Raised `bc/bc-race-tuck` to all riders exact through 6700.
> - **Gates** (`peak3/*` in web/test-ps2-captures.mjs): Much-2-Much exact to the end; Perpendiculous physics to the end; Kick Doubt through 3781 (the 1:00 time-out tick); Gravitude through 2963 (fall reset one tick early, open); The Throne through 696 (pose-seed contact). test-locations loads all five.
> - **Gaps:** Gravitude lineups by human character (lineups.json / grid scales) and Kick Doubt Quick Play rosters are not exported (the anchors' riders race); intros use the courses' NIS lists as exported; the connected Peak 3 world and CTM progression are next (peak3.md).

> **"Screw Up" plays: MicroTalk music streams decoded like the original (2026-09-25, audio agent):** not deployed.
> - **Root cause:** the only song whose stream bars are MicroTalk (`screwup.mus`, 261 bars, codec2 4, tag 0x80 = 2) was either rejected (`decodeStream` threw "unsupported stream codec 4" every tick) or read like SND bank data (fresh EA-wrapped decoder per block, first byte as the 0xEE flag) = full-scale noise. The original's stream voice (`0x3C9960` / read `0x3C96F0`, voice type 2 of codec 4 in `sub_003C92F8`) differs in three ways: it **skips the first byte** of each block's channel data (`01` first block, `00` after), it parses the header only on the **first block of each SCHl** and otherwise **keeps the decoder state across blocks** (bit 31 of the block count from `0x3B6EA8`, `ctx+0xD5C`), and with tag **0x80** < 3 it runs **plain MicroTalk** (`ctx+0xD64 = 0`: no flag bytes, no PCM patches, no byte re-alignment). Details: docs/audio-formats.md section 4 "MicroTalk streams".
> - **Port:** `web/audio-decode.js` (`UtkDecoder` plain mode + `open()` without header, `decodeStream` MicroTalk branch, one decoder per channel per SCHl; bars stay self-contained). A bar costs ~40-50 ms, so `web/pathfinder.js` worker-decodes MicroTalk bars when the player commits them (`decodeSegment(seg, {aheadMs})`, `musicDecodeWorker()` -> `web/audio-decode-worker.js`, sync fallback when due / no Worker) and `prefetchSongStart` decodes the opening bar before the song starts (dry run with recorded random draws replayed by the player); `web/game-audio.js playSong` uses it. There was no skip-this-song workaround to remove; `audioSafe` in main.js stays as a general guard.
> - **Exactness:** `tools/test_microtalk_native.py` now also runs the recompiled stream voice read `0x3C96F0` (entry label added to the oracle copy, ring-buffer fetch `0x3C6DE0` / release `0x3C7010` stubbed from the SCDl blocks, 1000-sample reads) on every bar and channel: `web/test-microtalk.mjs` = 522 bar channels / 31,334,400 samples equal sample for sample (plus the 812 bank streams as before). ARMSX2: the live frame buffers `ctx+0x684` of both stream voices in the five `metro-city*.p2s` states each match exactly one frame of the port, at the frame `ctx+0xD54/0xD44` points at, within 0.006-0.11 LSB (emulator FPU rounding vs the oracle's scalar model; next best frame thousands of LSB off); those states also show `voice+0x30 = 2`, `ctx+0xD64 = 0`. Stats: RMS 0.284 over all bars (EA-XA songs 0.2-0.3). In game (Chromium pane, Metro-City race, radio switched to Screw Up via `previewSong`): Now Playing box, music bus RMS 0.21, 0 NaN over ~25 s, no console errors, 60 fps.
> - **Tests:** `test-audio-decode` (Screw Up case), `test-microtalk` (music segments finite + oracle), `test-pathfinder` (worker path + prefetch). Rerun the oracle after re-exporting audio: `python3 tools/test_microtalk_native.py`.

> **Conquer the Mountain event flow from free ride + audible cutscene sounds (2026-09-25, CTM-flow agent):** see [ctm-flow.md](ctm-flow.md). Not deployed.
> - **PS2 trace** (scratchpad `ctm/trace.md`, captures `ctm/caps/CAPTURES-ctm.md`, sound logs `ctm/audio/NIS-AUDIO.md`): riding into a course's RaceRideState gate (builtin 67 → 0x22D6C8) starts the event **in the streamed world** (world state 1 arg 1): no prompt, no load, no streaming change. The free-ride view fades to black over 30 ticks under the sliding bars and "Loading..." (the fly-over's header fade), then the venue fly-over (music code 21-25 = next song + EA RADIO BIG, PA venue intro, its own bank sound), a bright hard cut to the random approach (PA rider race intro), the start-gate idle fading in under the growing round card (PA sponsor intro), Cross → hard cut to the race camera and countdown. Results "Transport" → `endevent_trans_arr` #152 (not at Snow Jam) → map → the course (event or free ride) = free ride at session point 1 with a white fade; Restart / Next heat = the gondola ride-up then the card; the final heat adds the start hut.
> - **Browser:** new `web/ctm-event.js` (`careerUI.rideIn`, called by main.js's gate listener): pre-fade over the running world (`cutscenes.preFade`, drawn by ui.js on the game screen), the fly-over in the streamed world, then the in-page course switch (the other agent's `navigateCourse`) under a black "Loading..." cover (`loading-screen.js` world mode, no Basic Controls screen) with the fly-over's song carried through (`gameAudio.carryWorld`), then the approach ('career-ridein') and the card. career-ui.js: `begin(..., {rideIn})`, card grow-in + 30-tick Cross lockout + the idle's fade under it, `restartToCard` (confirm → gondola → card), `transportAfterEvent`, free-ride Transport list events transport to the course, podium pre-fade, final-heat start hut. Side-by-side sheets vs PS2 at matching script times: scratchpad `ctm/sheets/ara1-flow-sbs.png`, `bra2-flow-sbs.png`.
> - **Cutscene sounds were silent because** a step's clock started before its bank finished loading: the sound was requested with no bank, the failure was latched, and it never retried. Now a step starts when its actors/bank/set are in (`seq.ready`); NIS sounds go to CHARACTER at speaker 0's gain (0x280F3C); actor sounds are positioned in PS2 cm (they were in scene metres → distance gain ~0). PS2 logs confirm: the Single Event start-hut intro has **no** CHARACTER sound on the PS2 either (only ambience, crowd, stage loops); those world sounds now run under intros (`gameAudio.cutscene`, `nisWorldTick`). New: `gameAudio.cutsceneMusic(code)` (21-25 PickNextSong + FadeOut 1 s + forced PlayMusic(36) at 3 s; 19/20 no-op with a2 = 0), PA cues 1/2/7 (`riderIntro`, `riderRaceIntro`, `medals` = PA_Medals on every podium). Fades chain like 0x277980 (`cutscenes.fadeAt`: a later step fades in with the previous step's fade-out record). docs/audio-logic.md corrected (PA_Rider_Intro/Race_Intro/Medals are retail, 2A19D8 is the NIS cue dispatcher) + 9.10.
> - **Measured (Chrome, per bus):** CTM fly-over CHARACTER 0.20-0.35 (was 0), PA at #94 t30 / #66 t0 / card; Single Event intro UI bus (ambience + crowd) live, CHARACTER 0 as on PS2; podium PA_Medals + CHARACTER from t240; gondola #146 CHARACTER. **Safari:** measured before the change (CHARACTER 0, MUSIC/PA only); after it, Safari in this workspace stalls at every course load (after the course's world textures, before terrain-render; Chrome loads fine) — not caused by these files as far as I could tell, needs a look with the course-switch work.
> - **Tests:** test-cutscenes (ride-in lists, fade chain vs PS2 frame means, flow cues), test-game-audio (music codes, carry across a switch) extended; test-career, test-loading-screen, test-fe-screens, test-audio-sfx, test-peak-mountain pass. No core (wasm) state added.
> - **Gaps:** ctm-flow.md section 7 (black hold during the page's course switch instead of the PS2's few ticks, no computer riders in the fly-over, no transport cut on the way back, card grow-in is a clip reveal, stage-script loops under intros).

> **Port-made UI rebuilt from the game's own screens (2026-09-25, UI-fit agent):** see [mobile.md](mobile.md) ("Settings: Options > Display & Touch", "Prompts") and [hosting.md](hosting.md) ("Login page"). Not deployed.
> - **Phone settings:** the ≡ HTML panel is gone. Options (18options) has a "Display & Touch" item above DONE (`fe-options.js optionsWithDisplay`), and `fe-display` is Game Options (19game_opt) with its rows replaced (`displayScreen`, `DISPLAY_ROWS`): Resolution, Upscaling, Frame rate, Quality, Touch controls, Touch layout, Touch stick, Touch vibration; values apply at once (`quality.js setQuality`, `touch-controls.js setSetting`), Quality and "touch Off" ask with the original Yes/No popup (FeScreens.prompt, now also drawn over sub-screens), touch rows greyed for keyboard/gamepad play. ≡ pauses a race and opens it over the pause menu (△ returns there), on menus opens Options. No reload anywhere (the course-switch agent's `applyAntialias` path).
> - **Prompts:** Add to Home Screen hint and a turn-your-device card (only when an orientation has no room: `computeLayout().cramped`) are the FE popup box on their own canvas (`web/phone-prompts.js`), loading their own FEFONT / popup / snow assets so they work before the UI or 3D. `?phonePrompt=home|rotate` for QA.
> - **Login page:** `gate.mjs page()` redrawn as an SSX 3-style screen with CSS/SVG only (no game data before login) + a strict CSP (style pinned by hash).
> - **Fix:** a whitefade started on a screen FeScreens does not draw (Square on Main Menu / Select Character, ≡ on the title or a pause menu) never completed; `ui.js drawStreamingNote` now calls `FeScreens.drawForeignFlash`.
> - **Files:** web/fe-options.js, web/fe-screens.js (Options item, `openDisplay`, prompt over sub-screens, `drawForeignFlash`, choose() ignores input under a prompt), web/touch-controls.js, web/phone-prompts.js (new), web/mobile.css (panel CSS -> `#tc-prompt`), web/ui.js (one line), web/quality.js (comments), web/server/gate.mjs; tests: test-fe-screens (Display & Touch flow, DONE is index 8), test-touch-controls (cramped rule, prompt texts, settings load). Checked in Chrome iPhone emulation (menus, race pause, prompts), the iOS Simulator (Safari: ≡ -> Options -> Display & Touch by the deck, live layout switch, home hint, login page) and desktop keyboard.

> **Course changes without page reloads (2026-09-25, navigation agent):** see [course-switch.md](course-switch.md). Not deployed.
> - **What:** Single Event course picks, entering / leaving the Peak 1 world (free ride, transport to a race course, peak runs), the online lobby's course and Back / Forward all run in the page: `main.js navigateCourse` pushes a history entry with the old reload query (`?course&rider&base&autostart`, `peakCourse` / `peakMode`, online `&online=1&lobby=`; deep links and reload-from-URL unchanged), `switchCourse` shows the original load screen, `unloadCourse` releases the old course, `loadCourse` (the old per-course half of `init`) builds the new one, then it ends like a load with `?autostart=1` (career `resume()` or the event load: warm-up, pipelines, audio prep, intro, all under the load screen). `init` keeps the page-level setup (renderer, UI, audio engine, input, cutscene player, online session, render loop).
> - **Contract:** `ui.cb.course` / `freeRide` / `peakRun` unchanged (return `false` while the page changes course); `ui.courseReady` is a Promise for the switch in progress. Back / Forward: leave the run / online lobby / career session, load the entry's course, land on the menu it was left from (Single Event selector -> Select Peak), else Select Peak for an event entry, else the main menu. Online: `net/mp-game.js` switches in place (socket and seat kept, no reconnect); a host start during the load waits for it. Quality tier (touch settings): no reload, MSAA via `applyAntialias` (`renderer._samples`) + a re-warm.
> - **Release:** a fresh core instance per course from one compiled wasm module (`newCore`); the old one is released (`window.__coreRefs` WeakRefs empty after GC). Fixed holders of old cores: FE preview / cutscene actors (`fe-preview.js releasePreviewCores`), `CharacterSelect.core`, `game-audio` `state.humanCore` (cleared in `leaveWorld`), `lineup.js pendingRace` (`settleRaced`), `ssxEffects.core`. Three objects: every course object's dispose event + materials / geometries / skeletons / textures (TSL texture nodes and the snow flipbook `warmTextures` included), post passes, workers (terrain, peak world), encoded-pass effects (`snow-composite.js pruneEncodedEffects`), rider shadows (`rider-shadow.js` dispose releases its silhouettes). Audio errors no longer stop the frame (`audioSafe`: a song with stream codec 4 = microtalk threw every tick from `pathfinder.js` and froze the race; pre-existing, audio agent's to fix).
> - **Measured, Chrome** (6 switches Metro-City / Snow Jam from the menus): unload 9-51 ms, load 1.9-2.6 s, warm-up 1.3-1.6 s, click -> objectives 14.7-14.9 s (7 s load-screen minimum + intro; a first page load of the same event: 15.1 s); long tasks per switch 5-6, longest 170-274 ms (first load 7, 177-182 ms). GPU objects at the same race point flat with `?cutscenes=0` (Metro-City geometries 2745/2746/2747, textures 490/492/494, render objects 4129/4131/4133; Snow Jam 3484/3485/3485, 497/499/499, 6549/6551/6551); with cutscenes the actor pool (8 idle models, by design) fills up. **Safari** (safaridriver, trusted pointer clicks, 5 switches): unload 16-54 ms, load 1.86-1.99 s, warm-up 4.3-5.7 s, click -> objectives 16.1-17.3 s, load-screen frames p50 17 ms / p95 43-55 ms; same GPU pattern as Chrome; WebContent footprint at the menu after each event 2504 / 2525 / 2061 / 2238 / 2014 MB (boot 1214), old cores collected. Back / Forward land on Snow Jam / Metro-City Select Peak. Free ride PEAK1 load 2.9 s, Peak 1 Race load 2.8 s up to its objectives card (the race start after Continue was checked in Chrome only: the Mac's screen locked during the last Safari run, and a locked screen hides every Safari page, so rAF stops and loads wait). Online (Chrome host + Safari guest, 5174/8787): ARA1 -> BRA2 in 2.6 s, -> BHP1 0.6 s, same client id, no new WebSocket, race on BHP1.
> - **Tests:** npm test: everything passes (test-ps2-captures 197 scenarios) except the pre-existing `test-slopestyle-bigair` R&B "every Quick Play opponent observed" assertion (listed in the entries below as independent of this work); the tests after it in the chain were run one by one: all pass. Production build (`vite build --config server/vite.online.config.js` into a scratch dir): main.js and core.js reference the same hashed core.wasm.
> - **Also fixed on the way:** `free-ride.js stop()` now releases locations with `releaseLocation` (textures / atlas; ~330 textures leaked per Peak 1 world before), `game-audio.js leaveWorld` evicts cached song files (the page used to drop them on every course reload; JS heap was growing ~25 MB per event, now flat: Metro-City 174 / 189 / 186 MB, Snow Jam 201 / 207 / 200 MB after GC in Chrome). Peak 1 free ride <-> Peak 1 Race x3: GPU objects flat (geometries 3711 / 3711 / 3711).
> - **Gaps / for others:** save import (`fe-saveload.js`) still reloads the page (a dozen modules read their save only at start-up); the song with codec 4 (above; fixed 2026-09-25, see the "Screw Up" entry).

> **Download progress inside the game (2026-09-25):** the HTML download bar is gone (user: additions must fit the game itself). `web/downloads.js` now only shares downloads and exposes `downloadProgress()`; the original loading screen's percentage is capped by the real transfer (up to 90%, then the original curve; never backwards), the title card's "Loading..." shows a percentage, and menus draw a FEFONT "Loading..." bottom right while data streams (`drawStreamingNote` in web/ui.js, called after the frame's ui.draw in main.js). Rule for all future additions: build them from the game's own screens, fonts and art.

> **Saves, the Single Event track selector, main menu, Previews, Save/Load (2026-09-25, front-end agent):** see [characters.md](characters.md) ("Main menu, Single Event track selector, Previews, Save/Load", "Saving progress"). Not deployed.
> - **Progress persists:** the career is `localStorage ssx3.career.v2` (schema v2, `web/career-save.js`; v1 migrates on first load and stays as a backup), written atomically through `web/save-store.js` (`<key>~tmp`, then the key, recovery on read; blocked / full storage never throws). Career, free-play outfits (`ssx3.outfit.free.v1`, now atomic), options, controls, audio, player name, cheat unlocks, relationships all come back; new `ssx3.selection.v1` restores the last rider (on the first Select Character of a visit; `?rider=` wins) and the track selector's peak / mode / event. localStorage, not cookies (why: characters.md). Options > Save/Load is the original 25saveload (`web/fe-saveload.js`): Save game, Load game (93profile_load), Save / Load options, Load replay (greyed), New game (Yes/No popup), plus Export / Import save file (`{format:'ssx3-save',version:2,keys}`, validated + migrated).
> - **Single Event:** `web/fe-event-select.js` plays the original `Map` + `femap_template` screens (new exporter `tools/export_fe_menus.py` -> `UI/fe-menus.json`, in `npm run setup`): Select Peak (Peak 2 / 3 shown with locks and locked for now), Select Mode (routes of the mode red), Select Event (Snow Jam / Metro-City / Happiness, R&B / Crow's Nest / The Junction / Happiness Jam; focused route red + start indicator from the Map states, map tab, original help; medal column + Square INFO table from the career save). `ui.set('event')` redirects to `fe-peak`; Cross -> `ui.startSingleEvent` -> `careerUI.single`; `careerUI.afterCourse` accepts cb.course / cb.peakRun returning false, true or a Promise. Side by side with the PS2 frames (`local/ps2-capture/menus/single/`): near pixel match. Fix: Metro-City's help (`kT_HELPBRA2` collides with CBA2 Launch Time; `...Blah` first, also in the MCOMM Transport).
> - **Main menu:** drawn from 07main_men (`web/fe-main-menu.js`); Single Event, Conquer The Mountain, Previews and Online enabled, Multi Play (no split screen) greyed; per-item help; Square = Options. **Previews** (helper agent): `web/fe-previews.js` (146Bonusmat) plays the EA trailers NFSXSELL / NFLXSELL / ST3XSELL (`tools/export_movies.py`, exported, ~60 MB) via `web/fe-movie.js` (shared with the Rewards videos). INTRO / MTNALIVE stay rewards, as in the original.
> - **For the course-switch agent:** main.js popstate maps `fe-(peak|mode|event)` back only when `ui.feScreens.owns(...)`; those screens belong to `ui.eventSelect` (use `ui.eventSelect?.owns(st.screen)`, or push `'event'`, which `ui.set` redirects).
> - **Tests:** new `web/test-save-store.mjs` and `web/test-fe-previews.mjs` (npm test); test-career and test-messages updated for the v2 key. Full `npm test` (2026-09-25): test-ps2-captures 197 scenarios and everything else pass except the pre-existing test-slopestyle-bigair R&B "every Quick Play opponent observed" (Griff), noted by earlier agents. QA: `scratchpad/ts/qa-fe-harness.html` (the UI without the world; copy into web/ to serve it) + `cdp-shot.mjs` (headless-shell screenshots, `png.py` side by side).
> - **Gaps:** the FE background's white right-hand gradient and the main menu's dark left panel (animated vertex alpha) are not drawn (all FE screens); 122Autosave message box not shown; Peak runs appear in Single Event only once a career unlocks them (untested end to end).

> **Peak 1 mountain, the rest (2026-09-24, Peak 1 agent + helpers):** see [peak-mountain.md](peak-mountain.md) ("Free ride", "Set pieces and painters", "Audio", "Cutscene hooks", "Captures and checks", "Gaps").
> - **Arrivals fixed + exact:** 11DE60 → 26B5E0 returns the row of the *runtime* kind (= exported kind + 1) or the bank's FIRST row. Course arrivals (11D390: courses (1,2), backcountry (0,1), stations (0,2)) used a missing row: Metro-City, R&B, Crow's Nest and The Junction spawned at the Snow Jam start and fell through the void (also every Transport into them). Now `free-ride.js spawnFor/arrivalFor/regionRow`, and the original placement `place_rider_region` (11D390 → 11DE60 → 11D660 semantic 5 → 11DF18 chop-rounded, z 0; `animation_bridge.cpp reset_place_at` shared with the reset control). New gates `peak1-arrive-{ass1,aba1,bra2,bhp1}` (new PS2 Transport-arrival captures) exact to the end.
> - **Exactness:** `peak1-fr-aara1-glide` now exact to the end (1849 ticks): the 3666 step was the route heading +0x4CC from the wrong region row in `peak_world.inc deliver_paths` (112180 = exported kind 0 index 0) plus the bank delivery during the read (`peak-capture.mjs`). New gate `peak1-race-start` exact (1599 ticks; `--peak-run`, core `peak_run_start_seed`, `reset_route_seed`).
> - **Visuals:** one core stage world for PEAK1 (`web/peak-set-pieces.js`, `PEAK1/SETPIECES/` = event exports merged by resource + hub/connector lists from the Peak 1 stage tables): particles, halos, LiveComp, UV scroll, flag cloth, DeadNode/Hide states, MeshAnim pieces, magnets, crowds; the location packages split their batches like event packages. Painters by region: Lighting (core `lighting_region`, IRR banks `lighting_banks`, 22E180 default bank at 22D088), Sun and glare (`setTree`), light glows of the drawn locations (`PEAK1/LIGHT_GLOW`, `setTracks`); the B-side sky now shows at a B-side start. Free-ride instance audits for ABA1/ASS1/BHP1/BRA2 from new PS2 savestates (`local/ps2-capture/menus/fr-courses/`). Released locations dispose meshes/textures/atlas (renderer memory flat over repeated transports).
> - **Play:** in-world Transport (22CEA8(dest,7) / 22D088(dest,7), no reload; `peak_world_transport`), MCOMM Session menu (0x440770 counts, item k → point k+1), collectibles on the career save for every Peak 1 stage (one shared slot owned by a track, rebuilt at read completion / freed at unload; `set_stage_collect_row`, `peak_collect_refresh`), peak-run clock/splits without a career. Cut hooks: `web/cutscenes.js playCutscene` (the cutscene agent's module) at the lodge door, booth, transport depart/loop and station arrival.
> - **Audio** (helper): station song, DJ kinds 0-5/7, MusicTrigger zones, pktrans, Peak1Amb, world sounds of resident locations, `travel/arrived` (docs/audio-logic.md 9.9).
> - **Tests:** new `web/test-peak-mountain.mjs` (npm test: arrivals vs PS2, session rows, 8 collectible lists vs 0x43FA70, painters, glows); test-peak-world, test-peak-run, test-career, test-fe-screens, test-stage-world, test-stage-collect, test-game-audio, sun/glare/light-glow tests pass; test-ps2-captures 197/197 pass (with the one-core rider layout).
> - **Big Challenges:** a helper agent is implementing them (web/mission_gameplay.inc, web/big-challenges.js); its section in peak-mountain.md lands with it.
> - **Gaps:** peak-mountain.md "Gaps" (MultiSpline cars / spline pieces / Parent children not run in PEAK1, flag wind JS stand-in, local lights still Snow Jam's, no Session map / white fade, the cuts themselves).

> **In-engine cutscenes (NIS) ported (2026-09-24, cutscene agent):** see [cutscenes.md](cutscenes.md). Race intro deployed by the coordinator; the rest not deployed.
> - **What they are:** 167 EA "NIS" scripts in `DATA/SCRIPTS/SCDAT.BIG` (names from `scmasterdbg.dat`), each with its own AFL clips and sound bank, chosen per location from `scfilter<LOC>.dat` (ScriptChoice 0x27B0C0, rider masks = CHARDB bits), queued on the NIS list players (flags 1 Cross-skippable, 2 chained, 8 held); each camera/actor track picks one of 2-3 alternatives at random per playback. Format, engine, anchors/locators, every trigger with addresses and a per-rider table in cutscenes.md. No ending cutscene exists on the disc.
> - **Tools** (all in `npm run setup` except movies): `tools/cutscene_isb.py`, `tools/cutscene_locators.py`, `tools/export_cutscenes.py` -> `web/public/assets/CUTSCENES/` (~101 MB), `tools/export_cutscene_sets.py` (TRANSP cabins), `tools/export_transition_screens.py` (lodge load screens 117/118), `tools/export_cutscene_props.py` (the station PDA with its flip morph), `tools/export_movies.py ABC1 DBC2 EBC3 (+WS)` (silent backcountry movies; needs ffmpeg, not run).
> - **Browser:** `web/cutscenes.js` (`playCutscene({kind, id, rider, location, ...})` -> Promise; kinds intro/podium/rival/heat/transport/transport-arrive/lodge/arrival/script; FE-preview actors with the NIS head/eyes/hands/morphs and worn outfit, FE lighting with the cutscene camera's view (`fe-preview.js light(T, core, view)`); letterbox, Press X to skip, Loading caption, fades; skip = Cross/Space/pad 0/touch X; no Start pause), `web/transition-screens.js`. Hooks: `ui.loadEvent` (intro after the load screen, idle loop under the card until `startRun`), `career-ui.js` (podium + once-per-peak rival after a CTM result, next-heat gondola, new-career drop, lodge load screens), `main.js` (transport ride, lodge walk-in, booth arrival). Online races skip the intro; `?cutscenes=0` disables. No core (wasm) state added.
> - **Checks:** `web/test-cutscenes.mjs` (npm test): cameras of scripts 89/73 = live PS2 RAM, anchors, bindings, clip time, ScriptChoice, step lists, PDA. PS2 side-by-side at matching script times (`scratchpad/cs/web/*-sbs.png`, PS2 runs in `scratchpad/cs/ps2` and `scratchpad/cs/ps2b`): start hut for Zoe/Psymon/Kaori/Sam, CTM fly-over + approach, podium Zoe/Psymon/Moby/2nd place, rival challenge, heli + gondola rides, lodge walk-in and booth arrival with the PDA, the Single Event flow and the next-heat gondola. Fix found: the rival scene's actor is the race's first computer rider, not the peak's named rival.
> - **npm test** (last run 2026-09-24 evening): `test-ps2-captures` 197 scenarios and every other test pass except two that do not load any file this work changed: `test-lineups.mjs` (wasm "memory access out of bounds" in `createNodeRace`, with the `runtime/core.wasm` rebuilt 19:40 by the one-core computer-rider work) and `test-slopestyle-bigair.mjs` ("every possible Quick Play opponent observed": Griff missing from the R&B observations).
> - **Gaps:** new-career drop without a PS2 comparison and without the ABC1 movie (no ffmpeg); kind-7 stage-script calls (helicopter LiveComp/rotor wash) not run; PA_Medals and music codes 21-25 not wired; Peak 2/3 heli arrivals not triggered; actors use the FE lighting, not the world's.

> **One core, six riders (2026-09-24, computer-rider agent):** see [ai-racers.md](ai-racers.md#one-core-six-riders-2026-09-24-webrider_localhpp-webrider_contextcpp-webai-racersjs). Not deployed.
> - **What:** the five computer riders are no longer five extra cores: they are rider contexts of the human's core. Every mutable global / function-local static of the core is `RIDER_LOCAL` (web/rider_local.hpp: `no_destroy constinit thread_local`; `RIDER_LOCAL_LAZY` for maps and objects holding `std::function`), one TLS block per rider; the build (web/build-core.sh) adds `-matomics -mbulk-memory -ftls-model=local-exec`, web/rider_tls.S (mutable `__tls_base`) and web/patch-rider-tls.mjs (its initial value from the link map). `rider_context_create()` (web/rider_context.cpp) copies the pristine block and re-runs the static-initialisation work, so a context starts like a fresh core; switching is `Module.___tls_base.value = block` (web/ai-racers.js `riderContextCore` gives a context the core API). Shared: course geometry (`world`, `cameraTerrain`), constant tables, environment textures, the course-package parse cache (web/world_bridge.cpp). Everything else (world-entity copies, RNG cursors, visual words, race session) stays per rider, so the pass-order choreography and shared-world replay are unchanged. web/ai-world.js / ai-world-worker.js removed (no template, collision.bin no longer fetched for the riders).
> - **Guard:** web/check-rider-globals.mjs runs in build-core.sh and fails the build on a new plain mutable global in web/ or engine/ objects (mark it `RIDER_LOCAL` or add it to the shared list with a reason). Other agents adding core state: use `RIDER_LOCAL`.
> - **Presentation:** computer riders' skin palettes + rider lighting captured only on the last two ticks of a drawn frame (web/fixed-step-clock.js `FixedStepClock.ticksLeft`, web/ai-race.js); the choreography allocates ~30% less per tick (cached RNG views, no rest-arg wrappers, typed copies).
> - **Exact:** test-ps2-captures 193 scenarios (the 7 six-rider reports byte-identical to the multi-core build), test-ai-racers, test-lineups, test-long-runs, test-stage-world, test-mp-*, test-remote-riders, test-course-spawn, test-opponent-riders, the whole `npm test` list log-for-log vs the old build (only timings differ), 16000-tick Snow Jam / Metro-City races side by side (every rider, RNG and visual words each tick). New `web/test-rider-contexts.mjs` (npm test). Pre-existing, identical in old and new builds: test-slopestyle-bigair stops at the R&B "every Quick Play opponent observed" assertion, and past it the 3 R&B held-out captures lose the RNG at 221.
> - **Measured:** wasm memory for a race 805 MB (6 memories) -> 134 MB (1); Chrome 390x844 4x (interleaved with the old build, loaded machine) renderer RSS after GC 1020-1075 MB (was 1315-1454; `ai=0` 923-1041), 7.5-10 fps (was 6-7.5); desktop 1108-1235 MB (was 1372-1385), 60 fps; node RSS 336 MB (was 1028); load ~8 s unchanged at 1x. Browser: full Snow Jam and Metro-City races with five riders (trails, snow, shadows, results), online e2e (5174/8787) OK.
> - **Next:** the simulation in a worker (ai-racers.md "Next step"), sharing the world entities between contexts (drops the replay), a real iPhone run.

> **Safari mid-race hitching: causes found and fixed (2026-09-24, perf agent):** full Snow Jam run with the computer riders, Safari via `scratchpad/perf/safari-hitch2.mjs URL SECONDS OUT.json` (sandbox off). **Before:** p50 17-18 ms, p99 25-33, max 448-504 ms, 15-34 frames > 50 ms, 3-19 > 100 ms (stalls of 300-800 ms every 10-60 s). **After** (machine load ~10): p50 18, p99 35, max 89, 32 frames > 50 ms, **0 > 100 ms**, no pipeline/program/texture/node build after the race starts. Chrome: max 18.8 ms over 250 s, nothing built after the start.
> - **1. Pipelines and node materials built mid-race** (set-piece particles/halos, snow and computer-rider snow in the encoded pass, crowd flashes, sun Z query, light glows, computer riders' shadow silhouettes, refined terrain and flag cloth layouts, snow flipbook textures): the warm-up drew slices, but the per-frame effect updates hid or emptied those batches before the warm render. `main.js warmView` re-applies each warm frame's draw set right before the render (empty batches get one degenerate instance, restored after); `sunFlare.warm`/`lightGlow.warm`; `userData.warmTextures` (snow flipbooks, `renderer.initTexture`); `rider-shadow.js` warms every rider's silhouette; `warmProxies()` in `terrain-overlays.js`/`terrain-refinement.js` and `set-pieces-renderer.js` (flag cloth).
> - **2. The 300-800 ms freezes:** identified with `sample` on the WebContent process (`scratchpad/perf/sampler.sh`): a JSC Eden GC stopping the world waited in `JITWorklist::suspendAllThreads` for an FTL compile of `set-piece-particle-eval.js burstSpritesFast` / `trailSpritesFast`, whose DFG integer range optimization took 1-2 s (reproduced in the system jsc shell: `scratchpad/perf/irtest.sh`, inputs `scratchpad/fxsnaps.json`). Rewritten compile-friendly with identical output: LFSR streams and VU accumulators in small separate loops, parameter words read once, trail ring walk / row / blur copies split. Every kernel now FTL-compiles in < 35 ms; bit-identical to the old kernels on 13,680 captured + fuzzed emitters (`scratchpad/perf/fuzzfx.mjs`), `test-stage-world` particle parts and `test-set-piece-particle-sprites` pass. Bisection method for the next one: disable subsystems, noai runs stall at the same course positions.
> - **3. Memory (Safari's per-page memory-pressure handler runs every 30 s; above its thresholds it drops JIT code and forces GCs, the periodic 40-150 ms frames):** a three.js leak: a mesh removed with only `geometry.dispose()` keeps its render objects (bindings, uniform GPU buffers, a listener on the shared material). Refined terrain patches (and flag cloths) leaked ~50 of each per second; now `mesh.dispatchEvent({type:'dispose'})` (`terrain-overlays.js`, `set-pieces-renderer.js`; `peak-set-pieces.js` has the same pattern, not changed). Computer-rider cores: `ai-racers.js copyTemplate` copies only the template's non-zero 64 KiB chunks and clears only what a fresh core wrote (`ai-world.js` trims the template to 62.7 MB and records the fresh extent + hash; byte-identical, verified in node): ~385 MB less resident (Chrome 390x844 4x-throttle renderer RSS 1984 -> 1601 MB; Safari WebAssembly Memory ~216 MB for 6 cores). `ai-race.js` drops the ~20 MB of world texts after the template build; `main.js` drops the event world's JS vertex arrays (~35 MB) after the warm-up (GPU keeps them; index arrays stay); `opponent-riders.js` no longer keeps each rider's parsed rider.json; `terrain-refinement.js` drops worker-only patch arrays; `terrain-overlays.js` keeps triangle references in typed arrays (was 286k objects); `downloads.js` SHARE_MS 20 s -> 5 s (every repeat /assets request comes within 1.8 s; the whole ~107 MB load was held ~20 s into the race). Chrome live heap 2.17 M -> 1.61 M objects, 240 -> 195 MB.
> - **4. Allocation churn** (Chrome sampler, V8 bytes include boxed doubles that JSC does not allocate): 93-100 -> 69-73 MB/s. set-pieces-renderer copied its growing trigger/section/start logs every frame (now the unread tail), per-frame Maps/Sets/destructuring; `livecomp-animation.js nodeDeltas` cached per state (was recomputed per mesh per frame, ~30 arrays per node; bit-identical, `scratchpad/perf/ndcheck.mjs`); halos/crowd flashes read the core in place; light-glow vertex writes; terrain-detail distances; snow billboard extents; ai-racers core/rider lists, pair view copy, world-buffer slices.
> - **What remains (Safari):** every remaining > 50 ms frame (55-89 ms, about one per 7-15 s) coincides with a JSC full collection (the FinalizationRegistry probe). Next levers: fewer live JS objects, and the WebContent graphics footprint (~1.1 GB, ~42k regions: WebKit charges each GPU buffer/bind group, ~12k buffers from three's per-mesh render objects over ~3,300 world batch meshes; merging static world batches or sharing their index buffer would cut it). The one-core computer-rider restructure (other agent) removes most of the remaining WebAssembly memory.
> - Tools: `scratchpad/perf/safari-hitch2.mjs` (pipelines, rAF split, main-thread gaps, tasks, audio buffers, GC probe, rider position), `sampler.sh` (sample WebContent + GPU during a stall), `fpgrowth.sh` (footprint categories), `chrome-pipes.mjs` (anything built after race start), `alloc.mjs`/`heap.mjs`/`leak.mjs`/`native.mjs`, `mobile.mjs` (390x844, 4x throttle). `?perf=1` no longer pauses on blur. Gates: `test-ps2-captures` (192 scenarios, exit 0), `test-ai-racers`, `test-rider-shadow`, set-piece/terrain/light-glow/sun/snow/opponent tests pass; `test-stage-world` part 3 (visual log kind) fails independently of these edits.

> **Free-play wardrobe unlocked (2026-09-24):** Single Event and online (anything outside Conquer the Mountain) now own every Equip Gear item for every rider (`web/wardrobe.js` freePlay: `Wardrobe(..., {allOwned:true})`), starting from the outfit the rider already wears and saved per rider in `localStorage ssx3.outfit.free.v1`. The career keeps the original profile record, its lodge purchases and unlocks; free-play changes never touch it (`web/test-wardrobe.mjs` "free_play_unlocked"). Online races show the worn outfit to the others (outfitKey -> remoteOutfitRider); the old "online keeps the default outfit" shortcut is gone. Also new: `web/diagnostics.js` field reports from real devices -> host `~/ssx-host/logs/diag.log` (`POST /mp/diag`, gated, rate limited).

> **Phones: touch deck, quality tiers, mobile browser fixes (2026-09-24, mobile agent):** see [mobile.md](mobile.md). Deployed (`deploy/deploy.sh --no-assets`).
> - **Controls:** `web/touch-controls.js` + `web/mobile.css`: portrait = the 4:3 frame at the top in a console-style case, a DualShock-style deck under it (L1 L2 · R2 R1 strip above the frame; d-pad, floating left stick, right stick, SELECT/START, face diamond ✕ □ ○ △ below); "All below" layout and a landscape fallback (controls beside the frame) in the ≡ settings. Every control holds the keyboard code `pad-input.js` maps to that PS2 button (IJKL d-pad, Space/Shift/C/Y, Q/Z/E/X, Backspace), the sticks are a virtual gamepad's axes and START is gamepad button 9, so `buildPad()` gives the same 24 channels as keyboard/DualShock (`web/test-touch-controls.mjs`, in npm test). Per-finger pointer capture, rolls between face buttons, 70 ms minimum press (taps shorter than a frame reached nothing), menu key synthesis off the race screen (arrows, ✕ Space, △ Escape, □ Shift, ○ Backspace, START Enter when no run), the loading screen shows the pad legend. Shown on coarse pointers / first touch, hidden on real keys or gamepad.
> - **Browser:** viewport `viewport-fit=cover` + no zoom, apple-mobile-web-app metas, `manifest.webmanifest` (fullscreen; `.webmanifest` MIME added to mp-server), touch-action/overscroll/gesture/dblclick suppression, visualViewport + safe-area layout, fullscreen button (Fullscreen API + orientation lock; iPhone: Add to Home Screen hint), `navigator.audioSession.type='playback'`. Removed the old `#touch` buttons; `startRun` and the results path wrote `$('#touch').hidden` (the results one threw once the element was gone).
> - **Quality:** `web/quality.js` tiers (phone = low: 640x448 drawing buffer = PS2 frame, no MSAA, auto 60/30 fps, 4-tick cap per drawn frame; desktop = high, unchanged: DPR 1.5 + MSAA). `?quality=`, `?renderScale=native|native512|css|full`, `?upscale=smooth|pixelated`, `?fps=`, `?aa=`. main.js: `sizeRenderer` in `layoutStage`, `antialias:quality.antialias`, `frameGate` around `frame`, `min(dt, quality.maxFrameDt)` into the fixed clock (offline only; same ticks, same per-tick inputs), `ResizeObserver` on `#stage`.
> - **Measured (Chrome iPhone emulation, 4x CPU throttle, Snow Jam + 5 computer riders):** before 4-5 fps (the fixed clock's debt spiral: 12 ticks per frame), after ~10-12 fps; without computer riders 33-38 fps. Render scale makes no measurable difference (CPU-bound even on SwiftShader). Profile: simulation 68% (computer riders' `_fx_pass` 13%, `_animation_pose` 11%, `_rider_skin_palette` capture 10%), `fogRenderer.render` 19%. Memory: renderer footprint 1.73 GB (0.99 GB with `ai=0`; ~147 MB per computer rider): the five AI cores are 128 MB resident each though only 57 MB is used, because `ai-racers.js copyTemplate` reads/zeroes every chunk of a fresh core (on XNU that makes untouched wasm pages resident); skipping chunks above the fresh core's heap top would save ~335 MB (not changed here: that file is another agent's). Details and the open levers (AI cores off the main thread, palette capture per drawn frame, copyTemplate heap-top skip) in mobile.md.
> - **Checks:** test-touch-controls, test-pad-input, test-widescreen, test-frame-clock, test-loading-screen, test-fe-screens, test-audio-menu, test-mp-gate, test-precompress, test-ps2-captures (192 scenarios) pass. iOS Simulator (iPhone 17 Pro, Safari): title -> menus -> character select by the deck, race with two-finger stick + ✕ jump, START pause/resume, settings panel, both layouts. Needs a real phone: WebGPU/WebGL on device, thermals, the iOS 18 switch haptic, Safari's real memory ceiling with computer riders.

> **Fixes (2026-09-24, later):** (1) **Superpipe / non-Snow-Jam spawn:** a character's `original_event_start` (recorded on Snow Jam) replaced the course seed wholesale, so Sam/Psymon/Elise/... started at Snow Jam's coordinates on The Junction, Happiness, R&B, Crow's Nest (mid-air, stuck). `web/animation_bridge.cpp human_event_seed_for_course` now carries it over relative to the course's compiled Zoe seed (offset in the gate frame, depth deltas, stance/lift scalars); Snow Jam unchanged. Test `web/test-course-spawn.mjs`. (2) **Menu hover stole keyboard/pad navigation:** `sync()` rebuilds the overlay buttons after every move and the browser reported the button under a resting cursor as entered; `web/ui.js` now selects on hover only after real mouse movement.

> **Hosting follow-up (2026-09-24):** gate bypass via the Cloudflare edge cache fixed (gated responses `private`); gzip copies of the game data (first visit 84 -> 31 MB); `web/downloads.js` shared downloads + download bar; empty-file server crash fixed; edge worker `deploy/edge-worker.js` (`web/test-edge-worker.mjs`) deployed as Cloudflare Worker `ssx-edge` on the site's `/*` route: gate checked at the edge, game files served from Cloudflare's cache (MISS once, then HIT), pre-fix cached files now 401. Details: [hosting.md](hosting.md#downloads-compression-and-caching).

> **Hosted (2026-09-24):** see [hosting.md](hosting.md). A Mac host (`SSX_HOST` in the git-ignored `deploy/.env.local`, SSH), a Cloudflare Tunnel -> sandboxed `web/server/mp-server.mjs` on 127.0.0.1:8787 serving `dist-online` + `public` behind a shared-password gate (`web/server/gate.mjs`, password in `~/ssx-host/etc/gate-password`). Update with `deploy/deploy.sh`. Code mirror (no game data): https://github.com/owattenmaker/ssx-web, working clone `~/Documents/ChatGPT/ssx-web` (sync from here, then commit/push there; this repo stays uncommitted).

> **Big-air reset, rider shadow, Safari hitching (2026-09-24):**
> - **"Reset to the top of the run" after a super-big air:** `web/core.cpp` had a host failsafe that respawned the rider at the grid spawn after 6 s of air (`!grounded&&airtime>6`). A charged Snow Jam kicker + trick boost stays up longer. Removed (only the non-finite-position guard is left); the original's own long-air reset (1210B0: predicted air > 45 s or > 5 air bounces, `originalCollisionTimerReset`) is already wired at core.cpp ~511. Repro: `scratchpad/bigair2.mjs 2196 1.3 3000` (6.0 s air teleported to the spawn before, lands now).
> - **Shadow "twirling / long top hat":** the silhouette atlas was written mirrored across the fit's B axis on both backends (three's convention: texture v = 0 samples the row drawn at clip y = +1; WGSL never flips, GLSL flips render targets). `web/rider-shadow.js` now draws at -n.y and samples v unflipped (the backend `flipV` is gone); verified by projecting head/hands/feet into the atlas (lit at v, not 1 - v) and by frozen-frame on/off diffs. Also: the box is fitted to the pose the skin draws (`web/rider-skinning.js shadowPose`: world_pose_bones captured with each palette, blended by the same alpha), hidden parts/gear cast nothing, and terrain outside the box receives nothing (the GS CLAMP edge would otherwise streak a silhouette that reaches the tile edge, e.g. Sam). `test-rider-shadow.mjs` still exact.
> - **Hitching:** done, see the "Safari mid-race hitching" entry at the top.

> **Pad vibration exact (2026-09-23, characters agent):** `web/rumble.js` now runs on the core's rumble calls (audio events 30 impact -> owner +0xDFC max, 31 slide/get-up -> +0xE00 set) with the 0x125B18 decay/motors; main.js calls `rumble.tick(core, enabled)` right before `gameAudio.gameTick` (sfx-game drains the queue). Core observer events added at every +0xDFC site (landings 10EAD8 incl. rail/instance, obstacle 105E7C, instance 105C30) and the phase-3 get-up 12D8E8 slide event (animation_bridge.cpp). `web/test-rumble.mjs` (in npm test): v0/v1/motors exact on every tick vs new derived captures `local/ps2-capture/runs/rumble/*-rumble` (`--watch 0x147018c:8`) and event-race. docs/characters.md "Vibration 1P".

> **Peak 1 freestyle: R&B slope style (ASS1) and Crow's Nest big air (ABA1) playable (2026-09-23):** see [slopestyle-bigair.md](slopestyle-bigair.md).
> - **Loaded like the other courses:** `tools/locations.py` ASS1/ABA1, `tools/prepare_location.py`, new event savestates `local/reference/pcsx2/r-and-b-*.p2s` / `crows-nest-*.p2s` (ps2_navigate from freestyle-events), opponent `npc-riders.json` (Moby), `freestyle-event.json` (`tools/export_freestyle_event.py`: mode/handler/kind, time limit, checkpoint list 0x4D33B8, rank mode). Set pieces, stage scripts/world, sections, particles by the set-piece agent (docs/set-pieces.md).
> - **Rules from the ELF:** freestyle handler 0 by kind; slope style posts 4 riders and slot 1 is a computer opponent who rides (Quick Play: the last shuffled character, career: the peak rival), place by score (10F998 mode 2, `race_world_score`), OPPONENT HUD line (1ED5C8), checkpoints add +60 s and the '+60' popup (1194C0/0x2398E8), multiplier icons 10E830/119448 (new port, stage effect type 3), final-round estimate 0x122E50; big air = pipe rules with the 60 s table row. Single Event and Conquer the Mountain use them (`web/career.js`, `web/career-ui.js`, `web/ai-race.js opponent()`).
> - **Gates:** `peak1/crows-event-tuck` physics+bones exact to the end (2446), score through 2072 (2000-point icon); `peak1/rnb-event-tuck` physics through 6671, score through 6675 (x5 icon, trainbox crash, checkpoint +60), bones through 1560. `test-slopestyle-bigair.mjs` in npm test. 184 capture scenarios pass, test-ai-racers unchanged.
> - **Screens:** Chrome and Safari (60 fps median in both) vs PS2 frames: `scratchpad/peak1/{ass1,aba1}-{start,midrun,results}.png`, `ass1-card.png`.
> - **Gaps:** career slope opponent is still the Quick Play anchor's Moby (no Mac/Griff R&B assembly); progress meter is the static race approximation; R&B 6672 soft-collision 0.03 cm and bones 24/25 from 1561; Crow's post-finish combo clock at 2073.

> **World: stage world, pickups, collectibles, shadows, visual RNG order (2026-09-23, world agent):** see [set-pieces.md](set-pieces.md#stage-world-2026-09-23), [stage-scripts.md](stage-scripts.md#builtin-audit-ara1--bra2--bhp1-race-programs-2026-09-23), [terrain-render-fidelity.md](terrain-render-fidelity.md#rider-shadows-2026-09-23), [visual-rng-order.md](visual-rng-order.md).
> - **Stage world (web/stage_world.inc):** every builtin of the ARA1/BRA2/BHP1 programs now acts in the core: particles (2000+ effects word-exact vs 90 PS2 savestates), MultiParticle roadflares (105/106, exact), MeshAnim break pieces (13, deterministic words exact incl. life with the EE sub.s mask), node states, MagnetModifier point pickups (90: BHP1 pointa/pointc fly to the rider, award exact: **pipe-finish 2862 closed**), HaloModifier glows (97, `web/set-piece-halos.js`), one-way volumes (7, `engine/one_way_volume.hpp`, entity words exact), CrowdMan2d (88: animated crowd texture + camera flashes, `web/crowd-2d.js`), contact guard (87), event-kind test (43: the finish reset planes kill themselves, **setpieces/full 12199 closed**; the gate runs the stage world to the end), current resource -1 (61). Sounds 30/31/73 are queued for audio (`audio_stage_sound`).
> - **Collectibles (37/38/39):** `set_stage_collect_state(path, mask)` = 0x535C11 + the career collect row: single events keep every collectible dead (as before), career races show the uncollected ones and each collect marks `Career.markCollected` (bit + cash + earnings, `ssx3.career.v1`) via `web/stage-collect.js`. Fixed BHP1 `course_index` 12 -> 11 (award 1000 -> 500).
> - **Visual RNG 0x4FF018:** the world consumers draw on the human core's stream in the PS2 order (section pass: flag grid builds; post-rider pass: flag wind, camera splash, lightning, crowd timers), checked against the PS2 draw trace. Remaining owners' gaps (FX pass before the contact programs, computer riders' emitters on separate streams, audio Math.random, stream seed) are listed in visual-rng-order.md section 3.
> - **Rider shadows:** projected silhouettes of every visible rider on the terrain (`web/rider-shadow.js`, `riderShadowReceiver` in world-material.js), fit exact vs 8 PS2 shadow objects. main.js: one tag in asset(), one `riderShadows.update(camera,warming)` before the fog render; opponent-riders.js tags its groups; rider-skinning.js exports `worldNode`.
> - **Tests:** `test-stage-world.mjs` (+ MeshAnim, MultiParticle, Boost, BHP1 score, visual-pass trace), `test-stage-collect.mjs`, `test-rider-shadow.mjs` in `npm test`; gates `pipe-finish` and `setpieces/full` now run with `stageWorld: true` (`STAGE_WORLD=1`).
> - **For other agents:** physics: wire `browserStageTeleport(matrix)` (BRA2 beams, builtin 34; spec [stage-teleport.md](stage-teleport.md), no PS2 capture yet); move the FX pass after the contact programs and share the visual stream with the computer riders' cores (visual-rng-order.md G1/G2); setpieces/full 12297 finish boost +0.35 (also without the stage world). Audio: call `set_stage_crowd_cheer(level)` (crowd record +0x18C) before each tick and move rand15 draws onto the stream (G5). Exporters are shared: `tools/export_set_piece_particle_textures.py` now keeps every location in its manifest (a single-location run had dropped ARA1's textures).
> - **Gaps:** rider side of the teleport; shadow receivers are all terrain in the slab (PS2: near patch lists), LOD-0 meshes for the silhouette; lightning strikes never happen on these courses (chance 0) so the flash overlay is not ported; no PS2 ground truth for a career-race collect or a one-way push (plans in the docs).

> **Connected Peak 1 mountain: free ride, streaming, stations, Peak 1 Race/Jam (2026-09-23, in progress):** see [peak-mountain.md](peak-mountain.md).
> - **World:** every Peak 1 location is its own package (`tools/export_peak_world.py` -> `web/public/assets/PEAK1/<LOC>/` + `peak.json`); the core appends locations (`web/peak_world.inc`, append mode of the ordinary init functions) and gates every world query per track (`engine/world_residency.hpp`, default all resident: race events unchanged). `?course=PEAK1[&peakCourse=N|&peakMode=6|9]`.
> - **Streaming = the original state machine** (rows 0x442168 states 0..8, 22CEA8/22D088 from the connectors' Unload/Load trigger volumes via stage builtin 68, eviction +8 passes, one disc read at a time with the measured read times, activation at the Load trigger, path banks 12A340 at read completion, sky switch 22DE98, 0x535C08 at 22DF50). Browser data feeds in slices from a worker; the game waits (+0x1D0) if a location would become collidable without its data.
> - **Career:** CTM starts in the world (new career: Happiness plane drop; then the last station); Start = MCOMM (Return/Transport); Transport freeride list in PS2 order goes into the world; lodge door -> "Would you like to enter the lodge?"; RaceRideState gates (builtin 67) open the course's event; Peak 1 Race/Jam playable (web/peak-run.js: tiers, limits, splits, medals, cash) with the countdown clock HUD.
> - **Section activation (0x101B60) for the streamed world:** `tools/export_peak_sections.py` -> `PEAK1/SECTIONS/sections.json`; the core gates it by the rows (instances in the octree from read completion; 6->1/8->2 rescan; 5->7 drops the location's list; eviction resets its stage state). Its slot-1 programs are what kill the free-ride challenge reset planes (builtin43 != kind -> DeadNode); before, touching one gave "Wrong Way!" and a reset. Shared edits (additive): `engine/section_streaming.hpp` (`Activation::present`, `dropTrack`), `web/section_gameplay.inc` (PEAK1 hooks), `web/stage_script_gameplay.inc` (PEAK1 slot-1/3 programs run without a stage-world package; section ids carry the track in bits 16+), `web/stage_world.inc` (`browser_stage_track_reset`, `browser_stage_seed_node`).
> - **Checks:** `web/test-peak-world.mjs` (streamed Snow Jam == event package on 1200 probes; residency gating; whole peak), `web/test-peak-run.mjs`; gate `peak1-fr-aara1-glide` in `test-ps2-captures.mjs`: physics bit-exact 2773..3665 across the A -> A_ARA1 unload/eviction of six locations and the ARA1 read (3666: open 0.95 cm/s velocity step; bones/score not seeded for this baseline). Open items: see peak-mountain.md "Gaps".

> **Characters, follow-up: no-gap pass (2026-09-23):** details in [characters.md](characters.md).
> - **Lineups:** the original roster build 0x23A4F0 (Tricky picks, peak rival, 25-swap shuffle, cheat skins on the human's base) with its RNG (0x4C9548 seeded from presentation draw 130 of 0x4FF018) for every human; `web/lineup.js`, `tools/export_lineups.py`, 37 new lineup states (Snow Jam + Metro-City), anchor RNG rule, relationships 0x155E58/0x155BF0; held-out captures bit-exact. Between-event presentation draws are an estimate (values drawn per frame by unported effects).
> - **Grid spot / body scale per human:** `settings.original_event_start` (core, rebuilt): position/lift follow the scale, reverse stance the base; cheat composition verified on `characters/brodi-on-psymon/`. Sam: scale 0.96 (5'11" = Elise), Mac's template (`sam_settings`).
> - **FE preview:** NIS head/eyes/hands with morphs, IRR lighting, FE clips/cheer timing (`web/fe-preview.js`); race rider textures now PS2 texels (were 2x bright).
> - **Screens:** Setup Character, Rider Details, Rider Profile, Rewards room, Ubertrick Setup (uber preview), Options (Game/Controller/HUD/Sound/Credits/Enter Cheat, screen position), Load game (`web/fe-screens.js`, `web/fe-options.js`); Pro controller = INPUT2.MAP + late-spin modes (gate `pro-event` exact); speed units/HUD levels in race; `web/rumble.js`; reward videos (`tools/export_movies.py`).
> - **Equip Gear:** BOLT rules/texture rule 0x14B988/bit-exact binds (`web/wardrobe.js`, `tools/export_wardrobe.py`), lodge + Setup, shared outfit record, boards camera; default packages follow the texture rule.
> - **Speech:** Post_Selection/Customize via `web/rider-speech.js` (game-audio `speak()` routes these to the Events.evt requests 0x20BC/0x20BD for the rider's CHARDB id, not to a `<bank>_eng` file; verified in Chrome and Safari for Nate/Viggo (swap), Moby, Zoe, Psymon; test `web/test-audio-sfx.mjs`).
> - **Career inbox:** relationship + all other message kinds (`web/career-messages.js`).
> - **Rider switch freeze:** race model loads lazily (`ensureRider` at event load), FE preview prepared in `web/fe-preview-worker.js`, compiled before swap, roster prefetch, one skinning pipeline. Chrome and Safari switches now stay near the idle frame time (worst ~30-50 ms under machine load ~30; were 66-548 ms).
> - **Sam in the roster row:** silhouettes rendered from his model (`tools/export_sam_roster.py`), drawn with the strip/highlights inside the group.
> - **Open:** crash-controller rumble needs a core observer at 12D28C/12D6F8/12D8A0 (physics owner); MP must send `base` + outfit key (docs "Online races"); Sam's extra outfit packages (Sam agent); free-ride goal messages never fire (no free-ride goals in the port).

> **Uber tricks, Super Uber, Monster Tricks, stall bonus: PS2 parity with sound (2026-09-23, trick/score agent):** see [tricks-scoring.md](tricks-scoring.md#uber-tricks-super-uber-and-monster-tricks-ps2-parity-2026-09-23).
> - **New tooling:** `tools/ps2_audio_log.py` (`ps2_capture.py build --audio-log`, `local/ps2-capture/uber-capture.sh`) logs every PS2 SFX voice start (2906B8), speech request and arcade speech category, the pending-Uber sound start/stop, Tricky start/end, Uber commit and monster speech; `web/uber-audio-compare.mjs` (compare-ps2-capture.mjs `TICK_HOOK=`) runs sfx-game.js on the replay and compares them per tick. The comparer also gates the boost words (+0x2F8/+0x2F4/+0x2F0) on every capture.
> - **12 new gated pipe captures**, exact to the end incl. audio: uber-chain, uber-multi (two Ubers per air), uber-super, uber-super-expire, uber-bail, monster-housecat, monster-repeat, monster-smurphy, trick-stall. 179 scenarios, test-ai-racers and npm test pass.
> - **Monster tricks** were already detected by the port (11B1A8, "named tricks"); recovered table, unlock medals and FE order in `web/monster-tricks.js` (+ test-monster-tricks.mjs). The score-object oracle now seeds monster states.
> - **Fixed:** monster speech (Arcade_Uber 8) fired on every trick; AE_COMBO run count; landing reactions 1 (great trick 149778) and 3 (combo); the timer reaction 6 (one tick later, all-bone mask, control 0 only); sfx-game.js: Super Uber speech 29B738, Arcade_Bonus dispatch, meter fill ticks (never played), points tick gate, air-phase crash sound, per-frame audio reads of the previous tick.
> - **Follow-up (same day):** the "UBER TRICK = L1/R1 + Square" hint is drawn (web/trick-hud.js uberHint, test-uber-hint.mjs); the HUD pre-pass (orb/coil palette, flash phase, hint gating) runs one tick behind the slot draws, as the PS2 frames show (the gold-vs-orange coil); Career Highlights: the eight monster-trick medal stats (0x155420 personal bests of the score object run statistics, KO +0x128 via core `pair_knockout` = 10E468/119400 + meter 1.0) are recorded per rider in the career save and drive the Rider Details -> Career Highlights list (the PS2 frame's layout and texts); 3 more monster captures (monster-chembro, -finger11, -stoneage with a poked lodge Uber row, applied by compare-ps2-capture.mjs). 185 scenarios.
> - **Second follow-up:** Crow's Nest monster captures (monster-yellowcard, -deepsky, -swollen, -xexec; 11 monster tricks confirmed on the PS2) with the countdown beeps fixed (sfx-game.js: remaining % 60 and the GO beep); a six-rider knockdown capture (ko-attack: KO +0x128, popup 0x2C, meter 1.0 exact; compare-ai-capture.mjs compares the human score object/HUD/boost); the HUD update/draw order traced on the PS2 (draw -> update -> rider -> 117C28); Career Highlights drawn from the original 35car_stat LUI (tools/export_career_highlights.py, web/career-highlights.js), equal to the PS2 frame.
> - **Gaps:** ko-attack's post-knockdown pair separation is 0.005 cm off from 266 (rider-pair physics); BHP1 pointa pickup (world agent).

> **Rider physics/animation/camera/controls parity, end to end (2026-09-23):** `web/test-ps2-captures.mjs` now gates 166 PS2 captures plus 3 AI captures, 169 in all. It runs them in parallel (about 2.5 min) and, by default, to the END of each capture: physics bits, all 29 posed bones including hair, and the score object/HUD whenever recorded.
> - **Exceptions:**
>   - pipe-finish score 4497, peak1/crows-event-tuck score 2072, uber-super score 1746 and the setpieces/full bones/score limits: these belong to other agents (stage pickups and scripts).
>   - Closed later the same day:
>     - handplant-spring: the old capture had no shared-RNG words, so the landing variant (semantic 62, leaves 84/85/86) was drawn from zeros. It is replaced by handplant-spring-rng, which is exact to the end.
>     - score-uber 739: the hair (120378) now selects from the pose that the reset placement (11D660 -> 11EB98) builds.
>     - jump-tricks is retired in favour of jump-tricks16 (same baseline and script, 16 KiB records).
>     - The boost amount (+0x2FC) is gated again. In the air, control 0 only requests control 4, and the boost stop belongs to 12F730 on the next tick (tech-speedcap-groomed 421).
>   - Crash rumble: `AE_RUMBLE_IMPACT` (30) at 12D28C / 12D6F8 / 12D8A0 (owner+0xDFC = max(v0, a); exact against the recorded +0xDFC, e.g. mix-glide 648) and `AE_RUMBLE_SLIDE` (31) at 12D23C (owner+0xE00 = a, phase 2 only).
> - **Speedrun techniques:** 89 new tech-* captures ( spin timing, prewind/re-prewind, spinboosts, speed caps, landers, frame cancels, rail glitch, strong stance, free-fall slide, trip turn, select warp, out of bounds, finish bonus) plus jump-tricks16 and passive-inputs16. All match to the end; see [tricks-scoring.md](tricks-scoring.md#speedrun-techniques-ps2-captures-2026-09-23).
> - **Camera:** every DEFAULT_3 word and every compositor word is compared on every tick in 9 new cam-* captures (Near, Mid and Far, 3 courses). The event camera is seeded with the countdown-anchor words, which removed the 2.5 cm event-start offset. Two input fixes: the landing-crash predictor restart and the crash surface. See engine/CAMERA_RECOVERY.md "Word-level PS2 parity".
> - **Fixes this round:**
>   - 1210B0 reset timing (immediate motion 3; re-request restarts the progress).
>   - Crash-air timers.
>   - Finish-line trick bonus 1194C0 with `AE_ARCADE_BONUS` (29).
>   - A settled pad history (first-sample press edges).
>   - ARA1 endmode colliders are inert, as on the PS2. They had made long runs throw core errors; see obstacle-collision.md. The new `web/test-long-runs.mjs` guards this.
>   - Keyboard Simple mode roles (D-pad spin + opposite stick).
>   - Earlier the same day: EE denormal flush (FTZ/DAZ), the pending animation rate, controller wind for hair, and prewind branches.
> - **For other agents:**
>   - The stage reset plane at setpieces/full 12199 and the pointa pickup at pipe-finish 2862 belong to the world agent.
>   - The camera shake is exact given the same visual RNG (0x4FF018) state. The PS2 draws about 10 visual numbers per tick, and stage effects must draw in the PS2 order.
>   - The freestyle celebrate rule: rider+0x100 is cleared at 0-based place >= 3 (race) or > 0 (freestyle).

> **Computer riders leave trails (2026-09-23):** the computer riders' cores already built the original board trail, wake and snow particles each tick, but only the human's were drawn. web/opponent-fx.js adds one set of the human's renderers per computer rider: board-trail.js, wake-renderer.js, and snow-renderer.js without the start fire, with the snow registered in the encoded composite. It is created with the AI race, so the loading warm-up covers it, and updated after the gameplay visibility pass, only while the computer riders race. Verified in Chrome: all 5 riders have 1410 ribbon vertices and 118-228 snow particles, the grooves and spray are visible, and frame times stay at 60 fps.

> **Online multiplayer complete (2026-09-23):** see [multiplayer.md](multiplayer.md). `cd web && npm run online` (LAN/dev) or `npm run online:serve` (one process: hosting build + server).
> - **Packets ~1.9 KB (was ~10 KB):** the rider's world pose (`world_pose_bones`) + body scale; the receiver rebuilds the skin palette **bit for bit** (`web/net/pose-codec.js` ports software_float + 310120/3106CC/386BD0; every tick of a recorded run matches the sender core, Zoe and Psymon). Plus pair view, lighting inputs, 3 FX records (delta coded).
> - **Rider vs rider:** each client runs the original race world for its own rider (`web/net/pair-net.js`): remote riders are dead-reckoned ghosts in 0x107888 (`rider_host(2)`); each side applies its own half of a contact; attack hits go to the victim (`attackHit` callback → packet → `race_world_pair_respond`). Shared world events/triggers are replayed as between the AI cores.
> - **HUD/results:** ranking 0x10F998 as an identical pipeline on exact shared packet ticks (all clients agree), original place display; original Single Event Results with every racer (times, 0x122D78 estimates, DNF), server final order, time-up 90 s after the first finisher.
> - **Remote riders:** interpolated pose 100 ms back moved to the dead-reckoned present; own lighting (`shade_external_rider_lighting`); effects via a puppet core per remote racer (`web/fx_puppet.inc` + `web/net/remote-fx.js`): track, wake, boost, sparks and snow particles equal the sender's bit for bit given its pose. No name tags (none in the original).
> - **Robustness:** late join waits, slow loader DNF, host migration, leave/quit DNF, auto-reconnect into the race, reload = DNF, version check, rate limits, clock-sync window + race-clock pace, no pause online + worker ticker for hidden tabs, static serving (ETag/range), Dockerfile.
> - **Core:** new exports only (`rider_skin_scale`, `race_world_pair_disable/_respond`, `shade_external_rider_lighting`, `fx_*`), `fx_record()` at the start of the FX pass (records only while online). Capture gates exact, test-ai-racers unchanged.
> - **Verified:** node tests `test-mp.mjs`, `test-remote-riders.mjs`, `test-mp-pairs.mjs`, `test-mp-fx.mjs` (npm test); two Chrome contexts through a full Snow Jam race to the results and back to the lobby; network drop + reconnect; production server build; Chrome host + real Safari joiner (hidden Safari window keeps racing).
> - **Closed later the same day:** contact reactions draw from streams keyed by (race seed, contact tick, pair, rider) -- deterministic, the victim's reaction equals the attacker's prediction; lag compensation predicts the ghost's half of a contact until its client's packets show it (200 ms: path divergence 209 → 32 cm vs a zero-latency reference, `test-mp-latency.mjs`); boost pickups arbitrated first-take-wins with `pickup_revoke`; server finish plausibility (clock, trace, motion, resets, distance; real runs pass, forgeries caught, browser race `verified: true`).
> - **Any rider online:** profiles carry a cheat skin's base rider and the worn outfit (`outfitKey` → `remoteOutfitRider` builds the same package on the other client); every racer starts on its slot's spot for its own body scale and stance (`web/net/grid-seed.js`, the five anchor computer riders rebuilt bit for bit on both courses); leaving an online race restores the rider's own grid seed (core `human_grid_seed('')`). Checked with Zoe in an Equip Gear outfit against Brodi on Psymon in two browsers. The cheat-skin scales no computer rider has (0.7 Canhuck, 1.2, 1.3, 1.5, 2.0) now have PS2-recorded slot spots (`tools/export_grid_scales.py`: 0x23A668 patched to put one skin in every slot; `grid-scales.json` for ARA1/BRA2); every rider x skin-base x slot x course starts on a recorded spot (`test-mp-grid.mjs`).
> - **Limits:** contacts are close to, not equal to, the zero-latency result under latency; reaction words differ from the PS2's single world sequence; a physically plausible forged stream is not detectable without server-side simulation.

> **Sam's outfits in Equip Gear (2026-09-23):**
> - **Items:** the tops Midwest Unc, Sunday Unc (Packers), Uphill Club and Lodge Legend, and the back kits Catch & Release (net) and Packed for the Creek (rod kit). All are owned from the start. They equip and unequip in Setup Character and the lodge (one shared record) and carry into the race.
> - **Packages:** 12 whole-outfit packages (`RIDER_SAM[_PACKERS|_UPHILL|_LODGE][_FISHING|_FISHING_TUBE]`), each with an `fe/` preview copy.
> - **Where it lives:** `tools/sam_mesh.py --all`, `tools/build_sam_web.py --all`, `config/characters/sam.json` `equip_items`, `sam_character/gear-names.json` (names, price 0, icons su03..su06 from `tools/sam_icons/render.mjs`), and `SamWardrobe.cs`/`SamTextures.cs`.
> - **Changes in shared files:** `tools/export_wardrobe.py` copies all `su*` icons; `web/test-wardrobe.mjs` expects the Sam tops list with Sunday Unc; `web/package.json` gains `test-sam-model` + `test-sam-gear`.
> - **Gaps:** the PS2 disc has no model variants for the new items; `earn_your_turns` has no item.

> **Sam model: roster density and fit (2026-09-23):** see [sam_character/model/README.md](../sam_character/model/README.md#third-pass-roster-density-and-fit-2026-09-23).
> - **Audit (browser, beside Zoe/Mac/Moby and the Snow Jam computer riders):** photographic 1254px maps, a 30 cm flannel check, tan and flannel lit almost white, a bare neck column, source hair spikes behind the cap, a projected egg-shaped face, a board 9% short and 17% narrow, flat white bindings, 329 weight groups, floating accessories, a rigid (non-swaying) hair and a printed label from the source jacket.
> - **Rebuild:** five original-size painted maps (`tools/sam_textures.py`) and derived parts on their own UVs and weights. A sculpted head with its own mirrored unwrap and painted head map (`tools/sam_head.py`). Hair clumps on `sec_dangle_l/r` (the original SH_* channel sways them). PS2 LODs (`tools/sam_lod.py`, `lods.json`) and `parts[].ps2_family`. Derived exports go to git-ignored `local/sam-model/`. `tools/build_sam_web.py` packages the web copy, compiles the skin and refreshes `RIDER_SAM/fe`. The rig and bind matrices are unchanged. At the roster's new 0.96 scale nothing in the package needs changing, because the board and bindings scale with the body like the originals.
> - **PS2:** `tools/sam_ps2` reads the new package (`SamSource.cs`) and was compiled with `local/dotnet`. Maps are written in the PS2 texel domain (half range); they were drawing twice as bright. Sam's default headwear row carries his hair (Dangle, file 28): Mac's default equips a hat, not the hair. The preview's Hands NIS item gets its own `sam_hands_nis.mpf`. The new derived disc is `local/sam-ps2/roster/sam-density-v5-test.iso`: the playtest source with only `BOLTPS2.DAT` and the two Sam archives replaced. From a fresh boot in ARMSX2 it shows Sam on Select Character with hair, gloves and roster brightness, then goes through Setup Character and into a Snow Jam race (`local/ps2-capture/menus/density-v5/verify.*`; head views at front, 3/4, side and back in Equip Gear, `density-v5/{sam,mac,zoe}h-*`). It is not installed in Downloads.
> - **Checks:** all pass: the new `test-sam-model.mjs`, test-rider-bind/skinning/frame, check-camera, test-characters, test-opponent-riders, test-original, test-rider-lighting, test-crash-*, test-start-animation, test-event-start, test-board-trail, test-rider-attributes. The Sam palette count is 171 in test-rider-skinning and check-camera.
> - **Head:** the side and three-quarter regions and the ears are painted from Sam's three-quarter photo (a landmark-fitted projection onto the sculpted head, shading halved, skin/hair only), and the cap back has its strapback (`tools/sam_head.py`, `tools/sam_textures.py`).
> - **Gaps:** none known for the model.

> **Every character selectable, classic Select Character screen (2026-09-23):** see [characters.md](characters.md).
> - **Roster:** the ten CHARDB riders in the original order (0x440F68), Sam as the eleventh entry (as in the Sam PS2 build), and the twenty cheat characters (ids 10..29) as skins on the chosen base rider, picked in Rider Details > Cheat Characters (131cheat_char). `web/public/assets/riders.json` comes from `tools/export_roster.py` (names, cards, bios, DNA, FE clips, unlock help/price, the original Enter Cheat codes checked against 0x187D38).
> - **Evidence:** a derived savestate set per character in `local/reference/pcsx2/characters/<id>/` (ps2_navigate from `character-selection.p2s`; cheats with the unlock bits poked in a copy). Zoe's derived countdown extracts identically to the reference anchor.
> - **Packages + settings (`tools/export_characters.py`):** the live human assembly (LOD0 header match), the equipped textures found resident in EE RAM, live bind matrices, and `RIDER_<ID>/settings.json` = the leaf differences from Zoe of the export_npc_riders extractors (scale, pivot, contact legs, uber rows, stance, masks, hair slot) plus identity (channel-1 masks, pair weight). Zoe/Sam have none (bit-exact). Kept packages got only texture fixes: Zoe's boots and Moby's helmet. New native packages are in `local/assets/native/CHARACTERS/` (`local/assets/native/RIDER_MAC` stays the sam_mesh source).
> - **Core rebuilt** (`web/build-core.sh`): `init_animation` takes the channel-1 masks from `settings.original_rider_identity` (else Zoe's). `web/character-roster.js` merges the settings (and composes cheat skin over base), `main.js selectRider` applies them and the human's pair weight.
> - **Screen:** `web/character-select.js` + `web/lui-player.js` play the original 08sel_char (`tools/export_character_select.py`): intro, per-rider states, stats/ranking, arrows, snow, flash; the 3D rider unscaled at the original spot and camera, the FE idle `FE_GEAR_*` and the Setup cheer `FE_CHARSEL_*` from library.json's fe clips; cheat faces list. Unlocks: career rewards, typed Enter Cheat codes, `?unlockAll=1`.
> - **Checks:** capture gates (event-race, boardpress-rail, metro-event-race, three AI) exact, test-ai-racers unchanged, npm test passes incl. the new `test-characters.mjs` (all 31 packages + a cheat on another base).
> - **Gaps:** the computer-rider lineup still is the Zoe anchor's (a Psymon human meets a computer Psymon); per-character grid spot; FE NIS head/hands/lighting; Setup/Details stay simplified.

> **Audio menus, Now Playing popup, UI sounds (2026-09-23):** original Music/Audio (140audio, 142audio_pda), Edit Playlist (16radio, 143radio_pda) and Sound Options (141advsettings) screens from the disc LUIs (`web/audio-menu.js`, `tools/export_audio_menus.py`), the "EA RADIO BIG" popup (`web/now-playing.js`) and UI sounds for every menu; menu row -> radio mode 0,2,1,3. See [audio-menus.md](audio-menus.md).

> **Audio gain staging (2026-09-23, final):** the missing per-channel scales `288D18` (world: DJ 0.55, PA 0.43, ch5 0.8,
> BOARD 0.75, ARCADE 0.75, AMBIENT 0.5, CHARACTER 0.8 x per-speaker 0.8..1.0) and the SPU Gaussian interpolation for
> hardware voices (`web/spu-interp.js`) bring the browser within ~0.5 dB of the ARMSX2 recordings per group and at GO.
> Open (race flow, not audio): the browser starts the countdown right after load; the PS2 waits for Cross in PreRace,
> so the PA venue intro overlaps GO only in the browser.

> **Audio fixes (2026-09-23, later):** Safari silence at GO was NaN from the MicroTalk decoder: exported bank params
> are keyed '0x1a', so looped patches decoded their intro straight into the body. Fixed, and the decoder is now
> bit-exact with the original (EE FPU order and rounding, short-intro behaviour, unbounded PCM patch), verified on all
> 812 streams by `web/test-microtalk.mjs` against the recompiled decoder (`tools/test_microtalk_native.py`); MicroTalk
> banks decode in `web/audio-decode-worker.js`. Gain staging from ARMSX2 recordings (docs/audio-logic.md 2.1): SND
> master 115/127, default sliders 10/11, linear law; pause silences voices (no DC); SPU 48 / IOP 8 voice pools.
> Pass/hit friend-foe use the live relationship records (`aiRace.racers.relation`, before the contact's 155BF0
> update); the pass gate counts 600 ticks from GO; medal-run PA = CTM round 3 (race/freestyle).

> **All game audio (2026-09-23):** sound effects, speech, crowd, world sounds, painters and the big-air duck now
> follow the original exactly (docs/audio-logic.md section 9). The core posts a compact audio event queue
> (`web/audio_events.hpp/.inc`, observers only, capture gates exact) that `web/sfx-game.js` dispatches for the human
> and every computer rider: board loops (A/B/C, surface classes, doppler-overwritten pitch as on hardware),
> take-off/landing (land.bnk), crash sounds, slide loops, grunts (GRNT banks), boost/grab/Uber/handplant, HUD sounds
> (points tick, pending Uber, Tricky, fill ticks), countdown/GO, overtakes, rider pairs, animation-event sounds,
> air whoosh and the DUCKTOLOOPS music duck. `web/sfx.js` plays every layer with the patch envelopes/LFOs/pan and the
> 30 m distance law. Speech runs the original Events.evt interpreter (`web/audio-speech*.js`): rider lines (big air,
> tricks, wipeout, pass, hit, whooh, collisions), arcade speech, DJ and PA categories with priorities, the 180-frame
> request table and the DJ duck. `web/audio-crowd.js` plays the CROWD.INF .eam reactions and emitter-fed crowd loops;
> `web/audio-world.js` the course emitters, instance contacts, location ambience (bank slots 8/9 from the world
> data), named banks and stage-script sounds; `web/audio-painters.js` the Mix (Metro-City mix 2, Happiness mix 1),
> MusicTrigger and Ambience painters. Podium chartune by the winner's character, restart rules, radio modes and
> settings for the audio menus. ARMSX2 answered the open questions (288AE0 = instant replay, 0x535C10 kinds, slot
> 8/9 loader, board pitch, chartune order, crash loops, whoosh). New exporters: `tools/export_speech_events.py`,
> `tools/export_world_audio.py`, `tools/export_animation_audio.py` (in `npm run setup`). Test: `web/test-audio-sfx.mjs`.

> **Sound, first playable version (2026-09-23):**
> - **Decoding:** everything plays from the disc's own data, exported compressed by tools/export_audio.py to web/public/assets/AUDIO (1.3 GB, git-ignored) and decoded in the browser (web/audio-decode.js, codecs ported from SNDDRV.IRX and the EE).
> - **Music runtime:** web/pathfinder.js is EA's Pathfinder music runtime (song graphs, events, intensity branches, loop overlay).
> - **Mixer:** web/audio-engine.js has the 11 original channels, the MIX.INF mixes, the DJ duck and the slider groups.
> - **Game-audio flow** (web/game-audio.js, per docs/audio-logic.md):
>   - front-end theme with a section per screen;
>   - the loading-screen loop;
>   - on load, a song picked by the original rule plays its idle section, with the PA venue intro;
>   - GO releases it, with DJ Atomika's Radio BIG and artist intros;
>   - race intensity ramp;
>   - pause freezes the music; the finish plays the ending; quitting stops it.
> - **Verified:** in Chrome, keypress unlock → menus → Snow Jam race.
> - **Not yet:**
>   - (all done since, see "All game audio" above).

> **Safari measured, computer-rider world in a worker, Classic keyboard default (2026-09-23):**
> - **Measuring Safari:** real Safari runs through safaridriver with the in-page probe `index.html?perf=1` (window.__perf: frame times, errors, screen changes, and performance marks `ai:*` / `warm:*`). Use the WebGPU backend.
> - **Warm-up:** each warm frame now draws only its new slice, then one full-scene pass follows. The sky compile runs alongside the slices.
> - **Computer-rider world:** the shared world template is built in `web/ai-world-worker.js` (the same `buildWorldTemplate` as the in-thread build in `web/ai-world.js`; in Chrome the image is byte-identical, 134 MB, 0 bytes differ). If the worker fails, it builds in-thread.
> - **Safari results:** 60 fps median through the race. Loading stalls left are about 150-250 ms in the human core's world init and 350 ms for the one-time post-graph node build.
> - **Keyboard:** the default mode is now Classic (web/pad-input.js). Simple remains an option.

> **Native wasm exceptions, loading-screen warm-up, stage VM (2026-09-23):**
> - **Core build:** web/build-core.sh now uses `-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=1`, replacing JS-emulated exceptions. Every call inside a try region used to go wasm -> JS `invoke_*` -> wasm. The six-rider tick went from 4.5-5.5 ms to 2.5-3.1 ms in node. `getExceptionMessage` still works. The capture gates event-race, metro-event-race, boardpress-rail and the three AI gates are exact, and npm test passes.
> - **Warm-up:** main.js `warmupRender()` hides every drawable, then reveals it in slices during real frames through the post passes. A slice is at most 60 drawables or 3 new material variants per frame. It then draws until the pipeline cache stops growing, for at most 30 more frames. The loading screen keeps animating: Chrome WebGPU loading is down to about 50 ms frames, apart from the computer-rider `init_animation` calls (~100-150 ms each) and the one-time post graph build.
> - **Loading render:** the canvas no longer renders the hidden scene behind the opaque loading screen. `fogRenderer.compileAsync()` pre-compiles the world and sky passes.
> - **Perf hooks:** `?perf=1` exposes `window.__perfRenderer` and `__perfScene()` for the profiling scripts.
> - **Chrome in-race:** steady 60 fps.
> - **Safari:** untested so far. safaridriver needs Safari > Develop > Allow Remote Automation.
> - **Stage scripts:** handler programs now run through the full LUN VM (`engine/stage_script_vm.hpp`) instead of the straight-line subset, which skipped ~240 live trigger programs (branches, 0x27 pushes, register pushes). Details:
>   - Per-track globals tables are used, and each stage's global programs run at reset with builtins 37/38 only (the collectible set ctx+0x2C0).
>   - The builtins that return values are ported: 39 (human player, plus the 30B9A0 collectible award 119EF8 kind 3 with 500/1000/2000 by the course table peak level, exported as `collectible_award`), 52 (instance has an entity), 61 (current resource) and 77 (random float from the shared RNG).
>   - Shared-world event 6 now carries the human flag.
>   - In single events (0x535C11 != 0) the collectibles are dead nodes, which the countdown audit already reflects. Conquer the Mountain (0) should show uncollected ones: not done.

> **Computer riders use their own upper-body masks (2026-09-22):** the channel-1 bone masks rider+0x8C0/+0x8C8/+0x8D0 (11C298, 310CE8 bone lists 457A90/457B38/4A1090) were hard-coded to the human's 0x8000fffe/0x8000fff8/0x870. They are now per rider: `tools/export_npc_riders.py` exports `identity.upper_mask8c*` from the countdown savestate, and `npc_configure` sets `riderMask8C0/8C8/8D0` in web/animation_bridge.cpp. Psymon and Luther use bit 30, Allegra and Moby bit 33, and Griff bit 31 like Zoe and Sam. The exporter checks that the human's masks equal the defaults. `upper_request_info()[6..10]` exposes the masks, and `test-ai-racers.mjs` asserts them. The AI capture gates remain exact, and npm test passes. See [ai-racers.md](ai-racers.md).

> **Race-start performance (2026-09-22, lead):** (1) web/ai-racers.js builds the course world once (first computer-rider core, init calls yield to the loading screen), snapshots that core's linear memory and copies it into the other four cores, which then only run init_animation/npc_configure — computer-rider setup 3.5 s → 0.95 s of main thread; the three AI capture gates stay bit-exact. (2) main.js `warmupRender()` (ui.loadEvent adds it as loading work, once per page): compiles the whole race scene with view culling off (`renderer.compileAsync`) and renders 4 real frames through the post passes with every top-level object shown, all under the opaque loading screen — the first race frames no longer build node materials or GPU pipelines (was up to 3.7 s frozen on a cold shader cache, ~350 ms warm). Race start is now a steady 60 fps. Profiling scripts: scratchpad perf/race-start.mjs (Playwright + Chrome for Testing, long tasks, frame times, CPU profile) and perf/analyze.mjs. `ONLY=name,...` filters web/test-ps2-captures.mjs.

> **Six-rider race exact like the PS2 (2026-09-22):** the six rider cores now form one world in the original 0x128AF0 pass order (`web/ai-racers.js`): every rider's pose first (`animation_pose`), then each rider's 121750 (`animation_post`) with rider pairs 0x107888 dispatched from inside it (`rider_pair_point`, before the 13F358 clamp), then each 121818 (`race_end`); the human core drives this through `Module.riderHost` hooks, main.js is unchanged. Shared world entities (crashbags, pickups, log teeters, trigger contacts, section MultiSplines, stage triggers) are logged and replayed across cores by `web/shared_world.inc`. Fixes: WASM `std::sqrt` in the pair kernel (chop mode), departure motion switch at 13F2CC, landing crash before 13AA48 runs as a ragdoll, 137860 without a reaction, 115D48 rival flag +0x1C, passive landings keep +2DC, 13C140 rail post query without the normal filter (Psymon 1846), NPC 105398 near the route, finish 0x114CC0 turn-around, the 1st-place glow. event-race-ai, ai-idle and event-race-ai-pairs are now bit-exact for all six riders, the shared RNG, ranks and pair records to the end (pairs was human 285). See [ai-racers.md](ai-racers.md).

> **The Junction super pipe plays like the PS2 (2026-09-22):** the pipe behaviour is patch data the browser had hard-coded: rider+0x2D4 = patch flags (13D1B8; 0x20 keeps a vertical-wall lip launch at full speed in 114298 0x114A5C, the old "1.25x" gap; 0x10 heading boost 13C948; 0x2 ground reset 13F178) and rider+0x434 = patch location id (1218D0 -> 22E0E0; 11..13 half pipes in 13C948/114298: the push-off gap). Also fixed for every course: 1135B8 full predictor restart in 105D98/1057B8, 1162C8 held-jump latch after control-0 entry, 108388 soft collision gated on motion, 13AA48 after a touchdown push (106538), post-pose departure push, 1210B0 timer resets + 11A088 "Wrong Way!", finish control 10 from the controller slot. 11 pipe captures gated (6 bit-exact end to end incl. score, handplants on the coping, Ubers with a poked meter, tricks, full event runs to the finish). Freestyle event: core time limit 125228 (DNF at 2:00), freestyle HUD (flags 0x1530C016: standings, countdown, no progress meter), FINISH!/TIME'S UP banners and the "Final run / Nth place / pts" panel. The inverted landing probe (pipe-tricks 1361) was the rider query scope: 3342D0 only sees what 332DB8 collected from the rider query bounds, refreshed every third game tick; the browser now scopes the landing probe the same way (pipe-tricks exact end to end, `tools/test_rider_scope_landing_live.py`). The BHP1 crashbag roller is exact (pipe-uber to its end 1158 and the roller-watch capture `bag-bhp1/uber-bag`: a 105398 bounce on a departure tick seeds the flight, 13F2CC -> 1399E0). Gaps: stage-script point pickups and reset zones. See [locations](locations.md#the-junction-super-pipe-2026-09-22) and [career events](career-events.md#freestyle-run-in-the-browser-2026-09-22-super-pipe-agent).

> **Metro-City physics closed, area lighting live (2026-09-22):** all eight Metro-City (BRA2) PS2 captures are bit-exact end to end. metro-event-race was 1093 and is now 2318. metro-glide-carve was 1077 and is now 1544. metro-jump-tricks was 1492 and is now 1623. metro-air-tricks was 887 and is now 1821. metro-mix-glide was 1095 and is now 2020. Root causes (none was in the response arithmetic):
>
> - Completion table `0x456990` kinds 6 and 7 were swapped. Kind 6 = `104C38` plays 18/19/20 over the finished air rail entry through `312BD0`. The new `engine/animation_completion.hpp` has a 60,000-case oracle, `tools/test_animation_completion_native.py`.
> - A jump on a rail runs control 7's exit `132048`.
> - The air release `12E9B8` resets the default root for rail styles 3/4 and clears +0x328.
> - Soft control 3 tests rail attaches (`12E778` -> `106848`).
> - Tuck plus boost press selects semantic 8, because `131878` reads +0x2FC after `114130`. This is the "0.08 cm push-out".
> - A soft collision re-play inherits its fading copy (`108388` is an ordinary play).
> - Crash `105D98` restarts the predictor with `1135B8`.
> - Crash queries share the rider's +0x864/+0x868 contact caches.
>
> The rider Lighting painter (type 11: BPBRR/B/G/Y/2 bright banks by area, `2C0778` at rider+0x460) now runs in `web/environment_bridge.cpp`. The new capture `metro-event-race-light` (`--lighting`) matches the PS2 bank on all 2300 ticks. Snow Jam: no capture stops at a push-out. The "same class" case was event-race 1989 (`106F78`, already fixed). See [obstacle-collision.md](obstacle-collision.md#metro-city-contact-differences-closed-2026-09-22), [RAIL_RECOVERY.md 3.10](../engine/RAIL_RECOVERY.md) and [terrain-render-fidelity.md](terrain-render-fidelity.md#rider-lighting-painter-by-area-metro-city-2026-09-22).

> **Computer riders / AI racers (2026-09-22):** Snow Jam (and Metro-City) races now run the five original computer riders (Psymon, Allegra, Moby, Griff, Luther) and a full race is completable with correct standings. Each computer rider runs the complete rider pipeline in its own core instance (`web/ai-racers.js`, `web/ai-race.js`; `?ai=0` races alone), driven by the ported NPC provider 0x10A768 (now incl. control-7 rail producer 0x10AED8, oracle 5000 cases) instead of the pad; seeds from the countdown anchor (`tools/export_npc_riders.py` -> `<course>/npc-riders.json`). Shared world in `web/race_world.cpp`: 0x10F560 ranking/proximity/pacing (+0xEC bit-exact), phase-ordered shared RNG (two cursors per core), rider pairs 0x107888 (bumps/soft reactions/crash knockdowns across cores), shared set-piece triggers. New control 10 finish stop 0x12C678 for every rider (`web/finish_gameplay.inc`), place HUD 0x21E1B0 (`web/race-place-hud.js`, "2ND/6"), Single Event Results rows (0x238BF8 + 0x122D78 estimates), career `ui.cb.standings/lineup`. Verified with new `--ai-state` captures (`web/compare-ai-capture.mjs`, gated in `test-ps2-captures.mjs`): event-race-ai: human + Allegra/Moby/Griff/Luther bit-exact all 2400 ticks with the real AI RNG (no opponent emulation), Psymon through 1845; ai-idle: 4/5 exact 2600 ticks. Removed the host height failsafe in `core.cpp` (it teleported riders to the grid before the finish). See [ai-racers.md](ai-racers.md).

> **Impact/contact particles (2026-09-22):** every contact effect was traced (table in [terrain-render-fidelity.md](terrain-render-fidelity.md#impact-and-contact-effects-2026-09-22)): the 28B180/296088 dispatchers are audio only, crash/scenery/landing/rail-attach visuals all go through `111AA0` snow impacts. New: board sparks/glints/grind chunks RFX+0x470 `2DABC8` (`engine/board_sparks.hpp`, `sprk` on metal surfaces and rails, kept in the air after a grind) and the attack fist sparkle RFX+0xC70 `2F1150` (`engine/fist_sparkle.hpp`), drawn by `web/impact-fx-renderer.js` (`tools/export_impact_fx.py`). Fixed: landing snow impacts use \|v\| (10E910) not the normal speed; snow reads rider+0x438 (rail surface 10/9 now exported as `runtime_surface`). `test-impact-fx.mjs` checks three new watch captures (`runs/fx/`): spark kernel bit-exact, snow impact strength exact on 3,255 ticks.

> **Snow Jam moving set pieces (2026-09-22):** the six chairlift chairs (MultiSplineModifier 0x48F168, bit-exact vs PS2 on every tick of two race captures), 41 waving flags (0x34BCA0 grids), 324 UV-scrolling instances incl. the chevron fences, LiveComp players (start-gate doors at GO tick 181, searchlights, pinlights, ravens taking off, rock slide, snow crumbs; node matrices bit-exact vs 634 PS2 snapshots), spline pieces (raven flyby drawn; rocket/spintwin/dragon trail carriers) and the log teeters (AnimTeeter + RailModifier: rails answered through the moving log, attach force) now run in the core entity pass / `web/set-pieces-renderer.js`. Trigger volumes fire from the selected-contact log. A full-course PS2 capture (`local/ps2-capture/runs/setpieces/full.*`, finish at 12,297) inventories every set piece. Not drawn: the particle systems (rocket smoke, dragon fire, fireworks, fire gushes); computer-rider trigger contacts are not forwarded yet. Per location: Metro-City bins (section-activated) and the Junction traffic/blimp run from their own seeds (Junction traffic bit-exact vs 88 PS2 snapshots). See [set pieces](set-pieces.md).

> **Lodge shops + attributes in gameplay (2026-09-22):** Buy Attributes now feeds the original stat getters `0x1494C0..0x149208` through the new `set_rider_attributes` (`web/attribute_bridge.cpp`), which patches the ground, landing, air and grab stat fields the physics already reads. Defaults stay bit-exact: `test-rider-attributes.mjs`, and `test-ps2-captures.mjs` still passes. The lodge now has Rewards (`RWRDPS2.DAT`, original prices and per-peak lodges, with pictures), Trophies, Buy/Equip Gear (`BOLTPS2.DAT`: inventory, equip rules and buy list checked against the savestate and PS2 frames), Ubertrick Setup (table `0x45AEB8`) and song purchases. Goal and first-gold awards (`0x159CD0`) and collection bonuses are included. Tables come from `tools/export_lodge_shop.py`, rules are in `web/lodge.js`, screens in `web/lodge-ui.js`. See [career-events.md](career-events.md#lodge-shops-awards-and-attributes-follow-up).

> **Event loading screen (2026-09-22):** event loads now show the original "Basic Controls" load screen, GL.LUI 110ctrl_load, played from the disc's layout, animations and GL_1 art (`tools/export_loading_screen.py` -> `web/public/assets/LOADING/`, `web/loading-screen.js`, hook `ui.loadEvent`, used by career `begin()`, the event pick and `?autostart=1` during init). It adds a DualShock or keyboard (Simple/Classic) controls panel and one of the 15 original kT_FEHINT tips (rotation 0x245950). Minimum display is 7 s, then 98% until loaded, fade to black, no skip; the original picks the screen at 0x232E20 and holds it ~13.6 s on ARMSX2. See [loading-screen.md](loading-screen.md).

> **Framebuffer glare 36C790 / world painter type 6 (2026-09-22):** ported as `web/glare-pass.js` (run just before ScreenTint via `fog-renderer.js`, WebGPU + WebGL2, `?glare=0`, `ssxQA.glare()`). Painter: ctor `2BC830`, vtable `484DA8`, blend `2BD068` (w = weight^2), compare `2BDBD0`, reset `2BE140` (1,1,1,1,1,0,0); values (debug menu `249200`) Cutoff, Post-Cutoff Scale, Copy, Frame Source, Frame Blend, Blend Texture 2/3 -> gp+12E4.. -> ctx+6CD4 (`36C740`). Pass: skip unless some trunc(127.5*BT) != 0; 256^2 copy of the frame (`36B9D8`) x FrameSource, threshold `(Cd-Cs)*FIX>>7` (`36C188`), three 4-tap +-2-texel box downsamples to 32^2, composite `Cd*FrameBlend>>7 + L*BT>>7` (`36C398`). `tools/export_glare.py` exports every location (16 SDB records author type 6; ARA1 none, so Snow Jam is unchanged). Verified: all 40 original `36C790` GS packet streams (`tools/test_glare_pass_native.py` on Metro-City RAM) equal `glarePlan()`, painter blend bit-exact vs live memory (9/14 steps), GPU vs byte model 0 byte errors on both backends (`glare-gpu-test.html`), Metro-City frame block MAE 25.9/24.6/40.2 -> 22.3/25.2/31.0. `test-glare-pass.mjs` is in npm test. See [terrain-render-fidelity.md](terrain-render-fidelity.md#framebuffer-glare-pass-36c790-world-painter-type-6-2026-09-22).

> **Tricks, combos and trick HUD from the original score object (2026-09-22):** `engine/score_object.cpp` ports the score object *(rider+0x790) and its 44-slot HUD message bank. It covers per-tick 117C28 (combo clock, payout 117718, landing messages, spin dial), commit 11A228 (identity, repeat division, combo multiplier clamp((n+10)*0.05,0.5,2), popups), landing 119D40/10E910 (Uber tier progression), takeoff 119E38, trick start 119C98, bail 119B08 (-0.25), quick recovery 119BB0 (+0.1), reset 119368 and finish 1193E0. It is wired through `web/score_gameplay.inc`. PS2 captures now record the object and bank per tick; eight score-* captures are exact (in test-ps2-captures). `web/trick-hud.js` ports the 0x1E9A30 slot cases (277/277 draw lists match the original) and is drawn by ui.js; the HUD total (+0x198) feeds career results. Gaps: race/career popups 0x29+, recover bar approximate, BHP1 pipe unverified. See `docs/tricks-scoring.md`.

> **Other courses: Metro-City (BRA2) and The Junction super pipe (BHP1) load in the browser (2026-09-22):** `tools/locations.py` registers each location (identity from the ELF location/course tables); every exporter takes `--location` (Snow Jam output byte-identical) and `tools/prepare_location.py CODE` builds a course end to end, then `sh web/build-core.sh` (course event + glide seeds are compiled in and selected by `world_collision.json` `location`). Browser: `?course=BRA2` / `?course=BHP1` (`&rider=zoe&autostart=1`), course list in Select Event from `web/public/assets/courses.json`. PS2 ground truth from new headless menu driving (`tools/ps2_navigate.py`) and `ps2_capture.py` address discovery: `metro-city-*` and `the-junction-*` savestates; metro-event-start 689/689 and metro-glide-neutral 1499/1499 ticks bit-exact, metro-event-race through 1093; pipe captures exact until the pipe lip takeoff (PS2 launches ~1.25x faster, pipe air not ported) and the pipe push-off. All gated in `web/test-ps2-captures.mjs` plus `web/test-locations.mjs`. See [locations](locations.md).

> **Rail balance direction fixed (2026-09-22):** the original passes RailBalance (LStickR−LStickL) to `0x113F38` unnegated (control 7 `0x131E00`, Uber `0x1365DC`, soft `0x12E850`, prewind `0x12EC1C`), so stick left slides the rider to screen left. The PS2 has no rail balance meter. The browser negated it in all four places (`web/rail_gameplay.inc`), so the rider slid the wrong way; soft control on a rail now reads RailBalance, and a held jump on a rail reads PrewindSpin (the D-pad) for spin. New gated captures `rail-balance-lr`/`rail-balance-slide` are bit-exact end to end, with slide bones exact through the grind. Pad/keyboard left = channel 20, the same as a DualShock. See [RAIL_RECOVERY.md 3.9](../engine/RAIL_RECOVERY.md).

> **Conquer the Mountain / event rounds (2026-09-22):** Main Menu → Conquer The Mountain now runs the original career flow. The MCOMM hub leads to Transport (peaks, peak goals, race/freestyle/freeride lists with their original lock rules), then to the round objectives card, the race, and the standings. Races run Qualifier → Semi Final → Final (top 3 advance). Freestyle runs heat 1 → heat 2 → solo final against posted scores from `0x1453D0`. Medals come from placement, with platinum from `0x440E80`; cash from `0x4405A0` pays half on a repeat. Peak passes open through any goal, and the lodge has working Buy Attributes and Save Game. Progress is saved to `localStorage` (`ssx3.career.v1`). Single Event runs its final-only round through the same screens. Rules are in `web/career.js` (`test-career.mjs` in npm test), screens in `web/career-ui.js`, tables in `tools/export_career.py` (in setup). PS2 menus were captured with the new `tools/ps2_menu_capture.py`. Race placement needs the AI racers' `ui.cb.standings` hook; until then only the player is ranked. See [career-events.md](career-events.md).

> **Hips/rail contact 106F78, core sphere mask, clamp after contacts (2026-09-22):** event-race is exact for all 2399 compared ticks (through 2417, was 1988), passive-inputs through its end 808 (was 730), boardpress-rail 837 (was 820), boardpress-railjump 838 (was 824), air-steer-fb end to end (was 462). 105398 first runs 106F78 (`engine/rail_body_contact.hpp`, 100,000-case instruction oracle `tools/test_rail_body_contact_native.py`): the hips bone's Y axis against the rail splines (334680 mask 1), push to 50 cm, +55.56 cm/s along the de-tangented normal, 105D98 soft/crash reaction; PS2 notifications with ra 0x107554 are these. 13F488 and 13AA48 query only spheres 0/1 (13F178/139C88 set the body mask +0x28 to words 0x4A115C/0x4A1120 = 3), 13F358 clamps after 13F488/105398, and 1211F8 approaches the rail tolerance +0x25C off the rail too (ground rail re-attach). The passive-inputs 693 pose report no longer reproduces (pose exact through 730). See [obstacle collision](obstacle-collision.md#hipsrail-contact-106f78-and-the-core-sphere-mask-2026-09-22).

> **Keyboard Simple mode (2026-09-22, lead):** Options → Keyboard (Simple default / Classic, `ssx3.keyboard`). Simple: arrows/WASD are the left stick on the ground/rails and become the D-pad in the air (controllers 4/5); a direction held through the takeoff keeps steering (air adjust) until released, directions pressed airborne are D-pad spins/flips. Classic keeps IJKL as the D-pad. Only host key routing changes — the original input path (pad_tick) is untouched; gamepads unaffected (`web/pad-input.js`, `test-pad-input.mjs`). Also: Widescreen defaults to Anamorphic on widescreen displays when no choice is saved (`defaultWidescreen`), and web/build-core.sh now swaps core.js/wasm atomically.

> **Snow Jam race-event world membership (2026-09-22):** the event keeps five locations resident all race (streaming table `0x442168` state 2 / world manager location state 6): ARA1 (track 8) plus its connectors A_ARA1 (3) and ARA1_B (9), TRANSP and ASKY. The browser package now holds the whole event world: 2238 patches (132+1913+193), 3496 instances, 182 rails (822 segments), 133 glow sources; `tools/export_event_membership.py` audits the live octree and fails unless the package equals it. Event vs free-ride is runtime instance state: drawn = static `(flags&3)==3` (`22A5A0`) or entity draw (`0x356298` + flags&4; type-16/DeadNode draws are empty), so the 74 free-ride Big Challenge gates/flags/arrows are now hidden. Rail segments are numbered per track (`rail_bridge.cpp`). Crash/roller oracles probe the connector terrain (331 / 10,358 exact, 0 excluded); `tools/test_rider_scope_query_live.py` checks rider queries there through a refreshed `332DB8` scope. Texture-chunk streaming (`W+0x3F0`) is not membership and is not emulated. Set-piece hand-off: `event-membership.json` `dynamic_instances`/`runtime_created_instances`. See [obstacle collision](obstacle-collision.md#snow-jam-race-event-world-membership-2026-09-22).

> **Left-stick air adjust is exact (2026-09-22):** turning in the air with the left stick past ~90 degrees performs the original in-flight stance switch `0x135BE0`/`0x114DB8` (pivot turn, `0x115168` flip, new flight, total-spin wrap, clip 288), which was never ported (`engine/air_switch.hpp`, 60,000 oracle cases in `tools/test_air_switch_native.py`; core.cpp redoes the tick's air translation from the pre-motion state). Fading adjust clips 297..304 now keep reading +0x28C/+0x298 after landing, landing clips revive a fading copy (3128E8 a2 = 0), and the passive control-4 request tick approaches +0x1FC. Six `air-steer-*` captures are gated with body-bone exactness (`bonesThrough` in `test-ps2-captures.mjs`, also on jump-tricks/air-tricks/neutral-3000/mix-glide). See [ANIMATION_RECOVERY.md](../engine/ANIMATION_RECOVERY.md#left-stick-air-adjust-in-flight-stance-switch-fading-adjust-clips-2026-09-22).

> **Crash entry/recovery + departure pose + event hair (2026-09-22):** event-race exact through 1988 (was 898), air-tricks through 1117 (was 925), jump-tricks through 939 (was 664). `--sync-rng` now defers opponent RNG draws to the human's first motion-phase draw (899 landing-crash variant); motion 2 keeps +380/+3A0/+3B0 and uses rider+370; 131608 idle reset, 10E028/115B58 get-up reaction 314, 12E980 +1FC target, passive-departure +1FC tick, slow-crouch 22 input, still-hair request on tick 0. See [animation recovery](../engine/ANIMATION_RECOVERY.md#crash-entryrecovery-departure-windows-and-hair-in-the-event-race-2026-09-22).

> **Light glow halos, hidden helper instances, 36C790 (2026-09-22):** the original LightGlow system (kind-7 world lights, update `2E2B00`, 16x8 Z query `2E3130`/`2EC478`, draw `2E2868` -> `3781A0`: `shal`/`mhal`, half size 180/100 cm, rotated camera quad drawn twice, GS `0x48` at priority 7) is drawn by `web/light-glow.js` (`tools/export_light_glow.py`, `test-light-glow.mjs`, `?glow=0`). The 313 instances whose countdown runtime flag bit0 is clear (RaceRideState box, reset planes, collision/trigger/emitter placeholders, cameraflash) are no longer drawn (`prepare.py` -> `hidden_resource`); the RaceRideState box had tinted the whole start orange and hidden the halos. Countdown halo boxes vs PS2 MAE ~70/37/60 -> ~17/17/25, frame blocks 53/25/35 -> 30/22/28. `36C790` is the world-painter type-6 glare pass, inert on ARA1 (no type-6 painter). See [terrain-render-fidelity.md](terrain-render-fidelity.md#light-glow-halos-never-drawn-helper-instances-36c790-2026-09-22).

> **Board press is live (2026-09-22):** control 1 (right stick nose/tail press, BoardPivot, R3 ollie) runs the original `0x1161D0`/`0x12FC80` port (`engine/board_press.hpp`, `web/boardpress_gameplay.inc`). This covers every phase, the pivot and stance flip, the ollie, the full-depth crash, air `+0x330`, landing and rail entry into 26/34, control 1 on rails, animation kinds 13/14/15 and completion 9, and the exporter clips for 24–38. 1,360,000 instruction-oracle cases match. Ten ARMSX2 captures are bit-exact end to end, and three rail captures are exact until non-board-press divergences. Air-adjust triplets `+0x28C/+0x298` are now approached. See [attacks and board press](attack-boardpress-recovery.md).

> **Rail attach and grind are exact (2026-09-22):** in the original, `0x106848`'s requests switch immediately. `requestMotion(4)` runs `0x13AD20` (the `0x11FA10` bake and zeroes), and `requestControl(13)` runs the old controller's exit (`0x134CB0` for air), both before `0x115358` and `0x13ADC0`. The browser now follows that order and has several other fixes:
>
> - `0x13AF28`: vertical clamp is ±1000·dt; the entry offset pull always applies; f20 is 0.5 below 555 cm/s; the detach push points away from the rail.
> - `0x115358`/`0x115168`: the rebuild and negations run, and `0x311B48` turns the roots by −angle.
> - The class-10 entry clip is not replaced by cycle 18.
> - `0x13BFA8` runs every tick as motion 4's post stage.
>
> `rail-air-fence` is exact through 784 (was 672); 785 is a web-only instance contact. `handplant-rail` is exact for all 499 ticks (was 637). Both are gated in `test-ps2-captures.mjs`. New oracles: `tools/test_rail_motion_native.py` covers `0x13AF28` and the `0x106848` attach sequence, 60,000 cases each. See [RAIL_RECOVERY.md section 3.8](../engine/RAIL_RECOVERY.md).

> **Crashbags are live (2026-09-22):** the 16 authored dynamic instances load with their countdown runtime flags (static route); a rider contact runs crashbag program 49 (builtin0 Object entity + builtin15 RollerModifier 35DA70), after which the bag takes the entity route (bounds/matrix from the roller) and is simulated every tick before the rider by the original rigid body 35E850 with its sphere-tree world query 336850 (`engine/roller_modifier.hpp`, `engine/roller_world_query.hpp`, `web/roller_gameplay.inc`, drawn by `web/moving-instances.js`). Oracles: 245,000 roller + 275,200 world-query randomized cases, live lockstep and 10,246 live queries from savestates byte-identical. Carve is exact through its last tick 777 (was 604) and `bag/carve-bag` matches both rollers on all 344 roller ticks. See [obstacle collision](obstacle-collision.md#crashbags-scripted-instances-and-the-rollermodifier-2026-09-22). The 74 type-16 node entities (course-script flags without 0x20/0x40) now skip every collider from load: rail-air-fence exact through 788 (was 784).

> **Sun glow + lens flare, effects after fog (2026-09-22):** the original tWPIGD_Sun painter (type 9, `2BC910`/`2BD378`), sun object `2F4DB8`/`2F4A08`/`2F4690` with its 16x16 Z-readback visibility (`2EC478`), `sun1` glow (half 280) and nine `lens` flare sprites, GS `0x48` additive at priority 8 (after fog `36AC00`, before ScreenTint) are now drawn by `web/sun-flare.js` (`tools/export_sun_flare.py`, `test-sun-flare.mjs`, `?sun=0` to compare). PS2 tick 618 top-right block MAE 97.6/59.8/18.7 -> 2.9/2.7/2.7. Snow/wake/boost (priority 7) now blend onto the fogged world instead of being fogged. LightGlow halos (`2E2868`) still missing. See [terrain-render-fidelity.md](terrain-render-fidelity.md#sun-glow-and-lens-flare-2026-09-22).

> **Race-start route, air surface frame, departure order, PS2 screenshots (2026-09-22, lead):** the grid start now seeds the retained human route from the countdown-anchor savestate (`original_reset.event_route`, rider+0x490..0x4CC; the glide route made 0x4CC heading, hence groundForwardDrive's auto-boost, wrong) — event-start is bit-exact for all 689 ticks. Airborne motion keeps the departure tick's ground tangents +3A0/+3B0 (publish_motion no longer rebuilds them), copies +180 into +370 and zeroes +3D0 each air tick (0x139A80), and the pose root reads +3B0 directly. On a departure tick 114298 runs before the 13F358 speed clamp. neutral-3000 is now exact end to end; new `event-race` (40 s from the grid) exact through 898 (899 = landing-crash placement, agent working). In-race HUD clock is `hh:mm:ss` (0x21F81C, `hudRaceTime`). Camera audit with real PS2 frames: default is DEFAULT_3 Mid (0x1673F8/0x161950) and web frames overlay PS2 frames pixel-for-pixel; the "zoomed-in" feel is the 4:3 frame on 16:9 (see Widescreen). Tools: `ps2_capture.py run --snap N,...` saves PS2 screenshots (savestate Screenshot.png) mid-run; comparer adds `--sync-rng` (computer riders share the RNG; the gate uses it), FIELD_EXACT, BONE_SCAN, PROBE_TRACE, ROUTE_TRACE; `ssxQA.rider('zoe')`; `LANDING_STATES=` extends the 13A7B0 landing oracle.

> **Original Widescreen modes (2026-09-22):** pause Options "Widescreen" Off/16:9/Anamorphic (persisted, Off default) reproduces 0x228C08 -> 0x377950: 16:9 letterboxes 3D+HUD into lines 56..392 with GS scales x0.75/x0.75, Anamorphic renders a 16:9 stage (x scale 0.75, full height, HUD stretched); both keep the 4:3 vertical fov. Verified with patched ARMSX2 captures. See [engine/CAMERA_RECOVERY.md](../engine/CAMERA_RECOVERY.md#widescreen-modes-2026-09-22).

> **Instance-contact phase 105398 is live (2026-09-22):** solid surface -1 scenery (most authored instances) is answered by the original 105398/104E70/1057B8 instance-contact phase, not the 13F488 obstacle response. The filter-2 rule is now the original one (skip all surface -1 nodes). Port: `engine/instance_contact.hpp`, with 90,000 oracle cases in `tools/test_instance_contact_native.py`. Browser: `web/instance_contact_gameplay.inc`, which runs after the ground/air body response, in crash contacts and on rail leave, and also drives pickup collection. Carve is exact through 604 (was 407). Tick 605 hits a crashbag, a dynamic entity that is unsupported. See [obstacle collision](obstacle-collision.md#instance-contact-phase-105398-2026-09-22).

> **Handplant is live (2026-09-22):** Circle (C key) near any Snow Jam rail runs the original control 11/motion 5 (engine/handplant.hpp, web/handplant_gameplay.inc): cruise/natural-air/spin entries, balance, handspring and exits 3..6, launch, scoring (Handplant/Handspring). 548,000 instruction-oracle cases and 8 ARMSX2 captures match bit-exactly (the phase-6 rail handoff too, since the 2026-09-22 rail attach fix). See [handplant recovery](handplant-recovery.md).

<!-- Session 2026-09-22: PS2 ground truth, controls, camera projection -->
## 2026-09-22 checkpoint (read first)

**PS2 ground truth is now automated.** Rosetta is not installed on this macOS 27 machine, so the x86 PCSX2 in
Downloads cannot run; the user approved ARMSX2 (native arm64 PCSX2 fork, official GitHub nightly) in
`local/vendor/armsx2/ARMSX2.app`. `tools/ps2_capture.py` derives a hooked savestate: a pad-sample hook at the
consumed-pad history update 0x321298 replays a scripted 24-channel pad (`local/ps2-capture/scripts/*.json`), and a
log hook at the provider exit 0x128630 records per tick: command words, controller/motion state, rider
+0x100..0xB40, owner fields, cached world bones, DEFAULT_3 and outer camera, AI rider positions, and the collision
body (16 KiB records; readers take the size from the manifest). `--isolate` disables only human<->AI pair records;
`--camera-variant 0x3C/0x3E` swaps the chase driver. ARMSX2 reproduces the old PCSX2 checkpoints bit-for-bit.
`local/ps2-capture/capture.sh NAME FRAMES [BASELINE]` builds, runs headless (unique PINE slot per run) and compares.
`web/compare-ps2-capture.mjs RUN.bin --pad --zoe [--event] [--trace a:b] [--fields]` replays the same pad through
the production browser path (`pad_tick`) and reports first divergences, command-word mismatches, per-field state
differences, camera errors and pose errors (POSE_TRACE). `web/test-ps2-captures.mjs` (npm test) gates regressions.

**Fixed this session (all verified against captures):**
- Controls: the browser now runs the original input path (24 PS2 channels -> button history 0x321298 -> INPUT.MAP ->
  provider 0x127998 -> per-controller decode; `engine/original_input_provider.*`, `web/input_bridge.inc` pad_tick,
  `web/pad-input.js`). Gamepad Circle is Handplant (not brake), right stick is BoardPress/BoardPivot, original stick
  dead zone/response, Select = ResetPath, Start pauses, crash recovery is WipeoutRecover = Square.pressed (was held
  jump; `web/test-crash-recovery-input.mjs`). Keyboard: WASD/arrows stick, IJKL D-pad, TFGH right stick, Space
  Cross, Shift Square, C Circle, Y Triangle, Q/Z/E/X L1/L2/R1/R2, V R3, Backspace Select.
- Camera projection: DEFAULT_3 fov 0.7539 is the HALF horizontal angle of a 4:3 view (context+0x5930 GS scale
  272.65/318.09 px at 512x448); browser used it as a full vertical angle (1.6x zoom) and a 640/448 aspect.
- Near/Mid/Far chase variants (pause Options "Camera 1", original OVAMER strings) with exact per-variant constants
  from drivers 0x176B10/0x176E10/0x177110; verified with vtable-patched captures.
- Landing speed: passive takeoff (114298 charge<0) sets owner+0x14 = -1 so the next landing keeps full speed; rail
  exits run 13C5A0 (tolerance ramp) not 13F410; event start seeds owner+0x14 = 0; ground-focus tick is the absolute
  13C7A8 stamp (off-by-one changed re-jump launch speed).
- Rails: runtime spline flags (record+0x1C) exported; 11 set-piece paths (rocket/dragon/raven/spintwin) are no
  longer grindable (the rider used to grab the rocket path mid-air).

**Known divergences:** superseded. See the 2026-09-23 parity entry at the top: every gated capture is exact to the end except the documented cases. Visual: terrain uses GameCube lightmaps (duller than PS2); wake mound/start-gate FX differ.

Complete rim coefficient computation now matches10,000 full original389CB8 runs;
its rotation stage separately matches20,000 cases. Live environment/local-light
assembly and normal shader binding remain open. See
[rider-lighting-recovery.md](rider-lighting-recovery.md).

Original rider/object irradiance data recovered:45 named records/1,800 float words
match across PS2 IRR.DAT and GameCube irrngc.dat. ARA1 Lighting references resolve
to APBR1/APDK1/APTN1/AOBR1. Lighting painter blend passes20,000 source cases.
Normal-basis evaluation and live shading remain open; see
[rider-lighting-recovery.md](rider-lighting-recovery.md).

Zoe's full implemented animation/regression coverage is now in npm test:497 clips
at three sample times,15 grabs, trick names, crash recovery and four Uber grinds.
All pass; rail pose checks include her27th bone. Coverage scope is documented in
[browser-rider-selection.md](browser-rider-selection.md); this is not complete
original trigger/wardrobe/console replay parity.

Zoe is now selectable and playable alongside Sam. Her27-bone original assembly
uses the existing recovered Zoe gameplay profile; switching reinitializes animation
and disposes prior graphics resources. Original biography comes from FEAMER.LOC.
Full suite includes --zoe gameplay checks; browser round-trip switching and race
startup pass. Other riders, wardrobe variants and UI stats remain incomplete.
See [browser-rider-selection.md](browser-rider-selection.md).

The source-derived GPU fog compositor is now default. Scene depth selects the
recovered palette and encoded-byte blend; material fog is disabled to avoid double
fog. WebGPU and WebGL2 pixel tests pass (blend, depth indices, wrapping, caching),
and live startup/sky/race render correctly. Generic comparison: originalFog=0.
See [fog-painter-recovery.md](fog-painter-recovery.md). Console framebuffer parity,
full lighting and broader gameplay goals remain incomplete.

Fog final-pass packet verified in1,000 original executions: ALPHA1=1 means CLUT
alpha is scene transmittance (128 retains scene), with DECAL/RGBA T8H sprite
sampling. Byte compositor helper added. Intermediate depth preparation and live
shader hookup remain open. See [fog-painter-recovery.md](fog-painter-recovery.md).

ARA1 spatial fog is live. The final camera eye drives the recovered point/payload
lookup and transition driver once per camera tick. All nine payloads are covered
by camera-position fixtures; regular jump/landing fixtures check sampling/timing.
Full npm suite and production build pass. Native/browser trace now includes fog:
9,630 frames, 645 fields, zero mismatches. Original fog-mode shader, far-cap coupling
and other regions remain incomplete. See [fog-painter-recovery.md](fog-painter-recovery.md).

Fog transition driver now source-tested in20,000 cases; Fog comparison/default
reset also match in20,000 cases each. Spatial point/payload lookup and weighted
blend were previously verified. Live fog integration still requires caller
coordinates/scheduling and renderer hookup. See [fog-painter-recovery.md](fog-painter-recovery.md).

Fog painter blend recovered and source-tested in20,000 cases (all12 float slots).
Spatial region selection and smoothing remain open; gameplay still uses start fog.
See [fog-painter-recovery.md](fog-painter-recovery.md).

Snow Jam now hides the same 81 captured DeadNodes already excluded from collision.
The batch ownership test verifies 13,199 event triangles with no geometry/vertex/
color changes. This is a bounded DeadNode visibility correction; general renderer
eligibility, other event owners and stage VM execution remain incomplete. See
[event-instance-activation.md](event-instance-activation.md).

Snow Jam now applies the 81 verified DeadNode collision exclusions at event start,
using the original body and mode0/2 ray routing rules. Terrain/other nodes remain
unchanged; respawns preserve exclusions and event restart reapplies them. The extra
visual geometry is still present. Full stage VM and renderer eligibility remain
open. See [event-instance-activation.md](event-instance-activation.md).

Event ray routing now source-verified:262,144 mode0/2 flag/entity cases pass.
All81 captured DeadNodes skip body and both ray collectors. Mode2 assumes a
non-null entity on its dynamic path; mode0 checks it. Renderer eligibility and
event-state application remain open. See [event-instance-activation.md](event-instance-activation.md).

Event body-collision routing is now source-verified across131,072 flag/entity
cases; all81 captured DeadNodes skip the original collector. The event audit now
records each instance's static/entity/skip route. Runtime application and renderer/
ray eligibility remain pending; see [event-instance-activation.md](event-instance-activation.md).

Event-instance recovery: [event-instance-activation.md](event-instance-activation.md)
documents original2FC2C0,22,400 source conformance cases, and a verified Snow Jam
countdown audit with81 DeadNodes among3,052 instances. Render/collision eligibility
and stage VM integration remain open; no event geometry was hidden yet.

<!-- Latest browser camera correction: 2026-09-12 -->
Browser camera now reads the pose controller's filtered crouch (source15F710,
rider+220), rather than the jump-button charge that clears on release. See
[Camera recovery](../engine/CAMERA_RECOVERY.md#filtered-crouch-input-repaired).
Full npm suite, native/browser comparison and production build pass. Jump/landing
visual fidelity and the reported intermittent airborne reversal remain open.

> **Human finish UI connected:** Authored finish now opens a basic Single Event Results panel with immutable original finish ticks and Restart/Quit. Rank remains unassigned until opponents exist. Full browser tests and9,630-frame parity pass; see [event recovery](event-demo-recovery.md) for precise verification and remaining results/variant/roster work.

> **Human race start is live:** Normal browser starts use the original grid, zero timer/progress,180-tick countdown and control6 push-off. W/S/vertical pad controls anticipation. Sam remains the sole runtime rider; NPCs, original rider selection, results and map-variant activation remain unfinished. Full browser tests and9,630-frame parity pass. See [event recovery](event-demo-recovery.md).

> **Start animation ready:** Control6 entry/update and driver-kind8 semantic0 are recovered; fresh entry+18 neutral ticks matches the original countdown snapshot exactly. Shared graph has startPose and clip8192 lookup. Browser race startup/roster wiring remains pending; see [event recovery](event-demo-recovery.md).

> **Event-start recovery:** Verified original six-rider countdown grid exported to local/assets/native/ARA1/event-start.json; native control6 update now passes20,000 original instruction cases. Browser start/roster integration is still pending. See [event recovery](event-demo-recovery.md).

> **Latest additions:** Terrain base UV order corrected and applied; boost strips are visible with original textures and geometry. The active scope now also requires original riders/opponents, a real event start/finish/timer and event-specific map activation. Read [event recovery](event-demo-recovery.md). These event requirements are not yet implemented; the captured mid-race seed and human-only filtering are confirmed gaps.

> **Latest browser checkpoint:** Browser gameplay is the current delivery target. Original Uber grinds are connected (Q/Z/E/X while Tricky is active), including ordinary rail-exit contacts and light/crash handoffs. Full browser tests and9,030-frame native/browser parity pass. See [rail/Uber recovery](rail-uber-recovery.md) and the latest sections of [browser continuation](browser-demo.md) for coverage and remaining fidelity gaps. This is still an incomplete SSX3 port; older route/status notes below are historical.

> **Current browser work (2026-09-11):** See [browser continuation](browser-demo.md). User explicitly requires original SSX3 UI/animation/physics fidelity; invented browser presentation was rejected. Original clip/controller ports, timing tests and remaining trigger gaps are documented there.

# Agent handoff — 2026-09-09 (evening checkpoint)

> **Route change — 2026-09-09:** The user has now authorized resuming the GameCube engine route, preserving original game code compiled ahead of time with platform/graphics adapters. `gamecube/` is the active delivery path; `engine/` remains a preserved native research/playable prototype. First milestone: original menus and one playable run, not another full-engine rebuild. See [GameCube continuation](gamecube-continuation.md).


Read this first. The sections below "Previous checkpoint" are the morning handoff and remain accurate except where this summary supersedes them. The repository is a **native riding prototype, not a complete or perfectly matching SSX3 port**.

## What changed today (all in the installed app)

- **Hard crashes and recovery are live.** Inverted/hard landings (`13A14C` classification) and hard scenery impacts (`105D98`/`108388` dispatch after cruise/air body bounces) enter the original control 8 / motion 2 lifecycle: airborne ragdoll, sliding, `12CB68` recovery meter (hold jump), continuation/get-up clips, then back to control 0 or 5. Soft impacts enter control 3. Hazard surfaces, the direction-change accumulator, the recover input and crash reset clips raise `PrototypeRider::resetRequested`, which the app honours by respawning (`116120`/control 9 still unrecovered). See [crash motion](crash-motion.md) "Gameplay integration" for the exact bindings and the documented approximations.
- **Rail grinding is live.** `tools/import_rails.py` exports SSB kind-8 rails (`<AREA>/rails.json`, all five race areas). `engine/rail_motion.hpp` + `engine/RAIL_RECOVERY.md` hold the recovered query/attach/motion 4/control 7 code; `riding.hpp` attaches from controls 0/2/5 and passive air, runs the rail motion and control ticks, leaves via `13BFA8` to the air controllers, and restores stance natively (`stance_restore` is now linked). Rail cycle animations 18/19/20 are the recovered kind-5 driver (`0x104238`→`0x103CC8`, `engine/rail_animation.hpp`, `RAIL_ANIMATION_RECOVERY.md`, 30,000-tick oracle): three-way weight blends of `RS[_FS|_BS]_BAL_R_CYC / _FWD_CYC / _BAL_L_CYC` on rider+238 negated by the switch flag, exported into every rider's `animation-packets.json` and the exporter. Airborne entries 68/69/70 complete to semantic 3 (kind 6) and the über family completes to 19 (kind 8). The `RS*_INTO_FS_GRINDn / GRINDn_CYC/BAL/LAND / OUTOF_GRINDn` clips belong to the rail über trick (control 12, helpers recovered in `rail_animation.hpp`, not yet wired). Rotations map to the spin axis; transfers/trick identity/dynamic rails throw explicitly; rail scoring is recorded as `GameplayAnimation` events only.
- **Sky and fog.** `tools/import_sky.py` exports each area's original sky dome (ASKY..ESKY) and painted fog (world painter kind 15); `engine/sky_asset.mm` draws it camera-anchored before the world, the clear colour is the fog colour and distant terrain blends toward it at half strength (fog mode undecoded).
- **Lighting.** GameCube lightmaps now use a 1.75x gain (median texel ~90; unit gain left shadowed snow navy). Start-area orange chevrons/blue lane stripes are opaque ground patches in the source data; do not alpha-blend them.
- **Original chase camera.** `engine/original_camera.hpp` (+ `CAMERA_RECOVERY.md`, `local/reference/original_camera_oracle.hpp`) reproduces the PS2 DEFAULT_3 chase camera (velocity/direction filters, follow distance, vertical offset, FOV 0.9599·π/4, look-at height, jump splines, swing/pull/lock stages, compositor terrain lift and landing shake); four of five PCSX2 savestate ticks are bit-exact. `main.mm` and `scene_audit.mm` step it once per 60 Hz tick from `GameplayAnimation::afterFrame` (head bone = posed bone 0, terrain probe = collision raycast, snow RNG not shared yet) and render with its eye/look-at/FOV; the preview follow camera remains the fallback when the animator is absent. Rider fields +5A4/+5AC/+3C0 and the two trajectory vectors are approximated by defaults (see the camera doc's uncertainty list).
- **Crash snow.** `engine/snow_crash.hpp` (+ `SNOW_CRASH_RECOVERY.md`, oracle `tools/test_snow_crash_native.py`) recovers the `2E2260` BodySnow producer (emitter 9, cycling 30-bone table, gated by the impact buildup) and the `111AA0` crash impact trigger arguments; `gameplay_effects.mm` fires impacts from `10EB30`/`137860`/`137D18` (point/normal/surface recorded on the rider) and emits body snow every tick. Ragdoll scenery hits intentionally raise no snow.
- **Retro presentation filter.** The app renders the scene at native resolution into an offscreen MSAA target and presents it through a subtle post pass (softness sized to a 640-wide framebuffer plus faint horizontal chroma bleed). H toggles it; the choice persists in `SSXRetroFilterOff`. `SSX_AUDIT_RETRO=1` applies the same pass in `ssx3_scene_audit` renders.
- **Unconnected inputs no longer pause play.** Attack/handplant inputs log once and are ignored.
- **Animation player** gained crash/rail root-bake helpers (`presentRoot`, `previewRoot`, `scaledLocalRoot`, `offsetSequenceRoots`, `seekChannel`, mirror/rail-balance setters, detached-board override).
- **Telemetry** rows carry `motion_mode`, `crash_phase`, `crash_submode`, `crash_recovery`, `reset_requested`.

## Verification

- `ctest`: 24/24 (18 previous + `native_crash_recovery`, `native_original_rail`, `native_rail_grind`, `native_original_camera`, `native_snow_crash`, `native_rail_animation`). Python: 50/50 (`tests/`, includes `test_import_sky.py`).
- Headless renders checked in the scratch area for the start, mid-run, crash (air/slide/get-up) and grind frames.
- Installed `~/Applications/SSX 3 Native Preview.app` binary SHA-256 equals the build after the rail integration; not interactively verified in a window this session (headless only).

## Known approximations introduced today (verify before claiming parity)

Crash: `117948` boost-loss test represented as "boost meter > 0" (penalty -0.25); `+1C` clip scale 1; sliding contact depth from landing material `depth3`; control-13 entry leaves the physical transform; `30ECD8` preview does not draw the variant RNG; rider category B20 = 1, reset permission -1; rumble/score observers recorded only. Rails: rider+2FC boost level from the boost tier (0/.25/.625/1); balance stat = landing stat; `13BFA8` runs its contact phases every rail tick but its score/audio observers are no-ops; airborne attach re-entry follows the recovered attach test (re-attaching after a lateral fall-off is therefore possible). Presentation: lightmap gain 1.75, fog strength 0.5.

## Suggested continuation

Camera refinements (shared snow RNG for shake, untracked rider fields, algorithm blending/SPOKE), rail transfers (`1161D0` positive branch) and trick identity (`132620`), forced reset control 9 / motion 3, handplants/attacks, scoring, per-character stats. Keep every gap explicit.

---

# Previous checkpoint (morning)


Read this first. This is the current integration checkpoint; older recovery/status documents describe narrower or earlier stages. The repository is a **native riding prototype, not a complete or perfectly matching SSX3 port**.

## User intent and immediate priorities

- Build a definitive native macOS game with direct Metal rendering and native gameplay. No console CPU/GX compatibility layer in the final product. PS2 and GameCube discs may both supply assets and behavior; choose quality by evidence.
- Preserve independent L1/L2/R1/R2 tricks. The user wants improvements visible in **normal gameplay**, not more animation inspection tools or isolated physics demonstrations.
- Recent priority: original animation controllers/clips, snow spray, landing bursts, and tracks. These are now integrated in the playable ARA1 development start.
- Latest completed fix: holding jump while leaving a ledge, landing while still holding it, and releasing jump after already becoming airborne. See below.
- Sam is the user's cousin, supplied in `sam_character/`. A playable prototype model and outfits exist now; older docs saying “concept only” are obsolete. Keep original SSX styling. Facts: 5′11″, 160 lb, regular stance, Wisconsin/Midwest upbringing, modest snowboard ability, afraid of inversions, “chopped unc,” prefers uphill. Bio/content is in `config/characters/sam.json` and `sam_character/README.md`.
- User previously requested Astra/high subagent help and values fast, concrete playable updates; do not resume expensive broad research automatically. Do not create new Codex tasks unless requested. Do not create branches with a `codex/` prefix.

## Workspace, build and installed app

Workspace: the project root (this repo's checkout).

Active product: `engine/`. Root PS2 and `gamecube/` targets are **offline/reference research**, not the final app architecture. Native source uses AppKit/MetalKit/GameController and native assets; it does not run the original executable or guest-memory runtime.

```sh
.venv/bin/cmake -S engine -B build/metal-engine -G Ninja -DCMAKE_MAKE_PROGRAM="$PWD/.venv/bin/ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo
.venv/bin/cmake --build build/metal-engine -j8
.venv/bin/ctest --test-dir build/metal-engine --output-on-failure
.venv/bin/python -m unittest discover -s tests -p 'test_*.py'
```

- Built bundle: `build/metal-engine/ssx3_metal.app`.
- Installed bundle: `~/Applications/SSX 3 Native Preview.app`.
- Desktop shortcut: `~/Desktop/SSX 3 Native Preview.app` (symlink to installed bundle).
- Installed binary was updated and SHA-256 equality with the build verified **after the held-jump fix**. An already running process must quit/reopen to load it.
- Assets use the absolute compiled workspace `local/assets/native` root. This is not a self-contained distributable; keep the workspace in place.
- Install executable updates atomically (copy to temporary sibling, `os.replace`) so running processes keep their existing mapping. Copy changed bundle resources too when applicable. Verify installed/build hashes and shortcut target.
- Git currently shows source directories as untracked; do not clean/reset or assume they are disposable. `local/`, `.venv/`, `build/` are ignored and contain irreplaceable private working evidence/assets. No commit was made for these changes.

Controls: WASD/arrows or left stick; hold/release Space or Cross/A jump; Shift or Square/X boost; Q/Z/E/X = L1/L2/R1/R2; R respawn; P/Escape pause; V inspection camera; K reload. App defaults to ARA1 and offers Mac, Zoe and Sam outfits. ARA1 alone has a reference-backed `riding-start.json`; other areas have provisional initialization.

Last attempted live UI check was blocked by the Mac being locked. Do not claim the most recent installed build was interactively verified. Headless Metal gameplay renders passed. Use CUA for UI and obtain fresh state; old app bindings, emulator PIDs, dialogs and screen state are stale.

## Current integration map

| Files | Responsibility |
|---|---|
| `engine/main.mm` | Playable app, input, selected rider/outfit, shared animation/effects session, draw loop |
| `engine/riding.hpp` | Fixed 60 Hz controller/motion/contact/timer/event phases; original source state and callbacks |
| `engine/gameplay_animation.h/.mm` | Binds original ground/passive-air/grab/air animation decisions, shared RNG, rates, fades, leg weights |
| `engine/rider_animation_player.h/.mm` | Decoded original sequences, local/world pose, completion ordering, collision pose, retargeting |
| `engine/animation_asset.h/.mm`, `mesh_asset.h/.mm` | Original animation definitions/bindings; legacy Mac/Sam bind conversion |
| `engine/gameplay_effects.h/.mm` | Live board geometry, emission, particle simulation, Metal track/sprite rendering and lighting |
| `engine/board_trail.hpp` | Source six-band/54-slice track ring, gates, geometry and fade |
| `engine/snow_emission.*`, `snow_particles.*`, `snow_context.*` | Recovered snow emission/context/particle math |
| `engine/environment_lighting.*`, `environment_asset.*` | Original PS2 terrain CPU texture sampling/filter and native data loader |
| `engine/scene_audit.mm` | Headless Metal renderer using the same gameplay controllers/effects as app |
| `tools/export_riding_start.py` | Adds source animation/grab/trail/snow/environment initialization to development start |
| `tools/export_animation_samples.py` | Original semantic/variant/driver mappings; Zoe regenerated for gameplay integration |
| `tools/test_held_jump_gameplay.py` | Repeatable regression test for held ledge departure/touchdown and airborne release |

### Animation behavior

The app no longer chooses gameplay poses through clip-name heuristics or a manual clip picker. Original ground, passive-air, grab and air selectors choose decoded packets, sequence markers, blends, rates and completion transitions. Main and collision pose use the same animation player. `sampledLayers` and pose-time root/control state are frozen before completion/collision mutations; do not reconstruct the rendered pose from post-collision state.

Mac and Sam retarget original sampled layers onto their own bind rigs. **The ARA1 development start still uses Zoe's source controller/physics profile even when another visual rider is selected.** This is not finished character-specific gameplay. Source Zoe collision scale is .85; Mac/Sam render retarget scale is 1. Avoid coupling collision support to selected cosmetic rig.

Covered helpers include full grab FSM (normal/tweak/Uber definitions), weighted variants, partial leg IK, passive control4, ground selector and air-adjust driver11. There is also an offline 564-clip library (497 basic + 67 frontend) under `local/assets/native/ANIMATIONS`; decoding all clips does not mean every gameplay state that uses them is implemented.

### Snow, tracks and lighting

- Integrated source emitter families 0/1/2/5/6/7: trail, chunks, impacts, cloud; source authored profiles and raw textures, particle lifecycle and view-facing quads.
- Tracks use original board geometry, six bands, 54 slices, distance sampling and segment fade. Five source masking/depth passes are represented with Metal stencil. No invented polygon bias or generic ribbon lifetime.
- Effects run at fixed gameplay ticks through `GameplayAnimation.afterFrame`. Shared visual LCG `4A3AFC`: tracks before spray. Particle six-word RNG `4FF018` is separate from gameplay RNG `4FF030`.
- Board snow frame uses unit axes (source geometry+30); trail uses scaled axes (geometry+34). World conversion is source cm/Z-up → `(x,z,-y)*.01` meters/Y-up.
- App/scene render targets are `BGRA8Unorm`, not `_sRGB`: input texels/baked colors are already raw encoded values, and the GS blend arithmetic must not receive an extra sRGB encoding. Depth/stencil is `Depth32Float_Stencil8`.
- Live environment sampler uses original PS2 patch/base/light UVs and CPU texture texels, not the visible GC lightmap approximation. Use `originalAirTrajectoryState()` (current trajectory), not the earlier animation trajectory cache.
- All six areas have environment-lighting packages; only ARA1 has authentic playable starting state. Unknown nonzero-weight texture guard fetches throw `OriginalEnvironmentUnavailable`; effects retain last valid light color and log once. Malformed asset errors remain fatal.
- Original producer families 3/4/8/9 (rock/breath/kicker/body) and live wake entities are not connected. Do not claim complete FX or pixel parity.

## Latest bug fix: held jump over a ledge

User reported missing animation when leaving the ground before releasing jump. Reproduction originally failed with `Original touchdown control/manual continuation is not yet recovered`.

Changes in `engine/riding.hpp`:

1. Control2 jump release is recognized in the air, not only on ground. Source `12EAE4..12EB1C` skips takeoff/motion re-entry when already airborne: transition to control5 and its release animation without another impulse or trajectory reset.
2. Held control2 continues prewind/animation selection while airborne.
3. Source `13A594..13A5D0` keeps control2 on landing. Small ledge impacts preserve crouch/prewind; only impact speed below -1388.888916015625 cm/s requests the source landing animation. Existing manual/nonzero-style restrictions remain explicit.
4. Do not route control2 through passive4 or automatically release jump just because ground contact is lost.

Regression: `native_held_jump_ledge` uses the real ARA1 initial state and `ssx3_scene_audit`, checks no missing collision poses, preserves control2 on held landing, and checks airborne release to5 without an upward impulse.

Recorded evidence in `local/native-qa/source-gameplay/`:

- `hold-edge.json` → `hold-edge-result.json`: control2 at frame1, ledge departure84, held landing143, later intentional jump release221.
- `release-edge.json` → `release-edge-result.json`: departure84, airborne release to5 at101, landing144. Vertical velocity -16.06135→-16.37802 m/s across release (no second launch).
- Both complete300 frames with zero missing collision poses. Test builds its own temporary scenarios; private JSON files are supporting evidence, not required checked-in fixtures.

## Verification and limits

Latest code: the existing17 CTests passed after the fix, and the new18th held-jump CTest passed separately. All18 therefore passed on this revision. The46 Python tests last passed during the preceding animation/effects integration; no Python production logic changed in the held-jump fix.

Other recent integration checks:

- All15 nonempty shoulder-button combinations completed90 gameplay frames without unsupported-animation errors.
- Sam180-frame jump/grab: takeoff31, landing116, no missing airborne collision poses; source effects rendered.
- Mac braking and Zoe air steering completed with effects and Metal output.
- Evidence/build logs: `local/native-qa/source-gameplay/`; latest held fix logs `edge-ctest.log`, `edge-final-build.log`; prior final suite `ctest-final.log`.

Historical source-oracle proofs are documented with their helpers. Earlier14 single-rider endpoint and six-rider30-frame exact comparisons predate the latest joined gameplay changes; do not describe them as freshly rerun on this app build. The prior shared-world run stopped at Luther's passive4 transition around frame72. Main app callbacks now support more states, but `native_world` integration has not been established through that whole path. Short comparisons and helper oracles do not establish full-course or full-game fidelity.

## Remaining work / suggested continuation

Start from a concrete user-visible bug or missing behavior, reproduce through the app's session in `scene_audit`, inspect original call sites, integrate, add a focused regression, and update the installed app. Prioritize gameplay over another asset browser or broad helper-only research pass.

Known incomplete systems:

- Full crashes/recovery/reset, rails and attachment, handplants and attacks. Some standalone recovered helpers exist but are not wired into the gameplay lifecycle; do not replace explicit failures with fictitious success.
- Score/boost event commits and advanced trick awards: GameplayAnimation records requests; complete scoring/progression integration is absent.
- Full event initialization, per-character stats/unique behavior, NPC/shared-world completeness, races beyond narrow recovered paths, menus/progression/audio/saves.
- Camera and general terrain/rider lighting/presentation fidelity; some shaders and follow camera remain preview approximations. Track rendering is source-derived but full visual parity remains unverified.
- All source mappings need auditing before applying Zoe's newly expanded export behavior to every other original rider asset.

The original full-port goal remains unfinished. Do not mark it complete based on these focused updates.

## Private assets, source evidence and numerical rules

- PS2 ISO: `~/Downloads/SSX 3 (USA).iso` (your own disc image).
- GameCube RVZ: `~/Downloads/SSX 3 (USA)/SSX 3 (USA).rvz` (your own disc image).
- Original ELF: `local/disc/SLUS_207.72`, SHA-1 `77114dfd1205eaccf1ccc18c5f9650097fa78bd8`.
- Recompiled source evidence: `local/output/`; function files sometimes merge routines, so search exact instruction comments as well as filenames. These are offline research artifacts, not original high-level source or app dependencies.
- GameCube extracted files: `local/gamecube/disc/files`. Native assets: `local/assets/native`.
- Snapshot/oracle workflows: `docs/reference-harness.md`; preserve hashes and original snapshots. Original discs remain untouched. Patch only derived private oracle copies, with opcode guards. No publishing extracted proprietary data.
- Numerical rules are essential: EE/VU chop uses FE_TOWARDZERO; scalar EE DIV/SQRT use nearest; VU operations retain chop. EE scalar ADD/SUB use the source one-guard-bit helpers; do not apply those scalar rules to VU vector ops. Preserve `-frounding-math -ffp-contract=off` and source operation order. Zero VRSQRT saturates to `0x7F7FFFFF`, rather than generic normalization producing NaN.
- Do not feed recorded expected poses or trajectories into production simulation. Seed source inputs/state; evaluate native behavior independently. Requested emulator pad timing differs from game-accepted commands—use accepted traces for parity claims.

## Detailed reference map

- Gameplay overview: [gameplay-animation-effects.md](gameplay-animation-effects.md), [native-engine.md](native-engine.md).
- Animation: [ground selection](ground-animation-selection.md), [air selection](air-animation-selection.md), [air-adjust driver](air-adjust-animation-driver.md), [grab lifecycle](grab-lifecycle.md), [passive air](passive-air-control.md), `engine/ANIMATION_RECOVERY.md`, `engine/AIR_ENTRY_RECOVERY.md`.
- Effects: [board tracks](board-trails.md), [track passes](board-trail-render-passes.md), `engine/SNOW_RECOVERY.md`, `engine/ENVIRONMENT_LIGHTING_RECOVERY.md`.
- Motion/world: [terrain](terrain-contact.md), [shared world](native-shared-world.md), [historical parity evidence](core-gameplay-fidelity.md), `engine/LANDING_RECOVERY.md` and other `engine/*_RECOVERY.md`.
- `current-work.md` and `port-status.md` contain old translation/runtime research. They are useful history, not a direction to restore the compatibility architecture.


<!-- Camera investigation 2026-09-13 -->
Original landing latch/offset decay now has20,000 direct executable comparisons,
all matching. Reproducible camera/head relative jolts of0.50/0.56m occur at
charged/held fixture touchdowns773/782, where camera mode is grounded but head
comes from the pre-contact airborne pose. See engine/CAMERA_RECOVERY.md's latest
section; source frame-order comparison remains necessary before changing this
binding. web/check-camera.mjs --trace emits the replay records for either rider.

Camera follow-up: the original cached pose also survives same-tick landing/contact;
do not treat the mixed camera inputs above as a proven bug or resample the pose.
Complete original15F780 landing-angle getter now matches20,000 cases using real
asin, in tools/test_camera_landing_angle_native.py. Continuous original-vs-browser
camera comparison remains the next camera fidelity requirement.


Continuous camera milestone: tools/test_camera_sequence_native.py now verifies
121 real original-game updates from jump tick369 to490, including touchdown451
and40 grounded updates. All compared algorithm fields, eye and target match
exactly with state carried across the whole sequence. Source captures are in
local/camera-continuous/jump, hashes in jump-manifest.json. This isolates further
camera work to host inputs/initialization and compositor/rendering; it does not
establish browser physics or final-image fidelity. See CAMERA_RECOVERY.md for
capture/validation scope and the excluded earlier neutral capture gap.


Latest browser fix: charged release no longer advances crouch twice, and air
filters update shared physics state before landing so old crouch cannot rebound.
First jump press preserves source targets; launch reads retained rider crouch.
web/audit-camera-inputs.mjs asserts original crouch at122 captured checkpoints;
all match. Full npm suite, production build and rebuilt native/browser host trace
pass. Broader launch/pose fidelity is still open (see latest CAMERA_RECOVERY.md).


Takeoff follow-up: previousNormal now updates in the source ground-contact phase
(13D218), preserving historical normal through charged release/air. Added the
21-float jump_takeoff_info diagnostic and matched-fixture assertion. Launch error
remains and is now traced to already-different incoming velocity, not crouch or
previous-normal overwrite. See CAMERA_RECOVERY.md for measured inputs.


Latest physics milestone: preserving the authored reference spawn (instead of
raycast-snapping it) fixes the measured jump transient. All122 original jump
checkpoints now match motion mode/touchdown timing, with maximum position error
0.487mm and launch-value error0.000184cm/s. Predicted airtime now clears on landing.
Ground surface direction is retained across telemetry/force phases. Posed head
still differs up to17.45cm, so animation/head binding is the next camera lead.
See web/audit-camera-inputs.mjs and latest CAMERA_RECOVERY.md for exact scope.


Latest browser animation/camera corrections: original0.85 rider scale now applies
to pose, mesh/bind bones, pivot and broad collision radius; air landing-animation
selection reads pre-motion prediction;121750/310530 late geometry translation
moves cached rendered bones/head with contact displacement without resampling.
This fixes the measured7.53cm touchdown target lag: residual0.276mm, with head
within1mm at all captured frames60..152. Release still has~0.8cm residual.
Reference crouch/motion and <1mm rider-position assertions continue to pass.
See latest CAMERA_RECOVERY.md; full visual/animation fidelity remains unfinished.


Latest: charged-release presentation filters now run after source ground-leave
targets, fixing the remaining takeoff lift/board-alignment timing. Both rider
position and posed head stay within1mm of all122 captured original jump states.
Point-light kind2 conformance now passes12,000 cases; its previous failure was
an inline host-infinity branch left in the local reference oracle. Live rider
lighting/shader integration remains next, not complete (rider-lighting-recovery.md).


Lighting progress: renderer slot228 resolves3954D0, copying rider coefficients to
renderer+6BB0;396B40 uploads ten VU rows. Added irradiance_evaluate.hpp for program2
255 preprocessing, normal polynomial, clamp and FTOI0 color conversion. All12,000
original VU comparisons pass for float and byte outputs. See rider-lighting-recovery.md
for exact scope: draw/program selection, normal/material mapping and live GPU
binding remain unfinished; no gameplay lighting change is claimed yet.


Rider lighting follow-up: audited renderer callbacks31C->37A610,37C->386BD0,
3E4->396B40 confirm the geometry draw reaches coefficient upload. Added original
ITOF15 normal decode plus supplied-matrix transform;12,000 combined original VU
normal/lighting/quantization comparisons pass. Exact per-part material/template
selection and live GPU binding remain the next lighting work.


Rider material finding:37A94C explicitly selects TEX0.TFX3 HIGHLIGHT2, TCC1 in all
9 audited Zoe material batches. RGB=texture*lighting/128 + lighting alpha (rim),
clamped; output alpha comes from texture. Added native rider_texture.hpp and an
exhaustive16,777,216-case check against the source-extracted GS software combiner.
Hair/TopB secondary batch select blend5; ordinary batches select blend1. See
rider-lighting-recovery.md; live GPU integration and full render state remain open.


Browser GPU lighting stage added: rider-lighting-nodes.js implements the verified
byte-domain HIGHLIGHT2 combiner. rider-lighting-gpu-test.html passes393,216 RGBA
pixel comparisons on WebGPU and WebGL2 using synthetic original-GS-reference
goldens. Build passes. This helper is not yet connected to live rider materials;
coefficient evaluation/update, sampling and material pass state remain open.


GPU irradiance stage added: riderIrradianceNode evaluates/quantizes all ten rows.
Original VU references for12,000 cases match RGBA bytes exactly on WebGPU/WebGL2.
A one-byte float-rounding mismatch was fixed with integer mantissa arithmetic,
not an epsilon or weakened reference. QA: irradiance-gpu-test.html; reports in
local/rider-lighting/gpu-irradiance-*.json. Live per-vertex normal/material and
coefficient-bank updates remain unconnected; gameplay rider shading unchanged.


Live environment irradiance now updates in the browser from actual terrain ratio,
original brightness curve, authored ARA1 bright/dark banks and0.5 history blend.
APIs environment_irradiance (40floats) and environment_irradiance_info (10floats).
Normal rider branch only; final rim/local-light assembly and GPU material hookup
remain unfinished. One texture-lattice sampling gap at fixture445 is exposed and
preserves last valid coefficients. Explicit WASM rounding fixed in environment
sampling;60k original cases, full npm and9,630-frame/713-field parity pass. See
rider-lighting-recovery.md for initialization and scope limits.


Local lighting: exported190 authored ARA1 kind6 records, byte-matched captured
lights to asset IDs, and recovered2F5D30 ranking plus2F5B68 eight-slot ordering.
40,000 original comparisons pass. Three authored candidate scopes match fresh
original/native selection, but glide's retained list is older/different (3 vs2
at the captured current point). World query332DB8 and update-phase ownership
remain open; do not use a global nearest-light heuristic or claim retained-list
parity. See rider-lighting-recovery.md and selection-test.log.


Spatial-light groundwork:328360 padded region classifier now passes20,000
original comparisons across all return codes/boundaries. audit_light_query_scope.py
records active roots/bounds and continuous evidence that light4104 leaves current
point radius at371 but remains selected until373. No fixed cadence is inferred.
Full332DB8 traversal, candidate ordering and update-phase integration remain open.


Light candidate traversal now implemented in spatial_light_query.hpp: partial
Gray-code order vs full numeric order, source bounds and minimum depth.5,000
original randomized queries and three full-snapshot/pruned-tree checks pass.
export_light_tree.py produces an identical118-node/188-light Snow Jam index
across three snapshots, using only authored light IDs/topology. IDs8/264 are
explicitly unindexed. Runtime bounds/refresh timing, dynamic indexing and final
rider lighting remain open. See rider-lighting-recovery.md for oracle scope.


Rider query bounds now implemented/exposed:11E150 matches20k original cases and
all122 original-field snapshot boxes. Browser live bounds retain up to3.4mm input
state difference; no full source-state parity claim. Guarded disposable timing
probe logged light refreshes369/372/375/378/381 using each completed pose; selected
list change at372 appears in capture373. All720 rider words at384 unchanged.
Original emulator state restored and hook/code arena verified clean. Do not infer
universal tick%3 gating before26CC48/26CD20 job ownership is recovered. Full npm,
build and9,630-frame/725-field native/WASM comparison pass. See lighting doc.

Rider lighting assembly1220D8 now implemented in rider_irradiance.hpp;2,000
complete original comparisons pass all40 coefficient words, including non-null
ignored kind0/3 entries and optional controller lights. camera_transform.hpp ports
166F90/31B748/31B7A8 and395750:20,000 original cases pass algorithm matrix,
position/quaternion and renderer Euler view words. New test:
tools/test_camera_transform_native.py. Renderer rim view getter395C68 returns
renderer+13E4 matrix-stack pointer; Euler setter395750 is vtable slot10C, matrix
copy395C38 slot114. Next establish actual gameplay producer/stack/update timing
before wiring live rim lighting. These kernels are not yet browser material
integration; riders remain unlit. Details in rider-lighting-recovery.md.

Camera view continuation: originalCameraRenderView now ports15E968..15EAE8
(quaternion -> G axis conversion -> negative eye translation), passing20,000
original stage comparisons. Browser camera publishes camera_render_view16floats
from the final camera result. Full npm/build and9,630-frame/741-field native/WASM
comparison pass. Charged/held jump checks verify view origin/target for Sam/Zoe.
Source22B250/2D9270 copy selected camera+40 through renderer slot114, but the
stack at the exact rider-lighting call still needs confirmation; completed
savestates have identity at renderer+13E4, not evidence for the draw-time matrix.
No visible material change yet. See latest rider-lighting-recovery section.

Direct rider-view probe resolves the normal draw-time matrix source:1220D8 log
at ticks370..374 sees actual camera matrix at renderer61CB40/depth0, not the
identity present after rendering. Final view equals camera1597D10+40. All720
rider words at374 match unmodified replay. Four captured PS2 eye/quaternion/view
fixtures now pass all16 words in test_camera_transform_native.py, alongside20k
original cases. Tools:build_rider_view_probe.py, audit_rider_view_probe.py.
Isolated PCSX2 restored to unmodified jump31, paused; original prologue and empty
probe arena verified. Remaining lighting work is live local selection/refresh,
assembly at the correct draw phase and GPU material/normal binding. See latest
rider-lighting-recovery section; no new visible shading change in this step.

Authored light-world backend added: rider_light_world.hpp combines spatial query,
ranking, eight retained resource IDs and full shading; rider_light_asset.hpp shares
catalog/index loading across native/browser. Refresh and draw stay explicit,
separate phases. Three source query/selection scopes pass;20k ranks/20k selections
still pass;2k complete original assemblies also pass through retained-ID shading.
No automatic re-ranking during shade and no guessed tick%3 cadence.120E50 and
12B788 have no local refresh gate; timing belongs to26DBF0/job caller. Browser
material/runtime invocation still pending. See latest lighting doc and logs.

Browser lighting bridge compiled/exported and authored catalog/index loaded during
normal startup. New prepare-rider-lighting.py is part of setup; race start resets
retained lighting state. Explicit refresh_rider_lighting and shade_rider_lighting
APIs are available but not yet invoked for gameplay shading.192 native/WASM bridge
draws pass all7,680 coefficient words, selection IDs and phase/reset counters via
test-rider-lighting.mjs (now in npm test). Reference generator:
tools/build_rider_lighting_bridge_reference.py. Full npm/build and existing
9,630-frame/741-field comparison pass; browser startup/menu works. Visible rider
material remains unlit pending cadence/draw/GPU hookup; no completion claim.

GPU normal stage added:riderTransformNormalNode preserves original ITOF15 and
three-column VU transform with no renormalization. New rider-normal-gpu-test.html
passes36,000 float words on WebGPU/WebGL2. Source oracle emits20-float normal
records separately from existing irradiance goldens. Fixed shared chopAdd's
mixed signed-zero case;12k irradiance color comparisons still pass both backends.
Build passes. Matrix palette selection/blending and live material hookup remain
open; existing GC-derived rider normals must not be casually treated as raw PS2
normal data. See final lighting doc section and gpu-normal reports.

Skin palette386BD0 recovered in engine/skin_palette.hpp: integer percent weights
multiplied by original0.01f, then VU matrix accumulation with no normalization.
20k complete original calls pass all output words. Zoe source audit:200 groups,
all sum100; no large weight-sum error, but source rounding differs from current
Three skinning. rider_assets.py preserves raw integer weights; new
export_rider_skin_weights.py added source_skin metadata to installed native/browser
Zoe for3,058 vertices after source-hash/mapping checks. Existing skinning unchanged.
Zoe gameplay/build pass. Remaining: geometry+34/+38 input matrix ownership,
per-vertex palette selectors and GPU/live material integration. See lighting doc.

Pose matrix stage added in pose_matrix.hpp:310120 quaternion/position construction
and geometry+140 column scaling;310640 prefix pose*bind multiplication.20k calls
each match original, plus87 actual PS2 unscaled/scaled matrix pairs across three
states. Existing20k weight-palette cases still pass. Original geometry has29 slots
vs browser27 bones; map extra slots before assuming direct indexing.310530 updates
both unscaled/scaled cached translation columns. Bind-data producer/mapping and
GPU skinning/live material still pending; no rendering change yet. See lighting doc.

Resolved Zoe29/27 bone slots: inactive eye bones24/25; active hair maps browser
24..26 -> source26..28. export_rider_bind_matrices.py validates names/file IDs,
authored bind floats and static inverse-bind words across three original states,
then exports only static metadata (no live pose). Zoe now uses those inverse binds
in live browser Skeleton via rider-bind.js and explicit Mesh.bind matrix. Sam has
a different Dangle hair rig, so keeps its own bind path. New test-rider-bind.mjs,
full npm/build and live Zoe selection/race inspection pass. Still Three pose/weight
arithmetic and unlit material; GPU palette/shading/cadence integration remains.

GPU weighted skin matrix stage added:riderSkinColumnNode reproduces386BD0 using
raw integer weights and source0.01 conversion/accumulation.12k original column
records pass all48k float words on WebGPU/WebGL2 via rider-skin-gpu-test.html.
Native source palette/pose tests and87 captured matrix checks still pass; build
passes. Golden layout28floats/record, reports gpu-skin-*.json. Next feed live
source-space pose/bind matrices and per-vertex selectors into this stage; visible
renderer still uses Three skinning with original Zoe binds and unlit materials.

Live animation bridge exposes rider_skin_matrix_count / rider_skin_matrices:
Zoe27*16 original source-space scaled pose*bind floats from final cached world
pose, including late contact translation. Cached per animation tick; init/reset
invalidate; Sam lacks matching bind metadata and returns0/null. Jump/landing and
reset/repeat checks pass. Expanded native/browser comparison supports --zoe and
native RIDER_ZOE arg:9,630frames each, Sam742fields/Zoe1188fields, zero mismatches;
Zoe includes432 matrix floats per frame. Full npm/build pass. GPU vertex/material
hookup remains; do not double-scale or reapply object transform to source matrices.

Vertex position stage added:skin_vertex.hpp and riderTransformPositionNode match
program2 upper04C0..04D8, including original lower04C0 translation-column reload
in the oracle.12k original cases pass native and48k float words pass WebGPU/WebGL2
via rider-position-gpu-test.html. New test_skin_vertex_native.py emits24-float
records with hashes in skin-vertex-source.json. Live mesh/palette/projection and
material hookup remain pending; helpers alone do not change visible rendering.

Live Zoe source GPU position skinning is now enabled via rider-skinning.js:
source attributes/weights + previous/current matrices -> verified weighted columns
and vertex transform -> camera clip coordinates, bypassing duplicate object scale.
Menu uses Three pose; Sam remains existing rig path. Texture lifecycle and reset
snap handled; new mapping/lifecycle test in npm. Full suite/build and live longer
WebGPU/WebGL2 runs inspected. Uniform If/Else around custom vertex path caused
stretched meshes; reverted to select, then live rechecked. Cause unresolved; do
not re-add that branch without material-level regression coverage. backend=webgl
QA supported. Rider still unlit; next normal/lighting/material hookup and actual
performance optimization (consider per-group palette reuse) remain open.

Live skinning optimization: original386BD0 now runs once per unique ordered weight
group per animation tick in the core, cached for rendering. Zoe3058 vertices share
161 groups; new palette count/indices/data APIs. Shader fetches cached columns
instead of per-vertex weighting;4x322 RGBA32F previous/current texture. More upload,
less repeated GPU work; no measured power/FPS claim. All palette floats compared:
Sam743/Zoe3765 fields,9630frames each,zero native/WASM mismatches. Full npm/build
and longer WebGPU/WebGL2 live checks pass. Material still unlit; normals/lighting
and performance measurement remain open. Keep select, not prior broken If/Else.

Confirmed original normal palette via full program2 entry0080: weighted matrix
first3 columns copied unchanged to VU117+, independent of projection/extra inputs.
2k runs/312k words pass (test_skin_normal_palette_native.py). rider-skinning.js now
exposes lightingNormal from shared weighted palette and unmodified GC-derived
source-normal attributes, with source transform/no renormalization. Mapping test
and build pass. Node not yet consumed by visible material; coefficient/material
hookup and refresh timing remain open. See latest lighting doc for exact scope.

Zoe lighting is now visibly enabled via rider-material.js: native environment/rim/
local-light assembly -> original255-scaled bank -> quantized vertex lighting ->
encoded HIGHLIGHT2 -> linear output for Three/fog. Separate raw texture clone keeps
menu sRGB intact. Real material byte test passes both backends; live races inspected,
full npm/build pass. Explicit remaining gaps: web queries lights each simulation
tick (original async cadence unresolved), controller extra lights unwired, selector0
only, GS filtering/per-part blend/texture variants incomplete, Sam still old material.
Rim getter2C15D8 returns painter+50; prepare-environment exports authored.75 plus
five ELF constants. New GPU bank getter/reset tested in192-frame bridge comparison.

Sam now uses source skinning/lighting too. compile_sam_skin.py compiles his own
26-bone inverse binds (not Zoe hair) and integer-percent weights; metadata labels
authored content.5814 vertices/329 groups, max weight change0.7016 percentage
points;81 sampled poses show<=1.71mm quantization displacement. Source normals
quantized to ITOF15 at load. His425 tinted vertices preserved before HIGHLIGHT2;
tinted real-material tests pass WebGPU/WebGL2. Sam trace6423fields/9630frames has
zero native/WASM mismatches; full npm/build and live both-backend checks pass.
Compiler part of setup; reference reports in rider-lighting. Timing/filter/blend/
controller lights, fog/render fidelity and measured performance remain open.

Fixed major live fog omission: final original fog sprites use TEST_1=0x70000
(strict GREATER) and Z=0xFFFF, so scene depth>=65535 bypasses fog. Old shader
wrapped those near24-bit depths into CLUT, producing foreground bands/washed-out
riders. Source packet/sprite checks extended; shader now gates depth, samples
sceneColor with explicit screenUV and materializes it before selection. GPU tests
pass threshold/large-depth cases on both backends; full npm/build pass. Live view
has clear foreground; terrain clipping still visible/separate. See fog doc latest.

Terrain clipping investigation: installed render patches all use8x8 grids while
contact uses bicubic surfaces. audit_terrain_tessellation.py checks native/browser
vertex identity and actual triangle topology, then samples244,864 triangle
centroids/contained-XZ heights on1913 patches.678 patches have sampled render-above-
curve gaps>10cm;183>50cm (steep slopes amplify vertical differences). This is not
proof of the exact observed clipping event or an error bound. Reports in
local/browser-validation/terrain-tessellation-audit.*. New terrain_tessellation.py
uses Bernstein derivative hulls for conservative Euclidean interpolation bounds;
11478 authored samples and analytic cases pass. A global2cm plan costs10.8M tris
and still caps76 patches above target, so do not blindly replace the whole mesh.
Next localize the rider's actual clipping patch and integrate adaptive/near-rider
render tessellation with seam/budget handling. No mesh/physics change applied yet.

Clipping localized with new read-only terrain_contact_info + audit-rider-terrain.mjs
4500-tick neutral event replay. Sam/Zoe contact results match. Worst tick2172,
patch149512: board0.747m below analytic surface; tessellation adds only8mm.
BUT this is authored powder2, whose scaled depth3 target42.5cm matches measured
normal penetration41.5cm. Do not 'fix' intended powder sinking. Hard-surface0/4
examples still show up to24.7/26.3cm render errors; check CURRENT filtered depth
through powder transitions before blaming the solver. Sam JSON includes per-surface
worst frames. No behavior correction yet; diagnostics/full npm/build pass.
See terrain-render-fidelity.md for scope and next steps.

Current-depth diagnostic confirms hard-ground deep samples can be powder transitions
(e.g tick2313 depth3=39.17cm). Firm patch112136 at3261 instead has board above
analytic plane and26.3cm mesh error. New terrain-mesh.js builds bicubic meshes
with independently refined edges and center-fan stitching, keeping UVs/ordinary
cell topology. Synthetic curved8/32 seam, winding, coverage, open-edge and UV tests
pass;4 actual replay-site errors drop19..26cm ->0.8..1.5cm at32x32. Test in npm;
full suite/build pass. NOT live yet: implement authored adjacency matching,
distance-based selection and caching before swapping rendered patches. No physics
snap/mesh replacement performed in this step. See terrain-render-fidelity.md.

Terrain-render descriptors/adjacency ready. prepare_terrain_render.py publishes
1913 patches with installed material/UV/color provenance, bounds and3437 unique
reciprocal cubic-edge links (max6.4um control gap).762 unmatched/16 ambiguous edges
left unresolved. Base154953 vertices reproduce within0.5mm, UVs exactly. New
terrain-detail.js prioritizes contact, selects<=8 fully linked nearby patches and
reconciles edges across<=40 affected patches. Tests/setup wired; full npm/build
pass. Next worker/cache + atomic render replacement; current rendering unchanged.

Terrain worker/cache ready: terrain-worker-core.js + browser terrain-worker.js.
Validated<=40-patch complete batches, reciprocal edges,8MiB LRU, transfer-safe
copies, latest-generation cancellation and clear/init invalidation. Node tests
and actual browser-worker QA pass (21 patches,799608 cache bytes; sample19.4ms
cold/.3ms warm, NOT gameplay/performance proof). Full npm/build pass. Client still
must reject stale IDs and atomically swap overrides/suppress corresponding coarse
triangles; no live terrain replacement enabled yet. See terrain-render doc.

Live terrain refinement ENABLED. terrain-refinement.js schedules worker plans near
rider/contact, rejects stale IDs and commits complete batches between frames.
terrain-overlays.js validates geometry first, reuses unchanged overlays, suppresses
only corresponding coarse render triangles and restores indices exactly on reset.
Materials/UVs shared; collision assets untouched. Actual1913-patch mapping test,
21-patch/2688-triangle suppression/restoration and committed-site raycast(<3cm)
pass; full npm/build and live both-backend/restart checks pass. Remaining partial/
ambiguous edge coverage, crack/pop validation, measured upload/frame cost and
quality/budget tuning. Powder sinking is intentional and preserved.

Terrain coarse-index updates now use merged dirty BufferAttribute ranges, preserving
prior pending edits across reset/replacement/culling. Actual21-patch fixture schedules
32256 vs82296 bytes,34 ranges/28 buffers; not a measured FPS/power gain. Queued GPU
copy simulation/restoration and existing course/atomicity checks pass; full npm/build
and live both-backend checks pass. Vertex/collision assets unchanged. See terrain doc.

Terrain client now verifies full worker result against requested patch IDs,
resolutions and edges before commit. Dependency-injected client tests cover
pre-reset pending replies, superseded/disposed replies, incomplete batches and
committed (not requested) stats. Full npm/build and live startup/refinement pass.
No physics change; broad edge/visual/performance validation remains open.

Terrain selection churn measured on4500-tick neutral route:240 plans/1337 mesh-key
changes. Added2m retention advantage/exit margin using COMMITTED refined indices,
cleared on reset; contact priority and8/40 budgets unchanged. Selection-only replay
now212 plans/1233 mesh changes (not FPS/pop/power proof). Boundary/client/edge tests,
full npm/build and live startup pass. Audit scripts/reports documented in terrain
render fidelity. Broad coverage and measured renderer performance remain open.

Snow billboard view cap now live: verified program4 D98/DA0 caps projected half
size128 source pixels; viewport512x448 confirmed in3states. export_snow_view.py
adds asset metadata/setup; snow-billboard.js applies independent X/Y cap. Cache
includes camera world/projection changes.180 projection tests/full npm/build/live
render check pass. Emission/physics unchanged; full source clipping/depth/blend/fog
ordering and visual fidelity still open. See SNOW_RECOVERY latest section.

Snow instance uploads limited to active prefixes; empty emitters skip fresh
uploads and camera-only changes keep colors untouched. Shrink/regrow rewrites
visible data safely. Actual renderer construction/buffer lifecycle test added,
full npm/build pass. Initial allocation may still upload capacity; no FPS/power
claim. Particle behavior unchanged; depth/blend/fog ordering still open.

Found missing snow flipbook assets: chunk emitters1/2/3 declare8 textures but only
tmb1 was packaged. export_snow_flipbooks.py now verifies PS2 IDs14..21=tmb1..8,
decodes all8 distinct owned GC frames, preserves tmb1/view metadata and exports
sequence/rate bindings (30/45/45). Asset/renderer/full npm/build checks pass. NOT
animated yet: recover original frame phase/index calculation and renderer texture
switching; do not assume age*rate. See SNOW_RECOVERY latest section.

Snow tumble animation NOW CONNECTED.370788 uses shared per-emitter phase
(rate*elapsed; reset0 on overflow), draw370A3C selects base+integer phase; constructor
starts0. Not particle age/random phase. Native helper20k original cases pass.
Current infinite profiles step once/visual tick, reset phase, expose20 phase/ID
fields; renderer switches TextureNode without splitting instance batches. Core tests
reach8 frames; actual GPU renderer shows8 distinct images on both backends. Full
npm/build and Sam6443/Zoe3785-field9630frame comparisons pass. Full-scene source
phase alignment and remaining blend/depth/fog fidelity remain open.

Camera follow-up: the user identifies jump/landing movement as the main camera
complaint. Output audit now finds a gap despite passing input tests: fresh
browser DEFAULT_3 eye versus retained PS2 history differs57.76cm early,34.18cm
at touchdown,15.22cm at the last checkpoint. See the latest CAMERA_RECOVERY.md
section. No runtime camera tuning was applied; initialization/history needs
matched comparison. Current four air-steering fixtures pass; this does not
resolve the user's reported intermittent reversal.

Camera history follow-up resolved the diagnostic ambiguity: new
`tools/test_camera_browser_history.py` replays actual browser camera inputs.
Cold initialization reproduces browser output exactly; original tick369
initialization yields <1mm eye/target error through121 updates including
touchdown. Thus the34cm discrepancy above comes from fixture history, not
algorithm drift. Runtime camera behavior was not retuned. Next compare final
compositor/display and normal event camera lifecycle with matched history.

Camera terrain avoidance: fixed two source-backed errors after a20,000-call
original15EE00 comparison. Large lift-push rejection is150cm, not350cm; browser
terrain queries use preferredFraction0.5, not0. The oracle matches eye, lift,
normal, flags and query endpoints with controlled terrain hits and override
disabled. See CAMERA_RECOVERY.md and tools/test_camera_collision_native.py.

Camera terrain-query follow-up: enabled original kind2 refinement in the
browser camera callback. Its previous coarse-only normal/point path was wrong.
3,000 original queries across three captured patches match point/normal/UV
and retained coarse fraction exactly (1,110 hits;1,109 differ from coarse).
See tools/test_camera_terrain_reference.py and latest CAMERA_RECOVERY.md.
Full-world/cached-order query equivalence and final camera images remain open.

Rail audit: no missing spline query coverage (16,317 samples across all777
segments), but253/29,526 ideal bidirectional approaches are rejected because
the selected point is0.2..1.019cm behind the board. The original tolerance is
confirmed as0.2cm from ELF49B2BC. Next compare335128 search results directly
with original instructions; do not simply loosen latch tolerances. Reports:
local/browser-validation/rail-coverage.json and rail-attach-coverage.json.
Reproduce with tools/audit_rail_coverage.py. No gameplay behavior changed.

Rail follow-up: original335128 now directly compared on29,526 approaches over
all777 segments: zero mismatches in chosen point/distance/tangent/parameter.
Behind-board audit rejections are not a segment-search translation error.
Fixed separate rail-entry pose binding: use graph.scale instead of{1,1,1} for
originalRiderRootPresentation, matching normal/crash/native presentation.
Dynamic rail types2/3 and source host attachment scheduling remain open.

Object rail investigation: types2/3 are transformed object-bound descriptors
(35C698/35C5A0) and object-owned cubic arrays(348290), still unsupported.
A read-only census of33 saved Snow Jam human nearby caches finds neither type.
Thus unsupported object rails are not yet established as the cause on those
saved paths. Reproduce with tools/audit_original_rail_layers.py; report includes
snapshot hashes and positions. Next compare actual original attachment timing
and animation gates; do not treat this limited census as full-course coverage.

Rail animation gates:20,000 original108A48/1086B8 cases match acceptance and
query sequence with controlled world/animation inputs. Found and fixed browser
channel fade: clears requested semantic to438/class0 and cancels completion63
while preserving fading poses and ordinary markers. Matches native player and
311E88 channel-clear semantics. Tests: test_rail_attach_native.py and
test_browser_channel_fade.py. Live attachment scheduling remains separate.

Animation follow-up: BrowserAnimationGraph::setRate now changes only the first
playback sequence on the selected channel, matching311B20/314760 and the native
player. Previously it also froze/reversed outgoing crossfade clips when grabs
changed speed. Expanded test_browser_channel_fade.py covers the resulting
independent clip clocks and reverse/resume behavior.

Animation completion dispatch correction: shared native/browser graph now
collects a fixed completion batch in channel/list order before handlers mutate
sequences. New entries wait for the next pass; already-collected callbacks
survive flag cancellation.63 original312490 cases validate this order under
controlled mutation. Helper uses transient tokens for vector movement. Cross-
callback destruction of another queued node remains unverified (host skips
destroyed nodes). See test_animation_completion_dispatch.py and latest
ANIMATION_RECOVERY.md.

Snow render fix: clamps modulated RGB before EOTF/blending, preventing
over-range particle colours from producing extra brightness. Four saved PS2
base textures verified MODULATE/TCC1. Actual snow-material controlled-pixel
tests pass WebGPU/WebGL2; reports snow-colour-webgpu/webgl.json. Still uses
linear-space framebuffer blending; encoded GS blend/order parity remains open.
See latest SNOW_RECOVERY.md and tools/audit_snow_material.py.

Snow blend equation now verified through original profile/table/setter chain:
all10 profiles use GS ALPHA0x44, encoded source-alpha blending. Audit of165,120
ideal byte combinations finds up to74-level difference versus current linear
blend (white/black half opacity127 vs188). This is calculated, not full-scene
image parity. tools/audit_snow_blend.py writes snow-blend-audit.json. Next render
work needs encoded destination blending that retains shared depth and particle
overlap order; do not fake the correction with reduced emission or opacity.

Encoded snow pass now live: snow-composite.js copies encoded world color plus
depth, renders snow into one shared destination, then feeds encoded color to
fog. Normal gameplay uses it; originalFog=0 diagnostic still bypasses it.
Both GPU backends pass midtone/overlap/depth/resize/fog-integration tests with
and without antialiasing; full npm/build pass. Live Snow Jam smoke checked.
Remaining: GS byte rounding, original draw order, silhouette comparison and
profiling/empty-pass optimization. See latest SNOW_RECOVERY.md.

Snow optimization: encoded composite is skipped when no visible snow instances
exist; fog uses ordinary encoded world color instead. Tested empty-first,
start/clear/restart, empty resize, hide/show, pass counts and background pixels
on WebGPU/WebGL2 including antialiasing. Reports snow-empty-pass-*.json.
Pass tests must advance RAF between renders (FRAME scheduling); no measured
FPS/power claim. Focused snow tests/build pass.

Snow emitter ownership correction: live rider4930D0 class uses3710D0 per-birth
phase update(+10), draw371380, not370788/3708C0. Browser phase update moved
into birth submit using stepSeconds.20k original birth cases now verify phase
with varied dt/count/rate. Nonzero saved/reset phase and CPU upload/inter-
emitter draw order remain open. Audit: original-snow-emitter-bindings.json.
Also clarified that core RGB is already<=128; previous shader clamp is a guard,
not evidence for real over-range particles. Encoded blending fixed brightness.

Snow saved-start phases now exported and restored: state.emitter_flipbook_phases
from live emitter+10, validated against profile counts. Animated chunks start
at1.5/4.5/3.0, matching original-snow-emitter-bindings.json, instead of0.
Reset/64-tick texture-phase tests updated to this nonzero seed. No RNG draws
added to saved-start restoration. Fresh370DC8 constructor randomization and
its event lifecycle remain separate open work.
